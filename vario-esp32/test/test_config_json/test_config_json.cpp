#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/config_json.h"
#include "core/config_schema.h"
#include "core/web_page.h"

void setUp() {}
void tearDown() {}

namespace {

using vario::applyJsonToConfig;
using vario::Config;
using vario::configToJson;

// Cherche une sous-chaîne : le JSON n'a pas à être analysé finement pour ces
// vérifications de présence.
bool contains(const char* haystack, const char* needle) {
  return std::strstr(haystack, needle) != nullptr;
}

// ---------------------------------------------------------------------------
// Sérialisation : le JSON reflète le schéma et les valeurs.
// ---------------------------------------------------------------------------

void test_json_lists_every_field() {
  Config config;
  char buf[2048];
  const std::size_t n = configToJson(buf, sizeof(buf), config);
  TEST_ASSERT_TRUE(n > 0);

  // Chaque clé du schéma doit apparaître dans le document.
  for (std::size_t i = 0; i < vario::configFieldCount(); ++i) {
    char key_token[24];
    std::snprintf(key_token, sizeof(key_token), "\"key\":\"%s\"",
                  vario::configField(i).key);
    TEST_ASSERT_TRUE(contains(buf, key_token));
  }

  // Structure de base présente.
  TEST_ASSERT_TRUE(contains(buf, "\"fields\":["));
  TEST_ASSERT_EQUAL_CHAR('}', buf[n - 1]);
}

void test_json_reflects_values_and_text() {
  Config config;
  config.qnh_hpa = 1020.0f;
  config.volume = 0;  // doit produire le texte OFF (zero_disables)

  char buf[2048];
  TEST_ASSERT_TRUE(configToJson(buf, sizeof(buf), config) > 0);

  TEST_ASSERT_TRUE(contains(buf, "\"value\":1020"));
  TEST_ASSERT_TRUE(contains(buf, "\"text\":\"OFF\""));
}

void test_json_truncates_safely_on_small_buffer() {
  Config config;
  char buf[16];  // beaucoup trop petit
  const std::size_t n = configToJson(buf, sizeof(buf), config);
  TEST_ASSERT_EQUAL_UINT(0u, n);
  TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);  // laissé vide, pas de débordement
}

// ---------------------------------------------------------------------------
// Application : le POST modifie la config, en restant borné.
// ---------------------------------------------------------------------------

void test_apply_sets_known_fields() {
  Config config;
  const int applied =
      applyJsonToConfig("{\"clmb\":0.5,\"qnh\":1005,\"ble\":false}", config);

  TEST_ASSERT_EQUAL_INT(3, applied);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, config.climb_threshold_ms);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1005.0f, config.qnh_hpa);
  TEST_ASSERT_FALSE(config.ble_enabled);
}

void test_apply_clamps_out_of_range() {
  Config config;
  // QNH plafonné à kQnhMaxHpa (1100), montée plafonnée à 5.
  applyJsonToConfig("{\"qnh\":5000,\"clmb\":99}", config);

  TEST_ASSERT_TRUE(config.qnh_hpa <= 1100.0f + 0.001f);
  TEST_ASSERT_TRUE(config.climb_threshold_ms <= 5.0f + 0.001f);
}

void test_apply_ignores_unknown_keys() {
  Config config;
  const float qnh_before = config.qnh_hpa;
  // Une clé inconnue (client plus récent) ne casse pas les autres.
  const int applied =
      applyJsonToConfig("{\"inconnu\":42,\"qnh\":1000,\"futur\":true}", config);

  TEST_ASSERT_EQUAL_INT(1, applied);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, config.qnh_hpa);
  (void)qnh_before;
}

void test_apply_handles_true_false_for_bool() {
  Config config;
  config.ble_enabled = false;
  applyJsonToConfig("{\"ble\":true}", config);
  TEST_ASSERT_TRUE(config.ble_enabled);
  applyJsonToConfig("{\"ble\":false}", config);
  TEST_ASSERT_FALSE(config.ble_enabled);
}

void test_apply_rejects_garbage_without_crashing() {
  Config config;
  const float qnh = config.qnh_hpa;
  TEST_ASSERT_EQUAL_INT(0, applyJsonToConfig("", config));
  TEST_ASSERT_EQUAL_INT(0, applyJsonToConfig("pas du json", config));
  TEST_ASSERT_EQUAL_INT(0, applyJsonToConfig("{", config));
  TEST_ASSERT_EQUAL_INT(0, applyJsonToConfig(nullptr, config));
  // La config n'a pas bougé.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, qnh, config.qnh_hpa);
}

// ---------------------------------------------------------------------------
// Aller-retour : sérialiser puis réappliquer le même document est stable.
// C'est ce qui garantit qu'un « Enregistrer » sans modification ne dérive pas.
// ---------------------------------------------------------------------------

void test_round_trip_is_stable() {
  Config original;
  original.qnh_hpa = 1017.0f;
  original.climb_threshold_ms = 0.3f;
  original.volume = 2;
  original.tone_profile = vario::ToneProfile::Classic;

  char buf[2048];
  TEST_ASSERT_TRUE(configToJson(buf, sizeof(buf), original) > 0);

  // On ne peut pas rejouer le document GET tel quel (il porte le schéma), mais
  // un POST minimal reconstruit depuis les valeurs doit converger.
  char post[512];
  std::snprintf(post, sizeof(post), "{\"qnh\":%g,\"clmb\":%g,\"vol\":%d,\"tone\":%d}",
                static_cast<double>(original.qnh_hpa),
                static_cast<double>(original.climb_threshold_ms), original.volume,
                original.tone_profile == vario::ToneProfile::Modern ? 1 : 0);

  Config applied;
  applyJsonToConfig(post, applied);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, original.qnh_hpa, applied.qnh_hpa);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, original.climb_threshold_ms,
                           applied.climb_threshold_ms);
  TEST_ASSERT_EQUAL_INT(original.volume, applied.volume);
  TEST_ASSERT_TRUE(original.tone_profile == applied.tone_profile);
}

// ---------------------------------------------------------------------------
// La page servie est cohérente.
// ---------------------------------------------------------------------------

void test_page_is_self_contained_and_wired() {
  const char* page = vario::kConfigPageHtml;
  TEST_ASSERT_TRUE(vario::configPageLength() > 100);

  // Un document HTML clos.
  TEST_ASSERT_TRUE(contains(page, "<!doctype html>"));
  TEST_ASSERT_TRUE(contains(page, "</html>"));

  // Interroge et écrit les bons points d'accès.
  TEST_ASSERT_TRUE(contains(page, "fetch('/config')"));
  TEST_ASSERT_TRUE(contains(page, "method:'POST'"));

  // Autonome : aucune ressource distante.
  TEST_ASSERT_FALSE(contains(page, "http://"));
  TEST_ASSERT_FALSE(contains(page, "https://"));
  TEST_ASSERT_FALSE(contains(page, "cdn"));
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_json_lists_every_field);
  RUN_TEST(test_json_reflects_values_and_text);
  RUN_TEST(test_json_truncates_safely_on_small_buffer);
  RUN_TEST(test_apply_sets_known_fields);
  RUN_TEST(test_apply_clamps_out_of_range);
  RUN_TEST(test_apply_ignores_unknown_keys);
  RUN_TEST(test_apply_handles_true_false_for_bool);
  RUN_TEST(test_apply_rejects_garbage_without_crashing);
  RUN_TEST(test_round_trip_is_stable);
  RUN_TEST(test_page_is_self_contained_and_wired);
  return UNITY_END();
}
