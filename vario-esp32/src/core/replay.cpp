#include "core/replay.h"

#include "core/isa.h"

namespace vario {

ReplayEngine::ReplayEngine(const Config& config) : config_(config) {
  clampConfig(config_);

  // Report des réglages dans les filtres, identique à VarioApp::begin().
  attitude_.config().trim_speed_ms = config_.trim_speed_ms;
  tones_.config().climb_threshold_ms = config_.climb_threshold_ms;
  tones_.config().sink_threshold_ms = config_.sink_threshold_ms;
  tones_.config().profile = config_.tone_profile;
}

bool ReplayEngine::align(const RawFrame* rest_frames, std::size_t count) {
  if (rest_frames == nullptr || count == 0) return false;

  Vec3 accel_sum;
  Vec3 gyro_sum;
  bool have_baro = false;
  float pressure_hpa = last_pressure_hpa_;
  float temperature_c = last_temperature_c_;
  Micros last_ts = rest_frames[0].timestamp;

  for (std::size_t i = 0; i < count; ++i) {
    const RawFrame& f = rest_frames[i];
    accel_sum = accel_sum + f.accel;
    gyro_sum = gyro_sum + f.gyro;
    if (f.has_baro) {
      have_baro = true;
      pressure_hpa = f.pressure_hpa;
      temperature_c = f.temperature_c;
    }
    last_ts = f.timestamp;
  }

  const float inv = 1.0f / static_cast<float>(count);
  attitude_.align(accel_sum * inv, gyro_sum * inv);

  // Sans mesure barométrique pendant la fenêtre de repos, on ne peut pas
  // amorcer proprement l'altitude : c'est une trace inexploitable.
  if (!have_baro) return false;

  const float altitude =
      pressureToAltitude(config_.qnh_hpa, pressure_hpa) + config_.altitude_offset_m;
  kalman_.reset(altitude);
  flight_.reset(altitude, last_ts);
  audio_gate_.onAligned(last_ts);

  last_timestamp_ = last_ts;
  last_pressure_hpa_ = pressure_hpa;
  last_temperature_c_ = temperature_c;
  aligned_ = true;
  started_ = true;
  return true;
}

ReplayOutput ReplayEngine::step(const RawFrame& frame) {
  ReplayOutput out;
  out.timestamp = frame.timestamp;

  if (!aligned_) return out;

  const Micros now = frame.timestamp;

  // 1. Centrale inertielle, à chaque trame — comme sensorLoop().
  const float imu_dt_s = toSeconds(now - last_timestamp_);
  attitude_.update(frame.accel, frame.gyro, imu_dt_s);
  kalman_.predict(attitude_.verticalAccel(frame.accel), imu_dt_s);

  // 2. Baromètre, seulement quand la trame en porte un.
  if (frame.has_baro) {
    last_pressure_hpa_ = frame.pressure_hpa;
    last_temperature_c_ = frame.temperature_c;
    const float altitude = pressureToAltitude(config_.qnh_hpa, frame.pressure_hpa) +
                           config_.altitude_offset_m;
    kalman_.update(altitude);
  }

  // 3. Lissages du vario, sur le Δt réel de la trame — comme sensorLoop, qui
  //    utilise sa période nominale ; ici on dispose du Δt exact, plus fidèle.
  const float dt_s = imu_dt_s;
  out.altitude_m = kalman_.altitude();
  out.vario_audio_ms = vario_audio_.update(kalman_.velocity(), dt_s);
  out.vario_display_ms = vario_display_.update(kalman_.velocity(), dt_s);
  out.innovation_m = kalman_.innovation();

  // 4. Inhibition du son (audio_gate) puis machine à états du bipeur, dans le
  //    même ordre que sensorLoop + audioLoop.
  out.audio_muted = audio_gate_.update(out.vario_display_ms, out.innovation_m, now) ||
                    config_.volume <= 0;

  tones_.config().muted = config_.volume <= 0;
  tones_.config().inhibited = out.audio_muted;
  const ToneOutput tone = tones_.update(out.vario_audio_ms, now);
  out.sounding = tone.sounding;
  out.frequency_hz = tone.frequency_hz;

  // 5. Détection de vol, cadence libre (la logique est temporelle).
  const FlightDetector::Event ev = flight_.update(out.altitude_m, now);
  (void)ev;
  out.flying = flight_.flying();

  last_timestamp_ = now;
  return out;
}

}  // namespace vario
