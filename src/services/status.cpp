#include "status.h"
#include "prefs.h"
#include "fw_version.h"
#include "../app/sonos/state.h"
#include "../app/sonos/eventing.h"

#include <Arduino.h>
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
    json += "  \"wifi_connected\": " + String(connected ? "true" : "false") + ",\n";
    json += "  \"ip\": " + json_str(ip) + ",\n";
    json += "  \"wifi_ssid\": " + json_str(ssid) + ",\n";
    json += "  \"speaker_count\": " + String((int)st.speakers.size()) + ",\n";
    json += "  \"home_room_name\": " + json_str(home_name) + ",\n";
    json += "  \"home_room_uuid\": " + json_str(home_uuid) + ",\n";
    json += "  \"state_source\": " + json_str(source) + "\n";
    json += "}";

    s_server->send(200, "application/json", json);
}

void status::register_handler(WebServer& server) {
    s_server = &server;
    server.on("/status", HTTP_GET, handle_get_status);
}
