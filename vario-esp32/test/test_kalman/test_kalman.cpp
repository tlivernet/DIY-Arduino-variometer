#include <unity.h>

#include <cmath>

#include "core/attitude.h"
#include "core/kalman.h"
#include "core/vario_filter.h"
#include "support/synth.h"

void setUp() {}
void tearDown() {}

namespace {

// Cadence de la boucle capteur : le MS5611 en OSR 4096 demande ~9 ms par
// conversion, ce qui donne un créneau de 10,4 ms, soit ~96 Hz.
constexpr float kDt = 1.0f / 96.0f;
constexpr float kBaseAltitude = 1000.0f;

// Profil de vitesse verticale : palier, puis rampe vers target en ramp_s.
float trueVelocity(float t_s, float target_ms, float start_s, float ramp_s) {
  if (t_s <= start_s) return 0.0f;
  if (t_s >= start_s + ramp_s) return target_ms;
  return target_ms * (t_s - start_s) / ramp_s;
}

float trueAccel(float t_s, float target_ms, float start_s, float ramp_s) {
  if (t_s <= start_s || t_s >= start_s + ramp_s) return 0.0f;
  return target_ms / ramp_s;
}

}  // namespace

static void test_reset_initializes_on_measurement() {
  vario::VarioKalman kf;
  kf.reset(1234.5f);

  TEST_ASSERT_TRUE(kf.initialized());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1234.5f, kf.altitude());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, kf.velocity());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, kf.bias());
}

static void test_tracks_steady_climb() {
  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);

  float altitude = kBaseAltitude;
  for (int i = 1; i <= static_cast<int>(20.0f / kDt); ++i) {
    altitude += 2.0f * kDt;
    kf.predict(0.0f, kDt);  // vitesse constante ⇒ accélération nulle
    kf.update(altitude);
  }

  TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.0f, kf.velocity());
  TEST_ASSERT_FLOAT_WITHIN(0.5f, altitude, kf.altitude());
}

// Réponse indicielle : 0 → 2 m/s en 200 ms, accéléromètre et baromètre
// cohérents. Comme l'accélération est une ENTRÉE de commande et non une mesure,
// la vitesse doit suivre presque sans retard.
static void test_step_response_is_fast() {
  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);

  const float target = 2.0f;
  const float start = 1.0f;
  const float ramp = 0.2f;

  float altitude = kBaseAltitude;
  float time_to_reach = -1.0f;

  for (int i = 1; i <= static_cast<int>(5.0f / kDt); ++i) {
    const float t = static_cast<float>(i) * kDt;
    altitude += trueVelocity(t, target, start, ramp) * kDt;

    kf.predict(trueAccel(t, target, start, ramp), kDt);
    kf.update(altitude);

    if (time_to_reach < 0.0f && kf.velocity() >= 0.9f * target) {
      time_to_reach = t - start;
    }
  }

  TEST_ASSERT_TRUE(time_to_reach > 0.0f);
  TEST_ASSERT_TRUE(time_to_reach < 0.25f);
}

// La comparaison qui justifie toute la complexité ajoutée : la même rampe,
// vue par le filtre purement barométrique de 2017 (deux EMA en cascade) et par
// le filtre de Kalman. Le Kalman doit être plusieurs fois plus rapide.
static void test_kalman_is_much_faster_than_baro_only_filter() {
  const float target = 2.0f;
  const float start = 1.0f;
  const float ramp = 0.2f;
  const float threshold = 0.9f * target;

  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);

  vario::VarioFilter ema;
  ema.reset(synth::pressureForAltitude(kBaseAltitude), 1013.25f, 0.0f, 0);

  float altitude = kBaseAltitude;
  float kf_time = -1.0f;
  float ema_time = -1.0f;

  for (int i = 1; i <= static_cast<int>(10.0f / kDt); ++i) {
    const float t = static_cast<float>(i) * kDt;
    altitude += trueVelocity(t, target, start, ramp) * kDt;

    kf.predict(trueAccel(t, target, start, ramp), kDt);
    kf.update(altitude);

    const vario::Micros now = static_cast<vario::Micros>(t * 1e6f);
    ema.update(synth::pressureForAltitude(altitude), 1013.25f, 0.0f, now);

    if (kf_time < 0.0f && kf.velocity() >= threshold) kf_time = t - start;
    if (ema_time < 0.0f && ema.varioMs() >= threshold) ema_time = t - start;
  }

  TEST_ASSERT_TRUE(kf_time > 0.0f);
  TEST_ASSERT_TRUE(ema_time > 0.0f);
  TEST_ASSERT_TRUE(kf_time * 3.0f < ema_time);
}

// Bruit au repos. Le montage de 2017 affichait 0,3 à 0,4 m/s RMS, soit PLUS que
// son propre seuil de déclenchement à 0,3 m/s — d'où le bipeur qui bavardait.
// C'est ce plancher qu'il faut faire descendre pour rendre un seuil à 0,2 m/s
// utilisable.
static void test_noise_at_rest_is_low() {
  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);
  synth::Noise noise(4242u);

  double sum_squares = 0.0;
  int samples = 0;

  for (int i = 1; i <= static_cast<int>(60.0f / kDt); ++i) {
    const float t = static_cast<float>(i) * kDt;

    // Bruit d'accéléromètre modeste + bruit barométrique réaliste sur table.
    kf.predict(noise.accelNoise(0.05f).z, kDt);
    kf.update(kBaseAltitude + noise.altitudeNoiseM(0.12f));

    if (t > 10.0f) {  // après établissement
      sum_squares += static_cast<double>(kf.velocity()) * kf.velocity();
      ++samples;
    }
  }

  const float rms = static_cast<float>(std::sqrt(sum_squares / samples));
  TEST_ASSERT_TRUE(samples > 0);
  TEST_ASSERT_TRUE(rms < 0.10f);
}

// Le troisième état doit absorber un biais d'accélération verticale constant,
// sinon la vitesse dériverait indéfiniment. Ce biais représente autant l'erreur
// d'assiette résiduelle que le décalage propre du capteur.
static void test_bias_is_estimated_and_velocity_stays_null() {
  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);

  const float injected_bias = 0.5f;

  for (int i = 1; i <= static_cast<int>(30.0f / kDt); ++i) {
    kf.predict(injected_bias, kDt);  // appareil immobile, accéléromètre biaisé
    kf.update(kBaseAltitude);
  }

  TEST_ASSERT_FLOAT_WITHIN(0.05f, injected_bias, kf.bias());
  TEST_ASSERT_FLOAT_WITHIN(0.10f, 0.0f, kf.velocity());
}

static void test_bias_convergence_is_reasonably_quick() {
  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);

  float settled_at = -1.0f;
  for (int i = 1; i <= static_cast<int>(30.0f / kDt); ++i) {
    const float t = static_cast<float>(i) * kDt;
    kf.predict(0.5f, kDt);
    kf.update(kBaseAltitude);

    if (settled_at < 0.0f && std::fabs(kf.velocity()) < 0.10f && t > 1.0f) {
      settled_at = t;
    }
  }
  TEST_ASSERT_TRUE(settled_at > 0.0f);
  TEST_ASSERT_TRUE(settled_at < 10.0f);
}

// Bug 2017 n°3 : aucune garde sur Δt, donc division par zéro, NaN, et un état
// dont on ne sortait plus jamais.
static void test_bad_dt_is_rejected() {
  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);

  TEST_ASSERT_FALSE(kf.predict(1.0f, 0.0f));
  TEST_ASSERT_FALSE(kf.predict(1.0f, -0.01f));
  TEST_ASSERT_FALSE(kf.predict(1.0f, 10.0f));

  TEST_ASSERT_TRUE(std::isfinite(kf.velocity()));
  TEST_ASSERT_TRUE(std::isfinite(kf.altitude()));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, kBaseAltitude, kf.altitude());
}

static void test_non_finite_inputs_are_rejected() {
  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);
  const float nan_value = std::nanf("");

  TEST_ASSERT_FALSE(kf.predict(nan_value, kDt));
  TEST_ASSERT_FALSE(kf.update(nan_value));

  TEST_ASSERT_TRUE(std::isfinite(kf.velocity()));
  TEST_ASSERT_TRUE(std::isfinite(kf.altitude()));
}

// Un pic de pression isolé (rafale, ombre de voile devant la prise) ne doit pas
// faire sauter la vitesse verticale. L'adoucissement de Huber gonfle R plutôt
// que de rejeter franchement, un rejet dur créant des blocages.
static void test_single_outlier_is_softened() {
  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);

  for (int i = 0; i < static_cast<int>(5.0f / kDt); ++i) {
    kf.predict(0.0f, kDt);
    kf.update(kBaseAltitude);
  }

  // Pic isolé de 5 m.
  kf.predict(0.0f, kDt);
  kf.update(kBaseAltitude + 5.0f);

  TEST_ASSERT_TRUE(std::fabs(kf.velocity()) < 0.5f);
}

// Garde de divergence : un écart franc ET persistant avec le baro doit
// provoquer une réinitialisation sur la mesure, plutôt qu'un entêtement.
static void test_persistent_divergence_triggers_reset() {
  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);

  for (int i = 0; i < static_cast<int>(2.0f / kDt); ++i) {
    kf.predict(0.0f, kDt);
    kf.update(kBaseAltitude);
  }
  TEST_ASSERT_EQUAL_UINT32(0, kf.resetCount());

  // Le baromètre saute de 100 m et y reste (défaillance ou recalage brutal).
  const float jumped = kBaseAltitude + 100.0f;
  for (int i = 0; i < static_cast<int>(5.0f / kDt); ++i) {
    kf.predict(0.0f, kDt);
    kf.update(jumped);
  }

  TEST_ASSERT_TRUE(kf.resetCount() > 0);
  TEST_ASSERT_FLOAT_WITHIN(5.0f, jumped, kf.altitude());
  TEST_ASSERT_TRUE(std::isfinite(kf.velocity()));
}

// Chaîne complète : estimation d'assiette puis Kalman, sur une spirale sans
// ascendance. C'est le scénario où un vario naïf annonce une fausse montée.
static void test_full_chain_no_false_climb_in_turn() {
  const float bank = synth::degToRad(30.0f);
  const float imu_dt = 1.0f / 480.0f;

  vario::AttitudeFilter attitude;
  attitude.align(
      synth::earthToBody(0.0f, 0.0f, bank) * vario::Vec3{0, 0, 1} * -vario::kGravity,
      vario::Vec3{});

  vario::VarioKalman kf;
  kf.reset(kBaseAltitude);

  float worst_velocity = 0.0f;
  float accel_sum = 0.0f;
  int accel_count = 0;

  for (int i = 1; i <= static_cast<int>(60.0f / imu_dt); ++i) {
    const float t = static_cast<float>(i) * imu_dt;
    const synth::Sample s = synth::coordinatedTurn(bank, t, 10.0f, kBaseAltitude);

    attitude.update(s.accel, s.gyro, imu_dt);
    accel_sum += attitude.verticalAccel(s.accel);
    ++accel_count;

    // Un créneau baro tous les 5 pas IMU : on moyenne l'accélération sur le
    // créneau, ce qui équivaut au premier ordre à cinq sous-pas.
    if (i % 5 == 0) {
      kf.predict(accel_sum / static_cast<float>(accel_count), kDt);
      kf.update(s.true_altitude_m);
      accel_sum = 0.0f;
      accel_count = 0;

      if (t > 5.0f) worst_velocity = std::fmax(worst_velocity, std::fabs(kf.velocity()));
    }
  }

  // La vitesse verticale vraie est nulle : on doit rester bien sous le seuil de
  // déclenchement du bipeur.
  TEST_ASSERT_TRUE(worst_velocity < 0.20f);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_reset_initializes_on_measurement);
  RUN_TEST(test_tracks_steady_climb);
  RUN_TEST(test_step_response_is_fast);
  RUN_TEST(test_kalman_is_much_faster_than_baro_only_filter);
  RUN_TEST(test_noise_at_rest_is_low);
  RUN_TEST(test_bias_is_estimated_and_velocity_stays_null);
  RUN_TEST(test_bias_convergence_is_reasonably_quick);
  RUN_TEST(test_bad_dt_is_rejected);
  RUN_TEST(test_non_finite_inputs_are_rejected);
  RUN_TEST(test_single_outlier_is_softened);
  RUN_TEST(test_persistent_divergence_triggers_reset);
  RUN_TEST(test_full_chain_no_false_climb_in_turn);
  return UNITY_END();
}
