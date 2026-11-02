#include "status.h"
#include "prefs.h"
#include "fw_version.h"
#include "diag.h"
#include "tls_mem.h"
#include "../app/sonos/state.h"
#include "../app/sonos/eventing.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <WebServer.h>

static WebServer* s_server = nullptr;

static String json_str(const String& s) {
    String out = s;
    out.replace("\\", "\\\\");
    out.replace("\"", "\\\"");
    return "\"" + out + "\"";
}

// GET /status — built entirely from cached state. No SSDP/SOAP/network I/O:
// a single state snapshot plus already-resident WiFi/eventing/prefs reads.
static void handle_get_status() {
    SonosState  st        = sonos::state::snapshot();
    bool        connected = (WiFi.status() == WL_CONNECTED);
    String      ip        = connected ? WiFi.localIP().toString() : String("");
    String      ssid      = connected ? WiFi.SSID() : String("");
    String      home_uuid = prefs::home_room();
    String      home_name = (st.home_room_idx >= 0)
                                ? st.speakers[st.home_room_idx].friendly_name
                                : String("");
    const char* source    = sonos::eventing::healthy() ? "gena" : "polling";

    String json = "{\n";
    json += "  \"device\": \"opensonos-controller\",\n";
    json += "  \"firmware_version\": " + json_str(fw::version()) + ",\n";
    json += "  \"board\": " + json_str(fw::board()) + ",\n";
    char dev_id[13];
    snprintf(dev_id, sizeof(dev_id), "%012llx", (unsigned long long)ESP.getEfuseMac());
    json += "  \"device_id\": " + json_str(String(dev_id)) + ",\n";
    json += "  \"wifi_connected\": " + String(connected ? "true" : "false") + ",\n";
    json += "  \"ip\": " + json_str(ip) + ",\n";
    json += "  \"wifi_ssid\": " + json_str(ssid) + ",\n";
    json += "  \"speaker_count\": " + String((int)st.speakers.size()) + ",\n";
    json += "  \"home_room_name\": " + json_str(home_name) + ",\n";
    json += "  \"home_room_uuid\": " + json_str(home_uuid) + ",\n";
    json += "  \"state_source\": " + json_str(source) + ",\n";
    json += "  \"uptime_s\": " + String((unsigned long)(millis() / 1000)) + ",\n";
    json += "  \"heap_internal_free\": " + String((unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) + ",\n";
    json += "  \"heap_internal_min_free\": " + String((unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)) + ",\n";
    json += "  \"heap_internal_largest\": " + String((unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)) + ",\n";
    json += "  \"psram_free\": " + String((unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) + ",\n";
    json += "  \"psram_largest\": " + String((unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)) + ",\n";
    unsigned tm[5];
    tls_mem::stats(tm);
    json += "  \"tls_mem\": {\"psram_allocs\": " + String(tm[0]) + ", \"psram_bytes\": " + String(tm[1]) +
            ", \"internal_allocs\": " + String(tm[2]) + ", \"internal_bytes\": " + String(tm[3]) +
            ", \"fallbacks\": " + String(tm[4]) + "},\n";
    json += diag::json_fragment() + "\n";
    json += "}";

    s_server->send(200, "application/json", json);
}

void status::register_handler(WebServer& server) {
    s_server = &server;
    server.on("/status", HTTP_GET, handle_get_status);
}
