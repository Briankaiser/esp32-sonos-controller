## Why

The now-playing screen shows title, artist, album, album art, and a progress bar, but nothing to read along with. Time-aligned lyrics turn the idle-glance device into something you actively watch. Lyrics are not part of the Sonos UPnP surface, so they come from a third-party source — but the fetch fits the exact shape of the existing off-UI-task album-art pipeline, and the CPU cost of parsing and highlighting a few hundred lines is negligible next to the JPEG decode already running there.

This flips the settled "**No lyrics** in v1" scope boundary. That boundary was a deliberate deferral, not a technical impossibility; the flip is an explicit, tracked part of this change (see Impact).

## What Changes

- **New `synced-lyrics` capability.** Fetch time-aligned LRC lyrics for the current track from [LRCLIB](https://lrclib.net) — free, no API key, no account — keyed on artist / title / album / track duration. Parse the LRC into timestamped lines. All fetch and parse work runs off the interface task on core 0, delivering the parsed line list back over a queue, exactly like album art.
- **Graceful, invisible failure is a first-class requirement.** No internet, lyrics not found (404), a duration-fingerprint mismatch, malformed LRC, or a timed-out/failed call all degrade silently to the normal now-playing screen: no lyric region, no error text, no user-visible impact. Same spirit as the album-art fetch-fail scenario.
- **Real-time line highlighting.** When timed lyrics are available and the transport is Playing, the current line is highlighted and advances using the on-device interpolated playback position (resynced on each poll). The position estimate must be trustworthy at line granularity; when it cannot be (duration mismatch, plain unsynced lyrics, unreliable position), the feature degrades to a static unsynced view or hides entirely rather than showing a drifting highlight.
- **Now-playing layout adapts to lyrics.** When lyrics are present, the title/artist/album metadata area shrinks to free vertical space for the lyric region. When lyrics are absent (the common case, and every failure case), the layout is exactly today's — the metadata area reclaims the space.
- **Performance is non-negotiable.** The sustained ≥55 fps budget must hold with lyrics active. Lyric buffers are allocated from PSRAM; the interface task only renders already-parsed lines and never fetches, parses, or blocks.
- Lyrics are cached for the current track keyed on track identity, so re-polling the same track issues no new fetch (mirrors album art's "don't re-fetch the same URL" rule).

## Capabilities

### New Capabilities
- `synced-lyrics`: Fetching, parsing, caching, and time-aligning lyrics for the current track from LRCLIB over the LAN's internet uplink, off the interface task, with silent degradation on every failure or low-confidence-timing path.

### Modified Capabilities
- `now-playing-screen`: The screen SHALL adapt its layout when timed lyrics are available — shrinking the title/artist/album metadata area to present a lyric region with the current line highlighted — and SHALL present exactly today's layout whenever lyrics are absent.
- `runtime-architecture`: Lyric fetch and LRC parse SHALL run on a task other than the interface task and deliver results by message passing; lyric buffers SHALL be allocated from PSRAM; the sustained ≥55 fps budget SHALL hold while lyrics are active.

## Impact

- **Scope boundary flip (BREAKING to a settled decision):** remove/rewrite the "No lyrics" line in `AGENTS.md` (§ Scope boundaries) and in `openspec/config.yaml` (`context:` scope boundaries block). Both must stop forbidding lyrics, or this change contradicts the project's own source-of-truth docs.
- **New network dependency:** LRCLIB is the first *deliberate* internet dependency stated as such. (Album art already reaches streaming CDNs today, so this is not the device's first outbound-internet or first TLS traffic — TLS already ships via `WiFiClientSecure` in `src/app/sonos/art.cpp`.) Behaviour when the internet uplink is down is specified as silent degradation.
- **Code:** new lyrics service under `src/app/` (fetch + LRC parse + cache), modeled on `src/app/sonos/art.cpp`; a new lyric region + adaptive layout slots on the now-playing screen (`src/ui/screens/now_playing.cpp` and the layout slot tables); a queue/message to hand parsed lines from the lyrics task to the interface task; wiring in `main.cpp` alongside the art trigger.
- **No new libraries or credentials.** LRC parsing is hand-rolled (trivial text format); no API key to provision, so no captive-portal / NVS changes.
- **Docs:** `CHANGELOG.md` gets an `## [Unreleased]` entry (user-visible feature).
