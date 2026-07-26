#pragma once

#include <cstddef>
#include <cstdint>

#include "core/tone_law.h"

namespace vario {

// Version du schéma stocké. À incrémenter uniquement pour un changement de
// SENS d'un champ (unité, renommage, découpage) : l'ajout d'un champ est
// gratuit, puisque la lecture se fait clé par clé et qu'une clé absente rend sa
// valeur par défaut.
constexpr std::uint16_t kConfigVersion = 1;

// Réglages de l'appareil.
struct Config {
  float climb_threshold_ms = 0.2f;
  float sink_threshold_ms = -1.1f;
  float qnh_hpa = 1013.25f;
  float altitude_offset_m = 0.0f;
  float trim_speed_ms = 10.0f;
  int volume = 3;  // 0 = muet, 1 à 4
  int contrast = 50;
  bool ble_enabled = true;
  ToneProfile tone_profile = ToneProfile::Modern;
};

// Type d'un champ, pour l'IHM et le stockage.
enum class FieldType : std::uint8_t { Float, Int, Bool, Enum };

// Description d'un réglage.
//
// Cette table unique pilote QUATRE consommateurs : la lecture/écriture en NVS,
// le rendu du menu embarqué, l'IHM web (servie en JSON pour que la page se
// dessine elle-même) et la validation des bornes. Elle remplace le switch géant
// de renderMenu() de 2017, où ajouter un réglage demandait de modifier cinq
// endroits différents.
struct ConfigField {
  const char* key;  // 15 caractères maximum, limite de NVS
  FieldType type;
  float min;
  float max;
  float step;
  const char* label;  // libellé du menu, en français comme en 2017
  const char* unit;
  // Valeur qui désactive le réglage, si applicable (les seuils à 0).
  bool zero_disables;
};

// Accès à la table.
std::size_t configFieldCount();
const ConfigField& configField(std::size_t index);
// Renvoie nullptr si la clé est inconnue.
const ConfigField* findConfigField(const char* key);

// Lecture et écriture générique d'un champ, par clé.
// Renvoie false si la clé est inconnue.
bool getConfigValue(const Config& config, const char* key, float& out);
bool setConfigValue(Config& config, const char* key, float value);

// Borne tous les champs dans leurs limites.
//
// Le montage de 2017 ne bornait presque rien : le QNH pouvait descendre à 0, ce
// qui produisait une altitude NaN définitive, et les seuils comme le contraste
// débordaient de leur type entier pour réapparaître à l'autre extrémité.
void clampConfig(Config& config);

// Formate la valeur d'un champ pour l'affichage (« 0.2 », « OFF », « ON »…).
// Renvoie le nombre de caractères écrits, hors terminateur.
std::size_t formatConfigValue(char* buf, std::size_t size, const Config& config,
                              const char* key);

}  // namespace vario
