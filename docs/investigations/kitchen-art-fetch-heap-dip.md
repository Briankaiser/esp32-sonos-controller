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

After `add-wifi-best-node-selection` (fw `42517af`, boot join now scans all
channels and takes the strongest node): **Kitchen -76 dBm** on `E4:19:7F:47:7A:46`
ch 1 (was -80), **Bathroom -55 dBm** on `0C:93:A5:1D:CC:86` ch 1 (was -62). The
Kitchen's strongest *visible* node is still only -76 dBm, so it was not simply on
the wrong node: the link is weak because of distance/obstruction, and the fix is
physical (a node closer to the Kitchen, or a better antenna position) rather than
firmware. The roam monitor was exercised on hardware with relaxed thresholds
(re-join worked, cooldown held).

## Update 2026-11-14

Nightly samples after node selection (`42517af`), plus a Bathroom test build with the roam monitor disabled
(`2de229d-dirty-noroam`, flashed 2026-11-09, 5 days).

- **Idle free heap is flat, not leaking.** An apparent drop (about -4 KB/day on both boards) on 11-12 was
  daytime samples taken during playback; overnight samples recover (Bathroom 75.9 -> 75.1 KB, Kitchen
  66.0 -> 68.9 KB).
- **Kitchen is stable after its reboot.** 3.9 days: one dip at boot (floor 3,880 B), no allocation failures,
  -51 to -63 dBm on `E4:19:7F:64:AC:86`. The 288 B floor and 14 failures were from the earlier boot.
- **The roam monitor is not the cause.** Bathroom with the monitor off still dipped (860 B, then 648 B), and sat
  on a weak node (-61 to -71 dBm) it could not leave. The normal build was restored on 2026-11-14 and Bathroom
  rejoined the strong node at -45 dBm.
- **Lyrics path also makes small internal allocations.** Bathroom logged one failed 235 B internal
  allocation from the Lyrics task and a 648 B floor during `lyrics_tls` at 3.9 days. This is the same class as
  the Kitchen art failures (small allocations outside mbedTLS, probably network buffers).
- Mesh channel moved from 1 to 11 on both boards around 11-13; nodes unchanged.

## Hypotheses

1. **WiFi/lwIP receive buffers.** A large download over a weak link fills TCP
   receive/out-of-order buffers, which live in internal RAM.
2. **Startup contention.** Several network tasks start together after boot.
3. **Art read pattern.** The fetch loop reads in chunks that may pin pbufs.

## Plan

- [ ] Log internal free RAM at 200 ms through a full art fetch (serial trace or a
      temporary ring on `/status`) to see the shape of the dip.
- [x] Compare `wifi_rssi` between the two controllers (Kitchen -80, Bathroom -62 dBm)
- [x] Watch `wifi_rssi` over several nights (logged nightly) and correlate with dips (done 2026-11-14: dips occur at good and weak signal; not signal-driven)
- [ ] Improve Kitchen's link physically (closer node, antenna position/orientation, away from the microwave/metal) and see if the dips stop during the fetch;
      compare with the Bathroom.
- [ ] Test a smaller lwIP TCP receive window / reading in smaller chunks and
      measure the floor.
- [ ] Check whether delaying the first art fetch after boot (or staggering it
      from discovery/GENA setup) removes the early dips.
- [ ] If a code change is warranted, propose it through OpenSpec.

- [x] Rule out the roam monitor as a cause (2026-11-14)
- [ ] Identify what the Lyrics and Art tasks allocate from internal RAM during TLS (per-task allocation breakdown)

## Tools

`python3 scripts/heap_log.py` (nightly via launchd) and `curl http://<ip>/status`
— see `dip_count`, `heap_dips`, `heap_internal_min_free`, `tls_psram_allocs`.
