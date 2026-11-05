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
BOARD_MARKER = b"OPENSONOS_BOARD:"


def make_bin(version, size=128, board=None):
    """A fake firmware image carrying the embedded version marker in its bytes,
    and optionally the board marker (parallel to how the real image embeds both)."""
    blob = bytearray(b"\x00" * size)
    tag = MARKER + version.encode() + b"\x00"
    blob[8:8 + len(tag)] = tag
    if board is not None:
        btag = BOARD_MARKER + board.encode() + b"\x00"
        off = 8 + len(tag) + 4
        blob[off:off + len(btag)] = btag
    return bytes(blob)


class MockController:
    """A stand-in Sonos controller: serves GET /status and POST /update.

    Reports `version`; flips to `flip_to` after a valid authenticated upload
    (None = never flips, to exercise the verification timeout / abort path)."""

    def __init__(self, room="Kitchen", version="old", flip_to="new", password="pw",
                 device="opensonos-controller", board="crowpanel_21_round"):
        self.room, self.version, self.flip_to = room, version, flip_to
        self.password, self.device = password, device
        self.board = board
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
                    status = {
                        "device": ctrl.device,
                        "firmware_version": ctrl.version,
                        "home_room_name": ctrl.room,
                    }
                    if ctrl.board is not None:
                        status["board"] = ctrl.board
                    self._send(200, json.dumps(status).encode())
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
        fu.DISCOVER_PASS_DELAY_S = 0

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

    # ── board marker / target resolution ────────────────────────────────

    def test_board_from_bin_extracts_marker(self):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(make_bin("v1", board="crowpanel_21_round"))
            path = f.name
        self.addCleanup(os.unlink, path)
        self.assertEqual(fu.board_from_bin(path), "crowpanel_21_round")

    def test_board_from_bin_absent_marker(self):
        # A legacy image with a version marker but no board marker → None.
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(make_bin("v1"))  # no board
            path = f.name
        self.addCleanup(os.unlink, path)
        self.assertIsNone(fu.board_from_bin(path))

    def test_default_target_board_prefers_marker_over_sidecar(self):
        d = tempfile.mkdtemp()
        binp = os.path.join(d, "firmware.bin")
        with open(binp, "wb") as f:
            f.write(make_bin("v1", board="marker_board"))
        with open(os.path.join(d, "fw_board.txt"), "w") as f:
            f.write("sidecar_board\n")
        self.assertEqual(fu.default_target_board(binp), "marker_board")

    def test_default_target_board_falls_back_to_sidecar(self):
        d = tempfile.mkdtemp()
        binp = os.path.join(d, "firmware.bin")
        with open(binp, "wb") as f:
            f.write(make_bin("v1"))  # no board marker
        with open(os.path.join(d, "fw_board.txt"), "w") as f:
            f.write("sidecar_board\n")
        self.assertEqual(fu.default_target_board(binp), "sidecar_board")

    # ── discovery / identity marker ─────────────────────────────────────

    def test_probe_matches_controller(self):
        info = fu.probe_status("127.0.0.1", timeout=2)
        self.assertIsNotNone(info)
        self.assertEqual(info["ip"], "127.0.0.1")  # reached address, not self-reported
        self.assertEqual(info["home_room_name"], "Kitchen")

    def test_probe_records_board(self):
        self.ctrl.board = "crowpanel_21_round"
        info = fu.probe_status("127.0.0.1", timeout=2)
        self.assertEqual(info["board"], "crowpanel_21_round")

    def test_probe_ignores_non_controller(self):
        self.ctrl.device = "some-other-device"
        self.assertIsNone(fu.probe_status("127.0.0.1", timeout=2))

    def test_discover_unions_across_passes(self):
        # A controller seen in only one pass (briefly unresponsive in the other)
        # must still be discovered — the union is what makes "update all" reliable.
        calls = {"n": 0}
        A = {"ip": "10.0.0.1", "home_room_name": "A", "firmware_version": "x"}
        B = {"ip": "10.0.0.2", "home_room_name": "B", "firmware_version": "y"}

        def fake_scan(subnet, workers=64):
            calls["n"] += 1
            return [A] if calls["n"] == 1 else [B]  # pass 1 sees only A, pass 2 only B

        orig = fu.scan
        fu.scan = fake_scan
        try:
            result = fu.discover(["10.0.0.0/24"], passes=2)
        finally:
            fu.scan = orig
        self.assertEqual({c["ip"] for c in result}, {"10.0.0.1", "10.0.0.2"})

    # ── dry-run ─────────────────────────────────────────────────────────

    def _make_firmware(self, version, board=None):
        f = tempfile.NamedTemporaryFile(suffix=".bin", delete=False)
        f.write(make_bin(version, board=board))
        f.close()
        self.addCleanup(os.unlink, f.name)
        return f.name

    def test_dry_run_pending_exits_1_no_upload(self):
        binp = self._make_firmware("new")
        ctrl_info = {"ip": "127.0.0.1", "home_room_name": "Kitchen", "firmware_version": "old"}
        orig = fu.discover
        fu.discover = lambda subnets, passes=2: [ctrl_info]
        try:
            rc = fu.main(["--subnet", "127.0.0.1/32", "--yes", "--dry-run", binp])
        finally:
            fu.discover = orig
        self.assertEqual(rc, 1)
        self.assertEqual(self.ctrl.uploads, 0)

    def test_dry_run_already_current_exits_0(self):
        binp = self._make_firmware("new")
        ctrl_info = {"ip": "127.0.0.1", "home_room_name": "Kitchen", "firmware_version": "new"}
        orig = fu.discover
        fu.discover = lambda subnets, passes=2: [ctrl_info]
        try:
            rc = fu.main(["--subnet", "127.0.0.1/32", "--yes", "--dry-run", binp])
        finally:
            fu.discover = orig
        self.assertEqual(rc, 0)
        self.assertEqual(self.ctrl.uploads, 0)

    # ── board targeting ─────────────────────────────────────────────────

    def _dry_run(self, binp, controllers, extra_args=()):
        orig = fu.discover
        fu.discover = lambda subnets, passes=2: [dict(c) for c in controllers]
        try:
            return fu.main(["--subnet", "127.0.0.1/32", "--yes", "--dry-run",
                            *extra_args, binp])
        finally:
            fu.discover = orig

    def test_board_mismatch_excluded(self):
        # Image targets crowpanel; the only controller is a ref_rect_stub →
        # excluded, nothing to do (exit 0), no upload.
        binp = self._make_firmware("new", board="crowpanel_21_round")
        rc = self._dry_run(binp, [
            {"ip": "127.0.0.1", "home_room_name": "Garage",
             "board": "ref_rect_stub", "firmware_version": "old"},
        ])
        self.assertEqual(rc, 0)
        self.assertEqual(self.ctrl.uploads, 0)

    def test_board_unknown_excluded(self):
        # A controller that reports no board can't be confirmed compatible →
        # excluded when the image declares a target board.
        binp = self._make_firmware("new", board="crowpanel_21_round")
        rc = self._dry_run(binp, [
            {"ip": "127.0.0.1", "home_room_name": "Attic",
             "firmware_version": "old"},  # no 'board' key
        ])
        self.assertEqual(rc, 0)
        self.assertEqual(self.ctrl.uploads, 0)

    def test_matching_board_is_pending(self):
        binp = self._make_firmware("new", board="crowpanel_21_round")
        rc = self._dry_run(binp, [
            {"ip": "127.0.0.1", "home_room_name": "Kitchen",
             "board": "crowpanel_21_round", "firmware_version": "old"},
        ])
        self.assertEqual(rc, 1)  # a pending update exists

    def test_force_includes_mismatched_board(self):
        # --force disables board matching: a mismatched board becomes pending.
        binp = self._make_firmware("new", board="crowpanel_21_round")
        rc = self._dry_run(binp, [
            {"ip": "127.0.0.1", "home_room_name": "Garage",
             "board": "ref_rect_stub", "firmware_version": "old"},
        ], extra_args=("--force",))
        self.assertEqual(rc, 1)

    def test_legacy_image_no_board_marker_skips_filtering(self):
        # Image without a board marker → no board filtering; a mismatched-board
        # controller is still selected by version alone.
        binp = self._make_firmware("new")  # no board marker
        rc = self._dry_run(binp, [
            {"ip": "127.0.0.1", "home_room_name": "Garage",
             "board": "ref_rect_stub", "firmware_version": "old"},
        ])
        self.assertEqual(rc, 1)

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

    # ── post_update failure classification ──────────────────────────────

    def test_post_update_unreachable_is_failure(self):
        import socket as _s
        with _s.socket() as sk:  # grab a port nothing listens on
            sk.bind(("127.0.0.1", 0)); closed_port = sk.getsockname()[1]
        fu.HTTP_PORT = closed_port
        ok, msg = fu.post_update("127.0.0.1", b"fw", "pw", timeout=2)
        self.assertFalse(ok)
        self.assertIn("could not connect", msg)

    def test_post_update_success_and_reject(self):
        ok, _ = fu.post_update("127.0.0.1", b"fw", "pw", timeout=5)
        self.assertTrue(ok)
        ok, msg = fu.post_update("127.0.0.1", b"fw", "WRONG", timeout=5)
        self.assertFalse(ok)
        self.assertIn("401", msg)

    def test_post_update_drop_after_upload_is_accepted(self):
        import socket as _s
        srv = _s.socket(); srv.bind(("127.0.0.1", 0)); srv.listen(1)
        def serve():
            conn, _ = srv.accept()
            conn.recv(1 << 20)  # read (part of) the upload, then drop it like a rebooting device
            conn.close()
        threading.Thread(target=serve, daemon=True).start()
        fu.HTTP_PORT = srv.getsockname()[1]
        ok, msg = fu.post_update("127.0.0.1", b"fw", "pw", timeout=5)
        srv.close()
        self.assertTrue(ok)
        self.assertIn("expected", msg)

    def test_post_update_timeout_is_failure(self):
        import socket as _s
        srv = _s.socket(); srv.bind(("127.0.0.1", 0)); srv.listen(1)
        held = []
        def serve():
            conn, _ = srv.accept(); held.append(conn)  # accept, never answer
        threading.Thread(target=serve, daemon=True).start()
        fu.HTTP_PORT = srv.getsockname()[1]
        ok, msg = fu.post_update("127.0.0.1", b"fw", "pw", timeout=1)
        srv.close()
        self.assertFalse(ok)
        self.assertIn("timed out", msg)


if __name__ == "__main__":
    unittest.main(verbosity=2)
