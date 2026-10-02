#ifndef EATS_TIMELINE_SCRUB_PRESENTER_HPP
#define EATS_TIMELINE_SCRUB_PRESENTER_HPP

#include "eatsbits/presenter/presenter_base.hpp"
#include "eatsbits/presenter/drag_handler.hpp"
#include <functional>
#include <algorithm>
#include <cmath>

namespace eatsbits::presenter {

enum class ScrubType {
    Ruler,
    OverviewMinimap
};

/**
 * @brief Headless presenter encapsulating transport playhead scrubbing math,
 * pixel-to-step translation, zoom/header offsets, and step bounds clamping.
 */
class TimelineScrubPresenter : public PresenterBase, public IDragHandler {
public:
    TimelineScrubPresenter() = default;

    /**
     * @brief Begin ruler scrub (X offset maps via bar width).
     */
    void startRulerScrub(float startX, float trackHeaderW, float barW,
                         uint32_t currentStep, uint32_t maxSteps,
                         std::function<void(uint32_t)> onStepChanged,
                         std::function<void(uint32_t)> onScrubCommitted = nullptr);

    /**
     * @brief Begin overview minimap scrub (X offset maps across total minimap width).
     */
    void startOverviewScrub(float startX, float trackHeaderW, float overviewW,
                            uint32_t currentStep, uint32_t maxSteps,
                            std::function<void(uint32_t)> onStepChanged,
                            std::function<void(uint32_t)> onScrubCommitted = nullptr);

    // IDragHandler Implementation
    using IDragHandler::onPointerMove;
    using IDragHandler::onPointerUp;
    void onPointerMove(const ui::PointerEvent& ev) override;
    void onPointerUp(const ui::PointerEvent& ev) override;
    void cancelDrag() override;

    [[nodiscard]] bool isDragging() const noexcept override { return isDragging_; }
    [[nodiscard]] ui::DragMode getDragMode() const noexcept override {
        if (!isDragging_) return ui::DragMode::None;
        return (type_ == ScrubType::Ruler) ? ui::DragMode::ArrangerRulerScrub : ui::DragMode::ArrangerOverviewScroll;
    }

    [[nodiscard]] uint32_t getCurrentStep() const noexcept { return currentStep_; }
    [[nodiscard]] uint32_t getInitialStep() const noexcept { return initialStep_; }
    [[nodiscard]] ScrubType getScrubType() const noexcept { return type_; }

private:
    bool isDragging_{false};
    ScrubType type_{ScrubType::Ruler};
    float trackHeaderW_{210.0f};
    float scaleFactor_{60.0f}; // barW for Ruler, overviewW for Minimap
    uint32_t initialStep_{0};
    uint32_t currentStep_{0};
    uint32_t maxSteps_{288};

    std::function<void(uint32_t)> onStepChanged_{nullptr};
    std::function<void(uint32_t)> onScrubCommitted_{nullptr};
};

} // namespace eatsbits::presenter

#endif // EATS_TIMELINE_SCRUB_PRESENTER_HPP
