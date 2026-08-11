#pragma once
#include <Arduino.h>

// Pure-logic parsers for UPnP GENA (event subscription) traffic.
// No networking, no FreeRTOS — compiles under test_native so every parser
// can be exercised host-side against canned Sonos payloads.
namespace sonos::gena {

// ── SUBSCRIBE response ────────────────────────────────────────────────────────
// Parsed from the raw HTTP response head (status line + headers).
struct SubscribeResponse {
    bool     ok        = false;  // HTTP 200 with a SID present
    int      status    = 0;      // HTTP status code (412 = speaker lost the SID)
    String   sid;                // subscription id, e.g. "uuid:RINCON_...._sub0000000001"
    uint32_t timeout_s = 0;      // granted lease; 300 when the header is absent/unparseable
};
SubscribeResponse parse_subscribe_response(const String& http_head);

// ── NOTIFY request head ───────────────────────────────────────────────────────
struct NotifyHead {
    bool     ok             = false;  // request line is a NOTIFY with a SID header
    String   sid;
    uint32_t seq            = 0;
    uint32_t content_length = 0;      // 0 = absent (read until close)
    bool     chunked        = false;
};
NotifyHead parse_notify_head(const String& http_head);

// ── AVTransport LastChange ────────────────────────────────────────────────────
// Input: the NOTIFY body (e:propertyset XML). The LastChange property is
// HTML-entity-encoded event XML whose CurrentTrackMetaData val is encoded
// a second time (DIDL-Lite).
struct AVTransportEvent {
    bool     has_transport = false;
    String   transport_state;        // raw: "PLAYING", "PAUSED_PLAYBACK", ...
    bool     has_metadata  = false;
    String   title;
    String   artist;
    String   album;
    String   art_uri;
    String   track_uri;
    bool     has_duration  = false;
    uint32_t duration_s    = 0;
};
AVTransportEvent parse_avtransport_event(const String& notify_body);

// ── RenderingControl LastChange ───────────────────────────────────────────────
// Master-channel volume and mute only; LF/RF channel entries are ignored.
struct RenderingControlEvent {
    bool    has_volume = false;
    uint8_t volume     = 0;      // clamped 0-100
    bool    has_mute   = false;
    bool    muted      = false;
};
RenderingControlEvent parse_rendering_control_event(const String& notify_body);

// ── ZoneGroupTopology ─────────────────────────────────────────────────────────
// Returns the decoded ZoneGroupState XML from a topology NOTIFY body, ready
// for the same parser the SOAP GetZoneGroupState path uses. Empty on miss.
String extract_zone_group_state(const String& notify_body);

// ── Renewal math ──────────────────────────────────────────────────────────────
// Renew at half the granted lease. Pure so the wrap-safe arithmetic is testable.
uint32_t renew_at_ms(uint32_t now_ms, uint32_t lease_s);

} // namespace sonos::gena
