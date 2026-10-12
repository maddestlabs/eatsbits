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
    ScopeScreen,
    Divider
};

enum class GuiKnobStyle {
    CreamFluted,
    BakeliteSkirt,
    AnodizedKnurled,
    TwoToneStepped,
    Tb303Halo,
    Standard,
    Tb303Potentiometer,
    Tb303SelectorSilver,
    Tb303SelectorBlack,
    MiniPotCream
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
    BrushedAluminum,
    BrushedMetal = BrushedAluminum,
    MattePowderCoat,
    Bakelite,
    CrinklePaint,
    Carbon
};

enum class GuiLayoutMode : uint8_t {
    AutoRowFlow = 0,
    AbsoluteCanvas = 1
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
    bool isMacro{false};
    uint8_t macroOrder{0};
    Rect2D designBounds{0.0f, 0.0f, 0.0f, 0.0f};
    float designSize{56.0f};
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
    std::optional<Color> chassisTint{std::nullopt};
    float textureWear{0.20f};
    float chassisRotation{0.0f};
    bool woodCheeks{true};
    bool hideHeader{false};
    float cornerRadius{8.0f};
    GuiLayoutMode layoutMode{GuiLayoutMode::AutoRowFlow};
    float designWidth{800.0f};
    float designHeight{480.0f};
    std::vector<GuiWidgetDef> widgets; // For AbsoluteCanvas layout
    std::vector<GuiRowDef> rows;       // For AutoRowFlow layout
    Rect2D bounds{0.0f, 0.0f, 0.0f, 0.0f};

    [[nodiscard]] std::vector<const GuiWidgetDef*> getMacroWidgets() const noexcept {
        std::vector<const GuiWidgetDef*> res;
        if (layoutMode == GuiLayoutMode::AbsoluteCanvas) {
            for (const auto& w : widgets) {
                if (w.isMacro) res.push_back(&w);
            }
        } else {
            for (const auto& row : rows) {
                for (const auto& w : row.widgets) {
                    if (w.isMacro) res.push_back(&w);
                }
            }
        }
        return res;
    }

    [[nodiscard]] std::vector<GuiWidgetDef*> getMacroWidgets() noexcept {
        std::vector<GuiWidgetDef*> res;
        if (layoutMode == GuiLayoutMode::AbsoluteCanvas) {
            for (auto& w : widgets) {
                if (w.isMacro) res.push_back(&w);
            }
        } else {
            for (auto& row : rows) {
                for (auto& w : row.widgets) {
                    if (w.isMacro) res.push_back(&w);
                }
            }
        }
        return res;
    }
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
