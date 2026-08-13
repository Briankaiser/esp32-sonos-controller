# Delta Spec: connection-status

## Purpose

Continuously diagnoses the device's current connectivity fault from local signals only, presents a distinct full-screen status per fault with plain-language guidance, recovers silently when the fault clears, and owns the on-device action that re-runs WiFi setup. Turns the previously indefinite "Connecting…" dead-end into an explained, recoverable state.

## ADDED Requirements

### Requirement: Device Diagnoses a Single Connectivity Fault from Local Signals

The device SHALL continuously determine which connectivity fault, if any, currently applies, using only local signals — WiFi station connectivity and LAN speaker discovery. It SHALL NOT consult Home Assistant, the Sonos cloud, or any external service to make this determination. When more than one condition is true, the device SHALL present only the highest-priority fault in this order: (1) no WiFi connection, (2) no Sonos system found, (3) home room offline. When none apply, no fault SHALL be presented and normal operation proceeds.

#### Scenario: WiFi down takes priority

- **WHEN** the device is not connected to its WiFi network
- **THEN** the diagnosed fault is "no WiFi connection" regardless of any speaker or home-room state

#### Scenario: WiFi up but no speakers

- **WHEN** WiFi is connected and discovery has found zero speakers
- **THEN** the diagnosed fault is "no Sonos system found"

#### Scenario: Speakers present but home room missing

- **WHEN** WiFi is connected, at least one speaker is discovered, and a home room is configured but not among the discovered speakers
- **THEN** the diagnosed fault is "home room offline"

#### Scenario: Everything reachable

- **WHEN** WiFi is connected, speakers are discovered, and the configured home room is resolved
- **THEN** no fault is presented and the now-playing screen operates normally

### Requirement: No-WiFi Fault Screen Offers a Confirmed Re-Setup Action

When the diagnosed fault is "no WiFi connection", the device SHALL display a full-screen status, positioned within the round display's inscribed safe-area circle, that states in plain language that it cannot reach the configured WiFi network. The screen SHALL present a control to re-run WiFi setup. Activating that control SHALL require an explicit on-screen confirmation before any destructive action. On confirmation, the device SHALL clear the stored WiFi credentials and reboot into the `OpenSonos-Setup` captive portal. On cancellation, the device SHALL return to the status screen with no change to stored credentials.

#### Scenario: No-WiFi screen shown

- **WHEN** the device cannot connect to its WiFi network
- **THEN** a full-screen "No WiFi connection" status is shown within the safe-area circle
- **AND** a control to re-run WiFi setup is visible

#### Scenario: Re-setup requires confirmation

- **WHEN** the user activates the re-run-WiFi-setup control
- **THEN** the device asks for an explicit confirmation before doing anything destructive
- **AND** no credentials are cleared unless the user confirms

#### Scenario: Confirmed re-setup wipes credentials and enters portal

- **WHEN** the user confirms the re-run-WiFi-setup action
- **THEN** the device clears the stored WiFi credentials
- **AND** the device reboots into the `OpenSonos-Setup` captive portal

#### Scenario: Cancelled re-setup changes nothing

- **WHEN** the user cancels the confirmation
- **THEN** stored credentials are unchanged
- **AND** the device remains on the no-WiFi status screen

### Requirement: No-Sonos-Found Fault Screen After a Bounded Search

When WiFi is connected but discovery has found zero speakers after a bounded startup search window (on the order of 15 seconds), the device SHALL display a full-screen status, positioned within the round display's inscribed safe-area circle, stating that no Sonos system was found and suggesting the user verify their Sonos is powered on and on the same network. During the initial search window, before it elapses, the device MAY show a transient searching/connecting indication instead. Discovery SHALL continue retrying while the status is shown.

#### Scenario: No speakers after the search window

- **WHEN** WiFi is connected and no speakers have been discovered once the bounded search window has elapsed
- **THEN** a full-screen "No Sonos system found" status is shown within the safe-area circle
- **AND** the device keeps retrying discovery in the background

#### Scenario: Brief search does not immediately alarm

- **WHEN** the device has just booted or reconnected and discovery has not yet completed its first search window
- **THEN** a transient searching indication MAY be shown rather than the "No Sonos system found" status

### Requirement: Home-Room-Offline Fault Screen Names the Missing Room

When WiFi is connected and speakers are discovered, but a configured home room is not present among them, the device SHALL display a full-screen status, positioned within the round display's inscribed safe-area circle, that names the configured home room and indicates the device will reconnect automatically when it returns. The device SHALL keep retrying discovery. This fault applies only when a home room is configured; when no home room is configured, this fault SHALL NOT be raised.

#### Scenario: Configured home room absent

- **WHEN** speakers are discovered but the configured home room is not among them
- **THEN** a full-screen status naming the home room and stating it is offline is shown within the safe-area circle
- **AND** the device keeps retrying discovery in the background

#### Scenario: No home room configured is not this fault

- **WHEN** speakers are discovered and no home room has been configured
- **THEN** the home-room-offline status is not shown

### Requirement: Fault Screens Recover Silently When the Condition Clears

Every fault status SHALL clear automatically, with no user interaction, as soon as its underlying condition is resolved, and normal operation SHALL resume. Because diagnosis is continuous, a fault that resolves while its screen is shown SHALL be dismissed promptly.

#### Scenario: WiFi returns

- **WHEN** the no-WiFi status is shown and WiFi reconnects
- **THEN** the status clears automatically and the device proceeds toward normal operation

#### Scenario: Sonos appears

- **WHEN** the no-Sonos-found status is shown and a speaker is discovered
- **THEN** the status clears automatically

#### Scenario: Home room returns

- **WHEN** the home-room-offline status is shown and the configured home room reappears in discovery
- **THEN** the status clears automatically and the now-playing screen resumes
