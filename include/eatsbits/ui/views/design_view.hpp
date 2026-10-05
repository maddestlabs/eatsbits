#pragma once

#include "view_base.hpp"
#include "../gui_panel_def.hpp"
#include "../widgets/text_editor_widget.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace eatsbits::ui {

enum class DesignSubMode {
    Code = 0,
    Eatscript = 0, // Backward compatibility with unit tests
    ModularRack = 1,
    Split = 2,
    GuiDesigner = 3,
    GuiPreview = 3 // Backward compatibility with unit tests
};

enum class ScriptTargetType {
    TrackDsp,
    AudioFx,
    MidiFx,
    ClipScript,
    BuiltinPreset,
    ProjectAction
};

struct ScriptTarget {
    std::string id;
    std::string title;
    std::string subtitle;
    ScriptTargetType type{ScriptTargetType::TrackDsp};
    std::string typeBadge{"SYNTH DSP"};
    Color badgeColor{0.0f, 0.95f, 1.0f}; // Cyan
    Color trackColor{1.0f, 0.55f, 0.0f};
    int trackIndex{-1};
    int clipIndex{-1};
};

struct ScriptParamDef {
    std::string name;
    float minVal{0.0f};
    float maxVal{1.0f};
    float defaultVal{0.5f};
    float currentVal{0.5f};
    std::string unit{""};
    Rect2D bounds{0.0f, 0.0f, 0.0f, 0.0f};
};

struct PatchCord {
    int sourceModule{0};
    int sourceJack{0};
    int destModule{0};
    int destJack{0};
    float r{1.0f}, g{0.55f}, b{0.0f};
};

struct ModularKnobDef {
    std::string label;
    float value{0.5f};
    float minVal{0.0f};
    float maxVal{1.0f};
    std::string unit;
};

struct ModularModuleDef {
    std::string title;
    std::string type;
    float x{0.0f}, y{0.0f}, w{200.0f}, h{280.0f};
    float r{0.20f}, g{0.22f}, b{0.26f};
    std::vector<ModularKnobDef> knobs;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
};

// --- GUI Designer Types are declared in eatsbits/ui/gui_panel_def.hpp ---


/**
 * DesignView: High-fidelity Design Workbench for Eatsbits.
 * Aligned with original Eatsbeats: Code Editor with Live Parameters & Oscilloscope,
 * Eurorack Modular rack with physics catenary cables, Split view, and Skeuomorphic
 * GUI Interface with integrated visual GUI designer.
 */
class DesignView : public ViewBase {
public:
    DesignView();
    ~DesignView() override = default;

    void layout(const Rect2D& bounds, const ViewContext& ctx) override;
    void render(const ViewContext& ctx) override;
    bool handlePointer(const PointerEvent& ev, const ViewContext& ctx) override;
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) override;
    bool handleChar(char32_t codepoint, const ViewContext& ctx) override;

    [[nodiscard]] TextEditorWidget& getTextEditor() noexcept { return textEditor_; }
    [[nodiscard]] const TextEditorWidget& getTextEditor() const noexcept { return textEditor_; }

    void setSubMode(DesignSubMode mode) noexcept;
    [[nodiscard]] DesignSubMode getSubMode() const noexcept { return mode_; }
    [[nodiscard]] const std::vector<PatchCord>& getPatchCords() const noexcept { return patchCords_; }
    [[nodiscard]] const std::vector<ModularModuleDef>& getModules() const noexcept { return modules_; }
    void addModule(const ModularModuleDef& mod);
    void resetPatch();

    // Target Selection & Script Management
    void selectTargetByIndex(int index);
    void selectTargetById(const std::string& id);
    void selectTargetByTrackAndType(int trackIndex, ScriptTargetType type);
    [[nodiscard]] const ScriptTarget& getActiveTarget() const;
    [[nodiscard]] const std::vector<ScriptTarget>& getAllTargets() const noexcept { return allTargets_; }
    [[nodiscard]] const std::string& getScriptCode() const noexcept { return currentScriptCode_; }
    void setScriptCode(const std::string& code);
    void compileCurrentScript(const ViewContext& ctx);

    // GUI Designer methods
    [[nodiscard]] const GuiPanelDef& getGuiPanel() const noexcept { return guiPanel_; }
    [[nodiscard]] GuiPanelDef& getGuiPanel() noexcept { return guiPanel_; }
    void setGuiDesignMode(bool designMode) noexcept { isGuiDesignMode_ = designMode; }
    [[nodiscard]] bool isGuiDesignMode() const noexcept { return isGuiDesignMode_; }
    void addGuiRow();
    void deleteGuiRow(int rowIndex);
    void addGuiWidget(GuiWidgetType type, GuiKnobStyle knobStyle = GuiKnobStyle::CreamFluted,
                      const std::string& label = "", const std::string& param = "");
    void deleteSelectedGuiWidget();
    void duplicateSelectedGuiWidget();
    void selectDesignerWidget(int row, int widget);
    void selectDesignerChassis();
    [[nodiscard]] int getSelectedDesignerRow() const noexcept { return selectedDesignerRow_; }
    [[nodiscard]] int getSelectedDesignerWidget() const noexcept { return selectedDesignerWidget_; }
    [[nodiscard]] bool isChassisSelected() const noexcept { return isChassisSelected_; }

    // Callbacks to host window/engine
    std::function<void(const std::string& targetId, const std::string& paramName, float val)> onParamChanged;
    std::function<void(const std::string& targetId, const std::string& code)> onCompileScript;
    std::function<void(const std::string& text)> onCopyToClipboard;

    void setAudioScopeBuffer(const float* buffer, size_t count);

private:
    void initDefaultTargetsAndCode();
    void updateActiveTargetCodeAndParams();
    void initDefaultGuiPanel();
    void syncGuiPanelToScript();

    void renderSubNavHeader(const ViewContext& ctx);
    void renderExplorerSidebar(const ViewContext& ctx);
    void renderCodeEditorCanvas(const ViewContext& ctx);
    void renderOscilloscope(const ViewContext& ctx, const Rect2D& rect);
    void renderModularRack(const ViewContext& ctx, const Rect2D& rect);
    void renderSplitView(const ViewContext& ctx);
    void renderGuiPreview(const ViewContext& ctx);
    void renderHardwareFaceplate(const ViewContext& ctx, const Rect2D& rect);
    void renderGuiDesigner(const ViewContext& ctx, const Rect2D& rect);
    void renderCatenaryCable(BatchRenderer2D& r, float x1, float y1, float x2, float y2, float cr, float cg, float cb);

    // Mode state
    DesignSubMode mode_{DesignSubMode::ModularRack}; // Default to ModularRack for test compatibility
    bool isExplorerOpen_{true};
    std::string scriptFilterQuery_{""};
    bool isTargetDropdownOpen_{false};
    bool splitShowsGui_{true}; // In split view: true = faceplate, false = modular rack
    bool isGuiDesignMode_{false}; // In GUI tab: false = live interaction, true = design mode

    // Layout bounds
    Rect2D headerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D explorerToggleBtn_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D targetBadgeBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D targetDropdownBtn_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D targetDropdownMenuBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnCode_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnModular_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnSplit_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnGui_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnCompile_{0.0f, 0.0f, 0.0f, 0.0f};

    Rect2D contentBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D explorerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D studioBodyBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Code & Editor Bounds
    Rect2D scopeBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D editorHeaderBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnSelectAll_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnCopy_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnSubmitPr_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnApiDocs_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D editorCanvasBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D statusBannerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D liveParamsSectionBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Split / GUI Sub-bars
    Rect2D splitSubBarBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnSplitFaceplate_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnSplitModular_{0.0f, 0.0f, 0.0f, 0.0f};

    Rect2D guiSubBarBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnGuiLive_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnGuiDesign_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnGuiPr_{0.0f, 0.0f, 0.0f, 0.0f};

    // Targets & Parameters
    std::vector<ScriptTarget> allTargets_;
    int activeTargetIndex_{0};
    std::vector<ScriptParamDef> currentParams_;
    int draggingParamIndex_{-1};
    int draggingFaceplateKnobIndex_{-1};
    float lastDragY_{0.0f};

    // Script text & lines
    std::string currentScriptCode_;
    std::vector<std::string> codeLines_;
    TextEditorWidget textEditor_;
    int cursorLine_{0};
    int cursorCol_{0};
    bool isCompiled_{true};
    std::string statusMsg_{"Compiled successfully (Eatscript Live Engine) Active parameters: 5"};
    std::string errorMsg_{""};

    // Scope audio waveform
    std::vector<float> scopeSamples_;

    // Modular rack state & toolbar
    std::vector<ModularModuleDef> modules_;
    std::vector<PatchCord> patchCords_;
    Rect2D modularToolbarBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnModularResetPatch_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnModularAddModule_{0.0f, 0.0f, 0.0f, 0.0f};
    bool isDraggingCord_{false};
    int dragCordSrcMod_{-1};
    int dragCordSrcJack_{-1};
    bool dragCordSrcIsOutput_{true};
    float cordStartX_{0.0f};
    float cordStartY_{0.0f};
    float cordCurrentX_{0.0f};
    float cordCurrentY_{0.0f};
    float dragCordR_{1.0f}, dragCordG_{0.55f}, dragCordB_{0.0f};
    int draggingModuleKnobMod_{-1};
    int draggingModuleKnobIdx_{-1};
    float knobDragStartY_{0.0f};
    float knobDragStartVal_{0.0f};
    int hoveredJackMod_{-1};
    int hoveredJackIdx_{-1};
    bool hoveredJackIsOutput_{false};

    // GUI Designer state & components
    GuiPanelDef guiPanel_;
    bool isPaletteOpen_{true};
    bool isInspectorOpen_{true};
    bool isChassisSelected_{false};
    int selectedDesignerRow_{-1};
    int selectedDesignerWidget_{-1};
    int draggingGuiWidgetRow_{-1};
    int draggingGuiWidgetIdx_{-1};
    float guiDragStartY_{0.0f};
    float guiDragStartVal_{0.0f};

    // GUI Designer Toolbars & Buttons
    Rect2D designerToolbarBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnTogglePalette_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnToggleInspector_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnAddRow_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnDeleteRow_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnDuplicateWidget_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnDeleteWidget_{0.0f, 0.0f, 0.0f, 0.0f};

    std::vector<PaletteItemDef> paletteItems_;
    std::vector<std::pair<GuiChassisStyle, Rect2D>> inspectorThemeBtns_;
    std::vector<std::pair<Color, Rect2D>> inspectorAccentBtns_;
    std::vector<std::pair<std::optional<Color>, Rect2D>> inspectorChassisTintBtns_;
    Rect2D inspectorWoodCheeksBtn_{0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<std::pair<std::string, Rect2D>> inspectorParamBtns_;
    std::vector<std::pair<GuiKnobStyle, Rect2D>> inspectorKnobStyleBtns_;
    std::vector<std::pair<float, Rect2D>> inspectorSizeBtns_;
    Rect2D inspectorDeleteBtn_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D inspectorDuplicateBtn_{0.0f, 0.0f, 0.0f, 0.0f};

    // Target Dropdown Item Rects
    std::vector<Rect2D> targetDropdownItemBounds_;
    // Explorer Item Rects
    std::vector<std::pair<int, Rect2D>> explorerItemBounds_;
};

} // namespace eatsbits::ui
