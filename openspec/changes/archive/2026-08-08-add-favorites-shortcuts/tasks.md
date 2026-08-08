## 1. Favorites State and Fetch

- [x] 1.1 Create `src/app/sonos/favorites.h` declaring:
  - `struct FavoriteItem { String title; String uri; String metadata; }`
  - `struct FavoritesState { std::vector<FavoriteItem> items; bool fetched = false; uint32_t version = 0; }`
  - `namespace favorites`: `void init()`, `void fetch(const SpeakerInfo& speaker)` (sonos task), `FavoritesState snapshot()` (any task)
- [x] 1.2 Create `src/app/sonos/favorites.cpp`:
  - Static globals: `SemaphoreHandle_t s_mutex`, `FavoritesState s_state`
  - `favorites::init()`: creates mutex
  - `favorites::snapshot()`: take mutex, copy state, release, return
  - `favorites::fetch(speaker)`:
    - Call `sonos::soap::call(speaker.ip, speaker.port, "/MediaServer/ContentDirectory/Control", "urn:schemas-upnp-org:service:ContentDirectory:1", "Browse", "<ObjectID>FV:2</ObjectID><BrowseFlag>BrowseDirectChildren</BrowseFlag><Filter>*</Filter><StartingIndex>0</StartingIndex><RequestedCount>100</RequestedCount><SortCriteria></SortCriteria>")`
    - If empty, log `[Favorites] fetch failed` and return
    - Extract `<Result>` from response, `html_decode()` it
    - Parse each `<item ...>...</item>` block: extract `dc:title`, first `<res>` text as URI, and store the full `<item>...</item>` substring as metadata
    - Update `s_state` under mutex: set `items`, `fetched = true`, increment `version`
    - Log `[Favorites] loaded N favorites`

## 2. PlayFavorite Command

- [x] 2.1 In `src/app/sonos/playback.h`, add `PlayFavorite` to `PlaybackCommand`
- [x] 2.2 In `src/app/sonos/playback.h`, add `void set_favorite(const String& uri, const String& metadata)` to the `playback` namespace
- [x] 2.3 In `src/app/sonos/playback.cpp`, add `static String s_favorite_uri` and `static String s_favorite_metadata` at file scope
- [x] 2.4 Implement `playback::set_favorite(uri, metadata)`: assign the two statics
- [x] 2.5 In `drain_commands()`, add handling for `PlaybackCommand::PlayFavorite`:
  - Call `SetAVTransportURI` with `<InstanceID>0</InstanceID><CurrentURI><![CDATA[s_favorite_uri]]></CurrentURI><CurrentURIMetaData><![CDATA[s_favorite_metadata]]></CurrentURIMetaData>` — note: use raw string concatenation, not CDATA, since SOAP call wraps in envelope: args = `"<InstanceID>0</InstanceID><CurrentURI>" + s_favorite_uri + "</CurrentURI><CurrentURIMetaData>" + s_favorite_metadata + "</CurrentURIMetaData>"`
  - If successful, call `Play`
  - Log `[Favorites] playing: <title-substring>`; log failure if SOAP returns empty
  - Set `s_boost_polls.store(4)`

## 3. Sonos Task Wiring

- [x] 3.1 In `src/main.cpp`, add `#include "app/sonos/favorites.h"` after existing sonos includes
- [x] 3.2 In `setup()`, call `favorites::init()` after `art::init()` and before `art::start_task()`
- [x] 3.3 In `sonos_task()`, add a `static bool s_favorites_fetched = false` flag; after the first successful `poll()` (when `st.home_room_idx >= 0` and `!s_favorites_fetched`), call `favorites::fetch(hr)` and set `s_favorites_fetched = true`

## 4. Favorites Overlay UI

- [x] 4.1 In `src/ui/screens/now_playing.h`, add `void now_playing_open_favorites_overlay()`
- [x] 4.2 In `src/ui/screens/now_playing.cpp`, add `static lv_obj_t* s_favorites_overlay = nullptr`
- [x] 4.3 In `now_playing_create()`, add `LV_OBJ_FLAG_CLICKABLE` to `art_cont` and register a click callback: `lv_obj_add_event_cb(art_cont, [](lv_event_t*){ screens::now_playing_open_favorites_overlay(); }, LV_EVENT_CLICKED, nullptr)` — store `art_cont` as a static `s_art_cont` so the callback can be registered
- [x] 4.4 Implement `screens::now_playing_open_favorites_overlay()` in `now_playing.cpp`:
  - If `s_favorites_overlay != nullptr`, return
  - Create full-screen opaque overlay on `lv_layer_top()` (same pattern as group overlay: `#111111` background, `LV_OPA_COVER`)
  - Add backdrop click-to-close callback
  - Create `lv_list_t` sized to SLOT_LIST geometry
  - Read `favorites::snapshot()`; if `!state.fetched || state.items.empty()`: add a single disabled list item "No favorites yet" and return
  - For each `FavoriteItem`: add a list button with `item.title.c_str()`; button height 72px; Title font; `lv_obj_set_ext_click_area(btn, 8)`; truncate label with LV_LABEL_LONG_DOT
  - Each button callback: calls `sonos::playback::set_favorite(item.uri, item.metadata)`, `enqueue_command(PlaybackCommand::PlayFavorite)`, closes overlay
  - Store `item.uri` and `item.metadata` via heap-allocated struct with user_data (same pattern as group overlay's `BtnData`)
- [x] 4.5 In `now_playing_update()`, no changes needed — favorites overlay is fully self-contained

## 5. Verification

- [x] 5.1 `pio run -e crowpanel_21_round` exits 0
- [x] 5.2 `pio run -e ref_rect_stub` exits 0
- [x] 5.3 On hardware: boot and wait for discovery — confirm `[Favorites] loaded N favorites` appears in serial
- [x] 5.4 On hardware: tap the album art area — confirm the favorites overlay opens with a list of favorites by title
- [x] 5.5 On hardware: tap a favorite — confirm it starts playing on the home room speaker and the overlay closes
- [x] 5.6 On hardware: tap outside the list — confirm the overlay closes without changing playback
- [x] 5.7 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Tap the album art to open Favorites; tap any item to play it immediately on the home room speaker."
