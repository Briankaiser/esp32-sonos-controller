## Context

mbedTLS allocations are in PSRAM and fetches are serialized by a gate, yet
internal free RAM still dips toward zero during TLS fetches. The remaining
consumers are unknown.

## Decisions

- A small `diag::FetchProbe` object is created at the start of each fetch. It
  stores the internal free heap at that moment and, at each named phase, updates
  a per-kind maximum of (start free - current free). Static storage only; no
  allocation.
- The measurement is device-wide (other tasks also allocate), so it is an
  upper bound on the fetch's own cost. Taking the maximum over many fetches
  makes the dominant phase visible regardless.
- Phases: `connect` (after the TCP/TLS connect returns), `first_read` (after
  the first body bytes are read), `done` (before the client is stopped).
- Counters are updated under the existing diag critical-section pattern.

## Risks

- Concurrent activity can inflate a phase's reading. Mitigated by reporting
  the maximum over many fetches and by the existing serialization of TLS.
