#include "playback.h"
#include "soap.h"

#include <Arduino.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// ── Statics ──────────────────────────────────────────────────────────────────

static SemaphoreHandle_t  s_mutex     = nullptr;
static QueueHandle_t      s_cmd_queue = nullptr;
static sonos::PlaybackState s_state;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Converts "h:mm:ss" to total seconds.  Returns 0 on parse failure.
static uint32_t parse_time(const String& hmmss) {
    int first = hmmss.indexOf(':');
    if (first < 0) return 0;
    int second = hmmss.indexOf(':', first + 1);
    if (second < 0) return 0;
    uint32_t h = (uint32_t)hmmss.substring(0, first).toInt();
    uint32_t m = (uint32_t)hmmss.substring(first + 1, second).toInt();
    uint32_t s = (uint32_t)hmmss.substring(second + 1).toInt();
    return h * 3600u + m * 60u + s;
}

// Percent-decodes a URL-encoded string (%XX sequences, + → space).
static String url_decode(const String& s) {
    String out;
    out.reserve(s.length());
    for (int i = 0; i < (int)s.length(); ++i) {
        char c = s[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < (int)s.length()) {
            char hex[3] = { s[i + 1], s[i + 2], '\0' };
            out += (char)strtol(hex, nullptr, 16);
            i += 2;
        } else {
            out += c;
        }
    }
    return out;
}

// HTML-entity-decodes the common five entities in-place (used for DIDL blocks).
static String html_decode(const String& s) {
    String r = s;
    r.replace("&lt;",   "<");
    r.replace("&gt;",   ">");
    r.replace("&quot;", "\"");
    r.replace("&apos;", "'");
    r.replace("&amp;",  "&");  // must be last
    return r;
}

// Returns the text between the first <tag>…</tag> pair; empty on miss.
static String extract_xml_text(const String& xml, const String& tag) {
    String open  = "<" + tag + ">";
    String close = "</" + tag + ">";
    int s = xml.indexOf(open);
    if (s < 0) return "";
    s += open.length();
    int e = xml.indexOf(close, s);
    if (e < 0) return "";
    return xml.substring(s, e);
}

// ── Public interface ──────────────────────────────────────────────────────────

void sonos::playback::init() {
    s_mutex     = xSemaphoreCreateMutex();
    s_cmd_queue = xQueueCreate(4, sizeof(uint8_t));
}

void sonos::playback::update(const PlaybackState& next) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state         = next;
    s_state.version = s_state.version + 1;
    xSemaphoreGive(s_mutex);
}

sonos::PlaybackState sonos::playback::snapshot() {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    PlaybackState copy = s_state;
    xSemaphoreGive(s_mutex);
    return copy;
}

void sonos::playback::enqueue_command(PlaybackCommand cmd) {
    uint8_t raw = static_cast<uint8_t>(cmd);
    if (xQueueSend(s_cmd_queue, &raw, 0) != pdTRUE) {
        Serial.println("[Playback] command queue full — dropped");
    }
}

// ── drain_commands (task 3.1) ─────────────────────────────────────────────────

void sonos::playback::drain_commands(const SpeakerInfo& speaker) {
    static const char* kPath = "/MediaRenderer/AVTransport/Control";
    static const char* kSvc  = "urn:schemas-upnp-org:service:AVTransport:1";

    uint8_t raw;
    while (xQueueReceive(s_cmd_queue, &raw, 0) == pdTRUE) {
        PlaybackCommand cmd = static_cast<PlaybackCommand>(raw);
        const char* name = "?";
        String action, args;

        switch (cmd) {
            case PlaybackCommand::Play:
                name   = "Play";
                action = "Play";
                args   = "<InstanceID>0</InstanceID><Speed>1</Speed>";
                break;
            case PlaybackCommand::Pause:
                name   = "Pause";
                action = "Pause";
                args   = "<InstanceID>0</InstanceID>";
                break;
            case PlaybackCommand::Next:
                name   = "Next";
                action = "Next";
                args   = "<InstanceID>0</InstanceID>";
                break;
            case PlaybackCommand::Previous:
                name   = "Previous";
                action = "Previous";
                args   = "<InstanceID>0</InstanceID>";
                break;
        }

        Serial.printf("[Playback] command %s\n", name);
        String resp = sonos::soap::call(speaker.ip, speaker.port, kPath, kSvc, action, args);
        if (resp.isEmpty()) {
            Serial.printf("[Playback] command %s failed\n", name);
        }
    }
}

// ── poll (task 2.4) ───────────────────────────────────────────────────────────

void sonos::playback::poll(const SpeakerInfo& speaker) {
    static const char* kPath = "/MediaRenderer/AVTransport/Control";
    static const char* kSvc  = "urn:schemas-upnp-org:service:AVTransport:1";
    static const char* kArgs = "<InstanceID>0</InstanceID>";

    // GetTransportInfo
    String info_resp = sonos::soap::call(
        speaker.ip, speaker.port, kPath, kSvc, "GetTransportInfo", kArgs);
    if (info_resp.isEmpty()) {
        Serial.println("[Playback] GetTransportInfo failed");
        return;
    }

    String ts_str = extract_xml_text(info_resp, "CurrentTransportState");
    TransportState ts = TransportState::Unknown;
    if      (ts_str == "PLAYING")          ts = TransportState::Playing;
    else if (ts_str == "PAUSED_PLAYBACK")  ts = TransportState::Paused;
    else if (ts_str == "STOPPED")          ts = TransportState::Stopped;
    else if (ts_str == "TRANSITIONING")    ts = TransportState::Transitioning;

    // GetPositionInfo
    String pos_resp = sonos::soap::call(
        speaker.ip, speaker.port, kPath, kSvc, "GetPositionInfo", kArgs);
    if (pos_resp.isEmpty()) {
        Serial.println("[Playback] GetPositionInfo failed");
        return;
    }

    uint32_t duration_s = parse_time(extract_xml_text(pos_resp, "TrackDuration"));
    uint32_t position_s = parse_time(extract_xml_text(pos_resp, "RelTime"));
    String   track_uri  = extract_xml_text(pos_resp, "TrackURI");

    // TrackMetaData is HTML-entity-encoded DIDL-Lite XML
    String didl_encoded = extract_xml_text(pos_resp, "TrackMetaData");
    String didl = html_decode(didl_encoded);
    String title  = extract_xml_text(didl, "dc:title");
    String artist = extract_xml_text(didl, "dc:creator");
    String album  = extract_xml_text(didl, "upnp:album");

    PlaybackState next;
    next.transport_state   = ts;
    next.title             = title;
    next.artist            = artist;
    next.album             = album;
    next.track_uri         = track_uri;
    next.duration_s        = duration_s;
    next.position_s        = position_s;
    next.poll_timestamp_ms = millis();
    next.version           = snapshot().version;

    update(next);

    const char* ts_label =
        ts == TransportState::Playing      ? "Playing" :
        ts == TransportState::Paused       ? "Paused"  :
        ts == TransportState::Stopped      ? "Stopped" :
        ts == TransportState::Transitioning? "Transitioning" : "Unknown";
    Serial.printf("[Playback] title=%s artist=%s state=%s\n",
                  title.c_str(), artist.c_str(), ts_label);
}
