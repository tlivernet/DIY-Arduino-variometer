#pragma once
// Talon minimal — voir README.md. Ne reproduit pas fidèlement Arduino-ESP32.
#include <cstddef>

class Preferences {
 public:
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  float getFloat(const char*, float defaultValue = 0.0f) { return defaultValue; }
  std::size_t putFloat(const char*, float) { return 4; }
};
