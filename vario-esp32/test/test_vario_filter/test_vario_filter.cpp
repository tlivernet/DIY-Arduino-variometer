#include <unity.h>

#include <cmath>

#include "core/isa.h"
#include "core/vario_filter.h"

void setUp() {}
void tearDown() {}

namespace {

constexpr float kQnh = 1013.25f;

// Inverse de la formule ISA : quelle pression correspond à une altitude ?
// Sert à fabriquer des trajectoires d'altitude exactes pour les tests.
float altitudeToPressure(float altitude_m) {
  return kQnh *
         std::pow(1.0f - altitude_m / vario::kIsaScaleM, 1.0f / vario::kIsaExponent);
}

// Rejoue une montée à vitesse verticale constante et renvoie le vario final.
float replayConstantClimb(float rate_ms, float duration_s, float sample_hz,
                          vario::Micros start_time = 0) {
  vario::VarioFilter filter;
  const vario::Micros step = static_cast<vario::Micros>(1e6f / sample_hz);
  const int samples = static_cast<int>(duration_s * sample_hz);

  filter.reset(altitudeToPressure(0.0f), kQnh, 0.0f, start_time);

  for (int i = 1; i <= samples; ++i) {
    const vario::Micros now = start_time + step * i;
    const float altitude = rate_ms * vario::toSeconds(now - start_time);
    filter.update(altitudeToPressure(altitude), kQnh, 0.0f, now);
  }
  return filter.varioMs();
}

}  // namespace

static void test_reset_starts_at_zero_vario() {
  vario::VarioFilter filter;
  filter.reset(altitudeToPressure(500.0f), kQnh, 0.0f, 0);

  TEST_ASSERT_EQUAL_FLOAT(0.0f, filter.varioMs());
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 500.0f, filter.altitudeM());
}

static void test_altitude_offset_is_applied() {
  vario::VarioFilter filter;
  filter.reset(altitudeToPressure(0.0f), kQnh, 250.0f, 0);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 250.0f, filter.altitudeM());
}

static void test_steady_climb_converges_to_true_rate() {
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.0f, replayConstantClimb(2.0f, 20.0f, 50.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -1.5f, replayConstantClimb(-1.5f, 20.0f, 50.0f));
}

// Bug 2017 n°1 : les coefficients de filtrage étaient fixes par itération de
// boucle (0,95 et 0,8), donc la réponse dépendait de la cadence d'appel. Avec
// alpha = exp(-dt/tau), la même trajectoire doit produire le même vario quelle
// que soit la fréquence d'échantillonnage.
static void test_filter_is_sample_rate_independent() {
  const float at_10hz = replayConstantClimb(2.0f, 20.0f, 10.0f);
  const float at_100hz = replayConstantClimb(2.0f, 20.0f, 100.0f);
  const float at_237hz = replayConstantClimb(2.0f, 20.0f, 237.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.05f, at_10hz, at_100hz);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, at_10hz, at_237hz);
}

// Bug 2017 n°3 : aucune garde sur Δt. Un Δt nul produisait une division par
// zéro, donc un vario NaN — et comme NaN×0,8 + x = NaN, l'état ne s'en
// remettait jamais : plus d'affichage ni de son jusqu'au redémarrage.
static void test_zero_dt_is_rejected_without_nan() {
  vario::VarioFilter filter;
  filter.reset(altitudeToPressure(0.0f), kQnh, 0.0f, 1000);

  TEST_ASSERT_FALSE(filter.update(altitudeToPressure(1.0f), kQnh, 0.0f, 1000));
  TEST_ASSERT_TRUE(std::isfinite(filter.varioMs()));
  TEST_ASSERT_TRUE(std::isfinite(filter.altitudeM()));
}

static void test_backwards_clock_is_rejected_without_nan() {
  vario::VarioFilter filter;
  filter.reset(altitudeToPressure(0.0f), kQnh, 0.0f, 1000000);

  TEST_ASSERT_FALSE(filter.update(altitudeToPressure(1.0f), kQnh, 0.0f, 500000));
  TEST_ASSERT_TRUE(std::isfinite(filter.varioMs()));
}

static void test_nan_pressure_is_rejected() {
  vario::VarioFilter filter;
  filter.reset(altitudeToPressure(0.0f), kQnh, 0.0f, 0);
  filter.update(altitudeToPressure(1.0f), kQnh, 0.0f, 100000);
  const float before = filter.varioMs();

  TEST_ASSERT_FALSE(filter.update(std::nanf(""), kQnh, 0.0f, 200000));
  TEST_ASSERT_EQUAL_FLOAT(before, filter.varioMs());
  TEST_ASSERT_TRUE(std::isfinite(filter.varioMs()));
}

static void test_huge_gap_is_rejected() {
  vario::VarioFilter filter;
  filter.reset(altitudeToPressure(0.0f), kQnh, 0.0f, 0);

  // Dix secondes de trou (tâche préemptée, sortie de veille) : la dérivée
  // serait absurde, l'échantillon doit être écarté.
  TEST_ASSERT_FALSE(filter.update(altitudeToPressure(100.0f), kQnh, 0.0f, 10000000));
  TEST_ASSERT_TRUE(std::isfinite(filter.varioMs()));
}

// Bug 2017 n°2 : micros() était stocké dans un float (mantisse de 24 bits) et
// comparé à travers son débordement à 71,6 min. Avec un temps monotone en
// int64_t, un horodatage bien au-delà de cette limite doit fonctionner
// exactement comme au démarrage.
static void test_works_far_beyond_32bit_micros_wrap() {
  // 5·10^9 µs ≈ 83 min : au-delà du débordement de l'ancien uint32_t.
  const float late = replayConstantClimb(2.0f, 20.0f, 50.0f, 5000000000LL);
  const float early = replayConstantClimb(2.0f, 20.0f, 50.0f, 0);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, early, late);

  // Et bien plus loin encore : 10^13 µs ≈ 116 jours.
  const float much_later = replayConstantClimb(2.0f, 20.0f, 50.0f, 10000000000000LL);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, early, much_later);
}

static void test_level_flight_reads_near_zero() {
  const float vario_ms = replayConstantClimb(0.0f, 10.0f, 50.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, vario_ms);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_reset_starts_at_zero_vario);
  RUN_TEST(test_altitude_offset_is_applied);
  RUN_TEST(test_steady_climb_converges_to_true_rate);
  RUN_TEST(test_filter_is_sample_rate_independent);
  RUN_TEST(test_zero_dt_is_rejected_without_nan);
  RUN_TEST(test_backwards_clock_is_rejected_without_nan);
  RUN_TEST(test_nan_pressure_is_rejected);
  RUN_TEST(test_huge_gap_is_rejected);
  RUN_TEST(test_works_far_beyond_32bit_micros_wrap);
  RUN_TEST(test_level_flight_reads_near_zero);
  return UNITY_END();
}
