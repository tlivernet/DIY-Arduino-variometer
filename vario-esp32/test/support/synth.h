#pragma once

// Générateurs de trajectoires synthétiques pour tester la chaîne d'estimation
// sans matériel.
//
// Le point clé est la COHÉRENCE MUTUELLE : accéléromètre, gyromètre et pression
// sont dérivés d'une même vérité terrain, exactement comme le seraient de vrais
// capteurs. Un filtre qui se comporte bien sur ces jeux de données se comportera
// bien en vol, aux imperfections des capteurs près.
//
// Repère terrestre : X nord, Y est, Z vers le BAS.
// Repère boîtier   : X avant, Y droite, Z vers le bas du boîtier.

#include <cmath>
#include <random>

#include "core/isa.h"
#include "core/vec3.h"

namespace synth {

using vario::Vec3;

// Matrice de rotation 3×3, terre → boîtier.
struct Mat3 {
  float m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

  Vec3 operator*(const Vec3& v) const {
    return Vec3{m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
                m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
                m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
  }
};

// Rotation terre → boîtier depuis les angles d'Euler (cap, assiette, roulis),
// convention aéronautique Rx(roulis)·Ry(assiette)·Rz(cap).
inline Mat3 earthToBody(float yaw, float pitch, float roll) {
  const float cy = std::cos(yaw), sy = std::sin(yaw);
  const float cp = std::cos(pitch), sp = std::sin(pitch);
  const float cr = std::cos(roll), sr = std::sin(roll);

  Mat3 r;
  r.m[0][0] = cp * cy;
  r.m[0][1] = cp * sy;
  r.m[0][2] = -sp;
  r.m[1][0] = sr * sp * cy - cr * sy;
  r.m[1][1] = sr * sp * sy + cr * cy;
  r.m[1][2] = sr * cp;
  r.m[2][0] = cr * sp * cy + sr * sy;
  r.m[2][1] = cr * sp * sy - sr * cy;
  r.m[2][2] = cr * cp;
  return r;
}

// Un échantillon de capteurs, tel que le firmware le verrait.
struct Sample {
  Vec3 accel;          // m/s², accélération propre dans le repère boîtier
  Vec3 gyro;           // rad/s, repère boîtier
  float pressure_hpa;  // hPa
  float true_vertical_speed = 0.0f;  // m/s, positive vers le haut
  float true_altitude_m = 0.0f;
};

// Pression correspondant à une altitude, sous un QNH donné : inverse exact de
// la formule ISA utilisée par le firmware.
inline float pressureForAltitude(float altitude_m, float qnh_hpa = 1013.25f) {
  return qnh_hpa *
         std::pow(1.0f - altitude_m / vario::kIsaScaleM, 1.0f / vario::kIsaExponent);
}

// Vol en palier, boîtier à plat, immobile.
inline Sample level(float altitude_m = 1000.0f) {
  Sample s;
  // À l'arrêt, l'accéléromètre mesure l'accélération propre, soit −g vers le
  // bas, c'est-à-dire +g vers le haut du boîtier. Avec Z boîtier vers le bas :
  s.accel = Vec3{0.0f, 0.0f, -vario::kGravity};
  s.gyro = Vec3{0.0f, 0.0f, 0.0f};
  s.pressure_hpa = pressureForAltitude(altitude_m);
  s.true_altitude_m = altitude_m;
  return s;
}

// Montée à vitesse verticale constante.
inline Sample climb(float rate_ms, float t_s, float altitude0_m = 1000.0f) {
  const float altitude = altitude0_m + rate_ms * t_s;
  Sample s = level(altitude);
  // Vitesse constante ⇒ aucune accélération propre supplémentaire.
  s.true_vertical_speed = rate_ms;
  return s;
}

// VIRAGE COORDONNÉ ÉTABLI, sans ascendance.
//
// C'est le jeu de données décisif : la vitesse verticale VRAIE est nulle, mais
// l'accéléromètre mesure un facteur de charge de g/cos(φ) aligné sur le bas de
// l'aile et non sur le bas terrestre. Un filtre naïf en déduit une fausse
// ascendance de g·(sec φ − 1), soit 1,52 m/s² à 30° d'inclinaison.
//
// Inclinaison constante ⇒ la vitesse de rotation du boîtier est exactement la
// rotation autour de la verticale terrestre, ce qui donne un gyromètre exact
// sans dérivation numérique.
inline Sample coordinatedTurn(float bank_rad, float t_s, float speed_ms = 10.0f,
                              float altitude_m = 1000.0f) {
  // Virage coordonné : tan(φ) = V·Ω / g.
  const float turn_rate = vario::kGravity * std::tan(bank_rad) / speed_ms;
  const float yaw = turn_rate * t_s;

  const Mat3 r = earthToBody(yaw, 0.0f, bank_rad);

  // Avant et droite dans le repère terrestre, à ce cap.
  const Vec3 right{-std::sin(yaw), std::cos(yaw), 0.0f};

  // Accélération centripète : vers le centre, donc vers la droite en virage à
  // droite (Ω > 0 autour de Z vers le bas).
  const Vec3 centripetal = right * (speed_ms * turn_rate);
  const Vec3 gravity_earth{0.0f, 0.0f, vario::kGravity};

  Sample s;
  // Accélération propre = accélération inertielle − pesanteur.
  s.accel = r * (centripetal - gravity_earth);
  s.gyro = r * Vec3{0.0f, 0.0f, turn_rate};
  s.pressure_hpa = pressureForAltitude(altitude_m);
  s.true_altitude_m = altitude_m;
  s.true_vertical_speed = 0.0f;
  return s;
}

// Ajoute du bruit gaussien reproductible à la pression, exprimé en mètres
// d'altitude équivalents.
class Noise {
 public:
  explicit Noise(unsigned seed = 12345u) : rng_(seed) {}

  float altitudeNoiseM(float sigma_m) {
    std::normal_distribution<float> dist(0.0f, sigma_m);
    return dist(rng_);
  }

  Vec3 accelNoise(float sigma_ms2) {
    std::normal_distribution<float> dist(0.0f, sigma_ms2);
    return Vec3{dist(rng_), dist(rng_), dist(rng_)};
  }

 private:
  std::mt19937 rng_;
};

inline float degToRad(float deg) {
  return deg * 3.14159265358979f / 180.0f;
}

}  // namespace synth
