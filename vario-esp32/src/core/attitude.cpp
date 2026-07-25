#include "core/attitude.h"

namespace vario {

void AttitudeFilter::align(const Vec3& mean_accel, const Vec3& mean_gyro) {
  if (!isFinite(mean_accel) || !isFinite(mean_gyro)) return;

  // À l'arrêt l'accéléromètre mesure +g vers le haut, donc le bas est l'opposé.
  down_ = normalized(mean_accel * -1.0f, Vec3{0.0f, 0.0f, 1.0f});
  gyro_bias_ = clampNorm(mean_gyro, config_.max_bias_rps);
  aligned_ = true;
}

Vec3 AttitudeFilter::centrifugalAccel(const Vec3& omega) const {
  if (!config_.centrifugal_compensation) return Vec3{};

  // Taux de rotation autour de la verticale terrestre. down_ pointe vers le
  // bas, donc la composante « autour du haut » est l'opposé.
  const Vec3 up = down_ * -1.0f;
  const float turn_rate = dot(omega, up);

  // Direction d'avancement : l'axe X du boîtier, projeté à l'horizontale.
  // Si le boîtier est monté quasi vertical (axe X aligné sur la verticale),
  // la projection dégénère et on renonce à compenser plutôt que d'inventer
  // une direction.
  const Vec3 body_forward{1.0f, 0.0f, 0.0f};
  const Vec3 horizontal = body_forward - down_ * dot(body_forward, down_);
  if (norm(horizontal) < 0.1f) return Vec3{};
  const Vec3 forward = normalized(horizontal);

  // a = Ω × v, avec Ω le long de la verticale et v horizontale vers l'avant.
  return cross(up * turn_rate, forward * config_.trim_speed_ms);
}

void AttitudeFilter::update(const Vec3& accel_mps2, const Vec3& gyro_rps, float dt_s) {
  last_accepted_ = false;

  if (!isFinite(accel_mps2) || !isFinite(gyro_rps)) return;
  if (!std::isfinite(dt_s) || dt_s <= 0.0f || dt_s > 1.0f) return;

  const Vec3 omega = gyro_rps - gyro_bias_;

  // 1. Propagation par le gyromètre.
  //
  // Un vecteur fixe dans le repère terrestre paraît tourner à −ω dans le
  // repère du boîtier, d'où le signe négatif.
  down_ = normalized(down_ - cross(omega, down_) * dt_s, down_);

  // 2. Référence de gravité, corrigée de la force centrifuge.
  const Vec3 reference = accel_mps2 - centrifugalAccel(omega);
  const float reference_norm = norm(reference);

  // 3. Fenêtre d'acceptation.
  if (std::fabs(reference_norm - kGravity) > config_.accel_gate_ms2) return;
  if (norm(omega) > config_.gyro_gate_rps) return;

  // 4. Recalage. measured est le bas mesuré ; on fait glisser l'estimation
  // vers lui. La normalisation absorbe la composante radiale, ce qui rend
  // cette forme équivalente au premier ordre à une rotation pure.
  const Vec3 measured = normalized(reference * -1.0f, down_);
  const Vec3 error = cross(down_, measured);

  down_ = normalized(down_ + (measured - down_) * (config_.kp * dt_s), down_);

  // L'erreur d'assiette résiduelle est alignée sur la composante observable
  // du biais gyrométrique : l'intégrer estime donc ce biais.
  gyro_bias_ = clampNorm(gyro_bias_ + error * (config_.ki * dt_s), config_.max_bias_rps);

  last_accepted_ = true;
}

float AttitudeFilter::verticalAccel(const Vec3& accel_mps2) const {
  if (!isFinite(accel_mps2)) return 0.0f;
  const float value = -dot(accel_mps2, down_) - kGravity;
  return std::isfinite(value) ? value : 0.0f;
}

}  // namespace vario
