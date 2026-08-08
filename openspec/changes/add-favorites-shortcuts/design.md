## Context

The Sonos `ContentDirectory` service exposes Favorites via the `FV:2` container. A single `Browse` SOAP call to `/MediaServer/ContentDirectory/Control` returns DIDL-Lite XML with all saved Favorites. Each item has a `<res>` element (playback URI) and metadata including title, art URI, and the full DIDL item string needed for `SetAVTransportURI`.

`sonos::soap::call()` already handles the HTTP POST/SOAP envelope and chunked decoding. `html_decode()` and `extract_xml_text()` are already in `discovery.cpp` and can be reused via a shared header or duplicated as static helpers.

Playing a Favorite requires two SOAP calls on the home room's AVTransport:
1. `SetAVTransportURI(CurrentURI=<res>, CurrentURIMetaData=<item_didl>)`
2. `Play(Speed=1)`

This is identical to the JoinGroup flow but with a different URI scheme.

## Goals / Non-Goals

**Goals:**
- Browse and play any Sonos Favorite from the device
- Fetch runs off-UI-task
- Minimal PSRAM impact (favorites list is short text, no pixel buffers)

**Non-Goals:**
- Pagination (Sonos Favorites rarely exceed 30–40 items; fetch all at once)
- Favorite art display in the list (titles only; art display is a future enhancement)
- Editing favorites (read-only browse)

## Decisions

### D1: `FavoritesState` as a simple mutex-protected vector — same pattern as `SonosState`

**Choice:** `src/app/sonos/favorites.h` declares `struct FavoriteItem { String title, uri, metadata; }` and `FavoritesState { std::vector<FavoriteItem> items; bool fetched = false; uint32_t version = 0; }`. Functions: `init()`, `fetch(const SpeakerInfo&)` (sonos task), `snapshot()` (any task).

**Why:** Consistent with the established pattern. No new IPC needed — the overlay reads `favorites::snapshot()` the same way the group overlay reads `sonos::state::snapshot()`.

### D2: Fetch triggered once after home room first resolves; re-fetched when overlay opens if empty

**Choice:** In `sonos_task`, add a `static bool s_favorites_fetched = false` flag. After the first successful poll (home room resolved), if `!s_favorites_fetched`, call `favorites::fetch(hr)` and set the flag. When the overlay opens and `favorites::snapshot().items.empty()`, trigger another fetch (the overlay shows a "Loading…" message and closes — the user can reopen).

**Why:** Fetching favorites on every discovery cycle would waste 1–2 SOAP calls per minute for rarely-changing data. Fetching once at boot and on-demand is sufficient.

### D3: Album art area tap opens the overlay; same overlay pattern as group

**Choice:** In `now_playing_create()`, add `LV_OBJ_FLAG_CLICKABLE` to `art_cont` and register a click callback that calls `now_playing_open_favorites_overlay()`. The overlay is a full-screen opaque layer on `lv_layer_top()` with a scrollable `lv_list`, identical structure to the group overlay.

**Why:** The album art area is the largest tappable region on the screen and currently unused for interaction. It is naturally discoverable ("tap the art to see what else is available"). Taps on the track info band and transport buttons are at higher z-order and will not propagate to art_cont, so there is no accidental trigger risk.

### D4: `PlayFavorite` command carries URI + metadata via statics in playback.cpp

**Choice:** Add `static String s_favorite_uri` and `static String s_favorite_metadata` alongside the existing `s_group_target_uuid`. `set_favorite(uri, metadata)` sets them; `PlayFavorite` in `drain_commands` reads them to call SetAVTransportURI then Play.

**Why:** Consistent with D1 from `add-room-grouping`. The URI and metadata strings are too large for the `uint8_t` queue item. Writing them before enqueue and reading them in drain is safe because the command queue is depth-4 and the UI serializes writes.

### D5: DIDL item metadata — store the full `<item>` element for SetAVTransportURI

**Choice:** When parsing the Browse response, extract each `<item ...>...</item>` element as a raw string and store it as `metadata`. Pass this verbatim as `CurrentURIMetaData` in the SetAVTransportURI call.

**Why:** Sonos requires the full DIDL metadata for favorites (especially radio stations and service tracks) to resolve the stream correctly. The raw item element is already available as a substring of the decoded DIDL; no re-serialisation needed.
