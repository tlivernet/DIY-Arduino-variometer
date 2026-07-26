#pragma once
// Talon minimal — voir README.md. Ne reproduit pas fidèlement Arduino-ESP32.
//
// Regroupe tout le nécessaire BLE pour compiler hal/ble_uart. Les autres
// en-têtes BLE (BLEServer.h, BLEUtils.h, BLE2902.h) se contentent d'inclure
// celui-ci.
#include <cstddef>
#include <cstdint>

class BLEDescriptor {};

class BLE2902 : public BLEDescriptor {};

class BLECharacteristic {
 public:
  static constexpr uint32_t PROPERTY_NOTIFY = 0x10;
  static constexpr uint32_t PROPERTY_WRITE = 0x08;

  void addDescriptor(BLEDescriptor*) {}
  void setValue(uint8_t*, size_t) {}
  void notify() {}
};

class BLEService {
 public:
  BLECharacteristic* createCharacteristic(const char*, uint32_t) {
    return &characteristic_;
  }
  void start() {}

 private:
  BLECharacteristic characteristic_;
};

class BLEAdvertising {
 public:
  void addServiceUUID(const char*) {}
  void setScanResponse(bool) {}
  void start() {}
};

class BLEServer;

class BLEServerCallbacks {
 public:
  virtual ~BLEServerCallbacks() = default;
  virtual void onConnect(BLEServer*) {}
  virtual void onDisconnect(BLEServer*) {}
};

class BLEServer {
 public:
  void setCallbacks(BLEServerCallbacks*) {}
  BLEService* createService(const char*) { return &service_; }
  BLEAdvertising* getAdvertising() { return &advertising_; }

 private:
  BLEService service_;
  BLEAdvertising advertising_;
};

class BLEDevice {
 public:
  static void init(const char*) {}
  static void deinit(bool) {}
  static BLEServer* createServer() { return &server_; }
  static BLEAdvertising* getAdvertising() { return server_.getAdvertising(); }

 private:
  static inline BLEServer server_{};
};
