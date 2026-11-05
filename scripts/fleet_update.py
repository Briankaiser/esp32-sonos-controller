#!/usr/bin/env python3
"""OpenSonos fleet firmware updater — stdlib only.

Discovers every OpenSonos controller on the local network via its GET /status
endpoint, lists them, asks for confirmation, then updates each one sequentially
via the authenticated POST /update (OTA) endpoint. After each device it tolerates
the reboot drop-out, polls /status until the device returns *on the new firmware
version*, and holds a settle floor before moving on. If a device does not come
back on the new version within the timeout, the run ABORTS rather than risk
rolling a bad build across the whole fleet.

Discovery marker: a host is treated as a controller only if GET /status returns
JSON whose "device" field equals "opensonos-controller".

Usage:
  # target version is read from a marker embedded in the .bin (self-contained);
  # falls back to the fw_version.txt sidecar beside it, or pass --version.
  python3 tools/fleet_update.py firmware.bin
  python3 tools/fleet_update.py firmware.bin --subnet 192.168.1.0/24
  python3 tools/fleet_update.py firmware.bin --version v1.2.3-4-gabcdef --timeout 180

OTA password:
  Read from the OPENSONOS_OTA_PASSWORD environment variable; if unset, you are
  prompted interactively (input hidden). It is never taken as a CLI argument.
"""

import argparse
import concurrent.futures
import getpass
import http.client
import ipaddress
import json
import os
import socket
import sys
import time
import urllib.error
import urllib.request
import uuid as uuidlib

DEVICE_MARKER = "opensonos-controller"
HTTP_PORT = 80
SCAN_TIMEOUT_S = 1.0        # per-host probe during subnet scan
SETTLE_FLOOR_S = 15         # minimum wait between devices (operator constraint)
VERIFY_POLL_INTERVAL_S = 3  # how often to poll /status while waiting for reboot
DEFAULT_VERIFY_TIMEOUT_S = 120
DISCOVER_PASS_DELAY_S = 15  # wait between discovery passes (covers ESP32 reboot ~10-20s)


# ── Discovery ────────────────────────────────────────────────────────────────

def probe_status(ip, timeout=SCAN_TIMEOUT_S):
    """GET http://<ip>/status. Return the parsed dict for a controller, else None."""
    url = "http://%s:%d/status" % (ip, HTTP_PORT)
    try:
        with urllib.request.urlopen(url, timeout=timeout) as resp:
            body = resp.read()
        data = json.loads(body)
    except (urllib.error.URLError, socket.timeout, ValueError, OSError):
        return None
    if not isinstance(data, dict) or data.get("device") != DEVICE_MARKER:
        return None
    data["ip"] = ip  # trust the address we reached it on, not a self-reported field
    return data


MAX_AUTO_HOSTS = 4096  # don't auto-scan a range larger than this without --subnet


def _local_ip():
    """The source IP the OS would route from (no packets sent)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.0.2.1", 9))  # TEST-NET-1, unroutable
        return s.getsockname()[0]
    finally:
        s.close()


def _prefix_for_ip(local_ip):
    """Best-effort netmask/prefix for local_ip from the OS. Returns an int prefix
    length, or None if it can't be determined. Stdlib only (parses ifconfig / ip)."""
    import re
    import subprocess
    # Linux: `ip -o -f inet addr show` -> "... inet 192.168.4.37/22 ..."
    try:
        out = subprocess.run(["ip", "-o", "-f", "inet", "addr", "show"],
                             capture_output=True, text=True, timeout=5)
        if out.returncode == 0:
            for line in out.stdout.splitlines():
                m = re.search(r"inet %s/(\d+)" % re.escape(local_ip), line)
                if m:
                    return int(m.group(1))
    except (OSError, subprocess.SubprocessError):
        pass
    # macOS/BSD: `ifconfig` -> "inet 192.168.4.37 netmask 0xfffffc00 ..."
    try:
        out = subprocess.run(["ifconfig"], capture_output=True, text=True, timeout=5)
        if out.returncode == 0:
            for line in out.stdout.splitlines():
                m = re.search(r"inet %s .*netmask (0x[0-9a-fA-F]+)" % re.escape(local_ip), line)
                if m:
                    mask = int(m.group(1), 16)
                    return bin(mask).count("1")
    except (OSError, subprocess.SubprocessError):
        pass
    return None


def local_subnets():
    """Best-effort local subnet(s) this host sits on, using the interface's real
    netmask (falling back to /24). Ranges larger than MAX_AUTO_HOSTS are narrowed
    to a /24 with a note to pass --subnet, so auto-scan never balloons."""
    try:
        local_ip = _local_ip()
    except OSError:
        return []
    prefix = _prefix_for_ip(local_ip)
    if prefix is None:
        prefix = 24  # couldn't read the mask; assume the common case
    net = ipaddress.ip_network("%s/%d" % (local_ip, prefix), strict=False)
    if net.num_addresses - 2 > MAX_AUTO_HOSTS:
        narrowed = ipaddress.ip_network("%s/24" % local_ip, strict=False)
        print("Note: local network %s is large (%d hosts); scanning %s only.\n"
              "      Pass --subnet %s to scan the whole range."
              % (net, net.num_addresses - 2, narrowed, net))
        return [narrowed]
    return [net]


def scan(subnet, workers=64):
    """Concurrently probe every host in the subnet. Return controllers found."""
    net = ipaddress.ip_network(subnet, strict=False)
    hosts = [str(h) for h in net.hosts()]
    found = []
    print("Scanning %s (%d hosts)..." % (net, len(hosts)))
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        for result in pool.map(probe_status, hosts):
            if result:
                found.append(result)
    return found


def discover(subnets, passes=2):
    """Scan all subnets `passes` times and union the results by IP, keeping the
    freshest record per controller. A single scan is a point-in-time snapshot: a
    controller that is briefly unresponsive (rebooting, busy, mid-DHCP) during
    that one pass would be silently missed — fatal for a tool whose job is to
    update *all* controllers. Unioning across passes closes that gap."""
    by_ip = {}
    for p in range(max(1, passes)):
        if p:
            print("Re-scanning to catch any controller missed on the previous pass...")
            time.sleep(DISCOVER_PASS_DELAY_S)
        for sn in subnets:
            for c in scan(sn):
                by_ip[c["ip"]] = c  # last seen wins (freshest firmware_version)
    return list(by_ip.values())


# ── Presentation ─────────────────────────────────────────────────────────────

def sort_key(ctrl):
    name = (ctrl.get("home_room_name") or "").lower()
    try:
        ip_key = ipaddress.ip_address(ctrl["ip"])
    except ValueError:
        ip_key = ctrl["ip"]
    return (name == "", name, ip_key)  # unnamed sinks to the bottom


def print_list(controllers):
    name_w = max([len(c.get("home_room_name") or "(unnamed)") for c in controllers] + [9])
    ip_w = max([len(c["ip"]) for c in controllers] + [2])
    board_w = max([len(c.get("board") or "?") for c in controllers] + [5])
    print("\nDiscovered %d controller(s):\n" % len(controllers))
    print("  %-*s  %-*s  %-*s  %s" % (name_w, "ROOM", ip_w, "IP", board_w, "BOARD", "FIRMWARE"))
    print("  %-*s  %-*s  %-*s  %s" % (name_w, "-" * name_w, ip_w, "-" * ip_w,
                                      board_w, "-" * board_w, "--------"))
    for c in controllers:
        print("  %-*s  %-*s  %-*s  %s" % (
            name_w, c.get("home_room_name") or "(unnamed)",
            ip_w, c["ip"],
            board_w, c.get("board") or "?",
            c.get("firmware_version") or "?"))
    print()


# ── OTA upload ───────────────────────────────────────────────────────────────

UPLOAD_TIMEOUT_S = 300  # a slow WiFi link can need minutes to push ~1.7 MB


def post_update(ip, firmware, password, timeout=UPLOAD_TIMEOUT_S):
    """POST the firmware as multipart/form-data with the X-OTA-Password header.

    Returns (accepted, message). `accepted` True means the device took the whole
    image and is rebooting; the caller's version check is still the real gate.

    Only a connection that drops *after* the image was fully sent counts as
    accepted-and-rebooting (the device restarts as/after it responds). Failing
    to connect at all (refused, no route, unreachable) or timing out means the
    upload did not demonstrably happen, so it is reported as a failure; an HTTP
    401/400 is a hard rejection.
    """
    boundary = "----opensonos%s" % uuidlib.uuid4().hex
    pre = (
        "--%s\r\n"
        'Content-Disposition: form-data; name="firmware"; filename="firmware.bin"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n" % boundary
    ).encode()
    post = ("\r\n--%s--\r\n" % boundary).encode()
    body = pre + firmware + post
    headers = {
        "X-OTA-Password": password,
        "Content-Type": "multipart/form-data; boundary=%s" % boundary,
        "Content-Length": str(len(body)),
    }

    conn = http.client.HTTPConnection(ip, HTTP_PORT, timeout=timeout)
    try:
        # Phase 1: reach the device. Any failure here means nothing was uploaded.
        try:
            conn.connect()
        except (OSError, socket.timeout) as e:
            return False, "could not connect to %s: %s" % (ip, e)
        # Phase 2: send the image and read the reply.
        try:
            conn.request("POST", "/update", body=body, headers=headers)
            resp = conn.getresponse()
            text = resp.read().decode(errors="replace").strip()
        except socket.timeout:
            return False, "timed out after %ds waiting for the upload to finish" % timeout
        except (ConnectionError, http.client.HTTPException, OSError) as e:
            # Dropped/reset around the response — expected as the device
            # reboots. Treat as accepted; verification below is the real gate.
            return True, "connection dropped during reboot (expected): %s" % e
        if 200 <= resp.status < 300:
            return True, "HTTP %d %s" % (resp.status, text)
        # The device answered with a status code — a real rejection (401/400).
        return False, "HTTP %d %s" % (resp.status, text)
    finally:
        conn.close()


# ── Verified completion ──────────────────────────────────────────────────────

def wait_for_version(ip, target_version, timeout):
    """Poll /status until the device reports target_version. Return True on success."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        info = probe_status(ip, timeout=SCAN_TIMEOUT_S)
        if info is not None:
            got = info.get("firmware_version") or ""
            if got == target_version:
                return True
        time.sleep(VERIFY_POLL_INTERVAL_S)
    return False


def update_one(ctrl, firmware, target_version, password, verify_timeout):
    """Update a single controller and verify it returns on target_version."""
    ip = ctrl["ip"]
    room = ctrl.get("home_room_name") or "(unnamed)"
    print("→ Updating %s (%s): uploading %d bytes..." % (room, ip, len(firmware)))
    accepted, msg = post_update(ip, firmware, password)
    print("    upload: %s" % msg)
    if not accepted:
        return False, "device rejected the upload"
    print("    waiting for %s to reboot on %s (timeout %ds)..." % (ip, target_version, verify_timeout))
    if not wait_for_version(ip, target_version, verify_timeout):
        return False, "did not report version %s within %ds" % (target_version, verify_timeout)
    print("    ✓ %s is back on %s" % (room, target_version))
    return True, "ok"


# ── Target version ───────────────────────────────────────────────────────────

FW_MARKER = b"OPENSONOS_FW_VERSION:"  # sentinel the firmware embeds in .rodata


def version_from_bin(firmware_path):
    """Read the version embedded in the image behind the greppable marker, so a
    firmware.bin is self-describing (no companion file needed)."""
    try:
        with open(firmware_path, "rb") as f:
            data = f.read()
    except OSError:
        return None
    i = data.find(FW_MARKER)
    if i < 0:
        return None
    start = i + len(FW_MARKER)
    end = data.find(b"\x00", start)
    if end < 0:
        return None
    v = data[start:end].decode("ascii", "replace").strip()
    return v or None


def default_target_version(firmware_path):
    """Target version: prefer the marker embedded in the .bin (self-contained),
    then fall back to the fw_version.txt sidecar the build writes beside it."""
    v = version_from_bin(firmware_path)
    if v:
        return v
    # firmware is typically .pio/build/<env>/firmware.bin; sidecar sits beside it.
    sidecar = os.path.join(os.path.dirname(os.path.abspath(firmware_path)), "fw_version.txt")
    if os.path.isfile(sidecar):
        with open(sidecar) as f:
            v = f.read().strip()
        if v:
            return v
    return None


BOARD_MARKER = b"OPENSONOS_BOARD:"  # sentinel the firmware embeds in .rodata


def board_from_bin(firmware_path):
    """Read the board identifier embedded in the image behind the greppable
    marker, so a firmware.bin declares which board it targets. Returns None for a
    legacy image built before board embedding."""
    try:
        with open(firmware_path, "rb") as f:
            data = f.read()
    except OSError:
        return None
    i = data.find(BOARD_MARKER)
    if i < 0:
        return None
    start = i + len(BOARD_MARKER)
    end = data.find(b"\x00", start)
    if end < 0:
        return None
    b = data[start:end].decode("ascii", "replace").strip()
    return b or None


def default_target_board(firmware_path):
    """Target board: prefer the marker embedded in the .bin (self-contained),
    then fall back to the fw_board.txt sidecar the build writes beside it. Returns
    None when neither is present (a legacy image with no board identity)."""
    b = board_from_bin(firmware_path)
    if b:
        return b
    sidecar = os.path.join(os.path.dirname(os.path.abspath(firmware_path)), "fw_board.txt")
    if os.path.isfile(sidecar):
        with open(sidecar) as f:
            b = f.read().strip()
        if b:
            return b
    return None


# ── Main ─────────────────────────────────────────────────────────────────────

def parse_args(argv):
    p = argparse.ArgumentParser(description="Update OpenSonos controllers over the LAN.")
    p.add_argument("firmware", help="path to the firmware .bin to roll out")
    p.add_argument("--subnet", help="subnet/CIDR to scan (default: auto-detect from the "
                                    "interface netmask)")
    p.add_argument("--version", help="target firmware version to verify after reboot "
                                     "(default: read from the marker embedded in the .bin, "
                                     "else the fw_version.txt sidecar beside it)")
    p.add_argument("--timeout", type=int, default=DEFAULT_VERIFY_TIMEOUT_S,
                   help="seconds to wait for a device to return on the new version "
                        "(default: %d)" % DEFAULT_VERIFY_TIMEOUT_S)
    p.add_argument("--dry-run", action="store_true",
                   help="scan and list which controllers would be updated, then exit "
                        "without sending any firmware")
    p.add_argument("--passes", type=int, default=2,
                   help="number of discovery scan passes to union (default 2); more "
                        "passes catch controllers that are briefly unresponsive")
    p.add_argument("--yes", action="store_true", help="skip the confirmation prompt")
    p.add_argument("--force", action="store_true",
                   help="disable board matching and update every discovered controller "
                        "regardless of board (default: flash only controllers whose board "
                        "matches the firmware image's target board)")
    return p.parse_args(argv)


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)

    if not os.path.isfile(args.firmware):
        print("error: firmware file not found: %s" % args.firmware, file=sys.stderr)
        return 2
    with open(args.firmware, "rb") as f:
        firmware = f.read()

    target_version = args.version or default_target_version(args.firmware)
    if not target_version:
        print("error: could not determine target version. Pass --version, or run from a\n"
              "       build tree where fw_version.txt sits next to the .bin.", file=sys.stderr)
        return 2
    print("Target firmware version: %s (%d bytes)" % (target_version, len(firmware)))

    # Determine the image's target board. board_matching is disabled when the
    # operator forces it, or when the image carries no board identity (a legacy
    # build) — in which case we warn and fall back to version-only selection, so
    # the tool is never less safe than a board-unaware version.
    target_board = default_target_board(args.firmware)
    board_matching = bool(target_board) and not args.force
    if args.force:
        print("Board matching: DISABLED (--force) — every discovered controller is eligible.")
    elif target_board:
        print("Target board: %s (only matching controllers will be flashed)" % target_board)
    else:
        print("WARNING: firmware image carries no board identifier; board matching is\n"
              "         unavailable for this image. Falling back to version-only selection.")

    # Determine subnets to scan.
    if args.subnet:
        subnets = [args.subnet]
    else:
        subnets = [str(n) for n in local_subnets()]
        if not subnets:
            print("error: could not auto-detect a local subnet; pass --subnet.", file=sys.stderr)
            return 2

    controllers = discover(subnets, args.passes)
    if not controllers:
        print("No OpenSonos controllers found.")
        return 0

    controllers.sort(key=sort_key)
    print_list(controllers)

    # Board filter (before the version split): exclude controllers the image was
    # not built for. A mismatched board is a bricking risk; an unknown board
    # (legacy firmware) can't be confirmed compatible, so it is also excluded.
    # Excluded controllers are always reported with a reason — never dropped
    # silently. --force / legacy-image fallback disable this via board_matching.
    if board_matching:
        eligible = [c for c in controllers if (c.get("board") or "") == target_board]
        skipped_board = [c for c in controllers
                         if c.get("board") and c.get("board") != target_board]
        skipped_unknown = [c for c in controllers if not c.get("board")]
        for c in skipped_board:
            print("  skipped (board %s != %s): %s (%s)"
                  % (c.get("board"), target_board,
                     c.get("home_room_name") or "(unnamed)", c["ip"]))
        for c in skipped_unknown:
            print("  skipped (board unknown; needs board-aware firmware): %s (%s)"
                  % (c.get("home_room_name") or "(unnamed)", c["ip"]))
        if not eligible:
            print("\nNo discovered controllers match target board %s. Nothing to do."
                  % target_board)
            return 0
    else:
        eligible = controllers

    # Split already-updated from those needing a flash.
    pending = [c for c in eligible if (c.get("firmware_version") or "") != target_version]
    already = [c for c in eligible if (c.get("firmware_version") or "") == target_version]
    for c in already:
        print("  already up to date: %s (%s)" % (c.get("home_room_name") or "(unnamed)", c["ip"]))
    if not pending:
        print("\nAll discovered controllers are already on %s. Nothing to do." % target_version)
        return 0

    if args.dry_run:
        print("\nWould update %d controller(s) to %s:" % (len(pending), target_version))
        for c in pending:
            print("  %s (%s): %s -> %s"
                  % (c.get("home_room_name") or "(unnamed)", c["ip"],
                     c.get("firmware_version") or "?", target_version))
        print("Dry run — no firmware sent.")
        return 1  # pending updates exist; lets scripts detect "would do work"

    # Confirmation gate.
    if not args.yes:
        try:
            answer = input("\nUpdate the %d controller(s) above to %s? [y/N] "
                           % (len(pending), target_version)).strip().lower()
        except EOFError:
            answer = ""
        if answer not in ("y", "yes"):
            print("Aborted; no firmware sent.")
            return 1

    # OTA password: env var, else hidden prompt.
    password = os.environ.get("OPENSONOS_OTA_PASSWORD")
    if password is None:
        password = getpass.getpass("OTA password: ")

    # Sequential update with per-device verification and abort-on-failure.
    updated = 0
    for i, ctrl in enumerate(pending):
        ok, why = update_one(ctrl, firmware, target_version, password, args.timeout)
        if not ok:
            room = ctrl.get("home_room_name") or "(unnamed)"
            print("\n✗ ABORTING: %s (%s) %s." % (room, ctrl["ip"], why), file=sys.stderr)
            print("  %d of %d controller(s) updated successfully before the failure."
                  % (updated, len(pending)), file=sys.stderr)
            print("  Remaining controllers were NOT touched.", file=sys.stderr)
            return 1
        updated += 1
        if i < len(pending) - 1:
            print("    settling %ds before the next controller..." % SETTLE_FLOOR_S)
            time.sleep(SETTLE_FLOOR_S)

    print("\n✓ Done. %d controller(s) updated to %s." % (updated, target_version))
    return 0


if __name__ == "__main__":
    sys.exit(main())
