#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cmath>

#include "eatsbits/ui/batch_renderer_2d.hpp"
#include "eatsbits/terminal/terminal_grid.hpp"
#include "eatsbits/terminal/ansi_parser.hpp"
#include "eatsbits/eatscript/repl.hpp"
#include "eatsbits/abi/host_registry.hpp"

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        std::cout << "Assertion failed: (" #expr ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while(0)

using namespace eatsbits;
using namespace eatsbits::ui;
using namespace eatsbits::terminal;
using namespace eatsbits::eatscript;

// ============================================================================
// 1. WebGPU Glyph Atlas & Monospace Text Pipeline Tests
// ============================================================================
void testBatchRendererMonospacePipeline() {
    std::cout << "[Test 1/5] BatchRenderer2D Monospace Atlas & Vector Glyphs..." << std::endl;

    BatchRenderer2D renderer;
    bool initOk = renderer.initialize(nullptr, 800, 600, RenderBackendType::Filament);
    REQUIRE(initOk);

    // Verify initial state
    REQUIRE(!renderer.isMonospaceAtlasReady());
    renderer.initDefaultMonospaceAtlas();
    REQUIRE(renderer.isMonospaceAtlasReady());

    renderer.beginFrame(800.0f, 600.0f);
    size_t initialVerts = renderer.getVertexCount();

    // Test A: Draw single ASCII character
    renderer.drawMonospaceCell(10.0f, 20.0f, 8.0f, 16.0f, 'A', 0xFFFFFFFF, 0x00000000, 0);
    REQUIRE(renderer.getVertexCount() == initialVerts + 6); // 2 triangles

    // Test B: Draw bold ASCII character (double pass / faux bold)
    size_t preBold = renderer.getVertexCount();
    renderer.drawMonospaceCell(30.0f, 20.0f, 8.0f, 16.0f, 'B', 0xFFFFFFFF, 0x00000000,
                              static_cast<uint8_t>(tui::TextAttr::Bold));
    REQUIRE(renderer.getVertexCount() == preBold + 12); // 4 triangles for bold

    // Test C: Draw cell with background and underline
    size_t preUnderline = renderer.getVertexCount();
    renderer.drawMonospaceCell(50.0f, 20.0f, 8.0f, 16.0f, 'C', 0xFFFFFFFF, 0xFF222222,
                              static_cast<uint8_t>(tui::TextAttr::Underline));
    // 6 for bg rect + 6 for glyph + 6 for underline rect = 18 vertices
    REQUIRE(renderer.getVertexCount() == preUnderline + 18);

    // Test D: Draw custom vector box-drawing glyphs (0x2500: ─, 0x2502: │, 0x250C: ┌, 0x253C: ┼, 0x2550: ═)
    size_t preBox = renderer.getVertexCount();
    renderer.drawMonospaceCell(70.0f, 20.0f, 10.0f, 20.0f, 0x2500, 0xFF00FF00, 0); // ─ (1 rect = 6 verts)
    REQUIRE(renderer.getVertexCount() == preBox + 6);

    preBox = renderer.getVertexCount();
    renderer.drawMonospaceCell(90.0f, 20.0f, 10.0f, 20.0f, 0x253C, 0xFF00FF00, 0); // ┼ (2 rects = 12 verts)
    REQUIRE(renderer.getVertexCount() == preBox + 12);

    preBox = renderer.getVertexCount();
    renderer.drawMonospaceCell(110.0f, 20.0f, 10.0f, 20.0f, 0x2550, 0xFF00FF00, 0); // ═ double line (2 rects = 12 verts)
    REQUIRE(renderer.getVertexCount() == preBox + 12);

    // Test E: Draw block elements (0x2588: █, 0x2580: ▀, 0x2584: ▄, 0x2596: ▖)
    preBox = renderer.getVertexCount();
    renderer.drawMonospaceCell(130.0f, 20.0f, 10.0f, 20.0f, 0x2588, 0xFF00FFFF, 0); // █ (1 rect = 6 verts)
    REQUIRE(renderer.getVertexCount() == preBox + 6);

    preBox = renderer.getVertexCount();
    renderer.drawMonospaceCell(150.0f, 20.0f, 10.0f, 20.0f, 0x2596, 0xFF00FFFF, 0); // ▖ quadrant (1 rect = 6 verts)
    REQUIRE(renderer.getVertexCount() == preBox + 6);

    // Test F: Draw Braille glyph (0x28FF: all 8 dots)
    preBox = renderer.getVertexCount();
    renderer.drawMonospaceCell(170.0f, 20.0f, 10.0f, 20.0f, 0x28FF, 0xFFFFAA00, 0); // 8 dots = 8 rects = 48 verts
    REQUIRE(renderer.getVertexCount() == preBox + 48);

    // Test G: String text rendering with subpixel positioning
    preBox = renderer.getVertexCount();
    renderer.drawMonospaceText(200.0f, 20.0f, 8.0f, 16.0f, "Eatsbits 1.0\nSecond Line",
                              0xFFFFFFFF, 0x00000000, 0, 0.25f, 0.50f);
    REQUIRE(renderer.getVertexCount() > preBox);

    // Test H: Grid rendering
    std::vector<tui::Cell> testGrid(80 * 24);
    for (int i = 0; i < 80; ++i) {
        testGrid[i] = tui::Cell{'=', tui::Color::White(), tui::Color::DarkGray(), 0};
    }
    preBox = renderer.getVertexCount();
    renderer.drawTerminalGrid(0.0f, 0.0f, 8.0f, 16.0f, testGrid.data(), 80, 24, 0.0f, 0.0f);
    REQUIRE(renderer.getVertexCount() > preBox);

    renderer.endFrame();
    renderer.shutdown();

    std::cout << "  [PASS] WebGPU monospace text & vector glyph pipeline verified." << std::endl;
}

// ============================================================================
// 2. Dual-Buffered Terminal Cell Grid Tests
// ============================================================================
void testTerminalGridBuffer() {
    std::cout << "[Test 2/5] TerminalGrid Double-Buffering, Scrolling & Dirty Tracking..." << std::endl;

    TerminalGrid grid(80, 24, 100);
    REQUIRE(grid.getCols() == 80);
    REQUIRE(grid.getRows() == 24);
    REQUIRE(grid.hasAnyDirty());

    grid.swapBuffers();
    REQUIRE(!grid.hasAnyDirty());
    REQUIRE(!grid.isRowDirty(0));

    // Writing text updates back buffer and marks row dirty
    grid.writeString("Hello Eatscript!");
    REQUIRE(grid.hasAnyDirty());
    REQUIRE(grid.isRowDirty(0));
    REQUIRE(!grid.isRowDirty(1));
    REQUIRE(grid.getCursorX() == 16);
    REQUIRE(grid.getCursorY() == 0);

    // Front buffer is still clean until swapBuffers()
    REQUIRE(grid.getFrontBuffer()[0].codepoint == ' ');
    REQUIRE(grid.getBackBuffer()[0].codepoint == 'H');

    grid.swapBuffers();
    REQUIRE(!grid.hasAnyDirty());
    REQUIRE(grid.getFrontBuffer()[0].codepoint == 'H');

    // Test Cursor Repositioning and Clamping
    grid.setCursorPos(50, 10);
    REQUIRE(grid.getCursorX() == 50);
    REQUIRE(grid.getCursorY() == 10);
    grid.setCursorPos(999, 999);
    REQUIRE(grid.getCursorX() == 79);
    REQUIRE(grid.getCursorY() == 23);

    // Test Scrolling & Scrollback
    grid.setCursorPos(0, 23);
    grid.writeString("Bottom line");
    REQUIRE(grid.getScrollbackCount() == 0);

    grid.newline(); // Should scroll up 1 line
    REQUIRE(grid.getScrollbackCount() == 1);
    REQUIRE(grid.getScrollbackRow(0)[0].codepoint == 'H'); // First line evicted into scrollback
    REQUIRE(grid.getCell(0, 22).codepoint == 'B');         // "Bottom line" shifted to row 22

    // Test Erase Display and Line
    grid.setCursorPos(5, 22);
    grid.eraseInLine(0); // Erase to end of line
    REQUIRE(grid.getCell(0, 22).codepoint == 'B');
    REQUIRE(grid.getCell(5, 22).codepoint == ' ');

    grid.eraseInDisplay(2); // Erase whole display
    REQUIRE(grid.getCell(0, 22).codepoint == ' ');
    REQUIRE(grid.getScrollbackCount() == 1); // Preserves scrollback in mode 2

    grid.eraseInDisplay(3); // Erase display + clear scrollback
    REQUIRE(grid.getScrollbackCount() == 0);

    // Test Alternate Screen Buffer Switching
    grid.setCursorPos(0, 0);
    grid.writeString("Primary Screen");
    REQUIRE(grid.dumpRowText(0) == "Primary Screen");

    grid.setAlternateScreen(true);
    REQUIRE(grid.isAlternateScreen());
    REQUIRE(grid.dumpRowText(0) == ""); // Alt screen is blank
    grid.writeString("Alt Screen Mode");
    REQUIRE(grid.dumpRowText(0) == "Alt Screen Mode");

    grid.setAlternateScreen(false);
    REQUIRE(!grid.isAlternateScreen());
    REQUIRE(grid.dumpRowText(0) == "Primary Screen"); // Primary screen preserved!

    std::cout << "  [PASS] TerminalGrid dual-buffering & screen management passed." << std::endl;
}

// ============================================================================
// 3. VT100 / Xterm ANSI Escape Sequence Parser Tests
// ============================================================================
void testAnsiParser() {
    std::cout << "[Test 3/5] VT100/Xterm Zero-Allocation ANSI Byte Stream Parser..." << std::endl;

    TerminalGrid grid(80, 24);
    AnsiParser parser(grid);

    // A: Plain text stream
    parser.parse("Hello Terminal\r\n");
    REQUIRE(grid.dumpRowText(0) == "Hello Terminal");
    REQUIRE(grid.getCursorY() == 1);
    REQUIRE(grid.getCursorX() == 0);

    // B: SGR Colors - 24-bit TrueColor Foreground & Background
    parser.parse("\x1b[38;2;255;120;0m\x1b[48;2;10;20;30mTrueColor\x1b[0m");
    const auto& cell = grid.getCell(0, 1);
    REQUIRE(cell.codepoint == 'T');
    REQUIRE(cell.fg == tui::Color::FromRgb(255, 120, 0));
    REQUIRE(cell.bg == tui::Color::FromRgb(10, 20, 30));

    // C: SGR 256-color palette
    parser.parse("\r\n\x1b[38;5;196mRed256\x1b[0m");
    const auto& c256 = grid.getCell(0, 2);
    REQUIRE(c256.codepoint == 'R');
    REQUIRE(c256.fg == tui::Color::FromRgb(255, 0, 0));

    // D: Text Attributes (Bold, Underline, Reverse, Dim)
    parser.parse("\r\n\x1b[1m\x1b[4mBoldUnderline\x1b[0m");
    const auto& cAttr = grid.getCell(0, 3);
    REQUIRE(cAttr.codepoint == 'B');
    uint8_t expectedAttrs = static_cast<uint8_t>(tui::TextAttr::Bold) |
                            static_cast<uint8_t>(tui::TextAttr::Underline);
    REQUIRE((cAttr.attrs & expectedAttrs) == expectedAttrs);

    // E: Cursor Repositioning (CUP \x1b[row;colH)
    parser.parse("\x1b[15;40H");
    REQUIRE(grid.getCursorY() == 14); // 1-indexed to 0-indexed
    REQUIRE(grid.getCursorX() == 39);

    // Cursor relative moves (CUU, CUD, CUF, CUB)
    parser.parse("\x1b[2A"); // Up 2
    REQUIRE(grid.getCursorY() == 12);
    parser.parse("\x1b[5B"); // Down 5
    REQUIRE(grid.getCursorY() == 17);
    parser.parse("\x1b[10C"); // Forward 10
    REQUIRE(grid.getCursorX() == 49);
    parser.parse("\x1b[4D"); // Back 4
    REQUIRE(grid.getCursorX() == 45);

    // F: Erase in Line / Display
    parser.parse("\x1b[1;1H\x1b[2J"); // Cursor to 1,1 and clear display
    REQUIRE(grid.dumpRowText(0) == "");
    REQUIRE(grid.dumpRowText(1) == "");

    // G: DEC Private Modes (Alternate Screen & Bracketed Paste)
    REQUIRE(!grid.isAlternateScreen());
    parser.parse("\x1b[?1049h");
    REQUIRE(grid.isAlternateScreen());
    parser.parse("\x1b[?1049l");
    REQUIRE(!grid.isAlternateScreen());

    REQUIRE(!grid.isBracketedPaste());
    parser.parse("\x1b[?2004h");
    REQUIRE(grid.isBracketedPaste());
    parser.parse("\x1b[?2004l");
    REQUIRE(!grid.isBracketedPaste());

    // H: OSC Window Title
    parser.parse("\x1b]0;Ghostty Terminal Test\x07");
    REQUIRE(grid.getTitle() == "Ghostty Terminal Test");

    // I: UTF-8 Multibyte Character Stream
    parser.parse("\x1b[1;1H┌───┐\r\n│ E │\r\n└───┘");
    REQUIRE(grid.getCell(0, 0).codepoint == 0x250C); // ┌
    REQUIRE(grid.getCell(1, 0).codepoint == 0x2500); // ─
    REQUIRE(grid.getCell(4, 0).codepoint == 0x2510); // ┐
    REQUIRE(grid.getCell(0, 1).codepoint == 0x2502); // │
    REQUIRE(grid.getCell(2, 1).codepoint == 'E');
    REQUIRE(grid.getCell(0, 2).codepoint == 0x2514); // └
    REQUIRE(grid.getCell(4, 2).codepoint == 0x2518); // ┘

    std::cout << "  [PASS] VT100/Xterm ANSI state machine & UTF-8 streaming passed." << std::endl;
}

// ============================================================================
// 4. Interactive Eatscript REPL Engine Tests
// ============================================================================
void testEatscriptRepl() {
    std::cout << "[Test 4/5] Interactive Eatscript Shell / REPL Engine..." << std::endl;

    abi::HostRegistry hostRegistry;
    ReplEngine repl(&hostRegistry);
    repl.setColorOutput(false); // Plain text for deterministic assertions

    // A: Single-line expression evaluation
    ReplResult res1 = repl.feedLine("x = 40 + 2");
    REQUIRE(res1.status == ReplResult::Status::Complete);
    REQUIRE(res1.resultValue.asInt() == 42);

    ReplResult res2 = repl.feedLine("x * 2");
    REQUIRE(res2.status == ReplResult::Status::Complete);
    REQUIRE(res2.resultValue.asInt() == 84);

    // B: Multi-line function definition continuation
    ReplResult m1 = repl.feedLine("def add(a, b):");
    REQUIRE(m1.status == ReplResult::Status::Incomplete);
    REQUIRE(repl.isIncomplete());
    REQUIRE(repl.getCurrentPrompt() == "... ");

    ReplResult m2 = repl.feedLine("    return a + b");
    REQUIRE(m2.status == ReplResult::Status::Incomplete);

    ReplResult m3 = repl.feedLine(""); // Blank line concludes multi-line block
    REQUIRE(m3.status == ReplResult::Status::Complete);
    REQUIRE(!repl.isIncomplete());
    REQUIRE(repl.getCurrentPrompt() == ">>> ");

    ReplResult m4 = repl.feedLine("add(100, 250)");
    REQUIRE(m4.status == ReplResult::Status::Complete);
    REQUIRE(m4.resultValue.asInt() == 350);

    // C: Auto-completion of keywords, globals & stdlib symbols
    auto kwMatches = repl.complete("de");
    REQUIRE(std::find(kwMatches.begin(), kwMatches.end(), "def") != kwMatches.end());

    auto mathMatches = repl.complete("math.s");
    REQUIRE(std::find(mathMatches.begin(), mathMatches.end(), "math.sin") != mathMatches.end());
    REQUIRE(std::find(mathMatches.begin(), mathMatches.end(), "math.sqrt") != mathMatches.end());

    auto sysMatches = repl.complete("sys.p");
    REQUIRE(std::find(sysMatches.begin(), sysMatches.end(), "sys.platform") != sysMatches.end());

    // D: Structured Value Pretty-Printing
    Value numVal(12345);
    REQUIRE(ReplEngine::prettyPrint(numVal, 0, false) == "12345");

    Value strVal("hello \"world\"");
    REQUIRE(ReplEngine::prettyPrint(strVal, 0, false) == "\"hello \\\"world\\\"\"");

    Value boolVal(true);
    REQUIRE(ReplEngine::prettyPrint(boolVal, 0, false) == "true");

    std::vector<Value> listItems = {Value(1), Value(2), Value(3)};
    Value listVal(listItems);
    REQUIRE(ReplEngine::prettyPrint(listVal, 0, false) == "[1, 2, 3]");

    std::map<std::string, Value> dictItems;
    dictItems["name"] = Value("TB-303");
    dictItems["cutoff"] = Value(440);
    Value dictVal(dictItems);
    std::string dictStr = ReplEngine::prettyPrint(dictVal, 0, false);
    REQUIRE(dictStr.find("\"name\": \"TB-303\"") != std::string::npos);
    REQUIRE(dictStr.find("\"cutoff\": 440") != std::string::npos);

    // E: History Navigation
    REQUIRE(repl.getHistory().size() >= 4);
    std::string prev = repl.historyPrev();
    REQUIRE(prev == "add(100, 250)");

    std::cout << "  [PASS] Eatscript REPL evaluation, multi-line & completion passed." << std::endl;
}

// ============================================================================
// 5. End-to-End Terminal-to-Renderer Pipeline Integration
// ============================================================================
void testTerminalReplPipelineIntegration() {
    std::cout << "[Test 5/5] End-to-End REPL -> ANSI Parser -> Terminal Grid -> WebGPU BatchRenderer..." << std::endl;

    // 1. REPL produces formatted output with ANSI escape codes
    ReplEngine repl;
    repl.setColorOutput(true); // ANSI colors enabled

    ReplResult res = repl.feedLine("{\"track\": \"Acid Bass\", \"tempo\": 140}");
    REQUIRE(res.status == ReplResult::Status::Complete);
    REQUIRE(!res.output.empty());

    // 2. TerminalGrid + AnsiParser ingest the REPL byte stream
    TerminalGrid grid(80, 24);
    AnsiParser parser(grid);

    parser.parse(">>> {\"track\": \"Acid Bass\", \"tempo\": 140}\r\n");
    parser.parse(res.output);
    parser.parse("\r\n>>> ");

    grid.swapBuffers();
    REQUIRE(grid.getFrontBuffer()[0].codepoint == '>');

    // 3. BatchRenderer2D renders the terminal grid into vertex batches
    BatchRenderer2D renderer;
    bool initOk = renderer.initialize(nullptr, 640, 384, RenderBackendType::Filament);
    REQUIRE(initOk);

    renderer.beginFrame(640.0f, 384.0f);
    size_t preVerts = renderer.getVertexCount();

    renderer.drawTerminalGrid(0.0f, 0.0f, 8.0f, 16.0f, grid.getFrontBuffer(), 80, 24, 0.0f, 0.0f);
    REQUIRE(renderer.getVertexCount() > preVerts);

    renderer.endFrame();
    renderer.shutdown();

    std::cout << "  [PASS] End-to-end REPL -> Terminal -> BatchRenderer pipeline passed." << std::endl;
}

int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << " Running Eatsbits Phase 3: WebGPU Terminal & Eatscript Shell" << std::endl;
    std::cout << "============================================================" << std::endl;

    testBatchRendererMonospacePipeline();
    testTerminalGridBuffer();
    testAnsiParser();
    testEatscriptRepl();
    testTerminalReplPipelineIntegration();

    std::cout << "============================================================" << std::endl;
    std::cout << " All Phase 3 WebGPU Terminal & REPL Unit Tests PASSED!" << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}
