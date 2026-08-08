## Purpose

Fetches the Sonos Favorites list and allows the user to play any Favorite directly from the device. All network I/O runs off the UI task.

## ADDED Requirements

### Requirement: Device Fetches and Displays the Sonos Favorites List

After the home room speaker is resolved, the device SHALL fetch the Sonos Favorites list from that speaker. The list SHALL be accessible from the now-playing screen via a tap gesture on the album art area. When the list is displayed, each entry SHALL show the favorite's title. The list SHALL be positioned within the display's safe area on round displays.

#### Scenario: Favorites list opens from now-playing screen

- **WHEN** the user taps the album art area of the now-playing screen
- **THEN** a favorites overlay appears listing all Sonos Favorites by title
- **AND** all list items are within the safe area on round displays

#### Scenario: No favorites exist

- **WHEN** the home room has no Sonos Favorites
- **THEN** the overlay shows a message indicating no favorites are available

#### Scenario: Favorites not yet loaded

- **WHEN** the favorites list has not yet been fetched
- **THEN** the overlay shows a loading indicator or empty state

### Requirement: Tapping a Favorite Plays It on the Home Room Speaker

When the user selects a Favorite from the list, the device SHALL play that Favorite on the home room speaker. The command SHALL be executed on the sonos task. The overlay SHALL close after selection.

#### Scenario: User selects a favorite

- **WHEN** the user taps a favorite in the overlay
- **THEN** playback of that favorite begins on the home room speaker
- **AND** the overlay closes

#### Scenario: Overlay closes without playing

- **WHEN** the user taps outside the favorites list
- **THEN** the overlay closes without changing playback

### Requirement: Favorites Are Fetched Off the UI Task

The ContentDirectory browse operation SHALL execute on the sonos task, not the UI task. The UI task SHALL only read a snapshot of the already-fetched list.

#### Scenario: UI task is not blocked during fetch

- **WHEN** the device is fetching or refreshing the favorites list
- **THEN** the UI task continues to handle LVGL timer events and input without stalling
