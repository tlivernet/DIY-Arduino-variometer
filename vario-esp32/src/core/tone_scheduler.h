#pragma once

#include <cstdint>

#include "core/clock.h"
#include "core/tone_law.h"

namespace vario {

// Ce que la couche audio doit produire à l'instant présent.
struct ToneOutput {
  bool sounding = false;
  std::uint16_t frequency_hz = 0;

  bool operator==(const ToneOutput& other) const {
    return sounding == other.sounding &&
           (!sounding || frequency_hz == other.frequency_hz);
  }
};

// Machine à états du bipeur.
//
// Reproduit le déroulé du montage de 2017, mais en temps réel plutôt qu'en
// nombre d'itérations de boucle :
//
//   Silence ──(franchissement de seuil)──▶ Amorce ──▶ Note ⇄ Pause
//
// L'amorce est un son continu de ~1,35 s dont la hauteur suit le vario en
// direct. C'était un effet de bord du compteur timeNoPauseBeep d'origine
// (41 itérations avec la temporisation court-circuitée), mais c'est une vraie
// bonne idée : à l'entrée dans une ascendance on entend immédiatement le son
// monter, avant que les bips ne s'installent. On la garde explicitement.
class ToneScheduler {
 public:
  struct Config {
    // Seuils de déclenchement, en m/s. 0 désactive le seuil concerné,
    // comme dans l'original.
    float climb_threshold_ms = 0.3f;
    float sink_threshold_ms = -1.1f;

    // Hystérésis, en m/s. Absente en 2017 : le vario bruitait de ±0,3 m/s
    // pour un seuil à 0,3 m/s, donc à l'arrêt le bipeur franchissait le seuil
    // en permanence. Chaque franchissement relançant l'amorce continue, on
    // obtenait des chaînes de sons continus de 1,35 s au sol.
    float hysteresis_ms = 0.1f;

    // Durée de l'amorce continue.
    int lead_in_ms = 1350;

    ToneProfile profile = ToneProfile::Classic;

    // Coupe toute sortie sonore (réglage « Sons : Off »).
    bool muted = false;

    // Inhibe le son tant que les capteurs ne sont pas stabilisés. Sert à
    // masquer la convergence du biais d'accéléromètre au démarrage, qui sinon
    // produit des sons parasites (« bruits de canard ») pendant quelques
    // secondes.
    bool inhibited = false;
  };

  enum class State : std::uint8_t { Silent, LeadIn, Note, Pause };

  ToneScheduler() = default;
  explicit ToneScheduler(const Config& config) : config_(config) {}

  // À appeler aussi souvent que possible ; renvoie l'état sonore courant.
  ToneOutput update(float vario_ms, Micros now);

  State state() const { return state_; }
  Config& config() { return config_; }
  const Config& config() const { return config_; }

 private:
  bool triggered(float vario_ms) const;
  bool released(float vario_ms) const;
  void enter(State state, Micros now);

  Config config_;
  State state_ = State::Silent;
  Micros state_since_ = 0;
  std::uint16_t note_ms_ = 0;
  std::uint16_t note_frequency_hz_ = 0;
  bool initialized_ = false;
};

}  // namespace vario
