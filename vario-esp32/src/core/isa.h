#pragma once

namespace vario {

// Bornes de QNH acceptées, en hPa.
//
// Le code de 2017 laissait le réglage QNH totalement libre (± 1 hPa par cran,
// sans borne). Descendre à 0 faisait calculer pow(p/0, 0.1903) = inf, puis une
// altitude NaN — et comme le vario dérive de l'altitude, NaN se propageait
// définitivement : plus d'affichage, plus de son, jusqu'au redémarrage.
constexpr float kQnhMinHpa = 850.0f;
constexpr float kQnhMaxHpa = 1100.0f;
constexpr float kQnhDefaultHpa = 1013.25f;

// Constantes de l'atmosphère standard (OACI), telles qu'utilisées par la
// bibliothèque BMP085 d'origine :
//   44330 = T0/L = 288,15 / 0,0065
//   0,1903 = 1/5,255 = R·L/(g·M)
constexpr float kIsaScaleM = 44330.0f;
constexpr float kIsaExponent = 0.1903f;

// Borne un QNH dans une plage physiquement plausible.
float clampQnh(float qnh_hpa);

// Altitude pression, en mètres, selon l'atmosphère standard.
//
// Formule identique à celle de 2017 pour que les altitudes affichées
// restent comparables avec les vols déjà enregistrés. La température est
// volontairement ignorée, comme dans l'original.
//
// Le QNH est borné et la pression validée : la fonction ne renvoie jamais
// NaN ni infini.
float pressureToAltitude(float qnh_hpa, float pressure_hpa);

}  // namespace vario
