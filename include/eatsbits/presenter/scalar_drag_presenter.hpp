#ifndef EATS_SCALAR_DRAG_PRESENTER_HPP
#define EATS_SCALAR_DRAG_PRESENTER_HPP

#include "eatsbits/presenter/presenter_base.hpp"
#include "eatsbits/presenter/drag_handler.hpp"
#include "eatsbits/presenter/parameter_presenter.hpp"
#include <functional>
#include <algorithm>
#include <cmath>

namespace eatsbits::presenter {

/**
 * @brief Spatial delta interpretation orientation for scalar parameters.
 */
enum class DragDirection {
    VerticalUpIncreases,     ///< Dragging upward increases value (knobs, faders)
    VerticalDownIncreases,   ///< Dragging downward increases value
    HorizontalRightIncreases,///< Dragging rightward increases value (horizontal sliders)
    BidirectionalPan         ///< (dx - dy) diagonal panning delta
};

/**
 * @brief Configuration tuning continuous mouse/pointer drag behavior.
 */
struct ScalarDragConfig {
    ui::DragMode dragMode{ui::DragMode::None};
    DragDirection direction{DragDirection::VerticalUpIncreases};
    float minValue{0.0f};
    float maxValue{1.0f};
    float sensitivity{1.0f / 160.0f};
    float fineRatio{0.15f};
    float stepSnap{0.0f};   ///< 0.0 means continuous smooth variation
    bool isInteger{false};
};

/**
 * @brief Headless presenter encapsulating 1D continuous parameter dragging,
 * sensitivity scaling, fine control, bounds clamping, step snapping, commit, and cancel.
 */
class ScalarDragPresenter : public PresenterBase, public IDragHandler {
public:
    ScalarDragPresenter() = default;

    /**
     * @brief Begin a continuous drag session with raw callbacks.
     */
    void startDrag(float startX, float startY, float initialValue, const ScalarDragConfig& config,
                   std::function<void(float)> onValueChanged = nullptr,
                   std::function<void(float)> onDragCommitted = nullptr);

    /**
     * @brief Begin a continuous drag session bound directly to a ParameterPresenter.
     */
    void startDragWithPresenter(float startX, float startY, ParameterPresenter& param,
                                const ScalarDragConfig& config,
                                std::function<void(float)> onDragCommitted = nullptr);

    // IDragHandler Implementation
    using IDragHandler::onPointerMove;
    using IDragHandler::onPointerUp;
    void onPointerMove(const ui::PointerEvent& ev) override;
    void onPointerUp(const ui::PointerEvent& ev) override;
    void cancelDrag() override;

    [[nodiscard]] bool isDragging() const noexcept override { return isDragging_; }
    [[nodiscard]] ui::DragMode getDragMode() const noexcept override { return isDragging_ ? config_.dragMode : ui::DragMode::None; }

    [[nodiscard]] float getCurrentValue() const noexcept { return currentValue_; }
    [[nodiscard]] float getInitialValue() const noexcept { return initialValue_; }
    [[nodiscard]] const ScalarDragConfig& getConfig() const noexcept { return config_; }

    void setFineControl(bool fine) noexcept { fineControl_ = fine; }
    [[nodiscard]] bool isFineControl() const noexcept { return fineControl_; }

private:
    bool isDragging_{false};
    bool fineControl_{false};
    float startX_{0.0f};
    float startY_{0.0f};
    float initialValue_{0.0f};
    float currentValue_{0.0f};
    ScalarDragConfig config_{};

    ParameterPresenter* boundParam_{nullptr};
    std::function<void(float)> onValueChanged_{nullptr};
    std::function<void(float)> onDragCommitted_{nullptr};
};

} // namespace eatsbits::presenter

#endif // EATS_SCALAR_DRAG_PRESENTER_HPP
