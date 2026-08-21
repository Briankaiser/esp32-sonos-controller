## MODIFIED Requirements

### Requirement: Tool Discovers Controllers by Scanning the Local Subnet

The tool SHALL scan a local IPv4 subnet for OpenSonos controllers by issuing
`GET /status` to hosts on the subnet and treating a host as a controller only
when the response is valid JSON whose `device` field equals
`"opensonos-controller"`. The tool SHALL determine the subnet to scan
automatically (from the host's own network configuration) and SHALL also accept
an explicitly supplied subnet or CIDR. The scan SHALL probe hosts concurrently
so a typical `/24` completes in seconds, and SHALL use a short per-host timeout
so unreachable addresses do not stall the scan. Hosts that do not respond, that
respond with non-JSON, or whose `device` marker does not match SHALL be silently
ignored.

For each discovered controller the tool SHALL record, from its `/status`
response, at least its IP address, home room name, firmware version, and board
identifier (the `board` field). When a controller's response omits the `board`
field — a legacy controller predating board reporting — the tool SHALL record
its board identifier as unknown rather than dropping the controller from the
discovered set.

To avoid silently missing a controller that is only briefly unresponsive (for
example rebooting, busy, or mid-DHCP) during a single scan, the tool SHALL run
the scan more than once by default and SHALL treat the union of all passes as the
discovered set. The number of passes SHALL be configurable.

#### Scenario: Controllers on the subnet are discovered

- **WHEN** the tool scans a subnet on which one or more controllers are reachable
- **THEN** each host returning a `/status` JSON body with
  `device` == `"opensonos-controller"` is collected as a discovered controller
- **AND** its IP, home room name, firmware version, and board identifier are
  recorded from the response

#### Scenario: A controller that does not report a board is recorded as unknown

- **WHEN** a discovered controller's `/status` response has no `board` field
- **THEN** the tool still collects it as a discovered controller
- **AND** records its board identifier as unknown

#### Scenario: Non-controllers are ignored

- **WHEN** a scanned host does not respond, returns non-JSON, or returns JSON
  whose `device` field is absent or not `"opensonos-controller"`
- **THEN** the tool does not treat that host as a controller
- **AND** the scan continues without error

#### Scenario: A briefly-unresponsive controller is still discovered

- **WHEN** a controller responds to `/status` on at least one discovery pass but
  not on another
- **THEN** the tool includes it in the discovered set via the union of passes
- **AND** it is not silently dropped from the fleet

## ADDED Requirements

### Requirement: Tool Updates Only Controllers Matching the Firmware's Target Board

The tool SHALL determine the target board of the firmware image being rolled out
by reading a board identifier embedded in the image itself, so the image is
self-describing and no companion file is required. When the image carries a board
identifier, the tool SHALL flash only those discovered controllers whose reported
board identifier equals the image's target board. A controller whose board does
not match, and a controller that does not report a board at all, SHALL be
excluded from the set of controllers to update. Excluded controllers SHALL be
reported to the operator with the reason for exclusion — never silently dropped —
so the operator can see that a device was intentionally skipped rather than
missed.

The tool SHALL provide an explicit override flag (e.g. `--force`) that disables
board matching and updates every discovered controller regardless of board, for
the deliberate case where the operator accepts responsibility for a cross-board
flash.

When the firmware image carries no board identifier — a legacy image built before
board embedding — the tool SHALL fall back to its prior behavior of not filtering
by board, and SHALL warn the operator that board matching is unavailable for this
image. This fallback SHALL NOT make the tool less safe than a board-unaware
version: version-based selection and all confirmation, verification, and
abort-on-failure behavior continue to apply.

Board matching SHALL be applied when selecting which controllers to update, in
addition to (not instead of) skipping controllers already on the target firmware
version.

#### Scenario: Matching-board controllers are updated

- **WHEN** the firmware image declares a target board and one or more discovered
  controllers report the same board
- **THEN** those controllers are eligible for update (subject to version and
  confirmation)

#### Scenario: Mismatched-board controllers are excluded and reported

- **WHEN** a discovered controller reports a board different from the image's
  target board
- **THEN** the tool excludes it from the controllers to update
- **AND** reports it as skipped, stating that its board does not match the image

#### Scenario: Unknown-board controllers are excluded and reported

- **WHEN** a discovered controller does not report a board and the image declares
  a target board
- **THEN** the tool excludes it from the controllers to update
- **AND** reports it as skipped, stating that its board could not be determined

#### Scenario: Force overrides board matching

- **WHEN** the operator passes the override flag (e.g. `--force`)
- **THEN** the tool disables board matching
- **AND** every discovered controller is eligible for update regardless of board

#### Scenario: Legacy image without a board identifier falls back safely

- **WHEN** the firmware image carries no embedded board identifier
- **THEN** the tool does not filter controllers by board
- **AND** warns the operator that board matching is unavailable for this image
- **AND** continues to apply version selection, confirmation, verification, and
  abort-on-failure exactly as before
