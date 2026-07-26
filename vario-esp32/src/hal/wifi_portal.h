#pragma once

#include "core/config_schema.h"

namespace vario::hal {

// Portail Wi-Fi de configuration (étape P6).
//
// L'appareil ouvre un point d'accès isolé (pas de mot de passe partagé avec un
// routeur, pas d'accès Internet) et sert une page unique qui se dessine depuis
// le schéma des réglages. On s'y connecte avec un téléphone, on modifie, on
// enregistre : c'est le remplaçant du menu à encodeur pour les réglages fins,
// bien plus confortable qu'un bouton unique.
//
// SUR QUEL CŒUR. Le portail (Wi-Fi + serveur HTTP) vit sur le cœur 0, avec la
// pile radio, exactement comme le BLE. Le cœur 1 garde le temps réel du son et
// des capteurs intact : ouvrir le portail ne doit pas faire hoqueter les bips.
//
// La logique métier (rendu de la page, lecture/écriture des réglages en JSON)
// est dans core/ et se teste sans matériel. Cette classe n'est que le transport
// HTTP autour d'elle.
class WifiPortal {
 public:
  struct Config {
    // Nom du réseau ouvert. Court et reconnaissable.
    const char* ssid = "Variometre";
    // Mot de passe. Au moins 8 caractères pour un WPA2 ; vide = réseau ouvert.
    const char* password = "parapente";
    // Canal Wi-Fi.
    int channel = 1;
  };

  explicit WifiPortal(vario::Config& config) : config_(config) {}

  // Démarre le point d'accès et le serveur HTTP. Renvoie false si le point
  // d'accès n'a pas pu être créé.
  bool begin(const Config& portal);
  bool begin() { return begin(Config{}); }

  // Coupe le serveur et le point d'accès, et rend la radio à d'autres usages.
  void end();

  // À appeler régulièrement depuis la tâche radio : traite les requêtes en
  // attente. Sans appel régulier, le serveur ne répond pas.
  void handle();

  bool running() const { return running_; }

  // true dès qu'un réglage a été enregistré via le portail : la tâche
  // principale doit alors persister la config et relire ses filtres. Remis à
  // false à la lecture.
  bool takeDirty();

 private:
  void handleGetConfig();
  void handlePostConfig();
  void handleRoot();

  vario::Config& config_;
  bool running_ = false;
  bool dirty_ = false;
};

}  // namespace vario::hal
