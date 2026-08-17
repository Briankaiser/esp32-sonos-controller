#include "lrc.h"
#include <algorithm>

namespace lyrics {

// ── Small char helpers (avoid <cctype> locale surprises) ──────────────────────

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

// Parse a run of ASCII digits [from, to) as an unsigned value. Returns false if
// the range is empty or holds any non-digit.
static bool parse_uint(const String& s, int from, int to, uint32_t& out) {
    if (from >= to) return false;
    uint32_t v = 0;
    for (int i = from; i < to; ++i) {
        if (!is_digit(s[i])) return false;
        v = v * 10u + (uint32_t)(s[i] - '0');
    }
    out = v;
    return true;
}

// Interpret the content between '[' and ']' as a timestamp mm:ss(.xx|.xxx).
// Returns true and sets ms on success; false if it is not a timestamp (e.g. an
// ID tag like ti: or length:). mm and ss must be all-digit; the fractional part
// is optional and read as centiseconds (2 digits) or milliseconds (3 digits).
static bool parse_timestamp(const String& body, uint32_t& ms) {
    int colon = body.indexOf(':');
    if (colon <= 0) return false;

    uint32_t minutes = 0;
    if (!parse_uint(body, 0, colon, minutes)) return false;  // rejects ID tags (letters)

    int dot = body.indexOf('.', colon + 1);
    int sec_end = (dot >= 0) ? dot : (int)body.length();
    uint32_t seconds = 0;
    if (!parse_uint(body, colon + 1, sec_end, seconds)) return false;

    uint32_t frac_ms = 0;
    if (dot >= 0) {
        int frac_from = dot + 1;
        int frac_to   = (int)body.length();
        // Normalize the fraction to milliseconds: pad/truncate to 3 digits.
        uint32_t frac = 0;
        if (!parse_uint(body, frac_from, frac_to, frac)) return false;
        int digits = frac_to - frac_from;
        if (digits == 1) frac_ms = frac * 100u;
        else if (digits == 2) frac_ms = frac * 10u;
        else if (digits == 3) frac_ms = frac;
        else {
            // >3 digits: keep the leading 3 (millisecond precision).
            uint32_t keep = 0;
            parse_uint(body, frac_from, frac_from + 3, keep);
            frac_ms = keep;
        }
    }

    ms = minutes * 60000u + seconds * 1000u + frac_ms;
    return true;
}

// Strip a single trailing '\r' (CRLF line endings) in place-ish via substring.
static String rstrip_cr(const String& line) {
    int n = (int)line.length();
    if (n > 0 && line[n - 1] == '\r') return line.substring(0, n - 1);
    return line;
}

LyricsDoc parse_lrc(const String& lrc) {
    LyricsDoc doc;

    int start = 0;
    const int total = (int)lrc.length();
    while (start <= total) {
        int nl = lrc.indexOf('\n', start);
        int end = (nl < 0) ? total : nl;
        String line = rstrip_cr(lrc.substring(start, end));
        start = (nl < 0) ? total + 1 : nl + 1;

        // Collect all leading [..] tags; classify each as timestamp or ID tag.
        std::vector<uint32_t> stamps;
        int cursor = 0;
        for (;;) {
            // Skip nothing — timestamps must be at the current cursor to count as
            // "leading". A '[' not at the cursor means lyric text has begun.
            if (cursor >= (int)line.length() || line[cursor] != '[') break;
            int close = line.indexOf(']', cursor + 1);
            if (close < 0) break;  // unbalanced — treat the rest as text
            String body = line.substring(cursor + 1, close);
            uint32_t ms = 0;
            if (parse_timestamp(body, ms)) {
                stamps.push_back(ms);
            } else {
                // ID tag. Capture [length:mm:ss] for the duration hint; ignore others.
                int c = body.indexOf(':');
                if (c > 0) {
                    String key = body.substring(0, c);
                    if (key == "length") {
                        uint32_t len_ms = 0;
                        if (parse_timestamp(body.substring(c + 1), len_ms))
                            doc.length_ms = len_ms;
                    }
                }
            }
            cursor = close + 1;
        }

        String text = line.substring(cursor);
        text.trim();

        if (!stamps.empty()) {
            doc.synced = true;
            for (uint32_t ms : stamps) {
                LyricLine ln;
                ln.ms = ms;
                ln.text = text;   // may be empty (an instrumental gap marker)
                doc.lines.push_back(ln);
            }
        } else if (text.length() > 0) {
            // No timestamp on this line. Keep it as a plain (unsynced) line; if
            // the whole doc turns out to have no timestamps it stays unsynced.
            LyricLine ln;
            ln.ms = 0;
            ln.text = text;
            doc.lines.push_back(ln);
        }
    }

    // If any line was timestamped, this is a synced doc: drop the plain (ms==0,
    // untimed) carry-overs that were really ID-tag noise, then sort by time.
    if (doc.synced) {
        std::vector<LyricLine> timed;
        timed.reserve(doc.lines.size());
        for (const LyricLine& ln : doc.lines) {
            // A genuine [00:00.00] line keeps ms==0 but came from a stamp; we
            // cannot distinguish it from a stray plain line here, so we only
            // drop ms==0 lines that have no text (pure noise). Timed zero-ms
            // lines with text are legitimate opening lines and are retained.
            timed.push_back(ln);
        }
        std::stable_sort(timed.begin(), timed.end(),
            [](const LyricLine& a, const LyricLine& b) { return a.ms < b.ms; });
        doc.lines.swap(timed);
    }

    return doc;
}

int current_line_index(const LyricsDoc& doc, uint32_t position_ms) {
    if (!doc.synced || doc.lines.empty()) return -1;
    if (position_ms < doc.lines[0].ms) return -1;

    int idx = -1;
    for (int i = 0; i < (int)doc.lines.size(); ++i) {
        if (doc.lines[i].ms <= position_ms) idx = i;
        else break;  // sorted ascending — no later line can match
    }
    return idx;
}

} // namespace lyrics
