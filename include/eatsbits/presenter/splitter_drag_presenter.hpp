#ifndef EATS_SPLITTER_DRAG_PRESENTER_HPP
#define EATS_SPLITTER_DRAG_PRESENTER_HPP

#include "eatsbits/presenter/presenter_base.hpp"
#include "eatsbits/presenter/drag_handler.hpp"
#include <functional>
#include <algorithm>
#include <cmath>

namespace eatsbits::presenter {

struct SplitterConfig {
    ui::DragMode dragMode{ui::DragMode::ArrangerPropertiesResize};
    float minWidth{240.0f};
    float maxWidth{640.0f};
    float defaultWidth{300.0f};
    float collapseThresholdMargin{25.0f};
};

/**
 * @brief Headless presenter encapsulating collapsible sidebar / properties drawer resizing,
 * expansion threshold hysteresis, and click-vs-drag toggle mechanics.
 */
class SplitterDragPresenter : public PresenterBase, public IDragHandler {
public:
    SplitterDragPresenter() = default;

    /**
     * @brief Begin a splitter drag interaction.
     */
    void startDrag(float startX, float currentWidth, bool currentlyExpanded,
                   const SplitterConfig& config,
                   std::function<void(float newWidth, bool expanded)> onLayoutChanged);

    // IDragHandler Implementation
    using IDragHandler::onPointerMove;
    using IDragHandler::onPointerUp;
    void onPointerMove(const ui::PointerEvent& ev) override;
    void onPointerUp(const ui::PointerEvent& ev) override;
    void cancelDrag() override;

    [[nodiscard]] bool isDragging() const noexcept override { return isDragging_; }
    [[nodiscard]] ui::DragMode getDragMode() const noexcept override {
        return isDragging_ ? config_.dragMode : ui::DragMode::None;
    }

    [[nodiscard]] float getCurrentWidth() const noexcept { return currentWidth_; }
    [[nodiscard]] bool isExpanded() const noexcept { return isExpanded_; }

private:
    bool isDragging_{false};
    float startX_{0.0f};
    float initialWidth_{300.0f};
    float currentWidth_{300.0f};
    bool initialExpanded_{false};
    bool isExpanded_{false};
    SplitterConfig config_{};

    std::function<void(float newWidth, bool expanded)> onLayoutChanged_{nullptr};
};

} // namespace eatsbits::presenter

#endif // EATS_SPLITTER_DRAG_PRESENTER_HPP
