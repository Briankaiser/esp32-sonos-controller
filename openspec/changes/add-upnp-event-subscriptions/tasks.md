# Tasks: add-upnp-event-subscriptions

> **Status note (2026-08-11):** All code was implemented and compile-verified
> without a physical device. Unchecked tasks below are exactly the on-device
> verification steps — they need the CrowPanel + real speakers. The GENA
> parsers are covered host-side by `pio test -e test_native` (test_gena, 27
> tests), and `tools/mock_gena_speaker.py` (self-tested) stands in for a
> speaker to exercise subscribe/renew/notify against the device later.

## 1. Phase 1 — GENA plumbing (subscribe + listen + log only)

- [x] 1.1 Create `src/app/sonos/eventing.h/.cpp`: subscription table (service, target speaker, SID, granted lease, renewal deadline), `init()`, `eventing::healthy()`, and serial logging of lifecycle transitions
- [x] 1.2 Implement SUBSCRIBE / renewal (SID-based) / UNSUBSCRIBE HTTP requests with granted-TIMEOUT parsing (missing header → 300s) and backoff retry (5s/15s/60s) on failure
- [x] 1.3 Implement raw NOTIFY listener: `WiFiServer` on dedicated port (3400), minimal request parsing (SID/SEQ/Content-Length), single 32 KB PSRAM body buffer, immediate 200 ACK, oversized bodies drained and discarded
- [x] 1.4 Create `event_task` (core 0, prio 3) in `main.cpp` running the accept/renewal loop; establish the three subscriptions (AVTransport@coordinator, RenderingControl@home room, ZoneGroupTopology@home room) once the home room resolves
- [ ] 1.5 **[hardware]** Flash and verify against real speakers: initial SEQ-0 events arrive, renewals succeed, granted lease duration observed and noted (device-side logging is in place; `tools/mock_gena_speaker.py` available for bench testing)

## 2. Phase 2 — RenderingControl ingestion (volume/mute)

- [x] 2.1 Parse the double-encoded LastChange payload for Master volume and mute (`gena_parse.cpp`, host-tested)
- [x] 2.2 Ingest into `sonos::playback` state via the existing mutex-protected write path (`ingest_event_rendering`) — **[hardware]** verify phone-app volume changes appear on the arc within 1s
- [x] 2.3 Add local-echo suppression: inbound volume ignored while a SetVolume flush is pending or within ~1.5s of one when values differ; confirming values retire the optimistic desired volume (mute ingests unconditionally)

## 3. Phase 3 — AVTransport ingestion (transport + metadata)

- [x] 3.1 Parse LastChange TransportState and CurrentTrackMetaData DIDL (title, artist, album, art URI, duration) — host-tested including triple-encoded entities
- [x] 3.2 Ingest transport/metadata into `sonos::playback` (`ingest_event_transport`, with the same optimistic-transport guard as polls); trigger `art::notify_new_uri()` on art URI change — **[hardware]** verify app-initiated play/pause/skip appears on-device within 1s
- [x] 3.3 Re-target the AVTransport subscription when the home room's coordinator changes: unsubscribe old, subscribe new, discard NOTIFYs with stale SIDs

## 4. Phase 4 — ZoneGroupTopology ingestion

- [x] 4.1 Parse ZoneGroupState event bodies via the shared topology parser (extracted from discovery.cpp; never drops speakers absent from an event); refresh coordinator resolution (`discovery::ingest_topology_xml`)
- [x] 4.2 Oversized topology event → drain, ACK, fall back to SOAP fetch (`discovery::refresh_topology_via_soap`)
- [ ] 4.3 **[hardware]** Verify: grouping/ungrouping from the phone app updates the group overlay and coordinator targeting within a few seconds

## 5. Phase 5 — Demote polling, health fallback, reconnection

- [x] 5.1 Gate `sonos_task` steady-state polls behind `eventing::healthy()`: healthy → skip GetTransportInfo/GetVolume cycles; unhealthy → today's 2s cadence; `s_boost_polls` boost retained in both modes, position-only while healthy
- [x] 5.1b Recency guard on full-state poll writes: poll snapshots the event-ingest counter at entry and discards its result if an event landed mid-flight
- [x] 5.2 Slim position path: `poll_position` (GetPositionInfo only) on evented transport/track changes (resync flag + notify), after commands (boost wakes the loop), and 30s resync while Playing; interpolation unchanged
- [x] 5.3 WiFi reconnect / IP change handling: drop subscription table, resubscribe with the new callback address; polling fallback covers the outage gap automatically (unhealthy until resubscribed), initial SEQ-0 events restore full state
- [ ] 5.4 **[hardware]** Health watchdog end-to-end test: block the listener port (or unplug a speaker) → device falls back to 2s polling with serial log; restore → eventing resumes, polling suspends
- [ ] 5.5 **[hardware]** Idle integration: verify subscriptions renew through an idle period longer than one lease; wake-on-play fires within ~1s of starting music from the app (standalone home room AND idle home room joined to a playing group); fallback mode keeps the 10s idle poll cadence

## 6. Verification & documentation

- [x] 6.1 Both PlatformIO envs build (`crowpanel_21_round`, `ref_rect_stub`); host parser tests pass (`pio test -e test_native`, test_gena 27/27)
- [ ] 6.2 **[hardware]** Soak test: leave device running ≥12h with periodic app-side changes; confirm renewals hold, no reboots, heap stable (debug overlay)
- [ ] 6.3 **[hardware]** Socket-budget check under load: NOTIFY burst + `/screenshot` + album-art fetch concurrently, all succeed
- [x] 6.4 `openspec validate --all --strict` clean; CHANGELOG.md entry under `[Unreleased]`
