#include "core/tone_scheduler.h"

#include <cmath>

namespace vario {

bool ToneScheduler::triggered(float vario_ms) const {
  if (!std::isfinite(vario_ms)) return false;

  const bool climb_enabled = config_.climb_threshold_ms != 0.0f;
  const bool sink_enabled = config_.sink_threshold_ms != 0.0f;
  const float h = std::fabs(config_.hysteresis_ms);

  if (climb_enabled && vario_ms > config_.climb_threshold_ms + h) return true;
  if (sink_enabled && vario_ms < config_.sink_threshold_ms - h) return true;
  return false;
}

bool ToneScheduler::released(float vario_ms) const {
  if (!std::isfinite(vario_ms)) return true;

  const bool climb_enabled = config_.climb_threshold_ms != 0.0f;
  const bool sink_enabled = config_.sink_threshold_ms != 0.0f;

  // On relâche seulement une fois redescendu sous le seuil diminué de
  // l'hystérésis, d'où une bande morte de 2·h autour du seuil.
  const bool in_climb = climb_enabled && vario_ms > config_.climb_threshold_ms;
  const bool in_sink = sink_enabled && vario_ms < config_.sink_threshold_ms;
  return !in_climb && !in_sink;
}

void ToneScheduler::enter(State state, Micros now) {
  state_ = state;
  state_since_ = now;
}

ToneOutput ToneScheduler::update(float vario_ms, Micros now) {
  if (!initialized_) {
    initialized_ = true;
    state_since_ = now;
  }

  // Une horloge qui recule ne doit pas figer la machine à états.
  if (now < state_since_) state_since_ = now;

  if (config_.muted || config_.inhibited) {
    state_ = State::Silent;
    state_since_ = now;
    return ToneOutput{};
  }

  const Micros elapsed = now - state_since_;

  switch (state_) {
    case State::Silent:
      if (triggered(vario_ms)) {
        enter(State::LeadIn, now);
        return ToneOutput{true, toneFrequencyHz(vario_ms, config_.profile)};
      }
      return ToneOutput{};

    case State::LeadIn:
      if (released(vario_ms)) {
        enter(State::Silent, now);
        return ToneOutput{};
      }
      if (elapsed >= fromMillis(config_.lead_in_ms)) {
        // Fin de l'amorce : on fige note et hauteur pour le premier bip.
        note_ms_ = toneNoteMs(vario_ms, config_.profile);
        note_frequency_hz_ = toneFrequencyHz(vario_ms, config_.profile);
        enter(State::Note, now);
        return ToneOutput{true, note_frequency_hz_};
      }
      // Pendant l'amorce, la hauteur suit le vario en direct.
      return ToneOutput{true, toneFrequencyHz(vario_ms, config_.profile)};

    case State::Note:
      if (released(vario_ms)) {
        enter(State::Silent, now);
        return ToneOutput{};
      }
      if (elapsed >= fromMillis(note_ms_)) {
        enter(State::Pause, now);
        return ToneOutput{};
      }
      return ToneOutput{true, note_frequency_hz_};

    case State::Pause:
    default:
      if (released(vario_ms)) {
        enter(State::Silent, now);
        return ToneOutput{};
      }
      // Pause = moitié de la note précédente (rapport 2:1 d'origine).
      if (elapsed >= fromMillis(note_ms_ / 2)) {
        note_ms_ = toneNoteMs(vario_ms, config_.profile);
        note_frequency_hz_ = toneFrequencyHz(vario_ms, config_.profile);
        enter(State::Note, now);
        return ToneOutput{true, note_frequency_hz_};
      }
      return ToneOutput{};
  }
}

}  // namespace vario
