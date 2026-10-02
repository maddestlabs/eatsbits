#include "eatsbits/presenter/splitter_drag_presenter.hpp"

namespace eatsbits::presenter {

void SplitterDragPresenter::startDrag(float startX, float currentWidth, bool currentlyExpanded,
                                      const SplitterConfig& config,
                                      std::function<void(float newWidth, bool expanded)> onLayoutChanged) {
    startX_ = startX;
    initialWidth_ = currentWidth;
    currentWidth_ = currentWidth;
    initialExpanded_ = currentlyExpanded;
    isExpanded_ = currentlyExpanded;
    config_ = config;
    onLayoutChanged_ = std::move(onLayoutChanged);
    isDragging_ = true;
    markDirty();
}

void SplitterDragPresenter::onPointerMove(const ui::PointerEvent& ev) {
    if (!isDragging_) return;

    if (!isExpanded_) {
        // Dragging left opens the drawer if moved past threshold
        if (ev.x - startX_ < -4.0f) {
            isExpanded_ = true;
            startX_ = ev.x;
            initialWidth_ = config_.defaultWidth;
            currentWidth_ = config_.defaultWidth;
            markDirty();
            if (onLayoutChanged_) {
                onLayoutChanged_(currentWidth_, isExpanded_);
            }
        }
    } else {
        float newWidth = initialWidth_ + (startX_ - ev.x);
        if (newWidth < (config_.minWidth - config_.collapseThresholdMargin)) {
            // Collapse drawer
            isExpanded_ = false;
            currentWidth_ = config_.defaultWidth;
            isDragging_ = false; // Collapsing immediately stops drag session
            markDirty();
            if (onLayoutChanged_) {
                onLayoutChanged_(currentWidth_, isExpanded_);
            }
        } else {
            float clampedW = std::clamp(newWidth, config_.minWidth, config_.maxWidth);
            if (std::abs(currentWidth_ - clampedW) > 0.5f) {
                currentWidth_ = clampedW;
                markDirty();
                if (onLayoutChanged_) {
                    onLayoutChanged_(currentWidth_, isExpanded_);
                }
            }
        }
    }
}

void SplitterDragPresenter::onPointerUp(const ui::PointerEvent& ev) {
    if (!isDragging_) return;

    // Detect click vs drag (delta < 4.0f toggles expanded state)
    if (std::abs(ev.x - startX_) < 4.0f) {
        isExpanded_ = !initialExpanded_;
        if (isExpanded_ && currentWidth_ < config_.minWidth) {
            currentWidth_ = config_.defaultWidth;
        }
        markDirty();
        if (onLayoutChanged_) {
            onLayoutChanged_(currentWidth_, isExpanded_);
        }
    }

    isDragging_ = false;
    markDirty();
}

void SplitterDragPresenter::cancelDrag() {
    if (!isDragging_) return;

    currentWidth_ = initialWidth_;
    isExpanded_ = initialExpanded_;
    isDragging_ = false;
    markDirty();

    if (onLayoutChanged_) {
        onLayoutChanged_(currentWidth_, isExpanded_);
    }
}

} // namespace eatsbits::presenter
