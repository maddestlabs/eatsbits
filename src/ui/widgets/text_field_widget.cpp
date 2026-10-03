#include "eatsbits/ui/widgets/text_field_widget.hpp"
#include <cmath>
#include <algorithm>

namespace eatsbits::ui {

bool TextFieldWidget::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    if (ctx.clipboard) {
        clipboard_ = ctx.clipboard;
    }

    float charW = getMonoCharAdvance(fontSize_);
    float textStartX = bounds_.x + padding_ - presenter_.getScrollX();

    if (ev.action == PointerAction::Down) {
        if (!bounds_.contains(ev.x, ev.y)) {
            if (isFocused_) {
                if (ctx.focusManager && ctx.focusManager->hasFocus(this)) {
                    ctx.focusManager->clearFocus();
                }
            }
            return false;
        }

        if (ctx.focusManager) {
            ctx.focusManager->requestFocus(this);
        }

        auto now = std::chrono::steady_clock::now();
        auto diffMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClickTime_).count();
        lastClickTime_ = now;

        if (diffMs < 350) {
            clickCount_++;
        } else {
            clickCount_ = 1;
        }

        int idx = std::clamp(static_cast<int>(std::round((ev.x - textStartX) / charW)),
                             0, static_cast<int>(presenter_.length()));

        if (clickCount_ == 2) {
            presenter_.selectWordAt(idx);
        } else if (clickCount_ >= 3) {
            presenter_.selectAll();
        } else {
            presenter_.setCursor(idx, /*keepAnchor=*/ ev.mods.shift);
            isDraggingSelection_ = true;
        }

        cursorBlinkTimer_ = 0.0f;
        presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
        return true;
    }

    if (ev.action == PointerAction::Move) {
        if (isDraggingSelection_) {
            int idx = std::clamp(static_cast<int>(std::round((ev.x - textStartX) / charW)),
                                 0, static_cast<int>(presenter_.length()));
            if (idx != presenter_.getCursor()) {
                presenter_.setCursor(idx, /*keepAnchor=*/ true);
                cursorBlinkTimer_ = 0.0f;
                presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
            }
            return true;
        }
    }

    if (ev.action == PointerAction::Up) {
        if (isDraggingSelection_) {
            isDraggingSelection_ = false;
            return true;
        }
    }

    return bounds_.contains(ev.x, ev.y);
}

bool TextFieldWidget::handleKey(int key, int /*scancode*/, int action, int mods) {
    if (!isFocused_ || action == 0 /* GLFW_RELEASE */) return false;

    bool isShift = (mods & 0x0001) != 0;
    bool isCtrl = (mods & 0x0002) != 0;
    float charW = getMonoCharAdvance(fontSize_);
    std::string oldText = presenter_.getText();

    // Enter / Return -> Commit
    if (key == 257 /* GLFW_KEY_ENTER */ || key == 335 /* GLFW_KEY_KP_ENTER */) {
        if (onCommit) onCommit(presenter_.getText());
        return true;
    }

    // Escape -> Cancel
    if (key == 256 /* GLFW_KEY_ESCAPE */) {
        if (onCancel) onCancel();
        return true;
    }

    // Ctrl shortcuts
    if (isCtrl) {
        // Ctrl+A -> Select All
        if (key == 65 || key == 97) {
            presenter_.selectAll();
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        // Ctrl+C -> Copy
        if (key == 67 || key == 99) {
            if (clipboard_ && presenter_.hasSelection()) {
                clipboard_->setString(presenter_.getSelectedText());
            }
            return true;
        }
        // Ctrl+X -> Cut
        if (key == 88 || key == 120) {
            if (presenter_.hasSelection()) {
                if (clipboard_) {
                    clipboard_->setString(presenter_.getSelectedText());
                }
                presenter_.deleteSelection();
                cursorBlinkTimer_ = 0.0f;
                presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
                if (onTextChanged) onTextChanged(presenter_.getText());
            }
            return true;
        }
        // Ctrl+V -> Paste
        if (key == 86 || key == 118) {
            if (clipboard_) {
                std::string clip = clipboard_->getString();
                if (!clip.empty()) {
                    presenter_.insertText(clip);
                    cursorBlinkTimer_ = 0.0f;
                    presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
                    if (onTextChanged) onTextChanged(presenter_.getText());
                }
            }
            return true;
        }
        // Ctrl+Z -> Undo
        if (key == 90 || key == 122) {
            if (!isShift) {
                presenter_.undo();
            } else {
                presenter_.redo();
            }
            cursorBlinkTimer_ = 0.0f;
            presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
            if (onTextChanged) onTextChanged(presenter_.getText());
            return true;
        }
        // Ctrl+Y -> Redo
        if (key == 89 || key == 121) {
            presenter_.redo();
            cursorBlinkTimer_ = 0.0f;
            presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
            if (onTextChanged) onTextChanged(presenter_.getText());
            return true;
        }
    }

    // Left Arrow
    if (key == 263 /* GLFW_KEY_LEFT */) {
        presenter_.moveLeft(isShift, isCtrl);
        cursorBlinkTimer_ = 0.0f;
        presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
        return true;
    }

    // Right Arrow
    if (key == 262 /* GLFW_KEY_RIGHT */) {
        presenter_.moveRight(isShift, isCtrl);
        cursorBlinkTimer_ = 0.0f;
        presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
        return true;
    }

    // Home
    if (key == 268 /* GLFW_KEY_HOME */) {
        presenter_.moveHome(isShift);
        cursorBlinkTimer_ = 0.0f;
        presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
        return true;
    }

    // End
    if (key == 269 /* GLFW_KEY_END */) {
        presenter_.moveEnd(isShift);
        cursorBlinkTimer_ = 0.0f;
        presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
        return true;
    }

    // Backspace
    if (key == 259 /* GLFW_KEY_BACKSPACE */) {
        presenter_.backspace(/*wordDelete=*/ isCtrl);
        cursorBlinkTimer_ = 0.0f;
        presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
        if (onTextChanged) onTextChanged(presenter_.getText());
        return true;
    }

    // Delete
    if (key == 261 /* GLFW_KEY_DELETE */) {
        presenter_.forwardDelete(/*wordDelete=*/ isCtrl);
        cursorBlinkTimer_ = 0.0f;
        presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
        if (onTextChanged) onTextChanged(presenter_.getText());
        return true;
    }

    // Spacebar
    if (key == 32) {
        presenter_.insertChar(' ');
        cursorBlinkTimer_ = 0.0f;
        presenter_.ensureCursorVisible(bounds_.w, padding_, charW);
        if (onTextChanged) onTextChanged(presenter_.getText());
        return true;
    }

    return true; // Absorb keys while focused
}

bool TextFieldWidget::handleChar(char32_t codepoint) {
    if (!isFocused_) return false;
    if (codepoint < 32 || codepoint == 127) return false;

    presenter_.insertChar(codepoint);
    cursorBlinkTimer_ = 0.0f;
    float charW = getMonoCharAdvance(fontSize_);
    presenter_.ensureCursorVisible(bounds_.w, padding_, charW);

    if (onTextChanged) onTextChanged(presenter_.getText());
    return true;
}

void TextFieldWidget::render(BatchRenderer2D& r, const ThemeTokens& theme, const Color& accentColor) {
    // 1. Recessed dark well
    drawRoundedRect(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h, 5.0f,
                    theme.controlWell.r, theme.controlWell.g, theme.controlWell.b, 1.0f);

    // 2. Active glowing accent outline or subtle inactive border
    Color bdrCol = isFocused_ ? accentColor : Color(theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.70f);
    float bdrWidth = isFocused_ ? 1.5f : 1.0f;
    drawRoundedRectOutline(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h, 5.0f,
                           bdrCol.r, bdrCol.g, bdrCol.b, bdrCol.a, bdrWidth);

    float clipMinX = bounds_.x + padding_;
    float clipMaxX = bounds_.x + bounds_.w - padding_;
    float textY = bounds_.y + (bounds_.h - fontSize_) * 0.5f - 1.0f;
    float charW = getMonoCharAdvance(fontSize_);
    float textStartX = clipMinX - presenter_.getScrollX();

    // 3. Selection Highlight Pill
    if (presenter_.hasSelection()) {
        int s = presenter_.getSelectionStart();
        int e = presenter_.getSelectionEnd();
        float selX1 = std::clamp(textStartX + static_cast<float>(s) * charW, clipMinX, clipMaxX);
        float selX2 = std::clamp(textStartX + static_cast<float>(e) * charW, clipMinX, clipMaxX);
        float selW = selX2 - selX1;

        if (selW > 0.5f) {
            float selY = bounds_.y + 4.0f;
            float selH = bounds_.h - 8.0f;
            drawRoundedRect(r, selX1, selY, selW, selH, 3.0f,
                            accentColor.r, accentColor.g, accentColor.b, 0.35f);
            drawRoundedRectOutline(r, selX1, selY, selW, selH, 3.0f,
                                   accentColor.r, accentColor.g, accentColor.b, 0.70f, 1.0f);
        }
    }

    // 4. Text or Placeholder (strictly clipped to padding box)
    const std::string& text = presenter_.getText();
    if (text.empty() && !placeholder_.empty() && !isFocused_) {
        drawMonoTextClipped(r, placeholder_, textStartX, textY, fontSize_, clipMinX, clipMaxX,
                            theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.60f);
    } else {
        drawMonoTextClipped(r, text, textStartX, textY, fontSize_, clipMinX, clipMaxX,
                            accentColor.r, accentColor.g, accentColor.b, 1.0f);
    }

    // 5. Blinking Cursor (only when focused)
    if (isFocused_) {
        cursorBlinkTimer_ += 0.035f;
        if (std::fmod(cursorBlinkTimer_, 1.0f) < 0.55f) {
            int cur = presenter_.getCursor();
            float curX = textStartX + static_cast<float>(cur) * charW;
            if (curX >= clipMinX && curX <= clipMaxX) {
                drawLine(r, curX, bounds_.y + 6.0f, curX, bounds_.y + bounds_.h - 6.0f,
                         accentColor.r, accentColor.g, accentColor.b, 1.0f, 1.5f);
            }
        }
    }
}

} // namespace eatsbits::ui
