#pragma once
#include "freertos/FreeRTOS.h"
using TaskHandle_t = void*;
inline TickType_t xTaskGetTickCount() { return 0; }
inline void vTaskDelayUntil(TickType_t*, TickType_t) {}
inline BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char*, std::uint32_t,
                                         void*, int, TaskHandle_t*, int) {
  return pdTRUE;
}
