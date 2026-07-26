#include "hal/ms5611.h"

#include <Arduino.h>
#include <Wire.h>

#include <cmath>

namespace vario::hal {
namespace {

// Jeu de commandes de la fiche technique.
constexpr std::uint8_t kCmdReset = 0x1E;
constexpr std::uint8_t kCmdConvertD1Osr4096 = 0x48;  // pression
constexpr std::uint8_t kCmdConvertD2Osr1024 = 0x52;  // température
constexpr std::uint8_t kCmdAdcRead = 0x00;
constexpr std::uint8_t kCmdPromRead = 0xA0;

// Durées de conversion maximales, majorées d'une marge de sécurité.
constexpr Micros kPressureConversionUs = 10000;    // 9,04 ms + marge
constexpr Micros kTemperatureConversionUs = 3000;  // 2,28 ms + marge
constexpr Micros kResetSettleUs = 5000;

}  // namespace

bool Ms5611::writeCommand(std::uint8_t command) {
  Wire.beginTransmission(config_.address);
  Wire.write(command);
  if (Wire.endTransmission() != 0) {
    ++error_count_;
    return false;
  }
  return true;
}

bool Ms5611::readBytes(std::uint8_t command, std::uint8_t* out, std::size_t count) {
  Wire.beginTransmission(config_.address);
  Wire.write(command);
  if (Wire.endTransmission(false) != 0) {  // pas de STOP : lecture enchaînée
    ++error_count_;
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(config_.address), static_cast<int>(count)) !=
      static_cast<int>(count)) {
    ++error_count_;
    return false;
  }
  for (std::size_t i = 0; i < count; ++i) out[i] = static_cast<std::uint8_t>(Wire.read());
  return true;
}

bool Ms5611::reset() {
  if (!writeCommand(kCmdReset)) return false;
  delayMicroseconds(kResetSettleUs);
  return true;
}

bool Ms5611::readCalibration() {
  for (int i = 0; i < 6; ++i) {
    std::uint8_t buffer[2] = {0, 0};
    // Les coefficients occupent les mots 1 à 6 de la PROM ; le mot 0 contient
    // la référence usine et le mot 7 le CRC.
    const std::uint8_t command = static_cast<std::uint8_t>(kCmdPromRead + 2 * (i + 1));
    if (!readBytes(command, buffer, 2)) return false;
    coefficients_[i] = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(buffer[0]) << 8) | buffer[1]);
  }

  // Contrôle de vraisemblance. Un module contrefait ou un bus mal câblé renvoie
  // typiquement tout à 0 ou tout à 0xFFFF ; mieux vaut le refuser au démarrage
  // que produire des altitudes absurdes en vol.
  bool all_zero = true;
  bool all_ones = true;
  for (int i = 0; i < 6; ++i) {
    if (coefficients_[i] != 0x0000) all_zero = false;
    if (coefficients_[i] != 0xFFFF) all_ones = false;
  }
  return !all_zero && !all_ones;
}

bool Ms5611::begin() {
  calibrated_ = false;
  have_temperature_ = false;
  phase_ = Phase::Idle;

  if (!reset()) return false;
  if (!readCalibration()) return false;

  calibrated_ = true;
  return true;
}

bool Ms5611::startConversion(bool temperature) {
  return writeCommand(temperature ? kCmdConvertD2Osr1024 : kCmdConvertD1Osr4096);
}

bool Ms5611::readAdc(std::uint32_t& raw) {
  std::uint8_t buffer[3] = {0, 0, 0};
  if (!readBytes(kCmdAdcRead, buffer, 3)) return false;

  raw = (static_cast<std::uint32_t>(buffer[0]) << 16) |
        (static_cast<std::uint32_t>(buffer[1]) << 8) |
        static_cast<std::uint32_t>(buffer[2]);

  // Une conversion lue trop tôt renvoie 0 : on la considère comme invalide.
  return raw != 0;
}

void Ms5611::compensate() {
  // Compensation du premier ordre de la fiche technique. Les calculs
  // intermédiaires exigent du 64 bits : en 32 bits, OFF et SENS débordent.
  const std::int64_t c1 = coefficients_[0];
  const std::int64_t c2 = coefficients_[1];
  const std::int64_t c3 = coefficients_[2];
  const std::int64_t c4 = coefficients_[3];
  // c5 n'apparaît pas ici : il sert à calculer l'écart de température, fait
  // dans service() au moment de la mesure pour pouvoir le lisser.
  const std::int64_t c6 = coefficients_[5];

  const std::int64_t d1 = raw_pressure_;

  // L'écart de température utilisé est la version LISSÉE : la température varie
  // lentement, son bruit n'a aucune raison de se propager dans la pression.
  // Elle est renseignée par service() à chaque mesure de température.
  const std::int64_t dt = static_cast<std::int64_t>(smoothed_dt_);

  std::int64_t temp = 2000 + ((dt * c6) >> 23);
  std::int64_t off = (c2 << 16) + ((c4 * dt) >> 7);
  std::int64_t sens = (c1 << 15) + ((c3 * dt) >> 8);

  // Compensation du SECOND ordre, applicable sous 20 °C. Elle compte vraiment
  // ici : un parapentiste voit couramment 0 à −10 °C à 3000 m au printemps, et
  // l'omettre y introduit une erreur d'altitude de plusieurs mètres.
  if (temp < 2000) {
    const std::int64_t delta = temp - 2000;
    const std::int64_t delta2 = delta * delta;

    std::int64_t t2 = (dt * dt) >> 31;
    std::int64_t off2 = (5 * delta2) / 2;
    std::int64_t sens2 = (5 * delta2) / 4;

    if (temp < -1500) {
      const std::int64_t very_cold = temp + 1500;
      const std::int64_t very_cold2 = very_cold * very_cold;
      off2 += 7 * very_cold2;
      sens2 += (11 * very_cold2) / 2;
    }

    temp -= t2;
    off -= off2;
    sens -= sens2;
  }

  const std::int64_t pressure = (((d1 * sens) >> 21) - off) >> 15;

  temperature_c_ = static_cast<float>(temp) / 100.0f;
  pressure_hpa_ = static_cast<float>(pressure) / 100.0f;  // Pa → hPa

  if (!std::isfinite(pressure_hpa_) || pressure_hpa_ < 10.0f || pressure_hpa_ > 1200.0f) {
    // Valeur non physique : on ne la propage pas.
    pressure_hpa_ = 1013.25f;
  }
}

bool Ms5611::service(Micros now) {
  if (!calibrated_) return false;

  switch (phase_) {
    case Phase::Idle: {
      // On intercale une mesure de température de temps en temps.
      const bool want_temperature =
          !have_temperature_ || pressure_since_temperature_ >= config_.temperature_every;
      if (!startConversion(want_temperature)) return false;
      phase_ =
          want_temperature ? Phase::ConvertingTemperature : Phase::ConvertingPressure;
      phase_started_ = now;
      return false;
    }

    case Phase::ConvertingTemperature: {
      if (now - phase_started_ < kTemperatureConversionUs) return false;
      std::uint32_t raw = 0;
      if (readAdc(raw)) {
        raw_temperature_ = raw;
        have_temperature_ = true;
        pressure_since_temperature_ = 0;

        // Lissage de l'écart de température, indépendant de la cadence.
        const float dt_raw =
            static_cast<float>(static_cast<std::int64_t>(raw_temperature_) -
                               (static_cast<std::int64_t>(coefficients_[4]) << 8));
        if (!smoothed_dt_valid_) {
          smoothed_dt_ = dt_raw;
          smoothed_dt_valid_ = true;
        } else {
          // Période effective entre deux mesures de température.
          const float period_s =
              static_cast<float>(config_.temperature_every) * 0.010f + 0.003f;
          const float alpha = std::exp(-period_s / config_.temperature_tau_s);
          smoothed_dt_ = alpha * smoothed_dt_ + (1.0f - alpha) * dt_raw;
        }
      }
      phase_ = Phase::Idle;
      return false;
    }

    case Phase::ConvertingPressure:
    default: {
      if (now - phase_started_ < kPressureConversionUs) return false;
      std::uint32_t raw = 0;
      const bool ok = readAdc(raw);
      phase_ = Phase::Idle;
      if (!ok) return false;

      raw_pressure_ = raw;
      ++pressure_since_temperature_;
      if (!have_temperature_) return false;  // pas de compensation possible

      compensate();
      return true;
    }
  }
}

}  // namespace vario::hal
