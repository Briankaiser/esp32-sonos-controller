#pragma once

#include <stdint.h>

// Keeps the controller on a good node of a mesh network. Joining always prefers
// the strongest node (see main.cpp); this monitor handles the long-running case:
// a link that stays weak while the device is idle triggers a scan, and if a node
// of the same network is clearly stronger the device re-joins through it.
namespace wifi_roam {

// `is_idle` is polled from the monitor task; it must be cheap and thread-safe.
// Call once, after the initial WiFi join succeeded.
void begin(bool (*is_idle)());

// Number of times the device moved to a stronger node since startup.
uint32_t reselect_count();

}  // namespace wifi_roam
