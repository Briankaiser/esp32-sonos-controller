# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Each entry describes observable device behavior in one sentence and links to the
archived OpenSpec change that carries the rationale. Design discussion belongs in
`openspec/changes/archive/`, not here.

## [Unreleased]

### Fixed

- Long-unattended-operation hardening pass: WiFi now automatically reconnects
  after a drop (previously required a manual power cycle); Sonos Favorites
  retry until they load successfully instead of giving up permanently after
  one failed attempt at boot; a per-speaker memory leak in the Settings room
  picker and group-management overlays is fixed (every open leaked one
  allocation per speaker not clicked); network reads no longer risk a
  watchdog reboot on a slow device response; and free-heap is logged
  periodically to serial for diagnosing any future leak over a long soak.

### Added

- Rotary encoder now navigates modal overlays (favorites, settings, grouping):
  turning the knob moves a selection indicator through the list and a press
  activates the selected item, just like a touch tap. Volume and play/pause
  control resume automatically once the overlay closes.
  ([add-rotary-modal-nav](openspec/changes/add-rotary-modal-nav/proposal.md))
- Settings screen (gear icon on now-playing): select home room speaker, manage zone
  groups, and toggle group-wide volume control.
  ([add-settings-screen](openspec/changes/add-settings-screen/proposal.md))
- Display turns off after 5 minutes of inactivity; wakes on playback resumption
  or touch (the waking tap is consumed so it doesn't trigger a control). Background
  polling reduces to 10 s while idle.
  ([add-idle-screensaver](openspec/changes/archive/2026-08-09-add-idle-screensaver/proposal.md))

- Interface is significantly more responsive: removed blocking debug logging from
  the touch path, tightened the render cadence, and moved Sonos commands to a
  dedicated high-priority task so taps no longer wait behind background polling.
  ([improve-ui-responsiveness](openspec/changes/improve-ui-responsiveness/proposal.md))

- Tap the album art to open Favorites; tap any item to play it immediately on
  the home room speaker.
  ([add-favorites-shortcuts](openspec/changes/add-favorites-shortcuts/proposal.md))

- Tap the status strip to open the group overlay; join any speaker's group or
  go solo with a single tap.
  ([add-room-grouping](openspec/changes/add-room-grouping/proposal.md))

- A dimmed loading overlay with connecting text covers the now-playing screen
  until the first Sonos state is received.
  ([add-loading-state](openspec/changes/add-loading-state/proposal.md))

- Album art now loads asynchronously — transport controls and volume respond
  immediately during art decode. Switched to JPEGDEC for faster, more compatible
  JPEG decoding.
  ([improve-art-pipeline](openspec/changes/improve-art-pipeline/proposal.md))

- Now-playing screen displays album art; the interface accent colour animates
  to match each track's art.
  ([add-album-art-accent-theming](openspec/changes/add-album-art-accent-theming/proposal.md))

- Rotary encoder now controls Sonos volume in real time; short-press the encoder
  to toggle mute. The volume arc reflects the current level and grays out when muted.
  ([add-encoder-volume-control](openspec/changes/add-encoder-volume-control/proposal.md))

- `GET /rooms` lists discovered Sonos speakers as JSON; `POST /rooms/select` sets the home
  room by UUID without requiring factory reset, enabling remote room override.
  ([add-home-room-web-api](openspec/changes/add-home-room-web-api/proposal.md))

- Now-playing screen displays current track title, artist, album, and a live progress bar;
  play/pause, next, and previous controls let you operate Sonos directly from the device.
  ([add-now-playing-playback](openspec/changes/add-now-playing-playback/proposal.md))

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
