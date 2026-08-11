#include "gena_parse.h"
#include "xml_util.h"

using sonos::xml::field;
using sonos::xml::html_decode;
using sonos::xml::parse_time;
using sonos::xml::tag_val;
using sonos::xml::tag_val_channel;

// Case-insensitive search for a header name at line start; returns the
// trimmed value or empty. HTTP header names are case-insensitive and Sonos
// firmware varies (SID vs Sid, TIMEOUT vs Timeout).
static String header_value(const String& head, const String& name) {
    String lower_head = head;
    lower_head.toLowerCase();
    String lower_name = name;
    lower_name.toLowerCase();
    String pat = lower_name + ":";
    int pos = 0;
    while (true) {
        pos = lower_head.indexOf(pat, pos);
        if (pos < 0) return "";
        // Must be at the start of a line
        if (pos == 0 || lower_head[pos - 1] == '\n') break;
        pos += pat.length();
    }
    int vstart = pos + pat.length();
    int vend = head.indexOf('\n', vstart);
    if (vend < 0) vend = head.length();
    String v = head.substring(vstart, vend);
    v.trim();
    return v;
}

sonos::gena::SubscribeResponse
sonos::gena::parse_subscribe_response(const String& http_head) {
    SubscribeResponse r;

    // Status line: "HTTP/1.1 200 OK"
    int sp = http_head.indexOf(' ');
    if (sp >= 0) r.status = http_head.substring(sp + 1, sp + 4).toInt();

    r.sid = header_value(http_head, "SID");

    // "TIMEOUT: Second-3600" — missing or unparseable defaults to 300s so a
    // quirky speaker still gets renewed (early renewal is harmless; a lease
    // assumed too long would silently expire).
    r.timeout_s = 300;
    String t = header_value(http_head, "TIMEOUT");
    int dash = t.indexOf('-');
    if (dash >= 0) {
        long secs = t.substring(dash + 1).toInt();
        if (secs > 0) r.timeout_s = (uint32_t)secs;
        // "Second-infinite" → toInt() = 0 → keep the 300s default
        if (r.timeout_s > 86400u) r.timeout_s = 86400u;  // keep renew_at_ms math overflow-safe
    }

    r.ok = (r.status == 200) && !r.sid.isEmpty();
    return r;
}

sonos::gena::NotifyHead
sonos::gena::parse_notify_head(const String& http_head) {
    NotifyHead n;

    // Request line: "NOTIFY /notify HTTP/1.1"
    String first = http_head.substring(0, http_head.indexOf('\n'));
    if (!first.startsWith("NOTIFY ")) return n;

    n.sid = header_value(http_head, "SID");
    if (n.sid.isEmpty()) return n;

    String seq = header_value(http_head, "SEQ");
    n.seq = (uint32_t)strtoul(seq.c_str(), nullptr, 10);

    String cl = header_value(http_head, "Content-Length");
    n.content_length = (uint32_t)strtoul(cl.c_str(), nullptr, 10);

    String te = header_value(http_head, "Transfer-Encoding");
    n.chunked = te.indexOf("chunked") >= 0;

    n.ok = true;
    return n;
}

sonos::gena::AVTransportEvent
sonos::gena::parse_avtransport_event(const String& notify_body) {
    AVTransportEvent ev;

    // propertyset → LastChange (encoded event XML)
    String last_change = html_decode(field(notify_body, "LastChange"));
    if (last_change.isEmpty()) return ev;

    String ts = tag_val(last_change, "TransportState");
    if (!ts.isEmpty()) {
        ev.has_transport   = true;
        ev.transport_state = ts;
    }

    // CurrentTrackMetaData val is DIDL-Lite, encoded a second time
    String didl_encoded = tag_val(last_change, "CurrentTrackMetaData");
    if (!didl_encoded.isEmpty() && didl_encoded != "NOT_IMPLEMENTED") {
        String didl = html_decode(didl_encoded);
        ev.has_metadata = true;
        ev.title     = html_decode(field(didl, "dc:title"));
        ev.artist    = html_decode(field(didl, "dc:creator"));
        ev.album     = html_decode(field(didl, "upnp:album"));
        ev.art_uri   = field(didl, "upnp:albumArtURI");
        ev.track_uri = field(didl, "res");
    }

    String dur = tag_val(last_change, "CurrentTrackDuration");
    if (!dur.isEmpty() && dur != "NOT_IMPLEMENTED") {
        ev.has_duration = true;
        ev.duration_s   = parse_time(dur);
    }

    return ev;
}

sonos::gena::RenderingControlEvent
sonos::gena::parse_rendering_control_event(const String& notify_body) {
    RenderingControlEvent ev;

    String last_change = html_decode(field(notify_body, "LastChange"));
    if (last_change.isEmpty()) return ev;

    bool found = false;
    String vol = tag_val_channel(last_change, "Volume", "Master", found);
    if (found) {
        long v = vol.toInt();
        ev.has_volume = true;
        ev.volume     = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    }

    String mute = tag_val_channel(last_change, "Mute", "Master", found);
    if (found) {
        ev.has_mute = true;
        ev.muted    = (mute == "1");
    }

    return ev;
}

String sonos::gena::extract_zone_group_state(const String& notify_body) {
    String raw = field(notify_body, "ZoneGroupState");
    if (raw.isEmpty()) return "";
    String decoded = html_decode(raw);
    // Some firmware wraps the group list in an outer <ZoneGroupState> element
    // after decoding; the topology parser searches for <ZoneGroup … either way.
    return decoded;
}

uint32_t sonos::gena::renew_at_ms(uint32_t now_ms, uint32_t lease_s) {
    return now_ms + (lease_s * 1000u) / 2u;
}
