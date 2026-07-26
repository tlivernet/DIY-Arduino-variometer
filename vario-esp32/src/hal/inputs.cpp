#include "hal/inputs.h"

#include <Arduino.h>

#include <cmath>

namespace vario::hal {

void ButtonInputs::begin() {
  for (int i = 0; i < 3; ++i) pinMode(pins_[i], INPUT_PULLUP);
}

void ButtonInputs::read(bool out[3]) const {
  for (int i = 0; i < 3; ++i) out[i] = digitalRead(pins_[i]) != LOW;
}

void BatterySense::begin() {
  pinMode(pin_, INPUT);
  analogSetPinAttenuation(pin_, ADC_11db);
}

float BatterySense::readVolts(int samples) {
  if (samples <= 0) samples = 1;

  std::uint32_t total_mv = 0;
  for (int i = 0; i < samples; ++i) total_mv += analogReadMilliVolts(pin_);

  const float pin_volts =
      static_cast<float>(total_mv) / static_cast<float>(samples) / 1000.0f;
  return pin_volts * divider_ratio_;
}

float BatterySense::smoothedVolts(float dt_s) {
  const float measured = readVolts();
  if (!std::isfinite(measured)) return smoothed_v_;

  if (!initialized_) {
    smoothed_v_ = measured;
    initialized_ = true;
    return smoothed_v_;
  }
  if (!std::isfinite(dt_s) || dt_s <= 0.0f) return smoothed_v_;

  constexpr float kTauS = 20.0f;
  const float alpha = std::exp(-dt_s / kTauS);
  smoothed_v_ = alpha * smoothed_v_ + (1.0f - alpha) * measured;
  return smoothed_v_;
}

}  // namespace vario::hal
