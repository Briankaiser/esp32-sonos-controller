## Purpose

Exposes an HTTP server on the device's home WiFi network for: capturing the current display as a BMP image for visual review, uploading new firmware over the air without a USB connection, and managing home room configuration remotely.
## Requirements
### Requirement: Device Connects to Home WiFi on Boot

After confirming provisioned credentials are present, the device SHALL connect to the home WiFi network using the SSID and password stored in NVS. The device SHALL log its assigned IP address to serial once the connection is established, so the operator can locate the HTTP endpoints without a network scan.

#### Scenario: Successful WiFi connection logs IP

- **WHEN** the device boots with valid WiFi credentials in NVS
- **THEN** the device connects to the home WiFi network in station mode
- **AND** the device prints its assigned IP address to the serial console

#### Scenario: WiFi connection failure is reported

- **WHEN** the device attempts to connect to the home network and does not receive an IP within 15 seconds
- **THEN** the device prints a connection-failed message to the serial console
- **AND** the device continues booting without the HTTP server

### Requirement: Screenshot Endpoint Returns Current Display as BMP

The device SHALL expose `GET /screenshot` on port 80. A successful response SHALL contain the full contents of the active LVGL display rendered as a valid 24-bit BMP image. The endpoint SHALL require no authentication.

#### Scenario: Screenshot returns BMP with correct content type

- **WHEN** a client on the home network issues `GET /screenshot`
- **THEN** the device responds with HTTP 200
- **AND** the `Content-Type` header is `image/bmp`
- **AND** the response body is a valid BMP file representing the current display contents

#### Scenario: Concurrent screenshot request is serialized

- **WHEN** a second `GET /screenshot` request arrives while a screenshot is already being captured
- **THEN** the second request waits for the first to complete before the capture begins
- **AND** both requests eventually receive a valid BMP response

### Requirement: OTA Update Endpoint Accepts Authenticated Firmware Upload

The device SHALL expose `POST /update` on port 80. The endpoint SHALL require the correct OTA password, supplied in the `X-OTA-Password` request header, before accepting a firmware binary. On a valid upload the device SHALL apply the firmware and reboot. On an incorrect password the device SHALL respond with HTTP 401 and reject the upload without writing any data.

#### Scenario: Authenticated upload applies firmware and reboots

- **WHEN** a client sends a `POST /update` with the correct `X-OTA-Password` header and a valid firmware binary body
- **THEN** the device writes the firmware
- **AND** the device responds with HTTP 200 and a confirmation message
- **AND** the device reboots into the new firmware

#### Scenario: Wrong password is rejected before any write

- **WHEN** a client sends a `POST /update` with an incorrect `X-OTA-Password` header
- **THEN** the device responds with HTTP 401
- **AND** no firmware data is written to flash

#### Scenario: Malformed upload is rejected with 400

- **WHEN** a client sends a `POST /update` with the correct password but a body that is not a valid firmware image
- **THEN** the device responds with HTTP 400
- **AND** the device remains on the current firmware

### Requirement: Device Exposes Discovered Speaker List via HTTP

The device SHALL expose `GET /rooms` on port 80. The response SHALL be a JSON array where each element represents one discovered Sonos speaker. Each element SHALL include at minimum the speaker's UUID and its human-readable friendly name. When no speakers have been discovered yet, the response SHALL be an empty JSON array. The endpoint SHALL require no authentication.

#### Scenario: Speakers have been discovered

- **WHEN** a client issues `GET /rooms` and one or more Sonos speakers have been discovered
- **THEN** the device responds with HTTP 200
- **AND** the `Content-Type` header is `application/json`
- **AND** the body is a JSON array with one object per speaker containing `"uuid"` and `"name"` fields

#### Scenario: No speakers discovered yet

- **WHEN** a client issues `GET /rooms` and no discovery cycle has completed or no speakers were found
- **THEN** the device responds with HTTP 200
- **AND** the body is an empty JSON array `[]`

### Requirement: Device Accepts Home Room Selection via HTTP

The device SHALL expose `POST /rooms/select` on port 80. The endpoint SHALL accept a `uuid` parameter in either a URL-encoded form body or as a query parameter. If the UUID is non-empty, the device SHALL write it to NVS as the home room UUID. The next discovery cycle SHALL attempt to resolve the stored UUID against the speaker inventory. The endpoint SHALL require no authentication. If the UUID is empty or the parameter is absent, the device SHALL respond with HTTP 400.

#### Scenario: Valid UUID is stored as home room

- **WHEN** a client sends `POST /rooms/select` with a non-empty `uuid` parameter
- **THEN** the device responds with HTTP 200
- **AND** the `Content-Type` header is `application/json`
- **AND** the response body confirms the stored UUID
- **AND** the next discovery cycle resolves the home room if the UUID matches a discovered speaker

#### Scenario: Missing or empty UUID returns 400

- **WHEN** a client sends `POST /rooms/select` with an empty or absent `uuid` parameter
- **THEN** the device responds with HTTP 400
- **AND** the NVS home room value is unchanged

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
- `board`: the running board identifier string (never empty), naming the
  hardware profile the image was built for. A tool comparing this against a
  firmware image's target board can tell whether that image is compatible with
  this device.
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
  `board`, `wifi_connected`, `ip`, `wifi_ssid`, `speaker_count`,
  `home_room_name`, `home_room_uuid`, and `state_source`
- **AND** the `device` field equals `"opensonos-controller"`
- **AND** the `board` field is a non-empty string

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

### Requirement: Running Board Identifier Is Discoverable

The device SHALL be built with a board identifier string that names the hardware
profile the image was built for (the build's board/target identity, e.g.
`crowpanel_21_round`). On boot, after establishing the WiFi connection, the
device SHALL log this board identifier to the serial console. The board
identifier SHALL also be reported by the status endpoint (see below), so an
operator or tool can tell which board an image is running on — and therefore
which image is compatible with a given device — without a USB connection. The
board identifier SHALL be a non-empty, stable string for a given build target
and SHALL be identical across firmware versions built for the same board.

#### Scenario: Board identifier is logged on boot

- **WHEN** the device boots and connects to the home WiFi network
- **THEN** the device prints its board identifier string to the serial console

#### Scenario: Board identifier is stable across versions

- **WHEN** two firmware versions are built for the same board target
- **THEN** both report the same board identifier
- **AND** the identifier distinguishes them from an image built for a different
  board target

### Requirement: Status Endpoint Reports Memory Health And Device Identity

The `GET /status` response SHALL additionally include, served from cached state only:

- `device_id`: a stable per-device identifier that survives reboots and firmware updates and is unique across devices.
- `uptime_s`: seconds since the device last started.
- `heap_internal_free`, `heap_internal_min_free`, `heap_internal_largest`: current free internal RAM, the lowest free internal RAM since startup, and the largest contiguous free internal block, in bytes.
- `psram_free`, `psram_largest`: current free external RAM and its largest contiguous free block, in bytes.
- `alloc_fail_count`: the number of failed heap allocations since startup, and `alloc_fail_last`: the size, memory capabilities, calling function and task of the most recent failure.
- `dip_count` and `heap_dips`: the number of times internal free RAM fell below a low threshold since startup, and a bounded history of recent dips, each with the time since startup, duration, lowest free value, largest free block, and the activities (for example album-art fetch, TLS, JPEG decode, lyrics, speaker command, discovery, event notification) in progress at the time.
- `tls_psram_allocs` and `tls_fallback_allocs`: the number of TLS allocations placed in external RAM and the number that fell back to internal RAM.

Collecting these figures SHALL NOT affect interface frame rate or playback control.

#### Scenario: Reading memory health

- **WHEN** a client requests `GET /status`
- **THEN** the response includes the heap, external RAM, allocation-failure, dip-history and TLS allocation fields
- **AND** no outbound network request is made to produce them

#### Scenario: Distinguishing devices

- **WHEN** two controllers on the same network are queried
- **THEN** their `device_id` values differ
- **AND** each device reports the same `device_id` after a reboot

#### Scenario: Near-exhaustion is recorded

- **WHEN** internal free RAM briefly falls below the low threshold while an album-art fetch is in progress
- **THEN** a later `/status` response shows a dip entry listing the album-art fetch among the activities
- **AND** `heap_internal_min_free` reflects the lowest value reached

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

