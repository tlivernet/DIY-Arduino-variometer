#pragma once

#include <cstddef>

namespace vario {

// Décomposition de la vitesse verticale pour l'affichage.
struct VarioText {
  char sign = ' ';  // '+', '-' ou ' ' dans la bande morte
  int whole = 0;    // partie entière de la valeur absolue
  int tenth = 0;    // dixième, toujours dans 0..9
};

// Bande morte d'affichage du signe, reprise de l'original.
constexpr float kVarioSignDeadbandMs = 0.05f;

// Décompose une vitesse verticale en signe / entier / dixième.
//
// Le code de 2017 calculait séparément floor(|v|) et round(10·|v|) − 10·floor(|v|).
// Quand l'arrondi du dixième reportait, le second terme valait 10 : 1,96 m/s
// s'affichait « 1.10 » au lieu de « 2.0 ». On arrondit ici une seule fois, en
// dixièmes, puis on décompose — le report est structurellement impossible.
VarioText formatVario(float vario_ms, float deadband_ms = kVarioSignDeadbandMs);

// Écrit la valeur formatée (par exemple "+2.0") dans buf.
// Renvoie le nombre de caractères écrits, hors terminateur.
std::size_t formatVarioTo(char* buf, std::size_t size, float vario_ms,
                          float deadband_ms = kVarioSignDeadbandMs);

}  // namespace vario
