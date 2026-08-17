/* Layout unit tests — run on the host with no hardware.
 *
 * Tests constexpr slot geometry, the pct() proportional helper, and the
 * safe-area disc-containment predicate. These mirror the compile-time
 * static_asserts in safe_area.h; running them at runtime confirms the
 * same invariants hold under the host compiler and gives meaningful
 * assertion messages when a regression is introduced.
 *
 * Task 2.5 note: testing layout_container() itself would require a
 * functional LVGL object tree. The meaningful unit is the slot geometry
 * that layout_container() reads; that is fully tested here.
 */

#include <unity.h>
#include "ui/layout/layout.h"

void setUp()    {}
void tearDown() {}

// ── pct() helper ─────────────────────────────────────────────────────────────

void test_pct_50_percent() {
    TEST_ASSERT_EQUAL_UINT16(240, pct(480, 50));
}

void test_pct_0_percent() {
    TEST_ASSERT_EQUAL_UINT16(0, pct(480, 0));
}

void test_pct_100_percent() {
    TEST_ASSERT_EQUAL_UINT16(480, pct(480, 100));
}

void test_pct_22_percent() {
    // 480 * 22 / 100 = 105 (expected SLOT_ALBUM_ART x offset)
    TEST_ASSERT_EQUAL_UINT16(105, pct(480, 22));
}

// ── Slot count ────────────────────────────────────────────────────────────────

void test_slot_count_equals_nine() {
    TEST_ASSERT_EQUAL_INT(9, static_cast<int>(SLOT_COUNT));
}

void test_archetype_defines_all_slots() {
    // Verify we can access every SlotId without asserting out-of-bounds
    for (int i = 0; i < static_cast<int>(SLOT_COUNT); ++i) {
        const LayoutSlot& s = slot(static_cast<SlotId>(i));
        TEST_ASSERT_GREATER_THAN_INT(0, static_cast<int>(s.w));
        TEST_ASSERT_GREATER_THAN_INT(0, static_cast<int>(s.h));
    }
}

// ── Album art slot geometry (round 480×480 archetype) ────────────────────────

void test_album_art_slot_is_full_bleed() {
    // SLOT_ALBUM_ART is a full-bleed background layer — the round display
    // hardware clips it to the disc (see archetype_round_compact.h).
    const LayoutSlot& s = slot(SLOT_ALBUM_ART);
    TEST_ASSERT_EQUAL_UINT16(0,   s.x);
    TEST_ASSERT_EQUAL_UINT16(0,   s.y);
    TEST_ASSERT_EQUAL_UINT16(480, s.w);
    TEST_ASSERT_EQUAL_UINT16(480, s.h);
}

void test_album_art_slot_is_square() {
    const LayoutSlot& s = slot(SLOT_ALBUM_ART);
    TEST_ASSERT_EQUAL_UINT16(s.w, s.h);
}

// ── Safe-area disc-containment ────────────────────────────────────────────────
// These mirror the static_asserts in safe_area.h and confirm the same
// invariants hold at runtime under the host compiler.

void test_all_slots_within_inscribed_disc() {
    // Disc: cx=240, cy=240, r²=57600
    static constexpr int32_t CX = 240, CY = 240;
    static constexpr uint32_t R2 = 240u * 240u;

    for (int i = 0; i < static_cast<int>(SLOT_COUNT); ++i) {
        // Mirror the safe_area.h exemptions: SLOT_ALBUM_ART is an intentional
        // full-bleed background and SLOT_VOLUME_ARC's bounding box spans the
        // framebuffer while its arc content is geometrically confined to the
        // disc. Neither is subject to the disc-containment invariant.
        if (i == static_cast<int>(SLOT_ALBUM_ART) ||
            i == static_cast<int>(SLOT_VOLUME_ARC)) continue;
        const LayoutSlot& s = slot(static_cast<SlotId>(i));
        int16_t x2 = static_cast<int16_t>(s.x + s.w - 1);
        int16_t y2 = static_cast<int16_t>(s.y + s.h - 1);

        // Test all four corners
        auto in_disc = [&](int16_t px, int16_t py) -> bool {
            int32_t dx = px - CX, dy = py - CY;
            return static_cast<uint32_t>(dx*dx + dy*dy) <= R2;
        };
        TEST_ASSERT_TRUE_MESSAGE(in_disc(s.x, s.y),   "top-left outside disc");
        TEST_ASSERT_TRUE_MESSAGE(in_disc(x2,  s.y),   "top-right outside disc");
        TEST_ASSERT_TRUE_MESSAGE(in_disc(s.x, y2),    "bottom-left outside disc");
        TEST_ASSERT_TRUE_MESSAGE(in_disc(x2,  y2),    "bottom-right outside disc");
    }
}

void test_slot_sizes_are_non_zero() {
    for (int i = 0; i < static_cast<int>(SLOT_COUNT); ++i) {
        const LayoutSlot& s = slot(static_cast<SlotId>(i));
        TEST_ASSERT_GREATER_THAN_UINT16(0, s.w);
        TEST_ASSERT_GREATER_THAN_UINT16(0, s.h);
    }
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_pct_50_percent);
    RUN_TEST(test_pct_0_percent);
    RUN_TEST(test_pct_100_percent);
    RUN_TEST(test_pct_22_percent);

    RUN_TEST(test_slot_count_equals_nine);
    RUN_TEST(test_archetype_defines_all_slots);

    RUN_TEST(test_album_art_slot_is_full_bleed);
    RUN_TEST(test_album_art_slot_is_square);

    RUN_TEST(test_all_slots_within_inscribed_disc);
    RUN_TEST(test_slot_sizes_are_non_zero);

    return UNITY_END();
}
