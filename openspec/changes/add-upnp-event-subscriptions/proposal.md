# Add UPnP Event Subscriptions (GENA)

## Why

The device currently learns about Sonos state changes only by polling (`GetTransportInfo` + `GetPositionInfo` every 2s, volume every 4th cycle, topology every discovery cycle). This means: state changes initiated from other controllers (phone app, another room) appear up to 2s late; every poll costs 2–4 SOAP round-trips with fresh TCP handshakes even when nothing changed; and volume/topology changes made elsewhere can take 8s–10min to appear. Sonos speakers natively push state via UPnP GENA event subscriptions (SUBSCRIBE/NOTIFY) — the same mechanism the official app uses. Subscribing gives near-instant state updates, eliminates most steady-state polling traffic, and makes external changes (grouping from the app, volume from another controller) appear immediately.

This remains direct UPnP over the LAN — no Home Assistant, no Sonos cloud/OAuth API. GENA is served by the speakers themselves.

## What Changes

- New GENA subscription manager on core 0: subscribes to `AVTransport`, `RenderingControl`, and `ZoneGroupTopology` event endpoints on the relevant speakers; renews subscriptions before expiry; unsubscribes/resubscribes on target changes.
- New lightweight HTTP NOTIFY listener (dedicated TCP listener, not the existing port-80 `WebServer`) that accepts NOTIFY callbacks from speakers, ACKs them promptly, and hands parsed state to the existing state stores.
- Event parsing for the double-encoded `LastChange` payloads (transport state + track metadata from AVTransport; volume/mute from RenderingControl) and `ZoneGroupState` payloads (topology).
- Subscription targeting follows grouping: AVTransport events are subscribed on the current group **coordinator** (members only mirror transport); RenderingControl stays on the home room speaker. On a topology change event, the AVTransport subscription is re-targeted to the new coordinator.
- Polling is demoted, not removed:
  - Track **position** is not evented — `GetPositionInfo` still runs, but only on transport-state/track changes plus a slow periodic resync (position interpolation between resyncs is unchanged).
  - A **fallback watchdog** reverts to the current 2s polling whenever eventing is unhealthy (subscribe/renew failures, WiFi reconnect, stale-event detection) and restores eventing when it recovers.
  - SSDP discovery still runs periodically to track speaker IP changes; boost-polling after user commands is retained as a safety net.
- Reconnection handling: WiFi drop or speaker reboot invalidates subscriptions (renewal returns 412) → full resubscribe; callback IP re-registered after DHCP change.
- ESP32-S3 constraints handled explicitly: bounded number of concurrent subscriptions (≤4), NOTIFY body size caps with PSRAM buffers, prompt 200 ACK before parsing so speakers don't drop the subscription, lwIP socket budget accounted against the existing WebServer/SOAP/art usage.

## Capabilities

### New Capabilities

- `sonos-eventing`: GENA subscription lifecycle (subscribe, renew, unsubscribe, re-target on coordinator change), NOTIFY callback listener, event payload parsing, health watchdog with automatic fallback to polling, and resource bounds on the embedded target.

### Modified Capabilities

- `sonos-playback`: The primary source of transport/metadata/volume/mute state changes from periodic polling to pushed events; polling becomes (a) position resync and (b) fallback when eventing is unhealthy. The "Device Polls Playback State" requirement is rewritten accordingly.
- `sonos-discovery`: Zone-group topology SHALL additionally be updated from pushed topology events between periodic discovery cycles, so grouping changes made elsewhere appear promptly rather than at the next discovery cycle.
- `now-playing-screen`: The two idle requirements are rewritten in state-source-neutral terms — while eventing is healthy, idle state tracking comes from pushed notifications (subscriptions maintained through idle, wake-on-play within ~1s instead of up to 10s); reduced-frequency idle polling applies only while eventing is unhealthy.

## Impact

- **New code**: `src/app/sonos/eventing.{h,cpp}` (subscription manager + NOTIFY listener + parsers), likely a dedicated FreeRTOS task on core 0.
- **Modified code**: `src/main.cpp` (task creation, sonos_task poll loop becomes conditional on eventing health), `src/app/sonos/playback.{h,cpp}` (state ingestion from events, poll demotion), `src/app/sonos/discovery.cpp` (topology ingestion path shared with events), `src/app/sonos/soap.h/.cpp` (possibly shared HTTP helpers).
- **Runtime**: +1 task, +1 listening TCP socket, transient sockets for inbound NOTIFYs; heap/PSRAM for event buffers. Frame-rate budget on core 1 is untouched — all eventing work stays on core 0.
- **Risk**: This is the largest runtime change since the cmd_task split. Rollout is guarded by the fallback watchdog — if eventing misbehaves, the device degrades to today's polling behavior rather than losing state updates.
