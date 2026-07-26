#pragma once

#include <cstddef>
#include <cstdint>

namespace vario {

// Enregistrement d'un vol : exactement 64 octets, largeurs explicites.
//
// Le montage de 2017 écrivait ses structures en EEPROM par memcpy brut, à des
// offsets calculés avec sizeof(). Deux conséquences : le moindre changement de
// champ décalait tous les enregistrements suivants, et la disposition dépendait
// du compilateur (sur AVR un int fait 2 octets et l'alignement est de 1 ; sur
// ESP32 c'est 4 et 4). D'où des largeurs explicites et un static_assert.
struct FlightRecord {
  std::uint16_t magic;      // kFlightMagic
  std::uint8_t record_ver;  // version de la disposition
  std::uint8_t flags;
  std::uint32_t id;  // monotone, jamais réutilisé
  std::uint32_t start_unix;
  std::uint32_t duration_s;
  std::int32_t alt_start_dm;  // décimètres, pour tenir en 32 bits signés
  std::int32_t alt_max_dm;
  std::int32_t alt_min_dm;
  std::int32_t alt_end_dm;
  std::int16_t climb_max_dms;  // dixièmes de m/s, comme l'IHM les affiche
  std::int16_t sink_max_dms;
  std::uint32_t gain_m;  // 32 bits : le uint16_t de 2017 débordait à 65535
  std::uint32_t qnh_pa;
  std::int16_t temp_min_dc;  // dixièmes de °C
  std::int16_t temp_max_dc;
  std::uint8_t batt_start_pct;
  std::uint8_t batt_end_pct;
  std::uint8_t reserved[12];  // réservé pour éviter de changer record_ver
  std::uint16_t crc16;        // sur les 62 premiers octets
};

static_assert(sizeof(FlightRecord) == 64, "FlightRecord doit faire exactement 64 octets");

constexpr std::uint16_t kFlightMagic = 0x564C;  // « VL »
constexpr std::uint8_t kFlightRecordVersion = 1;

// Drapeaux.
constexpr std::uint8_t kFlagRecovered = 1u << 0;  // reconstitué après coupure

// CRC-16/CCITT-FALSE.
std::uint16_t crc16Ccitt(const void* data, std::size_t size);

// Renseigne magie, version et CRC.
void sealFlightRecord(FlightRecord& record);

// Vérifie magie et CRC.
bool flightRecordValid(const FlightRecord& record);

// Journal de vols, en ajout seul.
//
// Remplace le carrousel de 8 emplacements de 2017, qui produisait le bug
// suivant : après un atterrissage, chrono_start et chrono étaient tous deux non
// nuls, donc plus aucun bloc de test ne s'exécutait ; si l'emplacement suivant
// était occupé, l'appareil n'enregistrait PLUS AUCUN VOL, silencieusement, au
// bout de huit vols.
//
// Ici il n'y a plus d'index d'emplacement à faire tourner : on ajoute, et quand
// c'est plein on évince le plus ancien. Le bug n'est pas corrigé, il est
// devenu impossible à écrire. L'IHM continue de présenter « M1 » à « M8 », qui
// ne sont plus qu'une vue sur les huit enregistrements les plus récents.
class FlightLog {
 public:
  // Nombre d'enregistrements conservés en mémoire vive pour l'IHM.
  // Le journal sur mémoire flash en contient bien plus.
  static constexpr std::size_t kWindow = 8;

  // Ajoute un vol. Renvoie false si l'enregistrement est invalide.
  bool append(const FlightRecord& record);

  // Nombre d'enregistrements dans la fenêtre.
  std::size_t count() const { return count_; }

  // Nombre total de vols ajoutés depuis toujours, évictions comprises.
  std::uint32_t totalAppended() const { return total_appended_; }

  // Accès du plus récent (index 0, soit « M1 ») au plus ancien.
  // Renvoie false si l'index est hors de la fenêtre.
  bool at(std::size_t index_from_newest, FlightRecord& out) const;

  // Identifiant à attribuer au prochain vol.
  std::uint32_t nextId() const { return next_id_; }

  void clear();

 private:
  FlightRecord records_[kWindow] = {};
  std::size_t count_ = 0;
  std::size_t newest_ = 0;  // index dans records_ du plus récent
  std::uint32_t next_id_ = 1;
  std::uint32_t total_appended_ = 0;
};

}  // namespace vario
