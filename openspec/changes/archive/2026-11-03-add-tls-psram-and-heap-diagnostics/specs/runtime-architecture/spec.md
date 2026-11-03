## MODIFIED Requirements

### Requirement: Interface Memory Is Allocated From External RAM

Interface draw buffers, cover-art buffers, and lyric buffers SHALL be allocated from external PSRAM. Memory used by transport-security (TLS) sessions SHALL also be allocated from external PSRAM, so that a secure fetch does not consume the internal RAM reserved for wireless networking and display scan-out. If external RAM is exhausted, a TLS allocation MAY fall back to internal RAM rather than fail. Memory reserved for wireless networking SHALL remain in internal RAM.

#### Scenario: Draw buffers placed in external RAM

- **WHEN** the device finishes starting up
- **THEN** the interface draw buffers reside in external PSRAM
- **AND** sufficient internal RAM remains available for wireless networking

#### Scenario: Lyric buffers placed in external RAM

- **WHEN** lyrics are fetched and parsed for a track
- **THEN** the buffers holding the retrieved lyrics and parsed lines reside in external PSRAM
- **AND** internal RAM remains available for wireless networking

#### Scenario: Secure fetch does not drain internal RAM

- **WHEN** the device fetches album art or lyrics over TLS
- **THEN** the TLS session's memory is placed in external PSRAM
- **AND** internal RAM free space does not fall to the point where allocations fail or the display flickers

#### Scenario: External RAM exhausted during a secure fetch

- **WHEN** external RAM cannot satisfy a TLS allocation
- **THEN** the allocation is satisfied from internal RAM if available
- **AND** the fallback is counted in the status output

#### Scenario: Allocation failure is reported

- **WHEN** an external RAM allocation for an interface buffer fails
- **THEN** the failure is reported over the diagnostic serial output
- **AND** the device does not silently render with a partial buffer

## ADDED Requirements

### Requirement: Secure Fetches Do Not Overlap

The device SHALL run at most one TLS session at a time across album-art and lyrics fetches. A fetch that cannot start because another TLS session is in flight SHALL wait for it, up to a bounded time, and SHALL then give up for that attempt without affecting playback or the rest of the interface. Lyrics remain best-effort: giving up degrades silently to the normal now-playing screen.

#### Scenario: Art and lyrics requested together

- **WHEN** a track change requires both an album-art fetch and a lyrics fetch over TLS
- **THEN** the two fetches run one after the other, not concurrently
- **AND** both complete if each finishes within the bounded wait

#### Scenario: Secure session stuck for too long

- **WHEN** another TLS session has been in flight longer than the bounded wait
- **THEN** the waiting fetch gives up for that attempt
- **AND** the interface and playback control remain responsive
