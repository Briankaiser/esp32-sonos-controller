#include "art.h"
#include "state.h"
#include "playback.h"

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <JPEGDEC.h>

// ── Statics ──────────────────────────────────────────────────────────────────

static uint8_t*          s_jpeg_buf        = nullptr;
static uint8_t*          s_pixel_buf       = nullptr;
static SemaphoreHandle_t s_mutex           = nullptr;
static ArtState          s_state;
static TaskHandle_t      s_art_task_handle = nullptr;
static JPEGDEC           s_jpeg;
static uint16_t          s_decoded_w       = 0;  // set before decode(), read by jpeg_draw()

static constexpr uint32_t kArtTaskStackBytes = 12 * 1024;

// ── JPEGDEC draw callback ─────────────────────────────────────────────────────
// Called per MCU row by JPEGDEC::decode(). Writes RGB565 directly into s_pixel_buf.

static int jpeg_draw(JPEGDRAW* pDraw) {
    // JPEGDEC delivers MCU blocks: iHeight rows, each iWidth pixels wide
    for (int row = 0; row < pDraw->iHeight; ++row) {
        uint8_t* dst = s_pixel_buf
                     + (uint32_t)(pDraw->y + row) * s_decoded_w * 2u
                     + (uint32_t)pDraw->x * 2u;
        const uint16_t* src = pDraw->pPixels + row * pDraw->iWidth;
        memcpy(dst, src, (uint32_t)pDraw->iWidth * 2u);
    }
    return 1;
}

// ── Dominant colour extraction (RGB565 input) ─────────────────────────────────

static lv_color_t extract_dominant(const uint8_t* pixels, uint16_t w, uint16_t h) {
    uint32_t sum_r = 0, sum_g = 0, sum_b = 0, count = 0;
    uint32_t step_x = (w > 64) ? (w / 64) : 1;
    uint32_t step_y = (h > 64) ? (h / 64) : 1;

    for (uint32_t y = 0; y < h; y += step_y) {
        for (uint32_t x = 0; x < w; x += step_x) {
            const uint16_t* px16 = reinterpret_cast<const uint16_t*>(
                pixels + (y * (uint32_t)w + x) * 2u);
            uint16_t p = *px16;
            // Unpack RGB565 → 8-bit channels
            uint8_t r = (uint8_t)(((p >> 11) & 0x1Fu) << 3);
            uint8_t g = (uint8_t)(((p >>  5) & 0x3Fu) << 2);
            uint8_t b = (uint8_t)(( p        & 0x1Fu) << 3);
            uint8_t L = (uint8_t)(((uint32_t)77 * r + 150 * g + 29 * b) >> 8);
            if (L < 40) continue;
            uint8_t dev = (uint8_t)((r > g ? r - g : g - r) + (g > b ? g - b : b - g));
            if (dev < 15) continue;
            sum_r += r; sum_g += g; sum_b += b; ++count;
        }
    }

    if (count < 100) return lv_color_hex(0xD4A843);

    uint8_t avg_r = (uint8_t)(sum_r / count);
    uint8_t avg_g = (uint8_t)(sum_g / count);
    uint8_t avg_b = (uint8_t)(sum_b / count);

    uint8_t L = (uint8_t)(((uint32_t)77 * avg_r + 150 * avg_g + 29 * avg_b) >> 8);
    if (L < 60 && L > 0) {
        float sc = 60.0f / (float)L;
        avg_r = (uint8_t)(avg_r * sc > 255 ? 255 : avg_r * sc);
        avg_g = (uint8_t)(avg_g * sc > 255 ? 255 : avg_g * sc);
        avg_b = (uint8_t)(avg_b * sc > 255 ? 255 : avg_b * sc);
    }

    return lv_color_make(avg_r, avg_g, avg_b);
}

// ── HTTP fetch ────────────────────────────────────────────────────────────────

// Wrap-safe deadline check — millis() < deadline inverts at the 32-bit rollover
// (~49.7 days uptime), which would make every art fetch return instantly empty
// for the ~1 minute it takes millis() to cross back past `deadline`.
static inline bool time_before(uint32_t deadline) {
    return (int32_t)(millis() - deadline) < 0;
}

// Reads one '\n'-terminated line, yielding delay(1) when no data is available.
// readStringUntil routes through Stream::timedRead which busy-spins without
// yielding to FreeRTOS — this replacement prevents the IDLE watchdog from firing.
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

// If a non-chunked fetch makes no read progress for this long, bail early.
static constexpr uint32_t kFetchStallMs = 3000;
// Chunked CDN-proxied responses (e.g. Spotify via Sonos) can have natural
// pauses between bursts; allow a longer stall before giving up.
static constexpr uint32_t kChunkedStallMs = 8000;

// Diagnostics for the last fetch, read by fetch_and_decode's failure logging.
static uint32_t s_last_fetch_ms       = 0;   // wall time spent in the last fetch
static long     s_last_content_length = -1;  // Content-Length header, -1 if absent
static bool     s_last_fetch_stalled  = false;
static int      s_last_http_status    = -1;   // HTTP status code, -1 if unparsed
static String   s_last_host;                   // host:port actually contacted

static size_t fetch_jpeg(const String& url,
                         const String& speaker_ip,
                         uint16_t      speaker_port) {
    s_last_content_length = -1;
    s_last_fetch_stalled  = false;
    s_last_http_status    = -1;
    uint32_t t_start      = millis();

    String decoded_url = url;
    decoded_url.replace("&amp;", "&");

    // Resolve the target. A relative path is served by the Sonos speaker over
    // plain HTTP (the /getaa proxy). An absolute URL — e.g. Spotify hands back
    // https://i.scdn.co/image/... directly — must go to ITS OWN host, over TLS
    // when https; the speaker does not host that path. (This is why Spotify art
    // 404'd: the path was requested from the speaker instead of the CDN.)
    String   host;
    uint16_t port;
    String   path;
    bool     use_tls = false;
    if (decoded_url.startsWith("/")) {
        host    = speaker_ip;
        port    = speaker_port;
        path    = decoded_url;
    } else {
        int proto_end = decoded_url.indexOf("://");
        if (proto_end < 0) {
            Serial.printf("[Art] bad URL: %s\n", decoded_url.c_str());
            s_last_fetch_ms = millis() - t_start;
            return 0;
        }
        use_tls    = decoded_url.substring(0, proto_end).equalsIgnoreCase("https");
        String rest = decoded_url.substring(proto_end + 3);
        int slash   = rest.indexOf('/');
        String authority = (slash >= 0) ? rest.substring(0, slash) : rest;
        path             = (slash >= 0) ? rest.substring(slash)    : "/";
        int colon = authority.indexOf(':');
        if (colon >= 0) {
            host = authority.substring(0, colon);
            port = (uint16_t)authority.substring(colon + 1).toInt();
        } else {
            host = authority;
            port = use_tls ? 443 : 80;
        }
    }

    s_last_host = host + ":" + String(port) + (use_tls ? " (tls)" : "");

    // Pick the transport. WiFiClientSecure carries the TLS stack (heavier); only
    // engaged for https absolute URLs. setInsecure() skips cert validation, which
    // is acceptable for public album-art images and avoids shipping a CA bundle.
    WiFiClient       plain;
    WiFiClientSecure secure;
    Client*          client;
    if (use_tls) {
        secure.setInsecure();
        client = &secure;
    } else {
        client = &plain;
    }
    client->setTimeout(8000);
    if (!client->connect(host.c_str(), port)) {
        Serial.printf("[Art] connect failed %s:%u%s\n",
                      host.c_str(), (unsigned)port, use_tls ? " (tls)" : "");
        s_last_fetch_ms = millis() - t_start;
        return 0;
    }
    // Host header: omit the port for the scheme defaults so CDNs are happy.
    String host_hdr = host;
    if (!((use_tls && port == 443) || (!use_tls && port == 80)))
        host_hdr += ":" + String(port);
    client->printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                   path.c_str(), host_hdr.c_str());

    // Sonos proxies Spotify/streaming art from cloud CDN, which can be slow
    // on cross-subnet WiFi paths (observed ~1-4 KB/s for a ~110 KB JPEG).
    // 60 s covers a 150 KB image at worst-case 2.5 KB/s; the stall detections
    // below bail out early when the connection actually drops mid-transfer.
    uint32_t deadline = millis() + 60000;
    while (!client->available() && time_before(deadline)) delay(5);

    // Status line, e.g. "HTTP/1.1 200 OK". Sonos answers 404 (with a short
    // error-page body) for missing/expired art; feeding that body to JPEGDEC is
    // exactly the "openRAM failed (~200 bytes)" symptom. Parse the code and bail
    // on anything that isn't 2xx so we never try to decode an error page.
    {
        String status = read_line(*client, deadline);
        int sp = status.indexOf(' ');
        if (sp >= 0) s_last_http_status = (int)strtol(status.c_str() + sp + 1, nullptr, 10);
        if (s_last_http_status < 200 || s_last_http_status >= 300) {
            client->stop();
            s_last_fetch_ms = millis() - t_start;
            return 0;
        }
    }

    bool is_chunked = false;
    while (client->connected() && time_before(deadline)) {
        String line = read_line(*client, deadline);
        if (line.isEmpty()) break;
        String lower = line;
        lower.toLowerCase();
        if (lower.indexOf("chunked") >= 0) is_chunked = true;
        if (lower.startsWith("content-length:")) {
            s_last_content_length = strtol(line.substring(line.indexOf(':') + 1).c_str(),
                                           nullptr, 10);
        }
    }

    size_t   fetched       = 0;
    uint32_t last_progress = millis();
    if (is_chunked) {
        while ((client->connected() || client->available()) && time_before(deadline)) {
            String size_line = read_line(*client, deadline);
            if (size_line.isEmpty()) {
                // Empty size line: either still waiting or the connection stalled.
                if (millis() - last_progress > kChunkedStallMs) {
                    s_last_fetch_stalled = true; break;
                }
                continue;
            }
            uint32_t chunk_size = strtoul(size_line.c_str(), nullptr, 16);
            if (chunk_size == 0) break;
            // Read chunk body in bursts: readBytes of currently-available data,
            // then yield — prevents starving IDLE when the data stream is continuous.
            for (uint32_t i = 0; i < chunk_size && time_before(deadline); ) {
                int av = client->available();
                if (av > 0 && fetched < art::kArtJpegBufBytes) {
                    size_t to_read = (size_t)av;
                    if (to_read > chunk_size - i)             to_read = chunk_size - i;
                    if (to_read > art::kArtJpegBufBytes - fetched)
                        to_read = art::kArtJpegBufBytes - fetched;
                    size_t got = (size_t)client->readBytes(
                        (char*)(s_jpeg_buf + fetched), to_read);
                    fetched += got;
                    i      += (uint32_t)got;
                    if (got > 0) last_progress = millis();
                } else if (fetched >= art::kArtJpegBufBytes) {
                    break;
                } else if (client->connected()) {
                    if (millis() - last_progress > kChunkedStallMs) {
                        s_last_fetch_stalled = true; goto done_chunked;
                    }
                    delay(1);
                    continue;
                } else {
                    break;
                }
                delay(1);  // yield after every burst
            }
            read_line(*client, deadline);  // trailing CRLF after chunk data
            if (fetched >= art::kArtJpegBufBytes) break;
        }
        done_chunked:;
    } else {
        while ((client->connected() || client->available()) && time_before(deadline)) {
            int av = client->available();
            if (av > 0 && fetched < art::kArtJpegBufBytes) {
                size_t to_read = (size_t)av;
                if (to_read > art::kArtJpegBufBytes - fetched)
                    to_read = art::kArtJpegBufBytes - fetched;
                size_t got = (size_t)client->readBytes(
                    (char*)(s_jpeg_buf + fetched), to_read);
                fetched += got;
                if (got > 0) last_progress = millis();
            }
            if (fetched >= art::kArtJpegBufBytes) break;
            // Full body received — stop now rather than waiting for the server
            // to close the connection.
            if (s_last_content_length >= 0 && (long)fetched >= s_last_content_length) break;
            if (millis() - last_progress > kFetchStallMs) { s_last_fetch_stalled = true; break; }
            delay(1);
        }
    }
    client->stop();
    s_last_fetch_ms = millis() - t_start;
    return fetched;
}

// ── Decode ────────────────────────────────────────────────────────────────────

static void fetch_and_decode(const String& url,
                             const String& speaker_ip,
                             uint16_t      speaker_port) {
    if (!s_jpeg_buf || !s_pixel_buf) return;

    size_t fetched = fetch_jpeg(url, speaker_ip, speaker_port);
    if (fetched == 0) {
        if (s_last_http_status >= 0 && (s_last_http_status < 200 || s_last_http_status >= 300))
            Serial.printf("[Art] HTTP %d from %s — no art (%lums)\n",
                          s_last_http_status, s_last_host.c_str(), (unsigned long)s_last_fetch_ms);
        else
            Serial.printf("[Art] empty response from %s (%lums%s)\n",
                          s_last_host.c_str(), (unsigned long)s_last_fetch_ms,
                          s_last_fetch_stalled ? ", stalled" : "");
        return;
    }
    if (fetched >= art::kArtJpegBufBytes) {
        Serial.printf("[Art] JPEG too large (>= %u bytes)\n", (unsigned)art::kArtJpegBufBytes);
        return;
    }

    // A 2xx body that isn't actually a JPEG (rare text placeholder, or a partial
    // that lost its header) has no Start-Of-Image marker. Skip it cleanly rather
    // than letting openRAM fail obscurely.
    if (fetched < 2 || s_jpeg_buf[0] != 0xFF || s_jpeg_buf[1] != 0xD8) {
        Serial.printf("[Art] not a JPEG (%u bytes, starts 0x%02X 0x%02X)\n",
                      (unsigned)fetched, s_jpeg_buf[0], s_jpeg_buf[1]);
        return;
    }

    // Open JPEG from RAM
    if (!s_jpeg.openRAM(s_jpeg_buf, (int)fetched, jpeg_draw)) {
        Serial.printf("[Art] JPEGDEC openRAM failed (%u bytes, %lums)\n",
                      (unsigned)fetched, (unsigned long)s_last_fetch_ms);
        return;
    }

    int img_w = s_jpeg.getWidth();
    int img_h = s_jpeg.getHeight();

    // Choose scale so decoded dimensions fit in kArtPixelBufBytes (≤320×320).
    // JPEGDEC 1.x: 0=1:1, JPEG_SCALE_HALF=1:2, JPEG_SCALE_QUARTER=1:4
    int scale = 0;
    if (img_w > 320 || img_h > 320) scale = JPEG_SCALE_HALF;
    if (img_w > 640 || img_h > 640) scale = JPEG_SCALE_QUARTER;

    uint16_t dec_w = (uint16_t)(scale == JPEG_SCALE_QUARTER ? (img_w >> 2) :
                                 scale == JPEG_SCALE_HALF    ? (img_w >> 1) : img_w);
    uint16_t dec_h = (uint16_t)(scale == JPEG_SCALE_QUARTER ? (img_h >> 2) :
                                 scale == JPEG_SCALE_HALF    ? (img_h >> 1) : img_h);

    if ((uint32_t)dec_w * dec_h * 2u > art::kArtPixelBufBytes) {
        Serial.printf("[Art] decoded size %ux%u too large for buffer\n", dec_w, dec_h);
        s_jpeg.close();
        return;
    }

    s_decoded_w = dec_w;

    uint32_t t_dec = millis();
    int ok = s_jpeg.decode(0, 0, scale);
    uint32_t decode_ms = millis() - t_dec;
    s_jpeg.close();

    if (!ok) {
        // openRAM parsed the header but decode ran short — almost always a
        // truncated download. Log the byte count against Content-Length and
        // the elapsed time so a stall is distinguishable from a genuinely
        // corrupt JPEG.
        if (s_last_content_length >= 0 && (long)fetched < s_last_content_length) {
            Serial.printf("[Art] fetch truncated %u/%ld bytes in %lums%s — decode failed\n",
                          (unsigned)fetched, s_last_content_length,
                          (unsigned long)s_last_fetch_ms,
                          s_last_fetch_stalled ? " (stalled)" : "");
        } else {
            Serial.printf("[Art] JPEGDEC decode failed (%u bytes, %lums%s)\n",
                          (unsigned)fetched, (unsigned long)s_last_fetch_ms,
                          s_last_fetch_stalled ? ", stalled" : "");
        }
        return;
    }

    lv_color_t accent = extract_dominant(s_pixel_buf, dec_w, dec_h);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.pixels  = s_pixel_buf;
    s_state.w       = dec_w;
    s_state.h       = dec_h;
    s_state.accent  = accent;
    s_state.uri     = url;
    s_state.version++;
    xSemaphoreGive(s_mutex);

    Serial.printf("[Art] decoded %ux%u accent=#%02X%02X%02X (fetch %lums, decode %lums)\n",
                  dec_w, dec_h,
                  (unsigned)accent.red, (unsigned)accent.green, (unsigned)accent.blue,
                  (unsigned long)s_last_fetch_ms, (unsigned long)decode_ms);
}

// ── Art task body ─────────────────────────────────────────────────────────────

static void art_task_body(void* /*arg*/) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // block until notified

        sonos::PlaybackState pb  = sonos::playback::snapshot();
        ArtState             cur = art::snapshot();

        if (pb.album_art_uri.isEmpty() || pb.album_art_uri == cur.uri) continue;

        SonosState st = sonos::state::snapshot();
        if (st.home_room_idx < 0 || st.home_room_idx >= (int)st.speakers.size()) continue;

        const SpeakerInfo& hr = st.speakers[st.home_room_idx];

        // Sonos serves the /getaa art proxy ONLY from the speaker actually
        // playing the track — the group coordinator. A grouped member (or any
        // unrelated room) returns 404, which is why album art disappears once
        // the home room is grouped under another coordinator. So fetch from the
        // home room's coordinator, not an arbitrary speaker.
        //
        // group_coordinator_uuid: empty or == home room  => the home room is
        // itself the coordinator (standalone, or coordinator of its group), so
        // fetch from it. Otherwise it names the coordinator speaker to use.
        const SpeakerInfo* src = &hr;
        if (!pb.group_coordinator_uuid.isEmpty() &&
            pb.group_coordinator_uuid != hr.uuid) {
            for (const SpeakerInfo& s : st.speakers) {
                if (s.uuid == pb.group_coordinator_uuid) { src = &s; break; }
            }
        }
        fetch_and_decode(pb.album_art_uri, src->ip, src->port);
    }
}

// ── Public interface ──────────────────────────────────────────────────────────

void art::init() {
    s_jpeg_buf  = static_cast<uint8_t*>(
        heap_caps_malloc(kArtJpegBufBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    s_pixel_buf = static_cast<uint8_t*>(
        heap_caps_malloc(kArtPixelBufBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    s_mutex = xSemaphoreCreateMutex();

    if (!s_jpeg_buf || !s_pixel_buf) {
        Serial.printf("[Art] PSRAM alloc failed (jpeg=%p pixel=%p)\n",
                      (void*)s_jpeg_buf, (void*)s_pixel_buf);
    } else {
        Serial.printf("[Art] PSRAM buffers: %u + %u bytes\n",
                      (unsigned)kArtJpegBufBytes, (unsigned)kArtPixelBufBytes);
    }
}

void art::start_task() {
    xTaskCreatePinnedToCore(art_task_body, "Art",
                            kArtTaskStackBytes, nullptr,
                            1,                  // priority 1 < sonos_task priority 3
                            &s_art_task_handle, 0);  // core 0
}

void art::notify_new_uri() {
    if (s_art_task_handle) xTaskNotifyGive(s_art_task_handle);
}

ArtState art::snapshot() {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    ArtState copy = s_state;
    xSemaphoreGive(s_mutex);
    return copy;
}
