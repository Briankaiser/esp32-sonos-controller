## Context

`art::fetch_and_decode()` is currently called from inside `sonos_task`'s inner polling loop. On a 640×640 JPEG it takes 3–5 seconds — the full duration of which blocks `drain_commands()`. Every encoder tick and button press enqueued during that window waits until decode completes.

JPEGDEC (bitbank2/JPEGDEC) is a mature Arduino JPEG library with native RGB565 output, good format coverage, and an `openRAM()` / `decode()` API that processes a buffer in memory. It does not require any special pool or I/O callback infrastructure.

The `ArtState` snapshot interface in `art.h` is unchanged. The UI task calls `art::snapshot()` the same way it always has.

## Goals / Non-Goals

**Goals:**
- Commands and polls proceed unimpeded during art decode
- Replace TJpgDec entirely — no more `tjpgdcnf.h` patch in `.pio/libdeps/`
- Native RGB565 output from decoder

**Non-Goals:**
- Hardware JPEG decode via `esp_jpeg` — DMA/PSRAM alignment requirements on Arduino ESP32 are not yet stable enough for production use; JPEGDEC software decode at 2–3× TJpgDec speed is sufficient
- Art caching across reboots

## Decisions

### D1: Dedicated `art_task` on core 0 at priority 1, woken by task notification

**Choice:** Spawn `art_task` in `setup()` via `art::start_task()`. The task body calls `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` and blocks until woken. The sonos_task calls `xTaskNotifyGive(s_art_task_handle)` when `album_art_uri` changes, replacing the inline `fetch_and_decode` call. The art task runs its one fetch+decode cycle and then returns to blocking.

**Why:** Task notifications are the lightest IPC primitive in FreeRTOS — zero allocation, single instruction to send, binary semaphore semantics sufficient here (at most one pending decode at a time). Priority 1 < sonos_task priority 3, so the sonos_task preempts art decode whenever it needs CPU on core 0. This gives the sonos_task full access for its SOAP calls without any co-ordination overhead.

**Alternative considered:** A separate FreeRTOS queue carrying the URI string. Rejected because String allocation in a queue item is fragile and the task notification is simpler — the art task reads the URI directly from `playback::snapshot()` when it wakes.

### D2: JPEGDEC replaces TJpgDec; `LV_USE_TJPGD` removed

**Choice:** Add `bitbank2/JPEGDEC` to `lib_deps`. In `art.cpp`, replace the `jd_prepare`/`jd_decomp` path with `JPEGDEC jpeg; jpeg.openRAM(s_jpeg_buf, fetched, draw_callback); jpeg.decode(0, 0, 0)`. The draw callback writes decoded pixel lines (RGB565) directly into `s_pixel_buf`. Remove `#define LV_USE_TJPGD 1` from `lv_conf.h`; delete `lib/tjpgd_override/`.

**Why:** Eliminates the entire `tjpgdcnf.h` patch chain. JPEGDEC outputs RGB565 natively so the BGR→RGB565 conversion pass is gone. No more JDR_PAR/JDR_FMT1 format errors.

### D3: Pixel buffer returns to 320×320×2 (RGB565, no RGB888 intermediate)

**Choice:** `kArtPixelBufBytes = 320u * 320u * 2u` (204 800 bytes).

**Why:** JPEGDEC's draw callback is called per line of the decoded image. We pass target dimensions (320×320) to `decode()` which handles scaling internally. The callback receives RGB565 line data and we copy it directly into `s_pixel_buf`. No intermediate RGB888 buffer needed; the PSRAM requirement drops by 100 KB.

### D4: JPEGDEC scaling via `decode(0, 0, JPEG_SCALE_HALF)` for large images

**Choice:** If `jpeg.getWidth() > 320 || jpeg.getHeight() > 320`, decode with `JPEG_SCALE_HALF` (1:2); otherwise `JPEG_SCALE_NONE` (1:1). This maps to JPEGDEC's built-in scaling, which is handled correctly for all chroma sampling formats.

**Why:** Replaces our brittle TJpgDec scale selection. JPEGDEC's built-in scaling works correctly for all JPEG types; no equivalent of the JDR_PAR scale-disabled issue. For 640×640 input, `JPEG_SCALE_HALF` gives 320×320 exactly fitting our buffer and display slot. The nearest-neighbour downsample loop in `art.cpp` is removed.
