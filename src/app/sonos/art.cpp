#include "art.h"
#include "state.h"
#include "playback.h"

#include <Arduino.h>
#include <WiFiClient.h>
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

// Reads one '\n'-terminated line, yielding delay(1) when no data is available.
// readStringUntil routes through Stream::timedRead which busy-spins without
// yielding to FreeRTOS — this replacement prevents the IDLE watchdog from firing.
static String read_line(WiFiClient& client, uint32_t deadline) {
    String line;
    while (millis() < deadline) {
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

static size_t fetch_jpeg(const String& url,
                         const String& speaker_ip,
                         uint16_t      speaker_port) {
    String decoded_url = url;
    decoded_url.replace("&amp;", "&");

    String path;
    if (decoded_url.startsWith("/")) {
        path = decoded_url;
    } else {
        int proto_end = decoded_url.indexOf("://");
        if (proto_end < 0) {
            Serial.printf("[Art] bad URL: %s\n", decoded_url.c_str());
            return 0;
        }
        String rest = decoded_url.substring(proto_end + 3);
        int slash = rest.indexOf('/');
        path = (slash >= 0) ? rest.substring(slash) : "/";
    }

    WiFiClient client;
    client.setTimeout(8000);
    if (!client.connect(speaker_ip.c_str(), speaker_port, 5000)) {
        Serial.printf("[Art] connect failed %s:%u\n",
                      speaker_ip.c_str(), (unsigned)speaker_port);
        return 0;
    }
    client.printf("GET %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\n\r\n",
                  path.c_str(), speaker_ip.c_str(), (unsigned)speaker_port);

    uint32_t deadline = millis() + 10000;
    while (!client.available() && millis() < deadline) delay(5);
    read_line(client, deadline);  // status line
    bool is_chunked = false;
    while (client.connected() && millis() < deadline) {
        String line = read_line(client, deadline);
        if (line.isEmpty()) break;
        if (line.indexOf("chunked") >= 0) is_chunked = true;
    }

    size_t fetched = 0;
    if (is_chunked) {
        while ((client.connected() || client.available()) && millis() < deadline) {
            String size_line = read_line(client, deadline);
            if (size_line.isEmpty()) continue;
            uint32_t chunk_size = strtoul(size_line.c_str(), nullptr, 16);
            if (chunk_size == 0) break;
            // Read chunk body in bursts: readBytes of currently-available data,
            // then yield — prevents starving IDLE when the data stream is continuous.
            for (uint32_t i = 0; i < chunk_size && millis() < deadline; ) {
                int av = client.available();
                if (av > 0 && fetched < art::kArtJpegBufBytes) {
                    size_t to_read = (size_t)av;
                    if (to_read > chunk_size - i)             to_read = chunk_size - i;
                    if (to_read > art::kArtJpegBufBytes - fetched)
                        to_read = art::kArtJpegBufBytes - fetched;
                    size_t got = (size_t)client.readBytes(
                        (char*)(s_jpeg_buf + fetched), to_read);
                    fetched += got;
                    i      += (uint32_t)got;
                } else if (fetched >= art::kArtJpegBufBytes) {
                    break;
                } else if (client.connected()) {
                    delay(1);
                    continue;
                } else {
                    break;
                }
                delay(1);  // yield after every burst
            }
            read_line(client, deadline);  // trailing CRLF after chunk data
            if (fetched >= art::kArtJpegBufBytes) break;
        }
    } else {
        while ((client.connected() || client.available()) && millis() < deadline) {
            int av = client.available();
            if (av > 0 && fetched < art::kArtJpegBufBytes) {
                size_t to_read = (size_t)av;
                if (to_read > art::kArtJpegBufBytes - fetched)
                    to_read = art::kArtJpegBufBytes - fetched;
                fetched += (size_t)client.readBytes(
                    (char*)(s_jpeg_buf + fetched), to_read);
            }
            if (fetched >= art::kArtJpegBufBytes) break;
            delay(1);
        }
    }
    client.stop();
    return fetched;
}

// ── Decode ────────────────────────────────────────────────────────────────────

static void fetch_and_decode(const String& url,
                             const String& speaker_ip,
                             uint16_t      speaker_port) {
    if (!s_jpeg_buf || !s_pixel_buf) return;

    size_t fetched = fetch_jpeg(url, speaker_ip, speaker_port);
    if (fetched == 0) { Serial.println("[Art] empty response"); return; }
    if (fetched >= art::kArtJpegBufBytes) {
        Serial.printf("[Art] JPEG too large (>= %u bytes)\n", (unsigned)art::kArtJpegBufBytes);
        return;
    }

    // Open JPEG from RAM
    if (!s_jpeg.openRAM(s_jpeg_buf, (int)fetched, jpeg_draw)) {
        Serial.println("[Art] JPEGDEC openRAM failed");
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

    int ok = s_jpeg.decode(0, 0, scale);
    s_jpeg.close();

    if (!ok) {
        Serial.println("[Art] JPEGDEC decode failed");
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

    Serial.printf("[Art] decoded %ux%u accent=#%02X%02X%02X\n",
                  dec_w, dec_h,
                  (unsigned)accent.red, (unsigned)accent.green, (unsigned)accent.blue);
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
        fetch_and_decode(pb.album_art_uri, hr.ip, hr.port);
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
