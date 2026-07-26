#pragma once

#include <cstdint>

#include "core/clock.h"

namespace vario {

// Machine à états des trois boutons.
//
// Choix de trois boutons plutôt qu'un encodeur rotatif, révisé par rapport au
// montage de 2017 : avec des gants on ne distingue pas les crans d'un encodeur,
// et son axe est le pire chemin d'entrée d'eau et de poussière de l'appareil.
// Tous les instruments de vol libre du commerce utilisent des boutons.
//
// Bénéfice annexe, décisif ici : plus de décodage en quadrature par
// interruption, donc une logique purement testable — impossible avec la
// bibliothèque Encoder de l'ancien projet.
//
// Le menu de 2017 se réduisait déjà à haut / bas / valider, la transposition est
// directe.
class Buttons {
 public:
  enum class Id : std::uint8_t { Up = 0, Ok = 1, Down = 2, Count = 3 };

  enum class Event : std::uint8_t {
    None,
    Press,      // appui bref relâché
    LongPress,  // maintenu au-delà du seuil
    Repeat,     // répétition automatique pendant le maintien
  };

  struct Config {
    // Anti-rebond. 30 ms est confortable pour un bouton silicone.
    float debounce_s = 0.030f;
    // Seuil de l'appui long.
    float long_press_s = 0.600f;
    // Première répétition après ce délai de maintien, puis toutes les
    // repeat_interval_s. Sert à faire défiler vite une valeur numérique.
    float repeat_delay_s = 0.500f;
    float repeat_interval_s = 0.120f;
    // Les boutons sont actifs à l'état bas (tirage au plus).
    bool active_low = true;
  };

  Buttons() = default;
  explicit Buttons(const Config& config) : config_(config) {}

  // À appeler à cadence régulière (10 ms convient) avec l'état électrique brut
  // des trois entrées. Renvoie l'événement du bouton concerné, s'il y en a un.
  //
  // raw[i] est la lecture de la broche : true = niveau haut.
  Event update(const bool raw[static_cast<int>(Id::Count)], Micros now, Id& which);

  // true si le bouton est actuellement enfoncé (après anti-rebond).
  bool held(Id id) const;

  // Durée de maintien courante, en secondes. 0 si relâché.
  float heldFor(Id id, Micros now) const;

  // true si les deux boutons haut et bas sont maintenus ensemble au-delà du
  // seuil d'appui long : combinaison réservée à l'extinction.
  bool powerOffRequested(Micros now) const;

  const Config& config() const { return config_; }

 private:
  struct State {
    bool pressed = false;      // état stable après anti-rebond
    bool raw_pressed = false;  // dernière lecture interprétée
    Micros changed_at = 0;     // dernier changement de la lecture brute
    Micros pressed_at = 0;     // début de l'appui stable
    Micros last_repeat = 0;    // dernière répétition émise
    bool long_press_sent = false;

    // Bouton déjà enfoncé au tout premier appel : on l'ignore jusqu'à son
    // relâchement. Sans cela, un appareil rangé dans une poche avec un bouton
    // comprimé émettrait un clic au moment où on le ressort, et naviguerait
    // tout seul dans le menu. L'utilisateur n'a rien appuyé pendant cette
    // session, il a seulement cessé d'appuyer.
    bool ignore_until_release = false;
  };

  bool isPressed(bool raw) const { return config_.active_low ? !raw : raw; }

  Config config_;
  State states_[static_cast<int>(Id::Count)];
  bool initialized_ = false;
};

}  // namespace vario
