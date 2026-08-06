#include "discovery.h"
#include "state.h"
#include "soap.h"
#include "../../services/prefs.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <vector>

// ── XML / string helpers ─────────────────────────────────────────────────────

static String xml_field(const String& xml, const String& tag) {
    String open  = "<" + tag + ">";
    String close = "</" + tag + ">";
    int s = xml.indexOf(open);
    if (s < 0) return "";
    s += open.length();
    int e = xml.indexOf(close, s);
    if (e < 0) return "";
    return xml.substring(s, e);
}

static String xml_attr(const String& element, const String& attr) {
    String pat = attr + "=\"";
    int s = element.indexOf(pat);
    if (s < 0) return "";
    s += pat.length();
    int e = element.indexOf('"', s);
    if (e < 0) return "";
    return element.substring(s, e);
}

static String html_decode(const String& s) {
    String r = s;
    r.replace("&lt;",   "<");
    r.replace("&gt;",   ">");
    r.replace("&quot;", "\"");
    r.replace("&amp;",  "&");  // must be last
    return r;
}

// Parse "http://host:port/path" → fills ip, port, path_out.
// Returns false if the URL cannot be parsed.
static bool parse_url(const String& url,
                      String& ip_out, uint16_t& port_out, String& path_out) {
    int proto_end = url.indexOf("://");
    if (proto_end < 0) return false;
    String rest = url.substring(proto_end + 3);
    int slash = rest.indexOf('/');
    String host_port = (slash >= 0) ? rest.substring(0, slash) : rest;
    path_out         = (slash >= 0) ? rest.substring(slash)    : "/";
    int colon = host_port.indexOf(':');
    if (colon >= 0) {
        ip_out   = host_port.substring(0, colon);
        port_out = (uint16_t)host_port.substring(colon + 1).toInt();
    } else {
        ip_out   = host_port;
        port_out = 80;
    }
    return !ip_out.isEmpty();
}

// ── M-SEARCH ─────────────────────────────────────────────────────────────────

static void ssdp_search(std::vector<String>& locations) {
    WiFiUDP udp;
    if (!udp.begin(1982)) {
        Serial.println("[Sonos] M-SEARCH: UDP bind failed");
        return;
    }

    static const char kMSearch[] =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n"
        "\r\n";

    IPAddress mcast(239, 255, 255, 250);

    auto send_msearch = [&]() {
        udp.beginPacket(mcast, 1900);
        udp.write((const uint8_t*)kMSearch, sizeof(kMSearch) - 1);
        udp.endPacket();
    };

    // Send immediately; resend at 1.5 s and 3 s to catch speakers that miss
    // the first multicast (multicast delivery is not guaranteed).
    send_msearch();
    uint32_t deadline   = millis() + 5000;
    uint32_t next_send  = millis() + 1500;

    while (millis() < deadline) {
        if (millis() >= next_send) {
            send_msearch();
            next_send += 1500;
        }

        // Drain ALL packets that arrived since last iteration — a single
        // parsePacket() call per loop tick drops concurrent responses.
        while (udp.parsePacket() > 0) {
            char buf[1024] = {};
            int n = udp.read(buf, (int)sizeof(buf) - 1);
            if (n <= 0) break;
            String response(buf);
            int pos = response.indexOf("LOCATION:");
            if (pos < 0) pos = response.indexOf("Location:");
            if (pos >= 0) {
                int start = pos + 9;
                while (start < (int)response.length() && response[start] == ' ') start++;
                int end = response.indexOf('\n', start);
                if (end < 0) end = response.length();
                String loc = response.substring(start, end);
                loc.trim();
                if (!loc.isEmpty()) {
                    bool dup = false;
                    for (const auto& l : locations) { if (l == loc) { dup = true; break; } }
                    if (!dup) locations.push_back(loc);
                }
            }
        }
        delay(1);
    }
    udp.stop();
}

// ── Device description fetch and parse ───────────────────────────────────────

static bool fetch_speaker_info(const String& location, SpeakerInfo& out) {
    String ip;
    uint16_t port = 0;
    String path;
    if (!parse_url(location, ip, port, path)) {
        Serial.printf("[Sonos] bad LOCATION URL: %s\n", location.c_str());
        return false;
    }

    WiFiClient client;
    client.setTimeout(5000);
    if (!client.connect(ip.c_str(), port, 5000)) {
        Serial.printf("[Sonos] device desc connect failed: %s:%u\n", ip.c_str(), (unsigned)port);
        return false;
    }
    client.printf("GET %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\n\r\n",
                  path.c_str(), ip.c_str(), (unsigned)port);

    // Read status line and headers; detect chunked transfer encoding
    uint32_t deadline = millis() + 8000;
    while (!client.available() && millis() < deadline) delay(10);
    client.readStringUntil('\n');  // status line

    bool is_chunked = false;
    while (client.connected() && millis() < deadline) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) break;  // end of headers
        if (line.indexOf("chunked") >= 0) is_chunked = true;
    }

    // Read and decode body
    String xml = "";
    if (is_chunked) {
        while ((client.connected() || client.available()) && millis() < deadline) {
            String size_line = client.readStringUntil('\n');
            size_line.trim();
            if (size_line.isEmpty()) continue;
            uint32_t chunk_size = strtoul(size_line.c_str(), nullptr, 16);
            if (chunk_size == 0) break;
            for (uint32_t i = 0; i < chunk_size && millis() < deadline; ) {
                if (client.available()) { xml += (char)client.read(); i++; }
                else delay(1);
            }
            client.readStringUntil('\n');  // trailing CRLF after chunk data
        }
    } else {
        while ((client.connected() || client.available()) && millis() < deadline) {
            while (client.available()) xml += (char)client.read();
            delay(1);
        }
    }
    client.stop();

    if (xml.isEmpty()) {
        Serial.printf("[Sonos] empty device desc from %s\n", location.c_str());
        return false;
    }

    String udn = xml_field(xml, "UDN");
    if (udn.isEmpty()) {
        Serial.printf("[Sonos] no UDN in device desc from %s\n", location.c_str());
        return false;
    }
    // Strip "uuid:" prefix
    if (udn.startsWith("uuid:")) udn = udn.substring(5);
    if (udn.isEmpty()) {
        Serial.printf("[Sonos] empty UUID after strip from %s\n", location.c_str());
        return false;
    }

    String fname = xml_field(xml, "friendlyName");

    // Prefer URLBase for IP/port; fall back to LOCATION URL
    String base = xml_field(xml, "URLBase");
    if (!base.isEmpty()) {
        String base_ip;
        uint16_t base_port = 0;
        String base_path;
        if (parse_url(base, base_ip, base_port, base_path)) {
            ip   = base_ip;
            port = base_port;
        }
    }

    out.uuid             = udn;
    out.friendly_name    = fname;
    out.ip               = ip;
    out.port             = port;
    out.control_url_base = "";  // Sonos uses well-known service paths
    return true;
}

// ── Zone-group topology ───────────────────────────────────────────────────────

static std::vector<ZoneGroup> fetch_topology(const SpeakerInfo& home,
                                              std::vector<SpeakerInfo>& speakers) {
    String path = home.control_url_base + "/ZoneGroupTopology/Control";
    String resp = sonos::soap::call(
        home.ip, home.port, path,
        "urn:schemas-upnp-org:service:ZoneGroupTopology:1",
        "GetZoneGroupState", "");

    if (resp.isEmpty()) return {};

    // ZoneGroupState content is HTML-entity-encoded inside <ZoneGroupState>…</ZoneGroupState>
    String raw = xml_field(resp, "ZoneGroupState");
    if (raw.isEmpty()) {
        Serial.println("[Sonos] topology: no ZoneGroupState element in response");
        return {};
    }
    String decoded = html_decode(raw);

    std::vector<ZoneGroup> groups;
    int pos = 0;
    while (true) {
        int s = decoded.indexOf("<ZoneGroup ", pos);
        if (s < 0) break;
        int e = decoded.indexOf("</ZoneGroup>", s);
        if (e < 0) break;
        String grp_xml = decoded.substring(s, e + 12);

        ZoneGroup grp;
        grp.coordinator_uuid = xml_attr(grp_xml, "Coordinator");

        int m = 0;
        while (true) {
            int ms = grp_xml.indexOf("<ZoneMember ", m);
            if (ms < 0) break;
            int me = grp_xml.indexOf('>', ms);
            if (me < 0) break;
            String mem_xml   = grp_xml.substring(ms, me + 1);
            String muuid     = xml_attr(mem_xml, "UUID");
            String zone_name = xml_attr(mem_xml, "ZoneName");
            if (!muuid.isEmpty()) {
                grp.member_uuids.push_back(muuid);
                if (!zone_name.isEmpty()) {
                    for (auto& sp : speakers) {
                        if (sp.uuid == muuid) { sp.friendly_name = zone_name; break; }
                    }
                }
            }
            m = me + 1;
        }

        if (!grp.coordinator_uuid.isEmpty()) groups.push_back(grp);
        pos = e + 12;
    }

    Serial.printf("[Sonos] topology updated: %u group(s)\n", (unsigned)groups.size());
    return groups;
}

// ── Public entry point ────────────────────────────────────────────────────────

void sonos::discovery::run_cycle() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Sonos] WiFi not connected — skipping discovery");
        return;
    }

    // ── 3.2: M-SEARCH ──────────────────────────────────────────────────────
    std::vector<String> locations;
    ssdp_search(locations);

    // ── 3.3: Fetch device descriptions ─────────────────────────────────────
    std::vector<SpeakerInfo> speakers;
    for (const auto& loc : locations) {
        SpeakerInfo info;
        if (fetch_speaker_info(loc, info)) {
            speakers.push_back(info);
        }
    }

    // ── 3.4: Log count and resolve home room ────────────────────────────────
    if (speakers.empty()) {
        Serial.println("[Sonos] no speakers found");
    } else {
        Serial.printf("[Sonos] found %u speaker(s)\n", (unsigned)speakers.size());
    }

    String home_uuid = prefs::home_room();
    int home_room_idx = -1;

    if (home_uuid.isEmpty()) {
        Serial.println("[Sonos] home room not yet configured");
    } else {
        for (int i = 0; i < (int)speakers.size(); i++) {
            if (speakers[i].uuid == home_uuid) {
                home_room_idx = i;
                Serial.printf("[Sonos] home room resolved: %s\n",
                              speakers[i].friendly_name.c_str());
                break;
            }
        }
        if (home_room_idx < 0) {
            Serial.printf("[Sonos] home room UUID %s not found\n", home_uuid.c_str());
        }
    }

    // ── 4.1 & 4.2: Zone-group topology ─────────────────────────────────────
    SonosState next;
    next.speakers      = speakers;
    next.home_room_idx = home_room_idx;

    if (home_room_idx >= 0) {
        // Snapshot current groups to restore if fetch fails
        std::vector<ZoneGroup> prev_groups = sonos::state::snapshot().groups;
        std::vector<ZoneGroup> groups = fetch_topology(speakers[home_room_idx], speakers);
        next.groups = groups.empty() ? prev_groups : groups;
    } else {
        next.groups = sonos::state::snapshot().groups;
    }

    sonos::state::update(next);
}
