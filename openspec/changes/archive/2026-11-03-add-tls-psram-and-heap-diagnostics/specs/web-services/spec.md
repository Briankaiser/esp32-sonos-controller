## ADDED Requirements

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
