#pragma once

#include "eatsbits/presenter/text_presenter.hpp"
#include "eatsbits/ui/input/focus_manager.hpp"
#include "eatsbits/ui/geometry.hpp"
#include "eatsbits/ui/theme.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/views/view_base.hpp"
#include <string>
#include <functional>
#include <chrono>

namespace eatsbits::ui {

/**
 * @brief Reusable, focusable single-line text input widget with auto-scroll containment,
 * drag-selection, double-click word selection, clipboard copy/paste, and no text bleeding.
 */
class TextFieldWidget : public IFocusable {
public:
    TextFieldWidget() = default;
    explicit TextFieldWidget(std::string initialText, std::string placeholder = "")
        : presenter_(std::move(initialText), /*selectAllOnSet=*/ false),
          placeholder_(std::move(placeholder)) {}

    // Focusable implementation
    bool onFocusGained() override {
        isFocused_ = true;
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    void onFocusLost() override {
        isFocused_ = false;
        isDraggingSelection_ = false;
        presenter_.clearSelection();
    }

    [[nodiscard]] bool isFocused() const noexcept override {
        return isFocused_;
    }

    // Geometry & Layout
    void setBounds(const Rect2D& b) noexcept { bounds_ = b; }
    void setBounds(float x, float y, float w, float h) noexcept { bounds_ = Rect2D{x, y, w, h}; }
    [[nodiscard]] const Rect2D& getBounds() const noexcept { return bounds_; }

    void setPadding(float pad) noexcept { padding_ = pad; }
    [[nodiscard]] float getPadding() const noexcept { return padding_; }

    void setFontSize(float sz) noexcept { fontSize_ = sz; }
    [[nodiscard]] float getFontSize() const noexcept { return fontSize_; }

    // Text & State
    void setText(std::string text, bool selectAll = false) {
        presenter_.setText(std::move(text), selectAll);
        if (onTextChanged) onTextChanged(presenter_.getText());
    }
    [[nodiscard]] const std::string& getText() const noexcept { return presenter_.getText(); }

    void setPlaceholder(std::string ph) { placeholder_ = std::move(ph); }
    [[nodiscard]] const std::string& getPlaceholder() const noexcept { return placeholder_; }

    void setCharFilter(std::function<bool(char32_t)> filter) {
        presenter_.setCharFilter(std::move(filter));
    }

    [[nodiscard]] presenter::SingleLineTextPresenter& getPresenter() noexcept { return presenter_; }
    [[nodiscard]] const presenter::SingleLineTextPresenter& getPresenter() const noexcept { return presenter_; }

    // Interaction & Event Handling
    bool handlePointer(const PointerEvent& ev, const ViewContext& ctx);
    bool handleKey(int key, int scancode, int action, int mods) override;
    bool handleChar(char32_t codepoint) override;

    // Rendering
    void render(BatchRenderer2D& r, const ThemeTokens& theme, const Color& accentColor);

    // Callbacks
    std::function<void(const std::string&)> onTextChanged{nullptr};
    std::function<void(const std::string&)> onCommit{nullptr};
    std::function<void()> onCancel{nullptr};

private:
    Rect2D bounds_{0.0f, 0.0f, 200.0f, 32.0f};
    float padding_{10.0f};
    float fontSize_{14.0f};
    std::string placeholder_{};

    presenter::SingleLineTextPresenter presenter_{};
    bool isFocused_{false};
    bool isDraggingSelection_{false};
    float cursorBlinkTimer_{0.0f};
    IClipboard* clipboard_{nullptr};

    std::chrono::steady_clock::time_point lastClickTime_{};
    int clickCount_{0};
};

} // namespace eatsbits::ui
