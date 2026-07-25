#pragma once

// États de référence des écrans, partagés entre les tests de comparaison
// d'images et le générateur d'images de référence.
//
// Le partage est indispensable : si le test et le générateur définissaient
// chacun leur état, ils dériveraient et la comparaison ne voudrait plus rien
// dire.

#include "ui/screens.h"

namespace reference {

inline vario::ui::DisplayState varioState() {
  vario::ui::DisplayState s;
  s.vario_ms = 2.4f;
  s.altitude_m = 2456.0f;
  s.temperature_c = 4.0f;
  s.qnh_hpa = 1013.25f;
  s.hour = 13;
  s.minute = 42;
  s.second = 7;
  s.flying = true;
  s.flight_seconds = 5025;  // 1:23:45
  s.stat_slot = 3;
  s.battery_percent = 72;
  s.ble_connected = true;

  // Rampe déterministe : pas de nombre aléatoire dans une image de référence.
  for (int i = 0; i < vario::ui::DisplayState::kTrackSize; ++i) {
    s.track_ms[i] = 1.5f + 0.05f * static_cast<float>(i);
  }
  s.track_count = vario::ui::DisplayState::kTrackSize;
  return s;
}

inline float calibrationProgress() {
  return 0.6f;
}

inline vario::ui::StatsView statsView() {
  vario::ui::StatsView stats;
  stats.slot = 3;
  stats.empty = false;
  stats.day = 12;
  stats.month = 5;
  stats.year = 2026;
  stats.start_hour = 13;
  stats.start_minute = 42;
  stats.duration_s = 5025;
  stats.alt_max_m = 2456.0f;
  stats.alt_min_m = 1180.0f;
  stats.best_climb_ms = 4.2f;
  stats.worst_sink_ms = -3.1f;
  stats.cumulative_gain_m = 1840.0f;
  return stats;
}

inline vario::ui::MenuView menuView() {
  vario::ui::MenuView menu;
  menu.section = "OPTIONS";
  menu.item = "MONTEE";
  menu.value = "0.2";
  menu.unit = "M/S";
  menu.editing = false;
  menu.index = 3;
  menu.count = 8;
  return menu;
}

}  // namespace reference
