#pragma once

#include "geometry.hpp"
#include "theme.hpp"
#include "batch_renderer_2d.hpp"
#include <string>
#include <vector>

namespace eatsbits::ui {

// --- GUI Designer & Skeuomorphic Faceplate Types ---
enum class GuiWidgetType {
    Knob,
    Slider,
    ToggleSwitch,
    NixieDisplay,
    VuMeter,
    ActionButton,
    ScopeScreen
};

enum class GuiKnobStyle {
    CreamFluted,
    BakeliteSkirt,
    AnodizedKnurled,
    TwoToneStepped,
    Tb303Halo,
    Standard
};

enum class GuiChassisStyle {
    DarkChassis,
    PcbGreen,
    MinimalWhite,
    Silver,
    Snes,
    Grunge,
    Walnut,
    Rosewood,
    BrushedSteel,
    Carbon
};

struct GuiWidgetDef {
    std::string id;
    GuiWidgetType type{GuiWidgetType::Knob};
    std::string label{"CUTOFF"};
    std::string param{"Cutoff"};
    GuiKnobStyle knobStyle{GuiKnobStyle::CreamFluted};
    float size{56.0f};
    float currentVal{0.5f};
    float minVal{0.0f};
    float maxVal{1.0f};
    std::string unit{"Hz"};
    Color accentColor{0.0f, 0.95f, 1.0f};
    Rect2D bounds{0.0f, 0.0f, 0.0f, 0.0f};
};

struct GuiRowDef {
    std::vector<GuiWidgetDef> widgets;
    Rect2D bounds{0.0f, 0.0f, 0.0f, 0.0f};
};

struct GuiPanelDef {
    std::string title{"Eats-303 Synthesizer"};
    std::string subtitle{"Analog Diode Ladder Bassline"};
    GuiChassisStyle chassisStyle{GuiChassisStyle::DarkChassis};
    Color accentColor{0.0f, 0.95f, 1.0f};
    bool woodCheeks{true};
    float cornerRadius{8.0f};
    std::vector<GuiRowDef> rows;
    Rect2D bounds{0.0f, 0.0f, 0.0f, 0.0f};
};

struct PaletteItemDef {
    std::string id;
    std::string title;
    std::string category;
    GuiWidgetType type{GuiWidgetType::Knob};
    GuiKnobStyle knobStyle{GuiKnobStyle::CreamFluted};
    Rect2D bounds{0.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * Unified GUI Faceplate Renderer
 *
 * Renders an authentic skeuomorphic hardware faceplate matching DesignView & Eatsbeats:
 * - Vintage wooden end cheeks (left/right) with bevel grain lines
 * - Chassis background styled according to GuiChassisStyle (Silver, Grunge, Walnut, DarkChassis, etc.)
 * - 4 corner hex mounting screws with specular chamfers and slots
 * - Top machined header strip with track glowing LED lamp, panel title, and subtitle
 * - Multi-row widget layout supporting 3D radial 3-stop gradient knobs (CreamFluted, BakeliteSkirt,
 *   AnodizedKnurled, TwoToneStepped, Tb303Halo, Standard), sliders, tactile toggle switches,
 *   nixie tube displays, VU meters, and oscilloscope screens.
 */
void drawGuiFaceplate(BatchRenderer2D& r,
                      GuiPanelDef& panel,
                      const Rect2D& rect,
                      const ThemeTokens& theme,
                      const float* scopeBuffer = nullptr,
                      size_t scopeBufferCount = 0,
                      int draggingRow = -1,
                      int draggingWidget = -1);

} // namespace eatsbits::ui
