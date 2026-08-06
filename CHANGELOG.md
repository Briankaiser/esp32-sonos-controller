# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Each entry describes observable device behavior in one sentence and links to the
archived OpenSpec change that carries the rationale. Design discussion belongs in
`openspec/changes/archive/`, not here.

## [Unreleased]

### Added

- Screenshot endpoint (`GET /screenshot`) returns the current display as a BMP image for
  visual review; OTA firmware update (`POST /update`) enables wireless reflashing without USB.
  ([add-web-debug-services](openspec/changes/add-web-debug-services/proposal.md))

- Sonos speaker discovery via SSDP on the local network: the device now finds all Sonos
  speakers, resolves the provisioned home room, and fetches zone-group topology on boot
  and every 60 seconds.
  ([add-sonos-discovery](openspec/changes/add-sonos-discovery/proposal.md))

- Device screen shows "Initial Setup" and the AP name to connect to while the first-boot
  captive portal is active, replacing a blank display.
  ([add-portal-setup-screen](openspec/changes/add-portal-setup-screen/proposal.md))

- First-boot captive portal configures WiFi, home room, and weather location without
  credentials in firmware; long-press the rotary encoder for 30 seconds to factory reset.
  ([add-device-provisioning](openspec/changes/add-device-provisioning/proposal.md))

- Firmware project scaffolding: PlatformIO + Arduino framework + LVGL 9.x for two board
  environments (`crowpanel_21_round` and `ref_rect_stub`), with a board-abstraction layer that
  ensures screen code never references hardware directly and a CI pipeline that enforces this.
  ([bootstrap-firmware-skeleton](openspec/changes/bootstrap-firmware-skeleton/proposal.md))

[Unreleased]: https://github.com/brian-kaiser/esp32-sonos-controller/compare/HEAD
