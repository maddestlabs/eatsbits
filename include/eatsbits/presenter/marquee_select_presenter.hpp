#ifndef EATS_MARQUEE_SELECT_PRESENTER_HPP
#define EATS_MARQUEE_SELECT_PRESENTER_HPP

#include "eatsbits/presenter/presenter_base.hpp"
#include "eatsbits/presenter/drag_handler.hpp"
#include <functional>
#include <algorithm>
#include <cmath>

namespace eatsbits::presenter {

struct MarqueeRect {
    float minX{0.0f};
    float minY{0.0f};
    float maxX{0.0f};
    float maxY{0.0f};

    [[nodiscard]] bool contains(float x, float y) const noexcept {
        return x >= minX && x <= maxX && y >= minY && y <= maxY;
    }

    [[nodiscard]] bool intersects(float rx, float ry, float rw, float rh) const noexcept {
        return !(rx + rw < minX || rx > maxX || ry + rh < minY || ry > maxY);
    }
};

/**
 * @brief Headless presenter managing 2D rubber-band / marquee box selection,
 * deadband hysteresis thresholding, and box bounds calculation.
 */
class MarqueeSelectPresenter : public PresenterBase, public IDragHandler {
public:
    MarqueeSelectPresenter() = default;

    /**
     * @brief Begin a marquee selection drag session.
     */
    void startSelection(float startX, float startY, bool isAdditive = false,
                        std::function<void(const MarqueeRect&, bool isAdditive)> onSelectionUpdated = nullptr,
                        std::function<void(const MarqueeRect&, bool isAdditive, bool isDrag)> onSelectionCommitted = nullptr);

    // IDragHandler Implementation
    using IDragHandler::onPointerMove;
    using IDragHandler::onPointerUp;
    void onPointerMove(const ui::PointerEvent& ev) override;
    void onPointerUp(const ui::PointerEvent& ev) override;
    void cancelDrag() override;

    [[nodiscard]] bool isDragging() const noexcept override { return isDragging_; }
    [[nodiscard]] ui::DragMode getDragMode() const noexcept override {
        return isDragging_ ? ui::DragMode::PianoRollMarquee : ui::DragMode::None;
    }

    [[nodiscard]] bool isMarqueeActive() const noexcept { return isMarqueeActive_; }
    [[nodiscard]] bool isAdditive() const noexcept { return isAdditive_; }
    [[nodiscard]] MarqueeRect getBounds() const noexcept;

    [[nodiscard]] float getStartX() const noexcept { return startX_; }
    [[nodiscard]] float getStartY() const noexcept { return startY_; }
    [[nodiscard]] float getCurrentX() const noexcept { return curX_; }
    [[nodiscard]] float getCurrentY() const noexcept { return curY_; }

private:
    bool isDragging_{false};
    bool isMarqueeActive_{false};
    bool isAdditive_{false};
    float startX_{0.0f};
    float startY_{0.0f};
    float curX_{0.0f};
    float curY_{0.0f};

    std::function<void(const MarqueeRect&, bool isAdditive)> onSelectionUpdated_{nullptr};
    std::function<void(const MarqueeRect&, bool isAdditive, bool isDrag)> onSelectionCommitted_{nullptr};
};

} // namespace eatsbits::presenter

#endif // EATS_MARQUEE_SELECT_PRESENTER_HPP
