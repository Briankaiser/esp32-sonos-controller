#!/usr/bin/env python3
"""
Mock Sonos server for firmware testing.

Listens for SSDP discovery requests and responds with a fake zone-player
description. Serves SOAP command endpoints that record received actions.
Tests query GET /log to assert on the command sequence without touching
real speakers.

Usage:
    python3 scripts/mock_sonos.py [--host HOST] [--port PORT]

    HOST  HTTP server bind address (default: 0.0.0.0)
    PORT  HTTP server port; 0 = random (default: 0)

The server prints its HTTP port to stdout on startup so callers can read it.

Endpoints:
    GET  /device.xml                 UPnP device description XML
    POST /AVTransport/control        Accepts SOAP play/pause/skip/etc
    POST /RenderingControl/control   Accepts SOAP volume/mute
    GET  /log                        Returns JSON array of received SOAP actions
    POST /reset                      Clears the command log

SSDP:
    Responds to M-SEARCH on 239.255.255.250:1900 for zone-player targets.
    Falls back to unicast if multicast socket bind fails (e.g. in restricted CI).
"""

import argparse
import json
import re
import socket
import struct
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer


# ── Shared state ──────────────────────────────────────────────────────────────

_lock = threading.Lock()
_commands: list[dict] = []   # [{action: str, service: str}, ...]


def _record(service: str, action: str) -> None:
    with _lock:
        _commands.append({"service": service, "action": action})


def _get_log() -> list[dict]:
    with _lock:
        return list(_commands)


def _reset() -> None:
    with _lock:
        _commands.clear()


# ── SOAP helpers ──────────────────────────────────────────────────────────────

def _soap_success(service_ns: str, action: str) -> bytes:
    body = (
        '<?xml version="1.0"?>'
        '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"'
        ' s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">'
        '<s:Body>'
        f'<u:{action}Response xmlns:u="{service_ns}"/>'
        '</s:Body>'
        '</s:Envelope>'
    )
    return body.encode()


def _extract_action(soap_body: bytes) -> str:
    """Extract the action name from a SOAPAction header or body."""
    match = re.search(rb'<u:([A-Za-z]+)\s', soap_body)
    return match.group(1).decode() if match else "Unknown"


# ── UPnP device description ───────────────────────────────────────────────────

AVTRANSPORT_NS = "urn:schemas-upnp-org:service:AVTransport:1"
RENDERING_NS   = "urn:schemas-upnp-org:service:RenderingControl:1"


def _device_xml(host: str, port: int) -> bytes:
    base = f"http://{host}:{port}"
    xml = f"""<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <specVersion><major>1</major><minor>0</minor></specVersion>
  <device>
    <deviceType>urn:schemas-upnp-org:device:ZonePlayer:1</deviceType>
    <friendlyName>Mock Sonos (test)</friendlyName>
    <manufacturer>Mock</manufacturer>
    <modelName>MockSonos</modelName>
    <UDN>uuid:mock-sonos-0000-0000-0000-000000000001</UDN>
    <serviceList>
      <service>
        <serviceType>{AVTRANSPORT_NS}</serviceType>
        <serviceId>urn:upnp-org:serviceId:AVTransport</serviceId>
        <controlURL>/AVTransport/control</controlURL>
        <eventSubURL>/AVTransport/event</eventSubURL>
        <SCPDURL>/AVTransport/scpd.xml</SCPDURL>
      </service>
      <service>
        <serviceType>{RENDERING_NS}</serviceType>
        <serviceId>urn:upnp-org:serviceId:RenderingControl</serviceId>
        <controlURL>/RenderingControl/control</controlURL>
        <eventSubURL>/RenderingControl/event</eventSubURL>
        <SCPDURL>/RenderingControl/scpd.xml</SCPDURL>
      </service>
    </serviceList>
  </device>
</root>"""
    return xml.encode()


# ── HTTP request handler ──────────────────────────────────────────────────────

class _Handler(BaseHTTPRequestHandler):

    server_version = "MockSonos/1.0"

    def log_message(self, fmt, *args):
        pass  # suppress access log; test callers use /log endpoint

    def do_GET(self):
        if self.path == "/device.xml":
            host, port = self.server.server_address
            body = _device_xml(host, port)
            self._respond(200, "text/xml", body)
        elif self.path == "/log":
            body = json.dumps(_get_log()).encode()
            self._respond(200, "application/json", body)
        else:
            self._respond(404, "text/plain", b"Not found")

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length)

        if self.path == "/reset":
            _reset()
            self._respond(200, "text/plain", b"OK")
            return

        # Determine service from path
        if "/AVTransport/" in self.path:
            service = "AVTransport"
            ns = AVTRANSPORT_NS
        elif "/RenderingControl/" in self.path:
            service = "RenderingControl"
            ns = RENDERING_NS
        else:
            self._respond(404, "text/plain", b"Unknown service")
            return

        action = _extract_action(body)
        _record(service, action)
        self._respond(200, "text/xml", _soap_success(ns, action))

    def _respond(self, code: int, content_type: str, body: bytes) -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


# ── SSDP responder ────────────────────────────────────────────────────────────

SSDP_ADDR = "239.255.255.250"
SSDP_PORT = 1900
SSDP_TARGETS = {
    "urn:schemas-upnp-org:device:ZonePlayer:1",
    "ssdp:all",
    "upnp:rootdevice",
}


def _ssdp_response(location: str) -> bytes:
    return (
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        f"LOCATION: {location}\r\n"
        "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n"
        "USN: uuid:mock-sonos-0000-0000-0000-000000000001"
        "::urn:schemas-upnp-org:device:ZonePlayer:1\r\n"
        "EXT:\r\n"
        "\r\n"
    ).encode()


def _run_ssdp(http_host: str, http_port: int) -> None:
    location = f"http://{http_host if http_host != '0.0.0.0' else '127.0.0.1'}:{http_port}/device.xml"
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind((SSDP_ADDR, SSDP_PORT))
        mreq = struct.pack("4sL", socket.inet_aton(SSDP_ADDR), socket.INADDR_ANY)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        print(f"[mock_sonos] SSDP listening on {SSDP_ADDR}:{SSDP_PORT} (multicast)",
              flush=True)
    except OSError:
        # Multicast bind failed (common on restricted CI); fall back to unicast
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("0.0.0.0", SSDP_PORT))
        print(f"[mock_sonos] SSDP listening on 0.0.0.0:{SSDP_PORT} (unicast fallback)",
              flush=True)

    while True:
        data, addr = sock.recvfrom(1024)
        if b"M-SEARCH" in data:
            for target in SSDP_TARGETS:
                if target.encode() in data:
                    sock.sendto(_ssdp_response(location), addr)
                    break


# ── Entry point ───────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="0.0.0.0", help="HTTP bind address")
    parser.add_argument("--port", type=int, default=0,
                        help="HTTP port; 0 = random (default)")
    args = parser.parse_args()

    server = HTTPServer((args.host, args.port), _Handler)
    _, actual_port = server.server_address
    print(f"[mock_sonos] HTTP server on port {actual_port}", flush=True)
    print(f"[mock_sonos] Device XML: http://127.0.0.1:{actual_port}/device.xml",
          flush=True)
    print(f"[mock_sonos] Command log: http://127.0.0.1:{actual_port}/log",
          flush=True)

    # SSDP in a daemon thread (exits when main thread exits)
    t = threading.Thread(target=_run_ssdp, args=(args.host, actual_port), daemon=True)
    t.start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
