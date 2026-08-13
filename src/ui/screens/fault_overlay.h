#pragma once

// Connectivity fault status layer.
//
// Diagnoses the current connectivity fault (no WiFi / no Sonos found / home
// room offline) from local signals only (WiFi station status + discovery
// state) and shows a distinct full-screen status overlay per fault, with
// silent auto-recovery when the condition clears. The no-WiFi screen offers a
// confirm-guarded "re-run WiFi setup" that factory-resets and reboots to the
// captive portal.
//
// All entry points run on the UI task (core 1) — they call lv_* directly.
namespace screens {
    // Call once per UI tick from the UI task. Internally throttled: it
    // re-evaluates on a slow cadence and only shows/hides the overlay when the
    // diagnosed fault changes (with debounce against flapping).
    void fault_overlay_tick();
}
