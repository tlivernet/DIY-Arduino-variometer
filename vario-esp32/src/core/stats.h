#pragma once

#include <cstdint>

namespace vario {

// Statistiques d'un vol.
//
// Les types de 2017 étaient trop étroits et mal nommés :
//   - alti_max / alti_min en `int`, donc troncature de l'altitude flottante ;
//   - txchutemax / txchutemin en uint8_t de dixièmes de m/s, saturant au-delà
//     de 25,5 m/s, et de noms inversés (txchutemax contenait en réalité la
//     valeur absolue du taux de chute, txchutemin le meilleur taux de montée) ;
//   - cumul_alt en uint16_t, débordant au-delà de 32767 m après un
//     `(int)` intermédiaire.
struct FlightStats {
  std::uint32_t start_unix = 0;
  std::uint32_t duration_s = 0;
  float alt_max_m = 0.0f;
  float alt_min_m = 0.0f;
  float best_climb_ms = 0.0f;  // le plus fort taux de montée, positif
  float worst_sink_ms = 0.0f;  // le plus fort taux de chute, négatif
  float cumulative_gain_m = 0.0f;
  bool valid = false;
};

// Agrège les statistiques pendant un vol.
class StatsAccumulator {
 public:
  struct Config {
    // Zone morte sur le gain cumulé, en m/s.
    //
    // L'original ajoutait la variation d'altitude brute à ~30 Hz dès que le
    // vario lissé était positif. Comme l'altitude portait ~4 cm de bruit, il
    // intégrait ce bruit en continu et surestimait fortement le cumulé. On
    // intègre désormais vario·dt, et seulement au-delà d'une zone morte.
    float gain_deadband_ms = 0.15f;
  };

  StatsAccumulator() = default;
  explicit StatsAccumulator(const Config& config) : config_(config) {}

  // Démarre un vol à l'altitude courante.
  void begin(std::uint32_t start_unix, float altitude_m);

  // À appeler à la cadence du filtre vario.
  void sample(float altitude_m, float vario_ms, float dt_s);

  // Clôt le vol et fige la durée.
  void end(std::uint32_t end_unix);

  const FlightStats& stats() const { return stats_; }
  bool running() const { return running_; }

 private:
  Config config_;
  FlightStats stats_;
  bool running_ = false;
};

}  // namespace vario
