#include "eatsbits/ui/widgets/text_editor_widget.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace eatsbits::ui {

namespace {

enum class ToolbarAction : uint8_t {
    Indent,
    Outdent,
    Parens,
    Brackets,
    Braces,
    DoubleQuote,
    SingleQuote,
    Colon,
    Equals,
    Plus,
    Minus,
    Asterisk,
    Slash,
    Dot,
    Comma,
    Underscore,
    Comment,
    Arrow,
    Params,
    Return,
    NudgeLeft,
    NudgeRight,
    NudgeUp,
    NudgeDown,
    Undo,
    Redo,
    RunCompile
};

struct ToolbarItem {
    ToolbarAction action;
    std::string label;
    float width;
};

const std::vector<ToolbarItem>& getToolbarItems() {
    static const std::vector<ToolbarItem> sItems = {
        {ToolbarAction::Indent, "⇥", 34.0f},
        {ToolbarAction::Outdent, "⇤", 34.0f},
        {ToolbarAction::Parens, "( )", 38.0f},
        {ToolbarAction::Brackets, "[ ]", 38.0f},
        {ToolbarAction::Braces, "{ }", 38.0f},
        {ToolbarAction::DoubleQuote, "\"", 28.0f},
        {ToolbarAction::SingleQuote, "'", 26.0f},
        {ToolbarAction::Colon, ":", 26.0f},
        {ToolbarAction::Equals, "=", 26.0f},
        {ToolbarAction::Plus, "+", 26.0f},
        {ToolbarAction::Minus, "-", 26.0f},
        {ToolbarAction::Asterisk, "*", 26.0f},
        {ToolbarAction::Slash, "/", 26.0f},
        {ToolbarAction::Dot, ".", 24.0f},
        {ToolbarAction::Comma, ",", 24.0f},
        {ToolbarAction::Underscore, "_", 26.0f},
        {ToolbarAction::Comment, "#", 26.0f},
        {ToolbarAction::Arrow, "->", 32.0f},
        {ToolbarAction::Params, "param", 44.0f},
        {ToolbarAction::Return, "ret", 32.0f},
        {ToolbarAction::NudgeLeft, "◀", 30.0f},
        {ToolbarAction::NudgeRight, "▶", 30.0f},
        {ToolbarAction::NudgeUp, "▲", 30.0f},
        {ToolbarAction::NudgeDown, "▼", 30.0f},
        {ToolbarAction::Undo, "↶", 30.0f},
        {ToolbarAction::Redo, "↷", 30.0f},
        {ToolbarAction::RunCompile, "▶ RUN", 58.0f}
    };
    return sItems;
}

} // namespace

TextEditorWidget::TextEditorWidget() {
    focusGainTime_ = std::chrono::steady_clock::now();
    cursorBlinkResetTime_ = focusGainTime_;
    charWidth_ = getMonoCharAdvance(10.0f);
}

bool TextEditorWidget::onFocusGained() {
    isFocused_ = true;
    focusGainTime_ = std::chrono::steady_clock::now();
    resetCursorBlink();
#ifdef __EMSCRIPTEN__
    EM_ASM({
        if (window.eatsbitsShowSoftKeyboard) {
            window.eatsbitsShowSoftKeyboard();
        }
    });
#endif
    return true;
}

void TextEditorWidget::onFocusLost() {
    isFocused_ = false;
    isDraggingText_ = false;
    isDraggingMinimap_ = false;
#ifdef __EMSCRIPTEN__
    EM_ASM({
        if (window.eatsbitsHideSoftKeyboard) {
            window.eatsbitsHideSoftKeyboard();
        }
    });
#endif
}

void TextEditorWidget::layout(const Rect2D& bounds, const ViewContext& ctx) {
    if (ctx.clipboard) clipboard_ = ctx.clipboard;
    bounds_ = bounds;

    bool effectiveToolbar = showToolbar_ || (autoToolbar_ && (ctx.isMobile || bounds_.w < 650.0f));
    float toolbarH = effectiveToolbar ? (ctx.isMobile ? 36.0f : 32.0f) : 0.0f;
    float contentH = std::max(0.0f, bounds_.h - toolbarH);

    if (effectiveToolbar) {
        toolbarBounds_ = Rect2D(bounds_.x, bounds_.y + contentH, bounds_.w, toolbarH);
    } else {
        toolbarBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
    }

    gutterBounds_ = Rect2D(bounds_.x, bounds_.y, gutterW_, contentH);
    minimapBounds_ = Rect2D(bounds_.x + bounds_.w - minimapW_, bounds_.y, minimapW_, contentH);
    float textW = std::max(0.0f, bounds_.w - gutterW_ - minimapW_);
    textAreaBounds_ = Rect2D(bounds_.x + gutterW_, bounds_.y, textW, contentH);

    charWidth_ = getMonoCharAdvance(10.0f);
    presenter_.setViewport(textAreaBounds_.w, textAreaBounds_.h, lineHeight_, charWidth_);
}

void TextEditorWidget::setText(std::string_view text) {
    presenter_.setText(text);
    resetCursorBlink();
    lastCursor_ = presenter_.getCursor();
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

    // Reset cursor blink if cursor changed position
    auto curCursor = presenter_.getCursor();
    if (curCursor.line != lastCursor_.line || curCursor.column != lastCursor_.column) {
        resetCursorBlink();
        lastCursor_ = curCursor;
    }

    // 1. Overall Editor Background
    drawRect(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h, 0.06f, 0.07f, 0.09f, 1.0f);

    // 2. Active Line Highlight across text area and gutter
    int curLine = presenter_.getCursor().line;
    float activeLineY = textAreaBounds_.y + static_cast<float>(curLine) * lineHeight_ - presenter_.getScrollY();
    if (activeLineY >= bounds_.y - lineHeight_ && activeLineY <= bounds_.y + bounds_.h) {
        r.pushScissor(bounds_.x, bounds_.y, bounds_.w - minimapW_, bounds_.h);
        drawRect(r, bounds_.x, activeLineY, bounds_.w - minimapW_, lineHeight_, 1.0f, 1.0f, 1.0f, 0.045f);
        r.popScissor();
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

    // 6. Mobile Code Accessory Toolbar
    if (toolbarBounds_.h > 0.0f) {
        renderAccessoryToolbar(ctx);
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

    static auto packColorU32 = [](float red, float green, float blue, float alpha = 1.0f) noexcept -> uint32_t {
        uint8_t cr = static_cast<uint8_t>(std::clamp(red, 0.0f, 1.0f) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::clamp(green, 0.0f, 1.0f) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::clamp(blue, 0.0f, 1.0f) * 255.0f);
        uint8_t ca = static_cast<uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
        return (static_cast<uint32_t>(ca) << 24) |
               (static_cast<uint32_t>(cb) << 16) |
               (static_cast<uint32_t>(cg) << 8) |
                static_cast<uint32_t>(cr);
    };

    const float charH = std::min(lineHeight_ - 2.0f, std::round(charWidth_ * 2.0f));
    const float textOffsetY = (lineHeight_ - charH) * 0.5f;

    // Render Selection Highlighting
    if (presenter_.hasSelection()) {
        auto sel = presenter_.getSelectionRange();
        for (int l = std::max(sel.start.line, firstVisible); l <= std::min(sel.end.line, lastVisible); ++l) {
            int lineLen = static_cast<int>(doc.getLineLength(static_cast<size_t>(l)));
            int sCol = (l == sel.start.line) ? sel.start.column : 0;
            int eCol = (l == sel.end.line) ? sel.end.column : (lineLen + 1);

            float selX = textAreaBounds_.x + 6.0f + static_cast<float>(sCol) * charWidth_ - presenter_.getScrollX();
            float selW = static_cast<float>(std::max(1, eCol - sCol)) * charWidth_;
            float selY = textAreaBounds_.y + static_cast<float>(l) * lineHeight_ - presenter_.getScrollY() + textOffsetY;

            drawRect(r, selX, selY, selW, charH,
                     theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.35f);
        }
    }

    // Render Text Lines via Monospace Token Lexer (zero allocation, theme-backed syntax highlighting)
    auto packColor = [](const Color& c, float alpha = 1.0f) noexcept -> uint32_t {
        uint8_t cr = static_cast<uint8_t>(std::clamp(c.r, 0.0f, 1.0f) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::clamp(c.g, 0.0f, 1.0f) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::clamp(c.b, 0.0f, 1.0f) * 255.0f);
        uint8_t ca = static_cast<uint8_t>(std::clamp(alpha * c.a, 0.0f, 1.0f) * 255.0f);
        return (static_cast<uint32_t>(ca) << 24) |
               (static_cast<uint32_t>(cb) << 16) |
               (static_cast<uint32_t>(cg) << 8) |
                static_cast<uint32_t>(cr);
    };

    uint32_t colKeyword    = packColor(theme.syntaxKeyword);
    uint32_t colString     = packColor(theme.syntaxString);
    uint32_t colNumber     = packColor(theme.syntaxNumber);
    uint32_t colComment    = packColor(theme.syntaxComment, 0.90f);
    uint32_t colFunction   = packColor(theme.syntaxFunction);
    uint32_t colIdentifier = packColor(theme.syntaxIdentifier, 0.95f);
    uint32_t colOperator   = packColor(theme.syntaxOperator, 0.90f);
    uint32_t colType       = packColor(theme.syntaxType);

    auto isEatIdentStart = [](char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    };
    auto isEatIdent = [isEatIdentStart](char c) noexcept {
        return isEatIdentStart(c) || (c >= '0' && c <= '9');
    };
    auto isEatDigit = [](char c) noexcept {
        return c >= '0' && c <= '9';
    };
    auto isEatKeyword = [](std::string_view w) noexcept {
        static constexpr std::string_view kKeywords[] = {
            "def", "fn", "function", "return", "if", "else", "elif",
            "for", "while", "in", "import", "from", "as", "and", "or",
            "not", "true", "false", "nil", "null", "none", "class",
            "struct", "var", "let", "const", "end", "break", "continue",
            "yield", "async", "await", "self", "this", "do"
        };
        for (const auto& kw : kKeywords) {
            if (w == kw) return true;
        }
        return false;
    };
    auto isEatBuiltin = [](std::string_view w) noexcept {
        static constexpr std::string_view kBuiltins[] = {
            "eat", "track", "clip", "midi", "audio", "param", "math",
            "time", "int", "float", "string", "bool", "voice", "sample",
            "synth", "fx", "gain", "pan", "freq", "cutoff", "resonance",
            "print", "log", "note", "velocity", "gate", "pitch"
        };
        for (const auto& b : kBuiltins) {
            if (w == b) return true;
        }
        return false;
    };

    for (int l = firstVisible; l <= lastVisible && l < totalLines; ++l) {
        float ly = textAreaBounds_.y + static_cast<float>(l) * lineHeight_ - presenter_.getScrollY() + textOffsetY;
        float textX = textAreaBounds_.x + 6.0f - presenter_.getScrollX();
        const auto& line = doc.getLine(static_cast<size_t>(l));
        if (line.empty()) continue;

        std::string_view sv(line);
        size_t i = 0;
        const size_t len = sv.length();

        while (i < len) {
            // Skip whitespace
            if (sv[i] == ' ' || sv[i] == '\t' || sv[i] == '\r') {
                i++;
                continue;
            }

            size_t tokenStart = i;

            // 1. Comments: # or // or --
            if (sv[i] == '#' || (i + 1 < len && sv[i] == '/' && sv[i + 1] == '/') ||
                (i + 1 < len && sv[i] == '-' && sv[i + 1] == '-')) {
                std::string_view tok = sv.substr(tokenStart);
                float tx = textX + static_cast<float>(tokenStart) * charWidth_;
                r.drawMonospaceText(tx, ly, charWidth_, charH, tok, colComment);
                break;
            }

            // 2. String literals: "..." or '...'
            if (sv[i] == '"' || sv[i] == '\'') {
                char quote = sv[i++];
                while (i < len && sv[i] != quote) {
                    if (sv[i] == '\\' && i + 1 < len) {
                        i += 2;
                    } else {
                        i++;
                    }
                }
                if (i < len && sv[i] == quote) i++;
                std::string_view tok = sv.substr(tokenStart, i - tokenStart);
                float tx = textX + static_cast<float>(tokenStart) * charWidth_;
                r.drawMonospaceText(tx, ly, charWidth_, charH, tok, colString);
                continue;
            }

            // 3. Numbers: hex (0x...) or decimal float/int
            if (isEatDigit(sv[i]) || (sv[i] == '.' && i + 1 < len && isEatDigit(sv[i + 1]))) {
                if (sv[i] == '0' && i + 1 < len && (sv[i + 1] == 'x' || sv[i + 1] == 'X')) {
                    i += 2;
                    while (i < len && (isEatDigit(sv[i]) || (sv[i] >= 'a' && sv[i] <= 'f') || (sv[i] >= 'A' && sv[i] <= 'F'))) {
                        i++;
                    }
                } else {
                    bool hasDot = false;
                    while (i < len && (isEatDigit(sv[i]) || (sv[i] == '.' && !hasDot))) {
                        if (sv[i] == '.') hasDot = true;
                        i++;
                    }
                    if (i < len && (sv[i] == 'f' || sv[i] == 'F')) i++;
                }
                std::string_view tok = sv.substr(tokenStart, i - tokenStart);
                float tx = textX + static_cast<float>(tokenStart) * charWidth_;
                r.drawMonospaceText(tx, ly, charWidth_, charH, tok, colNumber);
                continue;
            }

            // 4. Identifiers, Keywords, Builtins, Functions
            if (isEatIdentStart(sv[i])) {
                while (i < len && isEatIdent(sv[i])) {
                    i++;
                }
                std::string_view tok = sv.substr(tokenStart, i - tokenStart);
                uint32_t col = colIdentifier;

                if (isEatKeyword(tok)) {
                    col = colKeyword;
                } else if (isEatBuiltin(tok)) {
                    col = colType;
                } else {
                    // Lookahead for '(' (function call or def)
                    size_t k = i;
                    while (k < len && (sv[k] == ' ' || sv[k] == '\t')) k++;
                    if (k < len && sv[k] == '(') {
                        col = colFunction;
                    }
                }

                float tx = textX + static_cast<float>(tokenStart) * charWidth_;
                r.drawMonospaceText(tx, ly, charWidth_, charH, tok, col);
                continue;
            }

            // 5. Operators, Punctuation, Delimiters
            size_t opLen = 1;
            if (i + 1 < len) {
                char c1 = sv[i];
                char c2 = sv[i + 1];
                if ((c1 == '=' && c2 == '=') || (c1 == '!' && c2 == '=') ||
                    (c1 == '<' && c2 == '=') || (c1 == '>' && c2 == '=') ||
                    (c1 == '+' && c2 == '=') || (c1 == '-' && c2 == '=') ||
                    (c1 == '*' && c2 == '=') || (c1 == '/' && c2 == '=') ||
                    (c1 == '-' && c2 == '>')) {
                    opLen = 2;
                }
            }
            std::string_view tok = sv.substr(tokenStart, opLen);
            i += opLen;
            float tx = textX + static_cast<float>(tokenStart) * charWidth_;
            r.drawMonospaceText(tx, ly, charWidth_, charH, tok, colOperator);
        }
    }

    // Render Blinking Cursor
    if (isFocused_) {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - cursorBlinkResetTime_).count();
        bool showCursor = (ms % 1000) < 550;

        if (showCursor) {
            auto cur = presenter_.getCursor();
            float curX = textAreaBounds_.x + 6.0f + static_cast<float>(cur.column) * charWidth_ - presenter_.getScrollX();
            float curY = textAreaBounds_.y + static_cast<float>(cur.line) * lineHeight_ - presenter_.getScrollY() + textOffsetY;

            drawRect(r, curX, curY, 2.0f, charH,
                     theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
        }
    }

    r.popScissor();

    // Render Gutter Line Numbers (scissored strictly within gutterBounds_)
    r.pushScissor(gutterBounds_.x, gutterBounds_.y, gutterBounds_.w, gutterBounds_.h);
    for (int l = firstVisible; l <= lastVisible && l < totalLines; ++l) {
        float ly = textAreaBounds_.y + static_cast<float>(l) * lineHeight_ - presenter_.getScrollY() + textOffsetY;
        if (ly + charH < gutterBounds_.y || ly > gutterBounds_.y + gutterBounds_.h) {
            continue;
        }
        std::string numStr = std::to_string(l + 1);
        bool isActive = (l == presenter_.getCursor().line);

        float numX = gutterBounds_.x + gutterBounds_.w - 8.0f - static_cast<float>(numStr.length()) * charWidth_;
        uint32_t numCol = isActive ? packColorU32(1.0f, 1.0f, 1.0f, 0.95f)
                                   : packColorU32(theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.60f);
        r.drawMonospaceText(numX, ly, charWidth_, charH, numStr, numCol);
    }
    r.popScissor();
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

        if (line.starts_with("#") || line.starts_with("--") || line.starts_with("//")) {
            drawRect(r, barX, microY, barW, microBarH, theme.syntaxComment.r, theme.syntaxComment.g, theme.syntaxComment.b, 0.75f);
        } else if (line.find("def ") != std::string::npos || line.find("import ") != std::string::npos ||
                   line.find("function ") != std::string::npos || line.find("return ") != std::string::npos) {
            drawRect(r, barX, microY, barW, microBarH, theme.syntaxKeyword.r, theme.syntaxKeyword.g, theme.syntaxKeyword.b, 0.80f);
        } else if (line.find("eat.") != std::string::npos || line.find("param") != std::string::npos) {
            drawRect(r, barX, microY, barW, microBarH, theme.syntaxFunction.r, theme.syntaxFunction.g, theme.syntaxFunction.b, 0.80f);
        } else {
            drawRect(r, barX, microY, barW, microBarH, theme.syntaxIdentifier.r, theme.syntaxIdentifier.g, theme.syntaxIdentifier.b, 0.65f);
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

    // Check Mobile Code Accessory Toolbar first
    if (toolbarBounds_.h > 0.0f) {
        if (handleToolbarPointer(ev, ctx)) {
            return true;
        }
    }

    if (ev.action == PointerAction::Down) {
        if (!bounds_.contains(ev.x, ev.y)) {
            return false;
        }

        if (ctx.focusManager) {
            ctx.focusManager->requestFocus(this);
        }
#ifdef __EMSCRIPTEN__
        EM_ASM({
            if (window.eatsbitsShowSoftKeyboard) {
                window.eatsbitsShowSoftKeyboard();
            }
        });
#endif

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

            resetCursorBlink();
            lastCursor_ = presenter_.getCursor();

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
            resetCursorBlink();
            lastCursor_ = presenter_.getCursor();
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
            float sy = ev.scrollY;
            float sx = ev.scrollX;
            if (ev.mods.shift && sx == 0.0f) {
                sx = sy;
                sy = 0.0f;
            }
            float newScrollY = presenter_.getScrollY() - sy * lineHeight_ * 3.0f;
            presenter_.setScrollY(newScrollY);
            float newScrollX = presenter_.getScrollX() - sx * charWidth_ * 3.0f;
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
    resetCursorBlink();

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
    resetCursorBlink();
    presenter_.insertChar(codepoint);
    presenter_.ensureCursorVisible(gutterW_);
    lastCursor_ = presenter_.getCursor();

    if (onTextChanged) {
        onTextChanged(getText());
    }
    return true;
}

void TextEditorWidget::renderAccessoryToolbar(const ViewContext& ctx) {
    if (!ctx.renderer || !ctx.theme) return;
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;
    const auto& items = getToolbarItems();

    // 1. Scissor to toolbar bounds
    r.pushScissor(toolbarBounds_.x, toolbarBounds_.y, toolbarBounds_.w, toolbarBounds_.h);

    // 2. Toolbar Background and Top Divider
    drawRect(r, toolbarBounds_.x, toolbarBounds_.y, toolbarBounds_.w, toolbarBounds_.h, 0.08f, 0.09f, 0.12f, 0.98f);
    drawLine(r, toolbarBounds_.x, toolbarBounds_.y,
             toolbarBounds_.x + toolbarBounds_.w, toolbarBounds_.y,
             0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

    float btnH = std::max(22.0f, toolbarBounds_.h - 8.0f);
    float btnY = toolbarBounds_.y + (toolbarBounds_.h - btnH) * 0.5f;
    float curX = 6.0f;

    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        float btnX = toolbarBounds_.x + curX - toolbarScrollX_;

        if (btnX + item.width >= toolbarBounds_.x && btnX <= toolbarBounds_.x + toolbarBounds_.w) {
            bool isPressed = (pressedToolbarBtn_ == static_cast<int>(i));

            Color bgCol, bdrCol, textCol;
            if (item.action == ToolbarAction::RunCompile) {
                if (isPressed) {
                    bgCol = Color(theme.primaryAccent.r * 0.5f, theme.primaryAccent.g * 0.5f, theme.primaryAccent.b * 0.5f, 0.95f);
                } else {
                    bgCol = Color(theme.primaryAccent.r * 0.25f, theme.primaryAccent.g * 0.25f, theme.primaryAccent.b * 0.25f, 0.85f);
                }
                bdrCol = theme.primaryAccent;
                textCol = Color(1.0f, 1.0f, 1.0f, 1.0f);
            } else {
                if (isPressed) {
                    bgCol = Color(theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.35f);
                    bdrCol = theme.primaryAccent;
                    textCol = Color(1.0f, 1.0f, 1.0f, 1.0f);
                } else {
                    bgCol = Color(0.13f, 0.14f, 0.19f, 0.90f);
                    bdrCol = Color(0.22f, 0.24f, 0.30f, 0.85f);
                    textCol = theme.textPrimary;
                }
            }

            drawButton(r, Rect2D(btnX, btnY, item.width, btnH), item.label, bgCol, bdrCol, textCol, 10.5f, 4.0f, 1.0f);
        }

        curX += item.width + 4.0f;
    }

    r.popScissor();
}

bool TextEditorWidget::handleToolbarPointer(const PointerEvent& ev, const ViewContext& ctx) {
    if (toolbarBounds_.h <= 0.0f) return false;

    const auto& items = getToolbarItems();
    float totalW = 6.0f;
    for (const auto& item : items) {
        totalW += item.width + 4.0f;
    }
    float maxScroll = std::max(0.0f, totalW + 8.0f - toolbarBounds_.w);

    if (ev.action == PointerAction::Scroll) {
        if (toolbarBounds_.contains(ev.x, ev.y)) {
            toolbarScrollX_ = std::clamp(toolbarScrollX_ - ev.scrollX * 30.0f - ev.scrollY * 30.0f, 0.0f, maxScroll);
            return true;
        }
        return false;
    }

    if (ev.action == PointerAction::Down) {
        if (!toolbarBounds_.contains(ev.x, ev.y)) {
            return false;
        }

        if (ctx.focusManager) {
            ctx.focusManager->requestFocus(this);
        }

        lastToolbarDragX_ = ev.x;
        float btnH = std::max(22.0f, toolbarBounds_.h - 8.0f);
        float btnY = toolbarBounds_.y + (toolbarBounds_.h - btnH) * 0.5f;

        float curX = 6.0f;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            float btnX = toolbarBounds_.x + curX - toolbarScrollX_;
            Rect2D btnRect(btnX, btnY, item.width, btnH);

            if (btnRect.contains(ev.x, ev.y)) {
                pressedToolbarBtn_ = static_cast<int>(i);

                // Execute action
                switch (item.action) {
                    case ToolbarAction::Indent:
                        if (presenter_.hasSelection()) {
                            presenter_.indentSelection(4);
                        } else {
                            presenter_.insertText("    ");
                        }
                        break;
                    case ToolbarAction::Outdent:
                        presenter_.unindentSelection(4);
                        break;
                    case ToolbarAction::Parens:
                        if (presenter_.hasSelection()) {
                            presenter_.insertText("(" + presenter_.getSelectedText() + ")");
                        } else {
                            presenter_.insertText("()");
                            auto cur = presenter_.getCursor();
                            if (cur.column > 0) cur.column--;
                            presenter_.setCursor(cur, false);
                        }
                        break;
                    case ToolbarAction::Brackets:
                        if (presenter_.hasSelection()) {
                            presenter_.insertText("[" + presenter_.getSelectedText() + "]");
                        } else {
                            presenter_.insertText("[]");
                            auto cur = presenter_.getCursor();
                            if (cur.column > 0) cur.column--;
                            presenter_.setCursor(cur, false);
                        }
                        break;
                    case ToolbarAction::Braces:
                        if (presenter_.hasSelection()) {
                            presenter_.insertText("{" + presenter_.getSelectedText() + "}");
                        } else {
                            presenter_.insertText("{}");
                            auto cur = presenter_.getCursor();
                            if (cur.column > 0) cur.column--;
                            presenter_.setCursor(cur, false);
                        }
                        break;
                    case ToolbarAction::DoubleQuote:
                        if (presenter_.hasSelection()) {
                            presenter_.insertText("\"" + presenter_.getSelectedText() + "\"");
                        } else {
                            presenter_.insertText("\"\"");
                            auto cur = presenter_.getCursor();
                            if (cur.column > 0) cur.column--;
                            presenter_.setCursor(cur, false);
                        }
                        break;
                    case ToolbarAction::SingleQuote:
                        if (presenter_.hasSelection()) {
                            presenter_.insertText("'" + presenter_.getSelectedText() + "'");
                        } else {
                            presenter_.insertText("''");
                            auto cur = presenter_.getCursor();
                            if (cur.column > 0) cur.column--;
                            presenter_.setCursor(cur, false);
                        }
                        break;
                    case ToolbarAction::Colon:
                        presenter_.insertText(":");
                        break;
                    case ToolbarAction::Equals:
                        presenter_.insertText("=");
                        break;
                    case ToolbarAction::Plus:
                        presenter_.insertText("+");
                        break;
                    case ToolbarAction::Minus:
                        presenter_.insertText("-");
                        break;
                    case ToolbarAction::Asterisk:
                        presenter_.insertText("*");
                        break;
                    case ToolbarAction::Slash:
                        presenter_.insertText("/");
                        break;
                    case ToolbarAction::Dot:
                        presenter_.insertText(".");
                        break;
                    case ToolbarAction::Comma:
                        presenter_.insertText(",");
                        break;
                    case ToolbarAction::Underscore:
                        presenter_.insertText("_");
                        break;
                    case ToolbarAction::Comment:
                        presenter_.toggleLineComment("#");
                        break;
                    case ToolbarAction::Arrow:
                        presenter_.insertText("->");
                        break;
                    case ToolbarAction::Params:
                        presenter_.insertText("params[");
                        break;
                    case ToolbarAction::Return:
                        presenter_.insertText("return ");
                        break;
                    case ToolbarAction::NudgeLeft:
                        presenter_.moveLeft(false);
                        break;
                    case ToolbarAction::NudgeRight:
                        presenter_.moveRight(false);
                        break;
                    case ToolbarAction::NudgeUp:
                        presenter_.moveUp(false);
                        break;
                    case ToolbarAction::NudgeDown:
                        presenter_.moveDown(false);
                        break;
                    case ToolbarAction::Undo:
                        presenter_.getDocument().undo();
                        presenter_.setCursor(presenter_.getCursor(), false);
                        break;
                    case ToolbarAction::Redo:
                        presenter_.getDocument().redo();
                        presenter_.setCursor(presenter_.getCursor(), false);
                        break;
                    case ToolbarAction::RunCompile:
                        if (onCompileTriggered) {
                            onCompileTriggered();
                        }
                        break;
                }

                presenter_.ensureCursorVisible(gutterW_);
                resetCursorBlink();
                lastCursor_ = presenter_.getCursor();
                if (onTextChanged) onTextChanged(getText());
                return true;
            }

            curX += item.width + 4.0f;
        }

        return true; // Clicked on toolbar background
    }

    if (ev.action == PointerAction::Move) {
        if (pressedToolbarBtn_ >= 0) {
            float deltaX = ev.x - lastToolbarDragX_;
            if (std::abs(deltaX) > 4.0f) {
                toolbarScrollX_ = std::clamp(toolbarScrollX_ - deltaX, 0.0f, maxScroll);
                lastToolbarDragX_ = ev.x;
            }
            return true;
        }
    }

    if (ev.action == PointerAction::Up) {
        if (pressedToolbarBtn_ >= 0) {
            pressedToolbarBtn_ = -1;
            return true;
        }
    }

    return false;
}

} // namespace eatsbits::ui
