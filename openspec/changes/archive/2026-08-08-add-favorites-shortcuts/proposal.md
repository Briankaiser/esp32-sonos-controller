## Why

The device can control playback but has no way to start playing something — it can only continue what Sonos is already playing. Adding Favorites browse-and-play closes that gap: the user can launch any saved Favorite (playlist, album, radio station, service shortcut) directly from the device without touching a phone app.

Sonos Favorites are already available on the LAN via the `ContentDirectory` service using the well-known `FV:2` container. No cloud, no OAuth, no third-party service dependency.

## What Changes

- Add `src/app/sonos/favorites.h/.cpp`: fetch the `FV:2` container from the home room speaker, parse each DIDL item into a `FavoriteItem` struct (title, playback URI, DIDL metadata, art URI), store in a mutex-protected `FavoritesState` snapshot. Fetch runs on the sonos task after the home room is resolved and on-demand when the overlay opens.
- Add `PlaybackCommand::PlayFavorite` and `set_favorite(const String& uri, const String& metadata)` to the playback namespace; `drain_commands()` executes `SetAVTransportURI` + `Play` on the home room speaker.
- Add a favorites overlay to the now-playing screen: tapping the album art area (any exposed region not covered by track info or transport controls) opens a full-screen list overlay. Each item shows the favorite's title; tapping enqueues `PlayFavorite` and closes the overlay.
- Trigger a background favorites fetch in `sonos_task` once after the home room is first resolved.

## Capabilities

### New Capabilities

- `sonos-favorites`: The device fetches the Sonos Favorites list from the home room speaker and displays it in an overlay. Tapping a favorite plays it on the home room speaker immediately. All I/O runs on the sonos task; the UI task only reads snapshots and enqueues commands.

## Impact

- New files: `src/app/sonos/favorites.h/.cpp`.
- `src/app/sonos/playback.h/.cpp`: add `PlayFavorite` command and `set_favorite()`.
- `src/main.cpp`: trigger favorites fetch after home room resolves; wire `art_cont` tap to open overlay.
- `src/ui/screens/now_playing.h/.cpp`: favorites overlay open/close, list population.
- No changes to BSP, layout archetype, or web services.
