#pragma once

#include <cstdint>

#include "core/clock.h"

namespace vario::hal {

// Baromètre MS5611, en I2C, avec un ordonnancement NON BLOQUANT.
//
// C'est le point structurel de tout ce pilote. Le montage de 2017 appelait
// bmp085.getEvent(), qui contenait delay(5) puis delay(26) : la boucle était
// donc bridée à ~30 Hz par des attentes actives, et TOUTES les constantes de
// filtrage étaient exprimées en nombre d'itérations de cette boucle.
//
// Ici, service() lance une conversion et rend la main. Le résultat est lu au
// passage suivant, une fois le temps de conversion écoulé. Rien ne bloque, donc
// la centrale inertielle peut être lue à 500 Hz pendant que le baromètre
// travaille.
//
// Budget d'échantillonnage, calculé depuis la fiche technique :
//   OSR 4096 : 9,04 ms max par conversion, bruit ~0,012 mbar ≈ 10 cm
//   OSR 1024 : 2,28 ms max
//
// La pression est donc échantillonnée à ~100 Hz, et la température seulement à
// 4 Hz. Alterner température et pression à parts égales diviserait la cadence
// de pression par deux sans rien apporter : la température varie lentement. Elle
// est en plus lissée avec une constante de temps de 5 s, ce qui supprime sa
// contribution au bruit pour rien.
class Ms5611 {
 public:
  // Adresse I2C : 0x77 quand CSB est au niveau bas (cas des modules GY-63),
  // 0x76 sinon.
  static constexpr std::uint8_t kAddressPrimary = 0x77;
  static constexpr std::uint8_t kAddressSecondary = 0x76;

  struct Config {
    std::uint8_t address = kAddressPrimary;
    // Une mesure de température toutes N mesures de pression.
    int temperature_every = 25;
    // Constante de temps du lissage de la température, en secondes.
    float temperature_tau_s = 5.0f;
  };

  Ms5611() = default;
  explicit Ms5611(const Config& config) : config_(config) {}

  // Réinitialise le capteur et lit son étalonnage d'usine.
  // Renvoie false si le capteur ne répond pas ou si l'étalonnage est
  // manifestement invalide — cas fréquent avec les modules contrefaits.
  bool begin();

  // À appeler aussi souvent que possible. Fait avancer la machine à états et
  // renvoie true quand une NOUVELLE mesure de pression est disponible.
  bool service(Micros now);

  float pressureHpa() const { return pressure_hpa_; }
  float temperatureC() const { return temperature_c_; }
  bool ready() const { return calibrated_ && have_temperature_; }

  // Nombre d'échanges I2C ayant échoué depuis le démarrage. Une valeur qui
  // grimpe en vol signale un problème de câblage ou de bus.
  std::uint32_t errorCount() const { return error_count_; }

 private:
  enum class Phase : std::uint8_t {
    Idle,
    ConvertingPressure,
    ConvertingTemperature,
  };

  bool reset();
  bool readCalibration();
  bool startConversion(bool temperature);
  bool readAdc(std::uint32_t& raw);
  void compensate();

  bool writeCommand(std::uint8_t command);
  bool readBytes(std::uint8_t command, std::uint8_t* out, std::size_t count);

  Config config_;

  // Étalonnage d'usine, C1 à C6.
  std::uint16_t coefficients_[6] = {};
  bool calibrated_ = false;

  Phase phase_ = Phase::Idle;
  Micros phase_started_ = 0;
  int pressure_since_temperature_ = 0;

  std::uint32_t raw_pressure_ = 0;
  std::uint32_t raw_temperature_ = 0;
  bool have_temperature_ = false;

  float pressure_hpa_ = 1013.25f;
  float temperature_c_ = 15.0f;
  float smoothed_dt_ = 0.0f;
  bool smoothed_dt_valid_ = false;

  std::uint32_t error_count_ = 0;
};

}  // namespace vario::hal
