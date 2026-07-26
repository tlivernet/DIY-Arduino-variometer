#pragma once

#include <cstddef>

#include "core/config_schema.h"

namespace vario {

// Pont entre les réglages et le portail web de configuration (étape P6).
//
// La table de schéma (config_schema) est la source unique : la page web ne
// connaît AUCUN réglage en dur, elle reçoit le schéma et les valeurs en JSON et
// se dessine elle-même. Ajouter un réglage reste donc une seule ligne dans
// kFields, sans toucher à la page — c'est tout l'intérêt de la table par rapport
// au switch géant de renderMenu() de 2017.
//
// Ces fonctions sont du C++ pur, sans réseau : elles se testent en natif. Le
// serveur HTTP (hal/wifi_portal) ne fait que du transport autour d'elles.

// Sérialise le schéma ET les valeurs courantes en un document JSON, destiné à
// la requête GET /config. Forme :
//
//   {"fields":[
//      {"key":"clmb","label":"MONTEE","unit":"M/S","type":"float",
//       "min":0,"max":5,"step":0.1,"value":0.2,"text":"0.2"},
//      ...
//   ]}
//
// Le champ "text" reprend le formatage embarqué (« OFF », « MODERNE »…), pour
// que la page affiche exactement ce que montre l'écran. Renvoie le nombre de
// caractères écrits hors terminateur, ou 0 si le tampon est trop petit (le
// document est alors laissé vide). Prévoir ~1 Ko.
std::size_t configToJson(char* buf, std::size_t size, const Config& config);

// Applique un document JSON plat de la forme {"clé":valeur,...} reçu en POST
// /config. Seules les clés connues sont prises en compte ; chaque valeur passe
// par setConfigValue, donc est bornée à sa plage. Les clés inconnues et les
// valeurs non numériques sont ignorées sans faire échouer l'ensemble — un
// client d'une version plus récente ne casse pas un firmware plus ancien.
//
// Renvoie le nombre de champs effectivement appliqués. La valeur des booléens
// et énumérés accepte indifféremment un nombre (0/1) ou les littéraux JSON
// true/false.
int applyJsonToConfig(const char* json, Config& config);

}  // namespace vario
