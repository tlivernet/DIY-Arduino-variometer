#pragma once
// Talon minimal — voir README.md. Ne reproduit pas fidèlement Arduino-ESP32.
#include <cstdarg>
#include <cstdint>
#include <cstdio>

#define OUTPUT 0x02
#define INPUT 0x01
#define INPUT_PULLUP 0x05
#define LOW 0x0
#define HIGH 0x1
#define MSBFIRST 1
#define LSBFIRST 0
#define SPI_MODE0 0x00
#define HSPI 2
#define ADC_11db 3
#define F(x) (x)

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
inline unsigned long millis() { return 0; }
inline std::uint32_t analogReadMilliVolts(int) { return 0; }
inline void analogSetPinAttenuation(int, int) {}

class HardwareSerial {
 public:
  void begin(unsigned long) {}
  void println(const char*) {}
  template <typename T>
  void println(T) {}
  template <typename T>
  void print(T) {}
  template <typename T>
  void print(T, int) {}
  int printf(const char*, ...) { return 0; }
};
extern HardwareSerial Serial;
