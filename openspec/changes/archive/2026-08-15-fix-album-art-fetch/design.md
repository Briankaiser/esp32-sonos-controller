## Context

See proposal.md — Why. The fix lives entirely in `src/app/sonos/art.cpp`
(`fetch_jpeg` + the art task's source selection). The original code assumed every
art URL was a relative path served by any speaker over plain HTTP; three real
cases break that assumption, and the diagnosis was confirmed on hardware.

## Goals / Non-Goals

**Goals:**
- Album art appears for Spotify (absolute HTTPS CDN URLs) and for grouped rooms.
- An error response can never be mis-decoded as an image.

**Non-Goals:**
- No CA-certificate pinning/validation for art fetches (public images only).
- No HTTP-redirect following (not needed for the observed CDN responses).
- No change to decode, colour extraction, or the task model.

## Decisions

### Absolute URLs are fetched from their own host; HTTPS via WiFiClientSecure

`fetch_jpeg` now classifies the URL: a leading `/` is a relative path served by a
Sonos speaker (plain HTTP, existing behavior); anything with a scheme is absolute
and is fetched from its own host/port. `https` uses `WiFiClientSecure` with
`setInsecure()` — album art is public, so skipping certificate validation avoids
shipping and maintaining a CA bundle while still encrypting the transport. Both
transports share the read path via a `Client*` base pointer.

- *Alternative — route Spotify art through the speaker's `/getaa?u=<trackURI>`
  proxy (no TLS on device):* rejected. It requires plumbing the track's `res`
  URI into the art task and depends on the speaker's proxy behavior; direct fetch
  is self-contained in `art.cpp` and works for any absolute-URL service. TLS cost
  (~30–40 KB heap, ~1.5 s including handshake, measured) is acceptable for a
  single sequential art fetch on a board with 8 MB PSRAM.

### Relative art is fetched from the group coordinator

Sonos serves the `/getaa` proxy only from the speaker actually playing the track —
the group coordinator. The art task resolves the home room's coordinator from
`PlaybackState.group_coordinator_uuid` (empty or equal to the home room ⇒ the home
room is itself the coordinator) and fetches from there, replacing the previous
"pick an arbitrary non-home-room speaker" logic, which returned 404 for any
speaker not playing the track.

### Reject non-image responses before decoding

`fetch_jpeg` parses the HTTP status line and returns early on non-2xx; before
`openRAM`, the body is checked for the JPEG SOI marker (`0xFF 0xD8`). Either
failure logs a diagnostic and leaves the previous art/placeholder in place,
instead of feeding an error page to JPEGDEC.

## Risks / Trade-offs

- **TLS heap during handshake** → concurrent with LVGL/WiFi; measured fine on the
  CrowPanel (8 MB PSRAM). If a future service needs a larger TLS record, buffer
  sizes may need tuning. Mitigation: art fetch is sequential on its own task.
- **`setInsecure()` skips cert validation** → acceptable for public album art; no
  credentials or private data traverse the connection.
