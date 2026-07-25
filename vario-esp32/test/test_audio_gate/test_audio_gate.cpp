#include <unity.h>

#include "core/audio_gate.h"

void setUp() {}
void tearDown() {}

namespace {

using vario::AudioGate;

// Fait avancer le temps par pas de 10 ms en gardant l'appareil calme.
bool advanceCalm(AudioGate& gate, vario::Micros& now, int duration_ms) {
  bool muted = true;
  for (int elapsed = 0; elapsed < duration_ms; elapsed += 10) {
    now += vario::fromMillis(10);
    muted = gate.update(0.0f, 0.0f, now);
  }
  return muted;
}

}  // namespace

static void test_starts_muted_before_alignment() {
  AudioGate gate;
  vario::Micros now = 0;

  TEST_ASSERT_TRUE(gate.muted());
  TEST_ASSERT_TRUE(advanceCalm(gate, now, 10000));
  TEST_ASSERT_EQUAL(static_cast<int>(AudioGate::State::BootMute),
                    static_cast<int>(gate.state()));
}

static void test_unmutes_after_alignment_and_settling() {
  AudioGate gate;
  vario::Micros now = 0;

  gate.onAligned(now);
  TEST_ASSERT_EQUAL(static_cast<int>(AudioGate::State::Settling),
                    static_cast<int>(gate.state()));

  // Le délai minimal de stabilisation doit être respecté.
  TEST_ASSERT_TRUE(advanceCalm(gate, now, 1000));

  // Puis le son doit être rendu.
  TEST_ASSERT_FALSE(advanceCalm(gate, now, 1500));
  TEST_ASSERT_EQUAL(static_cast<int>(AudioGate::State::Live),
                    static_cast<int>(gate.state()));
}

// L'événement « le son vient d'être armé » ne doit se produire qu'une seule
// fois : c'est lui qui déclenche le petit motif de confirmation.
static void test_just_went_live_fires_exactly_once() {
  AudioGate gate;
  vario::Micros now = 0;
  gate.onAligned(now);

  int fired = 0;
  for (int i = 0; i < 1000; ++i) {
    now += vario::fromMillis(10);
    gate.update(0.0f, 0.0f, now);
    if (gate.justWentLive()) ++fired;
  }
  TEST_ASSERT_EQUAL_INT(1, fired);
}

// Le vario qui s'agite pendant la stabilisation doit retarder le son : c'est
// exactement la phase de convergence du biais qui produisait les sons parasites
// à l'allumage sur le projet de référence.
static void test_unstable_velocity_delays_unmute() {
  AudioGate gate;
  vario::Micros now = 0;
  gate.onAligned(now);

  // 3 s d'excursions de ±2 m/s : sous le plafond dur de 4 s, le son doit
  // rester coupé.
  for (int i = 0; i < 300; ++i) {
    now += vario::fromMillis(10);
    const float velocity = (i % 2 == 0) ? 2.0f : -2.0f;
    TEST_ASSERT_TRUE(gate.update(velocity, 0.0f, now));
  }
}

// Mais le plafond dur doit finir par rendre le son : un allumage en voiture ou
// en télésiège ne doit pas laisser l'appareil muet indéfiniment.
static void test_hard_timeout_always_unmutes() {
  AudioGate gate;
  vario::Micros now = 0;
  gate.onAligned(now);

  bool muted = true;
  for (int i = 0; i < 600; ++i) {  // 6 s d'agitation continue
    now += vario::fromMillis(10);
    const float velocity = (i % 2 == 0) ? 3.0f : -3.0f;
    muted = gate.update(velocity, 1.0f, now);
  }
  TEST_ASSERT_FALSE(muted);
  TEST_ASSERT_EQUAL(static_cast<int>(AudioGate::State::Live),
                    static_cast<int>(gate.state()));
}

static void test_large_innovation_delays_unmute() {
  AudioGate gate;
  vario::Micros now = 0;
  gate.onAligned(now);

  // Vario calme mais résidu barométrique important : le filtre n'est pas
  // encore d'accord avec le capteur, on attend.
  for (int i = 0; i < 300; ++i) {
    now += vario::fromMillis(10);
    TEST_ASSERT_TRUE(gate.update(0.0f, 2.0f, now));
  }
}

static void test_resettle_mutes_again() {
  AudioGate gate;
  vario::Micros now = 0;
  gate.onAligned(now);
  advanceCalm(gate, now, 3000);
  TEST_ASSERT_FALSE(gate.muted());

  // Après une garde de divergence du Kalman, on repasse en stabilisation.
  gate.resettle(now);
  TEST_ASSERT_TRUE(gate.muted());
  TEST_ASSERT_EQUAL(static_cast<int>(AudioGate::State::Settling),
                    static_cast<int>(gate.state()));

  TEST_ASSERT_FALSE(advanceCalm(gate, now, 3000));
}

static void test_total_mute_never_exceeds_worst_case() {
  AudioGate gate;
  vario::Micros now = 0;

  // Alignement à 1,5 s (durée typique de la fenêtre d'alignement).
  advanceCalm(gate, now, 1500);
  gate.onAligned(now);

  // Plafond dur à 4 s après l'alignement : au total 5,5 s au pire, soit moins
  // que l'écran d'accueil.
  const bool muted = advanceCalm(gate, now, 4100);
  TEST_ASSERT_FALSE(muted);
}

static void test_backwards_clock_is_tolerated() {
  AudioGate gate;
  vario::Micros now = vario::fromMillis(100000);
  gate.onAligned(now);

  gate.update(0.0f, 0.0f, 0);  // horloge qui recule

  now = vario::fromMillis(5000);
  TEST_ASSERT_FALSE(gate.update(0.0f, 0.0f, now));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_muted_before_alignment);
  RUN_TEST(test_unmutes_after_alignment_and_settling);
  RUN_TEST(test_just_went_live_fires_exactly_once);
  RUN_TEST(test_unstable_velocity_delays_unmute);
  RUN_TEST(test_hard_timeout_always_unmutes);
  RUN_TEST(test_large_innovation_delays_unmute);
  RUN_TEST(test_resettle_mutes_again);
  RUN_TEST(test_total_mute_never_exceeds_worst_case);
  RUN_TEST(test_backwards_clock_is_tolerated);
  return UNITY_END();
}
