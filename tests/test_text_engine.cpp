#include "eatsbits/core/text_document.hpp"
#include "eatsbits/presenter/text_presenter.hpp"
#include "eatsbits/ui/input/focus_manager.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace eatsbits::core;
using namespace eatsbits::presenter;
using namespace eatsbits::ui;

// Dummy focusable element for testing FocusManager
class MockFocusable : public IFocusable {
public:
    bool onFocusGained() override {
        focused_ = true;
        gainedCount_++;
        return allowFocus_;
    }

    void onFocusLost() override {
        focused_ = false;
        lostCount_++;
    }

    bool handleKey(int key, int, int, int) override {
        lastKey_ = key;
        return true;
    }

    bool handleChar(char32_t codepoint) override {
        lastChar_ = codepoint;
        return true;
    }

    [[nodiscard]] bool isFocused() const noexcept override {
        return focused_;
    }

    bool allowFocus_{true};
    bool focused_{false};
    int gainedCount_{0};
    int lostCount_{0};
    int lastKey_{-1};
    char32_t lastChar_{0};
};

static void testTextDocument() {
    std::cout << "[Test] TextDocument..." << std::endl;

    // 1. Creation and line splitting
    TextDocument doc("line 1\nline 2\nline 3");
    assert(doc.getLineCount() == 3);
    assert(doc.getLine(0) == "line 1");
    assert(doc.getLine(1) == "line 2");
    assert(doc.getLine(2) == "line 3");
    assert(doc.getText() == "line 1\nline 2\nline 3");

    // 2. Coordinate clamping
    TextCoord c1 = doc.clampCoord(TextCoord{10, 100});
    assert(c1.line == 2);
    assert(c1.column == 6); // "line 3" has length 6

    TextCoord c2 = doc.clampCoord(TextCoord{-5, -2});
    assert(c2.line == 0);
    assert(c2.column == 0);

    // 3. Single-line insert
    doc.insert(TextCoord{0, 6}, " extended");
    assert(doc.getLine(0) == "line 1 extended");
    assert(doc.getLineCount() == 3);

    // 4. Multi-line insert
    doc.insert(TextCoord{1, 4}, "\ninserted line\nanother line");
    assert(doc.getLineCount() == 5);
    assert(doc.getLine(1) == "line");
    assert(doc.getLine(2) == "inserted line");
    assert(doc.getLine(3) == "another line 2");

    // 5. Range Text extraction
    std::string rangeStr = doc.getRangeText(TextRange{TextCoord{1, 0}, TextCoord{3, 7}});
    assert(rangeStr == "line\ninserted line\nanother");

    // 6. Range Erase
    doc.erase(TextRange{TextCoord{1, 4}, TextCoord{3, 12}});
    assert(doc.getLineCount() == 3);
    assert(doc.getLine(1) == "line 2");

    // 7. Undo / Redo
    TextDocument undoDoc("alpha beta");
    undoDoc.insert(TextCoord{0, 10}, " gamma");
    assert(undoDoc.getText() == "alpha beta gamma");
    assert(undoDoc.canUndo());

    undoDoc.undo();
    assert(undoDoc.getText() == "alpha beta");
    assert(undoDoc.canRedo());

    undoDoc.redo();
    assert(undoDoc.getText() == "alpha beta gamma");

    undoDoc.erase(TextRange{TextCoord{0, 5}, TextCoord{0, 10}});
    assert(undoDoc.getText() == "alpha gamma");
    undoDoc.undo();
    assert(undoDoc.getText() == "alpha beta gamma");

    // 8. Word Boundaries
    TextDocument wordDoc("hello world_123   foo.bar");
    TextCoord w1 = wordDoc.findWordRight(TextCoord{0, 0});
    assert(w1.column == 6); // points to 'w'

    TextCoord w2 = wordDoc.findWordRight(w1);
    assert(w2.column == 18); // points to 'f'

    TextCoord wLeft = wordDoc.findWordLeft(w2);
    assert(wLeft.column == 6); // jumps back to start of 'world_123'

    TextRange wordRange = wordDoc.getWordRangeAt(TextCoord{0, 8}); // inside 'world_123'
    assert(wordRange.start.column == 6);
    assert(wordRange.end.column == 15);

    std::cout << "  -> TextDocument passed!" << std::endl;
}

static void testSingleLinePresenter() {
    std::cout << "[Test] SingleLineTextPresenter..." << std::endl;

    SingleLineTextPresenter p("initial text", /*selectAllOnSet=*/ false);
    assert(p.getText() == "initial text");
    assert(p.getCursor() == 12);
    assert(!p.hasSelection());

    // 1. Navigation & Word Jumps
    p.moveLeft(/*select=*/ false, /*wordJump=*/ true);
    assert(p.getCursor() == 8); // 't' in text

    p.moveLeft(/*select=*/ true, /*wordJump=*/ true);
    assert(p.getCursor() == 0);
    assert(p.hasSelection());
    assert(p.getSelectedText() == "initial ");

    // 2. Replacement on typing
    p.insertText("start ");
    assert(p.getText() == "start text");
    assert(p.getCursor() == 6);

    // 3. Word deletion
    p.moveEnd();
    p.backspace(/*wordDelete=*/ true);
    assert(p.getText() == "start ");
    assert(p.getCursor() == 6);

    // 4. Unicode / UTF-8 insertion
    p.insertChar(U'\u266B');
    assert(p.getText() == "start \xe2\x99\xab");

    // 5. Select word
    p.selectWordAt(2);
    assert(p.getSelectedText() == "start");

    // 6. Undo / Redo
    p.undo();
    assert(p.getSelectedText().empty());

    // 7. Auto-Scroll Containment Math
    p.setText("very long string that definitely overflows standard display boundaries", false);
    float charAdvance = 8.0f;
    float viewportWidth = 100.0f; // Only fits ~10 chars
    float padding = 10.0f;

    // Cursor at end
    p.ensureCursorVisible(viewportWidth, padding, charAdvance);
    [[maybe_unused]] float scrollX = p.getScrollX();
    assert(scrollX > 0.0f); // must have scrolled right

    // Move to start
    p.moveHome();
    p.ensureCursorVisible(viewportWidth, padding, charAdvance);
    assert(p.getScrollX() == 0.0f); // must scroll back to 0

    // 8. Char Filter (e.g. numeric only)
    SingleLineTextPresenter numField("123", false);
    numField.setCharFilter([](char32_t c) {
        return c >= '0' && c <= '9';
    });
    numField.insertChar(U'a'); // rejected
    assert(numField.getText() == "123");
    numField.insertChar(U'4'); // accepted
    assert(numField.getText() == "1234");

    std::cout << "  -> SingleLineTextPresenter passed!" << std::endl;
}

static void testMultiLinePresenter() {
    std::cout << "[Test] MultiLineTextPresenter..." << std::endl;

    MultiLineTextPresenter p("def synth():\n    freq = 440\n    return sin(freq)");
    assert(p.getDocument().getLineCount() == 3);

    // 1. 2D Navigation
    p.setCursor(TextCoord{0, 3});
    p.moveDown();
    assert(p.getCursor().line == 1);
    assert(p.getCursor().column == 3);

    p.moveEnd();
    assert(p.getCursor().column == 14);

    p.moveHome();
    assert(p.getCursor().column == 4); // jumps to first non-ws
    p.moveHome();
    assert(p.getCursor().column == 0); // toggles to 0

    // 2. Auto-indentation on new line
    p.setCursor(TextCoord{1, 14}); // end of "    freq = 440"
    p.insertNewLine();
    assert(p.getDocument().getLineCount() == 4);
    assert(p.getDocument().getLine(2) == "    ");
    assert(p.getCursor().line == 2);
    assert(p.getCursor().column == 4);

    // 3. Indent / Unindent selection
    p.setCursor(TextCoord{1, 0}, false);
    p.setCursor(TextCoord{2, 0}, true); // select lines 1 and 2
    p.indentSelection(4);
    assert(p.getDocument().getLine(1).starts_with("        freq"));

    p.unindentSelection(4);
    assert(p.getDocument().getLine(1).starts_with("    freq"));

    // 4. Comment / Uncomment toggle
    p.setCursor(TextCoord{1, 0}, false);
    p.setCursor(TextCoord{1, 5}, true);
    p.toggleLineComment("#");
    assert(p.getDocument().getLine(1).starts_with("    # freq"));

    p.toggleLineComment("#");
    assert(p.getDocument().getLine(1).starts_with("    freq"));

    // 5. Line Duplication
    p.setCursor(TextCoord{0, 0});
    p.clearSelection();
    p.duplicateCurrentLineOrSelection();
    assert(p.getDocument().getLineCount() == 5);
    assert(p.getDocument().getLine(0) == "def synth():");
    assert(p.getDocument().getLine(1) == "def synth():");

    // 6. Minimap Layout and Touch Scrubber Math
    p.setViewport(800.0f, 40.0f, 20.0f, 8.0f); // 5 lines * 20 = 100px total height, view is 40px
    assert(p.getMaxScrollY() > 0.0f);

    auto [thumbY, thumbH] = p.getMinimapThumbBounds(100.0f);
    assert(thumbY >= 0.0f);
    assert(thumbH > 0.0f && thumbH <= 100.0f);

    p.scrubMinimap(50.0f, 100.0f); // scrub to 50%
    assert(p.getScrollY() > 0.0f);

    std::cout << "  -> MultiLineTextPresenter passed!" << std::endl;
}

static void testFocusManager() {
    std::cout << "[Test] FocusManager..." << std::endl;

    FocusManager fm;
    assert(!fm.isAnyFocused());

    MockFocusable f1;
    MockFocusable f2;

    // 1. Focus acquisition
    fm.requestFocus(&f1);
    assert(fm.hasFocus(&f1));
    assert(f1.focused_);
    assert(f1.gainedCount_ == 1);

    // 2. Focus shift
    fm.requestFocus(&f2);
    assert(!fm.hasFocus(&f1));
    assert(fm.hasFocus(&f2));
    assert(!f1.focused_);
    assert(f1.lostCount_ == 1);
    assert(f2.focused_);
    assert(f2.gainedCount_ == 1);

    // 3. Dispatching
    [[maybe_unused]] bool keyDispatched = fm.dispatchKey(65, 0, 1, 0);
    assert(keyDispatched);
    assert(f2.lastKey_ == 65);

    [[maybe_unused]] bool charDispatched = fm.dispatchChar(U'Z');
    assert(charDispatched);
    assert(f2.lastChar_ == U'Z');

    // 4. Focus clear
    fm.clearFocus();
    assert(!fm.isAnyFocused());
    assert(f2.lostCount_ == 1);

    // 5. Dispatch when unfocused returns false
    assert(!fm.dispatchKey(65, 0, 1, 0));
    assert(!fm.dispatchChar(U'A'));

    std::cout << "  -> FocusManager passed!" << std::endl;
}

int main() {
    std::cout << "=== Running Eatsbits Text Engine Unit Tests ===" << std::endl;
    testTextDocument();
    testSingleLinePresenter();
    testMultiLinePresenter();
    testFocusManager();
    std::cout << "=== All Text Engine Tests Passed Successfully! ===" << std::endl;
    return 0;
}
