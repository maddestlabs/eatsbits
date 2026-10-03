#ifndef EATS_VALUE_EDIT_DIALOG_HPP
#define EATS_VALUE_EDIT_DIALOG_HPP

#include <string>
#include <functional>
#include <vector>
#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "text_field_state.hpp"
#include "eatsbits/presenter/value_edit_presenter.hpp"

namespace eatsbits::ui {

/**
 * @brief Configuration request for opening the manual value edit dialog.
 * Supports hard value entry, percentage calculation mode ('%'), text editing,
 * default resets, and parameter units.
 */
struct ValueEditRequest {
    std::string title{"EDIT VALUE"};
    std::string paramName{""};
    bool isTextMode{false};
    std::string initialText{""};
    std::string currentIconRef{""};
    float currentValue{0.0f};
    float minValue{0.0f};
    float maxValue{1.0f};
    float defaultValue{0.0f};
    bool hasDefault{true};
    bool isInteger{false};
    bool allowPercentage{true};
    std::string unit{""}; // e.g. "dB", "Hz", "%", "BPM"
    Color accentColor{0.0f, 0.90f, 1.0f, 1.0f}; // Default Cyan/Amber theme accent
    std::string actionLinkLabel{""};
    std::function<void()> onActionLink;
    std::function<void(float newValue)> onCommit;
    std::function<void(const std::string& newText)> onCommitText;
};

/**
 * @brief Reusable modal dialog for entering numeric parameter values or text.
 * Center-positioned popover with soft backdrop blur and fade, % percentage mode,
 * quick preset pills, direct typing, and full keyboard navigation.
 * Parity with Flutter's CompactValueEditDialog from Eatsbeats.
 */
class ValueEditDialog {
public:
    ValueEditDialog();
    ~ValueEditDialog() = default;

    void open(const ValueEditRequest& req);
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }
    void setIconRef(const std::string& iconRef) noexcept { req_.currentIconRef = iconRef; }
    [[nodiscard]] const std::string& getIconRef() const noexcept { return req_.currentIconRef; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);

    [[nodiscard]] const ValueEditRequest& getRequest() const noexcept { return req_; }
    [[nodiscard]] const std::string& getInputText() const noexcept { return textModel_.getText(); }
    void setInputText(const std::string& txt, bool selectAll = true) { textModel_.setText(txt, selectAll); }
    [[nodiscard]] bool isPercentMode() const noexcept { return isPercentMode_; }
    [[nodiscard]] bool isTextMode() const noexcept { return req_.isTextMode; }
    void setPercentMode(bool enabled);
    void togglePercentMode();
    void resetToDefault();
    void submit();
    [[nodiscard]] const Rect2D& getDialogBounds() const noexcept { return dialogBounds_; }
    [[nodiscard]] const Rect2D& getCloseButtonBounds() const noexcept { return closeBtnBounds_; }

    // Text Selection & Cursor inspection / control
    [[nodiscard]] bool hasSelection() const noexcept { return textModel_.hasSelection(); }
    [[nodiscard]] std::string getSelectedText() const { return textModel_.getSelectedText(); }
    [[nodiscard]] int getSelectionStart() const noexcept { return textModel_.getSelectionStart(); }
    [[nodiscard]] int getSelectionEnd() const noexcept { return textModel_.getSelectionEnd(); }
    void selectAll() noexcept { textModel_.selectAll(); }
    void clearSelection() noexcept { textModel_.clearSelection(); }
    [[nodiscard]] int getCursorPosition() const noexcept { return textModel_.getCursor(); }
    void setCursorPosition(int pos, bool keepAnchor = false) noexcept { textModel_.setCursor(pos, keepAnchor); }
    [[nodiscard]] const TextFieldState& getTextFieldState() const noexcept { return textModel_; }
    [[nodiscard]] TextFieldState& getTextFieldState() noexcept { return textModel_; }

    [[nodiscard]] const presenter::ValueEditPresenter& getPresenter() const noexcept { return presenter_; }
    [[nodiscard]] presenter::ValueEditPresenter& getPresenter() noexcept { return presenter_; }

    std::function<void(const std::string& text)> onCopyToClipboard;
    std::function<std::string()> onPasteFromClipboard;

private:
    void formatBufferFromValue(float val);
    void setQuickPercent(float pct);

    bool isOpen_{false};
    presenter::ValueEditPresenter presenter_{};
    ValueEditRequest req_{};
    bool isPercentMode_{false};
    TextFieldState textModel_{};
    float inputScrollX_{0.0f};
    bool isDraggingSelection_{false};
    float cursorBlinkTimer_{0.0f};

    float screenWidth_{1280.0f};
    float screenHeight_{800.0f};

    // Hit test geometry
    Rect2D dialogBounds_{0.0f, 0.0f, 360.0f, 240.0f};
    Rect2D inputFieldBounds_{};
    Rect2D percentToggleBounds_{};
    Rect2D defaultBtnBounds_{};
    Rect2D closeBtnBounds_{};
    Rect2D cancelBtnBounds_{};
    Rect2D okBtnBounds_{};
    Rect2D actionLinkBounds_{};
    std::vector<std::pair<Rect2D, float>> quickPercentPills_{};
    float lastMouseX_{0.0f};
    float lastMouseY_{0.0f};
};

} // namespace eatsbits::ui

#endif // EATS_VALUE_EDIT_DIALOG_HPP
