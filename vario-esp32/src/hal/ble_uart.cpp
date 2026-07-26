#include "hal/ble_uart.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace vario::hal {
namespace {

// UUID du Nordic UART Service et de sa caractéristique TX (notification), tels
// qu'attendus par XCTrack, XCSoar et LK8000.
constexpr const char* kNusService = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
constexpr const char* kNusTx = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
constexpr const char* kNusRx = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";

BLEServer* g_server = nullptr;
BLECharacteristic* g_tx = nullptr;
bool g_connected = false;

// Suivi de connexion, et relance de la publicité à la déconnexion : sans cela,
// une fois le téléphone déconnecté, l'appareil devient invisible.
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { g_connected = true; }
  void onDisconnect(BLEServer* server) override {
    g_connected = false;
    server->getAdvertising()->start();
  }
};

ServerCallbacks g_callbacks;

}  // namespace

void BleUart::begin(const Config& config) {
  if (running_) return;

  BLEDevice::init(config.device_name);
  g_server = BLEDevice::createServer();
  g_server->setCallbacks(&g_callbacks);

  BLEService* service = g_server->createService(kNusService);

  g_tx = service->createCharacteristic(kNusTx, BLECharacteristic::PROPERTY_NOTIFY);
  g_tx->addDescriptor(new BLE2902());

  // La caractéristique RX existe pour la conformité au service, même si nous ne
  // recevons rien : certains clients refusent de se connecter sans elle.
  service->createCharacteristic(kNusRx, BLECharacteristic::PROPERTY_WRITE);

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kNusService);
  advertising->setScanResponse(true);
  advertising->start();

  running_ = true;
}

void BleUart::end() {
  if (!running_) return;
  BLEDevice::deinit(true);
  g_server = nullptr;
  g_tx = nullptr;
  g_connected = false;
  running_ = false;
}

bool BleUart::connected() const {
  return running_ && g_connected;
}

void BleUart::publish(const Lk8ex1Data& data) {
  if (!connected() || g_tx == nullptr) return;

  char frame[64];
  const std::size_t n = formatLk8ex1(frame, sizeof(frame), data);
  if (n == 0) return;

  g_tx->setValue(reinterpret_cast<uint8_t*>(frame), n);
  g_tx->notify();
}

}  // namespace vario::hal
