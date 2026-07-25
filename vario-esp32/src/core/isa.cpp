#include "core/isa.h"

#include <cmath>

namespace vario {

float clampQnh(float qnh_hpa) {
  if (!std::isfinite(qnh_hpa)) return kQnhDefaultHpa;
  if (qnh_hpa < kQnhMinHpa) return kQnhMinHpa;
  if (qnh_hpa > kQnhMaxHpa) return kQnhMaxHpa;
  return qnh_hpa;
}

float pressureToAltitude(float qnh_hpa, float pressure_hpa) {
  const float qnh = clampQnh(qnh_hpa);

  // Une pression non finie ou non physique ne doit pas contaminer le calcul.
  // 10 hPa correspond à ~31 km : bien au-delà de tout usage en vol libre,
  // mais suffisant pour garder la fonction totalement définie.
  if (!std::isfinite(pressure_hpa) || pressure_hpa < 10.0f) {
    return 0.0f;
  }

  const float ratio = pressure_hpa / qnh;
  const float altitude = kIsaScaleM * (1.0f - std::pow(ratio, kIsaExponent));

  return std::isfinite(altitude) ? altitude : 0.0f;
}

}  // namespace vario
