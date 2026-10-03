#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "eatsbits/presenter/scalar_drag_presenter.hpp"
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <algorithm>
#include <chrono>

namespace eatsbits::audio {
    class AudioEngine;
}

namespace eatsbits::ui {

struct ArrangerTimelineTrack;
struct ViewContext;

/**
 * ArrangerMixerDrawer: Collapsible sliding bottom mixer console docked in the Arranger tab.
 *
 * Provides full mixing console capabilities directly inside the composition timeline:
 * - Expandable pull-tab docked at the bottom of the Arranger workspace (toggled via hotkey 'M').
 * - Sliding vertical animation with exponential damping matching VirtualKeyboardDrawer.
 * - Top drag handle allowing vertical resize between 140px and 340px.
 * - Master Channel strip on left with volume fader, dB audio taper, mute, and peak meters.
 * - Horizontally scrollable track channel strips aligned with project tracks.
 * - Each track strip provides:
 *   - Track color strip, number, and name header.
 *   - Pan knob / control with center detent ("C", "Lxx", "Rxx").
 *   - Long-throw fader with audio-taper dB scaling, unity 0 dB at 0.75, double-click reset to unity.
 *   - Dual stereo peak meter (L/R) with LED ladder colors.
 *   - Mute ('M') and Solo ('S') buttons.
 *   - Live formatted dB readout.
 *   - Track selection synchronization with Arranger track properties.
 */
class ArrangerMixerDrawer {
public:
    ArrangerMixerDrawer();
    ~ArrangerMixerDrawer() = default;

    void layout(const Rect2D& containerBounds, float rightMargin = 0.0f);
    void update(float dt) noexcept;
    void render(BatchRenderer2D& r, const ThemeTokens& theme,
                const std::vector<ArrangerTimelineTrack>& tracks,
                uint32_t activeTrackIndex,
                float masterVol, float masterPan, bool masterMute,
                float masterPeakL, float masterPeakR,
                const float* chPeaksL, const float* chPeaksR, size_t numPeaks,
                float mouseX = -1.0f, float mouseY = -1.0f);

    bool handlePointer(const PointerEvent& ev,
                       std::vector<ArrangerTimelineTrack>& tracks,
                       uint32_t& activeTrackIndex,
                       float& masterVol, float& masterPan, bool& masterMute,
                       const ViewContext& ctx);

    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx);

    [[nodiscard]] bool isExpanded() const noexcept { return isExpanded_; }
    void setExpanded(bool exp) noexcept { isExpanded_ = exp; }
    void toggle() noexcept { isExpanded_ = !isExpanded_; }

    [[nodiscard]] float getAnimProgress() const noexcept { return animProgress_; }
    [[nodiscard]] bool isAnimating() const noexcept {
        return std::abs(animProgress_ - (isExpanded_ ? 1.0f : 0.0f)) > 0.001f;
    }

    [[nodiscard]] float getHeight() const noexcept { return height_; }
    void setHeight(float h) noexcept { height_ = std::clamp(h, kMinHeight, kMaxHeight); }

    [[nodiscard]] const Rect2D& getPullTabBounds() const noexcept { return pullTabBounds_; }
    [[nodiscard]] const Rect2D& getDrawerBounds() const noexcept { return drawerBounds_; }
    [[nodiscard]] const Rect2D& getCloseButtonBounds() const noexcept { return closeBtnBounds_; }

    [[nodiscard]] bool isDragging() const noexcept {
        return activeFaderIndex_ != -999 || activePanIndex_ != -999 || isDraggingScrollbar_ || isResizing_;
    }

    static constexpr float kPullTabWidth = 160.0f;
    static constexpr float kPullTabHeight = 22.0f;
    static constexpr float kDefaultHeight = 195.0f;
    static constexpr float kMinHeight = 140.0f;
    static constexpr float kMaxHeight = 340.0f;
    static constexpr float kMasterStripWidth = 84.0f;
    static constexpr float kChannelWidth = 88.0f;
    static constexpr float kChannelGap = 6.0f;

    // Callbacks
    std::function<void(uint32_t trackIndex)> onTrackSelected;
    std::function<void(uint32_t trackIndex, float volume)> onVolumeChanged;
    std::function<void(uint32_t trackIndex, float pan)> onPanChanged;
    std::function<void(uint32_t trackIndex, bool mute)> onMuteToggled;
    std::function<void(uint32_t trackIndex, bool solo)> onSoloToggled;
    std::function<void(float masterVol)> onMasterVolumeChanged;
    std::function<void(float masterPan)> onMasterPanChanged;
    std::function<void(bool masterMute)> onMasterMuteToggled;

private:
    struct FaderGeometry {
        Rect2D well;
        Rect2D thumb;
        float travel{0.0f}; // 0.0 at bottom, 1.0 at top
    };

    [[nodiscard]] FaderGeometry computeFaderGeometry(float wellX, float wellY, float wellW, float wellH, float gain) const;

    bool isExpanded_{false};
    float animProgress_{0.0f};
    float height_{kDefaultHeight};
    float scrollX_{0.0f};
    float maxScrollX_{0.0f};

    Rect2D containerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    float rightMargin_{0.0f};
    Rect2D drawerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D pullTabBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D headerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D closeBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D resizeHandleBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D masterStripBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D channelsViewportBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D scrollBarBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D scrollThumbBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Interaction states
    // -1 = Master, >= 0 = Track index, -999 = None
    int activeFaderIndex_{-999};
    float dragStartY_{0.0f};
    float dragStartGain_{1.0f};

    int activePanIndex_{-999};
    float dragStartX_{0.0f};
    float dragStartPan_{0.0f};

    bool isResizing_{false};
    float resizeStartY_{0.0f};
    float initialResizeH_{kDefaultHeight};

    bool isDraggingScrollbar_{false};
    float scrollbarDragStartX_{0.0f};
    float scrollbarDragStartScrollX_{0.0f};

    // Double-click detection for faders and pan
    std::chrono::steady_clock::time_point lastFaderClickTime_{};
    int lastFaderClickedIndex_{-999};
    std::chrono::steady_clock::time_point lastPanClickTime_{};
    int lastPanClickedIndex_{-999};
};

} // namespace eatsbits::ui
