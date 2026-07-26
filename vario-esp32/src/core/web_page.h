#pragma once

namespace vario {

// Page de configuration du portail Wi-Fi (étape P6).
//
// Volontairement AUTONOME et pilotée par le schéma : la page ne code aucun
// réglage en dur. Au chargement elle interroge GET /config, reçoit le schéma et
// les valeurs (voir configToJson), et construit ses champs elle-même. Un
// réglage ajouté dans config_schema apparaît donc sans toucher à cette page.
//
// Aucune ressource externe (ni police, ni script, ni feuille de style distante)
// : le portail sert un point d'accès isolé, sans accès à Internet. Tout est
// donc en ligne dans le document.
//
// La page est stockée en flash (PROGMEM sur la cible) et servie telle quelle
// par le serveur HTTP. Elle est ici pour être testable — un test vérifie
// qu'elle est non vide, close, et référence les bons points d'accès.
extern const char kConfigPageHtml[];

// Longueur de la page, hors terminateur. Pratique pour l'en-tête Content-Length.
unsigned long configPageLength();

}  // namespace vario
