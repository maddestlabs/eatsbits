#pragma once

#include "view_base.hpp"
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

    void setSubMode(DesignSubMode mode) noexcept;
    [[nodiscard]] DesignSubMode getSubMode() const noexcept { return mode_; }
    [[nodiscard]] const std::vector<PatchCord>& getPatchCords() const noexcept { return patchCords_; }

    // Target Selection & Script Management
    void selectTargetByIndex(int index);
    void selectTargetById(const std::string& id);
    [[nodiscard]] const ScriptTarget& getActiveTarget() const;
    [[nodiscard]] const std::vector<ScriptTarget>& getAllTargets() const noexcept { return allTargets_; }
    [[nodiscard]] const std::string& getScriptCode() const noexcept { return currentScriptCode_; }
    void setScriptCode(const std::string& code);
    void compileCurrentScript(const ViewContext& ctx);

    // Callbacks to host window/engine
    std::function<void(const std::string& targetId, const std::string& paramName, float val)> onParamChanged;
    std::function<void(const std::string& targetId, const std::string& code)> onCompileScript;
    std::function<void(const std::string& text)> onCopyToClipboard;

    void setAudioScopeBuffer(const float* buffer, size_t count);

private:
    void initDefaultTargetsAndCode();
    void updateActiveTargetCodeAndParams();

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
    int cursorLine_{0};
    int cursorCol_{0};
    bool isCompiled_{true};
    std::string statusMsg_{"Compiled successfully (Eatscript Live Engine) Active parameters: 5"};
    std::string errorMsg_{""};

    // Scope audio waveform
    std::vector<float> scopeSamples_;

    // Modular rack state
    std::vector<ModularModuleDef> modules_;
    std::vector<PatchCord> patchCords_;
    bool isDraggingCord_{false};
    int dragCordSrcMod_{-1};
    int dragCordSrcJack_{-1};
    float cordStartX_{0.0f};
    float cordStartY_{0.0f};
    float cordCurrentX_{0.0f};
    float cordCurrentY_{0.0f};
    int draggingModuleKnobMod_{-1};
    int draggingModuleKnobIdx_{-1};
    float knobDragStartY_{0.0f};
    float knobDragStartVal_{0.0f};

    // GUI Designer state
    int selectedDesignerRow_{-1};
    int selectedDesignerWidget_{-1};
    Rect2D btnAddRow_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnRemoveRow_{0.0f, 0.0f, 0.0f, 0.0f};

    // Target Dropdown Item Rects
    std::vector<Rect2D> targetDropdownItemBounds_;
    // Explorer Item Rects
    std::vector<std::pair<int, Rect2D>> explorerItemBounds_;
};

} // namespace eatsbits::ui
