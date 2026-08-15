## ADDED Requirements

### Requirement: Running Firmware Version Is Discoverable

The device SHALL be built with a firmware version string that identifies the
running build. On boot, after establishing the WiFi connection, the device SHALL
log this version string to the serial console. The version string SHALL also be
reported by the status endpoint (see below), so an operator or tool can tell
which firmware a device is running without a USB connection. When build-time
version metadata is unavailable, the device SHALL report a non-empty fallback
value rather than an empty string.

#### Scenario: Firmware version is logged on boot

- **WHEN** the device boots and connects to the home WiFi network
- **THEN** the device prints its firmware version string to the serial console

#### Scenario: Firmware version is never empty

- **WHEN** the firmware is built without usable version metadata
- **THEN** the reported firmware version is a non-empty fallback string
- **AND** it is never reported as an empty string or absent field

### Requirement: Status Endpoint Returns Cached Device Status

The device SHALL expose `GET /status` on port 80. The response SHALL be a JSON
object describing the device's current state, and the `Content-Type` header
SHALL be `application/json`. The endpoint SHALL require no authentication.

Every field in the response SHALL be served from state the device already holds
in memory or in NVS. Handling a `GET /status` request SHALL NOT trigger any SSDP
discovery, SOAP call, or other outbound network request; it is a read of cached
state only.

The JSON object SHALL include at least the following fields:

- `device`: the constant string `"opensonos-controller"`, a stable identity
  marker that lets a network scan distinguish this device from any other HTTP
  server on port 80.
- `firmware_version`: the running firmware version string (never empty).
- `wifi_connected`: boolean, true when the device is associated with the home
  WiFi network.
- `ip`: the device's current IP address as a string, or an empty string when
  not connected.
- `wifi_ssid`: the SSID of the connected WiFi network, or an empty string when
  not connected.
- `speaker_count`: integer count of Sonos speakers currently in the device's
  discovered inventory.
- `home_room_name`: the friendly name of the resolved home room, or an empty
  string when the home room is not currently resolved.
- `home_room_uuid`: the configured home room UUID, or an empty string when none
  is configured.
- `state_source`: `"gena"` when Sonos state is currently driven by healthy GENA
  event subscriptions, otherwise `"polling"`.

The HTTP server is only running while the device is connected to the home WiFi
network, so in practice a caller that reaches `/status` always observes
`wifi_connected: true` with a populated `ip` and `wifi_ssid`. The empty-string
values specified for the disconnected case are defensive defaults, not an
externally reachable state of this endpoint.

#### Scenario: Status returns cached JSON

- **WHEN** a client on the home network issues `GET /status`
- **THEN** the device responds with HTTP 200
- **AND** the `Content-Type` header is `application/json`
- **AND** the body is a JSON object containing `device`, `firmware_version`,
  `wifi_connected`, `ip`, `wifi_ssid`, `speaker_count`, `home_room_name`,
  `home_room_uuid`, and `state_source`
- **AND** the `device` field equals `"opensonos-controller"`

#### Scenario: Status handler performs no network I/O

- **WHEN** a client issues `GET /status`
- **THEN** the device answers from cached state only
- **AND** the request does not trigger an SSDP search, a SOAP request, or any
  other outbound network call

#### Scenario: Status reflects the active state source

- **WHEN** a client issues `GET /status` while GENA event subscriptions are
  healthy
- **THEN** the `state_source` field is `"gena"`
- **WHEN** a client issues `GET /status` while GENA subscriptions are not
  established
- **THEN** the `state_source` field is `"polling"`

#### Scenario: Status reflects an unresolved home room

- **WHEN** a client issues `GET /status` and no home room is currently resolved
  against the discovered speaker inventory
- **THEN** the device responds with HTTP 200
- **AND** the `home_room_name` field is an empty string
- **AND** the `home_room_uuid` field reflects the configured UUID, or an empty
  string when none is configured
