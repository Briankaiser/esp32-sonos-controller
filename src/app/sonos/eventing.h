#pragma once
#include <Arduino.h>

// UPnP GENA event subscriptions — push notifications from Sonos speakers.
//
// Owns three subscriptions (AVTransport on the home room's group coordinator,
// RenderingControl on the home room, ZoneGroupTopology on the home room) and a
// raw TCP listener for inbound NOTIFY callbacks. Runs entirely on event_task
// (core 0); parsed state is ingested through the existing mutex-protected
// playback / state interfaces.
//
// healthy() gates sonos_task's steady-state polling: while true, polls are
// suspended except GetPositionInfo resyncs; while false, polling runs at the
// pre-eventing cadence so the device is never worse than polling-only.
namespace sonos::eventing {

    constexpr uint16_t kNotifyPort = 3400;  // inbound NOTIFY listener

    void init();           // allocate NOTIFY body buffer (PSRAM); call from setup()
    void start_task();     // spawn event_task on core 0; call after init()
    bool healthy();        // all three subscriptions established and renewing
    void request_reset();  // any task: drop all subscriptions on next event_task tick
}
