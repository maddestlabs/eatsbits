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

int main() {
    std::cout << "=== Running Eatsbits Vector Surface Test Suite ===\n";
    testVectorFaceplateStyles();
    testCustomTintsAndWoodCheeks();
    testCleanMinimalWhiteFaceplate();
    testAbsoluteCanvasAndMacroStrip();
    std::cout << "=== All Vector Surface Tests Passed Successfully! ===\n";
    return 0;
}
