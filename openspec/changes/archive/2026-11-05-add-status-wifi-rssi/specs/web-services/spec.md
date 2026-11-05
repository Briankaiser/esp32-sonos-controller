## ADDED Requirements

### Requirement: Status Endpoint Reports WiFi Signal Strength

The `GET /status` response SHALL include `wifi_rssi`: the received signal
strength of the device's current WiFi connection, as an integer in dBm
(typically between -30 and -90, higher meaning stronger). The value SHALL come
from state the WiFi driver already holds; reading it SHALL NOT cause any
network traffic. When the device is not associated with a network the field
SHALL be `null`.

#### Scenario: Signal strength is reported

- **WHEN** a client issues `GET /status` while the device is connected to the home WiFi
- **THEN** the response includes `wifi_rssi` as a negative integer in dBm
- **AND** the request triggers no outbound network call

#### Scenario: Signal strength when not associated

- **WHEN** the device is not associated with a WiFi network
- **THEN** `wifi_rssi` is `null`, not zero
