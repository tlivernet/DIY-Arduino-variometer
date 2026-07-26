#include <unity.h>

#include "core/buttons.h"

void setUp() {}
void tearDown() {}

namespace {

using vario::Buttons;

constexpr int kCount = static_cast<int>(Buttons::Id::Count);
constexpr int kUp = static_cast<int>(Buttons::Id::Up);
constexpr int kOk = static_cast<int>(Buttons::Id::Ok);
constexpr int kDown = static_cast<int>(Buttons::Id::Down);

// Les boutons sont actifs à l'état bas : relâché = niveau haut.
struct Pins {
  bool level[kCount] = {true, true, true};

  void press(int index) { level[index] = false; }
  void release(int index) { level[index] = true; }
};

// Fait avancer le temps par pas de 10 ms et collecte les événements.
struct Recorder {
  int presses[kCount] = {};
  int long_presses[kCount] = {};
  int repeats[kCount] = {};

  void run(Buttons& buttons, Pins& pins, vario::Micros& now, int duration_ms) {
    for (int elapsed = 0; elapsed < duration_ms; elapsed += 10) {
      now += vario::fromMillis(10);
      Buttons::Id which = Buttons::Id::Up;
      const Buttons::Event event = buttons.update(pins.level, now, which);
      const int index = static_cast<int>(which);
      switch (event) {
        case Buttons::Event::Press:
          ++presses[index];
          break;
        case Buttons::Event::LongPress:
          ++long_presses[index];
          break;
        case Buttons::Event::Repeat:
          ++repeats[index];
          break;
        case Buttons::Event::None:
          break;
      }
    }
  }
};

}  // namespace

static void test_short_press_emits_one_press_on_release() {
  Buttons buttons;
  Pins pins;
  Recorder recorder;
  vario::Micros now = 0;

  recorder.run(buttons, pins, now, 100);  // repos initial
  TEST_ASSERT_EQUAL_INT(0, recorder.presses[kOk]);

  pins.press(kOk);
  recorder.run(buttons, pins, now, 150);
  // Tant que le bouton est maintenu (et sous le seuil long), rien n'est émis.
  TEST_ASSERT_EQUAL_INT(0, recorder.presses[kOk]);

  pins.release(kOk);
  recorder.run(buttons, pins, now, 100);
  TEST_ASSERT_EQUAL_INT(1, recorder.presses[kOk]);
}

// Un rebond de contact ne doit pas produire plusieurs appuis.
static void test_contact_bounce_is_filtered() {
  Buttons buttons;
  Pins pins;
  Recorder recorder;
  vario::Micros now = 0;
  recorder.run(buttons, pins, now, 100);

  // Rebond : cinq allers-retours de 10 ms, sous le seuil d'anti-rebond de 30 ms.
  for (int i = 0; i < 5; ++i) {
    pins.press(kOk);
    recorder.run(buttons, pins, now, 10);
    pins.release(kOk);
    recorder.run(buttons, pins, now, 10);
  }
  // Puis un appui franc.
  pins.press(kOk);
  recorder.run(buttons, pins, now, 100);
  pins.release(kOk);
  recorder.run(buttons, pins, now, 100);

  TEST_ASSERT_EQUAL_INT(1, recorder.presses[kOk]);
}

static void test_long_press_emits_once_and_suppresses_press() {
  Buttons buttons;
  Pins pins;
  Recorder recorder;
  vario::Micros now = 0;
  recorder.run(buttons, pins, now, 100);

  pins.press(kOk);
  recorder.run(buttons, pins, now, 1500);

  TEST_ASSERT_EQUAL_INT(1, recorder.long_presses[kOk]);

  pins.release(kOk);
  recorder.run(buttons, pins, now, 100);

  // Un même geste ne doit pas produire à la fois LongPress et Press.
  TEST_ASSERT_EQUAL_INT(0, recorder.presses[kOk]);
  TEST_ASSERT_EQUAL_INT(1, recorder.long_presses[kOk]);
}

// La répétition automatique sert à faire défiler vite une valeur numérique.
static void test_arrow_repeats_while_held() {
  Buttons buttons;
  Pins pins;
  Recorder recorder;
  vario::Micros now = 0;
  recorder.run(buttons, pins, now, 100);

  pins.press(kUp);
  recorder.run(buttons, pins, now, 2000);
  pins.release(kUp);

  // Après 500 ms de délai, une répétition toutes les 120 ms sur ~1,5 s.
  TEST_ASSERT_TRUE(recorder.repeats[kUp] >= 8);
  TEST_ASSERT_TRUE(recorder.repeats[kUp] <= 16);
}

// Mais « valider » ne doit JAMAIS se répéter : des validations en rafale
// traverseraient le menu tout seul.
static void test_ok_never_repeats() {
  Buttons buttons;
  Pins pins;
  Recorder recorder;
  vario::Micros now = 0;
  recorder.run(buttons, pins, now, 100);

  pins.press(kOk);
  recorder.run(buttons, pins, now, 5000);

  TEST_ASSERT_EQUAL_INT(0, recorder.repeats[kOk]);
}

static void test_buttons_are_independent() {
  Buttons buttons;
  Pins pins;
  Recorder recorder;
  vario::Micros now = 0;
  recorder.run(buttons, pins, now, 100);

  pins.press(kUp);
  recorder.run(buttons, pins, now, 100);
  pins.release(kUp);
  recorder.run(buttons, pins, now, 100);

  pins.press(kDown);
  recorder.run(buttons, pins, now, 100);
  pins.release(kDown);
  recorder.run(buttons, pins, now, 100);

  TEST_ASSERT_EQUAL_INT(1, recorder.presses[kUp]);
  TEST_ASSERT_EQUAL_INT(1, recorder.presses[kDown]);
  TEST_ASSERT_EQUAL_INT(0, recorder.presses[kOk]);
}

static void test_held_and_held_for() {
  Buttons buttons;
  Pins pins;
  Recorder recorder;
  vario::Micros now = 0;
  recorder.run(buttons, pins, now, 100);

  TEST_ASSERT_FALSE(buttons.held(Buttons::Id::Up));

  pins.press(kUp);
  recorder.run(buttons, pins, now, 200);
  TEST_ASSERT_TRUE(buttons.held(Buttons::Id::Up));
  TEST_ASSERT_TRUE(buttons.heldFor(Buttons::Id::Up, now) > 0.1f);

  pins.release(kUp);
  recorder.run(buttons, pins, now, 100);
  TEST_ASSERT_FALSE(buttons.held(Buttons::Id::Up));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, buttons.heldFor(Buttons::Id::Up, now));
}

// Haut + bas maintenus ensemble : extinction. Combinaison volontairement
// difficile à déclencher par accident dans une poche.
static void test_power_off_combination() {
  Buttons buttons;
  Pins pins;
  Recorder recorder;
  vario::Micros now = 0;
  recorder.run(buttons, pins, now, 100);

  pins.press(kUp);
  recorder.run(buttons, pins, now, 100);
  TEST_ASSERT_FALSE(buttons.powerOffRequested(now));

  pins.press(kDown);
  recorder.run(buttons, pins, now, 200);
  // Les deux sont enfoncés mais pas encore assez longtemps.
  TEST_ASSERT_FALSE(buttons.powerOffRequested(now));

  recorder.run(buttons, pins, now, 800);
  TEST_ASSERT_TRUE(buttons.powerOffRequested(now));

  pins.release(kDown);
  recorder.run(buttons, pins, now, 100);
  TEST_ASSERT_FALSE(buttons.powerOffRequested(now));
}

// Un bouton déjà enfoncé au démarrage ne doit pas produire d'événement
// fantôme : sinon un appareil rangé avec un bouton comprimé se réveillerait en
// naviguant tout seul dans le menu.
static void test_button_held_at_startup_emits_nothing() {
  Buttons buttons;
  Pins pins;
  Recorder recorder;
  vario::Micros now = 0;

  pins.press(kOk);
  recorder.run(buttons, pins, now, 100);
  TEST_ASSERT_EQUAL_INT(0, recorder.presses[kOk]);

  pins.release(kOk);
  recorder.run(buttons, pins, now, 100);
  TEST_ASSERT_EQUAL_INT(0, recorder.presses[kOk]);
}

static void test_active_high_wiring_is_supported() {
  Buttons::Config config;
  config.active_low = false;
  Buttons buttons(config);
  Recorder recorder;
  vario::Micros now = 0;

  // Câblage inverse : relâché = niveau bas.
  Pins pins;
  pins.level[kUp] = false;
  pins.level[kOk] = false;
  pins.level[kDown] = false;
  recorder.run(buttons, pins, now, 100);

  pins.level[kOk] = true;
  recorder.run(buttons, pins, now, 100);
  pins.level[kOk] = false;
  recorder.run(buttons, pins, now, 100);

  TEST_ASSERT_EQUAL_INT(1, recorder.presses[kOk]);
}

static void test_backwards_clock_is_tolerated() {
  Buttons buttons;
  Pins pins;
  Recorder recorder;
  vario::Micros now = vario::fromMillis(100000);
  recorder.run(buttons, pins, now, 100);

  pins.press(kOk);
  Buttons::Id which = Buttons::Id::Up;
  buttons.update(pins.level, 0, which);  // horloge qui recule

  now = vario::fromMillis(1000);
  recorder.run(buttons, pins, now, 200);
  pins.release(kOk);
  recorder.run(buttons, pins, now, 200);

  // La machine doit rester fonctionnelle.
  TEST_ASSERT_EQUAL_INT(1, recorder.presses[kOk]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_short_press_emits_one_press_on_release);
  RUN_TEST(test_contact_bounce_is_filtered);
  RUN_TEST(test_long_press_emits_once_and_suppresses_press);
  RUN_TEST(test_arrow_repeats_while_held);
  RUN_TEST(test_ok_never_repeats);
  RUN_TEST(test_buttons_are_independent);
  RUN_TEST(test_held_and_held_for);
  RUN_TEST(test_power_off_combination);
  RUN_TEST(test_button_held_at_startup_emits_nothing);
  RUN_TEST(test_active_high_wiring_is_supported);
  RUN_TEST(test_backwards_clock_is_tolerated);
  return UNITY_END();
}
