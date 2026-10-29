#include "diag.h"

#include <atomic>
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr int      kBits         = 7;
constexpr size_t   kRing         = 24;
constexpr uint32_t kDipBytes     = 16 * 1024;
constexpr uint32_t kDipExitBytes = 24 * 1024;  // hysteresis so one dip is one entry
constexpr uint32_t kPollMs       = 200;

const char* const kNames[kBits] = {
    "art_fetch", "art_tls", "art_decode", "lyrics_tls",
    "soap", "discovery", "gena_notify",
};

struct Dip {
    uint32_t up_ms;
    uint32_t dur_ms;
    uint32_t floor;
    uint32_t largest;
    uint32_t mask;
    bool     new_low;
};

std::atomic<int16_t>  s_count[kBits];   // nesting-safe: SOAP runs on two tasks
std::atomic<uint32_t> s_sticky{0};      // bits touched since the last poll

Dip      s_ring[kRing];
size_t   s_head       = 0;              // next write slot
size_t   s_len        = 0;
uint32_t s_dip_total  = 0;
portMUX_TYPE s_mux    = portMUX_INITIALIZER_UNLOCKED;

std::atomic<uint32_t> s_fail_count{0};
volatile uint32_t     s_fail_size = 0;
volatile uint32_t     s_fail_caps = 0;
const char* volatile  s_fail_fn   = nullptr;
char                  s_fail_task[16] = {0};

uint32_t active_mask() {
    uint32_t m = 0;
    for (int i = 0; i < kBits; i++)
        if (s_count[i].load(std::memory_order_relaxed) > 0) m |= (1u << i);
    return m;
}

void IRAM_ATTR on_alloc_failed(size_t size, uint32_t caps, const char* fn) {
    s_fail_count.fetch_add(1, std::memory_order_relaxed);
    s_fail_size = (uint32_t)size;
    s_fail_caps = caps;
    s_fail_fn   = fn;
    if (!xPortInIsrContext()) {
        const char* t = pcTaskGetName(nullptr);
        if (t) { strncpy(s_fail_task, t, sizeof(s_fail_task) - 1); }
    }
    esp_rom_printf("[Diag] alloc FAILED size=%u caps=0x%x fn=%s\n",
                   (unsigned)size, (unsigned)caps, fn ? fn : "?");
}

void watcher(void*) {
    uint32_t last_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    bool     in_dip   = false;
    size_t   cur      = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(kPollMs));
        uint32_t free_now = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        uint32_t min_now  = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        uint32_t mask     = active_mask() | s_sticky.exchange(0);
        bool new_low      = min_now < last_min;
        bool low_now      = free_now < kDipBytes || (new_low && min_now < kDipBytes);

        if (!in_dip && low_now) {
            in_dip = true;
            Dip d{millis(), 0, new_low ? min_now : free_now,
                  (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                  mask, new_low};
            portENTER_CRITICAL(&s_mux);
            cur = s_head;
            s_ring[cur] = d;
            s_head = (s_head + 1) % kRing;
            if (s_len < kRing) s_len++;
            s_dip_total++;
            portEXIT_CRITICAL(&s_mux);
            Serial.printf("[Diag] heap dip floor=%u new_low=%d active=0x%02x\n",
                          (unsigned)d.floor, (int)new_low, (unsigned)mask);
        } else if (in_dip) {
            portENTER_CRITICAL(&s_mux);
            Dip& d = s_ring[cur];
            d.mask |= mask;
            if (new_low) { d.new_low = true; d.floor = min_now; }
            else if (free_now < d.floor) d.floor = free_now;
            d.dur_ms = millis() - d.up_ms;
            portEXIT_CRITICAL(&s_mux);
            if (free_now > kDipExitBytes && !new_low) in_dip = false;
        }
        last_min = min_now;
    }
}

void append_names(String& out, uint32_t mask) {
    bool first = true;
    out += "[";
    for (int i = 0; i < kBits; i++) {
        if (!(mask & (1u << i))) continue;
        if (!first) out += ",";
        out += "\"";
        out += kNames[i];
        out += "\"";
        first = false;
    }
    out += "]";
}

}  // namespace

void diag::begin() {
    heap_caps_register_failed_alloc_callback(on_alloc_failed);
    xTaskCreatePinnedToCore(watcher, "Diag", 3072, nullptr, 1, nullptr, 0);
}

void diag::mark(uint32_t bits) {
    for (int i = 0; i < kBits; i++)
        if (bits & (1u << i)) s_count[i].fetch_add(1, std::memory_order_relaxed);
    s_sticky.fetch_or(bits, std::memory_order_relaxed);
}

void diag::unmark(uint32_t bits) {
    for (int i = 0; i < kBits; i++)
        if (bits & (1u << i)) s_count[i].fetch_sub(1, std::memory_order_relaxed);
}

String diag::json_fragment() {
    Dip snap[kRing];
    size_t len, head;
    uint32_t total;
    portENTER_CRITICAL(&s_mux);
    memcpy(snap, s_ring, sizeof(snap));
    len = s_len; head = s_head; total = s_dip_total;
    portEXIT_CRITICAL(&s_mux);

    String out;
    out.reserve(1024);
    out += "  \"dip_count\": " + String((unsigned)total) + ",\n";
    out += "  \"alloc_fail_count\": " + String((unsigned)s_fail_count.load()) + ",\n";
    out += "  \"alloc_fail_last\": {\"size\": " + String((unsigned)s_fail_size) +
           ", \"caps\": " + String((unsigned)s_fail_caps) +
           ", \"fn\": \"" + String(s_fail_fn ? s_fail_fn : "") +
           "\", \"task\": \"" + String(s_fail_task) + "\"},\n";
    out += "  \"heap_dips\": [";
    for (size_t i = 0; i < len; i++) {
        const Dip& d = snap[(head + kRing - len + i) % kRing];
        if (i) out += ",";
        out += "\n    {\"up_s\": " + String((unsigned)(d.up_ms / 1000)) +
               ", \"dur_ms\": " + String((unsigned)d.dur_ms) +
               ", \"floor\": " + String((unsigned)d.floor) +
               ", \"largest\": " + String((unsigned)d.largest) +
               ", \"new_low\": " + String(d.new_low ? "true" : "false") +
               ", \"active\": ";
        append_names(out, d.mask);
        out += "}";
    }
    out += "\n  ]";
    return out;
}
