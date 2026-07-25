#pragma once

#include "core/clock.h"

namespace vario {

// Moyenne glissante exponentielle indépendante de la cadence d'appel.
//
// Le code de 2017 utilisait des coefficients fixes par itération
// (pression × 0,95 + mesure × 0,05, puis vario × 0,8 + taux × 0,2). Comme la
// boucle tournait à ~30 Hz, ces poids correspondaient à des constantes de
// temps de ~645 ms et ~148 ms — mais uniquement à cette cadence. L'itération
// qui rafraîchissait l'écran durait deux fois plus longtemps et recevait
// pourtant le même poids : le filtre n'était pas invariant dans le temps.
//
// alpha = exp(-dt/tau) rend le comportement identique quelle que soit la
// cadence d'échantillonnage, ce qui est indispensable sur ESP32 où la boucle
// tourne bien plus vite et de façon moins régulière.
class Ema {
 public:
  explicit Ema(float tau_s) : tau_s_(tau_s) {}

  void reset(float value);
  float update(float value, float dt_s);

  float value() const { return value_; }
  bool initialized() const { return initialized_; }
  float tau() const { return tau_s_; }
  void setTau(float tau_s) { tau_s_ = tau_s; }

 private:
  float tau_s_;
  float value_ = 0.0f;
  bool initialized_ = false;
};

// Chaîne pression → altitude → vitesse verticale.
//
// Topologie conservée depuis 2017 (lissage de la pression, conversion ISA,
// puis dérivée lissée), mais exprimée en temps réel et protégée contre les
// entrées aberrantes. L'interface est volontairement celle qu'un filtre de
// Kalman baro+accéléromètre pourra reprendre à l'étape P2 : seule
// l'implémentation interne changera.
class VarioFilter {
 public:
  struct Config {
    // Constantes de temps reproduisant le ressenti du montage d'origine.
    float pressure_tau_s = 0.645f;
    float vario_tau_s = 0.148f;

    // Δt hors de ces bornes fait rejeter l'échantillon.
    //
    // La borne basse évite la division par ~0 : le code de 2017 ne testait
    // rien, ce qui produisait un vario NaN. Et comme NaN × 0,8 + x = NaN,
    // l'état ne s'en remettait jamais — écran et son morts jusqu'au reset.
    // La borne haute écarte les trous de plusieurs secondes (réveil, tâche
    // préemptée), qui donneraient une dérivée absurde.
    float min_dt_s = 0.0005f;
    float max_dt_s = 2.0f;
  };

  VarioFilter();
  explicit VarioFilter(const Config& config);

  // Réinitialise la chaîne sur une mesure de pression.
  // À appeler au démarrage et après un Tare / changement de QNH.
  void reset(float pressure_hpa, float qnh_hpa, float altitude_offset_m, Micros now);

  // Intègre une nouvelle mesure de pression.
  // Renvoie false si l'échantillon a été rejeté (Δt hors bornes ou valeur non
  // finie) ; dans ce cas l'état reste inchangé et reste exploitable.
  bool update(float pressure_hpa, float qnh_hpa, float altitude_offset_m, Micros now);

  float altitudeM() const { return altitude_m_; }
  float varioMs() const { return vario_ms_; }
  float smoothedPressureHpa() const { return pressure_.value(); }
  bool initialized() const { return pressure_.initialized(); }

  const Config& config() const { return config_; }

 private:
  Config config_;
  Ema pressure_;
  Ema vario_;
  Micros last_update_ = 0;
  float altitude_m_ = 0.0f;
  float vario_ms_ = 0.0f;
};

}  // namespace vario
