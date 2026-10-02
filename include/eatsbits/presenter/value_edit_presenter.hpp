#ifndef EATS_VALUE_EDIT_PRESENTER_HPP
#define EATS_VALUE_EDIT_PRESENTER_HPP

#include "presenter_base.hpp"
#include "parameter_presenter.hpp"
#include <string>
#include <functional>

namespace eatsbits::presenter {

/**
 * @brief Configuration model for opening a value edit session.
 * Decoupled from graphical windowing, fonts, and colors.
 */
struct ValueEditConfig {
    std::string title{"EDIT VALUE"};
    std::string paramName{""};
    bool isTextMode{false};
    std::string initialText{""};
    float currentValue{0.0f};
    float minValue{0.0f};
    float maxValue{1.0f};
    float defaultValue{0.0f};
    bool hasDefault{true};
    bool isInteger{false};
    bool allowPercentage{true};
    std::string unit{""};
    std::function<void(float newValue)> onCommit{nullptr};
    std::function<void(const std::string& newText)> onCommitText{nullptr};
    std::function<void()> onCancel{nullptr};
};

struct ValidationResult {
    bool isValid{false};
    float parsedValue{0.0f};
    std::string error{""};
};

/**
 * @brief Headless presenter managing numerical parsing, validation, percentage conversions,
 * default resets, and commit lifecycle for both GUI modals and TUI command buffers.
 */
class ValueEditPresenter : public PresenterBase {
public:
    ValueEditPresenter() = default;

    void open(const ValueEditConfig& config);
    void bindToParameter(ParameterPresenter& param, std::function<void()> onFinished = nullptr);
    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }
    [[nodiscard]] bool isPercentMode() const noexcept { return isPercentMode_; }
    [[nodiscard]] bool isTextMode() const noexcept { return config_.isTextMode; }
    [[nodiscard]] const ValueEditConfig& getConfig() const noexcept { return config_; }

    [[nodiscard]] std::string getInitialEditText() const;
    [[nodiscard]] std::string formatValueForEdit(float val) const;

    std::string togglePercentMode(const std::string& currentInput);
    std::string setPercentMode(bool enabled, const std::string& currentInput);
    std::string resetToDefault();
    std::string calculateQuickPercent(float pct);

    [[nodiscard]] ValidationResult validate(const std::string& input) const;
    bool submit(const std::string& input);
    void cancel();

private:
    bool isOpen_{false};
    bool isPercentMode_{false};
    ValueEditConfig config_{};
};

} // namespace eatsbits::presenter

#endif // EATS_VALUE_EDIT_PRESENTER_HPP
