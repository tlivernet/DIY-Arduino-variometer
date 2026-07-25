// Rend les écrans du firmware dans des fichiers PBM.
//
// Comme la couche d'affichage est du C++ pur, on peut produire exactement les
// images que le Sharp Memory LCD affichera, sans matériel. Sert à valider la
// mise en page, à illustrer la documentation, et de référence pour les tests de
// comparaison d'images.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/screens.h"

namespace {

using vario::ui::Canvas;
using vario::ui::kScreenHeight;
using vario::ui::kScreenWidth;

// PBM binaire (P4) : un bit par pixel, 1 = noir, exactement notre tampon.
bool writePbm(const std::string& path, const Canvas& canvas) {
  std::FILE* file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) return false;

  std::fprintf(file, "P4\n%d %d\n", canvas.width(), canvas.height());
  const std::size_t bytes = static_cast<std::size_t>(canvas.stride()) * canvas.height();
  const bool ok = std::fwrite(canvas.buffer(), 1, bytes, file) == bytes;
  std::fclose(file);
  return ok;
}

vario::ui::DisplayState climbingState() {
  vario::ui::DisplayState s;
  s.vario_ms = 2.4f;
  s.altitude_m = 2456.0f;
  s.temperature_c = 4.0f;
  s.hour = 13;
  s.minute = 42;
  s.second = 7;
  s.flying = true;
  s.flight_seconds = 5025;  // 1:23:45
  s.stat_slot = 3;
  s.battery_percent = 72;
  s.ble_connected = true;

  // Une trace plausible : entrée dans un thermique.
  const float samples[] = {-1.1f, -0.9f, -1.4f, -0.6f, 0.2f, 0.9f,  1.8f, 2.6f,
                           3.1f,  2.4f,  1.6f,  2.2f,  2.9f, 3.4f,  2.8f, 1.9f,
                           2.5f,  3.0f,  2.2f,  1.4f,  0.8f, -0.3f, 0.6f, 1.7f,
                           2.3f,  2.9f,  2.1f,  1.5f,  2.0f, 2.4f};
  s.track_count = static_cast<int>(sizeof(samples) / sizeof(samples[0]));
  std::memcpy(s.track_ms, samples, sizeof(samples));
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string out = (argc > 1) ? argv[1] : ".";

  std::vector<std::uint8_t> buffer(
      static_cast<std::size_t>((kScreenWidth + 7) / 8) * kScreenHeight, 0);
  Canvas canvas(buffer.data(), kScreenWidth, kScreenHeight);

  int written = 0;
  const auto save = [&](const char* name) {
    if (writePbm(out + "/" + name + std::string(".pbm"), canvas)) {
      ++written;
    } else {
      std::fprintf(stderr, "echec ecriture %s\n", name);
    }
  };

  // Vario en montée, cas nominal.
  vario::ui::drawVarioScreen(canvas, climbingState());
  save("01-vario-montee");

  // Vario en descente, son coupé, Wi-Fi actif.
  {
    vario::ui::DisplayState s = climbingState();
    s.vario_ms = -1.7f;
    s.altitude_m = 1180.0f;
    s.audio_muted = true;
    s.wifi_active = true;
    s.ble_connected = false;
    s.battery_percent = 24;
    s.temperature_c = 12.0f;
    for (int i = 0; i < s.track_count; ++i) s.track_ms[i] = -s.track_ms[i] * 0.6f;
    vario::ui::drawVarioScreen(canvas, s);
    save("02-vario-descente");
  }

  // Au sol, tout à zéro : c'est l'état où l'ancien vario bipait dans le vide.
  {
    vario::ui::DisplayState s;
    s.vario_ms = 0.0f;
    s.altitude_m = 412.0f;
    s.hour = 9;
    s.minute = 5;
    s.second = 31;
    s.flying = false;
    s.battery_percent = 100;
    s.temperature_c = 18.0f;
    vario::ui::drawVarioScreen(canvas, s);
    save("03-vario-au-sol");
  }

  // Fort taux de montée, pour vérifier la saturation de la bande et la largeur
  // des chiffres.
  {
    vario::ui::DisplayState s = climbingState();
    s.vario_ms = 7.8f;
    s.altitude_m = 3980.0f;
    s.temperature_c = -6.0f;
    s.battery_percent = 48;
    for (int i = 0; i < s.track_count; ++i) s.track_ms[i] = 3.0f + 0.15f * i;
    vario::ui::drawVarioScreen(canvas, s);
    save("04-vario-fort-taux");
  }

  // Calibration au démarrage.
  vario::ui::drawCalibrationScreen(canvas, 0.6f);
  save("05-calibration");

  // Statistiques d'un vol.
  {
    vario::ui::StatsView stats;
    stats.slot = 3;
    stats.empty = false;
    stats.day = 12;
    stats.month = 5;
    stats.year = 2026;
    stats.start_hour = 13;
    stats.start_minute = 42;
    stats.duration_s = 5025;
    stats.alt_max_m = 2456.0f;
    stats.alt_min_m = 1180.0f;
    stats.best_climb_ms = 4.2f;
    stats.worst_sink_ms = -3.1f;
    stats.cumulative_gain_m = 1840.0f;
    vario::ui::drawStatsScreen(canvas, stats);
    save("06-statistiques");
  }

  // Menu, en consultation puis en édition.
  {
    vario::ui::MenuView menu;
    menu.section = "OPTIONS";
    menu.item = "MONTEE";
    menu.value = "0.2";
    menu.unit = "M/S";
    menu.index = 3;
    menu.count = 8;
    vario::ui::drawMenuScreen(canvas, menu);
    save("07-menu");

    menu.editing = true;
    menu.value = "0.3";
    vario::ui::drawMenuScreen(canvas, menu);
    save("08-menu-edition");
  }

  std::printf("%d images ecrites dans %s\n", written, out.c_str());
  return written > 0 ? 0 : 1;
}
