#include "eatsbits/ui/geometry.hpp"
#include "eatsbits/ui/widgets/skeuomorphic_knob.hpp"
#include "eatsbits/ui/widgets/skeuomorphic_slider.hpp"
#include "eatsbits/ui/widgets/skeuomorphic_switch.hpp"
#include "eatsbits/ui/widgets/glowing_nixie.hpp"
#include "eatsbits/ui/widgets/vu_meter.hpp"
#include "eatsbits/ui/widgets/scrollable_area.hpp"
#include "eatsbits/ui/widgets/drum_pad_grid_widget.hpp"
#include "eatsbits/ui/input/pointer_event.hpp"

#include <iostream>
#include <cassert>
#include <cmath>

using namespace eatsbits::ui;

static void assertNear(float actual, float expected, float eps = 0.01f, const char* msg = "") {
    if (std::abs(actual - expected) > eps) {
        std::cerr << "Assertion failed: " << msg << " (actual: " << actual << ", expected: " << expected << ")" << std::endl;
        std::abort();
    }
}

void testRectGeometry() {
    std::cout << "[Test] Rect geometry and layout slicing..." << std::endl;

    Rect r(10.0f, 20.0f, 100.0f, 200.0f);
    assert(r.left() == 10.0f);
    assert(r.top() == 20.0f);
    assert(r.right() == 110.0f);
    assert(r.bottom() == 220.0f);
    assert(r.center().x == 60.0f);
    assert(r.center().y == 120.0f);

    // Hit testing
    assert(r.contains(10.0f, 20.0f));
    assert(r.contains(60.0f, 120.0f));
    assert(r.contains(110.0f, 220.0f));
    assert(!r.contains(9.9f, 20.0f));
    assert(!r.contains(60.0f, 220.1f));

    // Inset
    Rect insetR = r.inset(5.0f);
    assert(insetR.x == 15.0f);
    assert(insetR.y == 25.0f);
    assert(insetR.w == 90.0f);
    assert(insetR.h == 190.0f);

    // Slicing operations
    Rect base(0.0f, 0.0f, 200.0f, 100.0f);
    Rect topBar = base.cutTop(30.0f);
    assert(topBar.x == 0.0f && topBar.y == 0.0f && topBar.w == 200.0f && topBar.h == 30.0f);
    assert(base.y == 30.0f && base.h == 70.0f);

    Rect leftNav = base.cutLeft(40.0f);
    assert(leftNav.x == 0.0f && leftNav.y == 30.0f && leftNav.w == 40.0f && leftNav.h == 70.0f);
    assert(base.x == 40.0f && base.w == 160.0f);

    // Subdividing
    Rect row(0.0f, 0.0f, 300.0f, 50.0f);
    auto cells = row.splitHorizontal(3, 10.0f);
    assert(cells.size() == 3);
    // (300 - 20) / 3 = 280 / 3 = 93.333
    assertNear(cells[0].w, 93.33f, 0.1f);
    assertNear(cells[1].x, 103.33f, 0.1f);

    // LayoutBox sequential layout
    LayoutBox layout(Rect(0.0f, 0.0f, 100.0f, 500.0f), LayoutBox::Direction::Vertical, 10.0f);
    Rect item1 = layout.next(50.0f);
    assert(item1.y == 0.0f && item1.h == 50.0f);
    Rect item2 = layout.next(50.0f);
    assert(item2.y == 60.0f && item2.h == 50.0f);

    std::cout << "  [PASS] Rect geometry and layout slicing tests passed." << std::endl;
}

void testSkeuomorphicKnob() {
    std::cout << "[Test] SkeuomorphicKnob interaction & formatting..." << std::endl;

    Rect bounds(50.0f, 50.0f, 60.0f, 60.0f);
    SkeuomorphicKnob knob(bounds, "CUTOFF", 0.5f, 20.0f, 20000.0f, "Hz");

    assert(knob.hitTest(60.0f, 60.0f));
    assert(!knob.hitTest(120.0f, 120.0f));

    // Dragging up (deltaY < 0) should increase value
    knob.handleDrag(-20.0f, false);
    assert(knob.normValue > 0.5f);

    // Dragging down (deltaY > 0) should decrease value
    knob.handleDrag(40.0f, false);
    assert(knob.normValue < 0.5f);

    // Shift drag (fine tune) should move 5x slower
    float beforeShift = knob.normValue;
    knob.handleDrag(10.0f, true);
    float shiftDelta = std::abs(knob.normValue - beforeShift);

    knob.normValue = beforeShift;
    knob.handleDrag(10.0f, false);
    float normalDelta = std::abs(knob.normValue - beforeShift);
    assertNear(normalDelta / shiftDelta, 5.0f, 0.1f);

    // Angle calculation: 0.5 norm should be 0 deg (12 o'clock)
    knob.normValue = 0.5f;
    assertNear(knob.getPointerAngleDegrees(), 0.0f, 0.1f);

    knob.normValue = 0.0f;
    assertNear(knob.getPointerAngleDegrees(), -135.0f, 0.1f);

    knob.normValue = 1.0f;
    assertNear(knob.getPointerAngleDegrees(), 135.0f, 0.1f);

    // Value formatting
    knob.normValue = 0.5f;
    std::string valStr = knob.getFormattedValue();
    assert(valStr.find("Hz") != std::string::npos);

    std::cout << "  [PASS] SkeuomorphicKnob tests passed." << std::endl;
}

void testSkeuomorphicSlider() {
    std::cout << "[Test] SkeuomorphicSlider interaction & dB mapping..." << std::endl;

    Rect bounds(100.0f, 100.0f, 40.0f, 200.0f);
    SkeuomorphicSlider fader(bounds, "MASTER", 1.0f); // 1.0 = unity gain (0 dB)

    assert(fader.hitTest(110.0f, 150.0f));

    Rect capBounds = fader.getFaderCapBounds();
    assert(capBounds.w == fader.capWidth);
    assert(capBounds.h == fader.capHeight);
    assert(fader.hitTestCap(capBounds.x + 5.0f, capBounds.y + 5.0f));

    // Formatting unity gain (1.0 = 0.0 dB)
    std::string dbStr = fader.getFormattedDb();
    assert(dbStr.find("0.0 dB") != std::string::npos);

    // Dragging mouse to top of fader
    fader.handleDragY(bounds.y);
    assertNear(fader.normValue, 1.5f, 0.02f);
    assert(fader.getFormattedDb().find("+") != std::string::npos);

    // Dragging mouse to bottom of fader
    fader.handleDragY(bounds.bottom());
    assertNear(fader.normValue, 0.0f, 0.02f);
    assert(fader.getFormattedDb().find("-INF") != std::string::npos);

    std::cout << "  [PASS] SkeuomorphicSlider tests passed." << std::endl;
}

void testSkeuomorphicSwitch() {
    std::cout << "[Test] SkeuomorphicSwitch states & toggling..." << std::endl;

    Rect bounds(200.0f, 100.0f, 60.0f, 40.0f);
    SkeuomorphicSwitch toggle(bounds, "FILTER", {"LP", "BP", "HP"}, 0);

    assert(toggle.state == 0);
    assert(toggle.getActiveLabel() == "LP");

    toggle.toggle();
    assert(toggle.state == 1);
    assert(toggle.getActiveLabel() == "BP");

    toggle.toggle();
    assert(toggle.state == 2);
    assert(toggle.getActiveLabel() == "HP");

    toggle.toggle();
    assert(toggle.state == 0);
    assert(toggle.getActiveLabel() == "LP");

    std::cout << "  [PASS] SkeuomorphicSwitch tests passed." << std::endl;
}

void testNixieAndVuMeter() {
    std::cout << "[Test] GlowingNixie and VuMeter..." << std::endl;

    GlowingNixie nixie(Rect(10.0f, 10.0f, 100.0f, 40.0f), "TEMPO", "128 BPM");
    assert(nixie.hitTest(15.0f, 15.0f));
    assert(nixie.title == "TEMPO");
    assert(nixie.valueText == "128 BPM");

    VuMeter meter(Rect(200.0f, 10.0f, 30.0f, 150.0f));
    meter.update(0.7f, 0.8f);
    assert(meter.peakLeft == 0.7f);
    assert(meter.peakRight == 0.8f);
    assert(!meter.clipLeft && !meter.clipRight);

    // Test clipping trigger
    meter.update(1.05f, 0.5f);
    assert(meter.clipLeft);
    assert(!meter.clipRight);

    meter.resetClip();
    assert(!meter.clipLeft);

    std::cout << "  [PASS] GlowingNixie and VuMeter tests passed." << std::endl;
}

void testContextualLightingAndTb303() {
    std::cout << "[Test] Contextual LightSource2D and TB-303 Knob Geometry..." << std::endl;

    LightSource2D light(640.0f, -180.0f, 500.0f, 0.88f);

    // Center element (x=640): shadow should fall purely downward (dx = 0, dy > 0)
    ShadowOffset centerShadow = light.computeShadowOffset(640.0f, 300.0f, 10.0f);
    assertNear(centerShadow.dx, 0.0f, 0.001f, "Center shadow dx should be zero");
    assert(centerShadow.dy > 0.0f); // Shadows cast down from top light
    assertNear(centerShadow.blur, 4.0f, 0.01f);
    assert(centerShadow.opacity > 0.35f);

    // Left element (x=200): shadow must diverge to the left (dx < 0)
    ShadowOffset leftShadow = light.computeShadowOffset(200.0f, 300.0f, 10.0f);
    assert(leftShadow.dx < 0.0f);
    assert(leftShadow.dy > 0.0f);

    // Right element (x=1080): shadow must diverge to the right (dx > 0)
    ShadowOffset rightShadow = light.computeShadowOffset(1080.0f, 300.0f, 10.0f);
    assert(rightShadow.dx > 0.0f);
    assert(rightShadow.dy > 0.0f);
    assertNear(std::abs(leftShadow.dx), rightShadow.dx, 0.01f, "Symmetric divergence across light center");

    // Height elevation scaling: doubling elevation should double shadow displacement
    ShadowOffset tallShadow = light.computeShadowOffset(200.0f, 300.0f, 20.0f);
    assertNear(tallShadow.dx, leftShadow.dx * 2.0f, 0.01f, "Elevation scaling dx");
    assertNear(tallShadow.dy, leftShadow.dy * 2.0f, 0.01f, "Elevation scaling dy");

    // Light direction vector
    Point lDir = light.getLightDirection(640.0f, 300.0f);
    assertNear(lDir.x, 0.0f, 0.001f);
    assertNear(lDir.y, -1.0f, 0.001f, "Directly above light points -Y in screen coords");

    // TB-303 Knob style & angle ranges
    Rect kBounds(100.0f, 100.0f, 48.0f, 48.0f);
    SkeuomorphicKnob knob303(kBounds, "CUTOFF", 0.5f, 200.0f, 4500.0f, "Hz", KnobStyle::Tb303Potentiometer);
    assert(knob303.style == KnobStyle::Tb303Potentiometer);

    // Radians angle checks: -135 deg to +135 deg
    assertNear(knob303.getPointerAngleDegrees(), 0.0f, 0.01f, "Midpoint degree");
    assertNear(knob303.getPointerAngleRadians(), 0.0f, 0.01f, "Midpoint radian");

    knob303.normValue = 0.0f;
    assertNear(knob303.getPointerAngleDegrees(), -135.0f, 0.01f, "Min angle degree");
    assertNear(knob303.getPointerAngleRadians(), -2.35619449f, 0.01f, "Min angle radian");

    knob303.normValue = 1.0f;
    assertNear(knob303.getPointerAngleDegrees(), 135.0f, 0.01f, "Max angle degree");
    assertNear(knob303.getPointerAngleRadians(), 2.35619449f, 0.01f, "Max angle radian");

    // Tooth normal N.L lighting test: tooth pointing up towards light should have maximum alignment
    float toothAngleUp = -1.5707963f; // pointing directly up in screen space (towards top light)
    float nx = std::cos(toothAngleUp);
    float ny = std::sin(toothAngleUp);
    float dotUp = nx * lDir.x + ny * lDir.y;
    assertNear(dotUp, 1.0f, 0.01f, "Upward tooth facing overhead light should have N.L = 1.0");

    // Tooth pointing down should face away from light (dot < 0)
    float toothAngleDown = 1.5707963f;
    float dotDown = std::cos(toothAngleDown) * lDir.x + std::sin(toothAngleDown) * lDir.y;
    assertNear(dotDown, -1.0f, 0.01f, "Downward tooth facing away should have N.L = -1.0");

    std::cout << "  [PASS] Contextual LightSource2D and TB-303 tests passed." << std::endl;
}

void testScrollableArea() {
    std::cout << "[Test] ScrollableArea viewport and scrollbar math..." << std::endl;

    ScrollableArea area;
    area.setViewport(10.0f, 50.0f, 400.0f, 300.0f);
    area.setContentHeight(600.0f);

    assert(area.canScroll());
    assertNear(area.getMaxScroll(), 300.0f, 0.01f, "Max scroll should be 600 - 300 = 300");
    assertNear(area.getScrollY(), 0.0f, 0.01f, "Initial scroll should be 0");

    // Scroll operations
    area.scrollBy(100.0f);
    assertNear(area.getScrollY(), 100.0f, 0.01f, "ScrollBy 100");
    assertNear(area.getScrollRatio(), 1.0f / 3.0f, 0.01f, "Scroll ratio 100/300");

    // Coordinate conversions
    float screenY = area.contentToScreenY(150.0f);
    assertNear(screenY, 100.0f, 0.01f, "50 (vp.y) + 150 - 100 = 100");
    float contentY = area.screenToContentY(100.0f);
    assertNear(contentY, 150.0f, 0.01f, "100 - 50 + 100 = 150");

    // Visibility test
    assert(area.isVisible(60.0f, 20.0f));
    assert(area.isVisible(340.0f, 20.0f));
    assert(!area.isVisible(400.0f, 20.0f)); // below 50 + 300 = 350
    assert(!area.isVisible(10.0f, 20.0f));  // above 50

    // Geometry of track and thumb
    Rect2D track = area.getScrollbarTrackBounds();
    assertNear(track.w, 5.0f, 0.01f, "Track width");
    assertNear(track.h, 300.0f, 0.01f, "Track height matches viewport height");

    Rect2D thumb = area.getScrollbarThumbBounds();
    assertNear(thumb.h, 150.0f, 0.01f, "Thumb height should be 300/600 * 300 = 150");
    assertNear(thumb.y, 50.0f + (1.0f / 3.0f) * (300.0f - 150.0f), 0.5f, "Thumb position");

    // Pointer events
    PointerEvent scrollEv;
    scrollEv.action = PointerAction::Scroll;
    scrollEv.x = 200.0f;
    scrollEv.y = 100.0f;
    scrollEv.scrollY = 1.0f;
    bool scrollHandled = area.handlePointer(scrollEv);
    assert(scrollHandled);
    assertNear(area.getScrollY(), 68.0f, 0.01f, "Scrolled up by 32 from 100");

    // Thumb dragging
    PointerEvent downEv;
    downEv.action = PointerAction::Down;
    downEv.button = PointerButton::Left;
    downEv.x = thumb.x + 2.0f;
    downEv.y = thumb.y + 10.0f;
    bool downHandled = area.handlePointer(downEv);
    assert(downHandled);
    assert(area.isDragging());

    PointerEvent moveEv;
    moveEv.action = PointerAction::Move;
    moveEv.x = thumb.x + 2.0f;
    moveEv.y = downEv.y + 30.0f;
    bool moveHandled = area.handlePointer(moveEv);
    assert(moveHandled);

    PointerEvent upEv;
    upEv.action = PointerAction::Up;
    bool upHandled = area.handlePointer(upEv);
    assert(upHandled);
    assert(!area.isDragging());

    std::cout << "  [PASS] ScrollableArea tests passed." << std::endl;
}

void testMultiTouchAndKineticTouch() {
    std::cout << "[Test] Multi-touch polyphony and kinetic momentum scroller..." << std::endl;

    // 1. Test KineticScroller
    KineticScroller scroller;
    assert(!scroller.isGliding());
    scroller.addSample(100.0f, 200.0f, 0.0);
    scroller.addSample(100.0f, 150.0f, 50.0);
    scroller.addSample(100.0f, 100.0f, 100.0);
    scroller.endDrag(100.0);
    assert(scroller.isGliding());

    float dx = 0.0f, dy = 0.0f;
    scroller.step(0.016f, dx, dy);
    assert(dy != 0.0f);
    assert(std::abs(dy) > 0.01f);

    // 2. Test DrumPadGridWidget multi-touch polyphony
    DrumPadGridWidget drumGrid;
    drumGrid.layout(Rect2D{0.0f, 0.0f, 320.0f, 240.0f});

    std::vector<uint8_t> triggeredNotes;
    std::vector<uint8_t> releasedNotes;
    drumGrid.onPadTrigger = [&](uint8_t note, float [[maybe_unused]] vel) {
        triggeredNotes.push_back(note);
    };
    drumGrid.onPadRelease = [&](uint8_t note) {
        releasedNotes.push_back(note);
    };

    const auto& pads = drumGrid.getActivePads();
    assert(pads.size() >= 4);

    // Finger 1 touches Pad 0 (id = 1)
    PointerEvent touch1;
    touch1.id = 1;
    touch1.type = PointerType::Touch;
    touch1.action = PointerAction::Down;
    touch1.x = pads[0].bounds.center().x;
    touch1.y = pads[0].bounds.center().y;
    touch1.pressure = 0.90f;
    drumGrid.handlePointer(touch1);

    // Finger 2 touches Pad 1 (id = 2) simultaneously!
    PointerEvent touch2;
    touch2.id = 2;
    touch2.type = PointerType::Touch;
    touch2.action = PointerAction::Down;
    touch2.x = pads[1].bounds.center().x;
    touch2.y = pads[1].bounds.center().y;
    touch2.pressure = 0.80f;
    drumGrid.handlePointer(touch2);

    assert(triggeredNotes.size() == 2);
    assert(triggeredNotes[0] == pads[0].note);
    assert(triggeredNotes[1] == pads[1].note);

    // Release Finger 1
    touch1.action = PointerAction::Up;
    drumGrid.handlePointer(touch1);
    assert(releasedNotes.size() == 1);
    assert(releasedNotes[0] == pads[0].note);

    // Release Finger 2
    touch2.action = PointerAction::Up;
    drumGrid.handlePointer(touch2);
    assert(releasedNotes.size() == 2);
    assert(releasedNotes[1] == pads[1].note);

    std::cout << "  [PASS] Multi-touch and kinetic momentum tests passed." << std::endl;
}

int main() {
    std::cout << "=== Running Eatsbits UI Geometry & Widgets Tests ===" << std::endl;
    testRectGeometry();
    testSkeuomorphicKnob();
    testSkeuomorphicSlider();
    testSkeuomorphicSwitch();
    testNixieAndVuMeter();
    testContextualLightingAndTb303();
    testScrollableArea();
    testMultiTouchAndKineticTouch();
    std::cout << "=== All UI Geometry & Widgets Tests Passed! ===" << std::endl;
    return 0;
}
