#include "eatsbits/presenter/scroll_physics_presenter.hpp"

namespace eatsbits::presenter {

void ScrollPhysicsPresenter::scrollBy(float delta) noexcept {
    if (!overscrollBounce_) {
        offset_ = std::clamp(offset_ + delta, minOffset_, maxOffset_);
    } else {
        if (offset_ < minOffset_ && delta < 0.0f) {
            float overscroll = minOffset_ - offset_;
            float resistance = 1.0f / (1.0f + overscroll * 0.015f);
            offset_ += delta * resistance;
        } else if (offset_ > maxOffset_ && delta > 0.0f) {
            float overscroll = offset_ - maxOffset_;
            float resistance = 1.0f / (1.0f + overscroll * 0.015f);
            offset_ += delta * resistance;
        } else {
            offset_ += delta;
        }
    }
    markDirty();
}

void ScrollPhysicsPresenter::step(float dt) noexcept {
    if (isDragging_) {
        return;
    }

    float clampedDt = std::clamp(dt, 0.0001f, 0.1f);

    if (offset_ >= minOffset_ && offset_ <= maxOffset_) {
        if (std::abs(velocity_) > 0.5f) {
            offset_ += velocity_ * clampedDt;
            float decay = std::pow(friction_, clampedDt * 60.0f);
            velocity_ *= decay;

            if (std::abs(velocity_) < 0.5f) {
                velocity_ = 0.0f;
            }

            if (!overscrollBounce_) {
                if (offset_ < minOffset_) {
                    offset_ = minOffset_;
                    velocity_ = 0.0f;
                } else if (offset_ > maxOffset_) {
                    offset_ = maxOffset_;
                    velocity_ = 0.0f;
                }
            }
            markDirty();
        }
    } else {
        if (!overscrollBounce_) {
            clampToBounds();
            velocity_ = 0.0f;
            markDirty();
        } else {
            float target = (offset_ < minOffset_) ? minOffset_ : maxOffset_;
            float displacement = offset_ - target;

            float springForce = -bounceStiffness_ * displacement;
            float dampingForce = -bounceDamping_ * velocity_;
            float acceleration = springForce + dampingForce;

            velocity_ += acceleration * clampedDt;
            float nextOffset = offset_ + velocity_ * clampedDt;

            // Check if boundary was crossed during spring return or settled
            if ((displacement > 0.0f && nextOffset <= target) ||
                (displacement < 0.0f && nextOffset >= target) ||
                (std::abs(nextOffset - target) < 0.1f && std::abs(velocity_) < 2.0f)) {
                offset_ = target;
                velocity_ = 0.0f;
            } else {
                offset_ = nextOffset;
            }
            markDirty();
        }
    }
}

float ScrollPhysicsPresenter::getNormalizedThumbSize() const noexcept {
    if (contentLength_ <= 0.0f || viewportLength_ <= 0.0f) {
        return 1.0f;
    }
    if (viewportLength_ >= contentLength_) {
        return 1.0f;
    }
    float ratio = viewportLength_ / contentLength_;
    return std::clamp(ratio, minThumbFraction_, 1.0f);
}

float ScrollPhysicsPresenter::getNormalizedThumbPosition() const noexcept {
    if (maxOffset_ <= minOffset_) {
        return 0.0f;
    }
    float thumbSize = getNormalizedThumbSize();
    float travelRange = 1.0f - thumbSize;
    if (travelRange <= 0.0f) {
        return 0.0f;
    }
    float scrollFraction = (offset_ - minOffset_) / (maxOffset_ - minOffset_);
    scrollFraction = std::clamp(scrollFraction, 0.0f, 1.0f);
    return scrollFraction * travelRange;
}

void ScrollPhysicsPresenter::scrollFromThumbNormalized(float normalizedThumbPos) noexcept {
    if (maxOffset_ <= minOffset_) {
        return;
    }
    float thumbSize = getNormalizedThumbSize();
    float travelRange = 1.0f - thumbSize;
    if (travelRange <= 0.0001f) {
        return;
    }
    float fraction = std::clamp(normalizedThumbPos / travelRange, 0.0f, 1.0f);
    offset_ = minOffset_ + fraction * (maxOffset_ - minOffset_);
    velocity_ = 0.0f;
    markDirty();
}

void ScrollPhysicsPresenter::scrollFromThumbPixels(float thumbPixelPos, float trackLength) noexcept {
    if (trackLength <= 0.0f) {
        return;
    }
    scrollFromThumbNormalized(thumbPixelPos / trackLength);
}

} // namespace eatsbits::presenter
