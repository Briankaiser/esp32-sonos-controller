## Context

Internal RAM on the ESP32-S3 is ~80 KB free at idle and is shared by WiFi, lwIP,
DMA bounce buffers and everything else. The prebuilt Arduino-ESP32 3.3.11 / IDF
5.5.5 libraries pin mbedTLS to internal RAM (`CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC`),
so each HTTPS session took tens of KB of it for its lifetime. See proposal.md
for the observed failures.

## Goals / Non-Goals

**Goals:**
- No failed allocations and no near-exhaustion from TLS fetches in normal use.
- Keep using the stock PlatformIO/pioarduino toolchain — no custom IDF build.
- Make memory health observable remotely so regressions are caught.

**Non-Goals:**
- Eliminating TLS. Art from Sonos' own proxy is already plain HTTP.
- Fixing every internal-RAM dip (see Open Questions).

## Decisions

**Swap the mbedTLS allocator at runtime** with `mbedtls_platform_set_calloc_free()`
(exported by the prebuilt libs) instead of rebuilding IDF with
`CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC`. Same effect, no toolchain fork. Called first
thing in `setup()`, before any TLS use. Alternative rejected: custom sdkconfig
build — large maintenance cost for no extra benefit.

**Send every allocation to PSRAM, not only large ones.** A first version moved
only allocations ≥512 B (the 16 KB record buffers). Soak data showed ~33 KB of
small handshake/x509 allocations per session stayed internal and still dipped
the heap; routing everything removed all internal TLS allocation. Falls back to
internal RAM when PSRAM is full so TLS degrades rather than fails.

**Serialize TLS sessions with a mutex** (30 s bounded wait). Overlapping an art
fetch with a lyrics fetch doubled the peak. Art that cannot get the gate is
skipped for that track (not retried); lyrics simply return nothing.

**Do not proxy HTTPS art through the speaker.** Tested against real speakers:
`/getaa` is keyed on the track URI, returns 404 for an absolute image URL, and
returns 404 for Spotify Connect (`x-sonos-vli:`) URIs — exactly the cases that
need TLS. Relative `/getaa` art is already fetched over plain HTTP.

**Diagnostics are a 200 ms low-priority poller**, not hooks in the allocator,
so they cannot slow allocation. Activity markers are RAII scopes with per-bit
counters. The failed-allocation callback prints with ROM printf and stores
only trivially-copyable data.

## Risks / Trade-offs

- PSRAM is slower than internal RAM; handshakes take somewhat longer. Art
  fetch+decode remained ~1.2 s on hardware, within the existing budget.
- Hardware AES/SHA operate on PSRAM buffers; verified working on device
  (sessions complete, art decodes, zero failures over multi-day soak).
- A 200 ms poll can miss a spike shorter than the poll interval; the
  all-time `heap_internal_min_free` low-water mark still captures it.

## Open Questions

- Kitchen still shows brief (~200 ms) dips to ~1.6 KB during `art_fetch+art_tls`
  although no mbedTLS memory is internal any more, and none for ~21 h after the
  first hours of uptime. Suspected: WiFi/TCP receive buffers on a weak link.
  Tracked in `docs/investigations/kitchen-art-fetch-heap-dip.md`; not part of
  this change.
