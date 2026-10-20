#pragma once

#include "geometry.hpp"
#include "theme.hpp"
#include "batch_renderer_2d.hpp"
#include "draw_utils.hpp"
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
    Divider,
    ListBox
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
    MiniPotCream,
    DaviesFluted,
    RoganMoog,
    ChickenHead,
    PastelDimple,
    ModernEncoder,
    CustomShape,
    StudioClean,
    StudioRuby,
    StudioMagenta,
    StudioCyan,
    StudioDark,
    StudioBipolar
};

enum class KnobBodyShape : uint8_t {
    Round = 0,           // Standard cylindrical smooth rotor
    FlutedScalloped = 1, // Rogan / Davies 1900H style continuous scalloped ridges
    RibbedKnurled = 2,   // Fine micro-flutes / knurled grip teeth
    ChickenHead = 3,     // Asymmetrical vintage pointer wing / beak
    FacetedPolygon = 4,  // Hexagonal, octagonal, or dodecagonal faceted knob
    SkirtedVintage = 5   // Flared base skirt + fluted rotor with graduation markings
};

enum class KnobCapStyle : uint8_t {
    Flat = 0,            // Clean minimalist / matte cap
    LatheMetallic = 1,   // Brushed concentric lathe machining rings with specular highlight
    Domed = 2,           // Spherical 3D highlight
    IndentedDimple = 3,  // Soft fingertip dish / center dimple (e.g. Joni style)
    InlayDisc = 4,       // Contrasting color or metallic inlay disc
    DeclinedScoop = 5    // Authentic Roland TB-303 sloped wedge scoop with integrated etched pointer notch
};

enum class SwitchStyle : uint8_t {
    VintageBatLever = 0, // Classic gold/brass collar & ball-tipped lever
    RectangularSlide = 1 // Authentic hardware rectangular slide switch with corrugated thumb
};

enum class SwitchOrientation : uint8_t {
    Vertical = 0,
    Horizontal = 1
};

enum class SwitchSymbol : uint8_t {
    None = 0,
    SquareWave = 1,
    SawWave = 2,
    SineWave = 3,
    TriangleWave = 4,
    Power = 5
};

enum class KnobIndicatorType : uint8_t {
    LineGroove = 0,      // Molded or painted line extending across cap and body
    DotPip = 1,          // Circular pip or jewel marker near rim
    PointerNotch = 2,    // Physical notch cutout on skirt / perimeter edge
    PointerNose = 3,     // Triangular nose point (e.g. chicken head tip)
    None = 4
};

struct KnobShapeDef {
    KnobBodyShape shape{KnobBodyShape::FlutedScalloped};
    int flutes{12};               // Scallop or facet count (e.g. 6, 8, 12, 18, 24)
    float fluteDepth{0.14f};      // Depth of flutes relative to radius [0.0, 0.35]
    float cornerRadius{1.0f};     // Softness/bevel of facet corners
    bool hasSkirt{false};         // Flared base skirt underneath
    float skirtRatio{1.25f};      // Skirt radius relative to knob radius
    int skirtTicks{0};            // Graduation ticks printed on skirt (e.g. 11 ticks)
    KnobCapStyle capStyle{KnobCapStyle::LatheMetallic};
    float capRatio{0.72f};        // Cap radius relative to body
    KnobIndicatorType indicator{KnobIndicatorType::LineGroove};
    float indicatorLength{0.80f}; // Fraction of radius
    float indicatorWidth{2.2f};   // Width in pixels
    Color bodyColor{0.16f, 0.17f, 0.20f, 1.0f};    // Main knob body color
    Color capColor{0.82f, 0.84f, 0.88f, 1.0f};     // Cap / inlay color
    Color indicatorColor{0.0f, 0.95f, 1.0f, 1.0f}; // Needle / glow color
    bool showHaloArc{true};                        // LED progress arc around dial
    float sunkenSlotWidth{0.0f};                  // Faceplate recessed mounting gutter width (0.0f = unrendered)
    Color sunkenSlotColor{0.10f, 0.11f, 0.13f, 1.0f}; // Recessed trench color
    std::vector<std::string> dialLabels;          // Custom perimeter graduations (any string array, e.g. {"0".."10"}, {"low", "mid", "high"})
    int dialTicks{0};                             // Graduation tick count (0 = auto from dialLabels)
    bool hasCenterDetent{false};                  // Center detent tick at 12 o'clock
    bool hasBlockCenterDetent{false};             // TB-303 authentic bold calibration block at 12 o'clock
    float dialLabelDistanceRatio{1.28f};          // Radius multiplier for text label placement
    float dialLabelFontSize{7.5f};                // Base font size for dial labels
    std::optional<Color> dialLabelColor{std::nullopt};
    std::optional<Color> dialTickColor{std::nullopt};
    std::optional<Color> trackColor{std::nullopt};
    bool isLightRotor{false};
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
    std::vector<std::string> options;
    int selectedIndex{0};
    float scrollOffset{0.0f};
    std::string action; // e.g. "randomize"
    std::optional<KnobShapeDef> customShape{std::nullopt};
    float step{0.0f}; // 0.0f = continuous; > 0.0f = discrete step increment (e.g. 1.0f for octaves 0..10)
    SwitchStyle switchStyle{SwitchStyle::VintageBatLever};
    SwitchOrientation switchOrientation{SwitchOrientation::Vertical};
    SwitchSymbol switchSymbolTop{SwitchSymbol::None};
    SwitchSymbol switchSymbolBottom{SwitchSymbol::None};
    std::vector<std::string> dialLabels;          // Optional per-widget dial labels override
    float sunkenSlotWidth{0.0f};                  // Optional per-widget sunken slot width override (0.0f = unrendered)
    KnobLayoutMode labelAlignment{KnobLayoutMode::StackedVertical}; // Label/value alignment configuration
    std::string sublabel;                         // Secondary label (e.g. "ANDALL", "12AX7", "FILTER", "7-UNISON")
    bool showValueBox{false};                     // Skeuomorphic dark pill container box for value readout
    std::string valueDisplay;                     // Explicit formatted string override
    float defaultVal{0.5f};

    [[nodiscard]] std::string getFormattedValue() const;
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
    bool isClapPlugin{false};
    float cornerRadius{8.0f};
    GuiLayoutMode layoutMode{GuiLayoutMode::AutoRowFlow};
    float designWidth{800.0f};
    float designHeight{480.0f};
    std::vector<GuiWidgetDef> widgets; // For AbsoluteCanvas layout
    std::vector<GuiRowDef> rows;       // For AutoRowFlow layout
    Rect2D bounds{0.0f, 0.0f, 0.0f, 0.0f};

    void layoutControls(const Rect2D& rect);

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
                      int draggingWidget = -1,
                      const std::string& focusedWidgetId = "");

/**
 * Returns default KnobShapeDef corresponding to a GuiKnobStyle.
 */
KnobShapeDef getKnobShapeDef(GuiKnobStyle style, const Color& accent = Color(0.0f, 0.95f, 1.0f));

/**
 * High-performance vector skeuomorphic knob renderer.
 * Handles continuous fluted contours, chicken-head pointers, faceted polygons,
 * skirts with graduation ticks, 3D lathe caps, and glowing indicator needle/grooves.
 */
void drawKnobWidget(BatchRenderer2D& r,
                    float cx, float cy, float radius, float normVal,
                    const GuiWidgetDef& w,
                    const LightSource2D& light,
                    bool isActive = false, bool isHovered = false,
                    bool isLightChassis = false,
                    const ThemeTokens* theme = nullptr);

} // namespace eatsbits::ui
