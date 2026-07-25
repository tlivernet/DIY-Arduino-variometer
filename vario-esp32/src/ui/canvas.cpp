#include "ui/canvas.h"

#include <cstring>

#include "ui/font5x7.h"

namespace vario::ui {

Canvas::Canvas(std::uint8_t* buffer, int width, int height)
    : buffer_(buffer), width_(width), height_(height), stride_((width + 7) / 8) {
  if (height_ > kMaxHeight) height_ = kMaxHeight;
  if (width_ < 0) width_ = 0;
  if (height_ < 0) height_ = 0;
}

void Canvas::markDirty(int y) {
  if (y < 0 || y >= height_) return;
  dirty_[y >> 3] |= static_cast<std::uint8_t>(1u << (y & 7));
}

bool Canvas::lineDirty(int y) const {
  if (y < 0 || y >= height_) return false;
  return (dirty_[y >> 3] & (1u << (y & 7))) != 0;
}

int Canvas::dirtyLineCount() const {
  int count = 0;
  for (int y = 0; y < height_; ++y) {
    if (lineDirty(y)) ++count;
  }
  return count;
}

void Canvas::clearDirty() {
  std::memset(dirty_, 0, sizeof(dirty_));
}

void Canvas::dirtyRange(int& first, int& last) const {
  first = -1;
  last = -1;
  for (int y = 0; y < height_; ++y) {
    if (!lineDirty(y)) continue;
    if (first < 0) first = y;
    last = y;
  }
}

void Canvas::clear(bool on) {
  if (buffer_ == nullptr) return;
  std::memset(buffer_, on ? 0xFF : 0x00, static_cast<std::size_t>(stride_) * height_);
  for (int y = 0; y < height_; ++y) markDirty(y);
}

void Canvas::setPixel(int x, int y, bool on) {
  if (buffer_ == nullptr) return;
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;

  std::uint8_t& byte = buffer_[static_cast<std::size_t>(y) * stride_ + (x >> 3)];
  const std::uint8_t mask = static_cast<std::uint8_t>(0x80u >> (x & 7));
  const std::uint8_t before = byte;

  if (on) {
    byte |= mask;
  } else {
    byte = static_cast<std::uint8_t>(byte & ~mask);
  }
  if (byte != before) markDirty(y);
}

bool Canvas::pixel(int x, int y) const {
  if (buffer_ == nullptr) return false;
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return false;
  const std::uint8_t byte = buffer_[static_cast<std::size_t>(y) * stride_ + (x >> 3)];
  return (byte & (0x80u >> (x & 7))) != 0;
}

void Canvas::fillRect(int x, int y, int w, int h, bool on) {
  for (int row = y; row < y + h; ++row) {
    for (int col = x; col < x + w; ++col) setPixel(col, row, on);
  }
}

void Canvas::hLine(int x, int y, int w, bool on) {
  fillRect(x, y, w, 1, on);
}

void Canvas::vLine(int x, int y, int h, bool on) {
  fillRect(x, y, 1, h, on);
}

void Canvas::drawLine(int x0, int y0, int x1, int y1, bool on) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;

  const int sx = x0 < x1 ? 1 : -1;
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx - dy;

  for (;;) {
    setPixel(x0, y0, on);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * error;
    if (e2 > -dy) {
      error -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      error += dx;
      y0 += sy;
    }
  }
}

void Canvas::drawRect(int x, int y, int w, int h, bool on) {
  if (w <= 0 || h <= 0) return;
  hLine(x, y, w, on);
  hLine(x, y + h - 1, w, on);
  vLine(x, y, h, on);
  vLine(x + w - 1, y, h, on);
}

int Canvas::textWidth(const char* text, int scale) {
  if (text == nullptr || scale <= 0) return 0;
  int chars = 0;
  for (const char* p = text; *p != '\0'; ++p) ++chars;
  if (chars == 0) return 0;
  // Pas de colonne d'avance après le dernier glyphe.
  return (chars * kFontAdvance - 1) * scale;
}

int Canvas::drawText(int x, int y, const char* text, bool on, int scale) {
  if (text == nullptr || scale <= 0) return x;

  int cursor = x;
  for (const char* p = text; *p != '\0'; ++p) {
    const int code = static_cast<unsigned char>(*p);
    if (code < kFontFirstChar || code > kFontLastChar) {
      cursor += kFontAdvance * scale;
      continue;
    }
    const unsigned char* glyph = &kFont5x7[(code - kFontFirstChar) * kFontWidth];

    for (int col = 0; col < kFontWidth; ++col) {
      const unsigned char bits = glyph[col];
      for (int row = 0; row < kFontHeight; ++row) {
        if ((bits & (1u << row)) == 0) continue;
        if (scale == 1) {
          setPixel(cursor + col, y + row, on);
        } else {
          fillRect(cursor + col * scale, y + row * scale, scale, scale, on);
        }
      }
    }
    cursor += kFontAdvance * scale;
  }
  return cursor - scale;  // on retire la colonne d'avance finale
}

int Canvas::drawTextCentered(int x, int y, int w, const char* text, bool on, int scale) {
  const int text_w = textWidth(text, scale);
  return drawText(x + (w - text_w) / 2, y, text, on, scale);
}

}  // namespace vario::ui
