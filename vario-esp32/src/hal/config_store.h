#pragma once

#include "core/config_schema.h"

namespace vario::hal {

// Persistance des réglages en mémoire non volatile (NVS), via Preferences.
//
// La table de schéma (config_schema) est la source unique : le stockage lit et
// écrit CLÉ PAR CLÉ, avec la clé courte de chaque champ (≤ 15 caractères, limite
// de NVS). Conséquence directe et voulue : une clé absente en flash rend sa
// valeur par défaut, donc ajouter un réglage n'invalide pas la config
// existante et il n'y a aucune migration à écrire. C'est l'inverse du montage
// de 2017, où toute modification de la structure EEPROM imposait de tout
// réinitialiser (le fameux drapeau `initialisation`).
class ConfigStore {
 public:
  // Espace de noms NVS. Court, comme l'exige NVS.
  static constexpr const char* kNamespace = "vario";

  ConfigStore() = default;

  // Charge les réglages depuis la flash dans config. Les champs absents gardent
  // la valeur déjà présente dans config (donc leur défaut). Renvoie false si
  // l'espace de noms n'a jamais été écrit — l'appelant garde alors les défauts.
  bool load(Config& config);

  // Écrit tous les champs. N'écrit une clé que si sa valeur a changé, pour
  // épargner les cycles d'effacement de la flash — Preferences le fait déjà,
  // mais on évite en plus d'ouvrir l'espace en écriture pour rien.
  void save(const Config& config);

 private:
  // Écrit un champ d'après son type, en float (NVS stocke le float tel quel).
  // Regroupé pour que load/save restent pilotés par la table.
};

}  // namespace vario::hal
