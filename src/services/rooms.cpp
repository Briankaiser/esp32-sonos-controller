#include "rooms.h"
#include "prefs.h"
#include "../app/sonos/state.h"

#include <Arduino.h>
#include <WebServer.h>

static WebServer* s_server = nullptr;

static String json_str(const String& s) {
    String out = s;
    out.replace("\\", "\\\\");
    out.replace("\"", "\\\"");
    return "\"" + out + "\"";
}

static void handle_get_rooms() {
    SonosState st    = sonos::state::snapshot();
    String home_uuid = prefs::home_room();

    String json = "{\n";
    json += "  \"home_room_uuid\": " + json_str(home_uuid) + ",\n";
    json += "  \"home_room_resolved\": " + String(st.home_room_idx >= 0 ? "true" : "false") + ",\n";
    json += "  \"speakers\": [\n";

    for (int i = 0; i < (int)st.speakers.size(); ++i) {
        const SpeakerInfo& sp = st.speakers[i];
        bool is_home = (i == st.home_room_idx);
        json += "    {\n";
        json += "      \"uuid\": "        + json_str(sp.uuid)          + ",\n";
        json += "      \"name\": "        + json_str(sp.friendly_name) + ",\n";
        json += "      \"ip\": "          + json_str(sp.ip)            + ",\n";
        json += "      \"port\": "        + String(sp.port)            + ",\n";
        json += "      \"is_home_room\": " + String(is_home ? "true" : "false") + "\n";
        json += "    }";
        if (i + 1 < (int)st.speakers.size()) json += ",";
        json += "\n";
    }

    json += "  ]\n}";
    s_server->send(200, "application/json", json);
}

static void handle_post_select() {
    String uuid = s_server->arg("uuid");
    if (uuid.isEmpty()) {
        s_server->send(400, "application/json", "{\"error\":\"uuid required\"}");
        return;
    }
    prefs::set_home_room(uuid);
    s_server->send(200, "application/json", "{\"ok\":true,\"uuid\":\"" + uuid + "\"}");
}

void rooms::register_handlers(WebServer& server) {
    s_server = &server;
    server.on("/rooms",        HTTP_GET,  handle_get_rooms);
    server.on("/rooms/select", HTTP_POST, handle_post_select);
}
