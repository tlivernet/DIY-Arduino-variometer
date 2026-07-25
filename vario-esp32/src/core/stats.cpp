#include "core/stats.h"

#include <cmath>

namespace vario {

void StatsAccumulator::begin(std::uint32_t start_unix, float altitude_m) {
  const float altitude = std::isfinite(altitude_m) ? altitude_m : 0.0f;

  stats_ = FlightStats{};
  stats_.start_unix = start_unix;
  stats_.alt_max_m = altitude;
  stats_.alt_min_m = altitude;
  stats_.valid = true;
  running_ = true;
}

void StatsAccumulator::sample(float altitude_m, float vario_ms, float dt_s) {
  if (!running_) return;
  if (!std::isfinite(altitude_m) || !std::isfinite(vario_ms)) return;
  if (!std::isfinite(dt_s) || dt_s <= 0.0f) return;

  if (altitude_m > stats_.alt_max_m) stats_.alt_max_m = altitude_m;
  if (altitude_m < stats_.alt_min_m) stats_.alt_min_m = altitude_m;

  if (vario_ms > stats_.best_climb_ms) stats_.best_climb_ms = vario_ms;
  if (vario_ms < stats_.worst_sink_ms) stats_.worst_sink_ms = vario_ms;

  // Gain cumulé : intégration de la vitesse verticale, hors zone morte.
  if (vario_ms > config_.gain_deadband_ms) {
    stats_.cumulative_gain_m += vario_ms * dt_s;
  }
}

void StatsAccumulator::end(std::uint32_t end_unix) {
  if (!running_) return;
  running_ = false;
  stats_.duration_s = (end_unix > stats_.start_unix) ? end_unix - stats_.start_unix : 0;
}

}  // namespace vario
