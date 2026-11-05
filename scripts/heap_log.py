#!/usr/bin/env python3
"""Nightly heap logger + leak analyzer for OpenSonos controllers.

Discovers controllers via fleet_update's scanner, appends one JSON line per
controller to its own file ~/.opensonos-heaplog/heap-<device_id>.jsonl, then analyzes the history and
writes ~/.opensonos-heaplog/report.txt. A macOS notification fires if a leak
or fragmentation trend is flagged. Stdlib only.

Usage: heap_log.py [--analyze-only] [--until YYYY-MM-DD]
"""
import argparse
import datetime
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fleet_update  # noqa: E402

LOG_DIR = os.path.expanduser("~/.opensonos-heaplog")
LOG_PREFIX = "heap-"   # one file per device: heap-<device_id>.jsonl
REPORT_FILE = os.path.join(LOG_DIR, "report.txt")
FIELDS = ["heap_internal_free", "heap_internal_min_free", "heap_internal_largest",
          "psram_free", "psram_largest"]

SLOPE_FLAG_BYTES_PER_DAY = -1024      # sustained loss faster than 1 KB/day
LOW_INTERNAL_FREE = 40 * 1024
LOW_LARGEST_BLOCK = 20 * 1024
EXTRA_FIELDS = ["dip_count", "alloc_fail_count", "alloc_fail_last", "heap_dips",
                "tls_psram_allocs", "tls_fallback_allocs", "wifi_rssi",
                "wifi_bssid", "wifi_channel", "wifi_reselects"]
MIN_FREE_FLOOR = 4 * 1024             # low-water mark below this = near-OOM at some point


def collect():
    now = datetime.datetime.now().astimezone().isoformat(timespec="seconds")
    controllers = fleet_update.discover(fleet_update.local_subnets(), passes=2)
    os.makedirs(LOG_DIR, exist_ok=True)
    for c in controllers:
        dev = c.get("device_id")
        if dev:
            rec = {"ts": now, "device_id": dev, "room": c.get("home_room_name") or "",
                   "ip": c["ip"], "fw": c.get("firmware_version"), "uptime_s": c.get("uptime_s")}
            for k in FIELDS + EXTRA_FIELDS:
                rec[k] = c.get(k)
            with open(os.path.join(LOG_DIR, "%s%s.jsonl" % (LOG_PREFIX, dev)), "a") as f:
                f.write(json.dumps(rec) + "\n")
    return len(controllers)


def load():
    """{device_id: [records]} from the per-device log files."""
    by_dev = {}
    if not os.path.isdir(LOG_DIR):
        return by_dev
    for name in sorted(os.listdir(LOG_DIR)):
        if not (name.startswith(LOG_PREFIX) and name.endswith(".jsonl")):
            continue
        dev = name[len(LOG_PREFIX):-len(".jsonl")]
        with open(os.path.join(LOG_DIR, name)) as f:
            for line in f:
                try:
                    by_dev.setdefault(dev, []).append(json.loads(line))
                except ValueError:
                    continue
    return by_dev


def slope_per_day(points):
    """Least-squares slope (units/day) over [(t_days, value)]."""
    n = len(points)
    if n < 3:
        return None
    mt = sum(t for t, _ in points) / n
    mv = sum(v for _, v in points) / n
    den = sum((t - mt) ** 2 for t, _ in points)
    return None if den == 0 else sum((t - mt) * (v - mv) for t, v in points) / den


def analyze():
    lines, flags = [], []
    by_dev = load()
    if not by_dev:
        return "No data yet.\n", []
    for dev, recs in sorted(by_dev.items()):
        recs = [r for r in recs if r.get("heap_internal_free") is not None]
        room = "%s [%s]" % (recs[-1]["room"] or "?", dev) if recs else dev
        lines.append("== %s ==  (%d samples)" % (room, len(recs)))
        if not recs:
            lines.append("  no heap data (firmware predates heap stats on /status)")
            continue
        t0 = datetime.datetime.fromisoformat(recs[0]["ts"])
        last = recs[-1]
        lines.append("  last: %s  fw=%s  uptime=%.1fd" % (
            last["ts"], last["fw"], (last.get("uptime_s") or 0) / 86400))
        reboots = sum(1 for a, b in zip(recs, recs[1:])
                      if (b.get("uptime_s") or 0) < (a.get("uptime_s") or 0))
        lines.append("  reboots detected: %d" % reboots)
        # Analyze only the current boot (since the most recent uptime reset).
        start = 0
        for i in range(1, len(recs)):
            if (recs[i].get("uptime_s") or 0) < (recs[i - 1].get("uptime_s") or 0):
                start = i
        cur = recs[start:]
        for k in FIELDS:
            pts = [((datetime.datetime.fromisoformat(r["ts"]) - t0).total_seconds() / 86400, r[k])
                   for r in cur if r.get(k) is not None]
            s = slope_per_day(pts)
            first_v, last_v = (pts[0][1], pts[-1][1]) if pts else (None, None)
            lines.append("  %-24s now=%-9s first=%-9s slope/day=%s" % (
                k, last_v, first_v, "n/a" if s is None else "%+.0f" % s))
            if s is not None and s < SLOPE_FLAG_BYTES_PER_DAY and k != "heap_internal_min_free":
                flags.append("%s: %s falling %.1f KB/day" % (room, k, -s / 1024))
        rssi = [r["wifi_rssi"] for r in recs if r.get("wifi_rssi") is not None]
        if rssi:
            lines.append("  wifi_rssi (dBm)          now=%s  min=%s  avg=%.0f  (%d samples)" % (
                rssi[-1], min(rssi), sum(rssi) / len(rssi), len(rssi)))
        if last.get("wifi_bssid"):
            lines.append("  wifi node: %s ch%s  reselects=%s" % (
                last["wifi_bssid"], last.get("wifi_channel"), last.get("wifi_reselects")))
        dips = last.get("heap_dips") or []
        lines.append("  dips since boot: %s   alloc failures: %s" % (
            last.get("dip_count", "n/a"), last.get("alloc_fail_count", "n/a")))
        for d in dips[-8:]:
            lines.append("    up=%.2fd floor=%dB largest=%dB dur=%dms new_low=%s active=%s" % (
                d["up_s"] / 86400, d["floor"], d["largest"], d["dur_ms"], d["new_low"],
                "+".join(d["active"]) or "none"))
        if (last.get("alloc_fail_count") or 0) > 0:
            flags.append("%s: %d failed heap allocation(s), last %s" % (
                room, last["alloc_fail_count"], last.get("alloc_fail_last")))
        if last["heap_internal_min_free"] < MIN_FREE_FLOOR:
            flags.append("%s: internal heap low-water mark only %d bytes since boot (near-OOM)"
                         % (room, last["heap_internal_min_free"]))
        if len(cur) >= 2:
            d = cur[-2]["heap_internal_min_free"] - cur[-1]["heap_internal_min_free"]
            if d >= 4096:
                flags.append("%s: low-water mark dropped %d bytes since the previous sample (new low)"
                             % (room, d))
        if last["heap_internal_free"] < LOW_INTERNAL_FREE:
            flags.append("%s: internal heap low (%d KB)" % (room, last["heap_internal_free"] // 1024))
        if last["heap_internal_largest"] < LOW_LARGEST_BLOCK:
            flags.append("%s: largest internal block small (%d KB) - fragmentation"
                         % (room, last["heap_internal_largest"] // 1024))
    lines.append("")
    lines.append("FLAGS: " + ("; ".join(flags) if flags else "none"))
    return "\n".join(lines) + "\n", flags


def notify(title, msg):
    try:
        subprocess.run(["osascript", "-e",
                        'display notification "%s" with title "%s"' % (msg.replace('"', "'"), title)],
                       timeout=10)
    except (OSError, subprocess.SubprocessError):
        pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--analyze-only", action="store_true")
    ap.add_argument("--until", default="2026-11-15", help="stop collecting after this date")
    args = ap.parse_args()

    if datetime.date.today() > datetime.date.fromisoformat(args.until):
        print("Past --until date %s; nothing to do." % args.until)
        return 0
    if not args.analyze_only:
        print("collected %d controller(s)" % collect())
    report, flags = analyze()
    os.makedirs(LOG_DIR, exist_ok=True)
    with open(REPORT_FILE, "w") as f:
        f.write(report)
    print(report)
    if flags:
        notify("OpenSonos heap warning", "; ".join(flags)[:200])
    return 0


if __name__ == "__main__":
    sys.exit(main())
