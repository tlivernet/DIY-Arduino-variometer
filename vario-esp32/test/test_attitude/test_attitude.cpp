#include <unity.h>

#include <cmath>
#include <initializer_list>

#include "core/attitude.h"
#include "support/synth.h"

void setUp() {}
void tearDown() {}

namespace {

constexpr float kDt = 1.0f / 480.0f;  // cadence IMU visée

// Angle, en degrés, entre le bas estimé et le bas vrai.
float tiltErrorDeg(const vario::Vec3& estimated, const vario::Vec3& truth) {
  const float c = vario::dot(vario::normalized(estimated), vario::normalized(truth));
  const float clamped = c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c);
  return std::acos(clamped) * 180.0f / 3.14159265358979f;
}

// Bas vrai dans le repère boîtier, pour un virage à l'inclinaison donnée.
vario::Vec3 trueDownInBody(float bank_rad, float yaw) {
  const synth::Mat3 r = synth::earthToBody(yaw, 0.0f, bank_rad);
  return r * vario::Vec3{0.0f, 0.0f, 1.0f};
}

}  // namespace

static void test_align_sets_down_from_gravity() {
  vario::AttitudeFilter filter;
  const synth::Sample s = synth::level();

  filter.align(s.accel, s.gyro);

  TEST_ASSERT_TRUE(filter.aligned());
  // Boîtier à plat, Z vers le bas : le bas est +Z.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, filter.down().z);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, filter.down().x);
}

// Après alignement, l'accélération verticale est identiquement nulle dans
// l'assiette de démarrage : c'est ce qui fait partir le biais du Kalman juste
// par construction, au lieu de le laisser converger à l'aveugle.
static void test_vertical_accel_is_zero_at_rest() {
  vario::AttitudeFilter filter;
  const synth::Sample s = synth::level();
  filter.align(s.accel, s.gyro);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, filter.verticalAccel(s.accel));
}

static void test_vertical_accel_sign_is_positive_up() {
  vario::AttitudeFilter filter;
  const synth::Sample s = synth::level();
  filter.align(s.accel, s.gyro);

  // Une accélération vers le haut ajoute au facteur de charge, donc rend la
  // composante mesurée plus négative sur Z boîtier (qui pointe vers le bas).
  const vario::Vec3 accelerating_up{0.0f, 0.0f, -vario::kGravity - 1.0f};
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, filter.verticalAccel(accelerating_up));

  const vario::Vec3 accelerating_down{0.0f, 0.0f, -vario::kGravity + 1.0f};
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, filter.verticalAccel(accelerating_down));
}

static void test_stays_level_when_stationary() {
  vario::AttitudeFilter filter;
  const synth::Sample s = synth::level();
  filter.align(s.accel, s.gyro);

  for (int i = 0; i < 480 * 30; ++i) {
    filter.update(s.accel, s.gyro, kDt);
  }
  TEST_ASSERT_TRUE(tiltErrorDeg(filter.down(), vario::Vec3{0, 0, 1}) < 0.5f);
}

namespace {

// Résultat d'une spirale simulée.
struct TurnResult {
  float worst_vertical_accel = 0.0f;
  float worst_tilt_deg = 0.0f;
  int acceptance_percent = 0;
};

// Rejoue un virage coordonné établi.
//
// Le filtre est initialisé sur l'assiette VRAIE à t = 0 : le jeu de données
// démarre déjà incliné, alors qu'un appareil réel se serait aligné à plat au
// décollage avant de s'engager. Partir de l'assiette vraie isole donc la
// question centrifuge d'un simple problème de condition initiale.
TurnResult replayTurn(float bank_rad, float actual_speed_ms, bool compensation,
                      float gyro_bias_dps = 0.0f, float duration_s = 60.0f) {
  const float turn_rate = vario::kGravity * std::tan(bank_rad) / actual_speed_ms;

  vario::AttitudeFilter::Config config;
  config.centrifugal_compensation = compensation;
  vario::AttitudeFilter filter(config);
  filter.align(trueDownInBody(bank_rad, 0.0f) * -vario::kGravity, vario::Vec3{});

  const vario::Vec3 bias{0.0f, synth::degToRad(gyro_bias_dps), 0.0f};

  TurnResult result;
  int accepted = 0;
  int total = 0;
  const int steps = static_cast<int>(duration_s * 480.0f);

  for (int i = 1; i <= steps; ++i) {
    const float t = static_cast<float>(i) * kDt;
    const synth::Sample s = synth::coordinatedTurn(bank_rad, t, actual_speed_ms);
    filter.update(s.accel, s.gyro + bias, kDt);

    ++total;
    if (filter.lastSampleAccepted()) ++accepted;

    if (t > 3.0f) {  // on laisse le régime s'établir
      result.worst_vertical_accel = std::fmax(result.worst_vertical_accel,
                                              std::fabs(filter.verticalAccel(s.accel)));
      result.worst_tilt_deg =
          std::fmax(result.worst_tilt_deg,
                    tiltErrorDeg(filter.down(), trueDownInBody(bank_rad, turn_rate * t)));
    }
  }
  result.acceptance_percent = total > 0 ? 100 * accepted / total : 0;
  return result;
}

}  // namespace

// LE TEST DÉCISIF : virage coordonné établi, vitesse verticale VRAIE nulle.
// L'accélération verticale déduite doit rester négligeable, à toute
// inclinaison.
static void test_coordinated_turn_produces_no_false_vertical_accel() {
  for (float bank_deg : {15.0f, 30.0f, 45.0f}) {
    const TurnResult r = replayTurn(synth::degToRad(bank_deg), 10.0f, true);
    TEST_ASSERT_TRUE(r.worst_vertical_accel < 0.05f);
    TEST_ASSERT_TRUE(r.worst_tilt_deg < 1.0f);
  }
}

// Ce que la compensation apporte réellement, cas 1 : AUX FAIBLES INCLINAISONS.
//
// À 15°, le facteur de charge ne vaut que g/cos(15°) = 10,15 m/s², soit 0,34 de
// plus que g : la fenêtre d'acceptation (±0,8) le laisse passer. Sans
// compensation, l'accéléromètre corrompt donc activement l'assiette et le
// filtre converge vers une erreur de plus de 8°. C'est précisément le régime
// où la fenêtre ne protège pas, et c'est là que la compensation est
// indispensable.
static void test_shallow_turn_without_compensation_corrupts_attitude() {
  const float bank = synth::degToRad(15.0f);

  const TurnResult without = replayTurn(bank, 10.0f, false);
  const TurnResult with = replayTurn(bank, 10.0f, true);

  TEST_ASSERT_TRUE(without.worst_tilt_deg > 5.0f);
  TEST_ASSERT_TRUE(with.worst_tilt_deg < 1.0f);

  // Et cela se traduit directement en fausse accélération verticale.
  TEST_ASSERT_TRUE(without.worst_vertical_accel > 0.10f);
  TEST_ASSERT_TRUE(with.worst_vertical_accel < 0.05f);
}

// Ce que la compensation apporte réellement, cas 2 : AUX FORTES INCLINAISONS,
// elle garde l'accéléromètre UTILISABLE.
//
// Au-delà de ~23° d'inclinaison, le facteur de charge sort de la fenêtre
// d'acceptation et, sans compensation, plus AUCUN échantillon ne sert au
// recalage : le filtre tourne en boucle ouverte sur le gyromètre pendant toute
// la spirale. Avec compensation, le module de la référence revient près de g et
// le recalage continue de fonctionner.
static void test_compensation_keeps_accelerometer_usable_in_steep_turn() {
  for (float bank_deg : {30.0f, 45.0f}) {
    const float bank = synth::degToRad(bank_deg);

    TEST_ASSERT_EQUAL_INT(0, replayTurn(bank, 10.0f, false).acceptance_percent);
    TEST_ASSERT_TRUE(replayTurn(bank, 10.0f, true).acceptance_percent > 90);
  }
}

// Conséquence concrète du point précédent : en boucle ouverte, un biais
// gyrométrique réaliste fait dériver l'assiette sans que rien ne la rattrape.
// Avec compensation, la boucle reste fermée et l'erreur reste bornée.
static void test_gyro_bias_does_not_drift_attitude_during_long_turn() {
  const float bank = synth::degToRad(30.0f);
  const TurnResult r = replayTurn(bank, 10.0f, true, 0.5f, 120.0f);

  TEST_ASSERT_TRUE(r.worst_tilt_deg < 3.0f);
  TEST_ASSERT_TRUE(r.acceptance_percent > 90);
}

// Une erreur de ±15 % sur la vitesse de calage supposée (V₀ = 10 m/s pour une
// aile qui vole en réalité à 8,5 ou 11,5 m/s) doit rester tolérable : c'est ce
// qui autorise à se passer de GPS.
static void test_trim_speed_mismatch_is_tolerable() {
  const float bank = synth::degToRad(30.0f);

  for (float actual_speed : {8.5f, 11.5f}) {
    const TurnResult r = replayTurn(bank, actual_speed, true);
    TEST_ASSERT_TRUE(r.worst_tilt_deg < 3.0f);
    TEST_ASSERT_TRUE(r.worst_vertical_accel < 0.20f);
  }
}

static void test_gyro_bias_is_estimated() {
  vario::AttitudeFilter filter;
  const synth::Sample s = synth::level();
  filter.align(s.accel, vario::Vec3{});

  // Biais constant de 0,5 °/s sur l'axe Y, non déclaré au filtre.
  const vario::Vec3 bias{0.0f, synth::degToRad(0.5f), 0.0f};

  for (int i = 0; i < 480 * 300; ++i) {
    filter.update(s.accel, s.gyro + bias, kDt);
  }

  // Le biais estimé doit avoir convergé vers le biais réel, sinon l'assiette
  // dériverait continuellement.
  TEST_ASSERT_FLOAT_WITHIN(synth::degToRad(0.15f), bias.y, filter.gyroBias().y);
  TEST_ASSERT_TRUE(tiltErrorDeg(filter.down(), vario::Vec3{0, 0, 1}) < 1.0f);
}

static void test_violent_transient_is_gated_out() {
  vario::AttitudeFilter filter;
  const synth::Sample s = synth::level();
  filter.align(s.accel, s.gyro);

  // Choc de turbulence : 4 g pendant 100 ms. Le recalage doit être refusé.
  const vario::Vec3 shock{0.0f, 0.0f, -4.0f * vario::kGravity};
  for (int i = 0; i < 48; ++i) {
    filter.update(shock, s.gyro, kDt);
    TEST_ASSERT_FALSE(filter.lastSampleAccepted());
  }
  TEST_ASSERT_TRUE(tiltErrorDeg(filter.down(), vario::Vec3{0, 0, 1}) < 1.0f);
}

static void test_rejects_bad_dt_and_non_finite() {
  vario::AttitudeFilter filter;
  const synth::Sample s = synth::level();
  filter.align(s.accel, s.gyro);
  const vario::Vec3 before = filter.down();

  filter.update(s.accel, s.gyro, 0.0f);
  filter.update(s.accel, s.gyro, -1.0f);
  filter.update(s.accel, s.gyro, 100.0f);
  filter.update(vario::Vec3{std::nanf(""), 0, 0}, s.gyro, kDt);

  TEST_ASSERT_TRUE(vario::isFinite(filter.down()));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, before.z, filter.down().z);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_align_sets_down_from_gravity);
  RUN_TEST(test_vertical_accel_is_zero_at_rest);
  RUN_TEST(test_vertical_accel_sign_is_positive_up);
  RUN_TEST(test_stays_level_when_stationary);
  RUN_TEST(test_coordinated_turn_produces_no_false_vertical_accel);
  RUN_TEST(test_shallow_turn_without_compensation_corrupts_attitude);
  RUN_TEST(test_compensation_keeps_accelerometer_usable_in_steep_turn);
  RUN_TEST(test_gyro_bias_does_not_drift_attitude_during_long_turn);
  RUN_TEST(test_trim_speed_mismatch_is_tolerable);
  RUN_TEST(test_gyro_bias_is_estimated);
  RUN_TEST(test_violent_transient_is_gated_out);
  RUN_TEST(test_rejects_bad_dt_and_non_finite);
  return UNITY_END();
}
