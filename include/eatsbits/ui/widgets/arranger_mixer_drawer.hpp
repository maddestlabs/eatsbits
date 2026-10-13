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

#include "sliding_dock.hpp"

namespace eatsbits::audio {
    class AudioEngine;
}

namespace eatsbits::ui {

struct ArrangerTimelineTrack;
struct ViewContext;

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

    [[nodiscard]] bool isExpanded() const noexcept { return drawer_.isExpanded(); }
    void setExpanded(bool exp) noexcept { drawer_.setExpanded(exp); }
    void toggle() noexcept { drawer_.toggle(); }

    [[nodiscard]] float getAnimProgress() const noexcept { return drawer_.getAnimProgress(); }
    [[nodiscard]] bool isAnimating() const noexcept { return drawer_.isAnimating(); }

    [[nodiscard]] float getHeight() const noexcept { return drawer_.getExtent(); }
    void setHeight(float h) noexcept { drawer_.setExtent(h); }

    [[nodiscard]] const Rect2D& getPullTabBounds() const noexcept { return drawer_.getPullTabBounds(); }
    [[nodiscard]] const Rect2D& getDrawerBounds() const noexcept { return drawer_.getDrawerBounds(); }
    [[nodiscard]] const Rect2D& getCloseButtonBounds() const noexcept { return drawer_.getCloseButtonBounds(); }

    [[nodiscard]] bool isDragging() const noexcept {
        return activeFaderIndex_ != -999 || activePanIndex_ != -999 || isDraggingScrollbar_ || drawer_.isDragging();
    }

    [[nodiscard]] SlidingDrawer& getDrawer() noexcept { return drawer_; }
    [[nodiscard]] const SlidingDrawer& getDrawer() const noexcept { return drawer_; }

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

    SlidingDrawer drawer_;
    float scrollX_{0.0f};
    float maxScrollX_{0.0f};

    Rect2D containerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    float rightMargin_{0.0f};
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
