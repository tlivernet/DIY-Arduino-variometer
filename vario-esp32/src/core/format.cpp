#include "core/format.h"

#include <cmath>
#include <cstdio>

namespace vario {

VarioText formatVario(float vario_ms, float deadband_ms) {
  VarioText text;

  if (!std::isfinite(vario_ms)) return text;

  if (vario_ms >= deadband_ms) {
    text.sign = '+';
  } else if (vario_ms <= -deadband_ms) {
    text.sign = '-';
  }

  // Un seul arrondi, en dixièmes : pas de report possible entre les deux
  // composantes.
  long tenths = std::lround(std::fabs(vario_ms) * 10.0f);
  if (tenths < 0) tenths = 0;

  text.whole = static_cast<int>(tenths / 10);
  text.tenth = static_cast<int>(tenths % 10);
  return text;
}

std::size_t formatVarioTo(char* buf, std::size_t size, float vario_ms,
                          float deadband_ms) {
  if (buf == nullptr || size == 0) return 0;

  const VarioText text = formatVario(vario_ms, deadband_ms);
  const int written =
      std::snprintf(buf, size, "%c%d.%d", text.sign, text.whole, text.tenth);

  if (written < 0) {
    buf[0] = '\0';
    return 0;
  }
  return (static_cast<std::size_t>(written) < size) ? static_cast<std::size_t>(written)
                                                    : size - 1;
}

}  // namespace vario
