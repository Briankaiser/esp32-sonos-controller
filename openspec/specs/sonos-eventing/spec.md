## Purpose

Receives pushed state-change notifications from Sonos speakers over the local network so the device reflects transport, volume, and grouping changes near-instantly, replacing most steady-state polling. Manages the notification subscriptions' full lifecycle — establishment, renewal, re-targeting when the group coordinator changes, and recovery after network interruption — with automatic fallback to polling whenever push notifications are unavailable.

## Requirements

### Requirement: Device Subscribes to Pushed State Notifications from Speakers

While a home room speaker is resolved and the network is connected, the device SHALL establish push-notification subscriptions with speakers so that transport state, track metadata, volume, mute, and zone-group topology changes are delivered to the device without polling. Transport and track-metadata notifications SHALL be sourced from the speaker currently acting as the home room's group coordinator. Volume and mute notifications SHALL be sourced from the home room speaker itself. Topology notifications SHALL be sourced from one reachable speaker.

#### Scenario: Playback state changed from another controller

- **WHEN** subscriptions are established and a user pauses playback from the Sonos phone app
- **THEN** the device receives a pushed notification of the change
- **AND** the recorded transport state updates to Paused within 1 second without a poll having run

#### Scenario: Volume changed from another controller

- **WHEN** subscriptions are established and the home room's volume is changed from another controller
- **THEN** the device receives a pushed notification
- **AND** the recorded volume matches the new value without a poll having run

#### Scenario: No home room resolved

- **WHEN** no home room speaker is resolved
- **THEN** no subscriptions are established
- **AND** the device continues operating without error

### Requirement: Subscriptions Are Renewed Before Expiry

Each subscription SHALL be renewed before the lease period granted by the speaker expires. When a renewal is rejected or receives no response, the device SHALL attempt to establish a fresh subscription. Repeated failure SHALL NOT crash or reboot the device.

#### Scenario: Routine renewal

- **WHEN** a subscription's lease is past half its granted duration
- **THEN** the device renews it before expiry
- **AND** pushed notifications continue uninterrupted

#### Scenario: Speaker rebooted and forgot the subscription

- **WHEN** a renewal is rejected because the speaker no longer recognizes the subscription
- **THEN** the device establishes a fresh subscription with that speaker
- **AND** state notifications resume

### Requirement: Notifications Are Acknowledged Promptly and Processed Off the Rendering Task

The device SHALL acknowledge each inbound notification within the time window speakers tolerate before considering the delivery failed, independent of how long parsing takes. Parsing and state ingestion SHALL occur off the rendering task, and resulting state updates SHALL reach the interface through the existing thread-safe state interfaces.

#### Scenario: Notification arrives while the interface is animating

- **WHEN** a notification arrives while the interface is rendering
- **THEN** the notification is acknowledged promptly
- **AND** the interface's sustained frame rate does not drop below the stated budget

#### Scenario: Malformed notification payload

- **WHEN** a notification contains an unparseable payload
- **THEN** the notification is still acknowledged
- **AND** the previously recorded state remains unchanged
- **AND** the device does not crash or reboot

### Requirement: Transport Subscription Follows the Group Coordinator

When the home room's group coordinator changes — the home room joins a group, leaves a group, or the group's coordinator changes — the device SHALL move its transport/metadata subscription to the new coordinator. Notifications from a speaker that is no longer the relevant source SHALL be ignored rather than applied.

#### Scenario: Home room joins another group

- **WHEN** the home room becomes a member of a group coordinated by another speaker
- **THEN** the device subscribes to transport notifications from the new coordinator
- **AND** subsequent track metadata reflects what the coordinator reports

#### Scenario: Home room leaves a group

- **WHEN** the home room becomes standalone
- **THEN** the device re-targets transport notifications to the home room speaker
- **AND** stale notifications from the former coordinator are not applied

### Requirement: Eventing Health Is Monitored with Automatic Fallback to Polling

The device SHALL monitor whether push notifications are functioning. While eventing is unhealthy — subscriptions cannot be established or renewed, or expected notifications stop arriving — the device SHALL revert to periodic state polling at the pre-eventing cadence so state accuracy is preserved. When eventing recovers, steady-state polling SHALL be suspended again. Transitions between modes SHALL be logged to serial.

#### Scenario: Subscriptions cannot be established

- **WHEN** subscription attempts fail repeatedly
- **THEN** the device polls playback state periodically as it did before eventing existed
- **AND** the fallback is logged to serial

#### Scenario: Eventing recovers

- **WHEN** a subscription is successfully re-established after a fallback period
- **THEN** steady-state polling is suspended
- **AND** the recovery is logged to serial

### Requirement: Subscriptions Recover After Network Interruption

After WiFi reconnection or a change of the device's own network address, the device SHALL discard prior subscriptions and establish fresh ones registering the current callback address. State SHALL be refreshed promptly after resubscription so changes missed during the outage are not lost.

#### Scenario: WiFi drops and reconnects

- **WHEN** the device loses WiFi and reconnects
- **THEN** fresh subscriptions are established after reconnection
- **AND** the displayed state reflects the speaker's current state within a few seconds of reconnection

### Requirement: Eventing Remains Active While the Display Is Idle

Subscriptions SHALL be maintained — including renewals and coordinator re-targeting — for the entire time the display is idle. A pushed notification indicating the home room (or the group it belongs to) has started playing SHALL reach the existing wake path so the display wakes promptly, without waiting for a poll cycle.

#### Scenario: Music started on the home room while idle

- **WHEN** the device is idle and playback is started on the home room from another controller
- **THEN** a pushed notification delivers the Playing state
- **AND** the display wakes within approximately 1 second

#### Scenario: Idle home room is added to a playing group

- **WHEN** the device is idle and the home room is joined to an already-playing group from another controller
- **THEN** the topology notification re-targets the transport subscription to that group's coordinator
- **AND** the initial notification from the new coordinator delivers the Playing state and the display wakes

### Requirement: Eventing Operates Within Embedded Resource Bounds

The device SHALL bound the number of concurrent subscriptions and the memory consumed by inbound notifications. A notification larger than the accepted bound SHALL be drained and acknowledged without being parsed, and SHALL NOT corrupt state or exhaust memory. Eventing SHALL leave enough network sockets available for the device's other services (control requests, album art, web endpoints) to keep functioning.

#### Scenario: Oversized notification

- **WHEN** a speaker delivers a notification body exceeding the device's accepted size bound
- **THEN** the notification is discarded without being applied
- **AND** the device continues to accept subsequent notifications

#### Scenario: Concurrent bursts of notifications

- **WHEN** several speakers deliver notifications at nearly the same time
- **THEN** the device processes or discards each without deadlock or memory exhaustion
- **AND** control requests and the web endpoints remain functional
