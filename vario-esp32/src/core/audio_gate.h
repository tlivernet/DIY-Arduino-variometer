#pragma once

#include <cstdint>

#include "core/clock.h"

namespace vario {

// Inhibition du son au démarrage, le temps que le filtre se stabilise.
//
// Sans cela, l'estimation du biais d'accéléromètre part de zéro avec une
// grande incertitude et met quelques secondes à converger, en produisant des
// excursions de ±1 à 3 m/s. Sur le projet de référence de har-in-air, cela se
// traduit à l'allumage par des « bruits de canard » que l'auteur documente
// explicitement.
//
// La vraie correction est structurelle : l'alignement à l'arrêt (voir
// AttitudeFilter::align) fait partir le biais juste par construction. Cette
// machine à états est la seconde ligne de défense, et surtout elle donne au
// pilote une confirmation positive que le son est armé.
class AudioGate {
 public:
  struct Config {
    // Délai minimal après l'alignement avant de pouvoir rendre le son.
    float min_settle_s = 1.5f;
    // Plafond dur : même si les critères ne sont pas réunis, on rend le son.
    // Indispensable — un allumage en voiture ou en télésiège ne doit pas
    // laisser l'appareil muet indéfiniment.
    float max_settle_s = 4.0f;
    // Seuils de stabilité.
    float max_innovation_m = 0.30f;
    float max_velocity_ms = 0.30f;
    // Durée pendant laquelle la vitesse doit rester calme.
    float quiet_s = 0.5f;
  };

  enum class State : std::uint8_t {
    BootMute,  // avant l'alignement : silence total
    Settling,  // aligné, en attente de stabilisation
    Live,      // son opérationnel
  };

  AudioGate() = default;
  explicit AudioGate(const Config& config) : config_(config) {}

  // À appeler quand AttitudeFilter::align() a réussi.
  void onAligned(Micros now);

  // Repasse en stabilisation, par exemple après une garde de divergence.
  void resettle(Micros now);

  // À appeler régulièrement. Renvoie true si le son doit rester coupé.
  bool update(float velocity_ms, float innovation_m, Micros now);

  State state() const { return state_; }
  bool muted() const { return state_ != State::Live; }

  // Passe à true pendant un seul appel, au moment où le son devient
  // opérationnel : de quoi jouer le petit motif « prêt ».
  bool justWentLive() const { return just_went_live_; }

  const Config& config() const { return config_; }

 private:
  Config config_;
  State state_ = State::BootMute;
  Micros state_since_ = 0;
  Micros quiet_since_ = 0;
  bool quiet_ = false;
  bool just_went_live_ = false;
};

}  // namespace vario
