## 1. TLS memory in PSRAM

- [x] 1.1 Install a PSRAM-first mbedTLS allocator (internal fallback) at the start of `setup()`
- [x] 1.2 Expose PSRAM-vs-fallback allocation counters

## 2. Serialize TLS sessions

- [x] 2.1 Add a bounded-wait TLS gate shared by art and lyrics fetches
- [x] 2.2 Skip the art fetch (with a log line) when the gate times out

## 3. Memory-health diagnostics

- [x] 3.1 Add heap-dip watcher, activity markers and failed-allocation callback
- [x] 3.2 Serve heap, PSRAM, device_id, uptime, dip and allocation-failure fields on `/status`
- [x] 3.3 Log the fields nightly per device (`scripts/heap_log.py`) with leak and near-OOM flags

## 4. Verification

- [x] 4.1 Both PlatformIO envs build (`crowpanel_21_round`, `ref_rect_stub`)
- [x] 4.2 `openspec validate --all --strict` is clean
- [x] 4.3 Hardware check: two CrowPanels run >1 day on the final image with zero failed allocations, zero internal TLS allocations and no fallbacks
- [x] 4.4 Add a `CHANGELOG.md` entry under `## [Unreleased]`
