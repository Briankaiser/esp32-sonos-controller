# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Each entry describes observable device behavior in one sentence and links to the
archived OpenSpec change that carries the rationale. Design discussion belongs in
`openspec/changes/archive/`, not here.

## [Unreleased]

### Removed

- The captive-portal setup form no longer asks for a weather latitude and
  longitude, and the device no longer stores them. These were collected and
  persisted but never used — there is no weather display or service — so they
  were dead configuration that added fields to first-time setup. Only WiFi
  SSID, WiFi password, and OTA password are collected now. (Previously stored
  values become harmless orphaned keys, cleared on the next factory reset.)

### Added

- Connectivity problems now show a clear, recoverable status screen instead of
  hanging on "Connecting…" forever. Three faults are distinguished from local
  signals: **No WiFi** (with a confirm-guarded "Re-run WiFi Setup" that factory-
  resets and reboots into the setup portal), **No Sonos found** (after a brief
  search, rather than an endless spinner), and **Home room offline** (naming the
  room and reconnecting on its own). Each screen clears itself automatically the
  moment the condition resolves. Implemented without hardware present: on-device
  verification of each fault is tracked in the OpenSpec change
  (`add-error-state-handling`).
- Sonos state changes now arrive by push (UPnP GENA event subscriptions to
  AVTransport, RenderingControl, and ZoneGroupTopology) instead of 2-second
  polling: play/pause/track/volume/grouping changes made from the phone app
  or any other controller appear on the device within about a second, and
  the display wakes from idle the moment music starts. Steady-state polling
  is suspended while eventing is healthy (only a slow track-position resync
  remains) and resumes automatically — at the previous cadence — whenever
  subscriptions cannot be established or renewed, so degraded conditions
  behave exactly like the previous firmware. Implemented without hardware
  present: on-device verification steps are tracked in the OpenSpec change
  (`add-upnp-event-subscriptions`), with host-side parser tests and a mock
  GENA speaker (`tools/mock_gena_speaker.py`) covering what a bench can.

### Fixed

- Title, artist, and album could go blank (while album art stayed frozen on
  the last successful track) whenever the home room was grouped with another
  speaker as a non-coordinator member. The internal record of which speaker
  is the group's coordinator was being wiped on every playback poll (every
  ~2s) immediately after being set once per discovery cycle, so the device
  fell back to polling the member itself — which reports no track metadata
  for a grouped speaker — for the rest of the interval between discovery
  cycles. This bug predates this session, but was far more noticeable once
  discovery slowed to a 10-minute interval (see below): what used to
  self-correct within 2 minutes could previously go unnoticed, and now
  would have stayed broken for up to 10.
- `/screenshot` now captures modal overlays (settings, favorites, grouping).
  It previously snapshotted only the base now-playing screen, missing
  anything drawn on the top layer.
- A speaker's display name could get stuck on its raw, un-friendly SSDP
  `friendlyName` (e.g. "192.168.4.27 - Sonos Roam - RINCON_...") until the
  next successful zone-topology fetch corrected it to the real room name.
  A single failed fetch (transient network hiccup) now falls back to the
  last known-good name instead of the raw one — more consequential once
  discovery slowed to a 10-minute interval (see below), since a miss used
  to self-correct within 2 minutes and now would have taken up to 10.
- Home theater surround/subwoofer satellites and the non-primary unit of a
  stereo pair (e.g. Era 300s bonded to an Arc Ultra) no longer appear as
  selectable entries in the home room picker or the group-management
  overlay — these are detected from the `Invisible` flag in Sonos'
  zone-group topology and were previously listed as if independently
  playable.
- Long-unattended-operation hardening pass: WiFi now automatically reconnects
  after a drop (previously required a manual power cycle); Sonos Favorites
  retry until they load successfully instead of giving up permanently after
  one failed attempt at boot; a per-speaker memory leak in the Settings room
  picker and group-management overlays is fixed (every open leaked one
  allocation per speaker not clicked); network reads no longer risk a
  watchdog reboot on a slow device response; and free-heap is logged
  periodically to serial for diagnosing any future leak over a long soak.

### Changed

- Speaker discovery now runs every 10 minutes instead of every 2 — speaker
  IPs and topology rarely change, so this cuts SSDP/SOAP chatter 5x.

### Added

- The Settings screen shows the device's current IP address as small,
  non-interactive text at the bottom of the list, for on-site debugging.
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
