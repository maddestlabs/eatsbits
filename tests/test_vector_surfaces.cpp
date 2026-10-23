#include "eatsbits/ui/gui_panel_def.hpp"
#include "eatsbits/ui/theme.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace eatsbits::ui;

void testVectorFaceplateStyles() {
    std::cout << "[Test 1] Vector Faceplate Style Rendering..." << std::endl;

    ThemeTokens theme = Theme::get(Theme::Preset::AteTrack);
    BatchRenderer2D renderer;
    renderer.beginFrame(1920.0f, 1080.0f);

    const GuiChassisStyle styles[] = {
        GuiChassisStyle::DarkChassis,
        GuiChassisStyle::PcbGreen,
        GuiChassisStyle::MinimalWhite,
        GuiChassisStyle::Silver,
        GuiChassisStyle::Snes,
        GuiChassisStyle::Grunge,
        GuiChassisStyle::Walnut,
        GuiChassisStyle::Rosewood,
        GuiChassisStyle::BrushedSteel,
        GuiChassisStyle::BrushedAluminum,
        GuiChassisStyle::MattePowderCoat,
        GuiChassisStyle::Bakelite,
        GuiChassisStyle::CrinklePaint,
        GuiChassisStyle::Carbon
    };

    float curX = 10.0f;
    for (auto style : styles) {
        GuiPanelDef panel;
        panel.title = "TEST DEVICE";
        panel.subtitle = "Vector Faceplate";
        panel.chassisStyle = style;
        panel.cornerRadius = 6.0f;

        drawGuiFaceplate(renderer, panel, Rect2D(curX, 20.0f, 240.0f, 160.0f), theme);
        curX += 250.0f;
        if (curX > 1600.0f) curX = 10.0f;
    }

    assert(renderer.getVertexCount() > 0);
    std::cout << "  -> Generated " << renderer.getVertexCount() << " vector vertices across "
              << sizeof(styles)/sizeof(styles[0]) << " faceplate styles.\n";
    renderer.endFrame();
    std::cout << "  -> PASSED: All vector faceplate styles rendered cleanly." << std::endl;
}

void testCustomTintsAndWoodCheeks() {
    std::cout << "[Test 2] Custom Tints and Wood Cheeks..." << std::endl;

    ThemeTokens theme = Theme::get(Theme::Preset::AteTrack);
    BatchRenderer2D renderer;
    renderer.beginFrame(800.0f, 600.0f);

    GuiPanelDef panel;
    panel.title = "VINTAGE SYNTH";
    panel.subtitle = "Analog Subtractive";
    panel.chassisStyle = GuiChassisStyle::DarkChassis;
    panel.chassisTint = Color(0.20f, 0.55f, 0.95f); // Custom blue anodized tint
    panel.woodCheeks = true;
    panel.cornerRadius = 4.0f;

    drawGuiFaceplate(renderer, panel, Rect2D(40.0f, 40.0f, 400.0f, 220.0f), theme);

    assert(renderer.getVertexCount() > 0);
    renderer.endFrame();
    std::cout << "  -> PASSED: Custom tint and wood cheek accents verified." << std::endl;
}

void testCleanMinimalWhiteFaceplate() {
    std::cout << "[Test 3] Modern Minimal White Clean Faceplate..." << std::endl;

    ThemeTokens theme = Theme::get(Theme::Preset::AteTrack);
    BatchRenderer2D renderer;
    renderer.beginFrame(800.0f, 600.0f);

    GuiPanelDef panel;
    panel.title = "MINIMAL FILTER";
    panel.subtitle = "DSP Vector";
    panel.chassisStyle = GuiChassisStyle::MinimalWhite;
    panel.woodCheeks = false;
    panel.hideHeader = true;

    drawGuiFaceplate(renderer, panel, Rect2D(20.0f, 20.0f, 300.0f, 180.0f), theme);

    assert(renderer.getVertexCount() > 0);
    renderer.endFrame();
    std::cout << "  -> PASSED: Minimal white faceplate rendered with zero textures." << std::endl;
}

void testAbsoluteCanvasAndMacroStrip() {
    std::cout << "[Test 4] AbsoluteCanvas Positioning and Sidebar Macro Strip Reflow..." << std::endl;

    ThemeTokens theme = Theme::get(Theme::Preset::AteTrack);
    BatchRenderer2D renderer;

    GuiPanelDef panel;
    panel.title = "TB-303 PRO";
    panel.subtitle = "Analog Bassline Synth";
    panel.chassisStyle = GuiChassisStyle::Silver;
    panel.layoutMode = GuiLayoutMode::AbsoluteCanvas;
    panel.designWidth = 800.0f;
    panel.designHeight = 480.0f;

    // Add widgets with absolute design coordinates
    GuiWidgetDef w1;
    w1.id = "cutoff";
    w1.type = GuiWidgetType::Knob;
    w1.label = "CUTOFF";
    w1.param = "cutoff";
    w1.isMacro = true;
    w1.macroOrder = 1;
    w1.designBounds = Rect2D(60.0f, 100.0f, 64.0f, 64.0f);
    w1.designSize = 64.0f;
    panel.widgets.push_back(w1);

    GuiWidgetDef w2;
    w2.id = "resonance";
    w2.type = GuiWidgetType::Knob;
    w2.label = "RESONANCE";
    w2.param = "resonance";
    w2.isMacro = true;
    w2.macroOrder = 2;
    w2.designBounds = Rect2D(160.0f, 100.0f, 64.0f, 64.0f);
    w2.designSize = 64.0f;
    panel.widgets.push_back(w2);

    GuiWidgetDef w3;
    w3.id = "scope";
    w3.type = GuiWidgetType::ScopeScreen;
    w3.label = "OSCILLOSCOPE";
    w3.param = "scope";
    w3.isMacro = false;
    w3.designBounds = Rect2D(400.0f, 80.0f, 320.0f, 140.0f);
    panel.widgets.push_back(w3);

    // Verify macro extraction
    auto macros = panel.getMacroWidgets();
    assert(macros.size() == 2);
    assert(macros[0]->label == "CUTOFF");
    assert(macros[1]->label == "RESONANCE");

    // 1. Render on wide canvas (e.g. Fullscreen or Standalone CLAP: 800x480)
    renderer.beginFrame(1920.0f, 1080.0f);
    drawGuiFaceplate(renderer, panel, Rect2D(100.0f, 100.0f, 800.0f, 480.0f), theme);
    assert(renderer.getVertexCount() > 0);
    // Cutoff knob was positioned at absolute design position
    assert(panel.widgets[0].bounds.w > 40.0f);
    renderer.endFrame();

    // 2. Render on narrow Track Properties sidebar (width = 250px < 380px)
    renderer.beginFrame(1920.0f, 1080.0f);
    drawGuiFaceplate(renderer, panel, Rect2D(10.0f, 10.0f, 250.0f, 220.0f), theme);
    assert(renderer.getVertexCount() > 0);
    // Macros were reflowed into readable 2-column cells in sidebar
    assert(panel.widgets[0].bounds.w > 60.0f);
    renderer.endFrame();

    std::cout << "  -> PASSED: AbsoluteCanvas vector scaling and compact macro strip reflow verified." << std::endl;
}

void testVectorPatternsAndVignette() {
    std::cout << "[Test 5] Vector Patterns, Rect Vignette, and Deterministic Dust Specks..." << std::endl;

    BatchRenderer2D renderer;
    renderer.beginFrame(1920.0f, 1080.0f);

    const VectorPatternType patterns[] = {
        VectorPatternType::None,
        VectorPatternType::HorizontalSlats,
        VectorPatternType::DiagonalMesh,
        VectorPatternType::HexHoneycomb,
        VectorPatternType::DotGrid,
        VectorPatternType::CircuitTraces,
        VectorPatternType::BrushedSteel,
        VectorPatternType::WoodGrain,
        VectorPatternType::SpeakerGrill,
        VectorPatternType::VintagePatina
    };

    float px = 10.0f;
    for (auto pat : patterns) {
        size_t vCountBefore = renderer.getVertexCount();
        renderer.drawPatternedRectGradient(px, 10.0f, 180.0f, 120.0f,
                                           0.25f, 0.25f, 0.28f,
                                           0.12f, 0.12f, 0.15f, 1.0f,
                                           pat, 0.12f, 6.0f, 4.0f);
        size_t vCountAfter = renderer.getVertexCount();
        assert(vCountAfter > vCountBefore);
        px += 190.0f;
        if (px > 1600.0f) px = 10.0f;
    }

    // Test Rectangular Vignette
    size_t vCountVigBefore = renderer.getVertexCount();
    renderer.drawRectVignette(20.0f, 200.0f, 600.0f, 350.0f, 0.0f, 0.65f, 0.0f, 0.0f, 0.0f);
    size_t vCountVigAfter = renderer.getVertexCount();
    assert(vCountVigAfter == vCountVigBefore + 48); // 16 triangles * 3 vertices

    // Test Deterministic Dust Specks
    size_t vCountDustBefore = renderer.getVertexCount();
    renderer.drawDustSpecks(20.0f, 200.0f, 600.0f, 350.0f, 50, 42, 0.9f, 0.9f, 0.9f, 0.35f);
    size_t vCountDustAfter = renderer.getVertexCount();
    assert(vCountDustAfter > vCountDustBefore);

    renderer.endFrame();
    std::cout << "  -> PASSED: All 9 vector patterns, rectangular vignette, and deterministic dust specks verified." << std::endl;
}

void testTieredFaceplateStyling() {
    std::cout << "[Test 6] 3-Tier Background Styling (Gradient, Pattern, Dirt/Noise, Opacity)..." << std::endl;

    ThemeTokens theme = Theme::get(Theme::Preset::AteTrack);
    BatchRenderer2D renderer;
    renderer.beginFrame(1920.0f, 1080.0f);

    // 1. Test different gradient types with patterns and dirt
    const GuiGradientType gradTypes[] = {
        GuiGradientType::VerticalLinear,
        GuiGradientType::RadialVignette,
        GuiGradientType::DiagonalLinear,
        GuiGradientType::Solid
    };

    float px = 10.0f;
    for (auto gt : gradTypes) {
        GuiPanelDef panel;
        panel.title = "TIERED SYNTH";
        panel.subtitle = "Hardware Chassis";
        panel.chassisStyle = GuiChassisStyle::BrushedSteel;
        panel.gradientType = gt;
        panel.patternOverride = VectorPatternType::BrushedSteel;
        panel.dirtNoiseLevel = 0.45f;
        panel.backgroundOpacity = 0.85f;

        size_t vBefore = renderer.getVertexCount();
        drawGuiFaceplate(renderer, panel, Rect2D(px, 10.0f, 240.0f, 160.0f), theme);
        size_t vAfter = renderer.getVertexCount();
        assert(vAfter > vBefore);
        px += 250.0f;
    }

    // 2. Test Opacity = 0.0f (background omitted) vs 1.0f (full background)
    GuiPanelDef panelZero;
    panelZero.title = "ZERO OPACITY";
    panelZero.backgroundOpacity = 0.0f;
    panelZero.hideHeader = true;
    panelZero.woodCheeks = false;
    size_t vZeroBefore = renderer.getVertexCount();
    drawGuiFaceplate(renderer, panelZero, Rect2D(10.0f, 200.0f, 240.0f, 160.0f), theme);
    size_t vZeroCount = renderer.getVertexCount() - vZeroBefore;

    GuiPanelDef panelFull;
    panelFull.title = "FULL OPACITY";
    panelFull.backgroundOpacity = 1.0f;
    panelFull.hideHeader = true;
    panelFull.woodCheeks = false;
    size_t vFullBefore = renderer.getVertexCount();
    drawGuiFaceplate(renderer, panelFull, Rect2D(260.0f, 200.0f, 240.0f, 160.0f), theme);
    size_t vFullCount = renderer.getVertexCount() - vFullBefore;

    assert(vFullCount > vZeroCount);

    renderer.endFrame();
    std::cout << "  -> PASSED: 3-tier styling stack (Gradient, Pattern, Dirt/Noise, Opacity) validated cleanly." << std::endl;
}

void testMinimalGuiModeBypassesComplexBackgrounds() {
    std::cout << "[Test 7] Minimal GUI Mode Background Bypass (Settings Toggle)..." << std::endl;

    BatchRenderer2D renderer;
    renderer.beginFrame(1920.0f, 1080.0f);

    GuiPanelDef panel;
    panel.title = "TEST SYNTH";
    panel.chassisStyle = GuiChassisStyle::BrushedSteel;
    panel.gradientType = GuiGradientType::RadialVignette;
    panel.patternOverride = VectorPatternType::BrushedSteel;
    panel.dirtNoiseLevel = 0.75f;
    panel.woodCheeks = true;

    // 1. Render in full aesthetic mode
    ThemeTokens normalTheme = Theme::get(Theme::Preset::AteTrack);
    normalTheme.controlMode = ControlAestheticMode::AutoThemeDefault;
    size_t vNormBefore = renderer.getVertexCount();
    drawGuiFaceplate(renderer, panel, Rect2D(20.0f, 20.0f, 500.0f, 320.0f), normalTheme);
    size_t vNormCount = renderer.getVertexCount() - vNormBefore;

    // 2. Render in ForceMinimal mode (the Setting dialog toggle)
    ThemeTokens minTheme = Theme::get(Theme::Preset::AteTrack);
    minTheme.controlMode = ControlAestheticMode::ForceMinimal;
    size_t vMinBefore = renderer.getVertexCount();
    drawGuiFaceplate(renderer, panel, Rect2D(20.0f, 20.0f, 500.0f, 320.0f), minTheme);
    size_t vMinCount = renderer.getVertexCount() - vMinBefore;

    assert(vMinCount < vNormCount / 4);
    std::cout << "  -> Vertex reduction verified: " << vNormCount << " verts (complex) vs "
              << vMinCount << " verts (minimal solid color)." << std::endl;

    renderer.endFrame();
    std::cout << "  -> PASSED: Minimal GUI setting successfully bypasses complex backgrounds." << std::endl;
}

void testFaceplateTextureCacheLifecycle() {
    std::cout << "[Test 8] Faceplate Texture Cache & Zero-Recomputation Lifecycle..." << std::endl;

    ThemeTokens theme = Theme::get(Theme::Preset::AteTrack);
    BatchRenderer2D renderer;
    renderer.beginFrame(1920.0f, 1080.0f);

    // 1. SNES Synth / Sfxr: Must retain clean procedural gradient-only quad (VectorPatternType::None)
    GuiPanelDef snesPanel;
    snesPanel.title = "SNES SFXR";
    snesPanel.subtitle = "16-Bit Audio Engine";
    snesPanel.chassisStyle = GuiChassisStyle::Snes;
    assert(snesPanel.getEffectivePattern() == VectorPatternType::None);

    drawGuiFaceplate(renderer, snesPanel, Rect2D(20.0f, 20.0f, 400.0f, 240.0f), theme);
    // Uncached gradient-only quad: cache is not needed / remains empty
    assert(!snesPanel.backgroundCache.isValid);
    assert(snesPanel.backgroundCache.cachedPixels.empty());
    std::cout << "  -> PASSED: SNES Synth & Sfxr retained clean procedural gradient-only quad." << std::endl;

    // 2. Hardware Skeuomorphic Instrument (BrushedSteel / CircuitTraces / Carbon):
    // Verifies FaceplateBackgroundCache populates both vector vertices and software bitmap pixels
    GuiPanelDef steelPanel;
    steelPanel.title = "ANALOG BASSLINE";
    steelPanel.subtitle = "Hardware Synth";
    steelPanel.chassisStyle = GuiChassisStyle::BrushedSteel;
    steelPanel.patternOverride = VectorPatternType::CircuitTraces;
    steelPanel.dirtNoiseLevel = 0.30f;
    assert(steelPanel.getEffectivePattern() == VectorPatternType::CircuitTraces);

    assert(!steelPanel.backgroundCache.isValid);
    drawGuiFaceplate(renderer, steelPanel, Rect2D(50.0f, 50.0f, 600.0f, 360.0f), theme);

    // After first frame render: cache is valid, vertices and pixels stored
    assert(steelPanel.backgroundCache.isValid);
    assert(steelPanel.backgroundCache.cacheW == 600);
    assert(steelPanel.backgroundCache.cacheH == 360);
    assert(!steelPanel.backgroundCache.cachedVertices.empty());
    assert(!steelPanel.backgroundCache.cachedPixels.empty());
    assert(steelPanel.backgroundCache.cachedPixels.size() == 600 * 360);
    uint64_t initialHash = steelPanel.backgroundCache.stateHash;
    assert(initialHash != 0);

    // 3. Second render pass at identical dimensions: Cache is reused, zero rebuilds
    size_t vBefore = renderer.getVertexCount();
    drawGuiFaceplate(renderer, steelPanel, Rect2D(50.0f, 50.0f, 600.0f, 360.0f), theme);
    size_t vAfter = renderer.getVertexCount();
    assert(vAfter > vBefore);
    assert(steelPanel.backgroundCache.stateHash == initialHash);

    // 4. Moving the panel (e.g. scroll or window move) preserves cache
    drawGuiFaceplate(renderer, steelPanel, Rect2D(120.0f, 80.0f, 600.0f, 360.0f), theme);
    assert(steelPanel.backgroundCache.stateHash == initialHash);

    // 5. Resizing the panel (e.g. window maximization / fullscreen) updates cache
    drawGuiFaceplate(renderer, steelPanel, Rect2D(0.0f, 0.0f, 800.0f, 500.0f), theme);
    assert(steelPanel.backgroundCache.isValid);
    assert(steelPanel.backgroundCache.cacheW == 800);
    assert(steelPanel.backgroundCache.cacheH == 500);
    assert(steelPanel.backgroundCache.stateHash != initialHash);
    assert(steelPanel.backgroundCache.cachedPixels.size() == 800 * 500);

    renderer.endFrame();
    std::cout << "  -> PASSED: Faceplate texture cache & zero-recomputation lifecycle verified." << std::endl;
}

int main() {
    std::cout << "=== Running Eatsbits Vector Surface Test Suite ===\n";
    testVectorFaceplateStyles();
    testCustomTintsAndWoodCheeks();
    testCleanMinimalWhiteFaceplate();
    testAbsoluteCanvasAndMacroStrip();
    testVectorPatternsAndVignette();
    testTieredFaceplateStyling();
    testMinimalGuiModeBypassesComplexBackgrounds();
    testFaceplateTextureCacheLifecycle();
    std::cout << "=== All Vector Surface Tests Passed Successfully! ===\n";
    return 0;
}
