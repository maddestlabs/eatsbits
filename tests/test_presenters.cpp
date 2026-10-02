#include <iostream>
#include <cstdlib>
#include <cmath>
#include <string>

#include "eatsbits/presenter/presenter_base.hpp"
#include "eatsbits/presenter/parameter_presenter.hpp"
#include "eatsbits/presenter/value_edit_presenter.hpp"

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        std::cerr << "Assertion failed: (" #expr ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while(0)

#define REQUIRE_NEAR(a, b, eps) do { \
    if (std::abs((a) - (b)) > (eps)) { \
        std::cerr << "Assertion failed: |" #a " - " #b "| <= " #eps \
                  << " (" << (a) << " vs " << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while(0)

#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/presenter/drag_types.hpp"
#include "eatsbits/presenter/drag_handler.hpp"
#include "eatsbits/presenter/scalar_drag_presenter.hpp"
#include "eatsbits/presenter/timeline_scrub_presenter.hpp"
#include "eatsbits/presenter/splitter_drag_presenter.hpp"
#include "eatsbits/presenter/cable_patch_presenter.hpp"
#include "eatsbits/presenter/marquee_select_presenter.hpp"
#include "eatsbits/presenter/telemetry_presenter.hpp"
#include "eatsbits/presenter/scroll_physics_presenter.hpp"
#include "eatsbits/presenter/zoom_pan_presenter.hpp"

using namespace eatsbits;
using namespace eatsbits::audio;
using namespace eatsbits::ui;
using namespace eatsbits::presenter;

void testPresenterBase() {
    std::cout << "[Test 1/3] PresenterBase dirty tracking & revisions..." << std::endl;

    class TestPresenter : public PresenterBase {
    public:
        void trigger() { markDirty(); }
    };

    TestPresenter p;
    REQUIRE(p.isDirty());
    REQUIRE(p.getRevision() == 1);

    p.clearDirty();
    REQUIRE(!p.isDirty());
    REQUIRE(p.getRevision() == 1);

    bool callbackFired = false;
    p.setOnDirtyChanged([&]() { callbackFired = true; });

    p.trigger();
    REQUIRE(p.isDirty());
    REQUIRE(p.getRevision() == 2);
    REQUIRE(callbackFired);

    std::cout << "  [PASS] PresenterBase lifecycle validated." << std::endl;
}

void testParameterPresenter() {
    std::cout << "[Test 2/3] ParameterPresenter bounds, normalization, stepping & string caching..." << std::endl;

    ParameterDescriptor desc;
    desc.name = "cutoff";
    desc.label = "303 CUTOFF";
    desc.unit = "Hz";
    desc.minValue = 100.0f;
    desc.maxValue = 6000.0f;
    desc.defaultValue = 850.0f;
    desc.step = 50.0f;
    desc.isInteger = false;
    desc.allowPercentage = true;

    ParameterPresenter param(desc);
    REQUIRE_NEAR(param.getValue(), 850.0f, 1e-4f);
    REQUIRE(param.getFormattedValue() == "850 Hz");
    REQUIRE(param.getRawFormattedValue() == "850");

    // Normalization check: 850 in [100, 6000] -> (850 - 100) / 5900 = 750 / 5900 ≈ 0.1271186
    float expectedNorm = (850.0f - 100.0f) / 5900.0f;
    REQUIRE_NEAR(param.getNormalizedValue(), expectedNorm, 1e-3f);

    // Value change notification
    float notifiedVal = 0.0f;
    param.onValueChanged = [&](float v) { notifiedVal = v; };

    param.setValue(1200.0f);
    REQUIRE_NEAR(param.getValue(), 1200.0f, 1e-4f);
    REQUIRE_NEAR(notifiedVal, 1200.0f, 1e-4f);
    REQUIRE(param.getFormattedValue() == "1200 Hz");

    // Step test
    param.step(2); // +100 Hz
    REQUIRE_NEAR(param.getValue(), 1300.0f, 1e-4f);
    REQUIRE_NEAR(notifiedVal, 1300.0f, 1e-4f);
    REQUIRE(param.getFormattedValue() == "1300 Hz");

    // Step below minimum clamps
    param.step(-100);
    REQUIRE_NEAR(param.getValue(), 100.0f, 1e-4f);
    REQUIRE(param.getFormattedValue() == "100 Hz");

    // Reset to default
    param.resetToDefault();
    REQUIRE_NEAR(param.getValue(), 850.0f, 1e-4f);
    REQUIRE(param.getFormattedValue() == "850 Hz");

    // Integer parameter check
    ParameterDescriptor intDesc;
    intDesc.name = "semitone";
    intDesc.minValue = -12.0f;
    intDesc.maxValue = 12.0f;
    intDesc.defaultValue = 0.0f;
    intDesc.isInteger = true;
    intDesc.unit = "st";

    ParameterPresenter intParam(intDesc);
    intParam.setValue(3.4f);
    REQUIRE_NEAR(intParam.getValue(), 3.0f, 1e-4f);
    REQUIRE(intParam.getFormattedValue() == "3 st");

    std::cout << "  [PASS] ParameterPresenter validated." << std::endl;
}

void testValueEditPresenter() {
    std::cout << "[Test 3/3] ValueEditPresenter validation, % math, and submit routing..." << std::endl;

    ValueEditConfig cfg;
    cfg.title = "MASTER VOLUME";
    cfg.paramName = "Volume";
    cfg.minValue = 0.0f;
    cfg.maxValue = 2.0f;
    cfg.defaultValue = 1.0f;
    cfg.currentValue = 0.8f;
    cfg.allowPercentage = true;
    cfg.unit = "dB";

    float committedVal = -999.0f;
    cfg.onCommit = [&](float v) { committedVal = v; };

    ValueEditPresenter presenter;
    presenter.open(cfg);
    REQUIRE(presenter.isOpen());
    REQUIRE(!presenter.isPercentMode());
    REQUIRE(presenter.getInitialEditText() == "0.8");

    // Toggle % mode: 0.8 in [0, 2] is 40%
    std::string pctText = presenter.togglePercentMode("0.8");
    REQUIRE(presenter.isPercentMode());
    REQUIRE(pctText == "40");

    // Toggle back to direct value
    std::string directText = presenter.togglePercentMode("50");
    REQUIRE(!presenter.isPercentMode());
    // 50% of [0, 2] is 1.0
    REQUIRE_NEAR(std::stof(directText), 1.0f, 1e-3f);

    // Direct submit with % string "75%"
    bool ok = presenter.submit("75%");
    REQUIRE(ok);
    REQUIRE(!presenter.isOpen());
    // 75% of [0, 2] = 1.5
    REQUIRE_NEAR(committedVal, 1.5f, 1e-3f);

    // Test binding to ParameterPresenter
    ParameterDescriptor pDesc;
    pDesc.name = "tempo";
    pDesc.label = "BPM";
    pDesc.minValue = 60.0f;
    pDesc.maxValue = 240.0f;
    pDesc.defaultValue = 120.0f;
    pDesc.unit = "BPM";
    ParameterPresenter bpmParam(pDesc);
    bpmParam.setValue(135.0f);

    bool finished = false;
    presenter.bindToParameter(bpmParam, [&]() { finished = true; });
    REQUIRE(presenter.isOpen());
    REQUIRE(presenter.getInitialEditText() == "135");

    presenter.submit("140.5");
    REQUIRE(!presenter.isOpen());
    REQUIRE(finished);
    REQUIRE_NEAR(bpmParam.getValue(), 140.5f, 1e-3f);
    REQUIRE(bpmParam.getFormattedValue() == "140.5 BPM");

    // Text mode test
    ValueEditConfig textCfg;
    textCfg.isTextMode = true;
    textCfg.paramName = "Track 1";
    textCfg.initialText = "Acid 303";
    std::string committedText = "";
    textCfg.onCommitText = [&](const std::string& s) { committedText = s; };

    presenter.open(textCfg);
    REQUIRE(presenter.isTextMode());
    REQUIRE(presenter.getInitialEditText() == "Acid 303");
    presenter.submit("TB-303 Synth");
    REQUIRE(committedText == "TB-303 Synth");

    std::cout << "  [PASS] ValueEditPresenter validated." << std::endl;
}

#include "eatsbits/presenter/track_properties_presenter.hpp"

void testTrackPropertiesPresenter() {
    std::cout << "[Test 4/4] TrackPropertiesPresenter parameter descriptors & configuration..." << std::endl;

    TrackPropertiesPresenter presenter;
    REQUIRE(presenter.getTrackName() == "Track 1");

    presenter.setTrackName("Acid 303");
    REQUIRE(presenter.getTrackName() == "Acid 303");

    auto* volParam = presenter.getParameter(TrackParamType::Volume);
    REQUIRE(volParam != nullptr);
    REQUIRE(volParam->getName() == "volume");
    REQUIRE_NEAR(volParam->getDefaultValue(), 0.8f, 1e-4f);
    REQUIRE_NEAR(volParam->getMaxValue(), 1.5f, 1e-4f);

    float committed = -1.0f;
    auto cfg = TrackPropertiesPresenter::makeEditConfig(
        TrackParamType::EqHpf, 80.0f, "Acid 303", [&](float v) { committed = v; });
    REQUIRE(cfg.title == "Acid 303 • HPF Cut");
    REQUIRE(cfg.paramName == "HPF Cut");
    REQUIRE_NEAR(cfg.minValue, 20.0f, 1e-4f);
    REQUIRE_NEAR(cfg.maxValue, 500.0f, 1e-4f);
    REQUIRE(cfg.unit == "Hz");
    REQUIRE(!cfg.allowPercentage);

    cfg.onCommit(150.0f);
    REQUIRE_NEAR(committed, 150.0f, 1e-4f);

    // Generic knob config
    auto knobCfg = TrackPropertiesPresenter::makeGenericKnobEditConfig(
        "Delay • Feedback", "Feedback", 0.45f, nullptr, 0.0f, 0.95f, 0.5f, "%");
    REQUIRE(knobCfg.title == "Delay • Feedback");
    REQUIRE_NEAR(knobCfg.maxValue, 0.95f, 1e-4f);
    REQUIRE(knobCfg.allowPercentage);

    std::cout << "  [PASS] TrackPropertiesPresenter validated." << std::endl;
}

#include "eatsbits/presenter/scalar_drag_presenter.hpp"
#include "eatsbits/presenter/timeline_scrub_presenter.hpp"
#include "eatsbits/presenter/splitter_drag_presenter.hpp"
#include "eatsbits/presenter/cable_patch_presenter.hpp"
#include "eatsbits/presenter/marquee_select_presenter.hpp"

void testScalarDragPresenter() {
    std::cout << "[Test 5/9] ScalarDragPresenter continuous drag, bounds, fine control & cancel..." << std::endl;

    ScalarDragPresenter presenter;
    REQUIRE(!presenter.isDragging());
    REQUIRE(presenter.getDragMode() == ui::DragMode::None);

    // 1. Knob dragging (VerticalUpIncreases, sensitivity = 1/160)
    ScalarDragConfig knobCfg{};
    knobCfg.dragMode = ui::DragMode::Knob;
    knobCfg.direction = DragDirection::VerticalUpIncreases;
    knobCfg.minValue = 0.0f;
    knobCfg.maxValue = 1.0f;
    knobCfg.sensitivity = 1.0f / 160.0f;
    knobCfg.fineRatio = 0.1f;

    float currentVal = 0.5f;
    float committedVal = -1.0f;
    presenter.startDrag(100.0f, 200.0f, 0.5f, knobCfg,
        [&](float v) { currentVal = v; },
        [&](float v) { committedVal = v; });

    REQUIRE(presenter.isDragging());
    REQUIRE(presenter.getDragMode() == ui::DragMode::Knob);
    REQUIRE_NEAR(presenter.getCurrentValue(), 0.5f, 1e-4f);

    // Drag up 80px -> +0.5 normalized increase -> reaches 1.0
    presenter.onPointerMove(100.0f, 120.0f);
    REQUIRE_NEAR(presenter.getCurrentValue(), 1.0f, 1e-4f);
    REQUIRE_NEAR(currentVal, 1.0f, 1e-4f);

    // Drag further up -> clamps at 1.0
    presenter.onPointerMove(100.0f, 40.0f);
    REQUIRE_NEAR(presenter.getCurrentValue(), 1.0f, 1e-4f);

    // Drag down by 160px from start -> drops to 0.0
    presenter.onPointerMove(100.0f, 360.0f);
    REQUIRE_NEAR(presenter.getCurrentValue(), 0.0f, 1e-4f);

    // 2. Test Fine Control (Shift key modifier)
    presenter.setFineControl(true);
    // Dragging up by 160px with fineRatio=0.1 -> only +0.1 increase!
    presenter.onPointerMove(100.0f, 40.0f); // 160px up from start
    REQUIRE_NEAR(presenter.getCurrentValue(), 0.6f, 1e-3f);
    presenter.setFineControl(false);

    // 3. Test Commit on pointer up
    presenter.onPointerUp(100.0f, 40.0f);
    REQUIRE(!presenter.isDragging());
    REQUIRE(presenter.getDragMode() == ui::DragMode::None);
    REQUIRE_NEAR(committedVal, 0.6f, 1e-3f);

    // 4. Test Step Snapping (e.g. UI scale 0.5 to 2.0 with stepSnap = 0.25)
    ScalarDragConfig snapCfg{};
    snapCfg.dragMode = ui::DragMode::UiScaleSlider;
    snapCfg.direction = DragDirection::HorizontalRightIncreases;
    snapCfg.minValue = 0.50f;
    snapCfg.maxValue = 2.00f;
    snapCfg.sensitivity = 1.50f / 400.0f;
    snapCfg.stepSnap = 0.25f;

    presenter.startDrag(200.0f, 200.0f, 1.0f, snapCfg, [&](float v) { currentVal = v; });
    // Drag right by 70px -> delta ~ 0.2625 -> snaps to 1.25
    presenter.onPointerMove(270.0f, 200.0f);
    REQUIRE_NEAR(presenter.getCurrentValue(), 1.25f, 1e-4f);

    // 5. Test Cancel / Rollback
    presenter.cancelDrag();
    REQUIRE(!presenter.isDragging());
    REQUIRE_NEAR(presenter.getCurrentValue(), 1.0f, 1e-4f);
    REQUIRE_NEAR(currentVal, 1.0f, 1e-4f);

    // 6. Test direct ParameterPresenter binding
    ParameterDescriptor bpmDesc{};
    bpmDesc.name = "BPM";
    bpmDesc.minValue = 40.0f;
    bpmDesc.maxValue = 300.0f;
    bpmDesc.defaultValue = 135.0f;
    ParameterPresenter bpmParam(bpmDesc);
    bpmParam.setValue(135.0f);

    ScalarDragConfig bpmCfg{};
    bpmCfg.dragMode = ui::DragMode::BpmScrubber;
    bpmCfg.direction = DragDirection::VerticalUpIncreases;
    bpmCfg.sensitivity = 0.5f;

    presenter.startDragWithPresenter(50.0f, 50.0f, bpmParam, bpmCfg);
    presenter.onPointerMove(50.0f, 30.0f); // 20px up -> +10 BPM
    REQUIRE_NEAR(bpmParam.getValue(), 145.0f, 1e-4f);
    presenter.onPointerUp(50.0f, 30.0f);
    REQUIRE(!presenter.isDragging());

    std::cout << "  [PASS] ScalarDragPresenter validated." << std::endl;
}

void testTimelineScrubPresenter() {
    std::cout << "[Test 6/9] TimelineScrubPresenter ruler & overview scrub math..." << std::endl;

    TimelineScrubPresenter presenter;
    REQUIRE(!presenter.isDragging());
    REQUIRE(presenter.getDragMode() == ui::DragMode::None);

    uint32_t activeStep = 0;
    // 1. Ruler scrub: trackHeaderW = 210, barW = 60, stepsPerBar = 16
    presenter.startRulerScrub(270.0f, 210.0f, 60.0f, 0, 288, [&](uint32_t s) { activeStep = s; });
    REQUIRE(presenter.isDragging());
    REQUIRE(presenter.getDragMode() == ui::DragMode::ArrangerRulerScrub);
    // startX = 270 (60px past header = 1 bar = 16 steps)
    REQUIRE(activeStep == 16);

    // Scrub to bar 3 (210 + 3 * 60 = 390px) -> 48 steps
    presenter.onPointerMove(390.0f, 50.0f);
    REQUIRE(activeStep == 48);

    // Scrub before header (< 210px) -> clamps to 0
    presenter.onPointerMove(150.0f, 50.0f);
    REQUIRE(activeStep == 0);

    presenter.onPointerUp(150.0f, 50.0f);
    REQUIRE(!presenter.isDragging());

    // 2. Minimap overview scrub: header = 210, overviewW = 1000, maxSteps = 288
    presenter.startOverviewScrub(710.0f, 210.0f, 1000.0f, 0, 288, [&](uint32_t s) { activeStep = s; });
    REQUIRE(presenter.getDragMode() == ui::DragMode::ArrangerOverviewScroll);
    // (710 - 210) / 1000 = 0.50 -> 144 steps
    REQUIRE(activeStep == 144);

    presenter.cancelDrag();
    REQUIRE(!presenter.isDragging());
    REQUIRE(activeStep == 0); // Reverted to initial

    std::cout << "  [PASS] TimelineScrubPresenter validated." << std::endl;
}

void testSplitterDragPresenter() {
    std::cout << "[Test 7/9] SplitterDragPresenter drawer resizing & auto-collapse..." << std::endl;

    SplitterDragPresenter presenter;
    SplitterConfig cfg{};
    cfg.dragMode = ui::DragMode::ArrangerPropertiesResize;
    cfg.minWidth = 240.0f;
    cfg.maxWidth = 640.0f;
    cfg.defaultWidth = 300.0f;
    cfg.collapseThresholdMargin = 25.0f;

    float width = 300.0f;
    bool expanded = true;

    // 1. Drag to expand
    presenter.startDrag(1000.0f, width, expanded, cfg, [&](float w, bool exp) {
        width = w;
        expanded = exp;
    });
    REQUIRE(presenter.isDragging());
    REQUIRE(presenter.getDragMode() == ui::DragMode::ArrangerPropertiesResize);

    // Drag left by 50px (to 950.0f) -> width increases by 50px to 350
    presenter.onPointerMove(950.0f, 100.0f);
    REQUIRE_NEAR(width, 350.0f, 1e-4f);

    // Drag right to collapse threshold (< 215px) -> collapses drawer
    presenter.onPointerMove(1100.0f, 100.0f); // width would be 300 - 100 = 200 (< 215)
    REQUIRE(!expanded);
    REQUIRE_NEAR(width, 300.0f, 1e-4f);
    REQUIRE(!presenter.isDragging());

    // 2. Click detection on pull-tab (delta < 4.0px toggles expanded state)
    presenter.startDrag(1000.0f, width, false, cfg, [&](float w, bool exp) {
        width = w;
        expanded = exp;
    });
    presenter.onPointerUp(1001.0f, 100.0f); // 1px move -> click
    REQUIRE(expanded);
    REQUIRE(!presenter.isDragging());

    std::cout << "  [PASS] SplitterDragPresenter validated." << std::endl;
}

void testCablePatchPresenter() {
    std::cout << "[Test 8/9] CablePatchPresenter modular graph connection..." << std::endl;

    audio::AudioGraph graph;
    audio::NodeId osc = graph.addNode(std::make_shared<audio::Tb303Node>());
    audio::NodeId del = graph.addNode(std::make_shared<audio::DelayNode>());

    CablePatchPresenter presenter;
    bool connected = false;
    presenter.startPatch(osc, 0, 100.0f, 200.0f, [&](audio::NodeId, uint32_t, audio::NodeId, uint32_t) {
        connected = true;
    });

    REQUIRE(presenter.isDragging());
    REQUIRE(presenter.getDragMode() == ui::DragMode::PatchCable);
    REQUIRE(presenter.getSourceNode() == osc);
    REQUIRE_NEAR(presenter.getSourceX(), 100.0f, 1e-4f);

    presenter.onPointerMove(250.0f, 320.0f);
    REQUIRE_NEAR(presenter.getCurrentX(), 250.0f, 1e-4f);

    // Output to output is invalid
    REQUIRE(!presenter.canConnect(del, 0, /*isOutput=*/ true));
    // Self-connection is invalid
    REQUIRE(!presenter.canConnect(osc, 0, /*isOutput=*/ false));
    // Valid connection
    REQUIRE(presenter.canConnect(del, 0, /*isOutput=*/ false));

    bool ok = presenter.commitConnection(del, 0, /*isOutput=*/ false, graph);
    REQUIRE(ok);
    REQUIRE(connected);
    REQUIRE(!presenter.isDragging());

    std::cout << "  [PASS] CablePatchPresenter validated." << std::endl;
}

void testMarqueeSelectPresenter() {
    std::cout << "[Test 9/9] MarqueeSelectPresenter 2D rubber-band box selection..." << std::endl;

    MarqueeSelectPresenter presenter;
    MarqueeRect rect{};
    bool isAdd = false;
    bool isDrag = false;

    presenter.startSelection(100.0f, 100.0f, false,
        [&](const MarqueeRect& r, bool add) { rect = r; isAdd = add; },
        [&](const MarqueeRect& r, bool add, bool drag) { rect = r; isAdd = add; isDrag = drag; });

    REQUIRE(presenter.isDragging());
    REQUIRE(presenter.getDragMode() == ui::DragMode::PianoRollMarquee);
    REQUIRE(!presenter.isMarqueeActive()); // Deadband not yet crossed

    // Move by 2px (within 4px deadband)
    presenter.onPointerMove(102.0f, 101.0f);
    REQUIRE(!presenter.isMarqueeActive());

    // Move past deadband
    presenter.onPointerMove(200.0f, 150.0f);
    REQUIRE(presenter.isMarqueeActive());
    REQUIRE_NEAR(rect.minX, 100.0f, 1e-4f);
    REQUIRE_NEAR(rect.minY, 100.0f, 1e-4f);
    REQUIRE_NEAR(rect.maxX, 200.0f, 1e-4f);
    REQUIRE_NEAR(rect.maxY, 150.0f, 1e-4f);

    // Test reverse drag (up-left)
    presenter.onPointerMove(50.0f, 40.0f);
    auto bounds = presenter.getBounds();
    REQUIRE_NEAR(bounds.minX, 50.0f, 1e-4f);
    REQUIRE_NEAR(bounds.minY, 40.0f, 1e-4f);
    REQUIRE_NEAR(bounds.maxX, 100.0f, 1e-4f);
    REQUIRE_NEAR(bounds.maxY, 100.0f, 1e-4f);

    // Test intersection
    REQUIRE(bounds.contains(75.0f, 75.0f));
    REQUIRE(!bounds.contains(120.0f, 120.0f));
    REQUIRE(bounds.intersects(90.0f, 90.0f, 20.0f, 20.0f));

    presenter.onPointerUp(50.0f, 40.0f);
    REQUIRE(!presenter.isDragging());
    REQUIRE(isDrag);

    std::cout << "  [PASS] MarqueeSelectPresenter validated." << std::endl;
}

void testTelemetryPresenter() {
    std::cout << "[Test 10] Testing TelemetryPresenter ballistics, settling, FFT & dirty tracking..." << std::endl;

    TelemetryPresenter presenter(128, 8);
    REQUIRE(presenter.isDirty());
    REQUIRE(!presenter.isSettled());
    presenter.clearDirty();
    REQUIRE(!presenter.isDirty());

    // 1. Attack Ballistics (Instant response to peak)
    presenter.feedCustomAudio(nullptr, 0, 0.85f, 0.60f, 0.40f, 0.30f);
    REQUIRE_NEAR(presenter.getMasterMeter().peakL, 0.85f, 1e-4f);
    REQUIRE_NEAR(presenter.getMasterMeter().peakR, 0.60f, 1e-4f);
    REQUIRE_NEAR(presenter.getMasterMeter().peakHoldL, 0.85f, 1e-4f);
    REQUIRE_NEAR(presenter.getMasterMeter().peakHoldR, 0.60f, 1e-4f);
    REQUIRE_NEAR(presenter.getMasterMeter().rmsL, 0.40f, 1e-4f);
    REQUIRE(presenter.isDirty());
    REQUIRE(!presenter.isSettled());

    // Flat compatibility fields
    REQUIRE_NEAR(presenter.getSnapshot().peakL, 0.85f, 1e-4f);
    REQUIRE_NEAR(presenter.getSnapshot().peakR, 0.60f, 1e-4f);

    // 2. Exponential Release Decay
    presenter.clearDirty();
    presenter.feedCustomAudio(nullptr, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.016666f);
    float expectedDecayL = 0.85f * 0.91f;
    REQUIRE_NEAR(presenter.getMasterMeter().peakL, expectedDecayL, 0.015f);
    REQUIRE(presenter.isDirty());
    REQUIRE(!presenter.isSettled());

    // 3. Peak Hold retention (holds for 1.0s)
    REQUIRE_NEAR(presenter.getMasterMeter().peakHoldL, 0.85f, 1e-4f);

    // Advance 60 frames (~1.0s) with silence to exhaust peak hold timer
    for (int i = 0; i < 60; ++i) {
        presenter.feedCustomAudio(nullptr, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.016666f);
    }
    // Peak hold timer expired -> should have started decaying downward
    REQUIRE(presenter.getMasterMeter().peakHoldL < 0.85f);

    // 4. Settling & Silence Threshold clamping (< -66 dB / 0.0005f)
    for (int i = 0; i < 150; ++i) {
        presenter.feedCustomAudio(nullptr, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.016666f);
    }
    REQUIRE_NEAR(presenter.getMasterMeter().peakL, 0.0f, 1e-6f);
    REQUIRE_NEAR(presenter.getMasterMeter().peakR, 0.0f, 1e-6f);
    REQUIRE_NEAR(presenter.getMasterMeter().peakHoldL, 0.0f, 1e-6f);
    REQUIRE(presenter.isSettled());

    // Once settled, subsequent updates while stopped and quiet do NOT mark dirty
    presenter.clearDirty();
    presenter.feedCustomAudio(nullptr, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.016666f);
    REQUIRE(presenter.isSettled());
    REQUIRE(!presenter.isDirty());

    // 5. Transport State & Bar/Beat/16th calculations
    presenter.setTransport(true, 37, 130.0, 5.25);
    REQUIRE(presenter.getTransport().isPlaying);
    REQUIRE(presenter.getTransport().currentStep == 37);
    REQUIRE(presenter.getTransport().bar == 3);      // (37 / 16) + 1 = 3
    REQUIRE(presenter.getTransport().beat == 2);     // ((37 % 16) / 4) + 1 = (5 / 4) + 1 = 2
    REQUIRE(presenter.getTransport().sixteenth == 2);// (37 % 4) + 1 = 2
    REQUIRE(!presenter.isSettled());
    REQUIRE(presenter.isDirty());

    // 6. 16-Band Log-Spaced FFT Spectrum Decomposition
    std::vector<float> sineWave(128);
    for (size_t n = 0; n < 128; ++n) {
        sineWave[n] = static_cast<float>(std::sin(2.0 * 3.141592653589793 * 1000.0 * n / 44100.0));
    }
    presenter.feedCustomAudio(sineWave.data(), sineWave.size(), 1.0f, 1.0f);
    const auto& bands = presenter.getSpectrumBands();
    // 1000 Hz corresponds to bin 7 (center freq 1000 Hz)
    REQUIRE(bands[7] > 0.05f);
    REQUIRE(bands[7] > bands[0]);  // 1 kHz peak much higher than 40 Hz bin
    REQUIRE(bands[7] > bands[15]); // 1 kHz peak much higher than 16 kHz bin

    // 7. Dirty Callback
    bool callbackTriggered = false;
    presenter.setOnDirtyChanged([&]() { callbackTriggered = true; });
    presenter.feedCustomAudio(nullptr, 0, 0.5f, 0.5f);
    REQUIRE(callbackTriggered);

    std::cout << "  [PASS] TelemetryPresenter validated." << std::endl;
}

void testPointerEventInteraction() {
    std::cout << "[Test 11] Testing PointerEvent normalization across all IDragHandlers..." << std::endl;

    // 1. ScalarDragPresenter with PointerEvent modifiers
    {
        ScalarDragPresenter presenter;
        ScalarDragConfig cfg{};
        cfg.dragMode = ui::DragMode::Knob;
        cfg.direction = DragDirection::VerticalUpIncreases;
        cfg.minValue = 0.0f;
        cfg.maxValue = 100.0f;
        cfg.sensitivity = 1.0f; // 1 pixel = 1 unit
        cfg.fineRatio = 0.1f;   // Fine control = 0.1x

        float currentVal = 50.0f;
        presenter.startDrag(100.0f, 100.0f, 50.0f, cfg, [&](float v) { currentVal = v; });

        // Normal drag: 10px up -> +10
        PointerEvent evNormal{};
        evNormal.type = PointerType::Mouse;
        evNormal.action = PointerAction::Move;
        evNormal.x = 100.0f;
        evNormal.y = 90.0f;
        presenter.onPointerMove(evNormal);
        REQUIRE_NEAR(currentVal, 60.0f, 1e-4f);

        // Shift drag: 10px up with shift modifier -> +1.0
        PointerEvent evShift{};
        evShift.type = PointerType::Mouse;
        evShift.action = PointerAction::Move;
        evShift.x = 100.0f;
        evShift.y = 80.0f; // 20px total up from start (100)
        evShift.mods.shift = true;
        presenter.onPointerMove(evShift);
        // With fineControl activated: delta = 20 * 1.0 * 0.1 = 2.0 -> 50 + 2.0 = 52.0
        REQUIRE_NEAR(currentVal, 52.0f, 1e-4f);
        REQUIRE(presenter.isFineControl());

        // Touch event with touchId and pressure
        PointerEvent evTouch{};
        evTouch.type = PointerType::Touch;
        evTouch.id = 1;
        evTouch.pressure = 0.75f;
        evTouch.action = PointerAction::Move;
        evTouch.x = 100.0f;
        evTouch.y = 70.0f; // 30px total up -> +3.0
        evTouch.mods.shift = true;
        presenter.onPointerMove(evTouch);
        REQUIRE_NEAR(currentVal, 53.0f, 1e-4f);

        // PointerUp via PointerEvent
        PointerEvent evUp{};
        evUp.type = PointerType::Touch;
        evUp.id = 1;
        evUp.action = PointerAction::Up;
        evUp.x = 100.0f;
        evUp.y = 70.0f;
        presenter.onPointerUp(evUp);
        REQUIRE(!presenter.isDragging());
    }

    // 2. Polymorphic IDragHandler dispatch with MarqueeSelectPresenter + Ctrl modifier
    {
        MarqueeSelectPresenter marquee;
        IDragHandler* handler = &marquee;

        MarqueeRect updatedBounds{};
        bool additiveReported = false;
        marquee.startSelection(50.0f, 50.0f, false,
            [&](const MarqueeRect& r, bool isAdd) {
                updatedBounds = r;
                additiveReported = isAdd;
            });

        REQUIRE(handler->isDragging());
        REQUIRE(handler->getDragMode() == ui::DragMode::PianoRollMarquee);

        // Move with Ctrl modifier: automatically makes selection additive!
        PointerEvent evMove{};
        evMove.type = PointerType::Mouse;
        evMove.action = PointerAction::Move;
        evMove.x = 120.0f;
        evMove.y = 80.0f;
        evMove.mods.ctrl = true;
        handler->onPointerMove(evMove);

        REQUIRE(marquee.isMarqueeActive());
        REQUIRE(marquee.isAdditive());
        REQUIRE(additiveReported);
        REQUIRE_NEAR(updatedBounds.minX, 50.0f, 1e-4f);
        REQUIRE_NEAR(updatedBounds.maxX, 120.0f, 1e-4f);
        REQUIRE_NEAR(updatedBounds.minY, 50.0f, 1e-4f);
        REQUIRE_NEAR(updatedBounds.maxY, 80.0f, 1e-4f);

        PointerEvent evUp{};
        evUp.action = PointerAction::Up;
        evUp.x = 120.0f;
        evUp.y = 80.0f;
        handler->onPointerUp(evUp);
        REQUIRE(!handler->isDragging());
    }

    // 3. Backward-compatible float overloads delegation
    {
        TimelineScrubPresenter scrub;
        IDragHandler* handler = &scrub;
        uint32_t step = 0;
        scrub.startRulerScrub(0.0f, 100.0f, 100.0f, 0, 64, [&](uint32_t s) { step = s; });

        // Call old signature: handler->onPointerMove(float, float)
        handler->onPointerMove(150.0f, 0.0f); // 50px past header on 100px bar -> 0.5 bar = step 8
        REQUIRE(step == 8);

        handler->onPointerUp(150.0f, 0.0f, 1);
        REQUIRE(!handler->isDragging());
    }

    // 4. SplitterDragPresenter with PointerEvent
    {
        SplitterDragPresenter splitter;
        SplitterConfig scfg{};
        scfg.defaultWidth = 300.0f;
        scfg.minWidth = 200.0f;
        scfg.maxWidth = 500.0f;

        float width = 300.0f;
        bool expanded = true;
        splitter.startDrag(300.0f, 300.0f, true, scfg,
            [&](float w, bool exp) { width = w; expanded = exp; });

        // Drag 50px left (x = 250) -> width increases by 50px to 350px
        PointerEvent evMove{};
        evMove.x = 250.0f;
        evMove.y = 200.0f;
        splitter.onPointerMove(evMove);
        REQUIRE_NEAR(width, 350.0f, 1e-4f);

        PointerEvent evUp{};
        evUp.x = 250.0f;
        evUp.y = 200.0f;
        splitter.onPointerUp(evUp);
        REQUIRE(!splitter.isDragging());
    }

    // 5. CablePatchPresenter with PointerEvent
    {
        CablePatchPresenter cable;
        cable.startPatch(1, 0, 40.0f, 60.0f);
        REQUIRE(cable.isDragging());

        PointerEvent evMove{};
        evMove.x = 180.0f;
        evMove.y = 240.0f;
        cable.onPointerMove(evMove);
        REQUIRE_NEAR(cable.getCurrentX(), 180.0f, 1e-4f);
        REQUIRE_NEAR(cable.getCurrentY(), 240.0f, 1e-4f);

        PointerEvent evUp{};
        evUp.x = 180.0f;
        evUp.y = 240.0f;
        cable.onPointerUp(evUp);
        REQUIRE(!cable.isDragging());
    }

    std::cout << "  [PASS] PointerEvent normalization validated." << std::endl;
}

void testScrollPhysicsPresenter() {
    std::cout << "[Test 12] ScrollPhysicsPresenter kinetic flings, decay, edge clamping, bounce & thumb..." << std::endl;

    ScrollPhysicsPresenter scroller;
    REQUIRE_NEAR(scroller.getOffset(), 0.0f, 1e-4f);
    REQUIRE_NEAR(scroller.getVelocity(), 0.0f, 1e-4f);
    REQUIRE(!scroller.canScroll());
    REQUIRE(!scroller.isGliding());
    REQUIRE(!scroller.isMoving());
    REQUIRE_NEAR(scroller.getNormalizedThumbSize(), 1.0f, 1e-4f);
    REQUIRE_NEAR(scroller.getNormalizedThumbPosition(), 0.0f, 1e-4f);

    // 1. Content and viewport configuration
    scroller.setContentAndViewport(1000.0f, 200.0f);
    REQUIRE(scroller.canScroll());
    REQUIRE_NEAR(scroller.getMinOffset(), 0.0f, 1e-4f);
    REQUIRE_NEAR(scroller.getMaxOffset(), 800.0f, 1e-4f);
    REQUIRE_NEAR(scroller.getContentLength(), 1000.0f, 1e-4f);
    REQUIRE_NEAR(scroller.getViewportLength(), 200.0f, 1e-4f);

    // 2. Scrollbar thumb size & position calculations
    // Normalized thumb size: 200 / 1000 = 0.2f
    REQUIRE_NEAR(scroller.getNormalizedThumbSize(), 0.2f, 1e-4f);
    REQUIRE_NEAR(scroller.getNormalizedThumbPosition(), 0.0f, 1e-4f);
    REQUIRE_NEAR(scroller.getThumbSize(400.0f), 80.0f, 1e-4f);
    REQUIRE_NEAR(scroller.getThumbPosition(400.0f), 0.0f, 1e-4f);

    // Scroll halfway (400 / 800 = 0.5)
    scroller.scrollTo(400.0f);
    REQUIRE_NEAR(scroller.getOffset(), 400.0f, 1e-4f);
    // travelRange = 1.0 - 0.2 = 0.8. Pos = 0.5 * 0.8 = 0.4f
    REQUIRE_NEAR(scroller.getNormalizedThumbPosition(), 0.4f, 1e-4f);
    REQUIRE_NEAR(scroller.getThumbPosition(400.0f), 160.0f, 1e-4f);

    // Scroll to end (800 / 800 = 1.0)
    scroller.scrollTo(800.0f);
    REQUIRE_NEAR(scroller.getNormalizedThumbPosition(), 0.8f, 1e-4f);
    REQUIRE_NEAR(scroller.getThumbPosition(400.0f), 320.0f, 1e-4f);
    // Right edge of thumb at 320 + 80 = 400px (exact track end)
    REQUIRE_NEAR(scroller.getThumbPosition(400.0f) + scroller.getThumbSize(400.0f), 400.0f, 1e-4f);

    // 3. Scroll from thumb interactions
    scroller.scrollFromThumbNormalized(0.4f);
    REQUIRE_NEAR(scroller.getOffset(), 400.0f, 1e-4f);

    scroller.scrollFromThumbPixels(320.0f, 400.0f);
    REQUIRE_NEAR(scroller.getOffset(), 800.0f, 1e-4f);

    scroller.scrollFromThumbPixels(0.0f, 400.0f);
    REQUIRE_NEAR(scroller.getOffset(), 0.0f, 1e-4f);

    // 4. Kinetic flings & velocity friction decay
    scroller.scrollTo(100.0f);
    scroller.setFriction(0.92f);
    scroller.fling(600.0f);
    REQUIRE(scroller.isGliding());
    REQUIRE(scroller.isMoving());
    REQUIRE_NEAR(scroller.getVelocity(), 600.0f, 1e-4f);

    float initialOffset = scroller.getOffset();
    scroller.step(0.016f); // 1 frame
    REQUIRE(scroller.getOffset() > initialOffset);
    REQUIRE(scroller.getVelocity() < 600.0f); // Decayed by friction

    // Step until momentum fully decays to rest
    for (int i = 0; i < 120 && scroller.isGliding(); ++i) {
        scroller.step(0.016f);
    }
    REQUIRE(!scroller.isGliding());
    REQUIRE_NEAR(scroller.getVelocity(), 0.0f, 1e-4f);

    // 5. Hard edge clamping (without bounce)
    scroller.setOverscrollBounce(false);
    scroller.scrollTo(50.0f);
    scroller.scrollBy(-100.0f);
    REQUIRE_NEAR(scroller.getOffset(), 0.0f, 1e-4f); // Clamped at min

    scroller.scrollBy(1200.0f);
    REQUIRE_NEAR(scroller.getOffset(), 800.0f, 1e-4f); // Clamped at max

    // Flinging towards boundary with bounce disabled halts at boundary
    scroller.scrollTo(750.0f);
    scroller.fling(2000.0f);
    for (int i = 0; i < 60; ++i) {
        scroller.step(0.016f);
    }
    REQUIRE_NEAR(scroller.getOffset(), 800.0f, 1e-4f);
    REQUIRE_NEAR(scroller.getVelocity(), 0.0f, 1e-4f);

    // 6. Overscroll spring bounce
    scroller.setOverscrollBounce(true);
    scroller.scrollTo(800.0f);

    // Direct drag past boundary with rubber-band resistance
    scroller.scrollBy(40.0f);
    REQUIRE(scroller.getOffset() > 800.0f);
    float overAfterFirst = scroller.getOffset();
    scroller.scrollBy(40.0f);
    float overAfterSecond = scroller.getOffset();
    // Second step had more resistance than first
    REQUIRE((overAfterSecond - overAfterFirst) < (overAfterFirst - 800.0f));

    // Release and let spring damp back to boundary
    REQUIRE(scroller.isMoving());
    for (int i = 0; i < 90; ++i) {
        scroller.step(0.016f);
    }
    REQUIRE_NEAR(scroller.getOffset(), 800.0f, 0.15f);
    REQUIRE(!scroller.isMoving());
    REQUIRE_NEAR(scroller.getVelocity(), 0.0f, 1e-4f);

    // Negative boundary spring test
    scroller.setOffset(-60.0f);
    REQUIRE(scroller.isMoving());
    for (int i = 0; i < 90; ++i) {
        scroller.step(0.016f);
    }
    REQUIRE_NEAR(scroller.getOffset(), 0.0f, 0.15f);
    REQUIRE(!scroller.isMoving());

    // 7. Interactive drag session flag
    scroller.startDrag();
    REQUIRE(scroller.isDragging());
    scroller.step(0.016f); // Should not move while user holds drag
    scroller.endDrag(400.0f); // Release with fling
    REQUIRE(!scroller.isDragging());
    REQUIRE(scroller.isGliding());
    scroller.stop();
    REQUIRE(!scroller.isGliding());

    std::cout << "  [PASS] ScrollPhysicsPresenter validated." << std::endl;
}

void testZoomPanPresenter() {
    std::cout << "[Test 13] ZoomPanPresenter 2D transforms, anchor-preserving zoom & pinch..." << std::endl;

    ZoomPanPresenter vp;

    // 1. Initial defaults
    REQUIRE_NEAR(vp.getZoomX(), 1.0f, 1e-4f);
    REQUIRE_NEAR(vp.getZoomY(), 1.0f, 1e-4f);
    REQUIRE_NEAR(vp.getPanX(), 0.0f, 1e-4f);
    REQUIRE_NEAR(vp.getPanY(), 0.0f, 1e-4f);
    REQUIRE_NEAR(vp.getViewportWidth(), 800.0f, 1e-4f);
    REQUIRE_NEAR(vp.getViewportHeight(), 600.0f, 1e-4f);

    // 2. Coordinate transforms at identity
    float sx = 0.0f, sy = 0.0f;
    vp.worldToScreen(150.0f, 250.0f, sx, sy);
    REQUIRE_NEAR(sx, 150.0f, 1e-4f);
    REQUIRE_NEAR(sy, 250.0f, 1e-4f);

    float wx = 0.0f, wy = 0.0f;
    vp.screenToWorld(150.0f, 250.0f, wx, wy);
    REQUIRE_NEAR(wx, 150.0f, 1e-4f);
    REQUIRE_NEAR(wy, 250.0f, 1e-4f);

    // Vec2D overload match
    auto screenVec = vp.worldToScreen(ZoomPanPresenter::Vec2D{150.0f, 250.0f});
    REQUIRE_NEAR(screenVec.x, 150.0f, 1e-4f);
    REQUIRE_NEAR(screenVec.y, 250.0f, 1e-4f);

    auto worldVec = vp.screenToWorld(screenVec);
    REQUIRE_NEAR(worldVec.x, 150.0f, 1e-4f);
    REQUIRE_NEAR(worldVec.y, 250.0f, 1e-4f);

    // 3. Viewport offset and zoom scaling
    vp.setViewport(50.0f, 40.0f, 1280.0f, 720.0f);
    vp.setPan(10.0f, 20.0f);
    vp.setZoom(2.0f, 3.0f);

    // world (30, 40) -> screen: 50 + (30 - 10) * 2 = 90; 40 + (40 - 20) * 3 = 100
    REQUIRE_NEAR(vp.worldToScreenX(30.0f), 90.0f, 1e-4f);
    REQUIRE_NEAR(vp.worldToScreenY(40.0f), 100.0f, 1e-4f);

    // Inverse
    REQUIRE_NEAR(vp.screenToWorldX(90.0f), 30.0f, 1e-4f);
    REQUIRE_NEAR(vp.screenToWorldY(100.0f), 40.0f, 1e-4f);

    // Distance helpers
    REQUIRE_NEAR(vp.worldDistanceToScreenX(15.0f), 30.0f, 1e-4f);
    REQUIRE_NEAR(vp.worldDistanceToScreenY(10.0f), 30.0f, 1e-4f);
    REQUIRE_NEAR(vp.screenDistanceToWorldX(30.0f), 15.0f, 1e-4f);
    REQUIRE_NEAR(vp.screenDistanceToWorldY(30.0f), 10.0f, 1e-4f);

    // 4. Anchor-preserving zooming (Stationary cursor during wheel zoom)
    // Setup clean viewport
    vp.setViewport(0.0f, 0.0f, 1000.0f, 800.0f);
    vp.setPan(0.0f, 0.0f);
    vp.setZoom(1.0f, 1.0f);

    float anchorScreenX = 350.0f;
    float anchorScreenY = 220.0f;

    // Record world position under cursor before zoom
    float worldBeforeX = vp.screenToWorldX(anchorScreenX);
    float worldBeforeY = vp.screenToWorldY(anchorScreenY);

    // Zoom in by factor 3.2x around (anchorScreenX, anchorScreenY)
    vp.zoomAt(anchorScreenX, anchorScreenY, 3.2f);
    REQUIRE_NEAR(vp.getZoomX(), 3.2f, 1e-4f);
    REQUIRE_NEAR(vp.getZoomY(), 3.2f, 1e-4f);

    // The cursor MUST still point to exactly the same world coordinate!
    float worldAfterX = vp.screenToWorldX(anchorScreenX);
    float worldAfterY = vp.screenToWorldY(anchorScreenY);
    REQUIRE_NEAR(worldAfterX, worldBeforeX, 1e-4f);
    REQUIRE_NEAR(worldAfterY, worldBeforeY, 1e-4f);

    // And transforming that world coordinate back to screen must yield the exact cursor location
    REQUIRE_NEAR(vp.worldToScreenX(worldBeforeX), anchorScreenX, 1e-4f);
    REQUIRE_NEAR(vp.worldToScreenY(worldBeforeY), anchorScreenY, 1e-4f);

    // Zoom out by factor 0.5x around the same anchor
    vp.zoomAt(anchorScreenX, anchorScreenY, 0.5f);
    REQUIRE_NEAR(vp.getZoomX(), 1.6f, 1e-4f);
    REQUIRE_NEAR(vp.screenToWorldX(anchorScreenX), worldBeforeX, 1e-4f);
    REQUIRE_NEAR(vp.screenToWorldY(anchorScreenY), worldBeforeY, 1e-4f);

    // Non-uniform anchor zoom with viewport offset
    vp.setViewport(80.0f, 60.0f, 800.0f, 600.0f);
    vp.setPan(50.0f, 100.0f);
    vp.setZoom(2.0f, 1.5f);

    float testAnchorX = 400.0f;
    float testAnchorY = 300.0f;
    float worldRefX = vp.screenToWorldX(testAnchorX);
    float worldRefY = vp.screenToWorldY(testAnchorY);

    vp.zoomTo(testAnchorX, testAnchorY, 4.0f, 6.0f);
    REQUIRE_NEAR(vp.screenToWorldX(testAnchorX), worldRefX, 1e-4f);
    REQUIRE_NEAR(vp.screenToWorldY(testAnchorY), worldRefY, 1e-4f);

    // 5. Multi-touch Pinch & Drag
    float pinchCenterX = 400.0f;
    float pinchCenterY = 300.0f;
    float prePinchWorldX = vp.screenToWorldX(pinchCenterX);
    float prePinchWorldY = vp.screenToWorldY(pinchCenterY);

    // Pinch scale 1.25x with screen translation (+30px, -15px)
    vp.handlePinch(pinchCenterX, pinchCenterY, 1.25f, 30.0f, -15.0f);
    // After translation, the world point should now appear at pinchCenter + (30, -15)
    REQUIRE_NEAR(vp.worldToScreenX(prePinchWorldX), pinchCenterX + 30.0f, 1e-3f);
    REQUIRE_NEAR(vp.worldToScreenY(prePinchWorldY), pinchCenterY - 15.0f, 1e-3f);

    // 6. Panning & CenterOn
    vp.setViewport(0.0f, 0.0f, 800.0f, 600.0f);
    vp.setZoom(2.0f, 2.0f);
    vp.centerOn(500.0f, 300.0f);
    // Center of viewport (400, 300) should now map to (500, 300) in world
    REQUIRE_NEAR(vp.screenToWorldX(400.0f), 500.0f, 1e-4f);
    REQUIRE_NEAR(vp.screenToWorldY(300.0f), 300.0f, 1e-4f);

    // Screen pan
    vp.panByScreen(100.0f, 50.0f);
    // Panning right by 100 screen px at zoom 2 moves world left by 50 units
    REQUIRE_NEAR(vp.screenToWorldX(400.0f), 450.0f, 1e-4f);
    REQUIRE_NEAR(vp.screenToWorldY(300.0f), 275.0f, 1e-4f);

    // 7. Visible world bounds culling query
    auto bounds = vp.getVisibleWorldBounds();
    REQUIRE_NEAR(bounds.minX, vp.screenToWorldX(0.0f), 1e-4f);
    REQUIRE_NEAR(bounds.minY, vp.screenToWorldY(0.0f), 1e-4f);
    REQUIRE_NEAR(bounds.maxX, vp.screenToWorldX(800.0f), 1e-4f);
    REQUIRE_NEAR(bounds.maxY, vp.screenToWorldY(600.0f), 1e-4f);

    // 8. Zoom limits & Pan bounds clamping
    vp.setZoomLimits(0.5f, 8.0f, 0.5f, 8.0f);
    vp.setZoom(20.0f, 0.1f);
    REQUIRE_NEAR(vp.getZoomX(), 8.0f, 1e-4f);
    REQUIRE_NEAR(vp.getZoomY(), 0.5f, 1e-4f);

    vp.setPanBounds(0.0f, 1000.0f, 0.0f, 500.0f);
    vp.setPan(-50.0f, 800.0f);
    REQUIRE_NEAR(vp.getPanX(), 0.0f, 1e-4f);
    REQUIRE_NEAR(vp.getPanY(), 500.0f, 1e-4f);

    std::cout << "  [PASS] ZoomPanPresenter validated." << std::endl;
}

void testVirtualFrameTimeContext() {
    std::cout << "[Test 14] Virtual FrameTimeContext deterministic lockstep stepping & offline simulation..." << std::endl;

    // 1. Structure defaults & conversions
    FrameTimeContext time{};
    REQUIRE_NEAR(time.songTimeSeconds, 0.0, 1e-6);
    REQUIRE(time.frameIndex == 0);
    REQUIRE(!time.isOfflineExport);
    REQUIRE_NEAR(time.fps(), 60.0, 0.1);
    REQUIRE_NEAR(time.dt(), 0.016666f, 1e-4f);

    // 2. Exact advancing
    constexpr double kFps60Dt = 1.0 / 60.0;
    for (int i = 0; i < 60; ++i) {
        time.advance(kFps60Dt);
    }
    REQUIRE(time.frameIndex == 60);
    REQUIRE_NEAR(time.songTimeSeconds, 1.0, 1e-6);

    time.advanceFrames(120, kFps60Dt);
    REQUIRE(time.frameIndex == 180);
    REQUIRE_NEAR(time.songTimeSeconds, 3.0, 1e-6);

    // 3. Deterministic lockstep stepping with ScrollPhysicsPresenter
    ScrollPhysicsPresenter scrollerA;
    scrollerA.setContentAndViewport(1000.0f, 200.0f);
    scrollerA.setFriction(0.92f);
    scrollerA.fling(1200.0f);

    ScrollPhysicsPresenter scrollerB;
    scrollerB.setContentAndViewport(1000.0f, 200.0f);
    scrollerB.setFriction(0.92f);
    scrollerB.fling(1200.0f);

    FrameTimeContext exportTime{};
    exportTime.isOfflineExport = true;
    exportTime.deltaTime = 1.0 / 60.0;

    for (int f = 0; f < 45; ++f) {
        scrollerA.step(exportTime);
        scrollerB.step(exportTime);
        exportTime.advance(1.0 / 60.0);
    }

    // Both instances stepped in virtual time MUST match to bit-level precision
    REQUIRE_NEAR(scrollerA.getOffset(), scrollerB.getOffset(), 1e-6f);
    REQUIRE_NEAR(scrollerA.getVelocity(), scrollerB.getVelocity(), 1e-6f);

    // 4. TelemetryPresenter stepping with FrameTimeContext
    TelemetryPresenter telemetry;
    telemetry.setTransport(true, 16, 120.0, 2.0);
    REQUIRE(telemetry.getTransport().isPlaying);
    REQUIRE_NEAR(telemetry.getTransport().songTimeSeconds, 2.0, 1e-4);

    // Feed custom audio metrics with virtual time context
    float dummySample = 0.75f;
    telemetry.feedCustomAudio(&dummySample, 1, 0.85f, 0.82f, 0.40f, 0.38f, exportTime);
    REQUIRE_NEAR(telemetry.getMasterMeter().peakL, 0.85f, 1e-4f);
    REQUIRE_NEAR(telemetry.getMasterMeter().peakR, 0.82f, 1e-4f);

    // Advance 30 frames with silence
    for (int f = 0; f < 30; ++f) {
        dummySample = 0.0f;
        telemetry.feedCustomAudio(&dummySample, 1, 0.0f, 0.0f, 0.0f, 0.0f, exportTime);
        exportTime.advance(1.0 / 60.0);
    }
    // Meters should decay smoothly via virtual time
    REQUIRE(telemetry.getMasterMeter().peakL < 0.10f);
    REQUIRE(telemetry.getMasterMeter().peakR < 0.10f);

    std::cout << "  [PASS] Virtual FrameTimeContext deterministic stepping validated." << std::endl;
}

int main() {
    std::cout << "=== Running Presenter Core Unit Tests ===" << std::endl;
    testPresenterBase();
    testParameterPresenter();
    testValueEditPresenter();
    testTrackPropertiesPresenter();
    testScalarDragPresenter();
    testTimelineScrubPresenter();
    testSplitterDragPresenter();
    testCablePatchPresenter();
    testMarqueeSelectPresenter();
    testTelemetryPresenter();
    testPointerEventInteraction();
    testScrollPhysicsPresenter();
    testZoomPanPresenter();
    testVirtualFrameTimeContext();
    std::cout << "=== All Presenter Tests Passed Successfully! ===" << std::endl;
    return 0;
}


