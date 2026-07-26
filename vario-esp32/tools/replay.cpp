// Rejoue un enregistrement de trames brutes à travers toute la chaîne
// d'estimation, en natif.
//
//   replay trace.csv [--rest N] [--qnh 1013.25] [--volume 3] > sortie.csv
//
// Entrée : un CSV de trames brutes (voir core/raw_frame.h), tel que produit par
// tools/record.py branché sur le firmware. Sortie : un CSV de l'état estimé,
// une ligne par trame, avec altitude, vitesse verticale, son.
//
// C'est l'outil de mise au point du filtre : on rejoue un vrai vol, on trace le
// résultat, on ajuste un réglage, on rejoue — en une seconde, autant de fois
// qu'on veut, sur le même vol. Le test de non-régression (test/test_replay)
// s'appuie sur le même moteur.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/config_schema.h"
#include "core/raw_frame.h"
#include "core/replay.h"

namespace {

using vario::Config;
using vario::RawFrame;
using vario::ReplayEngine;
using vario::ReplayOutput;

// Nombre de trames de repos servant à l'alignement, si non précisé. À ~100 Hz,
// 200 trames font 2 s, comme la fenêtre d'alignement du firmware.
constexpr std::size_t kDefaultRestFrames = 200;

bool readFrames(const char* path, std::vector<RawFrame>& out) {
  std::FILE* file = std::fopen(path, "rb");
  if (file == nullptr) {
    std::fprintf(stderr, "Impossible d'ouvrir %s\n", path);
    return false;
  }

  char line[256];
  while (std::fgets(line, sizeof(line), file) != nullptr) {
    RawFrame frame;
    if (vario::parseRawFrame(line, frame)) out.push_back(frame);
  }
  std::fclose(file);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "Usage : %s trace.csv [--rest N] [--qnh HPA] [--volume V]\n",
                 argv[0]);
    return 2;
  }

  const char* path = argv[1];
  std::size_t rest = kDefaultRestFrames;
  Config config;  // valeurs par défaut du firmware

  for (int i = 2; i < argc; ++i) {
    if (std::strcmp(argv[i], "--rest") == 0 && i + 1 < argc) {
      rest = static_cast<std::size_t>(std::atol(argv[++i]));
    } else if (std::strcmp(argv[i], "--qnh") == 0 && i + 1 < argc) {
      config.qnh_hpa = static_cast<float>(std::atof(argv[++i]));
    } else if (std::strcmp(argv[i], "--volume") == 0 && i + 1 < argc) {
      config.volume = std::atoi(argv[++i]);
    } else {
      std::fprintf(stderr, "Argument inconnu : %s\n", argv[i]);
      return 2;
    }
  }

  std::vector<RawFrame> frames;
  if (!readFrames(path, frames)) return 1;
  if (frames.empty()) {
    std::fprintf(stderr, "Aucune trame exploitable dans %s\n", path);
    return 1;
  }
  if (rest == 0) rest = 1;
  if (rest > frames.size()) rest = frames.size();

  ReplayEngine engine(config);
  if (!engine.align(frames.data(), rest)) {
    std::fprintf(stderr,
                 "Alignement impossible : la fenetre de repos ne contient "
                 "aucune mesure barometrique.\n");
    return 1;
  }

  std::printf(
      "t_us,alt_m,vario_disp_ms,vario_audio_ms,innov_m,muted,sounding,freq_hz,flying\n");
  for (std::size_t i = rest; i < frames.size(); ++i) {
    const ReplayOutput o = engine.step(frames[i]);
    std::printf(
        "%lld,%.3f,%.3f,%.3f,%.3f,%d,%d,%u,%d\n", static_cast<long long>(o.timestamp),
        static_cast<double>(o.altitude_m), static_cast<double>(o.vario_display_ms),
        static_cast<double>(o.vario_audio_ms), static_cast<double>(o.innovation_m),
        o.audio_muted ? 1 : 0, o.sounding ? 1 : 0, static_cast<unsigned>(o.frequency_hz),
        o.flying ? 1 : 0);
  }
  return 0;
}
