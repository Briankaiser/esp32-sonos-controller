## 1. Join the strongest node

- [ ] 1.1 Scan all channels and sort by signal before every join (boot and automatic reconnect)

## 2. Move to a stronger node when idle

- [ ] 2.1 Add a low-priority monitor task: weak-signal dwell, idle gate, memory gate, 30-minute cooldown
- [ ] 2.2 Scan for same-network nodes and re-join when one is at least 8 dB stronger
- [ ] 2.3 Count moves and log each decision to serial

## 3. Observability

- [ ] 3.1 Serve `wifi_bssid`, `wifi_channel`, `wifi_reselects` on `/status`
- [ ] 3.2 Log them in `scripts/heap_log.py`

## 4. Verification

- [ ] 4.1 Both PlatformIO envs build (`crowpanel_21_round`, `ref_rect_stub`)
- [ ] 4.2 `openspec validate --all --strict` is clean
- [ ] 4.3 Hardware check: `/status` reports a plausible node/channel on both controllers; a test build with relaxed thresholds exercises the move path
- [ ] 4.4 Add a `CHANGELOG.md` entry under `## [Unreleased]`
