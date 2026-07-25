#pragma once

#include <cstdint>

namespace vario {

// Temps monotone en microsecondes.
//
// Le code de 2017 stockait micros() dans un float : au-delà de 2^24 µs (17 s
// d'uptime) la mantisse ne représentait plus les microsecondes exactement, et
// après une heure la quantification atteignait 512 µs sur des intervalles de
// 33 000 µs. Il comparait de plus les valeurs à travers le débordement des
// 71,6 minutes de micros(), ce qui produisait un Δt négatif.
//
// int64_t supprime les deux problèmes : ~292 000 ans de plage, résolution
// exacte à la microseconde.
using Micros = std::int64_t;

constexpr Micros kMicrosPerSecond = 1000000;

inline constexpr float toSeconds(Micros dt) {
  return static_cast<float>(dt) / static_cast<float>(kMicrosPerSecond);
}

inline constexpr Micros fromMillis(std::int64_t ms) {
  return ms * 1000;
}

}  // namespace vario
