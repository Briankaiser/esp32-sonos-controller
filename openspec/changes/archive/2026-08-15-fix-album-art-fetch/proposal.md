## Why

Album art frequently failed to appear — most visibly for Spotify — surfacing as
`[Art] JPEGDEC openRAM failed (239 bytes)` on the serial log. Three fetch
assumptions were wrong: HTTP error bodies were decoded as if they were images;
absolute art URLs (Spotify hands back `https://i.scdn.co/...`) were requested
from the Sonos speaker instead of their own host, over plain HTTP; and art was
requested from an arbitrary speaker rather than the one actually playing the
track. The result was a stream of 404s and no artwork.

## What Changes

- The device fetches album art from the correct source: for a relative
  `/getaa`-style path, from the **group coordinator** of the home room (the
  speaker actually playing the track — a grouped member or unrelated room
  returns 404); for an **absolute** URL, from that URL's own host, using
  **HTTPS** when the URL is `https` (e.g. Spotify's image CDN).
- The device rejects non-image responses cleanly: a non-2xx status, or a body
  that is not a JPEG, is discarded without being handed to the decoder, so an
  error page can never be mis-decoded and the previous art / placeholder stays.
- No behavioral change to decode, dominant-colour extraction, or the off-UI-task
  threading model.

This stays within scope: art is fetched directly over the LAN / from the
service's own CDN — no Home Assistant, no Sonos cloud API.

## Capabilities

### Modified Capabilities
- `album-art`: Adds requirements for fetching art from the correct source
  (coordinator for relative paths; the URL's own host over HTTPS for absolute
  URLs) and for rejecting non-image responses without decoding them.

## Impact

- **Firmware**: `src/app/sonos/art.cpp` only — `fetch_jpeg` now parses absolute
  URLs and connects to their host (`WiFiClientSecure` + `setInsecure()` for
  `https`; plain `WiFiClient` for relative paths on the speaker), parses the HTTP
  status line and bails on non-2xx, and guards the JPEG SOI marker before decode;
  the art task selects the home room's group coordinator as the fetch source.
- **Dependencies**: uses the ESP32 Arduino `WiFiClientSecure` (already in the
  core); no new libraries.
- **No** change to the now-playing screen layout, the art buffers, or the
  sonos/art task structure.
