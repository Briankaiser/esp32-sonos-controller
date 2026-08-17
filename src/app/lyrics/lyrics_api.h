#pragma once
#include <Arduino.h>
#include <cstdint>

// LRCLIB request/response helpers — pure logic (no networking), so the tricky
// URL-encoding, JSON field extraction, and duration trust-gate are unit-tested
// under test_native. The device service (service.cpp) does the actual TLS I/O
// and calls these to build the request and pick apart the response.
namespace lyrics {

// Percent-encode a query-parameter value (RFC 3986 unreserved chars pass
// through; everything else becomes %XX, spaces included — never '+').
String url_encode(const String& in);

// Build the LRCLIB GET path + query for the given track. album may be empty.
// duration_s of 0 omits the duration parameter. Example:
//   /api/get?artist_name=Foo&track_name=Bar&album_name=Baz&duration=233
String build_lrclib_path(const String& artist, const String& title,
                         const String& album, uint32_t duration_s);

// Extract the string value of a top-level JSON key, JSON-unescaping the
// contents (\n \r \t \" \\ \/ and \uXXXX for the BMP). `found` distinguishes a
// genuine empty string from a missing/null key. Returns "" when missing or null.
String json_extract_string(const String& body, const String& key, bool& found);

// Extract the integer part of a numeric JSON key value (handles "duration":233
// and "duration":233.0). Returns false when the key is missing or non-numeric.
bool json_extract_number(const String& body, const String& key, uint32_t& out);

// Duration trust gate: is a lyrics match whose track length is matched_s
// trustworthy enough to drive a real-time highlight against a track playing at
// playing_s? True only when both are non-zero and within tolerance_s seconds.
bool duration_trusted(uint32_t matched_s, uint32_t playing_s, uint32_t tolerance_s);

} // namespace lyrics
