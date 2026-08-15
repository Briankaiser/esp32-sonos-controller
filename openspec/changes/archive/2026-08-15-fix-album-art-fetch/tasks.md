## 1. Fetch source & transport

- [x] 1.1 In `fetch_jpeg`, classify the art URL: relative path (speaker, plain HTTP) vs absolute URL (parse scheme/host/port/path).
- [x] 1.2 Fetch absolute URLs from their own host/port; use `WiFiClientSecure` + `setInsecure()` for `https`, plain `WiFiClient` for `http`/relative. Share the read path via a `Client*` base pointer.
- [x] 1.3 In the art task, select the home room's group coordinator (`PlaybackState.group_coordinator_uuid`) as the source for relative paths, instead of an arbitrary speaker.

## 2. Reject non-image responses

- [x] 2.1 Parse the HTTP status line in `fetch_jpeg`; return early on non-2xx.
- [x] 2.2 Guard the JPEG SOI marker (`0xFF 0xD8`) before `openRAM`; reject non-JPEG bodies cleanly, leaving prior art/placeholder.

## 3. Verification

- [x] 3.1 `pio run -e crowpanel_21_round` builds clean.
- [x] 3.2 `pio run -e ref_rect_stub` builds clean.
- [x] 3.3 `openspec validate --all --strict` is clean.
- [x] 3.4 Hardware check (CrowPanel): relative `/getaa` art decodes over plain HTTP from the speaker; a one-shot self-test confirmed an absolute `https://i.scdn.co` JPEG decodes over TLS (320×320, ~1.5 s); `/screenshot` shows album art rendered on the now-playing screen.
- [x] 3.5 Add a `CHANGELOG.md` entry under `## [Unreleased]` for the user-visible fix.
