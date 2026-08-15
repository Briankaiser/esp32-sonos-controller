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

To avoid silently missing a controller that is only briefly unresponsive (for
example rebooting, busy, or mid-DHCP) during a single scan, the tool SHALL run
the scan more than once by default and SHALL treat the union of all passes as the
discovered set. The number of passes SHALL be configurable.

#### Scenario: Controllers on the subnet are discovered

- **WHEN** the tool scans a subnet on which one or more controllers are reachable
- **THEN** each host returning a `/status` JSON body with
  `device` == `"opensonos-controller"` is collected as a discovered controller
- **AND** its IP, home room name, and firmware version are recorded from the response

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
