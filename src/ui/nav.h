#pragma once
#include <lvgl.h>

/* ── Encoder navigation mode ──────────────────────────────────────────────────
 *
 * While a modal overlay is open, the rotary encoder repurposes from
 * volume/transport control to list navigation. Overlays register their
 * navigable list on open and clear it on close; the encoder handler in
 * main.cpp checks has_modal() before routing rotation/press events here.
 *
 * UI task only — all functions call lv_* internally.
 */

namespace nav {
    // Called by an overlay when it opens its navigable list. Resets selection
    // so the next turn starts fresh at the first item (per-modal lifecycle).
    void register_list(lv_obj_t* list_obj);

    // Called by an overlay on close. Removes any selection highlight and
    // drops the reference.
    void clear_list();

    // Move the selection one step forward (delta > 0) or backward (delta < 0).
    // First call after register_list() places the selection on the first item
    // regardless of delta's sign.
    void navigate(int delta);

    // Activate the currently selected item, identical to a touch tap.
    // No-op if there is no active selection.
    void select();

    // True when any non-hidden child exists on lv_layer_top() — i.e. a modal
    // overlay is currently visible.
    bool has_modal();

    // True when a list is registered and a selection index is active.
    bool has_selection();
}
