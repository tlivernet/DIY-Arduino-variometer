#include <unity.h>

#include <cstring>

#include "core/flight_log.h"

void setUp() {}
void tearDown() {}

namespace {

vario::FlightRecord makeRecord(std::uint32_t id, std::uint32_t duration_s = 3600) {
  vario::FlightRecord record{};
  record.id = id;
  record.start_unix = 1700000000u + id * 86400u;
  record.duration_s = duration_s;
  record.alt_start_dm = 4120;
  record.alt_max_dm = 24560;
  record.alt_min_dm = 11800;
  record.alt_end_dm = 4200;
  record.climb_max_dms = 42;
  record.sink_max_dms = -31;
  record.gain_m = 1840;
  record.qnh_pa = 101325;
  record.temp_min_dc = -60;
  record.temp_max_dc = 180;
  record.batt_start_pct = 100;
  record.batt_end_pct = 72;
  vario::sealFlightRecord(record);
  return record;
}

}  // namespace

// La disposition doit être stable et indépendante du compilateur : le montage de
// 2017 calculait ses offsets EEPROM avec sizeof(), or sur AVR un int fait
// 2 octets avec un alignement de 1, et sur ESP32 4 et 4.
static void test_record_layout_is_exactly_64_bytes() {
  TEST_ASSERT_EQUAL_UINT(64, sizeof(vario::FlightRecord));
}

static void test_seal_then_validate() {
  vario::FlightRecord record = makeRecord(1);

  TEST_ASSERT_TRUE(vario::flightRecordValid(record));
  TEST_ASSERT_EQUAL_UINT16(vario::kFlightMagic, record.magic);
  TEST_ASSERT_EQUAL_UINT8(vario::kFlightRecordVersion, record.record_ver);
}

// Un octet corrompu doit être détecté : c'est ce qui permet, au montage du
// système de fichiers, de tronquer le journal au premier enregistrement invalide
// plutôt que de lire n'importe quoi.
static void test_corruption_is_detected() {
  vario::FlightRecord record = makeRecord(1);

  record.gain_m ^= 0x01u;
  TEST_ASSERT_FALSE(vario::flightRecordValid(record));

  record.gain_m ^= 0x01u;  // remis en état
  TEST_ASSERT_TRUE(vario::flightRecordValid(record));

  record.magic = 0x1234;
  TEST_ASSERT_FALSE(vario::flightRecordValid(record));
}

static void test_zeroed_record_is_invalid() {
  vario::FlightRecord record{};
  TEST_ASSERT_FALSE(vario::flightRecordValid(record));
}

static void test_unknown_future_version_is_rejected() {
  vario::FlightRecord record = makeRecord(1);
  record.record_ver = 99;
  record.crc16 = vario::crc16Ccitt(&record, sizeof(record) - sizeof(std::uint16_t));

  // CRC correct mais version inconnue : on refuse plutôt que d'interpréter des
  // champs dont on ignore le sens.
  TEST_ASSERT_FALSE(vario::flightRecordValid(record));
}

static void test_crc_is_ccitt_false() {
  // Vecteur de référence du CRC-16/CCITT-FALSE.
  const char* data = "123456789";
  TEST_ASSERT_EQUAL_UINT16(0x29B1, vario::crc16Ccitt(data, 9));
}

static void test_append_and_read_newest_first() {
  vario::FlightLog log;

  TEST_ASSERT_EQUAL_UINT(0, log.count());
  TEST_ASSERT_TRUE(log.append(makeRecord(1)));
  TEST_ASSERT_TRUE(log.append(makeRecord(2)));
  TEST_ASSERT_TRUE(log.append(makeRecord(3)));
  TEST_ASSERT_EQUAL_UINT(3, log.count());

  vario::FlightRecord out{};
  // L'index 0 est « M1 », le plus récent.
  TEST_ASSERT_TRUE(log.at(0, out));
  TEST_ASSERT_EQUAL_UINT32(3, out.id);
  TEST_ASSERT_TRUE(log.at(1, out));
  TEST_ASSERT_EQUAL_UINT32(2, out.id);
  TEST_ASSERT_TRUE(log.at(2, out));
  TEST_ASSERT_EQUAL_UINT32(1, out.id);

  TEST_ASSERT_FALSE(log.at(3, out));
}

static void test_invalid_record_is_refused() {
  vario::FlightLog log;
  vario::FlightRecord bad{};  // ni magie ni CRC

  TEST_ASSERT_FALSE(log.append(bad));
  TEST_ASSERT_EQUAL_UINT(0, log.count());
}

// LE TEST QUI REMPLACE LE BUG 2017 N°5.
//
// L'ancien carrousel de huit emplacements cessait d'enregistrer au bout de huit
// vols si l'emplacement suivant n'était pas vide, et cela silencieusement. Ici,
// enchaîner bien plus de huit vols doit toujours fonctionner : les plus anciens
// sont évincés, jamais l'inverse.
static void test_more_than_eight_flights_keeps_recording() {
  vario::FlightLog log;

  for (std::uint32_t id = 1; id <= 25; ++id) {
    TEST_ASSERT_TRUE(log.append(makeRecord(id)));
  }

  // Les 25 ont bien été enregistrés.
  TEST_ASSERT_EQUAL_UINT32(25, log.totalAppended());
  // La fenêtre affichée en contient huit.
  TEST_ASSERT_EQUAL_UINT(vario::FlightLog::kWindow, log.count());

  // Et ce sont bien les huit DERNIERS, du plus récent au plus ancien.
  vario::FlightRecord out{};
  for (std::size_t i = 0; i < vario::FlightLog::kWindow; ++i) {
    TEST_ASSERT_TRUE(log.at(i, out));
    TEST_ASSERT_EQUAL_UINT32(25 - i, out.id);
  }
}

static void test_next_id_is_monotonic() {
  vario::FlightLog log;
  TEST_ASSERT_EQUAL_UINT32(1, log.nextId());

  log.append(makeRecord(1));
  TEST_ASSERT_EQUAL_UINT32(2, log.nextId());

  // Un identifiant plus grand fait avancer le compteur, il ne le fait pas
  // reculer : les identifiants ne doivent jamais être réutilisés.
  log.append(makeRecord(100));
  TEST_ASSERT_EQUAL_UINT32(101, log.nextId());

  log.append(makeRecord(5));
  TEST_ASSERT_EQUAL_UINT32(101, log.nextId());
}

static void test_clear_empties_the_log() {
  vario::FlightLog log;
  for (std::uint32_t id = 1; id <= 5; ++id) log.append(makeRecord(id));

  log.clear();

  TEST_ASSERT_EQUAL_UINT(0, log.count());
  TEST_ASSERT_EQUAL_UINT32(0, log.totalAppended());
  TEST_ASSERT_EQUAL_UINT32(1, log.nextId());

  vario::FlightRecord out{};
  TEST_ASSERT_FALSE(log.at(0, out));
}

// Les champs larges ne doivent plus déborder. Le cumulé de 2017 passait par un
// (int) puis un uint16_t, donc réapparaissait comme un nombre énorme au-delà de
// 32767 m.
static void test_large_values_survive_a_round_trip() {
  vario::FlightLog log;

  vario::FlightRecord record = makeRecord(1);
  record.gain_m = 80000;        // au-delà de l'ancienne limite de 65535
  record.alt_max_dm = 1200000;  // 120 km en décimètres
  record.duration_s = 86400;    // 24 h
  record.climb_max_dms = 300;   // 30 m/s, l'ancien uint8_t saturait à 25,5
  record.sink_max_dms = -300;
  vario::sealFlightRecord(record);

  TEST_ASSERT_TRUE(log.append(record));

  vario::FlightRecord out{};
  TEST_ASSERT_TRUE(log.at(0, out));
  TEST_ASSERT_EQUAL_UINT32(80000, out.gain_m);
  TEST_ASSERT_EQUAL_INT32(1200000, out.alt_max_dm);
  TEST_ASSERT_EQUAL_UINT32(86400, out.duration_s);
  TEST_ASSERT_EQUAL_INT16(300, out.climb_max_dms);
  TEST_ASSERT_EQUAL_INT16(-300, out.sink_max_dms);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_record_layout_is_exactly_64_bytes);
  RUN_TEST(test_seal_then_validate);
  RUN_TEST(test_corruption_is_detected);
  RUN_TEST(test_zeroed_record_is_invalid);
  RUN_TEST(test_unknown_future_version_is_rejected);
  RUN_TEST(test_crc_is_ccitt_false);
  RUN_TEST(test_append_and_read_newest_first);
  RUN_TEST(test_invalid_record_is_refused);
  RUN_TEST(test_more_than_eight_flights_keeps_recording);
  RUN_TEST(test_next_id_is_monotonic);
  RUN_TEST(test_clear_empties_the_log);
  RUN_TEST(test_large_values_survive_a_round_trip);
  return UNITY_END();
}
