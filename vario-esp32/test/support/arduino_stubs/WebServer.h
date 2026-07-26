#pragma once
// Talon minimal — voir README.md. Ne reproduit pas fidèlement Arduino-ESP32.
#include <cstddef>
#include <cstring>
#include <functional>
#include <string>

#define HTTP_GET 1
#define HTTP_POST 3

// String Arduino, réduit à ce dont la couche matérielle se sert.
class String {
 public:
  String() = default;
  String(const char* s) : data_(s ? s : "") {}
  const char* c_str() const { return data_.c_str(); }

 private:
  std::string data_;
};

class WebServer {
 public:
  using Handler = std::function<void()>;

  explicit WebServer(int = 80) {}

  void on(const char*, int, Handler) {}
  void onNotFound(Handler) {}
  void begin() {}
  void stop() {}
  void handleClient() {}

  void send(int, const char*, const char*) {}
  void send_P(int, const char*, const char*) {}

  String arg(const char*) { return String(); }
};
