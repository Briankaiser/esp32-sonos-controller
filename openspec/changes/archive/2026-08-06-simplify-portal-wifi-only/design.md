## Context

See proposal.md — Why. The captive portal is implemented in `src/services/portal.cpp`. The provisioning data structure is in `src/services/prefs.h/.cpp`. Home room UUID is currently a required portal field stored via `store_provisioning()` and checked by `is_provisioned()`.

The `prefs::home_room()` accessor and its NVS key (`home_room`) are retained — the home room selection screen (separate downstream change) will write to that key after first-boot discovery.

## Goals / Non-Goals

**Goals:**
- Remove home room UUID from the portal form and from provisioning validation
- Make `is_provisioned()` WiFi-credential-only

**Non-Goals:**
- Removing the `prefs::home_room()` accessor or its NVS key — still needed by discovery and future selection screen
- Any other portal field changes
- The home room selection UI — separate change

## Decisions

### D1: Remove `room` parameter from `store_provisioning()` entirely

**Choice:** Drop the `room` parameter from the function signature rather than making it optional with a default.

**Why:** There is exactly one call site (`portal.cpp`), and optional parameters with defaults silently allow the old behavior to persist. A signature change is a hard compile error at the one call site — immediately visible, easy to fix.

### D2: `is_provisioned()` checks only SSID + password

**Choice:** Return `true` iff both `wifi_ssid()` and `wifi_pass()` are non-empty.

**Why:** Home room is now a post-boot configuration step. Tying `is_provisioned()` to it would put the device back in portal mode on every boot until the user picks a room — which is the wrong gate. WiFi credentials are the minimal set needed to boot into normal operation.

## Risks / Trade-offs

- **Existing provisioned devices have a home room UUID in NVS** — that UUID continues to work correctly with the discovery resolution logic; this change only affects what the portal collects on fresh provisioning.
- **Signature break** — any future code that calls `store_provisioning()` with a room argument will fail to compile. That is the intended behavior.
