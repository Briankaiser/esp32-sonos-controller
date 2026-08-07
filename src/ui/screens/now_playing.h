#pragma once
#include <lvgl.h>
#include "../../app/sonos/playback.h"
#include "../../app/sonos/art.h"
#include "../theme/accent.h"

namespace screens {
    lv_obj_t* now_playing_create();
    void      now_playing_update(const sonos::PlaybackState& state);
    void      now_playing_init_art();  // call once from UI task after now_playing_create()
}
