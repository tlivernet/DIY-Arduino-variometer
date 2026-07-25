#include <unity.h>

#include <cmath>
#include <cstring>

#include "core/format.h"

void setUp() {}
void tearDown() {}

namespace {

void expectText(float vario_ms, const char* expected) {
  char buffer[16];
  vario::formatVarioTo(buffer, sizeof(buffer), vario_ms);
  TEST_ASSERT_EQUAL_STRING(expected, buffer);
}

}  // namespace

static void test_basic_values() {
  expectText(0.0f, " 0.0");
  expectText(1.0f, "+1.0");
  expectText(2.5f, "+2.5");
  expectText(-1.2f, "-1.2");
}

// Bug 2017 n°8 : la partie entière et le dixième étaient calculés séparément
// (floor(|v|) d'un côté, round(10·|v|) − 10·floor(|v|) de l'autre). Quand
// l'arrondi du dixième reportait, le second terme valait 10 : 1,96 m/s
// s'affichait « 1.10 » au lieu de « 2.0 ».
static void test_rounding_carry_does_not_produce_ten() {
  expectText(1.96f, "+2.0");
  expectText(0.96f, "+1.0");
  expectText(2.99f, "+3.0");
  expectText(-1.96f, "-2.0");
  expectText(9.99f, "+10.0");
}

static void test_tenth_digit_is_always_single_digit() {
  // Balayage complet : le dixième doit toujours rester dans 0..9.
  for (float v = -12.0f; v <= 12.0f; v += 0.01f) {
    const vario::VarioText text = vario::formatVario(v);
    TEST_ASSERT_TRUE(text.tenth >= 0 && text.tenth <= 9);
    TEST_ASSERT_TRUE(text.whole >= 0);
  }
}

static void test_sign_deadband() {
  // Bande morte de ±0,05 m/s reprise de l'original : ni + ni −.
  TEST_ASSERT_EQUAL_CHAR(' ', vario::formatVario(0.0f).sign);
  TEST_ASSERT_EQUAL_CHAR(' ', vario::formatVario(0.04f).sign);
  TEST_ASSERT_EQUAL_CHAR(' ', vario::formatVario(-0.04f).sign);
  TEST_ASSERT_EQUAL_CHAR('+', vario::formatVario(0.05f).sign);
  TEST_ASSERT_EQUAL_CHAR('-', vario::formatVario(-0.05f).sign);
}

static void test_magnitude_is_absolute() {
  const vario::VarioText text = vario::formatVario(-3.4f);
  TEST_ASSERT_EQUAL_CHAR('-', text.sign);
  TEST_ASSERT_EQUAL_INT(3, text.whole);
  TEST_ASSERT_EQUAL_INT(4, text.tenth);
}

static void test_non_finite_is_safe() {
  const vario::VarioText text = vario::formatVario(std::nanf(""));
  TEST_ASSERT_EQUAL_CHAR(' ', text.sign);
  TEST_ASSERT_EQUAL_INT(0, text.whole);
  TEST_ASSERT_EQUAL_INT(0, text.tenth);

  expectText(std::nanf(""), " 0.0");
}

static void test_null_and_tiny_buffers_are_safe() {
  TEST_ASSERT_EQUAL_UINT(0, vario::formatVarioTo(nullptr, 10, 1.0f));

  char buffer[3];
  const std::size_t written = vario::formatVarioTo(buffer, sizeof(buffer), 1.0f);
  TEST_ASSERT_TRUE(written < sizeof(buffer));
  TEST_ASSERT_EQUAL_CHAR('\0', buffer[sizeof(buffer) - 1]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_basic_values);
  RUN_TEST(test_rounding_carry_does_not_produce_ten);
  RUN_TEST(test_tenth_digit_is_always_single_digit);
  RUN_TEST(test_sign_deadband);
  RUN_TEST(test_magnitude_is_absolute);
  RUN_TEST(test_non_finite_is_safe);
  RUN_TEST(test_null_and_tiny_buffers_are_safe);
  return UNITY_END();
}
