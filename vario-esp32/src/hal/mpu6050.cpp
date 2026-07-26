#include "hal/mpu6050.h"

#include <Arduino.h>
#include <Wire.h>

namespace vario::hal {
namespace {

constexpr std::uint8_t kRegSmplrtDiv = 0x19;
constexpr std::uint8_t kRegConfig = 0x1A;
constexpr std::uint8_t kRegGyroConfig = 0x1B;
constexpr std::uint8_t kRegAccelConfig = 0x1C;
constexpr std::uint8_t kRegAccelXoutH = 0x3B;
constexpr std::uint8_t kRegPwrMgmt1 = 0x6B;
constexpr std::uint8_t kRegWhoAmI = 0x75;

// Pleines échelles retenues : ±4 g couvre largement une sortie de fermeture,
// ±500 °/s couvre une spirale engagée.
constexpr std::uint8_t kAccelFs4g = 0x08;     // AFS_SEL = 1
constexpr std::uint8_t kGyroFs500dps = 0x08;  // FS_SEL = 1
constexpr float kAccelLsbPerG = 8192.0f;
constexpr float kGyroLsbPerDps = 65.5f;
constexpr float kDegToRad = 0.01745329252f;

std::int16_t beInt16(const std::uint8_t* p) {
  return static_cast<std::int16_t>((static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

}  // namespace

bool Mpu6050::writeRegister(std::uint8_t reg, std::uint8_t value) {
  Wire.beginTransmission(config_.address);
  Wire.write(reg);
  Wire.write(value);
  if (Wire.endTransmission() != 0) {
    ++error_count_;
    return false;
  }
  return true;
}

bool Mpu6050::readRegisters(std::uint8_t reg, std::uint8_t* out, std::size_t count) {
  Wire.beginTransmission(config_.address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
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

bool Mpu6050::begin() {
  std::uint8_t who = 0;
  if (!readRegisters(kRegWhoAmI, &who, 1)) return false;
  // 0x68 pour un MPU6050, 0x70 ou 0x71 pour un MPU6500/9250 : on accepte la
  // famille, les registres utilisés ici étant communs.
  if (who != 0x68 && who != 0x70 && who != 0x71 && who != 0x73) return false;

  // Sortie de veille, horloge asservie au gyromètre X (plus stable que
  // l'oscillateur interne).
  if (!writeRegister(kRegPwrMgmt1, 0x01)) return false;
  delay(50);

  if (!writeRegister(kRegConfig, config_.dlpf)) return false;
  if (!writeRegister(kRegSmplrtDiv, config_.sample_rate_divider)) return false;
  if (!writeRegister(kRegGyroConfig, kGyroFs500dps)) return false;
  if (!writeRegister(kRegAccelConfig, kAccelFs4g)) return false;

  return true;
}

Vec3 Mpu6050::remap(float a, float b, float c) const {
  const float raw[3] = {a, b, c};
  const AxisMap& m = config_.axes;
  return Vec3{m.sign[0] * raw[m.source[0]], m.sign[1] * raw[m.source[1]],
              m.sign[2] * raw[m.source[2]]};
}

bool Mpu6050::read(Vec3& accel_mps2, Vec3& gyro_rps) {
  std::uint8_t buffer[14];
  if (!readRegisters(kRegAccelXoutH, buffer, sizeof(buffer))) return false;

  const float ax = static_cast<float>(beInt16(&buffer[0])) / kAccelLsbPerG * kGravity;
  const float ay = static_cast<float>(beInt16(&buffer[2])) / kAccelLsbPerG * kGravity;
  const float az = static_cast<float>(beInt16(&buffer[4])) / kAccelLsbPerG * kGravity;

  temperature_c_ = static_cast<float>(beInt16(&buffer[6])) / 340.0f + 36.53f;

  const float gx = static_cast<float>(beInt16(&buffer[8])) / kGyroLsbPerDps * kDegToRad;
  const float gy = static_cast<float>(beInt16(&buffer[10])) / kGyroLsbPerDps * kDegToRad;
  const float gz = static_cast<float>(beInt16(&buffer[12])) / kGyroLsbPerDps * kDegToRad;

  accel_mps2 = remap(ax, ay, az);
  gyro_rps = remap(gx, gy, gz);
  return true;
}

bool Mpu6050::measureGyroBias(int samples, Vec3& bias_rps) {
  if (samples <= 0) return false;

  Vec3 sum;
  int taken = 0;
  for (int i = 0; i < samples; ++i) {
    Vec3 accel;
    Vec3 gyro;
    if (read(accel, gyro)) {
      sum = sum + gyro;
      ++taken;
    }
    delay(2);
  }
  if (taken == 0) return false;

  bias_rps = sum * (1.0f / static_cast<float>(taken));
  return true;
}

}  // namespace vario::hal
