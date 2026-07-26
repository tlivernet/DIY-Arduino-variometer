#pragma once
#include <cstdint>
enum ledc_mode_t { LEDC_LOW_SPEED_MODE = 0 };
enum ledc_timer_bit_t { LEDC_TIMER_10_BIT = 10 };
enum ledc_timer_t { LEDC_TIMER_0 = 0 };
enum ledc_channel_t { LEDC_CHANNEL_0 = 0 };
enum ledc_intr_type_t { LEDC_INTR_DISABLE = 0 };
enum ledc_clk_cfg_t { LEDC_AUTO_CLK = 0 };
enum gpio_num_t { GPIO_NUM_0 = 0 };
enum gpio_mode_t { GPIO_MODE_OUTPUT = 2 };

struct ledc_timer_config_t {
  ledc_mode_t speed_mode;
  ledc_timer_bit_t duty_resolution;
  ledc_timer_t timer_num;
  std::uint32_t freq_hz;
  ledc_clk_cfg_t clk_cfg;
};
struct ledc_channel_config_t {
  int gpio_num;
  ledc_mode_t speed_mode;
  ledc_channel_t channel;
  ledc_intr_type_t intr_type;
  ledc_timer_t timer_sel;
  std::uint32_t duty;
  int hpoint;
};
inline int ledc_timer_config(const ledc_timer_config_t*) { return 0; }
inline int ledc_channel_config(const ledc_channel_config_t*) { return 0; }
inline int ledc_set_duty(ledc_mode_t, ledc_channel_t, std::uint32_t) { return 0; }
inline int ledc_update_duty(ledc_mode_t, ledc_channel_t) { return 0; }
inline int ledc_set_freq(ledc_mode_t, ledc_timer_t, std::uint32_t) { return 0; }
inline int gpio_set_direction(gpio_num_t, gpio_mode_t) { return 0; }
