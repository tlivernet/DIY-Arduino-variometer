#pragma once

#include <cstdint>

namespace vario::hal {

// Attaque du piezo en push-pull par le générateur PWM matériel (LEDC).
//
// UN SEUL canal LEDC est utilisé, et son signal est routé vers DEUX broches par
// la matrice GPIO, la seconde avec le bit d'inversion. On obtient ainsi un
// déphasage de 180° exact à n'importe quelle fréquence et n'importe quel rapport
// cyclique, avec la moitié des ressources et sans aucun calcul de phase à
// refaire à chaque note. L'approche à deux canaux décalés fonctionne aussi, mais
// exige de recalculer le point de bascule à chaque changement de fréquence et se
// casse dès qu'on utilise le rapport cyclique pour régler le volume.
//
// Pourquoi le matériel et non un synthétiseur logiciel : écrire en mémoire flash
// impose de désactiver le cache d'instructions, ce qui **gèle les deux cœurs**
// pendant 1 à 3 ms pour une page, et jusqu'à 50 ms pour un effacement. Un son
// engendré par LEDC traverse ces gels sans broncher.
//
// AVERTISSEMENT MATÉRIEL. Deux broches ESP32 en push-pull donnent 6,6 V crête à
// crête aux bornes du piezo, contre 10 pour le toneAC du montage AVR, soit
// ~3,6 dB de moins — sur le critère le plus important d'un variomètre, être
// audible dans le bruit du vent. Il faut tamponner les deux sorties par une
// porte logique alimentée en 5 V.
class LedcAudio {
 public:
  struct Config {
    int pin_a;
    int pin_b;
    // Canal et minuteur LEDC. À laisser libres pour les autres usages.
    int channel = 0;
    int timer = 0;
    // Résolution du rapport cyclique. 10 bits suffisent et laissent la
    // fréquence monter loin.
    int resolution_bits = 10;
  };

  explicit LedcAudio(const Config& config) : config_(config) {}

  void begin();

  // Émet un son. volume va de 0 (muet) à 4.
  void tone(std::uint16_t frequency_hz, int volume);

  // Coupe le son en laissant les deux broches au même niveau, donc aucune
  // tension aux bornes du piezo.
  void silence();

  bool sounding() const { return sounding_; }

 private:
  void applyDuty(std::uint32_t duty);

  Config config_;
  bool sounding_ = false;
  std::uint16_t current_frequency_ = 0;
};

}  // namespace vario::hal
