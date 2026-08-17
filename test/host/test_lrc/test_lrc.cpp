/* LRC lyrics parser unit tests — run on the host with no hardware.
 *
 * Exercises the pure-logic parser that turns LRC (or plain) lyric text into a
 * sorted, timestamped line list, plus the current-line lookup that the screen
 * uses to drive the highlight. Malformed input must never crash and must yield
 * an empty doc; plain (untimed) lyrics must be classified unsynced so the
 * screen suppresses the moving highlight.
 */

#include <unity.h>
#include "app/lyrics/lrc.h"

using namespace lyrics;

void setUp()    {}
void tearDown() {}

// ── well-formed synced LRC ────────────────────────────────────────────────────

void test_synced_basic_parse() {
    LyricsDoc d = parse_lrc(
        "[ti:Song]\n[ar:Artist]\n"
        "[00:12.34]First line\n"
        "[00:15.00]Second line\n"
        "[01:00.50]Third line\n");
    TEST_ASSERT_TRUE(d.synced);
    TEST_ASSERT_EQUAL_UINT32(3, d.lines.size());
    TEST_ASSERT_EQUAL_UINT32(12340, d.lines[0].ms);
    TEST_ASSERT_EQUAL_STRING("First line", d.lines[0].text.c_str());
    TEST_ASSERT_EQUAL_UINT32(15000, d.lines[1].ms);
    TEST_ASSERT_EQUAL_UINT32(60500, d.lines[2].ms);
    TEST_ASSERT_EQUAL_STRING("Third line", d.lines[2].text.c_str());
}

void test_synced_millisecond_fraction() {
    // 3-digit fraction is milliseconds; 2-digit is centiseconds.
    LyricsDoc d = parse_lrc("[00:01.5]a\n[00:02.05]b\n[00:03.005]c\n");
    TEST_ASSERT_EQUAL_UINT32(1500, d.lines[0].ms);   // .5  → 500ms
    TEST_ASSERT_EQUAL_UINT32(2050, d.lines[1].ms);   // .05 → 50ms
    TEST_ASSERT_EQUAL_UINT32(3005, d.lines[2].ms);   // .005 → 5ms
}

void test_synced_no_fraction() {
    LyricsDoc d = parse_lrc("[02:07]seven past two\n");
    TEST_ASSERT_TRUE(d.synced);
    TEST_ASSERT_EQUAL_UINT32(127000, d.lines[0].ms);
}

void test_length_tag_captured() {
    LyricsDoc d = parse_lrc("[length:03:20]\n[00:01.00]x\n");
    TEST_ASSERT_EQUAL_UINT32(200000, d.length_ms);
    TEST_ASSERT_EQUAL_UINT32(1, d.lines.size());
}

// ── plain (unsynced) lyrics ───────────────────────────────────────────────────

void test_plain_lyrics_classified_unsynced() {
    LyricsDoc d = parse_lrc("First line\nSecond line\nThird line\n");
    TEST_ASSERT_FALSE(d.synced);
    TEST_ASSERT_EQUAL_UINT32(3, d.lines.size());
    TEST_ASSERT_EQUAL_STRING("First line", d.lines[0].text.c_str());
    TEST_ASSERT_EQUAL_STRING("Third line", d.lines[2].text.c_str());
    // current-line lookup must refuse to drive a highlight on unsynced lyrics
    TEST_ASSERT_EQUAL_INT(-1, current_line_index(d, 5000));
}

// ── malformed / garbage input ─────────────────────────────────────────────────

void test_empty_input_empty_doc() {
    LyricsDoc d = parse_lrc("");
    TEST_ASSERT_FALSE(d.synced);
    TEST_ASSERT_EQUAL_UINT32(0, d.lines.size());
    TEST_ASSERT_EQUAL_INT(-1, current_line_index(d, 0));
}

void test_garbage_does_not_crash() {
    // Unbalanced brackets, stray colons, non-numeric stamps — must not crash,
    // and produces at most plain text lines, never a synced doc.
    LyricsDoc d = parse_lrc("[[[\n]]]\n[garbage]text\n[99:\n:59]\n[ti:only tags]\n");
    TEST_ASSERT_FALSE(d.synced);
    // No assertion on exact contents beyond "did not crash and stayed unsynced".
    (void)d;
}

void test_id_tags_only_yield_no_lines() {
    LyricsDoc d = parse_lrc("[ti:Title]\n[ar:Artist]\n[al:Album]\n[by:x]\n");
    TEST_ASSERT_FALSE(d.synced);
    TEST_ASSERT_EQUAL_UINT32(0, d.lines.size());
}

// ── out-of-order timestamps get sorted ────────────────────────────────────────

void test_out_of_order_sorted() {
    LyricsDoc d = parse_lrc(
        "[00:30.00]late\n"
        "[00:05.00]early\n"
        "[00:15.00]middle\n");
    TEST_ASSERT_TRUE(d.synced);
    TEST_ASSERT_EQUAL_STRING("early",  d.lines[0].text.c_str());
    TEST_ASSERT_EQUAL_STRING("middle", d.lines[1].text.c_str());
    TEST_ASSERT_EQUAL_STRING("late",   d.lines[2].text.c_str());
}

void test_repeated_timestamps_expand() {
    // A line prefixed with two timestamps is emitted at both times.
    LyricsDoc d = parse_lrc("[00:10.00][00:40.00]chorus\n");
    TEST_ASSERT_EQUAL_UINT32(2, d.lines.size());
    TEST_ASSERT_EQUAL_UINT32(10000, d.lines[0].ms);
    TEST_ASSERT_EQUAL_UINT32(40000, d.lines[1].ms);
    TEST_ASSERT_EQUAL_STRING("chorus", d.lines[0].text.c_str());
    TEST_ASSERT_EQUAL_STRING("chorus", d.lines[1].text.c_str());
}

// ── current-line lookup ───────────────────────────────────────────────────────

void test_current_line_lookup() {
    LyricsDoc d = parse_lrc(
        "[00:10.00]one\n"
        "[00:20.00]two\n"
        "[00:30.00]three\n");
    TEST_ASSERT_EQUAL_INT(-1, current_line_index(d, 0));       // before first line
    TEST_ASSERT_EQUAL_INT(-1, current_line_index(d, 9999));    // still before 10.00
    TEST_ASSERT_EQUAL_INT(0,  current_line_index(d, 10000));   // exactly on line 0
    TEST_ASSERT_EQUAL_INT(0,  current_line_index(d, 19999));   // within line 0
    TEST_ASSERT_EQUAL_INT(1,  current_line_index(d, 20000));   // on line 1
    TEST_ASSERT_EQUAL_INT(2,  current_line_index(d, 999999));  // past last → last line
}

// ── runner ────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_synced_basic_parse);
    RUN_TEST(test_synced_millisecond_fraction);
    RUN_TEST(test_synced_no_fraction);
    RUN_TEST(test_length_tag_captured);

    RUN_TEST(test_plain_lyrics_classified_unsynced);

    RUN_TEST(test_empty_input_empty_doc);
    RUN_TEST(test_garbage_does_not_crash);
    RUN_TEST(test_id_tags_only_yield_no_lines);

    RUN_TEST(test_out_of_order_sorted);
    RUN_TEST(test_repeated_timestamps_expand);

    RUN_TEST(test_current_line_lookup);

    return UNITY_END();
}
