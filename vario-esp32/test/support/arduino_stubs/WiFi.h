#pragma once
// Talon minimal — voir README.md. Ne reproduit pas fidèlement Arduino-ESP32.

enum WiFiMode_t { WIFI_OFF = 0, WIFI_AP = 2 };

class WiFiClass {
 public:
  void mode(WiFiMode_t) {}
  bool softAP(const char*, const char* = nullptr, int = 1) { return true; }
  void softAPdisconnect(bool = false) {}
};

extern WiFiClass WiFi;
