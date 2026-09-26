#pragma once

#include "../geometry.hpp"
#include "../views/view_base.hpp"
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>

namespace eatsbits::ui {

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
    float r{0.0f}, g{0.90f}, b{1.0f};

    // Mix Parameters
    float volume{0.8f};
    float pan{0.0f};
    bool mute{false};
    bool solo{false};
    bool freeze{false};

    // 3-Band Parametric Channel EQ (Eatsbeats parity)
    bool eqEnabled{true};
    float eqHpf{20.0f};        // 20..500 Hz
    float eqLowGain{0.0f};     // -18..+18 dB
    float eqMidFreq{1000.0f};  // 200..8000 Hz
    float eqMidGain{0.0f};     // -18..+18 dB
    float eqMidQ{1.0f};        // 0.3..10.0
    float eqHighGain{0.0f};    // -18..+18 dB

    // Instrument Card Parameters
    std::string instrument{"303 ACID BASS"};
    float knob1{0.55f};
    float knob2{0.75f};
    float knob3{0.45f};
    float knob4{0.80f};
    std::string knob1Name{"TONE"};
    std::string knob2Name{"SNAPPY"};
    std::string knob3Name{"DECAY"};
    std::string knob4Name{"VAR"};

    // MIDI & Audio FX Racks
    std::vector<TrackMidiFxItem> midiFx;
    std::vector<TrackAudioFxItem> audioFx;

    // Master Bus Parameters
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
    InstrumentKnob1,
    InstrumentKnob2,
    InstrumentKnob3,
    InstrumentKnob4,
    ChangeInstrument,
    AddMidiFx,
    AddAudioFx,
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
    ClipDuplicate
};

struct TrackPropertiesHitResult {
    bool hit{false};
    TrackPropertiesHitArea area{TrackPropertiesHitArea::None};
    int index{0};
    float normVal{0.0f};
};

class TrackPropertiesDrawer {
public:
    TrackPropertiesDrawer();
    ~TrackPropertiesDrawer() = default;

    void layout(const Rect2D& containerBounds, float browserOffset = 0.0f);
    void render(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data, float mouseX = -1.0f, float mouseY = -1.0f);
    [[nodiscard]] TrackPropertiesHitResult hitTest(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept;
    bool handlePointer(const PointerEvent& ev, TrackPropertiesDrawerData& data, const ViewContext& ctx);

    [[nodiscard]] bool isExpanded() const noexcept { return isExpanded_; }
    void setExpanded(bool exp) noexcept { isExpanded_ = exp; }
    void toggle() noexcept { isExpanded_ = !isExpanded_; }

    [[nodiscard]] float getWidth() const noexcept { return width_; }
    void setWidth(float w) noexcept { width_ = std::clamp(w, minWidth_, maxWidth_); }

    [[nodiscard]] const Rect2D& getPullTabBounds() const noexcept { return pullTabBounds_; }
    [[nodiscard]] const Rect2D& getDrawerBounds() const noexcept { return drawerBounds_; }
    [[nodiscard]] const Rect2D& getCloseButtonBounds() const noexcept { return closeButtonBounds_; }

    [[nodiscard]] float getScrollY() const noexcept { return scrollY_; }
    void setScrollY(float sy) noexcept { scrollY_ = std::max(0.0f, sy); }

    static constexpr float kPullTabWidth = 24.0f;
    static constexpr float kDefaultWidth = 360.0f;
    static constexpr float kMinWidth = 260.0f;
    static constexpr float kMaxWidth = 640.0f;

private:
    void renderTrackSection(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data, float contentX, float contentY, float contentW, float mouseX, float mouseY);
    void renderMasterSection(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data, float contentX, float contentY, float contentW, float mouseX, float mouseY);
    void renderClipSection(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data, float contentX, float contentY, float contentW, float mouseX, float mouseY);

    bool isExpanded_{true};
    float width_{kDefaultWidth};
    float minWidth_{kMinWidth};
    float maxWidth_{kMaxWidth};
    float scrollY_{0.0f};

    Rect2D containerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D pullTabBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D drawerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D closeButtonBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Drag resize tracking
    bool isResizing_{false};
    float resizeStartX_{0.0f};
    float resizeStartWidth_{kDefaultWidth};
};

} // namespace eatsbits::ui
