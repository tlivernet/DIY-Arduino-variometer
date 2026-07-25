#include <unity.h>

#include <initializer_list>

#include "core/flight_fsm.h"

void setUp() {}
void tearDown() {}

namespace {

// Maintient une altitude constante pendant une durée donnée, à 1 Hz, et
// renvoie true si un atterrissage a été détecté.
bool holdAltitude(vario::FlightDetector& detector, float altitude_m, vario::Micros& now,
                  int seconds) {
  bool landed = false;
  for (int i = 0; i < seconds; ++i) {
    now += vario::kMicrosPerSecond;
    if (detector.update(altitude_m, now).landed) landed = true;
  }
  return landed;
}

// Fait décoller le détecteur depuis une altitude au sol.
bool takeOff(vario::FlightDetector& detector, float ground_m, vario::Micros& now) {
  detector.reset(ground_m, now);
  now += vario::kMicrosPerSecond;
  return detector.update(ground_m + 10.0f, now).took_off;
}

}  // namespace

static void test_starts_idle() {
  vario::FlightDetector detector;
  vario::Micros now = 0;
  detector.reset(500.0f, now);
  TEST_ASSERT_EQUAL(static_cast<int>(vario::FlightDetector::State::Idle),
                    static_cast<int>(detector.state()));
  TEST_ASSERT_FALSE(detector.flying());
}

static void test_takeoff_detected_beyond_trigger() {
  vario::FlightDetector detector;
  vario::Micros now = 0;
  TEST_ASSERT_TRUE(takeOff(detector, 500.0f, now));
  TEST_ASSERT_TRUE(detector.flying());
}

static void test_small_excursion_does_not_trigger_takeoff() {
  vario::FlightDetector detector;
  vario::Micros now = 0;
  detector.reset(500.0f, now);

  // Le seuil porte sur l'écart à une ligne de base réarmée périodiquement :
  // ce qui compte est donc l'amplitude TOTALE de l'excursion, pas la
  // demi-amplitude. Une oscillation de ±1,5 m (3 m crête à crête) reste sous
  // le seuil de 4 m quelle que soit la phase du réarmement.
  for (int i = 0; i < 120; ++i) {
    now += vario::kMicrosPerSecond;
    const float altitude = 500.0f + ((i % 2 == 0) ? 1.5f : -1.5f);
    TEST_ASSERT_FALSE(detector.update(altitude, now).took_off);
  }
  TEST_ASSERT_FALSE(detector.flying());
}

static void test_slow_drift_does_not_trigger_takeoff() {
  vario::FlightDetector detector;
  vario::Micros now = 0;
  detector.reset(500.0f, now);

  // Dérive de 0,2 m/s : 3 m par fenêtre de réarmement de 15 s, donc sous le
  // seuil. C'est le rôle du réarmement périodique de la ligne de base.
  for (int i = 1; i <= 120; ++i) {
    now += vario::kMicrosPerSecond;
    TEST_ASSERT_FALSE(
        detector.update(500.0f + 0.2f * static_cast<float>(i), now).took_off);
  }
}

// Bug 2017 n°4, le plus grave. La ligne de base était un `int`, donc
// l'affectation depuis l'altitude flottante tronquait. En atterrissant à une
// altitude dont la partie fractionnaire dépassait 0,75 m — environ un
// atterrissage sur quatre — la bande de ±0,75 m autour de la base tronquée
// excluait l'altitude réelle, la condition d'arrêt ne pouvait jamais être
// vraie et LE CHRONO DE VOL NE S'ARRÊTAIT JAMAIS.
static void test_landing_detected_at_fractional_altitude() {
  // Toutes ces parties fractionnaires cassaient l'ancienne implémentation.
  const float altitudes[] = {1234.9f, 500.76f, 999.99f, 1500.8f, 42.95f};

  for (float altitude : altitudes) {
    vario::FlightDetector detector;
    vario::Micros now = 0;
    TEST_ASSERT_TRUE(takeOff(detector, altitude - 50.0f, now));

    // On se stabilise à l'altitude d'atterrissage, puis on attend.
    now += vario::kMicrosPerSecond;
    detector.update(altitude, now);

    TEST_ASSERT_TRUE(holdAltitude(detector, altitude, now, 20));
    TEST_ASSERT_FALSE(detector.flying());
  }
}

static void test_landing_needs_full_stop_duration() {
  vario::FlightDetector detector;
  vario::Micros now = 0;
  TEST_ASSERT_TRUE(takeOff(detector, 500.0f, now));

  now += vario::kMicrosPerSecond;
  detector.update(510.0f, now);

  // 10 s d'immobilité : insuffisant, il en faut 15.
  TEST_ASSERT_FALSE(holdAltitude(detector, 510.0f, now, 10));
  TEST_ASSERT_TRUE(detector.flying());

  // 6 s de plus : l'atterrissage doit être confirmé.
  TEST_ASSERT_TRUE(holdAltitude(detector, 510.0f, now, 6));
}

static void test_movement_resets_landing_timer() {
  vario::FlightDetector detector;
  vario::Micros now = 0;
  TEST_ASSERT_TRUE(takeOff(detector, 500.0f, now));

  // On alterne 10 s d'immobilité et un écart franc : jamais d'atterrissage.
  for (int cycle = 0; cycle < 5; ++cycle) {
    TEST_ASSERT_FALSE(holdAltitude(detector, 510.0f, now, 10));
    now += vario::kMicrosPerSecond;
    TEST_ASSERT_FALSE(detector.update(530.0f, now).landed);
  }
  TEST_ASSERT_TRUE(detector.flying());
}

// Bug 2017 n°5 : après un atterrissage, chrono_start et chrono étaient tous
// deux non nuls, donc aucun des deux blocs de test ne s'exécutait plus. Si la
// case de statistiques suivante était occupée, l'appareil n'enregistrait
// PLUS AUCUN VOL — silencieusement, au bout de 8 vols. La machine à états doit
// pouvoir enchaîner indéfiniment.
static void test_many_consecutive_flights_are_all_detected() {
  vario::FlightDetector detector;
  vario::Micros now = 0;
  detector.reset(500.0f, now);

  int flights = 0;
  for (int i = 0; i < 25; ++i) {
    const float ground = 500.0f + static_cast<float>(i) * 7.3f;

    now += vario::kMicrosPerSecond;
    const bool took_off = detector.update(ground + 30.0f, now).took_off;
    TEST_ASSERT_TRUE(took_off);

    now += vario::kMicrosPerSecond;
    detector.update(ground, now);
    TEST_ASSERT_TRUE(holdAltitude(detector, ground, now, 20));

    ++flights;
  }
  TEST_ASSERT_EQUAL_INT(25, flights);
}

// La logique est temporelle, plus comptée en itérations de boucle : le même
// profil doit être détecté quelle que soit la cadence d'appel.
static void test_detection_is_call_rate_independent() {
  for (int hz : {1, 5, 20}) {
    vario::FlightDetector detector;
    vario::Micros now = 0;
    detector.reset(500.0f, now);

    const vario::Micros step = vario::kMicrosPerSecond / hz;
    bool took_off = false;

    // Montée à 1 m/s pendant 10 s.
    for (int i = 1; i <= 10 * hz; ++i) {
      now += step;
      const float altitude = 500.0f + 1.0f * vario::toSeconds(now);
      if (detector.update(altitude, now).took_off) took_off = true;
    }
    TEST_ASSERT_TRUE(took_off);

    // Puis 20 s posé.
    bool landed = false;
    const float ground = 510.0f;
    for (int i = 1; i <= 20 * hz; ++i) {
      now += step;
      if (detector.update(ground, now).landed) landed = true;
    }
    TEST_ASSERT_TRUE(landed);
  }
}

static void test_reset_does_not_emit_takeoff() {
  vario::FlightDetector detector;
  vario::Micros now = 0;
  detector.reset(500.0f, now);

  // Un Tare ou un changement de QNH décale brutalement l'altitude ; après
  // reset() cela ne doit pas passer pour un décollage.
  detector.reset(1200.0f, now);
  now += vario::kMicrosPerSecond;
  TEST_ASSERT_FALSE(detector.update(1200.0f, now).took_off);
  TEST_ASSERT_FALSE(detector.flying());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_idle);
  RUN_TEST(test_takeoff_detected_beyond_trigger);
  RUN_TEST(test_small_excursion_does_not_trigger_takeoff);
  RUN_TEST(test_slow_drift_does_not_trigger_takeoff);
  RUN_TEST(test_landing_detected_at_fractional_altitude);
  RUN_TEST(test_landing_needs_full_stop_duration);
  RUN_TEST(test_movement_resets_landing_timer);
  RUN_TEST(test_many_consecutive_flights_are_all_detected);
  RUN_TEST(test_detection_is_call_rate_independent);
  RUN_TEST(test_reset_does_not_emit_takeoff);
  return UNITY_END();
}
