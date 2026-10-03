#pragma once

#include "../geometry.hpp"
#include "../views/view_base.hpp"
#include "plugin_search_dialog.hpp"
#include "../gui_panel_def.hpp"
#include "eatsbits/theory/chord_model.hpp"

#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>
#include <memory>
#include <chrono>

namespace eatsbits::ui {

using ChordFollowMode = eatsbits::theory::ChordFollowMode;

enum class TrackPropertiesTab {
    Track,
    Clip,
    Master
};

struct TrackPropertiesKnob {
    std::string name;
    std::string label;
    float value{0.5f};
    std::string display{"50%"};
    std::string unit{""};
};

using InspectorKnobDef = TrackPropertiesKnob;

struct TrackMidiFxItem {
    std::string id{"scale_snap"};
    std::string name{"Scale Snap"};
    std::string type{"SCALE_SNAP"};
    int rootKey{0};
    int scaleMode{0}; // 0 = Major, 1 = Natural Minor, etc.
    bool enabled{true};
    bool isExpanded{true};
    std::string background{"dark"};
    float accentR{1.0f}, accentG{0.75f}, accentB{0.2f};
    std::vector<TrackPropertiesKnob> knobs;

    TrackMidiFxItem() = default;
    TrackMidiFxItem(std::string n, std::string t, bool en = true)
        : id(t), name(std::move(n)), type(std::move(t)), enabled(en) {
        ensureDefaultKnobs();
    }
    TrackMidiFxItem(std::string n, std::string t, int rk, int sm, bool en = true)
        : id(t), name(std::move(n)), type(std::move(t)), rootKey(rk), scaleMode(sm), enabled(en) {
        ensureDefaultKnobs();
    }

    void ensureDefaultKnobs() {
        if (!knobs.empty()) return;
        std::string tUpper = type + " " + name;
        for (char& c : tUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (tUpper.find("ARP") != std::string::npos) {
            knobs = {
                {"rate", "RATE", 0.5f, "1/16", ""},
                {"pattern", "PATTERN", 0.0f, "UP", ""},
                {"octaves", "OCTAVES", 0.33f, "2", "oct"},
                {"gate", "GATE", 0.75f, "75%", ""}
            };
        } else if (tUpper.find("HUMANIZE") != std::string::npos) {
            knobs = {
                {"timing", "TIMING", 0.25f, "15 ms", "ms"},
                {"velocity", "VELOCITY", 0.35f, "20%", "%"},
                {"swing", "SWING", 0.50f, "50%", "%"},
                {"chance", "CHANCE", 1.0f, "100%", "%"}
            };
        } else { // SCALE_SNAP / generic
            knobs = {
                {"root", "ROOT", static_cast<float>(rootKey) / 11.0f, "C", ""},
                {"scale", "SCALE", static_cast<float>(scaleMode) / 6.0f, "Minor", ""},
                {"snap", "SNAP", 1.0f, "100%", "%"},
                {"transpose", "TRANS", 0.5f, "0 st", "st"}
            };
        }
    }
};

struct TrackAudioFxItem {
    std::string id{"bitcrusher"};
    std::string name{"8-Bit Crusher"};
    std::string type{"BITCRUSHER"};
    float drive{0.5f};
    float mix{0.8f};
    bool enabled{true};
    bool isExpanded{true};
    std::string background{"snes"}; // "snes", "dark", "grunge", "silver", "minimal_white"
    float accentR{0.13f}, accentG{0.75f}, accentB{1.0f};
    std::vector<TrackPropertiesKnob> knobs;

    TrackAudioFxItem() = default;
    TrackAudioFxItem(std::string n, std::string t, float d = 0.5f, float m = 0.8f, bool en = true)
        : id(t), name(std::move(n)), type(std::move(t)), drive(d), mix(m), enabled(en) {
        ensureDefaultKnobs();
    }

    void ensureDefaultKnobs() {
        if (!knobs.empty()) return;
        std::string tUpper = type + " " + name;
        for (char& c : tUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (tUpper.find("CRUSH") != std::string::npos || tUpper.find("8-BIT") != std::string::npos || tUpper.find("BITCRUSHER") != std::string::npos) {
            background = "snes";
            knobs = {
                {"Bits", "BITS", (8.0f - 1.0f) / 15.0f, "8 bit", "bit"},
                {"Downsample", "CRUSH", 0.0f, "1x", "x"},
                {"Drive", "DRIVE", (1.0f - 0.5f) / 3.5f, "1.0 x", "x"},
                {"Mix", "MIX", mix, "1.0", ""}
            };
        } else if (tUpper.find("CAB") != std::string::npos) {
            background = "grunge";
            knobs = {
                {"Width", "WIDTH", 0.5f, "0.76 m", "m"},
                {"Length", "LENGTH", 0.5f, "0.76 m", "m"},
                {"DryLevel", "DRY", 0.0f, "0.0", ""},
                {"WetLevel", "WET", 0.8f, "1.0", ""}
            };
        } else if (tUpper.find("DELAY") != std::string::npos || tUpper.find("ECHO") != std::string::npos) {
            background = "silver";
            knobs = {
                {"time", "TIME", drive, "375 ms", "ms"},
                {"feedback", "FEEDBK", 0.45f, "45%", "%"},
                {"filter", "DAMP", 0.30f, "3.2 kHz", "kHz"},
                {"mix", "MIX", mix, "30%", "%"}
            };
        } else if (tUpper.find("CHORUS") != std::string::npos || tUpper.find("FLANGER") != std::string::npos) {
            background = "snes";
            knobs = {
                {"rate", "RATE", 0.35f, "1.2 Hz", "Hz"},
                {"depth", "DEPTH", drive, "40%", "%"},
                {"feedback", "FEEDBK", 0.20f, "20%", "%"},
                {"mix", "MIX", mix, "35%", "%"}
            };
        } else if (tUpper.find("CONVOLVER") != std::string::npos || tUpper.find("REVERB") != std::string::npos) {
            background = "dark";
            knobs = {
                {"size", "SIZE", drive, "Medium", ""},
                {"decay", "DECAY", 0.5f, "2.4 s", "s"},
                {"predelay", "PREDLY", 0.2f, "25 ms", "ms"},
                {"mix", "MIX", mix, "35%", "%"}
            };
        } else if (tUpper.find("COMP") != std::string::npos || tUpper.find("DYNAMICS") != std::string::npos) {
            background = "dark";
            knobs = {
                {"thresh", "THRESH", 0.65f, "-18 dB", "dB"},
                {"ratio", "RATIO", 0.45f, "4:1", ""},
                {"attack", "ATTACK", 0.20f, "12 ms", "ms"},
                {"gain", "GAIN", mix, "+2.5 dB", "dB"}
            };
        } else { // DISTORTION / TUBE_DISTORTION / generic
            background = "grunge";
            knobs = {
                {"drive", "DRIVE", drive, "50%", "%"},
                {"tone", "TONE", 0.60f, "60%", "%"},
                {"bias", "BIAS", 0.40f, "40%", "%"},
                {"mix", "MIX", mix, "80%", "%"}
            };
        }
    }
};

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
    std::string instrument{""};
    std::string instrumentEngine{""}; // "tb303", "tr808", "tr909", "dx7", "piano", "snes", "c64", "convolver", "synth"
    std::vector<TrackPropertiesKnob> knobs;
    size_t activePresetIdx{0};
    size_t totalPresets{1};
    std::string presetTitle{""};
    std::string presetSubtitle{""};
    bool instrumentExpanded{true};

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
        for (auto& fx : audioFx) fx.ensureDefaultKnobs();
        for (auto& fx : midiFx) fx.ensureDefaultKnobs();
        for (auto& fx : masterAudioFx) fx.ensureDefaultKnobs();
        if (!knobs.empty()) return;
        if (instrumentEngine == "tb303" || instrument.find("303") != std::string::npos || presetTitle.find("303") != std::string::npos) {
            knobs = {
                {"waveform", "WAVEFORM", 0.0f, "Saw"},
                {"pitch", "PITCH", 0.50f, "0 st"},
                {"cutoff", "CUTOFF", knob1, std::to_string(static_cast<int>(std::round(knob1 * 100.0f))) + "%"},
                {"resonance", "RESONANCE", knob2, std::to_string(static_cast<int>(std::round(knob2 * 100.0f))) + "%"},
                {"envMod", "ENV MOD", 0.60f, "+2.4 oct"},
                {"decay", "DECAY", knob3, std::to_string(static_cast<int>(std::round(knob3 * 500.0f))) + " ms"},
                {"accent", "ACCENT", knob4, std::to_string(static_cast<int>(std::round(knob4 * 100.0f))) + "%"},
                {"octave", "OCTAVE", 0.50f, "0"},
                {"subOsc", "SUB OSC", 0.0f, "Off"},
                {"subVol", "SUB VOL", 0.35f, "35%"},
                {"glideCurve", "GLIDE CURVE", 0.40f, "40%"},
                {"drive", "DRIVE", 0.25f, "25%"}
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
        } else if (instrumentEngine == "piano" || instrumentEngine == "piano_physical") {
            knobs = {
                {"stiffness", "STIFF", 0.50f, "50%"},
                {"hammer", "HAMMER", 0.65f, "Hard"},
                {"decay", "DECAY", 0.70f, "2.2 s"},
                {"damping", "DAMP", 0.30f, "30%"},
                {"pedal", "PEDAL", 0.0f, "Off"},
                {"reverb", "REVERB", 0.40f, "40%"}
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
    TrackIcon,
    DesignButton,
    PresetButton,
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
    ToggleInstrumentExpand,
    ChordFollowChip,
    BakeChords,
    AddMidiFx,
    AddAudioFx,
    FullscreenMidiFx,
    FullscreenAudioFx,
    ToggleMidiFx,
    ToggleAudioFx,
    ToggleAudioFxExpand,
    MoveAudioFxUp,
    MoveAudioFxDown,
    ToggleMidiFxExpand,
    MoveMidiFxUp,
    MoveMidiFxDown,
    RemoveMidiFx,
    RemoveAudioFx,
    AudioFxKnob,
    MidiFxKnob,
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
    bool handleChar(char32_t codepoint) {
        if (pluginDialog_.isOpen()) {
            return pluginDialog_.handleChar(codepoint);
        }
        return false;
    }

    void setShowTrackRibbon(bool show) noexcept { showTrackRibbon_ = show; }
    [[nodiscard]] bool getShowTrackRibbon() const noexcept { return showTrackRibbon_; }

    [[nodiscard]] float getScrollY() const noexcept { return scrollY_; }
    void setScrollY(float sy) noexcept { scrollY_ = std::max(0.0f, sy); }
    [[nodiscard]] float getTotalContentHeight() const noexcept { return totalContentHeight_; }

    [[nodiscard]] const Rect2D& getBounds() const noexcept { return bounds_; }
    [[nodiscard]] const Rect2D& getHeaderCardBounds() const noexcept { return headerCardBounds_; }
    [[nodiscard]] bool isDragging() const noexcept { return dragMode_ != DragMode::None; }

    void syncGuiPanelFromTrackData(const TrackPropertiesDrawerData& data);
    [[nodiscard]] const GuiPanelDef& getGuiPanel() const noexcept { return guiPanel_; }
    [[nodiscard]] GuiPanelDef& getGuiPanel() noexcept { return guiPanel_; }
    [[nodiscard]] std::string getTooltip(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept;
    void setIsInsideDrawer(bool inside) noexcept { isInsideDrawer_ = inside; }
    [[nodiscard]] bool isInsideDrawer() const noexcept { return isInsideDrawer_; }

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
    std::function<void(uint32_t trackIndex)> onOpenDesign;
    std::function<void(uint32_t trackIndex)> onOpenPresets;
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
    std::function<void(uint32_t trackIndex, size_t fxIndex, bool enabled)> onToggleMidiFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex, bool enabled)> onToggleAudioFx;
    std::function<void(uint32_t trackIndex, size_t fromIdx, size_t toIdx)> onReorderMidiFx;
    std::function<void(uint32_t trackIndex, size_t fromIdx, size_t toIdx)> onReorderAudioFx;
    std::function<void(uint32_t trackIndex)> onMidiFxChanged;
    std::function<void(uint32_t trackIndex)> onAudioFxChanged;
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
    bool executeHitAction(const TrackPropertiesHitResult& hit, TrackPropertiesDrawerData& data, const ViewContext& ctx);
    bool openValueEditForHit(const TrackPropertiesHitResult& hit, TrackPropertiesDrawerData& data,
                             const std::function<void(const ValueEditRequest&)>& onOpenValueEdit);
    void update(float dt) noexcept;

    enum class DragMode {
        None,
        VolumeSlider,
        PanKnob,
        PanSlider = PanKnob,
        InstrumentKnob,
        EqKnob,
        AudioFxKnob,
        MidiFxKnob,
        Scrollbar,
        TouchScroll
    } dragMode_{DragMode::None};

    KineticScroller scroller_;
    float touchStartY_{0.0f};
    float touchStartX_{0.0f};
    float touchStartScrollY_{0.0f};
    bool touchDragCommitted_{false};
    double touchStartTimeMs_{0.0};
    TrackPropertiesHitResult pendingHitResult_;

    // Mobile Long-Press / Touch State
    bool isLongPressActive_{false};
    float longPressTimer_{0.0f};
    std::chrono::steady_clock::time_point touchDownTimePoint_{};
    Point2D longPressPos_{0.0f, 0.0f};
    TrackPropertiesHitResult longPressHit_{};
    std::function<void(const ValueEditRequest& req)> longPressOpenValueEdit_;

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
    size_t activeFxIndex_{0};
    std::string activeFxParam_{""};

    GuiPanelDef guiPanel_;
    int draggingRow_{-1};
    int draggingWidget_{-1};

    PluginSearchDialog pluginDialog_;
    bool isInsideDrawer_{false};
};

} // namespace eatsbits::ui
