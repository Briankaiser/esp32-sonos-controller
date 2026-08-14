#pragma once
#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct SpeakerInfo {
    String   uuid;
    String   friendly_name;  // room name (ZoneName from topology, HTML-decoded)
    String   model_name;     // device model, e.g. "Roam 2", "Amp" (from friendlyName, "Sonos " stripped)
    String   ip;
    String   control_url_base;
    uint16_t port = 0;
    // True for a bonded-set member that isn't independently playable — a home
    // theater surround/sub satellite or the non-primary unit of a stereo pair.
    // Set from the ZoneGroupTopology "Invisible" attribute; these still respond
    // to SSDP and get a full SpeakerInfo, but must not be offered as a home room
    // or a grouping target. false until the first successful topology fetch.
    bool     invisible = false;
};

struct ZoneGroup {
    String              coordinator_uuid;
    std::vector<String> member_uuids;
};

struct SonosState {
    std::vector<SpeakerInfo> speakers;
    std::vector<ZoneGroup>   groups;
    int                      home_room_idx          = -1;
    bool                     initial_discovery_done = false;  // set after first run_cycle completes
};

namespace sonos::state {
    void              init();
    void              update(const SonosState& next);  // sonos task only
    SonosState        snapshot();                       // any task; copies under mutex
    SemaphoreHandle_t mutex();
}
