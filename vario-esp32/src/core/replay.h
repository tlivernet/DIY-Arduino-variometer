#pragma once

#include "core/attitude.h"
#include "core/audio_gate.h"
#include "core/clock.h"
#include "core/config_schema.h"
#include "core/flight_fsm.h"
#include "core/isa.h"
#include "core/kalman.h"
#include "core/raw_frame.h"
#include "core/tone_scheduler.h"
#include "core/vario_filter.h"

namespace vario {

// Résultat de l'estimation pour une trame rejouée. C'est l'équivalent, dans le
// harnais de rejeu, de ce que la tâche capteurs publie sur la cible.
struct ReplayOutput {
  Micros timestamp = 0;
  float altitude_m = 0.0f;
  float vario_audio_ms = 0.0f;
  float vario_display_ms = 0.0f;
  float innovation_m = 0.0f;
  bool audio_muted = true;
  bool sounding = false;  // état sonore à cet instant
  std::uint16_t frequency_hz = 0;
  bool flying = false;
};

// Rejoue une suite de trames brutes à travers TOUTE la chaîne d'estimation.
//
// L'ordre des opérations reproduit EXACTEMENT VarioApp::sensorLoop() puis
// audioLoop() : mêmes appels, mêmes lissages, mêmes seuils. C'est la condition
// pour que le rejeu soit fidèle — sinon on réglerait un filtre qui n'est pas
// celui qui vole. Toute divergence entre ce moteur et le firmware est un bug à
// corriger, pas une liberté.
//
// Le moteur est du C++17 pur : il se compile et se teste en natif, sans
// Arduino. C'est là tout l'intérêt de P5 — dérouler dix vrais vols en une
// seconde sur un portable plutôt qu'en dix week-ends sur une pente.
class ReplayEngine {
 public:
  // Construit le moteur avec une configuration utilisateur (seuils, QNH,
  // profil de son…), reportée dans les filtres comme le fait VarioApp::begin().
  explicit ReplayEngine(const Config& config);

  // Aligne l'assiette et amorce les filtres sur une fenêtre de repos, à l'image
  // de VarioApp::alignAtRest(). Fournir les premières trames au repos ; leur
  // moyenne définit le « bas » et le biais gyrométrique. Renvoie false si la
  // fenêtre est vide.
  bool align(const RawFrame* rest_frames, std::size_t count);

  // Rejoue une trame et renvoie l'estimation correspondante. À appeler dans
  // l'ordre chronologique. align() doit avoir été appelé avant.
  ReplayOutput step(const RawFrame& frame);

  bool aligned() const { return aligned_; }

 private:
  Config config_;

  AttitudeFilter attitude_;
  VarioKalman kalman_;
  AudioGate audio_gate_;
  ToneScheduler tones_;
  FlightDetector flight_;

  // Mêmes constantes de temps que VarioApp : lissage rapide pour le son, lent
  // pour l'écran.
  Ema vario_audio_{0.15f};
  Ema vario_display_{0.60f};

  Micros last_timestamp_ = 0;
  float last_pressure_hpa_ = kQnhDefaultHpa;
  float last_temperature_c_ = 15.0f;
  bool aligned_ = false;
  bool started_ = false;
};

}  // namespace vario
