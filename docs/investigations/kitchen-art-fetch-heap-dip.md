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

## Update 2026-11-05

Nightly sample (fw `3bfbfe0`, ~1.6 d): Kitchen low-water mark fell to 400 B with
3 dips (floors 800 → 448 → 400 B, all `art_fetch+art_tls`, the last overlapping a
discovery cycle) and **one failed 198-byte allocation in the lwIP `tiT` task** —
a networking-stack allocation, not TLS. Bathroom: 0 dips, low-water 78,440 B.
This points at network-stack memory under a slow download. `/status` now reports
`wifi_rssi` (change `add-status-wifi-rssi`) so the link quality of the two
controllers can be compared; the nightly log records it.

First RSSI reading (2026-11-05, fw `b5764d0`): **Kitchen -80 dBm, Bathroom -62 dBm.**
Kitchen's link is ~18 dB weaker (below the usual -70 dBm "reliable" line), which
fits the slow OTA uploads, ~1 s ping and the dips appearing only on that board.
Supports hypothesis 1 below; not yet proof of mechanism.

## Hypotheses

1. **WiFi/lwIP receive buffers.** A large download over a weak link fills TCP
   receive/out-of-order buffers, which live in internal RAM.
2. **Startup contention.** Several network tasks start together after boot.
3. **Art read pattern.** The fetch loop reads in chunks that may pin pbufs.

## Plan

- [ ] Log internal free RAM at 200 ms through a full art fetch (serial trace or a
      temporary ring on `/status`) to see the shape of the dip.
- [x] Compare `wifi_rssi` between the two controllers (Kitchen -80, Bathroom -62 dBm)
- [ ] Watch `wifi_rssi` over several nights (logged nightly) and correlate with dips
- [ ] Try improving Kitchen's link (move/replace AP, check channel) and see if the dips stop during the fetch;
      compare with the Bathroom.
- [ ] Test a smaller lwIP TCP receive window / reading in smaller chunks and
      measure the floor.
- [ ] Check whether delaying the first art fetch after boot (or staggering it
      from discovery/GENA setup) removes the early dips.
- [ ] If a code change is warranted, propose it through OpenSpec.

## Tools

`python3 scripts/heap_log.py` (nightly via launchd) and `curl http://<ip>/status`
— see `dip_count`, `heap_dips`, `heap_internal_min_free`, `tls_psram_allocs`.
