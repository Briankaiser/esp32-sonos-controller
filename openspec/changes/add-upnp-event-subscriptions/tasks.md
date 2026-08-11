# Tasks: add-upnp-event-subscriptions

## 1. Phase 1 — GENA plumbing (subscribe + listen + log only)

- [ ] 1.1 Create `src/app/sonos/eventing.h/.cpp`: subscription table (service, target speaker, SID, granted lease, renewal deadline), `begin()`, `eventing_healthy()`, and serial logging of lifecycle transitions
- [ ] 1.2 Implement SUBSCRIBE / renewal (SID-based) / UNSUBSCRIBE HTTP requests with granted-TIMEOUT parsing (missing header → 300s) and backoff retry (5s/15s/60s) on failure
- [ ] 1.3 Implement raw NOTIFY listener: `WiFiServer` on dedicated port, minimal request parsing (SID/SEQ/Content-Length), single 32 KB PSRAM body buffer, immediate 200 ACK, oversized bodies drained and discarded
- [ ] 1.4 Create `event_task` (core 0, prio 3) in `main.cpp` running the accept/renewal loop; establish the three subscriptions (AVTransport@coordinator, RenderingControl@home room, ZoneGroupTopology@home room) once the home room resolves
- [ ] 1.5 Log parsed NOTIFY summaries (service, SID match, SEQ, body size) without ingesting; flash and verify against real speakers: initial SEQ-0 events arrive, renewals succeed, granted lease duration observed and noted

## 2. Phase 2 — RenderingControl ingestion (volume/mute)

- [ ] 2.1 Parse the double-encoded LastChange payload for Master volume and mute (reuse `xml_field`/`html_decode` helpers)
- [ ] 2.2 Ingest into `sonos::playback` state via the existing mutex-protected write path; verify phone-app volume changes appear on the arc within 1s
- [ ] 2.3 Add local-echo suppression: ignore inbound volume values while a SetVolume flush is pending or within ~1.5s of one (mute ingests unconditionally)

## 3. Phase 3 — AVTransport ingestion (transport + metadata)

- [ ] 3.1 Parse LastChange TransportState and CurrentTrackMetaData DIDL (title, artist, album, art URI, duration) with the existing decode helpers
- [ ] 3.2 Ingest transport/metadata into `sonos::playback`; trigger `art::notify_new_uri()` on art URI change; verify app-initiated play/pause/skip appears on-device within 1s
- [ ] 3.3 Re-target the AVTransport subscription when the home room's coordinator changes (from topology ingest or discovery): unsubscribe old, subscribe new, discard NOTIFYs with stale SIDs

## 4. Phase 4 — ZoneGroupTopology ingestion

- [ ] 4.1 Parse ZoneGroupState event bodies into the existing groups/speakers model in `sonos::state` (never dropping speakers merely absent from an event); refresh coordinator resolution
- [ ] 4.2 Oversized topology event → drain, ACK, and nudge `sonos_task` to run the existing SOAP `GetZoneGroupState` fetch instead
- [ ] 4.3 Verify: grouping/ungrouping from the phone app updates the group overlay and coordinator targeting within a few seconds

## 5. Phase 5 — Demote polling, health fallback, reconnection

- [ ] 5.1 Gate `sonos_task` steady-state polls behind `eventing_healthy()`: healthy → skip GetTransportInfo/GetVolume cycles; unhealthy → today's 2s cadence; keep `s_boost_polls` post-command boost in both modes, narrowed to position-only while healthy
- [ ] 5.1b Add recency guard on full-state poll writes: record poll-request issue time, skip the write if an event was ingested after it (prevents a slow stale poll response from overwriting fresher evented state)
- [ ] 5.2 Slim position path: `GetPositionInfo` on initial subscribe, on evented transport/track changes, after commands, and 30s resync while Playing; interpolation unchanged
- [ ] 5.3 WiFi reconnect / IP change handling: drop subscription table, resubscribe, force one full poll to cover the outage gap
- [ ] 5.4 Health watchdog end-to-end test: block the listener port (or unplug a speaker) → device falls back to 2s polling with serial log; restore → eventing resumes, polling suspends
- [ ] 5.5 Idle integration: verify subscriptions renew through an idle period longer than one lease; wake-on-play fires within ~1s of starting music from the app (standalone home room AND idle home room joined to a playing group); fallback mode keeps the 10s idle poll cadence

## 6. Verification & documentation

- [ ] 6.1 Both PlatformIO envs build (`crowpanel_21_round`, `ref_rect_stub`)
- [ ] 6.2 Soak test: leave device running ≥12h with periodic app-side changes; confirm renewals hold, no reboots, heap stable (debug overlay)
- [ ] 6.3 Socket-budget check under load: NOTIFY burst + `/screenshot` + album-art fetch concurrently, all succeed
- [ ] 6.4 `openspec validate --all --strict` clean; CHANGELOG.md entry under `[Unreleased]`
