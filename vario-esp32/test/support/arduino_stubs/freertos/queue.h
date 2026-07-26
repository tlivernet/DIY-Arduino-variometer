#pragma once
#include "freertos/FreeRTOS.h"
using QueueHandle_t = void*;
inline QueueHandle_t xQueueCreate(std::uint32_t, std::uint32_t) {
  static int dummy = 0;
  return &dummy;
}
inline BaseType_t xQueueOverwrite(QueueHandle_t, const void*) { return pdTRUE; }
inline BaseType_t xQueuePeek(QueueHandle_t, void*, TickType_t) { return pdTRUE; }
