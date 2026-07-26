#include "core/lk8ex1.h"

#include <cmath>
#include <cstdio>

namespace vario {

std::uint8_t nmeaChecksum(const char* payload) {
  if (payload == nullptr) return 0;
  std::uint8_t checksum = 0;
  for (const char* p = payload; *p != '\0'; ++p) {
    checksum ^= static_cast<std::uint8_t>(*p);
  }
  return checksum;
}

std::size_t formatLk8ex1(char* buf, std::size_t size, const Lk8ex1Data& data) {
  if (buf == nullptr || size < 32) {
    if (buf != nullptr && size > 0) buf[0] = '\0';
    return 0;
  }

  const long pressure =
      (data.pressure_valid && std::isfinite(data.pressure_pa) && data.pressure_pa > 0.0f)
          ? std::lround(data.pressure_pa)
          : 999999L;

  // On laisse volontairement l'altitude indisponible : le téléphone la calcule
  // depuis la pression avec son propre QNH.
  constexpr long kAltitudeUnavailable = 99999L;

  const long vario_cms =
      std::isfinite(data.vario_ms) ? std::lround(data.vario_ms * 100.0f) : 0L;

  const long temperature = (data.temperature_valid && std::isfinite(data.temperature_c))
                               ? std::lround(data.temperature_c)
                               : 99L;

  int percent = data.battery_percent;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  // Corps de la trame, sans le « $ » ni le « * » qui sont exclus du XOR.
  char payload[64];
  const int payload_len =
      std::snprintf(payload, sizeof(payload), "LK8EX1,%ld,%ld,%ld,%ld,%d,", pressure,
                    kAltitudeUnavailable, vario_cms, temperature, 1000 + percent);
  if (payload_len <= 0 || static_cast<std::size_t>(payload_len) >= sizeof(payload)) {
    buf[0] = '\0';
    return 0;
  }

  const int written = std::snprintf(buf, size, "$%s*%02X\r\n", payload,
                                    static_cast<unsigned>(nmeaChecksum(payload)));
  if (written <= 0 || static_cast<std::size_t>(written) >= size) {
    buf[0] = '\0';
    return 0;
  }
  return static_cast<std::size_t>(written);
}

}  // namespace vario
