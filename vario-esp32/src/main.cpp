// Point d'entrée du firmware — étape P0.
//
// À ce stade le dépôt ne contient que le coeur métier (src/core), qui est du
// C++ pur et couvert par les tests natifs. Les pilotes matériel (MS5611, IMU,
// écran Sharp, audio LEDC, boutons) et les tâches FreeRTOS arrivent à l'étape
// P1 : voir README.md.
//
// Ce main sert donc uniquement à vérifier que le coeur se compile bien pour la
// cible ESP32-S3 en plus de la machine de développement, et à donner un signe
// de vie sur le port série.

#include <Arduino.h>

#include "core/battery.h"
#include "core/flight_fsm.h"
#include "core/format.h"
#include "core/isa.h"
#include "core/stats.h"
#include "core/tone_scheduler.h"
#include "core/vario_filter.h"

namespace {

vario::VarioFilter g_filter;
vario::ToneScheduler g_tones;
vario::FlightDetector g_flight;

// Horloge monotone en microsecondes. esp_timer_get_time() renvoie un int64_t
// et ne déborde pas (~292 000 ans), contrairement au micros() sur 32 bits du
// montage AVR qui repassait à zéro toutes les 71,6 minutes.
vario::Micros nowMicros() {
  return static_cast<vario::Micros>(esp_timer_get_time());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println(F("Variometre ESP32-S3 - etape P0 (coeur metier uniquement)"));
  Serial.print(F("Altitude a 1013.25 hPa / 900 hPa : "));
  Serial.println(vario::pressureToAltitude(1013.25f, 900.0f));

  g_filter.reset(1013.25f, 1013.25f, 0.0f, nowMicros());
  g_flight.reset(g_filter.altitudeM(), nowMicros());
}

void loop() {
  // Pas encore de capteur : on rejoue la pression de référence pour montrer
  // que la chaîne tourne sans produire de NaN. Remplacé par la lecture du
  // MS5611 à l'étape P1.
  const vario::Micros now = nowMicros();
  g_filter.update(1013.25f, 1013.25f, 0.0f, now);

  const vario::ToneOutput tone = g_tones.update(g_filter.varioMs(), now);
  g_flight.update(g_filter.altitudeM(), now);

  static vario::Micros last_report = 0;
  if (now - last_report >= vario::kMicrosPerSecond) {
    last_report = now;

    char vario_text[8];
    vario::formatVarioTo(vario_text, sizeof(vario_text), g_filter.varioMs());

    Serial.print(F("alt="));
    Serial.print(g_filter.altitudeM(), 1);
    Serial.print(F(" m  vario="));
    Serial.print(vario_text);
    Serial.print(F(" m/s  son="));
    Serial.println(tone.sounding ? F("oui") : F("non"));
  }

  delay(10);
}
