#include <unity.h>

#include <cmath>

#include "core/stats.h"

void setUp() {}
void tearDown() {}

static void test_duration_is_computed() {
  vario::StatsAccumulator accumulator;
  accumulator.begin(1000, 500.0f);
  accumulator.end(1750);
  TEST_ASSERT_EQUAL_UINT32(750, accumulator.stats().duration_s);
}

// Les extrema d'altitude étaient stockés en `int` : toute altitude flottante
// était tronquée, faussant le minimum et le maximum en sens opposés.
static void test_altitude_extremes_keep_fractional_precision() {
  vario::StatsAccumulator accumulator;
  accumulator.begin(0, 1000.0f);

  accumulator.sample(1234.9f, 1.0f, 0.02f);
  accumulator.sample(812.35f, -1.0f, 0.02f);

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1234.9f, accumulator.stats().alt_max_m);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 812.35f, accumulator.stats().alt_min_m);
}

// txchutemax/txchutemin étaient des uint8_t de dixièmes de m/s : au-delà de
// 25,5 m/s la valeur débordait silencieusement. Les noms étaient de plus
// inversés par rapport à leur contenu réel.
static void test_climb_and_sink_extremes_do_not_saturate() {
  vario::StatsAccumulator accumulator;
  accumulator.begin(0, 1000.0f);

  accumulator.sample(1000.0f, 30.0f, 0.02f);  // au-delà de l'ancienne limite
  accumulator.sample(1000.0f, -28.5f, 0.02f);

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f, accumulator.stats().best_climb_ms);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -28.5f, accumulator.stats().worst_sink_ms);
}

static void test_cumulative_gain_integrates_climb() {
  vario::StatsAccumulator accumulator;
  accumulator.begin(0, 1000.0f);

  // 1 m/s pendant 10 s, échantillonné à 50 Hz → 10 m de gain.
  for (int i = 0; i < 500; ++i) {
    accumulator.sample(1000.0f, 1.0f, 0.02f);
  }
  TEST_ASSERT_FLOAT_WITHIN(0.2f, 10.0f, accumulator.stats().cumulative_gain_m);
}

// Bug 2017 n°9 : le cumulé ajoutait la variation d'altitude BRUTE à ~30 Hz dès
// que le vario lissé était positif. Avec ~4 cm de bruit sur l'altitude, il
// intégrait ce bruit en continu et surestimait massivement le gain. La zone
// morte règle le problème.
static void test_noise_does_not_inflate_cumulative_gain() {
  vario::StatsAccumulator accumulator;
  accumulator.begin(0, 1000.0f);

  // 60 s de bruit à ±0,1 m/s autour de zéro, sous la zone morte de 0,15.
  for (int i = 0; i < 3000; ++i) {
    const float noise = (i % 2 == 0) ? 0.1f : -0.1f;
    accumulator.sample(1000.0f + noise, noise, 0.02f);
  }
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, accumulator.stats().cumulative_gain_m);
}

// cumul_alt passait par un `(int)` puis un uint16_t : au-delà de 32767 m la
// valeur réapparaissait comme un nombre non signé énorme.
static void test_large_cumulative_gain_does_not_overflow() {
  vario::StatsAccumulator accumulator;
  accumulator.begin(0, 1000.0f);

  // 3 m/s pendant 4 heures → 43 200 m, bien au-delà de l'ancienne limite.
  for (int i = 0; i < 14400 * 10; ++i) {
    accumulator.sample(1000.0f, 3.0f, 0.1f);
  }

  const float gain = accumulator.stats().cumulative_gain_m;
  TEST_ASSERT_TRUE(std::isfinite(gain));
  TEST_ASSERT_TRUE(gain > 40000.0f);
}

static void test_sink_does_not_reduce_cumulative_gain() {
  vario::StatsAccumulator accumulator;
  accumulator.begin(0, 1000.0f);

  for (int i = 0; i < 100; ++i) accumulator.sample(1000.0f, 2.0f, 0.1f);   // +20 m
  for (int i = 0; i < 100; ++i) accumulator.sample(1000.0f, -2.0f, 0.1f);  // ignoré

  TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, accumulator.stats().cumulative_gain_m);
}

static void test_samples_ignored_before_begin() {
  vario::StatsAccumulator accumulator;
  accumulator.sample(1000.0f, 5.0f, 0.1f);
  TEST_ASSERT_FALSE(accumulator.running());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, accumulator.stats().cumulative_gain_m);
}

static void test_non_finite_samples_are_ignored() {
  vario::StatsAccumulator accumulator;
  accumulator.begin(0, 1000.0f);

  const float nan_value = std::nanf("");
  accumulator.sample(nan_value, 1.0f, 0.02f);
  accumulator.sample(1000.0f, nan_value, 0.02f);
  accumulator.sample(1000.0f, 1.0f, nan_value);
  accumulator.sample(1000.0f, 1.0f, 0.0f);

  TEST_ASSERT_TRUE(std::isfinite(accumulator.stats().cumulative_gain_m));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, accumulator.stats().cumulative_gain_m);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, accumulator.stats().alt_max_m);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_duration_is_computed);
  RUN_TEST(test_altitude_extremes_keep_fractional_precision);
  RUN_TEST(test_climb_and_sink_extremes_do_not_saturate);
  RUN_TEST(test_cumulative_gain_integrates_climb);
  RUN_TEST(test_noise_does_not_inflate_cumulative_gain);
  RUN_TEST(test_large_cumulative_gain_does_not_overflow);
  RUN_TEST(test_sink_does_not_reduce_cumulative_gain);
  RUN_TEST(test_samples_ignored_before_begin);
  RUN_TEST(test_non_finite_samples_are_ignored);
  return UNITY_END();
}
