#pragma once

#include <cstdint>

#include "core/vec3.h"

namespace vario::hal {

// Centrale inertielle 6 axes MPU6050 / MPU6500, en I2C.
//
// Composant ancien, retenu pour une raison d'approvisionnement : les IMU
// techniquement supérieurs sont en tension (ICM-42688-P annoncé à 22+ semaines,
// série Bosch BMI à 26+ semaines au deuxième trimestre 2026), alors que le
// module GY-521 se trouve partout. Son bruit gyrométrique (~0,05 dps/√Hz contre
// ~0,025) est largement suffisant ici : on ne stabilise pas un drone, on corrige
// une accélération verticale à quelques hertz.
class Mpu6050 {
 public:
  static constexpr std::uint8_t kAddressPrimary = 0x68;  // AD0 au niveau bas
  static constexpr std::uint8_t kAddressSecondary = 0x69;

  // Réorientation des axes du capteur vers le repère du boîtier
  // (X avant, Y droite, Z vers le bas).
  //
  // Indispensable : l'orientation du module sur la carte est un choix
  // mécanique, et une erreur ici se traduit par une assiette fausse donc un
  // vario faux. À vérifier au banc, capteur à plat, avant tout vol.
  struct AxisMap {
    int source[3] = {0, 1, 2};  // indice de l'axe capteur pour X, Y, Z
    float sign[3] = {1.0f, 1.0f, 1.0f};
  };

  struct Config {
    std::uint8_t address = kAddressPrimary;
    AxisMap axes;
    // Diviseur de cadence : 1 kHz / (1 + div). 1 donne 500 Hz.
    std::uint8_t sample_rate_divider = 1;
    // Filtre passe-bas interne : 2 donne ~94 Hz sur l'accéléromètre.
    std::uint8_t dlpf = 2;
  };

  Mpu6050() = default;
  explicit Mpu6050(const Config& config) : config_(config) {}

  // Réveille le capteur et le configure. Renvoie false si WHO_AM_I ne répond
  // pas la bonne valeur.
  bool begin();

  // Lit un échantillon. Renvoie false en cas d'échec I2C.
  bool read(Vec3& accel_mps2, Vec3& gyro_rps);

  float temperatureC() const { return temperature_c_; }
  std::uint32_t errorCount() const { return error_count_; }

  // Mesure le biais du gyromètre, capteur immobile. À appeler pendant la
  // fenêtre d'alignement du démarrage.
  bool measureGyroBias(int samples, Vec3& bias_rps);

 private:
  bool writeRegister(std::uint8_t reg, std::uint8_t value);
  bool readRegisters(std::uint8_t reg, std::uint8_t* out, std::size_t count);
  Vec3 remap(float a, float b, float c) const;

  Config config_;
  float temperature_c_ = 15.0f;
  std::uint32_t error_count_ = 0;
};

}  // namespace vario::hal
