/* GENA parser unit tests — run on the host with no hardware.
 *
 * Exercises the pure-logic layer of the eventing capability against canned
 * payloads shaped like real Sonos traffic: SUBSCRIBE responses, NOTIFY
 * request heads, and the double-encoded LastChange / ZoneGroupState bodies.
 *
 * Encoding levels in an AVTransport NOTIFY body (the part most worth pinning
 * down with tests):
 *   level 0 — the propertyset XML itself
 *   level 1 — <LastChange> content: entity-encoded Event XML
 *   level 2 — CurrentTrackMetaData's val attribute: entity-encoded DIDL-Lite
 * A text field like a title containing an apostrophe is therefore
 * triple-encoded on the wire (&amp;amp;apos;).
 */

#include <unity.h>
#include "app/sonos/xml_util.h"
#include "app/sonos/gena_parse.h"

void setUp()    {}
void tearDown() {}

// ── xml_util ──────────────────────────────────────────────────────────────────

void test_parse_time() {
    TEST_ASSERT_EQUAL_UINT32(192,  sonos::xml::parse_time("0:03:12"));
    TEST_ASSERT_EQUAL_UINT32(3723, sonos::xml::parse_time("1:02:03"));
    TEST_ASSERT_EQUAL_UINT32(0,    sonos::xml::parse_time("garbage"));
    TEST_ASSERT_EQUAL_UINT32(0,    sonos::xml::parse_time(""));
}

void test_html_decode_single_level() {
    String out = sonos::xml::html_decode("&lt;a b=&quot;c&quot;&gt;&apos;x&apos;&amp;y");
    TEST_ASSERT_EQUAL_STRING("<a b=\"c\">'x'&y", out.c_str());
}

void test_html_decode_preserves_next_level() {
    // &amp;lt; must decode to &lt; (one level at a time), never to <
    String out = sonos::xml::html_decode("&amp;lt;DIDL&amp;gt;");
    TEST_ASSERT_EQUAL_STRING("&lt;DIDL&gt;", out.c_str());
}

void test_html_decode_smart_quotes() {
    String out = sonos::xml::html_decode("Don\xe2\x80\x99t");
    TEST_ASSERT_EQUAL_STRING("Don't", out.c_str());
}

void test_tag_val_basic() {
    String out = sonos::xml::tag_val("<Event><TransportState val=\"PLAYING\"/></Event>",
                                     "TransportState");
    TEST_ASSERT_EQUAL_STRING("PLAYING", out.c_str());
}

void test_tag_val_prefix_guard() {
    // <VolumeDB … must not satisfy a search for <Volume …
    String xml = "<VolumeDB channel=\"Master\" val=\"-15\"/><Volume channel=\"Master\" val=\"27\"/>";
    TEST_ASSERT_EQUAL_STRING("27", sonos::xml::tag_val(xml, "Volume").c_str());
}

void test_tag_val_channel_selects_master() {
    String xml = "<Volume channel=\"LF\" val=\"100\"/>"
                 "<Volume channel=\"RF\" val=\"100\"/>"
                 "<Volume channel=\"Master\" val=\"27\"/>";
    bool found = false;
    String out = sonos::xml::tag_val_channel(xml, "Volume", "Master", found);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("27", out.c_str());
}

void test_tag_val_channel_miss() {
    bool found = true;
    sonos::xml::tag_val_channel("<Volume channel=\"LF\" val=\"1\"/>", "Volume", "Master", found);
    TEST_ASSERT_FALSE(found);
}

// ── SUBSCRIBE response ────────────────────────────────────────────────────────

void test_subscribe_response_ok() {
    auto r = sonos::gena::parse_subscribe_response(
        "HTTP/1.1 200 OK\n"
        "SID: uuid:RINCON_ABC123_sub0000000001\n"
        "TIMEOUT: Second-3600\n"
        "Server: Linux UPnP/1.0 Sonos/83.1-61240\n"
        "\n");
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_INT(200, r.status);
    TEST_ASSERT_EQUAL_STRING("uuid:RINCON_ABC123_sub0000000001", r.sid.c_str());
    TEST_ASSERT_EQUAL_UINT32(3600, r.timeout_s);
}

void test_subscribe_response_missing_timeout_defaults_300() {
    auto r = sonos::gena::parse_subscribe_response(
        "HTTP/1.1 200 OK\nSID: uuid:RINCON_X_sub1\n\n");
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_UINT32(300, r.timeout_s);
}

void test_subscribe_response_infinite_timeout_defaults_300() {
    auto r = sonos::gena::parse_subscribe_response(
        "HTTP/1.1 200 OK\nSID: uuid:RINCON_X_sub1\nTIMEOUT: Second-infinite\n\n");
    TEST_ASSERT_EQUAL_UINT32(300, r.timeout_s);
}

void test_subscribe_response_412() {
    auto r = sonos::gena::parse_subscribe_response(
        "HTTP/1.1 412 Precondition Failed\n\n");
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL_INT(412, r.status);
}

void test_subscribe_response_case_insensitive_headers() {
    auto r = sonos::gena::parse_subscribe_response(
        "HTTP/1.1 200 OK\nSid: uuid:RINCON_Y_sub2\ntimeout: Second-1800\n\n");
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_STRING("uuid:RINCON_Y_sub2", r.sid.c_str());
    TEST_ASSERT_EQUAL_UINT32(1800, r.timeout_s);
}

void test_subscribe_response_huge_lease_clamped() {
    auto r = sonos::gena::parse_subscribe_response(
        "HTTP/1.1 200 OK\nSID: uuid:RINCON_Z_sub3\nTIMEOUT: Second-999999999\n\n");
    TEST_ASSERT_EQUAL_UINT32(86400, r.timeout_s);
}

// ── NOTIFY head ───────────────────────────────────────────────────────────────

void test_notify_head_ok() {
    auto n = sonos::gena::parse_notify_head(
        "NOTIFY /notify HTTP/1.1\n"
        "HOST: 192.168.1.50:3400\n"
        "CONTENT-TYPE: text/xml\n"
        "CONTENT-LENGTH: 1234\n"
        "NT: upnp:event\n"
        "NTS: upnp:propchange\n"
        "SID: uuid:RINCON_ABC123_sub0000000001\n"
        "SEQ: 7\n"
        "\n");
    TEST_ASSERT_TRUE(n.ok);
    TEST_ASSERT_EQUAL_STRING("uuid:RINCON_ABC123_sub0000000001", n.sid.c_str());
    TEST_ASSERT_EQUAL_UINT32(7, n.seq);
    TEST_ASSERT_EQUAL_UINT32(1234, n.content_length);
    TEST_ASSERT_FALSE(n.chunked);
}

void test_notify_head_rejects_non_notify() {
    auto n = sonos::gena::parse_notify_head("GET /notify HTTP/1.1\nSID: uuid:x\n\n");
    TEST_ASSERT_FALSE(n.ok);
}

void test_notify_head_rejects_missing_sid() {
    auto n = sonos::gena::parse_notify_head("NOTIFY /notify HTTP/1.1\nSEQ: 0\n\n");
    TEST_ASSERT_FALSE(n.ok);
}

void test_notify_head_chunked() {
    auto n = sonos::gena::parse_notify_head(
        "NOTIFY /notify HTTP/1.1\nSID: uuid:x\nTransfer-Encoding: chunked\n\n");
    TEST_ASSERT_TRUE(n.ok);
    TEST_ASSERT_TRUE(n.chunked);
}

// ── AVTransport event ─────────────────────────────────────────────────────────

// Level-0 body: LastChange holds encoded Event XML; CurrentTrackMetaData's val
// holds twice-encoded DIDL. Title carries a triple-encoded apostrophe.
static const char* kAVTransportBody =
    "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\"><e:property>"
    "<LastChange>"
    "&lt;Event xmlns=&quot;urn:schemas-upnp-org:metadata-1-0/AVT/&quot; "
    "xmlns:r=&quot;urn:schemas-rinconnetworks-com:metadata-1-0/&quot;&gt;"
    "&lt;InstanceID val=&quot;0&quot;&gt;"
    "&lt;TransportState val=&quot;PLAYING&quot;/&gt;"
    "&lt;CurrentPlayMode val=&quot;NORMAL&quot;/&gt;"
    "&lt;CurrentTrackDuration val=&quot;0:03:12&quot;/&gt;"
    "&lt;CurrentTrackMetaData val=&quot;"
    "&amp;lt;DIDL-Lite xmlns:dc=&amp;quot;http://purl.org/dc/elements/1.1/&amp;quot; "
    "xmlns:upnp=&amp;quot;urn:schemas-upnp-org:metadata-1-0/upnp/&amp;quot;&amp;gt;"
    "&amp;lt;item id=&amp;quot;-1&amp;quot; parentID=&amp;quot;-1&amp;quot;&amp;gt;"
    "&amp;lt;res&amp;gt;x-sonos-spotify:spotify%3atrack%3a123&amp;lt;/res&amp;gt;"
    "&amp;lt;dc:title&amp;gt;Don&amp;amp;apos;t Stop&amp;lt;/dc:title&amp;gt;"
    "&amp;lt;dc:creator&amp;gt;Fleetwood Mac&amp;lt;/dc:creator&amp;gt;"
    "&amp;lt;upnp:album&amp;gt;Rumours&amp;lt;/upnp:album&amp;gt;"
    "&amp;lt;upnp:albumArtURI&amp;gt;/getaa?s=1&amp;amp;amp;u=x-sonos-spotify"
    "&amp;lt;/upnp:albumArtURI&amp;gt;"
    "&amp;lt;/item&amp;gt;&amp;lt;/DIDL-Lite&amp;gt;"
    "&quot;/&gt;"
    "&lt;/InstanceID&gt;&lt;/Event&gt;"
    "</LastChange>"
    "</e:property></e:propertyset>";

void test_avtransport_event_full() {
    auto ev = sonos::gena::parse_avtransport_event(kAVTransportBody);
    TEST_ASSERT_TRUE(ev.has_transport);
    TEST_ASSERT_EQUAL_STRING("PLAYING", ev.transport_state.c_str());
    TEST_ASSERT_TRUE(ev.has_metadata);
    TEST_ASSERT_EQUAL_STRING("Don't Stop", ev.title.c_str());
    TEST_ASSERT_EQUAL_STRING("Fleetwood Mac", ev.artist.c_str());
    TEST_ASSERT_EQUAL_STRING("Rumours", ev.album.c_str());
    TEST_ASSERT_EQUAL_STRING("x-sonos-spotify:spotify%3atrack%3a123", ev.track_uri.c_str());
    TEST_ASSERT_TRUE(ev.has_duration);
    TEST_ASSERT_EQUAL_UINT32(192, ev.duration_s);
}

void test_avtransport_event_transport_only() {
    auto ev = sonos::gena::parse_avtransport_event(
        "<e:propertyset><e:property><LastChange>"
        "&lt;Event&gt;&lt;InstanceID val=&quot;0&quot;&gt;"
        "&lt;TransportState val=&quot;PAUSED_PLAYBACK&quot;/&gt;"
        "&lt;/InstanceID&gt;&lt;/Event&gt;"
        "</LastChange></e:property></e:propertyset>");
    TEST_ASSERT_TRUE(ev.has_transport);
    TEST_ASSERT_EQUAL_STRING("PAUSED_PLAYBACK", ev.transport_state.c_str());
    TEST_ASSERT_FALSE(ev.has_metadata);
    TEST_ASSERT_FALSE(ev.has_duration);
}

void test_avtransport_event_not_implemented_metadata() {
    auto ev = sonos::gena::parse_avtransport_event(
        "<e:propertyset><e:property><LastChange>"
        "&lt;Event&gt;&lt;InstanceID val=&quot;0&quot;&gt;"
        "&lt;TransportState val=&quot;PLAYING&quot;/&gt;"
        "&lt;CurrentTrackMetaData val=&quot;NOT_IMPLEMENTED&quot;/&gt;"
        "&lt;/InstanceID&gt;&lt;/Event&gt;"
        "</LastChange></e:property></e:propertyset>");
    TEST_ASSERT_TRUE(ev.has_transport);
    TEST_ASSERT_FALSE(ev.has_metadata);
}

void test_avtransport_event_malformed() {
    auto ev = sonos::gena::parse_avtransport_event("this is not xml at all");
    TEST_ASSERT_FALSE(ev.has_transport);
    TEST_ASSERT_FALSE(ev.has_metadata);
    TEST_ASSERT_FALSE(ev.has_duration);
}

// ── RenderingControl event ────────────────────────────────────────────────────

static const char* kRenderingControlBody =
    "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\"><e:property>"
    "<LastChange>"
    "&lt;Event xmlns=&quot;urn:schemas-upnp-org:metadata-1-0/RCS/&quot;&gt;"
    "&lt;InstanceID val=&quot;0&quot;&gt;"
    "&lt;Volume channel=&quot;Master&quot; val=&quot;27&quot;/&gt;"
    "&lt;Volume channel=&quot;LF&quot; val=&quot;100&quot;/&gt;"
    "&lt;Volume channel=&quot;RF&quot; val=&quot;100&quot;/&gt;"
    "&lt;Mute channel=&quot;Master&quot; val=&quot;0&quot;/&gt;"
    "&lt;Bass val=&quot;0&quot;/&gt;&lt;Treble val=&quot;0&quot;/&gt;"
    "&lt;/InstanceID&gt;&lt;/Event&gt;"
    "</LastChange>"
    "</e:property></e:propertyset>";

void test_rendering_control_event() {
    auto ev = sonos::gena::parse_rendering_control_event(kRenderingControlBody);
    TEST_ASSERT_TRUE(ev.has_volume);
    TEST_ASSERT_EQUAL_UINT8(27, ev.volume);
    TEST_ASSERT_TRUE(ev.has_mute);
    TEST_ASSERT_FALSE(ev.muted);
}

void test_rendering_control_volume_clamped() {
    auto ev = sonos::gena::parse_rendering_control_event(
        "<e:propertyset><e:property><LastChange>"
        "&lt;Event&gt;&lt;InstanceID val=&quot;0&quot;&gt;"
        "&lt;Volume channel=&quot;Master&quot; val=&quot;250&quot;/&gt;"
        "&lt;/InstanceID&gt;&lt;/Event&gt;"
        "</LastChange></e:property></e:propertyset>");
    TEST_ASSERT_TRUE(ev.has_volume);
    TEST_ASSERT_EQUAL_UINT8(100, ev.volume);
    TEST_ASSERT_FALSE(ev.has_mute);
}

// ── ZoneGroupTopology event ───────────────────────────────────────────────────

void test_zone_group_state_extraction() {
    String body =
        "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\"><e:property>"
        "<ZoneGroupState>"
        "&lt;ZoneGroupState&gt;&lt;ZoneGroups&gt;"
        "&lt;ZoneGroup Coordinator=&quot;RINCON_AAA&quot; ID=&quot;RINCON_AAA:42&quot;&gt;"
        "&lt;ZoneGroupMember UUID=&quot;RINCON_AAA&quot; ZoneName=&quot;Kitchen&quot;/&gt;"
        "&lt;ZoneGroupMember UUID=&quot;RINCON_BBB&quot; ZoneName=&quot;Brian&amp;apos;s Office&quot;/&gt;"
        "&lt;/ZoneGroup&gt;"
        "&lt;/ZoneGroups&gt;&lt;/ZoneGroupState&gt;"
        "</ZoneGroupState>"
        "</e:property></e:propertyset>";
    String zgs = sonos::gena::extract_zone_group_state(body);
    TEST_ASSERT_FALSE(zgs.isEmpty());
    // Decoded exactly one level: ready for the shared topology parser
    TEST_ASSERT_TRUE(zgs.indexOf("<ZoneGroup Coordinator=\"RINCON_AAA\"") >= 0);
    int member = zgs.indexOf("<ZoneGroupMember UUID=\"RINCON_BBB\"");
    TEST_ASSERT_TRUE(member >= 0);
    // The zone name stays entity-encoded at this level; the topology parser's
    // own html_decode handles it (same as the SOAP GetZoneGroupState path).
    TEST_ASSERT_TRUE(zgs.indexOf("Brian&apos;s Office") >= 0);
}

void test_zone_group_state_missing() {
    String zgs = sonos::gena::extract_zone_group_state(
        "<e:propertyset><e:property><ThirdPartyMediaServersX val=\"\"/></e:property></e:propertyset>");
    TEST_ASSERT_TRUE(zgs.isEmpty());
}

// ── Renewal math ──────────────────────────────────────────────────────────────

void test_renew_at_half_lease() {
    TEST_ASSERT_EQUAL_UINT32(1000 + 1800000, sonos::gena::renew_at_ms(1000, 3600));
    TEST_ASSERT_EQUAL_UINT32(500 + 150000,  sonos::gena::renew_at_ms(500, 300));
}

void test_renew_at_wraps_safely() {
    // Near the uint32 boundary the sum wraps — callers compare with signed
    // subtraction, so the wrapped value is still meaningful.
    uint32_t now = 0xFFFFF000u;
    uint32_t at  = sonos::gena::renew_at_ms(now, 3600);
    TEST_ASSERT_EQUAL_UINT32(now + 1800000u, at);  // wraps by design
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_time);
    RUN_TEST(test_html_decode_single_level);
    RUN_TEST(test_html_decode_preserves_next_level);
    RUN_TEST(test_html_decode_smart_quotes);
    RUN_TEST(test_tag_val_basic);
    RUN_TEST(test_tag_val_prefix_guard);
    RUN_TEST(test_tag_val_channel_selects_master);
    RUN_TEST(test_tag_val_channel_miss);

    RUN_TEST(test_subscribe_response_ok);
    RUN_TEST(test_subscribe_response_missing_timeout_defaults_300);
    RUN_TEST(test_subscribe_response_infinite_timeout_defaults_300);
    RUN_TEST(test_subscribe_response_412);
    RUN_TEST(test_subscribe_response_case_insensitive_headers);
    RUN_TEST(test_subscribe_response_huge_lease_clamped);

    RUN_TEST(test_notify_head_ok);
    RUN_TEST(test_notify_head_rejects_non_notify);
    RUN_TEST(test_notify_head_rejects_missing_sid);
    RUN_TEST(test_notify_head_chunked);

    RUN_TEST(test_avtransport_event_full);
    RUN_TEST(test_avtransport_event_transport_only);
    RUN_TEST(test_avtransport_event_not_implemented_metadata);
    RUN_TEST(test_avtransport_event_malformed);

    RUN_TEST(test_rendering_control_event);
    RUN_TEST(test_rendering_control_volume_clamped);

    RUN_TEST(test_zone_group_state_extraction);
    RUN_TEST(test_zone_group_state_missing);

    RUN_TEST(test_renew_at_half_lease);
    RUN_TEST(test_renew_at_wraps_safely);

    return UNITY_END();
}
