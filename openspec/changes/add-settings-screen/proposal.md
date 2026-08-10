## Why

After first-boot provisioning the device has no way to tell the user which speaker it is bound to, or let them change it from the device itself. If the home room UUID is missing or stale — speaker replaced, network changed — the now-playing screen is blank with no indication of what is wrong or how to fix it. The only path today is to manually `curl /rooms/select` from a laptop.

A settings screen gives the user an on-device surface for configuration tasks that do not belong in the normal playback flow. Room selection is the immediate need; the screen is deliberately designed as an extensible list so future settings (display brightness, encoder sensitivity, weather location, etc.) can be added without structural changes.

Consolidating settings also cleans up the now-playing screen: the group-management overlay trigger and the volume-mode preference both belong in a settings context, not cluttering the playback UI. The existing `SLOT_STATUS` strip — previously used for a group status label — becomes the single gear-icon entry point to settings.

No Home Assistant, no Sonos cloud, no phone app. Room selection reads the already-discovered speaker inventory from `sonos::state::snapshot()` and writes the chosen UUID to NVS via `prefs::set_home_room()` — the same path used by the existing HTTP API.

## What Changes

- Add a **settings screen** (`src/ui/screens/settings.h/.cpp`) built on the existing slot system. The screen renders as a scrollable list of settings entries within the round safe area.
- Add a **room-selection entry** as the first item: displays the current home room's friendly name (or "Not set" if unset), and opens a room-picker overlay when tapped.
- Add a **group management entry**: opens the group join/leave overlay (previously triggered from the now-playing status strip); this moves the overlay trigger into settings.
- Add a **group volume entry**: a toggle controlling whether volume changes affect only the home room speaker or every speaker in the current zone group. Persisted in NVS. `flush_volume()` reads this flag each call.
- The settings screen has a **back navigation** control (returns to now-playing).
- Add a **settings entry point** to the now-playing screen: the `SLOT_STATUS` strip, previously used for a group status label, is repurposed as a tappable gear icon (`LV_SYMBOL_SETTINGS`) that navigates to the settings screen. The group status label is removed from the now-playing screen.
- The settings screen SHALL NOT block playback; it reads speaker data from the existing snapshot and makes no network calls itself.

## Capabilities

### New Capabilities

- `settings-screen`: A standalone LVGL screen reachable from the now-playing screen. Provides a scrollable list of configuration entries: (1) Home Room — shows current speaker name, opens a speaker picker on tap; (2) Manage Groups — opens the group join/leave overlay; (3) Group Volume — toggle between "Home room only" and "Whole group" volume control.

### Modified Capabilities

- `now-playing-screen`: The `SLOT_STATUS` strip is repurposed from a group status label to a tappable gear icon that navigates to the settings screen. The group overlay is no longer triggered from the now-playing screen directly.
- `sonos-volume`: `flush_volume()` gains awareness of a persisted group-volume flag. When the flag is "whole group" and the home room is in a zone group, `flush_volume()` issues `SetVolume` on each group member in addition to the home room.

## Impact

- `src/ui/screens/settings.h/.cpp`: new screen — settings list, room-picker overlay, group overlay (moved from now_playing.cpp), back navigation.
- `src/ui/screens/now_playing.h/.cpp`: `SLOT_STATUS` repurposed as gear icon; group-status label removed; `now_playing_open_group_overlay()` moved to settings.cpp.
- `src/app/sonos/playback.cpp`: `flush_volume()` reads `prefs::group_volume_mode()` and iterates zone group members when enabled.
- `src/services/prefs.h/.cpp`: add `prefs::group_volume_mode()` and `prefs::set_group_volume_mode(bool)`.
- No changes to BSP, discovery, or web services.
