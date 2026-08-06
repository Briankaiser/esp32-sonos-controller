## Context

The web server (`s_web_server`) is already running on port 80 with `/screenshot` and `/update` registered. Speaker discovery results are held in `sonos::state::SonosState` behind a mutex; `sonos::state::snapshot()` is safe from any task. The NVS `home_room` key has a getter (`prefs::home_room()`) but no setter — `store_provisioning()` is the only write path today and bundles all provisioning fields together.

The sonos task reads `prefs::home_room()` inside `run_cycle()` on every discovery cycle. Writing to NVS from the web task and reading it on the sonos task is safe because the Arduino `Preferences` library serialises its own I2C/NVS operations.

## Goals / Non-Goals

**Goals:**
- Unblock hardware validation of the now-playing screen without a factory reset
- Provide a permanent remote-override path for the home room configuration

**Non-Goals:**
- Authenticated room selection — LAN-only access is the same trust boundary as `/screenshot`
- Triggering an immediate discovery cycle from the web handler (the 60 s interval is acceptable; document the behaviour)
- A full configuration REST API — only the home room is in scope here

## Decisions

### D1: Plain form POST body, no JSON input

**Choice:** `POST /rooms/select` accepts `uuid=<value>` as a URL-encoded form body (matching how `POST /save` on the portal works). `GET /rooms` returns JSON; the POST input is form-encoded.

**Why:** The Arduino `WebServer::arg("uuid")` helper parses both form bodies and query strings transparently. This means the endpoint is callable with a dead-simple `curl -d uuid=<uuid> http://<ip>/rooms/select` with no JSON serialisation on the client side. The response is JSON for machine readability.

**Alternative considered:** JSON body for both request and response — rejected because it requires the caller to serialize a body, which adds friction for a quick `curl` during development.

### D2: NVS write on the web task, no immediate discovery trigger

**Choice:** `prefs::set_home_room()` writes directly to NVS from within the web handler (web task, core 0). No semaphore or queue is used to signal the sonos task. The next `run_cycle()` call will see the new UUID.

**Why:** The Arduino `Preferences` library is thread-safe at the NVS level. Triggering an immediate rediscovery would require a cross-task signal (e.g., a binary semaphore the sonos task polls in its inner loop), adding complexity for a case where the 60 s wait is acceptable for the primary use case (initial setup). The serial log shows resolution within one cycle.

**Risk:** Up to 60 s delay between the POST and seeing `[Sonos] home room resolved: <name>` in the serial log. Acceptable for v1; a `POST /rooms/rediscover` endpoint can be added if needed.

### D3: `set_home_room()` as a standalone NVS write, not part of `store_provisioning()`

**Choice:** Add `prefs::set_home_room(const String& uuid)` that opens the `opensonos` namespace, writes just the `home_room` key, and closes. Does not touch any other provisioning field.

**Why:** The home room is intentionally decoupled from WiFi credentials — it can be changed without re-entering SSID/password. Bundling it into `store_provisioning()` would require passing all five fields on every home room change.

## Risks / Trade-offs

- **No authentication on POST /rooms/select** — any device on the LAN can change the home room. Mitigation: same risk profile as `/screenshot` (read-only data exfil), and the home room is not a credential. Acceptable for v1.
- **60 s resolution delay** — the operator may expect instant effect. Mitigation: the JSON response and the design notes document this explicitly; the serial log provides confirmation.
