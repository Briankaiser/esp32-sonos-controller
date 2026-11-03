## Why

After days of continuous operation the display flickered and album art went
missing. Instrumented devices showed why: every HTTPS fetch (album art from a
streaming CDN, lyrics) built its TLS session in internal RAM, and one session
alone drained the ~80 KB of internal heap shared with WiFi, lwIP and DMA —
dropping to a few hundred bytes free, failing allocations (212 on one device,
14 on another), and starving the display scan-out. Idle memory never leaked;
the damage came from short TLS spikes, sometimes two overlapping.

## What Changes

- All TLS session memory is placed in external PSRAM, with automatic fallback to
  internal RAM if PSRAM is exhausted. Reverses the standing rule that transport
  security memory stays internal.
- At most one TLS session is in flight at a time: an album-art fetch and a
  lyrics fetch no longer overlap.
- `GET /status` reports memory health: internal/PSRAM heap figures and low-water
  marks, uptime, a stable device identifier, failed-allocation count and last
  failure, a bounded history of heap dips with the activities in flight, and
  TLS allocation counters. Cached state only, as before.
- Scope unchanged: still direct LAN control, no Home Assistant, no Sonos
  cloud/OAuth API. Art and lyrics sources are unchanged.

## Capabilities

### New Capabilities

### Modified Capabilities
- `runtime-architecture`: transport-security memory moves to external RAM; TLS
  sessions are serialized.
- `web-services`: `/status` gains memory-health and device-identity fields.

## Impact

- New `src/services/tls_mem.{h,cpp}` (allocator hook), `tls_gate.{h,cpp}`
  (serialization), `diag.{h,cpp}` (heap-dip / failed-allocation watcher).
- `art.cpp`, `lyrics/service.cpp` take the TLS gate; `status.cpp` serves the new
  fields; `scripts/heap_log.py` logs them nightly.
- No new dependency and no IDF rebuild: the existing mbedTLS allocator hook is
  used at startup. Both PlatformIO envs affected only through shared services.
