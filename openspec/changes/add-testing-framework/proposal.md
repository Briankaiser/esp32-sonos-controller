## Why

Every subsequent change touches logic that is difficult or impossible to validate without physical hardware — Sonos SOAP parsing, layout slot geometry, accent colour math, NVS key management. Without a host-side test suite, regressions are invisible until the device is flashed, which slows iteration. Without a structured on-device test path, "does it work" stays informal and unrepeatable.

This change establishes the testing foundation before any Sonos, provisioning, or screen logic is added, so those changes can be driven by tests from the start. It is purely a tooling/infrastructure change: no production firmware behaviour changes, and no Sonos-specific or Home Assistant integration is introduced.

## What Changes

- Add a **native PlatformIO test environment** (`[env:test_native]`) that compiles selected firmware modules for the host machine and runs Unity-based unit tests. Tests run with `pio test -e test_native` and produce pass/fail output without any connected hardware.
- Add a **device PlatformIO test environment** (`[env:test_device]`) that flashes a minimal test image to the CrowPanel and runs Unity integration tests over the serial port. Results are collected by `pio test -e test_device`.
- Add a **mock Sonos server** (`scripts/mock_sonos.py`) — a Python script that listens on the local network for SSDP discovery and SOAP requests, replies with canned responses, and records which commands were issued. It is used by both host-side tests (via subprocess) and device tests (run on the development machine while the device is on the same WiFi network).
- Add initial test suites under `test/`:
  - `test/host/test_layout/` — verify `constexpr` slot-geometry computations and safe-area checks compile and produce correct values on the host.
  - `test/host/test_accent/` — verify dominant-colour legibility adjustment and accent token logic.
  - `test/device/test_display/` — flash the board and confirm display brings up, touch registers, and the FPS budget is met.
- Extend CI to run `pio test -e test_native` as a required gate. Device tests remain manual (CI cannot flash hardware) but are documented in `AGENTS.md` as part of the hardware verification checklist.

## Capabilities

### New Capabilities

- `testing-framework`: The project's ability to verify firmware behaviour in isolation on the host and end-to-end on the device, including the mock Sonos server that decouples protocol testing from real speakers.

### Modified Capabilities

None — no existing spec-level requirements change.

## Impact

- **New files**: `platformio.ini` gains `[env:test_native]` and `[env:test_device]`; `test/host/`, `test/device/`, `scripts/mock_sonos.py`
- **CI**: `.github/workflows/ci.yml` gains a `test-native` job
- **No production firmware changes**: test envs are separate from `crowpanel_21_round` and `ref_rect_stub`; production builds are unaffected
- **Dependencies**: PlatformIO's Unity runner is included with the framework; no new library dependencies added to production envs
