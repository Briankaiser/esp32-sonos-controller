## ADDED Requirements

### Requirement: Status Endpoint Reports Fetch Memory Cost By Phase

The `GET /status` response SHALL include `fetch_mem`: for each kind of
network fetch (`art_tls`, `art_plain`, `lyrics`), the number of fetches
started since boot and, for each phase of a fetch (`connect`, `first_read`,
`done`), the largest drop in free internal RAM in bytes measured from the start
of that fetch. A phase that has never been reached SHALL be reported as `null`.
Recording a phase SHALL read heap counters only and SHALL NOT cause any network
traffic, allocate memory on the fetch's critical path, or change fetch behavior.

#### Scenario: A TLS art fetch is recorded

- **WHEN** an album-art fetch over HTTPS runs
- **THEN** `fetch_mem.art_tls.count` increases by one
- **AND** each phase reports the largest internal-RAM drop seen so far at that phase

#### Scenario: A kind that has not run yet

- **WHEN** no lyrics fetch has run since boot
- **THEN** `fetch_mem.lyrics.count` is `0` and its phase values are `null`

#### Scenario: A fetch that fails before data arrives

- **WHEN** a fetch fails after connecting but before reading data
- **THEN** the `connect` phase is recorded
- **AND** the later phases are left unchanged
