#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include "state.h"

namespace sonos {

enum class TransportState : uint8_t {
    Unknown,
    Stopped,
    Playing,
    Paused,
    Transitioning,
};

struct PlaybackState {
    TransportState transport_state   = TransportState::Unknown;
    String         title;
    String         artist;
    String         album;
    String         album_art_uri;
    String         track_uri;
    uint32_t       duration_s        = 0;
    uint32_t       position_s        = 0;
    uint32_t       poll_timestamp_ms = 0;
    uint32_t       version           = 0;
    uint8_t        volume            = 50;
    bool           muted             = false;
};

enum class PlaybackCommand : uint8_t {
    Play,
    Pause,
    Next,
    Previous,
    ToggleMute,
};

namespace playback {
    constexpr uint32_t kPollIntervalMs = 2000;

    void          init();
    void          update(const PlaybackState& next);          // sonos task only
    PlaybackState snapshot();                                  // any task, copies under mutex
    void          enqueue_command(PlaybackCommand cmd);        // UI task, zero-timeout send
    void          enqueue_volume_delta(int16_t delta);        // UI task, atomic accumulate
    void          drain_commands(const SpeakerInfo& speaker); // sonos task
    void          poll(const SpeakerInfo& speaker);           // sonos task
    uint32_t      poll_interval_ms();                         // sonos task: 500ms if boost active, else kPollIntervalMs
}

} // namespace sonos
