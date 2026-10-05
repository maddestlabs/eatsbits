#pragma once

#include "eatsbits/core/text_document.hpp"
#include <string>
#include <string_view>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cctype>

namespace eatsbits::presenter {

/**
 * @brief Headless presenter for single-line text fields with auto-scrolling containment,
 * word boundaries, selection, and undo/redo.
 */
class SingleLineTextPresenter {
public:
    SingleLineTextPresenter() = default;
    explicit SingleLineTextPresenter(std::string initialText, bool selectAllOnSet = true) {
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
        scrollX_ = 0.0f;
        undoStack_.clear();
        redoStack_.clear();
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

    void selectWordAt(int pos) noexcept {
        if (text_.empty()) return;
        int p = std::clamp(pos, 0, static_cast<int>(text_.length()) - 1);
        bool isAlpha = std::isalnum(static_cast<unsigned char>(text_[static_cast<size_t>(p)])) || text_[static_cast<size_t>(p)] == '_';

        int s = p;
        while (s > 0) {
            char prev = text_[static_cast<size_t>(s - 1)];
            bool prevAlpha = std::isalnum(static_cast<unsigned char>(prev)) || prev == '_';
            if (prevAlpha != isAlpha) break;
            s--;
        }

        int e = p;
        while (e < static_cast<int>(text_.length())) {
            char ch = text_[static_cast<size_t>(e)];
            bool chAlpha = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
            if (chAlpha != isAlpha) break;
            e++;
        }

        anchor_ = s;
        cursor_ = e;
    }

    bool deleteSelection() {
        if (!hasSelection()) return false;
        pushUndo();
        int s = getSelectionStart();
        int e = getSelectionEnd();
        text_.erase(static_cast<size_t>(s), static_cast<size_t>(e - s));
        cursor_ = s;
        anchor_ = s;
        return true;
    }

    void insertChar(char32_t codepoint) {
        if (codepoint < 32 || codepoint == 127) return;
        if (charFilter_ && !charFilter_(codepoint)) return;

        pushUndo();
        deleteSelection();

        // UTF-8 conversion
        std::string utf8;
        if (codepoint < 0x80) {
            utf8.push_back(static_cast<char>(codepoint));
        } else if (codepoint < 0x800) {
            utf8.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint < 0x10000) {
            utf8.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            utf8.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }

        text_.insert(static_cast<size_t>(cursor_), utf8);
        cursor_ += static_cast<int>(utf8.length());
        anchor_ = cursor_;
    }

    void insertText(const std::string& s) {
        if (s.empty()) return;
        pushUndo();
        deleteSelection();
        text_.insert(static_cast<size_t>(cursor_), s);
        cursor_ += static_cast<int>(s.length());
        anchor_ = cursor_;
    }

    void backspace(bool wordDelete = false) {
        if (deleteSelection()) return;
        if (cursor_ <= 0) return;

        pushUndo();
        if (wordDelete) {
            int target = findWordLeft(cursor_);
            text_.erase(static_cast<size_t>(target), static_cast<size_t>(cursor_ - target));
            cursor_ = target;
            anchor_ = target;
        } else {
            text_.erase(static_cast<size_t>(cursor_ - 1), 1);
            cursor_--;
            anchor_ = cursor_;
        }
    }

    void forwardDelete(bool wordDelete = false) {
        if (deleteSelection()) return;
        if (cursor_ >= static_cast<int>(text_.length())) return;

        pushUndo();
        if (wordDelete) {
            int target = findWordRight(cursor_);
            text_.erase(static_cast<size_t>(cursor_), static_cast<size_t>(target - cursor_));
            anchor_ = cursor_;
        } else {
            text_.erase(static_cast<size_t>(cursor_), 1);
            anchor_ = cursor_;
        }
    }

    void moveLeft(bool select = false, bool wordJump = false) {
        if (!select && hasSelection() && !wordJump) {
            cursor_ = getSelectionStart();
            anchor_ = cursor_;
            return;
        }

        if (wordJump) {
            cursor_ = findWordLeft(cursor_);
        } else if (cursor_ > 0) {
            cursor_--;
        }

        if (!select) anchor_ = cursor_;
    }

    void moveRight(bool select = false, bool wordJump = false) {
        if (!select && hasSelection() && !wordJump) {
            cursor_ = getSelectionEnd();
            anchor_ = cursor_;
            return;
        }

        if (wordJump) {
            cursor_ = findWordRight(cursor_);
        } else if (cursor_ < static_cast<int>(text_.length())) {
            cursor_++;
        }

        if (!select) anchor_ = cursor_;
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
        if (!text_.empty()) {
            pushUndo();
            text_.clear();
            cursor_ = 0;
            anchor_ = 0;
        }
    }

    // Auto-Scroll Containment Math
    void ensureCursorVisible(float viewportWidth, float padding, float charAdvance) {
        if (viewportWidth <= padding * 2.0f || charAdvance <= 0.001f) return;

        float curX = static_cast<float>(cursor_) * charAdvance;
        float viewMinX = scrollX_;
        float viewMaxX = scrollX_ + (viewportWidth - padding * 2.0f);

        if (curX < viewMinX) {
            scrollX_ = curX;
        } else if (curX > viewMaxX) {
            scrollX_ = curX - (viewportWidth - padding * 2.0f);
        }
        scrollX_ = std::max(0.0f, scrollX_);
    }

    [[nodiscard]] float getScrollX() const noexcept { return scrollX_; }
    void setScrollX(float sx) noexcept { scrollX_ = std::max(0.0f, sx); }

    // Input Validation Filter
    void setCharFilter(std::function<bool(char32_t)> filter) {
        charFilter_ = std::move(filter);
    }

    // Undo / Redo
    [[nodiscard]] bool canUndo() const noexcept { return !undoStack_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !redoStack_.empty(); }

    void undo() {
        if (!canUndo()) return;
        Snapshot current{text_, cursor_, anchor_};
        Snapshot target = std::move(undoStack_.back());
        undoStack_.pop_back();

        redoStack_.push_back(std::move(current));
        text_ = std::move(target.text);
        cursor_ = target.cursor;
        anchor_ = target.anchor;
    }

    void redo() {
        if (!canRedo()) return;
        Snapshot current{text_, cursor_, anchor_};
        Snapshot target = std::move(redoStack_.back());
        redoStack_.pop_back();

        undoStack_.push_back(std::move(current));
        text_ = std::move(target.text);
        cursor_ = target.cursor;
        anchor_ = target.anchor;
    }

private:
    struct Snapshot {
        std::string text;
        int cursor{0};
        int anchor{0};
    };

    void pushUndo() {
        undoStack_.push_back({text_, cursor_, anchor_});
        redoStack_.clear();
    }

    [[nodiscard]] int findWordLeft(int pos) const noexcept {
        if (pos <= 0) return 0;
        int c = pos - 1;
        while (c > 0 && std::isspace(static_cast<unsigned char>(text_[static_cast<size_t>(c)]))) {
            c--;
        }
        bool isAlpha = std::isalnum(static_cast<unsigned char>(text_[static_cast<size_t>(c)])) || text_[static_cast<size_t>(c)] == '_';
        while (c > 0) {
            char prev = text_[static_cast<size_t>(c - 1)];
            if (std::isspace(static_cast<unsigned char>(prev))) break;
            bool prevAlpha = std::isalnum(static_cast<unsigned char>(prev)) || prev == '_';
            if (prevAlpha != isAlpha) break;
            c--;
        }
        return c;
    }

    [[nodiscard]] int findWordRight(int pos) const noexcept {
        int len = static_cast<int>(text_.length());
        if (pos >= len) return len;
        int c = pos;
        bool isAlpha = std::isalnum(static_cast<unsigned char>(text_[static_cast<size_t>(c)])) || text_[static_cast<size_t>(c)] == '_';
        while (c < len) {
            char ch = text_[static_cast<size_t>(c)];
            if (std::isspace(static_cast<unsigned char>(ch))) break;
            bool chAlpha = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
            if (chAlpha != isAlpha) break;
            c++;
        }
        while (c < len && std::isspace(static_cast<unsigned char>(text_[static_cast<size_t>(c)]))) {
            c++;
        }
        return c;
    }

    std::string text_{};
    int cursor_{0};
    int anchor_{0};
    float scrollX_{0.0f};
    std::function<bool(char32_t)> charFilter_{nullptr};

    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
};

/**
 * @brief Headless presenter for multi-line code/script editors with 2D cursor,
 * selection ranges, indentation, comment toggles, and minimap viewport math.
 */
class MultiLineTextPresenter {
public:
    MultiLineTextPresenter() = default;
    explicit MultiLineTextPresenter(std::string_view initialText)
        : doc_(initialText) {}

    [[nodiscard]] core::TextDocument& getDocument() noexcept { return doc_; }
    [[nodiscard]] const core::TextDocument& getDocument() const noexcept { return doc_; }

    void setText(std::string_view text) {
        doc_.setText(text);
        cursor_ = core::TextCoord{0, 0};
        anchor_ = cursor_;
        scrollY_ = 0.0f;
        scrollX_ = 0.0f;
    }

    [[nodiscard]] core::TextCoord getCursor() const noexcept { return cursor_; }
    void setCursor(core::TextCoord coord, bool keepAnchor = false) noexcept {
        cursor_ = doc_.clampCoord(coord);
        if (!keepAnchor) {
            anchor_ = cursor_;
        }
    }

    [[nodiscard]] core::TextCoord getAnchor() const noexcept { return anchor_; }
    [[nodiscard]] bool hasSelection() const noexcept { return cursor_ != anchor_; }

    [[nodiscard]] core::TextRange getSelectionRange() const noexcept {
        return core::TextRange{anchor_, cursor_}.normalized();
    }

    [[nodiscard]] std::string getSelectedText() const {
        if (!hasSelection()) return "";
        return doc_.getRangeText(getSelectionRange());
    }

    void selectAll() noexcept {
        anchor_ = core::TextCoord{0, 0};
        int lastLine = static_cast<int>(doc_.getLineCount()) - 1;
        int lastCol = static_cast<int>(doc_.getLineLength(static_cast<size_t>(lastLine)));
        cursor_ = core::TextCoord{lastLine, lastCol};
    }

    void clearSelection() noexcept {
        anchor_ = cursor_;
    }

    void setSelection(core::TextCoord anchor, core::TextCoord cursor) noexcept {
        anchor_ = doc_.clampCoord(anchor);
        cursor_ = doc_.clampCoord(cursor);
    }

    void selectWordAt(core::TextCoord coord) noexcept {
        core::TextRange range = doc_.getWordRangeAt(coord);
        anchor_ = range.start;
        cursor_ = range.end;
    }

    bool deleteSelection() {
        if (!hasSelection()) return false;
        core::TextRange range = getSelectionRange();
        cursor_ = doc_.erase(range);
        anchor_ = cursor_;
        return true;
    }

    void insertChar(char32_t codepoint) {
        if (codepoint == '\r') return;
        if (codepoint == '\n') {
            insertNewLine();
            return;
        }
        if (codepoint < 32 || codepoint == 127) return;

        deleteSelection();

        std::string utf8;
        if (codepoint < 0x80) {
            utf8.push_back(static_cast<char>(codepoint));
        } else if (codepoint < 0x800) {
            utf8.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint < 0x10000) {
            utf8.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            utf8.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }

        cursor_ = doc_.insert(cursor_, utf8);
        anchor_ = cursor_;
    }

    void insertText(std::string_view s) {
        if (s.empty()) return;
        deleteSelection();
        cursor_ = doc_.insert(cursor_, s);
        anchor_ = cursor_;
    }

    void insertNewLine() {
        deleteSelection();
        // Auto-indent: preserve leading whitespace of current line
        const auto& line = doc_.getLine(static_cast<size_t>(cursor_.line));
        std::string indent;
        for (char c : line) {
            if (c == ' ' || c == '\t') indent.push_back(c);
            else break;
        }

        std::string toInsert = "\n" + indent;
        cursor_ = doc_.insert(cursor_, toInsert);
        anchor_ = cursor_;
    }

    void backspace(bool wordDelete = false) {
        if (deleteSelection()) return;
        if (cursor_.line == 0 && cursor_.column == 0) return;

        if (wordDelete) {
            core::TextCoord target = doc_.findWordLeft(cursor_);
            cursor_ = doc_.erase(core::TextRange{target, cursor_});
            anchor_ = cursor_;
        } else {
            if (cursor_.column > 0) {
                core::TextCoord target{cursor_.line, cursor_.column - 1};
                cursor_ = doc_.erase(core::TextRange{target, cursor_});
                anchor_ = cursor_;
            } else {
                int prevLine = cursor_.line - 1;
                int prevCol = static_cast<int>(doc_.getLineLength(static_cast<size_t>(prevLine)));
                cursor_ = doc_.erase(core::TextRange{core::TextCoord{prevLine, prevCol}, cursor_});
                anchor_ = cursor_;
            }
        }
    }

    void forwardDelete(bool wordDelete = false) {
        if (deleteSelection()) return;
        int maxLine = static_cast<int>(doc_.getLineCount()) - 1;
        int lineLen = static_cast<int>(doc_.getLineLength(static_cast<size_t>(cursor_.line)));

        if (cursor_.line == maxLine && cursor_.column >= lineLen) return;

        if (wordDelete) {
            core::TextCoord target = doc_.findWordRight(cursor_);
            cursor_ = doc_.erase(core::TextRange{cursor_, target});
            anchor_ = cursor_;
        } else {
            if (cursor_.column < lineLen) {
                core::TextCoord target{cursor_.line, cursor_.column + 1};
                cursor_ = doc_.erase(core::TextRange{cursor_, target});
                anchor_ = cursor_;
            } else {
                core::TextCoord target{cursor_.line + 1, 0};
                cursor_ = doc_.erase(core::TextRange{cursor_, target});
                anchor_ = cursor_;
            }
        }
    }

    // 2D Cursor Navigation
    void moveLeft(bool select = false, bool wordJump = false) {
        if (!select && hasSelection() && !wordJump) {
            cursor_ = getSelectionRange().start;
            anchor_ = cursor_;
            return;
        }

        if (wordJump) {
            cursor_ = doc_.findWordLeft(cursor_);
        } else {
            if (cursor_.column > 0) {
                cursor_.column--;
            } else if (cursor_.line > 0) {
                cursor_.line--;
                cursor_.column = static_cast<int>(doc_.getLineLength(static_cast<size_t>(cursor_.line)));
            }
        }

        if (!select) anchor_ = cursor_;
    }

    void moveRight(bool select = false, bool wordJump = false) {
        if (!select && hasSelection() && !wordJump) {
            cursor_ = getSelectionRange().end;
            anchor_ = cursor_;
            return;
        }

        if (wordJump) {
            cursor_ = doc_.findWordRight(cursor_);
        } else {
            int lineLen = static_cast<int>(doc_.getLineLength(static_cast<size_t>(cursor_.line)));
            if (cursor_.column < lineLen) {
                cursor_.column++;
            } else if (cursor_.line + 1 < static_cast<int>(doc_.getLineCount())) {
                cursor_.line++;
                cursor_.column = 0;
            }
        }

        if (!select) anchor_ = cursor_;
    }

    void moveUp(bool select = false) {
        if (cursor_.line > 0) {
            cursor_.line--;
            cursor_.column = std::min(cursor_.column, static_cast<int>(doc_.getLineLength(static_cast<size_t>(cursor_.line))));
        } else {
            cursor_.column = 0;
        }
        if (!select) anchor_ = cursor_;
    }

    void moveDown(bool select = false) {
        if (cursor_.line + 1 < static_cast<int>(doc_.getLineCount())) {
            cursor_.line++;
            cursor_.column = std::min(cursor_.column, static_cast<int>(doc_.getLineLength(static_cast<size_t>(cursor_.line))));
        } else {
            cursor_.column = static_cast<int>(doc_.getLineLength(static_cast<size_t>(cursor_.line)));
        }
        if (!select) anchor_ = cursor_;
    }

    void moveHome(bool select = false) {
        // Toggle between first non-whitespace and column 0
        const auto& line = doc_.getLine(static_cast<size_t>(cursor_.line));
        int firstNonWs = 0;
        while (firstNonWs < static_cast<int>(line.length()) && std::isspace(static_cast<unsigned char>(line[static_cast<size_t>(firstNonWs)]))) {
            firstNonWs++;
        }

        if (cursor_.column == firstNonWs) {
            cursor_.column = 0;
        } else {
            cursor_.column = firstNonWs;
        }
        if (!select) anchor_ = cursor_;
    }

    void moveEnd(bool select = false) {
        cursor_.column = static_cast<int>(doc_.getLineLength(static_cast<size_t>(cursor_.line)));
        if (!select) anchor_ = cursor_;
    }

    void moveDocumentStart(bool select = false) {
        cursor_ = core::TextCoord{0, 0};
        if (!select) anchor_ = cursor_;
    }

    void moveDocumentEnd(bool select = false) {
        int lastLine = static_cast<int>(doc_.getLineCount()) - 1;
        cursor_ = core::TextCoord{lastLine, static_cast<int>(doc_.getLineLength(static_cast<size_t>(lastLine)))};
        if (!select) anchor_ = cursor_;
    }

    // Code Usability Actions
    void indentSelection(int spaces = 4) {
        std::string pad(static_cast<size_t>(spaces), ' ');
        core::TextRange range = getSelectionRange();
        int startLine = range.start.line;
        int endLine = range.end.line;

        // If selection is empty, just insert spaces
        if (range.empty()) {
            cursor_ = doc_.insert(cursor_, pad);
            anchor_ = cursor_;
            return;
        }

        for (int l = startLine; l <= endLine; ++l) {
            doc_.insert(core::TextCoord{l, 0}, pad);
        }
        anchor_.column += spaces;
        cursor_.column += spaces;
    }

    void unindentSelection(int spaces = 4) {
        core::TextRange range = getSelectionRange();
        int startLine = range.start.line;
        int endLine = range.end.line;

        for (int l = startLine; l <= endLine; ++l) {
            const auto& line = doc_.getLine(static_cast<size_t>(l));
            int removeCount = 0;
            while (removeCount < spaces && removeCount < static_cast<int>(line.length()) && line[static_cast<size_t>(removeCount)] == ' ') {
                removeCount++;
            }
            if (removeCount > 0) {
                doc_.erase(core::TextRange{core::TextCoord{l, 0}, core::TextCoord{l, removeCount}});
            }
        }
    }

    void toggleLineComment(std::string_view prefix = "#") {
        core::TextRange range = getSelectionRange();
        int startLine = range.start.line;
        int endLine = range.end.line;

        bool allCommented = true;
        for (int l = startLine; l <= endLine; ++l) {
            const auto& line = doc_.getLine(static_cast<size_t>(l));
            size_t idx = line.find_first_not_of(" \t");
            if (idx != std::string::npos) {
                if (!line.substr(idx).starts_with(prefix)) {
                    allCommented = false;
                    break;
                }
            }
        }

        std::string padPrefix = std::string(prefix) + " ";
        for (int l = startLine; l <= endLine; ++l) {
            const auto& line = doc_.getLine(static_cast<size_t>(l));
            size_t idx = line.find_first_not_of(" \t");
            if (idx == std::string::npos) continue;

            if (allCommented) {
                // Remove prefix
                int eraseLen = line.substr(idx).starts_with(padPrefix) ? static_cast<int>(padPrefix.length()) : static_cast<int>(prefix.length());
                doc_.erase(core::TextRange{core::TextCoord{l, static_cast<int>(idx)}, core::TextCoord{l, static_cast<int>(idx) + eraseLen}});
            } else {
                // Add prefix
                doc_.insert(core::TextCoord{l, static_cast<int>(idx)}, padPrefix);
            }
        }
    }

    void indentSelectedLines(int spaces = 4) { indentSelection(spaces); }
    void unindentSelectedLines(int spaces = 4) { unindentSelection(spaces); }
    void toggleComment(std::string_view prefix = "#") { toggleLineComment(prefix); }

    void duplicateCurrentLineOrSelection() {
        if (hasSelection()) {
            std::string text = getSelectedText();
            cursor_ = doc_.insert(getSelectionRange().end, text);
            anchor_ = cursor_;
        } else {
            const auto& line = doc_.getLine(static_cast<size_t>(cursor_.line));
            std::string toInsert = "\n" + line;
            doc_.insert(core::TextCoord{cursor_.line, static_cast<int>(line.length())}, toInsert);
            cursor_.line++;
            anchor_ = cursor_;
        }
    }

    // Viewport & Minimap Layout Math
    void setViewport(float w, float h, float lineH, float charW) noexcept {
        viewportW_ = w;
        viewportH_ = h;
        lineH_ = std::max(1.0f, lineH);
        charW_ = std::max(1.0f, charW);
    }

    [[nodiscard]] float getScrollY() const noexcept { return scrollY_; }
    [[nodiscard]] float getScrollX() const noexcept { return scrollX_; }
    void setScrollY(float sy) noexcept { scrollY_ = std::clamp(sy, 0.0f, getMaxScrollY()); }
    void setScrollX(float sx) noexcept { scrollX_ = std::max(0.0f, sx); }

    [[nodiscard]] float getTotalContentHeight() const noexcept {
        return static_cast<float>(doc_.getLineCount()) * lineH_;
    }

    [[nodiscard]] float getMaxScrollY() const noexcept {
        return std::max(0.0f, getTotalContentHeight() - viewportH_ + lineH_ * 2.0f);
    }

    void ensureCursorVisible(float gutterW = 48.0f) {
        float curY = static_cast<float>(cursor_.line) * lineH_;
        float curX = static_cast<float>(cursor_.column) * charW_;

        // Vertical scroll containment
        if (curY < scrollY_) {
            scrollY_ = curY;
        } else if (curY + lineH_ > scrollY_ + viewportH_) {
            scrollY_ = curY + lineH_ - viewportH_;
        }
        scrollY_ = std::clamp(scrollY_, 0.0f, getMaxScrollY());

        // Horizontal scroll containment
        float viewTextW = std::max(10.0f, viewportW_ - gutterW);
        if (curX < scrollX_) {
            scrollX_ = curX;
        } else if (curX + charW_ > scrollX_ + viewTextW) {
            scrollX_ = curX + charW_ - viewTextW;
        }
        scrollX_ = std::max(0.0f, scrollX_);
    }

    /**
     * @brief Computes the vertical position and height of the minimap lens indicator
     * for a given minimap track height.
     */
    [[nodiscard]] std::pair<float, float> getMinimapThumbBounds(float minimapTrackH) const noexcept {
        float totalH = getTotalContentHeight();
        if (totalH <= viewportH_ || totalH <= 0.001f) {
            return {0.0f, minimapTrackH};
        }

        float ratio = std::clamp(viewportH_ / totalH, 0.05f, 1.0f);
        float thumbH = std::max(20.0f, ratio * minimapTrackH);
        float maxThumbTravel = minimapTrackH - thumbH;
        float scrollRatio = (getMaxScrollY() > 0.001f) ? (scrollY_ / getMaxScrollY()) : 0.0f;
        float thumbY = scrollRatio * maxThumbTravel;

        return {thumbY, thumbH};
    }

    /**
     * @brief Converts a Y coordinate touch/click on the minimap into scrollY.
     */
    void scrubMinimap(float localMinimapY, float minimapTrackH) {
        float totalH = getTotalContentHeight();
        if (totalH <= viewportH_ || minimapTrackH <= 0.001f) return;

        float ratio = std::clamp(localMinimapY / minimapTrackH, 0.0f, 1.0f);
        setScrollY(ratio * getMaxScrollY());
    }

private:
    core::TextDocument doc_{};
    core::TextCoord cursor_{0, 0};
    core::TextCoord anchor_{0, 0};

    float viewportW_{800.0f};
    float viewportH_{600.0f};
    float lineH_{18.0f};
    float charW_{8.0f};

    float scrollY_{0.0f};
    float scrollX_{0.0f};
};

} // namespace eatsbits::presenter
