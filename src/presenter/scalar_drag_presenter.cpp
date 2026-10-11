#include "eatsbits/presenter/scalar_drag_presenter.hpp"

namespace eatsbits::presenter {

void ScalarDragPresenter::startDrag(float startX, float startY, float initialValue,
                                    const ScalarDragConfig& config,
                                    std::function<void(float)> onValueChanged,
                                    std::function<void(float)> onDragCommitted) {
    startX_ = startX;
    startY_ = startY;
    lastX_ = startX;
    lastY_ = startY;
    accumulatedDelta_ = 0.0f;
    initialValue_ = initialValue;
    currentValue_ = initialValue;
    config_ = config;
    if (config_.useAudioTaper) {
        initialTravel_ = audio_taper::gainToTravel(initialValue_);
    }
    boundParam_ = nullptr;
    onValueChanged_ = std::move(onValueChanged);
    onDragCommitted_ = std::move(onDragCommitted);
    fineControl_ = false;
    isDragging_ = true;

    if (config_.captureMouse) {
        isMouseCaptured_ = true;
        if (onMouseCapture_) {
            onMouseCapture_(true, startX_, startY_);
        }
    }

    markDirty();
}

void ScalarDragPresenter::startDragWithPresenter(float startX, float startY, ParameterPresenter& param,
                                                 const ScalarDragConfig& config,
                                                 std::function<void(float)> onDragCommitted) {
    startX_ = startX;
    startY_ = startY;
    lastX_ = startX;
    lastY_ = startY;
    accumulatedDelta_ = 0.0f;
    initialValue_ = param.getValue();
    currentValue_ = initialValue_;
    config_ = config;
    config_.minValue = param.getMinValue();
    config_.maxValue = param.getMaxValue();
    config_.defaultValue = param.getDefaultValue();
    config_.isInteger = param.isInteger();
    if (config_.stepSnap <= 0.0f && param.getStep() > 0.0f) {
        config_.stepSnap = param.getStep();
    }
    if (config_.useAudioTaper) {
        initialTravel_ = audio_taper::gainToTravel(initialValue_);
    }
    boundParam_ = &param;
    onValueChanged_ = nullptr;
    onDragCommitted_ = std::move(onDragCommitted);
    fineControl_ = false;
    isDragging_ = true;

    if (config_.captureMouse) {
        isMouseCaptured_ = true;
        if (onMouseCapture_) {
            onMouseCapture_(true, startX_, startY_);
        }
    }

    markDirty();
}

bool ScalarDragPresenter::onPointerDown(const ui::PointerEvent& ev) {
    // 1. Ctrl / Cmd + click -> immediately restore default value
    if (ev.mods.ctrl || ev.mods.meta) {
        resetToDefault();
        if (onDragCommitted_) {
            onDragCommitted_(currentValue_);
        }
        return true;
    }

    // 2. Double-click detection -> trigger value edit callback
    double nowMs = ev.timestampMs > 0.0 ? ev.timestampMs : 
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
    double timeSinceLastClick = nowMs - lastClickTimeMs_;
    float dist = std::hypot(ev.x - lastClickX_, ev.y - lastClickY_);

    if (timeSinceLastClick > 0.0 && timeSinceLastClick < 350.0 && dist < 6.0f) {
        lastClickTimeMs_ = 0.0;
        if (onDoubleClicked_) {
            onDoubleClicked_();
            return true;
        }
    } else {
        lastClickTimeMs_ = nowMs;
        lastClickX_ = ev.x;
        lastClickY_ = ev.y;
    }

    return false;
}

void ScalarDragPresenter::resetToDefault() {
    float defVal = config_.defaultValue;
    if (boundParam_) {
        boundParam_->resetToDefault();
        defVal = boundParam_->getValue();
    }
    currentValue_ = defVal;
    accumulatedDelta_ = 0.0f;
    if (config_.useAudioTaper) {
        initialTravel_ = audio_taper::gainToTravel(currentValue_);
    }
    markDirty();

    if (onValueChanged_) {
        onValueChanged_(currentValue_);
    }
    if (boundParam_) {
        boundParam_->setValue(currentValue_);
    }
}

void ScalarDragPresenter::onPointerMove(const ui::PointerEvent& ev) {
    if (!isDragging_) return;

    if (ev.mods.shift) {
        fineControl_ = true;
    }

    const float sens = config_.sensitivity * (fineControl_ ? config_.fineRatio : 1.0f);
    float clampedVal = 0.0f;

    if (config_.relativeMode) {
        // Track relative deltas without clamping to screen borders
        float stepDx = (ev.dx != 0.0f || ev.dy != 0.0f) ? ev.dx : (ev.x - lastX_);
        float stepDy = (ev.dx != 0.0f || ev.dy != 0.0f) ? ev.dy : (ev.y - lastY_);
        lastX_ = ev.x;
        lastY_ = ev.y;

        float stepDelta = 0.0f;
        switch (config_.direction) {
            case DragDirection::VerticalUpIncreases:
                stepDelta = -stepDy * sens;
                break;
            case DragDirection::VerticalDownIncreases:
                stepDelta = stepDy * sens;
                break;
            case DragDirection::HorizontalRightIncreases:
                stepDelta = stepDx * sens;
                break;
            case DragDirection::BidirectionalPan:
                stepDelta = (stepDx - stepDy) * sens;
                break;
        }

        accumulatedDelta_ += stepDelta;

        if (config_.useAudioTaper) {
            float newTravel = std::clamp(initialTravel_ + accumulatedDelta_, 0.0f, 1.0f);
            accumulatedDelta_ = newTravel - initialTravel_;
            clampedVal = audio_taper::travelToGain(newTravel);
        } else {
            float rawVal = initialValue_ + accumulatedDelta_;
            clampedVal = std::clamp(rawVal, config_.minValue, config_.maxValue);
            accumulatedDelta_ = clampedVal - initialValue_;

            if (config_.stepSnap > 0.0f) {
                clampedVal = std::round((clampedVal - config_.minValue) / config_.stepSnap) * config_.stepSnap + config_.minValue;
                clampedVal = std::clamp(clampedVal, config_.minValue, config_.maxValue);
            }

            if (config_.isInteger) {
                clampedVal = std::round(clampedVal);
            }
        }
    } else {
        // Absolute displacement from start coordinates
        float delta = 0.0f;
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

        if (config_.useAudioTaper) {
            float newTravel = std::clamp(initialTravel_ + delta, 0.0f, 1.0f);
            clampedVal = audio_taper::travelToGain(newTravel);
        } else {
            float rawVal = initialValue_ + delta;
            clampedVal = std::clamp(rawVal, config_.minValue, config_.maxValue);

            if (config_.stepSnap > 0.0f) {
                clampedVal = std::round((clampedVal - config_.minValue) / config_.stepSnap) * config_.stepSnap + config_.minValue;
                clampedVal = std::clamp(clampedVal, config_.minValue, config_.maxValue);
            }

            if (config_.isInteger) {
                clampedVal = std::round(clampedVal);
            }
        }
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

    if (isMouseCaptured_) {
        isMouseCaptured_ = false;
        if (onMouseCapture_) {
            onMouseCapture_(false, startX_, startY_);
        }
    }

    isDragging_ = false;
    markDirty();

    if (onDragCommitted_) {
        onDragCommitted_(currentValue_);
    }
}

void ScalarDragPresenter::cancelDrag() {
    if (!isDragging_) return;

    if (isMouseCaptured_) {
        isMouseCaptured_ = false;
        if (onMouseCapture_) {
            onMouseCapture_(false, startX_, startY_);
        }
    }

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
