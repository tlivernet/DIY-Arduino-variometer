#pragma once
#include <cstdint>
class TwoWire {
 public:
  void begin(int, int, std::uint32_t) {}
  void beginTransmission(std::uint8_t) {}
  std::size_t write(std::uint8_t) { return 1; }
  std::uint8_t endTransmission(bool = true) { return 0; }
  int requestFrom(int, int count) { return count; }
  int read() { return 0; }
};
extern TwoWire Wire;
