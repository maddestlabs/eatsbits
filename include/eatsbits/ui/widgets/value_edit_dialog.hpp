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

namespace eatsbits::ui {

/**
 * @brief Configuration request for opening the manual value edit dialog.
 * Supports hard value entry, percentage calculation mode ('%'), text editing,
 * default resets, and parameter units.
 */
struct ValueEditRequest {
    std::string title{"EDIT VALUE"};
    std::string paramName{""};
    float currentValue{0.0f};
    float minValue{0.0f};
    float maxValue{1.0f};
    float defaultValue{0.0f};
    bool hasDefault{true};
    bool isInteger{false};
    bool allowPercentage{true};
    std::string unit{""}; // e.g. "dB", "Hz", "%", "BPM"
    Color accentColor{0.0f, 0.90f, 1.0f, 1.0f}; // Default Cyan/Amber theme accent
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

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);

    [[nodiscard]] const ValueEditRequest& getRequest() const noexcept { return req_; }
    [[nodiscard]] const std::string& getInputText() const noexcept { return textModel_.getText(); }
    void setInputText(const std::string& txt, bool selectAll = true) { textModel_.setText(txt, selectAll); }
    [[nodiscard]] bool isPercentMode() const noexcept { return isPercentMode_; }
    void setPercentMode(bool enabled);
    void togglePercentMode();
    void resetToDefault();
    void submit();
    [[nodiscard]] const Rect2D& getDialogBounds() const noexcept { return dialogBounds_; }

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

    std::function<void(const std::string& text)> onCopyToClipboard;

private:
    void formatBufferFromValue(float val);
    void setQuickPercent(float pct);

    bool isOpen_{false};
    ValueEditRequest req_{};
    bool isPercentMode_{false};
    TextFieldState textModel_{};
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
    std::vector<std::pair<Rect2D, float>> quickPercentPills_{};
};

} // namespace eatsbits::ui

#endif // EATS_VALUE_EDIT_DIALOG_HPP
