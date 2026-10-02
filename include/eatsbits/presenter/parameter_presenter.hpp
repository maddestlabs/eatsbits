#ifndef EATS_PARAMETER_PRESENTER_HPP
#define EATS_PARAMETER_PRESENTER_HPP

#include "presenter_base.hpp"
#include <string>
#include <functional>
#include <algorithm>
#include <cmath>

namespace eatsbits::presenter {

/**
 * @brief Configuration descriptor for a controllable audio/synth/mixer parameter.
 */
struct ParameterDescriptor {
    std::string name{""};
    std::string label{""};
    std::string unit{""};
    float minValue{0.0f};
    float maxValue{1.0f};
    float defaultValue{0.0f};
    float step{0.05f};
    bool isInteger{false};
    bool allowPercentage{true};
};

/**
 * @brief Headless presenter encapsulating single-parameter behavior, bounds,
 * normalized translation, stepping, and cached zero-allocation formatted representations.
 */
class ParameterPresenter : public PresenterBase {
public:
    ParameterPresenter() = default;
    explicit ParameterPresenter(const ParameterDescriptor& desc);

    void configure(const ParameterDescriptor& desc);

    [[nodiscard]] float getValue() const noexcept { return value_; }
    void setValue(float val, bool notify = true);

    [[nodiscard]] float getNormalizedValue() const noexcept;
    void setNormalizedValue(float norm, bool notify = true);

    void step(int numSteps, bool notify = true);
    void resetToDefault(bool notify = true);

    [[nodiscard]] const std::string& getFormattedValue() const noexcept { return cachedDisplay_; }
    [[nodiscard]] const std::string& getRawFormattedValue() const noexcept { return cachedRawDisplay_; }

    [[nodiscard]] const std::string& getName() const noexcept { return desc_.name; }
    [[nodiscard]] const std::string& getLabel() const noexcept { return desc_.label.empty() ? desc_.name : desc_.label; }
    [[nodiscard]] const std::string& getUnit() const noexcept { return desc_.unit; }
    [[nodiscard]] float getMinValue() const noexcept { return desc_.minValue; }
    [[nodiscard]] float getMaxValue() const noexcept { return desc_.maxValue; }
    [[nodiscard]] float getDefaultValue() const noexcept { return desc_.defaultValue; }
    [[nodiscard]] float getStep() const noexcept { return desc_.step; }
    [[nodiscard]] bool isInteger() const noexcept { return desc_.isInteger; }
    [[nodiscard]] bool allowPercentage() const noexcept { return desc_.allowPercentage; }
    [[nodiscard]] const ParameterDescriptor& getDescriptor() const noexcept { return desc_; }

    void updateFromModel(float val) noexcept;

    std::function<void(float newValue)> onValueChanged{nullptr};

private:
    void updateCachedDisplay();

    ParameterDescriptor desc_{};
    float value_{0.0f};
    std::string cachedDisplay_{""};
    std::string cachedRawDisplay_{""};
};

} // namespace eatsbits::presenter

#endif // EATS_PARAMETER_PRESENTER_HPP
