#pragma once

#include "ui/canvas.h"

namespace vario::ui {

// Chiffres à sept segments, de taille libre.
//
// Pourquoi ce choix plutôt qu'une police bitmap : sur 400×240 les grands
// chiffres du vario font ~110 px de haut. Un jeu de glyphes bitmap à cette
// taille coûterait des dizaines de kilo-octets de flash, alors qu'un rendu par
// rectangles ne coûte rien, s'adapte à n'importe quelle dimension, et donne
// l'allure d'un instrument de bord — ce qui est exactement l'effet recherché.
// Agrandir 16× la police 5×7, à l'inverse, donne un rendu de calculatrice de
// 1982.

// Dimensions d'un chiffre.
struct SegmentMetrics {
  int width = 60;
  int height = 110;
  int thickness = 14;
  // Écart horizontal entre deux caractères.
  int spacing = 8;
};

// Largeur totale occupée par une chaîne rendue en sept segments.
// Caractères acceptés : 0-9, '-', '+', '.', ':', ' '.
int segmentTextWidth(const char* text, const SegmentMetrics& metrics);

// Dessine la chaîne et renvoie l'abscisse juste après le dernier caractère.
int drawSegmentText(Canvas& canvas, int x, int y, const char* text,
                    const SegmentMetrics& metrics, bool on = true);

// Largeur propre d'un caractère (les signes et séparateurs sont plus étroits
// que les chiffres).
int segmentCharWidth(char c, const SegmentMetrics& metrics);

}  // namespace vario::ui
