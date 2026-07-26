#pragma once

// Affectation des broches, variante A (carte ESP32-S3 du commerce + modules).
//
// Broches à éviter sur ESP32-S3 :
//   0        amorçage
//   19, 20   USB natif
//   26 à 32  flash SPI
//   33 à 37  PSRAM octale (sur les modules qui en ont)
//   43, 44   UART0
//   45, 46   broches de configuration au démarrage
//
// ADC1 = GPIO 1 à 10. **L'ADC2 est inutilisable quand le Wi-Fi est actif**,
// donc la mesure de batterie doit être sur ADC1.
// Les GPIO 0 à 21 sont des RTC GPIO, donc capables de réveiller depuis le
// sommeil profond : le bouton OK est placé là exprès.

namespace vario::pins {

// --- Bus I2C partagé : baromètre, centrale inertielle, horloge ---
//
// Les modules du commerce (GY-63, GY-521) sont en I2C, d'où ce choix pour la
// variante A. La variante B intégrée passera le baromètre en SPI, où il n'y a
// ni étirement d'horloge ni blocage de bus possible.
constexpr int kI2cSda = 8;
constexpr int kI2cScl = 9;
constexpr int kI2cFrequency = 400000;

// --- Écran Sharp, sur son propre bus SPI ---
//
// Séparé des capteurs : l'écran tourne à 1 MHz et monopolise le bus pendant
// des dizaines de millisecondes, ce qui ne doit pas retarder les capteurs.
constexpr int kDisplaySclk = 12;
constexpr int kDisplayMosi = 11;
constexpr int kDisplayCs = 10;        // ATTENTION : actif à l'état HAUT sur le Sharp
constexpr int kDisplayDisp = 13;      // activation de l'affichage
constexpr int kDisplayExtcomin = 14;  // basculement VCOM, ~1 Hz

// --- Son ---
//
// Deux broches attaquées par le MÊME signal LEDC, l'une inversée, pour
// doubler l'amplitude aux bornes du piezo. Tamponnées par une porte logique
// alimentée en 5 V : sans elle on n'obtient que 6,6 V crête à crête contre
// 10 sur le montage AVR, soit ~3,6 dB de moins.
constexpr int kBuzzerA = 1;
constexpr int kBuzzerB = 2;

// --- Boutons, actifs à l'état bas avec tirage au plus interne ---
constexpr int kButtonUp = 4;
constexpr int kButtonOk = 5;  // RTC GPIO : peut réveiller depuis le sommeil
constexpr int kButtonDown = 6;

// --- Mesure de batterie ---
// Diviseur 2:1 (100 k / 100 k) sur une broche ADC1.
constexpr int kBatterySense = 3;
constexpr float kBatteryDividerRatio = 2.0f;

// --- Interruption « donnée prête » de la centrale inertielle ---
constexpr int kImuInterrupt = 7;

}  // namespace vario::pins
