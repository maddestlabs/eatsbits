#include "eatsbits/presenter/marquee_select_presenter.hpp"

namespace eatsbits::presenter {

void MarqueeSelectPresenter::startSelection(float startX, float startY, bool isAdditive,
                                            std::function<void(const MarqueeRect&, bool isAdditive)> onSelectionUpdated,
                                            std::function<void(const MarqueeRect&, bool isAdditive, bool isDrag)> onSelectionCommitted) {
    startX_ = startX;
    startY_ = startY;
    curX_ = startX;
    curY_ = startY;
    isAdditive_ = isAdditive;
    isMarqueeActive_ = false;
    onSelectionUpdated_ = std::move(onSelectionUpdated);
    onSelectionCommitted_ = std::move(onSelectionCommitted);
    isDragging_ = true;
    markDirty();
}

void MarqueeSelectPresenter::onPointerMove(const ui::PointerEvent& ev) {
    if (!isDragging_) return;

    if (ev.mods.ctrl || ev.mods.shift) {
        isAdditive_ = true;
    }

    curX_ = ev.x;
    curY_ = ev.y;

    float dist = std::hypot(curX_ - startX_, curY_ - startY_);
    if (!isMarqueeActive_ && dist > 4.0f) {
        isMarqueeActive_ = true;
    }

    if (isMarqueeActive_) {
        markDirty();
        if (onSelectionUpdated_) {
            onSelectionUpdated_(getBounds(), isAdditive_);
        }
    }
}

void MarqueeSelectPresenter::onPointerUp(const ui::PointerEvent& /*ev*/) {
    if (!isDragging_) return;

    bool wasActive = isMarqueeActive_;
    isDragging_ = false;
    isMarqueeActive_ = false;
    markDirty();

    if (onSelectionCommitted_) {
        onSelectionCommitted_(getBounds(), isAdditive_, wasActive);
    }
}

void MarqueeSelectPresenter::cancelDrag() {
    if (!isDragging_) return;

    isDragging_ = false;
    isMarqueeActive_ = false;
    markDirty();
}

MarqueeRect MarqueeSelectPresenter::getBounds() const noexcept {
    return MarqueeRect{
        std::min(startX_, curX_),
        std::min(startY_, curY_),
        std::max(startX_, curX_),
        std::max(startY_, curY_)
    };
}

} // namespace eatsbits::presenter
