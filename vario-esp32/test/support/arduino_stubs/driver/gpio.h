#pragma once
// Talon minimal — voir README.md.
enum gpio_num_t { GPIO_NUM_0 = 0 };
enum gpio_mode_t { GPIO_MODE_OUTPUT = 2 };
inline int gpio_set_direction(gpio_num_t, gpio_mode_t) { return 0; }
