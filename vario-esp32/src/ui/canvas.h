#pragma once

#include <cstddef>
#include <cstdint>

namespace vario::ui {

// Tampon d'affichage monochrome, 1 bit par pixel, avec suivi des lignes
// modifiées.
//
// Le suivi n'est pas une optimisation mais une nécessité : sur le Sharp
// LS027B7DH01, une trame complète représente 12,5 ko à 1 MHz, soit ~100 ms.
// Bien trop lent pour rafraîchir à 10 Hz. En ne transmettant que les lignes
// réellement modifiées, la zone du gros chiffre tombe à ~46 ms et le champ
// horloge à ~8 ms.
//
// Le rendu est du C++ pur : les écrans se testent donc par comparaison d'images
// de référence, sans écran ni microcontrôleur.
class Canvas {
 public:
  // Hauteur maximale gérée par le suivi des lignes modifiées.
  static constexpr int kMaxHeight = 512;

  // buffer doit contenir au moins height × stride octets, avec
  // stride = (width + 7) / 8. Le bit de poids fort d'un octet est le pixel de
  // gauche, ce qui correspond à l'ordre attendu par le Sharp.
  Canvas(std::uint8_t* buffer, int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }
  int stride() const { return stride_; }
  const std::uint8_t* buffer() const { return buffer_; }

  // Efface tout l'écran. on = true remplit en noir.
  void clear(bool on = false);

  void setPixel(int x, int y, bool on);
  bool pixel(int x, int y) const;

  void fillRect(int x, int y, int w, int h, bool on);
  void drawRect(int x, int y, int w, int h, bool on);
  void hLine(int x, int y, int w, bool on);
  void vLine(int x, int y, int h, bool on);
  // Segment quelconque (Bresenham).
  void drawLine(int x0, int y0, int x1, int y1, bool on);

  // Texte en police 5×7. scale multiplie chaque pixel. Renvoie l'abscisse
  // juste après le dernier glyphe.
  int drawText(int x, int y, const char* text, bool on = true, int scale = 1);
  static int textWidth(const char* text, int scale = 1);
  static int textHeight(int scale = 1) { return 7 * scale; }

  // Texte centré horizontalement dans [x, x + w).
  int drawTextCentered(int x, int y, int w, const char* text, bool on = true,
                       int scale = 1);

  // --- Suivi des lignes modifiées ---
  bool lineDirty(int y) const;
  int dirtyLineCount() const;
  void clearDirty();
  // Première et dernière ligne modifiée, ou {-1, -1} si rien ne l'est.
  void dirtyRange(int& first, int& last) const;

 private:
  void markDirty(int y);

  std::uint8_t* buffer_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  int stride_ = 0;
  std::uint8_t dirty_[kMaxHeight / 8] = {};
};

}  // namespace vario::ui
