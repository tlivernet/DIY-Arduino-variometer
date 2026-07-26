#include "app/vario_app.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cmath>

#include "app/pins.h"

namespace vario::app {
namespace {

// Cadences des tâches.
constexpr int kSensorPeriodMs = 2;  // 500 Hz
constexpr int kAudioPeriodMs = 5;   // quantifie les notes à 5 ms, inaudible
constexpr int kUiPeriodMs = 100;    // 10 Hz
constexpr int kRadioPeriodMs = 20;  // 50 Hz : sert le HTTP sans latence sensible

// Une trame LK8EX1 par seconde suffit à XCTrack : c'est la cadence qu'il
// attend, et cela épargne la radio.
constexpr int kBlePeriodMs = 1000;

// Cœurs et priorités. Tout le temps réel est sur le cœur 1 ; la radio (Wi-Fi et
// BLE) est sur le cœur 0, où vit sa pile et où elle peut bloquer longuement sans
// jamais faire hoqueter le son.
constexpr int kRealtimeCore = 1;
constexpr int kRadioCore = 0;
constexpr int kSensorPriority = 10;
constexpr int kAudioPriority = 9;
constexpr int kUiPriority = 5;
constexpr int kRadioPriority = 4;

Micros nowMicros() {
  return static_cast<Micros>(esp_timer_get_time());
}

}  // namespace

VarioApp::VarioApp()
    : display_(hal::SharpDisplay::Config{pins::kDisplaySclk, pins::kDisplayMosi,
                                         pins::kDisplayCs, pins::kDisplayDisp,
                                         pins::kDisplayExtcomin}),
      audio_(hal::LedcAudio::Config{pins::kBuzzerA, pins::kBuzzerB}),
      buttons_io_(pins::kButtonUp, pins::kButtonOk, pins::kButtonDown),
      battery_(pins::kBatterySense, pins::kBatteryDividerRatio),
      canvas_(framebuffer_, 400, 240) {}

bool VarioApp::alignAtRest() {
  // Deux secondes d'échantillons, à ~2 ms d'intervalle.
  constexpr int kSamples = 500;

  Vec3 accel_sum;
  Vec3 gyro_sum;
  int taken = 0;

  for (int i = 0; i < kSamples; ++i) {
    Vec3 accel;
    Vec3 gyro;
    if (imu_.read(accel, gyro)) {
      accel_sum = accel_sum + accel;
      gyro_sum = gyro_sum + gyro;
      ++taken;
    }
    delay(kSensorPeriodMs);
  }
  if (taken < kSamples / 2) return false;

  const float inv = 1.0f / static_cast<float>(taken);
  attitude_.align(accel_sum * inv, gyro_sum * inv);

  // Le baromètre a eu le temps de fournir plusieurs mesures pendant la fenêtre.
  for (int i = 0; i < 200 && !baro_.ready(); ++i) {
    baro_.service(nowMicros());
    delay(kSensorPeriodMs);
  }

  const float altitude = pressureToAltitude(config_.qnh_hpa, baro_.pressureHpa()) +
                         config_.altitude_offset_m;
  kalman_.reset(altitude);
  flight_.reset(altitude, nowMicros());
  audio_gate_.onAligned(nowMicros());
  return true;
}

bool VarioApp::begin() {
  // Réglages relus depuis la flash AVANT tout : ils pilotent seuils, QNH,
  // profil de son, volume. Une première mise en route sans réglages stockés
  // garde simplement les défauts.
  store_.load(config_);
  clampConfig(config_);

  Wire.begin(pins::kI2cSda, pins::kI2cScl, pins::kI2cFrequency);

  display_.begin();
  audio_.begin();
  buttons_io_.begin();
  battery_.begin();

  // Écran de calibration affiché tout de suite : l'utilisateur voit que
  // l'appareil vit, et qu'il ne doit pas le bouger.
  ui::drawCalibrationScreen(canvas_, 0.0f);
  display_.flushAll(canvas_);

  const bool baro_ok = baro_.begin();
  const bool imu_ok = imu_.begin();
  sensors_ok_ = baro_ok && imu_ok;

  attitude_.config().trim_speed_ms = config_.trim_speed_ms;
  tones_.config().climb_threshold_ms = config_.climb_threshold_ms;
  tones_.config().sink_threshold_ms = config_.sink_threshold_ms;
  tones_.config().profile = config_.tone_profile;

  state_queue_ = xQueueCreate(1, sizeof(VarioState));
  if (state_queue_ == nullptr) return false;

  if (sensors_ok_) {
    if (!alignAtRest()) sensors_ok_ = false;
  }

  // La tâche capteurs est créée même si un capteur manque : l'écran doit
  // pouvoir afficher le défaut plutôt que de rester figé.
  xTaskCreatePinnedToCore(sensorTaskEntry, "sensor", 4096, this, kSensorPriority, nullptr,
                          kRealtimeCore);
  xTaskCreatePinnedToCore(audioTaskEntry, "audio", 2560, this, kAudioPriority, nullptr,
                          kRealtimeCore);
  xTaskCreatePinnedToCore(uiTaskEntry, "ui", 6144, this, kUiPriority, nullptr,
                          kRealtimeCore);

  // Tâche radio sur le cœur 0 : elle gère le BLE (émission LK8EX1) et, à la
  // demande, le portail Wi-Fi. Isolée du temps réel du son par le cœur.
  xTaskCreatePinnedToCore(radioTaskEntry, "radio", 8192, this, kRadioPriority, nullptr,
                          kRadioCore);

  return sensors_ok_;
}

void VarioApp::setPortal(bool on) {
  // On délègue l'ouverture/fermeture effective à la tâche radio : Wi-Fi et
  // serveur HTTP doivent démarrer sur le cœur 0. Ici on ne fait que poser
  // l'intention.
  portal_open_ = on;
}

void VarioApp::setRecording(bool on) {
  if (on && !recording_) {
    // En-tête en tête de flux, pour que record.py écrive un CSV autonome.
    Serial.println(kRawFrameCsvHeader);
  }
  recording_ = on;
}

void VarioApp::publish(const VarioState& state) {
  if (state_queue_ == nullptr) return;
  // File de longueur 1 en écrasement : la valeur la plus récente sans blocage,
  // sans verrou et sans risque d'inversion de priorité.
  xQueueOverwrite(static_cast<QueueHandle_t>(state_queue_), &state);
}

bool VarioApp::latestState(VarioState& out) const {
  if (state_queue_ == nullptr) return false;
  return xQueuePeek(static_cast<QueueHandle_t>(state_queue_), &out, 0) == pdTRUE;
}

void VarioApp::sensorTaskEntry(void* arg) {
  static_cast<VarioApp*>(arg)->sensorLoop();
}
void VarioApp::audioTaskEntry(void* arg) {
  static_cast<VarioApp*>(arg)->audioLoop();
}
void VarioApp::uiTaskEntry(void* arg) {
  static_cast<VarioApp*>(arg)->uiLoop();
}
void VarioApp::radioTaskEntry(void* arg) {
  static_cast<VarioApp*>(arg)->radioLoop();
}

void VarioApp::sensorLoop() {
  TickType_t last_wake = xTaskGetTickCount();
  Micros last_imu = nowMicros();

  for (;;) {
    const Micros now = nowMicros();

    // Trame brute, remplie au fil des lectures et émise en fin d'itération si
    // l'enregistrement est actif.
    RawFrame frame;
    frame.timestamp = now;
    bool imu_read = false;

    if (sensors_ok_) {
      // 1. Centrale inertielle, à chaque tick.
      Vec3 accel;
      Vec3 gyro;
      if (imu_.read(accel, gyro)) {
        imu_read = true;
        frame.accel = accel;
        frame.gyro = gyro;
        const float dt_s = toSeconds(now - last_imu);
        last_imu = now;
        attitude_.update(accel, gyro, dt_s);
        kalman_.predict(attitude_.verticalAccel(accel), dt_s);
      }

      // 2. Baromètre, NON BLOQUANT : service() lance une conversion et rend la
      // main. Le résultat arrive une dizaine de millisecondes plus tard, sans
      // que rien n'ait attendu.
      if (baro_.service(now)) {
        frame.has_baro = true;
        frame.pressure_hpa = baro_.pressureHpa();
        frame.temperature_c = baro_.temperatureC();
        const float altitude = pressureToAltitude(config_.qnh_hpa, baro_.pressureHpa()) +
                               config_.altitude_offset_m;
        kalman_.update(altitude);
      }
    }

    // Émission de la trame brute pour le harnais de rejeu. On n'émet que les
    // trames où la centrale a effectivement fourni un échantillon : une ligne
    // sans accéléromètre n'aurait aucun sens au rejeu.
    if (recording_ && imu_read) {
      char csv[160];
      if (formatRawFrame(csv, sizeof(csv), frame) > 0) {
        Serial.println(csv);
      }
    }

    // 3. Publication.
    const float dt_s = static_cast<float>(kSensorPeriodMs) / 1000.0f;
    VarioState state;
    state.timestamp = now;
    state.altitude_m = kalman_.altitude();
    state.vario_audio_ms = vario_audio_.update(kalman_.velocity(), dt_s);
    state.vario_display_ms = vario_display_.update(kalman_.velocity(), dt_s);
    state.pressure_hpa = baro_.pressureHpa();
    state.temperature_c = baro_.temperatureC();
    state.innovation_m = kalman_.innovation();
    state.sensors_ok = sensors_ok_;
    state.audio_muted =
        audio_gate_.update(state.vario_display_ms, state.innovation_m, now) ||
        config_.volume <= 0;
    publish(state);

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kSensorPeriodMs));
  }
}

void VarioApp::audioLoop() {
  TickType_t last_wake = xTaskGetTickCount();

  for (;;) {
    VarioState state;
    if (latestState(state)) {
      tones_.config().muted = config_.volume <= 0;
      tones_.config().inhibited = state.audio_muted;

      const ToneOutput out = tones_.update(state.vario_audio_ms, nowMicros());
      if (out.sounding) {
        audio_.tone(out.frequency_hz, config_.volume);
      } else {
        audio_.silence();
      }
    }
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kAudioPeriodMs));
  }
}

void VarioApp::uiLoop() {
  TickType_t last_wake = xTaskGetTickCount();
  bool aligned_shown = false;

  for (;;) {
    const std::uint32_t now_ms = millis();

    // Basculement VCOM : obligatoire, sous peine de rémanence permanente du
    // cristal liquide.
    display_.maintain(now_ms);

    VarioState state;
    if (latestState(state)) {
      if (state.audio_muted && !aligned_shown) {
        ui::drawCalibrationScreen(canvas_, 0.5f);
      } else {
        aligned_shown = true;

        ui::DisplayState view;
        view.vario_ms = state.vario_display_ms;
        view.altitude_m = state.altitude_m;
        view.temperature_c = state.temperature_c;
        view.qnh_hpa = config_.qnh_hpa;
        view.flying = flight_.flying();
        view.battery_percent =
            static_cast<int>(100.0f * batteryFraction(battery_.smoothedVolts(
                                          static_cast<float>(kUiPeriodMs) / 1000.0f)));
        view.audio_muted = config_.volume <= 0;
        ui::drawVarioScreen(canvas_, view);
      }

      // Seules les lignes réellement modifiées partent sur le bus : une trame
      // complète coûterait ~100 ms à 1 MHz, incompatible avec 10 Hz.
      display_.flushDirty(canvas_);
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kUiPeriodMs));
  }
}

void VarioApp::radioLoop() {
  TickType_t last_wake = xTaskGetTickCount();
  std::uint32_t last_ble_ms = 0;
  bool ble_started = false;

  for (;;) {
    // 1. BLE : suit le réglage. On démarre la pile à la première demande et on
    //    la coupe si l'utilisateur désactive le Bluetooth, pour l'énergie.
    if (config_.ble_enabled && !ble_started) {
      ble_.begin();
      ble_started = true;
    } else if (!config_.ble_enabled && ble_started) {
      ble_.end();
      ble_started = false;
    }

    // 2. Portail Wi-Fi : ouvert/fermé à la demande (portal_open_).
    if (portal_open_ && !portal_.running()) {
      portal_.begin();
    } else if (!portal_open_ && portal_.running()) {
      portal_.end();
    }
    if (portal_.running()) {
      portal_.handle();
      // Un réglage enregistré via la page : on le persiste et on répercute dans
      // les filtres, sans quoi le changement ne prendrait effet qu'au redémarrage.
      if (portal_.takeDirty()) {
        clampConfig(config_);
        store_.save(config_);
        attitude_.config().trim_speed_ms = config_.trim_speed_ms;
        tones_.config().climb_threshold_ms = config_.climb_threshold_ms;
        tones_.config().sink_threshold_ms = config_.sink_threshold_ms;
        tones_.config().profile = config_.tone_profile;
      }
    }

    // 3. Émission LK8EX1 à ~1 Hz, depuis le dernier état publié.
    const std::uint32_t now_ms = millis();
    if (ble_started && now_ms - last_ble_ms >= kBlePeriodMs) {
      last_ble_ms = now_ms;
      VarioState state;
      if (latestState(state)) {
        Lk8ex1Data frame;
        frame.pressure_pa = state.pressure_hpa * 100.0f;
        frame.vario_ms = state.vario_audio_ms;
        frame.temperature_c = state.temperature_c;
        frame.battery_percent =
            static_cast<int>(100.0f * batteryFraction(battery_.smoothedVolts(
                                          static_cast<float>(kBlePeriodMs) / 1000.0f)));
        frame.pressure_valid = state.sensors_ok;
        ble_.publish(frame);
      }
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kRadioPeriodMs));
  }
}

}  // namespace vario::app
