#include <unity.h>

#include <cmath>

#include "core/battery.h"

void setUp() {}
void tearDown() {}

static void test_full_and_empty_endpoints() {
  TEST_ASSERT_EQUAL_INT(8, vario::batteryPixels(4.20f));
  TEST_ASSERT_EQUAL_INT(0, vario::batteryPixels(3.35f));
}

static void test_midpoint_is_half() {
  TEST_ASSERT_EQUAL_INT(4, vario::batteryPixels(3.775f));
}

static void test_monotonic() {
  int previous = -1;
  for (float volts = 3.30f; volts <= 4.25f; volts += 0.01f) {
    const int pixels = vario::batteryPixels(volts);
    TEST_ASSERT_TRUE(pixels >= previous);
    previous = pixels;
  }
}

// Bug 2017 n°6 : map(mV, 3350, 4200, 0, 8) ne bornait pas et tronquait vers
// zéro. Sous 3,243 V le résultat était négatif ; affecté à un uint8_t il
// devenait 253-255, puis la ligne `if (pixels > 8) pixels = 8;` le ramenait à
// 8. UNE BATTERIE À PLAT S'AFFICHAIT DONC PLEINE — exactement l'inverse de ce
// qu'il faut savoir en vol.
static void test_deeply_discharged_battery_reads_empty() {
  TEST_ASSERT_EQUAL_INT(0, vario::batteryPixels(3.20f));
  TEST_ASSERT_EQUAL_INT(0, vario::batteryPixels(3.00f));
  TEST_ASSERT_EQUAL_INT(0, vario::batteryPixels(2.50f));
  TEST_ASSERT_EQUAL_INT(0, vario::batteryPixels(0.0f));
}

static void test_overvoltage_is_clamped() {
  TEST_ASSERT_EQUAL_INT(8, vario::batteryPixels(4.30f));
  TEST_ASSERT_EQUAL_INT(8, vario::batteryPixels(12.0f));
}

static void test_fraction_is_bounded() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, vario::batteryFraction(3.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, vario::batteryFraction(4.3f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, vario::batteryFraction(3.775f));
}

static void test_non_finite_reads_empty() {
  TEST_ASSERT_EQUAL_INT(0, vario::batteryPixels(std::nanf("")));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, vario::batteryFraction(std::nanf("")));
}

static void test_custom_pixel_count() {
  TEST_ASSERT_EQUAL_INT(20, vario::batteryPixels(4.20f, 20));
  TEST_ASSERT_EQUAL_INT(0, vario::batteryPixels(3.00f, 20));
  TEST_ASSERT_EQUAL_INT(0, vario::batteryPixels(4.20f, 0));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_full_and_empty_endpoints);
  RUN_TEST(test_midpoint_is_half);
  RUN_TEST(test_monotonic);
  RUN_TEST(test_deeply_discharged_battery_reads_empty);
  RUN_TEST(test_overvoltage_is_clamped);
  RUN_TEST(test_fraction_is_bounded);
  RUN_TEST(test_non_finite_reads_empty);
  RUN_TEST(test_custom_pixel_count);
  return UNITY_END();
}
