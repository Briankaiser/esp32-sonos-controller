#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <lvgl.h>

#include "bsp/bsp.h"
#include "bsp/input.h"
#include "ui/layout/layout.h"
#include "ui/screens/placeholder.h"
#include "ui/screens/now_playing.h"
#include "ui/screens/fault_overlay.h"
#include "ui/nav.h"
#include "services/prefs.h"
#include "services/portal.h"
#include "services/screenshot.h"
#include "services/ota.h"
#include "services/rooms.h"
#include "app/sonos/state.h"
#include "app/sonos/discovery.h"
#include "app/sonos/soap.h"
#include "app/sonos/playback.h"
#include "app/sonos/art.h"
#include "app/sonos/favorites.h"
#include "app/sonos/eventing.h"

/* ── Task configuration ─────────────────────────────────────────────────────
 *
 * ui_task    — core 1, owns ALL lv_* calls.  Never call LVGL from any other task.
 * cmd_task   — core 0, drains user commands; higher priority than sonos_task so
 *              a tap preempts an in-flight poll and executes immediately.
 * sonos_task — core 0, SSDP discovery + topology polling loop.
 * event_task — core 0, GENA subscriptions + NOTIFY listener (eventing.cpp);
 *              while healthy, sonos_task's steady-state polls are suspended.
 * web_task   — core 0, runs WebServer::handleClient loop for screenshot + OTA.
 */

static constexpr uint32_t kUiTaskStackBytes    = 12 * 1024;
static constexpr uint32_t kSonosTaskStackBytes = 16 * 1024;
static constexpr uint32_t kCmdTaskStackBytes    = 8 * 1024;
static constexpr uint32_t kWebTaskStackBytes    = 8 * 1024;
static constexpr UBaseType_t kUiTaskPriority    = 5;
static constexpr UBaseType_t kCmdTaskPriority   = 4;   // above sonos_task so commands preempt polls
static constexpr UBaseType_t kSonosTaskPriority = 3;

// Queue for worker → UI results (placeholder — actual message type defined per change)
static QueueHandle_t s_worker_to_ui = nullptr;
static constexpr uint32_t kWorkerQueueDepth = 8;
static constexpr uint32_t kWorkerQueueItemBytes = 64;  // placeholder size

static bool s_wifi_ok = false;
static WebServer s_web_server(80);

// Pointer to the active screen (used to guard intent delivery)
static lv_obj_t* volatile s_active_scr = nullptr;

// Last playback version seen by the UI task; used to detect state changes
static uint32_t s_playback_version = 0;

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

/* ── Command task (core 0, priority above sonos_task) ───────────────────────
 *
 * Blocks until enqueue_command/enqueue_volume_delta notifies it, then drains
 * the command queue (transport, volume, mute, grouping, favorites) against the
 * home room speaker.  Because it outranks sonos_task on core 0, a user command
 * preempts an in-flight poll and executes within tens of ms instead of waiting
 * for the poll's SOAP calls to finish.  After draining it nudges sonos_task to
 * re-poll so the display reflects the new state quickly.
 */
static void cmd_task(void* /*arg*/) {
    sonos::playback::set_cmd_notify_task(xTaskGetCurrentTaskHandle());
    for (;;) {
        uint32_t bits = 0;
        xTaskNotifyWait(0, UINT32_MAX, &bits, portMAX_DELAY);
        SonosState st = sonos::state::snapshot();
        if (st.home_room_idx < 0 || st.home_room_idx >= (int)st.speakers.size()) continue;
        const SpeakerInfo& speaker = st.speakers[st.home_room_idx];
        bool did_cmd    = false;
        bool did_volume = false;

        // Transport / grouping / favorites — drain immediately, no debounce
        if (bits & sonos::playback::kNotifyCommand) {
            sonos::playback::drain_commands(speaker);
            did_cmd = true;
        }

        // Volume — 80ms quiet-window debounce; 150ms cap so audio tracks during sustained spin
        if (bits & sonos::playback::kNotifyVolume) {
            uint32_t spin_start = millis();
            uint32_t extra;
            while (xTaskNotifyWait(0, UINT32_MAX, &extra, pdMS_TO_TICKS(80)) == pdTRUE) {
                bits |= extra;
                if (!(extra & sonos::playback::kNotifyVolume)) break;
                if (millis() - spin_start >= 150) break;
            }
            // Handle any command that arrived during the debounce window
            if (bits & sonos::playback::kNotifyCommand) {
                sonos::playback::drain_commands(speaker);
                did_cmd = true;
            }
            sonos::playback::flush_volume(speaker);
            did_volume = true;
        }

        if (did_volume || did_cmd) sonos::playback::notify_poll_task();
    }
}

/* ── Sonos task (core 0) ────────────────────────────────────────────────────
 *
 * Runs SSDP discovery + zone-group topology refresh on boot, then repeats
 * every kDiscoveryIntervalMs.  Results are published to sonos::state (mutex-
 * protected); the UI task reads state via sonos::state::snapshot().
 */
static void sonos_task(void* /*arg*/) {
    // Register this task as the boost-repoll target; cmd_task notifies us after a command
    sonos::playback::set_poll_notify_task(xTaskGetCurrentTaskHandle());

    uint8_t  s_volume_skip       = 0;
    uint32_t s_last_heap_log_ms  = 0;
    uint32_t s_last_pos_resync_ms = 0;    // event mode: last GetPositionInfo resync
    uint32_t s_home_room_unresolved_since_ms = 0;  // 0 = resolved; else when it first went missing
    bool     s_favorites_loaded  = false; // latches once fetch succeeds — avoids
                                           // a FavoritesState deep-copy every poll

    for (;;) {
        sonos::discovery::run_cycle();

        // Heap observability for long-unattended runs: logging only, never acts on
        // this — a slow leak should be diagnosable from serial history, not papered
        // over by a self-triggered reboot.
        if (millis() - s_last_heap_log_ms >= 60000) {
            s_last_heap_log_ms = millis();
            Serial.printf("[Heap] free=%u min_free=%u\n",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());
        }
        SonosState st = sonos::state::snapshot();
        uint32_t cycle_start_ms = millis();
        while (millis() - cycle_start_ms < sonos::discovery::kDiscoveryIntervalMs) {
            if (st.home_room_idx >= 0) {
                s_home_room_unresolved_since_ms = 0;  // resolved — clear retry backoff
                const SpeakerInfo& hr = st.speakers[st.home_room_idx];

                // When home room is a group member, poll the coordinator for track info.
                // Members mirror transport state but their GetPositionInfo returns
                // x-rincon:<coordinator> instead of actual track metadata.
                {
                    sonos::PlaybackState pb_cur = sonos::playback::snapshot();
                    const SpeakerInfo* poll_target = &hr;
                    if (!pb_cur.group_coordinator_uuid.isEmpty() &&
                        pb_cur.group_coordinator_uuid != hr.uuid) {
                        for (const auto& sp : st.speakers) {
                            if (sp.uuid == pb_cur.group_coordinator_uuid) {
                                poll_target = &sp;
                                break;
                            }
                        }
                    }
                    if (!sonos::eventing::healthy()) {
                        // Fallback mode — GENA subscriptions down: full-state polling
                        // at the pre-eventing cadence is the sole state source.
                        sonos::playback::poll(*poll_target);
                        // Volume: always home room, but only every 4th cycle (saves ~200ms/cycle)
                        // TODO(cleanup): skip poll_volume when poll_target == &hr (standalone) —
                        //   poll() already called GetVolume on the same speaker that cycle, making
                        //   this a redundant 2-SOAP-call round-trip; only needed for group members.
                        if (++s_volume_skip >= 4) {
                            s_volume_skip = 0;
                            const SpeakerInfo* vol_src = (poll_target != &hr) ? &hr : poll_target;
                            sonos::playback::poll_volume(*vol_src);
                        }
                    } else {
                        // Event mode — transport/metadata/volume/mute arrive by push.
                        // The only residual poll is a position resync: position is not
                        // evented. Runs on evented transport/track changes (flag), after
                        // user commands (boost wakes this loop), and every 30s while
                        // Playing to correct interpolation drift.
                        bool resync = sonos::playback::take_position_resync();
                        bool playing = pb_cur.transport_state == sonos::TransportState::Playing;
                        if (!resync && playing &&
                            millis() - s_last_pos_resync_ms >= 30000) {
                            resync = true;
                        }
                        if (resync) {
                            sonos::playback::poll_position(*poll_target);
                            s_last_pos_resync_ms = millis();
                        }
                    }
                }

                // Fetch favorites after each successful poll until it succeeds once.
                // Retrying on every cycle (instead of a fire-once flag set before the
                // call) means a failed first attempt — e.g. a transient hiccup during
                // boot — doesn't abandon favorites permanently. The local latch avoids
                // re-checking favorites::snapshot() (a full FavoritesState deep-copy)
                // on every ~2s poll for the rest of the device's uptime once loaded.
                if (!s_favorites_loaded) {
                    favorites::fetch(hr);
                    s_favorites_loaded = favorites::snapshot().fetched;
                }

                // Signal art_task when URI changes — art_task runs at lower priority
                {
                    sonos::PlaybackState pb  = sonos::playback::snapshot();
                    ArtState             art_cur = art::snapshot();
                    if (!pb.album_art_uri.isEmpty() && pb.album_art_uri != art_cur.uri) {
                        art::notify_new_uri();
                    }
                }
                // Use task notification instead of plain delay: enqueue_command()
                // notifies us, so we wake immediately when a command arrives instead
                // of waiting the full poll interval.
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(sonos::playback::poll_interval_ms()));
                // Re-snapshot so a freshly resolved home room is picked up mid-cycle
                st = sonos::state::snapshot();
            } else {
                // Home room absent from this discovery cycle (e.g. a battery Sonos
                // Roam that was asleep). Retry discovery on a short cadence — fast at
                // first, then backing off — instead of idling the full
                // kDiscoveryIntervalMs, so the controller recovers within seconds of
                // the speaker rejoining. Break to re-run discovery immediately.
                if (s_home_room_unresolved_since_ms == 0)
                    s_home_room_unresolved_since_ms = millis();
                uint32_t unresolved_for = millis() - s_home_room_unresolved_since_ms;
                uint32_t retry_ms =
                    (unresolved_for < sonos::discovery::kHomeRoomFastWindowMs)
                        ? sonos::discovery::kHomeRoomFastRetryMs
                        : sonos::discovery::kHomeRoomSlowRetryMs;
                Serial.printf("[Playback] no home room — retrying discovery in %lus\n",
                              (unsigned long)(retry_ms / 1000));
                vTaskDelay(pdMS_TO_TICKS(retry_ms));
                break;
            }
        }
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

    lv_obj_t* scr = screens::now_playing_create();
    lv_screen_load(scr);
    s_active_scr = scr;
    screens::now_playing_init_art();

    bsp::backlight(200);

    // Event loop
    for (;;) {
        // If idle and a touch is active, wake the display and suppress this touch
        // (handled in the BSP touch callback via now_playing_consume_wake_touch()).
        if (screens::now_playing_is_idle()) {
            lv_indev_t* indev = lv_indev_get_next(nullptr);
            if (indev && lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
                screens::now_playing_wake_from_touch();
            }
        }

        uint32_t next_ms = lv_timer_handler();
        screenshot::poll(); // services pending screenshot requests from web_task

        // Drain input events.
        bsp::InputEvent evt;
        while (bsp::pop_event(evt)) {
            switch (evt.intent) {
                case bsp::Intent::VolumeDelta: {
                    if (nav::has_modal()) {
                        nav::navigate(-evt.value);
                    } else {
                        sonos::playback::enqueue_volume_delta(-evt.value);  // negate: CW = up; updates s_desired_volume
                        sonos::PlaybackState pb = sonos::playback::snapshot();  // returns desired volume
                        screens::now_playing_update_volume_arc((int)pb.volume);
                    }
                    break;
                }
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
                    if (millis() - s_btn_down_ms < prefs::kMuteShortPressMs) {
                        if (nav::has_modal()) {
                            if (nav::has_selection()) nav::select();
                        } else {
                            sonos::PlaybackState pb = sonos::playback::snapshot();
                            bool playing = pb.transport_state == sonos::TransportState::Playing;
                            sonos::playback::enqueue_command(
                                playing ? sonos::PlaybackCommand::Pause : sonos::PlaybackCommand::Play);
                            sonos::playback::optimistic_transport(
                                playing ? sonos::TransportState::Paused : sonos::TransportState::Playing);
                            screens::now_playing_dim_transport();
                        }
                    }
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

        // Update now-playing screen when playback state changes
        {
            sonos::PlaybackState pb = sonos::playback::snapshot();
            if (pb.version != s_playback_version) {
                s_playback_version = pb.version;
                screens::now_playing_update(pb);
            }
        }

        // Apply freshly decoded album art independently of playback-state
        // changes. Under eventing, pb.version can stay quiescent for tens of
        // seconds between events, so gating art application on it left a
        // decoded frame unshown until the next bump. This is idempotent and
        // cheap when the art version is unchanged.
        screens::now_playing_refresh_art();

        // Diagnose connectivity faults and show/hide the fault status layer
        // (no WiFi / no Sonos / home room offline), with silent auto-recovery.
        // Cheap and internally throttled; safe to call every tick on the UI task.
        screens::fault_overlay_tick();

        // Yield up to one LVGL refresh period. Encoder/button events are ISR-driven
        // so nothing is missed; LVGL governs render cadence via LV_DEF_REFR_PERIOD.
        uint32_t delay_ms = (next_ms < 1) ? 1 : (next_ms > 16 ? 16 : next_ms);
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
        // The boot-race that setAutoReconnect(false) guarded against is over — the
        // initial WiFi.begin() has already completed. Re-enable it so the IDF
        // supplicant automatically recovers from AP reboots, roaming, and other
        // drops for the remaining weeks/months of uptime. discovery.cpp adds a
        // throttled WiFi.reconnect() backstop for disconnect reasons auto-reconnect
        // doesn't cover.
        WiFi.setAutoReconnect(true);
        Serial.printf("[WiFi] connected, IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WiFi] connection failed — web services unavailable");
    }

    // Web services (screenshot + OTA) — only when WiFi is up
    if (s_wifi_ok) {
        screenshot::init();
        screenshot::register_handler(s_web_server);
        ota::register_handler(s_web_server);
        rooms::register_handlers(s_web_server);
        s_web_server.begin();
    }

    // Worker → UI queue
    s_worker_to_ui = xQueueCreate(kWorkerQueueDepth, kWorkerQueueItemBytes);

    // Shared Sonos state mutex — must exist before any task that reads state
    sonos::state::init();
    sonos::soap::init();       // SOAP serialization mutex — before any SOAP-calling task
    sonos::playback::init();
    sonos::eventing::init();   // NOTIFY body buffer (PSRAM)
    art::init();
    favorites::init();
    art::start_task();

    // Spawn tasks. cmd_task first (priority 4) so its notify handle is registered
    // before the UI task can enqueue any command.
    xTaskCreatePinnedToCore(cmd_task, "Cmd", kCmdTaskStackBytes,
                            nullptr, kCmdTaskPriority, nullptr, 0);
    xTaskCreatePinnedToCore(sonos_task, "Sonos", kSonosTaskStackBytes,
                            nullptr, kSonosTaskPriority, nullptr, 0);
    xTaskCreatePinnedToCore(ui_task, "UI", kUiTaskStackBytes,
                            nullptr, kUiTaskPriority, nullptr, 1);
    if (s_wifi_ok) {
        xTaskCreatePinnedToCore(web_task, "Web", kWebTaskStackBytes,
                                nullptr, kSonosTaskPriority, nullptr, 0);
    }
    // GENA event subscriptions (core 0, prio 3). Started unconditionally — the
    // task idles until WiFi and a home room exist; sonos_task polls until
    // eventing::healthy() flips true.
    sonos::eventing::start_task();

    // main task (Arduino's setup/loop) is not needed after this point
}

void loop() {
    // Worker and UI are FreeRTOS tasks; loop() just yields forever.
    vTaskDelay(portMAX_DELAY);
}
