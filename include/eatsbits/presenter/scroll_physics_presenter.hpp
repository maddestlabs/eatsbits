#ifndef EATS_SCROLL_PHYSICS_PRESENTER_HPP
#define EATS_SCROLL_PHYSICS_PRESENTER_HPP

#include "eatsbits/presenter/presenter_base.hpp"
#include "eatsbits/presenter/frame_time_context.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::presenter {

/**
 * @brief Headless, zero-allocation scroll physics presenter.
 * Encapsulates kinetic momentum, friction decay, edge clamping, optional
 * spring overscroll bounce, and scrollbar thumb calculations.
 */
class ScrollPhysicsPresenter : public PresenterBase {
public:
    ScrollPhysicsPresenter() = default;
    ~ScrollPhysicsPresenter() override = default;

    // --- Configuration ---

    /**
     * @brief Configure scroll bounds directly.
     */
    void setBounds(float minOffset, float maxOffset) noexcept {
        minOffset_ = minOffset;
        maxOffset_ = std::max(minOffset, maxOffset);
        if (!overscrollBounce_) {
            clampToBounds();
        }
        markDirty();
    }

    /**
     * @brief Configure content and viewport lengths.
     * Computes minOffset (0) and maxOffset (max(0, contentLength - viewportLength)).
     */
    void setContentAndViewport(float contentLength, float viewportLength) noexcept {
        contentLength_ = std::max(0.0f, contentLength);
        viewportLength_ = std::max(0.0f, viewportLength);
        minOffset_ = 0.0f;
        maxOffset_ = std::max(0.0f, contentLength_ - viewportLength_);
        if (!overscrollBounce_) {
            clampToBounds();
        }
        markDirty();
    }

    /**
     * @brief Configure friction decay per 60Hz frame (e.g. 0.92f).
     */
    void setFriction(float friction) noexcept {
        friction_ = std::clamp(friction, 0.50f, 0.999f);
    }

    [[nodiscard]] float getFriction() const noexcept { return friction_; }

    /**
     * @brief Enable or disable rubber-band spring overscroll bounce.
     */
    void setOverscrollBounce(bool enabled) noexcept {
        overscrollBounce_ = enabled;
        if (!overscrollBounce_) {
            clampToBounds();
            velocity_ = 0.0f;
        }
        markDirty();
    }

    [[nodiscard]] bool isOverscrollBounceEnabled() const noexcept { return overscrollBounce_; }

    /**
     * @brief Configure spring stiffness and damping for overscroll bounce.
     */
    void setBounceParameters(float stiffness, float damping) noexcept {
        bounceStiffness_ = std::max(1.0f, stiffness);
        bounceDamping_ = std::max(0.1f, damping);
    }

    [[nodiscard]] float getBounceStiffness() const noexcept { return bounceStiffness_; }
    [[nodiscard]] float getBounceDamping() const noexcept { return bounceDamping_; }

    /**
     * @brief Configure minimum normalized thumb size fraction (default 0.05f = 5%).
     */
    void setMinThumbFraction(float fraction) noexcept {
        minThumbFraction_ = std::clamp(fraction, 0.01f, 1.0f);
        markDirty();
    }

    [[nodiscard]] float getMinThumbFraction() const noexcept { return minThumbFraction_; }

    // --- State Accessors ---

    [[nodiscard]] float getOffset() const noexcept { return offset_; }
    [[nodiscard]] float getVelocity() const noexcept { return velocity_; }
    [[nodiscard]] float getMinOffset() const noexcept { return minOffset_; }
    [[nodiscard]] float getMaxOffset() const noexcept { return maxOffset_; }
    [[nodiscard]] float getContentLength() const noexcept { return contentLength_; }
    [[nodiscard]] float getViewportLength() const noexcept { return viewportLength_; }

    [[nodiscard]] bool canScroll() const noexcept {
        return maxOffset_ > minOffset_;
    }

    [[nodiscard]] bool isGliding() const noexcept {
        return std::abs(velocity_) > 0.5f;
    }

    [[nodiscard]] bool isMoving() const noexcept {
        if (std::abs(velocity_) > 0.5f) return true;
        if (overscrollBounce_ && (offset_ < minOffset_ || offset_ > maxOffset_)) return true;
        return false;
    }

    [[nodiscard]] bool isDragging() const noexcept { return isDragging_; }

    // --- Direct Manipulation ---

    /**
     * @brief Set offset directly and stop velocity.
     */
    void scrollTo(float targetOffset) noexcept {
        offset_ = targetOffset;
        velocity_ = 0.0f;
        if (!overscrollBounce_) {
            clampToBounds();
        }
        markDirty();
    }

    /**
     * @brief Set offset without zeroing velocity.
     */
    void setOffset(float targetOffset) noexcept {
        offset_ = targetOffset;
        if (!overscrollBounce_) {
            clampToBounds();
        }
        markDirty();
    }

    /**
     * @brief Scroll by a delta amount.
     * Applies rubber-band resistance if outside bounds and bounce is enabled.
     */
    void scrollBy(float delta) noexcept;

    /**
     * @brief Start an interactive drag/touch session.
     */
    void startDrag() noexcept {
        isDragging_ = true;
        velocity_ = 0.0f;
    }

    /**
     * @brief End an interactive drag session with an optional release fling velocity.
     */
    void endDrag(float releaseVelocity = 0.0f) noexcept {
        isDragging_ = false;
        if (std::abs(releaseVelocity) > 0.5f) {
            fling(releaseVelocity);
        }
    }

    /**
     * @brief Initiate a kinetic momentum fling.
     */
    void fling(float initialVelocity) noexcept {
        constexpr float kMaxVelocity = 15000.0f;
        velocity_ = std::clamp(initialVelocity, -kMaxVelocity, kMaxVelocity);
        markDirty();
    }

    void setVelocity(float v) noexcept {
        velocity_ = v;
        markDirty();
    }

    /**
     * @brief Instantly halt velocity and snap to nearest bound if overscrolled.
     */
    void stop() noexcept {
        velocity_ = 0.0f;
        clampToBounds();
        markDirty();
    }

    // --- Physics Step (Zero Allocations) ---

    /**
     * @brief Advance kinetic momentum and spring damping by dt seconds.
     * Guaranteed zero heap allocations.
     */
    void step(float dt) noexcept;

    /**
     * @brief Advance kinetic momentum and spring damping using virtual FrameTimeContext.
     */
    void step(const FrameTimeContext& time) noexcept {
        step(time.dt());
    }

    // --- Headless Scrollbar Thumb Computation ---

    /**
     * @brief Compute the normalized thumb size [0.0, 1.0] relative to the track.
     */
    [[nodiscard]] float getNormalizedThumbSize() const noexcept;

    /**
     * @brief Compute the normalized thumb position [0.0, 1.0 - thumbSize] relative to the track.
     */
    [[nodiscard]] float getNormalizedThumbPosition() const noexcept;

    /**
     * @brief Compute absolute thumb size in pixels/cells given a track length.
     */
    [[nodiscard]] float getThumbSize(float trackLength) const noexcept {
        return getNormalizedThumbSize() * std::max(0.0f, trackLength);
    }

    /**
     * @brief Compute absolute thumb position in pixels/cells given a track length.
     */
    [[nodiscard]] float getThumbPosition(float trackLength) const noexcept {
        return getNormalizedThumbPosition() * std::max(0.0f, trackLength);
    }

    /**
     * @brief Update scroll offset based on a normalized thumb position [0.0, 1.0 - thumbSize].
     */
    void scrollFromThumbNormalized(float normalizedThumbPos) noexcept;

    /**
     * @brief Update scroll offset based on absolute thumb pixel coordinate along trackLength.
     */
    void scrollFromThumbPixels(float thumbPixelPos, float trackLength) noexcept;

private:
    void clampToBounds() noexcept {
        offset_ = std::clamp(offset_, minOffset_, maxOffset_);
    }

    float offset_{0.0f};
    float velocity_{0.0f};
    float minOffset_{0.0f};
    float maxOffset_{0.0f};
    float contentLength_{0.0f};
    float viewportLength_{0.0f};

    float friction_{0.92f};
    bool overscrollBounce_{true};
    float bounceStiffness_{250.0f};
    float bounceDamping_{25.0f};
    float minThumbFraction_{0.05f};
    bool isDragging_{false};
};

} // namespace eatsbits::presenter

#endif // EATS_SCROLL_PHYSICS_PRESENTER_HPP
