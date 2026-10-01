#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <filesystem>

#include "eatsbits/ui/dawn_bridge.hpp"
#include "eatsbits/ui/gui_window.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        std::cerr << "Assertion failed: (" #expr ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while(0)

using namespace eatsbits;
using namespace eatsbits::audio;
using namespace eatsbits::ui;

void testDawnBridgeLifecycle() {
    std::cout << "[Test] DawnBridge FBO, WGSL shader, and render pass lifecycle..." << std::endl;

    DawnBridge bridge(1024, 768);
    REQUIRE(bridge.getFboWidth() == 1024);
    REQUIRE(bridge.getFboHeight() == 768);
    REQUIRE(bridge.getShaderSource() != nullptr);
    REQUIRE(std::strlen(bridge.getShaderSource()) > 0);

    bridge.beginFrame(1024.0f, 768.0f, 1.0f);
    VectorVertex2D verts[4]{};
    uint32_t inds[6] = {0, 1, 2, 2, 3, 0};
    VectorDrawCall call{};
    bridge.submitBatch(verts, 4, inds, 6, call);
    bridge.endFrame();

    REQUIRE(bridge.getTotalFramesRendered() == 1);
    REQUIRE(bridge.getTotalVerticesSubmitted() == 4);

    std::cout << "  [PASS] DawnBridge lifecycle passed." << std::endl;
}

void testGuiWindowInteraction() {
    std::cout << "[Test] GuiWindow hit-testing, knob tweaking & cable drag-and-drop..." << std::endl;

    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.setupDefaultAcidGraph();

    // Track 1 for sequencer testing
    auto& seq = engine.getSequencer();
    seq.addTrack("303Track", 1, 16);

    GuiWindow window(1280, 800, "Test Window");
    bool initOk = window.initialize(engine);
    REQUIRE(initOk);

    // Switch to Design view for modular rack & cable patching tests
    window.setActiveView(WorkspaceView::Design);

    // 1. Test Knob hit-test (accounting for 56px top transport header offset)
    HitTestKnobResult knob = window.hitTestKnob(60.0f, 132.0f); // TB-303 Cutoff knob position
    REQUIRE(knob.hit);
    REQUIRE(knob.knobIndex == 0);
    REQUIRE(knob.knobName == "Cutoff");

    // 2. Test Jack hit-test
    const auto& modules = window.getCanvas().getModules();
    REQUIRE(!modules.empty());
    Point2D outJackPos = modules[0].outputJacks[0];
    HitTestJackResult jack = window.hitTestJack(outJackPos.x, outJackPos.y);
    REQUIRE(jack.hit);
    REQUIRE(jack.isOutput);

    // 3. Test Step Pad hit-test
    HitTestStepResult step0 = window.hitTestStep(400.0f, 700.0f);
    // Should return a valid step index if within tracker grid

    // 4. Test Mouse Drag & Drop Cable Connection
    // Start drag from output jack
    window.onMouseDown(0, outJackPos.x, outJackPos.y);
    REQUIRE(window.getDragMode() == DragMode::PatchCable);

    // Find an input jack in module 1 (Delay)
    Point2D inJackPos = modules[1].inputJacks[0];
    window.onMouseMove(inJackPos.x, inJackPos.y);

    // Drop cable onto input jack
    window.onMouseUp(0, inJackPos.x, inJackPos.y);
    REQUIRE(window.getDragMode() == DragMode::None);

    // 5. Test Knob Dragging
    const float initialKnobVal = window.getKnobValue(knob.nodeId, knob.knobIndex);
    window.onMouseDown(0, knob.position.x, knob.position.y);
    REQUIRE(window.getDragMode() == DragMode::Knob);
    window.onMouseMove(knob.position.x, knob.position.y - 30.0f); // Drag up
    const float updatedKnobVal = window.getKnobValue(knob.nodeId, knob.knobIndex);
    REQUIRE(updatedKnobVal > initialKnobVal);
    window.onMouseUp(0, knob.position.x, knob.position.y - 30.0f);
    REQUIRE(window.getDragMode() == DragMode::None);

    // 6. Test Spacebar Transport Toggle
    REQUIRE(!engine.getSequencer().isPlaying());
    window.onKeyDown(32); // Spacebar
    REQUIRE(engine.getSequencer().isPlaying());
    window.onKeyDown(32); // Spacebar
    REQUIRE(!engine.getSequencer().isPlaying());

    // 7. Test Transport Header Hit-Testing & Clicks (Flat 2D Mode)
    window.set3dConsoleEnabled(false);
    auto playHit = window.hitTestTransport(75.0f, 25.0f);
    REQUIRE(playHit.hit);
    REQUIRE(playHit.action == TransportAction::PlayPause);

    // Minimal BPM Readout Capsule Hit-Test
    auto bpmHit = window.hitTestTransport(220.0f, 25.0f);
    REQUIRE(bpmHit.hit);
    REQUIRE(bpmHit.action == TransportAction::Bpm);

    // Clicking in middle faceplate area does NOT initiate play or value changes
    auto emptyFaceplateHit = window.hitTestTransport(450.0f, 25.0f);
    REQUIRE(!emptyFaceplateHit.hit);

    // Click Play button
    window.onMouseDown(0, 75.0f, 25.0f);
    REQUIRE(engine.getSequencer().isPlaying());
    window.onMouseUp(0, 75.0f, 25.0f);

    // Click Stop button
    auto stopHit = window.hitTestTransport(115.0f, 25.0f);
    REQUIRE(stopHit.hit);
    REQUIRE(stopHit.action == TransportAction::Stop);
    window.onMouseDown(0, 115.0f, 25.0f);
    REQUIRE(!engine.getSequencer().isPlaying());
    window.onMouseUp(0, 115.0f, 25.0f);

    // Test Record button
    auto recHit = window.hitTestTransport(154.0f, 25.0f);
    REQUIRE(recHit.hit);
    REQUIRE(recHit.action == TransportAction::Record);

    // Test Lock button hit
    float rTop = static_cast<float>(window.getWidth());
    auto lockHit = window.hitTestTransport(rTop - 105.0f, 25.0f);
    REQUIRE(lockHit.hit);
    REQUIRE(lockHit.action == TransportAction::LockToggle);

    // Test BPM Scrubber Dragging on minimal capsule
    const double initialBpm = engine.getSequencer().getBpm();
    window.onMouseDown(0, 220.0f, 25.0f);
    REQUIRE(window.getDragMode() == DragMode::BpmScrubber);
    window.onMouseMove(220.0f, 5.0f); // Drag up by 20px
    REQUIRE(engine.getSequencer().getBpm() > initialBpm);
    window.onMouseUp(0, 220.0f, 5.0f);
    REQUIRE(window.getDragMode() == DragMode::None);

    // 8. Test Workspace View Navigation Bottom Bar & Hotkeys
    window.setActiveView(WorkspaceView::Arranger);
    REQUIRE(window.getActiveView() == WorkspaceView::Arranger);

    // Click Edit button in bottom panel (mid ~ 388, y = 776)
    window.onMouseDown(0, 388.0f, 776.0f);
    window.onMouseUp(0, 388.0f, 776.0f);
    REQUIRE(window.getActiveView() == WorkspaceView::Edit);

    // Click Track button in bottom panel (mid ~ 640, y = 776)
    window.onMouseDown(0, 640.0f, 776.0f);
    window.onMouseUp(0, 640.0f, 776.0f);
    REQUIRE(window.getActiveView() == WorkspaceView::Track);

    // Click Mixer button in bottom panel (mid ~ 891, y = 776)
    window.onMouseDown(0, 891.0f, 776.0f);
    window.onMouseUp(0, 891.0f, 776.0f);
    REQUIRE(window.getActiveView() == WorkspaceView::Mixer);

    // Click Design button in bottom panel (mid ~ 1142, y = 776)
    window.onMouseDown(0, 1142.0f, 776.0f);
    window.onMouseUp(0, 1142.0f, 776.0f);
    REQUIRE(window.getActiveView() == WorkspaceView::Design);

    // Click Arranger button in bottom panel (mid ~ 137, y = 776)
    window.onMouseDown(0, 137.0f, 776.0f);
    window.onMouseUp(0, 137.0f, 776.0f);
    REQUIRE(window.getActiveView() == WorkspaceView::Arranger);

    // Cycle views using Tab key (258): Arranger -> Edit -> Track -> Mixer -> Design -> Arranger
    window.onKeyDown(258);
    REQUIRE(window.getActiveView() == WorkspaceView::Edit);
    window.onKeyDown(258);
    REQUIRE(window.getActiveView() == WorkspaceView::Track);
    window.onKeyDown(258);
    REQUIRE(window.getActiveView() == WorkspaceView::Mixer);
    window.onKeyDown(258);
    REQUIRE(window.getActiveView() == WorkspaceView::Design);
    window.onKeyDown(258);
    REQUIRE(window.getActiveView() == WorkspaceView::Arranger);

    // Test Transport toggles: Metronome, Loop, Browser
    REQUIRE(!window.isMetronomeEnabled());
    window.onMouseDown(0, 1280.0f - 230.0f, 25.0f);
    window.onMouseUp(0, 1280.0f - 230.0f, 25.0f);
    REQUIRE(window.isMetronomeEnabled());

    REQUIRE(!window.isLoopEnabled());
    window.onMouseDown(0, 1280.0f - 150.0f, 25.0f);
    window.onMouseUp(0, 1280.0f - 150.0f, 25.0f);
    REQUIRE(window.isLoopEnabled());

    REQUIRE(!window.isBrowserOpen());
    window.onMouseDown(0, 1280.0f - 60.0f, 25.0f);
    window.onMouseUp(0, 1280.0f - 60.0f, 25.0f);
    REQUIRE(window.isBrowserOpen());
    window.setBrowserOpen(false);
    REQUIRE(!window.isBrowserOpen());

    // 9. Test Mixer Strip Hit-Testing and Mute/Solo Toggling
    window.setActiveView(WorkspaceView::Mixer);
    auto& strips = window.getMixerStrips();
    REQUIRE(strips.size() >= 3);
    REQUIRE(!strips[0].mute);

    // Channel 0 is at cx = 195.0f (after Master strip). Mute button at [cx + 12, 152, 50, 24] -> (220, 164)
    auto muteHit = window.hitTestMixer(220.0f, 164.0f);
    REQUIRE(muteHit.hit);
    REQUIRE(!muteHit.isMaster);
    REQUIRE(muteHit.channelIndex == 0);
    REQUIRE(muteHit.isMute);

    window.onMouseDown(0, 220.0f, 164.0f);
    window.onMouseUp(0, 220.0f, 164.0f);
    REQUIRE(strips[0].mute); // Toggled ON

    window.onMouseDown(0, 220.0f, 164.0f);
    window.onMouseUp(0, 220.0f, 164.0f);
    REQUIRE(!strips[0].mute); // Toggled OFF

    // Restore to rack view
    window.setActiveView(WorkspaceView::ModularRack);

    std::cout << "  [PASS] GuiWindow interaction tests passed." << std::endl;
}

void testPianoRollInteraction() {
    std::cout << "[Test] Piano Roll & Edit sub-view switcher interaction..." << std::endl;
    GuiWindow window(1280, 800, "Piano Roll Test");

    window.setActiveView(WorkspaceView::Edit);
    REQUIRE(window.getActiveView() == WorkspaceView::Edit);
    REQUIRE(window.getEditSubView() == EditSubView::PianoRoll);

    // 1. Test Sub-View Switcher Hit-Testing
    auto sn1 = window.hitTestEditSubNav(850.0f, 75.0f);
    REQUIRE(sn1.hit);
    REQUIRE(sn1.subView == EditSubView::PianoRoll);

    auto sn2 = window.hitTestEditSubNav(960.0f, 75.0f);
    REQUIRE(sn2.hit);
    REQUIRE(sn2.subView == EditSubView::Tracker);

    auto sn3 = window.hitTestEditSubNav(1080.0f, 75.0f);
    REQUIRE(sn3.hit);
    REQUIRE(sn3.subView == EditSubView::Score);

    auto sn4 = window.hitTestEditSubNav(1190.0f, 75.0f);
    REQUIRE(sn4.hit);
    REQUIRE(sn4.subView == EditSubView::Script);

    // Click to switch to Tracker and back to Piano Roll
    window.onMouseDown(0, 960.0f, 75.0f);
    window.onMouseUp(0, 960.0f, 75.0f);
    REQUIRE(window.getEditSubView() == EditSubView::Tracker);

    window.onMouseDown(0, 850.0f, 75.0f);
    window.onMouseUp(0, 850.0f, 75.0f);
    REQUIRE(window.getEditSubView() == EditSubView::PianoRoll);

    // 2. Test Virtual Piano Keyboard Hit-Testing
    // Top is pitch 60 (C4) at topY = 100.0f, rowH = (650 - 100) / 25 = 22.0f
    auto keyC4 = window.hitTestPianoKey(40.0f, 111.0f);
    REQUIRE(keyC4.hit);
    REQUIRE(keyC4.pitch == 60);
    REQUIRE(!keyC4.isBlackKey);

    auto keyB3 = window.hitTestPianoKey(40.0f, 133.0f);
    REQUIRE(keyB3.hit);
    REQUIRE(keyB3.pitch == 59);
    REQUIRE(!keyB3.isBlackKey);

    auto keyBb3 = window.hitTestPianoKey(40.0f, 155.0f);
    REQUIRE(keyBb3.hit);
    REQUIRE(keyBb3.pitch == 58);
    REQUIRE(keyBb3.isBlackKey);

    auto keyC2 = window.hitTestPianoKey(40.0f, 638.0f);
    REQUIRE(keyC2.hit);
    REQUIRE(keyC2.pitch == 36);
    REQUIRE(!keyC2.isBlackKey);

    // 3. Test Piano Roll Grid Hit-Testing
    auto grid0 = window.hitTestPianoRollGrid(130.0f, 111.0f);
    REQUIRE(grid0.hit);
    REQUIRE(grid0.step == 0);
    REQUIRE(grid0.pitch == 60);

    auto grid4 = window.hitTestPianoRollGrid(407.0f, 364.0f);
    REQUIRE(grid4.hit);
    REQUIRE(grid4.step == 4);
    REQUIRE(grid4.pitch == 48);

    std::cout << "  [PASS] Piano Roll & Edit sub-view switcher tests passed." << std::endl;
}

void testArrangerInteraction() {
    std::cout << "[Test] Arranger timeline ruler, track controls & clip interaction..." << std::endl;
    audio::AudioEngine engine;
    REQUIRE(engine.initialize());

    GuiWindow window(1280, 800, "Arranger Test");
    REQUIRE(window.initialize(engine));
    window.setActiveView(WorkspaceView::Arranger);
    REQUIRE(window.getActiveView() == WorkspaceView::Arranger);

    // 1. Time Ruler Hit-Testing & Scrubbing
    // trackHeaderW = 210, barW = 60. Bar 3 start = 210 + 2*60 = 330
    auto rulerHit = window.hitTestArranger(330.0f, 70.0f);
    REQUIRE(rulerHit.hit);
    REQUIRE(rulerHit.area == ArrangerHitArea::Ruler);
    REQUIRE(rulerHit.bar == 3);
    REQUIRE(rulerHit.step == 32);

    // Scrub ruler via mouse down + drag
    window.onMouseDown(0, 330.0f, 70.0f);
    REQUIRE(window.getDragMode() == DragMode::ArrangerRulerScrub);
    REQUIRE(engine.getSequencer().getTransport().getCurrentStep() == 32);

    // Drag by 1 bar (60px) to step 48
    window.onMouseMove(390.0f, 70.0f);
    REQUIRE(engine.getSequencer().getTransport().getCurrentStep() == 48);
    window.onMouseUp(0, 390.0f, 70.0f);
    REQUIRE(window.getDragMode() == DragMode::None);

    // 2. Track Header Controls (2-Row Layout):
    // Row 1: Mute, Solo, Freeze (Snowflake)
    // Row 2: Volume Slider & Meter, Track Pan Knob
    // Track 0 (TB-303): curY = 78.
    // Row 1: Y ~ 92. Mute (x: 128..153), Solo (x: 154..179), Freeze (x: 180..206)
    auto muteHit = window.hitTestArranger(140.0f, 92.0f);
    REQUIRE(muteHit.hit);
    REQUIRE(muteHit.area == ArrangerHitArea::TrackMute);
    REQUIRE(muteHit.trackIndex == 0);

    // Toggle Mute on Track 0
    window.onMouseDown(0, 140.0f, 92.0f);
    window.onMouseUp(0, 140.0f, 92.0f);
    REQUIRE(engine.getSequencer().getTrack(0)->isMuted());
    window.onMouseDown(0, 140.0f, 92.0f);
    window.onMouseUp(0, 140.0f, 92.0f);
    REQUIRE(!engine.getSequencer().getTrack(0)->isMuted());

    // Solo button on Track 0 (x: 154..179)
    auto soloHit = window.hitTestArranger(165.0f, 92.0f);
    REQUIRE(soloHit.hit);
    REQUIRE(soloHit.area == ArrangerHitArea::TrackSolo);
    REQUIRE(soloHit.trackIndex == 0);

    // Toggle Solo on Track 0
    window.onMouseDown(0, 165.0f, 92.0f);
    window.onMouseUp(0, 165.0f, 92.0f);
    REQUIRE(engine.getSequencer().getTrack(0)->isSolo());
    window.onMouseDown(0, 165.0f, 92.0f);
    window.onMouseUp(0, 165.0f, 92.0f);
    REQUIRE(!engine.getSequencer().getTrack(0)->isSolo());

    // Freeze button on Track 0 (x: 180..206)
    auto freezeHit = window.hitTestArranger(192.0f, 92.0f);
    REQUIRE(freezeHit.hit);
    REQUIRE(freezeHit.area == ArrangerHitArea::TrackFreeze);
    REQUIRE(freezeHit.trackIndex == 0);
    window.onMouseDown(0, 192.0f, 92.0f);
    window.onMouseUp(0, 192.0f, 92.0f);
    REQUIRE(window.getArrangerTracks()[0].freeze);
    window.onMouseDown(0, 192.0f, 92.0f);
    window.onMouseUp(0, 192.0f, 92.0f);
    REQUIRE(!window.getArrangerTracks()[0].freeze);

    // Row 2: Volume Slider & Meter on Track 0 (x: 36..156, y: 116..142)
    auto volHit = window.hitTestArranger(90.0f, 130.0f);
    REQUIRE(volHit.hit);
    REQUIRE(volHit.area == ArrangerHitArea::TrackVolume);
    window.onMouseDown(0, 90.0f, 130.0f);
    REQUIRE(window.getDragMode() == DragMode::ArrangerTrackVolume);
    window.onMouseMove(120.0f, 130.0f);
    window.onMouseUp(0, 120.0f, 130.0f);
    REQUIRE(window.getDragMode() == DragMode::None);

    // Row 2: Track Pan Knob on Track 0 (x: 168..202, y: 116..142)
    auto panHit = window.hitTestArranger(184.0f, 129.0f);
    REQUIRE(panHit.hit);
    REQUIRE(panHit.area == ArrangerHitArea::TrackPan);
    window.onMouseDown(0, 184.0f, 129.0f);
    REQUIRE(window.getDragMode() == DragMode::ArrangerTrackPan);
    window.onMouseMove(194.0f, 120.0f);
    window.onMouseUp(0, 194.0f, 120.0f);
    REQUIRE(window.getDragMode() == DragMode::None);

    // Track Header double-click -> Jump to Piano Roll Editor
    window.onMouseDown(0, 50.0f, 92.0f);
    window.onMouseUp(0, 50.0f, 92.0f);
    window.onMouseDown(0, 50.0f, 92.0f);
    window.onMouseUp(0, 50.0f, 92.0f);
    REQUIRE(window.getActiveView() == WorkspaceView::Edit);
    REQUIRE(window.getSelectedTrackIndex() == 0);

    // 3. Track Lane Clip Hit-Testing & Navigation
    window.setActiveView(WorkspaceView::Arranger);
    // Track 1 (808 Drums): curY = 84 + 76 = 160. Clip at bar 2 (x = 300)
    auto clipHit = window.hitTestArranger(300.0f, 185.0f);
    REQUIRE(clipHit.hit);
    REQUIRE(clipHit.area == ArrangerHitArea::Clip);
    REQUIRE(clipHit.trackIndex == 1);

    // Click clip -> selects track 1, sets transport, and opens Clip Properties sidebar
    window.onMouseDown(0, 300.0f, 185.0f);
    window.onMouseUp(0, 300.0f, 185.0f);
    REQUIRE(window.isArrangerPropertiesExpanded());
    REQUIRE(window.getArrangerInspectorTab() == ArrangerInspectorTab::Clip);
    REQUIRE(window.getSelectedArrangerClipTrack() == 1);
    REQUIRE(window.getSelectedTrackIndex() == 1);

    // Double-click clip -> jumps to Edit view (Piano roll / Tracker)
    window.onMouseDown(0, 300.0f, 185.0f);
    window.onMouseUp(0, 300.0f, 185.0f);
    REQUIRE(window.getActiveView() == WorkspaceView::Edit);

    // 4. Minimap Overview Scrollbar
    window.setActiveView(WorkspaceView::Arranger);
    const float miniY = 800.0f - 48.0f - 26.0f; // 726
    auto miniHit = window.hitTestArranger(400.0f, miniY + 10.0f);
    REQUIRE(miniHit.hit);
    REQUIRE(miniHit.area == ArrangerHitArea::OverviewScrollbar);

    window.onMouseDown(0, 400.0f, miniY + 10.0f);
    REQUIRE(window.getDragMode() == DragMode::ArrangerOverviewScroll);
    window.onMouseUp(0, 400.0f, miniY + 10.0f);
    REQUIRE(window.getDragMode() == DragMode::None);

    std::cout << "  [PASS] Arranger timeline, track controls & clip interaction tests passed." << std::endl;
}

void testArrangerPropertiesSidebar() {
    std::cout << "[Test] Arranger Track & Clip Properties Sidebar (Pull-Tab, Drag-to-Resize, Swatches, Transpose)..." << std::endl;
    audio::AudioEngine engine;
    REQUIRE(engine.initialize());

    GuiWindow window(1280, 800, "Sidebar Test");
    REQUIRE(window.initialize(engine));
    window.setActiveView(WorkspaceView::Arranger);

    // 1. Initial State: Sidebar collapsed, 24px pull tab at right edge
    REQUIRE(!window.isArrangerPropertiesExpanded());
    REQUIRE(window.getArrangerPropertiesWidth() == 300.0f);

    // Pull tab X position: w - kArrangerPullTabW = 1280 - 24 = 1256
    auto pullTabHit = window.hitTestArranger(1265.0f, 200.0f);
    REQUIRE(pullTabHit.hit);
    REQUIRE(pullTabHit.area == ArrangerHitArea::SidebarPullTab);

    // Click pull tab to expand
    window.onMouseDown(0, 1265.0f, 200.0f);
    REQUIRE(window.getDragMode() == DragMode::ArrangerPropertiesResize);
    window.onMouseUp(0, 1265.0f, 200.0f);
    REQUIRE(window.isArrangerPropertiesExpanded());

    // 2. Drag pull tab to resize width
    // With 300px width expanded: pull tab is at 1280 - (24 + 300) = 956. Pull tab X in [956, 980]
    auto pullTabExpandedHit = window.hitTestArranger(965.0f, 200.0f);
    REQUIRE(pullTabExpandedHit.hit);
    REQUIRE(pullTabExpandedHit.area == ArrangerHitArea::SidebarPullTab);

    // Drag left by 50px -> increases width to 350px
    window.onMouseDown(0, 965.0f, 200.0f);
    window.onMouseMove(915.0f, 200.0f);
    REQUIRE(window.getArrangerPropertiesWidth() == 350.0f);
    window.onMouseUp(0, 915.0f, 200.0f);
    REQUIRE(window.getArrangerPropertiesWidth() == 350.0f);
    REQUIRE(window.isArrangerPropertiesExpanded());

    // 3. Tab Switching: [ TRACK ] and [ CLIP ]
    // Chassis X starts at 1280 - 350 = 930.
    // Tabs Y in [100, 128]: halfW = (350 - 26)/2 = 162. Tab1 X: [940, 1102], Tab2 X: [1106, 1268]
    auto tabClipHit = window.hitTestArranger(1150.0f, 110.0f);
    REQUIRE(tabClipHit.hit);
    REQUIRE(tabClipHit.area == ArrangerHitArea::SidebarTabClip);

    window.onMouseDown(0, 1150.0f, 110.0f);
    window.onMouseUp(0, 1150.0f, 110.0f);
    REQUIRE(window.getArrangerInspectorTab() == ArrangerInspectorTab::Clip);

    auto tabTrackHit = window.hitTestArranger(1000.0f, 110.0f);
    REQUIRE(tabTrackHit.hit);
    REQUIRE(tabTrackHit.area == ArrangerHitArea::SidebarTabTrack);

    window.onMouseDown(0, 1000.0f, 110.0f);
    window.onMouseUp(0, 1000.0f, 110.0f);
    REQUIRE(window.getArrangerInspectorTab() == ArrangerInspectorTab::Track);

    // 4. Color Swatches: Select Pink (Swatch 3)
    // Swatches Y in [196 + 20..196 + 44] = [216, 240]
    // Card X = 940. Swatch step = (350 - 24) / 8 = 40.75. Swatch 3 X = 940 + 12 + 3 * 40.75 + 20 = 1094
    auto swatchHit = window.hitTestArranger(1094.0f, 225.0f);
    REQUIRE(swatchHit.hit);
    REQUIRE(swatchHit.area == ArrangerHitArea::SidebarColorSwatch);
    REQUIRE(swatchHit.colorSwatchIndex == 3);

    window.onMouseDown(0, 1094.0f, 225.0f);
    window.onMouseUp(0, 1094.0f, 225.0f);
    REQUIRE(window.getArrangerTracks()[0].r == 1.0f);
    REQUIRE(window.getArrangerTracks()[0].g == 0.0f);

    // 5. Select a Clip -> verifies Clip Inspector populated
    window.selectArrangerClip(0, 0);
    REQUIRE(window.getArrangerInspectorTab() == ArrangerInspectorTab::Clip);
    REQUIRE(window.getSelectedArrangerClipIndex() == 0);

    // Toggle Loop on Clip
    auto loopHit = window.hitTestArranger(1220.0f, 230.0f);
    REQUIRE(loopHit.hit);
    REQUIRE(loopHit.area == ArrangerHitArea::SidebarClipLoopToggle);

    bool initialLoop = window.getArrangerTracks()[0].clips[0].isLooped;
    window.onMouseDown(0, 1220.0f, 230.0f);
    window.onMouseUp(0, 1220.0f, 230.0f);
    REQUIRE(window.getArrangerTracks()[0].clips[0].isLooped != initialLoop);

    // Transpose Clip: button [+1] (index 2)
    auto transHit = window.hitTestArranger(1105.0f, 315.0f);
    REQUIRE(transHit.hit);
    REQUIRE(transHit.area == ArrangerHitArea::SidebarClipTranspose);
    REQUIRE(transHit.transposeDelta == 1);

    window.onMouseDown(0, 1105.0f, 315.0f);
    window.onMouseUp(0, 1105.0f, 315.0f);
    REQUIRE(window.getArrangerTracks()[0].clips[0].transposeSemitones == 1);

    // 6. Close Button [X] to collapse sidebar
    auto closeHit = window.hitTestArranger(1260.0f, 70.0f);
    REQUIRE(closeHit.hit);
    REQUIRE(closeHit.area == ArrangerHitArea::SidebarCloseButton);

    window.onMouseDown(0, 1260.0f, 70.0f);
    window.onMouseUp(0, 1260.0f, 70.0f);
    REQUIRE(!window.isArrangerPropertiesExpanded());

    std::cout << "  [PASS] Arranger Track & Clip Properties Sidebar tests passed." << std::endl;
}

void testMixerInteraction() {
    std::cout << "[Test] Studio Mixer Console: Master Strip, faders, Mute/Solo sync & [EDIT] jump..." << std::endl;

    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.setupDefaultAcidGraph();
    engine.initialize(cfg);

    GuiWindow window(1280, 800, "Mixer Test");
    window.initialize(engine);
    window.setActiveView(WorkspaceView::Mixer);

    // 1. Pinned Master Bus Strip Hit-Testing & Interaction (X: 40..175, Y: 110..640)
    // Master Mute Button at [54, 152, 50, 24] -> center (79, 164)
    auto masterMuteHit = window.hitTestMixer(79.0f, 164.0f);
    REQUIRE(masterMuteHit.hit);
    REQUIRE(masterMuteHit.isMaster);
    REQUIRE(masterMuteHit.isMute);

    REQUIRE(!window.isMasterMuted());
    window.onMouseDown(0, 79.0f, 164.0f);
    window.onMouseUp(0, 79.0f, 164.0f);
    REQUIRE(window.isMasterMuted());
    window.onMouseDown(0, 79.0f, 164.0f);
    window.onMouseUp(0, 79.0f, 164.0f);
    REQUIRE(!window.isMasterMuted());

    // Master Pan Pot at (107, 215)
    auto masterPanHit = window.hitTestMixer(107.0f, 215.0f);
    REQUIRE(masterPanHit.hit);
    REQUIRE(masterPanHit.isMaster);
    REQUIRE(masterPanHit.isPan);

    window.onMouseDown(0, 107.0f, 215.0f);
    REQUIRE(window.getDragMode() == DragMode::MasterPan);
    window.onMouseMove(107.0f, 190.0f); // drag upward
    REQUIRE(window.getMasterPan() > 0.0f);
    window.onMouseUp(0, 107.0f, 190.0f);
    REQUIRE(window.getDragMode() == DragMode::None);

    // Master Long-Throw Fader at [65, 265, 50, 290] -> (90, 400)
    auto masterFaderHit = window.hitTestMixer(90.0f, 400.0f);
    REQUIRE(masterFaderHit.hit);
    REQUIRE(masterFaderHit.isMaster);
    REQUIRE(masterFaderHit.isFader);

    float initVol = window.getMasterVolume();
    window.onMouseDown(0, 90.0f, 400.0f);
    REQUIRE(window.getDragMode() == DragMode::MasterFader);
    window.onMouseMove(90.0f, 370.0f); // drag upward
    REQUIRE(window.getMasterVolume() > initVol);
    window.onMouseUp(0, 90.0f, 370.0f);
    REQUIRE(window.getDragMode() == DragMode::None);

    // 2. Channel Strips (X starts at 195, stripW = 135, gap = 16)
    // Channel 0 (TB-303 Acid): cx = 195
    // MUTE at [cx + 12, 152, 50, 24] -> (220, 164)
    auto ch0MuteHit = window.hitTestMixer(220.0f, 164.0f);
    REQUIRE(ch0MuteHit.hit);
    REQUIRE(!ch0MuteHit.isMaster);
    REQUIRE(ch0MuteHit.channelIndex == 0);
    REQUIRE(ch0MuteHit.isMute);

    window.onMouseDown(0, 220.0f, 164.0f);
    window.onMouseUp(0, 220.0f, 164.0f);
    REQUIRE(window.getMixerStrips()[0].mute);
    REQUIRE(engine.getSequencer().getTrack(0)->isMuted());

    window.onMouseDown(0, 220.0f, 164.0f);
    window.onMouseUp(0, 220.0f, 164.0f);
    REQUIRE(!window.getMixerStrips()[0].mute);
    REQUIRE(!engine.getSequencer().getTrack(0)->isMuted());

    // SOLO at [cx + 68, 152, 50, 24] -> (275, 164)
    auto ch0SoloHit = window.hitTestMixer(275.0f, 164.0f);
    REQUIRE(ch0SoloHit.hit);
    REQUIRE(ch0SoloHit.channelIndex == 0);
    REQUIRE(ch0SoloHit.isSolo);

    window.onMouseDown(0, 275.0f, 164.0f);
    window.onMouseUp(0, 275.0f, 164.0f);
    REQUIRE(window.getMixerStrips()[0].solo);
    REQUIRE(engine.getSequencer().getTrack(0)->isSolo());

    window.onMouseDown(0, 275.0f, 164.0f);
    window.onMouseUp(0, 275.0f, 164.0f);
    REQUIRE(!window.getMixerStrips()[0].solo);
    REQUIRE(!engine.getSequencer().getTrack(0)->isSolo());

    // Channel Fader at [cx + 25, 265, 45, 290] -> (240, 400)
    auto ch0FaderHit = window.hitTestMixer(240.0f, 400.0f);
    REQUIRE(ch0FaderHit.hit);
    REQUIRE(ch0FaderHit.isFader);
    REQUIRE(ch0FaderHit.channelIndex == 0);

    window.onMouseDown(0, 240.0f, 400.0f);
    REQUIRE(window.getDragMode() == DragMode::MixerFader);
    window.onMouseMove(240.0f, 380.0f);
    window.onMouseUp(0, 240.0f, 380.0f);
    REQUIRE(window.getDragMode() == DragMode::None);

    // Channel Quick [ EDIT ] Button at [cx + 20, 602, 95, 24] -> (250, 614)
    auto ch0EditHit = window.hitTestMixer(250.0f, 614.0f);
    REQUIRE(ch0EditHit.hit);
    REQUIRE(ch0EditHit.isEditButton);
    REQUIRE(ch0EditHit.channelIndex == 0);

    // Mixer clicks must NOT link to EDIT view (Eatsbeats parity): stays in Mixer view
    window.onMouseDown(0, 250.0f, 614.0f);
    window.onMouseUp(0, 250.0f, 614.0f);
    REQUIRE(window.getActiveView() == WorkspaceView::Mixer);
    REQUIRE(window.getSelectedTrackIndex() == 0);

    // Channel 1 (808 Drums): cx = 195 + (135 + 16) = 346
    // Test clicking stays in Mixer view and selects track 1
    window.setActiveView(WorkspaceView::Mixer);
    auto ch1EditHit = window.hitTestMixer(346.0f + 50.0f, 614.0f);
    REQUIRE(ch1EditHit.hit);
    REQUIRE(ch1EditHit.channelIndex == 1);

    window.onMouseDown(0, 346.0f + 50.0f, 614.0f);
    window.onMouseUp(0, 346.0f + 50.0f, 614.0f);
    REQUIRE(window.getActiveView() == WorkspaceView::Mixer);
    REQUIRE(window.getSelectedTrackIndex() == 1);

    std::cout << "  [PASS] Studio Mixer Console Master & Channel strip tests passed." << std::endl;
}

void testTrackTabInteraction() {
    std::cout << "[Test] Track Workspace: top selector tabs & preset synchronization..." << std::endl;

    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.setupDefaultAcidGraph();
    engine.initialize(cfg);

    GuiWindow window(1280, 800, "Track Tab Test");
    window.initialize(engine);
    window.setActiveView(WorkspaceView::Track);

    // Tab 0: [40, 60, 120, 30] -> center (80, 75)
    auto tab0Hit = window.hitTestTrackTab(80.0f, 75.0f);
    REQUIRE(tab0Hit.hit);
    REQUIRE(tab0Hit.trackIndex == 0);

    window.onMouseDown(0, 80.0f, 75.0f);
    window.onMouseUp(0, 80.0f, 75.0f);
    REQUIRE(window.getSelectedTrackIndex() == 0);
    const auto* p0 = window.getActivePreset();
    REQUIRE(p0 != nullptr);
    REQUIRE(p0->metadata.id.find("303") != std::string::npos || p0->metadata.engineId.find("tb303") != std::string::npos);

    // Tab 1: [40 + 128, 60, 120, 30] -> center (200, 75)
    auto tab1Hit = window.hitTestTrackTab(200.0f, 75.0f);
    REQUIRE(tab1Hit.hit);
    REQUIRE(tab1Hit.trackIndex == 1);

    window.onMouseDown(0, 200.0f, 75.0f);
    window.onMouseUp(0, 200.0f, 75.0f);
    REQUIRE(window.getSelectedTrackIndex() == 1);
    const auto* p1 = window.getActivePreset();
    REQUIRE(p1 != nullptr);
    REQUIRE(p1->metadata.id.find("808") != std::string::npos || p1->metadata.name.find("808") != std::string::npos);

    // Tab 3: [40 + 3*128, 60, 120, 30] -> center (460, 75)
    auto tab3Hit = window.hitTestTrackTab(460.0f, 75.0f);
    REQUIRE(tab3Hit.hit);
    REQUIRE(tab3Hit.trackIndex == 3);

    window.onMouseDown(0, 460.0f, 75.0f);
    window.onMouseUp(0, 460.0f, 75.0f);
    REQUIRE(window.getSelectedTrackIndex() == 3);
    const auto* p3 = window.getActivePreset();
    REQUIRE(p3 != nullptr);
    REQUIRE(p3->metadata.id.find("dx7") != std::string::npos || p3->metadata.engineId.find("dx7") != std::string::npos);

    std::cout << "  [PASS] Track workspace selector tabs & preset synchronization passed." << std::endl;
}

void testPresetBrowserAndProjectActions() {
    std::cout << "[Test] Preset Browser Drawer, category filtering, hot-swapping & project I/O..." << std::endl;

    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.setupDefaultAcidGraph();
    engine.initialize(cfg);

    GuiWindow window(1280, 800, "Browser Test");
    window.initialize(engine);

    // 1. Open Browser via Top Transport Button [1280 - 105..1280 - 20, 11..45] -> (1220, 25)
    REQUIRE(!window.isBrowserOpen());
    window.onMouseDown(0, 1220.0f, 25.0f);
    window.onMouseUp(0, 1220.0f, 25.0f);
    REQUIRE(window.isBrowserOpen());

    const float drW = 440.0f;
    const float drX = 1280.0f - drW - 16.0f; // 824.0f
    const float drY = 60.0f;

    // 2. Test Close [ X ] Button at [drX + drW - 32, drY + 8, 24, 24] -> (1240, 75)
    auto closeHit = window.hitTestBrowser(1240.0f, 75.0f);
    REQUIRE(closeHit.hit);
    REQUIRE(closeHit.action == BrowserHitAction::Close);

    window.onMouseDown(0, 1240.0f, 75.0f);
    window.onMouseUp(0, 1240.0f, 75.0f);
    REQUIRE(!window.isBrowserOpen());

    // 3. Test 'B' Keyboard Shortcut Toggle
    window.onKeyDown(66); // 'B'
    REQUIRE(window.isBrowserOpen());

    // 4. Test Escape key close
    window.onKeyDown(256); // Escape
    REQUIRE(!window.isBrowserOpen());

    // Reopen for interactive tests
    window.toggleBrowser();
    REQUIRE(window.isBrowserOpen());

    // 5. Test Category Filter Selection Pills [drY + 76..drY + 102]
    // Pill 1: "BASS" at drX + 16 + 1*(58+8) = drX + 82 -> (920, 146)
    auto bassHit = window.hitTestBrowser(drX + 85.0f, drY + 88.0f);
    REQUIRE(bassHit.hit);
    REQUIRE(bassHit.action == BrowserHitAction::CategorySelect);
    REQUIRE(bassHit.category == "BASS");

    window.onMouseDown(0, drX + 85.0f, drY + 88.0f);
    window.onMouseUp(0, drX + 85.0f, drY + 88.0f);
    REQUIRE(window.getBrowserCategory() == "BASS");

    // Reset to "ALL"
    window.onMouseDown(0, drX + 25.0f, drY + 88.0f);
    window.onMouseUp(0, drX + 25.0f, drY + 88.0f);
    REQUIRE(window.getBrowserCategory() == "ALL");

    // 6. Test Preset Row Hit & Hot-Swapping into Track
    window.setSelectedTrackIndex(0);
    // Row 0 [drX + 16, drY + 110, drW - 32, 34]
    auto row0Hit = window.hitTestBrowser(drX + 100.0f, drY + 124.0f);
    REQUIRE(row0Hit.hit);
    REQUIRE(row0Hit.action == BrowserHitAction::PresetSelect);

    // [LOAD] button on Row 2 (Yamaha DX7)
    // Row 2 Y = drY + 110 + 2 * (34 + 4) = drY + 186
    auto load2Hit = window.hitTestBrowser(drX + drW - 40.0f, drY + 186.0f + 12.0f);
    REQUIRE(load2Hit.hit);
    REQUIRE(load2Hit.action == BrowserHitAction::PresetLoad);

    window.onMouseDown(0, drX + drW - 40.0f, drY + 186.0f + 12.0f);
    window.onMouseUp(0, drX + drW - 40.0f, drY + 186.0f + 12.0f);

    const auto* activeP = window.getActivePreset();
    REQUIRE(activeP != nullptr);
    REQUIRE(activeP->metadata.id.find("dx7") != std::string::npos || activeP->metadata.engineId.find("dx7") != std::string::npos);
    REQUIRE(window.getMixerStrips()[0].name == activeP->metadata.name);

    // 7. Test Project Save, Load and Audio Bounce
    std::string testProj = "test_ui_project.eats";
    std::string testWav = "test_ui_bounce.wav";

    bool saveOk = window.saveProjectToFile(testProj);
    REQUIRE(saveOk);
    REQUIRE(std::filesystem::exists(testProj));
    REQUIRE(window.getLastStatusMessage().find("PROJECT SAVED") != std::string::npos);

    bool loadOk = window.loadProjectFromFile(testProj);
    REQUIRE(loadOk);
    REQUIRE(window.getLastStatusMessage().find("LOADED PROJECT") != std::string::npos);

    bool bounceOk = window.bounceMasterToWav(testWav);
    REQUIRE(bounceOk);
    REQUIRE(std::filesystem::exists(testWav));
    REQUIRE(window.getLastStatusMessage().find("FAST-BOUNCE") != std::string::npos);

    // Clean up temporary test files
    try {
        std::filesystem::remove(testProj);
        std::filesystem::remove(testWav);
    } catch (...) {}

    std::cout << "  [PASS] Preset browser, hot-swapping & project file I/O tests passed." << std::endl;
}

void testTrackerInteraction() {
    std::cout << "[Test] FastTracker 2 / Renoise vertical Tracker matrix interaction..." << std::endl;
    audio::AudioEngine engine;
    REQUIRE(engine.initialize());
    GuiWindow window(1280, 800, "Tracker Test");
    REQUIRE(window.initialize(engine));

    // 1. Switch to WorkspaceView::Edit and EditSubView::Tracker
    window.setActiveView(WorkspaceView::Edit);
    window.setEditSubView(EditSubView::Tracker);
    REQUIRE(window.getActiveView() == WorkspaceView::Edit);
    REQUIRE(window.getEditSubView() == EditSubView::Tracker);

    // 2. Test QWERTY key to MIDI pitch mapping
    REQUIRE(GuiWindow::qwertyKeyToMidiPitch(90, 4) == 60); // 'Z' -> C4 (60)
    REQUIRE(GuiWindow::qwertyKeyToMidiPitch(83, 4) == 61); // 'S' -> C#4 (61)
    REQUIRE(GuiWindow::qwertyKeyToMidiPitch(88, 4) == 62); // 'X' -> D4 (62)
    REQUIRE(GuiWindow::qwertyKeyToMidiPitch(67, 4) == 64); // 'C' -> E4 (64)
    REQUIRE(GuiWindow::qwertyKeyToMidiPitch(81, 4) == 72); // 'Q' -> C5 (72)
    REQUIRE(GuiWindow::qwertyKeyToMidiPitch(73, 4) == 84); // 'I' -> C6 (84)

    // 3. Test Tracker Hit Testing (Header Mute / Solo & Rows)
    auto headerHit = window.hitTestTracker(150.0f, 150.0f);
    REQUIRE(headerHit.hit);
    REQUIRE(headerHit.isHeader);
    REQUIRE(headerHit.trackIndex == 0);

    // Tracker Row hit test (Y = 180 is row 0 if startRow = 0)
    window.setTrackerSelectedRow(0);
    window.setTrackerFollowPlayback(false);
    auto cellHit = window.hitTestTracker(150.0f, 180.0f);
    REQUIRE(cellHit.hit);
    REQUIRE(!cellHit.isHeader);
    REQUIRE(cellHit.trackIndex == 0);

    // 4. Test Mouse Click Selecting Row & Track
    window.onMouseDown(0, 350.0f, 204.0f); // Track 1, Row 1
    window.onMouseUp(0, 350.0f, 204.0f);
    REQUIRE(window.getTrackerSelectedTrack() == 1);
    REQUIRE(window.getSelectedTrackIndex() == 1);

    // 5. Test Keyboard Navigation (Arrows & Page Up/Down)
    window.setTrackerSelectedRow(10);
    window.setTrackerSelectedTrack(0);

    // Down Arrow (264)
    window.onKeyDown(264);
    REQUIRE(window.getTrackerSelectedRow() == 11);

    // Up Arrow (265)
    window.onKeyDown(265);
    REQUIRE(window.getTrackerSelectedRow() == 10);

    // Right Arrow (262)
    window.onKeyDown(262);
    REQUIRE(window.getTrackerSelectedTrack() == 1);

    // Left Arrow (263)
    window.onKeyDown(263);
    REQUIRE(window.getTrackerSelectedTrack() == 0);

    // Page Down (267)
    window.onKeyDown(267);
    REQUIRE(window.getTrackerSelectedRow() == 26);

    // Page Up (266)
    window.onKeyDown(266);
    REQUIRE(window.getTrackerSelectedRow() == 10);

    // 6. Test QWERTY Note Entry and Step Modification
    window.setTrackerSelectedRow(4);
    window.setTrackerSelectedTrack(0);
    auto* tr = engine.getSequencer().getTrack(0);
    REQUIRE(tr != nullptr);

    // Press 'Z' (90) -> writes C4 (60) to step 4, advances cursor to step 5
    window.onKeyDown(90);
    auto st4 = tr->getStep(4);
    REQUIRE(st4.active);
    REQUIRE(st4.note == 60);
    REQUIRE(window.getTrackerSelectedRow() == 5);

    // Press 'X' (88) -> writes D4 (62) to step 5, advances cursor to step 6
    window.onKeyDown(88);
    auto st5 = tr->getStep(5);
    REQUIRE(st5.active);
    REQUIRE(st5.note == 62);
    REQUIRE(window.getTrackerSelectedRow() == 6);

    // Press Delete (261) on step 6 -> clears step 6
    window.onKeyDown(261);
    auto st6 = tr->getStep(6);
    REQUIRE(!st6.active);
    REQUIRE(window.getTrackerSelectedRow() == 7);

    // 7. Verify frame rendering in Tracker mode does not crash
    window.renderFrame();

    std::cout << "  [PASS] FastTracker 2 / Renoise vertical Tracker matrix tests passed." << std::endl;
}

void testEatscriptIdeInteraction() {
    std::cout << "[Test] Testing Eatscript Live IDE & In-DAW Hot-Reloading..." << std::endl;
    audio::AudioEngine engine;
    REQUIRE(engine.initialize());
    engine.setupDefaultPolyGraph();

    GuiWindow window(1280, 800, "Eatscript IDE Test");
    REQUIRE(window.initialize(engine));

    // 1. Switch to WorkspaceView::Design and DesignSubView::Eatscript
    window.setActiveView(WorkspaceView::Design);
    window.setDesignSubView(DesignSubView::Eatscript);
    REQUIRE(window.getActiveView() == WorkspaceView::Design);
    REQUIRE(window.getDesignSubView() == DesignSubView::Eatscript);

    // 2. Verify Initial Buffer is loaded with default Acid 303 template and compiled
    REQUIRE(!window.getScriptBuffer().empty());
    REQUIRE(window.isScriptCompiled());
    REQUIRE(window.getScriptDisassembly().size() > 0);
    REQUIRE(window.getScriptCode().find("def process") != std::string::npos);

    // 3. Test Sub-Nav and Toolbar Hit Testing
    // Click [ MODULAR RACK ] at (50, 75)
    auto modHit = window.hitTestEatscript(50.0f, 75.0f);
    REQUIRE(modHit.hit);
    REQUIRE(modHit.action == HitTestEatscriptResult::Action::SubNavModular);

    // Click [ EATSCRIPT IDE ] at (200, 75)
    auto ideHit = window.hitTestEatscript(200.0f, 75.0f);
    REQUIRE(ideHit.hit);
    REQUIRE(ideHit.action == HitTestEatscriptResult::Action::SubNavEatscript);

    // Click [ COMPILE ] at (500, 75)
    auto compHit = window.hitTestEatscript(500.0f, 75.0f);
    REQUIRE(compHit.hit);
    REQUIRE(compHit.action == HitTestEatscriptResult::Action::Compile);

    // Click [ HOT-RELOAD ] at (620, 75)
    auto hrHit = window.hitTestEatscript(620.0f, 75.0f);
    REQUIRE(hrHit.hit);
    REQUIRE(hrHit.action == HitTestEatscriptResult::Action::HotReload);

    // Click [ AOT C++ ] at (750, 75)
    auto aotHit = window.hitTestEatscript(750.0f, 75.0f);
    REQUIRE(aotHit.hit);
    REQUIRE(aotHit.action == HitTestEatscriptResult::Action::ToggleAotView);

    // Click Template 1 [ DUAL SAW ] at (940, 75)
    auto tmplHit = window.hitTestEatscript(940.0f, 75.0f);
    REQUIRE(tmplHit.hit);
    REQUIRE(tmplHit.action == HitTestEatscriptResult::Action::LoadTemplate);
    REQUIRE(tmplHit.templateIndex == 1);

    // Click Code Editor Canvas at line 2
    auto edHit = window.hitTestEatscript(120.0f, 185.0f);
    REQUIRE(edHit.hit);
    REQUIRE(edHit.action == HitTestEatscriptResult::Action::EditorClick);

    // Click Bind Track Button on Right Panel
    auto bindHit = window.hitTestEatscript(900.0f, 275.0f);
    REQUIRE(bindHit.hit);
    REQUIRE(bindHit.action == HitTestEatscriptResult::Action::BindTrack);

    // 4. Test Text Editing & Cursor Navigation
    window.setScriptCursor(0, 0);
    REQUIRE(window.getScriptCursorLine() == 0);
    REQUIRE(window.getScriptCursorCol() == 0);

    // Type text
    window.insertScriptText("# Live Comment\n");
    REQUIRE(window.getScriptBuffer()[0] == "# Live Comment");
    REQUIRE(window.getScriptCursorLine() == 1);

    // Keyboard navigation (Down, Up, Right, Left)
    window.onKeyDown(264); // Down
    REQUIRE(window.getScriptCursorLine() == 2);
    window.onKeyDown(265); // Up
    REQUIRE(window.getScriptCursorLine() == 1);
    window.onKeyDown(262); // Right
    REQUIRE(window.getScriptCursorCol() == 1);
    window.onKeyDown(263); // Left
    REQUIRE(window.getScriptCursorCol() == 0);

    // Backspace
    window.setScriptCursor(0, 6);
    window.deleteScriptCharBackwards();
    REQUIRE(window.getScriptBuffer()[0] == "# Liv Comment");

    // 5. Test Stock Template Loading
    REQUIRE(window.getScriptTemplateCount() >= 5);
    window.loadScriptTemplate(1); // DUAL SAW
    REQUIRE(window.getScriptCode().find("DUAL SAW") != std::string::npos || window.getScriptCode().find("detune") != std::string::npos);
    REQUIRE(window.isScriptCompiled());

    window.loadScriptTemplate(2); // DELAY FX
    REQUIRE(window.getScriptCode().find("DELAY") != std::string::npos || window.getScriptCode().find("feedback") != std::string::npos);
    REQUIRE(window.isScriptCompiled());

    window.loadScriptTemplate(3); // FM BELL
    REQUIRE(window.getScriptCode().find("FM") != std::string::npos || window.getScriptCode().find("mod") != std::string::npos);
    REQUIRE(window.isScriptCompiled());

    window.loadScriptTemplate(4); // DISTORT
    REQUIRE(window.getScriptCode().find("tanh") != std::string::npos);
    REQUIRE(window.isScriptCompiled());

    // 6. Test Compilation Error Handling
    window.setScriptCode("def process(in_l, in_r):\n    out_l = 1.0 + \n");
    bool badCompile = window.compileActiveScript();
    REQUIRE(!badCompile);
    REQUIRE(!window.isScriptCompiled());
    REQUIRE(!window.getScriptError().empty());
    REQUIRE(window.getScriptErrorLine() >= 1);

    // 7. Test AOT C++ Transpiler View
    window.loadScriptTemplate(0); // Restore 303 template
    REQUIRE(window.compileActiveScript());
    REQUIRE(!window.getScriptTranspiledCode().empty());
    REQUIRE(window.getScriptTranspiledCode().find("EatsPluginDescriptor") != std::string::npos);
    REQUIRE(window.getScriptTranspiledCode().find("process") != std::string::npos);

    window.setScriptAotViewEnabled(true);
    REQUIRE(window.isScriptAotViewEnabled());
    window.setScriptAotViewEnabled(false);
    REQUIRE(!window.isScriptAotViewEnabled());

    // 8. Test Hot-Reloading into Active Track
    window.setSelectedTrackIndex(0);
    bool hrOk = window.hotReloadScriptToTrack(0);
    REQUIRE(hrOk);
    REQUIRE(window.getLastStatusMessage().find("HOT-RELOADED") != std::string::npos);
    REQUIRE(engine.getSequencer().getTrack(0)->getName() == "EATSCRIPT");

    // Process audio blocks through engine to verify real-time safety & sound generation
    std::vector<float> audioOutL(128, 0.0f);
    std::vector<float> audioOutR(128, 0.0f);
    engine.postNoteOn(60, 0.9f);
    engine.renderOfflineBlock(audioOutL.data(), audioOutR.data(), 128);
    float maxAmp = 0.0f;
    for (float s : audioOutL) {
        maxAmp = std::max(maxAmp, std::abs(s));
    }
    for (float s : audioOutR) {
        maxAmp = std::max(maxAmp, std::abs(s));
    }
    std::cout << "  Eatscript hot-reloaded audio peak amplitude: " << maxAmp << std::endl;
    REQUIRE(maxAmp > 0.0f);

    // 9. Verify Frame Rendering does not crash
    window.renderFrame();

    std::cout << "  [PASS] Eatscript Live IDE & In-DAW Hot-Reloading tests passed." << std::endl;
}

void testScoreNotationInteraction() {
    std::cout << "[Test] Score Notation, SMuFL Bravura Vector Glyphs & Grand Staff interaction..." << std::endl;

    // -----------------------------------------------------------------------
    // 1. Test ScoreLayoutEngine Math & SMuFL Diatonic Mapping
    // -----------------------------------------------------------------------
    // Middle C (C4, MIDI 60) must map to diatonic step 28
    auto midC = ScoreLayoutEngine::pitchToLayout(60);
    REQUIRE(midC.midiPitch == 60);
    REQUIRE(midC.diatonicStep == 28);
    REQUIRE(midC.accidental == ScoreAccidental::None);
    REQUIRE(midC.noteName == "C4");
    REQUIRE(midC.isTreble == true);

    // E4 (MIDI 64) -> Treble Line 1, step 30
    auto e4 = ScoreLayoutEngine::pitchToLayout(64);
    REQUIRE(e4.diatonicStep == ScoreLayoutEngine::trebleLine1Step); // 30
    REQUIRE(e4.noteName == "E4");

    // G4 (MIDI 67) -> Treble Line 2, step 32
    auto g4 = ScoreLayoutEngine::pitchToLayout(67);
    REQUIRE(g4.diatonicStep == 32);

    // B4 (MIDI 71) -> Treble Line 3, step 34
    auto b4 = ScoreLayoutEngine::pitchToLayout(71);
    REQUIRE(b4.diatonicStep == ScoreLayoutEngine::trebleLine3Step); // 34

    // F5 (MIDI 77) -> Treble Line 5, step 38
    auto f5 = ScoreLayoutEngine::pitchToLayout(77);
    REQUIRE(f5.diatonicStep == ScoreLayoutEngine::trebleLine5Step); // 38

    // G2 (MIDI 43) -> Bass Line 1, step 18
    auto g2 = ScoreLayoutEngine::pitchToLayout(43);
    REQUIRE(g2.diatonicStep == ScoreLayoutEngine::bassLine1Step); // 18
    REQUIRE(!g2.isTreble);

    // A3 (MIDI 57) -> Bass Line 5, step 26
    auto a3 = ScoreLayoutEngine::pitchToLayout(57);
    REQUIRE(a3.diatonicStep == ScoreLayoutEngine::bassLine5Step); // 26
    REQUIRE(!a3.isTreble);

    // Accidentals: F#4 (MIDI 66) vs Gb4
    auto fSharp = ScoreLayoutEngine::pitchToLayout(66, false);
    REQUIRE(fSharp.accidental == ScoreAccidental::Sharp);
    REQUIRE(fSharp.noteName == "F#4");

    auto gFlat = ScoreLayoutEngine::pitchToLayout(66, true);
    REQUIRE(gFlat.accidental == ScoreAccidental::Flat);
    REQUIRE(gFlat.noteName == "Gb4");

    // Duration to note type:
    auto [durW, dotW] = ScoreLayoutEngine::durationToNoteType(16.0f);
    REQUIRE(durW == ScoreNoteType::Whole);
    REQUIRE(!dotW);

    auto [durH, dotH] = ScoreLayoutEngine::durationToNoteType(8.0f);
    REQUIRE(durH == ScoreNoteType::Half);
    REQUIRE(!dotH);

    auto [durQ, dotQ] = ScoreLayoutEngine::durationToNoteType(4.0f);
    REQUIRE(durQ == ScoreNoteType::Quarter);
    REQUIRE(!dotQ);

    auto [durE, dotE] = ScoreLayoutEngine::durationToNoteType(2.0f);
    REQUIRE(durE == ScoreNoteType::Eighth);

    auto [durS, dotS] = ScoreLayoutEngine::durationToNoteType(1.0f);
    REQUIRE(durS == ScoreNoteType::Sixteenth);

    // Visual computation for Middle C on Grand Staff:
    // Treble staff line 1 Y = 256.0f, sp = 14.0f (halfSp = 7.0f)
    // Middle C step delta = 28 - 30 = -2 -> noteY = 256 - (-2 * 7) = 270.0f
    auto visTrebleC = ScoreLayoutEngine::computeVisual(midC, 4.0f, 256.0f, 14.0f, true);
    REQUIRE(std::abs(visTrebleC.yPos - 270.0f) < 0.001f);
    REQUIRE(visTrebleC.ledgerLineYPositions.size() == 1);
    REQUIRE(std::abs(visTrebleC.ledgerLineYPositions[0] - 270.0f) < 0.001f);
    REQUIRE(visTrebleC.isStemUp == true);

    // Bass staff line 1 Y = 340.0f, line 5 step = 26 (Line 5 Y = 340 - 56 = 284)
    // Middle C step delta = 28 - 18 = 10 -> noteY = 340 - (10 * 7) = 270.0f
    auto visBassC = ScoreLayoutEngine::computeVisual(midC, 4.0f, 340.0f, 14.0f, false);
    REQUIRE(std::abs(visBassC.yPos - 270.0f) < 0.001f);
    REQUIRE(visBassC.ledgerLineYPositions.size() == 1);
    REQUIRE(std::abs(visBassC.ledgerLineYPositions[0] - 270.0f) < 0.001f);

    // Invert screen Y to MIDI pitch:
    int invertedMidC = ScoreLayoutEngine::yToMidiPitch(270.0f, 256.0f, 14.0f, true);
    REQUIRE(invertedMidC == 60);

    int invertedE4 = ScoreLayoutEngine::yToMidiPitch(256.0f, 256.0f, 14.0f, true);
    REQUIRE(invertedE4 == 64);

    // -----------------------------------------------------------------------
    // 2. Test GuiWindow Score Mode Interaction & Real-Time Note Audition
    // -----------------------------------------------------------------------
    AudioEngine engine;
    GuiWindow window(1280, 800, "Score Integration Test");
    window.initialize(engine);

    // Switch to WorkspaceView::Edit and sub-view EditSubView::Score
    window.setActiveView(WorkspaceView::Edit);
    window.onMouseDown(0, 1090.0f, 75.0f);
    window.onMouseUp(0, 1090.0f, 75.0f);
    REQUIRE(window.getEditSubView() == EditSubView::Score);

    // Test Clef Toolbar Hit-Testing:
    // [ TREBLE ] at [148..212, 103..129]
    auto hitTreble = window.hitTestScore(170.0f, 115.0f);
    REQUIRE(hitTreble.hit);
    REQUIRE(hitTreble.action == ScoreHitAction::ClefTreble);
    window.onMouseDown(0, 170.0f, 115.0f);
    window.onMouseUp(0, 170.0f, 115.0f);
    REQUIRE(window.getScoreClef() == ScoreClef::Treble);

    // [ BASS ] at [216..270]
    auto hitBass = window.hitTestScore(240.0f, 115.0f);
    REQUIRE(hitBass.hit);
    REQUIRE(hitBass.action == ScoreHitAction::ClefBass);
    window.onMouseDown(0, 240.0f, 115.0f);
    window.onMouseUp(0, 240.0f, 115.0f);
    REQUIRE(window.getScoreClef() == ScoreClef::Bass);

    // [ GRAND ] at [80..144]
    auto hitGrand = window.hitTestScore(110.0f, 115.0f);
    REQUIRE(hitGrand.hit);
    REQUIRE(hitGrand.action == ScoreHitAction::ClefGrandStaff);
    window.onMouseDown(0, 110.0f, 115.0f);
    window.onMouseUp(0, 110.0f, 115.0f);
    REQUIRE(window.getScoreClef() == ScoreClef::GrandStaff);

    // Test Duration Toolbar Hit-Testing:
    // [ 1/2 ] at [368..408]
    auto hitHalf = window.hitTestScore(385.0f, 115.0f);
    REQUIRE(hitHalf.hit);
    REQUIRE(hitHalf.action == ScoreHitAction::DurationHalf);
    window.onMouseDown(0, 385.0f, 115.0f);
    window.onMouseUp(0, 385.0f, 115.0f);
    REQUIRE(window.getScoreDuration() == ScoreNoteType::Half);

    // [ 1/4 ] at [412..452]
    auto hitQuarter = window.hitTestScore(430.0f, 115.0f);
    REQUIRE(hitQuarter.hit);
    REQUIRE(hitQuarter.action == ScoreHitAction::DurationQuarter);
    window.onMouseDown(0, 430.0f, 115.0f);
    window.onMouseUp(0, 430.0f, 115.0f);
    REQUIRE(window.getScoreDuration() == ScoreNoteType::Quarter);

    // Test Accidental Toolbar Hit-Testing:
    // [ # ] at [646..678]
    auto hitSharp = window.hitTestScore(660.0f, 115.0f);
    REQUIRE(hitSharp.hit);
    REQUIRE(hitSharp.action == ScoreHitAction::AccidentalSharp);
    window.onMouseDown(0, 660.0f, 115.0f);
    window.onMouseUp(0, 660.0f, 115.0f);
    REQUIRE(window.getScoreAccidental() == ScoreAccidental::Sharp);

    // [ NAT ] at [600..642]
    auto hitNat = window.hitTestScore(620.0f, 115.0f);
    REQUIRE(hitNat.hit);
    REQUIRE(hitNat.action == ScoreHitAction::AccidentalNone);
    window.onMouseDown(0, 620.0f, 115.0f);
    window.onMouseUp(0, 620.0f, 115.0f);
    REQUIRE(window.getScoreAccidental() == ScoreAccidental::None);

    // -----------------------------------------------------------------------
    // 3. Test Interactive Note Placement, Audition, and Note Deletion on Staff
    // -----------------------------------------------------------------------
    auto* tr0 = engine.getSequencer().getTrack(0);
    REQUIRE(tr0 != nullptr);

    // Step 0 center X = 140 + 0.5 * ((1280 - 60 - 140) / 16) = 140 + 0.5 * 67.5 = 173.75
    // Middle C Y = 270.0f
    auto staffHit = window.hitTestScore(174.0f, 270.0f);
    REQUIRE(staffHit.hit);
    REQUIRE(staffHit.action == ScoreHitAction::StaffClick);
    REQUIRE(staffHit.step == 0);
    REQUIRE(staffHit.pitch == 60);

    // Click to place note on step 0
    window.onMouseDown(0, 174.0f, 270.0f);
    window.onMouseUp(0, 174.0f, 270.0f);

    auto step0 = tr0->getStep(0);
    REQUIRE(step0.active == true);
    REQUIRE(step0.note == 60);
    REQUIRE(step0.velocity > 0.8f);

    // Click same note again to delete / toggle off
    window.onMouseDown(0, 174.0f, 270.0f);
    window.onMouseUp(0, 174.0f, 270.0f);

    auto step0After = tr0->getStep(0);
    REQUIRE(step0After.active == false);

    // Place a series of notes across 4 beats to test SMuFL rendering of diverse pitches:
    // Step 0: C4 (60)
    // Step 4: E4 (64)
    // Step 8: G4 (67)
    // Step 12: C5 (72)
    float stepW = (1280.0f - 60.0f - 140.0f) / 16.0f;
    window.onMouseDown(0, 140.0f + 0.5f * stepW, 270.0f);  // Step 0, C4
    window.onMouseDown(0, 140.0f + 4.5f * stepW, 256.0f);  // Step 4, E4
    window.onMouseDown(0, 140.0f + 8.5f * stepW, 242.0f);  // Step 8, G4
    window.onMouseDown(0, 140.0f + 12.5f * stepW, 221.0f); // Step 12, C5

    REQUIRE(tr0->getStep(0).active);
    REQUIRE(tr0->getStep(4).active);
    REQUIRE(tr0->getStep(8).active);
    REQUIRE(tr0->getStep(12).active);

    // 4. Verify Frame Rendering does not crash
    window.renderFrame();

    std::cout << "  [PASS] Score Notation, SMuFL Bravura Vector Glyphs & Grand Staff tests passed." << std::endl;
}

void testNoteScriptEditorInteraction() {
    std::cout << "[Test] Note Script Editor (EDIT > SCRIPT) Bi-directional Sync & Editing..." << std::endl;
    GuiWindow window(1280, 800, "Test Note Script Editor");
    audio::AudioEngine engine;
    REQUIRE(window.initialize(engine));

    // 1. Switch to WorkspaceView::Edit and sub-view EditSubView::Script
    window.setActiveView(WorkspaceView::Edit);
    window.onMouseDown(0, 1190.0f, 75.0f);
    window.onMouseUp(0, 1190.0f, 75.0f);
    REQUIRE(window.getEditSubView() == EditSubView::Script);

    // 2. Test Toolbar Hit-Testing
    auto hitApply = window.hitTestNoteScript(100.0f, 110.0f);
    REQUIRE(hitApply.hit);
    REQUIRE(hitApply.action == HitTestNoteScriptResult::Action::ApplySync);

    auto hitRevert = window.hitTestNoteScript(280.0f, 110.0f);
    REQUIRE(hitRevert.hit);
    REQUIRE(hitRevert.action == HitTestNoteScriptResult::Action::RevertFromTrack);

    auto hitTmpl = window.hitTestNoteScript(440.0f, 110.0f);
    REQUIRE(hitTmpl.hit);
    REQUIRE(hitTmpl.action == HitTestNoteScriptResult::Action::InsertTemplate);

    auto hitClear = window.hitTestNoteScript(550.0f, 110.0f);
    REQUIRE(hitClear.hit);
    REQUIRE(hitClear.action == HitTestNoteScriptResult::Action::Clear);

    auto hitCanvas = window.hitTestNoteScript(200.0f, 200.0f);
    REQUIRE(hitCanvas.hit);
    REQUIRE(hitCanvas.action == HitTestNoteScriptResult::Action::EditorClick);

    // 3. Test Track -> Note Script Sync
    auto* tr0 = engine.getSequencer().getTrack(0);
    REQUIRE(tr0 != nullptr);
    tr0->clear();

    sequencer::StepData sd;
    sd.active = true;
    sd.note = 50; // D3
    sd.velocity = 0.88f;
    sd.gateLength = 0.75f;
    sd.slide = true;
    sd.accent = false;
    tr0->setStep(2, sd);

    window.syncTrackToNoteScript();
    std::string text = window.getNoteScriptText();
    REQUIRE(text.find("pitch = \"D3\"") != std::string::npos);
    REQUIRE(text.find("step = 2") != std::string::npos);
    REQUIRE(text.find("slide = true") != std::string::npos);
    REQUIRE(!window.isNoteScriptDirty());

    // 4. Test Note Script -> Track Sync
    std::string newScript =
        "# User Edited Script\n"
        "{ pitch = \"F3\", step = 4, dur = 0.50, vel = 0.90 }\n"
        "{ pitch = \"A#3\", step = 8, dur = 0.80, vel = 0.95, accent = true }\n";
    window.setNoteScriptText(newScript);
    REQUIRE(window.isNoteScriptDirty());

    bool syncOk = window.syncNoteScriptToTrack();
    REQUIRE(syncOk);
    REQUIRE(!window.isNoteScriptDirty());

    REQUIRE(!tr0->getStep(2).active); // Old step cleared
    const auto& s4 = tr0->getStep(4);
    REQUIRE(s4.active);
    REQUIRE(s4.note == 53); // F3
    const auto& s8 = tr0->getStep(8);
    REQUIRE(s8.active);
    REQUIRE(s8.note == 58); // A#3
    REQUIRE(s8.accent);

    // 5. Test Keyboard Typing & Text Manipulation in Note Script Editor
    window.setNoteScriptCursor(0, 0);
    window.insertNoteScriptChar('{');
    window.insertNoteScriptText(" pitch = \"C3\", step = 1 }");
    window.insertNoteScriptNewLine();
    REQUIRE(window.getNoteScriptBuffer().size() >= 2);
    REQUIRE(window.isNoteScriptDirty());

    // Test Delete & Backspace
    window.deleteNoteScriptCharBackwards();
    window.deleteNoteScriptCharForwards();

    // 6. Test Error Handling on Bad Script
    window.setNoteScriptText("{ pitch = \"INVALID\", step = 0 }");
    bool badSync = window.syncNoteScriptToTrack();
    REQUIRE(!badSync);
    REQUIRE(window.getNoteScriptErrorLine() == 1);
    REQUIRE(!window.getNoteScriptError().empty());

    // 7. Verify frame rendering in Script sub-view does not crash
    window.renderFrame();

    std::cout << "  [PASS] Note Script Editor Bi-directional Sync & Editing tests passed." << std::endl;
}

void testEatscriptPresetDualModeDispatch() {
    std::cout << "[Test] Eatscript Track Preset Dual-Mode Dispatch & Hot-Reloading..." << std::endl;

    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.setupDefaultAcidGraph();
    engine.initialize(cfg);

    GuiWindow window(1280, 800, "Dual-Mode Dispatch Test");
    window.initialize(engine);

    // 1. Verify Track 0 was initialized with TB-303 Eatscript
    auto* tr0 = engine.getSequencer().getTrack(0);
    REQUIRE(tr0 != nullptr);
    REQUIRE(!tr0->getEatscriptCode().empty());
    REQUIRE(tr0->getEatscriptCode().find("Eats303 = True") != std::string::npos);

    // 2. Switch to DESIGN > EATSCRIPT
    window.setActiveView(WorkspaceView::Design);
    window.setDesignSubView(DesignSubView::Eatscript);
    REQUIRE(window.getDesignSubView() == DesignSubView::Eatscript);

    // Verify script buffer has been loaded with Track 0's Eatscript
    std::string script = window.getScriptCode();
    REQUIRE(!script.empty());
    REQUIRE(script.find("Eats303 = True") != std::string::npos);

    // 3. Compile active script -> Native SIMD Dispatch detected
    bool compiled = window.compileActiveScript();
    REQUIRE(compiled);
    REQUIRE(window.isScriptCompiled());
    REQUIRE(window.getScriptDisassembly().size() > 0);
    REQUIRE(window.getScriptDisassembly()[0].find("NATIVE_SIMD_DISPATCH") != std::string::npos);

    // 4. Modify parameter in script and hot-reload to Track 0
    std::string modifiedScript =
        "# Eatsbeats Custom Diode Acid\n"
        "Eats303 = True\n\n"
        "def init():\n"
        "    params['Cutoff'] = 2750.0\n"
        "    params['Resonance'] = 14.5\n\n"
        "def process(time, freq, note, params):\n"
        "    pass\n";

    window.setScriptCode(modifiedScript);
    bool reloaded = window.hotReloadScriptToTrack(0);
    REQUIRE(reloaded);
    REQUIRE(tr0->getEatscriptCode() == window.getScriptCode());
    REQUIRE(tr0->getEatscriptCode().find("2750.0") != std::string::npos);

    // Verify parameter values updated in active preset
    const auto* p = window.getActivePreset();
    REQUIRE(p != nullptr);
    const auto* pCut = p->findParam("Cutoff");
    REQUIRE(pCut != nullptr);
    REQUIRE(std::abs(pCut->currentVal - 2750.0f) < 0.01f);
    const auto* pRes = p->findParam("Resonance");
    REQUIRE(pRes != nullptr);
    REQUIRE(std::abs(pRes->currentVal - 14.5f) < 0.01f);

    // 5. Test nextPreset() synchronization with Eatscript IDE
    window.nextPreset();
    std::string nextScript = window.getScriptCode();
    REQUIRE(!nextScript.empty());
    REQUIRE(nextScript != modifiedScript);

    // 6. Test fallback to interpreted Bytecode VM when no dispatch flag
    std::string customMathVoice =
        "# Custom Pure Math Voice\n"
        "def init():\n"
        "    return { \"Gain\": 0.5 }\n\n"
        "def process(time, freq, note, params):\n"
        "    return math.sin(2.0 * math.pi * freq * time) * 0.5\n";

    window.setScriptCode(customMathVoice);
    bool mathCompiled = window.compileActiveScript();
    REQUIRE(mathCompiled);
    REQUIRE(window.isScriptCompiled());
    // Verify it compiled into virtual machine bytecode instructions
    REQUIRE(window.getScriptDisassembly()[0].find("OP_") != std::string::npos);

    std::cout << "  [PASS] Eatscript Track Preset Dual-Mode Dispatch tests passed." << std::endl;
}

void testProjectBrowserMacrosInteraction() {
    std::cout << "[Test] Project Browser Drawer: Scripts & Macros execution..." << std::endl;
    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.setupDefaultAcidGraph();
    engine.initialize(cfg);

    GuiWindow window(1280, 800, "Macro Drawer Test");
    window.initialize(engine);

    // Open browser drawer
    window.setBrowserOpen(true);
    REQUIRE(window.isBrowserOpen());
    REQUIRE(window.getBrowserTab() == BrowserTab::Presets);

    // Switch to SCRIPTS / MACROS tab
    float drX = 1280.0f - 440.0f - 16.0f;
    float drY = 60.0f;
    auto tabHit = window.hitTestBrowser(drX + 200.0f, drY + 55.0f);
    REQUIRE(tabHit.hit);
    REQUIRE(tabHit.action == BrowserHitAction::TabMacros);

    window.onMouseDown(0, drX + 200.0f, drY + 55.0f);
    window.onMouseUp(0, drX + 200.0f, drY + 55.0f);
    REQUIRE(window.getBrowserTab() == BrowserTab::Macros);

    // Run Macro 0 (Generate Acid 303 Bassline)
    float btnX = drX + 440.0f - 45.0f;
    float btnY = drY + 84.0f + 20.0f;
    auto runHit = window.hitTestBrowser(btnX, btnY);
    REQUIRE(runHit.hit);
    REQUIRE(runHit.action == BrowserHitAction::MacroRun);
    REQUIRE(runHit.macroIndex == 0);

    window.onMouseDown(0, btnX, btnY);
    window.onMouseUp(0, btnX, btnY);

    // Verify macro executed and status message updated
    REQUIRE(window.getLastStatusMessage().find("Acid") != std::string::npos);

    // Verify track 0 now has active acid steps
    auto* trk = engine.getSequencer().getTrack(0);
    REQUIRE(trk != nullptr);
    int activeCount = 0;
    for (uint32_t s = 0; s < 16; ++s) {
        if (trk->getStep(s).active) activeCount++;
    }
    REQUIRE(activeCount > 0);

    // Switch to EDIT > SCRIPT to verify note script was populated
    window.setActiveView(WorkspaceView::Edit);
    window.setEditSubView(EditSubView::Script);
    REQUIRE(!window.getNoteScriptBuffer().empty());

    std::cout << "  [PASS] Project Browser Drawer Scripts & Macros tests passed." << std::endl;
}

void testProjectBrowserHistoryTabInteraction() {
    std::cout << "[Test] Project Browser Drawer: Pure Diff-Based History & Time-Travel..." << std::endl;
    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.setupDefaultAcidGraph();
    engine.initialize(cfg);

    GuiWindow window(1280, 800, "History Drawer Test");
    window.initialize(engine);

    // Open browser drawer
    window.setBrowserOpen(true);
    REQUIRE(window.isBrowserOpen());
    REQUIRE(window.getBrowserTab() == BrowserTab::Presets);

    float drX = 1280.0f - 440.0f - 16.0f;
    float drY = 60.0f;

    // 1. Switch to HISTORY & DELTAS tab (Pill 3: drX + 292..422, drY + 48..72)
    auto tabHit = window.hitTestBrowser(drX + 350.0f, drY + 55.0f);
    REQUIRE(tabHit.hit);
    REQUIRE(tabHit.action == BrowserHitAction::TabHistory);

    window.onMouseDown(0, drX + 350.0f, drY + 55.0f);
    window.onMouseUp(0, drX + 350.0f, drY + 55.0f);
    REQUIRE(window.getBrowserTab() == BrowserTab::History);

    // Initial history state
    const auto& hist = window.getDiffHistory();
    REQUIRE(!hist.canUndo());
    REQUIRE(!hist.canRedo());
    REQUIRE(hist.getTimelineCount() == 1);

    // 2. Perform actions to record history
    window.loadPresetToSelectedTrack(1); // loads preset 1
    REQUIRE(hist.canUndo());
    REQUIRE(!hist.canRedo());
    REQUIRE(hist.getTimelineCount() == 2);

    window.loadPresetToSelectedTrack(2); // loads preset 2
    REQUIRE(hist.canUndo());
    REQUIRE(hist.getTimelineCount() == 3);

    // 3. Test UNDO button click (drX + 16..106, drY + 76..102)
    auto undoHit = window.hitTestBrowser(drX + 50.0f, drY + 85.0f);
    REQUIRE(undoHit.hit);
    REQUIRE(undoHit.action == BrowserHitAction::HistoryUndo);

    window.onMouseDown(0, drX + 50.0f, drY + 85.0f);
    window.onMouseUp(0, drX + 50.0f, drY + 85.0f);
    REQUIRE(hist.canUndo());
    REQUIRE(hist.canRedo());
    REQUIRE(hist.getCurrentTimelineIndex() == 1);

    // 4. Test REDO button click (drX + 112..202, drY + 76..102)
    auto redoHit = window.hitTestBrowser(drX + 150.0f, drY + 85.0f);
    REQUIRE(redoHit.hit);
    REQUIRE(redoHit.action == BrowserHitAction::HistoryRedo);

    window.onMouseDown(0, drX + 150.0f, drY + 85.0f);
    window.onMouseUp(0, drX + 150.0f, drY + 85.0f);
    REQUIRE(hist.getCurrentTimelineIndex() == 2);

    // 5. Test Keyboard Shortcuts: Ctrl+Z (Undo) and Ctrl+Y (Redo)
    window.onKeyDown(90, 2); // Ctrl+Z
    REQUIRE(hist.getCurrentTimelineIndex() == 1);

    window.onKeyDown(89, 2); // Ctrl+Y
    REQUIRE(hist.getCurrentTimelineIndex() == 2);

    // 6. Test + CHECKPOINT button click (drX + 208..324, drY + 76..102)
    auto checkHit = window.hitTestBrowser(drX + 250.0f, drY + 85.0f);
    REQUIRE(checkHit.hit);
    REQUIRE(checkHit.action == BrowserHitAction::HistoryMilestone);

    window.onMouseDown(0, drX + 250.0f, drY + 85.0f);
    window.onMouseUp(0, drX + 250.0f, drY + 85.0f);
    auto timeline = hist.getTimeline();
    REQUIRE(timeline[2].isMilestone);

    // 7. Test Timeline Item Click (Jump to step 0 / origin)
    auto step0Hit = window.hitTestBrowser(drX + 100.0f, drY + 130.0f);
    REQUIRE(step0Hit.hit);
    REQUIRE(step0Hit.action == BrowserHitAction::HistoryStepSelect);

    window.onMouseDown(0, drX + 100.0f, drY + 130.0f);
    window.onMouseUp(0, drX + 100.0f, drY + 130.0f);
    REQUIRE(hist.getCurrentTimelineIndex() == 0);

    // 8. Test CLEAR button (drX + 330..424, drY + 76..102)
    auto clearHit = window.hitTestBrowser(drX + 370.0f, drY + 85.0f);
    REQUIRE(clearHit.hit);
    REQUIRE(clearHit.action == BrowserHitAction::HistoryClear);

    window.onMouseDown(0, drX + 370.0f, drY + 85.0f);
    window.onMouseUp(0, drX + 370.0f, drY + 85.0f);
    REQUIRE(hist.getTimelineCount() == 1);
    REQUIRE(!hist.canUndo());

    std::cout << "  [PASS] Project Browser Drawer History & Time-Travel tests passed." << std::endl;
}

void testWindowResizeAndUiScale() {
    std::cout << "[Test] Window resize, high-DPI scaling, and dynamic UI scale factor..." << std::endl;

    AudioEngine engine;
    GuiWindow window(1280, 800, "Eatsbits Scale Test");
    window.initialize(engine);

    // 1. Initial dimensions & 1.0 UI Scale
    REQUIRE(window.getWindowWidth() == 1280);
    REQUIRE(window.getWindowHeight() == 800);
    REQUIRE(window.getWidth() == 1280);
    REQUIRE(window.getHeight() == 800);
    REQUIRE(std::abs(window.getUiScale() - 1.0f) < 0.001f);

    // 2. Window resizing (e.g. going full screen or resizing to 1920x1080)
    window.onWindowResize(1920, 1080);
    window.onFramebufferResize(1920, 1080);
    REQUIRE(window.getWindowWidth() == 1920);
    REQUIRE(window.getWindowHeight() == 1080);
    REQUIRE(window.getWidth() == 1920);
    REQUIRE(window.getHeight() == 1080);

    // Verify bottom nav anchors correctly at y = 1080 - 48
    float bNavY = 1080.0f - 24.0f;
    auto navHit = window.hitTestBottomNav(50.0f, bNavY);
    REQUIRE(navHit.hit);
    REQUIRE(navHit.action == BottomNavAction::Arranger);

    // 3. UI Scale changes (e.g. 1.25x Relaxed / 1440p)
    window.setUiScale(1.25f);
    REQUIRE(std::abs(window.getUiScale() - 1.25f) < 0.001f);
    uint32_t expectedLogW = static_cast<uint32_t>(std::round(1920.0f / 1.25f)); // 1536
    uint32_t expectedLogH = static_cast<uint32_t>(std::round(1080.0f / 1.25f)); // 864
    REQUIRE(window.getWidth() == expectedLogW);
    REQUIRE(window.getHeight() == expectedLogH);

    // 4. Mouse Coordinate Mapping
    float screenX = 1250.0f;
    float screenY = 500.0f;
    float logX = window.windowToLogicalX(screenX);
    float logY = window.windowToLogicalY(screenY);
    REQUIRE(std::abs(logX - 1000.0f) < 0.01f);
    REQUIRE(std::abs(logY - 400.0f) < 0.01f);

    // 5. Scale Selector Chip Hit Test & Cycling
    float scaleBtnX = static_cast<float>(window.getWidth()) - 345.0f + 10.0f;
    auto scaleHit = window.hitTestTransport(scaleBtnX, 25.0f);
    REQUIRE(scaleHit.hit);
    REQUIRE(scaleHit.action == TransportAction::ScaleToggle);

    // Click scale chip to cycle scale preset (1.25 -> 1.50)
    window.onMouseDown(0, scaleBtnX, 25.0f);
    window.onMouseUp(0, scaleBtnX, 25.0f);
    REQUIRE(std::abs(window.getUiScale() - 1.50f) < 0.001f);

    // 6. Keyboard Zoom Shortcuts (Ctrl + '0', Ctrl + '=', Ctrl + '-')
    // Reset to 100% via Ctrl+0
    window.onKeyDown(48, 2); // key=48 ('0'), mods=2 (Ctrl)
    REQUIRE(std::abs(window.getUiScale() - 1.0f) < 0.001f);
    REQUIRE(window.getWidth() == 1920);

    // Zoom in via Ctrl+=
    window.onKeyDown(61, 2); // key=61 ('='), mods=2 (Ctrl)
    REQUIRE(std::abs(window.getUiScale() - 1.10f) < 0.001f);

    // Zoom out via Ctrl+-
    window.onKeyDown(45, 2); // key=45 ('-'), mods=2 (Ctrl)
    REQUIRE(std::abs(window.getUiScale() - 1.0f) < 0.001f);

    std::cout << "  [PASS] Window resize and dynamic UI scale tests passed." << std::endl;
}

void testProjectHubAndTopLeftMenu() {
    std::cout << "[Test] Top-Left Brand Logo Menu & Project Hub Modal interaction..." << std::endl;

    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.setupDefaultAcidGraph();

    GuiWindow window(1280, 800, "Project Hub Test Window");
    bool initOk = window.initialize(engine);
    REQUIRE(initOk);

    // 1. Top-Left Brand Logo Hit-Test [10, 8, 46, 48] (Clicking logo opens Project Hub / Settings)
    REQUIRE(!window.isProjectHubOpen());
    auto logoHit = window.hitTestTransport(30.0f, 25.0f);
    REQUIRE(logoHit.hit);
    REQUIRE(logoHit.action == TransportAction::ProjectHubToggle);

    // Clicking top-left logo toggles Project Hub
    window.onMouseDown(0, 30.0f, 25.0f);
    window.onMouseUp(0, 30.0f, 25.0f);
    REQUIRE(window.isProjectHubOpen());
    REQUIRE(window.getProjectHubSection() == 0);

    // Modal layout geometry
    const float hubW = 540.0f;
    const float hubH = 580.0f;
    const float hubX = std::max(12.0f, (1280.0f - hubW) * 0.5f); // 370.0f
    const float hubY = std::max(12.0f, (800.0f - hubH) * 0.5f);  // Centered vertically (110.0f)

    // 2. Hit-test Top-Right Close Button
    float closeX = hubX + hubW - 20.0f;
    float closeY = hubY + 20.0f;
    auto closeHit = window.hitTestProjectHub(closeX, closeY);
    REQUIRE(closeHit.hit);
    REQUIRE(closeHit.action == ProjectHubAction::Close);

    // 2b. Hit-test Bottom-Right CLOSE Button
    float botCloseX = hubX + hubW - 40.0f;
    float botCloseY = hubY + hubH - 20.0f;
    auto botCloseHit = window.hitTestProjectHub(botCloseX, botCloseY);
    REQUIRE(botCloseHit.hit);
    REQUIRE(botCloseHit.action == ProjectHubAction::Close);

    // Click close button
    window.onMouseDown(0, closeX, closeY);
    window.onMouseUp(0, closeX, closeY);
    REQUIRE(!window.isProjectHubOpen());

    // 3. Test Alt+F Keyboard Shortcut to Toggle Open
    window.onKeyDown(70, 4); // key=70 ('F'), mods=4 (Alt)
    REQUIRE(window.isProjectHubOpen());

    // Test Esc Key to Close
    window.onKeyDown(256, 0); // key=256 (Esc)
    REQUIRE(!window.isProjectHubOpen());

    // Re-open via F1 key (290)
    window.onKeyDown(290, 0); // F1
    REQUIRE(window.isProjectHubOpen());

    // 4. Section 0 (Project Hub) Action Buttons
    float contentY = hubY + 50.0f + 32.0f;
    const float btnW = (hubW - 48.0f - 16.0f) / 3.0f;
    const float row1Y = contentY + 104.0f;
    const float row2Y = contentY + 144.0f;

    // Test [ SAVE (.eats) ] button
    float saveBtnX = hubX + 24.0f + 20.0f;
    float saveBtnY = row1Y + 15.0f;
    auto saveHit = window.hitTestProjectHub(saveBtnX, saveBtnY);
    REQUIRE(saveHit.hit);
    REQUIRE(saveHit.action == ProjectHubAction::SaveProject);

    window.onMouseDown(0, saveBtnX, saveBtnY);
    window.onMouseUp(0, saveBtnX, saveBtnY);
    REQUIRE(window.getLastStatusMessage().find("PROJECT SAVED") != std::string::npos);

    // Test [ BOUNCE WAV ] button
    float bounceBtnX = hubX + 32.0f + btnW + 20.0f;
    float bounceBtnY = row2Y + 15.0f;
    auto bounceHit = window.hitTestProjectHub(bounceBtnX, bounceBtnY);
    REQUIRE(bounceHit.hit);
    REQUIRE(bounceHit.action == ProjectHubAction::BounceWav);

    window.onMouseDown(0, bounceBtnX, bounceBtnY);
    window.onMouseUp(0, bounceBtnX, bounceBtnY);
    REQUIRE(window.getLastStatusMessage().find("FAST-BOUNCE") != std::string::npos);

    // Test [ SCRIPT VIEW ] button
    float scriptBtnX = hubX + 40.0f + 2.0f * btnW + 20.0f;
    float scriptBtnY = row2Y + 15.0f;
    auto scriptHit = window.hitTestProjectHub(scriptBtnX, scriptBtnY);
    REQUIRE(scriptHit.hit);
    REQUIRE(scriptHit.action == ProjectHubAction::OpenScriptView);

    window.onMouseDown(0, scriptBtnX, scriptBtnY);
    window.onMouseUp(0, scriptBtnX, scriptBtnY);
    REQUIRE(!window.isProjectHubOpen());
    REQUIRE(window.getActiveView() == WorkspaceView::Edit);
    REQUIRE(window.getEditSubView() == EditSubView::Script);

    // Re-open Project Hub
    window.toggleProjectHub();
    REQUIRE(window.isProjectHubOpen());

    // 5. Test Title Editing in Modal
    float titleBoxX = hubX + 40.0f;
    float titleBoxY = contentY + 20.0f;
    auto titleHit = window.hitTestProjectHub(titleBoxX, titleBoxY);
    REQUIRE(titleHit.hit);
    REQUIRE(titleHit.action == ProjectHubAction::TitleClick);

    window.onMouseDown(0, titleBoxX, titleBoxY);
    window.onMouseUp(0, titleBoxX, titleBoxY);

    // Set custom project name and verify
    window.setProjectName("Acid Odyssey 303");
    REQUIRE(window.getProjectName() == "Acid Odyssey 303");

    window.setAuthorName("Producer One");
    REQUIRE(window.getAuthorName() == "Producer One");

    // 6. Test Accordion Sections: Switch to Section 1 (Session Persistence)
    float sec1HeaderY = hubY + 50.0f + 36.0f + 190.0f;
    auto sec1Hit = window.hitTestProjectHub(hubX + 50.0f, sec1HeaderY + 10.0f);
    REQUIRE(sec1Hit.hit);
    REQUIRE(sec1Hit.action == ProjectHubAction::SectionHeader);
    REQUIRE(sec1Hit.sectionIndex == 1);

    window.onMouseDown(0, hubX + 50.0f, sec1HeaderY + 10.0f);
    window.onMouseUp(0, hubX + 50.0f, sec1HeaderY + 10.0f);
    REQUIRE(window.getProjectHubSection() == 1);

    // Now that Section 0 is collapsed (36px), Section 1 begins at hubY + 50 + 36:
    bool initialRestore = window.isAutoRestoreSession();
    float sec1NewY = hubY + 50.0f + 36.0f;
    float sec1ContentY = sec1NewY + 32.0f;
    float switchRestoreX = hubX + hubW - 50.0f;
    float switchRestoreY = sec1ContentY + 18.0f;
    auto switchRestoreHit = window.hitTestProjectHub(switchRestoreX, switchRestoreY);
    REQUIRE(switchRestoreHit.hit);
    REQUIRE(switchRestoreHit.action == ProjectHubAction::ToggleRestoreSession);

    window.onMouseDown(0, switchRestoreX, switchRestoreY);
    window.onMouseUp(0, switchRestoreX, switchRestoreY);
    REQUIRE(window.isAutoRestoreSession() == !initialRestore);

    // Test Section 2 (Display & Workspace)
    window.setProjectHubSection(2);
    REQUIRE(window.getProjectHubSection() == 2);

    float sec2NewY = hubY + 50.0f + 2.0f * 36.0f; // Sec 0 & 1 collapsed (36px each)
    float sec2ContentY = sec2NewY + 32.0f;

    // Test UI Scale Slider (50% to 200% in 25% increments) and Quick-Preset Chips
    auto getCurHub = [&]() {
        float hx = std::max(12.0f, (static_cast<float>(window.getWidth()) - hubW) * 0.5f);
        float hy = std::max(12.0f, (static_cast<float>(window.getHeight()) - hubH) * 0.5f);
        float secY = hy + 50.0f + 2.0f * 36.0f;
        float cY = secY + 32.0f;
        return std::make_tuple(hx, hy, cY);
    };

    float chipGap = 5.0f;
    float chipW = (hubW - 48.0f - (6.0f * chipGap)) / 7.0f;
    float uiSliderW = hubW - 130.0f;

    // 1. Click Slider track at 50% (leftmost edge)
    auto [h1X, h1Y, c1Y] = getCurHub();
    float uiSlider1X = h1X + 24.0f;
    float uiSlider1Y = c1Y + 22.0f;
    auto scaleHit50 = window.hitTestProjectHub(uiSlider1X, uiSlider1Y);
    REQUIRE(scaleHit50.hit);
    REQUIRE(scaleHit50.action == ProjectHubAction::SetUiScale);
    REQUIRE(std::abs(scaleHit50.scaleValue - 0.50f) < 0.001f);
    window.onMouseDown(0, uiSlider1X, uiSlider1Y);
    window.onMouseUp(0, uiSlider1X, uiSlider1Y);
    REQUIRE(std::abs(window.getUiScale() - 0.50f) < 0.001f);
    // At 50% UI scale on 1280x800 window, logical canvas expands 2x to 2560x1600
    REQUIRE(window.getWidth() == 2560);
    REQUIRE(window.getHeight() == 1600);
    REQUIRE(std::abs(window.windowToLogicalX(100.0f) - 200.0f) < 0.01f);

    // 2. Click Quick-Preset Chip for 75%
    auto [h2X, h2Y, c2Y] = getCurHub();
    float chip75X = h2X + 24.0f + 1 * (chipW + chipGap) + chipW * 0.5f;
    float chip75Y = c2Y + 45.0f;
    auto chipHit75 = window.hitTestProjectHub(chip75X, chip75Y);
    REQUIRE(chipHit75.hit);
    REQUIRE(chipHit75.action == ProjectHubAction::SetUiScale);
    REQUIRE(std::abs(chipHit75.scaleValue - 0.75f) < 0.001f);
    window.onMouseDown(0, chip75X, chip75Y);
    window.onMouseUp(0, chip75X, chip75Y);
    REQUIRE(std::abs(window.getUiScale() - 0.75f) < 0.001f);

    // 3. Test Dragging UI Scale Slider to 200%
    auto [h3X, h3Y, c3Y] = getCurHub();
    float uiSlider3X = h3X + 24.0f;
    float uiSlider3Y = c3Y + 22.0f;
    window.onMouseDown(0, uiSlider3X + 20.0f, uiSlider3Y); // starts DragMode::UiScaleSlider
    auto [hDragX, hDragY, cDragY] = getCurHub();
    float dragSliderX = hDragX + 24.0f;
    window.onMouseMove(dragSliderX + uiSliderW, cDragY + 22.0f); // drag to max 2.0x
    window.onMouseUp(0, dragSliderX + uiSliderW, cDragY + 22.0f);
    REQUIRE(std::abs(window.getUiScale() - 2.00f) < 0.001f);
    REQUIRE(window.getWidth() == 640);
    REQUIRE(window.getHeight() == 400);

    // 4. Click Quick-Preset Chip for 100% to restore normal scale
    auto [h4X, h4Y, c4Y] = getCurHub();
    float chip100X = h4X + 24.0f + 2 * (chipW + chipGap) + chipW * 0.5f;
    float chip100Y = c4Y + 45.0f;
    auto chipHit100 = window.hitTestProjectHub(chip100X, chip100Y);
    REQUIRE(chipHit100.hit);
    REQUIRE(chipHit100.action == ProjectHubAction::SetUiScale);
    REQUIRE(std::abs(chipHit100.scaleValue - 1.00f) < 0.001f);
    window.onMouseDown(0, chip100X, chip100Y);
    window.onMouseUp(0, chip100X, chip100Y);
    REQUIRE(std::abs(window.getUiScale() - 1.00f) < 0.001f);
    REQUIRE(window.getWidth() == 1280);
    REQUIRE(window.getHeight() == 800);

    // Test Theme Switching across the 5 Eatsbeats presets
    float themeChipY = sec2ContentY + 84.0f; // [contentY + 74..100]

    // Click Theme Chip 1: Midnight Bites
    float chip1X = hubX + 24.0f + 1 * 95.0f + 20.0f;
    auto themeHit1 = window.hitTestProjectHub(chip1X, themeChipY);
    REQUIRE(themeHit1.hit);
    REQUIRE(themeHit1.action == ProjectHubAction::SelectTheme);
    REQUIRE(themeHit1.themeIndex == 1);

    window.onMouseDown(0, chip1X, themeChipY);
    window.onMouseUp(0, chip1X, themeChipY);
    REQUIRE(window.getActiveThemePreset() == 1);
    REQUIRE(window.getTheme().name == "Midnight Bites");
    REQUIRE(Theme::getCurrentPreset() == Theme::Preset::MidnightBites);

    // Click Theme Chip 0: Ate Track (Default Vintage Console)
    float chip0X = hubX + 24.0f + 0 * 95.0f + 20.0f;
    auto themeHit0 = window.hitTestProjectHub(chip0X, themeChipY);
    REQUIRE(themeHit0.hit);
    REQUIRE(themeHit0.action == ProjectHubAction::SelectTheme);
    REQUIRE(themeHit0.themeIndex == 0);

    window.onMouseDown(0, chip0X, themeChipY);
    window.onMouseUp(0, chip0X, themeChipY);
    REQUIRE(window.getActiveThemePreset() == 0);
    REQUIRE(window.getTheme().name == "Ate Track");
    REQUIRE(Theme::getCurrentPreset() == Theme::Preset::AteTrack);

    // Verify renderFrame runs smoothly with active theme
    window.renderFrame();

    // Test CRT Shader Toggle
    bool initialCrt = window.isCrtShaderEnabled();
    window.setCrtShaderEnabled(!initialCrt);
    REQUIRE(window.isCrtShaderEnabled() == !initialCrt);

    // Test GUI Animations Toggle
    bool initialAnim = window.isGuiAnimationsEnabled();
    window.setGuiAnimationsEnabled(!initialAnim);
    REQUIRE(window.isGuiAnimationsEnabled() == !initialAnim);

    // Test Anti-Aliasing Mode Chips (No AA, 2x Fast, 4x RGSS)
    float aaChipY = sec2ContentY + 125.0f; // [contentY + 116..144]
    // Click 2x Fast (aaMode = 1)
    float aa1X = hubX + 24.0f + 108.0f + 20.0f;
    auto aaHit1 = window.hitTestProjectHub(aa1X, aaChipY);
    REQUIRE(aaHit1.hit);
    REQUIRE(aaHit1.action == ProjectHubAction::SetAntiAliasing);
    REQUIRE(aaHit1.aaMode == 1);
    window.onMouseDown(0, aa1X, aaChipY);
    window.onMouseUp(0, aa1X, aaChipY);
    REQUIRE(window.getAntiAliasingMode() == 1);

    // Click No AA (aaMode = 0)
    float aa0X = hubX + 24.0f + 20.0f;
    auto aaHit0 = window.hitTestProjectHub(aa0X, aaChipY);
    REQUIRE(aaHit0.hit);
    REQUIRE(aaHit0.action == ProjectHubAction::SetAntiAliasing);
    REQUIRE(aaHit0.aaMode == 0);
    window.onMouseDown(0, aa0X, aaChipY);
    window.onMouseUp(0, aa0X, aaChipY);
    REQUIRE(window.getAntiAliasingMode() == 0);

    // Click 4x RGSS (aaMode = 2)
    float aa2X = hubX + 24.0f + 216.0f + 20.0f;
    auto aaHit2 = window.hitTestProjectHub(aa2X, aaChipY);
    REQUIRE(aaHit2.hit);
    REQUIRE(aaHit2.action == ProjectHubAction::SetAntiAliasing);
    REQUIRE(aaHit2.aaMode == 2);
    window.onMouseDown(0, aa2X, aaChipY);
    window.onMouseUp(0, aa2X, aaChipY);
    REQUIRE(window.getAntiAliasingMode() == 2);

    // Test HiDPI Canvas Toggle
    float hidpiY = sec2ContentY + 165.0f;
    auto hidpiHit = window.hitTestProjectHub(hubX + hubW - 50.0f, hidpiY);
    REQUIRE(hidpiHit.hit);
    REQUIRE(hidpiHit.action == ProjectHubAction::ToggleHiDpi);
    bool prevHiDpi = window.isHiDpiEnabled();
    window.onMouseDown(0, hubX + hubW - 50.0f, hidpiY);
    window.onMouseUp(0, hubX + hubW - 50.0f, hidpiY);
    REQUIRE(window.isHiDpiEnabled() != prevHiDpi);
    window.toggleHiDpi();
    REQUIRE(window.isHiDpiEnabled() == prevHiDpi);

    // 8. Test Section 3: CRT Shader Drawer Constraints & Scrollbar Area
    window.setProjectHubSection(3);
    window.renderFrame();

    const auto& crtScrollArea = window.getProjectHubScrollArea();
    REQUIRE(crtScrollArea.canScroll());
    REQUIRE(crtScrollArea.getViewport().w == hubW - 32.0f);
    REQUIRE(crtScrollArea.getViewport().h == 272.0f);
    REQUIRE(crtScrollArea.getContentHeight() == 586.0f);
    REQUIRE(crtScrollArea.getMaxScroll() == 586.0f - 272.0f); // 314.0f

    // Verify Scrollbar Track Bounds strictly inside drawer
    auto sbTrack = crtScrollArea.getScrollbarTrackBounds();
    REQUIRE(sbTrack.x > crtScrollArea.getViewport().x);
    REQUIRE(sbTrack.x + sbTrack.w <= crtScrollArea.getViewport().x + crtScrollArea.getViewport().w);
    REQUIRE(sbTrack.y == crtScrollArea.getViewport().y);
    REQUIRE(sbTrack.h == crtScrollArea.getViewport().h);

    // Verify mouse wheel scrolling anywhere over the Settings dialog scrolls the drawer
    float initialScroll = window.getProjectHubScrollY();
    REQUIRE(initialScroll == 0.0f);
    window.onMouseScroll(0.0, -2.0); // scroll down 2 notches
    REQUIRE(window.getProjectHubScrollY() == 64.0f);

    // Test CRT Preset button in drawer (MAX CLARITY)
    // Preset row is at drawerY - scrollY + 36.0f. Let's reset scroll to 0 for exact clicking
    window.onMouseScroll(0.0, 2.0);
    REQUIRE(window.getProjectHubScrollY() == 0.0f);

    const float topContentY = hubY + 50.0f;
    const float headerStep = 36.0f;
    float drawerX = hubX + 16.0f;
    float drawerY = topContentY + 4 * headerStep;
    float presetMaxClarityX = drawerX + 180.0f;
    float presetMaxClarityY = drawerY + 48.0f;
    auto presetHit = window.hitTestProjectHub(presetMaxClarityX, presetMaxClarityY);
    REQUIRE(presetHit.hit);
    REQUIRE(presetHit.action == ProjectHubAction::CrtPresetMaxClarity);
    window.onMouseDown(0, presetMaxClarityX, presetMaxClarityY);
    window.onMouseUp(0, presetMaxClarityX, presetMaxClarityY);
    REQUIRE(window.getLastStatusMessage().find("MAX CLARITY") != std::string::npos);

    // Test CRT Slider in drawer (Curvature slider, index 1)
    float sliderRowY = drawerY + 76.0f + 1 * 46.0f + 18.0f;
    float sliderTrackX = drawerX + 12.0f;
    float sliderTrackW = (hubW - 32.0f) - 32.0f;
    auto sliderHit = window.hitTestProjectHub(sliderTrackX + sliderTrackW * 0.5f, sliderRowY);
    REQUIRE(sliderHit.hit);
    REQUIRE(sliderHit.action == ProjectHubAction::CrtSlider);
    REQUIRE(sliderHit.crtSliderIndex == 1);
    window.onMouseDown(0, sliderTrackX + sliderTrackW * 0.5f, sliderRowY);
    window.onMouseUp(0, sliderTrackX + sliderTrackW * 0.5f, sliderRowY);
    REQUIRE(window.getLastStatusMessage().find("Curvature") != std::string::npos);

    // Test switching to another section resets scroll
    window.onMouseScroll(0.0, -3.0);
    REQUIRE(window.getProjectHubScrollY() > 0.0f);
    // Click Section 4 Header (Audio Engine Config)
    float sec4HeaderY = drawerY + 272.0f + 6.0f;
    auto sec4Hit = window.hitTestProjectHub(hubX + 50.0f, sec4HeaderY + 10.0f);
    REQUIRE(sec4Hit.hit);
    REQUIRE(sec4Hit.action == ProjectHubAction::SectionHeader);
    REQUIRE(sec4Hit.sectionIndex == 4);
    window.onMouseDown(0, hubX + 50.0f, sec4HeaderY + 10.0f);
    window.onMouseUp(0, hubX + 50.0f, sec4HeaderY + 10.0f);
    REQUIRE(window.getProjectHubSection() == 4);
    REQUIRE(window.getProjectHubScrollY() == 0.0f);

    // 9. Test Global Keyboard Shortcuts for Project Operations
    // Close modal
    window.setProjectHubOpen(false);

    // Ctrl+S: Quick Save
    window.onKeyDown(83, 2); // key=83 ('S'), mods=2 (Ctrl)
    REQUIRE(window.getLastStatusMessage().find("PROJECT SAVED") != std::string::npos);

    // Ctrl+E: Quick Bounce
    window.onKeyDown(69, 2); // key=69 ('E'), mods=2 (Ctrl)
    REQUIRE(window.getLastStatusMessage().find("FAST-BOUNCE") != std::string::npos);

    // Ctrl+N: Quick New / Reset
    window.onKeyDown(78, 2); // key=78 ('N'), mods=2 (Ctrl)
    REQUIRE(window.getLastStatusMessage().find("RESET WORKSPACE") != std::string::npos);
    REQUIRE(window.getProjectName() == "Untitled Song");

    std::cout << "  [PASS] Top-Left Brand Logo Menu & Project Hub Modal tests passed." << std::endl;
}

void test3dMouseCoordinateAugmentation() {
    std::cout << "[Test] Rectilinear 1:1 Mouse Hit-Detection..." << std::endl;
    AudioEngine engine;
    GuiWindow window(1024, 768, "Test Mouse");
    window.initialize(engine);

    // In flat 2D non-3D mode, mouse coordinates are strictly 1:1 rectilinear
    REQUIRE(!window.is3dConsoleEnabled());

    float outX = 0.0f, outY = 0.0f;
    window.transform3dMouseCoords(512.0f, 25.0f, outX, outY);
    REQUIRE(std::abs(outX - 512.0f) < 0.001f);
    REQUIRE(std::abs(outY - 25.0f) < 0.001f);

    window.transform3dMouseCoords(100.0f, 200.0f, outX, outY);
    REQUIRE(std::abs(outX - 100.0f) < 0.001f);
    REQUIRE(std::abs(outY - 200.0f) < 0.001f);

    std::cout << "  [PASS] Rectilinear Mouse Hit-Detection verified." << std::endl;
}

void testCrtMouseCoordinateRemapping() {
    std::cout << "[Test] CRT Shader Coordinate Distortion & Mouse Remapping Accuracy..." << std::endl;
    AudioEngine engine;
    GuiWindow window(1280, 800, "Test CRT Remap");
    window.initialize(engine);

    float outX = 0.0f, outY = 0.0f;

    // 1. When CRT shader is disabled, remapping is strictly 1:1 pass-through
    window.setCrtShaderEnabled(false);
    window.remapCrtMouseCoords(100.0f, 200.0f, outX, outY);
    REQUIRE(std::abs(outX - 100.0f) < 0.001f);
    REQUIRE(std::abs(outY - 200.0f) < 0.001f);

    // 2. Enable CRT shader
    window.setCrtShaderEnabled(true);
    REQUIRE(window.isCrtShaderEnabled());

    // 3. Top transport bar (y < 56.0f) must be 100% pristine and 1:1 rectilinear
    window.remapCrtMouseCoords(400.0f, 25.0f, outX, outY);
    REQUIRE(std::abs(outX - 400.0f) < 0.001f);
    REQUIRE(std::abs(outY - 25.0f) < 0.001f);

    window.remapCrtMouseCoords(640.0f, 55.0f, outX, outY);
    REQUIRE(std::abs(outX - 640.0f) < 0.001f);
    REQUIRE(std::abs(outY - 55.0f) < 0.001f);

    // 4. Bottom navigation chin (y > 800 - 48 = 752.0f) must be 100% pristine and 1:1 rectilinear
    window.remapCrtMouseCoords(300.0f, 760.0f, outX, outY);
    REQUIRE(std::abs(outX - 300.0f) < 0.001f);
    REQUIRE(std::abs(outY - 760.0f) < 0.001f);

    window.remapCrtMouseCoords(800.0f, 790.0f, outX, outY);
    REQUIRE(std::abs(outX - 800.0f) < 0.001f);
    REQUIRE(std::abs(outY - 790.0f) < 0.001f);

    // 5. Exact center of DAW area must map 1:1
    // Central DAW span is 56.0f to 752.0f (height = 696.0f, center Y = 56 + 348 = 404.0f)
    // Center X = 640.0f
    window.remapCrtMouseCoords(640.0f, 404.0f, outX, outY);
    REQUIRE(std::abs(outX - 640.0f) < 0.01f);
    REQUIRE(std::abs(outY - 404.0f) < 0.01f);

    // 6. Off-center in DAW area under curvature
    // Test that coordinates smoothly remap within the phosphor aperture
    window.remapCrtMouseCoords(200.0f, 300.0f, outX, outY);
    // Under barrel distortion pulling toward center, (200, 300) samples texture from further inward
    REQUIRE(outX > 0.0f);
    REQUIRE(outX < 1280.0f);
    REQUIRE(outY >= 56.0f);
    REQUIRE(outY <= 752.0f);

    // 7. Test Flat Monitor preset (Curvature = 0.0f)
    auto matCfg = window.getDawnBridge().getMaterialConfig();
    matCfg.curvature = 0.0f;
    window.getDawnBridge().setMaterialConfig(matCfg);

    window.remapCrtMouseCoords(640.0f, 404.0f, outX, outY);
    REQUIRE(std::abs(outX - 640.0f) < 0.01f);
    REQUIRE(std::abs(outY - 404.0f) < 0.01f);

    std::cout << "  [PASS] CRT Mouse Coordinate Remapping verified with exact shader parity." << std::endl;
}

void testTrackInspectorInteraction() {
    std::cout << "[Test] Track Inspector Modular Rack, Clean Track Names & Hit-Testing..." << std::endl;
    AudioEngine engine;
    GuiWindow window(1024, 768, "Test Track Inspector");
    window.initialize(engine);

    // Verify Arranger Tracks have clean names without numeric prefixes or invalid types
    const auto& tracks = window.getArrangerTracks();
    REQUIRE(tracks.size() >= 5);
    REQUIRE((tracks[0].name == "TB-303 Acid" || tracks[0].name == "303 Acid Bass"));
    REQUIRE((tracks[1].name == "TR-808 Drums" || tracks[1].name == "TR-808 Kit"));
    REQUIRE((tracks[2].name == "Sub Bass" || tracks[2].name == "TR-909 Drive"));
    REQUIRE((tracks[3].name == "Poly Lead" || tracks[3].name == "DX7 Rhodes"));
    REQUIRE((tracks[4].name == "Waveguide Piano" || tracks[4].name == "Concert Grand"));

    for (const auto& trk : tracks) {
        REQUIRE(trk.name.rfind("01 ", 0) == std::string::npos);
        REQUIRE(trk.name.rfind("02 ", 0) == std::string::npos);
        REQUIRE(trk.name.rfind("03 ", 0) == std::string::npos);
        REQUIRE(trk.name.rfind("04 ", 0) == std::string::npos);
        REQUIRE(trk.name.rfind("05 ", 0) == std::string::npos);
    }

    // Switch to Track Inspector view
    window.setActiveView(WorkspaceView::Track);
    REQUIRE(window.getActiveView() == WorkspaceView::Track);

    // Test mouse scrolling
    REQUIRE(window.getTrackInspectorScrollY() == 0.0f);
    window.onMouseScroll(0.0, -2.0); // Scroll down
    REQUIRE(window.getTrackInspectorScrollY() > 0.0f);
    window.onMouseScroll(0.0, 5.0); // Scroll up (clamps to 0)
    REQUIRE(window.getTrackInspectorScrollY() == 0.0f);

    // SECTION 1: Track Header Card (Starts cleanly below top transport chassis at Y = 66.0)
    float curY = 66.0f;
    float contentW = (1024.0f - 80.0f) - 16.0f; // 928.0f

    // [ CODE ] button: [inspX + contentW - 245.0, curY + 15, 62, 30]
    float codeBtnX = 40.0f + contentW - 240.0f;
    auto codeHit = window.hitTestTrackInspector(codeBtnX, curY + 25.0f);
    REQUIRE(codeHit.hit);
    REQUIRE(codeHit.area == TrackInspectorHitArea::CodeButton);

    // MUTE button: [inspX + contentW - 175.0, curY + 15, 52, 30]
    float muteBtnX = 40.0f + contentW - 160.0f;
    auto muteHit = window.hitTestTrackInspector(muteBtnX, curY + 25.0f);
    REQUIRE(muteHit.hit);
    REQUIRE(muteHit.area == TrackInspectorHitArea::MuteButton);

    // Click Mute button
    bool prevMute = window.getArrangerTracks()[0].mute;
    window.onMouseDown(0, muteBtnX, curY + 25.0f);
    window.onMouseUp(0, muteBtnX, curY + 25.0f);
    REQUIRE(window.getArrangerTracks()[0].mute != prevMute);

    // Click Solo button
    float soloBtnX = 40.0f + contentW - 100.0f;
    bool prevSolo = window.getArrangerTracks()[0].solo;
    window.onMouseDown(0, soloBtnX, curY + 25.0f);
    window.onMouseUp(0, soloBtnX, curY + 25.0f);
    REQUIRE(window.getArrangerTracks()[0].solo != prevSolo);

    // SECTION 2: Channel Mixer
    curY += 60.0f + 8.0f; // 134.0f
    auto volHit = window.hitTestTrackInspector(40.0f + 150.0f, curY + 45.0f);
    REQUIRE(volHit.hit);
    REQUIRE(volHit.area == TrackInspectorHitArea::VolumeSlider);

    // Drag Volume slider
    window.onMouseDown(0, 40.0f + 80.0f, curY + 45.0f); // 0%
    window.onMouseMove(40.0f + 80.0f + 220.0f, curY + 45.0f); // 100% (1.5x)
    window.onMouseUp(0, 40.0f + 80.0f + 220.0f, curY + 45.0f);
    REQUIRE(window.getArrangerTracks()[0].volume > 1.4f);

    // SECTION 3: Dynamic Instrument Faceplate (Y = 134 + 68 + 8 = 210.0f)
    curY += 68.0f + 8.0f; // 210.0f
    // Test Preset Navigation: < PREV and NEXT >
    auto prevPresetHit = window.hitTestTrackInspector(40.0f + 30.0f, curY + 25.0f);
    REQUIRE(prevPresetHit.hit);
    REQUIRE(prevPresetHit.area == TrackInspectorHitArea::PresetPrev);

    auto nextPresetHit = window.hitTestTrackInspector(40.0f + 110.0f, curY + 25.0f);
    REQUIRE(nextPresetHit.hit);
    REQUIRE(nextPresetHit.area == TrackInspectorHitArea::PresetNext);

    // Test Hardware Synth Knobs are interactive on the faceplate
    auto hwKnobHit = window.hitTestHardwareKnob(100.0f, curY + 120.0f, 40.0f, curY, 0.70f);
    // Even generic check: hit test track inspector anywhere on the faceplate knob area
    bool foundKnob = false;
    for (float testY = curY + 60.0f; testY <= curY + 230.0f; testY += 15.0f) {
        for (float testX = 50.0f; testX < 500.0f; testX += 20.0f) {
            auto kh = window.hitTestTrackInspector(testX, testY);
            if (kh.hit && kh.area == TrackInspectorHitArea::HardwareKnob) {
                foundKnob = true;
                // Test turning knob via mouse drag
                float startVal = kh.normVal;
                (void)startVal;
                window.onMouseDown(0, testX, testY);
                window.onMouseMove(testX, testY - 60.0f); // drag up to increase
                window.onMouseUp(0, testX, testY - 60.0f);
                break;
            }
        }
        if (foundKnob) break;
    }
    REQUIRE(foundKnob);

    // SECTION 4: Harmonic Chord Follow (Y = 210 + 260 + 12 = 482.0f)
    curY += 260.0f + 12.0f; // 482.0f
    auto chordHit = window.hitTestTrackInspector(40.0f + 16.0f + 90.0f, curY + 55.0f);
    REQUIRE(chordHit.hit);
    REQUIRE(chordHit.area == TrackInspectorHitArea::ChordFollowChip);

    // Click Chord Follow Chip (CHORD mode)
    window.onMouseDown(0, 40.0f + 16.0f + 90.0f, curY + 55.0f);
    window.onMouseUp(0, 40.0f + 16.0f + 90.0f, curY + 55.0f);
    REQUIRE(window.getArrangerTracks()[0].chordFollowMode == ChordFollowMode::Chord);

    // SECTION 5: MIDI FX Rack (Y = 482 + 80 + 10 = 572.0f)
    curY += 80.0f + 10.0f; // 572.0f
    float modW = (contentW - 48.0f) / 3.0f;
    float m1X = 40.0f + 16.0f;
    auto arpToggleHit = window.hitTestTrackInspector(m1X + modW - 20.0f, curY + 38.0f);
    REQUIRE(arpToggleHit.hit);
    REQUIRE(arpToggleHit.area == TrackInspectorHitArea::MidiFxArpToggle);

    bool prevArp = window.getArrangerTracks()[0].midiFx.arpEnabled;
    window.onMouseDown(0, m1X + modW - 20.0f, curY + 38.0f);
    window.onMouseUp(0, m1X + modW - 20.0f, curY + 38.0f);
    REQUIRE(window.getArrangerTracks()[0].midiFx.arpEnabled != prevArp);

    // SECTION 6: Audio FX Rack (scroll down to bring into view)
    window.setTrackInspectorScrollY(150.0f);
    float curYFx = (572.0f + 120.0f + 10.0f) - window.getTrackInspectorScrollY(); // 702 - 150 = 552
    float fxW = (contentW - 64.0f) / 5.0f;
    auto delayToggleHit = window.hitTestTrackInspector(40.0f + 16.0f + fxW - 15.0f, curYFx + 38.0f);
    REQUIRE(delayToggleHit.hit);
    REQUIRE(delayToggleHit.area == TrackInspectorHitArea::AudioFxDelayToggle);

    bool prevDelay = window.getArrangerTracks()[0].audioFx.delayEnabled;
    window.onMouseDown(0, 40.0f + 16.0f + fxW - 15.0f, curYFx + 38.0f);
    window.onMouseUp(0, 40.0f + 16.0f + fxW - 15.0f, curYFx + 38.0f);
    REQUIRE(window.getArrangerTracks()[0].audioFx.delayEnabled != prevDelay);

    // SCROLLBAR DRAG TEST
    // Scrollbar is located at X = 40.0f + (1024.0f - 80.0f) - 8.0f = 976.0f
    float sbX = 40.0f + (1024.0f - 80.0f) - 6.0f;
    auto sbHit = window.hitTestTrackInspector(sbX, 100.0f);
    REQUIRE(sbHit.hit);
    REQUIRE((sbHit.area == TrackInspectorHitArea::ScrollbarThumb || sbHit.area == TrackInspectorHitArea::ScrollbarTrack));

    // Drag scrollbar thumb
    window.onMouseDown(0, sbX, 100.0f);
    window.onMouseMove(sbX, 300.0f);
    window.onMouseUp(0, sbX, 300.0f);
    REQUIRE(window.getTrackInspectorScrollY() > 0.0f);

    std::cout << "  [PASS] Track Inspector Modular Rack, Clean Track Names & Hit-Testing verified." << std::endl;
}

void testPianoRollNoteSelectionAndSidebar() {
    std::cout << "[Test] Piano Roll Note Selection & Decoupled Sidebar interaction..." << std::endl;

    audio::AudioEngine engine;
    REQUIRE(engine.initialize());

    GuiWindow window(1280, 800, "Selection Test");
    REQUIRE(window.initialize(engine));
    window.setActiveView(WorkspaceView::Edit);
    window.setEditSubView(EditSubView::PianoRoll);

    auto* trk = engine.getSequencer().getTrack(0);
    REQUIRE(trk != nullptr);
    trk->clear();

    // -------------------------------------------------------------
    // PART 1: SequencerTrack Selection & Batch Operations
    // -------------------------------------------------------------
    sequencer::StepData s0{}; s0.active = true; s0.note = 60; s0.velocity = 0.85f; s0.gateLength = 0.75f;
    sequencer::StepData s4{}; s4.active = true; s4.note = 48; s4.velocity = 0.85f; s4.gateLength = 0.75f;
    sequencer::StepData s8{}; s8.active = true; s8.note = 55; s8.velocity = 0.85f; s8.gateLength = 0.75f;
    trk->setStep(0, s0);
    trk->setStep(4, s4);
    trk->setStep(8, s8);

    REQUIRE(!trk->hasSelectedNotes());
    REQUIRE(trk->getSelectedNoteCount() == 0);

    // Select step 0
    trk->selectStep(0);
    REQUIRE(trk->hasSelectedNotes());
    REQUIRE(trk->isStepSelected(0));
    REQUIRE(!trk->isStepSelected(4));
    REQUIRE(trk->getSelectedNoteCount() == 1);

    // Toggle step 4 into selection
    trk->toggleStepSelection(4);
    REQUIRE(trk->isStepSelected(0));
    REQUIRE(trk->isStepSelected(4));
    REQUIRE(trk->getSelectedNoteCount() == 2);

    // Select All
    trk->selectAllNotes();
    REQUIRE(trk->getSelectedNoteCount() == 3);
    REQUIRE(trk->isStepSelected(0));
    REQUIRE(trk->isStepSelected(4));
    REQUIRE(trk->isStepSelected(8));

    // Invert Selection (all active notes were selected -> 0 selected)
    trk->invertNoteSelection();
    REQUIRE(trk->getSelectedNoteCount() == 0);
    REQUIRE(!trk->hasSelectedNotes());

    // Single step batch transformations
    trk->selectStep(0);
    trk->transposeSelectedNotes(2);
    REQUIRE(trk->getStep(0).note == 62);
    trk->transposeSelectedNotes(-14);
    REQUIRE(trk->getStep(0).note == 48);

    trk->changeSelectedNotesDuration(0.5f);
    REQUIRE(std::abs(trk->getStep(0).gateLength - 1.25f) < 0.01f);

    trk->setSelectedNotesVelocity(0.5f);
    REQUIRE(std::abs(trk->getStep(0).velocity - 0.5f) < 0.01f);

    trk->setSelectedNotesSlide(true);
    REQUIRE(trk->getStep(0).slide == true);

    trk->setSelectedNotesAccent(true);
    REQUIRE(trk->getStep(0).accent == true);

    trk->nudgeSelectedNotes(1);
    REQUIRE(!trk->getStep(0).active);
    REQUIRE(trk->getStep(1).active);
    REQUIRE(trk->isStepSelected(1));

    trk->deleteSelectedNotes();
    REQUIRE(!trk->getStep(1).active);
    REQUIRE(!trk->hasSelectedNotes());

    // -------------------------------------------------------------
    // PART 2: GuiWindow Piano Roll Hit-Testing & Note Selection Click
    // -------------------------------------------------------------
    trk->clear();
    trk->setStep(0, s0); // Note 60 at step 0
    trk->clearSelection();
    REQUIRE(!trk->hasSelectedNotes());

    // Click note 0 body in Piano Roll
    // Note 0: x in [112, 161], y in [102, 120]
    window.onMouseDown(0, 125.0f, 110.0f);
    window.onMouseUp(0, 125.0f, 110.0f);
    REQUIRE(trk->hasSelectedNotes());
    REQUIRE(trk->isStepSelected(0));

    // -------------------------------------------------------------
    // PART 3: Decoupled Selection Sidebar Hit-Testing & Actions
    // -------------------------------------------------------------
    const float bPanelY = 800.0f - 48.0f; // 752.0f
    const float sbW = 265.0f;
    const float sbX = 1280.0f - sbW - 10.0f; // 1005.0f
    const float sbY = 98.0f;
    const float sbH = bPanelY - sbY - 6.0f;

    // Transpose Semi Up (+1)
    auto semiUpHit = window.hitTestSelectionSidebar(1198.0f, 210.0f, sbX, sbY, sbW, sbH, *trk);
    REQUIRE(semiUpHit.hit);
    REQUIRE(semiUpHit.action == SelectionSidebarAction::TransposeSemiUp);
    REQUIRE(semiUpHit.paramValue == 1);

    // Click +1
    window.onMouseDown(0, 1198.0f, 210.0f);
    window.onMouseUp(0, 1198.0f, 210.0f);
    REQUIRE(trk->getStep(0).note == 61);

    // Transpose Oct Down (-12)
    auto octDownHit = window.hitTestSelectionSidebar(1035.0f, 210.0f, sbX, sbY, sbW, sbH, *trk);
    REQUIRE(octDownHit.hit);
    REQUIRE(octDownHit.action == SelectionSidebarAction::TransposeOctDown);
    REQUIRE(octDownHit.paramValue == -12);

    // Click -12
    window.onMouseDown(0, 1035.0f, 210.0f);
    window.onMouseUp(0, 1035.0f, 210.0f);
    REQUIRE(trk->getStep(0).note == 49);

    // Nudge Right (+STEP)
    auto nudgeRightHit = window.hitTestSelectionSidebar(1230.0f, 256.0f, sbX, sbY, sbW, sbH, *trk);
    REQUIRE(nudgeRightHit.hit);
    REQUIRE(nudgeRightHit.action == SelectionSidebarAction::NudgeRight);

    // Click +STEP
    window.onMouseDown(0, 1230.0f, 256.0f);
    window.onMouseUp(0, 1230.0f, 256.0f);
    REQUIRE(!trk->getStep(0).active);
    REQUIRE(trk->getStep(1).active);
    REQUIRE(trk->isStepSelected(1));

    // Duration Preset 50%
    auto dur50Hit = window.hitTestSelectionSidebar(1100.0f, 304.0f, sbX, sbY, sbW, sbH, *trk);
    REQUIRE(dur50Hit.hit);
    REQUIRE(dur50Hit.action == SelectionSidebarAction::DurPreset50);

    // Click Duration Preset 50%
    window.onMouseDown(0, 1100.0f, 304.0f);
    window.onMouseUp(0, 1100.0f, 304.0f);
    REQUIRE(std::abs(trk->getStep(1).gateLength - 0.50f) < 0.01f);

    // Velocity Preset 100%
    auto vel100Hit = window.hitTestSelectionSidebar(1174.0f, 352.0f, sbX, sbY, sbW, sbH, *trk);
    REQUIRE(vel100Hit.hit);
    REQUIRE(vel100Hit.action == SelectionSidebarAction::Vel100);

    // Click Velocity Preset 100%
    window.onMouseDown(0, 1174.0f, 352.0f);
    window.onMouseUp(0, 1174.0f, 352.0f);
    REQUIRE(std::abs(trk->getStep(1).velocity - 1.00f) < 0.01f);

    // Articulations: Slide & Accent
    auto slideHit = window.hitTestSelectionSidebar(1072.0f, 402.0f, sbX, sbY, sbW, sbH, *trk);
    REQUIRE(slideHit.hit);
    REQUIRE(slideHit.action == SelectionSidebarAction::ToggleSlide);

    window.onMouseDown(0, 1072.0f, 402.0f);
    window.onMouseUp(0, 1072.0f, 402.0f);
    REQUIRE(trk->getStep(1).slide == true);

    auto accentHit = window.hitTestSelectionSidebar(1197.0f, 402.0f, sbX, sbY, sbW, sbH, *trk);
    REQUIRE(accentHit.hit);
    REQUIRE(accentHit.action == SelectionSidebarAction::ToggleAccent);

    window.onMouseDown(0, 1197.0f, 402.0f);
    window.onMouseUp(0, 1197.0f, 402.0f);
    REQUIRE(trk->getStep(1).accent == true);

    // Batch Utilities: Select All & Invert
    // Add another note at step 5
    sequencer::StepData s5{}; s5.active = true; s5.note = 48; s5.velocity = 0.8f; s5.gateLength = 0.75f;
    trk->setStep(5, s5);

    auto selAllHit = window.hitTestSelectionSidebar(1072.0f, 454.0f, sbX, sbY, sbW, sbH, *trk);
    REQUIRE(selAllHit.hit);
    REQUIRE(selAllHit.action == SelectionSidebarAction::SelectAll);

    window.onMouseDown(0, 1072.0f, 454.0f);
    window.onMouseUp(0, 1072.0f, 454.0f);
    REQUIRE(trk->getSelectedNoteCount() == 2);
    REQUIRE(trk->isStepSelected(1));
    REQUIRE(trk->isStepSelected(5));

    auto invHit = window.hitTestSelectionSidebar(1197.0f, 454.0f, sbX, sbY, sbW, sbH, *trk);
    REQUIRE(invHit.hit);
    REQUIRE(invHit.action == SelectionSidebarAction::Invert);

    window.onMouseDown(0, 1197.0f, 454.0f);
    window.onMouseUp(0, 1197.0f, 454.0f);
    REQUIRE(trk->getSelectedNoteCount() == 0);
    REQUIRE(!trk->hasSelectedNotes());

    // -------------------------------------------------------------
    // PART 4: Marquee Selection Drag
    // -------------------------------------------------------------
    // Click on empty grid starts marquee
    window.onMouseDown(0, 250.0f, 300.0f);
    REQUIRE(window.getDragMode() == DragMode::PianoRollMarquee);
    REQUIRE(window.isMarqueeSelecting());
    window.onMouseMove(600.0f, 600.0f);
    window.onMouseUp(0, 600.0f, 600.0f);
    REQUIRE(window.getDragMode() == DragMode::None);
    REQUIRE(!window.isMarqueeSelecting());
    REQUIRE(trk->hasSelectedNotes());

    // -------------------------------------------------------------
    // PART 5: Decoupled across Edit Sub-Views (Tracker, Score, Script)
    // -------------------------------------------------------------
    trk->selectStep(1);
    REQUIRE(trk->hasSelectedNotes());

    // Switch to Tracker
    window.setEditSubView(EditSubView::Tracker);
    REQUIRE(trk->hasSelectedNotes());
    auto trkSidebarHit = window.hitTestSelectionSidebar(1198.0f, 210.0f, sbX, sbY, sbW, sbH, *trk);
    REQUIRE(trkSidebarHit.hit);
    REQUIRE(trkSidebarHit.action == SelectionSidebarAction::TransposeSemiUp);

    int prevPitch = trk->getStep(1).note;
    window.onMouseDown(0, 1198.0f, 210.0f);
    window.onMouseUp(0, 1198.0f, 210.0f);
    REQUIRE(trk->getStep(1).note == prevPitch + 1);

    // Switch to Score
    window.setEditSubView(EditSubView::Score);
    REQUIRE(trk->hasSelectedNotes());

    // Switch to Script
    window.setEditSubView(EditSubView::Script);
    REQUIRE(trk->hasSelectedNotes());

    // Switch back to PianoRoll
    window.setEditSubView(EditSubView::PianoRoll);

    // -------------------------------------------------------------
    // PART 6: Keyboard Shortcuts (Up/Down, Shift, Left/Right, Esc, Del)
    // -------------------------------------------------------------
    int notePitch = trk->getStep(1).note;
    // Up arrow (+1)
    window.onKeyDown(265, 0);
    REQUIRE(trk->getStep(1).note == notePitch + 1);

    // Down arrow (-1)
    window.onKeyDown(264, 0);
    REQUIRE(trk->getStep(1).note == notePitch);

    // Shift + Up arrow (+12)
    window.onKeyDown(265, 1);
    REQUIRE(trk->getStep(1).note == notePitch + 12);

    // Shift + Down arrow (-12)
    window.onKeyDown(264, 1);
    REQUIRE(trk->getStep(1).note == notePitch);

    // Right arrow (nudge +1 step)
    window.onKeyDown(262, 0);
    REQUIRE(!trk->getStep(1).active);
    REQUIRE(trk->getStep(2).active);
    REQUIRE(trk->isStepSelected(2));

    // Left arrow (nudge -1 step)
    window.onKeyDown(263, 0);
    REQUIRE(trk->getStep(1).active);
    REQUIRE(!trk->getStep(2).active);
    REQUIRE(trk->isStepSelected(1));

    // Escape (clear selection)
    window.onKeyDown(256, 0);
    REQUIRE(!trk->hasSelectedNotes());

    // Ctrl+A (select all)
    window.onKeyDown(65, 2);
    REQUIRE(trk->hasSelectedNotes());
    REQUIRE(trk->getSelectedNoteCount() >= 1);

    // Delete (delete selected notes)
    window.onKeyDown(261, 0);
    REQUIRE(!trk->hasSelectedNotes());
    REQUIRE(!trk->getStep(1).active);

    std::cout << "  [PASS] Piano Roll Note Selection & Decoupled Sidebar interaction passed." << std::endl;
}

void testPianoRollScrollingAndPanning() {
    std::cout << "[Test] Piano Roll 2D scrolling, middle-mouse pan & FL Studio metrics..." << std::endl;
    audio::AudioEngine engine;
    REQUIRE(engine.initialize());
    GuiWindow window(1280, 800, "Piano Roll Test");
    REQUIRE(window.initialize(engine));
    window.setActiveView(WorkspaceView::Edit);
    window.setEditSubView(EditSubView::PianoRoll);

    // Initial state: default scroll
    float initScrollX = window.getPianoRollScrollX();
    float initScrollY = window.getPianoRollScrollY();
    REQUIRE(initScrollX == 0.0f);
    REQUIRE(initScrollY == 792.0f); // Centers C4 at topY

    // 1. Middle-mouse button 2D panning
    window.onMouseDown(2, 300.0f, 300.0f); // Middle mouse down
    window.onMouseMove(250.0f, 260.0f);    // Drag left 50px, up 40px
    REQUIRE(window.getPianoRollScrollX() == 50.0f);
    REQUIRE(window.getPianoRollScrollY() == 832.0f);
    window.onMouseUp(2, 250.0f, 260.0f);

    // 2. Mouse wheel vertical scrolling
    window.onMouseScroll(0.0, 1.0); // Scroll up
    REQUIRE(window.getPianoRollScrollY() == 802.0f); // 832 - 30 = 802

    // 3. Scrollbar hit-testing
    // Vertical scrollbar is at right of grid (x: gridX + gridW + 2)
    auto vsbHit = window.hitTestPianoRollScrollbar(1120.0f, 200.0f);
    if (!vsbHit.hit) {
        vsbHit = window.hitTestPianoRollScrollbar(970.0f, 200.0f);
    }
    // Vertical scrollbar hit test
    auto anyVsbHit = window.hitTestPianoRollScrollbar(window.getWindowWidth() - 30.0f, 200.0f);
    REQUIRE((vsbHit.hit || anyVsbHit.hit));

    // Horizontal scrollbar is at bottom of grid (y: 652..662)
    auto hsbHit = window.hitTestPianoRollScrollbar(200.0f, 656.0f);
    REQUIRE(hsbHit.hit);
    REQUIRE(!hsbHit.isVertical);

    // 4. Zooming step width and row height
    window.setPianoRollStepWidth(80.0f);
    REQUIRE(window.getPianoRollStepWidth() == 80.0f);
    window.setPianoRollRowHeight(26.0f);
    REQUIRE(window.getPianoRollRowHeight() == 26.0f);

    // 5. Centering on notes
    auto* trk = engine.getSequencer().getTrack(0);
    REQUIRE(trk != nullptr);
    trk->clearSelection();
    for (uint32_t stepIdx = 0; stepIdx < trk->getNumSteps(); ++stepIdx) {
        trk->setStep(stepIdx, sequencer::StepData{});
    }
    // Place a note at pitch 72 (C5)
    sequencer::StepData s{};
    s.active = true;
    s.note = 72;
    trk->setStep(0, s);
    window.centerPianoRollOnNotes();
    // targetY = (96 - 72) * 26 - 550 * 0.5 = 24 * 26 - 275 = 624 - 275 = 349.0f
    REQUIRE(window.getPianoRollScrollY() >= 340.0f);
    REQUIRE(window.getPianoRollScrollY() <= 360.0f);

    std::cout << "  [PASS] Piano Roll 2D scrolling, panning & FL Studio metrics passed." << std::endl;
}

void testDecoupledPianoKeyboardAndDrawer() {
    std::cout << "[Test] Decoupled PianoKeyboard & Collapsible Virtual Piano Drawer..." << std::endl;

    // 1. Test Decoupled PianoKeyboard component directly
    PianoKeyboardConfig vConfig{KeyboardOrientation::Vertical, 24, 96, 3, 3, 72.0f, 22.0f, 0.64f, true};
    PianoKeyboard vKb(vConfig);
    REQUIRE(vKb.getOrientation() == KeyboardOrientation::Vertical);
    REQUIRE(vKb.getMinPitch() == 24);
    REQUIRE(vKb.getMaxPitch() == 96);
    REQUIRE(vKb.getBlackKeyRatio() == 0.64f);
    REQUIRE(PianoKeyboard::isBlackKey(61));  // C#4 is black
    REQUIRE(!PianoKeyboard::isBlackKey(60)); // C4 is white
    REQUIRE(PianoKeyboard::getNoteName(60) == "C4");
    REQUIRE(PianoKeyboard::getOctaveLabel(60) == "C4");
    REQUIRE(PianoKeyboard::getOctaveLabel(62) == ""); // D4 has no octave label

    // Hit test vertical keyboard
    auto vHit = vKb.hitTest(36.0f, 111.0f, 0.0f, 100.0f, 72.0f, 550.0f, 792.0f);
    REQUIRE(vHit.hit);
    REQUIRE(vHit.pitch == 60);
    REQUIRE(vHit.velocity >= 0.5f);

    // 2. Test Decoupled Horizontal Keyboard
    PianoKeyboardConfig hConfig{KeyboardOrientation::Horizontal, 24, 96, 3, 3, 72.0f, 140.0f, 0.65f, true};
    PianoKeyboard hKb(hConfig);
    REQUIRE(hKb.getOrientation() == KeyboardOrientation::Horizontal);
    REQUIRE(hKb.getBaseOctave() == 3);
    REQUIRE(hKb.getOctavesCount() == 3);

    // Hit test horizontal keyboard across width 800px
    auto hHit = hKb.hitTest(400.0f, 100.0f, 0.0f, 0.0f, 800.0f, 140.0f, 0.0f);
    REQUIRE(hHit.hit);
    REQUIRE(hHit.pitch >= 48); // In C3..B5 range
    REQUIRE(hHit.pitch <= 84);

    // 3. Test GuiWindow Virtual Piano Keyboard Drawer
    audio::AudioEngine engine;
    REQUIRE(engine.initialize());
    GuiWindow window(1280, 800, "Virtual Keyboard Drawer Test");
    REQUIRE(window.initialize(engine));
    window.setGuiAnimationsEnabled(false);
    REQUIRE(!window.isVirtualKeyboardDrawerOpen());

    // Pull tab hit test when closed (bPanelY = 800 - 48 = 752.0f, tabX = 625 - 70 = 555..695, tabY = 732..752)
    auto tabHitClosed = window.hitTestVirtualKeyboardDrawer(625.0f, 742.0f);
    REQUIRE(tabHitClosed.hit);
    REQUIRE(tabHitClosed.isPullTab);

    // Toggle open
    window.toggleVirtualKeyboardDrawer();
    REQUIRE(window.isVirtualKeyboardDrawerOpen());

    // Drawer Y: 752 - 175 = 577.0f, Tab Y: 557..577
    auto tabHitOpen = window.hitTestVirtualKeyboardDrawer(625.0f, 567.0f);
    REQUIRE(tabHitOpen.hit);
    REQUIRE(tabHitOpen.isPullTab);

    // Octave controls in drawer toolbar
    // [< OCT] at [240..300, 580..602]
    auto octDownHit = window.hitTestVirtualKeyboardDrawer(270.0f, 590.0f);
    REQUIRE(octDownHit.hit);
    REQUIRE(octDownHit.isOctaveDown);

    // [OCT >] at [420..480, 580..602]
    auto octUpHit = window.hitTestVirtualKeyboardDrawer(450.0f, 590.0f);
    REQUIRE(octUpHit.hit);
    REQUIRE(octUpHit.isOctaveUp);

    // Change octave
    window.setVirtualKeyboardBaseOctave(4);
    REQUIRE(window.getVirtualKeyboardBaseOctave() == 4);

    // Keyboard keys hit test inside open drawer (ky = 577 + 28 = 605..752)
    auto keyHitDrawer = window.hitTestVirtualKeyboardDrawer(300.0f, 680.0f);
    REQUIRE(keyHitDrawer.hit);
    REQUIRE(keyHitDrawer.isKey);
    REQUIRE(keyHitDrawer.pitch >= 60); // C4 base
    REQUIRE(keyHitDrawer.velocity >= 0.25f);

    // Directional velocity test on white key (C4, x=35.0f): further to the top -> higher velocity
    auto keyHitWhiteTop = window.hitTestVirtualKeyboardDrawer(35.0f, 615.0f); // Near top
    auto keyHitWhiteBottom = window.hitTestVirtualKeyboardDrawer(35.0f, 740.0f); // Near bottom
    REQUIRE(keyHitWhiteTop.hit);
    REQUIRE(keyHitWhiteBottom.hit);
    REQUIRE(!PianoKeyboard::isBlackKey(keyHitWhiteTop.pitch));
    REQUIRE(keyHitWhiteTop.pitch == keyHitWhiteBottom.pitch);
    REQUIRE(keyHitWhiteTop.velocity > keyHitWhiteBottom.velocity);

    // Directional velocity test on black key (C#4, x=75.0f): further to the top -> higher velocity
    auto keyHitBlackTop = window.hitTestVirtualKeyboardDrawer(75.0f, 615.0f); // Near top
    auto keyHitBlackBottom = window.hitTestVirtualKeyboardDrawer(75.0f, 680.0f); // Near bottom of black key
    REQUIRE(keyHitBlackTop.hit);
    REQUIRE(keyHitBlackBottom.hit);
    REQUIRE(PianoKeyboard::isBlackKey(keyHitBlackTop.pitch));
    REQUIRE(keyHitBlackTop.pitch == keyHitBlackBottom.pitch);
    REQUIRE(keyHitBlackTop.velocity > keyHitBlackBottom.velocity);

    // FL Studio clean octave labels test: ONLY C keys have octave label
    REQUIRE(PianoKeyboard::getOctaveLabel(60) == "C4");
    REQUIRE(PianoKeyboard::getOctaveLabel(59).empty()); // B3 has no label
    REQUIRE(PianoKeyboard::getOctaveLabel(58).empty()); // Bb3 has no label
    REQUIRE(PianoKeyboard::getOctaveLabel(48) == "C3");

    // Mouse click on drawer key triggers note on and sets active pitch
    window.onMouseDown(0, 300.0f, 680.0f);
    REQUIRE(window.getVirtualKeyboardActivePitch() == keyHitDrawer.pitch);
    REQUIRE(window.getPreviewingVelocity() == keyHitDrawer.velocity);
    window.onMouseUp(0, 300.0f, 680.0f);
    REQUIRE(window.getVirtualKeyboardActivePitch() == -1);

    std::cout << "  [PASS] Decoupled PianoKeyboard & Collapsible Virtual Piano Drawer passed." << std::endl;
}

void testPianoRollVerticalKeyboardAndEatsbeatsMouseHandling() {
    std::cout << "[Test] Piano Roll Vertical Keyboard & Eatsbeats Mouse Parity..." << std::endl;
    audio::AudioEngine engine;
    REQUIRE(engine.initialize());
    GuiWindow window(1280, 800, "Vertical Piano Roll Keyboard Test");
    REQUIRE(window.initialize(engine));

    window.setActiveView(WorkspaceView::Edit);
    window.setEditSubView(EditSubView::PianoRoll);

    auto* tr = engine.getSequencer().getTrack(0);
    REQUIRE(tr != nullptr);

    // 1. Vertical Keyboard Note Audition & Continuous Glissando Drag
    // C4 is at (40.0f, 111.0f)
    window.onMouseDown(0, 40.0f, 111.0f);
    REQUIRE(window.getPreviewingPitch() == 60);
    REQUIRE(window.getPreviewingVelocity() > 0.0f);

    // Directional velocity test: further to the right -> higher velocity
    auto keyHitLeft = window.hitTestPianoKey(25.0f, 111.0f);   // Near left edge (kbX=20)
    auto keyHitRight = window.hitTestPianoKey(105.0f, 111.0f); // Near right edge (kbX+kbW=110)
    REQUIRE(keyHitLeft.hit);
    REQUIRE(keyHitRight.hit);
    REQUIRE(keyHitLeft.pitch == keyHitRight.pitch);
    REQUIRE(keyHitRight.velocity > keyHitLeft.velocity);

    // Drag vertically down to B3 (y = 133.0f)
    window.onMouseMove(40.0f, 133.0f);
    REQUIRE(window.getPreviewingPitch() == 59);

    // Drag vertically down to Bb3/A#3 (y = 155.0f)
    window.onMouseMove(40.0f, 155.0f);
    REQUIRE(window.getPreviewingPitch() == 58);

    // Release mouse
    window.onMouseUp(0, 40.0f, 155.0f);
    REQUIRE(window.getPreviewingPitch() == -1);

    // 2. Right-Click on Piano Key selects all notes matching pitch on active track (_selectNotesByPitch)
    // Setup notes: step 2 at C4 (60), step 6 at C4 (60), step 4 at D4 (62)
    sequencer::StepData s2{};
    s2.active = true;
    s2.note = 60;
    s2.velocity = 0.85f;
    s2.gateLength = 1.0f;
    tr->setStep(2, s2);

    sequencer::StepData s6{};
    s6.active = true;
    s6.note = 60;
    s6.velocity = 0.85f;
    s6.gateLength = 1.0f;
    tr->setStep(6, s6);

    sequencer::StepData s4{};
    s4.active = true;
    s4.note = 62;
    s4.velocity = 0.85f;
    s4.gateLength = 1.0f;
    tr->setStep(4, s4);

    tr->clearSelection();
    REQUIRE(!tr->hasSelectedNotes());

    // Right-click on C4 key (button == 1)
    window.onMouseDown(1, 40.0f, 111.0f);
    REQUIRE(tr->isStepSelected(2));
    REQUIRE(tr->isStepSelected(6));
    REQUIRE(!tr->isStepSelected(4));
    REQUIRE(window.getPreviewingPitch() == 60);
    window.onMouseUp(1, 40.0f, 111.0f);

    // 3. Single Click on Empty Grid clears note selection (marquee candidate threshold <= 4px)
    // Mouse down and up on empty canvas (x = 500, y = 300) without dragging
    window.onMouseDown(0, 500.0f, 300.0f);
    window.onMouseUp(0, 500.0f, 300.0f);
    REQUIRE(!tr->hasSelectedNotes());

    // 4. Shift+Click Toggles Note Multi-Selection
    // Step 2 is at center: 110 + 2*70 + 35 = 285.0f, ny = 111.0f
    // Step 6 is at center: 110 + 6*70 + 35 = 565.0f, ny = 111.0f
    window.setMockShiftPressed(true);
    window.onMouseDown(0, 285.0f, 111.0f);
    window.onMouseUp(0, 285.0f, 111.0f);
    REQUIRE(tr->isStepSelected(2));

    // Shift-click step 6
    window.onMouseDown(0, 565.0f, 111.0f);
    window.onMouseUp(0, 565.0f, 111.0f);
    REQUIRE(tr->isStepSelected(2));
    REQUIRE(tr->isStepSelected(6));

    // Shift-click step 2 again to toggle it off
    window.onMouseDown(0, 285.0f, 111.0f);
    window.onMouseUp(0, 285.0f, 111.0f);
    REQUIRE(!tr->isStepSelected(2));
    REQUIRE(tr->isStepSelected(6));
    window.setMockShiftPressed(false);

    std::cout << "  [PASS] Piano Roll Vertical Keyboard & Eatsbeats Mouse Parity passed." << std::endl;
}

void testSkeuomorphicModularMixer() {
    std::cout << "[Test] Skeuomorphic Modular Studio Console, Instant Meters & Height Scaling..." << std::endl;

    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.setupDefaultAcidGraph();
    engine.initialize(cfg);

    // 1. Audio Engine Instant Meter Draining (0-bar latency check)
    float left[128] = {0.0f};
    float right[128] = {0.0f};
    for (int b = 0; b < 16; ++b) {
        engine.renderOfflineBlock(left, right, 128);
    }
    MeterFeedback fb{};
    bool polled = engine.pollMeterFeedback(fb);
    REQUIRE(polled);
    // Track 0 peak feedback
    MeterFeedback trFb{};
    bool trPolled = engine.getTrackMeterFeedback(0, trFb);
    REQUIRE(trPolled);

    // 2. GUI Window with Modular Mixer
    GuiWindow window(1280, 800, "Modular Mixer Test");
    window.initialize(engine);
    window.setActiveView(WorkspaceView::Mixer);
    REQUIRE(window.getActiveView() == WorkspaceView::Mixer);

    // Initial default state: all sections visible
    REQUIRE(window.isMixerRoutingVisible());
    REQUIRE(window.isMixerAutomationVisible());
    REQUIRE(window.isMixerPanVisible());
    REQUIRE(window.isMixerFadersVisible());
    REQUIRE(window.isMixerMetersVisible());
    REQUIRE(window.isMixerButtonsVisible());
    REQUIRE(window.isMixerReadoutsVisible());

    // 3. Preset Switching: FadersOnly
    window.setMixerSectionPreset(MixerSectionPreset::FadersOnly);
    REQUIRE(!window.isMixerRoutingVisible());
    REQUIRE(!window.isMixerAutomationVisible());
    REQUIRE(!window.isMixerPanVisible());
    REQUIRE(window.isMixerFadersVisible());
    REQUIRE(!window.isMixerMetersVisible());
    REQUIRE(!window.isMixerButtonsVisible());
    REQUIRE(window.isMixerReadoutsVisible());

    // Preset Switching: FadersAndMeters
    window.setMixerSectionPreset(MixerSectionPreset::FadersAndMeters);
    REQUIRE(!window.isMixerRoutingVisible());
    REQUIRE(!window.isMixerAutomationVisible());
    REQUIRE(!window.isMixerPanVisible());
    REQUIRE(window.isMixerFadersVisible());
    REQUIRE(window.isMixerMetersVisible());
    REQUIRE(!window.isMixerButtonsVisible());

    // Preset Switching: Full
    window.setMixerSectionPreset(MixerSectionPreset::Full);
    REQUIRE(window.isMixerRoutingVisible());
    REQUIRE(window.isMixerAutomationVisible());
    REQUIRE(window.isMixerPanVisible());
    REQUIRE(window.isMixerFadersVisible());
    REQUIRE(window.isMixerMetersVisible());
    REQUIRE(window.isMixerButtonsVisible());

    // 4. Hit-Testing Toolbar Preset Pills & Section Toggles (Y: 58..90)
    // [ALL] at X=410, Y=74
    auto allHit = window.hitTestMixer(410.0f, 74.0f);
    REQUIRE(allHit.hit);
    REQUIRE(allHit.isPresetFull);

    // [FADERS ONLY] at X=500, Y=74
    auto fadersHit = window.hitTestMixer(500.0f, 74.0f);
    REQUIRE(fadersHit.hit);
    REQUIRE(fadersHit.isPresetFaders);

    window.onMouseDown(0, 500.0f, 74.0f);
    window.onMouseUp(0, 500.0f, 74.0f);
    REQUIRE(!window.isMixerRoutingVisible());
    REQUIRE(!window.isMixerMetersVisible());
    REQUIRE(window.isMixerFadersVisible());

    // Click [FADERS + METERS] at X=630, Y=74
    auto metersHit = window.hitTestMixer(630.0f, 74.0f);
    REQUIRE(metersHit.hit);
    REQUIRE(metersHit.isPresetMeters);

    window.onMouseDown(0, 630.0f, 74.0f);
    window.onMouseUp(0, 630.0f, 74.0f);
    REQUIRE(window.isMixerMetersVisible());
    REQUIRE(!window.isMixerPanVisible());

    // Click [ALL] to restore
    window.onMouseDown(0, 410.0f, 74.0f);
    window.onMouseUp(0, 410.0f, 74.0f);
    REQUIRE(window.isMixerRoutingVisible());
    REQUIRE(window.isMixerPanVisible());
    REQUIRE(window.isMixerMetersVisible());

    // Toggle individual buttons: [PAN] at X=810, Y=74
    auto panToggleHit = window.hitTestMixer(810.0f, 74.0f);
    REQUIRE(panToggleHit.hit);
    REQUIRE(panToggleHit.isTogglePan);

    window.onMouseDown(0, 810.0f, 74.0f);
    window.onMouseUp(0, 810.0f, 74.0f);
    REQUIRE(!window.isMixerPanVisible());

    window.onMouseDown(0, 810.0f, 74.0f);
    window.onMouseUp(0, 810.0f, 74.0f);
    REQUIRE(window.isMixerPanVisible());

    // 5. Hardware Console Strip Buttons (Channel 0: cx = 195.0f)
    float cx = 195.0f;

    // Automation Mode pill: [cx + 68, 184, 50, 18] -> center (cx + 93, 193)
    auto autoHit = window.hitTestMixer(cx + 93.0f, 193.0f);
    REQUIRE(autoHit.hit);
    REQUIRE(autoHit.channelIndex == 0);
    REQUIRE(autoHit.isAutomation);

    int prevAuto = window.getMixerStrips()[0].automationMode;
    window.onMouseDown(0, cx + 93.0f, 193.0f);
    window.onMouseUp(0, cx + 93.0f, 193.0f);
    REQUIRE(window.getMixerStrips()[0].automationMode == (prevAuto + 1) % 4);

    // Phase Invert button: [cx + 80, 269, 26, 20] -> center (cx + 93, 279)
    auto phaseHit = window.hitTestMixer(cx + 93.0f, 279.0f);
    REQUIRE(phaseHit.hit);
    REQUIRE(phaseHit.channelIndex == 0);
    REQUIRE(phaseHit.isPhase);

    bool prevPhase = window.getMixerStrips()[0].phaseInvert;
    window.onMouseDown(0, cx + 93.0f, 279.0f);
    window.onMouseUp(0, cx + 93.0f, 279.0f);
    REQUIRE(window.getMixerStrips()[0].phaseInvert != prevPhase);

    // FX IN button: [cx + 80, 295, 26, 20] -> center (cx + 93, 305)
    auto fxInHit = window.hitTestMixer(cx + 93.0f, 305.0f);
    REQUIRE(fxInHit.hit);
    REQUIRE(fxInHit.channelIndex == 0);
    REQUIRE(fxInHit.isFxIn);

    bool prevFxIn = window.getMixerStrips()[0].fxIn;
    window.onMouseDown(0, cx + 93.0f, 305.0f);
    window.onMouseUp(0, cx + 93.0f, 305.0f);
    REQUIRE(window.getMixerStrips()[0].fxIn != prevFxIn);

    // 6. Full Vertical Height Scaling Verification (1080p DAW layout)
    GuiWindow win1080(1920, 1080, "Mixer 1080p Test");
    win1080.initialize(engine);
    win1080.setActiveView(WorkspaceView::Mixer);

    // In 1080p: bottomY = 1080 - 48 - 6 = 1026. Fader slot extends far below 640px.
    // Hit test fader slot near bottom: Y = 900.0f
    auto fader1080Hit = win1080.hitTestMixer(cx + 35.0f, 900.0f);
    REQUIRE(fader1080Hit.hit);
    REQUIRE(fader1080Hit.isFader);
    REQUIRE(fader1080Hit.channelIndex == 0);

    // Master fader also extends down to ~990px in 1080p
    auto master1080Hit = win1080.hitTestMixer(85.0f, 900.0f);
    REQUIRE(master1080Hit.hit);
    REQUIRE(master1080Hit.isMaster);
    REQUIRE(master1080Hit.isFader);

    std::cout << "  [PASS] Skeuomorphic Modular Studio Console & Height Scaling passed." << std::endl;
}

void testMixerTrackPropertiesSidebarAndEatsbeatsParity() {
    std::cout << "[Test] Mixer Track Properties Sidebar (Pull-Tab, Resizing, LCD Screens, Freeze, Sidebar Reuse)..." << std::endl;

    audio::AudioEngine engine;
    REQUIRE(engine.initialize());

    GuiWindow window(1280, 800, "Mixer Parity Test");
    REQUIRE(window.initialize(engine));
    window.setActiveView(WorkspaceView::Mixer);
    REQUIRE(window.getActiveView() == WorkspaceView::Mixer);

    // 1. Initial State & Configuration
    REQUIRE(window.isMixerPropertiesExpanded());
    REQUIRE(std::abs(window.getMixerPropertiesWidth() - 360.0f) < 0.01f);

    // 2. Collapse / Expand toggling via API
    window.toggleMixerProperties();
    REQUIRE(!window.isMixerPropertiesExpanded());
    window.setMixerPropertiesExpanded(true);
    REQUIRE(window.isMixerPropertiesExpanded());

    // 3. Resizing clamping
    window.setMixerPropertiesWidth(200.0f); // below min 260
    REQUIRE(std::abs(window.getMixerPropertiesWidth() - 260.0f) < 0.01f);
    window.setMixerPropertiesWidth(800.0f); // above max 640
    REQUIRE(std::abs(window.getMixerPropertiesWidth() - 640.0f) < 0.01f);
    window.setMixerPropertiesWidth(360.0f);
    REQUIRE(std::abs(window.getMixerPropertiesWidth() - 360.0f) < 0.01f);

    // 4. Hit Testing Pull-Tab & Mouse Interaction
    // Expanded: pullTabX = 1280 - 360 - 24 = 896
    float pullTabX = 1280.0f - 360.0f - 24.0f;
    auto tabHit = window.hitTestMixer(pullTabX + 12.0f, 250.0f);
    REQUIRE(tabHit.hit);
    REQUIRE(tabHit.isPullTab);

    // Click pull-tab -> collapses
    window.onMouseDown(0, pullTabX + 12.0f, 250.0f);
    window.onMouseUp(0, pullTabX + 12.0f, 250.0f);
    REQUIRE(!window.isMixerPropertiesExpanded());

    // Collapsed: pullTabX = 1280 - 24 = 1256
    float collapsedPullTabX = 1280.0f - 24.0f;
    auto colTabHit = window.hitTestMixer(collapsedPullTabX + 12.0f, 250.0f);
    REQUIRE(colTabHit.hit);
    REQUIRE(colTabHit.isPullTab);

    // Click pull-tab -> expands
    window.onMouseDown(0, collapsedPullTabX + 12.0f, 250.0f);
    window.onMouseUp(0, collapsedPullTabX + 12.0f, 250.0f);
    REQUIRE(window.isMixerPropertiesExpanded());

    // 5. Backlit LCD Status Screen Selection & Parity
    // Channel 0 LCD screen: cx = 195, [cx + 20, 116, 95, 34]
    // Channel 1 LCD screen: cx = 195 + 146 = 341
    float c1LcdX = 341.0f + 50.0f;
    float c1LcdY = 130.0f;
    auto lcdHit1 = window.hitTestMixer(c1LcdX, c1LcdY);
    REQUIRE(lcdHit1.hit);
    REQUIRE(lcdHit1.isLcdScreen);
    REQUIRE(lcdHit1.channelIndex == 1);

    // Click Channel 1 LCD screen -> selects track 1
    window.onMouseDown(0, c1LcdX, c1LcdY);
    window.onMouseUp(0, c1LcdX, c1LcdY);
    REQUIRE(window.getSelectedTrackIndex() == 1);
    REQUIRE(window.isMixerPropertiesExpanded());

    // Click Channel 0 LCD screen -> selects track 0
    float c0LcdX = 195.0f + 50.0f;
    float c0LcdY = 130.0f;
    auto lcdHit0 = window.hitTestMixer(c0LcdX, c0LcdY);
    REQUIRE(lcdHit0.hit);
    REQUIRE(lcdHit0.isLcdScreen);
    REQUIRE(lcdHit0.channelIndex == 0);
    window.onMouseDown(0, c0LcdX, c0LcdY);
    window.onMouseUp(0, c0LcdX, c0LcdY);
    REQUIRE(window.getSelectedTrackIndex() == 0);

    // 6. Channel Strip Freeze [FZ] Button Interaction
    // [cx + 80, 321, 26, 20] -> center (cx + 93, 331)
    auto fzHit = window.hitTestMixer(195.0f + 93.0f, 331.0f);
    REQUIRE(fzHit.hit);
    REQUIRE(fzHit.channelIndex == 0);
    REQUIRE(fzHit.isFreeze);

    bool prevStripFreeze = window.getMixerStrips()[0].freeze;
    bool prevArrangerFreeze = window.getArrangerTracks()[0].freeze;
    window.onMouseDown(0, 195.0f + 93.0f, 331.0f);
    window.onMouseUp(0, 195.0f + 93.0f, 331.0f);
    REQUIRE(window.getMixerStrips()[0].freeze != prevStripFreeze);
    REQUIRE(window.getArrangerTracks()[0].freeze != prevArrangerFreeze);

    // 7. Right Sidebar Track Properties Interaction without leaving Mixer
    float propX = 1280.0f - 360.0f;
    // Close button: [propX + propW - 28, topY + 7, 22, 22] -> (propX + 360 - 17, 56 + 18)
    auto closeHit = window.hitTestMixer(propX + 360.0f - 17.0f, 56.0f + 18.0f);
    REQUIRE(closeHit.hit);
    REQUIRE(closeHit.isPropertiesClose);
    window.onMouseDown(0, propX + 360.0f - 17.0f, 56.0f + 18.0f);
    window.onMouseUp(0, propX + 360.0f - 17.0f, 56.0f + 18.0f);
    REQUIRE(!window.isMixerPropertiesExpanded());

    // Re-expand
    window.setMixerPropertiesExpanded(true);

    // Interact with sidebar content: click Mute button in compact sidebar
    // Compact Mute button is at [x + contentW - 110, curY + 12, 50, 30]
    // where x = propX + 10, contentW = 340 - 16 = 324, curY = 56 + 44 = 100
    float muteBtnX = (propX + 10.0f) + 324.0f - 85.0f;
    float muteBtnY = 100.0f + 25.0f;
    auto sidebarInspHit = window.hitTestMixer(muteBtnX, muteBtnY);
    REQUIRE(sidebarInspHit.hit);
    REQUIRE(sidebarInspHit.trackInspectorHit.hit);
    REQUIRE(sidebarInspHit.trackInspectorHit.area == TrackInspectorHitArea::MuteButton);

    bool prevMute = window.getArrangerTracks()[0].mute;
    window.onMouseDown(0, muteBtnX, muteBtnY);
    window.onMouseUp(0, muteBtnX, muteBtnY);
    REQUIRE(window.getArrangerTracks()[0].mute != prevMute);

    std::cout << "  [PASS] Mixer Track Properties Sidebar & Eatsbeats Parity passed." << std::endl;
}

void testFullscreenAndLiveResize() {
    std::cout << "[Test] Fullscreen toggle (F11 / Alt+Enter / Top-Bar button) & Live Resize..." << std::endl;
    AudioEngine engine;
    engine.initialize();
    GuiWindow window(1280, 800, "Fullscreen & Live Resize Test");
    window.initialize(engine);

    // Initial state
    REQUIRE(!window.isFullscreen());

    // 1. F11 Shortcut (key = 300)
    window.onKeyDown(300, 0);
    REQUIRE(window.isFullscreen());

    // 2. Alt+Enter Shortcut (key = 257, mods = 4)
    window.onKeyDown(257, 4);
    REQUIRE(!window.isFullscreen());

    // 3. Top-bar Transport Fullscreen button hit-test
    float r = static_cast<float>(window.getWidth());
    auto fsHit = window.hitTestTransport(r - 85.0f, 20.0f);
    REQUIRE(fsHit.hit);
    REQUIRE(fsHit.action == TransportAction::FullscreenToggle);

    // 4. Click Fullscreen button
    window.onMouseDown(0, r - 85.0f, 20.0f);
    REQUIRE(window.isFullscreen());
    r = static_cast<float>(window.getWidth());
    window.onMouseDown(0, r - 85.0f, 20.0f);
    REQUIRE(!window.isFullscreen());

    // 5. Live Resize without stretching
    window.onFramebufferResize(1440, 900);
    REQUIRE(window.getFramebufferWidth() == 1440);
    REQUIRE(window.getFramebufferHeight() == 900);

    std::cout << "  [PASS] Fullscreen toggling & Live Resize tests passed." << std::endl;
}

void testFullscreenDeviceModal() {
    std::cout << "[Test] Fullscreen Device & FX Dedicated Full-Display Mode (with DAW bypass optimization)..." << std::endl;

    AudioEngine engine;
    engine.initialize();
    GuiWindow window(1280, 800, "Fullscreen Device Modal Test");
    window.initialize(engine);

    // Initial state: modal should be closed
    REQUIRE(!window.isFullscreenDeviceOpen());

    // 1. Open Fullscreen Device for Instrument on Track 0
    window.openFullscreenDevice(0);
    REQUIRE(window.isFullscreenDeviceOpen());

    // Verify DAW Native Fullscreen (F11 / Alt+Enter) works while Fullscreen Device Modal is open
    REQUIRE(!window.isFullscreen());
    window.onKeyDown(300, 0); // F11 key
    REQUIRE(window.isFullscreen());
    REQUIRE(window.isFullscreenDeviceOpen()); // Modal stays open and active!

    window.onKeyDown(257, 4); // Alt+Enter
    REQUIRE(!window.isFullscreen());
    REQUIRE(window.isFullscreenDeviceOpen());

    // Verify Spacebar toggles DAW playback while modal is open
    REQUIRE(!engine.getSequencer().isPlaying());
    window.onKeyDown(32, 0); // Spacebar
    REQUIRE(engine.getSequencer().isPlaying());
    window.onKeyDown(32, 0); // Spacebar again
    REQUIRE(!engine.getSequencer().isPlaying());

    // Verify ESC closes the modal
    window.onKeyDown(256, 0); // ESC key
    REQUIRE(!window.isFullscreenDeviceOpen());

    // 2. Open Fullscreen Audio FX
    window.openFullscreenFx(0, 0);
    REQUIRE(window.isFullscreenDeviceOpen());
    REQUIRE(window.getFullscreenDeviceModal().getTarget().type == DeviceTargetType::AudioFx);
    REQUIRE(window.getFullscreenDeviceModal().getGuiPanel().title == "Tube Distortion");

    // Verify closing via closeFullscreenDevice
    window.closeFullscreenDevice();
    REQUIRE(!window.isFullscreenDeviceOpen());

    // 3. Open Fullscreen MIDI FX
    window.openFullscreenMidiFx(0, 0);
    REQUIRE(window.isFullscreenDeviceOpen());
    REQUIRE(window.getFullscreenDeviceModal().getTarget().type == DeviceTargetType::MidiFx);
    REQUIRE(window.getFullscreenDeviceModal().getGuiPanel().title == "Scale Snap");

    // Verify Shift+F toggles it contextually
    window.onKeyDown(70, 1); // 'F' with Shift
    REQUIRE(!window.isFullscreenDeviceOpen());
    window.onKeyDown(70, 1); // 'F' with Shift again
    REQUIRE(window.isFullscreenDeviceOpen());
    REQUIRE(window.getFullscreenDeviceModal().getTarget().type == DeviceTargetType::MidiFx);

    // 4. Test renderFrame runs cleanly with underlying views bypassed
    window.renderFrame();

    // 5. Test Interactive Knob Tweaking in Fullscreen Device Modal
    // Knobs are laid out horizontally at bodyBounds_.y + 60 (around y ~ 180..220)
    float knob1X = 24.0f + (1280.0f - 48.0f) / 12.0f; // Approx first knob center
    float knob1Y = 44.0f + 16.0f + 120.0f;           // Approx knob center Y
    window.onMouseDown(0, knob1X, knob1Y);            // Start drag
    window.onMouseMove(knob1X, knob1Y - 60.0f);        // Drag upwards
    window.onMouseUp(0, knob1X, knob1Y - 60.0f);          // Finish drag
    REQUIRE(window.isFullscreenDeviceOpen());

    // 6. Test Preset Button Click (opens Preset Dialog)
    float presetBtnX = 1280.0f - 28.0f - 14.0f - 92.0f - 10.0f;
    float presetBtnY = 22.0f;
    window.onMouseDown(0, presetBtnX + 20.0f, presetBtnY);
    REQUIRE(window.isFullscreenDeviceOpen());
    // ESC closes the open Preset Dialog while keeping Fullscreen Device open
    window.onKeyDown(256, 0);
    REQUIRE(window.isFullscreenDeviceOpen());

    // 7. Test Design Chip Icon Button Click (opens Design / Code Editor)
    float designBtnX = presetBtnX - 28.0f - 10.0f;
    float designBtnY = 22.0f;
    window.onMouseDown(0, designBtnX + 14.0f, designBtnY);
    // Clicking Design icon opens the code editor and closes the modal
    REQUIRE(!window.isFullscreenDeviceOpen());
    REQUIRE(window.getActiveView() == WorkspaceView::Design);

    // 8. Re-open and test Universal Screw Close button
    window.openFullscreenDevice(0);
    REQUIRE(window.isFullscreenDeviceOpen());
    float closeX = static_cast<float>(window.getWidth()) - 24.0f;
    float closeY = 22.0f;
    window.onMouseDown(0, closeX, closeY);
    REQUIRE(!window.isFullscreenDeviceOpen());

    // 9. Test ValueEditDialog interaction in Fullscreen Device Modal
    window.openFullscreenDevice(0);
    REQUIRE(window.isFullscreenDeviceOpen());
    float kx = knob1X;
    float ky = knob1Y;
    const auto& gui = window.getFullscreenDeviceModal().getGuiPanel();
    if (!gui.rows.empty() && !gui.rows[0].widgets.empty()) {
        const auto& w = gui.rows[0].widgets[0];
        kx = w.bounds.x + w.bounds.w * 0.5f;
        ky = w.bounds.y + w.bounds.h * 0.44f;
    }
    // Right click on first knob to open ValueEditDialog
    window.onMouseDown(1, kx, ky);
    REQUIRE(window.getValueEditDialog().isOpen());
    REQUIRE(window.isFullscreenDeviceOpen());

    // Clicking ValueEditDialog's [x] close button closes the dialog but keeps Fullscreen Device open
    const auto& clBtn = window.getValueEditDialog().getCloseButtonBounds();
    float dialogCloseX = clBtn.x + clBtn.w * 0.5f;
    float dialogCloseY = clBtn.y + clBtn.h * 0.5f;
    window.onMouseDown(0, dialogCloseX, dialogCloseY);
    REQUIRE(!window.getValueEditDialog().isOpen());
    REQUIRE(window.isFullscreenDeviceOpen());

    // Right-click to open ValueEditDialog again
    window.onMouseDown(1, kx, ky);
    REQUIRE(window.getValueEditDialog().isOpen());

    // Pressing ESC closes the ValueEditDialog, keeping Fullscreen Device open
    window.onKeyDown(256, 0); // Escape
    REQUIRE(!window.getValueEditDialog().isOpen());
    REQUIRE(window.isFullscreenDeviceOpen());

    // Pressing ESC again when dialog is closed exits Fullscreen Device mode
    window.onKeyDown(256, 0); // Escape
    REQUIRE(!window.isFullscreenDeviceOpen());

    std::cout << "  [PASS] Fullscreen Device Modal tests passed." << std::endl;
}

int main() {
    std::cout << "=== Running Eatsbits Phase 6 GUI Interaction Tests ===" << std::endl;
    testDawnBridgeLifecycle();
    test3dMouseCoordinateAugmentation();
    testCrtMouseCoordinateRemapping();
    testGuiWindowInteraction();
    testPianoRollInteraction();
    testPianoRollNoteSelectionAndSidebar();
    testPianoRollScrollingAndPanning();
    testDecoupledPianoKeyboardAndDrawer();
    testPianoRollVerticalKeyboardAndEatsbeatsMouseHandling();
    testArrangerInteraction();
    testArrangerPropertiesSidebar();
    testMixerInteraction();
    testSkeuomorphicModularMixer();
    testMixerTrackPropertiesSidebarAndEatsbeatsParity();
    testTrackTabInteraction();
    testTrackInspectorInteraction();
    testPresetBrowserAndProjectActions();
    testTrackerInteraction();
    testEatscriptIdeInteraction();
    testScoreNotationInteraction();
    testNoteScriptEditorInteraction();
    testEatscriptPresetDualModeDispatch();
    testProjectBrowserMacrosInteraction();
    testProjectBrowserHistoryTabInteraction();
    testWindowResizeAndUiScale();
    testProjectHubAndTopLeftMenu();
    testFullscreenAndLiveResize();
    testFullscreenDeviceModal();
    std::cout << "=== All Phase 6 GUI Interaction Tests Passed! ===" << std::endl;
    return 0;
}

