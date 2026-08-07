## Why

The album art slot on the now-playing screen has been a dark placeholder square since the screen was built. Album art is the most visually distinctive element of a music controller — without it the screen looks unfinished. Beyond display, extracting the dominant colour from each track's art and applying it as the interface accent is standard practice in modern music players (Spotify, Apple Music, etc.) and makes the device feel responsive to what's playing.

This change completes the now-playing screen's visual design. It also closes the last unimplemented requirement of the `ui-foundation` spec: the animated accent token.

No phone app, no Sonos cloud. Art URLs come from the `upnp:albumArtURI` field already embedded in the DIDL metadata that `GetPositionInfo` returns.

## What Changes

- Enable TJpgDec in `lib/lv_conf.h` so the library compiles.
- Add `album_art_uri` to `PlaybackState` (extracted from DIDL alongside existing title/artist/album).
- Add `src/app/sonos/art.h/.cpp`: fetches the JPEG on core 0, decodes it with TJpgDec into a PSRAM pixel buffer, downsamples to 64×64 to extract the dominant colour, and exposes the result through a mutex-protected `ArtState` snapshot (same pattern as `PlaybackState`).
- Extend the sonos task inner loop: when `album_art_uri` changes between polls, call `art::fetch_and_decode()` on core 0 before the next poll delay.
- Add `src/ui/theme/accent.h/.cpp`: owns the single accent `lv_color_t` token and an `lv_anim_t` that interpolates the progress-bar indicator and volume-arc indicator colours over ~400 ms whenever the token is updated.
- Update `now_playing.cpp`: replace the dark square in `SLOT_ALBUM_ART` with an `lv_image_t`; in `now_playing_update()`, check the art snapshot version and update image + accent when it changes.
- Wire `art::init()` in `setup()`.

## Capabilities

### New Capabilities

- `album-art`: Fetches the album art JPEG from the home room speaker's HTTP server when the track changes, decodes it on core 0 using TJpgDec into a PSRAM buffer, and makes the decoded pixels available to the UI task via a thread-safe snapshot. Extracts a dominant colour from a 64×64 downsample of the decoded image. All decode and extraction work runs off the UI task.

### Modified Capabilities

- `ui-foundation` (DELTA): The accent colour token is now a live value sourced from album art dominant colour. Updating it triggers a 400 ms animated transition on all accent-derived elements. A legibility guard ensures the accent remains distinguishable from the dark background.

## Impact

- `lib/lv_conf.h`: add `#define LV_USE_TJPGD 1`.
- New files: `src/app/sonos/art.h/.cpp`, `src/ui/theme/accent.h/.cpp`.
- `src/app/sonos/playback.h/.cpp`: add `album_art_uri` field; extract from DIDL in `poll()`.
- `src/main.cpp`: `art::init()` in `setup()`; art version check in UI task loop.
- `src/ui/screens/now_playing.h/.cpp`: swap dark square for `lv_image_t`; wire art and accent updates.
- PSRAM usage: ~100 KB raw JPEG buffer + ~160 KB decoded pixel buffer (230×230 RGB888). Both freed and reallocated per track.
- No changes to `src/bsp/`, the web services layer, or the discovery/topology code.
