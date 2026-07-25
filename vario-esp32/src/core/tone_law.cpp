#include "core/tone_law.h"

#include <cmath>

namespace {

float clampf(float v, float lo, float hi) {
  if (!std::isfinite(v)) return lo;
  return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace

namespace vario {

std::uint16_t toneFrequencyHz(float vario_ms, ToneProfile profile) {
  switch (profile) {
    case ToneProfile::Modern:
      // 1200 Hz au seuil, ~320 Hz par m/s, bornée sur la zone de bon
      // rendement d'un piezo.
      return static_cast<std::uint16_t>(
          clampf(1200.0f + 320.0f * vario_ms, 700.0f, 2200.0f));

    case ToneProfile::Classic:
    default:
      return static_cast<std::uint16_t>(
          clampf(690.0f + 150.0f * vario_ms, 100.0f, 1300.0f));
  }
}

std::uint16_t toneNoteMs(float vario_ms, ToneProfile profile) {
  switch (profile) {
    case ToneProfile::Modern: {
      // Cadence de 1 bip/s à l'entrée dans l'ascendance jusqu'à 10 bips/s
      // en forte montée. La note occupe 45 % de la période.
      const float cadence_hz = clampf(1.0f + 1.1f * std::fabs(vario_ms), 1.0f, 10.0f);
      const float period_ms = 1000.0f / cadence_hz;
      return static_cast<std::uint16_t>(clampf(period_ms * 0.45f, 40.0f, 500.0f));
    }

    case ToneProfile::Classic:
    default:
      return static_cast<std::uint16_t>(clampf(150.0f - 30.0f * vario_ms, 70.0f, 150.0f));
  }
}

}  // namespace vario
