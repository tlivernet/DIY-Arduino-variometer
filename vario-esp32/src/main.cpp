// Point d'entrée du firmware.
//
// setup() assemble l'appareil et crée les tâches ; loop() ne fait que de
// l'entretien. Toute la logique vit dans les tâches, et le domaine métier dans
// src/core, qui ne dépend ni d'Arduino ni de l'ESP32 et se teste sur PC.

#include <Arduino.h>

#include "app/vario_app.h"

namespace {
vario::app::VarioApp g_app;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("Variometre ESP32-S3"));

  if (!g_app.begin()) {
    // On ne s'arrête pas : l'écran doit pouvoir afficher le défaut, et le reste
    // de l'appareil rester utilisable.
    Serial.println(F("ATTENTION : un capteur n'a pas repondu"));
  }
}

void loop() {
  // Commandes série : « r » démarre l'émission des trames brutes (harnais de
  // rejeu, à enregistrer avec tools/record.py), « s » l'arrête.
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == 'r' || c == 'R') {
      g_app.setRecording(true);
    } else if (c == 's' || c == 'S') {
      g_app.setRecording(false);
    } else if (c == 'w' || c == 'W') {
      g_app.setPortal(true);
      Serial.println(F("Portail Wi-Fi ouvert : reseau \"Variometre\""));
    } else if (c == 'x' || c == 'X') {
      g_app.setPortal(false);
      Serial.println(F("Portail Wi-Fi ferme"));
    }
  }

  // Les tâches font le travail. Ici, seulement du diagnostic sur le port série,
  // une fois par seconde — mais pas pendant un enregistrement, pour ne pas
  // polluer le flux CSV.
  static std::uint32_t last_report_ms = 0;
  const std::uint32_t now_ms = millis();

  if (!g_app.recording() && now_ms - last_report_ms >= 1000) {
    last_report_ms = now_ms;

    vario::app::VarioState state;
    if (g_app.latestState(state)) {
      Serial.printf("alt=%.1f m  vario=%+.2f m/s  p=%.2f hPa  t=%.1f C  son=%s\n",
                    state.altitude_m, state.vario_display_ms, state.pressure_hpa,
                    state.temperature_c, state.audio_muted ? "coupe" : "actif");
    }
  }
  delay(50);
}
