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
    String         group_coordinator_uuid;  // empty = standalone; own uuid = coordinator; other = member
};

enum class PlaybackCommand : uint8_t {
    Play,
    Pause,
    Next,
    Previous,
    ToggleMute,
    JoinGroup,          // home room joins target coordinator's group
    LeaveGroup,         // home room goes standalone
    AddGroupMember,     // target speaker joins home room's group (SOAP on target)
    RemoveGroupMember,  // target speaker goes standalone (SOAP on target)
    PlayFavorite,       // play a Sonos Favorite; URI+metadata set via set_favorite()
};

namespace playback {
    constexpr uint32_t kPollIntervalMs = 2000;

    void          init();
    void          update(const PlaybackState& next);          // sonos task only
    PlaybackState snapshot();                                  // any task, copies under mutex
    void          enqueue_command(PlaybackCommand cmd);        // UI task — wakes cmd_task
    void          enqueue_volume_delta(int16_t delta);        // UI task — wakes cmd_task
    void          set_cmd_notify_task(void* task_handle);     // call from cmd_task at startup
    void          set_poll_notify_task(void* task_handle);    // call from sonos_task at startup
    void          notify_poll_task();                          // cmd_task → trigger boost re-poll
    void          set_group_target(const String& coordinator_uuid);   // before JoinGroup
    void          set_group_op_target(const String& member_uuid);    // before AddGroupMember/RemoveGroupMember
    void          set_favorite(const String& uri, const String& metadata, bool meta_ready); // before PlayFavorite
    void          update_group_uuid(const String& uuid);           // sonos task
    void          drain_commands(const SpeakerInfo& speaker);      // sonos task
    void          poll(const SpeakerInfo& speaker);                // sonos task — fetches transport + volume
    void          poll_volume(const SpeakerInfo& speaker);         // sonos task — fetches volume/mute only
    uint32_t      poll_interval_ms();                              // sonos task: 500ms if boost active, else kPollIntervalMs
}

} // namespace sonos
