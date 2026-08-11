#pragma once
#include <cstdint>

namespace sonos::discovery {
    constexpr uint32_t kDiscoveryIntervalMs = 600000;  // 10 min: speakers rarely change IP
    void run_cycle();
}
