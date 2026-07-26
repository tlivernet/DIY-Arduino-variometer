#include "core/config_json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vario {
namespace {

const char* fieldTypeName(FieldType type) {
  switch (type) {
    case FieldType::Float:
      return "float";
    case FieldType::Int:
      return "int";
    case FieldType::Bool:
      return "bool";
    case FieldType::Enum:
      return "enum";
  }
  return "float";
}

// Ajoute une chaîne au tampon en suivant une position courante. Renvoie false
// si ça ne tient pas ; l'appelant abandonne alors proprement.
bool append(char* buf, std::size_t size, std::size_t& pos, const char* text) {
  const std::size_t len = std::strlen(text);
  if (pos + len >= size) return false;
  std::memcpy(buf + pos, text, len);
  pos += len;
  return true;
}

// Ajoute un nombre formaté sobrement : entier si la valeur est entière, sinon
// un décimal court. Évite les « 5.000000 » dans le JSON.
bool appendNumber(char* buf, std::size_t size, std::size_t& pos, float value) {
  char tmp[32];
  int n;
  if (value == std::floor(value) && std::fabs(value) < 1e7f) {
    n = std::snprintf(tmp, sizeof(tmp), "%ld", static_cast<long>(value));
  } else {
    n = std::snprintf(tmp, sizeof(tmp), "%g", static_cast<double>(value));
  }
  if (n <= 0) return false;
  return append(buf, size, pos, tmp);
}

// Recopie une chaîne en échappant ce que JSON impose. Nos libellés sont en
// majuscules ASCII sans guillemet, mais on ne fait pas cette hypothèse.
bool appendEscaped(char* buf, std::size_t size, std::size_t& pos, const char* text) {
  for (const char* p = text; *p != '\0'; ++p) {
    const char c = *p;
    if (c == '"' || c == '\\') {
      if (pos + 2 >= size) return false;
      buf[pos++] = '\\';
      buf[pos++] = c;
    } else {
      if (pos + 1 >= size) return false;
      buf[pos++] = c;
    }
  }
  return true;
}

}  // namespace

std::size_t configToJson(char* buf, std::size_t size, const Config& config) {
  if (buf == nullptr || size == 0) return 0;

  std::size_t pos = 0;
  bool ok = append(buf, size, pos, "{\"fields\":[");

  const std::size_t count = configFieldCount();
  for (std::size_t i = 0; ok && i < count; ++i) {
    const ConfigField& f = configField(i);
    float value = 0.0f;
    getConfigValue(config, f.key, value);

    char text[24];
    formatConfigValue(text, sizeof(text), config, f.key);

    if (i != 0) ok = ok && append(buf, size, pos, ",");
    ok = ok && append(buf, size, pos, "{\"key\":\"");
    ok = ok && appendEscaped(buf, size, pos, f.key);
    ok = ok && append(buf, size, pos, "\",\"label\":\"");
    ok = ok && appendEscaped(buf, size, pos, f.label);
    ok = ok && append(buf, size, pos, "\",\"unit\":\"");
    ok = ok && appendEscaped(buf, size, pos, f.unit);
    ok = ok && append(buf, size, pos, "\",\"type\":\"");
    ok = ok && append(buf, size, pos, fieldTypeName(f.type));
    ok = ok && append(buf, size, pos, "\",\"min\":");
    ok = ok && appendNumber(buf, size, pos, f.min);
    ok = ok && append(buf, size, pos, ",\"max\":");
    ok = ok && appendNumber(buf, size, pos, f.max);
    ok = ok && append(buf, size, pos, ",\"step\":");
    ok = ok && appendNumber(buf, size, pos, f.step);
    ok = ok && append(buf, size, pos, ",\"value\":");
    ok = ok && appendNumber(buf, size, pos, value);
    ok = ok && append(buf, size, pos, ",\"text\":\"");
    ok = ok && appendEscaped(buf, size, pos, text);
    ok = ok && append(buf, size, pos, "\"}");
  }

  ok = ok && append(buf, size, pos, "]}");

  if (!ok) {
    buf[0] = '\0';
    return 0;
  }
  buf[pos] = '\0';
  return pos;
}

namespace {

// Mini-parseur pour un objet JSON PLAT {"clé":valeur,...}. Suffisant et sûr
// pour ce que le portail envoie ; on n'embarque pas de bibliothèque JSON pour
// une seule structure sans imbrication.
//
// Reconnaît : chaînes entre guillemets (clés et valeurs textuelles), nombres,
// true/false. Ignore les blancs. Tolère un objet mal formé en s'arrêtant.

const char* skipWs(const char* p) {
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
  return p;
}

// Lit une chaîne JSON dans out (tronquée à out_size). Renvoie le pointeur après
// le guillemet fermant, ou nullptr si malformé.
const char* parseString(const char* p, char* out, std::size_t out_size) {
  if (*p != '"') return nullptr;
  ++p;
  std::size_t i = 0;
  while (*p != '\0' && *p != '"') {
    char c = *p++;
    if (c == '\\' && *p != '\0') c = *p++;  // déséchappement minimal
    if (i + 1 < out_size) out[i++] = c;
  }
  if (*p != '"') return nullptr;
  if (out_size > 0) out[i] = '\0';
  return p + 1;
}

// Lit une valeur scalaire (nombre, true/false) et la convertit en float.
// Renvoie le pointeur après la valeur, ou nullptr si non reconnue.
const char* parseScalar(const char* p, float& value, bool& ok) {
  if (std::strncmp(p, "true", 4) == 0) {
    value = 1.0f;
    ok = true;
    return p + 4;
  }
  if (std::strncmp(p, "false", 5) == 0) {
    value = 0.0f;
    ok = true;
    return p + 5;
  }
  char* end = nullptr;
  const double v = std::strtod(p, &end);
  if (end == p) {
    ok = false;
    return nullptr;
  }
  value = static_cast<float>(v);
  ok = std::isfinite(value);
  return end;
}

}  // namespace

int applyJsonToConfig(const char* json, Config& config) {
  if (json == nullptr) return 0;

  const char* p = skipWs(json);
  if (*p != '{') return 0;
  ++p;

  int applied = 0;

  for (;;) {
    p = skipWs(p);
    if (*p == '}' || *p == '\0') break;

    char key[16];
    p = parseString(p, key, sizeof(key));
    if (p == nullptr) break;

    p = skipWs(p);
    if (*p != ':') break;
    p = skipWs(p + 1);

    // La valeur peut être une chaîne (on l'ignore : le portail envoie des
    // nombres) ou un scalaire.
    if (*p == '"') {
      char dummy[24];
      p = parseString(p, dummy, sizeof(dummy));
      if (p == nullptr) break;
    } else {
      float value = 0.0f;
      bool ok = false;
      p = parseScalar(p, value, ok);
      if (p == nullptr) break;
      if (ok && setConfigValue(config, key, value)) ++applied;
    }

    p = skipWs(p);
    if (*p == ',') {
      ++p;
      continue;
    }
    if (*p == '}') break;
    // Séparateur inattendu : on arrête proprement plutôt que de boucler.
    break;
  }

  return applied;
}

}  // namespace vario
