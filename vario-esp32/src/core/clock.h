#pragma once

#include <cstdint>

// Garde-fou sur la version du langage.
//
// Sans lui, une compilation en C++11 échoue avec des dizaines de messages
// « no matching function for call to ToneOutput::ToneOutput(...) » qui ne
// mentionnent jamais la version du langage : en C++11, une structure dotée
// d'initialiseurs de membres par défaut n'est pas un agrégat, donc toute
// initialisation de la forme ToneOutput{true, 690} est interprétée comme un
// appel de constructeur.
//
// Le cas se produit pour de bon : le coeur Arduino-ESP32 ajoute -std=gnu++11
// APRÈS nos options, et gcc retient le dernier -std. D'où la nécessité de
// build_unflags dans platformio.ini. Ce contrôle rend l'oubli explicite.
static_assert(__cplusplus >= 201703L,
              "Ce projet exige C++17. Si la compilation echoue ici, le "
              "-std=gnu++17 a ete ecrase : verifier build_unflags = -std=gnu++11 "
              "dans platformio.ini.");

namespace vario {

// Temps monotone en microsecondes.
//
// Le code de 2017 stockait micros() dans un float : au-delà de 2^24 µs (17 s
// d'uptime) la mantisse ne représentait plus les microsecondes exactement, et
// après une heure la quantification atteignait 512 µs sur des intervalles de
// 33 000 µs. Il comparait de plus les valeurs à travers le débordement des
// 71,6 minutes de micros(), ce qui produisait un Δt négatif.
//
// int64_t supprime les deux problèmes : ~292 000 ans de plage, résolution
// exacte à la microseconde.
using Micros = std::int64_t;

constexpr Micros kMicrosPerSecond = 1000000;

inline constexpr float toSeconds(Micros dt) {
  return static_cast<float>(dt) / static_cast<float>(kMicrosPerSecond);
}

inline constexpr Micros fromMillis(std::int64_t ms) {
  return ms * 1000;
}

}  // namespace vario
