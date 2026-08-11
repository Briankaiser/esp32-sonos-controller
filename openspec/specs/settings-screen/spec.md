## Purpose

A standalone settings screen reachable from the now-playing screen. Provides a scrollable list of configuration entries within the round safe area. The first release includes three entries: home room selection, group management, and group volume mode.

## Requirements

### Requirement: Settings Screen Is Reachable from Now-Playing via SLOT_STATUS Gear Icon

The now-playing screen SHALL display a settings entry-point control (a gear icon) in the `SLOT_STATUS` area. The gear icon SHALL be the sole content of `SLOT_STATUS`; no group status label is shown on the now-playing screen. Tapping the gear icon SHALL navigate to the settings screen. The settings screen SHALL have a back control that returns to the now-playing screen. Navigation SHALL not interrupt playback.

#### Scenario: User opens settings from now-playing

- **WHEN** the user taps the gear icon in the status area on the now-playing screen
- **THEN** the settings screen becomes the active screen
- **AND** audio playback continues uninterrupted

#### Scenario: User returns from settings

- **WHEN** the user taps the back control on the settings screen
- **THEN** the now-playing screen becomes the active screen

#### Scenario: Gear icon is within the safe area

- **WHEN** the gear icon is rendered on a round display
- **THEN** it is positioned entirely within the inscribed disc safe area

### Requirement: Settings Screen Renders a Scrollable Entry List

The settings screen SHALL display a vertically scrollable list of settings entries within the safe area. Each entry SHALL show a label and its current value (or a placeholder when no value is set). Entries that do not fit in the visible area SHALL be reachable by scrolling. The list SHALL be positioned and clipped within the display's safe area.

#### Scenario: Settings list is scrollable when it overflows

- **WHEN** the settings list contains more entries than fit in the visible safe area
- **THEN** the user can scroll the list to reveal additional entries

#### Scenario: All entries visible without scroll on initial release

- **WHEN** the settings list contains the three initial entries (Home Room, Manage Groups, Group Volume)
- **THEN** all entries are accessible; scrolling is not required if layout permits

### Requirement: Room-Selection Entry Displays the Current Home Room

The settings screen SHALL include a "Home Room" entry that shows the friendly name of the currently configured home room speaker. When no home room UUID is stored in NVS, or the stored UUID does not match any discovered speaker, the entry SHALL display a placeholder (e.g., "Not set"). The displayed name SHALL be read from the discovered speaker inventory; it SHALL NOT be fetched over the network at render time.

#### Scenario: Home room is configured and speaker is discovered

- **WHEN** a home room UUID is stored in NVS and a speaker with that UUID has been discovered
- **THEN** the Home Room entry shows that speaker's friendly name as its current value

#### Scenario: No home room configured

- **WHEN** no home room UUID is stored in NVS
- **THEN** the Home Room entry shows a "Not set" placeholder

#### Scenario: Stored UUID not in discovered list

- **WHEN** a home room UUID is stored but no discovered speaker matches it
- **THEN** the Home Room entry shows the UUID or a "Not found" placeholder rather than crashing

### Requirement: Tapping Home Room Entry Opens a Speaker Picker

Tapping the Home Room entry SHALL open a full-screen overlay listing all currently discovered, independently playable speakers. A bonded-set satellite (a home theater surround or subwoofer, or the non-primary unit of a stereo pair) is not independently playable and SHALL be excluded from this list even though it is separately discovered. Each list item SHALL show the speaker's friendly name. Tapping a speaker SHALL persist its UUID as the home room, close the overlay, and update the Home Room entry to reflect the chosen name. Tapping outside the list (or a cancel control) SHALL close the overlay without changing the stored value. The overlay SHALL be positioned within the safe area on round displays.

#### Scenario: User picks a speaker

- **WHEN** the user taps the Home Room entry and then taps a speaker in the overlay
- **THEN** that speaker's UUID is written to NVS as the home room
- **AND** the overlay closes
- **AND** the Home Room entry updates to show the newly selected speaker's friendly name
- **AND** the next discovery cycle resolves the home room against the new UUID

#### Scenario: User cancels the picker

- **WHEN** the user opens the picker overlay and then taps outside the list or a cancel control
- **THEN** the overlay closes
- **AND** the NVS home room value is unchanged

#### Scenario: Picker is empty when no speakers discovered

- **WHEN** the user opens the picker overlay and no speakers have been discovered yet
- **THEN** the overlay shows a message indicating no speakers are available
- **AND** no speaker entries are shown

#### Scenario: Picker overlay respects safe area on round display

- **WHEN** the picker overlay is displayed on a round display
- **THEN** all list entries and controls are drawn within the inscribed disc safe area

#### Scenario: Bonded-set satellites are excluded from the picker

- **WHEN** a discovered speaker is a home theater surround/subwoofer or the non-primary unit of a stereo pair
- **THEN** it does not appear as a selectable item in the picker overlay

### Requirement: Settings Screen Provides Access to Group Management

The settings screen SHALL include a "Manage Groups" entry. Tapping it SHALL open the group join/leave overlay (the same overlay previously accessible from the now-playing status strip). The overlay SHALL list all discovered, independently playable speakers; bonded-set satellites (home theater surrounds/subwoofer, or the non-primary unit of a stereo pair) SHALL be excluded even though they are separately discovered. Tapping one joins the home room to that group; a "Go Solo" option detaches the home room from its current group. The overlay SHALL be positioned within the safe area on round displays.

#### Scenario: User opens group management from settings

- **WHEN** the user taps "Manage Groups" in the settings list
- **THEN** the group overlay opens over the settings screen
- **AND** the overlay lists all discovered, independently playable speakers and a "Go Solo" option

#### Scenario: User joins a group from settings

- **WHEN** the group overlay is open and the user taps a speaker entry
- **THEN** a JoinGroup command is issued for the home room
- **AND** the overlay closes

#### Scenario: User goes solo from settings

- **WHEN** the group overlay is open and the user taps "Go Solo"
- **THEN** a LeaveGroup command is issued for the home room
- **AND** the overlay closes

#### Scenario: Group overlay respects safe area on round display

- **WHEN** the group overlay is displayed on a round display
- **THEN** all list entries and controls are drawn within the inscribed disc safe area

### Requirement: Group Volume Mode Setting Controls Volume Scope

The settings screen SHALL include a "Group Volume" toggle entry. The toggle SHALL have two states: "Home room only" (default) and "Whole group". The selected state SHALL be persisted in NVS and survive a reboot. When "Whole group" is active and the home room is a member of a zone group, volume changes SHALL be applied to every speaker in that zone group, not only the home room. When "Home room only" is active, or when the home room is not in any group, volume changes SHALL affect only the home room speaker.

#### Scenario: Default state is home room only

- **WHEN** the device has never had the Group Volume setting changed
- **THEN** the toggle shows "Home room only"
- **AND** volume changes affect only the home room speaker

#### Scenario: User switches to whole-group volume

- **WHEN** the user taps the Group Volume toggle and selects "Whole group"
- **THEN** the setting is persisted to NVS
- **AND** subsequent volume changes are applied to all speakers in the home room's zone group

#### Scenario: Whole-group volume when home room is standalone

- **WHEN** "Whole group" is active and the home room is not in any zone group
- **THEN** volume changes affect only the home room speaker (same as "Home room only")

#### Scenario: Rebooting retains the group volume setting

- **WHEN** the user sets Group Volume to "Whole group" and the device reboots
- **THEN** the setting is restored to "Whole group" from NVS
