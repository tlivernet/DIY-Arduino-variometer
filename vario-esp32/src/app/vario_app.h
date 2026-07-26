#pragma once

#include <cstdint>

#include "core/attitude.h"
#include "core/audio_gate.h"
#include "core/battery.h"
#include "core/buttons.h"
#include "core/config_schema.h"
#include "core/flight_fsm.h"
#include "core/isa.h"
#include "core/kalman.h"
#include "core/stats.h"
#include "core/tone_scheduler.h"
#include "core/vario_filter.h"
#include "hal/audio_ledc.h"
#include "hal/display_sharp.h"
#include "hal/inputs.h"
#include "hal/mpu6050.h"
#include "hal/ms5611.h"
#include "ui/canvas.h"
#include "ui/screens.h"

namespace vario::app {

// Instantané de l'état du vario, publié par la tâche capteurs et lu par toutes
// les autres. Type simple, sans pointeur, copiable tel quel dans une file.
struct VarioState {
  Micros timestamp = 0;
  float altitude_m = 0.0f;
  float vario_audio_ms = 0.0f;    // lissage rapide, pour le son
  float vario_display_ms = 0.0f;  // lissage lent, pour l'écran
  float pressure_hpa = 1013.25f;
  float temperature_c = 15.0f;
  float innovation_m = 0.0f;
  bool audio_muted = true;
  bool sensors_ok = false;
};

// Assemblage de l'appareil.
//
// RÉPARTITION SUR LES DEUX CŒURS — décision structurante.
//
// La pile Wi-Fi et BLE de l'ESP32 vit sur le cœur 0 et y bloque les
// interruptions pendant de longues périodes. Si le son partageait ce cœur, les
// bips deviendraient irréguliers dès l'ouverture du portail de configuration —
// défaut rédhibitoire sur un instrument qui se pilote à l'oreille.
//
//   Tâche      Cœur  Priorité  Cadence      Rôle
//   audio       1       9      5 ms         ordonnanceur de notes, LEDC
//   capteurs    1      10      2 ms         IMU, baromètre, Kalman
//   affichage   1       5      100 ms       rendu et transfert de l'écran
//   (radio)     0       -      -            Wi-Fi / BLE, étape suivante
//
// Le bus I2C est partagé entre le baromètre et la centrale inertielle, mais
// **seule la tâche capteurs y accède**, donc aucun verrou n'est nécessaire.
class VarioApp {
 public:
  VarioApp();

  // Initialise le matériel, effectue l'alignement à l'arrêt, puis crée les
  // tâches. Renvoie false si un capteur manque à l'appel.
  bool begin();

  // Dernier état publié.
  bool latestState(VarioState& out) const;

  const Config& config() const { return config_; }

 private:
  // Corps des tâches.
  static void sensorTaskEntry(void* arg);
  static void audioTaskEntry(void* arg);
  static void uiTaskEntry(void* arg);

  void sensorLoop();
  void audioLoop();
  void uiLoop();

  // Fenêtre d'alignement à l'arrêt : mesure la direction de la pesanteur et le
  // biais du gyromètre. Comme le bas estimé est DÉFINI comme la direction
  // moyenne mesurée, l'accélération verticale calculée est identiquement nulle
  // dans l'assiette de démarrage : le biais du Kalman part donc juste par
  // construction plutôt que de converger à l'aveugle.
  bool alignAtRest();

  void publish(const VarioState& state);

  Config config_;

  hal::Ms5611 baro_;
  hal::Mpu6050 imu_;
  hal::SharpDisplay display_;
  hal::LedcAudio audio_;
  hal::ButtonInputs buttons_io_;
  hal::BatterySense battery_;

  AttitudeFilter attitude_;
  VarioKalman kalman_;
  AudioGate audio_gate_;
  ToneScheduler tones_;
  FlightDetector flight_;
  StatsAccumulator stats_;
  Buttons buttons_;

  Ema vario_audio_{0.15f};
  Ema vario_display_{0.60f};

  // Tampon d'affichage : 400 × 240 en 1 bit par pixel, soit 12 ko.
  std::uint8_t framebuffer_[(400 + 7) / 8 * 240] = {};
  ui::Canvas canvas_;

  void* state_queue_ = nullptr;  // QueueHandle_t, gardé opaque dans l'en-tête
  bool sensors_ok_ = false;
};

}  // namespace vario::app
