#include <unity.h>

#include <cmath>
#include <cstring>

#include "core/config_schema.h"
#include "core/isa.h"

void setUp() {}
void tearDown() {}

static void test_defaults_are_sane() {
  vario::Config config;

  // Le seuil de montée descend à 0,2 m/s, contre 0,3 en 2017 : le plancher de
  // bruit du nouveau filtre est passé sous 0,1 m/s, ce qui rend ce seuil enfin
  // exploitable. C'est le bénéfice visible du filtre de Kalman.
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, config.climb_threshold_ms);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.1f, config.sink_threshold_ms);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1013.25f, config.qnh_hpa);
}

static void test_schema_is_not_empty_and_keys_are_unique() {
  const std::size_t count = vario::configFieldCount();
  TEST_ASSERT_TRUE(count > 0);

  for (std::size_t i = 0; i < count; ++i) {
    const vario::ConfigField& a = vario::configField(i);

    // Clé non vide et dans la limite de 15 caractères imposée par NVS.
    TEST_ASSERT_NOT_NULL(a.key);
    TEST_ASSERT_TRUE(std::strlen(a.key) > 0);
    TEST_ASSERT_TRUE(std::strlen(a.key) <= 15);

    // Libellé présent : c'est lui qu'affiche le menu embarqué.
    TEST_ASSERT_NOT_NULL(a.label);
    TEST_ASSERT_TRUE(std::strlen(a.label) > 0);

    // Bornes cohérentes.
    TEST_ASSERT_TRUE(a.min <= a.max);
    TEST_ASSERT_TRUE(a.step > 0.0f);

    for (std::size_t j = i + 1; j < count; ++j) {
      TEST_ASSERT_TRUE(std::strcmp(a.key, vario::configField(j).key) != 0);
    }
  }
}

static void test_every_field_is_readable_and_writable() {
  vario::Config config;

  // La table et les accesseurs doivent rester synchronisés : si un champ est
  // ajouté à la table sans être branché, ce test échoue.
  for (std::size_t i = 0; i < vario::configFieldCount(); ++i) {
    const vario::ConfigField& field = vario::configField(i);

    float value = 0.0f;
    TEST_ASSERT_TRUE(vario::getConfigValue(config, field.key, value));
    TEST_ASSERT_TRUE(vario::setConfigValue(config, field.key, value));

    // Aller-retour au milieu de la plage.
    const float middle = (field.min + field.max) / 2.0f;
    TEST_ASSERT_TRUE(vario::setConfigValue(config, field.key, middle));
    TEST_ASSERT_TRUE(vario::getConfigValue(config, field.key, value));
    TEST_ASSERT_TRUE(value >= field.min && value <= field.max);
  }
}

static void test_unknown_key_is_refused() {
  vario::Config config;
  float value = 0.0f;

  TEST_ASSERT_FALSE(vario::getConfigValue(config, "inconnu", value));
  TEST_ASSERT_FALSE(vario::setConfigValue(config, "inconnu", 1.0f));
  TEST_ASSERT_NULL(vario::findConfigField("inconnu"));
  TEST_ASSERT_FALSE(vario::getConfigValue(config, nullptr, value));
}

// Bug 2017 n°7 : le QNH n'était pas borné du tout. Le pousser à 0 produisait
// pow(p/0, 0.1903) = inf, donc une altitude NaN qui se propageait
// définitivement dans le vario.
static void test_qnh_is_clamped_by_the_schema() {
  vario::Config config;

  vario::setConfigValue(config, "qnh", 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, vario::kQnhMinHpa, config.qnh_hpa);

  vario::setConfigValue(config, "qnh", 99999.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, vario::kQnhMaxHpa, config.qnh_hpa);

  vario::setConfigValue(config, "qnh", -50.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, vario::kQnhMinHpa, config.qnh_hpa);
}

// Les autres réglages débordaient aussi de leur type : le contraste passait de
// 0 à 255 puis était ramené à 100, l'éclairage faisait de même. Toute valeur
// hors bornes doit désormais être ramenée dans la plage, sans jamais boucler.
static void test_all_fields_clamp_instead_of_wrapping() {
  for (std::size_t i = 0; i < vario::configFieldCount(); ++i) {
    const vario::ConfigField& field = vario::configField(i);
    vario::Config config;

    vario::setConfigValue(config, field.key, field.min - 1000.0f);
    float low = 0.0f;
    vario::getConfigValue(config, field.key, low);
    TEST_ASSERT_TRUE(low >= field.min);
    TEST_ASSERT_TRUE(low <= field.max);

    vario::setConfigValue(config, field.key, field.max + 1000.0f);
    float high = 0.0f;
    vario::getConfigValue(config, field.key, high);
    TEST_ASSERT_TRUE(high >= field.min);
    TEST_ASSERT_TRUE(high <= field.max);

    // La valeur basse ne doit pas être devenue la valeur haute par bouclage.
    TEST_ASSERT_TRUE(low <= high);
  }
}

static void test_non_finite_values_are_refused() {
  vario::Config config;
  const float nan_value = std::nanf("");

  vario::setConfigValue(config, "qnh", nan_value);
  TEST_ASSERT_TRUE(std::isfinite(config.qnh_hpa));

  vario::setConfigValue(config, "clmb", nan_value);
  TEST_ASSERT_TRUE(std::isfinite(config.climb_threshold_ms));
}

static void test_clamp_config_repairs_a_corrupted_struct() {
  vario::Config config;

  // Simule une configuration lue depuis un stockage abîmé.
  config.qnh_hpa = -1.0f;
  config.climb_threshold_ms = 500.0f;
  config.contrast = 9999;
  config.volume = -42;

  vario::clampConfig(config);

  TEST_ASSERT_TRUE(config.qnh_hpa >= vario::kQnhMinHpa);
  TEST_ASSERT_TRUE(config.climb_threshold_ms <= 5.0f);
  TEST_ASSERT_TRUE(config.contrast >= 0 && config.contrast <= 100);
  TEST_ASSERT_TRUE(config.volume >= 0 && config.volume <= 4);
}

static void test_zero_means_disabled_for_thresholds() {
  vario::Config config;
  char text[16];

  // Convention conservée depuis 2017 : un seuil à 0 désactive l'alarme.
  vario::setConfigValue(config, "clmb", 0.0f);
  vario::formatConfigValue(text, sizeof(text), config, "clmb");
  TEST_ASSERT_EQUAL_STRING("OFF", text);

  vario::setConfigValue(config, "clmb", 0.4f);
  vario::formatConfigValue(text, sizeof(text), config, "clmb");
  TEST_ASSERT_EQUAL_STRING("0.4", text);
}

static void test_value_formatting_per_type() {
  vario::Config config;
  char text[16];

  vario::setConfigValue(config, "qnh", 1013.0f);
  vario::formatConfigValue(text, sizeof(text), config, "qnh");
  TEST_ASSERT_EQUAL_STRING("1013", text);

  vario::setConfigValue(config, "cont", 45.0f);
  vario::formatConfigValue(text, sizeof(text), config, "cont");
  TEST_ASSERT_EQUAL_STRING("45", text);

  vario::setConfigValue(config, "ble", 1.0f);
  vario::formatConfigValue(text, sizeof(text), config, "ble");
  TEST_ASSERT_EQUAL_STRING("ON", text);

  vario::setConfigValue(config, "ble", 0.0f);
  vario::formatConfigValue(text, sizeof(text), config, "ble");
  TEST_ASSERT_EQUAL_STRING("OFF", text);

  vario::setConfigValue(config, "tone", 1.0f);
  vario::formatConfigValue(text, sizeof(text), config, "tone");
  TEST_ASSERT_EQUAL_STRING("MODERNE", text);

  vario::setConfigValue(config, "tone", 0.0f);
  vario::formatConfigValue(text, sizeof(text), config, "tone");
  TEST_ASSERT_EQUAL_STRING("CLASSIQUE", text);
}

static void test_formatting_is_buffer_safe() {
  vario::Config config;
  char tiny[3];

  vario::formatConfigValue(tiny, sizeof(tiny), config, "tone");
  TEST_ASSERT_EQUAL_CHAR('\0', tiny[sizeof(tiny) - 1]);

  TEST_ASSERT_EQUAL_UINT(0, vario::formatConfigValue(nullptr, 16, config, "qnh"));

  char buf[16];
  TEST_ASSERT_EQUAL_UINT(0,
                         vario::formatConfigValue(buf, sizeof(buf), config, "inconnu"));
  TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
}

// Un champ ajouté au schéma dans une version future doit simplement prendre sa
// valeur par défaut à la lecture : c'est ce qui rend la compatibilité ascendante
// gratuite, contrairement aux offsets calculés de 2017 où le moindre changement
// décalait tout.
static void test_stepping_a_field_stays_in_range() {
  for (std::size_t i = 0; i < vario::configFieldCount(); ++i) {
    const vario::ConfigField& field = vario::configField(i);
    vario::Config config;

    // Nombre de crans nécessaires pour traverser toute la plage, avec de la
    // marge. Le déduire du champ plutôt que de le fixer en dur : l'offset
    // d'altitude couvre 9500 m par pas de 5, soit 1900 crans.
    const int steps_to_cross =
        static_cast<int>((field.max - field.min) / field.step) + 10;

    // On parcourt toute la plage par pas successifs, dans les deux sens.
    for (int step = 0; step < steps_to_cross; ++step) {
      float value = 0.0f;
      vario::getConfigValue(config, field.key, value);
      vario::setConfigValue(config, field.key, value + field.step);
    }
    float high = 0.0f;
    vario::getConfigValue(config, field.key, high);
    TEST_ASSERT_FLOAT_WITHIN(field.step, field.max, high);

    for (int step = 0; step < 2 * steps_to_cross; ++step) {
      float value = 0.0f;
      vario::getConfigValue(config, field.key, value);
      vario::setConfigValue(config, field.key, value - field.step);
    }
    float low = 0.0f;
    vario::getConfigValue(config, field.key, low);
    TEST_ASSERT_FLOAT_WITHIN(field.step, field.min, low);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_are_sane);
  RUN_TEST(test_schema_is_not_empty_and_keys_are_unique);
  RUN_TEST(test_every_field_is_readable_and_writable);
  RUN_TEST(test_unknown_key_is_refused);
  RUN_TEST(test_qnh_is_clamped_by_the_schema);
  RUN_TEST(test_all_fields_clamp_instead_of_wrapping);
  RUN_TEST(test_non_finite_values_are_refused);
  RUN_TEST(test_clamp_config_repairs_a_corrupted_struct);
  RUN_TEST(test_zero_means_disabled_for_thresholds);
  RUN_TEST(test_value_formatting_per_type);
  RUN_TEST(test_formatting_is_buffer_safe);
  RUN_TEST(test_stepping_a_field_stays_in_range);
  return UNITY_END();
}
