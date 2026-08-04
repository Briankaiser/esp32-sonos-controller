## Context

The firmware is split across hardware-dependent code (BSP, LVGL rendering) and pure-logic code (SOAP XML building, slot geometry, accent math). Only the pure-logic portion can run on the host. The BSP and LVGL layers are too coupled to hardware to compile under a native toolchain without substantial shim work.

PlatformIO's built-in test runner supports `platform = native` environments that compile with the host compiler and run tests directly. Unity (the lightweight C unit-test library) is bundled with the PlatformIO test framework. No additional test library needs to be added to production lib_deps.

## Goals / Non-Goals

**Goals:**
- Host tests run in CI on every push with no hardware
- Device tests are a defined, runnable command when hardware is present
- Mock Sonos server is usable from both host unit tests and device integration tests
- Test code stays separate from production source; production envs are unaffected

**Non-Goals:**
- 100% code coverage (not achievable for BSP/LVGL on host)
- Mocking LVGL itself (the renderer is hardware-dependent; test it via `/screenshot` on device)
- Continuous device testing in CI (no hardware runners)
- Any network dependency on Home Assistant or the Sonos cloud API

## Decisions

### Host test environment via PlatformIO native

```ini
[env:test_native]
platform = native
build_src_filter =
    +<app/sonos/soap.cpp>
    +<app/sonos/state.cpp>
    +<ui/layout/layout.cpp>
    +<ui/theme/accent.cpp>
    +<ui/theme/fonts.cpp>
build_flags =
    -std=gnu++17
    -I src
    -I test/host/stubs
    -DOPENSONOS_BOARD_SELECTED
    -DOPENSONOS_SHAPE_ROUND
    -DLV_CONF_INCLUDE_SIMPLE
    -I lib
test_build_src = yes
```

*Rationale.* The native environment compiles only the pure-logic modules listed in `build_src_filter`, plus everything under `test/host/`. Arduino.h and BSP symbols are supplied by a thin stub layer at `test/host/stubs/`. No board file is compiled.

### Stub layer for host compilation

`test/host/stubs/` provides:
- `Arduino.h` stub: `millis()`, `delay()`, `digitalRead()`, `Serial` with no-op implementations
- `esp_heap_caps.h` stub: `heap_caps_malloc` forwarding to `malloc`
- `lvgl.h` stub: minimal type aliases (`lv_color_t`, `lv_style_prop_t`, etc.) so `accent.cpp` compiles; no actual rendering

*Rationale.* Stubs are deliberately minimal — they exist to allow compilation, not to simulate hardware. A stub that does too much hides real bugs.

### Test file layout

```
test/
├── host/
│   ├── stubs/              ← compilation shims, not real implementations
│   │   ├── Arduino.h
│   │   ├── esp_heap_caps.h
│   │   └── lvgl.h
│   ├── test_layout/
│   │   └── test_layout.cpp ← slot geometry, safe-area, archetype slot count
│   └── test_accent/
│       └── test_accent.cpp ← legibility adjustment, token update
└── device/
    └── test_display/
        └── test_display.cpp ← display bring-up, touch, FPS
```

Each `test_<name>/` directory is a PlatformIO test suite. The runner discovers them automatically.

### Device test environment

```ini
[env:test_device]
extends = [env:crowpanel_21_round]  ; inherits all board settings
test_speed = 115200
```

Device tests use Unity assertions over serial. `pio test -e test_device` flashes the test image, reads serial output, and reports results. The FPS test runs the placeholder screen for 3 seconds under synthetic worker load and asserts `lv_display_get_fps(lv_display_get_default()) >= 55`.

### Mock Sonos server

`scripts/mock_sonos.py` is a self-contained Python 3 script:
- UDP socket on 239.255.255.250:1900 responds to `M-SEARCH * HTTP/1.1` with a valid UPnP `200 OK` pointing to a local HTTP server
- HTTP server at a random port handles:
  - `GET /device.xml` → returns a minimal UPnP device description for `urn:schemas-upnp-org:device:ZonePlayer:1`
  - `POST /AVTransport/control` → returns a success SOAP envelope; records the action name
  - `POST /RenderingControl/control` → same
  - `GET /log` → returns JSON array of received commands (for test assertion)
- Started as a subprocess in host tests; run manually alongside device tests

*Rationale.* Python (no extra dependencies) is available in CI. The log endpoint eliminates the need to parse SOAP in the test assertions — tests just call `GET /log` and assert on the returned JSON array.

### CI integration

The existing `build` job in `ci.yml` gains a sibling:

```yaml
test-native:
  name: Host unit tests
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - run: pip install platformio
    - run: pio test -e test_native
```

This job is required-to-merge alongside the existing build and lint jobs. Device tests are explicitly NOT in CI and are documented as a manual step in `AGENTS.md`.

## Risks / Trade-offs

**Stub drift.** The `test/host/stubs/` headers will need updating when production code adopts new platform types. A `static_assert` or `#error` guard in each stub at a known version boundary mitigates silent staleness.

**Coverage gap.** The BSP, LVGL rendering path, and the FreeRTOS task model are entirely untested on the host. This is an accepted trade-off: those layers are tested by flashing the device and observing the `/screenshot` output and serial log.

**Device test fragility.** The FPS test depends on the task scheduler and PSRAM bandwidth. Variation between hardware units could cause flaky results near the 55 fps threshold. The test should report the measured value and set the pass threshold slightly below hardware capacity (55 fps rather than 60).

**SSDP multicast on CI.** The mock server uses multicast, which is available on Linux CI runners but may need the loopback interface for some environments. The mock should fall back to unicast if the multicast socket bind fails.
