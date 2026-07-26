#include "hal/audio_ledc.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>

namespace vario::hal {
namespace {

// Rapports cycliques des quatre niveaux de volume, en fraction de la pleine
// échelle. 50 % donne la puissance maximale sur une charge capacitive ; au-delà
// le signal redevient continu et le son diminue.
//
// La réponse d'un piezo au rapport cyclique n'est pas linéaire : ces valeurs
// sont un point de départ, à ajuster à l'oreille avec un sonomètre.
constexpr float kVolumeDuty[5] = {0.0f, 0.12f, 0.25f, 0.37f, 0.50f};

constexpr ledc_mode_t kSpeedMode = LEDC_LOW_SPEED_MODE;

}  // namespace

void LedcAudio::begin() {
  ledc_timer_config_t timer = {};
  timer.speed_mode = kSpeedMode;
  timer.duty_resolution = static_cast<ledc_timer_bit_t>(config_.resolution_bits);
  timer.timer_num = static_cast<ledc_timer_t>(config_.timer);
  timer.freq_hz = 2000;  // valeur de départ, remplacée à chaque note
  timer.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&timer);

  ledc_channel_config_t channel = {};
  channel.gpio_num = config_.pin_a;
  channel.speed_mode = kSpeedMode;
  channel.channel = static_cast<ledc_channel_t>(config_.channel);
  channel.intr_type = LEDC_INTR_DISABLE;
  channel.timer_sel = static_cast<ledc_timer_t>(config_.timer);
  channel.duty = 0;
  channel.hpoint = 0;
  ledc_channel_config(&channel);

  // La seconde broche reçoit LE MÊME signal, inversé par la matrice GPIO.
  // C'est ce qui garantit un déphasage de 180° exact sans aucun entretien.
  gpio_set_direction(static_cast<gpio_num_t>(config_.pin_b), GPIO_MODE_OUTPUT);
  esp_rom_gpio_connect_out_signal(static_cast<std::uint32_t>(config_.pin_b),
                                  LEDC_LS_SIG_OUT0_IDX + config_.channel,
                                  /*out_inv=*/true, /*oen_inv=*/false);

  silence();
}

void LedcAudio::applyDuty(std::uint32_t duty) {
  ledc_set_duty(kSpeedMode, static_cast<ledc_channel_t>(config_.channel), duty);
  ledc_update_duty(kSpeedMode, static_cast<ledc_channel_t>(config_.channel));
}

void LedcAudio::tone(std::uint16_t frequency_hz, int volume) {
  if (volume <= 0 || frequency_hz == 0) {
    silence();
    return;
  }
  if (volume > 4) volume = 4;

  if (frequency_hz != current_frequency_) {
    ledc_set_freq(kSpeedMode, static_cast<ledc_timer_t>(config_.timer), frequency_hz);
    current_frequency_ = frequency_hz;
    // Changer la fréquence reconfigure le diviseur du minuteur, ce qui remet le
    // rapport cyclique à zéro sur certaines versions de l'IDF. On le réécrit
    // donc systématiquement juste après : c'est un piège classique.
  }

  const std::uint32_t full = (1u << config_.resolution_bits) - 1u;
  applyDuty(static_cast<std::uint32_t>(static_cast<float>(full) * kVolumeDuty[volume]));
  sounding_ = true;
}

void LedcAudio::silence() {
  // Rapport cyclique nul : les deux broches se retrouvent à des niveaux
  // constants et opposés, donc une tension continue aux bornes du piezo. Un
  // piezo étant capacitif il ne consomme rien, mais on coupe malgré tout la
  // sortie inversée pour ne rien laisser sous tension.
  applyDuty(0);
  sounding_ = false;
}

}  // namespace vario::hal
