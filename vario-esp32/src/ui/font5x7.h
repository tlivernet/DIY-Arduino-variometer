#pragma once

namespace vario::ui {

// Police bitmap 5×7, plage ASCII imprimable 0x20 à 0x7E.
constexpr int kFontWidth = 5;
constexpr int kFontHeight = 7;
constexpr int kFontFirstChar = 0x20;
constexpr int kFontLastChar = 0x7E;
constexpr int kFontGlyphCount = kFontLastChar - kFontFirstChar + 1;

// Une colonne d'avance après chaque glyphe.
constexpr int kFontAdvance = kFontWidth + 1;

extern const unsigned char kFont5x7[kFontGlyphCount * kFontWidth];

}  // namespace vario::ui
