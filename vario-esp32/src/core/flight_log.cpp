#include "core/flight_log.h"

#include <cstring>

namespace vario {

namespace {
// Taille couverte par le CRC : tout sauf le champ crc16 lui-même.
constexpr std::size_t kCrcCoverage = sizeof(FlightRecord) - sizeof(std::uint16_t);
}  // namespace

std::uint16_t crc16Ccitt(const void* data, std::size_t size) {
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  std::uint16_t crc = 0xFFFF;

  for (std::size_t i = 0; i < size; ++i) {
    crc ^= static_cast<std::uint16_t>(bytes[i]) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000u) {
        crc = static_cast<std::uint16_t>((crc << 1) ^ 0x1021u);
      } else {
        crc = static_cast<std::uint16_t>(crc << 1);
      }
    }
  }
  return crc;
}

void sealFlightRecord(FlightRecord& record) {
  record.magic = kFlightMagic;
  record.record_ver = kFlightRecordVersion;
  record.crc16 = crc16Ccitt(&record, kCrcCoverage);
}

bool flightRecordValid(const FlightRecord& record) {
  if (record.magic != kFlightMagic) return false;
  if (record.record_ver == 0 || record.record_ver > kFlightRecordVersion) return false;
  return record.crc16 == crc16Ccitt(&record, kCrcCoverage);
}

bool FlightLog::append(const FlightRecord& record) {
  if (!flightRecordValid(record)) return false;

  if (count_ == 0) {
    newest_ = 0;
  } else {
    newest_ = (newest_ + 1) % kWindow;
  }
  records_[newest_] = record;

  if (count_ < kWindow) ++count_;
  ++total_appended_;

  // L'identifiant reste monotone même si l'appelant en fournit un plus grand.
  if (record.id >= next_id_) next_id_ = record.id + 1;

  return true;
}

bool FlightLog::at(std::size_t index_from_newest, FlightRecord& out) const {
  if (index_from_newest >= count_) return false;

  // On recule depuis le plus récent, en repliant les indices négatifs.
  const std::size_t slot = (newest_ + kWindow - (index_from_newest % kWindow)) % kWindow;
  out = records_[slot];
  return true;
}

void FlightLog::clear() {
  std::memset(records_, 0, sizeof(records_));
  count_ = 0;
  newest_ = 0;
  next_id_ = 1;
  total_appended_ = 0;
}

}  // namespace vario
