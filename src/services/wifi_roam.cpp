#include "wifi_roam.h"

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "prefs.h"

// Thresholds are overridable for bench tests (-D...); defaults are the spec values.
#ifndef OPENSONOS_ROAM_WEAK_DBM
#define OPENSONOS_ROAM_WEAK_DBM -75
#endif
#ifndef OPENSONOS_ROAM_MIN_GAIN_DB
#define OPENSONOS_ROAM_MIN_GAIN_DB 8
#endif
#ifndef OPENSONOS_ROAM_CHECK_MS
#define OPENSONOS_ROAM_CHECK_MS 60000u
#endif
#ifndef OPENSONOS_ROAM_DWELL_CHECKS
#define OPENSONOS_ROAM_DWELL_CHECKS 10u   // x CHECK_MS = 10 minutes
#endif
#ifndef OPENSONOS_ROAM_IGNORE_IDLE
#define OPENSONOS_ROAM_IGNORE_IDLE 0      // bench tests only: skip the idle gate
#endif
#ifndef OPENSONOS_ROAM_COOLDOWN_MS
#define OPENSONOS_ROAM_COOLDOWN_MS (30u * 60u * 1000u)
#endif

namespace {

constexpr uint32_t kMinFreeInternal = 40 * 1024;  // scan + rejoin need headroom
constexpr uint32_t kRejoinTimeoutMs = 20000;

bool (*s_is_idle)() = nullptr;
std::atomic<uint32_t> s_reselects{0};

// Scan for same-network nodes other than the current one; return the strongest
// RSSI found (INT_MIN when none).
int strongest_other_node_dbm(const String& ssid, const String& current_bssid) {
    int best = INT32_MIN;
    int n = WiFi.scanNetworks(false /*blocking*/, false, false, 300, 0, ssid.c_str());
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) != ssid) continue;
        if (WiFi.BSSIDstr(i) == current_bssid) continue;
        if (WiFi.RSSI(i) > best) best = WiFi.RSSI(i);
    }
    WiFi.scanDelete();
    return best;
}

void reselect(int current_dbm) {
    String ssid = prefs::wifi_ssid();
    String cur  = WiFi.BSSIDstr();
    int best = strongest_other_node_dbm(ssid, cur);
    if (best == INT32_MIN || best < current_dbm + OPENSONOS_ROAM_MIN_GAIN_DB) {
        Serial.printf("[WiFiRoam] staying on %s (%d dBm): best other node %s\n", cur.c_str(),
                      current_dbm, best == INT32_MIN ? "none" : String(best).c_str());
        return;
    }
    Serial.printf("[WiFiRoam] %s %d dBm -> stronger node at %d dBm; re-joining\n", cur.c_str(),
                  current_dbm, best);
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(500));
    // Same scan-all-channels / sort-by-signal settings as the boot join, so this
    // picks the strongest node. No BSSID is pinned: if that node later vanishes
    // the driver is free to join another.
    WiFi.begin(ssid.c_str(), prefs::wifi_pass().c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < kRejoinTimeoutMs) {
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    if (WiFi.status() == WL_CONNECTED) {
        s_reselects++;
        Serial.printf("[WiFiRoam] now on %s at %d dBm\n", WiFi.BSSIDstr().c_str(), (int)WiFi.RSSI());
    } else {
        // Auto-reconnect and discovery's reconnect backstop take it from here.
        Serial.println("[WiFiRoam] re-join did not complete; leaving recovery to auto-reconnect");
    }
}

void monitor(void*) {
    uint32_t weak_checks = 0;
    uint32_t last_attempt_ms = 0;
    bool     attempted = false;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(OPENSONOS_ROAM_CHECK_MS));
        if (WiFi.status() != WL_CONNECTED) { weak_checks = 0; continue; }
        int rssi = WiFi.RSSI();
        weak_checks = (rssi <= OPENSONOS_ROAM_WEAK_DBM) ? weak_checks + 1 : 0;
        if (weak_checks < OPENSONOS_ROAM_DWELL_CHECKS) continue;
        if (!OPENSONOS_ROAM_IGNORE_IDLE && s_is_idle && !s_is_idle()) continue;  // never while in use
        if (attempted && millis() - last_attempt_ms < OPENSONOS_ROAM_COOLDOWN_MS) continue;
        if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < kMinFreeInternal) continue;
        attempted = true;
        last_attempt_ms = millis();
        reselect(rssi);
    }
}

}  // namespace

void wifi_roam::begin(bool (*is_idle)()) {
    s_is_idle = is_idle;
    xTaskCreatePinnedToCore(monitor, "WifiRoam", 6144, nullptr, 1, nullptr, 0);
}

uint32_t wifi_roam::reselect_count() { return s_reselects; }
