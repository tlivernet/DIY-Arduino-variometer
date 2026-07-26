#include "core/raw_frame.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vario {

const char* const kRawFrameCsvHeader = "t_us,ax,ay,az,gx,gy,gz,baro,p_hpa,temp_c";

std::size_t formatRawFrame(char* buf, std::size_t size, const RawFrame& frame) {
  if (buf == nullptr || size == 0) return 0;

  // %.6g garde toute la précision utile d'un float sans traîner de zéros. Le
  // temps est un entier 64 bits, écrit exact.
  const int written = std::snprintf(
      buf, size, "%lld,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%d,%.6g,%.6g",
      static_cast<long long>(frame.timestamp), static_cast<double>(frame.accel.x),
      static_cast<double>(frame.accel.y), static_cast<double>(frame.accel.z),
      static_cast<double>(frame.gyro.x), static_cast<double>(frame.gyro.y),
      static_cast<double>(frame.gyro.z), frame.has_baro ? 1 : 0,
      static_cast<double>(frame.pressure_hpa), static_cast<double>(frame.temperature_c));

  if (written < 0 || static_cast<std::size_t>(written) >= size) {
    buf[0] = '\0';
    return 0;
  }
  return static_cast<std::size_t>(written);
}

namespace {

// Avance sur les blancs de début.
const char* skipSpaces(const char* p) {
  while (*p == ' ' || *p == '\t') ++p;
  return p;
}

// Lit le prochain champ jusqu'à la virgule ou la fin de ligne. Renvoie le
// pointeur juste après le séparateur consommé (ou sur le terminateur). Écrit la
// valeur analysée dans out ; met ok à false si le champ est vide ou non
// numérique.
const char* nextFloat(const char* p, float& out, bool& ok) {
  p = skipSpaces(p);
  char* end = nullptr;
  const double v = std::strtod(p, &end);
  if (end == p) {
    ok = false;
    return p;
  }
  out = static_cast<float>(v);
  end = const_cast<char*>(skipSpaces(end));
  if (*end == ',') ++end;
  return end;
}

const char* nextLongLong(const char* p, long long& out, bool& ok) {
  p = skipSpaces(p);
  char* end = nullptr;
  const long long v = std::strtoll(p, &end, 10);
  if (end == p) {
    ok = false;
    return p;
  }
  out = v;
  end = const_cast<char*>(skipSpaces(end));
  if (*end == ',') ++end;
  return end;
}

}  // namespace

bool parseRawFrame(const char* line, RawFrame& out) {
  if (line == nullptr) return false;

  const char* p = skipSpaces(line);

  // Lignes à ignorer : vides, commentaires, et l'en-tête (repéré à son premier
  // caractère alphabétique 't').
  if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#') return false;
  if (*p == 't' && std::strncmp(p, kRawFrameCsvHeader, 4) == 0) return false;

  bool ok = true;
  RawFrame frame;

  long long ts = 0;
  p = nextLongLong(p, ts, ok);
  frame.timestamp = static_cast<Micros>(ts);

  p = nextFloat(p, frame.accel.x, ok);
  p = nextFloat(p, frame.accel.y, ok);
  p = nextFloat(p, frame.accel.z, ok);
  p = nextFloat(p, frame.gyro.x, ok);
  p = nextFloat(p, frame.gyro.y, ok);
  p = nextFloat(p, frame.gyro.z, ok);

  long long baro_flag = 0;
  p = nextLongLong(p, baro_flag, ok);
  frame.has_baro = baro_flag != 0;

  p = nextFloat(p, frame.pressure_hpa, ok);
  p = nextFloat(p, frame.temperature_c, ok);

  if (!ok) return false;
  if (!isFinite(frame.accel) || !isFinite(frame.gyro)) return false;
  if (frame.has_baro &&
      (!std::isfinite(frame.pressure_hpa) || !std::isfinite(frame.temperature_c))) {
    return false;
  }

  out = frame;
  return true;
}

}  // namespace vario
