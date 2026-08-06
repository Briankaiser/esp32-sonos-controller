#pragma once
#include <lvgl.h>
#include "../../app/sonos/playback.h"

namespace screens {
    lv_obj_t* now_playing_create();
    void      now_playing_update(const sonos::PlaybackState& state);
}
