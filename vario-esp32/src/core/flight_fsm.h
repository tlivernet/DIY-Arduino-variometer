#pragma once

#include <cstdint>

#include "core/clock.h"

namespace vario {

// Détection automatique du décollage et de l'atterrissage.
//
// Logique et seuils conservés depuis 2017 :
//   - au sol, la ligne de base d'altitude est réarmée toutes les 15 s ;
//     un écart de plus de 4 m par rapport à elle déclenche le décollage ;
//   - en vol, rester 15 s consécutives dans une bande de ±0,75 m autour de la
//     ligne de base déclenche l'atterrissage.
//
// Deux corrections de fond par rapport à l'original :
//
// 1. La ligne de base est un float. Elle était un `int`, donc l'affectation
//    depuis l'altitude flottante tronquait. Si on atterrissait à une altitude
//    dont la partie fractionnaire dépassait 0,75 (par exemple 1234,9 m), la
//    ligne de base valait 1234 et la bande (1233,25 ; 1234,75) excluait
//    l'altitude réelle : la condition d'arrêt ne pouvait jamais être vraie et
//    **le chrono de vol ne s'arrêtait jamais**. Cela touchait environ un
//    atterrissage sur quatre.
//
// 2. Après un atterrissage, la machine revient en Idle et peut redétecter un
//    vol. L'original restait bloqué : une fois chrono_start et chrono tous
//    deux non nuls, aucun des deux blocs de test ne s'exécutait plus, et si la
//    case de statistiques suivante était déjà occupée, plus aucun vol n'était
//    jamais enregistré.
class FlightDetector {
 public:
  struct Config {
    // Écart d'altitude déclenchant le décollage, en m.
    float start_trigger_m = 4.0f;
    // Demi-largeur de la bande d'immobilité, en m.
    float stop_band_m = 0.75f;
    // Période de réarmement de la ligne de base au sol, en s.
    float rearm_s = 15.0f;
    // Durée d'immobilité confirmant l'atterrissage, en s.
    float stop_s = 15.0f;
  };

  enum class State : std::uint8_t { Idle, Flying };

  struct Event {
    bool took_off = false;
    bool landed = false;
  };

  FlightDetector() = default;
  explicit FlightDetector(const Config& config) : config_(config) {}

  // Réarme la détection sur l'altitude courante, sans émettre d'événement.
  // À appeler au démarrage et après un Tare ou un changement de QNH, pour
  // éviter qu'un recalage soit pris pour un décollage.
  void reset(float altitude_m, Micros now);

  // À appeler régulièrement (~1 Hz suffit, mais toute cadence fonctionne :
  // la logique est temporelle, pas en nombre d'appels comme en 2017).
  Event update(float altitude_m, Micros now);

  State state() const { return state_; }
  bool flying() const { return state_ == State::Flying; }
  float baselineM() const { return baseline_m_; }
  const Config& config() const { return config_; }

 private:
  Config config_;
  State state_ = State::Idle;
  float baseline_m_ = 0.0f;
  Micros baseline_since_ = 0;
  bool in_band_ = false;
  Micros in_band_since_ = 0;
  bool initialized_ = false;
};

}  // namespace vario
