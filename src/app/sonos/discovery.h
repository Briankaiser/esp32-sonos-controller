#pragma once
#include <cstdint>
#include <Arduino.h>

namespace sonos::discovery {
    constexpr uint32_t kDiscoveryIntervalMs = 600000;  // 10 min: speakers rarely change IP
    void run_cycle();

    // Event-driven topology ingestion (called from event_task, eventing.cpp).
    // Applies a decoded ZoneGroupState XML payload to stored topology without
    // waiting for the next discovery cycle. Never removes speakers.
    void ingest_topology_xml(const String& decoded_zgs);

    // Fallback for oversized topology events: re-fetch topology from the home
    // room via SOAP and publish it (same parser, same publication path).
    void refresh_topology_via_soap();
}
