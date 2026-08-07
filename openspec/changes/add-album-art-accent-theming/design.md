## Context

`GetPositionInfo` already returns `<TrackMetaData>` as a DIDL block. The DIDL includes `<upnp:albumArtURI>` alongside the title, artist, and album fields we already extract. The art URL is a full HTTP URL pointing to the Sonos speaker's built-in HTTP server (e.g., `http://192.168.4.27:1400/getaa?s=1&u=...`). A plain `WiFiClient` GET is sufficient — no special auth.

TJpgDec is the Tiny JPEG Decoder bundled inside LVGL under `src/libs/tjpgd/`. It requires enabling `LV_USE_TJPGD 1` in `lv_conf.h` to compile the source file. Its C API (`jd_prepare` / `jd_decomp`) can be called from any task — there is no LVGL thread-safety requirement on TJpgDec itself. Decoding on core 0 satisfies the architecture rule that JPEG decode never runs on the UI task.

The decoded pixel output format from TJpgDec is RGB888 (3 bytes per pixel, top-left origin). LVGL 9's `lv_image_t` widget can render a raw RGB888 buffer directly via an `lv_image_dsc_t` descriptor with `header.cf = LV_COLOR_FORMAT_RGB888`. The UI task only issues `lv_image_set_src()` with the pre-filled descriptor — zero decode on core 1.

SLOT_ALBUM_ART is 230×230 pixels. Typical Sonos art is 480×480 or 300×300. TJpgDec supports power-of-2 downscale during decode (scale 0=1:1, 1=1:2, 2=1:4, 3=1:8). Choosing scale 1 for a 480×480 source gives 240×240 — slightly oversized, which LVGL clips at the slot boundary. For smaller sources, scale 0 keeps full resolution and LVGL centers it.

## Goals / Non-Goals

**Goals:**
- Show real album art in SLOT_ALBUM_ART
- Drive accent colour from the art
- Animate accent transitions
- Keep all compute off the UI task

**Non-Goals:**
- Persistent art cache across reboots (PSRAM is volatile; re-fetch on reboot is fine)
- Progressive/streaming JPEG display (show the full image once decode completes)
- Palette quantisation for dominant colour (a luminance-weighted average is sufficient)
- Art for sources other than tracks (radio streams often have no art — placeholder is fine)

## Decisions

### D1: TJpgDec called directly from core 0, not via LVGL's decoder registration

**Choice:** Call `jd_prepare()` / `jd_decomp()` directly in `art::fetch_and_decode()` running on the sonos task (core 0). Do not call `lv_tjpgd_init()`, which registers TJpgDec as an LVGL decoder and would cause decode to happen on the UI task during `lv_timer_handler()`.

**Why:** The architecture rule is explicit — JPEG decode never runs on the UI task. LVGL's lazy-decode path violates this. Direct TJpgDec gives full control over when and where decode runs.

### D2: Dual PSRAM buffers — raw JPEG + decoded pixels

**Choice:** Allocate two PSRAM buffers on first use and reuse them per track: one for the raw JPEG bytes (up to `kArtJpegBufBytes = 150 * 1024`) and one for the decoded pixels (`kArtPixelBufBytes = 230 * 230 * 3 = 159270`). Both allocated with `heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`.

**Why:** Re-allocating per track would fragment PSRAM. Fixed-size buffers allocated once in `art::init()` eliminate fragmentation. 150 KB covers typical Sonos JPEG art (usually 30–100 KB). 230×230×3 is the exact slot size.

**Risk:** If a JPEG exceeds 150 KB it is silently skipped (placeholder retained). Logged to serial. Acceptable for v1 — Sonos serves standard thumbnails, not high-res masters.

### D3: Decode to slot size (230×230) using TJpgDec scale parameter

**Choice:** In the TJpgDec prepare step, read the image dimensions. Choose scale 0 if both dimensions ≤ 230, else scale 1 (1:2) if both ≤ 460, else scale 2 (1:4). After decode, store actual decoded `w` and `h` in `ArtState`. The LVGL image widget centers the image in the slot; the slot has overflow hidden by default so any slight oversize is clipped.

**Why:** TJpgDec's scale-during-decode is free — no extra memory or CPU. Decoding a 480×480 at scale 1 gives 240×240 at exactly the cost of the output, not the input. Scale 0 for small art (300×300 or less) preserves quality.

### D4: Dominant colour — luminance-weighted average of non-dark pixels in a 64×64 grid

**Choice:** Sample the decoded pixel buffer at stride to visit approximately 64×64 points. For each sample, compute luminance `L = 0.299R + 0.587G + 0.114B`. Skip pixels with `L < 40` (near-black) and pixels with colour deviation from grey `|R-G| + |G-B| < 15` (near-grey). Average R, G, B of the remaining pixels. If fewer than 100 qualifying pixels exist, fall back to `#1DB954` (the default accent green).

**Why:** Album art often has dark corners and grey gradients. Skipping black and grey leaves the vivid, representative tones. Weighted averaging is O(n) with no allocations. The fallback prevents desaturated or black accents on monochrome art.

### D5: Legibility guard — minimum luminance of 60 on the final accent

**Choice:** After computing the dominant colour, compute its luminance. If `L < 60`, scale each channel toward 255 by `60 / L` clamped at 255. This brightens very dark accents (deep blues, maroons) without shifting hue.

**Why:** The background is `#1A1A1A` (luminance ≈ 10). An accent at luminance 30 is only 3× brighter — barely distinguishable. Boosting to 60 ensures at least 6× luminance contrast against the background.

### D6: Accent animation via per-element lv_anim_t, 400 ms

**Choice:** `accent::update(lv_color_t new_color)` starts two `lv_anim_t` instances — one targeting the progress bar indicator style colour, one targeting the volume arc indicator style colour — each interpolating from current to target over 400 ms using `lv_anim_path_ease_out`. Called from the UI task only (triggered in `now_playing_update()` when the art version changes).

**Why:** LVGL's style animation system handles the per-frame interpolation without any code in the UI task's event loop. The animation plays entirely within `lv_timer_handler()`. 400 ms is long enough to be visible and feel intentional but short enough not to lag behind track changes.
