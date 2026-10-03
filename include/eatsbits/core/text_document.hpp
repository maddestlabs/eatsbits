#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cctype>
#include <sstream>

namespace eatsbits::core {

struct TextCoord {
    int line{0};
    int column{0};

    constexpr auto operator<=>(const TextCoord&) const = default;
};

struct TextRange {
    TextCoord start;
    TextCoord end;

    constexpr auto operator<=>(const TextRange&) const = default;

    [[nodiscard]] constexpr bool empty() const noexcept {
        return start == end;
    }

    [[nodiscard]] constexpr TextRange normalized() const noexcept {
        return (start <= end) ? *this : TextRange{end, start};
    }
};

class TextDocument {
public:
    struct EditAction {
        enum class Type { Insert, Erase };
        Type type;
        TextCoord pos;
        std::string text;
    };

    TextDocument() : lines_{""} {}
    explicit TextDocument(std::string_view initialText) {
        setText(initialText);
    }

    void setText(std::string_view text) {
        lines_.clear();
        std::string cur;
        for (char c : text) {
            if (c == '\r') continue;
            if (c == '\n') {
                lines_.push_back(std::move(cur));
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        lines_.push_back(std::move(cur));
        clearHistory();
    }

    [[nodiscard]] std::string getText() const {
        std::string result;
        for (size_t i = 0; i < lines_.size(); ++i) {
            result.append(lines_[i]);
            if (i + 1 < lines_.size()) {
                result.push_back('\n');
            }
        }
        return result;
    }

    [[nodiscard]] size_t getLineCount() const noexcept {
        return lines_.size();
    }

    [[nodiscard]] const std::string& getLine(size_t lineIdx) const {
        static const std::string emptyLine;
        if (lineIdx < lines_.size()) return lines_[lineIdx];
        return emptyLine;
    }

    [[nodiscard]] size_t getLineLength(size_t lineIdx) const noexcept {
        if (lineIdx < lines_.size()) return lines_[lineIdx].length();
        return 0;
    }

    [[nodiscard]] TextCoord clampCoord(TextCoord coord) const noexcept {
        if (lines_.empty()) return TextCoord{0, 0};
        int line = std::clamp(coord.line, 0, static_cast<int>(lines_.size()) - 1);
        int col = std::clamp(coord.column, 0, static_cast<int>(lines_[static_cast<size_t>(line)].length()));
        return TextCoord{line, col};
    }

    [[nodiscard]] std::string getRangeText(TextRange range) const {
        TextRange norm = range.normalized();
        TextCoord s = clampCoord(norm.start);
        TextCoord e = clampCoord(norm.end);

        if (s == e) return "";

        if (s.line == e.line) {
            return lines_[static_cast<size_t>(s.line)].substr(static_cast<size_t>(s.column),
                                                             static_cast<size_t>(e.column - s.column));
        }

        std::string result;
        result.append(lines_[static_cast<size_t>(s.line)].substr(static_cast<size_t>(s.column)));
        result.push_back('\n');

        for (int l = s.line + 1; l < e.line; ++l) {
            result.append(lines_[static_cast<size_t>(l)]);
            result.push_back('\n');
        }

        result.append(lines_[static_cast<size_t>(e.line)].substr(0, static_cast<size_t>(e.column)));
        return result;
    }

    TextCoord insert(TextCoord pos, std::string_view text, bool recordUndo = true) {
        if (text.empty()) return clampCoord(pos);
        TextCoord actualPos = clampCoord(pos);

        if (recordUndo) {
            undoStack_.push_back({EditAction::Type::Insert, actualPos, std::string(text)});
            redoStack_.clear();
        }

        // Split text by lines
        std::vector<std::string> insertLines;
        std::string cur;
        for (char c : text) {
            if (c == '\r') continue;
            if (c == '\n') {
                insertLines.push_back(std::move(cur));
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        insertLines.push_back(std::move(cur));

        size_t lineIdx = static_cast<size_t>(actualPos.line);
        std::string originalLine = lines_[lineIdx];
        std::string prefix = originalLine.substr(0, static_cast<size_t>(actualPos.column));
        std::string suffix = originalLine.substr(static_cast<size_t>(actualPos.column));

        if (insertLines.size() == 1) {
            lines_[lineIdx] = prefix + insertLines[0] + suffix;
            return TextCoord{actualPos.line, actualPos.column + static_cast<int>(insertLines[0].length())};
        }

        lines_[lineIdx] = prefix + insertLines[0];
        for (size_t i = 1; i + 1 < insertLines.size(); ++i) {
            lines_.insert(lines_.begin() + static_cast<ptrdiff_t>(lineIdx + i), insertLines[i]);
        }
        lines_.insert(lines_.begin() + static_cast<ptrdiff_t>(lineIdx + insertLines.size() - 1),
                      insertLines.back() + suffix);

        int endLine = actualPos.line + static_cast<int>(insertLines.size()) - 1;
        int endCol = static_cast<int>(insertLines.back().length());
        return TextCoord{endLine, endCol};
    }

    TextCoord erase(TextRange range, bool recordUndo = true) {
        TextRange norm = range.normalized();
        TextCoord s = clampCoord(norm.start);
        TextCoord e = clampCoord(norm.end);

        if (s == e) return s;

        if (recordUndo) {
            std::string deletedText = getRangeText(norm);
            undoStack_.push_back({EditAction::Type::Erase, s, std::move(deletedText)});
            redoStack_.clear();
        }

        if (s.line == e.line) {
            lines_[static_cast<size_t>(s.line)].erase(static_cast<size_t>(s.column),
                                                      static_cast<size_t>(e.column - s.column));
            return s;
        }

        std::string prefix = lines_[static_cast<size_t>(s.line)].substr(0, static_cast<size_t>(s.column));
        std::string suffix = lines_[static_cast<size_t>(e.line)].substr(static_cast<size_t>(e.column));

        lines_[static_cast<size_t>(s.line)] = prefix + suffix;
        lines_.erase(lines_.begin() + static_cast<ptrdiff_t>(s.line + 1),
                     lines_.begin() + static_cast<ptrdiff_t>(e.line + 1));

        return s;
    }

    TextCoord replace(TextRange range, std::string_view text) {
        TextRange norm = range.normalized();
        if (!norm.empty()) {
            erase(norm);
        }
        return insert(norm.start, text);
    }

    // Undo / Redo
    [[nodiscard]] bool canUndo() const noexcept { return !undoStack_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !redoStack_.empty(); }

    TextCoord undo() {
        if (!canUndo()) return TextCoord{0, 0};
        EditAction act = std::move(undoStack_.back());
        undoStack_.pop_back();

        if (act.type == EditAction::Type::Insert) {
            // Undo insert = erase
            TextDocument tempDoc(act.text);
            int endLine = act.pos.line + static_cast<int>(tempDoc.getLineCount()) - 1;
            int endCol = (tempDoc.getLineCount() == 1)
                ? act.pos.column + static_cast<int>(act.text.length())
                : static_cast<int>(tempDoc.getLine(tempDoc.getLineCount() - 1).length());

            TextRange range{act.pos, TextCoord{endLine, endCol}};
            erase(range, /*recordUndo=*/ false);
            redoStack_.push_back(std::move(act));
            return act.pos;
        } else {
            // Undo erase = insert
            insert(act.pos, act.text, /*recordUndo=*/ false);
            redoStack_.push_back(std::move(act));
            return act.pos;
        }
    }

    TextCoord redo() {
        if (!canRedo()) return TextCoord{0, 0};
        EditAction act = std::move(redoStack_.back());
        redoStack_.pop_back();

        if (act.type == EditAction::Type::Insert) {
            TextCoord res = insert(act.pos, act.text, /*recordUndo=*/ false);
            undoStack_.push_back(std::move(act));
            return res;
        } else {
            TextDocument tempDoc(act.text);
            int endLine = act.pos.line + static_cast<int>(tempDoc.getLineCount()) - 1;
            int endCol = (tempDoc.getLineCount() == 1)
                ? act.pos.column + static_cast<int>(act.text.length())
                : static_cast<int>(tempDoc.getLine(tempDoc.getLineCount() - 1).length());

            TextRange range{act.pos, TextCoord{endLine, endCol}};
            erase(range, /*recordUndo=*/ false);
            undoStack_.push_back(std::move(act));
            return act.pos;
        }
    }

    void clearHistory() noexcept {
        undoStack_.clear();
        redoStack_.clear();
    }

    // Word Boundary Navigation
    [[nodiscard]] TextCoord findWordLeft(TextCoord pos) const noexcept {
        TextCoord p = clampCoord(pos);
        if (p.column == 0) {
            if (p.line == 0) return p;
            int prevLine = p.line - 1;
            return TextCoord{prevLine, static_cast<int>(lines_[static_cast<size_t>(prevLine)].length())};
        }

        const auto& line = lines_[static_cast<size_t>(p.line)];
        int c = p.column - 1;

        // Skip whitespace left
        while (c > 0 && std::isspace(static_cast<unsigned char>(line[static_cast<size_t>(c)]))) {
            c--;
        }

        // Determine if delimiter or alphanumeric
        bool isAlpha = std::isalnum(static_cast<unsigned char>(line[static_cast<size_t>(c)])) || line[static_cast<size_t>(c)] == '_';
        while (c > 0) {
            char prev = line[static_cast<size_t>(c - 1)];
            if (std::isspace(static_cast<unsigned char>(prev))) break;
            bool prevAlpha = std::isalnum(static_cast<unsigned char>(prev)) || prev == '_';
            if (prevAlpha != isAlpha) break;
            c--;
        }

        return TextCoord{p.line, c};
    }

    [[nodiscard]] TextCoord findWordRight(TextCoord pos) const noexcept {
        TextCoord p = clampCoord(pos);
        const auto& line = lines_[static_cast<size_t>(p.line)];
        int len = static_cast<int>(line.length());

        if (p.column >= len) {
            if (p.line + 1 < static_cast<int>(lines_.size())) {
                return TextCoord{p.line + 1, 0};
            }
            return p;
        }

        int c = p.column;
        bool isAlpha = std::isalnum(static_cast<unsigned char>(line[static_cast<size_t>(c)])) || line[static_cast<size_t>(c)] == '_';

        while (c < len) {
            char ch = line[static_cast<size_t>(c)];
            if (std::isspace(static_cast<unsigned char>(ch))) break;
            bool chAlpha = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
            if (chAlpha != isAlpha) break;
            c++;
        }

        // Skip subsequent whitespace
        while (c < len && std::isspace(static_cast<unsigned char>(line[static_cast<size_t>(c)]))) {
            c++;
        }

        return TextCoord{p.line, c};
    }

    [[nodiscard]] TextRange getWordRangeAt(TextCoord pos) const noexcept {
        TextCoord p = clampCoord(pos);
        const auto& line = lines_[static_cast<size_t>(p.line)];
        int len = static_cast<int>(line.length());
        if (len == 0) return TextRange{p, p};

        int col = std::min(p.column, len - 1);
        bool isAlpha = std::isalnum(static_cast<unsigned char>(line[static_cast<size_t>(col)])) || line[static_cast<size_t>(col)] == '_';

        int startCol = col;
        while (startCol > 0) {
            char prev = line[static_cast<size_t>(startCol - 1)];
            bool prevAlpha = std::isalnum(static_cast<unsigned char>(prev)) || prev == '_';
            if (prevAlpha != isAlpha) break;
            startCol--;
        }

        int endCol = col;
        while (endCol < len) {
            char ch = line[static_cast<size_t>(endCol)];
            bool chAlpha = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
            if (chAlpha != isAlpha) break;
            endCol++;
        }

        return TextRange{TextCoord{p.line, startCol}, TextCoord{p.line, endCol}};
    }

private:
    std::vector<std::string> lines_;
    std::vector<EditAction> undoStack_;
    std::vector<EditAction> redoStack_;
};

} // namespace eatsbits::core
