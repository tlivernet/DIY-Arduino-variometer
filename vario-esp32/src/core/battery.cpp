#include "core/battery.h"

#include <cmath>

namespace vario {

float batteryFraction(float volts) {
  if (!std::isfinite(volts)) return 0.0f;

  const float fraction = (volts - kBatteryEmptyV) / (kBatteryFullV - kBatteryEmptyV);
  if (fraction < 0.0f) return 0.0f;
  if (fraction > 1.0f) return 1.0f;
  return fraction;
}

int batteryPixels(float volts, int max_pixels) {
  if (max_pixels <= 0) return 0;

  const float exact = batteryFraction(volts) * static_cast<float>(max_pixels);
  const int pixels = static_cast<int>(std::lround(exact));

  if (pixels < 0) return 0;
  if (pixels > max_pixels) return max_pixels;
  return pixels;
}

}  // namespace vario
