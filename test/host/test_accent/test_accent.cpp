/* Accent colour unit tests — run on the host with no hardware.
 *
 * Tests the luminance-based legibility adjustment algorithm and the
 * accent token's default state. The style-binding and animation paths
 * in accent.cpp are LVGL-runtime concerns; they are tested end-to-end
 * on the device, not here.
 */

#include <unity.h>
#include "ui/theme/accent.h"

void setUp()    {}
void tearDown() {}

// ── Luminance helper (mirrors the BT.601 coefficients in accent.cpp) ─────────

static int32_t lum(lv_color_t c) {
    return (299 * c.red + 587 * c.green + 114 * c.blue) / 1000;
}

static int32_t lum_diff(lv_color_t a, lv_color_t b) {
    int32_t d = lum(a) - lum(b);
    return d < 0 ? -d : d;
}

// ── Luminance algorithm ───────────────────────────────────────────────────────

void test_lum_black_is_zero() {
    TEST_ASSERT_EQUAL_INT32(0, lum(lv_color_make(0, 0, 0)));
}

void test_lum_white_is_255() {
    TEST_ASSERT_EQUAL_INT32(255, lum(lv_color_make(255, 255, 255)));
}

void test_lum_pure_red() {
    // BT.601: 299 * 255 / 1000 = 76
    TEST_ASSERT_EQUAL_INT32(76, lum(lv_color_make(255, 0, 0)));
}

void test_lum_pure_green() {
    // BT.601: 587 * 255 / 1000 = 149
    TEST_ASSERT_EQUAL_INT32(149, lum(lv_color_make(0, 255, 0)));
}

// ── adjusted_for_legibility ───────────────────────────────────────────────────

void test_legibility_identical_colors_get_adjusted() {
    lv_color_t gray = lv_color_make(128, 128, 128);
    lv_color_t result = accent::adjusted_for_legibility(gray, gray);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(48, lum_diff(result, gray));
}

void test_legibility_high_contrast_unchanged() {
    // Black on white — already has maximum contrast
    lv_color_t black = lv_color_make(0, 0, 0);
    lv_color_t white = lv_color_make(255, 255, 255);
    lv_color_t result = accent::adjusted_for_legibility(black, white);
    // Should be unchanged (contrast already ≥ 48)
    TEST_ASSERT_EQUAL_UINT8(black.red,   result.red);
    TEST_ASSERT_EQUAL_UINT8(black.green, result.green);
    TEST_ASSERT_EQUAL_UINT8(black.blue,  result.blue);
}

void test_legibility_near_identical_gets_at_least_48_diff() {
    lv_color_t accent_c = lv_color_make(100, 100, 100);
    lv_color_t bg       = lv_color_make(110, 110, 110);
    lv_color_t result   = accent::adjusted_for_legibility(accent_c, bg);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(48, lum_diff(result, bg));
}

void test_legibility_bright_accent_on_bright_bg_darkens() {
    // Both very bright — accent should be darkened
    lv_color_t bright_accent = lv_color_make(240, 240, 240);
    lv_color_t bright_bg     = lv_color_make(240, 240, 240);
    lv_color_t result = accent::adjusted_for_legibility(bright_accent, bright_bg);
    TEST_ASSERT_LESS_THAN_INT32(lum(bright_accent), lum(result) + 1);  // darkened or equal
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(48, lum_diff(result, bright_bg));
}

// ── Default accent token state ────────────────────────────────────────────────

void test_default_accent_color_is_non_zero() {
    lv_color_t c = accent::get();
    int32_t l = lum(c);
    TEST_ASSERT_GREATER_THAN_INT32(0, l);
}

void test_default_accent_color_matches_default_gold() {
    lv_color_t c = accent::get();
    // Default is 0xD4A843 (warm gold — accent.cpp s_current initializer):
    // r=0xD4=212, g=0xA8=168, b=0x43=67
    TEST_ASSERT_EQUAL_UINT8(212, c.red);
    TEST_ASSERT_EQUAL_UINT8(168, c.green);
    TEST_ASSERT_EQUAL_UINT8(67,  c.blue);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_lum_black_is_zero);
    RUN_TEST(test_lum_white_is_255);
    RUN_TEST(test_lum_pure_red);
    RUN_TEST(test_lum_pure_green);

    RUN_TEST(test_legibility_identical_colors_get_adjusted);
    RUN_TEST(test_legibility_high_contrast_unchanged);
    RUN_TEST(test_legibility_near_identical_gets_at_least_48_diff);
    RUN_TEST(test_legibility_bright_accent_on_bright_bg_darkens);

    RUN_TEST(test_default_accent_color_is_non_zero);
    RUN_TEST(test_default_accent_color_matches_default_gold);

    return UNITY_END();
}
