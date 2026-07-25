#pragma once

#include <cmath>

namespace vario {

// Accélération de la pesanteur, en m/s² (valeur normale OACI).
constexpr float kGravity = 9.80665f;

// Vecteur à trois composantes, strictement ce qu'il faut pour l'estimation
// d'assiette. Volontairement minimal : pas de bibliothèque d'algèbre à
// embarquer, et tout est constexpr-friendly et testable.
struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  Vec3() = default;
  constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

inline constexpr Vec3 operator+(const Vec3& a, const Vec3& b) {
  return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline constexpr Vec3 operator-(const Vec3& a, const Vec3& b) {
  return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline constexpr Vec3 operator*(const Vec3& v, float s) {
  return Vec3{v.x * s, v.y * s, v.z * s};
}

inline constexpr Vec3 operator*(float s, const Vec3& v) {
  return v * s;
}

inline constexpr float dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
  return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float norm(const Vec3& v) {
  return std::sqrt(dot(v, v));
}

inline bool isFinite(const Vec3& v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// Normalise, ou renvoie le vecteur de repli si la norme est trop faible pour
// que la direction ait un sens.
inline Vec3 normalized(const Vec3& v, const Vec3& fallback = Vec3{0.0f, 0.0f, 1.0f}) {
  const float n = norm(v);
  if (!std::isfinite(n) || n < 1e-6f) return fallback;
  return v * (1.0f / n);
}

// Borne la norme d'un vecteur sans changer sa direction.
inline Vec3 clampNorm(const Vec3& v, float max_norm) {
  const float n = norm(v);
  if (!std::isfinite(n) || n <= max_norm || n < 1e-12f) return v;
  return v * (max_norm / n);
}

}  // namespace vario
