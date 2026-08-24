#!/usr/bin/env python3
"""Mock Sonos GENA speaker — stdlib only.

Accepts SUBSCRIBE / UNSUBSCRIBE on the Sonos event paths, grants leases,
and pushes NOTIFY callbacks to the subscriber, exactly like a speaker.
Useful for exercising the device's eventing stack without real Sonos
hardware on the LAN (and for watching the device's SUBSCRIBE formatting).

Usage:
  python3 tools/mock_gena_speaker.py [--port 1400] [--lease 120]

Interactive commands once running:
  play / pause          push an AVTransport transport-state event
  track TITLE           push an AVTransport metadata event
  vol N                 push a RenderingControl volume event (0-100)
  mute / unmute         push a RenderingControl mute event
  topo                  push a ZoneGroupTopology event
  seq0 <svc>            re-push the initial full-state event (avt|rc|zgt)
  quit

Point the device at this host by faking an SSDP response, or simply watch
the mock's log while driving a real device build whose discovery found it.
The short default lease (120s) makes renewal behavior visible in minutes.
"""

import argparse
import http.client
import threading
import time
import uuid as uuidlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

SERVICES = {
    "/MediaRenderer/AVTransport/Event":      "avt",
    "/MediaRenderer/RenderingControl/Event": "rc",
    "/ZoneGroupTopology/Event":              "zgt",
}

# sid -> {service, callback, expires, seq}
SUBS = {}
SUBS_LOCK = threading.Lock()
LEASE_S = 120

STATE = {
    "transport": "PLAYING",
    "title": "Mock Track",
    "artist": "Mock Artist",
    "album": "Mock Album",
    "volume": 30,
    "mute": "0",
}


def enc(s: str) -> str:
    """One level of XML entity encoding."""
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
             .replace('"', "&quot;"))


def avt_body() -> str:
    didl = (
        '<DIDL-Lite xmlns:dc="http://purl.org/dc/elements/1.1/" '
        'xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">'
        '<item id="-1" parentID="-1">'
        f'<res>x-sonos-mock:track</res>'
        f'<dc:title>{STATE["title"]}</dc:title>'
        f'<dc:creator>{STATE["artist"]}</dc:creator>'
        f'<upnp:album>{STATE["album"]}</upnp:album>'
        '<upnp:albumArtURI>/getaa?mock=1</upnp:albumArtURI>'
        "</item></DIDL-Lite>"
    )
    event = (
        '<Event xmlns="urn:schemas-upnp-org:metadata-1-0/AVT/">'
        '<InstanceID val="0">'
        f'<TransportState val="{STATE["transport"]}"/>'
        '<CurrentTrackDuration val="0:03:30"/>'
        f'<CurrentTrackMetaData val="{enc(didl)}"/>'
        "</InstanceID></Event>"
    )
    return (
        '<e:propertyset xmlns:e="urn:schemas-upnp-org:event-1-0">'
        f"<e:property><LastChange>{enc(event)}</LastChange></e:property>"
        "</e:propertyset>"
    )


def rc_body() -> str:
    event = (
        '<Event xmlns="urn:schemas-upnp-org:metadata-1-0/RCS/">'
        '<InstanceID val="0">'
        f'<Volume channel="Master" val="{STATE["volume"]}"/>'
        '<Volume channel="LF" val="100"/><Volume channel="RF" val="100"/>'
        f'<Mute channel="Master" val="{STATE["mute"]}"/>'
        "</InstanceID></Event>"
    )
    return (
        '<e:propertyset xmlns:e="urn:schemas-upnp-org:event-1-0">'
        f"<e:property><LastChange>{enc(event)}</LastChange></e:property>"
        "</e:propertyset>"
    )


def zgt_body() -> str:
    zgs = (
        "<ZoneGroupState><ZoneGroups>"
        '<ZoneGroup Coordinator="RINCON_MOCK_A" ID="RINCON_MOCK_A:1">'
        '<ZoneGroupMember UUID="RINCON_MOCK_A" ZoneName="Mock Room" Invisible="0"/>'
        "</ZoneGroup>"
        "</ZoneGroups></ZoneGroupState>"
    )
    return (
        '<e:propertyset xmlns:e="urn:schemas-upnp-org:event-1-0">'
        f"<e:property><ZoneGroupState>{enc(zgs)}</ZoneGroupState></e:property>"
        "</e:propertyset>"
    )


BODY_FOR = {"avt": avt_body, "rc": rc_body, "zgt": zgt_body}


def send_notify(sid: str) -> None:
    with SUBS_LOCK:
        sub = SUBS.get(sid)
        if not sub:
            return
        seq = sub["seq"]
        sub["seq"] += 1
        callback = sub["callback"]
        service = sub["service"]
    body = BODY_FOR[service]().encode()
    u = urlparse(callback)
    try:
        conn = http.client.HTTPConnection(u.hostname, u.port or 80, timeout=5)
        conn.request(
            "NOTIFY", u.path or "/",
            body=body,
            headers={
                "CONTENT-TYPE": 'text/xml; charset="utf-8"',
                "NT": "upnp:event",
                "NTS": "upnp:propchange",
                "SID": sid,
                "SEQ": str(seq),
            },
        )
        resp = conn.getresponse()
        resp.read()
        conn.close()
        print(f"[mock] NOTIFY {service} seq={seq} → {callback} : HTTP {resp.status}")
    except OSError as e:
        print(f"[mock] NOTIFY {service} seq={seq} → {callback} FAILED: {e}")


def broadcast(service: str) -> None:
    with SUBS_LOCK:
        sids = [s for s, v in SUBS.items() if v["service"] == service]
    if not sids:
        print(f"[mock] no subscribers for {service}")
    for sid in sids:
        send_notify(sid)


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):  # quiet default access log
        pass

    def _reply(self, code, headers=None):
        self.send_response(code)
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_SUBSCRIBE(self):  # noqa: N802 — http.server dispatch name
        service = SERVICES.get(self.path)
        if not service:
            self._reply(404)
            return
        sid_hdr = self.headers.get("SID")
        callback = self.headers.get("CALLBACK", "").strip("<>")

        if sid_hdr:  # renewal
            with SUBS_LOCK:
                sub = SUBS.get(sid_hdr)
                if not sub:
                    print(f"[mock] renewal for unknown SID {sid_hdr} → 412")
                    self._reply(412)
                    return
                sub["expires"] = time.time() + LEASE_S
            print(f"[mock] renewed {service} {sid_hdr}")
            self._reply(200, {"SID": sid_hdr, "TIMEOUT": f"Second-{LEASE_S}"})
            return

        if not callback:
            self._reply(412)
            return
        sid = f"uuid:RINCON_MOCK_sub{uuidlib.uuid4().hex[:10]}"
        with SUBS_LOCK:
            SUBS[sid] = {
                "service": service,
                "callback": callback,
                "expires": time.time() + LEASE_S,
                "seq": 0,
            }
        print(f"[mock] SUBSCRIBE {service} ← {callback} sid={sid}")
        self._reply(200, {"SID": sid, "TIMEOUT": f"Second-{LEASE_S}"})
        # Initial full-state event (SEQ 0), as real Sonos does
        threading.Thread(target=send_notify, args=(sid,), daemon=True).start()

    def do_UNSUBSCRIBE(self):  # noqa: N802
        sid = self.headers.get("SID")
        with SUBS_LOCK:
            existed = SUBS.pop(sid, None) is not None
        print(f"[mock] UNSUBSCRIBE {sid} ({'ok' if existed else 'unknown'})")
        self._reply(200 if existed else 412)


def expiry_reaper():
    while True:
        time.sleep(5)
        now = time.time()
        with SUBS_LOCK:
            dead = [s for s, v in SUBS.items() if v["expires"] < now]
            for s in dead:
                print(f"[mock] lease expired: {s} ({SUBS[s]['service']})")
                del SUBS[s]


def main():
    global LEASE_S
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=1400)
    ap.add_argument("--lease", type=int, default=120)
    args = ap.parse_args()
    LEASE_S = args.lease

    server = ThreadingHTTPServer(("0.0.0.0", args.port), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    threading.Thread(target=expiry_reaper, daemon=True).start()
    print(f"[mock] GENA speaker on :{args.port}, lease {LEASE_S}s. "
          f"Commands: play pause track vol mute unmute topo quit")

    while True:
        try:
            line = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        if not line:
            continue
        cmd, _, arg = line.partition(" ")
        if cmd == "quit":
            break
        elif cmd in ("play", "pause"):
            STATE["transport"] = "PLAYING" if cmd == "play" else "PAUSED_PLAYBACK"
            broadcast("avt")
        elif cmd == "track":
            STATE["title"] = arg or "Mock Track"
            broadcast("avt")
        elif cmd == "vol":
            STATE["volume"] = max(0, min(100, int(arg or "30")))
            broadcast("rc")
        elif cmd in ("mute", "unmute"):
            STATE["mute"] = "1" if cmd == "mute" else "0"
            broadcast("rc")
        elif cmd == "topo":
            broadcast("zgt")
        elif cmd == "seq0":
            broadcast(arg or "avt")
        else:
            print(f"[mock] unknown command: {cmd}")


if __name__ == "__main__":
    main()
