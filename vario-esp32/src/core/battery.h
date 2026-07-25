#pragma once

namespace vario {

// Plage de la jauge, en volts, pour un élément LiPo 1S.
// Fenêtre volontairement conservatrice, reprise du dernier réglage de 2017
// (« + de durée de vie (cycles) + de sécu »).
constexpr float kBatteryEmptyV = 3.35f;
constexpr float kBatteryFullV = 4.20f;

// Niveau de charge, de 0 (vide) à 1 (pleine).
float batteryFraction(float volts);

// Nombre de pixels à remplir dans la jauge.
//
// Le code de 2017 utilisait map(mV, 3350, 4200, 0, 8), or map() d'Arduino ne
// borne pas et tronque vers zéro : sous 3,243 V le résultat était négatif,
// affecté à un uint8_t il devenait 253-255, et la ligne suivante
// (`if (pixels > 8) pixels = 8;`) le ramenait à 8. **Une batterie à plat
// s'affichait donc pleine.** On calcule ici en signé et on borne avant
// toute conversion.
int batteryPixels(float volts, int max_pixels = 8);

}  // namespace vario
