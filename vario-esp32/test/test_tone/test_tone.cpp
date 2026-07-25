#include <unity.h>

#include "core/tone_law.h"
#include "core/tone_scheduler.h"

void setUp() {}
void tearDown() {}

namespace {

using vario::ToneProfile;
using vario::ToneScheduler;

// Fait avancer le temps par pas de 10 ms et renvoie le dernier état sonore.
vario::ToneOutput advance(ToneScheduler& scheduler, float vario_ms, vario::Micros& now,
                          int duration_ms) {
  vario::ToneOutput output;
  for (int elapsed = 0; elapsed < duration_ms; elapsed += 10) {
    now += vario::fromMillis(10);
    output = scheduler.update(vario_ms, now);
  }
  return output;
}

// Compte les fronts montants du son sur une durée donnée : c'est la cadence
// de bips réellement perçue par le pilote.
int countBeeps(ToneScheduler& scheduler, float vario_ms, vario::Micros& now,
               int duration_ms) {
  int beeps = 0;
  bool was_sounding = scheduler.update(vario_ms, now).sounding;

  for (int elapsed = 0; elapsed < duration_ms; elapsed += 5) {
    now += vario::fromMillis(5);
    const bool sounding = scheduler.update(vario_ms, now).sounding;
    if (sounding && !was_sounding) ++beeps;
    was_sounding = sounding;
  }
  return beeps;
}

}  // namespace

// --- Lois de tonalité -------------------------------------------------------

static void test_classic_frequency_law() {
  // 690 Hz au vario nul, +150 Hz par m/s : les valeurs de 2017 à l'identique.
  TEST_ASSERT_EQUAL_UINT16(690, vario::toneFrequencyHz(0.0f, ToneProfile::Classic));
  TEST_ASSERT_EQUAL_UINT16(840, vario::toneFrequencyHz(1.0f, ToneProfile::Classic));
  TEST_ASSERT_EQUAL_UINT16(990, vario::toneFrequencyHz(2.0f, ToneProfile::Classic));
}

static void test_classic_frequency_is_clamped() {
  TEST_ASSERT_EQUAL_UINT16(1300, vario::toneFrequencyHz(50.0f, ToneProfile::Classic));
  TEST_ASSERT_EQUAL_UINT16(100, vario::toneFrequencyHz(-50.0f, ToneProfile::Classic));
}

static void test_classic_note_law() {
  TEST_ASSERT_EQUAL_UINT16(150, vario::toneNoteMs(0.0f, ToneProfile::Classic));
  TEST_ASSERT_EQUAL_UINT16(120, vario::toneNoteMs(1.0f, ToneProfile::Classic));
  TEST_ASSERT_EQUAL_UINT16(90, vario::toneNoteMs(2.0f, ToneProfile::Classic));
  // Saturation à 70 ms au-delà de +2,67 m/s.
  TEST_ASSERT_EQUAL_UINT16(70, vario::toneNoteMs(10.0f, ToneProfile::Classic));
}

static void test_modern_profile_stays_above_piezo_dead_zone() {
  // Un piezo a un très mauvais rendement sous 1 kHz : le profil moderne ne
  // descend jamais sous 700 Hz.
  for (float v = -10.0f; v <= 10.0f; v += 0.5f) {
    const std::uint16_t hz = vario::toneFrequencyHz(v, ToneProfile::Modern);
    TEST_ASSERT_TRUE(hz >= 700 && hz <= 2200);
  }
}

static void test_modern_profile_has_wider_cadence_range() {
  // Le profil classique ne couvre que 4,4 à 9,5 bips/s. Le profil moderne
  // doit s'étaler bien davantage entre l'entrée en ascendance et la forte
  // montée, ce qui rend le taux plus lisible à l'oreille.
  const std::uint16_t weak = vario::toneNoteMs(0.3f, ToneProfile::Modern);
  const std::uint16_t strong = vario::toneNoteMs(8.0f, ToneProfile::Modern);
  TEST_ASSERT_TRUE(weak > strong * 3);
}

static void test_laws_handle_non_finite_input() {
  const float nan_value = 0.0f / 0.0f;
  TEST_ASSERT_TRUE(vario::toneFrequencyHz(nan_value, ToneProfile::Classic) > 0);
  TEST_ASSERT_TRUE(vario::toneNoteMs(nan_value, ToneProfile::Classic) > 0);
}

// --- Machine à états du bipeur ---------------------------------------------

static void test_silent_below_threshold() {
  ToneScheduler scheduler;
  vario::Micros now = 0;
  TEST_ASSERT_FALSE(advance(scheduler, 0.0f, now, 3000).sounding);
  TEST_ASSERT_EQUAL(static_cast<int>(ToneScheduler::State::Silent),
                    static_cast<int>(scheduler.state()));
}

static void test_lead_in_is_continuous_then_beeps() {
  ToneScheduler scheduler;
  vario::Micros now = 0;

  // Franchissement du seuil : le son démarre immédiatement.
  TEST_ASSERT_TRUE(scheduler.update(2.0f, now).sounding);
  TEST_ASSERT_EQUAL(static_cast<int>(ToneScheduler::State::LeadIn),
                    static_cast<int>(scheduler.state()));

  // Pendant l'amorce, le son reste continu (~1,35 s).
  for (int elapsed = 0; elapsed < 1300; elapsed += 10) {
    now += vario::fromMillis(10);
    TEST_ASSERT_TRUE(scheduler.update(2.0f, now).sounding);
  }
  TEST_ASSERT_EQUAL(static_cast<int>(ToneScheduler::State::LeadIn),
                    static_cast<int>(scheduler.state()));

  // Passé l'amorce, on entre dans le régime de bips.
  advance(scheduler, 2.0f, now, 200);
  TEST_ASSERT_NOT_EQUAL(static_cast<int>(ToneScheduler::State::LeadIn),
                        static_cast<int>(scheduler.state()));
}

static void test_lead_in_pitch_tracks_vario() {
  ToneScheduler scheduler;
  vario::Micros now = 0;

  const std::uint16_t at_1ms = scheduler.update(1.0f, now).frequency_hz;
  now += vario::fromMillis(100);
  const std::uint16_t at_3ms = scheduler.update(3.0f, now).frequency_hz;

  TEST_ASSERT_TRUE(at_3ms > at_1ms);
}

static void test_beeps_alternate_after_lead_in() {
  ToneScheduler scheduler;
  vario::Micros now = 0;

  advance(scheduler, 2.0f, now, 1400);  // on passe l'amorce
  const int beeps = countBeeps(scheduler, 2.0f, now, 2000);

  // Loi classique à +2 m/s : note de 90 ms, pause de 45 ms, soit ~7,4 bips/s.
  TEST_ASSERT_TRUE(beeps >= 8);
  TEST_ASSERT_TRUE(beeps <= 20);
}

static void test_faster_climb_gives_faster_beeps() {
  ToneScheduler weak_scheduler;
  ToneScheduler strong_scheduler;
  vario::Micros weak_now = 0;
  vario::Micros strong_now = 0;

  advance(weak_scheduler, 0.5f, weak_now, 1400);
  advance(strong_scheduler, 4.0f, strong_now, 1400);

  const int weak = countBeeps(weak_scheduler, 0.5f, weak_now, 3000);
  const int strong = countBeeps(strong_scheduler, 4.0f, strong_now, 3000);

  TEST_ASSERT_TRUE(strong > weak);
}

static void test_sink_threshold_triggers() {
  ToneScheduler scheduler;
  vario::Micros now = 0;
  TEST_ASSERT_TRUE(advance(scheduler, -2.0f, now, 100).sounding);
}

static void test_zero_threshold_disables_alarm() {
  ToneScheduler::Config config;
  config.climb_threshold_ms = 0.0f;  // montée désactivée
  config.sink_threshold_ms = -1.1f;
  ToneScheduler scheduler(config);
  vario::Micros now = 0;

  TEST_ASSERT_FALSE(advance(scheduler, 5.0f, now, 2000).sounding);
  TEST_ASSERT_TRUE(advance(scheduler, -3.0f, now, 100).sounding);
}

// Le vario de 2017 portait ~0,3 m/s de bruit RMS pour un seuil par défaut à
// 0,3 m/s, et la machine à états n'avait aucune hystérésis : à l'arrêt, le
// seuil était franchi en permanence et chaque franchissement relançant
// l'amorce continue, l'appareil produisait des chaînes de sons de 1,35 s posé
// sur une table. C'est LE défaut que ce test verrouille.
static void test_hysteresis_prevents_chatter_at_threshold() {
  ToneScheduler::Config config;
  config.climb_threshold_ms = 0.3f;
  config.hysteresis_ms = 0.1f;
  ToneScheduler scheduler(config);
  vario::Micros now = 0;

  // Oscillation autour du seuil, dans la bande d'hystérésis : silence total.
  for (int i = 0; i < 400; ++i) {
    now += vario::fromMillis(10);
    const float noisy = (i % 2 == 0) ? 0.25f : 0.35f;
    TEST_ASSERT_FALSE(scheduler.update(noisy, now).sounding);
  }
}

static void test_clear_threshold_crossing_still_triggers() {
  ToneScheduler::Config config;
  config.climb_threshold_ms = 0.3f;
  config.hysteresis_ms = 0.1f;
  ToneScheduler scheduler(config);
  vario::Micros now = 0;

  // Franchement au-dessus de seuil + hystérésis : le son doit partir.
  TEST_ASSERT_TRUE(advance(scheduler, 0.8f, now, 100).sounding);
}

static void test_mute_silences_everything() {
  ToneScheduler::Config config;
  config.muted = true;
  ToneScheduler scheduler(config);
  vario::Micros now = 0;
  TEST_ASSERT_FALSE(advance(scheduler, 5.0f, now, 3000).sounding);
}

// Au démarrage, le biais d'accéléromètre du filtre de Kalman met quelques
// secondes à converger, ce qui produit des sons parasites. On doit pouvoir
// inhiber totalement le son pendant cette phase, puis le rendre.
static void test_inhibit_silences_then_releases() {
  ToneScheduler scheduler;
  vario::Micros now = 0;

  scheduler.config().inhibited = true;
  TEST_ASSERT_FALSE(advance(scheduler, 5.0f, now, 2000).sounding);

  scheduler.config().inhibited = false;
  TEST_ASSERT_TRUE(advance(scheduler, 5.0f, now, 100).sounding);
}

static void test_leaving_lift_goes_silent() {
  ToneScheduler scheduler;
  vario::Micros now = 0;

  advance(scheduler, 2.0f, now, 1500);
  TEST_ASSERT_FALSE(advance(scheduler, 0.0f, now, 500).sounding);
  TEST_ASSERT_EQUAL(static_cast<int>(ToneScheduler::State::Silent),
                    static_cast<int>(scheduler.state()));
}

static void test_backwards_clock_does_not_lock_up() {
  ToneScheduler scheduler;
  vario::Micros now = 1000000;
  advance(scheduler, 2.0f, now, 1500);

  // Horloge qui recule brutalement. On n'exige pas que le son reprenne sur
  // l'appel suivant — la machine peut légitimement se trouver dans une
  // demi-période de pause — mais qu'elle ne reste pas figée.
  scheduler.update(2.0f, 0);

  now = 0;
  TEST_ASSERT_TRUE(countBeeps(scheduler, 2.0f, now, 1000) >= 2);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_classic_frequency_law);
  RUN_TEST(test_classic_frequency_is_clamped);
  RUN_TEST(test_classic_note_law);
  RUN_TEST(test_modern_profile_stays_above_piezo_dead_zone);
  RUN_TEST(test_modern_profile_has_wider_cadence_range);
  RUN_TEST(test_laws_handle_non_finite_input);
  RUN_TEST(test_silent_below_threshold);
  RUN_TEST(test_lead_in_is_continuous_then_beeps);
  RUN_TEST(test_lead_in_pitch_tracks_vario);
  RUN_TEST(test_beeps_alternate_after_lead_in);
  RUN_TEST(test_faster_climb_gives_faster_beeps);
  RUN_TEST(test_sink_threshold_triggers);
  RUN_TEST(test_zero_threshold_disables_alarm);
  RUN_TEST(test_hysteresis_prevents_chatter_at_threshold);
  RUN_TEST(test_clear_threshold_crossing_still_triggers);
  RUN_TEST(test_mute_silences_everything);
  RUN_TEST(test_inhibit_silences_then_releases);
  RUN_TEST(test_leaving_lift_goes_silent);
  RUN_TEST(test_backwards_clock_does_not_lock_up);
  return UNITY_END();
}
