#include "eatsbits/ui/widgets/text_editor_widget.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::ui {

TextEditorWidget::TextEditorWidget() {
    focusGainTime_ = std::chrono::steady_clock::now();
    charWidth_ = getMonoCharAdvance(10.0f);
}

void TextEditorWidget::layout(const Rect2D& bounds, const ViewContext& ctx) {
    if (ctx.clipboard) clipboard_ = ctx.clipboard;
    bounds_ = bounds;
    gutterBounds_ = Rect2D(bounds_.x, bounds_.y, gutterW_, bounds_.h);
    minimapBounds_ = Rect2D(bounds_.x + bounds_.w - minimapW_, bounds_.y, minimapW_, bounds_.h);
    float textW = std::max(0.0f, bounds_.w - gutterW_ - minimapW_);
    textAreaBounds_ = Rect2D(bounds_.x + gutterW_, bounds_.y, textW, bounds_.h);

    charWidth_ = getMonoCharAdvance(10.0f);
    presenter_.setViewport(textAreaBounds_.w, textAreaBounds_.h, lineHeight_, charWidth_);
}

void TextEditorWidget::setText(std::string_view text) {
    presenter_.setText(text);
}

std::string TextEditorWidget::getText() const {
    return presenter_.getDocument().getText();
}

core::TextCoord TextEditorWidget::coordFromPoint(float px, float py) const {
    const auto& doc = presenter_.getDocument();
    if (doc.getLineCount() == 0) return {0, 0};

    float relY = py - textAreaBounds_.y + presenter_.getScrollY();
    int line = static_cast<int>(std::floor(relY / lineHeight_));
    line = std::clamp(line, 0, static_cast<int>(doc.getLineCount()) - 1);

    float relX = px - textAreaBounds_.x - 6.0f + presenter_.getScrollX();
    int col = static_cast<int>(std::round(relX / charWidth_));
    int lineLen = static_cast<int>(doc.getLineLength(static_cast<size_t>(line)));
    col = std::clamp(col, 0, lineLen);

    return {line, col};
}

void TextEditorWidget::render(const ViewContext& ctx) {
    if (ctx.clipboard) clipboard_ = ctx.clipboard;
    if (!ctx.renderer || !ctx.theme) return;
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // 1. Overall Editor Background
    drawRect(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h, 0.06f, 0.07f, 0.09f, 1.0f);

    // 2. Active Line Highlight across text area and gutter
    int curLine = presenter_.getCursor().line;
    float activeLineY = textAreaBounds_.y + static_cast<float>(curLine) * lineHeight_ - presenter_.getScrollY();
    if (activeLineY >= bounds_.y - lineHeight_ && activeLineY <= bounds_.y + bounds_.h) {
        drawRect(r, bounds_.x, activeLineY, bounds_.w - minimapW_, lineHeight_, 1.0f, 1.0f, 1.0f, 0.045f);
    }

    // 3. Render Gutter and Text Content
    renderGutterAndText(ctx);

    // 4. Render High-Performance Code Minimap
    renderMinimap(ctx);

    // 5. Border Outline
    if (isFocused_) {
        drawRoundedRectOutline(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h, 2.0f,
                               theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.75f, 1.2f);
    } else {
        drawRoundedRectOutline(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h, 2.0f,
                               0.18f, 0.20f, 0.26f, 0.8f, 1.0f);
    }
}

void TextEditorWidget::renderGutterAndText(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;
    const auto& doc = presenter_.getDocument();

    // Line number gutter background
    drawRect(r, gutterBounds_.x, gutterBounds_.y, gutterBounds_.w, gutterBounds_.h, 0.08f, 0.09f, 0.12f, 1.0f);
    drawLine(r, gutterBounds_.x + gutterBounds_.w, gutterBounds_.y,
             gutterBounds_.x + gutterBounds_.w, gutterBounds_.y + gutterBounds_.h,
             0.16f, 0.18f, 0.24f, 1.0f, 1.0f);

    int totalLines = static_cast<int>(doc.getLineCount());
    int firstVisible = static_cast<int>(std::floor(presenter_.getScrollY() / lineHeight_));
    int lastVisible = static_cast<int>(std::ceil((presenter_.getScrollY() + textAreaBounds_.h) / lineHeight_));
    firstVisible = std::clamp(firstVisible, 0, std::max(0, totalLines - 1));
    lastVisible = std::clamp(lastVisible, 0, std::max(0, totalLines - 1));

    // Scissor text area so nothing spills into gutter or minimap
    r.pushScissor(textAreaBounds_.x, textAreaBounds_.y, textAreaBounds_.w, textAreaBounds_.h);

    // Render Selection Highlighting
    if (presenter_.hasSelection()) {
        auto sel = presenter_.getSelectionRange();
        for (int l = std::max(sel.start.line, firstVisible); l <= std::min(sel.end.line, lastVisible); ++l) {
            int lineLen = static_cast<int>(doc.getLineLength(static_cast<size_t>(l)));
            int sCol = (l == sel.start.line) ? sel.start.column : 0;
            int eCol = (l == sel.end.line) ? sel.end.column : (lineLen + 1);

            float selX = textAreaBounds_.x + 6.0f + static_cast<float>(sCol) * charWidth_ - presenter_.getScrollX();
            float selW = static_cast<float>(std::max(1, eCol - sCol)) * charWidth_;
            float selY = textAreaBounds_.y + static_cast<float>(l) * lineHeight_ - presenter_.getScrollY();

            drawRect(r, selX, selY, selW, lineHeight_,
                     theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.35f);
        }
    }

    // Render Text Lines
    for (int l = firstVisible; l <= lastVisible && l < totalLines; ++l) {
        float ly = textAreaBounds_.y + static_cast<float>(l) * lineHeight_ - presenter_.getScrollY() + 3.0f;
        float textX = textAreaBounds_.x + 6.0f - presenter_.getScrollX();
        const auto& line = doc.getLine(static_cast<size_t>(l));

        if (line.starts_with("#") || line.starts_with("--")) {
            drawMonoText(r, line, textX, ly, 10.0f, 0.38f, 0.49f, 0.55f, 1.0f);
        } else if (line.find("def ") != std::string::npos || line.find("import ") != std::string::npos ||
                   line.find("function ") != std::string::npos || line.find("return ") != std::string::npos ||
                   line.find("for ") != std::string::npos || line.find("if ") != std::string::npos) {
            drawMonoText(r, line, textX, ly, 10.0f, 1.0f, 0.85f, 0.20f, 1.0f);
        } else if (line.find("eat.") != std::string::npos || line.find("param") != std::string::npos) {
            drawMonoText(r, line, textX, ly, 10.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
        } else if (line.find('"') != std::string::npos || line.find('\'') != std::string::npos) {
            drawMonoText(r, line, textX, ly, 10.0f, 0.96f, 0.65f, 0.35f, 1.0f);
        } else {
            drawMonoText(r, line, textX, ly, 10.0f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.95f);
        }
    }

    // Render Blinking Cursor
    if (isFocused_) {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - focusGainTime_).count();
        bool showCursor = (ms % 1000) < 550;

        if (showCursor) {
            auto cur = presenter_.getCursor();
            float curX = textAreaBounds_.x + 6.0f + static_cast<float>(cur.column) * charWidth_ - presenter_.getScrollX();
            float curY = textAreaBounds_.y + 2.0f + static_cast<float>(cur.line) * lineHeight_ - presenter_.getScrollY();

            drawRect(r, curX, curY, 2.0f, lineHeight_ - 4.0f,
                     theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
        }
    }

    r.popScissor();

    // Render Gutter Line Numbers
    for (int l = firstVisible; l <= lastVisible && l < totalLines; ++l) {
        float ly = textAreaBounds_.y + static_cast<float>(l) * lineHeight_ - presenter_.getScrollY() + 3.0f;
        std::string numStr = std::to_string(l + 1);
        bool isActive = (l == presenter_.getCursor().line);

        float numX = gutterBounds_.x + gutterBounds_.w - 8.0f - static_cast<float>(numStr.length()) * 7.5f;
        if (isActive) {
            drawMonoText(r, numStr, numX, ly, 9.5f, 1.0f, 1.0f, 1.0f, 0.95f);
        } else {
            drawMonoText(r, numStr, numX, ly, 9.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.60f);
        }
    }
}

void TextEditorWidget::renderMinimap(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;
    const auto& doc = presenter_.getDocument();

    // Minimap Background & Divider
    drawRect(r, minimapBounds_.x, minimapBounds_.y, minimapBounds_.w, minimapBounds_.h, 0.08f, 0.09f, 0.12f, 0.95f);
    drawLine(r, minimapBounds_.x, minimapBounds_.y,
             minimapBounds_.x, minimapBounds_.y + minimapBounds_.h,
             0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

    size_t lineCount = doc.getLineCount();
    if (lineCount == 0) return;

    // High-performance Code Silhouette Micro-bars
    // Compact fixed line pitch (2.0px bar with 1.0px separator = 3.0px pitch) anchored from top;
    // only scales slot height down when document lines exceed minimap bounds.
    constexpr float kFixedBarH = 2.0f;
    constexpr float kFixedPitch = 3.0f;
    float microSlotH = (static_cast<float>(lineCount) * kFixedPitch > minimapBounds_.h)
        ? (minimapBounds_.h / static_cast<float>(lineCount))
        : kFixedPitch;
    float microBarH = (microSlotH >= kFixedPitch)
        ? kFixedBarH
        : std::clamp(microSlotH * 0.75f, 0.8f, kFixedBarH);

    for (size_t i = 0; i < lineCount; ++i) {
        const auto& line = doc.getLine(i);
        if (line.empty()) continue;

        float microY = minimapBounds_.y + static_cast<float>(i) * microSlotH;
        if (microY + microBarH > minimapBounds_.y + minimapBounds_.h) break;

        size_t leadingSpaces = 0;
        while (leadingSpaces < line.length() && (line[leadingSpaces] == ' ' || line[leadingSpaces] == '\t')) {
            leadingSpaces++;
        }

        float barX = minimapBounds_.x + 4.0f + static_cast<float>(std::min<size_t>(leadingSpaces, 16)) * 1.5f;
        float contentLen = static_cast<float>(line.length() - leadingSpaces);
        float barW = std::clamp(contentLen * 0.75f, 2.0f, minimapBounds_.w - (barX - minimapBounds_.x) - 4.0f);

        if (line.starts_with("#") || line.starts_with("--")) {
            drawRect(r, barX, microY, barW, microBarH, 0.38f, 0.49f, 0.55f, 0.75f);
        } else if (line.find("def ") != std::string::npos || line.find("import ") != std::string::npos ||
                   line.find("function ") != std::string::npos || line.find("return ") != std::string::npos) {
            drawRect(r, barX, microY, barW, microBarH, 1.0f, 0.85f, 0.20f, 0.80f);
        } else if (line.find("eat.") != std::string::npos || line.find("param") != std::string::npos) {
            drawRect(r, barX, microY, barW, microBarH, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.80f);
        } else {
            drawRect(r, barX, microY, barW, microBarH, 0.80f, 0.85f, 0.90f, 0.65f);
        }
    }

    // Viewport Lens Overlay / Scrubber
    if (presenter_.getMaxScrollY() > 0.0f) {
        auto [thumbY, thumbH] = presenter_.getMinimapThumbBounds(minimapBounds_.h);
        float lensX = minimapBounds_.x + 2.0f;
        float lensW = minimapBounds_.w - 4.0f;
        float lensY = minimapBounds_.y + thumbY;

        drawRoundedRect(r, lensX, lensY, lensW, thumbH, 3.0f,
                        theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.18f);
        drawRoundedRectOutline(r, lensX, lensY, lensW, thumbH, 3.0f,
                               theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.75f, 1.2f);
    }
}

bool TextEditorWidget::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    if (ctx.clipboard) clipboard_ = ctx.clipboard;
    if (ev.action == PointerAction::Down) {
        if (!bounds_.contains(ev.x, ev.y)) {
            return false;
        }

        if (ctx.focusManager) {
            ctx.focusManager->requestFocus(this);
        }

        if (minimapBounds_.contains(ev.x, ev.y)) {
            isDraggingMinimap_ = true;
            presenter_.scrubMinimap(ev.y - minimapBounds_.y, minimapBounds_.h);
            return true;
        }

        if (textAreaBounds_.contains(ev.x, ev.y) || gutterBounds_.contains(ev.x, ev.y)) {
            auto now = std::chrono::steady_clock::now();
            double elapsedMs = std::chrono::duration<double, std::milli>(now - lastClickTime_).count();
            float dist = std::hypot(ev.x - lastClickPos_.x, ev.y - lastClickPos_.y);

            auto coord = coordFromPoint(ev.x, ev.y);

            if (elapsedMs < 300.0 && dist < 6.0f) {
                presenter_.selectWordAt(coord);
            } else {
                presenter_.setCursor(coord, false);
                isDraggingText_ = true;
            }

            lastClickTime_ = now;
            lastClickPos_ = Point2D(ev.x, ev.y);
            return true;
        }
    } else if (ev.action == PointerAction::Move) {
        if (isDraggingMinimap_) {
            presenter_.scrubMinimap(ev.y - minimapBounds_.y, minimapBounds_.h);
            return true;
        }
        if (isDraggingText_) {
            auto coord = coordFromPoint(ev.x, ev.y);
            presenter_.setCursor(coord, true);
            presenter_.ensureCursorVisible(gutterW_);
            return true;
        }
    } else if (ev.action == PointerAction::Up) {
        if (isDraggingMinimap_ || isDraggingText_) {
            isDraggingMinimap_ = false;
            isDraggingText_ = false;
            return true;
        }
    } else if (ev.action == PointerAction::Scroll) {
        if (bounds_.contains(ev.x, ev.y)) {
            float newScrollY = presenter_.getScrollY() - ev.scrollY * lineHeight_ * 3.0f;
            presenter_.setScrollY(newScrollY);
            float newScrollX = presenter_.getScrollX() - ev.scrollX * charWidth_ * 3.0f;
            presenter_.setScrollX(newScrollX);
            return true;
        }
    }

    return false;
}

bool TextEditorWidget::handleKey(int key, int scancode, int action, int mods) {
    ViewContext dummyCtx;
    dummyCtx.clipboard = clipboard_;
    return handleKey(key, scancode, action, mods, dummyCtx);
}

bool TextEditorWidget::handleKey(int key, int /*scancode*/, int action, int mods, const ViewContext& ctx) {
    if (!isFocused_) return false;
    if (action != 1 && action != 2) return false; // GLFW_PRESS (1) or GLFW_REPEAT (2)

    bool ctrl = (mods & 0x0002) != 0;
    bool shift = (mods & 0x0001) != 0;
    IClipboard* cb = ctx.clipboard ? ctx.clipboard : clipboard_;

    focusGainTime_ = std::chrono::steady_clock::now();

    // 1. Compile Trigger: Ctrl+Enter or F5
    if ((ctrl && key == 257) || key == 294) { // Enter=257, F5=294
        if (onCompileTriggered) {
            onCompileTriggered();
        }
        return true;
    }

    // 2. Select All: Ctrl+A
    if (ctrl && key == 65) { // 'A'
        presenter_.selectAll();
        return true;
    }

    // 3. Copy: Ctrl+C
    if (ctrl && key == 67) { // 'C'
        if (cb && presenter_.hasSelection()) {
            cb->setText(presenter_.getSelectedText());
        }
        return true;
    }

    // 4. Cut: Ctrl+X
    if (ctrl && key == 88) { // 'X'
        if (!readOnly_ && cb && presenter_.hasSelection()) {
            cb->setText(presenter_.getSelectedText());
            presenter_.deleteSelection();
            presenter_.ensureCursorVisible(gutterW_);
            if (onTextChanged) onTextChanged(getText());
        }
        return true;
    }

    // 5. Paste: Ctrl+V
    if (ctrl && key == 86) { // 'V'
        if (!readOnly_ && cb) {
            std::string clip = cb->getText();
            if (!clip.empty()) {
                presenter_.insertText(clip);
                presenter_.ensureCursorVisible(gutterW_);
                if (onTextChanged) onTextChanged(getText());
            }
        }
        return true;
    }

    // 6. Undo: Ctrl+Z / Redo: Ctrl+Y or Ctrl+Shift+Z
    if (ctrl && key == 90 && !shift) { // Ctrl+Z
        if (!readOnly_) {
            presenter_.getDocument().undo();
            presenter_.setCursor(presenter_.getCursor(), false);
            presenter_.ensureCursorVisible(gutterW_);
            if (onTextChanged) onTextChanged(getText());
        }
        return true;
    }
    if ((ctrl && key == 89) || (ctrl && shift && key == 90)) { // Ctrl+Y or Ctrl+Shift+Z
        if (!readOnly_) {
            presenter_.getDocument().redo();
            presenter_.setCursor(presenter_.getCursor(), false);
            presenter_.ensureCursorVisible(gutterW_);
            if (onTextChanged) onTextChanged(getText());
        }
        return true;
    }

    // 7. Comment toggle: Ctrl+/
    if (ctrl && key == 47) { // '/'
        if (!readOnly_) {
            presenter_.toggleLineComment("#");
            presenter_.ensureCursorVisible(gutterW_);
            if (onTextChanged) onTextChanged(getText());
        }
        return true;
    }

    // 8. Line / Selection Duplication: Ctrl+D
    if (ctrl && key == 68) { // 'D'
        if (!readOnly_) {
            presenter_.duplicateCurrentLineOrSelection();
            presenter_.ensureCursorVisible(gutterW_);
            if (onTextChanged) onTextChanged(getText());
        }
        return true;
    }

    // 9. Indent: Tab / Unindent: Shift+Tab
    if (key == 258) { // Tab
        if (!readOnly_) {
            if (shift) {
                presenter_.unindentSelection(4);
            } else if (presenter_.hasSelection()) {
                presenter_.indentSelection(4);
            } else {
                presenter_.insertText("    ");
            }
            presenter_.ensureCursorVisible(gutterW_);
            if (onTextChanged) onTextChanged(getText());
        }
        return true;
    }

    // 10. Enter: New line with auto-indent
    if (key == 257) { // Enter
        if (!readOnly_) {
            presenter_.insertNewLine();
            presenter_.ensureCursorVisible(gutterW_);
            if (onTextChanged) onTextChanged(getText());
        }
        return true;
    }

    // 11. Backspace & Delete
    if (key == 259) { // Backspace
        if (!readOnly_) {
            presenter_.backspace(ctrl);
            presenter_.ensureCursorVisible(gutterW_);
            if (onTextChanged) onTextChanged(getText());
        }
        return true;
    }
    if (key == 261) { // Delete
        if (!readOnly_) {
            presenter_.forwardDelete(ctrl);
            presenter_.ensureCursorVisible(gutterW_);
            if (onTextChanged) onTextChanged(getText());
        }
        return true;
    }

    // 12. Cursor Navigation
    if (key == 263) { // Left
        presenter_.moveLeft(shift, ctrl);
        presenter_.ensureCursorVisible(gutterW_);
        return true;
    }
    if (key == 262) { // Right
        presenter_.moveRight(shift, ctrl);
        presenter_.ensureCursorVisible(gutterW_);
        return true;
    }
    if (key == 265) { // Up
        presenter_.moveUp(shift);
        presenter_.ensureCursorVisible(gutterW_);
        return true;
    }
    if (key == 264) { // Down
        presenter_.moveDown(shift);
        presenter_.ensureCursorVisible(gutterW_);
        return true;
    }
    if (key == 268) { // Home
        if (ctrl) {
            presenter_.setCursor({0, 0}, shift);
        } else {
            presenter_.moveHome(shift);
        }
        presenter_.ensureCursorVisible(gutterW_);
        return true;
    }
    if (key == 269) { // End
        if (ctrl) {
            int lastLine = static_cast<int>(presenter_.getDocument().getLineCount()) - 1;
            int lastCol = static_cast<int>(presenter_.getDocument().getLineLength(static_cast<size_t>(lastLine)));
            presenter_.setCursor({lastLine, lastCol}, shift);
        } else {
            presenter_.moveEnd(shift);
        }
        presenter_.ensureCursorVisible(gutterW_);
        return true;
    }
    if (key == 266) { // PageUp
        for (int i = 0; i < 10; ++i) presenter_.moveUp(shift);
        presenter_.ensureCursorVisible(gutterW_);
        return true;
    }
    if (key == 267) { // PageDown
        for (int i = 0; i < 10; ++i) presenter_.moveDown(shift);
        presenter_.ensureCursorVisible(gutterW_);
        return true;
    }

    return true; // Consume keys when focused to prevent host DAW conflicts
}

bool TextEditorWidget::handleChar(char32_t codepoint) {
    ViewContext dummyCtx;
    return handleChar(codepoint, dummyCtx);
}

bool TextEditorWidget::handleChar(char32_t codepoint, const ViewContext& /*ctx*/) {
    if (!isFocused_ || readOnly_) return false;

    focusGainTime_ = std::chrono::steady_clock::now();
    presenter_.insertChar(codepoint);
    presenter_.ensureCursorVisible(gutterW_);

    if (onTextChanged) {
        onTextChanged(getText());
    }
    return true;
}

} // namespace eatsbits::ui
