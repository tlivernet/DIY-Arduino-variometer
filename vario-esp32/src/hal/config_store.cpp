#include "hal/config_store.h"

#include <Preferences.h>

#include <cmath>

namespace vario::hal {

bool ConfigStore::load(Config& config) {
  Preferences prefs;
  // readonly = true : n'échoue pas si l'espace n'existe pas encore, renvoie
  // simplement les valeurs par défaut fournies à chaque get.
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return false;
  }

  const std::size_t count = configFieldCount();
  for (std::size_t i = 0; i < count; ++i) {
    const ConfigField& f = configField(i);
    float current = 0.0f;
    getConfigValue(config, f.key, current);
    // Clé absente ⇒ getFloat rend la valeur courante inchangée (son défaut).
    const float stored = prefs.getFloat(f.key, current);
    setConfigValue(config, f.key, stored);
  }

  prefs.end();
  clampConfig(config);
  return true;
}

void ConfigStore::save(const Config& config) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }

  const std::size_t count = configFieldCount();
  for (std::size_t i = 0; i < count; ++i) {
    const ConfigField& f = configField(i);
    float value = 0.0f;
    if (!getConfigValue(config, f.key, value)) continue;

    // N'écrit que si la valeur diffère de celle en flash : chaque écriture use
    // la flash, et un « Enregistrer » sans changement ne doit rien coûter.
    const float stored = prefs.getFloat(f.key, std::nanf(""));
    if (std::isnan(stored) || stored != value) {
      prefs.putFloat(f.key, value);
    }
  }

  prefs.end();
}

}  // namespace vario::hal
