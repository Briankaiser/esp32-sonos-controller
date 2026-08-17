## Context

See `proposal.md` for motivation. The relevant existing machinery:

- **Album art is the template.** `src/app/sonos/art.cpp` runs a dedicated FreeRTOS task on core 0 that blocks on a task notification, snapshots the current `PlaybackState`, skips work when the art URI is unchanged, fetches over `WiFiClientSecure` (`setInsecure()`) or plain `WiFiClient`, decodes off the interface task, and publishes the result into a mutex-guarded `ArtState` that the interface task reads via `art::snapshot()`. Buffers are `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`. TLS therefore already ships and already reaches internet CDNs.
- **Position is already interpolated.** The device polls `GetTransportInfo` + `GetPositionInfo` every 2 s, interpolates position on a 1 s UI timer, and resyncs to the polled value each cycle. `AGENTS.md` is explicit: never poll faster to smooth timing.
- **Layout is compile-time slots.** Screen code positions content only through named `constexpr` layout slots; a slot outside the inscribed disc fails the build. No pixel literals in `src/ui/screens/`.

## Goals / Non-Goals

**Goals:**
- A lyrics service that mirrors the art task's structure so it inherits its proven off-UI-task, mutex-snapshot, PSRAM-buffer discipline.
- Line-level highlight driven by the *existing* interpolated position — no new polling, no faster polling.
- Silent degradation as the default path, not an afterthought: every failure lands on "no lyric region," which is byte-for-byte today's screen.

**Non-Goals:**
- No separate lyrics screen or modal. Lyrics render inline on the now-playing screen; `rotary-modal-nav` is untouched.
- No word-level ("karaoke") highlighting — line-level only.
- No lyrics translation, no plain-lyrics scrolling sync, no manual offset adjustment UI.
- No persisted lyrics cache across reboots — in-RAM, current-track only.

## Decisions

**LRCLIB as the source.** Free, no API key, no account, community-run, returns LRC-format synced lyrics via a duration-fingerprinted GET. Alternatives: Musixmatch/Genius (require API keys → captive-portal/NVS provisioning, and Genius has no synced timestamps) — rejected as heavier and less aligned with the "no cloud account" philosophy. LRCLIB's `GET /api/get?artist_name=&track_name=&album_name=&duration=` returns `syncedLyrics` (LRC) and `plainLyrics`; a 404 is the normal "not found."

**Duration fingerprint is the trust gate.** LRCLIB matches on duration; a remaster or live cut of a different length is the classic drift source. Decision: request with the track's duration and, on the response, compare the matched duration against the playing track's. Beyond a small tolerance (e.g. a couple of seconds) we do **not** drive a synced highlight — we fall back to static `plainLyrics` or nothing. This is what keeps a "working" fetch from feeling broken.

**Reuse the interpolated position; never add polling.** The highlight advances off the same 1 s interpolation + per-poll resync the progress bar already uses. LRC line lookup is O(n) over ~100–200 lines per tick — trivial. This directly honours the `AGENTS.md` "never poll faster to smooth" rule.

**Hand-rolled LRC parser.** LRC is `[mm:ss.xx] text` per line — a few lines of parsing, no library, no new `lib_deps`. Parser is pure text→struct, so it compiles into `test_native` and gets host unit tests (mirrors `xml_util.cpp`/`gena_parse.cpp` already in the native test env).

**Inline layout via a lyrics-present slot variant.** The now-playing layout gains a lyric region slot and a "compact metadata" arrangement, selected at screen-build/relayout time by whether lyrics are held — not recomputed per frame (layout rule). Absent lyrics → the existing slot arrangement, unchanged. All new slots sit inside the inscribed disc so the compile-time `static_assert` and CI positioning lint both still pass.

**Track identity key.** Cache/skip-refetch keyed on the same identity the metadata already carries (artist/title/album/duration), mirroring art's "don't re-fetch the same URI" rule. A track change mid-flight discards the stale result (compare identity at publish time, like the art task compares URI).

## Risks / Trade-offs

- **Position accuracy at line granularity** → 1 s Sonos granularity + 2 s poll + request latency can misplace a line near boundaries. Mitigation: resync-on-poll (already free), and treat low-confidence timing as a degrade-to-static/hidden case (spec'd) rather than showing a wrong highlight.
- **Duration-mismatched match drifting** → Mitigation: the duration fingerprint gate above; mismatch never drives a synced highlight.
- **Round-display space is tight** → a lyric region plus shrunken metadata plus art plus controls on a 480×480 disc is crowded. Mitigation: show the current line prominently with a small amount of context (e.g. one dim line either side), all safe-area-clipped; validate with a `/screenshot` capture before calling it done.
- **First deliberate internet dependency** → Mitigation: silent degradation is a written requirement; offline behaviour = today's screen. No control-path feature depends on lyrics.
- **Extra internal RAM for a second concurrent TLS session** → lyrics and art could fetch at once. Mitigation: lyric fetch is small (a few KB of text, no image decode); if internal-RAM pressure appears, serialise the two fetches behind the existing background-work structure rather than running both TLS sessions simultaneously.

## Migration Plan

- Additive feature, no data migration. Ships in a normal OTA image via `tools/fleet_update.py`.
- Rollback = flash the prior image; nothing persisted changes.
- The scope-boundary flip (`AGENTS.md` + `openspec/config.yaml`) lands in the same change so the docs never contradict the shipped behaviour.

## Open Questions

- Exact on-screen context count (current line only vs. current ± 1) and the metadata-shrink proportion — a visual-tuning detail settled against a `/screenshot`, and it changes neither the specs nor the task breakdown.
