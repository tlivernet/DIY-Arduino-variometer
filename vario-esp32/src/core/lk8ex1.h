#pragma once

#include <cstddef>
#include <cstdint>

namespace vario {

// Trame LK8EX1, le format de fait pour qu'un vario serve de capteur à un
// téléphone (XCTrack, XCSoar, LK8000). Émise sur le service Nordic UART en BLE.
//
//   $LK8EX1,pression,altitude,vario,température,batterie,*somme
//
//   pression    : pression brute en Pa, 999999 si indisponible
//   altitude    : mètres, 99999 si indisponible
//   vario       : cm/s, entier
//   température : °C, 99 si indisponible
//   batterie    : niveau en % + 1000, ou tension en mV
//   somme       : XOR de tous les caractères entre « $ » et « * », en hexa
//
// Choix important : on envoie la PRESSION BRUTE et on laisse l'altitude à
// 99999. Le téléphone applique alors son propre QNH, ce qui évite deux
// réglages d'altitude divergents entre l'appareil et l'application — source
// classique de confusion.

struct Lk8ex1Data {
  float pressure_pa = 0.0f;
  float vario_ms = 0.0f;
  float temperature_c = 0.0f;
  int battery_percent = 100;
  bool pressure_valid = true;
  bool temperature_valid = true;
};

// Écrit la trame, terminateur inclus, dans buf. Renvoie le nombre de caractères
// écrits hors terminateur, ou 0 si le tampon est trop petit.
//
// Prévoir au moins 64 octets. Une trame typique fait ~45 caractères, ce qui
// tient dans une seule notification BLE si le MTU a été porté à 128.
std::size_t formatLk8ex1(char* buf, std::size_t size, const Lk8ex1Data& data);

// Somme de contrôle NMEA : XOR des caractères d'une chaîne.
std::uint8_t nmeaChecksum(const char* payload);

}  // namespace vario
