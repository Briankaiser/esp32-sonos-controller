## ADDED Requirements

### Requirement: Device Joins The Strongest Node Of Its Network

When the device joins its home WiFi network, at boot and on every automatic
reconnect, it SHALL consider every access point advertising the configured
network name across all channels and SHALL join the one with the strongest
signal, not merely the first one found. Only access points of the configured
network name are candidates.

#### Scenario: Several nodes share the network name

- **WHEN** the device joins the home network and several nodes of that network are in range
- **THEN** it joins the node with the strongest signal

#### Scenario: A single node in range

- **WHEN** only one node of the network is in range
- **THEN** the device joins it as before

### Requirement: Device Moves To A Clearly Stronger Node When Idle

While connected, if the signal has stayed weak (at or below -75 dBm) for at
least 10 minutes AND the device is idle (no playback for the idle period) AND
there is enough free internal memory to scan safely, the device SHALL scan for
other nodes of the same network. If one is at least 8 dB stronger than the
current link, the device SHALL re-join the network through it. The device SHALL
NOT attempt such a move more than once every 30 minutes, whether or not the
previous attempt found a better node, and SHALL never move while playing. If a
move fails to reconnect, the device SHALL recover through its normal reconnect
behaviour. Speaker playback is unaffected by a move; the controller's own
connection may be interrupted for a few seconds.

#### Scenario: Weak link with a clearly better node nearby

- **WHEN** the link has been at or below -75 dBm for 10 minutes while the device is idle
- **AND** a scan finds a node of the same network at least 8 dB stronger
- **THEN** the device re-joins through that node
- **AND** `wifi_reselects` in `/status` increases by one

#### Scenario: No clearly better node

- **WHEN** the link is weak and idle but no node is at least 8 dB stronger
- **THEN** the device stays connected where it is
- **AND** it does not scan again for at least 30 minutes

#### Scenario: Playback in progress

- **WHEN** the link is weak but the device is not idle
- **THEN** the device does not scan or move

### Requirement: Status Endpoint Reports The Connected Node

The `GET /status` response SHALL additionally include, from cached state only:
`wifi_bssid` (the hardware address of the node the device is connected to, as a
string, or an empty string when not connected), `wifi_channel` (the channel as an
integer, or `null` when not connected), and `wifi_reselects` (the number of times
the device has moved to a stronger node since startup).

#### Scenario: Reading the connected node

- **WHEN** a client issues `GET /status` while the device is connected
- **THEN** the response includes the node address, channel and move count
- **AND** no outbound network call is made to produce them
