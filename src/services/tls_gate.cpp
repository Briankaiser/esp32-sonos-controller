#include "tls_gate.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t mutex() {
    static SemaphoreHandle_t m = xSemaphoreCreateMutex();  // thread-safe local static
    return m;
}

bool tls_gate::acquire(uint32_t timeout_ms) {
    return xSemaphoreTake(mutex(), pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void tls_gate::release() {
    xSemaphoreGive(mutex());
}
