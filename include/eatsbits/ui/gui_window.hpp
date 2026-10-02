#ifndef EATS_GUI_WINDOW_HPP
#define EATS_GUI_WINDOW_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

#include "canvas_renderer.hpp"
#include "dawn_bridge.hpp"
#include "batch_renderer_2d.hpp"
#include "theme.hpp"
#include "geometry.hpp"
#include "score/score_glyphs.hpp"
#include "score/score_layout_engine.hpp"
#include "../audio/audio_engine.hpp"
#include "../project/preset_loader.hpp"
#include "../project/preset_manager.hpp"
#include "../project/diff_history_manager.hpp"
#include "widgets/note_selection_sidebar.hpp"
#include "widgets/piano_keyboard.hpp"
#include "../eatscript/ast.hpp"
#include "views/view_base.hpp"
#include "views/arranger_view.hpp"
#include "views/edit_view.hpp"
#include "views/track_inspector_view.hpp"
#include "views/mixer_view.hpp"
#include "views/design_view.hpp"
#include "widgets/virtual_keyboard_drawer.hpp"
#include "widgets/project_browser_drawer.hpp"
#include "widgets/transport_header.hpp"
#include "widgets/bottom_nav_bar.hpp"
#include "widgets/terminal_console_drawer.hpp"
#include "widgets/value_edit_dialog.hpp"
#include "widgets/command_palette_dialog.hpp"
#include "widgets/audio_to_midi_dialog.hpp"
#include "widgets/plugin_search_dialog.hpp"
#include "widgets/fullscreen_device_modal.hpp"
#include "widgets/scrollable_area.hpp"
#include "input/pointer_event.hpp"
#include "eatsbits/presenter/drag_types.hpp"
#include "eatsbits/presenter/drag_handler.hpp"
#include "eatsbits/presenter/scalar_drag_presenter.hpp"
#include "eatsbits/presenter/timeline_scrub_presenter.hpp"
#include "eatsbits/presenter/splitter_drag_presenter.hpp"
#include "eatsbits/presenter/cable_patch_presenter.hpp"
#include "eatsbits/presenter/marquee_select_presenter.hpp"
#include "eatsbits/presenter/telemetry_presenter.hpp"

struct GLFWwindow;

namespace eatsbits::ui {

enum class WorkspaceView {
    Arranger,
    Edit,
    Track,
    Mixer,
    Design,
    // Backwards-compatible aliases:
    ModularRack = Design,
    HardwarePanel = Track,
    Tracker = Edit,
    Eatscript = Design
};

enum class EditSubView {
    Tracker,
    PianoRoll,
    Score,
    Script
};

enum class DesignSubView {
    ModularRack,
    Eatscript,
    GuiDesigner
};

enum class BottomNavAction {
    None,
    Arranger,
    Edit,
    Track,
    Mixer,
    Design
};

struct HitTestBottomNavResult {
    bool hit{false};
    BottomNavAction action{BottomNavAction::None};
};

enum class TransportAction {
    None,
    ProjectHubToggle,
    PlayPause,
    Stop,
    Record,
    Bpm,
    Swing,
    MasterVol,
    LoopToggle,
    MetronomeToggle,
    BrowserToggle,
    SnapToggle,
    ScaleToggle,
    TabRack,
    TabHardware,
    TabTracker,
    TabMixer,
    TabEatscript,
    PresetPrev,
    PresetNext,
    Toggle3dConsole,
    ToggleCameraFocus,
    FullscreenToggle,
    LockToggle,
    SearchToggle
};

struct HitTestTransportResult {
    bool hit{false};
    TransportAction action{TransportAction::None};
};

struct HitTestHardwareKnobResult {
    bool hit{false};
    std::string paramName;
    float currentNormVal{0.0f};
    Point2D position;
};

enum class MixerSectionPreset {
    Full,
    FadersOnly,
    FadersAndMeters
};



struct HitTestTrackTabResult {
    bool hit{false};
    uint32_t trackIndex{0};
};

enum class BrowserTab {
    Presets,
    Macros,
    History
};

enum class BrowserHitAction {
    None,
    Close,
    TabPresets,
    TabMacros,
    TabHistory,
    CategorySelect,
    PresetSelect,
    PresetLoad,
    MacroRun,
    SaveProject,
    LoadProject,
    BounceMaster,
    HistoryUndo,
    HistoryRedo,
    HistoryMilestone,
    HistoryClear,
    HistoryStepSelect
};

struct HitTestBrowserResult {
    bool hit{false};
    BrowserHitAction action{BrowserHitAction::None};
    std::string category;
    size_t presetIndex{0};
    size_t macroIndex{0};
    size_t historyStepIndex{0};
};

enum class ProjectHubAction {
    None,
    Close,
    SectionHeader,
    TitleClick,
    AuthorClick,
    SaveProject,
    SaveAsProject,
    LoadProject,
    NewProject,
    BounceWav,
    OpenScriptView,
    ToggleRestoreSession,
    ToggleAutosave,
    ResetCleanSlate,
    SetUiScale,
    SelectTheme,
    ToggleCrtShader,
    ToggleAnimations,
    SetAntiAliasing,
    ToggleHiDpi,
    Scrollbar,
    ContentDrag,
    OpenCrtTweaker,
    CrtPresetStudioRef,
    CrtPresetMaxClarity,
    CrtPresetWarmVintage,
    CrtPresetReset,
    CrtSlider
};

struct DialogFrameConfig {
    float width{540.0f};
    float height{600.0f};
    std::string title{"EATSBITS SETTINGS"};
    bool showLogo{true};
    bool showCloseButton{true};
    bool showBottomClose{true};
    float cornerRadius{14.0f};
    bool enableBlur{false};
};

struct DialogLayout {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};
    float contentX{0.0f};
    float contentY{0.0f};
    float contentW{0.0f};
    float contentH{0.0f};
    float closeBtnX{0.0f};
    float closeBtnY{0.0f};
    float closeBtnW{24.0f};
    float closeBtnH{24.0f};
    float bottomCloseX{0.0f};
    float bottomCloseY{0.0f};
    float bottomCloseW{60.0f};
    float bottomCloseH{24.0f};
};

struct HitTestProjectHubResult {
    bool hit{false};
    ProjectHubAction action{ProjectHubAction::None};
    int sectionIndex{-1};
    float scaleValue{1.0f};
    int themeIndex{0};
    int aaMode{2};
    int crtSliderIndex{-1};
};

struct HitTestJackResult {
    bool hit{false};
    audio::NodeId nodeId{0};
    uint32_t portIndex{0};
    bool isOutput{false};
    Point2D position;
};

struct HitTestKnobResult {
    bool hit{false};
    audio::NodeId nodeId{0};
    uint32_t knobIndex{0};
    std::string knobName;
    Point2D position;
};

struct HitTestStepResult {
    bool hit{false};
    uint32_t trackIndex{0};
    uint32_t stepIndex{0};
};

struct HitTestEditSubNavResult {
    bool hit{false};
    EditSubView subView{EditSubView::PianoRoll};
};

struct HitTestPianoKeyResult {
    bool hit{false};
    uint8_t pitch{0}; // MIDI note number
    bool isBlackKey{false};
    float velocity{0.85f};
};

struct HitTestPianoRollScrollbarResult {
    bool hit{false};
    bool isVertical{false};
    bool isThumb{false};
    float normOffset{0.0f};
};

struct HitTestVirtualKeyboardDrawerResult {
    bool hit{false};
    bool isPullTab{false};
    bool isOctaveDown{false};
    bool isOctaveUp{false};
    bool isKey{false};
    int pitch{0};
    float velocity{0.85f};
};

struct HitTestPianoRollGridResult {
    bool hit{false};
    uint32_t step{0};
    uint8_t pitch{0};
    bool hasExistingNote{false};
};

struct HitTestTrackerResult {
    bool hit{false};
    uint32_t row{0};
    uint32_t trackIndex{0};
    bool isHeader{false};
    bool isMute{false};
    bool isSolo{false};
};

struct ArrangerClip {
    std::string name{"Clip"};
    uint32_t startBar{1};
    uint32_t barLength{4};
    bool isLooped{true};
    uint32_t loopLengthBars{4};
    bool isAudio{false};
    int transposeSemitones{0};
    bool isReversed{false};
    float gainDb{0.0f};
    float r{0.0f}, g{0.9f}, b{1.0f};
};

// Note: ChordFollowMode, TrackMidiFxData, TrackAudioFxData are defined in "views/track_inspector_view.hpp"

enum class TrackInspectorHitArea {
    None,
    TrackTab,
    FullscreenDevice,
    PresetPrev,
    PresetNext,
    CodeButton,
    MuteButton,
    SoloButton,
    FreezeButton,
    VolumeSlider,
    PanKnob,
    ChordFollowChip,
    BakeChordsButton,
    MidiFxArpToggle,
    MidiFxArpKnob,
    MidiFxScaleToggle,
    MidiFxScaleKnob,
    MidiFxHumanizeToggle,
    MidiFxHumanizeKnob,
    AudioFxDelayToggle,
    AudioFxDelayKnob,
    AudioFxChorusToggle,
    AudioFxChorusKnob,
    AudioFxEqToggle,
    AudioFxEqKnob,
    AudioFxCompToggle,
    AudioFxCompKnob,
    AudioFxConvolverToggle,
    AudioFxConvolverKnob,
    HardwareKnob,
    ColorSwatch,
    ScrollbarThumb,
    ScrollbarTrack
};

struct HitTestTrackInspectorResult {
    bool hit{false};
    TrackInspectorHitArea area{TrackInspectorHitArea::None};
    uint32_t trackIndex{0};
    uint32_t colorSwatchIndex{0};
    ChordFollowMode chordMode{ChordFollowMode::Off};
    std::string paramName;
    float normVal{0.0f};
    Point2D position;
};

struct HitTestMixerResult {
    bool hit{false};
    uint32_t channelIndex{0};
    bool isMaster{false};
    bool isFader{false};
    bool isPan{false};
    bool isMute{false};
    bool isSolo{false};
    bool isFreeze{false};
    bool isPhase{false};
    bool isFxIn{false};
    bool isAutomation{false};
    bool isEditButton{false};
    bool isLcdScreen{false};
    bool isPullTab{false};
    bool isPropertiesClose{false};
    bool isPropertiesResize{false};
    bool isPresetFull{false};
    bool isPresetFaders{false};
    bool isPresetMeters{false};
    bool isToggleRouting{false};
    bool isTogglePan{false};
    bool isToggleButtons{false};
    bool isToggleMeters{false};
    float normVal{0.0f};
    HitTestTrackInspectorResult trackInspectorHit;
};

struct ArrangerTrackData {
    std::string name{"Track"};
    std::string instrument{""};
    std::string instrumentEngine{""};
    std::string type{"SYNTH"};
    std::string iconRef{""};
    float r{0.0f}, g{0.9f}, b{1.0f};
    float volume{0.8f};
    float pan{0.0f};
    bool mute{false};
    bool solo{false};
    bool freeze{false};
    float eqLow{0.5f};
    float eqMid{0.5f};
    float eqHigh{0.5f};
    ChordFollowMode chordFollowMode{ChordFollowMode::Off};
    TrackMidiFxData midiFx;
    TrackAudioFxData audioFx;
    std::vector<ArrangerClip> clips;
};

enum class ArrangerHitArea {
    None,
    Ruler,
    TrackHeader,
    TrackMute,
    TrackSolo,
    TrackFreeze,
    TrackVolume,
    TrackPan,
    TrackEditButton,
    Clip,
    OverviewScrollbar,
    SidebarPullTab,
    SidebarCloseButton,
    SidebarTabTrack,
    SidebarTabClip,
    SidebarColorSwatch,
    SidebarAction,
    SidebarClipTranspose,
    SidebarClipLoopToggle,
    SidebarEqLow,
    SidebarEqMid,
    SidebarEqHigh,
    SidebarTrackInspector
};

struct HitTestArrangerResult {
    bool hit{false};
    ArrangerHitArea area{ArrangerHitArea::None};
    uint32_t trackIndex{0};
    uint32_t bar{0};
    uint32_t step{0};
    float normVal{0.0f};
    int clipIndex{-1};
    uint32_t colorSwatchIndex{0};
    std::string actionName;
    int transposeDelta{0};
    HitTestTrackInspectorResult trackInspectorHit;
};

struct HitTestEatscriptResult {
    bool hit{false};
    enum class Action {
        None,
        EditorClick,
        Compile,
        HotReload,
        ToggleAotView,
        LoadTemplate,
        BindTrack,
        SubNavModular,
        SubNavEatscript,
        SubNavGuiDesigner
    } action{Action::None};
    int line{0};
    int col{0};
    uint32_t templateIndex{0};
    uint32_t trackIndex{0};
};

struct HitTestNoteScriptResult {
    bool hit{false};
    enum class Action {
        None,
        ApplySync,
        RevertFromTrack,
        InsertTemplate,
        Clear,
        EditorClick
    } action{Action::None};
    int line{0};
    int col{0};
};

enum class ScoreHitAction {
    None,
    StaffClick,
    NoteClick,
    ClefGrandStaff,
    ClefTreble,
    ClefBass,
    DurationWhole,
    DurationHalf,
    DurationQuarter,
    DurationEighth,
    DurationSixteenth,
    AccidentalNone,
    AccidentalSharp,
    AccidentalFlat,
    PlayheadScrub
};

struct HitTestScoreResult {
    bool hit{false};
    ScoreHitAction action{ScoreHitAction::None};
    uint32_t step{0};
    uint8_t pitch{60};
    bool isTrebleStaff{true};
    bool hasExistingNote{false};
};

struct FontRendererState;

/**
 * High-Performance Desktop GUI Window with Filament + NanoVG Pipeline.
 * Manages GLFW event loop, real-time mouse interaction, knob parameter sweeps,
 * live catenary cable patching, and animated oscilloscope/VU meter rendering.
 */
class GuiWindow {
public:
    GuiWindow(uint32_t width = 1280, uint32_t height = 800, const std::string& title = "Eatsbits Modular Workstation");
    ~GuiWindow();

    bool initialize(audio::AudioEngine& engine);
    void runEventLoop();
    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept;
    void renderFrame();

    [[nodiscard]] bool isIdle() const noexcept;
    [[nodiscard]] bool isAnyDrawerAnimating() const noexcept;
    void markNeedsRedraw() noexcept { needsRedraw_ = true; }
    [[nodiscard]] eatsbits::presenter::TelemetryPresenter& getTelemetryPresenter() noexcept { return telemetryPresenter_; }
    [[nodiscard]] const eatsbits::presenter::TelemetryPresenter& getTelemetryPresenter() const noexcept { return telemetryPresenter_; }

    // Mouse & Keyboard Event Handlers (Called by GLFW callbacks or unit tests)
    void onMouseMove(float x, float y);
    void onMouseDown(int button, float x, float y);
    void onMouseUp(int button, float x, float y);
    void onKeyDown(int key, int mods = 0);
    void onChar(unsigned int codepoint);
    void onMouseScroll(double xoffset, double yoffset);
    void onFilesDropped(const std::vector<std::string>& filePaths, float x, float y);

    std::function<void(const std::vector<std::string>&, float, float)> onExternalFilesDropped;

    // Window Dimensions, DPI, Resizing & Global UI Scale Factor
    [[nodiscard]] uint32_t getWidth() const noexcept { return width_; }
    [[nodiscard]] uint32_t getHeight() const noexcept { return height_; }
    [[nodiscard]] int getWindowWidth() const noexcept { return windowWidth_; }
    [[nodiscard]] int getWindowHeight() const noexcept { return windowHeight_; }
    [[nodiscard]] int getFramebufferWidth() const noexcept { return fbWidth_; }
    [[nodiscard]] int getFramebufferHeight() const noexcept { return fbHeight_; }

    [[nodiscard]] float getUiScale() const noexcept { return uiScale_; }
    [[nodiscard]] float getDpiScale() const noexcept { return dpiScale_; }
    [[nodiscard]] float getRenderScale() const noexcept { return renderScale_; }
    void setUiScale(float scale) noexcept;
    void zoomIn() noexcept;
    void zoomOut() noexcept;
    void resetZoom() noexcept;

    [[nodiscard]] int getAntiAliasingMode() const noexcept { return antiAliasingMode_; }
    void setAntiAliasingMode(int mode) noexcept;

    [[nodiscard]] bool isHiDpiEnabled() const noexcept { return hiDpiEnabled_; }
    void setHiDpiEnabled(bool enable) noexcept;
    void toggleHiDpi() noexcept;

    [[nodiscard]] GLFWwindow* getWindowHandle() const noexcept { return window_; }
    [[nodiscard]] BatchRenderer2D* getBatchRenderer() noexcept { return batchRenderer_.get(); }

    [[nodiscard]] float windowToLogicalX(double winX) const noexcept;
    [[nodiscard]] float windowToLogicalY(double winY) const noexcept;

    void onWindowResize(int width, int height) noexcept;
    void onFramebufferResize(int width, int height) noexcept;
    void pollWebResize() noexcept;

    [[nodiscard]] bool isFullscreen() const noexcept { return isFullscreen_; }
    void toggleFullscreen() noexcept;
    void setFullscreen(bool enable) noexcept;

    // Dedicated Full-Display Device GUI (Instruments & FX)
    [[nodiscard]] bool isFullscreenDeviceOpen() const noexcept;
    void openFullscreenDevice(uint32_t trackIndex = 0);
    void openFullscreenFx(uint32_t trackIndex, int fxIndex = 0);
    void openFullscreenMidiFx(uint32_t trackIndex, int fxIndex = 0);
    void closeFullscreenDevice() noexcept;
    void toggleFullscreenDevice() noexcept;
    [[nodiscard]] FullscreenDeviceModal& getFullscreenDeviceModal() noexcept { return fullscreenDeviceModal_; }
    [[nodiscard]] const FullscreenDeviceModal& getFullscreenDeviceModal() const noexcept { return fullscreenDeviceModal_; }
    [[nodiscard]] const DeviceTarget& getLastFocusedDevice() const noexcept { return lastFocusedDevice_; }
    void setLastFocusedDevice(const DeviceTarget& target) noexcept { lastFocusedDevice_ = target; }

    // Workspace View Navigation
    [[nodiscard]] WorkspaceView getActiveView() const noexcept { return activeView_; }
    void setActiveView(WorkspaceView view) noexcept { activeView_ = view; }

    [[nodiscard]] EditSubView getEditSubView() const noexcept { return editSubView_; }
    void setEditSubView(EditSubView subView) noexcept { editSubView_ = subView; }

    [[nodiscard]] DesignSubView getDesignSubView() const noexcept { return designSubView_; }
    void setDesignSubView(DesignSubView subView);

    [[nodiscard]] bool isLoopEnabled() const noexcept { return loopEnabled_; }
    void toggleLoop() noexcept { loopEnabled_ = !loopEnabled_; }

    [[nodiscard]] bool isMetronomeEnabled() const noexcept { return metronomeEnabled_; }
    void toggleMetronome() noexcept { metronomeEnabled_ = !metronomeEnabled_; }

    [[nodiscard]] bool isBrowserOpen() const noexcept { return browserOpen_; }
    void toggleBrowser() noexcept { browserOpen_ = !browserOpen_; }
    void setBrowserOpen(bool open) noexcept { browserOpen_ = open; }

    [[nodiscard]] bool isProjectHubOpen() const noexcept { return projectHubOpen_; }
    void toggleProjectHub() noexcept { projectHubOpen_ = !projectHubOpen_; }
    void setProjectHubOpen(bool open) noexcept { projectHubOpen_ = open; }
    [[nodiscard]] const std::string& getProjectName() const noexcept { return projectName_; }
    void setProjectName(const std::string& name) { projectName_ = name; }
    [[nodiscard]] const std::string& getAuthorName() const noexcept { return authorName_; }
    void setAuthorName(const std::string& name) { authorName_ = name; }
    [[nodiscard]] int getProjectHubSection() const noexcept { return projectHubSection_; }
    void setProjectHubSection(int sec) noexcept { projectHubSection_ = sec; }
    [[nodiscard]] float getProjectHubScrollY() const noexcept { return projectHubScrollY_; }
    [[nodiscard]] const ScrollableArea& getProjectHubScrollArea() const noexcept { return projectHubScrollArea_; }

    [[nodiscard]] bool isAutoRestoreSession() const noexcept { return autoRestoreSession_; }
    void setAutoRestoreSession(bool enable) noexcept { autoRestoreSession_ = enable; }
    [[nodiscard]] bool isAutoSaveEnabled() const noexcept { return autoSaveEnabled_; }
    void setAutoSaveEnabled(bool enable) noexcept { autoSaveEnabled_ = enable; }
    [[nodiscard]] bool isCrtShaderEnabled() const noexcept { return crtShaderEnabled_; }
    void setCrtShaderEnabled(bool enable) noexcept { crtShaderEnabled_ = enable; }
    void toggleCrtShader() noexcept { crtShaderEnabled_ = !crtShaderEnabled_; }
    [[nodiscard]] bool isCrtTweakerOpen() const noexcept { return crtTweakerOpen_; }
    void toggleCrtTweaker() noexcept { crtTweakerOpen_ = !crtTweakerOpen_; }
    void setCrtTweakerOpen(bool open) noexcept { crtTweakerOpen_ = open; }
    [[nodiscard]] bool isGuiAnimationsEnabled() const noexcept { return guiAnimationsEnabled_; }
    void setGuiAnimationsEnabled(bool enable) noexcept {
        guiAnimationsEnabled_ = enable;
        if (!enable) {
            virtualKeyboardAnimProgress_ = virtualKeyboardDrawerOpen_ ? 1.0f : 0.0f;
        }
    }
    [[nodiscard]] int getActiveThemePreset() const noexcept { return activeThemePreset_; }
    void setActiveThemePreset(int preset) noexcept {
        activeThemePreset_ = preset;
        Theme::setPreset(static_cast<Theme::Preset>(preset));
    }
    [[nodiscard]] const ThemeTokens& getTheme() const noexcept {
        return Theme::get(static_cast<Theme::Preset>(activeThemePreset_));
    }

    // Modular Subsystem Views & Ubiquitous Widgets (Option B: Modular C++ Architecture)
    [[nodiscard]] ArrangerView* getModularArrangerView() noexcept { return modularArrangerView_.get(); }
    [[nodiscard]] EditView* getModularEditView() noexcept { return modularEditView_.get(); }
    [[nodiscard]] TrackInspectorView* getModularTrackInspectorView() noexcept { return modularTrackInspectorView_.get(); }
    [[nodiscard]] MixerView* getModularMixerView() noexcept { return modularMixerView_.get(); }
    [[nodiscard]] DesignView* getModularDesignView() noexcept { return modularDesignView_.get(); }
    [[nodiscard]] VirtualKeyboardDrawer* getVirtualKeyboardDrawerWidget() noexcept { return virtualKeyboardDrawerWidget_.get(); }
    [[nodiscard]] ProjectBrowserDrawer* getProjectBrowserDrawerWidget() noexcept { return projectBrowserDrawerWidget_.get(); }
    [[nodiscard]] TransportHeader* getTransportHeaderWidget() noexcept { return transportHeaderWidget_.get(); }
    [[nodiscard]] BottomNavBar* getBottomNavBarWidget() noexcept { return bottomNavBarWidget_.get(); }
    [[nodiscard]] TerminalConsoleDrawer* getTerminalConsoleDrawerWidget() noexcept { return terminalConsoleDrawerWidget_.get(); }
    [[nodiscard]] bool isTerminalConsoleOpen() const noexcept {
        return terminalConsoleDrawerWidget_ && terminalConsoleDrawerWidget_->isExpanded();
    }
    void toggleTerminalConsole() noexcept {
        if (terminalConsoleDrawerWidget_) terminalConsoleDrawerWidget_->toggleExpanded();
    }
    void setTerminalConsoleOpen(bool open) noexcept {
        if (terminalConsoleDrawerWidget_) terminalConsoleDrawerWidget_->setExpanded(open);
    }
    [[nodiscard]] KineticScroller& getKineticScroller() noexcept { return kineticScroller_; }
    [[nodiscard]] GestureRecognizer& getGestureRecognizer() noexcept { return gestureRecognizer_; }
    [[nodiscard]] ViewContext createViewContext() noexcept;

    [[nodiscard]] DialogLayout computeDialogLayout(float w = 540.0f, float h = 580.0f) const noexcept;
    DialogLayout drawModalDialogFrame(const DialogFrameConfig& config);
    [[nodiscard]] HitTestProjectHubResult hitTestProjectHub(float x, float y) const noexcept;
    void resetToDefaultProject();
    bool saveProjectAs();
    bool loadProjectPrompt();

    [[nodiscard]] ValueEditDialog& getValueEditDialog() noexcept { return valueEditDialog_; }
    [[nodiscard]] const ValueEditDialog& getValueEditDialog() const noexcept { return valueEditDialog_; }
    void openValueEditDialog(const ValueEditRequest& req) { valueEditDialog_.open(req); }

    [[nodiscard]] CommandPaletteDialog& getCommandPaletteDialog() noexcept { return commandPaletteDialog_; }
    [[nodiscard]] const CommandPaletteDialog& getCommandPaletteDialog() const noexcept { return commandPaletteDialog_; }
    void openCommandPalette() noexcept;
    void closeCommandPalette() noexcept;
    void toggleCommandPalette() noexcept;
    [[nodiscard]] bool isCommandPaletteOpen() const noexcept;

    [[nodiscard]] AudioToMidiDialog& getAudioToMidiDialog() noexcept { return audioToMidiDialog_; }
    [[nodiscard]] const AudioToMidiDialog& getAudioToMidiDialog() const noexcept { return audioToMidiDialog_; }
    void openAudioToMidiConverter(const std::optional<audio::DecodedAudioBuffer>& initialBuffer = std::nullopt,
                                  const std::string& name = "");
    void closeAudioToMidiConverter() noexcept;
    [[nodiscard]] bool isAudioToMidiDialogOpen() const noexcept;

    // Hit-Testing logic
    [[nodiscard]] HitTestTransportResult hitTestTransport(float x, float y) const noexcept;
    [[nodiscard]] HitTestBottomNavResult hitTestBottomNav(float x, float y) const noexcept;
    [[nodiscard]] HitTestJackResult hitTestJack(float x, float y) const noexcept;
    [[nodiscard]] HitTestKnobResult hitTestKnob(float x, float y) const noexcept;
    [[nodiscard]] HitTestHardwareKnobResult hitTestHardwareKnob(float x, float y, float faceplateX = 40.0f, float faceplateY = 98.0f, float scale = 1.0f) const noexcept;
    [[nodiscard]] HitTestStepResult hitTestStep(float x, float y) const noexcept;
    [[nodiscard]] HitTestMixerResult hitTestMixer(float x, float y) const noexcept;
    [[nodiscard]] HitTestEditSubNavResult hitTestEditSubNav(float x, float y) const noexcept;
    [[nodiscard]] HitTestPianoKeyResult hitTestPianoKey(float x, float y) const noexcept;
    [[nodiscard]] HitTestPianoRollGridResult hitTestPianoRollGrid(float x, float y) const noexcept;
    [[nodiscard]] HitTestTrackerResult hitTestTracker(float x, float y) const noexcept;
    [[nodiscard]] HitTestArrangerResult hitTestArranger(float x, float y) const noexcept;
    [[nodiscard]] HitTestTrackTabResult hitTestTrackTab(float x, float y) const noexcept;
    [[nodiscard]] HitTestTrackInspectorResult hitTestTrackInspector(float x, float y) const noexcept;
    void drawTopTransportBar();
    void drawTrackPropertiesContainer(float x, float y, float width, float height, uint32_t trackIndex, float scrollY, bool isCompact);
    [[nodiscard]] HitTestTrackInspectorResult hitTestTrackPropertiesContainer(float mx, float my, float x, float y, float width, float height, uint32_t trackIndex, float scrollY, bool isCompact) const noexcept;
    [[nodiscard]] float getTrackInspectorScrollY() const noexcept { return trackInspectorScrollY_; }
    void setTrackInspectorScrollY(float scroll) noexcept { trackInspectorScrollY_ = scroll; }
    void handleTrackInspectorInteraction(const HitTestTrackInspectorResult& inspHit, float x, float y);
    [[nodiscard]] HitTestBrowserResult hitTestBrowser(float x, float y) const noexcept;
    [[nodiscard]] HitTestEatscriptResult hitTestEatscript(float x, float y) const noexcept;
    [[nodiscard]] HitTestNoteScriptResult hitTestNoteScript(float x, float y) const noexcept;

    // Note Selection & Inspector Sidebar (Decoupled across Piano Roll, Tracker, Score, Script)
    void drawNoteSelectionSidebar(float x, float y, float width, float height, sequencer::SequencerTrack& track);
    [[nodiscard]] HitTestSelectionSidebarResult hitTestSelectionSidebar(float mx, float my, float x, float y, float width, float height, const sequencer::SequencerTrack& track) const noexcept;
    void updatePianoRollMarqueeSelection(float startX, float startY, float curX, float curY, bool isShift);
    [[nodiscard]] bool isMarqueeSelecting() const noexcept { return isMarqueeSelecting_; }
    [[nodiscard]] Rect getMarqueeRect() const noexcept;

    // Piano Roll 2D Scrolling, Panning & FL Studio Keyboard / Grid Metrics
    [[nodiscard]] float getPianoRollScrollX() const noexcept { return pianoRollScrollX_; }
    void setPianoRollScrollX(float scrollX) noexcept;
    [[nodiscard]] float getPianoRollScrollY() const noexcept { return pianoRollScrollY_; }
    void setPianoRollScrollY(float scrollY) noexcept;
    [[nodiscard]] float getPianoRollStepWidth() const noexcept { return pianoRollStepW_; }
    void setPianoRollStepWidth(float stepW) noexcept;
    [[nodiscard]] float getPianoRollRowHeight() const noexcept { return pianoRollRowH_; }
    void setPianoRollRowHeight(float rowH) noexcept;
    void centerPianoRollOnNotes() noexcept;
    void selectPianoRollNotesByPitch(int pitch, bool additive = false);
    [[nodiscard]] HitTestPianoRollScrollbarResult hitTestPianoRollScrollbar(float mx, float my) const noexcept;

    // Collapsible Virtual Piano Keyboard Drawer (Horizontal)
    [[nodiscard]] bool isVirtualKeyboardDrawerOpen() const noexcept { return virtualKeyboardDrawerOpen_; }
    void setVirtualKeyboardDrawerOpen(bool open) noexcept {
        virtualKeyboardDrawerOpen_ = open;
        if (!guiAnimationsEnabled_) {
            virtualKeyboardAnimProgress_ = open ? 1.0f : 0.0f;
        }
    }
    void toggleVirtualKeyboardDrawer() noexcept {
        virtualKeyboardDrawerOpen_ = !virtualKeyboardDrawerOpen_;
        if (!guiAnimationsEnabled_) {
            virtualKeyboardAnimProgress_ = virtualKeyboardDrawerOpen_ ? 1.0f : 0.0f;
        }
    }
    [[nodiscard]] float getVirtualKeyboardAnimProgress() const noexcept { return virtualKeyboardAnimProgress_; }
    [[nodiscard]] float getVirtualKeyboardDrawerHeight() const noexcept { return virtualKeyboardDrawerHeight_; }
    void setVirtualKeyboardDrawerHeight(float h) noexcept { virtualKeyboardDrawerHeight_ = std::clamp(h, 120.0f, 280.0f); }
    [[nodiscard]] int getVirtualKeyboardBaseOctave() const noexcept { return virtualKeyboardBaseOctave_; }
    void setVirtualKeyboardBaseOctave(int octave) noexcept;
    [[nodiscard]] int getVirtualKeyboardActivePitch() const noexcept { return virtualKeyboardActivePitch_; }
    [[nodiscard]] const PianoKeyboard& getPianoRollKeyboard() const noexcept { return pianoRollKeyboard_; }
    [[nodiscard]] const PianoKeyboard& getVirtualPianoDrawerKeyboard() const noexcept { return virtualPianoDrawerKeyboard_; }
    [[nodiscard]] HitTestVirtualKeyboardDrawerResult hitTestVirtualKeyboardDrawer(float mx, float my) const noexcept;
    void drawVirtualKeyboardDrawer();

    // Note Script Editor (EDIT > SCRIPT)
    [[nodiscard]] std::string getNoteScriptText() const;
    void setNoteScriptText(const std::string& text);
    [[nodiscard]] const std::vector<std::string>& getNoteScriptBuffer() const noexcept { return noteScriptBuffer_; }
    [[nodiscard]] int getNoteScriptCursorLine() const noexcept { return noteScriptCursorLine_; }
    [[nodiscard]] int getNoteScriptCursorCol() const noexcept { return noteScriptCursorCol_; }
    void setNoteScriptCursor(int line, int col) noexcept;
    void insertNoteScriptChar(char c);
    void insertNoteScriptText(const std::string& text);
    void deleteNoteScriptCharBackwards();
    void deleteNoteScriptCharForwards();
    void insertNoteScriptNewLine();
    bool syncNoteScriptToTrack();
    void syncTrackToNoteScript();
    void insertNoteScriptTemplate();
    [[nodiscard]] const std::string& getNoteScriptStatus() const noexcept { return noteScriptStatusMsg_; }
    [[nodiscard]] const std::string& getNoteScriptError() const noexcept { return noteScriptError_; }
    [[nodiscard]] int getNoteScriptErrorLine() const noexcept { return noteScriptErrorLine_; }
    [[nodiscard]] bool isNoteScriptDirty() const noexcept { return noteScriptDirty_; }

    // Eatscript Live IDE & Workbench
    [[nodiscard]] std::string getScriptCode() const;
    void setScriptCode(const std::string& code);
    [[nodiscard]] const std::vector<std::string>& getScriptBuffer() const noexcept { return scriptBuffer_; }
    [[nodiscard]] int getScriptCursorLine() const noexcept { return scriptCursorLine_; }
    [[nodiscard]] int getScriptCursorCol() const noexcept { return scriptCursorCol_; }
    void setScriptCursor(int line, int col) noexcept;
    void insertScriptChar(char c);
    void insertScriptText(const std::string& text);
    void deleteScriptCharBackwards();
    void deleteScriptCharForwards();
    void insertScriptNewLine();
    bool compileActiveScript();
    bool hotReloadScriptToTrack(uint32_t trackIndex);
    void loadScriptTemplate(size_t index);
    [[nodiscard]] bool isScriptCompiled() const noexcept { return scriptCompiled_; }
    [[nodiscard]] const std::string& getScriptStatusMessage() const noexcept { return scriptStatusMsg_; }
    [[nodiscard]] const std::string& getScriptError() const noexcept { return scriptError_; }
    [[nodiscard]] int getScriptErrorLine() const noexcept { return scriptErrorLine_; }
    [[nodiscard]] const std::vector<std::string>& getScriptDisassembly() const noexcept { return scriptDisassembly_; }
    [[nodiscard]] const std::string& getScriptTranspiledCode() const noexcept { return scriptTranspiledCode_; }
    [[nodiscard]] bool isScriptAotViewEnabled() const noexcept { return scriptShowAotView_; }
    void setScriptAotViewEnabled(bool enabled) noexcept { scriptShowAotView_ = enabled; }
    [[nodiscard]] size_t getScriptTemplateCount() const noexcept { return scriptTemplates_.size(); }
    [[nodiscard]] std::string getScriptTemplateName(size_t index) const;
    [[nodiscard]] uint32_t getScriptTargetTrack() const noexcept { return scriptTargetTrack_; }
    void setScriptTargetTrack(uint32_t track) noexcept { scriptTargetTrack_ = track; }

    // Tracker State & QWERTY Musical Key Mapping
    [[nodiscard]] uint32_t getTrackerSelectedRow() const noexcept { return trackerSelectedRow_; }
    void setTrackerSelectedRow(uint32_t row) noexcept { trackerSelectedRow_ = row % 64; }
    [[nodiscard]] uint32_t getTrackerSelectedTrack() const noexcept { return trackerSelectedTrack_; }
    void setTrackerSelectedTrack(uint32_t track) noexcept { trackerSelectedTrack_ = track; }
    [[nodiscard]] bool isTrackerFollowPlayback() const noexcept { return trackerFollowPlayback_; }
    void setTrackerFollowPlayback(bool follow) noexcept { trackerFollowPlayback_ = follow; }
    [[nodiscard]] float getTrackerScrollY() const noexcept { return trackerScrollY_; }
    void setTrackerScrollY(float scroll) noexcept { trackerScrollY_ = scroll; }
    static int qwertyKeyToMidiPitch(int key, int baseOctave = 4) noexcept;

    // Preset Librarian & Project Management
    [[nodiscard]] BrowserTab getBrowserTab() const noexcept { return browserTab_; }
    void setBrowserTab(BrowserTab tab) noexcept { browserTab_ = tab; }
    void setBrowserCategory(const std::string& cat) noexcept { browserCategory_ = cat; }
    [[nodiscard]] const std::string& getBrowserCategory() const noexcept { return browserCategory_; }
    void openPresetDialog(PluginDialogMode mode = PluginDialogMode::SelectPreset, uint32_t trackIndex = 0);
    void closePresetDialog() noexcept;
    [[nodiscard]] bool isPresetDialogOpen() const noexcept;
    [[nodiscard]] PresetSearchDialog& getPresetSearchDialog() noexcept { return presetSearchDialog_; }
    [[nodiscard]] const PresetSearchDialog& getPresetSearchDialog() const noexcept { return presetSearchDialog_; }
    void loadPresetToSelectedTrack(size_t presetIndex);
    void runMacro(size_t macroIndex);
    bool saveProjectToFile(const std::string& filePath = "project.eats");
    bool loadProjectFromFile(const std::string& filePath = "project.eats");
    bool bounceMasterToWav(const std::string& filePath = "master_output.wav");
    [[nodiscard]] const std::string& getLastStatusMessage() const noexcept { return lastStatusMessage_; }
    [[nodiscard]] const std::string& getStatusToastText() const noexcept { return statusToastText_; }
    void setStatusMessage(const std::string& msg) noexcept;

    // Pure Diff-Based History & Time-Travel
    [[nodiscard]] const project::DiffHistoryManager& getDiffHistory() const noexcept { return diffHistory_; }
    [[nodiscard]] project::DiffHistoryManager& getDiffHistory() noexcept { return diffHistory_; }
    void initHistory();
    void recordProjectHistory(const std::string& description, const std::string& category = "EDIT", bool isMilestone = false, const std::string& milestoneName = "");
    bool undoHistory();
    bool redoHistory();
    bool jumpToHistoryIndex(size_t index);
    void createHistoryMilestone(const std::string& name = "Checkpoint");
    void clearHistory();
    [[nodiscard]] size_t getSelectedHistoryIndex() const noexcept { return selectedHistoryIndex_; }
    void setSelectedHistoryIndex(size_t index) noexcept { selectedHistoryIndex_ = index; }
    [[nodiscard]] float getHistoryScrollY() const noexcept { return historyScrollY_; }
    void setHistoryScrollY(float scroll) noexcept { historyScrollY_ = scroll; }

    [[nodiscard]] uint32_t getSelectedTrackIndex() const noexcept { return selectedTrackIndex_; }
    void setSelectedTrackIndex(uint32_t idx) noexcept;
    void setTrackMuteState(uint32_t trackIdx, bool mute);
    void setTrackSoloState(uint32_t trackIdx, bool solo);
    void setTrackFreezeState(uint32_t trackIdx, bool freeze);
    void syncActiveClipToEditView(uint32_t trackIdx, int clipIdx = -1);
    void syncArrangerToSequencer();
    void syncArrangerFromSequencer();
    void syncTrackAudioFxToEngine(uint32_t trackIdx);
    void syncTrackMidiFxToEngine(uint32_t trackIdx);
    void syncAllTracksFxToEngine();
    [[nodiscard]] int getPreviewingPitch() const noexcept { return previewingPitch_; }
    [[nodiscard]] float getPreviewingVelocity() const noexcept { return previewingVelocity_; }
    void setPreviewingVelocity(float vel) noexcept { previewingVelocity_ = std::clamp(vel, 0.0f, 1.0f); }
    [[nodiscard]] bool isShiftPressed() const noexcept;
    void setMockShiftPressed(bool pressed) noexcept { mockShiftPressed_ = pressed; }
    [[nodiscard]] bool isCtrlPressed() const noexcept;
    [[nodiscard]] bool isAltPressed() const noexcept;

    // Master Channel
    [[nodiscard]] float getMasterVolume() const noexcept { return masterVolume_; }
    void setMasterVolume(float vol) noexcept;
    [[nodiscard]] float getMasterPan() const noexcept { return masterPan_; }
    void setMasterPan(float pan) noexcept { masterPan_ = pan; }
    [[nodiscard]] bool isMasterMuted() const noexcept { return masterMute_; }
    void setMasterMuted(bool mute) noexcept { masterMute_ = mute; }

    // Preset Library & Hardware Panel
    void loadPresets();
    void loadPresetFile(const std::string& filePath);
    void nextPreset();
    void prevPreset();
    void syncTrackToPreset(uint32_t trackIndex);
    [[nodiscard]] const project::PresetDefinition* getActivePreset() const noexcept;
    [[nodiscard]] project::PresetDefinition* getActivePreset() noexcept;
    void dispatchHardwareParam(const std::string& paramName, float normVal) noexcept;

    // Mixer Strips
    struct MixerStrip {
        std::string name{"Channel"};
        std::string iconRef{""};
        audio::NodeId nodeId{0};
        float volume{0.8f};
        float pan{0.0f};
        bool mute{false};
        bool solo{false};
        bool freeze{false};
        bool phaseInvert{false};
        bool fxIn{true};
        int automationMode{0}; // 0: TRIM, 1: READ, 2: TOUCH, 3: LATCH
        float peakL{0.0f};
        float peakR{0.0f};
    };
    [[nodiscard]] const std::vector<MixerStrip>& getMixerStrips() const noexcept { return mixerStrips_; }
    [[nodiscard]] std::vector<MixerStrip>& getMixerStrips() noexcept { return mixerStrips_; }
    void updateMixerStrips();

    // Modular Mixer Section Flags
    [[nodiscard]] bool isMixerRoutingVisible() const noexcept { return showMixerRouting_; }
    void setMixerRoutingVisible(bool visible) noexcept { showMixerRouting_ = visible; }
    [[nodiscard]] bool isMixerAutomationVisible() const noexcept { return showMixerAutomation_; }
    void setMixerAutomationVisible(bool visible) noexcept { showMixerAutomation_ = visible; }
    [[nodiscard]] bool isMixerPanVisible() const noexcept { return showMixerPan_; }
    void setMixerPanVisible(bool visible) noexcept { showMixerPan_ = visible; }
    [[nodiscard]] bool isMixerFadersVisible() const noexcept { return showMixerFaders_; }
    void setMixerFadersVisible(bool visible) noexcept { showMixerFaders_ = visible; }
    [[nodiscard]] bool isMixerMetersVisible() const noexcept { return showMixerMeters_; }
    void setMixerMetersVisible(bool visible) noexcept { showMixerMeters_ = visible; }
    [[nodiscard]] bool isMixerButtonsVisible() const noexcept { return showMixerButtons_; }
    void setMixerButtonsVisible(bool visible) noexcept { showMixerButtons_ = visible; }
    [[nodiscard]] bool isMixerReadoutsVisible() const noexcept { return showMixerReadouts_; }
    void setMixerReadoutsVisible(bool visible) noexcept { showMixerReadouts_ = visible; }

    void setMixerSectionPreset(MixerSectionPreset preset) noexcept {
        switch (preset) {
            case MixerSectionPreset::Full:
                showMixerRouting_ = true;
                showMixerAutomation_ = true;
                showMixerPan_ = true;
                showMixerFaders_ = true;
                showMixerMeters_ = true;
                showMixerButtons_ = true;
                showMixerReadouts_ = true;
                break;
            case MixerSectionPreset::FadersOnly:
                showMixerRouting_ = false;
                showMixerAutomation_ = false;
                showMixerPan_ = false;
                showMixerFaders_ = true;
                showMixerMeters_ = false;
                showMixerButtons_ = false;
                showMixerReadouts_ = true;
                break;
            case MixerSectionPreset::FadersAndMeters:
                showMixerRouting_ = false;
                showMixerAutomation_ = false;
                showMixerPan_ = false;
                showMixerFaders_ = true;
                showMixerMeters_ = true;
                showMixerButtons_ = false;
                showMixerReadouts_ = true;
                break;
        }
    }

    [[nodiscard]] CanvasRenderer& getCanvas() noexcept { return canvas_; }
    [[nodiscard]] DawnBridge& getDawnBridge() noexcept { return dawnBridge_; }
    [[nodiscard]] bool is3dConsoleEnabled() const noexcept { return false; }
    void set3dConsoleEnabled(bool /*enabled*/) noexcept {}
    void toggle3dConsole() noexcept {}
    void transform3dMouseCoords(float inX, float inY, float& outX, float& outY) const noexcept { outX = inX; outY = inY; }
    void remapCrtMouseCoords(float inX, float inY, float& outX, float& outY) const noexcept;
    [[nodiscard]] DragMode getDragMode() const noexcept {
        if (activeDragHandler_ && activeDragHandler_->isDragging()) {
            return activeDragHandler_->getDragMode();
        }
        return dragMode_;
    }

    // Arranger Properties Sidebar
    [[nodiscard]] bool isArrangerPropertiesExpanded() const noexcept { return arrangerPropertiesExpanded_; }
    void setArrangerPropertiesExpanded(bool expanded) noexcept { arrangerPropertiesExpanded_ = expanded; }
    void toggleArrangerProperties() noexcept { arrangerPropertiesExpanded_ = !arrangerPropertiesExpanded_; }
    [[nodiscard]] float getArrangerPropertiesWidth() const noexcept { return arrangerPropertiesWidth_; }
    void setArrangerPropertiesWidth(float width) noexcept;

    // Mixer Properties Pullout Sidebar (Reusing Track Properties Inspector)
    [[nodiscard]] bool isMixerPropertiesExpanded() const noexcept { return mixerPropertiesExpanded_; }
    void setMixerPropertiesExpanded(bool expanded) noexcept { mixerPropertiesExpanded_ = expanded; }
    void toggleMixerProperties() noexcept { mixerPropertiesExpanded_ = !mixerPropertiesExpanded_; }
    [[nodiscard]] float getMixerPropertiesWidth() const noexcept { return mixerPropertiesWidth_; }
    void setMixerPropertiesWidth(float width) noexcept;
    [[nodiscard]] ArrangerInspectorTab getArrangerInspectorTab() const noexcept { return arrangerInspectorTab_; }
    void setArrangerInspectorTab(ArrangerInspectorTab tab) noexcept { arrangerInspectorTab_ = tab; }
    [[nodiscard]] int getSelectedArrangerClipTrack() const noexcept { return selectedArrangerClipTrack_; }
    [[nodiscard]] int getSelectedArrangerClipIndex() const noexcept { return selectedArrangerClipIndex_; }
    void selectArrangerClip(int trackIdx, int clipIdx) noexcept;
    [[nodiscard]] const std::vector<ArrangerTrackData>& getArrangerTracks() const noexcept { return arrangerTracks_; }
    [[nodiscard]] std::vector<ArrangerTrackData>& getArrangerTracks() noexcept { return arrangerTracks_; }

    // Typography & Font Configuration
    bool loadFont(const std::string& fontPath, const std::string& fontName = "ui");
    bool loadMonoFont(const std::string& fontPath, const std::string& fontName = "mono");
    [[nodiscard]] const std::string& getActiveFontName() const noexcept { return activeFontName_; }
    [[nodiscard]] const std::string& getActiveFontPath() const noexcept { return activeFontPath_; }
    [[nodiscard]] const std::string& getActiveMonoFontName() const noexcept { return activeMonoFontName_; }
    [[nodiscard]] const std::string& getActiveMonoFontPath() const noexcept { return activeMonoFontPath_; }
    [[nodiscard]] bool isFontLoaded() const noexcept;
    [[nodiscard]] bool isMonoFontLoaded() const noexcept;

    // Score Notation View
    [[nodiscard]] ScoreClef getScoreClef() const noexcept { return scoreClef_; }
    void setScoreClef(ScoreClef clef) noexcept { scoreClef_ = clef; }
    [[nodiscard]] ScoreNoteType getScoreDuration() const noexcept { return scoreDuration_; }
    void setScoreDuration(ScoreNoteType dur) noexcept { scoreDuration_ = dur; }
    [[nodiscard]] ScoreAccidental getScoreAccidental() const noexcept { return scoreAccidental_; }
    void setScoreAccidental(ScoreAccidental acc) noexcept { scoreAccidental_ = acc; }
    [[nodiscard]] float getScoreScrollX() const noexcept { return scoreScrollX_; }
    void setScoreScrollX(float scroll) noexcept { scoreScrollX_ = scroll; }
    [[nodiscard]] float getScoreStaffSpace() const noexcept { return scoreStaffSpace_; }
    void setScoreStaffSpace(float sp) noexcept { scoreStaffSpace_ = sp; }
    [[nodiscard]] HitTestScoreResult hitTestScore(float x, float y) const noexcept;

private:
    uint32_t width_{1280};
    uint32_t height_{800};
    int windowWidth_{1280};
    int windowHeight_{800};
    int fbWidth_{1280};
    int fbHeight_{800};
    float uiScale_{1.0f};
    float dpiScale_{1.0f};
    float renderScale_{1.0f};
    int antiAliasingMode_{2};
    bool hiDpiEnabled_{true};

    void updateLogicalDimensions() noexcept;

    std::string title_;

    GLFWwindow* window_{nullptr};

    // Font Engine
    std::unique_ptr<FontRendererState> fontRenderer_;
    std::string activeFontName_{"ui"};
    std::string activeFontPath_{""};
    std::string pendingFontPath_{""};
    std::string pendingFontName_{"ui"};
    std::string activeMonoFontName_{"mono"};
    std::string activeMonoFontPath_{""};
    std::string pendingMonoFontPath_{""};
    std::string pendingMonoFontName_{"mono"};
    void initFontRenderer();
    audio::AudioEngine* engine_{nullptr};

    CanvasRenderer canvas_;
    DawnBridge dawnBridge_;
    std::unique_ptr<BatchRenderer2D> batchRenderer_;

    // Interactive State
    WorkspaceView activeView_{WorkspaceView::Arranger};
    EditSubView editSubView_{EditSubView::PianoRoll};
    DesignSubView designSubView_{DesignSubView::ModularRack};
    uint32_t selectedTrackIndex_{0};
    int previewingPitch_{-1};
    float previewingVelocity_{0.85f};
    bool mockShiftPressed_{false};
    bool loopEnabled_{false};
    bool metronomeEnabled_{false};
    bool browserOpen_{false};
    bool projectHubOpen_{false};
    bool isFullscreen_{false};
    bool isRendering_{false};
    long savedWindowStyle_{0};
    int savedWindowX_{100};
    int savedWindowY_{100};
    int savedWindowW_{1280};
    int savedWindowH_{800};
    bool hasSavedWindowState_{false};
    int projectHubSection_{0};
    float projectHubScrollY_{0.0f};
    float projectHubMaxScroll_{0.0f};
    bool projectHubDraggingThumb_{false};
    ScrollableArea projectHubScrollArea_;
    std::string projectName_{"Untitled Song"};
    std::string authorName_{"Anonymous Producer"};
    std::string projectFilePath_{"project.eats"};
    bool autoRestoreSession_{true};
    bool autoSaveEnabled_{true};
    int activeThemePreset_{0};
    bool crtShaderEnabled_{false};
    bool crtTweakerOpen_{false};
    int crtTweakerSliderIndex_{-1};
    float crtTweakerTrackX_{0.0f};
    float crtTweakerTrackW_{0.0f};
    float lampTime_{0.0f};
    bool guiAnimationsEnabled_{true};
    bool isEditingTitle_{false};
    bool isEditingAuthor_{false};
    DragMode dragMode_{DragMode::None};
    presenter::IDragHandler* activeDragHandler_{nullptr};
    presenter::ScalarDragPresenter scalarDragPresenter_;
    presenter::TimelineScrubPresenter timelineScrubPresenter_;
    presenter::SplitterDragPresenter splitterDragPresenter_;
    presenter::CablePatchPresenter cablePatchPresenter_;
    presenter::MarqueeSelectPresenter marqueeSelectPresenter_;
    float mouseX_{0.0f};
    float mouseY_{0.0f};
    float dragStartY_{0.0f};
    float dragStartX_{0.0f};
    float dragStartValGeneric_{0.0f};
    float dragStartPan_{0.0f};
    uint32_t activeArrangerTrack_{0};
    double lastTrackClickTime_{0.0};
    uint32_t lastClickedTrack_{0};
    double lastClipClickTime_{0.0};
    int lastClickedClipTrack_{-1};
    int lastClickedClipIdx_{-1};

    // Arranger Properties Sidebar State
    bool arrangerPropertiesExpanded_{false};
    float arrangerPropertiesWidth_{300.0f};
    static constexpr float kArrangerPropertiesMinW{240.0f};
    static constexpr float kArrangerPropertiesMaxW{640.0f};
    static constexpr float kArrangerPullTabW{24.0f};
    ArrangerInspectorTab arrangerInspectorTab_{ArrangerInspectorTab::Track};

    // Mixer Properties Pullout Sidebar State (Reusing Track Properties from Arranger)
    bool mixerPropertiesExpanded_{true};
    float mixerPropertiesWidth_{360.0f};
    static constexpr float kMixerPropertiesMinW{260.0f};
    static constexpr float kMixerPropertiesMaxW{640.0f};
    static constexpr float kMixerPullTabW{24.0f};
    int selectedArrangerClipTrack_{-1};
    int selectedArrangerClipIndex_{-1};
    std::vector<ArrangerTrackData> arrangerTracks_;
    void initArrangerTracks();

    // Master Bus State
    float masterVolume_{0.85f};
    float masterPan_{0.0f};
    bool masterMute_{false};

    // Active Knob dragging
    audio::NodeId activeKnobNode_{0};
    uint32_t activeKnobIndex_{0};
    float dragStartValue_{0.0f};

    // Stored knob values: (nodeId, knobIndex) -> normalized [0.0, 1.0]
    std::map<std::pair<audio::NodeId, uint32_t>, float> knobValues_;

public:
    [[nodiscard]] float getKnobValue(audio::NodeId nodeId, uint32_t knobIndex) const noexcept;
    void setKnobValue(audio::NodeId nodeId, uint32_t knobIndex, float value) noexcept;
    void dispatchKnobParameter(audio::NodeId nodeId, uint32_t knobIndex, float normalizedVal) noexcept;
    void initDefaultKnobValues();

private:
    // Mixer State
    std::vector<MixerStrip> mixerStrips_;
    uint32_t activeMixerChannel_{0};

    // Active Cable dragging
    audio::NodeId dragCableSrcNode_{0};
    uint32_t dragCableSrcPort_{0};
    Point2D dragCableSrcPos_{0.0f, 0.0f};

    // Audio scope buffer
    float scopeBuffer_[256]{0};

    // Preset Library State
    std::vector<project::PresetDefinition> presets_;
    size_t activePresetIndex_{0};
    int64_t lastStopClickTimeMs_{0};
    std::string activeHardwareParam_;
    float dragStartHardwareVal_{0.0f};
    float trackInspectorScrollY_{0.0f};
    std::string activeTrackInspectorFxParam_{""};
    float dragStartTrackInspectorVal_{0.0f};
    float dragStartScrollY_{0.0f};

    // Preset Librarian & Browser Drawer State
    BrowserTab browserTab_{BrowserTab::Presets};
    std::string browserCategory_{"ALL"};
    std::string lastStatusMessage_{""};
    std::string statusToastText_{""};
    float statusToastTimer_{0.0f};
    size_t selectedBrowserPresetIndex_{0};
    bool projectLocked_{false};

    // Pure Diff-Based History State
    project::DiffHistoryManager diffHistory_;
    size_t selectedHistoryIndex_{0};
    float historyScrollY_{0.0f};
    float historyAutosaveTimer_{0.0f};
    bool historyDirty_{false};

    // FastTracker 2 / Renoise Multi-Track Tracker State
    uint32_t trackerSelectedRow_{0};
    uint32_t trackerSelectedTrack_{0};
    float trackerScrollY_{0.0f};
    bool trackerFollowPlayback_{true};

    // Note Script Editor State (EDIT > SCRIPT)
    std::vector<std::string> noteScriptBuffer_;
    int noteScriptCursorLine_{0};
    int noteScriptCursorCol_{0};
    float noteScriptScrollY_{0.0f};
    bool noteScriptDirty_{false};
    std::string noteScriptStatusMsg_{"Ready"};
    std::string noteScriptError_{""};
    int noteScriptErrorLine_{-1};
    uint32_t noteScriptLastTrack_{9999};
    void initNoteScriptEditor();

    // Eatscript Live IDE State
    std::vector<std::string> scriptBuffer_;
    int scriptCursorLine_{0};
    int scriptCursorCol_{0};
    float scriptScrollY_{0.0f};
    bool scriptShowAotView_{false};
    bool scriptCompiled_{false};
    std::string scriptStatusMsg_{"Ready"};
    std::string scriptError_{""};
    int scriptErrorLine_{-1};
    std::vector<std::string> scriptDisassembly_;
    std::string scriptTranspiledCode_{""};
    std::vector<std::pair<std::string, std::string>> scriptTemplates_;
    uint32_t scriptTargetTrack_{0};
    std::unique_ptr<eatscript::Program> activeProgram_;
    void initEatscriptIde();

    // Score Notation & SMuFL Engraving State
    ScoreClef scoreClef_{ScoreClef::GrandStaff};
    ScoreNoteType scoreDuration_{ScoreNoteType::Quarter};
    ScoreAccidental scoreAccidental_{ScoreAccidental::None};
    float scoreScrollX_{0.0f};
    float scoreStaffSpace_{13.0f};

    // Piano Roll Note Selection & Marquee Drag State
    bool isMarqueeSelecting_{false};
    float marqueeStartX_{0.0f};
    float marqueeStartY_{0.0f};
    float marqueeCurX_{0.0f};
    float marqueeCurY_{0.0f};
    int activeMoveStep_{-1};
    int activeResizeStep_{-1};
    float dragStartNoteX_{0.0f};
    float dragStartNoteY_{0.0f};
    std::map<uint32_t, uint8_t> batchMoveStartPitches_{};
    std::map<uint32_t, uint32_t> batchMoveStartSteps_{};
    std::map<uint32_t, float> batchResizeStartDurations_{};
    double lastNoteClickTime_{0.0};
    int lastClickedStep_{-1};

    // Decoupled Piano Keyboards (Vertical for Piano Roll, Horizontal for Drawer)
    PianoKeyboard pianoRollKeyboard_{PianoKeyboardConfig{KeyboardOrientation::Vertical, 24, 96, 3, 3, 90.0f, 22.0f, 0.62f, true}};
    PianoKeyboard virtualPianoDrawerKeyboard_{PianoKeyboardConfig{KeyboardOrientation::Horizontal, 24, 96, 3, 3, 72.0f, 130.0f, 0.65f, true}};
    float pianoRollScrollX_{0.0f};
    float pianoRollScrollY_{792.0f}; // Default C4 (pitch 60) at topY (100.0f)
    float pianoRollStepW_{70.0f};
    float pianoRollRowH_{22.0f};
    bool isMiddleMousePanning_{false};
    float panStartMouseX_{0.0f};
    float panStartMouseY_{0.0f};
    float panStartScrollX_{0.0f};
    float panStartScrollY_{0.0f};
    float dragStartScrollX_{0.0f};

    // Virtual Piano Keyboard Drawer (Horizontal)
    bool virtualKeyboardDrawerOpen_{false};
    float virtualKeyboardAnimProgress_{0.0f};
    float virtualKeyboardDrawerHeight_{175.0f};
    int virtualKeyboardBaseOctave_{3};
    int virtualKeyboardActivePitch_{-1};

    // Modular Mixer Section Flags & Ballistics
    bool showMixerRouting_{true};
    bool showMixerAutomation_{true};
    bool showMixerPan_{true};
    bool showMixerFaders_{true};
    bool showMixerMeters_{true};
    bool showMixerButtons_{true};
    bool showMixerReadouts_{true};
    float masterPeakL_{0.0f};
    float masterPeakR_{0.0f};
    std::array<float, 8> chPeakL_{};
    std::array<float, 8> chPeakR_{};
    bool needsRedraw_{true};
    float frameDt_{0.016f};
    uint64_t frameIndex_{0};
    presenter::TelemetryPresenter telemetryPresenter_;

    // Modular Views & Ubiquitous Widgets (Option B)
    std::unique_ptr<ArrangerView> modularArrangerView_;
    std::unique_ptr<EditView> modularEditView_;
    std::unique_ptr<TrackInspectorView> modularTrackInspectorView_;
    std::unique_ptr<MixerView> modularMixerView_;
    std::unique_ptr<DesignView> modularDesignView_;
    std::unique_ptr<VirtualKeyboardDrawer> virtualKeyboardDrawerWidget_;
    std::unique_ptr<ProjectBrowserDrawer> projectBrowserDrawerWidget_;
    std::unique_ptr<TransportHeader> transportHeaderWidget_;
    std::unique_ptr<BottomNavBar> bottomNavBarWidget_;
    std::unique_ptr<TerminalConsoleDrawer> terminalConsoleDrawerWidget_;
    ValueEditDialog valueEditDialog_;
    CommandPaletteDialog commandPaletteDialog_;
    AudioToMidiDialog audioToMidiDialog_;
    PresetSearchDialog presetSearchDialog_;
    FullscreenDeviceModal fullscreenDeviceModal_;
    DeviceTarget lastFocusedDevice_{DeviceTargetType::Instrument, 0, -1, ""};
    KineticScroller kineticScroller_;
    GestureRecognizer gestureRecognizer_;

    void renderCrtTweakerModal();
    bool handleCrtTweakerPointer(float x, float y, bool isDown, bool isUp);
    void handleCrtTweakerDrag(float x, float y);
};

} // namespace eatsbits::ui

#endif // EATS_GUI_WINDOW_HPP
