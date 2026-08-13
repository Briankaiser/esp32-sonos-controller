#pragma once
#include <Arduino.h>

// Shared XML/string helpers for Sonos SOAP responses and GENA event payloads.
// Pure logic — no networking, no FreeRTOS — so it compiles under test_native.
namespace sonos::xml {

// Returns the text between the first <tag>…</tag> pair; empty on miss.
String field(const String& xml, const String& tag);

// Returns the value of attr="…" inside an element string; empty on miss.
String attr(const String& element, const String& attr_name);

// Decodes common HTML entities and normalizes Unicode smart quotes to ASCII
// (the Montserrat font lacks U+2018/U+2019). &amp; is decoded last.
String html_decode(const String& s);

// Converts "h:mm:ss" to total seconds. Returns 0 on parse failure.
uint32_t parse_time(const String& hmmss);

// Finds the first <tag …/> or <tag …> element and returns its val="…"
// attribute. GENA LastChange payloads encode state as attribute values,
// e.g. <TransportState val="PLAYING"/>. Empty on miss.
String tag_val(const String& xml, const String& tag);

// Like tag_val, but only matches an element whose channel="…" attribute
// equals `channel`. RenderingControl events carry per-channel values
// (<Volume channel="Master" val="27"/> alongside LF/RF entries).
// Returns the val attribute; empty on miss. `found` reports the miss/hit.
String tag_val_channel(const String& xml, const String& tag,
                       const String& channel, bool& found);

} // namespace sonos::xml
