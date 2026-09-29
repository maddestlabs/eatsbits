#pragma once

#include "../geometry.hpp"
#include "../views/view_base.hpp"
#include "plugin_search_dialog.hpp"
#include "eatsbits/theory/chord_model.hpp"

#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>
#include <memory>

namespace eatsbits::ui {

using ChordFollowMode = eatsbits::theory::ChordFollowMode;

enum class TrackPropertiesTab {
    Track,
    Clip,
    Master
};

struct TrackMidiFxItem {
    std::string name{"Scale Snap"};
    std::string type{"SCALE_SNAP"};
    int rootKey{0};
    int scaleMode{0}; // 0 = Major, 1 = Natural Minor, etc.
    bool enabled{true};
};

struct TrackAudioFxItem {
    std::string name{"Tube Distortion"};
    std::string type{"TUBE_DISTORTION"};
    float drive{0.5f};
    float mix{0.8f};
    bool enabled{true};
};

struct TrackPropertiesKnob {
    std::string name;
    std::string label;
    float value{0.5f};
    std::string display{"50%"};
};

using InspectorKnobDef = TrackPropertiesKnob;

struct TrackMidiFxData {
    bool arpEnabled{true};
    int arpPattern{0};    // 0: Up, 1: Down, 2: UpDown, 3: Random, 4: Chord
    float arpRate{1.0f};  // 1.0 = 16th, 0.5 = 8th, 2.0 = 32nd
    int arpOctaves{2};
    float arpGate{0.85f};
    float arpSwing{0.0f};

    bool scaleSnapEnabled{true};
    int scaleRoot{0};     // 0 = C
    bool scaleMinor{true};

    bool humanizeEnabled{true};
    float humanizeTiming{0.15f};    // ±15ms
    float humanizeVelocity{0.20f};  // ±20%
};

struct TrackAudioFxData {
    bool delayEnabled{true};
    float delayTime{0.375f};
    float delayFeedback{0.45f};
    float delayMix{0.30f};

    bool chorusEnabled{false};
    float chorusRate{0.5f};
    float chorusDepth{0.4f};
    float chorusMix{0.35f};

    bool eqEnabled{true};
    float eqLow{0.6f};
    float eqMid{0.5f};
    float eqHigh{0.5f};

    bool compEnabled{true};
    float compThreshold{0.7f};
    float compRatio{0.4f};
    float compGain{0.5f};

    bool convolverEnabled{true};
    int convolverPreset{0}; // 0: Stone Cathedral, 1: Vintage Plate, 2: Warm Hall, etc.
    float convolverMix{0.35f};
};

struct TrackPropertiesDrawerData {
    // Mode
    TrackPropertiesTab tab{TrackPropertiesTab::Track};
    bool isMixerMode{false};
    bool isMasterSelected{false};

    // Track Identity
    uint32_t trackIndex{0};
    uint32_t totalTracks{1};
    std::string trackName{"Track"};
    std::string trackType{"SYNTH"};
    std::string iconRef{""};
    float r{0.0f}, g{0.90f}, b{1.0f};

    // Mix Parameters
    float volume{0.8f};
    float pan{0.0f};
    bool mute{false};
    bool solo{false};
    bool freeze{false};

    // 3-Band Parametric Channel EQ (Mixer specific)
    bool eqEnabled{true};
    float eqHpf{20.0f};        // 20..500 Hz
    float eqLowGain{0.0f};     // -18..+18 dB
    float eqMidFreq{1000.0f};  // 200..8000 Hz
    float eqMidGain{0.0f};     // -18..+18 dB
    float eqMidQ{1.0f};        // 0.3..10.0
    float eqHighGain{0.0f};    // -18..+18 dB

    // Dynamic Instrument Parameters
    std::string instrument{"Roland TB-303"};
    std::string instrumentEngine{"tb303"}; // "tb303", "tr808", "tr909", "dx7", "snes", "c64", "convolver", "synth"
    std::vector<TrackPropertiesKnob> knobs;
    size_t activePresetIdx{0};
    size_t totalPresets{1};
    std::string presetTitle{"TB-303 Acid Bassline"};
    std::string presetSubtitle{"Diode Ladder Synthesizer"};

    // Harmonic Chord Track Follow
    ChordFollowMode chordFollowMode{ChordFollowMode::Off};

    // Live Oscilloscope Audio Buffer
    const float* scopeBuffer{nullptr};
    size_t scopeBufferCount{0};

    // MIDI & Audio FX Pipeline Data
    TrackMidiFxData midiFxData;
    TrackAudioFxData audioFxData;

    // Legacy fallback parameters
    float knob1{0.55f};
    float knob2{0.75f};
    float knob3{0.45f};
    float knob4{0.80f};
    std::string knob1Name{"TONE"};
    std::string knob2Name{"SNAPPY"};
    std::string knob3Name{"DECAY"};
    std::string knob4Name{"VAR"};

    // Custom MIDI & Audio FX Racks
    std::vector<TrackMidiFxItem> midiFx;
    std::vector<TrackAudioFxItem> audioFx;

    // Master Bus Parameters (Mixer Master Console)
    bool masterEqEnabled{true};
    float masterSubCut{25.0f};    // 20..45 Hz
    float masterLowGain{0.0f};   // -12..+12 dB
    float masterMidGain{0.0f};   // -12..+12 dB
    float masterHighGain{0.0f};  // -12..+12 dB

    bool masterLimiterEnabled{true};
    float masterCeilingDbfs{-0.3f}; // -2.0..0.0 dB
    float masterLimiterDrive{0.0f}; // 0..12 dB
    float masterTargetLufs{-14.0f}; // -24..-6 LUFS

    std::vector<TrackAudioFxItem> masterAudioFx{
        {"Convolver Reverb", "CONVOLVER", 0.35f, 0.40f, true},
        {"Dynamic Limiter", "BRICKWALL_LIMITER", 0.80f, 1.00f, true}
    };

    // Clip Parameters (Arranger Mode)
    int selectedClipIndex{-1};
    std::string clipName{"Clip 1"};
    uint32_t clipStartBar{1};
    uint32_t clipLengthBars{4};
    bool clipLooped{false};
    uint32_t clipLoopLengthBars{4};
    int clipTranspose{0};

    // Multi-track info for top ribbon (when showRibbon is active)
    std::vector<std::string> allTrackNames;
    std::vector<Color> allTrackColors;

    void syncKnobsIfEmpty() {
        if (!knobs.empty()) return;
        if (instrumentEngine == "tb303") {
            knobs = {
                {"tuning", "TUNING", 0.50f, "440 Hz"},
                {"cutoff", "CUTOFF", knob1, std::to_string(static_cast<int>(std::round(knob1 * 100.0f))) + "%"},
                {"resonance", "RESON", knob2, std::to_string(static_cast<int>(std::round(knob2 * 100.0f))) + "%"},
                {"envMod", "ENV MOD", 0.60f, "+2.4 oct"},
                {"decay", "DECAY", knob3, std::to_string(static_cast<int>(std::round(knob3 * 500.0f))) + " ms"},
                {"accent", "ACCENT", knob4, std::to_string(static_cast<int>(std::round(knob4 * 100.0f))) + "%"}
            };
        } else if (instrumentEngine == "tr808") {
            knobs = {
                {"tone", "TONE", knob1, std::to_string(static_cast<int>(std::round(knob1 * 100.0f))) + "%"},
                {"snappy", "SNAPPY", knob2, std::to_string(static_cast<int>(std::round(knob2 * 100.0f))) + "%"},
                {"decay", "DECAY", knob3, std::to_string(static_cast<int>(std::round(knob3 * 1000.0f))) + " ms"},
                {"tuning", "TUNING", knob4, "55 Hz"},
                {"drive", "DRIVE", 0.35f, "15%"},
                {"punch", "PUNCH", 0.75f, "+3 dB"}
            };
        } else if (instrumentEngine == "tr909") {
            knobs = {
                {"attack", "ATTACK", knob1, "12 ms"},
                {"punch", "PUNCH", knob2, "+4 dB"},
                {"tune", "TUNE", knob3, "62 Hz"},
                {"crack", "CRACK", knob4, "60%"},
                {"decay", "DECAY", 0.55f, "450 ms"},
                {"snap", "SNAP", 0.75f, "75%"}
            };
        } else if (instrumentEngine == "dx7") {
            knobs = {
                {"algo", "ALGO", 0.35f, "Algo 5"},
                {"feedback", "FEEDBACK", 0.65f, "Lvl 6"},
                {"attack", "ATTACK", knob1, "8 ms"},
                {"decay", "DECAY", knob2, "1.4 s"},
                {"bright", "BRIGHT", knob3, "60%"},
                {"detune", "DETUNE", knob4, "+12 ct"}
            };
        } else {
            knobs = {
                {"param1", knob1Name, knob1, std::to_string(static_cast<int>(std::round(knob1 * 100.0f))) + "%"},
                {"param2", knob2Name, knob2, std::to_string(static_cast<int>(std::round(knob2 * 100.0f))) + "%"},
                {"param3", knob3Name, knob3, std::to_string(static_cast<int>(std::round(knob3 * 100.0f))) + "%"},
                {"param4", knob4Name, knob4, std::to_string(static_cast<int>(std::round(knob4 * 100.0f))) + "%"},
                {"decay", "DECAY", 0.55f, "55%"},
                {"output", "OUTPUT", 0.80f, "80%"}
            };
        }
    }
};

enum class TrackPropertiesHitArea {
    None,
    PullTab,
    CloseButton,
    TabTrack,
    TabClip,
    MuteButton,
    SoloButton,
    FreezeButton,
    RenameButton,
    VolumeSlider,
    PanKnob,
    PanSlider = PanKnob,
    CodeButton,
    ColorSwatch,
    EqToggle,
    EqReset,
    EqHpf,
    EqLowGain,
    EqHighGain,
    EqMidFreq,
    EqMidGain,
    EqMidQ,
    InstrumentPrevPreset,
    InstrumentNextPreset,
    InstrumentKnob,
    ChangeInstrument,
    FullscreenInstrument,
    ChordFollowChip,
    BakeChords,
    AddMidiFx,
    AddAudioFx,
    FullscreenMidiFx,
    FullscreenAudioFx,
    ToggleMidiFx,
    ToggleAudioFx,
    RemoveMidiFx,
    RemoveAudioFx,
    ReorderLeft,
    ReorderRight,
    MasterEqSubCut,
    MasterEqLow,
    MasterEqMid,
    MasterEqHigh,
    MasterLimiterToggle,
    MasterCeiling,
    MasterDrive,
    MasterLufs,
    AutoMasterButton,
    ClipLoopToggle,
    ClipTransposeUp,
    ClipTransposeDown,
    ClipEditInPianoRoll,
    ClipDuplicate,
    ScrollbarThumb,
    ScrollbarTrack
};

struct TrackPropertiesHitResult {
    bool hit{false};
    TrackPropertiesHitArea area{TrackPropertiesHitArea::None};
    int index{0};
    float normVal{0.0f};
};

/**
 * TrackPropertiesPanel: Canonical unified component for Track Channel Hardware Faceplate,
 * Mixer Settings, Parametric EQ, Harmonic Chord Follow, and Audio/MIDI FX Pipeline Rack.
 *
 * Shared identically across:
 * 1. Arranger View (Sliding sidebar drawer)
 * 2. Mixer View (Sliding sidebar drawer)
 * 3. Track Tab (Dedicated full-screen workspace)
 */
class TrackPropertiesPanel {
public:
    TrackPropertiesPanel();
    ~TrackPropertiesPanel() = default;

    void layout(const Rect2D& bounds, const ViewContext& ctx);
    void render(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                float mouseX = -1.0f, float mouseY = -1.0f);
    [[nodiscard]] TrackPropertiesHitResult hitTest(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept;
    bool handlePointer(const PointerEvent& ev, TrackPropertiesDrawerData& data, const ViewContext& ctx);
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx);

    void setShowTrackRibbon(bool show) noexcept { showTrackRibbon_ = show; }
    [[nodiscard]] bool getShowTrackRibbon() const noexcept { return showTrackRibbon_; }

    [[nodiscard]] float getScrollY() const noexcept { return scrollY_; }
    void setScrollY(float sy) noexcept { scrollY_ = std::max(0.0f, sy); }
    [[nodiscard]] float getTotalContentHeight() const noexcept { return totalContentHeight_; }

    [[nodiscard]] const Rect2D& getBounds() const noexcept { return bounds_; }
    [[nodiscard]] const Rect2D& getHeaderCardBounds() const noexcept { return headerCardBounds_; }

    // Embedded PluginSearchDialog modal overlay
    [[nodiscard]] PluginSearchDialog& getPluginSearchDialog() noexcept { return pluginDialog_; }
    [[nodiscard]] const PluginSearchDialog& getPluginSearchDialog() const noexcept { return pluginDialog_; }

    // Synchronized Callbacks
    std::function<void(uint32_t trackIndex)> onTrackSelected;
    std::function<void(uint32_t trackIndex)> onTrackRename;
    std::function<void(uint32_t trackIndex, const std::string& newName)> onTrackRenameWithText;
    std::function<void(uint32_t trackIndex)> onChooseTrackIcon;
    std::function<void(uint32_t trackIndex, float volume)> onVolumeChanged;
    std::function<void(uint32_t trackIndex, float pan)> onPanChanged;
    std::function<void(uint32_t trackIndex, bool mute)> onMuteToggled;
    std::function<void(uint32_t trackIndex, bool solo)> onSoloToggled;
    std::function<void(uint32_t trackIndex, bool freeze)> onFreezeToggled;
    std::function<void(uint32_t trackIndex, float r, float g, float b)> onColorChanged;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onParamChanged;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onMidiFxParamChanged;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onAudioFxParamChanged;
    std::function<void(uint32_t trackIndex)> onChangeInstrument;
    std::function<void()> onPrevPreset;
    std::function<void()> onNextPreset;
    std::function<void(uint32_t trackIndex, ChordFollowMode mode)> onChordFollowChanged;
    std::function<void(uint32_t trackIndex)> onBakeChords;
    std::function<void(TrackPropertiesTab newTab)> onTabSelected;
    std::function<void(uint32_t trackIndex, int clipIndex)> onEditInPianoRoll;
    std::function<void(uint32_t trackIndex)> onOpenCodeEditor;
    std::function<void(uint32_t trackIndex)> onOpenFullscreenDevice;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onOpenFullscreenAudioFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onOpenFullscreenMidiFx;
    std::function<void(uint32_t trackIndex)> onAddMidiFx;
    std::function<void(uint32_t trackIndex)> onAddAudioFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onRemoveMidiFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onRemoveAudioFx;
    std::function<void(float scrollY)> onScrollChanged;

private:
    void renderTrackSelectorRibbon(BatchRenderer2D& r, const ThemeTokens& theme, const TrackPropertiesDrawerData& data);
    void renderHeaderCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                          float cx, float cy, float cw, bool isWide, float mouseX = -1.0f, float mouseY = -1.0f);
    void renderMixerControlsCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                                 float cx, float cy, float cw, bool isWide);
    void renderEqCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                      float cx, float cy, float cw);
    void renderFaceplateCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                             float cx, float cy, float cw, bool isWide);
    void renderChordFollowCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                               float cx, float cy, float cw);
    void renderMidiFxCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                          float cx, float cy, float cw, bool isWide);
    void renderAudioFxCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                           float cx, float cy, float cw, bool isWide);
    void renderColorPalette(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                            float cx, float cy, float cw);
    void renderMasterSection(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                             float cx, float cy, float cw, float mouseX, float mouseY);
    void renderClipSection(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                           float cx, float cy, float cw, float mouseX, float mouseY);
    void renderScrollbar(BatchRenderer2D& r, const ThemeTokens& theme);

    enum class DragMode {
        None,
        VolumeSlider,
        PanKnob,
        PanSlider = PanKnob,
        InstrumentKnob,
        EqKnob,
        Scrollbar
    } dragMode_{DragMode::None};

    bool showTrackRibbon_{false};
    Rect2D bounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D ribbonBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D scrollbarBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    float scrollY_{0.0f};
    float totalContentHeight_{850.0f};
    bool needScrollbar_{false};

    // Sub-section layout bounds for hit-testing
    Rect2D headerCardBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D colorCardBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D mixerCardBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D eqCardBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D faceplateBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D chordFollowBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D midiFxBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D audioFxBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D colorPaletteBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Active drag interaction state
    float dragStartY_{0.0f};
    float dragStartX_{0.0f};
    float dragStartVal_{0.0f};
    float dragStartScrollY_{0.0f};
    int activeKnobIndex_{-1};
    std::string activeParamName_{""};

    PluginSearchDialog pluginDialog_;
};

} // namespace eatsbits::ui
