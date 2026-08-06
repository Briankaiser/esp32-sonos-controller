#pragma once
#include <cstdint>

namespace sonos::discovery {
    constexpr uint32_t kDiscoveryIntervalMs = 60000;
    void run_cycle();
}
