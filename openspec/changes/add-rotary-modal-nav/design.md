## Context

See proposal.md — Why.

Encoder events are processed in `main.cpp` inside `ui_task`'s event drain loop (`bsp::pop_event`). `VolumeDelta` adjusts volume; `EncoderButtonDown/Up` drives the short-press play/pause and long-press factory-reset flow. Overlays (favorites, settings, group picker, room picker) are LVGL objects parented to `lv_layer_top()`. The existing overlay guard in `now_playing_update` already walks `lv_layer_top()` children to detect active overlays — the same mechanism is available here.

## Goals / Non-Goals

**Goals:**
- Route encoder rotation and press to list navigation when any modal overlay is open
- Show a minimal selection indicator on the selected list item
- Activate the selected item on encoder press via LVGL event dispatch (identical to a touch tap)
- Zero overhead when no modal is open

**Non-Goals:**
- Keyboard/LVGL group-based focus system — this is a lightweight bespoke approach
- Supporting nested navigation (sub-menus within sub-menus) beyond what already exists
- Animated selection indicator movement

## Decisions

### 1. Overlay detection: poll `lv_layer_top()` at event time

**Decision**: At the top of the encoder event handler, check `lv_layer_top()` for non-hidden children. If found, route to navigation; otherwise route to volume/transport.

**Rationale**: Consistent with the existing overlay guard in `now_playing_update`. Avoids a separate global "modal open" flag that must be kept in sync across multiple overlay open/close sites.

**Alternative considered**: Per-overlay boolean flags or a reference-counted modal counter. Rejected — more state to maintain and more call sites to update.

### 2. Navigation target registration: explicit register/clear

**Decision**: Add a thin module `src/ui/nav.h` / `nav.cpp` with two functions:
- `nav::register_list(lv_obj_t* list_obj)` — called by each overlay when it opens its list
- `nav::clear_list()` — called by each overlay on close (or implicitly on first navigation if the registered object is deleted)

A module-static `lv_obj_t* s_list = nullptr` and `int s_index = -1` hold the current state.

**Rationale**: Explicit registration is safer than scanning the DOM for the first `lv_list` child — overlays can contain multiple LVGL objects and the navigation target may not be the only list.

**Alternative considered**: Scanning `lv_layer_top()` subtree for `lv_obj_t` with list class. Rejected — fragile; settings overlay contains non-list content, and the room picker list is a different object than the group list.

### 3. Selection indicator: style the selected button directly

**Decision**: `nav::navigate(delta)` gets the list child at `s_index`, removes the highlight style from the previous item, applies a named border style to the new item (1–2 px accent-coloured border, or `LV_STATE_CHECKED` if the list button supports it), and calls `lv_obj_scroll_to_view(child, LV_ANIM_ON)` to keep it visible. No separate overlay widget is created.

**Rationale**: LVGL list items are `lv_button` objects — adding a border style at a specific state is native LVGL. Avoids creating and positioning a separate transparent indicator object, which would require matching the item's geometry.

**Alternative considered**: A separate `lv_obj_t` child with no background and a 2 px border, absolutely positioned over the selected row. Rejected — requires computing item Y offsets, which is pixel arithmetic in screen code.

### 4. Item activation: `lv_obj_send_event(item, LV_EVENT_CLICKED, nullptr)`

**Decision**: On encoder press with a valid selection, retrieve the child at `s_index` and call `lv_obj_send_event(child, LV_EVENT_CLICKED, nullptr)` — LVGL 9.5's object-level dispatch function (the older `lv_event_send(obj, ...)` signature was replaced by `lv_event_send(lv_event_list_t*, ...)` in this LVGL version).

**Rationale**: Each list item already has a `LV_EVENT_CLICKED` callback registered by the overlay that built it (favorites item plays, group item toggles, settings row opens sub-screen). Re-using the same event keeps behavior identical to a touch tap and requires no duplication of action logic.

**Alternative considered**: Calling the action callback directly. Rejected — callbacks are lambda closures capturing local variables; there is no stable function pointer to call.

### 5. Encoder button interaction with existing long-press / factory-reset

**Decision**: The navigation select intent is emitted on `EncoderButtonUp` only when `s_index != -1` and elapsed press time is below `kMuteShortPressMs` — reusing the same short-press gate already in place. The long-press reset flow (`s_reset_timer`, `s_reset_overlay`) is independent and remains unchanged; a held encoder still triggers factory reset regardless of navigation mode.

**Rationale**: The reset flow is safety-critical and must not be suppressed by navigation mode. The short-press threshold ensures a brief tap dispatches select while a held press escalates to reset.

## Risks / Trade-offs

- **Overlay close without `nav::clear_list()`**: If an overlay is deleted without calling `nav::clear_list()`, `s_list` becomes a dangling pointer. Mitigation: `nav::navigate()` and `nav::select()` call `lv_obj_is_valid(s_list)` before dereferencing; invalid → auto-clear.
- **Multiple overlays open simultaneously**: The current codebase does not stack overlays (each overlay close guard prevents duplicates), but if a second overlay opens without the first closing, `register_list` would silently replace the target. Mitigation: document the single-active-overlay invariant; it is already enforced at each overlay open site.
- **ref_rect_stub**: The stub board has no encoder hardware but the code path must still compile. The `nav` module contains no board-specific code and compiles cleanly on both targets.

## Open Questions

- Exact visual style for the selection indicator (border width, colour — use accent token or a fixed highlight?). Safe to decide during implementation; does not affect the spec or task breakdown.
