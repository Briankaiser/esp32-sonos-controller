#pragma once
#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include "state.h"

enum class FavType : uint8_t {
    Unknown,
    Track,
    Album,
    Playlist,       // streaming service playlist
    SonosPlaylist,  // local Sonos queue playlist
    Radio,
};

struct FavoriteItem {
    String  title;
    String  uri;
    String  metadata;       // r:resMD content if present (already HTML-encoded), else raw <item>
    bool    metadata_ready; // true = metadata is pre-encoded (from r:resMD), false = needs encoding
    FavType type = FavType::Unknown;
};

struct FavoritesState {
    std::vector<FavoriteItem> items;
    bool     fetched = false;
    uint32_t version = 0;
};

namespace favorites {
    void          init();
    void          fetch(const SpeakerInfo& speaker);  // sonos task only
    FavoritesState snapshot();                          // any task
}
