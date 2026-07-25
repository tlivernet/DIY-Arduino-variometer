#include "core/vario_filter.h"

#include <cmath>

#include "core/isa.h"

namespace vario {

void Ema::reset(float value) {
  value_ = std::isfinite(value) ? value : 0.0f;
  initialized_ = true;
}

float Ema::update(float value, float dt_s) {
  if (!std::isfinite(value)) return value_;

  if (!initialized_) {
    reset(value);
    return value_;
  }

  if (tau_s_ <= 0.0f || dt_s <= 0.0f) {
    value_ = value;
    return value_;
  }

  const float alpha = std::exp(-dt_s / tau_s_);
  value_ = alpha * value_ + (1.0f - alpha) * value;

  // Ceinture et bretelles : si l'état devient non fini malgré les gardes
  // amont, on repart de la mesure plutôt que de propager un NaN à vie.
  if (!std::isfinite(value_)) value_ = value;

  return value_;
}

VarioFilter::VarioFilter() : VarioFilter(Config{}) {}

VarioFilter::VarioFilter(const Config& config)
    : config_(config), pressure_(config.pressure_tau_s), vario_(config.vario_tau_s) {}

void VarioFilter::reset(float pressure_hpa, float qnh_hpa, float altitude_offset_m,
                        Micros now) {
  pressure_.reset(pressure_hpa);
  vario_.reset(0.0f);
  last_update_ = now;
  altitude_m_ = pressureToAltitude(qnh_hpa, pressure_.value()) + altitude_offset_m;
  vario_ms_ = 0.0f;
}

bool VarioFilter::update(float pressure_hpa, float qnh_hpa, float altitude_offset_m,
                         Micros now) {
  if (!std::isfinite(pressure_hpa)) return false;

  if (!pressure_.initialized()) {
    reset(pressure_hpa, qnh_hpa, altitude_offset_m, now);
    return true;
  }

  const float dt_s = toSeconds(now - last_update_);
  if (!(dt_s >= config_.min_dt_s) || dt_s > config_.max_dt_s) {
    // Δt nul, négatif (horloge qui recule) ou trop grand : on ignore
    // l'échantillon. L'état reste cohérent et le vario reste fini.
    return false;
  }

  const float previous_altitude = altitude_m_;

  pressure_.update(pressure_hpa, dt_s);
  altitude_m_ = pressureToAltitude(qnh_hpa, pressure_.value()) + altitude_offset_m;

  const float instantaneous_rate = (altitude_m_ - previous_altitude) / dt_s;
  vario_ms_ = vario_.update(instantaneous_rate, dt_s);

  last_update_ = now;
  return true;
}

}  // namespace vario
