#include "playback.h"
#include "soap.h"

#include <Arduino.h>
#include <atomic>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// ── Statics ──────────────────────────────────────────────────────────────────

static SemaphoreHandle_t       s_mutex            = nullptr;
static QueueHandle_t           s_cmd_queue        = nullptr;
static sonos::PlaybackState    s_state;
static std::atomic<int16_t>    s_pending_delta{0};
static std::atomic<uint8_t>    s_boost_polls{0};
static String                  s_group_target_uuid;
static String                  s_group_op_target_uuid;
static TaskHandle_t            s_notify_task_handle = nullptr;

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

void sonos::playback::set_notify_task(void* handle) {
    s_notify_task_handle = static_cast<TaskHandle_t>(handle);
}

void sonos::playback::enqueue_command(PlaybackCommand cmd) {
    uint8_t raw = static_cast<uint8_t>(cmd);
    if (xQueueSend(s_cmd_queue, &raw, 0) != pdTRUE) {
        Serial.println("[Playback] command queue full — dropped");
    }
    // Wake the sonos_task immediately so it drains this command without waiting
    if (s_notify_task_handle) xTaskNotifyGive(s_notify_task_handle);
}

void sonos::playback::enqueue_volume_delta(int16_t delta) {
    s_pending_delta.fetch_add(delta, std::memory_order_relaxed);
    // Wake sonos_task immediately so SetVolume fires within ~100ms, same as button taps
    if (s_notify_task_handle) xTaskNotifyGive(s_notify_task_handle);
}

void sonos::playback::set_group_target(const String& uuid) {
    s_group_target_uuid = uuid;
}

void sonos::playback::set_group_op_target(const String& uuid) {
    s_group_op_target_uuid = uuid;
}

void sonos::playback::update_group_uuid(const String& uuid) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.group_coordinator_uuid = uuid;
    s_state.version++;
    xSemaphoreGive(s_mutex);
}

uint32_t sonos::playback::poll_interval_ms() {
    uint8_t remaining = s_boost_polls.load(std::memory_order_relaxed);
    if (remaining > 0) {
        s_boost_polls.fetch_sub(1, std::memory_order_relaxed);
        return 500;
    }
    return kPollIntervalMs;
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
            case PlaybackCommand::ToggleMute: {
                name = "ToggleMute";
                bool cur_muted = snapshot().muted;
                Serial.printf("[Playback] command ToggleMute (currently %s)\n",
                              cur_muted ? "muted" : "unmuted");
                String mute_args =
                    String("<InstanceID>0</InstanceID><Channel>Master</Channel><DesiredMute>") +
                    (cur_muted ? "0" : "1") + "</DesiredMute>";
                String mute_resp = sonos::soap::call(
                    speaker.ip, speaker.port,
                    "/MediaRenderer/RenderingControl/Control",
                    "urn:schemas-upnp-org:service:RenderingControl:1",
                    "SetMute", mute_args);
                if (mute_resp.isEmpty()) {
                    Serial.println("[Playback] command ToggleMute failed");
                }
                s_boost_polls.store(4, std::memory_order_relaxed);
                continue;
            }  // end ToggleMute

            case PlaybackCommand::LeaveGroup: {
                name = "LeaveGroup";
                Serial.println("[Group] leaving group");
                String r = sonos::soap::call(
                    speaker.ip, speaker.port,
                    "/MediaRenderer/AVTransport/Control",
                    "urn:schemas-upnp-org:service:AVTransport:1",
                    "BecomeCoordinatorOfStandaloneGroup",
                    "<InstanceID>0</InstanceID>");
                if (r.isEmpty()) Serial.println("[Group] LeaveGroup failed");
                else update_group_uuid("");  // optimistic: standalone immediately
                s_boost_polls.store(4, std::memory_order_relaxed);
                continue;
            }

            case PlaybackCommand::JoinGroup: {
                name = "JoinGroup";
                Serial.printf("[Group] joining coordinator %s\n",
                              s_group_target_uuid.c_str());
                String join_args = "<InstanceID>0</InstanceID><CurrentURI>x-rincon:"
                                   + s_group_target_uuid
                                   + "</CurrentURI><CurrentURIMetaData></CurrentURIMetaData>";
                String r = sonos::soap::call(
                    speaker.ip, speaker.port,
                    "/MediaRenderer/AVTransport/Control",
                    "urn:schemas-upnp-org:service:AVTransport:1",
                    "SetAVTransportURI", join_args);
                if (r.isEmpty()) {
                    Serial.println("[Group] JoinGroup SetAVTransportURI failed");
                } else {
                    String pr = sonos::soap::call(
                        speaker.ip, speaker.port,
                        "/MediaRenderer/AVTransport/Control",
                        "urn:schemas-upnp-org:service:AVTransport:1",
                        "Play", "<InstanceID>0</InstanceID><Speed>1</Speed>");
                    if (pr.isEmpty()) Serial.println("[Group] JoinGroup Play failed");
                    else update_group_uuid(s_group_target_uuid);
                }
                s_boost_polls.store(4, std::memory_order_relaxed);
                continue;
            }

            case PlaybackCommand::AddGroupMember: {
                // Add another speaker to the home room's current group.
                // Calls SetAVTransportURI on the TARGET speaker (not home room).
                SonosState st_snap = sonos::state::snapshot();
                String t_ip; uint16_t t_port = 0;
                for (const auto& sp : st_snap.speakers) {
                    if (sp.uuid == s_group_op_target_uuid) { t_ip = sp.ip; t_port = sp.port; break; }
                }
                if (!t_ip.isEmpty()) {
                    // Use coordinator UUID — home room's coordinator or itself if coordinator
                    PlaybackState pb_snap = snapshot();
                    String coord = pb_snap.group_coordinator_uuid.isEmpty()
                                   ? speaker.uuid   // home room is standalone — becomes coordinator
                                   : pb_snap.group_coordinator_uuid;
                    String args = "<InstanceID>0</InstanceID><CurrentURI>x-rincon:" + coord +
                                  "</CurrentURI><CurrentURIMetaData></CurrentURIMetaData>";
                    String r = sonos::soap::call(t_ip, t_port,
                        "/MediaRenderer/AVTransport/Control",
                        "urn:schemas-upnp-org:service:AVTransport:1", "SetAVTransportURI", args);
                    if (!r.isEmpty()) {
                        sonos::soap::call(t_ip, t_port,
                            "/MediaRenderer/AVTransport/Control",
                            "urn:schemas-upnp-org:service:AVTransport:1", "Play",
                            "<InstanceID>0</InstanceID><Speed>1</Speed>");
                    }
                    Serial.printf("[Group] AddGroupMember %s\n", s_group_op_target_uuid.c_str());
                }
                s_boost_polls.store(4, std::memory_order_relaxed);
                continue;
            }

            case PlaybackCommand::RemoveGroupMember: {
                // Make a specific speaker standalone.
                // Calls BecomeCoordinatorOfStandaloneGroup on the TARGET speaker.
                SonosState st_snap = sonos::state::snapshot();
                String t_ip; uint16_t t_port = 0;
                for (const auto& sp : st_snap.speakers) {
                    if (sp.uuid == s_group_op_target_uuid) { t_ip = sp.ip; t_port = sp.port; break; }
                }
                if (!t_ip.isEmpty()) {
                    sonos::soap::call(t_ip, t_port,
                        "/MediaRenderer/AVTransport/Control",
                        "urn:schemas-upnp-org:service:AVTransport:1",
                        "BecomeCoordinatorOfStandaloneGroup", "<InstanceID>0</InstanceID>");
                    Serial.printf("[Group] RemoveGroupMember %s\n", s_group_op_target_uuid.c_str());
                }
                s_boost_polls.store(4, std::memory_order_relaxed);
                continue;
            }
        }  // end switch

        Serial.printf("[Playback] command %s\n", name);
        String resp = sonos::soap::call(speaker.ip, speaker.port, kPath, kSvc, action, args);
        if (resp.isEmpty()) {
            Serial.printf("[Playback] command %s failed\n", name);
        }
        s_boost_polls.store(4, std::memory_order_relaxed);
    }

    // Flush accumulated encoder volume delta as a single SetVolume call
    int16_t delta = s_pending_delta.exchange(0, std::memory_order_relaxed);
    if (delta != 0) {
        PlaybackState cur = snapshot();
        int new_vol = (int)cur.volume + delta;
        if (new_vol < 0)   new_vol = 0;
        if (new_vol > 100) new_vol = 100;
        Serial.printf("[Playback] SetVolume %d\n", new_vol);
        String vol_args =
            String("<InstanceID>0</InstanceID><Channel>Master</Channel><DesiredVolume>") +
            new_vol + "</DesiredVolume>";
        String vol_resp = sonos::soap::call(
            speaker.ip, speaker.port,
            "/MediaRenderer/RenderingControl/Control",
            "urn:schemas-upnp-org:service:RenderingControl:1",
            "SetVolume", vol_args);
        if (vol_resp.isEmpty()) {
            Serial.printf("[Playback] SetVolume %d failed\n", new_vol);
        }
        s_boost_polls.store(4, std::memory_order_relaxed);
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
    String title   = extract_xml_text(didl, "dc:title");
    String artist  = extract_xml_text(didl, "dc:creator");
    String album   = extract_xml_text(didl, "upnp:album");
    String art_uri = extract_xml_text(didl, "upnp:albumArtURI");

    // GetVolume + GetMute via RenderingControl
    static const char* kRCPath = "/MediaRenderer/RenderingControl/Control";
    static const char* kRCSvc  = "urn:schemas-upnp-org:service:RenderingControl:1";
    static const char* kRCArgs = "<InstanceID>0</InstanceID><Channel>Master</Channel>";

    PlaybackState prev = snapshot();
    uint8_t volume = prev.volume;
    bool    muted  = prev.muted;

    String vol_resp = sonos::soap::call(speaker.ip, speaker.port, kRCPath, kRCSvc, "GetVolume", kRCArgs);
    if (vol_resp.isEmpty()) {
        Serial.println("[Playback] GetVolume failed");
    } else {
        int v = extract_xml_text(vol_resp, "CurrentVolume").toInt();
        volume = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    }

    String mute_resp = sonos::soap::call(speaker.ip, speaker.port, kRCPath, kRCSvc, "GetMute", kRCArgs);
    if (mute_resp.isEmpty()) {
        Serial.println("[Playback] GetMute failed");
    } else {
        muted = (extract_xml_text(mute_resp, "CurrentMute") == "1");
    }

    PlaybackState next;
    next.transport_state   = ts;
    next.title             = title;
    next.artist            = artist;
    next.album             = album;
    next.album_art_uri     = art_uri;
    next.track_uri         = track_uri;
    next.duration_s        = duration_s;
    next.position_s        = position_s;
    next.poll_timestamp_ms = millis();
    next.volume            = volume;
    next.muted             = muted;
    next.version           = prev.version;

    update(next);

    const char* ts_label =
        ts == TransportState::Playing      ? "Playing" :
        ts == TransportState::Paused       ? "Paused"  :
        ts == TransportState::Stopped      ? "Stopped" :
        ts == TransportState::Transitioning? "Transitioning" : "Unknown";
    Serial.printf("[Playback] title=%s artist=%s state=%s\n",
                  title.c_str(), artist.c_str(), ts_label);
}

// ── poll_volume — fetch volume/mute from a specific speaker (always home room) ─

void sonos::playback::poll_volume(const SpeakerInfo& speaker) {
    static const char* kRCPath = "/MediaRenderer/RenderingControl/Control";
    static const char* kRCSvc  = "urn:schemas-upnp-org:service:RenderingControl:1";
    static const char* kRCArgs = "<InstanceID>0</InstanceID><Channel>Master</Channel>";

    PlaybackState prev = snapshot();
    uint8_t volume = prev.volume;
    bool    muted  = prev.muted;

    String vol_resp = sonos::soap::call(speaker.ip, speaker.port, kRCPath, kRCSvc, "GetVolume", kRCArgs);
    if (!vol_resp.isEmpty()) {
        int v = extract_xml_text(vol_resp, "CurrentVolume").toInt();
        volume = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    }
    String mute_resp = sonos::soap::call(speaker.ip, speaker.port, kRCPath, kRCSvc, "GetMute", kRCArgs);
    if (!mute_resp.isEmpty()) {
        muted = (extract_xml_text(mute_resp, "CurrentMute") == "1");
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.volume  = volume;
    s_state.muted   = muted;
    s_state.version++;
    xSemaphoreGive(s_mutex);
}
