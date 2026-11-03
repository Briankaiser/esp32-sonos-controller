# Investigation: internal-heap dips during album-art fetch (Kitchen)

Status: **open** · Opened 2026-11-03 · Follows `add-tls-psram-and-heap-diagnostics`

## Problem

With every mbedTLS allocation in PSRAM (0 internal, 0 fallbacks), the Kitchen
controller still briefly drops internal free RAM to ~1.6–1.9 KB during an
album-art HTTPS fetch. The Bathroom controller on identical firmware bottoms out
at ~7.7 KB (during `art_decode+lyrics_tls`). Failed allocations are 0 on both, so
this is margin, not an outage — but 1.6 KB leaves no headroom.

## Evidence so far

| Build | Board | Low-water | Dip activity | Notes |
|---|---|---|---|---|
| `tlsmem3` (all TLS in PSRAM) | Kitchen | 1,920 B at up=69 s | `art_fetch+art_tls+gena_notify` | 263 ms |
| same, ~1 day | Kitchen | 1,648 B at up≈0.11 d | `art_fetch+art_tls` | 202 ms; none after for ~21 h |
| same, ~1 day | Bathroom | 7,752 B at up≈0.09 d | `art_decode+lyrics_tls` | 200 ms |

- Idle free memory is flat (~81 KB Kitchen, ~92 KB Bathroom) — no leak.
- All dips fall in the first ~3 h of uptime; none after. Suggests a startup burst
  (discovery, GENA subscribe, first art + lyrics at once) or cold caches.
- Kitchen's WiFi is slow (OTA upload took 203 s; ping averaged ~1 s), the
  Bathroom's is not. Both dips are during the art *download*.
- `/getaa` relative art is plain HTTP and was not involved in these dips.

## Hypotheses

1. **WiFi/lwIP receive buffers.** A large download over a weak link fills TCP
   receive/out-of-order buffers, which live in internal RAM.
2. **Startup contention.** Several network tasks start together after boot.
3. **Art read pattern.** The fetch loop reads in chunks that may pin pbufs.

## Plan

- [ ] Log internal free RAM at 200 ms through a full art fetch (serial trace or a
      temporary ring on `/status`) to see the shape of the dip.
- [ ] Record Kitchen's WiFi RSSI and retransmit behaviour during the fetch;
      compare with the Bathroom.
- [ ] Test a smaller lwIP TCP receive window / reading in smaller chunks and
      measure the floor.
- [ ] Check whether delaying the first art fetch after boot (or staggering it
      from discovery/GENA setup) removes the early dips.
- [ ] If a code change is warranted, propose it through OpenSpec.

## Tools

`python3 scripts/heap_log.py` (nightly via launchd) and `curl http://<ip>/status`
— see `dip_count`, `heap_dips`, `heap_internal_min_free`, `tls_psram_allocs`.
