#include "eatsbits/presenter/timeline_scrub_presenter.hpp"

namespace eatsbits::presenter {

void TimelineScrubPresenter::startRulerScrub(float startX, float trackHeaderW, float barW,
                                             uint32_t currentStep, uint32_t maxSteps,
                                             std::function<void(uint32_t)> onStepChanged,
                                             std::function<void(uint32_t)> onScrubCommitted) {
    type_ = ScrubType::Ruler;
    trackHeaderW_ = trackHeaderW;
    scaleFactor_ = std::max(1.0f, barW);
    initialStep_ = currentStep;
    currentStep_ = currentStep;
    maxSteps_ = maxSteps;
    onStepChanged_ = std::move(onStepChanged);
    onScrubCommitted_ = std::move(onScrubCommitted);
    isDragging_ = true;
    markDirty();

    // Immediately update position based on start position
    onPointerMove(startX, 0.0f);
}

void TimelineScrubPresenter::startOverviewScrub(float startX, float trackHeaderW, float overviewW,
                                               uint32_t currentStep, uint32_t maxSteps,
                                               std::function<void(uint32_t)> onStepChanged,
                                               std::function<void(uint32_t)> onScrubCommitted) {
    type_ = ScrubType::OverviewMinimap;
    trackHeaderW_ = trackHeaderW;
    scaleFactor_ = std::max(1.0f, overviewW);
    initialStep_ = currentStep;
    currentStep_ = currentStep;
    maxSteps_ = maxSteps;
    onStepChanged_ = std::move(onStepChanged);
    onScrubCommitted_ = std::move(onScrubCommitted);
    isDragging_ = true;
    markDirty();

    // Immediately update position based on start position
    onPointerMove(startX, 0.0f);
}

void TimelineScrubPresenter::onPointerMove(const ui::PointerEvent& ev) {
    if (!isDragging_) return;

    uint32_t targetStep = currentStep_;

    if (type_ == ScrubType::Ruler) {
        if (ev.x >= trackHeaderW_) {
            float barProgress = (ev.x - trackHeaderW_) / scaleFactor_;
            float rawStep = std::clamp(barProgress * 16.0f, 0.0f, static_cast<float>(maxSteps_));
            targetStep = static_cast<uint32_t>(rawStep);
        } else {
            targetStep = 0;
        }
    } else { // ScrubType::OverviewMinimap
        float norm = std::clamp((ev.x - trackHeaderW_) / scaleFactor_, 0.0f, 1.0f);
        targetStep = static_cast<uint32_t>(norm * static_cast<float>(maxSteps_));
    }

    if (targetStep != currentStep_) {
        currentStep_ = targetStep;
        markDirty();
        if (onStepChanged_) {
            onStepChanged_(currentStep_);
        }
    }
}

void TimelineScrubPresenter::onPointerUp(const ui::PointerEvent& /*ev*/) {
    if (!isDragging_) return;

    isDragging_ = false;
    markDirty();

    if (onScrubCommitted_) {
        onScrubCommitted_(currentStep_);
    }
}

void TimelineScrubPresenter::cancelDrag() {
    if (!isDragging_) return;

    currentStep_ = initialStep_;
    isDragging_ = false;
    markDirty();

    if (onStepChanged_) {
        onStepChanged_(currentStep_);
    }
}

} // namespace eatsbits::presenter
