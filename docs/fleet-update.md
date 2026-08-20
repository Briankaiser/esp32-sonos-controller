# Fleet Update Runbook

How to roll a new firmware build out to every OpenSonos controller on the
network, safely and in one command, using `tools/fleet_update.py`.

The updater discovers controllers over the LAN via their `GET /status` endpoint,
lists them, asks for confirmation, then updates each one **sequentially** — after
each device it waits for the controller to reboot back **on the new firmware
version** before moving on, and **aborts the whole run** if any device fails to
return. It never talks to Home Assistant or the Sonos cloud; discovery and
updates are direct HTTP on the local network.

---

## Prerequisites

- **Python 3** on the operator machine. Standard library only — no `pip install`.
- The operator machine is **on the same LAN** as the controllers (able to reach
  their IPs on port 80).
- Each controller is already running firmware that exposes `GET /status` and
  `POST /update` (i.e. a build from this project at or after the fleet-update
  work). A controller on older firmware without `/status` will not be
  discovered — flash it once over USB to bootstrap, then OTA works thereafter.
- The **OTA password** set during captive-portal provisioning.

---

## 1. Build the new firmware

See the **[Firmware Build Runbook](build-firmware.md)** for full details.

```bash
pio run -e crowpanel_21_round
```

The build stamps a version string from `git describe --tags --always --dirty`
and:

- compiles it into the firmware (reported by `/status` as `firmware_version` and
  logged to serial on boot);
- **embeds it in the image** behind a greppable marker, so the `.bin` is
  self-describing;
- writes it to a sidecar `fw_version.txt` next to the `.bin`.

It does the same for the **board identifier** (the PlatformIO env name, e.g.
`crowpanel_21_round`): compiled in (reported by `/status` as `board`, logged on
boot), embedded behind an `OPENSONOS_BOARD:` marker, and written to a
`fw_board.txt` sidecar. This is what lets the updater flash a given image only to
devices of the board it was built for — see [Board targeting](#board-targeting).

The built image and its markers are at:

```
.pio/build/crowpanel_21_round/firmware.bin      # flash this
.pio/build/crowpanel_21_round/fw_version.txt     # the version, for reference
.pio/build/crowpanel_21_round/fw_board.txt       # the target board, for reference
```

> The version must **differ** from what the controllers are currently running,
> or they'll be reported "already up to date" and skipped. Commit your changes
> first so the version is a clean SHA/tag rather than `…-dirty`.

---

## 2. Preview what would update (dry run)

```bash
python3 tools/fleet_update.py .pio/build/crowpanel_21_round/firmware.bin --dry-run
```

This scans the network, lists every controller and its current version, prints
which ones **would** be updated, and exits **without sending any firmware**. Use
it to confirm the fleet is fully discovered and the target version is right.

---

## 3. Run the update

```bash
export OPENSONOS_OTA_PASSWORD='your-ota-password'
python3 tools/fleet_update.py .pio/build/crowpanel_21_round/firmware.bin
```

You'll see the discovered controllers, then a confirmation prompt before anything
is flashed. On confirmation the tool updates each controller in turn and reports
progress. Exit status is `0` on success, non-zero if the run aborted.

### The OTA password

Read from the `OPENSONOS_OTA_PASSWORD` environment variable; if unset, you're
prompted interactively (input hidden). It is **never** passed as a command-line
argument (so it can't leak into shell history or the process list).

---

## Command reference

```
python3 tools/fleet_update.py FIRMWARE [options]
```

| Option | Default | Purpose |
|---|---|---|
| `FIRMWARE` | (required) | Path to the firmware `.bin` to roll out. |
| `--version V` | embedded marker → sidecar | Target version to verify after reboot. Defaults to the version read from the `.bin`'s embedded marker, falling back to the `fw_version.txt` sidecar. Override only for hand-built images. |
| `--subnet CIDR` | auto (interface netmask) | Network to scan, e.g. `192.168.4.0/22`. Auto-detected from the machine's own netmask; a range larger than 4096 hosts is narrowed to a `/24` with a hint to pass this explicitly. |
| `--passes N` | `2` | Discovery scan passes; results are unioned so a controller briefly unresponsive on one pass is still found. |
| `--timeout S` | `120` | Seconds to wait for a device to return on the new version before treating it as failed. |
| `--dry-run` | off | Scan and list what would update, then exit without flashing. |
| `--yes` | off | Skip the interactive confirmation (for unattended/scripted runs). |
| `--force` | off | Disable board matching — update every discovered controller regardless of board. See [Board targeting](#board-targeting). |

Environment: `OPENSONOS_OTA_PASSWORD` — the OTA password (prompted if unset).

---

## Board targeting

Different controller boards (e.g. `crowpanel_21_round` vs `ref_rect_stub`) need
different firmware images — wrong display driver, pin map, and flash layout.
Flashing one board's image onto another can brick it. Because every controller
advertises the same `"device": "opensonos-controller"` identity, the updater
**must not** assume a single image fits every device it finds on the LAN.

Each image and each running device is therefore tagged with a **board
identifier** (the PlatformIO env name). By default the updater:

- reads the **target board** from the `.bin`'s embedded marker (falling back to
  the `fw_board.txt` sidecar), and
- flashes **only** controllers whose `/status` reports the **same** board.

Controllers that don't match are **excluded and reported**, never silently
skipped:

- **board mismatch** — a different board than the image targets;
- **board unknown** — a controller running older firmware that predates board
  reporting (no `board` field). It is *not* flashed, because compatibility can't
  be confirmed. Flash it once with a matching image (over USB, or via `--force`
  if you're certain of the board) to make it board-aware, after which it filters
  correctly.

Overrides and fallbacks:

- **`--force`** disables board matching entirely and makes every discovered
  controller eligible — for the deliberate case where you accept responsibility
  for the pairing.
- If the **image itself carries no board marker** (a legacy build), the updater
  can't know the target board, so it **warns** and falls back to version-only
  selection — never less safe than a board-unaware tool.

---

## What the tool does, step by step

1. **Determine the target version** from the `.bin` (embedded marker → sidecar →
   `--version`), and the **target board** (embedded marker → `fw_board.txt`
   sidecar).
2. **Discover** controllers: scan the subnet (`--passes` times, unioned), keeping
   only hosts whose `/status` reports `"device": "opensonos-controller"`. Each
   controller's `board` is recorded from its `/status`.
3. **List** them, ordered by home room name then IP, with board and current
   firmware. Then **filter by board** (unless `--force`, or the image has no
   board marker): controllers whose board doesn't match — or that report no board
   — are reported as skipped and excluded. Of the rest, controllers already on
   the target version are reported and skipped.
4. **Confirm** — interactively, or via `--yes`. A bare non-interactive run with no
   `--yes` refuses to flash.
5. **Update each pending controller, one at a time:**
   - `POST /update` (multipart) with the `X-OTA-Password` header.
   - The device applies the image and reboots; the connection drop is expected
     and not treated as failure.
   - **Poll `/status` until the device returns reporting the new version**, up to
     `--timeout`.
   - Wait a **15-second settle floor** before the next controller.
6. **Abort on failure:** if a device doesn't come back on the new version within
   the timeout, stop immediately, report which controller failed and how many
   succeeded, and exit non-zero. Remaining controllers are left untouched.

---

## Safety properties

- **Verified completion, not a blind delay.** Each device must actually report
  the new version before the run continues — a device that fails to boot the new
  image is caught, not assumed good.
- **Abort-on-failure.** A bad build can't roll across the whole fleet; the run
  stops at the first device that doesn't return.
- **Sequential only.** Never flashes two controllers at once.
- **Board-matched by default.** An image is flashed only to controllers of the
  board it was built for; mismatched and unknown-board devices are excluded and
  reported, so one board's firmware can't reach another on the same LAN.
  `--force` overrides for deliberate cross-board flashes.
- **Skip already-updated.** Re-running is safe and idempotent — controllers on
  the target version are left alone.
- **No silent misses.** Multi-pass discovery means a controller that's briefly
  busy during one scan is still found.
- **Recoverable.** The device uses dual-slot OTA; a rejected/bad upload leaves it
  on the current firmware. After an abort, fix the issue and re-run — already-
  updated controllers are skipped and the run picks up the rest.

---

## Example session

```
$ python3 tools/fleet_update.py .pio/build/crowpanel_21_round/firmware.bin --yes
Target firmware version: 8d76ecc (1733872 bytes)
Target board: crowpanel_21_round (only matching controllers will be flashed)
Scanning 192.168.4.0/22 (1022 hosts)...
Re-scanning to catch any controller missed on the previous pass...
Scanning 192.168.4.0/22 (1022 hosts)...

Discovered 2 controller(s):

  ROOM            IP            BOARD               FIRMWARE
  --------------  ------------  ------------------  --------
  Brian's Office  192.168.5.25  crowpanel_21_round  37e1d7b
  Kitchen         192.168.5.38  crowpanel_21_round  37e1d7b

→ Updating Brian's Office (192.168.5.25): uploading 1733872 bytes...
    upload: HTTP 200 OK
    waiting for 192.168.5.25 to reboot on 8d76ecc (timeout 120s)...
    ✓ Brian's Office is back on 8d76ecc
    settling 15s before the next controller...
→ Updating Kitchen (192.168.5.38): uploading 1733872 bytes...
    upload: HTTP 200 OK
    waiting for 192.168.5.38 to reboot on 8d76ecc (timeout 120s)...
    ✓ Kitchen is back on 8d76ecc

✓ Done. 2 controller(s) updated to 8d76ecc.
```

---

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| **No controllers found** | Operator machine on a different subnet/VLAN than the controllers, or a large network narrowed to a `/24`. Pass `--subnet <CIDR>` explicitly (e.g. `--subnet 192.168.4.0/22`). |
| **Fewer controllers than expected** | One was briefly offline/rebooting. Re-run (idempotent), or raise `--passes`. |
| **`upload: HTTP 401`** and the device is skipped/aborts | Wrong OTA password. Check `OPENSONOS_OTA_PASSWORD`. |
| **Device doesn't return within the timeout** | Slow WiFi rejoin, or the new image failed to boot. The run aborts (safe). Raise `--timeout`, verify the build, and re-run; the device rolls back to its previous slot if the image was bad. |
| **A controller shows an unexpected IP** | Controllers use DHCP and IPs can change between reboots. Discovery finds them by scan regardless — don't hardcode IPs. |

---

## Testing the tool

The tool's logic has a stdlib unit-test suite (mock controller, no hardware):

```bash
python3 tools/test_fleet_update.py
```

Covers version- and board-marker extraction, target-version/board resolution,
controller identity and board matching, board filtering (mismatch / unknown /
`--force` / legacy-image fallback), multi-pass discovery union,
verified-completion polling, and the per-device update orchestration (accept /
wrong-password reject / timeout abort).

---

## How it relates to the specs

Behavior is defined in OpenSpec:

- `openspec/specs/fleet-update/spec.md` — the host tool's contract (discovery,
  listing, confirmation, authenticated OTA, verified sequential completion,
  abort-on-failure).
- `openspec/specs/web-services/spec.md` — the device `GET /status` endpoint and
  the discoverable firmware version and board identifier this tool depends on.

This runbook is operator documentation; the specs are the source of truth for
behavior.
