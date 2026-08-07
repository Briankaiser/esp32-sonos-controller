## Why

Album art decode currently runs synchronously inside `sonos_task`. A 640×640 JPEG takes 3–5 seconds to decode with TJpgDec, during which `drain_commands()` never runs — encoder turns and button presses queue up and are only actioned when the decode finishes. This makes the device feel sluggish for the full duration of every track change.

Two changes are needed: an architectural fix and a decoder swap.

**Architecture:** Move the art pipeline to its own `art_task` pinned to core 0 at lower FreeRTOS priority than `sonos_task`. The sonos task signals the art task via a FreeRTOS task notification when the art URI changes. Polls and command draining continue on schedule regardless of what the art task is doing.

**Decoder:** Replace TJpgDec with the JPEGDEC library (bitbank2/JPEGDEC). JPEGDEC decodes to RGB565 natively (eliminating the post-decode conversion pass we currently do), handles all JPEG sampling formats without errors, and is 2–3× faster than TJpgDec in software. It also eliminates the `tjpgdcnf.h` hack in `.pio/libdeps/` that was needed to enable TJpgDec scaling.

No phone app, no Sonos cloud.

## What Changes

- Add `bitbank2/JPEGDEC @ ^2.0.0` to `platformio.ini` lib_deps.
- Rewrite `src/app/sonos/art.cpp` to use JPEGDEC: replace the TJpgDec input/output callbacks with JPEGDEC's simpler `JPEGDEC::openRAM` / `decode` API; remove the post-decode BGR→RGB565 conversion (JPEGDEC outputs RGB565 directly).
- Remove `#define LV_USE_TJPGD 1` from `lib/lv_conf.h` — no longer needed.
- Remove the `lib/tjpgd_override/` directory — no longer needed.
- Add a dedicated `art_task` in `main.cpp` pinned to core 0, priority 1 (below `sonos_task` priority 3). The art task blocks on `ulTaskNotifyTake` and runs one fetch+decode cycle per notification.
- Replace the inline `art::fetch_and_decode()` call in `sonos_task` with `xTaskNotifyGive(s_art_task_handle)`.
- `ArtState` and the mutex-protected snapshot interface in `art.h/.cpp` are unchanged — the UI task continues to call `art::snapshot()` exactly as before.

## Capabilities

### Modified Capabilities

- `album-art` (DELTA): The art pipeline is now fully asynchronous. A dedicated `art_task` at lower priority fetches and decodes independently of the Sonos poll cycle. The decoder is JPEGDEC instead of TJpgDec, providing better format compatibility, native RGB565 output, and faster software decode. The observable behaviour (art appears a few seconds after a track change, accent colour updates) is unchanged; the improvement is that transport controls and volume remain responsive during the fetch.

## Impact

- `platformio.ini`: add JPEGDEC dependency.
- `lib/lv_conf.h`: remove `LV_USE_TJPGD 1`.
- `lib/tjpgd_override/`: delete directory.
- `src/app/sonos/art.h`: add `void start_task()` to spawn the art task; no other interface changes.
- `src/app/sonos/art.cpp`: replace TJpgDec path with JPEGDEC; add task body.
- `src/main.cpp`: remove `art::fetch_and_decode()` call from sonos_task inner loop; call `art::start_task()` in `setup()`.
- PSRAM layout: JPEGDEC decodes directly to `s_pixel_buf` in RGB565; `kArtPixelBufBytes` shrinks to `320 * 320 * 2` since no RGB888 intermediate is needed.
