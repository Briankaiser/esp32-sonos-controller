#include "state.h"

namespace {
    static SemaphoreHandle_t s_mutex = nullptr;
    static SonosState        s_state = {};
}

void sonos::state::init() {
    s_mutex = xSemaphoreCreateMutex();
}

void sonos::state::update(const SonosState& next) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = next;
    xSemaphoreGive(s_mutex);
}

SonosState sonos::state::snapshot() {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    SonosState copy = s_state;
    xSemaphoreGive(s_mutex);
    return copy;
}

SemaphoreHandle_t sonos::state::mutex() {
    return s_mutex;
}
