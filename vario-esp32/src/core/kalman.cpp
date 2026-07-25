#include "core/kalman.h"

#include <cmath>

namespace vario {

void VarioKalman::reset(float altitude_m) {
  altitude_ = std::isfinite(altitude_m) ? altitude_m : 0.0f;
  velocity_ = 0.0f;
  bias_ = 0.0f;
  innovation_ = 0.0f;

  p_[0][0] = 4.0f;
  p_[0][1] = 0.0f;
  p_[0][2] = 0.0f;
  p_[1][0] = 0.0f;
  p_[1][1] = 1.0f;
  p_[1][2] = 0.0f;
  p_[2][0] = 0.0f;
  p_[2][1] = 0.0f;
  p_[2][2] = 0.25f;

  divergent_for_s_ = 0.0f;
  initialized_ = true;
}

void VarioKalman::symmetrize() {
  for (int i = 0; i < 3; ++i) {
    for (int j = i + 1; j < 3; ++j) {
      const float mean = 0.5f * (p_[i][j] + p_[j][i]);
      p_[i][j] = mean;
      p_[j][i] = mean;
    }
  }
}

bool VarioKalman::sane() const {
  if (!std::isfinite(altitude_) || !std::isfinite(velocity_) || !std::isfinite(bias_)) {
    return false;
  }
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (!std::isfinite(p_[i][j])) return false;
    }
  }
  // Les variances diagonales ne peuvent pas être négatives.
  return p_[0][0] >= 0.0f && p_[1][1] >= 0.0f && p_[2][2] >= 0.0f;
}

bool VarioKalman::predict(float vertical_accel_ms2, float dt_s) {
  if (!initialized_) return false;
  if (!std::isfinite(vertical_accel_ms2)) return false;
  if (!std::isfinite(dt_s) || dt_s < config_.min_dt_s || dt_s > config_.max_dt_s) {
    return false;
  }

  const float dt = dt_s;
  const float accel = vertical_accel_ms2 - bias_;

  // Propagation de l'état.
  altitude_ += velocity_ * dt + 0.5f * accel * dt * dt;
  velocity_ += accel * dt;
  // Le biais est modélisé comme constant par morceaux (marche aléatoire).

  // P ← F·P·Fᵀ + Q, avec
  //   F = [1  dt  −dt²/2]
  //       [0   1  −dt   ]
  //       [0   0   1    ]
  const float f02 = -0.5f * dt * dt;
  const float f12 = -dt;

  float fp[3][3];
  for (int j = 0; j < 3; ++j) {
    fp[0][j] = p_[0][j] + dt * p_[1][j] + f02 * p_[2][j];
    fp[1][j] = p_[1][j] + f12 * p_[2][j];
    fp[2][j] = p_[2][j];
  }
  float next[3][3];
  for (int i = 0; i < 3; ++i) {
    next[i][0] = fp[i][0] + dt * fp[i][1] + f02 * fp[i][2];
    next[i][1] = fp[i][1] + f12 * fp[i][2];
    next[i][2] = fp[i][2];
  }

  // Q sous forme « pilotée par la secousse » : mieux conditionnée qu'une forme
  // de rang 1.
  const float sa = config_.accel_psd;
  const float dt2 = dt * dt;
  const float dt3 = dt2 * dt;
  next[0][0] += sa * dt3 / 3.0f;
  next[0][1] += sa * dt2 / 2.0f;
  next[1][0] += sa * dt2 / 2.0f;
  next[1][1] += sa * dt;
  next[2][2] += config_.bias_psd * dt;

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) p_[i][j] = next[i][j];
  }
  symmetrize();

  if (!sane()) {
    ++reset_count_;
    reset(altitude_);
    return false;
  }

  // Garde de divergence. Un écart franc ET PERSISTANT avec le baro brut
  // signifie que l'état ne décrit plus la réalité : mieux vaut repartir de la
  // mesure que de s'entêter. C'est la prédiction qui porte le compteur, parce
  // que c'est elle qui connaît Δt.
  if (std::fabs(innovation_) > config_.divergence_m) {
    divergent_for_s_ += dt;
    if (divergent_for_s_ >= config_.divergence_hold_s) {
      const float last_measurement = altitude_ + innovation_;
      ++reset_count_;
      reset(last_measurement);
      return false;
    }
  } else {
    divergent_for_s_ = 0.0f;
  }

  return true;
}

bool VarioKalman::update(float altitude_m) {
  if (!initialized_) return false;
  if (!std::isfinite(altitude_m)) return false;

  innovation_ = altitude_m - altitude_;

  // Le contrôle de cohérence en fin de fonction, combiné à la garde de
  // divergence de predict(), rend impossible le bug n°3 de 2017 : un NaN qui
  // tuait silencieusement écran et son jusqu'au redémarrage.
  const float r_nominal = config_.altitude_sigma * config_.altitude_sigma;
  float r = r_nominal;

  // Adoucissement de Huber : au-delà du seuil on gonfle R plutôt que de
  // rejeter, un rejet franc créant des blocages.
  const float gate = config_.huber_sigmas * config_.altitude_sigma;
  if (std::fabs(innovation_) > gate && gate > 0.0f) {
    const float ratio = std::fabs(innovation_) / gate;
    r = r_nominal * ratio * ratio;
  }

  const float s = p_[0][0] + r;
  if (!std::isfinite(s) || s <= 0.0f) {
    ++reset_count_;
    reset(altitude_m);
    return false;
  }

  const float k0 = p_[0][0] / s;
  const float k1 = p_[1][0] / s;
  const float k2 = p_[2][0] / s;

  altitude_ += k0 * innovation_;
  velocity_ += k1 * innovation_;
  bias_ += k2 * innovation_;

  // P ← (I − K·H)·P, avec H = [1 0 0], donc P[i][j] −= K[i]·P[0][j].
  const float row0[3] = {p_[0][0], p_[0][1], p_[0][2]};
  const float k[3] = {k0, k1, k2};
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) p_[i][j] -= k[i] * row0[j];
  }
  symmetrize();

  if (!sane()) {
    ++reset_count_;
    reset(altitude_m);
    return false;
  }
  return true;
}

}  // namespace vario
