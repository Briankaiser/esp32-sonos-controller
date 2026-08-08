#pragma once
#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include "state.h"

struct FavoriteItem {
    String title;
    String uri;
    String metadata;       // r:resMD content if present (already HTML-encoded), else raw <item>
    bool   metadata_ready; // true = metadata is pre-encoded (from r:resMD), false = needs encoding
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
