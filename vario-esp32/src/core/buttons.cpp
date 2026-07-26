#include "core/buttons.h"

namespace vario {

namespace {
constexpr int kCount = static_cast<int>(Buttons::Id::Count);
}

Buttons::Event Buttons::update(const bool raw[kCount], Micros now, Id& which) {
  which = Id::Up;

  if (!initialized_) {
    initialized_ = true;
    for (int i = 0; i < kCount; ++i) {
      states_[i].raw_pressed = isPressed(raw[i]);
      states_[i].pressed = states_[i].raw_pressed;
      states_[i].changed_at = now;
      states_[i].pressed_at = now;
      states_[i].last_repeat = now;
      // Un bouton déjà enfoncé au démarrage ne doit produire aucun événement,
      // ni au maintien ni au relâchement.
      states_[i].ignore_until_release = states_[i].pressed;
    }
    return Event::None;
  }

  for (int i = 0; i < kCount; ++i) {
    State& s = states_[i];

    // Horloge qui recule : on resynchronise plutôt que de rester bloqué.
    if (now < s.changed_at) s.changed_at = now;
    if (now < s.pressed_at) s.pressed_at = now;
    if (now < s.last_repeat) s.last_repeat = now;

    const bool reading = isPressed(raw[i]);

    // Anti-rebond : tout changement de la lecture brute relance le compteur, et
    // l'état stable ne bouge qu'après debounce_s de calme.
    if (reading != s.raw_pressed) {
      s.raw_pressed = reading;
      s.changed_at = now;
      continue;
    }
    if (toSeconds(now - s.changed_at) < config_.debounce_s) continue;

    if (reading && !s.pressed) {
      // Front d'appui confirmé.
      s.pressed = true;
      s.pressed_at = now;
      s.last_repeat = now;
      s.long_press_sent = false;
      s.ignore_until_release = false;
      continue;
    }

    if (!reading && s.pressed) {
      // Relâchement. On n'émet Press que si l'appui long n'a pas déjà été
      // signalé, pour ne pas produire deux événements sur un même geste.
      s.pressed = false;
      const bool ignored = s.ignore_until_release;
      s.ignore_until_release = false;
      if (!ignored && !s.long_press_sent) {
        which = static_cast<Id>(i);
        return Event::Press;
      }
      continue;
    }

    if (!s.pressed) continue;
    if (s.ignore_until_release) continue;

    const float held_s = toSeconds(now - s.pressed_at);

    if (!s.long_press_sent && held_s >= config_.long_press_s) {
      s.long_press_sent = true;
      s.last_repeat = now;
      which = static_cast<Id>(i);
      return Event::LongPress;
    }

    // Répétition automatique, seulement sur les flèches : répéter « valider »
    // n'aurait aucun sens et provoquerait des validations en rafale.
    if (static_cast<Id>(i) != Id::Ok && held_s >= config_.repeat_delay_s &&
        toSeconds(now - s.last_repeat) >= config_.repeat_interval_s) {
      s.last_repeat = now;
      which = static_cast<Id>(i);
      return Event::Repeat;
    }
  }

  return Event::None;
}

bool Buttons::held(Id id) const {
  const int index = static_cast<int>(id);
  if (index < 0 || index >= kCount) return false;
  return states_[index].pressed;
}

float Buttons::heldFor(Id id, Micros now) const {
  const int index = static_cast<int>(id);
  if (index < 0 || index >= kCount) return 0.0f;
  if (!states_[index].pressed) return 0.0f;
  const float held_s = toSeconds(now - states_[index].pressed_at);
  return held_s > 0.0f ? held_s : 0.0f;
}

bool Buttons::powerOffRequested(Micros now) const {
  if (!held(Id::Up) || !held(Id::Down)) return false;
  return heldFor(Id::Up, now) >= config_.long_press_s &&
         heldFor(Id::Down, now) >= config_.long_press_s;
}

}  // namespace vario
