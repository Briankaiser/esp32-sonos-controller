# Design: add-upnp-event-subscriptions

## Context

See `proposal.md — Why` for motivation. Current state relevant to the approach:

- **Tasks (core 0):** `sonos_task` (prio 3) runs discovery + the 2s poll loop; `cmd_task` (prio 4) drains user commands and nudges `sonos_task` for a boost re-poll (`s_boost_polls`, 4 × 500ms); `web_task` runs the synchronous Arduino `WebServer` on port 80 (screenshot/OTA/rooms); `art_task` fetches album art. UI is alone on core 1 and never does network I/O.
- **State stores:** `sonos::playback` (mutex-protected `PlaybackState`, written only from core-0 tasks) and `sonos::state` (speakers + groups + home room). Both already expose thread-safe snapshot/ingest patterns — eventing can write through them unchanged.
- **SOAP client:** `soap::call` opens a fresh `WiFiClient` per request, `Connection: close`.
- **Parsing helpers:** `xml_field` + `html_decode` (with smart-quote normalization) already exist in `discovery.cpp`/`playback.cpp` and handle Sonos's escaped-XML-inside-XML pattern.
- **GENA mechanics (Sonos):** `SUBSCRIBE <eventSubURL> HTTP/1.1` with `CALLBACK: <http://device-ip:port/path>`, `NT: upnp:event`, `TIMEOUT: Second-N`. Speaker responds with a `SID` and granted timeout, then immediately pushes an initial NOTIFY containing full current state (SEQ 0). Renewal = SUBSCRIBE with `SID`, no CALLBACK/NT; a speaker that lost the SID (reboot) answers 412. `AVTransport` and `RenderingControl` events wrap state in a double-encoded `LastChange` payload; `ZoneGroupTopology` events carry the full `ZoneGroupState` XML, which can be tens of KB on large households.
- **Event sourcing rules:** transport/metadata events are only meaningful from the group **coordinator** (members report `x-rincon:` URIs); volume/mute are per-device, so the home room itself. Transport events fire on change only — silence during steady playback is normal, so event *absence* cannot be used as a liveness signal. Position (`RelTime`) is **not** evented at all.

## Goals / Non-Goals

**Goals:**
- Sub-second reflection of external state changes (app, other controllers) with near-zero steady-state network traffic.
- Zero regression risk in degraded conditions: the polling path stays in the binary and takes over automatically.
- All eventing work on core 0; frame budget on core 1 untouched.

**Non-Goals:**
- `GroupRenderingControl` (group volume) eventing — group volume keeps its current SOAP fan-out.
- `ContentDirectory` (favorites) eventing — favorites remain fetch-on-boot/on-demand.
- Eventing state for rooms other than what the home room needs (its coordinator + itself + one topology source).
- Persistent/keep-alive SOAP connections (separate follow-up, already noted in the responsiveness plan).

## Decisions

### D1: Dedicated raw TCP listener for NOTIFY, not the port-80 `WebServer`

A new `WiFiServer` on a dedicated port (e.g. 3400) accepts NOTIFY callbacks with minimal hand-rolled HTTP parsing (request line, `SID`/`SEQ`/`Content-Length` headers, body).

- Why not the existing `WebServer`: Arduino's `WebServer` does not dispatch the nonstandard `NOTIFY` method to `on()` handlers (its `HTTPMethod` enum has no NOTIFY), so it would need fragile `onNotFound` tricks; and `web_task` can block for seconds during an OTA upload — which would stall NOTIFY ACKs and cause speakers to drop subscriptions mid-update.
- A raw listener gives exact control over the two things that matter: ACK latency (send `HTTP/1.1 200 OK` immediately after the body is drained) and buffer strategy.

### D2: One new task — `event_task` (core 0, priority 3)

Owns the listener socket, the subscription table, and renewal timing. Loop: `accept` with a short timeout → read request (bounded) → ACK → parse → ingest via existing `playback`/`state` write paths → check renewal deadlines. Priority 3 (same as `sonos_task`, below `cmd_task`) — user commands must still preempt event parsing.

- Alternative (parse on `sonos_task`): rejected — couples NOTIFY ACK latency to poll SOAP round-trips, exactly the coupling `cmd_task` was created to break.

### D3: Steady-state subscription set is exactly three

| Service | Target speaker | Yields |
|---|---|---|
| AVTransport | current group coordinator of home room | transport state, track metadata, art URI |
| RenderingControl | home room speaker | volume, mute |
| ZoneGroupTopology | home room speaker (any reachable speaker as fallback) | groups, coordinators, member changes |

Re-targeting: every topology ingest (event **or** periodic discovery) recomputes the home room's coordinator; if it differs from the AVTransport subscription's target, `event_task` unsubscribes the old and subscribes the new. NOTIFYs are matched by `SID` against the active table — a NOTIFY with an unknown/stale SID is ACKed and discarded, which cleanly handles late pushes from a former coordinator.

### D4: Single reusable PSRAM buffer for NOTIFY bodies, hard cap 32 KB

One `ps_malloc`'d 32 KB buffer, owned by `event_task` (single-threaded processing — one NOTIFY at a time; the listener backlog queues the rest briefly). Bodies over the cap are drained to the bitbucket and ACKed; for an oversized *topology* event the ingest is replaced by nudging `sonos_task` to run its existing `GetZoneGroupState` fetch. AVTransport/RenderingControl LastChange payloads are 1–8 KB and never realistically hit the cap.

- Socket budget check (lwIP default ~10): port-80 listener+client (2), eventing listener + 1 inbound NOTIFY (2), transient SOAP (1–2), art fetch (1), SSDP UDP (1) ≈ 8–9 worst case. Serial-processing NOTIFYs (never accept more than one concurrently) keeps this bounded.

### D5: Health model = subscription lifecycle, not event frequency

`eventing_healthy()` is true iff all three subscriptions are established and their last renewal succeeded. Event silence is *not* evidence of failure (events fire on change only), so no "no events for N seconds" staleness check. Renewal (every ~50% of the granted lease, request `Second-3600`) doubles as the liveness probe. Failure handling:

- Subscribe/renew fails → retry with backoff (5s, 15s, 60s); while any required subscription is down, `eventing_healthy()` is false.
- `sonos_task`'s loop consults `eventing_healthy()` each cycle: healthy → skip `GetTransportInfo`/`GetVolume` polls entirely; unhealthy → today's 2s cadence. The `s_boost_polls` post-command boost is **retained in both modes** as a safety net, but while eventing is healthy it is narrowed to position-only (`GetPositionInfo`) — position is not evented, so it cannot conflict with event ingestion. Full-state boost polls run only in fallback mode.
- WiFi reconnect / IP change → drop the whole table, resubscribe from scratch, then force one full poll to catch anything missed during the outage.
- **Idle interaction:** subscriptions are display-state-independent — `event_task` never consults idle, renewals continue through idle. Wake-on-play needs no new plumbing: events write `PlaybackState` through the same ingest path polls use, and the existing UI-side wake check (transport → Playing) fires on the next snapshot. Net effect: wake latency drops from ≤10s (idle poll cadence) to ~1s. The wake-time `idle_mode(false)` boost becomes position-only while healthy per the rule above — evented state is already fresh at wake; only position needs a resync. In fallback mode, idle keeps today's 10s cadence (`kIdlePollIntervalMs`).

### D6: Position keeps a slim polling path

`GetPositionInfo` runs: on the initial subscription, on every transport-state or track change delivered by event, after user commands (existing boost), and every 30s while Playing as drift resync. The existing 1s UI interpolation is unchanged. Rationale: position is not evented; this is the "when polling still makes sense" answer, and it cuts steady-state SOAP traffic from ~2 req/2s to ~1 req/30s.

### D7: Local-echo suppression for volume events

While a `SetVolume` flush is pending or within ~1.5s of one, inbound RenderingControl volume values are ignored (the encoder's optimistic arc + desired-volume model stays authoritative). Otherwise an event carrying the pre-set value arriving after a fast encoder spin would snap the arc backwards. Mute and all other fields ingest unconditionally.

## Risks / Trade-offs

- [Stale poll response overwrites a fresher event — two writers into `PlaybackState` means the mutex prevents torn writes but not out-of-order ones; right after a command (exactly when boost polls fire) the speaker is transitioning and a slow poll response can carry pre-command state, arriving after the event already ingested post-command state → display briefly snaps backwards] → Two guards: (a) healthy-mode boost polls are position-only per D5, so the conflicting fields aren't written at all; (b) any full-state poll write records the time the poll *request was issued* and is skipped if an event was ingested after that instant. Covers the fallback-transition edge (eventing recovers while a full poll is in flight).
- [Sonos grants shorter leases than requested / firmware quirks] → Renewal schedules off the *granted* `TIMEOUT` in each response, never the requested value; a missing TIMEOUT header is treated as 300s.
- [ZoneGroupState events too large on big households] → 32 KB cap + fallback to the existing SOAP topology fetch (D4); worst case behavior equals today's.
- [NOTIFY burst during rapid external volume drag] → single-threaded drain with fast ACK; if the backlog grows, later events simply overwrite earlier state — last-write-wins is correct for state snapshots.
- [Listener port collision / firewalled LAN] → subscribe failures surface immediately, watchdog keeps polling active; device is never worse than today.
- [Coordinator flaps during group edits (rapid re-targeting)] → re-target is idempotent and SID-filtered; a mid-flap NOTIFY from the "wrong" speaker is discarded, and the boost-poll after user grouping commands papers over any gap.
- [Extra task + buffer memory] → ~4 KB task stack + 32 KB PSRAM; PSRAM is 8 MB, negligible. Internal RAM impact is the task stack only.
- [Complexity: two state-update paths to reason about] → both paths funnel through the same ingest functions on `playback`/`state`; the poll path is unmodified code, only gated.

## Migration Plan

Implement in gated phases; each phase builds and runs on hardware with the previous behavior intact:

1. **Listener + subscribe + log only** — subscriptions established, NOTIFYs parsed and logged to serial, nothing ingested. Polling untouched. Validates GENA mechanics against real speakers.
2. **Ingest RenderingControl** (volume/mute) — lowest-risk state, easy to verify with the phone app.
3. **Ingest AVTransport** (transport + metadata) with coordinator re-targeting.
4. **Ingest ZoneGroupTopology** + oversized-event fallback.
5. **Demote polling** — gate `sonos_task` polls behind `eventing_healthy()`, add position-resync cadence, echo suppression, reconnect handling.

Rollback at any phase = the watchdog (turn eventing off → device is byte-for-byte today's polling behavior); no config or data migration exists.

## Open Questions

- Exact listener port (3400 vs something else) — pick at implementation; anything unused above 1024 works.
- Whether Sonos S2 firmware reliably grants `Second-3600` or clamps lower — measure in phase 1 on real hardware and let the renewal math absorb it.
- Whether the topology subscription should prefer the home room or the current coordinator — phase 1 logging will show which is more stable; either satisfies the spec.
