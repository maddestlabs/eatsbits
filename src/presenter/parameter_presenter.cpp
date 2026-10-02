#include "eatsbits/presenter/parameter_presenter.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace eatsbits::presenter {

ParameterPresenter::ParameterPresenter(const ParameterDescriptor& desc) {
    configure(desc);
}

void ParameterPresenter::configure(const ParameterDescriptor& desc) {
    desc_ = desc;
    value_ = std::clamp(desc_.defaultValue, desc_.minValue, desc_.maxValue);
    updateCachedDisplay();
    markDirty();
}

void ParameterPresenter::setValue(float val, bool notify) {
    float clamped = std::clamp(val, desc_.minValue, desc_.maxValue);
    if (desc_.isInteger) {
        clamped = std::round(clamped);
    }
    if (std::abs(value_ - clamped) > 1e-6f) {
        value_ = clamped;
        updateCachedDisplay();
        markDirty();
        if (notify && onValueChanged) {
            onValueChanged(value_);
        }
    }
}

float ParameterPresenter::getNormalizedValue() const noexcept {
    if (desc_.maxValue <= desc_.minValue) return 0.0f;
    float norm = (value_ - desc_.minValue) / (desc_.maxValue - desc_.minValue);
    return std::clamp(norm, 0.0f, 1.0f);
}

void ParameterPresenter::setNormalizedValue(float norm, bool notify) {
    float clampedNorm = std::clamp(norm, 0.0f, 1.0f);
    float physicalVal = desc_.minValue + clampedNorm * (desc_.maxValue - desc_.minValue);
    setValue(physicalVal, notify);
}

void ParameterPresenter::step(int numSteps, bool notify) {
    setValue(value_ + static_cast<float>(numSteps) * desc_.step, notify);
}

void ParameterPresenter::resetToDefault(bool notify) {
    setValue(desc_.defaultValue, notify);
}

void ParameterPresenter::updateFromModel(float val) noexcept {
    float clamped = std::clamp(val, desc_.minValue, desc_.maxValue);
    if (desc_.isInteger) {
        clamped = std::round(clamped);
    }
    if (std::abs(value_ - clamped) > 1e-6f) {
        value_ = clamped;
        updateCachedDisplay();
        markDirty();
    }
}

void ParameterPresenter::updateCachedDisplay() {
    if (desc_.isInteger) {
        cachedRawDisplay_ = std::to_string(static_cast<int>(std::round(value_)));
    } else {
        char buf[64];
        if (std::abs(value_) < 10.0f) {
            std::snprintf(buf, sizeof(buf), "%.2f", value_);
        } else if (std::abs(value_) < 1000.0f) {
            std::snprintf(buf, sizeof(buf), "%.1f", value_);
        } else {
            std::snprintf(buf, sizeof(buf), "%.0f", value_);
        }
        std::string s = buf;
        if (s.find('.') != std::string::npos) {
            while (s.back() == '0') s.pop_back();
            if (s.back() == '.') s.pop_back();
        }
        cachedRawDisplay_ = std::move(s);
    }

    if (!desc_.unit.empty()) {
        cachedDisplay_ = cachedRawDisplay_ + " " + desc_.unit;
    } else {
        cachedDisplay_ = cachedRawDisplay_;
    }
}

} // namespace eatsbits::presenter
