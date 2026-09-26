#include "eatsbits/ui/widgets/value_edit_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iomanip>

namespace eatsbits::ui {

ValueEditDialog::ValueEditDialog() {
    dialogBounds_ = Rect2D(0.0f, 0.0f, 370.0f, 240.0f);
}

void ValueEditDialog::open(const ValueEditRequest& req) {
    isOpen_ = true;
    req_ = req;
    isPercentMode_ = false;
    cursorBlinkTimer_ = 0.0f;
    isDraggingSelection_ = false;
    formatBufferFromValue(req_.currentValue);
}

void ValueEditDialog::formatBufferFromValue(float val) {
    std::string s;
    if (req_.isInteger) {
        s = std::to_string(static_cast<int>(std::round(val)));
    } else {
        char buf[64];
        if (std::abs(val) < 10.0f) {
            std::snprintf(buf, sizeof(buf), "%.2f", val);
        } else {
            std::snprintf(buf, sizeof(buf), "%.1f", val);
        }
        // Trim trailing zeros after decimal point
        s = buf;
        if (s.find('.') != std::string::npos) {
            while (s.back() == '0') s.pop_back();
            if (s.back() == '.') s.pop_back();
        }
    }
    // Existing value is selected by default so any typed input immediately replaces it
    textModel_.setText(s, /*selectAllOnSet=*/ true);
}

void ValueEditDialog::setPercentMode(bool enabled) {
    if (isPercentMode_ == enabled) return;
    togglePercentMode();
}

void ValueEditDialog::togglePercentMode() {
    if (!req_.allowPercentage || req_.maxValue <= req_.minValue) return;

    float current = 0.0f;
    try {
        current = std::stof(textModel_.getText());
    } catch (...) {
        current = req_.currentValue;
    }

    if (!isPercentMode_) {
        // Direct value -> Percentage (0% - 100%)
        float pct = ((current - req_.minValue) / (req_.maxValue - req_.minValue)) * 100.0f;
        pct = std::clamp(pct, 0.0f, 100.0f);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", pct);
        std::string s(buf);
        if (s.find('.') != std::string::npos) {
            while (s.back() == '0') s.pop_back();
            if (s.back() == '.') s.pop_back();
        }
        textModel_.setText(s, /*selectAllOnSet=*/ true);
        isPercentMode_ = true;
    } else {
        // Percentage -> Direct value
        float val = req_.minValue + (current / 100.0f) * (req_.maxValue - req_.minValue);
        val = std::clamp(val, req_.minValue, req_.maxValue);
        formatBufferFromValue(val);
        isPercentMode_ = false;
    }
}

void ValueEditDialog::resetToDefault() {
    if (!req_.hasDefault) return;
    isPercentMode_ = false;
    formatBufferFromValue(req_.defaultValue);
}

void ValueEditDialog::setQuickPercent(float pct) {
    if (!req_.allowPercentage || req_.maxValue <= req_.minValue) return;
    if (isPercentMode_) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(pct)));
        textModel_.setText(buf, /*selectAllOnSet=*/ true);
    } else {
        float val = req_.minValue + (pct / 100.0f) * (req_.maxValue - req_.minValue);
        val = std::clamp(val, req_.minValue, req_.maxValue);
        formatBufferFromValue(val);
    }
}

void ValueEditDialog::submit() {
    if (!isOpen_) return;

    const std::string& bufStr = textModel_.getText();
    if (bufStr.empty()) {
        close();
        return;
    }

    // Check if inputBuffer contains '%'
    bool hasPercentChar = (bufStr.find('%') != std::string::npos);
    std::string cleanStr = bufStr;
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), '%'), cleanStr.end());
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), ' '), cleanStr.end());

    float parsed = 0.0f;
    bool valid = false;
    try {
        parsed = std::stof(cleanStr);
        valid = true;
    } catch (...) {
        valid = false;
    }

    if (valid) {
        if ((isPercentMode_ || hasPercentChar) && req_.maxValue > req_.minValue) {
            float val = req_.minValue + (parsed / 100.0f) * (req_.maxValue - req_.minValue);
            val = std::clamp(val, req_.minValue, req_.maxValue);
            if (req_.onCommit) {
                req_.onCommit(val);
            }
        } else {
            float val = std::clamp(parsed, req_.minValue, req_.maxValue);
            if (req_.onCommit) {
                req_.onCommit(val);
            }
        }
    }

    if (req_.onCommitText) {
        req_.onCommitText(textModel_.getText());
    }

    close();
}

void ValueEditDialog::layout(float screenW, float screenH) {
    screenWidth_ = screenW;
    screenHeight_ = screenH;

    float dw = 370.0f;
    float dh = 240.0f;
    float dx = std::max(12.0f, (screenW - dw) * 0.5f);
    float dy = std::max(12.0f, (screenH - dh) * 0.5f);
    dialogBounds_ = Rect2D(dx, dy, dw, dh);

    closeBtnBounds_ = Rect2D(dx + dw - 34.0f, dy + 12.0f, 22.0f, 22.0f);
    percentToggleBounds_ = Rect2D(dx + dw - 74.0f, dy + 12.0f, 32.0f, 22.0f);
    defaultBtnBounds_ = Rect2D(dx + dw - 150.0f, dy + 12.0f, 70.0f, 22.0f);

    inputFieldBounds_ = Rect2D(dx + 20.0f, dy + 68.0f, dw - 40.0f, 40.0f);

    // 5 Quick percentage shortcut pills: 0%, 25%, 50%, 75%, 100%
    quickPercentPills_.clear();
    float pillY = dy + 124.0f;
    float pillW = 54.0f;
    float pillGap = 15.0f;
    float startPillX = dx + 20.0f;
    const float pcts[5] = {0.0f, 25.0f, 50.0f, 75.0f, 100.0f};
    for (int i = 0; i < 5; ++i) {
        quickPercentPills_.push_back({
            Rect2D(startPillX + static_cast<float>(i) * (pillW + pillGap), pillY, pillW, 24.0f),
            pcts[i]
        });
    }

    // Action buttons: CANCEL, OK
    cancelBtnBounds_ = Rect2D(dx + dw - 180.0f, dy + dh - 42.0f, 76.0f, 28.0f);
    okBtnBounds_ = Rect2D(dx + dw - 94.0f, dy + dh - 42.0f, 74.0f, 28.0f);
}

void ValueEditDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. High-performance GPU / Framebuffer soft blur & soft dimming backdrop
    r.applyBackdropBlur(4.0f, 0.48f);

    // Semi-transparent deep darkening overlay
    drawRect(r, 0.0f, 0.0f, screenWidth_, screenHeight_, 0.0f, 0.0f, 0.0f, 0.45f);

    const auto& accent = req_.accentColor;
    float dx = dialogBounds_.x;
    float dy = dialogBounds_.y;
    float dw = dialogBounds_.w;
    float dh = dialogBounds_.h;

    // 2. Dialog Chassis with drop shadow, theme background & subtle gold/accent outline
    drawRoundedRect(r, dx - 3.0f, dy - 3.0f, dw + 6.0f, dh + 6.0f, 13.0f, 0.0f, 0.0f, 0.0f, 0.55f);
    drawRoundedRect(r, dx, dy, dw, dh, 12.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawRoundedRectOutline(r, dx, dy, dw, dh, 12.0f,
                           accent.r, accent.g, accent.b, 0.85f, 1.5f);

    // 3. Header Strip: Title, Accent Dot, % Toggle, Default button, Close button
    drawCircle(r, dx + 22.0f, dy + 23.0f, 4.0f, accent.r, accent.g, accent.b, 1.0f);
    drawText(r, req_.title, dx + 32.0f, dy + 17.0f, 12.5f, accent.r, accent.g, accent.b, 1.0f);

    // Default button
    if (req_.hasDefault) {
        drawRoundedRect(r, defaultBtnBounds_.x, defaultBtnBounds_.y, defaultBtnBounds_.w, defaultBtnBounds_.h, 4.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
        drawRoundedRectOutline(r, defaultBtnBounds_.x, defaultBtnBounds_.y, defaultBtnBounds_.w, defaultBtnBounds_.h, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);
        drawText(r, "DEFAULT", defaultBtnBounds_.x + 10.0f, defaultBtnBounds_.y + 6.0f, 9.5f,
                 theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    }

    // % Mode Toggle Chip
    if (req_.allowPercentage) {
        if (isPercentMode_) {
            drawRoundedRect(r, percentToggleBounds_.x, percentToggleBounds_.y, percentToggleBounds_.w, percentToggleBounds_.h, 4.0f,
                            accent.r * 0.25f, accent.g * 0.25f, accent.b * 0.25f, 1.0f);
            drawRoundedRectOutline(r, percentToggleBounds_.x, percentToggleBounds_.y, percentToggleBounds_.w, percentToggleBounds_.h, 4.0f,
                                   accent.r, accent.g, accent.b, 1.0f, 1.2f);
            drawText(r, "%", percentToggleBounds_.x + 11.0f, percentToggleBounds_.y + 5.0f, 11.0f,
                     accent.r, accent.g, accent.b, 1.0f);
        } else {
            drawRoundedRect(r, percentToggleBounds_.x, percentToggleBounds_.y, percentToggleBounds_.w, percentToggleBounds_.h, 4.0f,
                            theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
            drawRoundedRectOutline(r, percentToggleBounds_.x, percentToggleBounds_.y, percentToggleBounds_.w, percentToggleBounds_.h, 4.0f,
                                   theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);
            drawText(r, "%", percentToggleBounds_.x + 11.0f, percentToggleBounds_.y + 5.0f, 11.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
        }
    }

    // Close button [x]
    drawRoundedRect(r, closeBtnBounds_.x, closeBtnBounds_.y, closeBtnBounds_.w, closeBtnBounds_.h, 4.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
    drawRoundedRectOutline(r, closeBtnBounds_.x, closeBtnBounds_.y, closeBtnBounds_.w, closeBtnBounds_.h, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);
    drawText(r, "X", closeBtnBounds_.x + 7.0f, closeBtnBounds_.y + 5.5f, 10.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);

    // Subtitle / Range hint
    std::string hint;
    if (isPercentMode_) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "PERCENT MODE (0%% - 100%% of %.2f - %.2f%s)",
                      req_.minValue, req_.maxValue, req_.unit.empty() ? "" : (" " + req_.unit).c_str());
        hint = buf;
    } else {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "RANGE: %.2f - %.2f%s",
                      req_.minValue, req_.maxValue, req_.unit.empty() ? "" : (" " + req_.unit).c_str());
        hint = buf;
    }
    drawText(r, hint, dx + 22.0f, dy + 45.0f, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);

    // 4. Input Field (Recessed dark well with active glowing accent border)
    drawRoundedRect(r, inputFieldBounds_.x, inputFieldBounds_.y, inputFieldBounds_.w, inputFieldBounds_.h, 6.0f,
                    theme.controlWell.r, theme.controlWell.g, theme.controlWell.b, 1.0f);
    drawRoundedRectOutline(r, inputFieldBounds_.x, inputFieldBounds_.y, inputFieldBounds_.w, inputFieldBounds_.h, 6.0f,
                           accent.r, accent.g, accent.b, 0.95f, 1.5f);

    // Formatted Text and Selection Highlight
    const std::string& textToShow = textModel_.getText();
    float textStartX = inputFieldBounds_.x + 12.0f;
    float charW = getMonoCharAdvance(16.0f);

    // Draw active text selection highlight if present
    if (textModel_.hasSelection()) {
        int s = textModel_.getSelectionStart();
        int e = textModel_.getSelectionEnd();
        float selX1 = textStartX + static_cast<float>(s) * charW;
        float selX2 = textStartX + static_cast<float>(e) * charW;
        float selW = std::max(2.0f, selX2 - selX1);
        float selY = inputFieldBounds_.y + 6.0f;
        float selH = inputFieldBounds_.h - 12.0f;

        // Glowing selection backdrop pill
        drawRoundedRect(r, selX1, selY, selW, selH, 3.0f,
                        accent.r, accent.g, accent.b, 0.38f);
        drawRoundedRectOutline(r, selX1, selY, selW, selH, 3.0f,
                               accent.r, accent.g, accent.b, 0.70f, 1.0f);
    }

    drawMonoText(r, textToShow, textStartX, inputFieldBounds_.y + 11.0f, 16.0f,
                 accent.r, accent.g, accent.b, 1.0f);

    // Suffix '%' or unit
    if (isPercentMode_) {
        float suffixX = textStartX + 4.0f + static_cast<float>(textToShow.length()) * charW;
        drawMonoText(r, "%", suffixX, inputFieldBounds_.y + 11.0f, 15.0f, accent.r, accent.g, accent.b, 0.85f);
    } else if (!req_.unit.empty()) {
        float suffixX = textStartX + 4.0f + static_cast<float>(textToShow.length()) * charW;
        drawMonoText(r, req_.unit, suffixX, inputFieldBounds_.y + 12.0f, 13.0f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.75f);
    }

    // Blinking Cursor
    cursorBlinkTimer_ += 0.035f;
    if (std::fmod(cursorBlinkTimer_, 1.0f) < 0.5f) {
        int cur = textModel_.getCursor();
        float curX = textStartX + static_cast<float>(cur) * charW;
        drawLine(r, curX, inputFieldBounds_.y + 8.0f, curX, inputFieldBounds_.y + inputFieldBounds_.h - 8.0f,
                 accent.r, accent.g, accent.b, 1.0f, 1.5f);
    }

    // 5. Quick Percentage Shortcut Pills: 0%, 25%, 50%, 75%, 100%
    if (req_.allowPercentage) {
        const char* pillLabels[5] = {"0%", "25%", "50%", "75%", "100%"};
        for (size_t i = 0; i < quickPercentPills_.size() && i < 5; ++i) {
            const auto& p = quickPercentPills_[i].first;
            drawRoundedRect(r, p.x, p.y, p.w, p.h, 4.0f,
                            theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
            drawRoundedRectOutline(r, p.x, p.y, p.w, p.h, 4.0f,
                                   theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
            drawText(r, pillLabels[i], p.x + 14.0f, p.y + 6.0f, 9.5f,
                     theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
        }
    }

    // 6. Action Buttons: CANCEL / OK
    // CANCEL button
    drawRoundedRect(r, cancelBtnBounds_.x, cancelBtnBounds_.y, cancelBtnBounds_.w, cancelBtnBounds_.h, 5.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
    drawRoundedRectOutline(r, cancelBtnBounds_.x, cancelBtnBounds_.y, cancelBtnBounds_.w, cancelBtnBounds_.h, 5.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.0f);
    drawText(r, "CANCEL", cancelBtnBounds_.x + 16.0f, cancelBtnBounds_.y + 8.0f, 10.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);

    // OK button
    drawRoundedRect(r, okBtnBounds_.x, okBtnBounds_.y, okBtnBounds_.w, okBtnBounds_.h, 5.0f,
                    accent.r, accent.g, accent.b, 1.0f);
    drawRoundedRectOutline(r, okBtnBounds_.x, okBtnBounds_.y, okBtnBounds_.w, okBtnBounds_.h, 5.0f,
                           accent.r, accent.g, accent.b, 1.0f, 1.2f);
    drawText(r, "OK", okBtnBounds_.x + 27.0f, okBtnBounds_.y + 8.0f, 11.0f,
             0.05f, 0.07f, 0.09f, 1.0f); // High contrast dark text on accent background
}

bool ValueEditDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;

    if (ev.action == PointerAction::Down) {
        // Click outside dialog chassis -> dismiss modal
        if (!dialogBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        // Close button [x]
        if (closeBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        // % mode toggle chip
        if (req_.allowPercentage && percentToggleBounds_.contains(ev.x, ev.y)) {
            togglePercentMode();
            return true;
        }

        // [ DEFAULT ] button
        if (req_.hasDefault && defaultBtnBounds_.contains(ev.x, ev.y)) {
            formatBufferFromValue(req_.defaultValue);
            isPercentMode_ = false;
            submit();
            return true;
        }

        // Quick percentage shortcut pills
        if (req_.allowPercentage) {
            for (const auto& pill : quickPercentPills_) {
                if (pill.first.contains(ev.x, ev.y)) {
                    setQuickPercent(pill.second);
                    return true;
                }
            }
        }

        // CANCEL button
        if (cancelBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        // OK button
        if (okBtnBounds_.contains(ev.x, ev.y)) {
            submit();
            return true;
        }

        // Click inside Input Field -> Place cursor or start drag selection
        if (inputFieldBounds_.contains(ev.x, ev.y)) {
            float textStartX = inputFieldBounds_.x + 12.0f;
            float charW = getMonoCharAdvance(16.0f);
            int idx = std::clamp(static_cast<int>(std::round((ev.x - textStartX) / charW)),
                                 0, static_cast<int>(textModel_.length()));
            textModel_.setCursor(idx, /*keepAnchor=*/ ev.mods.shift);
            isDraggingSelection_ = true;
            cursorBlinkTimer_ = 0.0f;
            return true;
        }

        // Intercept any click inside modal
        return true;
    }

    if (ev.action == PointerAction::Move) {
        if (isDraggingSelection_) {
            float textStartX = inputFieldBounds_.x + 12.0f;
            float charW = getMonoCharAdvance(16.0f);
            int idx = std::clamp(static_cast<int>(std::round((ev.x - textStartX) / charW)),
                                 0, static_cast<int>(textModel_.length()));
            textModel_.setCursor(idx, /*keepAnchor=*/ true);
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
    }

    if (ev.action == PointerAction::Up) {
        if (isDraggingSelection_) {
            isDraggingSelection_ = false;
            return true;
        }
    }

    return true; // Intercept all pointer events while modal is open
}

bool ValueEditDialog::handleKey(int key, int /*scancode*/, int action, int mods) {
    if (!isOpen_ || action == 0 /* GLFW_RELEASE */) return false;

    bool isShift = (mods & 0x0001) != 0;
    bool isCtrl = (mods & 0x0002) != 0;

    // Enter / Return -> Submit
    if (key == 257 /* GLFW_KEY_ENTER */ || key == 335 /* GLFW_KEY_KP_ENTER */) {
        submit();
        return true;
    }

    // Escape -> Close
    if (key == 256 /* GLFW_KEY_ESCAPE */) {
        close();
        return true;
    }

    // Ctrl shortcuts
    if (isCtrl) {
        // Ctrl+A -> Select All
        if (key == 65 || key == 97) {
            textModel_.selectAll();
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        // Ctrl+C -> Copy selected text
        if (key == 67 || key == 99) {
            if (onCopyToClipboard && textModel_.hasSelection()) {
                onCopyToClipboard(textModel_.getSelectedText());
            }
            return true;
        }
        // Ctrl+X -> Cut selected text
        if (key == 88 || key == 120) {
            if (textModel_.hasSelection()) {
                if (onCopyToClipboard) {
                    onCopyToClipboard(textModel_.getSelectedText());
                }
                textModel_.deleteSelection();
                cursorBlinkTimer_ = 0.0f;
            }
            return true;
        }
    }

    // Left Arrow
    if (key == 263 /* GLFW_KEY_LEFT */) {
        textModel_.moveLeft(isShift);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Right Arrow
    if (key == 262 /* GLFW_KEY_RIGHT */) {
        textModel_.moveRight(isShift);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Home
    if (key == 268 /* GLFW_KEY_HOME */) {
        textModel_.moveHome(isShift);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // End
    if (key == 269 /* GLFW_KEY_END */) {
        textModel_.moveEnd(isShift);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Backspace
    if (key == 259 /* GLFW_KEY_BACKSPACE */) {
        textModel_.backspace();
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Delete
    if (key == 261 /* GLFW_KEY_DELETE */) {
        textModel_.forwardDelete();
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Digits '0' through '9'
    if (key >= 48 && key <= 57) {
        textModel_.insertChar(static_cast<char>('0' + (key - 48)));
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Keypad 0-9 (GLFW_KEY_KP_0 through KP_9: 320 - 329)
    if (key >= 320 && key <= 329) {
        textModel_.insertChar(static_cast<char>('0' + (key - 320)));
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Period / Decimal point ('.' = 46, KP_DECIMAL = 330)
    if (key == 46 || key == 330) {
        std::string candidate = textModel_.getText();
        if (textModel_.hasSelection()) {
            candidate.erase(static_cast<size_t>(textModel_.getSelectionStart()),
                            static_cast<size_t>(textModel_.getSelectionLength()));
        }
        if (candidate.find('.') == std::string::npos) {
            textModel_.insertChar('.');
            cursorBlinkTimer_ = 0.0f;
        }
        return true;
    }

    // Minus / Hyphen ('-' = 45, KP_SUBTRACT = 333)
    if (key == 45 || key == 333) {
        textModel_.insertChar('-');
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // '%' toggle or character entry
    if (key == 53 /* '%' */) {
        if (isShift) {
            togglePercentMode();
        } else {
            textModel_.insertChar('%');
            cursorBlinkTimer_ = 0.0f;
        }
        return true;
    }

    return true; // Modal consumes key events
}

} // namespace eatsbits::ui
