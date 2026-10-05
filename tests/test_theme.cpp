#include "eatsbits/ui/theme.hpp"
#include "eatsbits/ui/widgets/theme_browser_dialog.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace eatsbits::ui;

static void assertNear(float actual, float expected, float eps = 0.02f, const char* msg = "") {
    if (std::abs(actual - expected) > eps) {
        std::cerr << "Assertion failed: " << msg << " (actual: " << actual << ", expected: " << expected << ")" << std::endl;
        std::abort();
    }
}

void testColorConversions() {
    std::cout << "[Test] Color conversions (Hex, RGBA, HSL)..." << std::endl;

    // Hex parsing
    Color cAmber = Color::fromHex(0xFF8C00); // 255, 140, 0
    assertNear(cAmber.r, 1.0f, 0.01f, "Amber red");
    assertNear(cAmber.g, 140.0f / 255.0f, 0.01f, "Amber green");
    assertNear(cAmber.b, 0.0f, 0.01f, "Amber blue");
    assertNear(cAmber.a, 1.0f, 0.01f, "Amber alpha");

    // RGBA8 packing
    uint32_t packed = cAmber.toRgba8();
    (void)packed;
    assert(((packed >> 24) & 0xFF) == 255);
    assert(((packed >> 16) & 0xFF) == 140);
    assert(((packed >> 8) & 0xFF) == 0);
    assert((packed & 0xFF) == 255);

    // HSL roundtrip
    Color::Hsl hsl = cAmber.toHsl();
    assertNear(hsl.h, 32.9f, 2.0f, "Amber hue in degrees");
    assertNear(hsl.s, 1.0f, 0.02f, "Amber saturation");
    assertNear(hsl.l, 0.50f, 0.05f, "Amber lightness");

    Color roundtrip = Color::fromHsl(hsl.h, hsl.s, hsl.l);
    assertNear(roundtrip.r, cAmber.r, 0.02f, "Roundtrip red");
    assertNear(roundtrip.g, cAmber.g, 0.02f, "Roundtrip green");
    assertNear(roundtrip.b, cAmber.b, 0.02f, "Roundtrip blue");

    // Pure Red, Green, Blue, White, Black HSL
    Color red = Color::fromHex(0xFF0000);
    assertNear(red.toHsl().h, 0.0f, 1.0f, "Red hue");
    Color green = Color::fromHex(0x00FF00);
    assertNear(green.toHsl().h, 120.0f, 1.0f, "Green hue");
    Color blue = Color::fromHex(0x0000FF);
    assertNear(blue.toHsl().h, 240.0f, 1.0f, "Blue hue");

    std::cout << "  [PASS] Color conversions passed." << std::endl;
}

void testColorAdjustments() {
    std::cout << "[Test] Color adjustments (lighten, darken, saturate, blend)..." << std::endl;

    Color c(0.5f, 0.2f, 0.2f, 1.0f);
    float initialL = c.toHsl().l;
    (void)initialL;

    Color lightened = c.lighten(0.2f);
    assert(lightened.toHsl().l > initialL);

    Color darkened = c.darken(0.15f);
    assert(darkened.toHsl().l < initialL);

    Color blended = Color::blend(Color(0.0f, 0.0f, 0.0f), Color(1.0f, 1.0f, 1.0f), 0.75f);
    assertNear(blended.r, 0.75f, 0.01f);
    assertNear(blended.g, 0.75f, 0.01f);
    assertNear(blended.b, 0.75f, 0.01f);

    // Luminance test: Pure White = 1.0, Pure Black = 0.0
    Color white(1.0f, 1.0f, 1.0f);
    Color black(0.0f, 0.0f, 0.0f);
    assertNear(white.luminance(), 1.0f, 0.01f, "White luminance");
    assertNear(black.luminance(), 0.0f, 0.01f, "Black luminance");
    assert(!white.isDark());
    assert(black.isDark());

    // Complementary color (Hue + 180)
    Color red = Color::fromHex(0xFF0000);
    Color comp = red.complementary();
    assertNear(comp.toHsl().h, 180.0f, 2.0f, "Red complementary (Cyan)");

    std::cout << "  [PASS] Color adjustments passed." << std::endl;
}

void testAutomaticThemeDerivation() {
    std::cout << "[Test] Automatic theme derivation from 2 seed colors (no overrides)..." << std::endl;

    ThemeSeed seed;
    seed.name = "Cyber Plum";
    seed.description = "Deep purple synth chassis with hot pink accent";
    seed.isLight = false;
    seed.chassisBase   = Color::fromHex(0x1F132B); // Deep purple
    seed.primaryAccent = Color::fromHex(0xFF007F); // Hot pink

    ThemeTokens tokens = Theme::buildTokens(seed);
    assert(tokens.name == "Cyber Plum");
    assert(!tokens.isLight);

    // Verify background is darker than chassis
    assert(tokens.backgroundDark.luminance() < tokens.panelBackground.luminance());

    // Verify header is lighter than chassis
    assert(tokens.panelHeader.luminance() > tokens.panelBackground.luminance());

    // Verify control well is darker than chassis (inset)
    assert(tokens.controlWell.luminance() < tokens.panelBackground.luminance());

    // Verify text primary has high contrast against dark background
    assert(!tokens.textPrimary.isDark());
    assert(tokens.textPrimary.luminance() > 0.7f);

    std::cout << "  [PASS] Automatic theme derivation passed." << std::endl;
}

void testCuratedEatsbeatsPresets() {
    std::cout << "[Test] Curated Eatsbeats presets & switching..." << std::endl;

    // Default preset must be AteTrack
    assert(Theme::getCurrentPreset() == Theme::Preset::AteTrack);
    const ThemeTokens& ateTrack = Theme::current();
    assert(ateTrack.name == "Ate Track");
    assert(!ateTrack.isLight);

    // Signature amber accent: #FF8C00
    assertNear(ateTrack.primaryAccent.r, 1.0f, 0.02f);
    assertNear(ateTrack.primaryAccent.g, 140.0f / 255.0f, 0.02f);
    assertNear(ateTrack.primaryAccent.b, 0.0f, 0.02f);

    // Check Midnight Bites
    Theme::setPreset(Theme::Preset::MidnightBites);
    assert(Theme::getCurrentPreset() == Theme::Preset::MidnightBites);
    const ThemeTokens& midnight = Theme::current();
    assert(midnight.name == "Midnight Bites");
    assert(!midnight.isLight);
    // Signature cyan accent: #21F4E8
    assertNear(midnight.primaryAccent.g, 244.0f / 255.0f, 0.02f);
    assertNear(midnight.primaryAccent.b, 232.0f / 255.0f, 0.02f);

    // Check Light Snack
    Theme::setPreset(Theme::Preset::LightSnack);
    assert(Theme::getCurrentPreset() == Theme::Preset::LightSnack);
    const ThemeTokens& lightSnack = Theme::current();
    assert(lightSnack.name == "Light Snack");
    assert(lightSnack.isLight);
    assert(lightSnack.textPrimary.isDark()); // Dark text on light background
    (void)lightSnack;

    // Check Breakfast
    Theme::setPreset(Theme::Preset::Breakfast);
    const ThemeTokens& breakfast = Theme::current();
    assert(breakfast.name == "Breakfast");
    assert(breakfast.isLight);
    assertNear(breakfast.syntaxKeyword.g, 153.0f / 255.0f, 0.05f); // Solarized green
    assertNear(breakfast.syntaxString.g, 161.0f / 255.0f, 0.05f); // Solarized cyan

    // Check Dinner
    Theme::setPreset(Theme::Preset::Dinner);
    const ThemeTokens& dinner = Theme::current();
    assert(dinner.name == "Dinner");
    assert(!dinner.isLight);
    assertNear(dinner.syntaxKeyword.g, 153.0f / 255.0f, 0.05f); // Solarized green

    // Check Count's Bite (Dracula)
    Theme::setPreset(Theme::Preset::CountsBite);
    const ThemeTokens& countsBite = Theme::current();
    assert(countsBite.name == "Count's Bite");
    assert(!countsBite.isLight);
    assertNear(countsBite.primaryAccent.r, 189.0f / 255.0f, 0.05f); // Dracula purple

    // Check Nordic Frost (Nord)
    Theme::setPreset(Theme::Preset::NordicFrost);
    const ThemeTokens& nord = Theme::current();
    assert(nord.name == "Nordic Frost");
    assert(!nord.isLight);
    assertNear(nord.primaryAccent.r, 136.0f / 255.0f, 0.05f); // Frost cyan

    // Check Catppuccino (Catppuccin Mocha)
    Theme::setPreset(Theme::Preset::Catppuccino);
    const ThemeTokens& catppuccin = Theme::current();
    assert(catppuccin.name == "Catppuccino");
    assert(!catppuccin.isLight);

    // Check Dark Roast (Monochrome)
    Theme::setPreset(Theme::Preset::DarkRoast);
    const ThemeTokens& darkRoast = Theme::current();
    assert(darkRoast.name == "Dark Roast");
    assert(!darkRoast.isLight);

    // Check Sea & Salt (DuoTone)
    Theme::setPreset(Theme::Preset::SeaAndSalt);
    const ThemeTokens& seaAndSalt = Theme::current();
    assert(seaAndSalt.name == "Sea & Salt");
    assert(!seaAndSalt.isLight);
    assertNear(seaAndSalt.syntaxKeyword.r, 255.0f / 255.0f, 0.05f); // Amber gold

    // Verify catalog count
    const auto& catalog = Theme::getPresetCatalog();
    assert(catalog.size() == Theme::kPresetCount);
    assert(Theme::getAllPresets().size() == Theme::kPresetCount);

    // Reset back to default AteTrack
    Theme::setPreset(Theme::Preset::AteTrack);
    assert(Theme::getCurrentPreset() == Theme::Preset::AteTrack);

    std::cout << "  [PASS] All 10 curated presets & syntax tokens verified." << std::endl;
}

void testThemeBrowserDialog() {
    std::cout << "[Test] Filterable ThemeBrowserDialog modal & live preview..." << std::endl;

    ThemeBrowserDialog dialog;
    assert(!dialog.isOpen());

    dialog.open();
    assert(dialog.isOpen());
    dialog.layout(1280.0f, 800.0f);

    // Initial state: ALL category should show all 10 presets
    assert(dialog.getFilteredCount() == Theme::kPresetCount);

    // 1. Category filter checks
    dialog.setSelectedCategoryIndex(1); // DARK
    assert(dialog.getFilteredCount() >= 6);

    dialog.setSelectedCategoryIndex(2); // LIGHT
    assert(dialog.getFilteredCount() == 2); // Light Snack, Breakfast

    dialog.setSelectedCategoryIndex(3); // SYNTAX PORTS
    assert(dialog.getFilteredCount() == 5); // Breakfast, Dinner, Count's Bite, Nordic Frost, Catppuccino

    dialog.setSelectedCategoryIndex(4); // MINIMAL / DUOTONE
    assert(dialog.getFilteredCount() == 2); // Dark Roast, Sea & Salt

    dialog.setSelectedCategoryIndex(0); // ALL
    assert(dialog.getFilteredCount() == Theme::kPresetCount);

    // 2. Real-time search query filtering
    dialog.setSearchQuery("dracula");
    assert(dialog.getFilteredCount() == 1);

    dialog.setSearchQuery("nord");
    assert(dialog.getFilteredCount() == 1);

    dialog.setSearchQuery("solar");
    assert(dialog.getFilteredCount() == 2); // Breakfast & Dinner

    dialog.clearSearch();
    assert(dialog.getFilteredCount() == Theme::kPresetCount);

    // 3. Live Preview & Cancel Revert test
    Theme::setPreset(Theme::Preset::AteTrack);
    dialog.open();
    dialog.setSelectedItemIndex(5); // Count's Bite (Dracula)
    assert(Theme::getCurrentPreset() == Theme::Preset::CountsBite); // Instant live preview!

    // Escape key press reverts back to AteTrack
    dialog.handleKey(256, 0, 1, 0); // GLFW_KEY_ESCAPE
    assert(!dialog.isOpen());
    assert(Theme::getCurrentPreset() == Theme::Preset::AteTrack); // Restored!

    // 4. Confirm & Apply test
    dialog.open();
    dialog.setSearchQuery("Nordic");
    assert(dialog.getFilteredCount() == 1);
    dialog.setSelectedItemIndex(0);

    bool callbackFired = false;
    dialog.onThemeApplied = [&](Theme::Preset p) {
        callbackFired = true;
        assert(p == Theme::Preset::NordicFrost);
    };

    dialog.handleKey(257, 0, 1, 0); // GLFW_KEY_ENTER
    assert(!dialog.isOpen());
    assert(callbackFired);
    assert(Theme::getCurrentPreset() == Theme::Preset::NordicFrost);

    // Reset back to default AteTrack
    Theme::setPreset(Theme::Preset::AteTrack);
    assert(Theme::getCurrentPreset() == Theme::Preset::AteTrack);

    std::cout << "  [PASS] ThemeBrowserDialog category filtering, search, and live preview verified." << std::endl;
}

int main() {
    std::cout << "=== Running Eatsbits Theme Engine Unit Tests ===" << std::endl;
    testColorConversions();
    testColorAdjustments();
    testAutomaticThemeDerivation();
    testCuratedEatsbeatsPresets();
    testThemeBrowserDialog();
    std::cout << "=== All Theme Engine Unit Tests Passed! ===" << std::endl;
    return 0;
}
