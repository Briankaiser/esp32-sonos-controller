#include "discovery.h"
#include "state.h"
#include "soap.h"
#include "playback.h"
#include "xml_util.h"
#include "../../services/prefs.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <vector>

// XML/string helpers shared with playback and eventing (see xml_util.h)
using sonos::xml::html_decode;
static inline String xml_field(const String& xml, const String& tag) {
    return sonos::xml::field(xml, tag);
}
static inline String xml_attr(const String& element, const String& attr) {
    return sonos::xml::attr(element, attr);
}

// Wrap-safe deadline check — millis() < deadline inverts at the 32-bit rollover
// (~49.7 days uptime), which would make every network read return instantly
// empty for the ~1 minute it takes millis() to cross back past `deadline`.
static inline bool time_before(uint32_t deadline) {
    return (int32_t)(millis() - deadline) < 0;
}

// Read one '\n'-terminated line, yielding while waiting for data. Replaces
// Stream::readStringUntil, which busy-spins in Stream::timedRead with no task
// yield and can starve the IDLE task (→ watchdog) — see soap.cpp for the same
// fix applied to SOAP calls. This function runs once per discovered speaker on
// every discovery cycle, so a single slow/hanging device description response
// here was the most likely real-world source of an unattended reboot.
static String read_line(WiFiClient& client, uint32_t deadline) {
    String line;
    while (time_before(deadline)) {
        if (client.available()) {
            char c = (char)client.read();
            if (c == '\n') break;
            if (c != '\r') line += c;
        } else if (client.connected()) {
            delay(1);
        } else {
            break;
        }
    }
    return line;
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

    while (time_before(deadline)) {
        if (!time_before(next_send)) {
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
    read_line(client, deadline);  // status line

    bool is_chunked = false;
    while (client.connected() && time_before(deadline)) {
        String line = read_line(client, deadline);
        line.trim();
        if (line.isEmpty()) break;  // end of headers
        if (line.indexOf("chunked") >= 0) is_chunked = true;
    }

    // Read and decode body. Bulk readBytes (instead of appending one char at a
    // time) avoids the repeated small String reallocations that fragment the
    // heap over long uptime — the device description can be a few KB.
    String xml = "";
    char   buf[512];
    if (is_chunked) {
        while ((client.connected() || client.available()) && time_before(deadline)) {
            String size_line = read_line(client, deadline);
            size_line.trim();
            if (size_line.isEmpty()) continue;
            uint32_t chunk_size = strtoul(size_line.c_str(), nullptr, 16);
            if (chunk_size == 0) break;
            for (uint32_t i = 0; i < chunk_size && time_before(deadline); ) {
                int av = client.available();
                if (av > 0) {
                    size_t want = chunk_size - i;
                    if (want > sizeof(buf)) want = sizeof(buf);
                    if ((size_t)av < want) want = (size_t)av;
                    size_t got = client.readBytes(buf, want);
                    xml.concat(buf, got);
                    i += (uint32_t)got;
                } else if (client.connected()) {
                    delay(1);
                } else {
                    break;
                }
            }
            read_line(client, deadline);  // trailing CRLF after chunk data
        }
    } else {
        while ((client.connected() || client.available()) && time_before(deadline)) {
            int av = client.available();
            if (av > 0) {
                size_t want = (size_t)av > sizeof(buf) ? sizeof(buf) : (size_t)av;
                size_t got = client.readBytes(buf, want);
                xml.concat(buf, got);
            } else {
                delay(1);
            }
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

    String fname = html_decode(xml_field(xml, "friendlyName"));

    // Extract model from "Room - Model - RINCON_UUID" format; strip "Sonos " prefix
    String model;
    int sep = fname.indexOf(" - ");
    if (sep >= 0) {
        model = fname.substring(sep + 3);
        int sep2 = model.indexOf(" - ");  // trim trailing " - RINCON_UUID" if present
        if (sep2 >= 0) model = model.substring(0, sep2);
        if (model.startsWith("Sonos ")) model = model.substring(6);
    }

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
    out.model_name       = model;
    out.ip               = ip;
    out.port             = port;
    out.control_url_base = "";  // Sonos uses well-known service paths
    return true;
}

// ── Zone-group topology ───────────────────────────────────────────────────────

// Parses decoded ZoneGroupState XML into groups, updating speakers' zone names
// and invisible flags in place. Shared by the SOAP fetch path (run_cycle) and
// the GENA topology-event path (eventing.cpp via ingest_topology_xml).
static std::vector<ZoneGroup> parse_zone_group_state(const String& decoded,
                                                     std::vector<SpeakerInfo>& speakers) {
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
            int ms = grp_xml.indexOf("<ZoneGroupMember", m);
            if (ms < 0) break;
            int tag_end = grp_xml.indexOf('>', ms);
            if (tag_end < 0) break;
            String mem_tag = grp_xml.substring(ms, tag_end + 1);

            // A member with bonded satellites (home theater surrounds/sub) is not
            // self-closed — it has <Satellite> children up to </ZoneGroupMember>.
            // Find that span so satellite tags aren't mistaken for sibling members.
            bool   self_closing = tag_end > 0 && grp_xml[tag_end - 1] == '/';
            int    resume_at    = tag_end + 1;
            String children_xml;
            if (!self_closing) {
                int close = grp_xml.indexOf("</ZoneGroupMember>", tag_end + 1);
                if (close >= 0) {
                    children_xml = grp_xml.substring(tag_end + 1, close);
                    resume_at    = close + (int)strlen("</ZoneGroupMember>");
                }
            }

            auto apply_member = [&](const String& tag) {
                String uuid      = xml_attr(tag, "UUID");
                String zone_name = html_decode(xml_attr(tag, "ZoneName"));
                bool   invisible = xml_attr(tag, "Invisible") == "1";
                if (uuid.isEmpty()) return;
                grp.member_uuids.push_back(uuid);
                for (auto& sp : speakers) {
                    if (sp.uuid != uuid) continue;
                    if (!zone_name.isEmpty()) sp.friendly_name = zone_name;
                    if (invisible) sp.invisible = true;
                    break;
                }
            };

            apply_member(mem_tag);

            // Satellites (surround/sub for home theater bonds) are independently
            // discoverable ZonePlayers with their own SpeakerInfo, but Sonos marks
            // them Invisible="1" here to signal they aren't directly playable —
            // the same signal used by e.g. python-soco's is_visible check.
            int sp_pos = 0;
            while (!children_xml.isEmpty()) {
                int ss = children_xml.indexOf("<Satellite", sp_pos);
                if (ss < 0) break;
                int se = children_xml.indexOf('>', ss);
                if (se < 0) break;
                apply_member(children_xml.substring(ss, se + 1));
                sp_pos = se + 1;
            }

            m = resume_at;
        }

        if (!grp.coordinator_uuid.isEmpty()) groups.push_back(grp);
        pos = e + 12;
    }

    Serial.printf("[Sonos] topology updated: %u group(s)\n", (unsigned)groups.size());
    return groups;
}

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
    return parse_zone_group_state(html_decode(raw), speakers);
}

// Derives the home room's group-coordinator UUID from `groups` and publishes it
// to playback state. Empty = standalone; own uuid = coordinator of a multi-
// member group; other uuid = member of that coordinator's group.
static void publish_group_uuid(const std::vector<ZoneGroup>& groups, const String& hr_uuid) {
    String g_uuid;
    for (const auto& grp : groups) {
        bool is_coord  = (grp.coordinator_uuid == hr_uuid);
        bool is_member = false;
        for (const auto& m : grp.member_uuids) {
            if (m == hr_uuid) { is_member = true; break; }
        }
        if (is_member || is_coord) {
            if (is_coord && grp.member_uuids.size() > 1) g_uuid = hr_uuid;
            else if (!is_coord) g_uuid = grp.coordinator_uuid;
            break;
        }
    }
    sonos::playback::update_group_uuid(g_uuid);
}

// ── Event-driven topology ingestion (called from event_task) ─────────────────
//
// Both functions read-modify-write shared state. A discovery run_cycle
// publishing between the snapshot and update here would be overwritten with
// this (equally fresh) topology; the window is milliseconds and the next
// event or cycle converges — accepted rather than holding the state mutex
// across parsing.

void sonos::discovery::ingest_topology_xml(const String& decoded_zgs) {
    SonosState st = sonos::state::snapshot();
    if (st.speakers.empty()) return;  // no speaker list yet to correlate against
    std::vector<ZoneGroup> groups = parse_zone_group_state(decoded_zgs, st.speakers);
    if (groups.empty()) return;       // malformed/empty event — keep current topology
    st.groups = groups;
    // st.speakers / home_room_idx carried through unchanged: an event never
    // removes speakers merely absent from its payload.
    sonos::state::update(st);
    if (st.home_room_idx >= 0 && st.home_room_idx < (int)st.speakers.size()) {
        publish_group_uuid(groups, st.speakers[st.home_room_idx].uuid);
    }
}

void sonos::discovery::refresh_topology_via_soap() {
    SonosState st = sonos::state::snapshot();
    if (st.home_room_idx < 0 || st.home_room_idx >= (int)st.speakers.size()) return;
    std::vector<ZoneGroup> groups = fetch_topology(st.speakers[st.home_room_idx], st.speakers);
    if (groups.empty()) return;
    st.groups = groups;
    sonos::state::update(st);
    publish_group_uuid(groups, st.speakers[st.home_room_idx].uuid);
}

// ── Public entry point ────────────────────────────────────────────────────────

void sonos::discovery::run_cycle() {
    if (WiFi.status() != WL_CONNECTED) {
        // Backstop for disconnect reasons the IDF's own auto-reconnect (enabled in
        // main.cpp after the initial connect) doesn't recover from on its own.
        // run_cycle() is only called once per kDiscoveryIntervalMs by sonos_task,
        // which already throttles this — no extra rate limiting needed.
        Serial.println("[Sonos] WiFi not connected — attempting reconnect");
        WiFi.reconnect();
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

    // Device description's own <friendlyName> is not the room name — it's an
    // "IP - Sonos Model - RINCON_UUID" string; fetch_topology() below corrects
    // friendly_name to the real room name (and sets invisible) from zone-group
    // topology. Seed both from the last known-good state first so that a single
    // failed topology fetch (SOAP timeout, transient network hiccup) leaves the
    // UI showing the last real name instead of reverting to the raw ugly one —
    // now more consequential since a miss isn't retried for kDiscoveryIntervalMs.
    {
        SonosState prev = sonos::state::snapshot();
        for (auto& sp : speakers) {
            for (const auto& old : prev.speakers) {
                if (old.uuid != sp.uuid) continue;
                sp.friendly_name = old.friendly_name;
                sp.invisible     = old.invisible;
                break;
            }
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
    // Topology fetch updates speakers[].friendly_name with ZoneGroupMember ZoneName.
    // next.speakers must be assigned AFTER fetch_topology so zone names are included.
    SonosState next;
    next.home_room_idx = home_room_idx;

    if (home_room_idx >= 0) {
        std::vector<ZoneGroup> prev_groups = sonos::state::snapshot().groups;
        std::vector<ZoneGroup> groups = fetch_topology(speakers[home_room_idx], speakers);
        next.groups = groups.empty() ? prev_groups : groups;
    } else {
        next.groups = sonos::state::snapshot().groups;
    }

    next.speakers = speakers;  // assign after fetch_topology has populated zone names
    sonos::state::update(next);

    // Derive group UUID from fresh topology and update playback state once per cycle.
    // This corrects any drift from external group changes; optimistic updates handle
    // the immediate feedback after user-initiated join/leave commands.
    if (home_room_idx >= 0) {
        publish_group_uuid(next.groups, speakers[home_room_idx].uuid);
    }
}
