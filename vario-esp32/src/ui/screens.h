#pragma once

#include <cstdint>

#include "ui/canvas.h"

namespace vario::ui {

// Dimensions de l'écran de référence : Sharp Memory LCD LS027B7DH01.
constexpr int kScreenWidth = 400;
constexpr int kScreenHeight = 240;

// Tout ce qu'un écran a besoin de savoir. Volontairement plat et sans logique :
// les écrans ne calculent rien, ils affichent. C'est ce qui les rend testables
// par simple comparaison d'images.
struct DisplayState {
  float vario_ms = 0.0f;
  float altitude_m = 0.0f;
  float temperature_c = 15.0f;
  float qnh_hpa = 1013.25f;

  int hour = 12;
  int minute = 0;
  int second = 0;

  bool flying = false;
  std::uint32_t flight_seconds = 0;
  int stat_slot = 1;  // « M1 » à « M8 »

  int battery_percent = 100;

  bool ble_connected = false;
  bool wifi_active = false;
  bool audio_muted = false;

  // Historique de vitesse verticale pour la courbe du bas, du plus ancien au
  // plus récent. Sur 30 s à 1 Hz.
  static constexpr int kTrackSize = 30;
  float track_ms[kTrackSize] = {};
  int track_count = 0;
};

// Écran principal en vol.
void drawVarioScreen(Canvas& canvas, const DisplayState& state);

// Écran de calibration au démarrage, pendant que le filtre se stabilise.
// progress va de 0 à 1.
void drawCalibrationScreen(Canvas& canvas, float progress);

// Statistiques d'un vol enregistré.
struct StatsView {
  int slot = 1;
  bool empty = true;
  int day = 1;
  int month = 1;
  int year = 2026;
  int start_hour = 12;
  int start_minute = 0;
  std::uint32_t duration_s = 0;
  float alt_max_m = 0.0f;
  float alt_min_m = 0.0f;
  float best_climb_ms = 0.0f;
  float worst_sink_ms = 0.0f;
  float cumulative_gain_m = 0.0f;
};

void drawStatsScreen(Canvas& canvas, const StatsView& stats);

// Écran de menu : un titre de rubrique, l'entrée courante et sa valeur.
struct MenuView {
  const char* section = "OPTIONS";
  const char* item = "MONTEE";
  const char* value = "0.2";
  const char* unit = "M/S";
  bool editing = false;
  int index = 0;
  int count = 1;
};

void drawMenuScreen(Canvas& canvas, const MenuView& menu);

}  // namespace vario::ui
