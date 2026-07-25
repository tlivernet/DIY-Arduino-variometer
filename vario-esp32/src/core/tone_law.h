#pragma once

#include <cstdint>

namespace vario {

enum class ToneProfile : std::uint8_t {
  // Lois exactes du montage de 2017, pour retrouver le son connu :
  //   fréquence = borne(690 + 150·v, 100, 1300) Hz
  //   note      = borne(150 −  30·v,  70,  150) ms, pause = note/2
  // Défaut : la cadence ne varie que de 4,4 à 9,5 bips/s sur toute la plage,
  // et le piezo rend très mal sous 1 kHz.
  Classic,

  // Plage dynamique élargie et bande de fréquence remontée là où un piezo a
  // un bon rendement : cadence de 1 à 10 bips/s, 700 à 2200 Hz.
  Modern,
};

// Hauteur du son pour une vitesse verticale donnée, en Hz.
std::uint16_t toneFrequencyHz(float vario_ms, ToneProfile profile);

// Durée de la note, en ms. La pause qui suit vaut la moitié de cette durée
// (rapport 2:1 du montage d'origine, soit un rapport cyclique de 67 %).
std::uint16_t toneNoteMs(float vario_ms, ToneProfile profile);

}  // namespace vario
