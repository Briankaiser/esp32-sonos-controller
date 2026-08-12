#include "eventing.h"
#include "gena_parse.h"
#include "xml_util.h"
#include "state.h"
#include "playback.h"
#include "discovery.h"
#include "soap.h"
#include "art.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <atomic>
#include <esp_heap_caps.h>

// ── Tunables ──────────────────────────────────────────────────────────────────

static constexpr uint32_t kNotifyBodyCap     = 32 * 1024;  // PSRAM buffer; larger bodies drained+discarded
static constexpr uint32_t kRequestedLeaseS   = 3600;       // TIMEOUT: Second-3600 requested
static constexpr uint32_t kSubscribeTimeoutMs = 5000;      // per SUBSCRIBE/UNSUBSCRIBE HTTP exchange
static constexpr uint32_t kNotifyReadTimeoutMs = 3000;     // per inbound NOTIFY read
static constexpr uint32_t kBackoffMs[]       = { 5000, 15000, 60000 };  // subscribe retry ladder

// ── Subscription table ────────────────────────────────────────────────────────

enum class Service : uint8_t { AVTransport, RenderingControl, ZoneGroupTopology };
static constexpr uint8_t kNumServices = 3;

static const char* service_name(Service s) {
    switch (s) {
        case Service::AVTransport:       return "AVTransport";
        case Service::RenderingControl:  return "RenderingControl";
        default:                         return "ZoneGroupTopology";
    }
}

static const char* event_path(Service s) {
    switch (s) {
        case Service::AVTransport:       return "/MediaRenderer/AVTransport/Event";
        case Service::RenderingControl:  return "/MediaRenderer/RenderingControl/Event";
        default:                         return "/ZoneGroupTopology/Event";
    }
}

struct Sub {
    // Desired target (recomputed each loop from shared state)
    String   target_uuid;
    String   ip;
    uint16_t port = 0;
    // Live subscription
    bool     active = false;
    String   sid;
    uint32_t renew_at_ms = 0;
    // Failure backoff
    uint8_t  fail_count  = 0;
    uint32_t retry_at_ms = 0;
};

static Sub               s_subs[kNumServices];        // event_task only
static std::atomic<bool> s_healthy{false};
static std::atomic<bool> s_reset_pending{false};
static char*             s_body_buf = nullptr;      // kNotifyBodyCap, PSRAM
static WiFiServer        s_server(sonos::eventing::kNotifyPort);
static bool              s_server_started = false;
static IPAddress         s_local_ip;                // callback IP the SIDs were registered with

// Wrap-safe deadline check (same idiom as soap.cpp / discovery.cpp)
static inline bool time_before(uint32_t deadline) {
    return (int32_t)(millis() - deadline) < 0;
}

// ── Public interface ──────────────────────────────────────────────────────────

void sonos::eventing::init() {
    s_body_buf = (char*)heap_caps_malloc(kNotifyBodyCap + 1, MALLOC_CAP_SPIRAM);
    if (!s_body_buf) {
        // PSRAM exhausted — fall back to internal heap; report loudly either way
        s_body_buf = (char*)malloc(kNotifyBodyCap + 1);
        Serial.println("[GENA] PSRAM alloc failed for NOTIFY buffer — using internal heap");
    }
}

bool sonos::eventing::healthy() {
    return s_healthy.load(std::memory_order_relaxed);
}

void sonos::eventing::request_reset() {
    s_reset_pending.store(true, std::memory_order_relaxed);
}

// ── HTTP client for SUBSCRIBE / UNSUBSCRIBE ───────────────────────────────────
//
// GENA verbs aren't SOAP, so soap::call can't carry them; this reads only the
// response head (GENA responses have no meaningful body). Serialized with SOAP
// via soap::net_lock so no two speaker connections are ever open at once.

static String gena_request(const String& ip, uint16_t port, const String& request_head) {
    sonos::soap::net_lock();

    WiFiClient client;
    client.setTimeout(kSubscribeTimeoutMs);
    if (!client.connect(ip.c_str(), port, kSubscribeTimeoutMs)) {
        Serial.printf("[GENA] connect failed %s:%u\n", ip.c_str(), (unsigned)port);
        sonos::soap::net_unlock();
        return "";
    }
    client.print(request_head);

    // Read the response head (through the blank line), yielding while waiting.
    String   head;
    uint32_t deadline   = millis() + kSubscribeTimeoutMs;
    bool     last_was_nl = false;
    while (time_before(deadline)) {
        if (client.available()) {
            char c = (char)client.read();
            if (c == '\r') continue;
            head += c;
            if (c == '\n') {
                if (last_was_nl) break;  // blank line — end of head
                last_was_nl = true;
            } else {
                last_was_nl = false;
            }
        } else if (client.connected()) {
            delay(1);
        } else {
            break;
        }
    }
    client.stop();
    sonos::soap::net_unlock();
    return head;
}

static bool subscribe(Service svc, Sub& sub) {
    String callback = "http://" + s_local_ip.toString() + ":" +
                      String(sonos::eventing::kNotifyPort) + "/notify";
    String req =
        "SUBSCRIBE " + String(event_path(svc)) + " HTTP/1.1\r\n"
        "HOST: " + sub.ip + ":" + String(sub.port) + "\r\n"
        "CALLBACK: <" + callback + ">\r\n"
        "NT: upnp:event\r\n"
        "TIMEOUT: Second-" + String(kRequestedLeaseS) + "\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    String head = gena_request(sub.ip, sub.port, req);
    sonos::gena::SubscribeResponse resp = sonos::gena::parse_subscribe_response(head);
    if (!resp.ok) {
        Serial.printf("[GENA] SUBSCRIBE %s @%s failed (HTTP %d)\n",
                      service_name(svc), sub.ip.c_str(), resp.status);
        return false;
    }
    sub.active      = true;
    sub.sid         = resp.sid;
    sub.renew_at_ms = sonos::gena::renew_at_ms(millis(), resp.timeout_s);
    sub.fail_count  = 0;
    Serial.printf("[GENA] SUBSCRIBE %s @%s ok — lease %us sid=%s\n",
                  service_name(svc), sub.ip.c_str(),
                  (unsigned)resp.timeout_s, resp.sid.c_str());
    return true;
}

static bool renew(Service svc, Sub& sub) {
    String req =
        "SUBSCRIBE " + String(event_path(svc)) + " HTTP/1.1\r\n"
        "HOST: " + sub.ip + ":" + String(sub.port) + "\r\n"
        "SID: " + sub.sid + "\r\n"
        "TIMEOUT: Second-" + String(kRequestedLeaseS) + "\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    String head = gena_request(sub.ip, sub.port, req);
    sonos::gena::SubscribeResponse resp = sonos::gena::parse_subscribe_response(head);
    if (resp.status == 200) {
        // Renewal responses may or may not echo the SID; the lease is what matters.
        sub.renew_at_ms = sonos::gena::renew_at_ms(millis(), resp.timeout_s);
        return true;
    }
    // 412 Precondition Failed = the speaker no longer knows this SID (reboot).
    Serial.printf("[GENA] renew %s @%s failed (HTTP %d) — resubscribing\n",
                  service_name(svc), sub.ip.c_str(), resp.status);
    sub.active = false;
    sub.sid    = "";
    return false;
}

static void unsubscribe(Service svc, Sub& sub) {
    if (!sub.active || sub.sid.isEmpty()) return;
    String req =
        "UNSUBSCRIBE " + String(event_path(svc)) + " HTTP/1.1\r\n"
        "HOST: " + sub.ip + ":" + String(sub.port) + "\r\n"
        "SID: " + sub.sid + "\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    gena_request(sub.ip, sub.port, req);  // best effort — result doesn't matter
    sub.active = false;
    sub.sid    = "";
}

// ── Target resolution ─────────────────────────────────────────────────────────
//
// AVTransport must be sourced from the group coordinator (members only mirror
// transport and hide track metadata); RenderingControl and topology come from
// the home room itself.

static bool resolve_targets(String desired_uuid[kNumServices],
                            String desired_ip[kNumServices],
                            uint16_t desired_port[kNumServices]) {
    SonosState st = sonos::state::snapshot();
    if (st.home_room_idx < 0 || st.home_room_idx >= (int)st.speakers.size()) return false;
    const SpeakerInfo& hr = st.speakers[st.home_room_idx];

    const SpeakerInfo* av_target = &hr;
    sonos::PlaybackState pb = sonos::playback::snapshot();
    if (!pb.group_coordinator_uuid.isEmpty() && pb.group_coordinator_uuid != hr.uuid) {
        for (const auto& sp : st.speakers) {
            if (sp.uuid == pb.group_coordinator_uuid) { av_target = &sp; break; }
        }
    }

    desired_uuid[0] = av_target->uuid;  desired_ip[0] = av_target->ip;  desired_port[0] = av_target->port;
    desired_uuid[1] = hr.uuid;          desired_ip[1] = hr.ip;          desired_port[1] = hr.port;
    desired_uuid[2] = hr.uuid;          desired_ip[2] = hr.ip;          desired_port[2] = hr.port;
    return true;
}

// ── NOTIFY handling ───────────────────────────────────────────────────────────

// Reads the head+body of an inbound NOTIFY. Returns false if the request is
// unusable. `body_len` is 0 when the body was oversized and drained.
static bool read_notify(WiFiClient& client, sonos::gena::NotifyHead& head_out,
                        uint32_t& body_len, bool& oversized) {
    uint32_t deadline = millis() + kNotifyReadTimeoutMs;

    // Head: accumulate until blank line
    String head;
    bool last_was_nl = false;
    while (time_before(deadline)) {
        if (client.available()) {
            char c = (char)client.read();
            if (c == '\r') continue;
            head += c;
            if (c == '\n') {
                if (last_was_nl) break;
                last_was_nl = true;
            } else {
                last_was_nl = false;
            }
        } else if (client.connected()) {
            delay(1);
        } else {
            break;
        }
    }

    head_out = sonos::gena::parse_notify_head(head);
    if (!head_out.ok) return false;

    // Body: bounded read into the shared buffer. Sonos sends Content-Length;
    // chunked or unknown length falls back to read-until-close within the cap.
    body_len  = 0;
    oversized = false;
    uint32_t want = head_out.content_length;
    if (want > kNotifyBodyCap) oversized = true;

    while (time_before(deadline)) {
        int av = client.available();
        if (av > 0) {
            if (oversized || body_len >= kNotifyBodyCap) {
                // Drain without storing so the speaker sees a complete read + ACK
                char sink[256];
                int drop = av > (int)sizeof(sink) ? (int)sizeof(sink) : av;
                client.readBytes(sink, drop);
                oversized = true;
            } else {
                uint32_t room = kNotifyBodyCap - body_len;
                uint32_t take = (uint32_t)av > room ? room : (uint32_t)av;
                size_t got = client.readBytes(s_body_buf + body_len, take);
                body_len += (uint32_t)got;
            }
            if (want > 0 && !oversized && body_len >= want) break;  // complete
        } else if (client.connected()) {
            if (want > 0 && body_len >= want) break;
            delay(1);
        } else {
            break;
        }
    }
    s_body_buf[body_len] = '\0';
    return true;
}

static void dispatch_notify(const sonos::gena::NotifyHead& head, bool oversized) {
    // Match SID against the active table — stale/unknown senders are discarded.
    int svc_idx = -1;
    for (uint8_t i = 0; i < kNumServices; i++) {
        if (s_subs[i].active && s_subs[i].sid == head.sid) { svc_idx = (int)i; break; }
    }
    if (svc_idx < 0) {
        Serial.printf("[GENA] NOTIFY with unknown SID %s — discarded\n", head.sid.c_str());
        return;
    }
    Service svc = static_cast<Service>(svc_idx);

    if (oversized) {
        Serial.printf("[GENA] NOTIFY %s oversized — discarded\n", service_name(svc));
        // Topology payloads are the only ones that can realistically exceed the
        // cap; recover the update through the SOAP fetch path instead.
        if (svc == Service::ZoneGroupTopology) sonos::discovery::refresh_topology_via_soap();
        return;
    }

    String body(s_body_buf);

    switch (svc) {
        case Service::AVTransport: {
            sonos::gena::AVTransportEvent ev = sonos::gena::parse_avtransport_event(body);
            if (!ev.has_transport && !ev.has_metadata && !ev.has_duration) return;
            String prev_art = sonos::playback::snapshot().album_art_uri;
            sonos::playback::ingest_event_transport(ev);
            // Nudge art_task on art change (same pattern sonos_task uses)
            if (ev.has_metadata && !ev.art_uri.isEmpty() && ev.art_uri != prev_art) {
                art::notify_new_uri();
            }
            break;
        }
        case Service::RenderingControl: {
            sonos::gena::RenderingControlEvent ev = sonos::gena::parse_rendering_control_event(body);
            if (!ev.has_volume && !ev.has_mute) return;
            sonos::playback::ingest_event_rendering(ev);
            break;
        }
        case Service::ZoneGroupTopology: {
            String zgs = sonos::gena::extract_zone_group_state(body);
            if (zgs.isEmpty()) return;
            sonos::discovery::ingest_topology_xml(zgs);
            break;
        }
    }
}

// Accepts and processes at most one inbound NOTIFY. The 200 ACK is written the
// moment the body is drained, before any parsing, so the speaker never times
// out waiting on our parse work. One-at-a-time processing bounds sockets and
// memory; the listener backlog briefly queues concurrent senders.
static void service_notify_listener() {
    WiFiClient client = s_server.available();
    if (!client) return;

    sonos::gena::NotifyHead head;
    uint32_t body_len  = 0;
    bool     oversized = false;
    bool ok = read_notify(client, head, body_len, oversized);

    // ACK unconditionally — even malformed or oversized notifications are
    // acknowledged so the speaker doesn't drop the subscription.
    client.print("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    client.stop();

    if (ok) dispatch_notify(head, oversized);
}

// ── Health ────────────────────────────────────────────────────────────────────

static void update_health() {
    bool now_healthy = s_subs[0].active && s_subs[1].active && s_subs[2].active;
    bool was_healthy = s_healthy.exchange(now_healthy, std::memory_order_relaxed);
    if (now_healthy != was_healthy) {
        if (now_healthy) {
            Serial.println("[GENA] eventing healthy — steady-state polling suspended");
            // Position needs an immediate resync: state will now arrive by push,
            // but the elapsed position was last anchored by a poll.
            sonos::playback::notify_poll_task();
        } else {
            Serial.println("[GENA] eventing unhealthy — falling back to polling");
            // Wake sonos_task immediately so it starts full polling now rather
            // than sleeping until the current poll_interval_ms expires.
            sonos::playback::notify_poll_task();
        }
    }
}

static void drop_all_subscriptions(const char* reason) {
    for (uint8_t i = 0; i < kNumServices; i++) {
        s_subs[i].active      = false;
        s_subs[i].sid         = "";
        s_subs[i].fail_count  = 0;
        s_subs[i].retry_at_ms = 0;
    }
    Serial.printf("[GENA] subscriptions dropped: %s\n", reason);
    update_health();
}

// ── event_task ────────────────────────────────────────────────────────────────

static void event_task(void* /*arg*/) {
    for (;;) {
        // Network gate: no WiFi → nothing to do; IP change → SIDs registered a
        // dead callback address, drop and resubscribe with the new one.
        if (WiFi.status() != WL_CONNECTED) {
            if (s_subs[0].active || s_subs[1].active || s_subs[2].active) {
                drop_all_subscriptions("WiFi disconnected");
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        IPAddress ip = WiFi.localIP();
        if (ip != s_local_ip) {
            if (s_local_ip != IPAddress()) drop_all_subscriptions("IP changed");
            s_local_ip = ip;
        }
        if (s_reset_pending.exchange(false, std::memory_order_relaxed)) {
            drop_all_subscriptions("home room changed");
        }
        if (!s_server_started) {
            s_server.begin();
            s_server.setNoDelay(true);
            s_server_started = true;
            Serial.printf("[GENA] NOTIFY listener on :%u\n",
                          (unsigned)sonos::eventing::kNotifyPort);
        }

        // Reconcile subscription targets with current home room / coordinator.
        String   want_uuid[kNumServices];
        String   want_ip[kNumServices];
        uint16_t want_port[kNumServices];
        bool have_home = resolve_targets(want_uuid, want_ip, want_port);

        for (uint8_t i = 0; i < kNumServices; i++) {
            Sub& sub = s_subs[i];
            Service svc = static_cast<Service>(i);

            if (!have_home) {
                if (sub.active) unsubscribe(svc, sub);
                continue;
            }

            // Coordinator / home-room change → move the subscription
            if (sub.active && sub.target_uuid != want_uuid[i]) {
                Serial.printf("[GENA] %s target %s → %s\n", service_name(svc),
                              sub.target_uuid.c_str(), want_uuid[i].c_str());
                unsubscribe(svc, sub);
                sub.retry_at_ms = 0;  // subscribe to the new target immediately
            }
            sub.target_uuid = want_uuid[i];
            sub.ip          = want_ip[i];
            sub.port        = want_port[i];

            // Establish / re-establish with backoff
            if (!sub.active) {
                if (sub.retry_at_ms == 0 || !time_before(sub.retry_at_ms)) {
                    if (subscribe(svc, sub)) {
                        // success — nothing more to do
                    } else {
                        uint8_t tier = sub.fail_count < 2 ? sub.fail_count : 2;
                        sub.retry_at_ms = millis() + kBackoffMs[tier];
                        if (sub.fail_count < 255) sub.fail_count++;
                    }
                }
                continue;
            }

            // Renewal — on failure fall through: active=false, retried next loop
            if (!time_before(sub.renew_at_ms)) {
                if (!renew(svc, sub)) sub.retry_at_ms = 0;
            }
        }

        update_health();

        // Service inbound NOTIFYs for the remainder of this ~250ms slice.
        // ACK latency is bounded by the slice granularity; the accept loop
        // runs far more often than events arrive.
        uint32_t slice_end = millis() + 250;
        while (time_before(slice_end)) {
            service_notify_listener();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void sonos::eventing::start_task() {
    xTaskCreatePinnedToCore(event_task, "GENA", 8 * 1024, nullptr,
                            /*priority=*/3, nullptr, /*core=*/0);
}
