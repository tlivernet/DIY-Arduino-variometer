#include "hal/display_sharp.h"

#include <Arduino.h>
#include <SPI.h>

namespace vario::hal {
namespace {

// Bits de l'octet de commande, avant inversion éventuelle.
constexpr std::uint8_t kModeWrite = 0x01;  // M0 : écriture de lignes
constexpr std::uint8_t kModeVcom = 0x02;   // M1 : état VCOM
constexpr std::uint8_t kModeClear = 0x04;  // M2 : effacement global

// Période de basculement VCOM : 500 ms, soit un signal carré à 1 Hz.
constexpr std::uint32_t kVcomPeriodMs = 500;

SPIClass* g_spi = nullptr;

}  // namespace

std::uint8_t reverseBits(std::uint8_t value) {
  value = static_cast<std::uint8_t>(((value & 0xF0u) >> 4) | ((value & 0x0Fu) << 4));
  value = static_cast<std::uint8_t>(((value & 0xCCu) >> 2) | ((value & 0x33u) << 2));
  value = static_cast<std::uint8_t>(((value & 0xAAu) >> 1) | ((value & 0x55u) << 1));
  return value;
}

void SharpDisplay::begin() {
  pinMode(config_.pin_cs, OUTPUT);
  digitalWrite(config_.pin_cs, LOW);  // CS actif HAUT : repos = bas

  pinMode(config_.pin_disp, OUTPUT);
  digitalWrite(config_.pin_disp, LOW);

  pinMode(config_.pin_extcomin, OUTPUT);
  digitalWrite(config_.pin_extcomin, LOW);

  if (g_spi == nullptr) g_spi = new SPIClass(HSPI);
  g_spi->begin(config_.pin_sclk, -1, config_.pin_mosi, -1);

  // Après la mise sous tension de DISP, la fiche technique demande d'attendre
  // avant d'activer SCS.
  digitalWrite(config_.pin_disp, HIGH);
  enabled_ = true;
  delayMicroseconds(100);

  clear();
}

void SharpDisplay::setEnabled(bool enabled) {
  enabled_ = enabled;
  digitalWrite(config_.pin_disp, enabled ? HIGH : LOW);
}

std::uint8_t SharpDisplay::controlByte() const {
  std::uint8_t value = kModeWrite;
  if (vcom_) value |= kModeVcom;
  return value;
}

void SharpDisplay::beginTransfer() {
  g_spi->beginTransaction(SPISettings(config_.frequency_hz, MSBFIRST, SPI_MODE0));
  digitalWrite(config_.pin_cs, HIGH);
  delayMicroseconds(6);
}

void SharpDisplay::endTransfer() {
  delayMicroseconds(6);
  digitalWrite(config_.pin_cs, LOW);
  g_spi->endTransaction();
}

void SharpDisplay::writeControl(std::uint8_t value) {
  g_spi->transfer(config_.reverse_control_bytes ? reverseBits(value) : value);
}

void SharpDisplay::writeLine(const std::uint8_t* row, int line_index, int stride) {
  // L'adresse de ligne est comptée à partir de 1.
  writeControl(static_cast<std::uint8_t>(line_index + 1));

  if (config_.reverse_pixel_bytes) {
    for (int i = 0; i < stride; ++i) g_spi->transfer(reverseBits(row[i]));
  } else {
    for (int i = 0; i < stride; ++i) g_spi->transfer(row[i]);
  }

  // Huit bits de bourrage clôturent la ligne.
  g_spi->transfer(0x00);
}

void SharpDisplay::clear() {
  beginTransfer();
  std::uint8_t value = kModeClear;
  if (vcom_) value |= kModeVcom;
  writeControl(value);
  g_spi->transfer(0x00);
  endTransfer();
}

void SharpDisplay::flushAll(const ui::Canvas& canvas) {
  const std::uint8_t* buffer = canvas.buffer();
  if (buffer == nullptr) return;

  const int stride = canvas.stride();

  beginTransfer();
  writeControl(controlByte());
  for (int y = 0; y < canvas.height(); ++y) {
    writeLine(&buffer[static_cast<std::size_t>(y) * stride], y, stride);
  }
  g_spi->transfer(0x00);  // bourrage final
  endTransfer();
}

void SharpDisplay::flushDirty(ui::Canvas& canvas) {
  const std::uint8_t* buffer = canvas.buffer();
  if (buffer == nullptr) return;

  int first = -1;
  int last = -1;
  canvas.dirtyRange(first, last);
  if (first < 0) return;  // rien à faire

  const int stride = canvas.stride();

  beginTransfer();
  writeControl(controlByte());
  // Le protocole permet d'envoyer n'importe quel sous-ensemble de lignes, dans
  // l'ordre croissant, chacune précédée de son adresse.
  for (int y = first; y <= last; ++y) {
    if (!canvas.lineDirty(y)) continue;
    writeLine(&buffer[static_cast<std::size_t>(y) * stride], y, stride);
  }
  g_spi->transfer(0x00);
  endTransfer();

  canvas.clearDirty();
}

void SharpDisplay::maintain(std::uint32_t now_ms) {
  if (now_ms - last_vcom_ms_ < kVcomPeriodMs) return;
  last_vcom_ms_ = now_ms;

  vcom_ = !vcom_;
  // La broche EXTCOMIN suffit quand EXTMODE est câblé au niveau haut ; on
  // maintient aussi le bit VCOM dans les commandes suivantes, ce qui couvre les
  // deux câblages.
  digitalWrite(config_.pin_extcomin, vcom_ ? HIGH : LOW);
}

}  // namespace vario::hal
