## Context

See `proposal.md — Why` for motivation. The portal runs entirely in `portal::run()` on core 1 before any FreeRTOS tasks exist. WiFi is currently in `WIFI_AP` mode. The page is a single static string; the `/save` POST handler is unchanged.

## Goals / Non-Goals

**Goals:** Replace the free-text SSID input with a scan-driven picker inside the existing portal flow, with zero changes to the NVS layout or post-provisioning boot path.

**Non-Goals:** Scan quality improvements (channel dwell time, repeated scans), signal strength history, or WPA Enterprise / hidden-SSID auto-detection.

## Decisions

### D1: Lazy scan on `/scan` request, not eager on AP start

Scan when the browser requests `/scan` rather than at AP startup. Rationale: the phone may take 10–30s to connect to the AP and load the page; a boot-time scan would be stale by then. Downside: page load is blocked ~1–3s while the scan runs. Acceptable — this is a one-time setup flow, not a real-time UI.

### D2: `WIFI_AP_STA` during scan, not a full mode switch

`WiFi.scanNetworks()` requires STA radio capability. The ESP32 supports `WIFI_AP_STA` concurrently — the AP stays up, the STA radio scans passively. Alternative (stop AP, scan, restart AP) is not viable: connected clients would be dropped. The portal loop continues calling `s_dns.processNextRequest()` and `s_server.handleClient()` during the synchronous scan, so the captive-portal redirect remains live.

**Open question for hardware verification:** whether `WIFI_AP_STA` mode causes any instability on the specific ESP32-S3 module during a scan. If it does, the fallback is an async scan (`WiFi.scanNetworks(async=true)`) polled from the portal loop, returning partial results on the first `/scan` call and full results on a second. Tasks are written for the synchronous path; the async variant is a drop-in if needed.

### D3: Deduplication in firmware, not client-side JS

Dedup by SSID (keep highest RSSI) is done in the `/scan` response handler before JSON serialization — the picker never receives duplicates. This keeps the in-page JS minimal and ensures consistent behavior across all browsers.

## Risks / Trade-offs

- [Scan blocks the portal loop for ~1–3s] → Acceptable for a one-time setup flow. DNS and HTTP requests issued during the scan are queued by lwIP and processed immediately after.
- [User's network is not visible at scan time] → Fallback text field in the form. Spec requires this path to work.
- [WIFI_AP_STA instability on specific hardware] → See D2 open question; async-scan fallback is ready if needed.
