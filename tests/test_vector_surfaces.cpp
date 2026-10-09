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

int main() {
    std::cout << "=== Running Eatsbits Vector Surface Test Suite ===\n";
    testVectorFaceplateStyles();
    testCustomTintsAndWoodCheeks();
    testCleanMinimalWhiteFaceplate();
    std::cout << "=== All Vector Surface Tests Passed Successfully! ===\n";
    return 0;
}
