#include "eatsbits/presenter/scalar_drag_presenter.hpp"

namespace eatsbits::presenter {

void ScalarDragPresenter::startDrag(float startX, float startY, float initialValue,
                                    const ScalarDragConfig& config,
                                    std::function<void(float)> onValueChanged,
                                    std::function<void(float)> onDragCommitted) {
    startX_ = startX;
    startY_ = startY;
    initialValue_ = initialValue;
    currentValue_ = initialValue;
    config_ = config;
    boundParam_ = nullptr;
    onValueChanged_ = std::move(onValueChanged);
    onDragCommitted_ = std::move(onDragCommitted);
    fineControl_ = false;
    isDragging_ = true;
    markDirty();
}

void ScalarDragPresenter::startDragWithPresenter(float startX, float startY, ParameterPresenter& param,
                                                 const ScalarDragConfig& config,
                                                 std::function<void(float)> onDragCommitted) {
    startX_ = startX;
    startY_ = startY;
    initialValue_ = param.getValue();
    currentValue_ = initialValue_;
    config_ = config;
    config_.minValue = param.getMinValue();
    config_.maxValue = param.getMaxValue();
    config_.isInteger = param.isInteger();
    if (config_.stepSnap <= 0.0f && param.getStep() > 0.0f) {
        config_.stepSnap = param.getStep();
    }
    boundParam_ = &param;
    onValueChanged_ = nullptr;
    onDragCommitted_ = std::move(onDragCommitted);
    fineControl_ = false;
    isDragging_ = true;
    markDirty();
}

void ScalarDragPresenter::onPointerMove(const ui::PointerEvent& ev) {
    if (!isDragging_) return;

    if (ev.mods.shift) {
        fineControl_ = true;
    }

    float delta = 0.0f;
    const float sens = config_.sensitivity * (fineControl_ ? config_.fineRatio : 1.0f);

    switch (config_.direction) {
        case DragDirection::VerticalUpIncreases:
            delta = (startY_ - ev.y) * sens;
            break;
        case DragDirection::VerticalDownIncreases:
            delta = (ev.y - startY_) * sens;
            break;
        case DragDirection::HorizontalRightIncreases:
            delta = (ev.x - startX_) * sens;
            break;
        case DragDirection::BidirectionalPan:
            delta = ((ev.x - startX_) - (ev.y - startY_)) * sens;
            break;
    }

    float rawVal = initialValue_ + delta;
    float clampedVal = std::clamp(rawVal, config_.minValue, config_.maxValue);

    if (config_.stepSnap > 0.0f) {
        clampedVal = std::round((clampedVal - config_.minValue) / config_.stepSnap) * config_.stepSnap + config_.minValue;
        clampedVal = std::clamp(clampedVal, config_.minValue, config_.maxValue);
    }

    if (config_.isInteger) {
        clampedVal = std::round(clampedVal);
    }

    if (std::abs(currentValue_ - clampedVal) > 1e-6f) {
        currentValue_ = clampedVal;
        markDirty();

        if (onValueChanged_) {
            onValueChanged_(currentValue_);
        }
        if (boundParam_) {
            boundParam_->setValue(currentValue_);
        }
    }
}

void ScalarDragPresenter::onPointerUp(const ui::PointerEvent& /*ev*/) {
    if (!isDragging_) return;

    isDragging_ = false;
    markDirty();

    if (onDragCommitted_) {
        onDragCommitted_(currentValue_);
    }
}

void ScalarDragPresenter::cancelDrag() {
    if (!isDragging_) return;

    currentValue_ = initialValue_;
    isDragging_ = false;
    markDirty();

    if (onValueChanged_) {
        onValueChanged_(currentValue_);
    }
    if (boundParam_) {
        boundParam_->setValue(currentValue_);
    }
}

} // namespace eatsbits::presenter
