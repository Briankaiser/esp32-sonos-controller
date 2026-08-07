#pragma once
#include <cstdint>

namespace sonos::discovery {
    constexpr uint32_t kDiscoveryIntervalMs = 120000;  // 2 min: speakers rarely change IP
    void run_cycle();
}
