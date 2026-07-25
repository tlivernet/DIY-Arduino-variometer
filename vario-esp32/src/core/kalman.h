#pragma once

#include <cstdint>

namespace vario {

// Filtre de Kalman à 3 états pour la vitesse verticale, fusionnant altitude
// barométrique et accélération verticale.
//
// État : [altitude (m), vitesse verticale (m/s, positive vers le haut),
//         biais d'accélération verticale (m/s²)]
//
// Le troisième état absorbe non seulement le biais propre du capteur mais aussi
// l'erreur d'assiette résiduelle : ce n'est donc pas une caractéristique du
// composant, et il doit pouvoir évoluer lentement.
//
// POURQUOI C'EST À LA FOIS PLUS RAPIDE ET MOINS BRUYANT QUE L'EMA DE 2017
//
// Point à saisir avant de toucher aux réglages : la boucle de correction
// barométrique est délibérément LENTE (τ ≈ 3,3 s). Lue naïvement, cette
// constante de temps suggère un filtre plus lent que l'EMA de 0,8 s du montage
// AVR. C'est faux, parce que **l'accélération est une entrée de commande, pas
// une mesure** : l'état de vitesse répond à une accélération réelle par
// intégration pure, sans aucun retard. La faible bande passante de correction
// achète le faible bruit, l'action directe achète la rapidité. On obtient les
// deux en même temps, ce que l'EMA purement barométrique ne pouvait pas faire.
//
// Attendu : ~0,04 m/s de bruit au repos (0,08 à 0,12 en vol, les erreurs
// d'assiette étant corrélées et non blanches) contre 0,3 à 0,4 m/s en 2017.
// C'est cette division par 4 à 8 qui rend enfin utilisable un seuil de montée
// à 0,2 m/s : l'ancien seuil de 0,3 m/s était SOUS le plancher de bruit, d'où
// le bipeur qui bavardait à l'arrêt.
class VarioKalman {
 public:
  struct Config {
    // Densité spectrale du bruit d'accélération, en m²s⁻⁴/Hz.
    // Équivaut à une erreur d'accélération verticale de ~0,31 m/s² à 96 Hz,
    // dominée par l'erreur d'assiette et non par le bruit du capteur.
    float accel_psd = 2.0e-3f;

    // Marche aléatoire du biais, en (m/s²)²/s. 0,005 m/s² par √s, soit
    // 0,05 m/s² sur 100 s : assez lent pour ne pas courir après les
    // transitoires de virage, assez rapide pour suivre la dérive thermique.
    float bias_psd = 2.5e-5f;

    // Écart-type de la mesure d'altitude barométrique, en m.
    //
    // Ce n'est PAS la résolution du capteur. Le MS5611 donne ~10 cm, mais la
    // microturbulence atmosphérique réelle et l'aérodynamique de la prise de
    // pression ajoutent 1 à 3 Pa de fluctuation authentique. D'où 0,20 m en
    // vol, ~0,12 m posé sur une table.
    float altitude_sigma = 0.20f;

    // Au-delà de ce nombre d'écarts-types, la mesure est adoucie (Huber)
    // plutôt que rejetée : un rejet franc crée des blocages.
    float huber_sigmas = 3.0f;

    // Garde de divergence : écart persistant entre l'état et le baro brut.
    float divergence_m = 3.0f;
    float divergence_hold_s = 2.0f;

    // Bornes sur Δt. Comme en 2017 le code ne testait rien et produisait un
    // NaN définitif, tout est borné ici.
    float min_dt_s = 1e-5f;
    float max_dt_s = 0.5f;
  };

  VarioKalman() = default;
  explicit VarioKalman(const Config& config) : config_(config) {}

  // (Ré)initialise sur une altitude mesurée.
  void reset(float altitude_m);

  // Prédiction : intègre une accélération verticale (m/s², positive vers le
  // haut). Renvoie false si l'échantillon a été écarté.
  bool predict(float vertical_accel_ms2, float dt_s);

  // Correction par l'altitude barométrique.
  bool update(float altitude_m);

  float altitude() const { return altitude_; }
  float velocity() const { return velocity_; }
  float bias() const { return bias_; }

  // Dernier résidu de mesure (baro − estimation), en m.
  float innovation() const { return innovation_; }

  // Nombre de réinitialisations déclenchées par la garde de divergence.
  // Exposé pour pouvoir l'afficher en diagnostic : une valeur non nulle en vol
  // signale un vrai problème.
  std::uint32_t resetCount() const { return reset_count_; }

  bool initialized() const { return initialized_; }

  Config& config() { return config_; }
  const Config& config() const { return config_; }

 private:
  void symmetrize();
  bool sane() const;

  Config config_;
  float altitude_ = 0.0f;
  float velocity_ = 0.0f;
  float bias_ = 0.0f;
  float innovation_ = 0.0f;

  // Covariance. σ initiaux : 2 m d'altitude, 1 m/s de vitesse, 0,5 m/s² de
  // biais (de quoi couvrir ±50 mg de biais capteur et ~2° d'erreur de montage).
  float p_[3][3] = {{4.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.25f}};

  float divergent_for_s_ = 0.0f;
  std::uint32_t reset_count_ = 0;
  bool initialized_ = false;
};

}  // namespace vario
