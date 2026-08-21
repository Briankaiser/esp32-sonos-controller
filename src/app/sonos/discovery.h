#pragma once
#include <cstdint>
#include <Arduino.h>

namespace sonos::discovery {
    constexpr uint32_t kDiscoveryIntervalMs = 600000;  // 10 min: speakers rarely change IP

    // When the provisioned home room is absent from a discovery cycle (e.g. a
    // battery Sonos Roam that was asleep), re-run discovery on a short cadence
    // instead of waiting the full kDiscoveryIntervalMs, so the controller
    // recovers within seconds of the speaker rejoining. Backs off after an
    // initial fast window so a speaker that stays off (charger overnight,
    // provisioning UUID mismatch) does not hammer SSDP/SOAP indefinitely.
    constexpr uint32_t kHomeRoomFastRetryMs   = 5000;    // retry every 5s...
    constexpr uint32_t kHomeRoomFastWindowMs  = 120000;  // ...for the first 2 min unresolved...
    constexpr uint32_t kHomeRoomSlowRetryMs   = 60000;   // ...then back off to every 60s

    void run_cycle();

    // Event-driven topology ingestion (called from event_task, eventing.cpp).
    // Applies a decoded ZoneGroupState XML payload to stored topology without
    // waiting for the next discovery cycle. Never removes speakers.
    void ingest_topology_xml(const String& decoded_zgs);

    // Fallback for oversized topology events: re-fetch topology from the home
    // room via SOAP and publish it (same parser, same publication path). Always
    // re-derives and publishes the home-room coordinator, even when the group
    // topology is unchanged (so a home-room switch updates the coordinator).
    void refresh_topology_via_soap();

    // Request an out-of-cycle topology refresh (e.g. after a home-room change).
    // request_topology_refresh() is called from the interface/HTTP task; sonos_task
    // (core 0) consumes it via take_topology_refresh() and runs the SOAP fetch, so
    // the poll/art/eventing coordinator for the new home room is corrected within
    // a poll interval instead of waiting up to kDiscoveryIntervalMs.
    void request_topology_refresh();
    bool take_topology_refresh();
}
