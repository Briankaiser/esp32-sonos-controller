## Why

Both controllers still drop to under 1 KB of internal RAM during art and lyrics
fetches, and one has failed small allocations in the Art and Lyrics tasks, even
though every mbedTLS allocation now lives in PSRAM. The remaining internal
memory is used by something else (sockets and lwIP buffers, the TLS client's
own buffers, or concurrent activity). Nothing records which phase of a fetch
takes it, so the cause cannot be pinned down or fixed with confidence.

## What Changes

- Each art and lyrics fetch records how much internal RAM the controller loses
  at each phase (connected, first data read, finished), relative to the start of
  the fetch.
- `GET /status` reports, per fetch kind, the number of fetches and the largest
  loss seen at each phase since boot.
- The nightly fleet logger records the new fields.
- Diagnostic only: no change to fetch behavior, no new network traffic, no Home
  Assistant, no Sonos cloud/OAuth API.

## Capabilities

### New Capabilities

### Modified Capabilities
- `web-services`: `/status` gains a per-fetch-kind memory cost breakdown.

## Impact

- `src/services/diag.h/.cpp` (probe), `src/app/sonos/art.cpp`,
  `src/app/lyrics/service.cpp` (mark phases), `src/services/status.cpp` (serve),
  `scripts/heap_log.py` (log).
- Supports `docs/investigations/kitchen-art-fetch-heap-dip.md`.
