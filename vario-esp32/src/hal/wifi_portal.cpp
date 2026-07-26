#include "hal/wifi_portal.h"

#include <WebServer.h>
#include <WiFi.h>

#include "core/config_json.h"
#include "core/web_page.h"

namespace vario::hal {
namespace {

// Un seul serveur, sur le port HTTP standard. Statique pour que les gestionnaires
// enregistrés par lambda puissent y accéder sans capture — l'API WebServer prend
// des fonctions libres.
WebServer* g_server = nullptr;
WifiPortal* g_portal = nullptr;

}  // namespace

bool WifiPortal::begin(const Config& portal) {
  if (running_) return true;

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(portal.ssid, portal.password, portal.channel)) {
    return false;
  }

  static WebServer server(80);
  g_server = &server;
  g_portal = this;

  // La page se sert depuis la flash, sans copie en RAM.
  server.on("/", HTTP_GET, [] { g_portal->handleRoot(); });
  server.on("/config", HTTP_GET, [] { g_portal->handleGetConfig(); });
  server.on("/config", HTTP_POST, [] { g_portal->handlePostConfig(); });
  server.onNotFound([] {
    // Toute autre requête renvoie la page : un portail captif basique.
    g_portal->handleRoot();
  });

  server.begin();
  running_ = true;
  return true;
}

void WifiPortal::end() {
  if (!running_) return;
  if (g_server != nullptr) g_server->stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  running_ = false;
}

void WifiPortal::handle() {
  if (running_ && g_server != nullptr) g_server->handleClient();
}

bool WifiPortal::takeDirty() {
  const bool was = dirty_;
  dirty_ = false;
  return was;
}

void WifiPortal::handleRoot() {
  g_server->send_P(200, "text/html", kConfigPageHtml);
}

void WifiPortal::handleGetConfig() {
  // Tampon dimensionné large : le document tient largement dans 2 Ko pour la
  // dizaine de réglages actuels.
  static char buf[2048];
  const std::size_t n = configToJson(buf, sizeof(buf), config_);
  if (n == 0) {
    g_server->send(500, "application/json", "{}");
    return;
  }
  g_server->send(200, "application/json", buf);
}

void WifiPortal::handlePostConfig() {
  const String body = g_server->arg("plain");
  const int applied = applyJsonToConfig(body.c_str(), config_);
  if (applied > 0) dirty_ = true;

  // On renvoie la config à jour : le client peut ainsi confirmer les valeurs
  // effectivement retenues (bornées), sans second aller-retour.
  static char buf[2048];
  configToJson(buf, sizeof(buf), config_);
  g_server->send(200, "application/json", buf);
}

}  // namespace vario::hal
