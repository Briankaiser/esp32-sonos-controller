#pragma once
#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct SpeakerInfo {
    String   uuid;
    String   friendly_name;
    String   ip;
    String   control_url_base;
    uint16_t port = 0;
};

struct ZoneGroup {
    String              coordinator_uuid;
    std::vector<String> member_uuids;
};

struct SonosState {
    std::vector<SpeakerInfo> speakers;
    std::vector<ZoneGroup>   groups;
    int                      home_room_idx = -1;
};

namespace sonos::state {
    void              init();
    void              update(const SonosState& next);  // sonos task only
    SonosState        snapshot();                       // any task; copies under mutex
    SemaphoreHandle_t mutex();
}
