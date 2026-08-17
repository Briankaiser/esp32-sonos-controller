# Firmware Build Runbook

How to build, flash, and test the OpenSonos controller firmware.

The project targets two boards, one firmware image each, selected by PlatformIO
environment. Toolchain: **PlatformIO + Arduino (ESP32-S3)**, C++17, **LVGL 9**.

| Environment | Board | Purpose |
|---|---|---|
| `crowpanel_21_round` | Elecrow CrowPanel 2.1" 480×480 round | **Primary** — the real hardware. |
| `ref_rect_stub` | 800×480 rectangular reference | Proves the display abstraction holds. Must always compile; may be untested on hardware. |
| `test_native` | host (native) | Host unit tests — no hardware. |
| `test_device` | CrowPanel over USB | On-device integration tests. |

---

## Prerequisites

- **PlatformIO Core 6.x** (`pio`). Install with `pip install -U platformio`, or
  use the PlatformIO IDE extension for VS Code. Verify: `pio --version`.
- For flashing/monitoring/on-device tests: a **CrowPanel connected over USB**.
- The platform, board packages, and libraries download automatically on the
  first build. The pinned toolchain is the
  [`pioarduino`](https://github.com/pioarduino/platform-espressif32) release
  `55.03.311` (Arduino core 3.3.11 / ESP-IDF 5.5.5); libraries are LVGL 9.2,
  GFX Library for Arduino 1.4.9, PCF8574 0.4, JPEGDEC 1.8.4 (see `platformio.ini`).

---

## Build

```bash
pio run -e crowpanel_21_round      # primary board
pio run -e ref_rect_stub           # rectangular reference (must always compile)
```

`crowpanel_21_round` is the default environment, so a bare `pio run` builds it.

Two `extra_scripts` run automatically during the build:

- `exclude_arm_asm.py` — neutralizes an ARM-only LVGL SIMD assembly file that the
  Xtensa assembler would otherwise reject. No action needed.
- `fw_version.py` — see **Firmware version** below.

### Build artifacts

```
.pio/build/<env>/firmware.bin          # the application image (flash / OTA this)
.pio/build/<env>/firmware.factory.bin   # full image incl. bootloader + partitions
.pio/build/<env>/firmware.elf           # symbols (debugging, strings)
.pio/build/<env>/fw_version.txt          # the version string this build stamped
```

---

## Firmware version

The build stamps a version from `git describe --tags --always --dirty` (via
`extra_scripts/fw_version.py`). That value is:

- compiled in and returned by `fw::version()` — reported by `GET /status` as
  `firmware_version` and logged to serial on boot;
- **embedded in the image** behind a greppable marker, so a distributed `.bin` is
  self-describing (the fleet updater reads the target version straight from it);
- written to the `fw_version.txt` sidecar next to the `.bin`.

> Commit your changes before a release build, so the version is a clean SHA/tag
> rather than `…-dirty`. A build with no git metadata falls back to `"unknown"`.

Check what a build produced:

```bash
cat .pio/build/crowpanel_21_round/fw_version.txt
strings -a .pio/build/crowpanel_21_round/firmware.elf | grep OPENSONOS_FW_VERSION:
```

---

## Flash

### Over USB (development)

```bash
pio run -e crowpanel_21_round -t upload
```

If multiple serial ports are present, pin the port:

```bash
pio device list                                   # find the CrowPanel's port
pio run -e crowpanel_21_round -t upload --upload-port /dev/cu.usbmodemXXXX
```

The device resets into the new firmware automatically after a successful write.

> On the ESP32-S3's native USB, **opening the serial port toggles the reset
> line** — so starting a monitor (or any tool that opens the port) reboots the
> board. Expected; not a fault.

### Over the air (fleet)

To flash controllers already on the network without USB, use the fleet updater —
see **[Fleet Update Runbook](fleet-update.md)**. It reads the target version from
the `.bin`, so build first, then point the updater at
`.pio/build/crowpanel_21_round/firmware.bin`.

---

## Serial monitor

```bash
pio device monitor -e crowpanel_21_round     # 115200 baud
```

Boot logs include the WiFi connection + assigned IP and the firmware version.

---

## Tests

```bash
pio test -e test_native      # host unit tests — pure logic, no hardware
pio test -e test_device      # on-device integration tests (CrowPanel via USB)
```

The **touch test** under `test_device` needs a physical finger tap within 5
seconds of the test starting; without one it is automatically skipped/IGNORED.

Host-side tool tests (Python, no hardware):

```bash
python3 tools/test_fleet_update.py
```

---

## Debugging aids

- **`GET /screenshot`** — capture the live framebuffer as a BMP over WiFi. This is
  the primary way to inspect the UI:
  ```bash
  curl http://<device-ip>/screenshot -o screen.bmp
  ```
  Attach a capture to any interface change.
- **FPS / free-heap overlay** — set `-DOPENSONOS_DEBUG_OVERLAY=1` in the env's
  `build_flags` (default `0`) and rebuild to show the on-screen performance
  overlay. Performance gate is **≥ 55 fps sustained**.
- **Mock Sonos** — `python3 scripts/mock_sonos.py` replays canned SOAP XML so
  playback logic can be exercised without real speakers.

---

## Definition of done (before merging)

1. **Both** firmware envs build: `pio run -e crowpanel_21_round` and
   `pio run -e ref_rect_stub`.
2. `openspec validate --all --strict` is clean.
3. UI changes include a `/screenshot` capture.
4. A `CHANGELOG.md` entry exists under `## [Unreleased]` if the change is
   user-visible.

See `AGENTS.md` for the full architecture rules and the spec-driven workflow.

---

## The round display

The primary panel is physically **circular** — the four corners of the 480×480
framebuffer are not visible. This is a hard constraint enforced three ways (a
compile-time `static_assert` over layout slots, runtime circular clipping, and a
CI lint against pixel literals in screen code). Screen code positions content
only through named layout slots; it never hardcodes pixels or board names.

---

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| `pio: command not found` | PlatformIO Core not installed or not on `PATH`. `pip install -U platformio`, or use the VS Code extension's terminal. |
| Upload can't find the board | Check the USB cable/port with `pio device list`; pass `--upload-port`. Close any open serial monitor first (it holds the port). |
| First build is slow | Platform + toolchain + libraries download once and are cached under `~/.platformio` and `.pio/`. |
| Version shows `unknown` | Build ran without git metadata (e.g. a source tarball with no `.git`). Build from a git checkout. |
| Assembler error about ARM/Helium opcodes | The `exclude_arm_asm.py` script should handle this automatically; a clean rebuild (`pio run -t clean`) re-applies the patch. |
