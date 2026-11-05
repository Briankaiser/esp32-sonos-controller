## Why

One controller (Kitchen) repeatedly drops to a few hundred bytes of internal RAM
and has dropped a networking-stack allocation, while an identical controller
(Bathroom) stays healthy. The leading hypothesis is that a weak WiFi link makes
large downloads queue data in internal RAM. Signal strength is not visible
anywhere today, so the hypothesis cannot be checked, and a poor link cannot be
spotted remotely.

## What Changes

- `GET /status` reports the WiFi signal strength (RSSI, dBm) of the current
  connection, read from state the WiFi driver already holds.
- The nightly fleet logger records it alongside the memory-health fields.
- Scope unchanged: diagnostic only, no new network traffic, no Home Assistant,
  no Sonos cloud/OAuth API.

## Capabilities

### New Capabilities

### Modified Capabilities
- `web-services`: `/status` gains a WiFi signal-strength field.

## Impact

- `src/services/status.cpp` (one field); `scripts/heap_log.py` (log the field).
- Supports `docs/investigations/kitchen-art-fetch-heap-dip.md`.
