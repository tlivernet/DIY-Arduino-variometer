#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/lk8ex1.h"

void setUp() {}
void tearDown() {}

namespace {

// Vérifie que la somme de contrôle inscrite dans la trame correspond bien au
// XOR des caractères situés entre « $ » et « * ».
bool checksumIsCorrect(const char* sentence) {
  const char* dollar = std::strchr(sentence, '$');
  const char* star = std::strchr(sentence, '*');
  if (dollar == nullptr || star == nullptr || star < dollar) return false;

  std::uint8_t expected = 0;
  for (const char* p = dollar + 1; p < star; ++p) {
    expected ^= static_cast<std::uint8_t>(*p);
  }

  unsigned int found = 0;
  if (std::sscanf(star + 1, "%02X", &found) != 1) return false;
  return found == expected;
}

// Extrait le n-ième champ séparé par des virgules.
long field(const char* sentence, int index) {
  const char* p = sentence;
  for (int i = 0; i <= index; ++i) {
    p = std::strchr(p, ',');
    if (p == nullptr) return -999999;
    ++p;
  }
  return std::atol(p);
}

}  // namespace

static void test_sentence_shape() {
  char buf[64];
  vario::Lk8ex1Data data;
  data.pressure_pa = 89876.0f;
  data.vario_ms = 2.35f;
  data.temperature_c = 4.0f;
  data.battery_percent = 72;

  const std::size_t written = vario::formatLk8ex1(buf, sizeof(buf), data);

  TEST_ASSERT_TRUE(written > 0);
  TEST_ASSERT_EQUAL_CHAR('$', buf[0]);
  TEST_ASSERT_TRUE(std::strncmp(buf, "$LK8EX1,", 8) == 0);
  // Terminaison CRLF, comme le veut NMEA.
  TEST_ASSERT_TRUE(std::strstr(buf, "\r\n") != nullptr);
  TEST_ASSERT_TRUE(checksumIsCorrect(buf));
}

static void test_checksum_matches_reference() {
  // Le XOR est calculé sur le corps seul, sans « $ » ni « * ».
  TEST_ASSERT_EQUAL_UINT8(0, vario::nmeaChecksum(""));
  TEST_ASSERT_EQUAL_UINT8('A', vario::nmeaChecksum("A"));
  TEST_ASSERT_EQUAL_UINT8('A' ^ 'B', vario::nmeaChecksum("AB"));
  TEST_ASSERT_EQUAL_UINT8(0, vario::nmeaChecksum(nullptr));
}

static void test_pressure_is_sent_raw_in_pascals() {
  char buf[64];
  vario::Lk8ex1Data data;
  data.pressure_pa = 89876.4f;

  vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_EQUAL_INT(89876, field(buf, 0));
}

// Choix délibéré : l'altitude est marquée indisponible pour que le téléphone
// applique SON QNH. Deux réglages d'altitude divergents entre l'appareil et
// l'application sont une source classique de confusion en vol.
static void test_altitude_is_left_unavailable() {
  char buf[64];
  vario::Lk8ex1Data data;
  data.pressure_pa = 101325.0f;

  vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_EQUAL_INT(99999, field(buf, 1));
}

static void test_vario_is_in_centimetres_per_second() {
  char buf[64];
  vario::Lk8ex1Data data;

  data.vario_ms = 2.35f;
  vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_EQUAL_INT(235, field(buf, 2));

  data.vario_ms = -1.07f;
  vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_EQUAL_INT(-107, field(buf, 2));

  data.vario_ms = 0.0f;
  vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_EQUAL_INT(0, field(buf, 2));
}

// La convention LK8EX1 encode un pourcentage de batterie en ajoutant 1000.
static void test_battery_percent_is_offset_by_1000() {
  char buf[64];
  vario::Lk8ex1Data data;

  data.battery_percent = 72;
  vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_EQUAL_INT(1072, field(buf, 4));

  data.battery_percent = 0;
  vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_EQUAL_INT(1000, field(buf, 4));

  // Hors bornes : ramené dans 0-100.
  data.battery_percent = 150;
  vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_EQUAL_INT(1100, field(buf, 4));

  data.battery_percent = -20;
  vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_EQUAL_INT(1000, field(buf, 4));
}

static void test_invalid_sensors_use_sentinels() {
  char buf[64];
  vario::Lk8ex1Data data;
  data.pressure_valid = false;
  data.temperature_valid = false;

  vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_EQUAL_INT(999999, field(buf, 0));
  TEST_ASSERT_EQUAL_INT(99, field(buf, 3));
  TEST_ASSERT_TRUE(checksumIsCorrect(buf));
}

static void test_non_finite_values_do_not_corrupt_sentence() {
  char buf[64];
  vario::Lk8ex1Data data;
  data.pressure_pa = std::nanf("");
  data.vario_ms = std::nanf("");
  data.temperature_c = std::nanf("");

  const std::size_t written = vario::formatLk8ex1(buf, sizeof(buf), data);

  TEST_ASSERT_TRUE(written > 0);
  TEST_ASSERT_TRUE(checksumIsCorrect(buf));
  TEST_ASSERT_TRUE(std::strstr(buf, "nan") == nullptr);
  TEST_ASSERT_EQUAL_INT(0, field(buf, 2));
}

// La trame doit tenir dans une seule notification BLE une fois le MTU porté à
// 128 octets, sinon la cadence de 5 Hz coûte deux paquets par mesure.
static void test_sentence_fits_one_ble_notification() {
  char buf[64];
  vario::Lk8ex1Data data;
  data.pressure_pa = 101325.0f;
  data.vario_ms = -9.99f;
  data.temperature_c = -40.0f;
  data.battery_percent = 100;

  const std::size_t written = vario::formatLk8ex1(buf, sizeof(buf), data);
  TEST_ASSERT_TRUE(written > 0);
  TEST_ASSERT_TRUE(written <= 60);
}

static void test_small_buffer_is_refused_safely() {
  char small[8];
  vario::Lk8ex1Data data;

  TEST_ASSERT_EQUAL_UINT(0, vario::formatLk8ex1(small, sizeof(small), data));
  TEST_ASSERT_EQUAL_CHAR('\0', small[0]);
  TEST_ASSERT_EQUAL_UINT(0, vario::formatLk8ex1(nullptr, 64, data));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_sentence_shape);
  RUN_TEST(test_checksum_matches_reference);
  RUN_TEST(test_pressure_is_sent_raw_in_pascals);
  RUN_TEST(test_altitude_is_left_unavailable);
  RUN_TEST(test_vario_is_in_centimetres_per_second);
  RUN_TEST(test_battery_percent_is_offset_by_1000);
  RUN_TEST(test_invalid_sensors_use_sentinels);
  RUN_TEST(test_non_finite_values_do_not_corrupt_sentence);
  RUN_TEST(test_sentence_fits_one_ble_notification);
  RUN_TEST(test_small_buffer_is_refused_safely);
  return UNITY_END();
}
