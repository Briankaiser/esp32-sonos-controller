#!/usr/bin/env python3
"""Tests for fleet_update.py — stdlib unittest, no hardware, no pip deps.

Run:  python3 tools/test_fleet_update.py      (or: python3 -m unittest -v tools.test_fleet_update)

Covers the pure logic (version marker extraction, target-version resolution,
controller identity match, verified-completion polling) and the per-device
update orchestration (upload accept/reject, verify-new-version, abort-on-timeout)
against a mock controller HTTP server. Does not touch a real device or network.
"""

import json
import os
import sys
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fleet_update as fu

MARKER = b"OPENSONOS_FW_VERSION:"


def make_bin(version, size=64):
    """A fake firmware image carrying the embedded version marker in its bytes."""
    blob = bytearray(b"\x00" * size)
    tag = MARKER + version.encode() + b"\x00"
    blob[8:8 + len(tag)] = tag
    return bytes(blob)


class MockController:
    """A stand-in Sonos controller: serves GET /status and POST /update.

    Reports `version`; flips to `flip_to` after a valid authenticated upload
    (None = never flips, to exercise the verification timeout / abort path)."""

    def __init__(self, room="Kitchen", version="old", flip_to="new", password="pw",
                 device="opensonos-controller"):
        self.room, self.version, self.flip_to = room, version, flip_to
        self.password, self.device = password, device
        self.uploads = 0

    def start(self):
        ctrl = self

        class H(BaseHTTPRequestHandler):
            def _send(self, code, body=b"", ctype="application/json"):
                self.send_response(code)
                self.send_header("Content-Type", ctype)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                if body:
                    self.wfile.write(body)

            def do_GET(self):
                if self.path == "/status":
                    self._send(200, json.dumps({
                        "device": ctrl.device,
                        "firmware_version": ctrl.version,
                        "home_room_name": ctrl.room,
                    }).encode())
                else:
                    self._send(404)

            def do_POST(self):
                if self.path != "/update":
                    self._send(404); return
                n = int(self.headers.get("Content-Length", 0))
                self.rfile.read(n)
                if self.headers.get("X-OTA-Password") != ctrl.password:
                    self._send(401, b"Unauthorized", "text/plain"); return
                ctrl.uploads += 1
                if ctrl.flip_to is not None:
                    ctrl.version = ctrl.flip_to
                self._send(200, b"OK", "text/plain")

            def log_message(self, *a):
                pass

        self.srv = ThreadingHTTPServer(("127.0.0.1", 0), H)
        self.port = self.srv.server_address[1]
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()
        return self

    def stop(self):
        self.srv.shutdown()


class FleetUpdateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Speed the timing constants up so tests run fast.
        fu.SETTLE_FLOOR_S = 0
        fu.VERIFY_POLL_INTERVAL_S = 0.02
        fu.SCAN_TIMEOUT_S = 1.0

    def setUp(self):
        self.ctrl = MockController().start()
        self.addCleanup(self.ctrl.stop)
        fu.HTTP_PORT = self.ctrl.port

    # ── version marker / target resolution ──────────────────────────────

    def test_version_from_bin_extracts_marker(self):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(make_bin("v1.2.3-4-gabcdef-dirty"))
            path = f.name
        self.addCleanup(os.unlink, path)
        self.assertEqual(fu.version_from_bin(path), "v1.2.3-4-gabcdef-dirty")

    def test_version_from_bin_absent_marker(self):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"\x00" * 64)
            path = f.name
        self.addCleanup(os.unlink, path)
        self.assertIsNone(fu.version_from_bin(path))

    def test_default_target_prefers_marker_over_sidecar(self):
        d = tempfile.mkdtemp()
        binp = os.path.join(d, "firmware.bin")
        with open(binp, "wb") as f:
            f.write(make_bin("from-marker"))
        with open(os.path.join(d, "fw_version.txt"), "w") as f:
            f.write("from-sidecar\n")
        self.assertEqual(fu.default_target_version(binp), "from-marker")

    def test_default_target_falls_back_to_sidecar(self):
        d = tempfile.mkdtemp()
        binp = os.path.join(d, "firmware.bin")
        with open(binp, "wb") as f:
            f.write(b"\x00" * 64)  # no marker
        with open(os.path.join(d, "fw_version.txt"), "w") as f:
            f.write("from-sidecar\n")
        self.assertEqual(fu.default_target_version(binp), "from-sidecar")

    # ── discovery / identity marker ─────────────────────────────────────

    def test_probe_matches_controller(self):
        info = fu.probe_status("127.0.0.1", timeout=2)
        self.assertIsNotNone(info)
        self.assertEqual(info["ip"], "127.0.0.1")  # reached address, not self-reported
        self.assertEqual(info["home_room_name"], "Kitchen")

    def test_probe_ignores_non_controller(self):
        self.ctrl.device = "some-other-device"
        self.assertIsNone(fu.probe_status("127.0.0.1", timeout=2))

    # ── verified completion ─────────────────────────────────────────────

    def test_wait_for_version_success_and_timeout(self):
        self.ctrl.version = "target"
        self.assertTrue(fu.wait_for_version("127.0.0.1", "target", timeout=2))
        self.assertFalse(fu.wait_for_version("127.0.0.1", "nope", timeout=0.5))

    # ── per-device update orchestration ─────────────────────────────────

    def test_update_one_happy_path(self):
        ctrl_info = {"ip": "127.0.0.1", "home_room_name": "Kitchen"}
        ok, why = fu.update_one(ctrl_info, make_bin("new"), "new", "pw", 3)
        self.assertTrue(ok, why)
        self.assertEqual(self.ctrl.uploads, 1)
        self.assertEqual(self.ctrl.version, "new")

    def test_update_one_wrong_password_rejected(self):
        ctrl_info = {"ip": "127.0.0.1", "home_room_name": "Kitchen"}
        ok, why = fu.update_one(ctrl_info, make_bin("new"), "new", "WRONG", 3)
        self.assertFalse(ok)
        self.assertIn("reject", why.lower())
        self.assertEqual(self.ctrl.uploads, 0)

    def test_update_one_timeout_when_not_returning(self):
        self.ctrl.flip_to = None  # never comes back on the new version
        ctrl_info = {"ip": "127.0.0.1", "home_room_name": "Loft"}
        ok, why = fu.update_one(ctrl_info, make_bin("new"), "new", "pw", 1)
        self.assertFalse(ok)
        self.assertIn("did not report", why)


if __name__ == "__main__":
    unittest.main(verbosity=2)
