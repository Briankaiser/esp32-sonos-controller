# Delta Spec: sonos-discovery

## ADDED Requirements

### Requirement: Topology Updates Are Ingested from Pushed Notifications Between Discovery Cycles

While topology push notifications are healthy, zone-group changes made from any controller SHALL be reflected in the device's stored topology promptly upon notification, without waiting for the next periodic discovery cycle. Periodic SSDP discovery SHALL continue to run on its existing cadence to track speaker address changes and serve as the authoritative refresh. A pushed topology update SHALL NOT remove speakers from the stored speaker list solely because they are absent from a notification.

#### Scenario: Group created from the phone app

- **WHEN** a user groups two rooms from the Sonos app while the device is running
- **THEN** the device's stored topology reflects the new group within a few seconds
- **AND** the group-selection overlay, when opened, shows the new group without a manual refresh

#### Scenario: Topology notifications stop

- **WHEN** topology push notifications become unavailable
- **THEN** the stored topology continues to be refreshed by the periodic discovery cycle
- **AND** the device continues operating without error
