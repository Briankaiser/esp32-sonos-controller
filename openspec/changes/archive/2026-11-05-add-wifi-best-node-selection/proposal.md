## Why

On a mesh network every node advertises the same network name. The controller
joins whichever node answers first and then never moves, so a board can sit on a
distant node (the Kitchen controller reads -80 dBm against -62 dBm for its
sibling) even when a much closer node exists. A weak link makes large downloads
queue data in internal RAM, which has produced near-exhaustion and a dropped
network-stack allocation on that board.

## What Changes

- At every join (boot and automatic reconnect) the controller scans all channels
  and joins the **strongest** node of its network, instead of the first match.
- While running, if the link has been weak for a sustained period and the
  controller is idle, it scans for a node that is clearly stronger and re-joins
  through it. Rate-limited so it cannot flap.
- `GET /status` additionally reports the node it is connected to (`wifi_bssid`),
  the `wifi_channel`, and how many times it moved (`wifi_reselects`), so the
  behaviour can be verified and audited. The nightly log records them.
- Scope unchanged: no Home Assistant, no Sonos cloud/OAuth API. 802.11k/v
  steering is out of scope (not compiled into the prebuilt WiFi libraries).

## Capabilities

### New Capabilities

### Modified Capabilities
- `web-services`: WiFi node selection at join time and while idle; new `/status` fields.

## Impact

- `src/main.cpp` (scan/sort settings before the join; start the monitor),
  new `src/services/wifi_roam.{h,cpp}`, `src/services/status.cpp`,
  `scripts/heap_log.py`.
- Briefly interrupts the controller's network (a few seconds) when it moves
  nodes; playback on the speakers is unaffected.
