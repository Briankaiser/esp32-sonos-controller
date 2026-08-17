#pragma once
#include <Arduino.h>
#include <vector>
#include <cstdint>

// LRC lyrics parsing — pure logic, no networking, no FreeRTOS, no LVGL, so it
// compiles and is unit-tested under test_native (mirrors app/sonos/xml_util).
//
// LRC is a plain-text format: optional ID tags like [ti:Title] / [length:mm:ss],
// then lyric lines each prefixed by one or more timestamps [mm:ss.xx]. A line
// may repeat at several timestamps ([t1][t2]text). "Plain" lyrics carry no
// timestamps at all — every line is text with no bracket prefix.
namespace lyrics {

struct LyricLine {
    uint32_t ms   = 0;   // milliseconds from track start; 0 for plain (unsynced) lines
    String   text;       // the lyric text with all leading timestamp tags stripped
};

struct LyricsDoc {
    std::vector<LyricLine> lines;        // ascending by ms when synced; file order when plain
    bool                   synced    = false;  // true iff ≥1 line carried a [mm:ss.xx] timestamp
    uint32_t               length_ms = 0;      // from a [length:mm:ss] tag if present, else 0
};

// Parse LRC (synced) or plain lyric text. Never throws; malformed or empty
// input yields an empty doc (lines empty, synced false). Lines carrying
// multiple timestamps are emitted once per timestamp. When any timestamp is
// present the result is marked synced and sorted ascending by timestamp
// (stable — equal timestamps keep source order).
LyricsDoc parse_lrc(const String& lrc);

// Index into doc.lines of the line active at position_ms: the last line whose
// ms <= position_ms. Returns -1 when the doc is empty, is not synced, or the
// position precedes the first line's timestamp.
int current_line_index(const LyricsDoc& doc, uint32_t position_ms);

} // namespace lyrics
