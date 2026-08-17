#include "service.h"
#include "lyrics_api.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace lyrics {

// ── Tunables ──────────────────────────────────────────────────────────────────

static constexpr char     kHost[]            = "lrclib.net";
static constexpr uint16_t kPort              = 443;
static constexpr size_t   kLyricsBufBytes    = 48u * 1024u;  // PSRAM; synced LRC is a few KB
static constexpr uint32_t kFetchTimeoutMs    = 8000;         // bounded — best-effort feature
static constexpr uint32_t kFetchStallMs      = 4000;         // bail if no read progress
static constexpr uint32_t kRetryBackoffMs    = 20000;        // don't hammer after a transient miss
static constexpr uint32_t kDurationToleranceS = 3;           // trust gate window
static constexpr uint32_t kTaskStackBytes    = 12u * 1024u;

// ── Statics (lyrics task + mutex-guarded published state) ─────────────────────

static char*             s_buf              = nullptr;   // PSRAM response buffer
static SemaphoreHandle_t s_mutex            = nullptr;
static LyricsState       s_state;
static TaskHandle_t      s_task_handle      = nullptr;

static String            s_last_fail_key;                // key whose last attempt failed transiently
static uint32_t          s_last_fail_ms     = 0;

// ── Deadline / line helpers (same shape as art.cpp) ───────────────────────────

static inline bool time_before(uint32_t deadline) {
    return (int32_t)(millis() - deadline) < 0;
}

static String read_line(Client& client, uint32_t deadline) {
    String line;
    while (time_before(deadline)) {
        if (client.available()) {
            char c = (char)client.read();
            if (c == '\n') break;
            if (c != '\r') line += c;
        } else if (client.connected()) {
            delay(1);
        } else {
            break;
        }
    }
    return line;
}

// ── Publish helpers ───────────────────────────────────────────────────────────

static void publish(const LyricsState& next) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint32_t v = s_state.version + 1;
    s_state = next;
    s_state.version = v;
    xSemaphoreGive(s_mutex);
}

// Publish an empty state so any previous track's lyrics vanish immediately when
// the track changes, before the (slow) fetch even starts. track_key is left
// empty on purpose: it marks "unresolved", so the poll-side trigger keeps
// notifying until a definitive result is published — which is what makes the
// transient-failure backoff below actually reachable.
static void publish_cleared() {
    LyricsState cleared;
    cleared.available = false;
    cleared.synced    = false;
    cleared.track_key = String();
    publish(cleared);
}

// ── HTTP body fetch into s_buf. Returns bytes read; sets *http_status. ────────
// A negative *http_status means the request never got a status line (transient
// network failure — connect refused, TLS error, timeout before headers).

static size_t fetch_body(const String& path, int* http_status) {
    *http_status = -1;
    if (!s_buf) return 0;

    WiFiClientSecure client;
    client.setInsecure();          // public lyrics text; no CA bundle shipped (as with art)
    client.setTimeout(kFetchTimeoutMs);   // Stream read timeout, in ms (matches art.cpp)
    uint32_t deadline = millis() + kFetchTimeoutMs;

    if (!client.connect(kHost, kPort)) {
        Serial.printf("[Lyrics] connect failed %s:%u\n", kHost, (unsigned)kPort);
        return 0;
    }

    client.printf(
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: OpenSonosController (https://github.com/esp32-sonos-controller)\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n\r\n",
        path.c_str(), kHost);

    // Status line
    {
        String status = read_line(client, deadline);
        int sp = status.indexOf(' ');
        if (sp >= 0) *http_status = (int)strtol(status.c_str() + sp + 1, nullptr, 10);
        if (*http_status < 200 || *http_status >= 300) {
            client.stop();
            return 0;   // 404 (not found) and any error: definitive "no lyrics"
        }
    }

    // Headers
    bool is_chunked = false;
    long content_length = -1;
    while (client.connected() && time_before(deadline)) {
        String line = read_line(client, deadline);
        if (line.isEmpty()) break;
        String lower = line; lower.toLowerCase();
        if (lower.indexOf("chunked") >= 0) is_chunked = true;
        if (lower.startsWith("content-length:"))
            content_length = strtol(line.substring(line.indexOf(':') + 1).c_str(), nullptr, 10);
    }

    size_t   fetched       = 0;
    uint32_t last_progress = millis();

    if (is_chunked) {
        while ((client.connected() || client.available()) && time_before(deadline)) {
            String size_line = read_line(client, deadline);
            if (size_line.isEmpty()) {
                if (millis() - last_progress > kFetchStallMs) break;
                continue;
            }
            uint32_t chunk_size = strtoul(size_line.c_str(), nullptr, 16);
            if (chunk_size == 0) break;
            for (uint32_t i = 0; i < chunk_size && time_before(deadline); ) {
                int av = client.available();
                if (av > 0 && fetched < kLyricsBufBytes - 1) {
                    size_t to_read = (size_t)av;
                    if (to_read > chunk_size - i)            to_read = chunk_size - i;
                    if (to_read > kLyricsBufBytes - 1 - fetched)
                        to_read = kLyricsBufBytes - 1 - fetched;
                    size_t got = (size_t)client.readBytes(s_buf + fetched, to_read);
                    fetched += got; i += (uint32_t)got;
                    if (got > 0) last_progress = millis();
                } else if (fetched >= kLyricsBufBytes - 1) {
                    break;
                } else if (client.connected()) {
                    if (millis() - last_progress > kFetchStallMs) goto done;
                    delay(1);
                } else {
                    break;
                }
            }
            read_line(client, deadline);  // trailing CRLF
            if (fetched >= kLyricsBufBytes - 1) break;
        }
    } else {
        while ((client.connected() || client.available()) && time_before(deadline)) {
            int av = client.available();
            if (av > 0 && fetched < kLyricsBufBytes - 1) {
                size_t to_read = (size_t)av;
                if (to_read > kLyricsBufBytes - 1 - fetched) to_read = kLyricsBufBytes - 1 - fetched;
                size_t got = (size_t)client.readBytes(s_buf + fetched, to_read);
                fetched += got;
                if (got > 0) last_progress = millis();
            }
            if (fetched >= kLyricsBufBytes - 1) break;
            if (content_length >= 0 && (long)fetched >= content_length) break;
            if (millis() - last_progress > kFetchStallMs) break;
            delay(1);
        }
    }
done:
    client.stop();
    s_buf[fetched] = '\0';
    return fetched;
}

// ── One fetch-and-publish cycle for the current track ─────────────────────────

static void handle_current_track() {
    sonos::PlaybackState pb = sonos::playback::snapshot();
    String key = service::track_key(pb);

    if (key.isEmpty()) { return; }             // no track metadata yet — nothing to do

    // Dedup on what is *currently published*, not on an ever-resolved key: we
    // keep only one published doc, so if the published key already matches this
    // track we have its definitive result (lyrics or a cached negative) and can
    // skip. A published key of "" means unresolved/pending.
    String pub = service::published_track_key();
    if (key == pub) return;

    // Backoff after a transient miss so we don't hammer LRCLIB during an outage.
    if (key == s_last_fail_key && (millis() - s_last_fail_ms) < kRetryBackoffMs) return;

    // Showing another track: clear its lyrics immediately so they never linger.
    if (pub != String()) publish_cleared();

    String path = build_lrclib_path(pb.artist, pb.title, pb.album, pb.duration_s);
    int http_status = -1;
    size_t n = fetch_body(path, &http_status);

    // Re-check the track hasn't changed under us during the fetch. If it has, a
    // pending notification will re-run this for the new track — discard now.
    {
        sonos::PlaybackState now = sonos::playback::snapshot();
        if (service::track_key(now) != key) return;
    }

    if (http_status < 0) {
        // Transient network failure: no definitive answer. Leave the cleared
        // state (no lyrics shown) and allow a backed-off retry.
        s_last_fail_key = key;
        s_last_fail_ms  = millis();
        return;
    }

    // Definitive answer (2xx with a body, or an error/404 with none). Publishing
    // with track_key=key below marks this track resolved so future polls dedup.
    s_last_fail_key = String();

    LyricsState next;
    next.track_key = key;
    next.available = false;
    next.synced    = false;

    if (n > 0) {
        String body(s_buf);
        bool found = false;
        String synced_text = json_extract_string(body, "syncedLyrics", found);
        if (found && synced_text.length() > 0) {
            uint32_t matched_s = 0;
            json_extract_number(body, "duration", matched_s);
            next.doc = parse_lrc(synced_text);
            if (!next.doc.lines.empty()) {
                next.available = true;
                // Drive the moving highlight only when the timing is trustworthy.
                next.synced = next.doc.synced &&
                              duration_trusted(matched_s, pb.duration_s, kDurationToleranceS);
            }
        }
        if (!next.available) {
            // No usable synced lyrics — fall back to plain text if present.
            bool pfound = false;
            String plain = json_extract_string(body, "plainLyrics", pfound);
            if (pfound && plain.length() > 0) {
                next.doc = parse_lrc(plain);   // no timestamps → doc.synced == false
                if (!next.doc.lines.empty()) {
                    next.available = true;
                    next.synced    = false;    // static only
                }
            }
        }
    }

    Serial.printf("[Lyrics] %s: %s (%u lines, http %d)\n",
                  key.c_str(),
                  next.available ? (next.synced ? "synced" : "static") : "none",
                  (unsigned)next.doc.lines.size(), http_status);
    publish(next);
}

// ── Task body ─────────────────────────────────────────────────────────────────

static void lyrics_task_body(void* /*arg*/) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        handle_current_track();
    }
}

// ── Public interface ──────────────────────────────────────────────────────────

void service::init() {
    s_buf   = static_cast<char*>(
        heap_caps_malloc(kLyricsBufBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    s_mutex = xSemaphoreCreateMutex();
    if (!s_buf) Serial.printf("[Lyrics] PSRAM alloc failed (%u bytes)\n",
                              (unsigned)kLyricsBufBytes);
    else        Serial.printf("[Lyrics] PSRAM buffer: %u bytes\n",
                              (unsigned)kLyricsBufBytes);
}

void service::start_task() {
    xTaskCreatePinnedToCore(lyrics_task_body, "Lyrics",
                            kTaskStackBytes, nullptr,
                            1,                    // priority 1 < sonos_task priority 3
                            &s_task_handle, 0);   // core 0
}

void service::notify_new_track() {
    if (s_task_handle) xTaskNotifyGive(s_task_handle);
}

LyricsState service::snapshot() {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    LyricsState copy = s_state;
    xSemaphoreGive(s_mutex);
    return copy;
}

uint32_t service::version() {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint32_t v = s_state.version;
    xSemaphoreGive(s_mutex);
    return v;
}

String service::published_track_key() {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    String k = s_state.track_key;
    xSemaphoreGive(s_mutex);
    return k;
}

String service::track_key(const sonos::PlaybackState& pb) {
    if (pb.title.isEmpty()) return String();
    // Unit-separator joins so no field's contents can spoof a boundary.
    String k = pb.artist; k += '\x1f';
    k += pb.title;        k += '\x1f';
    k += pb.album;        k += '\x1f';
    k += String(pb.duration_s);
    return k;
}

} // namespace lyrics
