#include "core/flight_fsm.h"

#include <cmath>

namespace vario {

void FlightDetector::reset(float altitude_m, Micros now) {
  state_ = State::Idle;
  baseline_m_ = std::isfinite(altitude_m) ? altitude_m : 0.0f;
  baseline_since_ = now;
  in_band_ = false;
  in_band_since_ = now;
  initialized_ = true;
}

FlightDetector::Event FlightDetector::update(float altitude_m, Micros now) {
  Event event;

  if (!std::isfinite(altitude_m)) return event;

  if (!initialized_) {
    reset(altitude_m, now);
    return event;
  }

  // Horloge qui recule : on resynchronise plutôt que de rester bloqué.
  if (now < baseline_since_) baseline_since_ = now;
  if (now < in_band_since_) in_band_since_ = now;

  const float deviation = std::fabs(altitude_m - baseline_m_);

  if (state_ == State::Idle) {
    if (deviation > config_.start_trigger_m) {
      state_ = State::Flying;
      baseline_m_ = altitude_m;
      baseline_since_ = now;
      in_band_ = false;
      in_band_since_ = now;
      event.took_off = true;
      return event;
    }

    // Réarmement périodique : la détection porte sur un écart accumulé depuis
    // le dernier réarmement, pas depuis le démarrage.
    if (toSeconds(now - baseline_since_) >= config_.rearm_s) {
      baseline_m_ = altitude_m;
      baseline_since_ = now;
    }
    return event;
  }

  // État Flying.
  if (deviation <= config_.stop_band_m) {
    if (!in_band_) {
      in_band_ = true;
      in_band_since_ = now;
    } else if (toSeconds(now - in_band_since_) >= config_.stop_s) {
      state_ = State::Idle;
      baseline_m_ = altitude_m;
      baseline_since_ = now;
      in_band_ = false;
      in_band_since_ = now;
      event.landed = true;
    }
  } else {
    // On bouge encore : on suit l'altitude et on repart de zéro.
    in_band_ = false;
    in_band_since_ = now;
    baseline_m_ = altitude_m;
  }

  return event;
}

}  // namespace vario
