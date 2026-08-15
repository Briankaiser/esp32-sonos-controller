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
  # target version defaults to the sidecar written by the build
  # (.pio/build/<env>/fw_version.txt); pass --version for a hand-built image.
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


def local_subnets():
    """Best-effort guess of the local /24(s) this host sits on."""
    subnets = []
    try:
        # Doesn't send packets; just picks the interface the OS would route from.
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect(("192.0.2.1", 9))  # TEST-NET-1, unroutable
            local_ip = s.getsockname()[0]
        finally:
            s.close()
        net = ipaddress.ip_network(local_ip + "/24", strict=False)
        subnets.append(net)
    except OSError:
        pass
    return subnets


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
    print("\nDiscovered %d controller(s):\n" % len(controllers))
    print("  %-*s  %-*s  %s" % (name_w, "ROOM", ip_w, "IP", "FIRMWARE"))
    print("  %-*s  %-*s  %s" % (name_w, "-" * name_w, ip_w, "-" * ip_w, "--------"))
    for c in controllers:
        print("  %-*s  %-*s  %s" % (
            name_w, c.get("home_room_name") or "(unnamed)",
            ip_w, c["ip"],
            c.get("firmware_version") or "?"))
    print()


# ── OTA upload ───────────────────────────────────────────────────────────────

def post_update(ip, firmware, password, timeout=60):
    """POST the firmware as multipart/form-data with the X-OTA-Password header.

    Returns (accepted, message). `accepted` True means the device took the image
    and is rebooting. A connection reset/drop after the request is sent is treated
    as accepted-and-rebooting (the device restarts as/after it responds); an HTTP
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

    req = urllib.request.Request(
        "http://%s:%d/update" % (ip, HTTP_PORT),
        data=body,
        method="POST",
        headers={
            "X-OTA-Password": password,
            "Content-Type": "multipart/form-data; boundary=%s" % boundary,
            "Content-Length": str(len(body)),
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return True, "HTTP %d %s" % (resp.status, resp.read().decode(errors="replace").strip())
    except urllib.error.HTTPError as e:
        # The device answered with a status code — a real rejection (401/400).
        return False, "HTTP %d %s" % (e.code, e.read().decode(errors="replace").strip())
    except (urllib.error.URLError, socket.timeout, ConnectionError, OSError) as e:
        # Connection dropped/reset around the response — expected as the device
        # reboots. Treat as accepted; verification below is the real gate.
        return True, "connection dropped during reboot (expected): %s" % e


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

def default_target_version(firmware_path):
    """Read the sidecar fw_version.txt the build wrote next to the .bin."""
    # firmware is typically .pio/build/<env>/firmware.bin; sidecar sits beside it.
    sidecar = os.path.join(os.path.dirname(os.path.abspath(firmware_path)), "fw_version.txt")
    if os.path.isfile(sidecar):
        with open(sidecar) as f:
            v = f.read().strip()
        if v:
            return v
    return None


# ── Main ─────────────────────────────────────────────────────────────────────

def parse_args(argv):
    p = argparse.ArgumentParser(description="Update OpenSonos controllers over the LAN.")
    p.add_argument("firmware", help="path to the firmware .bin to roll out")
    p.add_argument("--subnet", help="subnet/CIDR to scan (default: auto-detect local /24)")
    p.add_argument("--version", help="target firmware version to verify after reboot "
                                     "(default: read from fw_version.txt beside the .bin)")
    p.add_argument("--timeout", type=int, default=DEFAULT_VERIFY_TIMEOUT_S,
                   help="seconds to wait for a device to return on the new version "
                        "(default: %d)" % DEFAULT_VERIFY_TIMEOUT_S)
    p.add_argument("--yes", action="store_true", help="skip the confirmation prompt")
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

    # Determine subnets to scan.
    if args.subnet:
        subnets = [args.subnet]
    else:
        subnets = [str(n) for n in local_subnets()]
        if not subnets:
            print("error: could not auto-detect a local subnet; pass --subnet.", file=sys.stderr)
            return 2

    controllers = []
    for sn in subnets:
        controllers.extend(scan(sn))
    if not controllers:
        print("No OpenSonos controllers found.")
        return 0

    controllers.sort(key=sort_key)
    print_list(controllers)

    # Split already-updated from those needing a flash.
    pending = [c for c in controllers if (c.get("firmware_version") or "") != target_version]
    already = [c for c in controllers if (c.get("firmware_version") or "") == target_version]
    for c in already:
        print("  already up to date: %s (%s)" % (c.get("home_room_name") or "(unnamed)", c["ip"]))
    if not pending:
        print("\nAll discovered controllers are already on %s. Nothing to do." % target_version)
        return 0

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
