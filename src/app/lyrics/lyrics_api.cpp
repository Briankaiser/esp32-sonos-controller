#include "lyrics_api.h"

namespace lyrics {

static inline bool is_unreserved(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

static char hex_digit(int v) { return (char)(v < 10 ? ('0' + v) : ('A' + v - 10)); }

String url_encode(const String& in) {
    String out;
    for (int i = 0; i < (int)in.length(); ++i) {
        char c = in[i];
        if (is_unreserved(c)) {
            out += c;
        } else {
            unsigned char u = (unsigned char)c;
            out += '%';
            out += hex_digit((u >> 4) & 0xF);
            out += hex_digit(u & 0xF);
        }
    }
    return out;
}

String build_lrclib_path(const String& artist, const String& title,
                         const String& album, uint32_t duration_s) {
    String path = "/api/get?artist_name=" + url_encode(artist)
                + "&track_name=" + url_encode(title);
    if (album.length() > 0) path += "&album_name=" + url_encode(album);
    if (duration_s > 0)     path += "&duration=" + String(duration_s);
    return path;
}

// ── JSON field helpers ────────────────────────────────────────────────────────

// Locate the value start (index just past the ':') for a top-level "key".
// Returns -1 if the key is absent. Naive but adequate for LRCLIB's flat object;
// matches the quoted key followed by optional whitespace and a colon.
static int find_value_start(const String& body, const String& key) {
    String needle = "\"" + key + "\"";
    int k = body.indexOf(needle);
    if (k < 0) return -1;
    int i = k + (int)needle.length();
    // skip whitespace before ':'
    while (i < (int)body.length() && (body[i] == ' ' || body[i] == '\t' ||
                                      body[i] == '\r' || body[i] == '\n')) ++i;
    if (i >= (int)body.length() || body[i] != ':') return -1;
    ++i;
    while (i < (int)body.length() && (body[i] == ' ' || body[i] == '\t' ||
                                      body[i] == '\r' || body[i] == '\n')) ++i;
    return i;
}

static void append_utf8(String& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out += (char)cp;
    } else if (cp <= 0x7FF) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

String json_extract_string(const String& body, const String& key, bool& found) {
    found = false;
    int v = find_value_start(body, key);
    if (v < 0) return String();
    if (body[v] != '"') return String();  // null / number / not a string → treat as absent
    found = true;

    String out;
    int i = v + 1;
    const int n = (int)body.length();
    while (i < n) {
        char c = body[i];
        if (c == '"') break;                 // closing quote
        if (c == '\\' && i + 1 < n) {
            char e = body[i + 1];
            switch (e) {
                case 'n': out += '\n'; i += 2; break;
                case 'r': out += '\r'; i += 2; break;
                case 't': out += '\t'; i += 2; break;
                case 'b': out += '\b'; i += 2; break;
                case 'f': out += '\f'; i += 2; break;
                case '"': out += '"';  i += 2; break;
                case '\\': out += '\\'; i += 2; break;
                case '/': out += '/';  i += 2; break;
                case 'u': {
                    if (i + 5 < n) {
                        int h3 = hex_val(body[i + 2]), h2 = hex_val(body[i + 3]);
                        int h1 = hex_val(body[i + 4]), h0 = hex_val(body[i + 5]);
                        if (h3 >= 0 && h2 >= 0 && h1 >= 0 && h0 >= 0) {
                            uint32_t cp = (uint32_t)((h3 << 12) | (h2 << 8) | (h1 << 4) | h0);
                            append_utf8(out, cp);
                            i += 6;
                        } else { out += e; i += 2; }
                    } else { out += e; i += 2; }
                    break;
                }
                default: out += e; i += 2; break;
            }
        } else {
            out += c;
            ++i;
        }
    }
    return out;
}

bool json_extract_number(const String& body, const String& key, uint32_t& out) {
    int v = find_value_start(body, key);
    if (v < 0) return false;
    int i = v;
    const int n = (int)body.length();
    if (i < n && body[i] == '-') return false;   // negative durations are nonsense
    uint32_t val = 0;
    bool any = false;
    while (i < n && body[i] >= '0' && body[i] <= '9') {   // integer part only
        val = val * 10u + (uint32_t)(body[i] - '0');
        any = true;
        ++i;
    }
    if (!any) return false;
    out = val;
    return true;
}

bool duration_trusted(uint32_t matched_s, uint32_t playing_s, uint32_t tolerance_s) {
    if (matched_s == 0 || playing_s == 0) return false;
    uint32_t diff = (matched_s > playing_s) ? (matched_s - playing_s)
                                            : (playing_s - matched_s);
    return diff <= tolerance_s;
}

} // namespace lyrics
