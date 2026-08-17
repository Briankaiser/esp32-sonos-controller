## 1. Scope-boundary flip (do first — unblocks the rest without self-contradiction)

- [x] 1.1 Remove/rewrite the "No lyrics in v1" line in `AGENTS.md` § Scope boundaries so it no longer forbids lyrics
- [x] 1.2 Update the scope-boundaries block in `openspec/config.yaml` (`context:`) to match — drop the "NO lyrics" line
- [x] 1.3 Confirm `openspec validate --all --strict` still passes after the doc edits

## 2. LRC parser (pure logic, host-testable)

- [x] 2.1 Add an LRC parser module (`[mm:ss.xx] text` → sorted list of `{ms, text}` lines) with a plain-vs-synced classification and a duration-fingerprint field
- [x] 2.2 Add the parser source to the `test_native` `build_src_filter` in `platformio.ini`
- [x] 2.3 Host unit tests (Unity, `test/host/`): well-formed synced LRC, plain lyrics (no timestamps → classified unsynced), malformed/garbage input (no crash, empty result), out-of-order timestamps (sorted), and current-line lookup for a given position

## 3. Lyrics fetch service (core 0, modeled on art.cpp)

- [x] 3.1 Create the lyrics service under `src/app/` with a dedicated FreeRTOS task that blocks on a task notification, mirroring the art task lifecycle (`init`, task body, `snapshot`, mutex-guarded published state)
- [x] 3.2 Allocate lyric buffers from PSRAM via `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`; report and tolerate allocation failure without rendering partial state
- [x] 3.3 Build the LRCLIB request from current track identity (artist/title/album/duration); fetch over `WiFiClientSecure` with a bounded timeout, reusing art.cpp's transport pattern
- [x] 3.4 Skip-refetch when the current track identity matches lyrics already held; discard a stale in-flight result when the track changes before it completes (compare identity at publish time)
- [x] 3.5 Map every failure to "no lyrics held": no uplink, 404/no-match, HTTP error, timeout, unparseable body — no crash, no reboot, Sonos control path never stalled
- [x] 3.6 Apply the duration-fingerprint trust gate: matched-duration vs. playing-duration beyond tolerance ⇒ do not expose synced timing (fall back to static plain lyrics or nothing)
- [x] 3.7 Trigger the lyrics task from the same place the art trigger fires in `main.cpp`, on track-identity change

## 4. Now-playing screen: lyric region + adaptive layout

- [x] 4.1 Add a lyric-region layout slot and a compact-metadata slot arrangement to the now-playing slot table; keep all new slots inside the inscribed disc so the compile-time `static_assert` passes
- [x] 4.2 Select the layout arrangement (lyrics-present vs. today's) at screen-build/relayout time based on whether lyrics are held — never recomputed per frame; no pixel literals or board identifiers in screen code
- [x] 4.3 Render the current lyric line highlighted with a small amount of context, driven by the existing interpolated position with per-poll resync; freeze the highlight when not Playing
- [x] 4.4 Untrustworthy timing (plain/unsynced lyrics, duration mismatch, unreliable position) renders static text or nothing — never a moving highlight; verify all lyric glyphs are clipped within the safe area
- [x] 4.5 No-lyrics path renders exactly today's layout (metadata reclaims full space, no lyric region); controls, progress bar, and album art undisturbed across the layout switch

## 5. Verification and wrap-up

- [x] 5.1 Both PlatformIO envs build: `pio run -e crowpanel_21_round` and `pio run -e ref_rect_stub`
- [x] 5.2 Host tests pass: `pio test -e test_native` (71/71, incl. new test_lrc + test_lyrics_api)
- [ ] 5.3 Capture `/screenshot` in three states — synced lyrics active, no lyrics (unchanged layout), and plain/static fallback — and confirm safe-area compliance on the round panel — **REQUIRES HARDWARE (not available this session); geometry validated via the Round Lyrics Layout artifact + compile-time static_assert**
- [ ] 5.4 Confirm the ≥55 fps budget holds with lyrics active and the highlight advancing (debug FPS/heap overlay build) — **REQUIRES HARDWARE; UI-task work is limited to setting label text on pre-parsed lines (fetch/parse off-task)**
- [ ] 5.5 Verify graceful degradation on real failure: airplane-mode/no-uplink, a track LRCLIB has no match for, and a deliberately wrong-duration match — each lands on the no-lyrics or static-lyrics screen with no error UI — **REQUIRES HARDWARE; failure mapping is covered by host tests (garbage LRC, missing/null JSON, duration trust gate)**
- [x] 5.6 Add a `## [Unreleased]` entry to `CHANGELOG.md` for the user-visible lyrics feature
- [x] 5.7 `openspec validate --all --strict` is clean
