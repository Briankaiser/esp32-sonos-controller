## Purpose

Allows the user to join the home room speaker into another speaker's group, or remove it from a group, entirely from the device. All group operations are direct UPnP SOAP over the LAN.

## Requirements

### Requirement: User Can Join the Home Room to Another Speaker's Group

The device SHALL allow the user to select any other discovered speaker or group and add the home room speaker to that group. The join operation SHALL be executed on the sonos task. Once complete, the home room speaker SHALL play in sync with the target group within the next poll cycle.

#### Scenario: User joins home room to another speaker's group

- **WHEN** the user selects another speaker from the grouping overlay
- **THEN** the home room speaker joins that speaker's group
- **AND** the transport state reflects grouped playback within approximately 2 s

#### Scenario: Home room is already in a group

- **WHEN** the user selects a different group while the home room is already in a group
- **THEN** the home room leaves its current group and joins the new one

### Requirement: User Can Remove the Home Room from Its Current Group

The device SHALL allow the user to make the home room speaker standalone, removing it from any group it currently belongs to or dissolving any group it currently coordinates. After the operation, the home room speaker SHALL play independently.

#### Scenario: User leaves group

- **WHEN** the user selects "Go Solo" from the grouping overlay while the home room is in a group
- **THEN** the home room speaker becomes standalone
- **AND** the transport state reflects independent playback within approximately 2 s

#### Scenario: Go Solo when already standalone

- **WHEN** the user selects "Go Solo" and the home room is already standalone
- **THEN** no command is issued
- **AND** the overlay closes

### Requirement: Device Shows Current Group State and a Group-Selection Overlay

The now-playing screen SHALL display a visible indicator of the home room's current group state. Activating the indicator SHALL open an overlay listing all discovered speakers and a "Go Solo" option. The overlay SHALL be dismissible without taking any action. The overlay content SHALL be positioned within the display's safe area on round displays.

#### Scenario: Status indicator shows solo state

- **WHEN** the home room is not in any group
- **THEN** the status indicator reads "Solo"

#### Scenario: Status indicator shows grouped state

- **WHEN** the home room is in a group with N total members
- **THEN** the status indicator shows the group member count or coordinator name

#### Scenario: Overlay opens and lists speakers

- **WHEN** the user activates the status indicator
- **THEN** an overlay appears listing all discovered speakers with their current group status
- **AND** a "Go Solo" option is present
- **AND** all list items are within the safe area on round displays

#### Scenario: Overlay closes without action

- **WHEN** the user taps outside the overlay list items
- **THEN** the overlay dismisses without issuing any command
