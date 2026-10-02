#include "eatsbits/presenter/value_edit_presenter.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace eatsbits::presenter {

void ValueEditPresenter::open(const ValueEditConfig& config) {
    config_ = config;
    isOpen_ = true;
    isPercentMode_ = false;
    markDirty();
}

void ValueEditPresenter::bindToParameter(ParameterPresenter& param, std::function<void()> onFinished) {
    ValueEditConfig cfg;
    cfg.title = param.getLabel();
    cfg.paramName = param.getName();
    cfg.isTextMode = false;
    cfg.currentValue = param.getValue();
    cfg.minValue = param.getMinValue();
    cfg.maxValue = param.getMaxValue();
    cfg.defaultValue = param.getDefaultValue();
    cfg.hasDefault = true;
    cfg.isInteger = param.isInteger();
    cfg.allowPercentage = param.allowPercentage();
    cfg.unit = param.getUnit();
    cfg.onCommit = [&param, onFinished](float newVal) {
        param.setValue(newVal);
        if (onFinished) {
            onFinished();
        }
    };
    cfg.onCancel = [onFinished]() {
        if (onFinished) {
            onFinished();
        }
    };
    open(cfg);
}

void ValueEditPresenter::close() noexcept {
    isOpen_ = false;
    markDirty();
}

std::string ValueEditPresenter::getInitialEditText() const {
    if (config_.isTextMode) {
        return config_.initialText.empty() ? config_.paramName : config_.initialText;
    }
    return formatValueForEdit(config_.currentValue);
}

std::string ValueEditPresenter::formatValueForEdit(float val) const {
    if (config_.isInteger) {
        return std::to_string(static_cast<int>(std::round(val)));
    }

    char buf[64];
    if (std::abs(val) < 10.0f) {
        std::snprintf(buf, sizeof(buf), "%.2f", val);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f", val);
    }
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

std::string ValueEditPresenter::setPercentMode(bool enabled, const std::string& currentInput) {
    if (isPercentMode_ == enabled) {
        return currentInput;
    }
    return togglePercentMode(currentInput);
}

std::string ValueEditPresenter::togglePercentMode(const std::string& currentInput) {
    if (!config_.allowPercentage || config_.maxValue <= config_.minValue) {
        return currentInput;
    }

    float current = 0.0f;
    try {
        current = std::stof(currentInput);
    } catch (...) {
        current = config_.currentValue;
    }

    markDirty();

    if (!isPercentMode_) {
        // Direct value -> Percentage (0% - 100%)
        float pct = ((current - config_.minValue) / (config_.maxValue - config_.minValue)) * 100.0f;
        pct = std::clamp(pct, 0.0f, 100.0f);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", pct);
        std::string s(buf);
        if (s.find('.') != std::string::npos) {
            while (s.back() == '0') s.pop_back();
            if (s.back() == '.') s.pop_back();
        }
        isPercentMode_ = true;
        return s;
    } else {
        // Percentage -> Direct value
        float val = config_.minValue + (current / 100.0f) * (config_.maxValue - config_.minValue);
        val = std::clamp(val, config_.minValue, config_.maxValue);
        isPercentMode_ = false;
        return formatValueForEdit(val);
    }
}

std::string ValueEditPresenter::resetToDefault() {
    if (!config_.hasDefault) return "";
    isPercentMode_ = false;
    markDirty();
    return formatValueForEdit(config_.defaultValue);
}

std::string ValueEditPresenter::calculateQuickPercent(float pct) {
    if (!config_.allowPercentage || config_.maxValue <= config_.minValue) return "";
    markDirty();
    if (isPercentMode_) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(pct)));
        return std::string(buf);
    } else {
        float val = config_.minValue + (pct / 100.0f) * (config_.maxValue - config_.minValue);
        val = std::clamp(val, config_.minValue, config_.maxValue);
        return formatValueForEdit(val);
    }
}

ValidationResult ValueEditPresenter::validate(const std::string& input) const {
    ValidationResult res;
    if (config_.isTextMode) {
        res.isValid = true;
        return res;
    }

    if (input.empty()) {
        res.isValid = false;
        res.error = "Input is empty";
        return res;
    }

    bool hasPercentChar = (input.find('%') != std::string::npos);
    std::string cleanStr = input;
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), '%'), cleanStr.end());
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), ' '), cleanStr.end());

    float parsed = 0.0f;
    try {
        parsed = std::stof(cleanStr);
    } catch (...) {
        res.isValid = false;
        res.error = "Invalid numerical format";
        return res;
    }

    if ((isPercentMode_ || hasPercentChar) && config_.maxValue > config_.minValue) {
        float val = config_.minValue + (parsed / 100.0f) * (config_.maxValue - config_.minValue);
        val = std::clamp(val, config_.minValue, config_.maxValue);
        if (config_.isInteger) val = std::round(val);
        res.isValid = true;
        res.parsedValue = val;
    } else {
        float val = std::clamp(parsed, config_.minValue, config_.maxValue);
        if (config_.isInteger) val = std::round(val);
        res.isValid = true;
        res.parsedValue = val;
    }

    return res;
}

bool ValueEditPresenter::submit(const std::string& input) {
    if (!isOpen_) return false;

    if (config_.isTextMode) {
        if (config_.onCommitText) {
            config_.onCommitText(input);
        }
        close();
        return true;
    }

    if (input.empty()) {
        close();
        return false;
    }

    ValidationResult val = validate(input);
    if (!val.isValid) {
        close();
        return false;
    }

    if (config_.onCommit) {
        config_.onCommit(val.parsedValue);
    }
    close();
    return true;
}

void ValueEditPresenter::cancel() {
    if (!isOpen_) return;
    if (config_.onCancel) {
        config_.onCancel();
    }
    close();
}

} // namespace eatsbits::presenter
