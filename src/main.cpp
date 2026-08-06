#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <lvgl.h>

#include "bsp/bsp.h"
#include "bsp/input.h"
#include "ui/layout/layout.h"
#include "ui/screens/placeholder.h"
#include "services/prefs.h"
#include "services/portal.h"
#include "services/screenshot.h"
#include "services/ota.h"
#include "app/sonos/state.h"
#include "app/sonos/discovery.h"

/* ── Task configuration ─────────────────────────────────────────────────────
 *
 * ui_task    — core 1, owns ALL lv_* calls.  Never call LVGL from any other task.
 * sonos_task — core 0, SSDP discovery + topology polling loop.
 * web_task   — core 0, runs WebServer::handleClient loop for screenshot + OTA.
 */

static constexpr uint32_t kUiTaskStackBytes    = 12 * 1024;
static constexpr uint32_t kSonosTaskStackBytes = 16 * 1024;
static constexpr uint32_t kWebTaskStackBytes    = 8 * 1024;
static constexpr UBaseType_t kUiTaskPriority    = 5;
static constexpr UBaseType_t kSonosTaskPriority = 3;

// Queue for worker → UI results (placeholder — actual message type defined per change)
static QueueHandle_t s_worker_to_ui = nullptr;
static constexpr uint32_t kWorkerQueueDepth = 8;
static constexpr uint32_t kWorkerQueueItemBytes = 64;  // placeholder size

static bool s_wifi_ok = false;
static WebServer s_web_server(80);

// Pointer to the active screen (used to guard intent delivery)
static lv_obj_t* volatile s_active_scr = nullptr;

// ── Encoder long-press / factory-reset state (UI task only) ─────────────────
static bool        s_btn_held      = false;
static uint32_t    s_btn_down_ms   = 0;
static lv_obj_t*   s_reset_overlay = nullptr;
static lv_timer_t* s_reset_timer   = nullptr;

// Fires every 1 s while the encoder button is held.
// Creates / updates the countdown overlay after kResetCountdownStartMs,
// triggers factory_reset() at kResetTotalMs.  All LVGL calls run on core 1.
static void reset_tick(lv_timer_t*) {
    if (!s_btn_held) return;

    uint32_t elapsed = millis() - s_btn_down_ms;

    if (elapsed >= prefs::kResetTotalMs) {
        prefs::factory_reset();  // clears NVS, calls ESP.restart() — never returns
        return;
    }

    if (elapsed >= prefs::kResetCountdownStartMs) {
        uint32_t rem = (prefs::kResetTotalMs - elapsed) / 1000;

        if (!s_reset_overlay) {
            s_reset_overlay = lv_obj_create(lv_layer_top());
            lv_obj_set_size(s_reset_overlay, LV_PCT(100), LV_PCT(100));
            lv_obj_set_style_bg_color(s_reset_overlay, lv_color_black(), 0);
            lv_obj_set_style_bg_opa(s_reset_overlay, LV_OPA_70, 0);
            lv_obj_set_style_border_width(s_reset_overlay, 0, 0);
            lv_obj_clear_flag(s_reset_overlay, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* lbl = lv_label_create(s_reset_overlay);
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
            lv_label_set_text_fmt(lbl, "Hold to reset\n%u s", (unsigned)rem);
            lv_obj_center(lbl);
        } else {
            lv_obj_t* lbl = lv_obj_get_child(s_reset_overlay, 0);
            if (lbl) lv_label_set_text_fmt(lbl, "Hold to reset\n%u s", (unsigned)rem);
        }
    }
}

/* ── Web task (core 0) ──────────────────────────────────────────────────────
 *
 * Serves /screenshot and /update via the Arduino WebServer.
 * Runs only when s_wifi_ok is true (started in setup() after WiFi connects).
 */
static void web_task(void* /*arg*/) {
    for (;;) {
        s_web_server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* ── Sonos task (core 0) ────────────────────────────────────────────────────
 *
 * Runs SSDP discovery + zone-group topology refresh on boot, then repeats
 * every kDiscoveryIntervalMs.  Results are published to sonos::state (mutex-
 * protected); the UI task reads state via sonos::state::snapshot().
 */
static void sonos_task(void* /*arg*/) {
    for (;;) {
        sonos::discovery::run_cycle();
        vTaskDelay(pdMS_TO_TICKS(sonos::discovery::kDiscoveryIntervalMs));
    }
}

/* ── UI task (core 1) ───────────────────────────────────────────────────────
 *
 * Owns the LVGL event loop. Never yields to blocking I/O.
 * Drains:
 *   • lv_timer_handler()
 *   • bsp::pop_event()  → delivers intents to active screen
 *   • s_worker_to_ui queue (placeholder — consumed but not yet acted on)
 */
static void ui_task(void* /*arg*/) {
    lv_init();
    bsp::display_init();
    bsp::input_init();

    lv_obj_t* scr = screens::placeholder_create();
    lv_screen_load(scr);
    s_active_scr = scr;

    bsp::backlight(200);

    // Event loop
    for (;;) {
        uint32_t next_ms = lv_timer_handler();
        screenshot::poll(); // services pending screenshot requests from web_task

        // Drain input events.
        bsp::InputEvent evt;
        while (bsp::pop_event(evt)) {
            switch (evt.intent) {
                case bsp::Intent::EncoderButtonDown:
                    s_btn_held    = true;
                    s_btn_down_ms = millis();
                    if (!s_reset_timer)
                        s_reset_timer = lv_timer_create(reset_tick, 1000, nullptr);
                    break;
                case bsp::Intent::EncoderButtonUp:
                    s_btn_held = false;
                    if (s_reset_timer)   { lv_timer_delete(s_reset_timer);    s_reset_timer = nullptr; }
                    if (s_reset_overlay) { lv_obj_delete(s_reset_overlay); s_reset_overlay = nullptr; }
                    break;
                default:
                    (void)evt;
                    break;
            }
        }

        // Drain worker queue (results arrive here once Sonos etc. exist)
        uint8_t worker_msg[kWorkerQueueItemBytes];
        while (xQueueReceive(s_worker_to_ui, worker_msg, 0) == pdTRUE) {
            // Placeholder: ignore
        }

        // Yield for the remaining time LVGL says it won't need the CPU
        uint32_t delay_ms = (next_ms < 1) ? 1 : (next_ms > 20 ? 20 : next_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/* ── Arduino entry points ───────────────────────────────────────────────────
 *
 * setup() runs on core 1, but we immediately hand off to FreeRTOS tasks
 * so that core assignment is explicit and not reliant on Arduino's default.
 */
void setup() {
    Serial.begin(115200);

    // Hardware init (I2C, PCF8574, backlight off) — runs on both portal and
    // normal boot paths so the display is available before the provisioning check.
    bsp::init();

    if (!prefs::is_provisioned()) {
        Serial.println("[boot] no credentials — starting captive portal");
        portal::run();  // blocks until ESP.restart(); never returns
    }
    Serial.println("[boot] provisioned — starting normal init");

    // WiFi station connect (blocking, 20 s timeout).
    // After ESP.restart() the WiFi driver is uninitialized — no disconnect needed.
    // setAutoReconnect(false) prevents the driver using cached IDF-NVS credentials
    // that could race with our WiFi.begin() call.
    WiFi.setAutoReconnect(false);
    {
        String ssid = prefs::wifi_ssid();
        String pass = prefs::wifi_pass();
        Serial.printf("[WiFi] connecting to '%s'\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
    }
    {
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
            vTaskDelay(pdMS_TO_TICKS(250));
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        s_wifi_ok = true;
        Serial.printf("[WiFi] connected, IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WiFi] connection failed — web services unavailable");
    }

    // Web services (screenshot + OTA) — only when WiFi is up
    if (s_wifi_ok) {
        screenshot::init();
        screenshot::register_handler(s_web_server);
        ota::register_handler(s_web_server);
        s_web_server.begin();
    }

    // Worker → UI queue
    s_worker_to_ui = xQueueCreate(kWorkerQueueDepth, kWorkerQueueItemBytes);

    // Shared Sonos state mutex — must exist before any task that reads state
    sonos::state::init();

    // Spawn tasks
    xTaskCreatePinnedToCore(sonos_task, "Sonos", kSonosTaskStackBytes,
                            nullptr, kSonosTaskPriority, nullptr, 0);
    xTaskCreatePinnedToCore(ui_task, "UI", kUiTaskStackBytes,
                            nullptr, kUiTaskPriority, nullptr, 1);
    if (s_wifi_ok) {
        xTaskCreatePinnedToCore(web_task, "Web", kWebTaskStackBytes,
                                nullptr, kSonosTaskPriority, nullptr, 0);
    }

    // main task (Arduino's setup/loop) is not needed after this point
}

void loop() {
    // Worker and UI are FreeRTOS tasks; loop() just yields forever.
    vTaskDelay(portMAX_DELAY);
}
