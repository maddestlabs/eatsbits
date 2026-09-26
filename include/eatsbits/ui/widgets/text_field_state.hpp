#pragma once

#include <string>
#include <algorithm>

namespace eatsbits::ui {

/**
 * @brief Reusable text field buffer with full selection and cursor navigation support.
 * Features:
 * - Selection range tracking [anchor, cursor]
 * - Instant replacement of active selection upon typing
 * - Select All (Ctrl+A, default dialog focus)
 * - Directional cursor navigation with or without Shift (expanding selection)
 * - Selection-aware Backspace and Delete
 * - Mouse drag-to-select and click-to-position
 */
class TextFieldState {
public:
    TextFieldState() = default;
    explicit TextFieldState(std::string initialText, bool selectAllOnSet = true) {
        setText(std::move(initialText), selectAllOnSet);
    }

    void setText(std::string newText, bool selectAllOnSet = true) {
        text_ = std::move(newText);
        if (selectAllOnSet) {
            selectAll();
        } else {
            cursor_ = static_cast<int>(text_.length());
            anchor_ = cursor_;
        }
    }

    [[nodiscard]] const std::string& getText() const noexcept { return text_; }
    [[nodiscard]] bool empty() const noexcept { return text_.empty(); }
    [[nodiscard]] size_t length() const noexcept { return text_.length(); }

    [[nodiscard]] int getCursor() const noexcept { return cursor_; }
    void setCursor(int pos, bool keepAnchor = false) noexcept {
        cursor_ = std::clamp(pos, 0, static_cast<int>(text_.length()));
        if (!keepAnchor) {
            anchor_ = cursor_;
        }
    }

    [[nodiscard]] int getAnchor() const noexcept { return anchor_; }
    [[nodiscard]] bool hasSelection() const noexcept { return cursor_ != anchor_; }

    [[nodiscard]] int getSelectionStart() const noexcept {
        return std::min(cursor_, anchor_);
    }

    [[nodiscard]] int getSelectionEnd() const noexcept {
        return std::max(cursor_, anchor_);
    }

    [[nodiscard]] int getSelectionLength() const noexcept {
        return getSelectionEnd() - getSelectionStart();
    }

    [[nodiscard]] std::string getSelectedText() const {
        if (!hasSelection()) return "";
        return text_.substr(static_cast<size_t>(getSelectionStart()),
                           static_cast<size_t>(getSelectionLength()));
    }

    void selectAll() noexcept {
        anchor_ = 0;
        cursor_ = static_cast<int>(text_.length());
    }

    void clearSelection() noexcept {
        anchor_ = cursor_;
    }

    bool deleteSelection() {
        if (!hasSelection()) return false;
        int s = getSelectionStart();
        int e = getSelectionEnd();
        text_.erase(static_cast<size_t>(s), static_cast<size_t>(e - s));
        cursor_ = s;
        anchor_ = s;
        return true;
    }

    void insertChar(char c) {
        deleteSelection();
        text_.insert(static_cast<size_t>(cursor_), 1, c);
        cursor_++;
        anchor_ = cursor_;
    }

    void insertText(const std::string& s) {
        if (s.empty()) return;
        deleteSelection();
        text_.insert(static_cast<size_t>(cursor_), s);
        cursor_ += static_cast<int>(s.length());
        anchor_ = cursor_;
    }

    void backspace() {
        if (deleteSelection()) return;
        if (cursor_ > 0) {
            text_.erase(static_cast<size_t>(cursor_ - 1), 1);
            cursor_--;
            anchor_ = cursor_;
        }
    }

    void forwardDelete() {
        if (deleteSelection()) return;
        if (cursor_ < static_cast<int>(text_.length())) {
            text_.erase(static_cast<size_t>(cursor_), 1);
            anchor_ = cursor_;
        }
    }

    void moveLeft(bool select = false) {
        if (!select && hasSelection()) {
            cursor_ = getSelectionStart();
            anchor_ = cursor_;
        } else if (cursor_ > 0) {
            cursor_--;
            if (!select) anchor_ = cursor_;
        }
    }

    void moveRight(bool select = false) {
        if (!select && hasSelection()) {
            cursor_ = getSelectionEnd();
            anchor_ = cursor_;
        } else if (cursor_ < static_cast<int>(text_.length())) {
            cursor_++;
            if (!select) anchor_ = cursor_;
        }
    }

    void moveHome(bool select = false) {
        cursor_ = 0;
        if (!select) anchor_ = 0;
    }

    void moveEnd(bool select = false) {
        cursor_ = static_cast<int>(text_.length());
        if (!select) anchor_ = cursor_;
    }

    void clear() {
        text_.clear();
        cursor_ = 0;
        anchor_ = 0;
    }

private:
    std::string text_{};
    int cursor_{0};
    int anchor_{0};
};

} // namespace eatsbits::ui
