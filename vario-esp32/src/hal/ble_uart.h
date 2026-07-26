#pragma once

#include "core/lk8ex1.h"

namespace vario::hal {

// Émission des trames LK8EX1 vers un téléphone, par BLE (étape P6).
//
// Le service est le Nordic UART Service (NUS), que XCTrack, XCSoar et LK8000
// savent lire pour se servir du vario comme capteur de pression et de vitesse
// verticale. On envoie la pression BRUTE et on laisse l'altitude à l'application
// (voir core/lk8ex1) : un seul QNH, celui du téléphone, donc pas de deux
// altitudes divergentes.
//
// SUR QUEL CŒUR. La pile BLE vit sur le cœur 0. Le firmware ne fait que lui
// pousser une trame ~1 fois par seconde depuis la tâche radio ; le temps réel du
// son reste sur le cœur 1, intact.
class BleUart {
 public:
  struct Config {
    const char* device_name = "Variometre";
  };

  BleUart() = default;

  // Démarre la pile BLE et la publicité. Idempotent.
  void begin(const Config& config);
  void begin() { begin(Config{}); }

  // Arrête la publicité et libère la pile (économie d'énergie quand le BLE est
  // désactivé dans les réglages).
  void end();

  // true si un client (le téléphone) est connecté et abonné aux notifications.
  bool connected() const;

  // Formate puis notifie une trame LK8EX1. Sans abonné, ne fait rien. À appeler
  // à ~1 Hz depuis la tâche radio.
  void publish(const Lk8ex1Data& data);

  bool running() const { return running_; }

 private:
  bool running_ = false;
};

}  // namespace vario::hal
