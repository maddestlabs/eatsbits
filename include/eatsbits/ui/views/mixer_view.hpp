#pragma once

#include "view_base.hpp"
#include "../widgets/track_properties_drawer.hpp"
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

namespace eatsbits::ui {

enum class MixerMasterPosition {
    Left,
    Right
};

enum class ModularMixerPreset {
    Full,
    FadersOnly,
    FadersAndMeters
};

struct MixerChannelStrip {
    std::string name;
    std::string type{"SYNTH"};
    float r{0.0f}, g{0.85f}, b{1.0f};
    float fader{0.80f};
    float pan{0.0f};
    bool mute{false};
    bool solo{false};
    bool freeze{false};
    bool phaseInvert{false};
    bool fxIn{false};
    int automationMode{0}; // 0: TRIM, 1: READ, 2: TOUCH, 3: LATCH

    // Audio & Peak meters
    float peakL{0.0f};
    float peakR{0.0f};
    float peakHoldL{0.0f};
    float peakHoldR{0.0f};

    // 3-Band Parametric Channel EQ (Eatsbeats parity)
    bool eqEnabled{true};
    float eqHpf{20.0f};        // 20..500 Hz
    float eqLowGain{0.0f};     // -18..+18 dB
    float eqMidFreq{1000.0f};  // 200..8000 Hz
    float eqMidGain{0.0f};     // -18..+18 dB
    float eqMidQ{1.0f};        // 0.3..10.0
    float eqHighGain{0.0f};    // -18..+18 dB

    // Hardware parameters
    std::string instrument{"SYNTH"};
    float knob1{0.55f};
    float knob2{0.75f};
    float knob3{0.45f};
    float knob4{0.80f};
    std::string knob1Name{"TONE"};
    std::string knob2Name{"SNAPPY"};
    std::string knob3Name{"DECAY"};
    std::string knob4Name{"VAR"};

    // FX Racks
    std::vector<TrackMidiFxItem> midiFx;
    std::vector<TrackAudioFxItem> audioFx;
};

/**
 * MixerView: Authentic Studio Mixing Console.
 * Features:
 * - Tactile long-throw faders with dB scale markings (+6, 0, -6, -12, -inf)
 * - Dual multi-segment stereo peak meters with green/amber/red LED ladder & peak-hold
 * - 3-band parametric Channel EQ (HPF, Low shelf, Mid bell, High shelf)
 * - Pinned Master Bus channel strip (support Left or Far-Right pinning)
 *   with True Peak Brickwall Limiter, Master 4-Band EQ, and Convolver Reverb insert
 * - Sliding Track Properties pullout drawer matching Arranger track properties
 * - Section toggles (ROUTING, PAN, BUTTONS, METERS) and modular console presets
 */
class MixerView : public ViewBase {
public:
    MixerView();
    ~MixerView() override = default;

    void layout(const Rect2D& bounds, const ViewContext& ctx) override;
    void render(const ViewContext& ctx) override;
    bool handlePointer(const PointerEvent& ev, const ViewContext& ctx) override;
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) override;

    // Master Bus Controls
    [[nodiscard]] const Rect2D& getMasterBounds() const noexcept { return masterBounds_; }
    [[nodiscard]] const MixerChannelStrip& getMasterChannel() const noexcept { return masterChannel_; }
    [[nodiscard]] MixerChannelStrip& getMasterChannel() noexcept { return masterChannel_; }
    void setMasterFader(float f) noexcept { masterChannel_.fader = f; }

    [[nodiscard]] MixerMasterPosition getMasterPosition() const noexcept { return masterPosition_; }
    void setMasterPosition(MixerMasterPosition pos) noexcept { masterPosition_ = pos; }

    // Channel Access
    [[nodiscard]] const std::vector<MixerChannelStrip>& getChannels() const noexcept { return channels_; }
    [[nodiscard]] std::vector<MixerChannelStrip>& getChannels() noexcept { return channels_; }

    [[nodiscard]] int getSelectedChannel() const noexcept { return selectedChannel_; }
    void setSelectedChannel(int idx) noexcept { selectedChannel_ = idx; }
    [[nodiscard]] bool isMasterSelected() const noexcept { return isMasterSelected_; }
    void setMasterSelected(bool sel) noexcept { isMasterSelected_ = sel; }

    // Sliding Track Properties Drawer Controls
    [[nodiscard]] bool isPropertiesExpanded() const noexcept { return propertiesDrawer_.isExpanded(); }
    void toggleProperties() noexcept { propertiesDrawer_.toggle(); }
    void setPropertiesExpanded(bool exp) noexcept { propertiesDrawer_.setExpanded(exp); }
    [[nodiscard]] float getPropertiesWidth() const noexcept { return propertiesDrawer_.getWidth(); }
    void setPropertiesWidth(float w) noexcept { propertiesDrawer_.setWidth(w); }
    [[nodiscard]] TrackPropertiesDrawer& getPropertiesDrawer() noexcept { return propertiesDrawer_; }

    // Section Visibility & Presets
    void setPreset(ModularMixerPreset preset) noexcept;
    [[nodiscard]] bool isRoutingVisible() const noexcept { return showRouting_; }
    void setRoutingVisible(bool v) noexcept { showRouting_ = v; }
    [[nodiscard]] bool isPanVisible() const noexcept { return showPan_; }
    void setPanVisible(bool v) noexcept { showPan_ = v; }
    [[nodiscard]] bool isButtonsVisible() const noexcept { return showButtons_; }
    void setButtonsVisible(bool v) noexcept { showButtons_ = v; }
    [[nodiscard]] bool isMetersVisible() const noexcept { return showMeters_; }
    void setMetersVisible(bool v) noexcept { showMeters_ = v; }
    [[nodiscard]] bool isFadersVisible() const noexcept { return showFaders_; }
    void setFadersVisible(bool v) noexcept { showFaders_ = v; }

    // Master Bus Audio Processor Inserts
    [[nodiscard]] bool isMasterLimiterEnabled() const noexcept { return masterLimiterEnabled_; }
    void setMasterLimiterEnabled(bool en) noexcept { masterLimiterEnabled_ = en; }
    [[nodiscard]] float getMasterCeilingDbfs() const noexcept { return masterCeilingDbfs_; }
    void setMasterCeilingDbfs(float c) noexcept { masterCeilingDbfs_ = c; }
    [[nodiscard]] float getMasterLimiterDrive() const noexcept { return masterLimiterDrive_; }
    void setMasterLimiterDrive(float d) noexcept { masterLimiterDrive_ = d; }
    [[nodiscard]] float getMasterTargetLufs() const noexcept { return masterTargetLufs_; }
    void setMasterTargetLufs(float l) noexcept { masterTargetLufs_ = l; }

    // Master 4-Band EQ
    [[nodiscard]] float getMasterSubCut() const noexcept { return masterSubCut_; }
    void setMasterSubCut(float s) noexcept { masterSubCut_ = s; }
    [[nodiscard]] float getMasterLowGain() const noexcept { return masterLowGain_; }
    void setMasterLowGain(float g) noexcept { masterLowGain_ = g; }
    [[nodiscard]] float getMasterMidGain() const noexcept { return masterMidGain_; }
    void setMasterMidGain(float g) noexcept { masterMidGain_ = g; }
    [[nodiscard]] float getMasterHighGain() const noexcept { return masterHighGain_; }
    void setMasterHighGain(float g) noexcept { masterHighGain_ = g; }

    std::function<void(uint32_t channelIdx, bool mute)> onMuteToggled;
    std::function<void(uint32_t channelIdx, bool solo)> onSoloToggled;
    std::function<void(uint32_t channelIdx)> onTrackSelected;
    std::function<void(uint32_t channelIdx, const std::string& newName)> onTrackRename;
    std::function<void(uint32_t channelIdx)> onChooseTrackIcon;

    // Synchronization with DAW Window & Audio Engine
    void syncFromWindow(
        const std::vector<std::string>& trackNames,
        const std::vector<float>& trackVols,
        const std::vector<float>& trackPans,
        const std::vector<bool>& trackMutes,
        const std::vector<bool>& trackSolos,
        const std::vector<bool>& trackFreezes,
        const std::vector<Color>& trackColors,
        uint32_t selectedTrackIdx,
        float masterVol, float masterPanVal, bool masterMuteVal,
        float masterPeakLeft, float masterPeakRight,
        const float* chPeaksL, const float* chPeaksR, size_t numPeaks,
        bool propertiesExpanded, float propertiesWidth,
        bool showRouting, bool showPan, bool showButtons, bool showMeters,
        bool showAutomation, bool showReadouts, bool browserOpen, float scrollY);

private:
    void renderToolbar(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderMasterStrip(BatchRenderer2D& r, const ThemeTokens& theme, const Rect2D& b);
    void renderChannelStrip(BatchRenderer2D& r, const ThemeTokens& theme, MixerChannelStrip& ch,
                            const Rect2D& b, size_t index, bool isSelected);
    void renderLedMeter(BatchRenderer2D& r, const ThemeTokens& theme, float mx, float my, float mw, float mh,
                        float levelL, float levelR, float holdL, float holdR);
    void renderBacklitLcd(BatchRenderer2D& r, float x, float y, float w, float h,
                          const std::string& title, const std::string& leftText, const std::string& rightText,
                          const Color& titleColor);
    void renderRotaryPanKnob(BatchRenderer2D& r, float kx, float ky, float radius, float pan,
                             const Color& accentColor);
    void renderCenterButton(BatchRenderer2D& r, float bx, float by, float bw, float bh);
    void renderTooltip(BatchRenderer2D& r, const ThemeTokens& theme);

    // Layout configuration
    float masterStripWidth_{140.0f};
    float channelStripWidth_{140.0f};
    float channelGap_{10.0f};
    float scrollX_{0.0f};
    MixerMasterPosition masterPosition_{MixerMasterPosition::Left};

    // Bounds
    Rect2D toolbarBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D masterBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D channelsScrollBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // State
    MixerChannelStrip masterChannel_;
    std::vector<MixerChannelStrip> channels_;
    int selectedChannel_{0};
    bool isMasterSelected_{false};

    // Master DSP Inserts
    bool masterEqEnabled_{true};
    float masterSubCut_{25.0f};
    float masterLowGain_{0.0f};
    float masterMidGain_{0.0f};
    float masterHighGain_{0.0f};

    bool masterLimiterEnabled_{true};
    float masterCeilingDbfs_{-0.3f};
    float masterLimiterDrive_{0.0f};
    float masterTargetLufs_{-14.0f};

    std::vector<TrackAudioFxItem> masterAudioFx_{
        {"Convolver Reverb", "CONVOLVER", 0.35f, 0.40f, true},
        {"Dynamic Limiter", "BRICKWALL_LIMITER", 0.80f, 1.00f, true}
    };

    // Modular section flags
    bool showRouting_{true};
    bool showAutomation_{true};
    bool showPan_{true};
    bool showFaders_{true};
    bool showMeters_{true};
    bool showButtons_{true};
    bool showReadouts_{true};

    // Active drag interaction tracking
    int activeFaderIndex_{-2}; // -1 = Master, >= 0 = Channel, -2 = None
    float dragStartY_{0.0f};
    float initialFaderVal_{0.0f};

    int activePanIndex_{-2};
    float dragStartX_{0.0f};
    float initialPanVal_{0.0f};

    // Floating tactile tooltip badge
    bool showTooltip_{false};
    std::string tooltipText_;
    float tooltipX_{0.0f};
    float tooltipY_{0.0f};

    // Decoupled sliding Track Properties Drawer
    TrackPropertiesDrawer propertiesDrawer_;
    TrackPropertiesDrawerData drawerData_;
};

} // namespace eatsbits::ui
