/* LRCLIB request/response helper tests — host, no hardware.
 *
 * Pins the URL-encoding, JSON field extraction (with escape handling), and the
 * duration trust gate that decides whether a match may drive a synced highlight.
 */

#include <unity.h>
#include "app/lyrics/lyrics_api.h"

using namespace lyrics;

void setUp()    {}
void tearDown() {}

// ── url_encode ────────────────────────────────────────────────────────────────

void test_url_encode_spaces_and_specials() {
    TEST_ASSERT_EQUAL_STRING("Foo%20Bar", url_encode("Foo Bar").c_str());
    TEST_ASSERT_EQUAL_STRING("R%26B", url_encode("R&B").c_str());
    TEST_ASSERT_EQUAL_STRING("a%2Fb%3Fc%3Dd", url_encode("a/b?c=d").c_str());
    // unreserved pass through untouched
    TEST_ASSERT_EQUAL_STRING("Aa0-_.~", url_encode("Aa0-_.~").c_str());
}

void test_url_encode_utf8_bytes() {
    // "é" is 0xC3 0xA9 in UTF-8 → each byte percent-encoded
    TEST_ASSERT_EQUAL_STRING("%C3%A9", url_encode("\xC3\xA9").c_str());
}

// ── build_lrclib_path ─────────────────────────────────────────────────────────

void test_build_path_full() {
    String p = build_lrclib_path("Daft Punk", "One More Time", "Discovery", 320);
    TEST_ASSERT_EQUAL_STRING(
        "/api/get?artist_name=Daft%20Punk&track_name=One%20More%20Time"
        "&album_name=Discovery&duration=320", p.c_str());
}

void test_build_path_omits_empty_album_and_zero_duration() {
    String p = build_lrclib_path("A", "B", "", 0);
    TEST_ASSERT_EQUAL_STRING("/api/get?artist_name=A&track_name=B", p.c_str());
}

// ── json_extract_string ───────────────────────────────────────────────────────

void test_json_string_basic() {
    bool found = false;
    String v = json_extract_string("{\"artistName\":\"Queen\",\"id\":7}", "artistName", found);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("Queen", v.c_str());
}

void test_json_string_unescapes_newlines_and_quotes() {
    bool found = false;
    // syncedLyrics arrives with escaped newlines and quotes
    String body = "{\"syncedLyrics\":\"[00:01.00]line one\\n[00:02.00]she said \\\"hi\\\"\\n\"}";
    String v = json_extract_string(body, "syncedLyrics", found);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("[00:01.00]line one\n[00:02.00]she said \"hi\"\n", v.c_str());
}

void test_json_string_unicode_escape() {
    bool found = false;
    // \u00e9 → é (UTF-8 0xC3 0xA9)
    String v = json_extract_string("{\"t\":\"caf\\u00e9\"}", "t", found);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("caf\xC3\xA9", v.c_str());
}

void test_json_string_missing_key() {
    bool found = true;
    String v = json_extract_string("{\"a\":\"x\"}", "syncedLyrics", found);
    TEST_ASSERT_FALSE(found);
    TEST_ASSERT_EQUAL_STRING("", v.c_str());
}

void test_json_string_null_value_is_absent() {
    // LRCLIB returns "syncedLyrics":null for plain-only tracks
    bool found = true;
    String v = json_extract_string("{\"syncedLyrics\":null,\"plainLyrics\":\"la\"}",
                                   "syncedLyrics", found);
    TEST_ASSERT_FALSE(found);
    TEST_ASSERT_EQUAL_STRING("", v.c_str());
}

// ── json_extract_number ───────────────────────────────────────────────────────

void test_json_number_integer_and_float() {
    uint32_t out = 0;
    TEST_ASSERT_TRUE(json_extract_number("{\"duration\":233}", "duration", out));
    TEST_ASSERT_EQUAL_UINT32(233, out);
    TEST_ASSERT_TRUE(json_extract_number("{\"duration\":233.0}", "duration", out));
    TEST_ASSERT_EQUAL_UINT32(233, out);   // integer part only
}

void test_json_number_missing() {
    uint32_t out = 999;
    TEST_ASSERT_FALSE(json_extract_number("{\"a\":1}", "duration", out));
}

// ── duration_trusted ──────────────────────────────────────────────────────────

void test_duration_trust_gate() {
    TEST_ASSERT_TRUE(duration_trusted(233, 233, 2));    // exact
    TEST_ASSERT_TRUE(duration_trusted(233, 234, 2));    // within tolerance
    TEST_ASSERT_TRUE(duration_trusted(235, 233, 2));    // within, other direction
    TEST_ASSERT_FALSE(duration_trusted(233, 240, 2));   // remaster, longer
    TEST_ASSERT_FALSE(duration_trusted(0, 233, 2));     // unknown matched
    TEST_ASSERT_FALSE(duration_trusted(233, 0, 2));     // unknown playing
}

// ── runner ────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_url_encode_spaces_and_specials);
    RUN_TEST(test_url_encode_utf8_bytes);

    RUN_TEST(test_build_path_full);
    RUN_TEST(test_build_path_omits_empty_album_and_zero_duration);

    RUN_TEST(test_json_string_basic);
    RUN_TEST(test_json_string_unescapes_newlines_and_quotes);
    RUN_TEST(test_json_string_unicode_escape);
    RUN_TEST(test_json_string_missing_key);
    RUN_TEST(test_json_string_null_value_is_absent);

    RUN_TEST(test_json_number_integer_and_float);
    RUN_TEST(test_json_number_missing);

    RUN_TEST(test_duration_trust_gate);

    return UNITY_END();
}
