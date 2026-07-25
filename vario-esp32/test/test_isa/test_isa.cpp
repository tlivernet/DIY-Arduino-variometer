#include <unity.h>

#include <cmath>

#include "core/isa.h"

void setUp() {}
void tearDown() {}

static void test_altitude_zero_at_reference_pressure() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, vario::pressureToAltitude(1013.25f, 1013.25f));
}

static void test_altitude_known_value() {
  // 900 hPa sous un QNH de 1013,25 hPa ≈ 988,6 m en atmosphère standard.
  TEST_ASSERT_FLOAT_WITHIN(5.0f, 988.6f, vario::pressureToAltitude(1013.25f, 900.0f));
}

static void test_altitude_increases_when_pressure_drops() {
  const float low = vario::pressureToAltitude(1013.25f, 1000.0f);
  const float high = vario::pressureToAltitude(1013.25f, 950.0f);
  TEST_ASSERT_TRUE(high > low);
}

// Bug 2017 n°7 : le réglage QNH n'était pas borné. À 0 hPa, pow(p/0, 0.1903)
// valait inf, l'altitude devenait NaN, et le NaN se propageait définitivement
// dans le vario — écran et son morts jusqu'au redémarrage.
static void test_qnh_is_clamped() {
  TEST_ASSERT_EQUAL_FLOAT(vario::kQnhMinHpa, vario::clampQnh(0.0f));
  TEST_ASSERT_EQUAL_FLOAT(vario::kQnhMinHpa, vario::clampQnh(-500.0f));
  TEST_ASSERT_EQUAL_FLOAT(vario::kQnhMaxHpa, vario::clampQnh(5000.0f));
  TEST_ASSERT_EQUAL_FLOAT(1013.25f, vario::clampQnh(1013.25f));
}

static void test_qnh_zero_does_not_produce_nan() {
  const float altitude = vario::pressureToAltitude(0.0f, 1013.25f);
  TEST_ASSERT_TRUE(std::isfinite(altitude));
}

static void test_non_finite_inputs_are_handled() {
  const float nan_value = std::nanf("");
  TEST_ASSERT_TRUE(std::isfinite(vario::pressureToAltitude(nan_value, 1013.25f)));
  TEST_ASSERT_TRUE(std::isfinite(vario::pressureToAltitude(1013.25f, nan_value)));
  TEST_ASSERT_EQUAL_FLOAT(vario::kQnhDefaultHpa, vario::clampQnh(nan_value));
}

static void test_absurd_pressure_is_handled() {
  TEST_ASSERT_TRUE(std::isfinite(vario::pressureToAltitude(1013.25f, 0.0f)));
  TEST_ASSERT_TRUE(std::isfinite(vario::pressureToAltitude(1013.25f, -100.0f)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_altitude_zero_at_reference_pressure);
  RUN_TEST(test_altitude_known_value);
  RUN_TEST(test_altitude_increases_when_pressure_drops);
  RUN_TEST(test_qnh_is_clamped);
  RUN_TEST(test_qnh_zero_does_not_produce_nan);
  RUN_TEST(test_non_finite_inputs_are_handled);
  RUN_TEST(test_absurd_pressure_is_handled);
  return UNITY_END();
}
