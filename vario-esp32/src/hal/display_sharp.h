#pragma once

#include <cstdint>

#include "ui/canvas.h"

namespace vario::hal {

// Écran Sharp Memory LCD LS027B7DH01, 400×240, en SPI.
//
// Particularités à ne pas découvrir en route :
//
//  - **CS est actif à l'état HAUT** (broche SCS), contrairement à l'immense
//    majorité des périphériques SPI. Il est donc piloté à la main en GPIO.
//  - **Horloge à 1 MHz.** La fiche technique spécifie fSCLK = 1 MHz, 1,1 MHz en
//    limite. Beaucoup de projets poussent à 2 MHz et s'en sortent ; l'écran
//    n'étant pas le goulot dès qu'on ne transmet que les lignes modifiées, ce
//    n'est pas un pari à prendre.
//  - **Le basculement VCOM est obligatoire**, à environ 1 Hz. Une polarisation
//    continue du cristal liquide provoque une rémanence permanente. Il est
//    réalisé ici par un minuteur logiciel et non par un canal PWM : sur
//    l'ESP32-S3, la fréquence minimale d'un canal LEDC reste très au-dessus du
//    1 Hz voulu.
//  - Une trame complète représente 12,5 ko, soit ~100 ms à 1 MHz. D'où le
//    transfert par lignes modifiées uniquement, qui n'est pas une optimisation
//    mais une condition pour rafraîchir à 10 Hz.
class SharpDisplay {
 public:
  struct Config {
    int pin_sclk;
    int pin_mosi;
    int pin_cs;
    int pin_disp;
    int pin_extcomin;
    std::uint32_t frequency_hz = 1000000;

    // ORDRE DES BITS — le point le plus susceptible d'être faux au premier
    // allumage, et impossible à vérifier sans écran.
    //
    // Le Sharp reçoit ses octets en commençant par le bit de poids FAIBLE, et
    // ce premier bit reçu correspond au pixel le plus à GAUCHE. Notre tampon
    // stocke le pixel de gauche dans le bit de poids FORT. Transmettre en
    // MSB d'abord place donc bien le pixel de gauche en premier sur le fil :
    // les octets de pixels partent tels quels.
    //
    // En revanche l'octet de commande et l'adresse de ligne sont des NOMBRES,
    // pas des motifs de pixels : pour qu'ils arrivent bit de poids faible en
    // tête, il faut les inverser avant émission.
    //
    // Si à l'allumage l'image apparaît miroitée par groupes de 8 pixels,
    // passer reverse_pixel_bytes à true. Si l'image est correcte mais décalée
    // verticalement ou brouillée, c'est reverse_control_bytes qu'il faut
    // changer.
    bool reverse_pixel_bytes = false;
    bool reverse_control_bytes = true;
  };

  explicit SharpDisplay(const Config& config) : config_(config) {}

  // Configure les broches et le bus, puis efface l'écran.
  void begin();

  // Allume ou éteint l'affichage sans perdre le contenu.
  void setEnabled(bool enabled);

  // Efface tout l'écran par la commande dédiée, plus rapide qu'un transfert
  // complet.
  void clear();

  // Transmet uniquement les lignes marquées comme modifiées, puis remet le
  // marquage à zéro.
  void flushDirty(ui::Canvas& canvas);

  // Transmet tout l'écran. À réserver aux changements d'écran complets.
  void flushAll(const ui::Canvas& canvas);

  // À appeler au moins une fois par seconde : assure le basculement VCOM.
  // Sans cela le cristal liquide se dégrade de façon permanente.
  void maintain(std::uint32_t now_ms);

  int width() const { return 400; }
  int height() const { return 240; }

 private:
  void beginTransfer();
  void endTransfer();
  void writeControl(std::uint8_t value);
  void writeLine(const std::uint8_t* row, int line_index, int stride);
  std::uint8_t controlByte() const;

  Config config_;
  bool vcom_ = false;
  std::uint32_t last_vcom_ms_ = 0;
  bool enabled_ = false;
};

// Inverse l'ordre des bits d'un octet.
std::uint8_t reverseBits(std::uint8_t value);

}  // namespace vario::hal
