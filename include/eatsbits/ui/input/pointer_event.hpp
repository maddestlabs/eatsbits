#pragma once

#include <cstdint>
#include <cmath>
#include <vector>
#include <deque>
#include <chrono>
#include <algorithm>

namespace eatsbits::ui {

enum class PointerType {
    Mouse,
    Touch,
    Pen
};

enum class PointerAction {
    Down,
    Move,
    Up,
    Cancel,
    Scroll
};

enum class PointerButton {
    None = 0,
    Left = 1,
    Right = 2,
    Middle = 4
};

inline PointerButton operator|(PointerButton a, PointerButton b) {
    return static_cast<PointerButton>(static_cast<int>(a) | static_cast<int>(b));
}

inline bool hasButton(PointerButton flags, PointerButton test) {
    return (static_cast<int>(flags) & static_cast<int>(test)) != 0;
}

struct PointerModifiers {
    bool shift{false};
    bool ctrl{false};
    bool alt{false};
    bool meta{false};
};

struct PointerEvent {
    int id{0};                         // 0 = primary mouse, >0 = touch touchId
    PointerType type{PointerType::Mouse};
    PointerAction action{PointerAction::Move};
    float x{0.0f};                     // Logical UI coordinates
    float y{0.0f};
    float rawX{0.0f};                  // Physical window pixels
    float rawY{0.0f};
    float dx{0.0f};                    // Delta since last event
    float dy{0.0f};
    float scrollX{0.0f};               // Mouse wheel / trackpad scroll
    float scrollY{0.0f};
    PointerButton button{PointerButton::None};
    float pressure{1.0f};              // 0.0 - 1.0 (touch pressure or 1.0 for mouse)
    double timestampMs{0.0};           // Monotonic millisecond timestamp
    PointerModifiers mods;
    bool isPrimary{true};

    [[nodiscard]] bool isTouch() const noexcept { return type == PointerType::Touch; }
    [[nodiscard]] bool isMouse() const noexcept { return type == PointerType::Mouse; }
};

/**
 * Kinetic Momentum Scroller.
 * Computes velocity from recent pointer samples and simulates exponential decay friction.
 */
class KineticScroller {
public:
    struct Sample {
        float x{0.0f};
        float y{0.0f};
        double timeMs{0.0};
    };

    void reset() noexcept {
        samples_.clear();
        vx_ = 0.0f;
        vy_ = 0.0f;
        isGliding_ = false;
    }

    void addSample(float x, float y, double timeMs) {
        samples_.push_back({x, y, timeMs});
        while (samples_.size() > 5) {
            samples_.pop_front();
        }
    }

    void endDrag([[maybe_unused]] double timeMs) {
        if (samples_.size() < 2) {
            reset();
            return;
        }

        const auto& first = samples_.front();
        const auto& last = samples_.back();
        double dt = (last.timeMs - first.timeMs) / 1000.0;

        if (dt > 0.01 && dt < 0.25) {
            vx_ = static_cast<float>((last.x - first.x) / dt);
            vy_ = static_cast<float>((last.y - first.y) / dt);

            // Clamp max velocity
            constexpr float kMaxVelocity = 4000.0f;
            vx_ = std::clamp(vx_, -kMaxVelocity, kMaxVelocity);
            vy_ = std::clamp(vy_, -kMaxVelocity, kMaxVelocity);

            if (std::abs(vx_) > 60.0f || std::abs(vy_) > 60.0f) {
                isGliding_ = true;
            } else {
                reset();
            }
        } else {
            reset();
        }
    }

    // Step the momentum glide by frame dt seconds. Returns (dx, dy).
    void step(float dtSeconds, float& outDx, float& outDy) {
        if (!isGliding_) {
            outDx = 0.0f;
            outDy = 0.0f;
            return;
        }

        outDx = vx_ * dtSeconds;
        outDy = vy_ * dtSeconds;

        // Friction decay (exponential falloff, approx 0.92 per 16ms frame)
        float decay = std::pow(friction_, dtSeconds * 60.0f);
        vx_ *= decay;
        vy_ *= decay;

        if (std::abs(vx_) < 5.0f && std::abs(vy_) < 5.0f) {
            reset();
        }
    }

    [[nodiscard]] bool isGliding() const noexcept { return isGliding_; }
    void stop() noexcept { reset(); }

    void setFriction(float f) noexcept { friction_ = std::clamp(f, 0.80f, 0.98f); }

private:
    std::deque<Sample> samples_;
    float vx_{0.0f};
    float vy_{0.0f};
    float friction_{0.92f};
    bool isGliding_{false};
};

/**
 * Gesture Arena: Handles discrimination between Tap, Long-Press, Drag, and Pinch gestures.
 * Solves the desktop/mobile parity problem by enforcing slop thresholds:
 * - Touch: 16px slop (prevents accidental drags when tapping on touchscreens)
 * - Mouse: 3px slop (immediate crisp desktop precision)
 */
enum class GestureKind {
    None,
    Tap,
    DoubleTap,
    LongPress,
    Drag,
    Pinch
};

class GestureRecognizer {
public:
    struct GestureEvent {
        GestureKind kind{GestureKind::None};
        float startX{0.0f};
        float startY{0.0f};
        float currentX{0.0f};
        float currentY{0.0f};
        float totalDx{0.0f};
        float totalDy{0.0f};
        float deltaDx{0.0f};
        float deltaDy{0.0f};
        float pinchScale{1.0f}; // Ratio of current distance / initial distance
        PointerButton button{PointerButton::Left};
        bool isCompleted{false};
    };

    void onPointerDown(const PointerEvent& ev) {
        active_ = true;
        dragCommitted_ = false;
        pointerType_ = ev.type;
        startX_ = ev.x;
        startY_ = ev.y;
        currentX_ = ev.x;
        currentY_ = ev.y;
        startTimeMs_ = ev.timestampMs;
        lastTimeMs_ = ev.timestampMs;
        button_ = ev.button;

        slopThreshold_ = (ev.type == PointerType::Touch) ? 16.0f : 3.0f;
        scroller_.reset();
        scroller_.addSample(ev.x, ev.y, ev.timestampMs);
    }

    bool onPointerMove(const PointerEvent& ev, GestureEvent& outGesture) {
        if (!active_) return false;

        float dx = ev.x - currentX_;
        float dy = ev.y - currentY_;
        currentX_ = ev.x;
        currentY_ = ev.y;
        lastTimeMs_ = ev.timestampMs;

        scroller_.addSample(ev.x, ev.y, ev.timestampMs);

        float totalDx = currentX_ - startX_;
        float totalDy = currentY_ - startY_;
        float distance = std::sqrt(totalDx * totalDx + totalDy * totalDy);

        if (!dragCommitted_) {
            if (distance >= slopThreshold_) {
                dragCommitted_ = true;
            }
        }

        if (dragCommitted_) {
            outGesture.kind = GestureKind::Drag;
            outGesture.startX = startX_;
            outGesture.startY = startY_;
            outGesture.currentX = currentX_;
            outGesture.currentY = currentY_;
            outGesture.totalDx = totalDx;
            outGesture.totalDy = totalDy;
            outGesture.deltaDx = dx;
            outGesture.deltaDy = dy;
            outGesture.button = button_;
            outGesture.isCompleted = false;
            return true;
        }

        return false;
    }

    bool onPointerUp(const PointerEvent& ev, GestureEvent& outGesture) {
        if (!active_) return false;

        active_ = false;
        double durationMs = ev.timestampMs - startTimeMs_;
        float totalDx = ev.x - startX_;
        float totalDy = ev.y - startY_;
        float distance = std::sqrt(totalDx * totalDx + totalDy * totalDy);

        outGesture.startX = startX_;
        outGesture.startY = startY_;
        outGesture.currentX = ev.x;
        outGesture.currentY = ev.y;
        outGesture.totalDx = totalDx;
        outGesture.totalDy = totalDy;
        outGesture.button = button_;
        outGesture.isCompleted = true;

        if (dragCommitted_) {
            outGesture.kind = GestureKind::Drag;
            scroller_.endDrag(ev.timestampMs);
            return true;
        }

        // Tap or Double Tap
        if (distance < slopThreshold_) {
            if (durationMs > 500.0) {
                outGesture.kind = GestureKind::LongPress;
                return true;
            }

            double timeSinceLastTap = ev.timestampMs - lastTapTimeMs_;
            float distFromLastTap = std::sqrt(std::pow(ev.x - lastTapX_, 2) + std::pow(ev.y - lastTapY_, 2));

            if (timeSinceLastTap < 350.0 && distFromLastTap < 24.0f) {
                outGesture.kind = GestureKind::DoubleTap;
                lastTapTimeMs_ = 0.0;
            } else {
                outGesture.kind = GestureKind::Tap;
                lastTapTimeMs_ = ev.timestampMs;
                lastTapX_ = ev.x;
                lastTapY_ = ev.y;
            }
            return true;
        }

        return false;
    }

    [[nodiscard]] KineticScroller& getScroller() noexcept { return scroller_; }
    [[nodiscard]] bool isDragCommitted() const noexcept { return dragCommitted_; }

private:
    bool active_{false};
    bool dragCommitted_{false};
    PointerType pointerType_{PointerType::Mouse};
    float startX_{0.0f};
    float startY_{0.0f};
    float currentX_{0.0f};
    float currentY_{0.0f};
    double startTimeMs_{0.0};
    double lastTimeMs_{0.0};
    float slopThreshold_{3.0f};
    PointerButton button_{PointerButton::Left};

    double lastTapTimeMs_{0.0};
    float lastTapX_{0.0f};
    float lastTapY_{0.0f};

    KineticScroller scroller_;
};

} // namespace eatsbits::ui
