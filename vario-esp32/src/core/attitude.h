#pragma once

#include "core/vec3.h"

namespace vario {

// Estimation d'assiette à partir d'une centrale 6 axes (accéléromètre +
// gyromètre), sans magnétomètre.
//
// Simplification qui rend l'ensemble bien plus simple qu'une fusion complète :
// on n'a besoin que de la DIRECTION du bas terrestre exprimée dans le repère
// du boîtier, soit une grandeur à 2 degrés de liberté. Le cap est de toute
// façon inobservable avec 6 axes, et il est ici inutile : tourner autour de la
// verticale terrestre ne change pas les coordonnées de cette verticale. Donc
// pas de quaternion, pas de magnétomètre, pas de gestion de lacet — un seul
// vecteur unitaire à suivre.
//
// LE PROBLÈME CENTRAL : LA FORCE CENTRIFUGE
//
// En virage coordonné à l'inclinaison φ, l'accéléromètre mesure le facteur de
// charge, de module g/cos(φ), aligné sur le « bas » de l'aile et non sur le bas
// terrestre. Un filtre de Mahony naïf converge donc vers « inclinaison nulle »
// et fabrique une accélération verticale parasite de g·(sec φ − 1), soit
// **1,52 m/s² à 30° d'inclinaison** — ce qui ferait dériver la vitesse
// verticale de ~1,5 m/s par seconde. Inacceptable pour un vario, et c'est
// précisément la situation dans laquelle un parapentiste passe son temps.
//
// Trois lignes de défense, par ordre d'importance :
//
// 1. Gains gyro-dominants. Kp = 1/15 s⁻¹ : l'accéléromètre ne recale l'assiette
//    qu'à l'échelle de 15 s. Un virage de 25 s est donc suivi par le gyromètre,
//    lui insensible à la force centrifuge.
//
// 2. Compensation centrifuge à vitesse-bras fixe. C'est l'astuce métier qui
//    permet de s'en sortir sans GPS : **un parapente vole à une vitesse de
//    calage quasi constante**, 9 à 11 m/s. Poser V₀ = 10 m/s donne la force
//    centrifuge à ±15 % près, soit moins de 2° d'erreur d'assiette même à 45°.
//    Réglable (une aile de vitesse met 14, un biplace 11).
//
// 3. Fenêtre d'acceptation sur le module du facteur de charge, pour écarter les
//    transitoires violents (turbulence, sortie de fermeture) plutôt que les
//    virages établis.
class AttitudeFilter {
 public:
  struct Config {
    // Gain de recalage de l'assiette par l'accéléromètre, en s⁻¹.
    float kp = 1.0f / 15.0f;
    // Gain d'estimation du biais gyrométrique, en s⁻¹.
    float ki = 1.0f / 600.0f;
    // Vitesse-air de calage supposée, en m/s.
    float trim_speed_ms = 10.0f;
    // Écart toléré entre le module mesuré et g avant de refuser le recalage.
    float accel_gate_ms2 = 0.8f;
    // Vitesse de rotation au-delà de laquelle on refuse le recalage, en rad/s.
    float gyro_gate_rps = 1.05f;  // ~60 °/s
    // Borne sur le biais gyrométrique estimé, en rad/s.
    float max_bias_rps = 0.0524f;  // ~3 °/s
    // Active la compensation centrifuge. Le mettre à false sert à démontrer,
    // par un test, que c'est bien elle qui fait le travail.
    bool centrifugal_compensation = true;
  };

  AttitudeFilter() = default;
  explicit AttitudeFilter(const Config& config) : config_(config) {}

  // Initialise l'assiette depuis une moyenne de mesures prises à l'arrêt.
  //
  // Comme le bas estimé est défini comme la direction moyenne mesurée,
  // l'accélération verticale calculée est identiquement nulle dans l'assiette
  // de démarrage : le biais du filtre de Kalman part donc juste par
  // construction, et n'a plus qu'à suivre ses variations ultérieures. C'est
  // bien plus sûr que de partir à l'aveugle.
  void align(const Vec3& mean_accel, const Vec3& mean_gyro);

  // Intègre un échantillon de la centrale. dt en secondes.
  void update(const Vec3& accel_mps2, const Vec3& gyro_rps, float dt_s);

  // Direction du bas terrestre, dans le repère du boîtier.
  const Vec3& down() const { return down_; }
  const Vec3& gyroBias() const { return gyro_bias_; }
  bool aligned() const { return aligned_; }

  // Accélération verticale, positive vers le haut, nulle à l'arrêt, en m/s².
  //
  // L'accéléromètre mesure l'accélération propre : à l'arrêt, accel·bas = −g.
  // D'où le signe. Vérification : à l'arrêt, −(−g) − g = 0.
  float verticalAccel(const Vec3& accel_mps2) const;

  // true si le dernier échantillon a effectivement servi à recaler l'assiette.
  bool lastSampleAccepted() const { return last_accepted_; }

  Config& config() { return config_; }
  const Config& config() const { return config_; }

 private:
  Vec3 centrifugalAccel(const Vec3& omega) const;

  Config config_;
  Vec3 down_{0.0f, 0.0f, 1.0f};
  Vec3 gyro_bias_{0.0f, 0.0f, 0.0f};
  bool aligned_ = false;
  bool last_accepted_ = false;
};

}  // namespace vario
