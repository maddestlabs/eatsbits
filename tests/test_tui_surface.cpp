#include "eatsbits/tui/cell_surface.hpp"
#include "eatsbits/tui/ansi_diff_renderer.hpp"
#include "eatsbits/tui/input_parser.hpp"
#include <iostream>
#include <cassert>

using namespace eatsbits::tui;

void testCellSurfaceBasics() {
    std::cout << "[Test] Running testCellSurfaceBasics..." << std::endl;
    CellSurface surface(80, 24);
    assert(surface.getWidth() == 80);
    assert(surface.getHeight() == 24);
    assert(surface.getBrailleWidth() == 160);
    assert(surface.getBrailleHeight() == 96);
    assert(surface.hasAnyDirty());

    surface.swapBuffers();
    assert(!surface.hasAnyDirty());
    assert(!surface.isRowDirty(5));

    // Modifying a cell marks only that row dirty
    surface.setCell(10, 5, Cell{'X', Color::White(), Color::Black(), 0});
    assert(surface.hasAnyDirty());
    assert(surface.isRowDirty(5));
    assert(!surface.isRowDirty(4));
    assert(!surface.isRowDirty(6));

    const Cell& c = surface.getCell(10, 5);
    assert(c.codepoint == 'X');
    assert(c.fg == Color::White());
    assert(c.bg == Color::Black());
    std::cout << "  [PASS] CellSurface basics and dirty tracking passed." << std::endl;
}

void testBrailleSubpixels() {
    std::cout << "[Test] Running testBrailleSubpixels..." << std::endl;
    CellSurface surface(40, 20);
    surface.swapBuffers();

    // Dot (0, 0) in cell (0, 0) -> mask 0x01 -> Unicode 0x2801
    surface.setBrailleDot(0, 0, true);
    Cell c1 = surface.getCell(0, 0);
    assert(c1.codepoint == 0x2801);

    // Dot (1, 3) in cell (0, 0) -> mask 0x80 -> Unicode 0x2881
    surface.setBrailleDot(1, 3, true);
    Cell c2 = surface.getCell(0, 0);
    assert(c2.codepoint == 0x2881);

    // Dot in cell (1, 2) -> subX = 2, subY = 8
    // subX % 2 = 0, subY % 4 = 0 -> dot 1 (0x01)
    surface.setBrailleDot(2, 8, true);
    Cell c3 = surface.getCell(1, 2);
    assert(c3.codepoint == 0x2801);

    std::cout << "  [PASS] Braille subpixel dot mapping passed." << std::endl;
}

void testAnsiDiffRendering() {
    std::cout << "[Test] Running testAnsiDiffRendering..." << std::endl;
    CellSurface surface(80, 24);
    AnsiDiffRenderer renderer;

    std::string out;
    // Initial diff should render all dirty cells
    size_t count = renderer.renderDiff(surface, out, false);
    assert(count > 0);
    assert(!out.empty());

    // Second diff with no modifications should produce 0 output
    out.clear();
    size_t zeroCount = renderer.renderDiff(surface, out, false);
    assert(zeroCount == 0);
    assert(out.empty());

    // Modify single cell
    surface.setCell(5, 12, Cell{'Z', Color::AcidAmber(), Color::Black(), static_cast<uint8_t>(TextAttr::Bold)});
    out.clear();
    size_t modCount = renderer.renderDiff(surface, out, false);
    assert(modCount == 1);
    assert(!out.empty());
    // Cursor move to row 13, col 6 (\x1b[13;6H)
    assert(out.find("\x1b[13;6H") != std::string::npos);
    // TrueColor for AcidAmber (r=255, g=119, b=0)
    assert(out.find("255;119;0") != std::string::npos);

    std::cout << "  [PASS] ANSI diff renderer and escape sequence minimization passed." << std::endl;
}

void testInputParser() {
    std::cout << "[Test] Running testInputParser..." << std::endl;
    InputParser parser;

    // Arrow keys
    parser.feed("\x1b[A\x1b[B\x1b[C\x1b[D");
    KeyEvent k1, k2, k3, k4;
    assert(parser.pollKey(k1) && k1.code == KeyCode::Up);
    assert(parser.pollKey(k2) && k2.code == KeyCode::Down);
    assert(parser.pollKey(k3) && k3.code == KeyCode::Right);
    assert(parser.pollKey(k4) && k4.code == KeyCode::Left);

    // Carriage return, tab, space
    parser.feed("\r\t ");
    KeyEvent kEnter, kTab, kSpace;
    assert(parser.pollKey(kEnter) && kEnter.code == KeyCode::Enter);
    assert(parser.pollKey(kTab) && kTab.code == KeyCode::Tab);
    assert(parser.pollKey(kSpace) && kSpace.code == KeyCode::Space);

    // SGR Mouse press at col 15, row 25 (1-indexed: \x1b[<0;15;25M)
    parser.feed("\x1b[<0;15;25M");
    MouseEvent me;
    assert(parser.pollMouse(me));
    assert(me.x == 14);
    assert(me.y == 24);
    assert(me.button == 0);
    assert(!me.isRelease);

    std::cout << "  [PASS] Input parser keyboard and mouse sequence parsing passed." << std::endl;
}

#include "eatsbits/tui/views/arranger_view.hpp"

void testArrangerViewBasics() {
    std::cout << "[Test] Running testArrangerViewBasics..." << std::endl;
    ArrangerView arranger;
    assert(arranger.getTracks().size() >= 4);
    assert(arranger.getTracks()[0].name == "Eats-909");
    assert(arranger.getTracks()[1].name == "Eats-303");

    // Braille pan dial mapping
    assert(ArrangerView::getBraillePanGlyph(0.1f) == "⢎⡱");
    assert(ArrangerView::getBraillePanGlyph(0.5f) == "⠸⠇");
    assert(ArrangerView::getBraillePanGlyph(0.9f) == "⢹⡇");

    // Inspector toggle
    assert(arranger.isInspectorOpen());
    arranger.toggleInspector();
    assert(!arranger.isInspectorOpen());
    arranger.toggleInspector();
    assert(arranger.isInspectorOpen());

    // Render test
    CellSurface surface(120, 36);
    TelemetrySnapshot telemetry{};
    arranger.render(surface, {0, 0, 120, 36}, telemetry, true);
    assert(surface.hasAnyDirty());

    std::cout << "  [PASS] ArrangerView basics, Braille dials, and clip rendering passed." << std::endl;
}

int main() {
    std::cout << "=== Eatsbits TUI Engine Automated Verification ===" << std::endl;
    testCellSurfaceBasics();
    testBrailleSubpixels();
    testAnsiDiffRendering();
    testInputParser();
    testArrangerViewBasics();
    std::cout << "All TUI engine verification tests passed successfully!" << std::endl;
    return 0;
}
