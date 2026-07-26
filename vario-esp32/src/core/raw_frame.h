#pragma once

#include <cstddef>

#include "core/clock.h"
#include "core/vec3.h"

namespace vario {

// Trame brute de capteurs, telle que le firmware la voit à ~100 Hz.
//
// C'est le pivot du harnais de rejeu (étape P5). Le firmware émet ces trames en
// CSV sur l'USB ; un script les enregistre ; et le moteur de rejeu (ReplayEngine)
// les fait passer à travers TOUTE la chaîne d'estimation en natif, sur un
// portable. Un comportement bizarre constaté en vol devient ainsi un
// enregistrement de 30 s, qui devient un fichier de référence versionné, qui
// devient un test de non-régression permanent.
//
// On n'enregistre QUE ce qui n'est pas reproductible : les mesures physiques
// brutes. Tout ce qui en découle (assiette, altitude, vitesse verticale, son)
// est recalculé au rejeu par le même code que dans le firmware — c'est ce qui
// rend le rejeu utile pour régler le filtre.
struct RawFrame {
  // Horodatage monotone de la trame, en microsecondes. Provient d'esp_timer sur
  // la cible ; en test, on le fabrique. Sert à reconstruire les Δt exacts.
  Micros timestamp = 0;

  // Accéléromètre, en m/s², repère boîtier (X avant, Y droite, Z bas).
  Vec3 accel;

  // Gyromètre, en rad/s, repère boîtier.
  Vec3 gyro;

  // true si une nouvelle mesure barométrique accompagne cette trame. Le
  // baromètre tourne plus lentement que la centrale inertielle (~96 Hz contre
  // 500 Hz), donc la plupart des trames n'en portent pas : ce drapeau distingue
  // « pas de baro cette fois » d'une pression qui se trouverait valoir la même.
  bool has_baro = false;

  // Pression barométrique, en hPa. Significative seulement si has_baro.
  float pressure_hpa = 0.0f;

  // Température, en °C. Significative seulement si has_baro.
  float temperature_c = 0.0f;
};

// En-tête de la ligne CSV, sans saut de ligne final. Écrit tel quel en tête de
// fichier par l'outil d'enregistrement, et attendu tel quel par le parseur.
extern const char* const kRawFrameCsvHeader;

// Sérialise une trame en une ligne CSV (sans saut de ligne final). Renvoie le
// nombre de caractères écrits, hors terminateur, ou 0 si le tampon est trop
// petit. Le format est stable : c'est un format de fichier versionné.
//
// Volontairement sans <cstdio> ni allocation, pour être utilisable tel quel
// dans le firmware embarqué.
std::size_t formatRawFrame(char* buf, std::size_t size, const RawFrame& frame);

// Analyse une ligne CSV en une trame. Renvoie false si la ligne est vide, un
// commentaire (#…), l'en-tête, ou malformée. Tolère les espaces et un éventuel
// '\r' final (fichiers enregistrés sous Windows).
bool parseRawFrame(const char* line, RawFrame& out);

}  // namespace vario
