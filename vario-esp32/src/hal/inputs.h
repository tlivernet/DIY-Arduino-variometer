#pragma once

#include <cstdint>

#include "core/buttons.h"

namespace vario::hal {

// Lecture des trois boutons. Volontairement minuscule : toute la logique
// (anti-rebond, appui long, répétition) est dans core/buttons, donc testable
// sans matériel.
class ButtonInputs {
 public:
  ButtonInputs(int pin_up, int pin_ok, int pin_down) : pins_{pin_up, pin_ok, pin_down} {}

  void begin();
  // Remplit le tableau de niveaux électriques attendu par vario::Buttons.
  void read(bool out[3]) const;

  int pin(Buttons::Id id) const { return pins_[static_cast<int>(id)]; }

 private:
  int pins_[3];
};

// Mesure de la tension batterie.
//
// Le 4.89 * analogRead() du montage AVR n'est pas transposable : l'ADC de
// l'ESP32 est en 3,3 V, non linéaire, et l'ADC2 est inutilisable quand le Wi-Fi
// est actif. On lit donc une broche ADC1 via analogReadMilliVolts(), qui applique
// l'étalonnage inscrit en eFuse, derrière un diviseur de tension.
class BatterySense {
 public:
  BatterySense(int pin, float divider_ratio) : pin_(pin), divider_ratio_(divider_ratio) {}

  void begin();

  // Lit la tension en volts, moyennée sur plusieurs acquisitions.
  float readVolts(int samples = 16);

  // Valeur lissée, constante de temps ~20 s : la tension d'une batterie sous
  // charge variable est bruitée, et une jauge qui saute est inexploitable.
  float smoothedVolts(float dt_s);

 private:
  int pin_;
  float divider_ratio_;
  float smoothed_v_ = 0.0f;
  bool initialized_ = false;
};

}  // namespace vario::hal
