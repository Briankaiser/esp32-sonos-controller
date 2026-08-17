#pragma once
#include <Arduino.h>
#include "lrc.h"
#include "../sonos/playback.h"

// On-device lyrics service. Runs a dedicated task on core 0 that fetches
// time-aligned lyrics from LRCLIB (keyless, over the LAN's internet uplink),
// parses them, applies the duration trust gate, and publishes a snapshot the
// now-playing screen reads on the interface task. Modeled on app/sonos/art.
//
// Every failure path — no uplink, not found, HTTP error, timeout, unparseable
// body, low-confidence timing — resolves to "no usable lyrics held" so the
// screen simply shows no lyric region. Nothing here blocks the interface task
// or the Sonos control path.
namespace lyrics {

struct LyricsState {
    LyricsDoc doc;                 // parsed lines; empty when none held
    bool      available = false;   // true iff a lyric region should be shown at all
    bool      synced    = false;   // true iff the highlight may advance (trusted timing)
    String    track_key;           // identity of the track these lyrics belong to
    uint32_t  version   = 0;       // bumps on every publish so the UI can detect change
};

namespace service {
    void        init();               // allocate the PSRAM response buffer + mutex
    void        start_task();         // spawn the lyrics task (core 0, low priority)
    void        notify_new_track();   // wake the task to (re)fetch (sonos task only)
    LyricsState snapshot();           // any task — mutex-guarded copy
    String      track_key(const sonos::PlaybackState& pb);  // identity string for a track
}

} // namespace lyrics
