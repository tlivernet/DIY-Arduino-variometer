#include "ui/segment.h"

namespace vario::ui {
namespace {

// Segments d'un afficheur classique :
//
//      aaaa
//     f    b
//     f    b
//      gggg
//     e    c
//     e    c
//      dddd
enum Segment : unsigned char {
  kA = 1u << 0,
  kB = 1u << 1,
  kC = 1u << 2,
  kD = 1u << 3,
  kE = 1u << 4,
  kF = 1u << 5,
  kG = 1u << 6,
};

constexpr unsigned char kDigitSegments[10] = {
    kA | kB | kC | kD | kE | kF,       // 0
    kB | kC,                           // 1
    kA | kB | kD | kE | kG,            // 2
    kA | kB | kC | kD | kG,            // 3
    kB | kC | kF | kG,                 // 4
    kA | kC | kD | kF | kG,            // 5
    kA | kC | kD | kE | kF | kG,       // 6
    kA | kB | kC,                      // 7
    kA | kB | kC | kD | kE | kF | kG,  // 8
    kA | kB | kC | kD | kF | kG,       // 9
};

void drawSegments(Canvas& canvas, int x, int y, const SegmentMetrics& m,
                  unsigned char mask, bool on) {
  const int t = m.thickness;
  const int w = m.width;
  const int h = m.height;

  // Longueur des barres verticales : de la fin d'une barre horizontale au
  // début de la suivante.
  const int half = (h - t) / 2;
  const int bar_h = half - t;
  const int bar_w = w - 2 * t;

  if (mask & kA) canvas.fillRect(x + t, y, bar_w, t, on);
  if (mask & kG) canvas.fillRect(x + t, y + half, bar_w, t, on);
  if (mask & kD) canvas.fillRect(x + t, y + h - t, bar_w, t, on);

  if (mask & kF) canvas.fillRect(x, y + t, t, bar_h, on);
  if (mask & kB) canvas.fillRect(x + w - t, y + t, t, bar_h, on);
  if (mask & kE) canvas.fillRect(x, y + half + t, t, bar_h, on);
  if (mask & kC) canvas.fillRect(x + w - t, y + half + t, t, bar_h, on);
}

}  // namespace

int segmentCharWidth(char c, const SegmentMetrics& metrics) {
  switch (c) {
    case '.':
      return metrics.thickness;
    case ':':
      return metrics.thickness;
    case '-':
      return metrics.width / 2;
    case '+':
      return metrics.width / 2;
    case ' ':
      return metrics.width / 2;
    default:
      return metrics.width;
  }
}

int segmentTextWidth(const char* text, const SegmentMetrics& metrics) {
  if (text == nullptr || *text == '\0') return 0;

  int total = 0;
  for (const char* p = text; *p != '\0'; ++p) {
    total += segmentCharWidth(*p, metrics);
    if (*(p + 1) != '\0') total += metrics.spacing;
  }
  return total;
}

int drawSegmentText(Canvas& canvas, int x, int y, const char* text,
                    const SegmentMetrics& metrics, bool on) {
  if (text == nullptr) return x;

  const int t = metrics.thickness;
  const int h = metrics.height;
  const int half = (h - t) / 2;

  int cursor = x;
  for (const char* p = text; *p != '\0'; ++p) {
    const char c = *p;
    const int cw = segmentCharWidth(c, metrics);

    if (c >= '0' && c <= '9') {
      drawSegments(canvas, cursor, y, metrics, kDigitSegments[c - '0'], on);
    } else if (c == '-') {
      // Barre médiane, centrée sur la largeur réduite du signe.
      canvas.fillRect(cursor, y + half, cw, t, on);
    } else if (c == '+') {
      canvas.fillRect(cursor, y + half, cw, t, on);
      canvas.fillRect(cursor + (cw - t) / 2, y + half - cw / 2 + t / 2, t, cw, on);
    } else if (c == '.') {
      canvas.fillRect(cursor, y + h - t, t, t, on);
    } else if (c == ':') {
      canvas.fillRect(cursor, y + half / 2, t, t, on);
      canvas.fillRect(cursor, y + h - half / 2 - t, t, t, on);
    }
    // ' ' ne dessine rien.

    cursor += cw;
    if (*(p + 1) != '\0') cursor += metrics.spacing;
  }
  return cursor;
}

}  // namespace vario::ui
