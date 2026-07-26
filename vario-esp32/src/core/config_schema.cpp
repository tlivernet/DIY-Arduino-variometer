#include "core/config_schema.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/isa.h"

namespace vario {
namespace {

// Une seule source de vérité pour tous les réglages. Les clés sont courtes :
// NVS les limite à 15 caractères.
const ConfigField kFields[] = {
    {"clmb", FieldType::Float, 0.0f, 5.0f, 0.1f, "MONTEE", "M/S", true},
    {"sink", FieldType::Float, -10.0f, 0.0f, 0.1f, "DESCENTE", "M/S", true},
    {"qnh", FieldType::Float, kQnhMinHpa, kQnhMaxHpa, 1.0f, "QNH", "HPA", false},
    {"alto", FieldType::Float, -500.0f, 9000.0f, 5.0f, "ALTITUDE", "M", false},
    {"trim", FieldType::Float, 6.0f, 20.0f, 0.5f, "VITESSE AILE", "M/S", false},
    {"vol", FieldType::Int, 0.0f, 4.0f, 1.0f, "VOLUME", "", true},
    {"cont", FieldType::Int, 0.0f, 100.0f, 5.0f, "CONTRASTE", "", false},
    {"ble", FieldType::Bool, 0.0f, 1.0f, 1.0f, "BLUETOOTH", "", false},
    {"tone", FieldType::Enum, 0.0f, 1.0f, 1.0f, "SON", "", false},
};

constexpr std::size_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);

float clampToField(const ConfigField& field, float value) {
  if (!std::isfinite(value)) return field.min;
  if (value < field.min) return field.min;
  if (value > field.max) return field.max;
  return value;
}

}  // namespace

std::size_t configFieldCount() {
  return kFieldCount;
}

const ConfigField& configField(std::size_t index) {
  return kFields[index < kFieldCount ? index : 0];
}

const ConfigField* findConfigField(const char* key) {
  if (key == nullptr) return nullptr;
  for (std::size_t i = 0; i < kFieldCount; ++i) {
    if (std::strcmp(kFields[i].key, key) == 0) return &kFields[i];
  }
  return nullptr;
}

bool getConfigValue(const Config& config, const char* key, float& out) {
  if (key == nullptr) return false;

  if (std::strcmp(key, "clmb") == 0) {
    out = config.climb_threshold_ms;
  } else if (std::strcmp(key, "sink") == 0) {
    out = config.sink_threshold_ms;
  } else if (std::strcmp(key, "qnh") == 0) {
    out = config.qnh_hpa;
  } else if (std::strcmp(key, "alto") == 0) {
    out = config.altitude_offset_m;
  } else if (std::strcmp(key, "trim") == 0) {
    out = config.trim_speed_ms;
  } else if (std::strcmp(key, "vol") == 0) {
    out = static_cast<float>(config.volume);
  } else if (std::strcmp(key, "cont") == 0) {
    out = static_cast<float>(config.contrast);
  } else if (std::strcmp(key, "ble") == 0) {
    out = config.ble_enabled ? 1.0f : 0.0f;
  } else if (std::strcmp(key, "tone") == 0) {
    out = (config.tone_profile == ToneProfile::Modern) ? 1.0f : 0.0f;
  } else {
    return false;
  }
  return true;
}

bool setConfigValue(Config& config, const char* key, float value) {
  const ConfigField* field = findConfigField(key);
  if (field == nullptr) return false;

  const float clamped = clampToField(*field, value);

  if (std::strcmp(key, "clmb") == 0) {
    config.climb_threshold_ms = clamped;
  } else if (std::strcmp(key, "sink") == 0) {
    config.sink_threshold_ms = clamped;
  } else if (std::strcmp(key, "qnh") == 0) {
    config.qnh_hpa = clamped;
  } else if (std::strcmp(key, "alto") == 0) {
    config.altitude_offset_m = clamped;
  } else if (std::strcmp(key, "trim") == 0) {
    config.trim_speed_ms = clamped;
  } else if (std::strcmp(key, "vol") == 0) {
    config.volume = static_cast<int>(std::lround(clamped));
  } else if (std::strcmp(key, "cont") == 0) {
    config.contrast = static_cast<int>(std::lround(clamped));
  } else if (std::strcmp(key, "ble") == 0) {
    config.ble_enabled = clamped >= 0.5f;
  } else if (std::strcmp(key, "tone") == 0) {
    config.tone_profile = (clamped >= 0.5f) ? ToneProfile::Modern : ToneProfile::Classic;
  } else {
    return false;
  }
  return true;
}

void clampConfig(Config& config) {
  for (std::size_t i = 0; i < kFieldCount; ++i) {
    float value = 0.0f;
    if (getConfigValue(config, kFields[i].key, value)) {
      setConfigValue(config, kFields[i].key, value);
    }
  }
}

std::size_t formatConfigValue(char* buf, std::size_t size, const Config& config,
                              const char* key) {
  if (buf == nullptr || size == 0) return 0;
  buf[0] = '\0';

  const ConfigField* field = findConfigField(key);
  float value = 0.0f;
  if (field == nullptr || !getConfigValue(config, key, value)) return 0;

  int written = 0;

  if (field->zero_disables && std::fabs(value) < 1e-6f) {
    written = std::snprintf(buf, size, "OFF");
  } else if (field->type == FieldType::Bool) {
    written = std::snprintf(buf, size, value >= 0.5f ? "ON" : "OFF");
  } else if (field->type == FieldType::Enum) {
    // Le seul énuméré pour l'instant : le profil sonore.
    written = std::snprintf(buf, size, value >= 0.5f ? "MODERNE" : "CLASSIQUE");
  } else if (field->type == FieldType::Int) {
    written = std::snprintf(buf, size, "%d", static_cast<int>(std::lround(value)));
  } else if (std::fabs(field->step) >= 1.0f) {
    written = std::snprintf(buf, size, "%d", static_cast<int>(std::lround(value)));
  } else {
    written = std::snprintf(buf, size, "%.1f", static_cast<double>(value));
  }

  if (written <= 0) {
    buf[0] = '\0';
    return 0;
  }
  return (static_cast<std::size_t>(written) < size) ? static_cast<std::size_t>(written)
                                                    : size - 1;
}

}  // namespace vario
