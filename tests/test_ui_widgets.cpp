#include "eatsbits/ui/geometry.hpp"
#include "eatsbits/ui/widgets/skeuomorphic_knob.hpp"
#include "eatsbits/ui/widgets/skeuomorphic_slider.hpp"
#include "eatsbits/ui/widgets/skeuomorphic_switch.hpp"
#include "eatsbits/ui/widgets/glowing_nixie.hpp"
#include "eatsbits/ui/widgets/vu_meter.hpp"
#include "eatsbits/ui/widgets/scrollable_area.hpp"
#include "eatsbits/ui/widgets/drum_pad_grid_widget.hpp"
#include "eatsbits/ui/input/pointer_event.hpp"
#include "eatsbits/ui/element2d.hpp"
#include "eatsbits/ui/views/view_base.hpp"
#include "eatsbits/ui/gui_panel_def.hpp"
#include "eatsbits/ui/widgets/track_properties_panel.hpp"

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

    // Shift drag (fine tune) should move 10x slower (0.1x standard sensitivity)
    float beforeShift = knob.normValue;
    knob.handleDrag(10.0f, true);
    float shiftDelta = std::abs(knob.normValue - beforeShift);

    knob.normValue = beforeShift;
    knob.handleDrag(10.0f, false);
    float normalDelta = std::abs(knob.normValue - beforeShift);
    assertNear(normalDelta / shiftDelta, 10.0f, 0.1f);

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

    // Stepped Knob tests (e.g. Eats 303 Octave 0..10)
    SkeuomorphicKnob steppedKnob(kBounds, "OCTAVE", 0.5f, 0.0f, 10.0f, "", KnobStyle::Tb303Selector, 1.0f);
    assert(steppedKnob.step == 1.0f);
    assertNear(steppedKnob.getDisplayValue(), 5.0f, 0.01f, "Initial snapped display value");
    assert(steppedKnob.getFormattedValue() == "5");

    // Drag stepped knob: drag upward to increase
    steppedKnob.handleDrag(-30.0f); // Upward drag
    assert(steppedKnob.getDisplayValue() >= 5.0f);
    // Display value must always be integer multiple of step
    float disp = steppedKnob.getDisplayValue();
    assertNear(disp, std::round(disp), 0.001f, "Display value must snap to exact integer step");

    // TB-303 Potentiometer authentic hardware style: 48 serrated flutes and DeclinedScoop cap
    KnobShapeDef tb303Shape = getKnobShapeDef(GuiKnobStyle::Tb303Potentiometer);
    assert(tb303Shape.capStyle == KnobCapStyle::DeclinedScoop);
    assert(tb303Shape.flutes == 48);
    assert(tb303Shape.hasSkirt == true);
    assert(tb303Shape.indicator == KnobIndicatorType::None); // Integrated scoop notch
    assert(tb303Shape.sunkenSlotWidth == 1.8f);
    assert(tb303Shape.hasBlockCenterDetent == true);
    assert(tb303Shape.dialLabels.size() == 11);
    assert(tb303Shape.dialLabels.front() == "0");
    assert(tb303Shape.dialLabels.back() == "10");

    // Configurable knob custom shape: flutes, skirt, sunken slot (0.0 disables), and semantic string array
    KnobShapeDef customFluted;
    customFluted.flutes = 20;
    customFluted.fluteDepth = 0.16f;
    customFluted.skirtRatio = 1.25f;
    customFluted.hasSkirt = true;
    customFluted.sunkenSlotWidth = 0.0f; // 0 = unrendered
    customFluted.dialLabels = {"low", "mid", "high"};
    assert(customFluted.flutes == 20);
    assert(customFluted.sunkenSlotWidth == 0.0f);
    assert(customFluted.dialLabels.size() == 3);
    assert(customFluted.dialLabels[1] == "mid");

    // Slide switch with waveform symbols configuration
    GuiWidgetDef waveSwitch;
    waveSwitch.id = "w_waveform";
    waveSwitch.type = GuiWidgetType::ToggleSwitch;
    waveSwitch.switchStyle = SwitchStyle::RectangularSlide;
    waveSwitch.switchOrientation = SwitchOrientation::Vertical;
    waveSwitch.switchSymbolTop = SwitchSymbol::SquareWave;
    waveSwitch.switchSymbolBottom = SwitchSymbol::SawWave;
    assert(waveSwitch.switchStyle == SwitchStyle::RectangularSlide);
    assert(waveSwitch.switchOrientation == SwitchOrientation::Vertical);
    assert(waveSwitch.switchSymbolTop == SwitchSymbol::SquareWave);
    assert(waveSwitch.switchSymbolBottom == SwitchSymbol::SawWave);

    // BatchRenderer2D drawDeclinedScoopGradient zero-allocation test
    BatchRenderer2D renderer;
    size_t initialVerts = renderer.getVertexCount();
    renderer.drawDeclinedScoopGradient(100.0f, 100.0f, 20.0f, 0.0f, -0.12f,
                                       1.0f, 1.0f, 1.0f, 1.0f,
                                       0.8f, 0.8f, 0.8f, 1.0f,
                                       0.3f, 0.3f, 0.3f, 1.0f,
                                       0.45f, 24);
    assert(renderer.getVertexCount() > initialVerts);

    // Full widget rendering test for Tb303Potentiometer with dynamic lighting across angles
    GuiWidgetDef tb303Widget;
    tb303Widget.knobStyle = GuiKnobStyle::Tb303Potentiometer;
    tb303Widget.currentVal = 0.85f; // Similar to reference image (~4:30 pointing down-right)
    size_t vertsBeforeKnob = renderer.getVertexCount();
    drawKnobWidget(renderer, 200.0f, 200.0f, 24.0f, 0.85f, tb303Widget, light);
    assert(renderer.getVertexCount() > vertsBeforeKnob);

    // Test opposite angle (pointing up-left towards overhead light)
    tb303Widget.currentVal = 0.15f;
    size_t vertsBeforeOpposite = renderer.getVertexCount();
    drawKnobWidget(renderer, 200.0f, 200.0f, 24.0f, 0.15f, tb303Widget, light);
    assert(renderer.getVertexCount() > vertsBeforeOpposite);

    // Test popout / detached window localized lighting isolation:
    // When main window has a swaying CRT light offset (e.g. x=950), popout window must center its own light (e.g. x=200 for 400px window)
    LightSource2D swayingCrtLight(950.0f, -80.0f, 480.0f, 0.90f);
    float popoutW = 400.0f;
    ThemeTokens popoutTheme;
    popoutTheme.globalLight = LightSource2D(popoutW * 0.5f, -180.0f, 500.0f, swayingCrtLight.intensity);
    assertNear(popoutTheme.globalLight.x, 200.0f, 0.01f, "Popout window light must be centered at curW * 0.5");
    assertNear(popoutTheme.globalLight.y, -180.0f, 0.01f, "Popout window light Y must remain at stable elevation");
    assert(popoutTheme.globalLight.x != swayingCrtLight.x);

    size_t vertsBeforePopout = renderer.getVertexCount();
    drawKnobWidget(renderer, 200.0f, 150.0f, 22.0f, 0.5f, tb303Widget, popoutTheme.globalLight);
    assert(renderer.getVertexCount() > vertsBeforePopout);

    std::cout << "  [PASS] Contextual LightSource2D, scoop gradient, and TB-303 tests passed." << std::endl;
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

void testElement2D() {
    std::cout << "[Test] Element2D hierarchy, coordinate transforms, hit-testing & event bubbling..." << std::endl;

    // 1. Hierarchy creation & parent-child links
    auto root = std::make_shared<Element2D>("root");
    root->setBounds(0.0f, 0.0f, 1000.0f, 800.0f);

    auto container = std::make_shared<Element2D>("container");
    container->setBounds(50.0f, 100.0f, 400.0f, 300.0f);
    root->addChild(container);

    assert(container->getParent() == root.get());
    assert(root->getChildCount() == 1);
    assert(root->findById("container") == container.get());

    auto btn1 = std::make_shared<Element2D>("btn1");
    btn1->setBounds(20.0f, 30.0f, 80.0f, 40.0f);
    container->addChild(btn1);

    auto btn2 = std::make_shared<Element2D>("btn2");
    btn2->setBounds(60.0f, 50.0f, 80.0f, 40.0f);
    container->addChild(btn2);

    assert(btn1->getParent() == container.get());
    assert(btn2->getParent() == container.get());
    assert(container->getChildCount() == 2);
    assert(root->findById("btn2") == btn2.get());

    // 2. Coordinate transformations
    // btn1 local (0, 0) -> global: container(50, 100) + btn1(20, 30) = (70, 130)
    Point gPt = btn1->localToGlobal(Point{0.0f, 0.0f});
    assertNear(gPt.x, 70.0f, 1e-4f, "btn1 global origin X");
    assertNear(gPt.y, 130.0f, 1e-4f, "btn1 global origin Y");

    Point lPt = btn1->globalToLocal(Point{70.0f, 130.0f});
    assertNear(lPt.x, 0.0f, 1e-4f, "btn1 local origin X");
    assertNear(lPt.y, 0.0f, 1e-4f, "btn1 local origin Y");

    Rect2D btn1Gb = btn1->getGlobalBounds();
    assertNear(btn1Gb.x, 70.0f, 1e-4f, "btn1 global bounds X");
    assertNear(btn1Gb.y, 130.0f, 1e-4f, "btn1 global bounds Y");
    assertNear(btn1Gb.w, 80.0f, 1e-4f, "btn1 global bounds W");
    assertNear(btn1Gb.h, 40.0f, 1e-4f, "btn1 global bounds H");

    // 3. Clipping bounds
    container->setClipping(true);
    assert(container->isClippingEnabled());
    Rect2D clipRect = container->getGlobalClipRect();
    assertNear(clipRect.x, 50.0f, 1e-4f);
    assertNear(clipRect.y, 100.0f, 1e-4f);
    assertNear(clipRect.w, 400.0f, 1e-4f);
    assertNear(clipRect.h, 300.0f, 1e-4f);

    // 4. Hit-testing with reverse-z child ordering & overlap
    // btn1 global: [70, 130, 80, 40] -> X in [70, 150], Y in [130, 170]
    // btn2 global: container(50, 100) + (60, 50) = [110, 150, 80, 40] -> X in [110, 190], Y in [150, 190]
    // Overlap region: X in [110, 150], Y in [150, 170]
    // Since btn2 was added after btn1, clicking at (120, 160) MUST hit btn2
    Element2D* hitTarget = root->hitTest(120.0f, 160.0f);
    assert(hitTarget == btn2.get());

    // Clicking at (80, 140) is only inside btn1
    hitTarget = root->hitTest(80.0f, 140.0f);
    assert(hitTarget == btn1.get());

    // Clicking at (200, 200) is inside container, outside buttons
    hitTarget = root->hitTest(200.0f, 200.0f);
    assert(hitTarget == container.get());

    // Hiding btn2 makes hit-test fall through to btn1 in overlap region
    btn2->setVisible(false);
    hitTarget = root->hitTest(120.0f, 160.0f);
    assert(hitTarget == btn1.get());
    btn2->setVisible(true);

    // 5. Event dispatching & bubbling
    class TestClickableElement : public Element2D {
    public:
        using Element2D::Element2D;
        bool clicked{false};
        bool handlePointer(const PointerEvent& ev, [[maybe_unused]] const ViewContext& ctx) override {
            if (ev.action == PointerAction::Down) {
                clicked = true;
                return true; // Handled!
            }
            return false;
        }
    };

    class TestBubblingParent : public Element2D {
    public:
        using Element2D::Element2D;
        bool parentReceived{false};
        bool handlePointer(const PointerEvent& ev, [[maybe_unused]] const ViewContext& ctx) override {
            if (ev.action == PointerAction::Down) {
                parentReceived = true;
                return true;
            }
            return false;
        }
    };

    auto bubbleParent = std::make_shared<TestBubblingParent>("bubbleParent");
    bubbleParent->setBounds(0.0f, 0.0f, 200.0f, 200.0f);

    auto unhandledChild = std::make_shared<Element2D>("unhandledChild");
    unhandledChild->setBounds(20.0f, 20.0f, 100.0f, 100.0f);
    bubbleParent->addChild(unhandledChild);

    ViewContext ctx{};
    PointerEvent evDown{};
    evDown.action = PointerAction::Down;
    evDown.x = 50.0f;
    evDown.y = 50.0f;

    // Dispatching to unhandled child bubbles up to bubbleParent
    bool handled = bubbleParent->dispatchPointer(evDown, ctx);
    assert(handled);
    assert(bubbleParent->parentReceived);

    // 6. Dirty tracking & tree removal
    root->clearDirty();
    assert(!root->isDirty());
    assert(!btn1->isDirty());

    // Modifying child marks root dirty
    btn1->setBounds(25.0f, 35.0f, 80.0f, 40.0f);
    assert(btn1->isDirty());
    assert(root->isDirty());

    // Removal
    btn2->removeFromParent();
    assert(btn2->getParent() == nullptr);
    assert(container->getChildCount() == 1);
    assert(root->findById("btn2") == nullptr);

    std::cout << "  [PASS] Element2D hierarchy & event bubbling validated." << std::endl;
}

void testUnifiedControlsCore() {
    std::cout << "[Test] Unified Composable Control Core (Knobs, Sliders, Minimal Mode)..." << std::endl;

    ThemeTokens theme;
    theme.isLight = false;
    theme.primaryAccent = Color(0.18f, 0.85f, 0.95f, 1.0f);
    theme.controlMode = ControlAestheticMode::AutoThemeDefault;

    BatchRenderer2D renderer;
    renderer.beginFrame(800.0f, 600.0f);

    // 1. Minimal Vector Knob (gui-minimal / Bitwig / Ableton)
    KnobStyleCore minStyle = KnobStyleCore::minimal(theme.primaryAccent, false);
    assert(minStyle.layout == KnobLayoutMode::StackedVertical);
    assert(minStyle.arcType == KnobArcType::Unipolar);
    assert(minStyle.surface == RotorSurface::FlatVector);

    size_t v0 = renderer.getVertexCount();
    drawUnifiedKnob(renderer, 100.0f, 100.0f, 24.0f, 0.75f, minStyle, "AMOUNT", "75 %", &theme);
    size_t v1 = renderer.getVertexCount();
    assert(v1 > v0);

    // 2. Studio Modern Knob with Bipolar Center-Zero Arc (gui-variations / SMAOLAB)
    KnobStyleCore studioStyle = KnobStyleCore::studio(theme.primaryAccent, true);
    assert(studioStyle.layout == KnobLayoutMode::InlineRight);
    assert(studioStyle.arcType == KnobArcType::BipolarCenterZero);
    assert(studioStyle.surface == RotorSurface::SubtleGradient);
    assert(studioStyle.hasCenterDetent == true);

    size_t v2 = renderer.getVertexCount();
    drawUnifiedKnob(renderer, 250.0f, 100.0f, 28.0f, 0.65f, studioStyle, "BRILLIANCE", "+2.5 dB", &theme);
    size_t v3 = renderer.getVertexCount();
    assert(v3 > v2);

    // 3. Micro-Knob (Arranger track header)
    KnobStyleCore microStyle = KnobStyleCore::micro(Color(1.0f, 0.55f, 0.0f, 1.0f), true);
    assert(microStyle.layout == KnobLayoutMode::MicroInline);
    size_t v4 = renderer.getVertexCount();
    drawUnifiedKnob(renderer, 400.0f, 100.0f, 9.0f, 0.5f, microStyle, {}, {}, &theme);
    size_t v5 = renderer.getVertexCount();
    assert(v5 > v4);

    // 4. Unified Sliders (Horizontal Track & Vertical Fader)
    SliderStyleCore horizStyle = SliderStyleCore::horizontalTrack(theme.primaryAccent, 4.0f);
    assert(horizStyle.orientation == SliderOrientation::Horizontal);
    assert(horizStyle.thumbStyle == SliderThumbStyle::PillThumb);

    size_t v6 = renderer.getVertexCount();
    drawUnifiedSlider(renderer, Rect2D{50.0f, 200.0f, 120.0f, 12.0f}, 0.85f, horizStyle, theme.primaryAccent, &theme);
    size_t v7 = renderer.getVertexCount();
    assert(v7 > v6);

    SliderStyleCore vertStyle = SliderStyleCore::verticalFader(theme.primaryAccent);
    assert(vertStyle.orientation == SliderOrientation::Vertical);
    size_t v8 = renderer.getVertexCount();
    drawUnifiedSlider(renderer, Rect2D{50.0f, 250.0f, 24.0f, 100.0f}, 0.60f, vertStyle, theme.primaryAccent, &theme);
    size_t v9 = renderer.getVertexCount();
    assert(v9 > v8);

    // 5. Global ControlAestheticMode::ForceMinimal Setting Fast-Path
    GuiWidgetDef w;
    w.knobStyle = GuiKnobStyle::DaviesFluted; // heavily fluted skeuomorphic style
    w.accentColor = theme.primaryAccent;

    // Normal full skeuomorphic mode
    BatchRenderer2D rSkeuo;
    rSkeuo.beginFrame(800.0f, 600.0f);
    theme.controlMode = ControlAestheticMode::AutoThemeDefault;
    drawKnobWidget(rSkeuo, 100.0f, 400.0f, 28.0f, 0.5f, w, theme.globalLight, false, false, false, &theme);
    size_t skeuoVerts = rSkeuo.getVertexCount();

    // Now switch to ForceMinimal
    BatchRenderer2D rMin;
    rMin.beginFrame(800.0f, 600.0f);
    theme.controlMode = ControlAestheticMode::ForceMinimal;
    drawKnobWidget(rMin, 100.0f, 400.0f, 28.0f, 0.5f, w, theme.globalLight, false, false, false, &theme);
    size_t minimalVerts = rMin.getVertexCount();

    assert(minimalVerts > 0);
    // Minimal mode bypasses skirts, 3D fluting, lathe scoops, and drop shadows:
    assert(minimalVerts < skeuoVerts);

    std::cout << "  [PASS] Minimal mode vertex reduction verified: " << minimalVerts 
              << " verts (minimal) vs " << skeuoVerts << " verts (skeuomorphic)." << std::endl;
    std::cout << "  [PASS] Unified Composable Control Core validated successfully." << std::endl;
}

void testAstralKnobNormalizationAndFullRange() {
    std::cout << "[Test] Astral Shimmer Pad knob normalization and full rotational range..." << std::endl;

    TrackPropertiesDrawerData data;
    data.instrumentEngine = "ambient_pad";
    data.instrument = "astral_shimmer_pad";
    data.presetTitle = "ASTRAL SHIMMER PAD";
    data.syncKnobsIfEmpty();

    GuiPanelDef panel;
    populateCanonicalInstrumentGuiPanel(panel, data, -1, -1);

    assert(panel.rows.size() == 2);
    assert(panel.rows[0].widgets.size() == 4);
    assert(panel.rows[1].widgets.size() == 4);

    // Verify Cutoff initialization
    const auto& cutoffWidget = panel.rows[0].widgets[0];
    assert(cutoffWidget.param == "Cutoff");
    assert(cutoffWidget.minVal == 100.0f);
    assert(cutoffWidget.maxVal == 16000.0f);
    // Engineering value should be initialized properly to 2400.0f
    assertNear(cutoffWidget.currentVal, 2400.0f, 1.0f, "Cutoff currentVal should be ~2400 Hz");

    // Normalized value for rotation should be ~0.1446f
    float norm = (cutoffWidget.currentVal - cutoffWidget.minVal) / (cutoffWidget.maxVal - cutoffWidget.minVal);
    assertNear(norm, 0.1446f, 0.01f, "Cutoff normalized rotation should be ~0.1446, NOT 0.0f");

    // Formatted value should show "2400.0 Hz"
    std::string fmt = cutoffWidget.getFormattedValue();
    assert(fmt.find("2400") != std::string::npos);
    assert(fmt.find("Hz") != std::string::npos);

    // Verify interaction in TrackPropertiesPanel
    TrackPropertiesPanel propPanel;
    ViewContext ctx{};
    propPanel.layout(Rect2D(0.0f, 0.0f, 600.0f, 800.0f), ctx);
    propPanel.syncGuiPanelFromTrackData(data);

    // Give row 0, widget 0 (Cutoff) concrete bounds inside faceplate
    const auto& fp = propPanel.getFaceplateBounds();
    propPanel.getGuiPanel().rows[0].widgets[0].bounds = Rect2D(fp.x + 20.0f, fp.y + 40.0f, 60.0f, 60.0f);

    // Start pointer down inside Cutoff knob
    PointerEvent downEv{};
    downEv.type = PointerType::Mouse;
    downEv.action = PointerAction::Down;
    downEv.x = fp.x + 30.0f;
    downEv.y = fp.y + 50.0f;
    bool downHandled = propPanel.handlePointer(downEv, data, ctx);
    if (!downHandled) {
        std::cerr << "Assertion failed: downHandled must be true" << std::endl;
        std::abort();
    }

    // Drag up 160px (full sweep to max 1.0)
    PointerEvent moveEv{};
    moveEv.type = PointerType::Mouse;
    moveEv.action = PointerAction::Move;
    moveEv.x = downEv.x;
    moveEv.y = downEv.y - 160.0f; // Drag up 160px
    propPanel.handlePointer(moveEv, data, ctx);

    // Should reach max cutoff (16000 Hz)
    const auto& updatedPanel = propPanel.getGuiPanel();
    const auto& updatedCutoff = updatedPanel.rows[0].widgets[0];
    assertNear(updatedCutoff.currentVal, 16000.0f, 10.0f, "Cutoff should reach ~16000 Hz at full upward drag");
    float normMax = (updatedCutoff.currentVal - updatedCutoff.minVal) / (updatedCutoff.maxVal - updatedCutoff.minVal);
    assertNear(normMax, 1.0f, 0.01f, "Cutoff rotation norm should be 1.0 at full drag");

    // Drag down 300px to reach min 0.0
    moveEv.y = downEv.y + 160.0f;
    propPanel.handlePointer(moveEv, data, ctx);
    const auto& minCutoff = propPanel.getGuiPanel().rows[0].widgets[0];
    assertNear(minCutoff.currentVal, 100.0f, 5.0f, "Cutoff should reach ~100 Hz at full downward drag");
    float normMin = (minCutoff.currentVal - minCutoff.minVal) / (minCutoff.maxVal - minCutoff.minVal);
    assertNear(normMin, 0.0f, 0.01f, "Cutoff rotation norm should be 0.0 at full downward drag");

    std::cout << "  [PASS] Astral Shimmer Pad knob normalization, full 270-degree range, and unit readouts verified." << std::endl;
}

void testDetuneDecayIndependenceAndSync() {
    std::cout << "[Test] Detune and Decay parameter independence and no cross-talk..." << std::endl;

    TrackPropertiesDrawerData data;
    data.instrument = "Astral Shimmer Pad";
    data.instrumentEngine = "ambient_pad";
    data.syncKnobsIfEmpty();

    assert(data.knobs.size() == 8);
    // Find Detune and Decay in data.knobs
    int detuneIdx = -1;
    int decayIdx = -1;
    for (size_t i = 0; i < data.knobs.size(); ++i) {
        if (data.knobs[i].name == "Detune") detuneIdx = static_cast<int>(i);
        if (data.knobs[i].name == "Decay") decayIdx = static_cast<int>(i);
    }
    assert(detuneIdx >= 0 && "Detune must exist in data.knobs");
    assert(decayIdx >= 0 && "Decay must exist in data.knobs");
    assert(detuneIdx != decayIdx && "Detune and Decay must be separate slots");

    // 1. Modifying Detune must not alter Decay
    float origDecay = data.knobs[decayIdx].value;
    data.setKnobValue("Detune", 0.85f, "29.9 ct");
    assertNear(data.knobs[detuneIdx].value, 0.85f, 0.001f, "Detune should be 0.85");
    assertNear(data.knobs[decayIdx].value, origDecay, 0.001f, "Decay must NOT change when Detune changes");

    // 2. Modifying Decay must not alter Detune
    data.setKnobValue("Decay", 0.25f, "1.58 s");
    assertNear(data.knobs[decayIdx].value, 0.25f, 0.001f, "Decay should be 0.25");
    assertNear(data.knobs[detuneIdx].value, 0.85f, 0.001f, "Detune must NOT change when Decay changes");

    // 3. Sync to TrackPropertiesPanel and verify GUI widget values
    TrackPropertiesPanel propPanel;
    ViewContext ctx{};
    propPanel.layout(Rect2D(0.0f, 0.0f, 600.0f, 800.0f), ctx);
    propPanel.syncGuiPanelFromTrackData(data);

    const auto& fp = propPanel.getFaceplateBounds();
    auto& panelDef = propPanel.getGuiPanel();
    // Row 0 widget 2 is Detune (1.0 .. 35.0 ct)
    const auto& detuneWidget = panelDef.rows[0].widgets[2];
    assert(detuneWidget.param == "Detune");
    assertNear(detuneWidget.currentVal, 1.0f + 0.85f * 34.0f, 0.1f, "Detune widget currentVal mismatch");

    // Row 1 widget 1 is Decay (0.1 .. 6.0 s)
    const auto& decayWidget = panelDef.rows[1].widgets[1];
    assert(decayWidget.param == "Decay");
    assertNear(decayWidget.currentVal, 0.1f + 0.25f * 5.9f, 0.1f, "Decay widget currentVal mismatch");

    // 4. Drag Detune knob in panel
    panelDef.rows[0].widgets[2].bounds = Rect2D(fp.x + 150.0f, fp.y + 40.0f, 50.0f, 50.0f);
    panelDef.rows[1].widgets[1].bounds = Rect2D(fp.x + 80.0f, fp.y + 120.0f, 50.0f, 50.0f);

    PointerEvent downDetune{};
    downDetune.type = PointerType::Mouse;
    downDetune.action = PointerAction::Down;
    downDetune.x = fp.x + 160.0f;
    downDetune.y = fp.y + 50.0f;
    propPanel.handlePointer(downDetune, data, ctx);

    // Drag up 45px
    PointerEvent moveDetune{};
    moveDetune.type = PointerType::Mouse;
    moveDetune.action = PointerAction::Move;
    moveDetune.x = downDetune.x;
    moveDetune.y = downDetune.y - 45.0f;
    propPanel.handlePointer(moveDetune, data, ctx);

    // Release mouse
    PointerEvent upDetune{};
    upDetune.type = PointerType::Mouse;
    upDetune.action = PointerAction::Up;
    upDetune.x = moveDetune.x;
    upDetune.y = moveDetune.y;
    propPanel.handlePointer(upDetune, data, ctx);

    // Re-sync GUI panel and verify Decay did NOT change to Detune value!
    propPanel.syncGuiPanelFromTrackData(data);
    const auto& checkDecayAfterDetune = propPanel.getGuiPanel().rows[1].widgets[1];
    assertNear(checkDecayAfterDetune.currentVal, 0.1f + 0.25f * 5.9f, 0.1f, "Decay must retain its own value after Detune drag");

    // 5. Drag Decay knob in panel
    PointerEvent downDecay{};
    downDecay.type = PointerType::Mouse;
    downDecay.action = PointerAction::Down;
    downDecay.x = fp.x + 90.0f;
    downDecay.y = fp.y + 130.0f;
    propPanel.handlePointer(downDecay, data, ctx);

    // Drag down 60px
    PointerEvent moveDecay{};
    moveDecay.type = PointerType::Mouse;
    moveDecay.action = PointerAction::Move;
    moveDecay.x = downDecay.x;
    moveDecay.y = downDecay.y + 60.0f;
    propPanel.handlePointer(moveDecay, data, ctx);

    // Release mouse
    PointerEvent upDecay{};
    upDecay.type = PointerType::Mouse;
    upDecay.action = PointerAction::Up;
    upDecay.x = moveDecay.x;
    upDecay.y = moveDecay.y;
    propPanel.handlePointer(upDecay, data, ctx);

    // Re-sync and verify Decay does NOT revert to Detune!
    propPanel.syncGuiPanelFromTrackData(data);
    const auto& finalDecay = propPanel.getGuiPanel().rows[1].widgets[1];
    const auto& finalDetune = propPanel.getGuiPanel().rows[0].widgets[2];
    assert(std::abs(finalDecay.currentVal - finalDetune.currentVal) > 5.0f && "Decay must NOT snap to Detune!");

    std::cout << "  [PASS] Detune and Decay independence and no cross-talk verified." << std::endl;
}

void testNeumorphicLensAndCenterGlyph() {
    std::cout << "[Test] NeumorphicLens volcano mound, concave dish, and center glyph..." << std::endl;

    // 1. Validate NeumorphicLens shape definitions
    KnobShapeDef lensDef = getKnobShapeDef(GuiKnobStyle::NeumorphicLens);
    assert(lensDef.shape == KnobBodyShape::Round);
    assert(lensDef.capStyle == KnobCapStyle::ConcaveDish);
    assert(lensDef.hasVolcanoMound == true);
    assert(lensDef.moundElevation > 5.0f);
    assert(lensDef.indicator == KnobIndicatorType::DotPip);
    assert(lensDef.indicatorColor.r > 0.9f && lensDef.indicatorColor.g > 0.9f && lensDef.indicatorColor.b > 0.9f);

    KnobShapeDef lensPinkDef = getKnobShapeDef(GuiKnobStyle::NeumorphicLensPink);
    assert(lensPinkDef.capStyle == KnobCapStyle::ConcaveDish);
    assert(lensPinkDef.hasVolcanoMound == true);
    assert(lensPinkDef.indicatorColor.r > 0.9f && lensPinkDef.indicatorColor.b > 0.5f); // Hot pink
    assert(lensPinkDef.centerGlyphColor.has_value());

    // 2. Validate drawing with center glyphs (Unicode audio symbol & Nerd Font icon)
    BatchRenderer2D r;
    r.beginFrame(800.0f, 600.0f);
    LightSource2D light{};

    GuiWidgetDef w;
    w.id = "w_scatter";
    w.knobStyle = GuiKnobStyle::NeumorphicLens;
    w.centerGlyph = "∿"; // Sine wave symbol
    w.accentColor = Color(0.96f, 0.98f, 1.0f, 1.0f);
    w.currentVal = 0.5f;

    size_t v0 = r.getVertexCount();
    drawKnobWidget(r, 200.0f, 200.0f, 28.0f, 0.5f, w, light, false, false, false);
    size_t v1 = r.getVertexCount();
    assert(v1 > v0);

    // 3. Pink variant on silver/light chassis with multi-byte Nerd Font glyph
    GuiWidgetDef wPink;
    wPink.id = "w_mod";
    wPink.knobStyle = GuiKnobStyle::NeumorphicLensPink;
    wPink.centerGlyph = "\xEF\x87\xBE"; // Waveform chart Nerd Font glyph in UTF-8
    wPink.accentColor = Color(1.0f, 0.35f, 0.65f, 1.0f);
    wPink.currentVal = 0.75f;

    drawKnobWidget(r, 300.0f, 200.0f, 28.0f, 0.75f, wPink, light, true, false, true);
    size_t v2 = r.getVertexCount();
    assert(v2 > v1);

    std::cout << "  [PASS] NeumorphicLens volcano mound, concave dish, and center glyph validated." << std::endl;
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
    testElement2D();
    testUnifiedControlsCore();
    testAstralKnobNormalizationAndFullRange();
    testDetuneDecayIndependenceAndSync();
    testNeumorphicLensAndCenterGlyph();
    std::cout << "=== All UI Geometry & Widgets Tests Passed! ===" << std::endl;
    return 0;
}
