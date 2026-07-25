#include "ui/screens.h"

#include <cmath>
#include <cstdio>

#include "core/format.h"
#include "ui/segment.h"

namespace vario::ui {
namespace {

// L'écran est lu en vision périphérique, en pilotant une aile. La contrainte de
// conception n'est pas la quantité d'information mais la lisibilité au soleil,
// à bout de bras. D'où un découpage en deux colonnes : la vitesse verticale
// occupe seule les deux tiers gauche, tout le reste est relégué à droite.
constexpr int kDividerX = 262;
constexpr int kRightX = kDividerX + 8;

// Bande analogique de vitesse verticale, en bas à gauche.
constexpr int kTapeX = 8;
constexpr int kTapeY = 142;
constexpr int kTapeW = 246;
constexpr int kTapeH = 26;
constexpr float kTapeRange = 5.0f;  // ± m/s pleine échelle

// Étiquettes de l'échelle, puis la courbe. Les trois blocs sont espacés pour
// qu'aucun texte n'en chevauche un autre.
constexpr int kTapeLabelY = kTapeY + kTapeH + 10;  // 178
constexpr int kTrackY = 196;
constexpr int kTrackH = 42;

void drawFlightTime(Canvas& canvas, int x, int y, std::uint32_t seconds, int scale) {
  const unsigned h = seconds / 3600;
  const unsigned m = (seconds % 3600) / 60;
  const unsigned s = seconds % 60;
  char text[16];
  std::snprintf(text, sizeof(text), "%u:%02u:%02u", h, m, s);
  canvas.drawText(x, y, text, true, scale);
}

// Bande analogique remplie depuis le centre. En vol, l'œil lit la BARRE ;
// les chiffres ne servent qu'au sol.
void drawTape(Canvas& canvas, float vario_ms) {
  canvas.drawRect(kTapeX, kTapeY, kTapeW, kTapeH, true);

  const int center = kTapeX + kTapeW / 2;
  const int usable = kTapeW / 2 - 3;

  // Graduations à chaque m/s, plus marquées aux valeurs impaires étiquetées.
  for (int v = -static_cast<int>(kTapeRange); v <= static_cast<int>(kTapeRange); ++v) {
    const int tick_x = center + static_cast<int>(static_cast<float>(v) / kTapeRange *
                                                 static_cast<float>(usable));
    const int tick_h = (v % 2 == 0) ? 4 : 7;
    canvas.vLine(tick_x, kTapeY + kTapeH, tick_h, true);
  }

  // Repère central appuyé.
  canvas.vLine(center, kTapeY + 1, kTapeH - 2, true);

  if (!std::isfinite(vario_ms)) return;

  float clamped = vario_ms;
  if (clamped > kTapeRange) clamped = kTapeRange;
  if (clamped < -kTapeRange) clamped = -kTapeRange;

  const int length =
      static_cast<int>(std::fabs(clamped) / kTapeRange * static_cast<float>(usable));
  if (length <= 0) return;

  if (clamped > 0.0f) {
    canvas.fillRect(center + 1, kTapeY + 3, length, kTapeH - 6, true);
  } else {
    canvas.fillRect(center - length, kTapeY + 3, length, kTapeH - 6, true);
  }
}

// Étiquettes de l'échelle, sous la bande.
void drawTapeLabels(Canvas& canvas) {
  const int center = kTapeX + kTapeW / 2;
  const int usable = kTapeW / 2 - 3;
  const int y = kTapeLabelY;

  for (int v = -4; v <= 4; v += 2) {
    if (v == 0) continue;
    char label[8];
    std::snprintf(label, sizeof(label), "%d", v);
    const int x = center + static_cast<int>(static_cast<float>(v) / kTapeRange *
                                            static_cast<float>(usable));
    canvas.drawTextCentered(x - 8, y, 16, label, true, 1);
  }
  canvas.drawTextCentered(center - 8, y, 16, "0", true, 1);
}

// Courbe des 30 dernières secondes : la seule information réellement nouvelle
// que le grand écran permet d'ajouter par rapport au 84×48 de 2017.
//
// Tracée en polyligne d'un pixel. Des points épais reliés par des barres
// verticales donnaient un aspect de chenille illisible.
void drawTrack(Canvas& canvas, const DisplayState& state) {
  const int x0 = kTapeX;
  const int w = kTapeW;
  const int mid = kTrackY + kTrackH / 2;

  // Ligne de zéro en pointillé : présente comme référence, mais elle ne doit
  // pas concurrencer la courbe.
  for (int x = x0; x < x0 + w; x += 4) canvas.setPixel(x, mid, true);

  if (state.track_count <= 1) return;

  const int count = state.track_count < DisplayState::kTrackSize
                        ? state.track_count
                        : DisplayState::kTrackSize;
  const float scale = static_cast<float>(kTrackH / 2 - 1) / kTapeRange;

  const auto sample_y = [&](int i) {
    float clamped = std::isfinite(state.track_ms[i]) ? state.track_ms[i] : 0.0f;
    if (clamped > kTapeRange) clamped = kTapeRange;
    if (clamped < -kTapeRange) clamped = -kTapeRange;
    return mid - static_cast<int>(clamped * scale);
  };
  const auto sample_x = [&](int i) { return x0 + i * (w - 1) / (count - 1); };

  for (int i = 1; i < count; ++i) {
    canvas.drawLine(sample_x(i - 1), sample_y(i - 1), sample_x(i), sample_y(i), true);
  }
}

void drawBattery(Canvas& canvas, int x, int y, int percent) {
  constexpr int kBodyW = 34;
  constexpr int kBodyH = 14;

  canvas.drawRect(x, y, kBodyW, kBodyH, true);
  canvas.fillRect(x + kBodyW, y + 4, 3, kBodyH - 8, true);

  int clamped = percent;
  if (clamped < 0) clamped = 0;
  if (clamped > 100) clamped = 100;

  const int fill = (kBodyW - 4) * clamped / 100;
  if (fill > 0) canvas.fillRect(x + 2, y + 2, fill, kBodyH - 4, true);

  char text[8];
  std::snprintf(text, sizeof(text), "%d%%", clamped);
  canvas.drawText(x + kBodyW + 10, y + 4, text, true, 1);
}

}  // namespace

void drawVarioScreen(Canvas& canvas, const DisplayState& state) {
  canvas.clear(false);

  // --- Vitesse verticale, l'information principale ---
  const VarioText vario = formatVario(state.vario_ms);
  char vario_text[16];
  std::snprintf(vario_text, sizeof(vario_text), "%c%d.%d", vario.sign, vario.whole,
                vario.tenth);

  SegmentMetrics big;
  big.width = 58;
  big.height = 108;
  big.thickness = 13;
  big.spacing = 7;

  drawSegmentText(canvas, 4, 20, vario_text, big, true);
  canvas.drawText(198, 104, "M/S", true, 3);

  // --- Colonne de droite ---
  canvas.vLine(kDividerX, 0, kScreenHeight, true);

  // Altitude.
  canvas.drawText(kRightX, 4, "ALTI", true, 1);
  char altitude_text[12];
  const int altitude = static_cast<int>(
      std::lround(std::isfinite(state.altitude_m) ? state.altitude_m : 0.0f));
  std::snprintf(altitude_text, sizeof(altitude_text), "%d", altitude);

  SegmentMetrics medium;
  medium.width = 26;
  medium.height = 42;
  medium.thickness = 6;
  medium.spacing = 4;
  drawSegmentText(canvas, kRightX, 15, altitude_text, medium, true);

  // Unité et QNH sur la même ligne : le QNH conditionne toutes les altitudes
  // affichées, il doit donc rester lisible en vol.
  char qnh[24];
  std::snprintf(qnh, sizeof(qnh), "M    QNH %d",
                static_cast<int>(std::lround(state.qnh_hpa)));
  canvas.drawText(kRightX + 2, 62, qnh, true, 1);

  canvas.hLine(kDividerX + 1, 74, kScreenWidth - kDividerX - 1, true);

  // Heure.
  char clock[16];
  std::snprintf(clock, sizeof(clock), "%02d:%02d:%02d", state.hour, state.minute,
                state.second);
  canvas.drawText(kRightX, 82, clock, true, 2);

  // Chrono de vol et emplacement de statistiques.
  char slot[8];
  std::snprintf(slot, sizeof(slot), "M%d", state.stat_slot);
  canvas.drawText(kRightX, 106, state.flying ? "VOL" : "SOL", true, 1);
  canvas.drawText(kRightX + 30, 106, slot, true, 1);
  drawFlightTime(canvas, kRightX, 118, state.flight_seconds, 2);

  canvas.hLine(kDividerX + 1, 140, kScreenWidth - kDividerX - 1, true);

  // Batterie.
  drawBattery(canvas, kRightX, 148, state.battery_percent);

  // Température.
  char temperature[12];
  std::snprintf(temperature, sizeof(temperature), "%d C",
                static_cast<int>(std::lround(state.temperature_c)));
  canvas.drawText(kRightX, 172, temperature, true, 2);

  canvas.hLine(kDividerX + 1, 196, kScreenWidth - kDividerX - 1, true);

  // Indicateurs d'état. Le son coupé est affiché en inverse : c'est l'état le
  // plus important à ne pas manquer sur un instrument qui se pilote à l'oreille.
  if (state.audio_muted) {
    canvas.fillRect(kRightX - 2, 204, 56, 12, true);
    canvas.drawText(kRightX + 2, 207, "MUET", false, 1);
  } else {
    canvas.drawText(kRightX + 2, 207, "SON", true, 1);
  }
  if (state.ble_connected) canvas.drawText(kRightX + 2, 222, "BLE", true, 1);
  if (state.wifi_active) canvas.drawText(kRightX + 40, 222, "WIFI", true, 1);

  // --- Bas de la colonne de gauche ---
  drawTape(canvas, state.vario_ms);
  drawTapeLabels(canvas);
  drawTrack(canvas, state);
}

void drawCalibrationScreen(Canvas& canvas, float progress) {
  canvas.clear(false);

  canvas.drawTextCentered(0, 40, kScreenWidth, "VARIOMETRE", true, 4);
  canvas.drawTextCentered(0, 90, kScreenWidth, "CALIBRATION", true, 3);
  canvas.drawTextCentered(0, 120, kScreenWidth, "NE PAS BOUGER", true, 2);

  // Barre de progression.
  constexpr int kBarX = 80;
  constexpr int kBarY = 160;
  constexpr int kBarW = 240;
  constexpr int kBarH = 20;

  canvas.drawRect(kBarX, kBarY, kBarW, kBarH, true);

  float clamped = std::isfinite(progress) ? progress : 0.0f;
  if (clamped < 0.0f) clamped = 0.0f;
  if (clamped > 1.0f) clamped = 1.0f;
  const int fill = static_cast<int>(static_cast<float>(kBarW - 4) * clamped);
  if (fill > 0) canvas.fillRect(kBarX + 2, kBarY + 2, fill, kBarH - 4, true);

  canvas.drawTextCentered(0, 200, kScreenWidth, "SON COUPE PENDANT LA MESURE", true, 1);
}

void drawStatsScreen(Canvas& canvas, const StatsView& stats) {
  canvas.clear(false);

  char header[24];
  std::snprintf(header, sizeof(header), "STATISTIQUES M%d", stats.slot);
  canvas.fillRect(0, 0, kScreenWidth, 22, true);
  canvas.drawText(6, 8, header, false, 2);

  if (stats.empty) {
    canvas.drawTextCentered(0, 100, kScreenWidth, "EMPLACEMENT VIDE", true, 3);
    return;
  }

  char line[40];
  int y = 34;
  constexpr int kRow = 26;

  std::snprintf(line, sizeof(line), "%02d/%02d/%04d  %02d:%02d", stats.day, stats.month,
                stats.year, stats.start_hour, stats.start_minute);
  canvas.drawText(8, y, line, true, 2);
  y += kRow;

  const unsigned h = stats.duration_s / 3600;
  const unsigned m = (stats.duration_s % 3600) / 60;
  const unsigned s = stats.duration_s % 60;
  std::snprintf(line, sizeof(line), "DUREE    %u:%02u:%02u", h, m, s);
  canvas.drawText(8, y, line, true, 2);
  y += kRow;

  std::snprintf(line, sizeof(line), "ALT MAX  %d M",
                static_cast<int>(std::lround(stats.alt_max_m)));
  canvas.drawText(8, y, line, true, 2);
  y += kRow;

  std::snprintf(line, sizeof(line), "ALT MIN  %d M",
                static_cast<int>(std::lround(stats.alt_min_m)));
  canvas.drawText(8, y, line, true, 2);
  y += kRow;

  char climb[8];
  char sink[8];
  formatVarioTo(climb, sizeof(climb), stats.best_climb_ms);
  formatVarioTo(sink, sizeof(sink), stats.worst_sink_ms);
  std::snprintf(line, sizeof(line), "M/S      %s / %s", climb, sink);
  canvas.drawText(8, y, line, true, 2);
  y += kRow;

  std::snprintf(line, sizeof(line), "CUMUL    %d M",
                static_cast<int>(std::lround(stats.cumulative_gain_m)));
  canvas.drawText(8, y, line, true, 2);
}

void drawMenuScreen(Canvas& canvas, const MenuView& menu) {
  canvas.clear(false);

  // Bandeau de rubrique en inverse.
  canvas.fillRect(0, 0, kScreenWidth, 22, true);
  canvas.drawText(6, 8, menu.section, false, 2);

  // Position dans la liste, à droite du bandeau. Le tampon est dimensionné
  // pour le pire cas de deux int, pour qu'aucune troncature ne soit possible.
  char position[24];
  std::snprintf(position, sizeof(position), "%d/%d", menu.index + 1, menu.count);
  canvas.drawText(kScreenWidth - Canvas::textWidth(position, 2) - 6, 8, position, false,
                  2);

  // Entrée courante.
  canvas.drawText(10, 46, menu.item, true, 4);

  // Valeur : en inverse pendant l'édition, pour qu'on voie immédiatement que
  // tourner la commande va la modifier.
  const int value_y = 110;
  const int value_w = Canvas::textWidth(menu.value, 6);

  if (menu.editing) {
    canvas.fillRect(4, value_y - 8, value_w + 16, 60, true);
    canvas.drawText(12, value_y, menu.value, false, 6);
  } else {
    canvas.drawText(12, value_y, menu.value, true, 6);
  }

  if (menu.unit != nullptr && menu.unit[0] != '\0') {
    canvas.drawText(12 + value_w + 26, value_y + 24, menu.unit, true, 2);
  }

  canvas.hLine(0, 210, kScreenWidth, true);
  canvas.drawText(8, 220, menu.editing ? "VALIDER: OK" : "OK: MODIFIER", true, 1);
  canvas.drawText(240, 220, "APPUI LONG: RETOUR", true, 1);
}

}  // namespace vario::ui
