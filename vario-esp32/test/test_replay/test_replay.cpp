#include <unity.h>

#include <cmath>
#include <cstring>
#include <vector>

#include "core/config_schema.h"
#include "core/raw_frame.h"
#include "core/replay.h"
#include "support/synth.h"

void setUp() {}
void tearDown() {}

namespace {

using vario::Config;
using vario::parseRawFrame;
using vario::RawFrame;
using vario::ReplayEngine;
using vario::ReplayOutput;

// Cadence de rejeu. On modélise une trame par pas ; le baromètre n'accompagne
// qu'une trame sur kBaroEvery, comme sur la cible où l'IMU tourne bien plus vite
// que le MS5611.
constexpr float kDt = 1.0f / 100.0f;
constexpr int kBaroEvery = 5;

// Convertit un échantillon synthétique cohérent (accel, gyro, pression tous
// dérivés d'une même vérité terrain) en trame brute, avec un baromètre présent
// une fois sur kBaroEvery.
RawFrame frameFromSample(const synth::Sample& s, vario::Micros t_us, int index) {
  RawFrame f;
  f.timestamp = t_us;
  f.accel = s.accel;
  f.gyro = s.gyro;
  if (index % kBaroEvery == 0) {
    f.has_baro = true;
    f.pressure_hpa = s.pressure_hpa;
    f.temperature_c = 15.0f;
  }
  return f;
}

// ---------------------------------------------------------------------------
// Aller-retour CSV du format de trame brute.
// ---------------------------------------------------------------------------

void test_raw_frame_round_trip() {
  RawFrame in;
  in.timestamp = 123456789LL;
  in.accel = vario::Vec3{0.12f, -9.71f, 0.34f};
  in.gyro = vario::Vec3{-0.01f, 0.02f, 0.5f};
  in.has_baro = true;
  in.pressure_hpa = 898.42f;
  in.temperature_c = 4.5f;

  char buf[160];
  const std::size_t n = vario::formatRawFrame(buf, sizeof(buf), in);
  TEST_ASSERT_GREATER_THAN(0u, n);

  RawFrame out;
  TEST_ASSERT_TRUE(parseRawFrame(buf, out));

  TEST_ASSERT_EQUAL_INT64(in.timestamp, out.timestamp);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, in.accel.x, out.accel.x);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, in.accel.y, out.accel.y);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, in.accel.z, out.accel.z);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, in.gyro.z, out.gyro.z);
  TEST_ASSERT_TRUE(out.has_baro);
  TEST_ASSERT_FLOAT_WITHIN(1e-2f, in.pressure_hpa, out.pressure_hpa);
  TEST_ASSERT_FLOAT_WITHIN(1e-2f, in.temperature_c, out.temperature_c);
}

void test_parser_skips_header_and_comments() {
  RawFrame out;
  TEST_ASSERT_FALSE(parseRawFrame(vario::kRawFrameCsvHeader, out));
  TEST_ASSERT_FALSE(parseRawFrame("# un commentaire", out));
  TEST_ASSERT_FALSE(parseRawFrame("", out));
  TEST_ASSERT_FALSE(parseRawFrame("\r\n", out));
  // Ligne valide avec un '\r' final (enregistrement Windows).
  TEST_ASSERT_TRUE(parseRawFrame("1000,0,0,-9.8,0,0,0,1,1013.25,15\r", out));
  TEST_ASSERT_EQUAL_INT64(1000, out.timestamp);
  TEST_ASSERT_TRUE(out.has_baro);
}

void test_parser_rejects_malformed() {
  RawFrame out;
  TEST_ASSERT_FALSE(parseRawFrame("1000,0,0", out));               // champs manquants
  TEST_ASSERT_FALSE(parseRawFrame("abc,0,0,0,0,0,0,0,0,0", out));  // temps non numérique
}

// ---------------------------------------------------------------------------
// Le critère d'acceptation phare : SILENCE AU REPOS.
//
// Une trace de trente secondes, appareil immobile sur une table, avec du bruit
// réaliste sur la pression. Rejouée à travers toute la chaîne, elle ne doit
// produire AUCUN son et un vario affiché sous 0,05 m/s. C'est le test que le
// montage de 2017 échouait.
// ---------------------------------------------------------------------------

void test_silence_at_rest() {
  Config config;  // seuils par défaut (montée 0,2 m/s)
  ReplayEngine engine(config);

  synth::Noise noise(2024u);
  std::vector<RawFrame> frames;
  vario::Micros t = 0;
  const vario::Micros dt_us = static_cast<vario::Micros>(kDt * 1e6f);

  for (int i = 0; i < 3000; ++i) {  // 30 s à 100 Hz
    synth::Sample s = synth::level(1000.0f);
    // Bruit barométrique réaliste (~0,12 m d'altitude équivalents).
    const float noisy_alt = 1000.0f + noise.altitudeNoiseM(0.12f);
    s.pressure_hpa = synth::pressureForAltitude(noisy_alt);
    frames.push_back(frameFromSample(s, t, i));
    t += dt_us;
  }

  // 200 premières trames pour l'alignement.
  TEST_ASSERT_TRUE(engine.align(frames.data(), 200));

  bool any_sound = false;
  float max_abs_vario = 0.0f;
  // On laisse passer la fenêtre d'inhibition (audio_gate, jusqu'à 4 s) avant
  // d'évaluer le silence.
  for (std::size_t i = 200; i < frames.size(); ++i) {
    const ReplayOutput o = engine.step(frames[i]);
    if (i < 200 + 500) continue;  // 5 s de stabilisation
    if (o.sounding) any_sound = true;
    max_abs_vario = std::max(max_abs_vario, std::fabs(o.vario_display_ms));
  }

  TEST_ASSERT_FALSE(any_sound);
  // TEST_ASSERT_LESS_THAN compare des entiers : on reste en flottant.
  TEST_ASSERT_TRUE(max_abs_vario < 0.05f);
}

// ---------------------------------------------------------------------------
// Non-régression : une montée franche est suivie fidèlement et fait sonner.
// ---------------------------------------------------------------------------

void test_climb_is_tracked_and_beeps() {
  Config config;
  ReplayEngine engine(config);

  std::vector<RawFrame> frames;
  vario::Micros t = 0;
  const vario::Micros dt_us = static_cast<vario::Micros>(kDt * 1e6f);

  // 3 s au repos pour l'alignement + stabilisation, puis 10 s de montée à
  // 2 m/s.
  for (int i = 0; i < 300; ++i) {
    frames.push_back(frameFromSample(synth::level(1000.0f), t, i));
    t += dt_us;
  }
  for (int i = 300; i < 1300; ++i) {
    const float t_s = static_cast<float>(i - 300) * kDt;
    frames.push_back(frameFromSample(synth::climb(2.0f, t_s, 1000.0f), t, i));
    t += dt_us;
  }

  TEST_ASSERT_TRUE(engine.align(frames.data(), 200));

  float final_vario = 0.0f;
  bool sounded_in_climb = false;
  for (std::size_t i = 200; i < frames.size(); ++i) {
    const ReplayOutput o = engine.step(frames[i]);
    final_vario = o.vario_display_ms;
    if (i > 900 && o.sounding) sounded_in_climb = true;  // bien installé en montée
  }

  TEST_ASSERT_FLOAT_WITHIN(0.25f, 2.0f, final_vario);
  TEST_ASSERT_TRUE(sounded_in_climb);
}

// ---------------------------------------------------------------------------
// Déterminisme : deux rejeux du même flux donnent des sorties identiques.
// C'est ce qui fait d'une trace un test de non-régression fiable.
// ---------------------------------------------------------------------------

void test_replay_is_deterministic() {
  Config config;
  std::vector<RawFrame> frames;
  vario::Micros t = 0;
  const vario::Micros dt_us = static_cast<vario::Micros>(kDt * 1e6f);
  for (int i = 0; i < 800; ++i) {
    const float t_s = static_cast<float>(i) * kDt;
    frames.push_back(frameFromSample(synth::climb(1.0f, t_s, 1000.0f), t, i));
    t += dt_us;
  }

  ReplayEngine a(config);
  ReplayEngine b(config);
  TEST_ASSERT_TRUE(a.align(frames.data(), 200));
  TEST_ASSERT_TRUE(b.align(frames.data(), 200));

  for (std::size_t i = 200; i < frames.size(); ++i) {
    const ReplayOutput oa = a.step(frames[i]);
    const ReplayOutput ob = b.step(frames[i]);
    TEST_ASSERT_EQUAL_FLOAT(oa.altitude_m, ob.altitude_m);
    TEST_ASSERT_EQUAL_FLOAT(oa.vario_display_ms, ob.vario_display_ms);
    TEST_ASSERT_EQUAL(oa.sounding, ob.sounding);
    TEST_ASSERT_EQUAL(oa.frequency_hz, ob.frequency_hz);
  }
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_raw_frame_round_trip);
  RUN_TEST(test_parser_skips_header_and_comments);
  RUN_TEST(test_parser_rejects_malformed);
  RUN_TEST(test_silence_at_rest);
  RUN_TEST(test_climb_is_tracked_and_beeps);
  RUN_TEST(test_replay_is_deterministic);
  return UNITY_END();
}
