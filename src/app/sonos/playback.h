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
    String         track_uri;
    uint32_t       duration_s        = 0;
    uint32_t       position_s        = 0;
    uint32_t       poll_timestamp_ms = 0;
    uint32_t       version           = 0;
};

enum class PlaybackCommand : uint8_t {
    Play,
    Pause,
    Next,
    Previous,
};

namespace playback {
    constexpr uint32_t kPollIntervalMs = 2000;

    void          init();
    void          update(const PlaybackState& next);          // sonos task only
    PlaybackState snapshot();                                  // any task, copies under mutex
    void          enqueue_command(PlaybackCommand cmd);        // UI task, zero-timeout send
    void          drain_commands(const SpeakerInfo& speaker); // sonos task
    void          poll(const SpeakerInfo& speaker);           // sonos task
}

} // namespace sonos
