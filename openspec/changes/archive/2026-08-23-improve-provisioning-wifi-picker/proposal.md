## Why

The current setup form asks users to type their WiFi network name (SSID) by hand. This is error-prone — one wrong character causes a failed connection with no feedback until after the device reboots — and is especially frustrating on a phone keyboard. The device can scan for visible networks while hosting the captive portal, so there is no reason to require manual entry. Mesh networks (Eero, Google Home, Orbi, etc.) broadcast the same SSID from multiple access points and would appear as duplicates in a raw scan; showing only the strongest-signal entry keeps the list clean.

## What Changes

- The WiFi network name field in the setup form is replaced with a picker showing all SSIDs the device can currently see.
- The picker deduplicates by SSID: when the same network name appears on multiple access points (mesh systems), only the entry with the highest signal strength is shown.
- Each network entry displays its SSID and a signal-strength indicator so the user can distinguish their network from neighbours'.
- The device performs a fresh network scan when the setup page is loaded, so the list reflects current conditions rather than a stale boot-time snapshot.
- A fallback text field remains for hidden networks (networks that do not broadcast an SSID), so the user is never stuck if their network does not appear.

## Capabilities

### New Capabilities

*(none)*

### Modified Capabilities

- `device-provisioning`: The setup form requirement is updated — the WiFi SSID field becomes a picker populated from a live scan, deduplicated by SSID with best signal strength, with a manual fallback for hidden networks.

## Impact

- `src/services/portal.cpp` — the static HTML page gains a `<select>` populated from a `/scan` JSON endpoint; the `/save` handler is unchanged (still receives an `ssid` POST field).
- No changes to `prefs.cpp`, NVS layout, or the post-provisioning boot path.
- Scan runs in AP+STA mode, which the ESP32 radio supports concurrently; scan duration adds a short delay (1–3s) on page load, bounded by the existing portal loop.
