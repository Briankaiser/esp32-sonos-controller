#pragma once
#include <lvgl.h>
#include "../../app/sonos/playback.h"
#include "../../app/sonos/art.h"
#include "../../app/sonos/state.h"
#include "../theme/accent.h"

namespace screens {
    lv_obj_t* now_playing_create();
    void      now_playing_update(const sonos::PlaybackState& state);
    void      now_playing_init_art();
    void      now_playing_open_group_overlay();
    void      now_playing_update_volume_arc(int volume_0_100);  // immediate UI feedback on encoder turn
}
