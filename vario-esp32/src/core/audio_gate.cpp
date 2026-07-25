#include "core/audio_gate.h"

#include <cmath>

namespace vario {

void AudioGate::onAligned(Micros now) {
  if (state_ == State::BootMute) resettle(now);
}

void AudioGate::resettle(Micros now) {
  state_ = State::Settling;
  state_since_ = now;
  quiet_ = false;
  quiet_since_ = now;
  just_went_live_ = false;
}

bool AudioGate::update(float velocity_ms, float innovation_m, Micros now) {
  just_went_live_ = false;

  if (now < state_since_) state_since_ = now;
  if (now < quiet_since_) quiet_since_ = now;

  switch (state_) {
    case State::BootMute:
      // On attend l'alignement, signalé par onAligned().
      return true;

    case State::Settling: {
      const float elapsed = toSeconds(now - state_since_);

      const bool velocity_calm =
          std::isfinite(velocity_ms) && std::fabs(velocity_ms) < config_.max_velocity_ms;
      if (velocity_calm) {
        if (!quiet_) {
          quiet_ = true;
          quiet_since_ = now;
        }
      } else {
        quiet_ = false;
        quiet_since_ = now;
      }

      const bool innovation_calm = std::isfinite(innovation_m) &&
                                   std::fabs(innovation_m) < config_.max_innovation_m;
      const bool quiet_long_enough =
          quiet_ && toSeconds(now - quiet_since_) >= config_.quiet_s;

      const bool criteria_met =
          elapsed >= config_.min_settle_s && innovation_calm && quiet_long_enough;
      const bool timed_out = elapsed >= config_.max_settle_s;

      if (criteria_met || timed_out) {
        state_ = State::Live;
        state_since_ = now;
        just_went_live_ = true;
        return false;
      }
      return true;
    }

    case State::Live:
    default:
      return false;
  }
}

}  // namespace vario
