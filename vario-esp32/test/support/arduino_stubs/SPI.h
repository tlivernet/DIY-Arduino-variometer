#pragma once
#include <cstdint>
class SPISettings {
 public:
  SPISettings(std::uint32_t, int, int) {}
};
class SPIClass {
 public:
  explicit SPIClass(int = 0) {}
  void begin(int, int, int, int) {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  std::uint8_t transfer(std::uint8_t v) { return v; }
};
