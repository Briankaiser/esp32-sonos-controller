# OpenSonos Controller

A standalone physical Sonos room controller on an ESP32-S3 touch display. The
device is bound to one configurable **home room** and gives hands-on control over
playback, volume, multi-room grouping, and Sonos Favorites — **no phone app, no
cloud account**. Control is direct UPnP/SOAP over the LAN.

> This README is an early stub — it will be filled out over time. For now it
> orients newcomers and links the detailed docs.

## Hardware

Primary board: **Elecrow CrowPanel 2.1" ESP32-S3** — a 480×480 round IPS panel
with capacitive touch and a rotary encoder. A rectangular reference profile
(`ref_rect_stub`) exists to keep the display abstraction honest.

## Quick start

```bash
pio run -e crowpanel_21_round            # build the primary firmware
pio run -e crowpanel_21_round -t upload  # flash over USB
pio device monitor                       # watch serial (115200)
```

First boot with no saved WiFi opens a captive-portal setup AP
(`Sonos-Controller-Setup`) to enter WiFi + OTA credentials.

## Documentation

- **[Building the firmware](docs/build-firmware.md)** — environments,
  prerequisites, build/flash/test, versioning, debugging aids.
- **[Fleet update runbook](docs/fleet-update.md)** — roll a new build out to
  every controller on the network over the air, safely, with `tools/fleet_update.py`.
- **[AGENTS.md](AGENTS.md)** — architecture rules, the round-display constraints,
  and the spec-driven workflow.
- **[CHANGELOG.md](CHANGELOG.md)** — user-visible changes per release.

## How it's built

Behavior is defined in specs before it is implemented, using
[OpenSpec](https://github.com/Fission-AI/OpenSpec):

- `openspec/specs/<capability>/spec.md` — current behavior (source of truth).
- `openspec/changes/<name>/` — active proposals; archived under
  `openspec/changes/archive/` once shipped.

Validate specs with `openspec validate --all --strict`.

## Scope boundaries

Settled decisions — no Home Assistant dependency, no Sonos cloud/OAuth API, no
recently-played history (Favorites only), no TV/line-in source switching. See
`AGENTS.md` for the rationale.
