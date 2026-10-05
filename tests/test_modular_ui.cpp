#include <iostream>
#include <cassert>
#include <cmath>
#include <string>

#include "eatsbits/ui/input/pointer_event.hpp"
#include "eatsbits/ui/views/view_base.hpp"
#include "eatsbits/ui/views/arranger_view.hpp"
#include "eatsbits/ui/views/edit_view.hpp"
#include "eatsbits/ui/views/track_inspector_view.hpp"
#include "eatsbits/ui/views/mixer_view.hpp"
#include "eatsbits/ui/views/design_view.hpp"
#include "eatsbits/ui/widgets/virtual_keyboard_drawer.hpp"
#include "eatsbits/ui/widgets/arranger_mixer_drawer.hpp"
#include "eatsbits/ui/widgets/project_browser_drawer.hpp"
#include "eatsbits/ui/widgets/transport_header.hpp"
#include "eatsbits/ui/widgets/bottom_nav_bar.hpp"
#include "eatsbits/ui/widgets/plugin_search_dialog.hpp"
#include "eatsbits/ui/widgets/text_field_state.hpp"
#include "eatsbits/ui/widgets/text_field_widget.hpp"
#include "eatsbits/ui/widgets/text_editor_widget.hpp"
#include "eatsbits/ui/widgets/value_edit_dialog.hpp"
#include "eatsbits/ui/widgets/command_palette_dialog.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/gui_window.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/project/project_file.hpp"
#include <filesystem>

using namespace eatsbits;
using namespace eatsbits::ui;

static void assertNear(float actual, float expected, float eps = 0.01f, const char* msg = "") {
    if (std::abs(actual - expected) > eps) {
        std::cerr << "Assertion failed: " << msg << " (actual: " << actual << ", expected: " << expected << ")" << std::endl;
        std::abort();
    }
}

static PointerEvent makePointer(float x, float y, PointerAction action = PointerAction::Down,
                                PointerType type = PointerType::Mouse, double timestamp = 0.0,
                                PointerButton button = PointerButton::Left) {
    PointerEvent ev;
    ev.x = x;
    ev.y = y;
    ev.rawX = x;
    ev.rawY = y;
    ev.action = action;
    ev.type = type;
    ev.timestampMs = timestamp;
    ev.button = button;
    return ev;
}

void testPointerAndGestures() {
    std::cout << "[Test 1/11] PointerEvent, KineticScroller & GestureRecognizer..." << std::endl;

    PointerEvent pe = makePointer(100.0f, 200.0f, PointerAction::Down, PointerType::Touch);
    assert(pe.isTouch());
    assert(!pe.isMouse());
    assert(pe.x == 100.0f && pe.y == 200.0f);

    PointerEvent me = makePointer(150.0f, 250.0f, PointerAction::Down, PointerType::Mouse);
    assert(me.isMouse());
    assert(!me.isTouch());

    KineticScroller scroller;
    assert(!scroller.isGliding());
    scroller.addSample(10.0f, 20.0f, 1000.0);
    scroller.addSample(50.0f, 80.0f, 1050.0);
    scroller.endDrag(1050.0);
    assert(scroller.isGliding());
    assert(scroller.getVx() > 100.0f);
    assert(scroller.getVy() > 100.0f);

    float dx = 0.0f, dy = 0.0f;
    scroller.step(0.016f, dx, dy);
    assert(dx > 0.0f);
    assert(dy > 0.0f);

    scroller.reset();
    assert(!scroller.isGliding());

    GestureRecognizer gr;
    GestureRecognizer::GestureEvent ge;

    // Mouse click with 1px movement (< 3px mouse slop threshold) -> Should be Tap
    gr.onPointerDown(makePointer(100.0f, 100.0f, PointerAction::Down, PointerType::Mouse, 100.0));
    gr.onPointerMove(makePointer(101.0f, 100.0f, PointerAction::Move, PointerType::Mouse, 150.0), ge);
    bool tapOk = gr.onPointerUp(makePointer(101.0f, 100.0f, PointerAction::Up, PointerType::Mouse, 180.0), ge);
    assert(tapOk);
    assert(ge.kind == GestureKind::Tap);

    // Touch gesture with 25px movement (> 16px touch slop threshold) -> Should be Drag
    gr.onPointerDown(makePointer(200.0f, 200.0f, PointerAction::Down, PointerType::Touch, 500.0));
    bool dragOk = gr.onPointerMove(makePointer(230.0f, 200.0f, PointerAction::Move, PointerType::Touch, 550.0), ge);
    assert(dragOk);
    assert(ge.kind == GestureKind::Drag);

    std::cout << "  [PASS] Pointer & Gestures validated." << std::endl;
}

void testArrangerView() {
    std::cout << "[Test 2/11] ArrangerView (Loop Resize, Clip Move across tracks, Contextual Properties, Minimap)..." << std::endl;

    ArrangerView arranger;
    assert(arranger.getTracks().size() == 5);

    // 1. Verify NO Master Bus track exists in Arranger
    for (const auto& tr : arranger.getTracks()) {
        assert(tr.name != "Master Bus");
    }

    ViewContext ctx;
    ctx.isMobile = false;

    Rect2D bounds(0.0f, 56.0f, 1280.0f, 696.0f);
    arranger.layout(bounds, ctx);
    assert(arranger.getBounds().w == 1280.0f);

    // 2. Track Header Click -> Exposes Track Properties
    arranger.setInspectorTab(ArrangerInspectorTab::Clip);
    arranger.handlePointer(makePointer(50.0f, 120.0f, PointerAction::Down), ctx);
    assert(arranger.getActiveTrack() == 0);
    assert(arranger.getInspectorTab() == ArrangerInspectorTab::Track);
    assert(arranger.isInspectorOpen());

    // 3. Clip Click -> Exposes Clip Properties
    // Track 0 Clip 0 starts at bar 1 (x: 190, y: 100, w: 4*64=256, h: 64)
    arranger.handlePointer(makePointer(240.0f, 120.0f, PointerAction::Down), ctx);
    arranger.handlePointer(makePointer(240.0f, 120.0f, PointerAction::Up), ctx);
    assert(arranger.getSelectedClip() == 0);
    assert(arranger.getInspectorTab() == ArrangerInspectorTab::Clip);
    assert(arranger.isInspectorOpen());

    // 4. Clip Loop Resize Handle (Click top right loop icon on clip 0)
    // Clip 0 right edge is at ~446 (190 + 256). Loop handle is at x in [424..446], y in [100..120]
    auto& clip0 = arranger.getTracks()[0].clips[0];
    assert(!clip0.isLooped);
    arranger.handlePointer(makePointer(438.0f, 110.0f, PointerAction::Down), ctx);
    // Drag right by 2 bars (+128px)
    arranger.handlePointer(makePointer(438.0f + 128.0f, 110.0f, PointerAction::Move), ctx);
    arranger.handlePointer(makePointer(438.0f + 128.0f, 110.0f, PointerAction::Up), ctx);
    assert(clip0.lengthBars == 6);
    assert(clip0.isLooped);
    assert(clip0.loopLengthBars == 4);

    // 5. Clip Movement Across Tracks (Drag clip body from Track 0 down to Track 1)
    size_t trk0InitialClips = arranger.getTracks()[0].clips.size();
    size_t trk1InitialClips = arranger.getTracks()[1].clips.size();

    // Down on body of clip 0
    arranger.handlePointer(makePointer(250.0f, 120.0f, PointerAction::Down), ctx);
    // Drag down by 1 track row (+64px)
    arranger.handlePointer(makePointer(250.0f, 120.0f + 68.0f, PointerAction::Move), ctx);
    arranger.handlePointer(makePointer(250.0f, 120.0f + 68.0f, PointerAction::Up), ctx);

    assert(arranger.getTracks()[0].clips.size() == trk0InitialClips - 1);
    assert(arranger.getTracks()[1].clips.size() == trk1InitialClips + 1);
    assert(arranger.getActiveTrack() == 1);

    // 6. '+ ADD' row click below last track -> opens PluginSearchDialog
    // 5 tracks * 64px = 320px + offset
    float addRowY = 100.0f + 5.0f * 64.0f + 12.0f;
    arranger.handlePointer(makePointer(60.0f, addRowY, PointerAction::Down), ctx);
    assert(arranger.getPluginSearchDialog().isOpen());
    assert(arranger.getPluginSearchDialog().getMode() == PluginDialogMode::AddInstrument);
    arranger.getPluginSearchDialog().close();
    assert(!arranger.getPluginSearchDialog().isOpen());

    // 7. Middle-Click 2D Viewport Panning
    PointerEvent midDown = makePointer(400.0f, 300.0f, PointerAction::Down, PointerType::Mouse, 0.0, PointerButton::Middle);
    arranger.handlePointer(midDown, ctx);
    PointerEvent midMove = makePointer(350.0f, 280.0f, PointerAction::Move, PointerType::Mouse, 0.0, PointerButton::Middle);
    arranger.handlePointer(midMove, ctx);
    PointerEvent midUp = makePointer(350.0f, 280.0f, PointerAction::Up, PointerType::Mouse, 0.0, PointerButton::Middle);
    arranger.handlePointer(midUp, ctx);

    // 8. Minimap Scrub
    // Minimap is at y: 56 + 28 = 84, h: 16
    PointerEvent miniScrub = makePointer(300.0f, 92.0f, PointerAction::Down);
    arranger.handlePointer(miniScrub, ctx);

    // 9. Add new track: MUST NOT add default clip (clips.empty() == true)
    bool trackAddedNotified = false;
    uint32_t addedTrackIdx = 999;
    arranger.onTrackAdded = [&](uint32_t idx) {
        trackAddedNotified = true;
        addedTrackIdx = idx;
    };
    arranger.addTrack("Audio Vocals", "audio", 0.3f, 0.7f, 1.0f);
    assert(trackAddedNotified);
    assert(addedTrackIdx == 5);
    assert(arranger.getTracks().size() == 6);
    assert(arranger.getTracks()[5].clips.empty()); // Zero default clips!

    // 10. Double click empty grid to create new empty clip
    // Track 5 row is at y: 100 + 5*64 = 420. Bar 3 is at x: 190 + 2*64 = 318
    bool clipsChangedNotified = false;
    arranger.onClipsChanged = [&]() { clipsChangedNotified = true; };
    arranger.handlePointer(makePointer(320.0f, 440.0f, PointerAction::Down), ctx);
    arranger.handlePointer(makePointer(320.0f, 440.0f, PointerAction::Down), ctx); // Double click
    assert(arranger.getTracks()[5].clips.size() == 1);
    assert(arranger.getTracks()[5].clips[0].startBar == 3);
    assert(arranger.getTracks()[5].clips[0].notes.empty());
    assert(clipsChangedNotified);

    // 11. addClipToTrack appends after existing clips
    arranger.addClipToTrack(5);
    assert(arranger.getTracks()[5].clips.size() == 2);
    assert(arranger.getTracks()[5].clips[1].startBar >= 7);

    // 12. Duplicate track
    size_t countBeforeDup = arranger.getTracks().size();
    arranger.duplicateTrack(5);
    assert(arranger.getTracks().size() == countBeforeDup + 1);
    assert(arranger.getTracks().back().name.find("Copy") != std::string::npos);
    assert(arranger.getTracks().back().clips.size() == 2);

    // 13. Delete track
    size_t countBeforeDel = arranger.getTracks().size();
    arranger.deleteTrack(static_cast<uint32_t>(countBeforeDel - 1));
    assert(arranger.getTracks().size() == countBeforeDel - 1);

    std::cout << "  [PASS] ArrangerView validated." << std::endl;
}

void testEditView() {
    std::cout << "[Test 3/11] EditView (Piano Roll, Tracker, Score, Script, Note Inspector Sidebar)..." << std::endl;

    EditView edit;
    assert(edit.getSubView() == EditSubViewMode::PianoRoll);

    // 1. Sub-view switching
    edit.setSubView(EditSubViewMode::Tracker);
    assert(edit.getSubView() == EditSubViewMode::Tracker);

    edit.setSubView(EditSubViewMode::Score);
    assert(edit.getSubView() == EditSubViewMode::Score);

    edit.setSubView(EditSubViewMode::Script);
    assert(edit.getSubView() == EditSubViewMode::Script);

    edit.setSubView(EditSubViewMode::PianoRoll);
    assert(edit.getSubView() == EditSubViewMode::PianoRoll);

    // 2. Ghost notes opacity & toggle
    edit.setGhostNotesOpacity(0.45f);
    assertNear(edit.getGhostNotesOpacity(), 0.45f);
    edit.setGhostNotesOpacity(1.5f);
    assertNear(edit.getGhostNotesOpacity(), 1.0f);
    edit.toggleGhostNotes();
    assertNear(edit.getGhostNotesOpacity(), 0.0f);
    edit.toggleGhostNotes();
    assert(edit.getGhostNotesOpacity() > 0.0f);

    // 3. Track binding
    edit.setActiveTrackIndex(0);
    assert(edit.getActiveTrackIndex() == 0);
    edit.setActiveTrackIndex(3);
    assert(edit.getActiveTrackIndex() == 3);
    edit.setActiveTrackIndex(0);

    // 4. Note Selection & Sidebar Visibility
    assert(!edit.hasSelectedNotes());
    assert(edit.getSelectedNoteCount() == 0);

    edit.selectAllNotes();
    assert(edit.hasSelectedNotes());
    assert(edit.getSelectedNoteCount() == edit.getNotes().size());

    edit.clearSelection();
    assert(!edit.hasSelectedNotes());

    edit.getNotes()[0].isSelected = true;
    assert(edit.hasSelectedNotes());
    assert(edit.getSelectedNoteCount() == 1);

    // 5. Note Transformations & Batch operations
    uint8_t initialPitch = edit.getNotes()[0].pitch;
    edit.transposeSelectedNotes(2);
    assert(edit.getNotes()[0].pitch == initialPitch + 2);
    edit.transposeSelectedNotes(-2);
    assert(edit.getNotes()[0].pitch == initialPitch);

    float initialStep = edit.getNotes()[0].startStep;
    edit.nudgeSelectedNotes(1.0f);
    assertNear(edit.getNotes()[0].startStep, initialStep + 1.0f);
    edit.nudgeSelectedNotes(-1.0f);
    assertNear(edit.getNotes()[0].startStep, initialStep);

    edit.changeSelectedNotesDuration(0.5f);
    assert(edit.getNotes()[0].durationSteps > 1.0f);

    edit.setSelectedNotesVelocity(0.75f);
    assertNear(edit.getNotes()[0].velocity, 0.75f);

    edit.setSelectedNotesSlide(true);
    assert(edit.getNotes()[0].isSlide);
    edit.setSelectedNotesSlide(false);
    assert(!edit.getNotes()[0].isSlide);

    edit.setSelectedNotesArticulation("pizzicato");
    assert(edit.getNotes()[0].articulation == "pizzicato");

    // 6. Note creation, deletion, retrograde, and pitch inversion
    size_t beforeAddCount = edit.getNotes().size();
    edit.addNote(64, 20.0f, 2.0f, 0.90f, true, true);
    assert(edit.getNotes().size() == beforeAddCount + 1);
    assert(edit.getNotes().back().pitch == 64);
    assertNear(edit.getNotes().back().startStep, 20.0f);
    assert(edit.getNotes().back().isSlide);
    assert(edit.getNotes().back().isAccent);

    edit.deleteNoteAt(20.0f, 64);
    assert(edit.getNotes().size() == beforeAddCount);

    // Retrograde test on selected notes
    edit.clearSelection();
    edit.addNote(60, 0.0f, 1.0f);
    edit.addNote(67, 4.0f, 1.0f);
    edit.selectAllNotes();
    edit.retrogradeSelectedNotes();
    // After retrograde, note originally at 0.0 should now be at end
    edit.invertSelectedNotesPitch();
    edit.clearSelection();

    // 7. QWERTY Tracker Keymap Input & Arrow Navigation
    edit.setSubView(EditSubViewMode::Tracker);
    size_t initialCount = edit.getNotes().size();
    ViewContext ctx;
    // Press 'Z' (key 90) -> C note entry
    edit.handleKey(90, 0, 1, 0, ctx);
    assert(edit.getNotes().size() >= initialCount);

    // Arrow navigation
    edit.handleKey(265, 0, 1, 0, ctx); // Up
    edit.handleKey(264, 0, 1, 0, ctx); // Down
    edit.handleKey(262, 0, 1, 0, ctx); // Right
    edit.handleKey(263, 0, 1, 0, ctx); // Left

    // 8. Layout & Pointer Interaction
    edit.layout(Rect2D{0.0f, 56.0f, 1280.0f, 700.0f}, ctx);

    // Middle-click 2D Pan
    PointerEvent midDown = makePointer(500.0f, 300.0f, PointerAction::Down, PointerType::Mouse, 0.0, PointerButton::Middle);
    edit.handlePointer(midDown, ctx);
    PointerEvent midMove = makePointer(450.0f, 260.0f, PointerAction::Move, PointerType::Mouse, 0.0, PointerButton::Middle);
    edit.handlePointer(midMove, ctx);
    PointerEvent midUp = makePointer(450.0f, 260.0f, PointerAction::Up, PointerType::Mouse, 0.0, PointerButton::Middle);
    edit.handlePointer(midUp, ctx);

    // Scroll wheel navigation
    PointerEvent scrollEv;
    scrollEv.action = PointerAction::Scroll;
    scrollEv.scrollX = 1.0f;
    scrollEv.scrollY = 2.0f;
    edit.handlePointer(scrollEv, ctx);

    std::cout << "  [PASS] EditView validated." << std::endl;
}

void testTrackInspectorView() {
    std::cout << "[Test 4/11] TrackInspectorView (Ribbon, Header, Mixer, Faceplate, Chord Follow, FX Racks)..." << std::endl;

    TrackInspectorView inspector;
    assert(inspector.getActiveTrack() == 0);
    assert(inspector.getTrackCount() == 5);

    // 1. Validate default track channels
    assert(inspector.getTrack(0).name == "303 Acid Bass");
    assert(inspector.getTrack(0).instrumentEngine == "tb303");
    assert(inspector.getTrack(1).name == "TR-808 Kit");
    assert(inspector.getTrack(1).instrumentEngine == "tr808");
    assert(inspector.getTrack(3).name == "DX7 Rhodes");
    assert(inspector.getTrack(3).instrumentEngine == "dx7");

    // 2. Track selection and navigation
    inspector.setActiveTrack(2);
    assert(inspector.getActiveTrack() == 2);
    assert(inspector.getTrack(2).name == "TR-909 Drive");

    // 3. Knobs inspection
    assert(inspector.getTrack(0).knobs.size() == 6);
    assert(inspector.getTrack(0).knobs[1].name == "cutoff");

    // 4. Volume, Pan, Mute, Solo, Freeze
    auto& trk = inspector.getTrack(0);
    trk.volume = 1.15f;
    trk.pan = 0.40f;
    trk.mute = true;
    trk.solo = true;
    trk.freeze = true;
    assert(trk.volume > 1.10f);
    assert(trk.pan > 0.35f);
    assert(trk.mute);
    assert(trk.solo);
    assert(trk.freeze);

    // 5. Harmonic Chord Follow
    trk.chordFollowMode = ChordFollowMode::Bass;
    assert(trk.chordFollowMode == ChordFollowMode::Bass);
    trk.chordFollowMode = ChordFollowMode::Chord;
    assert(trk.chordFollowMode == ChordFollowMode::Chord);

    // 6. MIDI FX Rack & Audio FX Rack
    assert(trk.midiFx.arpEnabled);
    assert(trk.midiFx.scaleSnapEnabled);
    assert(trk.midiFx.humanizeEnabled);
    assert(trk.audioFx.delayEnabled);
    assert(trk.audioFx.convolverEnabled);

    // 7. Oscilloscope buffer
    float testScope[256];
    for (int i = 0; i < 256; ++i) testScope[i] = std::sin(static_cast<float>(i) * 0.1f);
    inspector.setAudioScopeBuffer(testScope, 256);

    // 8. Layout computation & Scrolling
    ViewContext ctx;
    ctx.logicalWidth = 1280.0f;
    ctx.logicalHeight = 800.0f;
    inspector.layout(Rect2D{0.0f, 60.0f, 1280.0f, 700.0f}, ctx);
    assert(inspector.getScrollY() == 0.0f);
    inspector.setScrollY(50.0f);
    assert(inspector.getScrollY() == 50.0f);

    // 9. Embedded PluginSearchDialog modal lifecycle
    assert(!inspector.getPluginSearchDialog().isOpen());
    inspector.getPluginSearchDialog().open(PluginDialogMode::AddInstrument, "303 Acid Bass", 0);
    assert(inspector.getPluginSearchDialog().isOpen());
    inspector.getPluginSearchDialog().close();
    assert(!inspector.getPluginSearchDialog().isOpen());

    // 10. Pointer interaction: clicking track ribbon tab
    PointerEvent pe;
    pe.action = PointerAction::Down;
    pe.type = PointerType::Mouse;
    pe.x = 30.0f;
    pe.y = 80.0f; // in ribbon area
    inspector.handlePointer(pe, ctx);

    std::cout << "  [PASS] TrackInspectorView validated with full Eatsbeats parity." << std::endl;
}

void testMixerView() {
    std::cout << "[Test 5/11] MixerView..." << std::endl;

    MixerView mixer;
    assert(mixer.getChannels().size() == 5);
    assert(mixer.getMasterChannel().name == "MASTER");

    ViewContext ctx;
    ctx.isMobile = false;
    mixer.layout(Rect2D(0.0f, 56.0f, 1280.0f, 696.0f), ctx);

    assert(mixer.getMasterBounds().x == 0.0f);
    assert(mixer.getMasterBounds().w == 140.0f);

    mixer.setMasterFader(0.75f);
    assertNear(mixer.getMasterChannel().fader, 0.75f);

    assert(!mixer.isPropertiesExpanded());
    mixer.setPropertiesExpanded(true);
    assert(mixer.isPropertiesExpanded());

    // Phase 3: Audio Taper Scaling Tests
    using namespace eatsbits::presenter::audio_taper;
    assertNear(travelToGain(0.75f), 1.0f); // 0 dB unity
    assertNear(gainToTravel(1.0f), 0.75f);
    assertNear(travelToGain(1.0f), 1.5f);  // Max gain
    assertNear(gainToTravel(1.5f), 1.0f);
    assertNear(travelToGain(0.0f), 0.0f);  // -inf
    assertNear(gainToTravel(0.0f), 0.0f);
    assert(formatDb(1.0f) == "0.0 dB");
    assert(formatDb(0.0f) == "-inf dB");

    // Phase 3: Structured Fader Geometry & Hit-Testing
    auto mGeom = mixer.getMasterFaderGeometry();
    assert(mGeom.wellH >= 100.0f);
    assert(mGeom.thumb.w > 0.0f && mGeom.thumb.h > 0.0f);
    assert(mixer.hitTestMasterFader(mGeom.thumb.x + 5.0f, mGeom.thumb.y + 5.0f));

    auto chGeom = mixer.getChannelFaderGeometry(0, 195.0f);
    assert(chGeom.wellH >= 100.0f);
    assert(chGeom.thumb.w > 0.0f && chGeom.thumb.h > 0.0f);
    assert(mixer.hitTestChannelFader(0, 195.0f, chGeom.thumb.x + 5.0f, chGeom.thumb.y + 5.0f));

    // Phase 3: Double-click to reset fader to 0 dB unity (1.0f)
    mixer.setMasterFader(0.2f);
    assertNear(mixer.getMasterChannel().fader, 0.2f);
    PointerEvent click1 = makePointer(mGeom.trackX, mGeom.thumbY, PointerAction::Down, PointerType::Mouse);
    mixer.handlePointer(click1, ctx);
    PointerEvent up1 = makePointer(mGeom.trackX, mGeom.thumbY, PointerAction::Up, PointerType::Mouse);
    mixer.handlePointer(up1, ctx);
    PointerEvent click2 = makePointer(mGeom.trackX, mGeom.thumbY, PointerAction::Down, PointerType::Mouse);
    mixer.handlePointer(click2, ctx);
    assertNear(mixer.getMasterChannel().fader, 1.0f); // Reset to unity!

    // Phase 3: Shift+Drag Fine Trim vs Normal Drag
    mixer.setMasterFader(1.0f);
    mGeom = mixer.getMasterFaderGeometry();
    PointerEvent dragStart = makePointer(mGeom.trackX, mGeom.thumbY, PointerAction::Down, PointerType::Mouse);
    mixer.handlePointer(dragStart, ctx);
    PointerEvent dragMoveNormal = makePointer(mGeom.trackX, mGeom.thumbY - 20.0f, PointerAction::Move, PointerType::Mouse);
    mixer.handlePointer(dragMoveNormal, ctx);
    float normalGain = mixer.getMasterChannel().fader;
    PointerEvent dragEnd = makePointer(mGeom.trackX, mGeom.thumbY - 20.0f, PointerAction::Up, PointerType::Mouse);
    mixer.handlePointer(dragEnd, ctx);

    mixer.setMasterFader(1.0f);
    mGeom = mixer.getMasterFaderGeometry();
    PointerEvent dragStart2 = makePointer(mGeom.trackX, mGeom.thumbY, PointerAction::Down, PointerType::Mouse);
    mixer.handlePointer(dragStart2, ctx);
    PointerEvent dragMoveShift = makePointer(mGeom.trackX, mGeom.thumbY - 20.0f, PointerAction::Move, PointerType::Mouse);
    dragMoveShift.mods.shift = true;
    mixer.handlePointer(dragMoveShift, ctx);
    float shiftGain = mixer.getMasterChannel().fader;
    PointerEvent dragEnd2 = makePointer(mGeom.trackX, mGeom.thumbY - 20.0f, PointerAction::Up, PointerType::Mouse);
    mixer.handlePointer(dragEnd2, ctx);

    assert(normalGain > 1.0f);
    assert(shiftGain > 1.0f);
    assert((shiftGain - 1.0f) < (normalGain - 1.0f)); // Fine trim moved much less!

    // Phase 4: Modular Density Modes & Responsive Auto-Density
    assert(mixer.getDensityMode() == MixerDensityMode::Comfortable);
    assert(!mixer.isToolbarVisible());
    assertNear(mixer.getChannelStripWidth(), 140.0f);
    assertNear(mixer.getMasterStripWidth(), 140.0f);

    mixer.setDensityMode(MixerDensityMode::Compact);
    assert(mixer.getDensityMode() == MixerDensityMode::Compact);
    assertNear(mixer.getChannelStripWidth(), 78.0f);
    assertNear(mixer.getMasterStripWidth(), 90.0f);

    mixer.setDensityMode(MixerDensityMode::Micro);
    assert(mixer.getDensityMode() == MixerDensityMode::Micro);
    assertNear(mixer.getChannelStripWidth(), 50.0f);
    assertNear(mixer.getMasterStripWidth(), 65.0f);

    // Toolbar visibility toggle
    mixer.toggleToolbar();
    assert(mixer.isToolbarVisible());
    mixer.setToolbarVisible(false);
    assert(!mixer.isToolbarVisible());

    // Responsive auto-density thresholds
    mixer.setAutoDensityEnabled(true);
    mixer.layout(Rect2D(0.0f, 56.0f, 500.0f, 600.0f), ctx);
    assert(mixer.getDensityMode() == MixerDensityMode::Micro);

    mixer.layout(Rect2D(0.0f, 56.0f, 750.0f, 600.0f), ctx);
    assert(mixer.getDensityMode() == MixerDensityMode::Compact);

    mixer.layout(Rect2D(0.0f, 56.0f, 1280.0f, 600.0f), ctx);
    assert(mixer.getDensityMode() == MixerDensityMode::Comfortable);

    // Mobile force-micro test
    ctx.isMobile = true;
    mixer.layout(Rect2D(0.0f, 56.0f, 1280.0f, 600.0f), ctx);
    assert(mixer.getDensityMode() == MixerDensityMode::Micro);
    ctx.isMobile = false;

    // Reset back to Comfortable desktop for remaining tests
    mixer.setAutoDensityEnabled(false);
    mixer.setDensityMode(MixerDensityMode::Comfortable);
    mixer.layout(Rect2D(0.0f, 56.0f, 1280.0f, 696.0f), ctx);

    // Verify channel shrinking when tracks are deleted
    std::vector<std::string> names3 = {"Track 1", "Track 2", "Track 3"};
    std::vector<float> vols3 = {0.8f, 0.8f, 0.8f};
    std::vector<float> pans3 = {0.0f, 0.0f, 0.0f};
    std::vector<bool> mutes3 = {false, false, false};
    std::vector<bool> solos3 = {false, false, false};
    std::vector<bool> freezes3 = {false, false, false};
    std::vector<Color> cols3 = {Color(1.0f, 0.0f, 0.0f), Color(0.0f, 1.0f, 0.0f), Color(0.0f, 0.0f, 1.0f)};
    mixer.syncFromWindow(names3, vols3, pans3, mutes3, solos3, freezes3, cols3,
                         0, 1.0f, 0.0f, false, 0.0f, 0.0f, nullptr, nullptr, 0,
                         false, 300.0f, true, true, true, true, true, true, false, 0.0f);
    assert(mixer.getChannels().size() == 3);

    std::cout << "  [PASS] MixerView validated." << std::endl;
}

void testDesignView() {
    std::cout << "[Test 6/11] DesignView (Modular Rack & Visual GUI Designer)..." << std::endl;

    DesignView design;
    assert(design.getSubMode() == DesignSubMode::ModularRack);

    design.setSubMode(DesignSubMode::Eatscript);
    assert(design.getSubMode() == DesignSubMode::Eatscript);

    // Initial patch cords
    assert(design.getPatchCords().size() >= 2);
    const auto& c1 = design.getPatchCords()[0];
    assert(c1.r > 0.0f || c1.g > 0.0f || c1.b > 0.0f);

    // 1. Test Modular Rack Layout & Interactive Knob Dragging
    design.setSubMode(DesignSubMode::ModularRack);
    assert(design.getModules().size() == 4);

    const ThemeTokens& themeTokens = Theme::current();
    BatchRenderer2D renderer;
    ViewContext ctx;
    ctx.renderer = &renderer;
    ctx.theme = &themeTokens;
    ctx.isMobile = false;

    design.layout(Rect2D{0.0f, 56.0f, 1280.0f, 700.0f}, ctx);

    std::string changedParamName;
    float changedParamVal = -1.0f;
    design.onParamChanged = [&](const std::string&, const std::string& pName, float val) {
        changedParamName = pName;
        changedParamVal = val;
    };

    // Grab OSC 1 TUNE knob
    const auto& oscMod = design.getModules()[0];
    float knobX = oscMod.x + 40.0f;
    float knobY = oscMod.y + 65.0f;
    float origTuneVal = oscMod.knobs[0].value;

    PointerEvent pDown = makePointer(knobX, knobY, PointerAction::Down);
    bool handledDown = design.handlePointer(pDown, ctx);
    assert(handledDown);

    // Drag knob upwards
    PointerEvent pMove = makePointer(knobX, knobY - 30.0f, PointerAction::Move);
    bool handledMove = design.handlePointer(pMove, ctx);
    assert(handledMove);
    assert(design.getModules()[0].knobs[0].value > origTuneVal);
    assert(changedParamName == "TUNE");

    PointerEvent pUp = makePointer(knobX, knobY - 30.0f, PointerAction::Up);
    bool handledUp = design.handlePointer(pUp, ctx);
    assert(handledUp);

    // 2. Test Interactive Patch Cable Dragging & Connection
    // Start dragging from OSC 1 OUT jack
    float outJackX = oscMod.x + oscMod.w - 30.0f;
    float outJackY = oscMod.y + oscMod.h - 45.0f;
    PointerEvent pJackDown = makePointer(outJackX, outJackY, PointerAction::Down);
    bool handledJackDown = design.handlePointer(pJackDown, ctx);
    assert(handledJackDown);

    // Move cable to VCF AUDIO IN
    const auto& vcfMod = design.getModules()[1];
    float inJackX = vcfMod.x + 30.0f;
    float inJackY = vcfMod.y + vcfMod.h - 45.0f;

    PointerEvent pJackMove = makePointer(inJackX, inJackY, PointerAction::Move);
    bool handledJackMove = design.handlePointer(pJackMove, ctx);
    assert(handledJackMove);

    size_t cordsBefore = design.getPatchCords().size();
    PointerEvent pJackUp = makePointer(inJackX, inJackY, PointerAction::Up);
    bool handledJackUp = design.handlePointer(pJackUp, ctx);
    assert(handledJackUp);

    // 3. Test Modular Rack Reset Patch & Add Module
    design.addModule({"LFO MODULATOR", "LFO", 0.0f, 0.0f, 200.0f, 320.0f, 0.2f, 0.2f, 0.2f,
                      {{"RATE", 0.5f, 0.1f, 20.0f, "Hz"}}, {"SYNC"}, {"TRI"}});
    assert(design.getModules().size() == 5);

    design.resetPatch();
    assert(design.getPatchCords().size() == 2);

    // 4. Test Visual GUI Designer
    design.setSubMode(DesignSubMode::GuiDesigner);
    assert(design.getSubMode() == DesignSubMode::GuiDesigner);

    const auto& panel = design.getGuiPanel();
    assert(!panel.rows.empty());
    assert(panel.rows[0].widgets.size() >= 3);

    // Test Design Mode vs Live Interaction Mode
    design.setGuiDesignMode(true);
    assert(design.isGuiDesignMode());

    // Add a new row
    size_t rowsBefore = design.getGuiPanel().rows.size();
    design.addGuiRow();
    assert(design.getGuiPanel().rows.size() == rowsBefore + 1);

    // Add widgets from palette to row
    size_t widgetsBefore = design.getGuiPanel().rows.back().widgets.size();
    design.addGuiWidget(GuiWidgetType::Knob, GuiKnobStyle::CreamFluted, "PITCH DIAL", "Cutoff");
    assert(design.getGuiPanel().rows.back().widgets.size() == widgetsBefore + 1);

    // Add hardware slider and nixie display
    design.addGuiWidget(GuiWidgetType::Slider, GuiKnobStyle::Standard, "FINE TUNE", "Tuning");
    design.addGuiWidget(GuiWidgetType::NixieDisplay, GuiKnobStyle::Standard, "BPM DISPLAY", "Tempo");
    assert(design.getGuiPanel().rows.back().widgets.size() == widgetsBefore + 3);

    // Test widget selection and inspector property edits
    design.selectDesignerWidget(static_cast<int>(design.getGuiPanel().rows.size()) - 1, 0);
    assert(design.getSelectedDesignerRow() == static_cast<int>(design.getGuiPanel().rows.size()) - 1);
    assert(design.getSelectedDesignerWidget() == 0);

    // Duplicate selected widget
    size_t rowWidgetsCount = design.getGuiPanel().rows.back().widgets.size();
    design.duplicateSelectedGuiWidget();
    assert(design.getGuiPanel().rows.back().widgets.size() == rowWidgetsCount + 1);

    // Delete selected widget
    design.deleteSelectedGuiWidget();
    assert(design.getGuiPanel().rows.back().widgets.size() == rowWidgetsCount);

    // Select chassis and change theme
    design.selectDesignerChassis();
    assert(design.isChassisSelected());
    design.getGuiPanel().chassisStyle = GuiChassisStyle::PcbGreen;
    assert(design.getGuiPanel().chassisStyle == GuiChassisStyle::PcbGreen);

    // Verify script serialization contains GUI definition
    assert(design.getScriptCode().find("# --- Hardware GUI Layout ---") != std::string::npos);

    // Switch to Live Interaction Mode and verify widget tweaking
    design.setGuiDesignMode(false);
    assert(!design.isGuiDesignMode());

    // Layout and render without crashing
    design.layout(Rect2D{0.0f, 56.0f, 1280.0f, 700.0f}, ctx);
    design.render(ctx);

    std::cout << "  [PASS] DesignView validated." << std::endl;
}

void testVirtualKeyboardDrawer() {
    std::cout << "[Test 7/13] VirtualKeyboardDrawer & Dual-Mode Instrument Auditioning..." << std::endl;

    VirtualKeyboardDrawer drawer;
    assert(!drawer.isExpanded());
    assert(drawer.getMode() == KeyboardDrawerMode::Piano);

    drawer.layout(1280.0f, 752.0f);
    float collapsedH = drawer.getDrawerHeight();
    assertNear(collapsedH, 22.0f, 1.0f);

    drawer.toggleExpanded();
    assert(drawer.isExpanded());
    float expandedH = drawer.getDrawerHeight();
    assert(expandedH > collapsedH);

    drawer.setBaseOctave(4);
    assert(drawer.getBaseOctave() == 4);

    // Test Dual-Mode switching: Piano vs. DrumPads
    drawer.setMode(KeyboardDrawerMode::DrumPads);
    assert(drawer.getMode() == KeyboardDrawerMode::DrumPads);

    drawer.layout(1280.0f, 752.0f);
    float drumExpandedH = drawer.getDrawerHeight();
    assert(drumExpandedH > expandedH); // 16-pad drum matrix allocates comfortable height

    drawer.update(0.016f);
    assert(drawer.getDrumPadGrid().getActivePads().size() == 16);

    std::cout << "  [PASS] VirtualKeyboardDrawer validated." << std::endl;
}

void testDrumPadGridWidget() {
    std::cout << "[Test 8/13] DrumPadGridWidget (16-Pad MPC Matrix, Banks, Velocity & Triggers)..." << std::endl;

    DrumPadGridWidget grid;
    assert(grid.getBank() == DrumKitBank::CoreKit);
    assert(grid.getProfile() == DrumKitProfile::StandardGm);

    // 1. Verify Core Kit 16 pads
    const auto& corePads = grid.getActivePads();
    assert(corePads.size() == 16);
    assert(corePads[0].note == 49);  // CRASH 1
    assert(corePads[8].note == 42);  // CLOSED HAT
    assert(corePads[12].note == 36); // KICK 1
    assert(corePads[13].note == 38); // AC. SNARE
    assert(corePads[15].note == 39); // HAND CLAP

    // 2. Layout 4x4 Grid
    grid.layout(Rect2D{10.0f, 600.0f, 800.0f, 150.0f});
    for (const auto& pad : grid.getActivePads()) {
        assert(pad.bounds.w > 0.0f);
        assert(pad.bounds.h > 0.0f);
        assert(pad.bounds.x >= 10.0f);
        assert(pad.bounds.y >= 600.0f);
    }

    // 3. Test Bank Switching to Percussion
    grid.setBank(DrumKitBank::Percussion);
    assert(grid.getBank() == DrumKitBank::Percussion);
    const auto& percPads = grid.getActivePads();
    assert(percPads.size() == 16);
    assert(percPads[0].note == 57);  // CRASH 2
    assert(percPads[2].note == 56);  // COWBELL
    assert(percPads[4].note == 60);  // HI BONGO
    assert(percPads[8].note == 62);  // MUTE CONGA
    assert(percPads[12].note == 75); // CLAVES
    assert(percPads[15].note == 81); // TRIANGLE

    // 4. Test Kit Profiles (808 / 909)
    grid.setProfile(DrumKitProfile::Eats808);
    assert(grid.getProfile() == DrumKitProfile::Eats808);
    grid.setProfile(DrumKitProfile::Eats909);
    assert(grid.getProfile() == DrumKitProfile::Eats909);

    // 5. Test Pad Trigger Callback & Velocity Sensitivity
    grid.setBank(DrumKitBank::CoreKit);
    uint8_t triggeredNote = 0;
    float triggeredVel = 0.0f;
    uint8_t releasedNote = 0;

    grid.onPadTrigger = [&](uint8_t note, float vel) {
        triggeredNote = note;
        triggeredVel = vel;
    };
    grid.onPadRelease = [&](uint8_t note) {
        releasedNote = note;
    };

    // Click on Kick 1 pad (index 12, Note 36)
    const auto& kickPad = grid.getActivePads()[12];
    assert(kickPad.note == 36);

    PointerEvent downEv = makePointer(kickPad.bounds.x + 10.0f, kickPad.bounds.y + 5.0f, PointerAction::Down);
    bool handledDown = grid.handlePointer(downEv);
    assert(handledDown);
    assert(triggeredNote == 36);
    assert(triggeredVel > 0.80f); // Top of pad gives high velocity
    assert(grid.getActivePads()[12].isTriggered);

    // Update timers
    grid.update(0.20f); // Timer should expire
    assert(!grid.getActivePads()[12].isTriggered);

    // Release pad
    PointerEvent upEv = makePointer(kickPad.bounds.x + 10.0f, kickPad.bounds.y + 5.0f, PointerAction::Up);
    bool handledUp = grid.handlePointer(upEv);
    assert(handledUp);
    assert(releasedNote == 36);

    std::cout << "  [PASS] DrumPadGridWidget validated with 16-pad MPC matrix." << std::endl;
}

void testProjectBrowserDrawer() {
    std::cout << "[Test 8/11] ProjectBrowserDrawer (6 Workstation Tabs, Presets, Projects, History)..." << std::endl;

    ProjectBrowserDrawer browser;
    assert(!browser.isOpen());

    browser.open();
    assert(browser.isOpen());

    // Test all 6 tabs
    browser.setTab(BrowserDrawerTab::Assets);
    assert(browser.getTab() == BrowserDrawerTab::Assets);

    browser.setTab(BrowserDrawerTab::Scripts);
    assert(browser.getTab() == BrowserDrawerTab::Scripts);

    browser.setTab(BrowserDrawerTab::Presets);
    assert(browser.getTab() == BrowserDrawerTab::Presets);

    browser.setTab(BrowserDrawerTab::Packs);
    assert(browser.getTab() == BrowserDrawerTab::Packs);

    browser.setTab(BrowserDrawerTab::Projects);
    assert(browser.getTab() == BrowserDrawerTab::Projects);

    browser.setTab(BrowserDrawerTab::History);
    assert(browser.getTab() == BrowserDrawerTab::History);

    // Layout
    browser.layout(1280.0f, 800.0f, 56.0f, 48.0f);
    assert(browser.getDrawerBounds().w == ProjectBrowserDrawer::getDrawerWidth());

    // Callbacks
    bool presetTriggered = false;
    browser.onSelectPreset = [&](const std::string& id) {
        presetTriggered = true;
        assert(!id.empty());
    };
    if (browser.onSelectPreset) {
        browser.onSelectPreset("acid_303");
    }
    assert(presetTriggered);

    bool projectTriggered = false;
    browser.onLoadProject = [&](const std::string& path) {
        projectTriggered = true;
        assert(!path.empty());
    };
    if (browser.onLoadProject) {
        browser.onLoadProject("./Projects/demo.eats");
    }
    assert(projectTriggered);

    bool undoTriggered = false;
    browser.onUndo = [&]() {
        undoTriggered = true;
    };
    if (browser.onUndo) {
        browser.onUndo();
    }
    assert(undoTriggered);

    std::cout << "  [PASS] ProjectBrowserDrawer validated." << std::endl;
}

void testTransportHeader() {
    std::cout << "[Test 9/11] TransportHeader..." << std::endl;

    TransportHeader transport;
    transport.layout(1280.0f, 56.0f);
    assert(transport.getBounds().w == 1280.0f);
    assert(transport.getBounds().h == 56.0f);

    bool playToggled = false;
    transport.onTogglePlay = [&]() { playToggled = true; };

    PointerEvent click = makePointer(90.0f, 28.0f, PointerAction::Down, PointerType::Mouse);
    transport.handlePointer(click, nullptr);
    assert(playToggled);

    std::cout << "  [PASS] TransportHeader validated." << std::endl;
}

void testBottomNavBar() {
    std::cout << "[Test 10/11] BottomNavBar..." << std::endl;

    BottomNavBar nav;
    nav.layout(1280.0f, 800.0f, 48.0f);
    assert(nav.getBounds().y == 752.0f);
    assert(nav.getBounds().h == 48.0f);

    int newTab = -1;

    nav.handleKey('3', 0, 1, 0, newTab);
    assert(newTab == 2);

    nav.handleKey('4', 0, 1, 0, newTab);
    assert(newTab == 3);

    nav.handleKey('5', 0, 1, 0, newTab);
    assert(newTab == 4);

    PointerEvent click = makePointer(50.0f, 770.0f, PointerAction::Down, PointerType::Mouse);
    bool hit = nav.handlePointer(click, newTab);
    assert(hit);
    assert(newTab == 0);

    std::cout << "  [PASS] BottomNavBar validated." << std::endl;
}

void testGuiWindowIntegration() {
    std::cout << "[Test 11/11] GuiWindow Modular Subsystem Integration..." << std::endl;

    audio::AudioEngine engine;
    engine.initialize();

    GuiWindow window(1280, 800, "Modular Eatsbits Test");
    bool initOk = window.initialize(engine);
    assert(initOk);

    // Verify all modular views and widgets were instantiated
    assert(window.getModularArrangerView() != nullptr);
    assert(window.getModularEditView() != nullptr);
    assert(window.getModularTrackInspectorView() != nullptr);
    assert(window.getModularMixerView() != nullptr);
    assert(window.getModularDesignView() != nullptr);
    assert(window.getVirtualKeyboardDrawerWidget() != nullptr);
    assert(window.getProjectBrowserDrawerWidget() != nullptr);
    assert(window.getTransportHeaderWidget() != nullptr);
    assert(window.getBottomNavBarWidget() != nullptr);

    // Verify NO Master Bus track in Arranger
    for (const auto& tr : window.getModularArrangerView()->getTracks()) {
        assert(tr.name != "Master Bus");
    }

    // Mute/Solo routing and cross-view synchronization verification
    // 1. Verify track mute state
    window.setTrackMuteState(0, true);
    assert(window.getModularArrangerView()->getTracks()[0].mute == true);
    assert(engine.getSequencer().getTrack(0)->isMuted() == true);

    // 2. Verify track solo state isolation
    window.setTrackSoloState(3, true);
    assert(window.getModularArrangerView()->getTracks()[3].solo == true);
    assert(engine.getSequencer().getTrack(3)->isSolo() == true);
    // Unsolo
    window.setTrackSoloState(3, false);
    assert(window.getModularArrangerView()->getTracks()[3].solo == false);
    assert(engine.getSequencer().getTrack(3)->isSolo() == false);
    // Unmute
    window.setTrackMuteState(0, false);
    assert(window.getModularArrangerView()->getTracks()[0].mute == false);

    // Clip synchronization to EditView verification (DX7 Rhodes Track 3)
    window.syncActiveClipToEditView(3, 0);
    assert(window.getModularEditView()->getActiveTrackIndex() == 3);
    assert(window.getModularEditView()->getActiveClipName() == "Chords A");
    assert(window.getModularEditView()->getNotes().size() == 16);
    assert(window.getModularArrangerView()->getTracks()[3].clips[0].detectedChords.size() == 4);

    // Verify Sequencer track received polyphonic chord extra notes
    auto* seqTrk3 = engine.getSequencer().getTrack(3);
    assert(seqTrk3 != nullptr);
    assert(seqTrk3->getStep(0).active);
    assert(seqTrk3->getStep(0).note == 60);
    assert(seqTrk3->getStep(0).extraNotes.size() == 3); // 63, 67, 70

    // 3. Verify Timeline Arranger-to-Sequencer synchronization:
    // Track 2 (TR-909 Drive): default clip starts at Bar 5 (step 64). Bar 1..4 (step 0..63) MUST be silent!
    auto* seqTrk2 = engine.getSequencer().getTrack(2);
    assert(seqTrk2 != nullptr);
    for (uint32_t s = 0; s < 64; ++s) {
        assert(!seqTrk2->getStep(s).active); // Silence before Bar 5!
    }
    // Bar 5 (step 64) has four-on-the-floor kick!
    assert(seqTrk2->getStep(64).active);
    assert(seqTrk2->getStep(64).note == 36);

    // Track 4 (Concert Grand): default clip starts at Bar 9 (step 128). Steps 0..127 MUST be silent!
    auto* seqTrk4 = engine.getSequencer().getTrack(4);
    assert(seqTrk4 != nullptr);
    for (uint32_t s = 0; s < 128; ++s) {
        assert(!seqTrk4->getStep(s).active); // Silence before Bar 9!
    }
    assert(seqTrk4->getStep(128).active);

    // Track 1 (TR-808 Kit): Verify 132 BPM tempo rhythm (snare at step 4 and step 12)
    auto* seqTrk1 = engine.getSequencer().getTrack(1);
    assert(seqTrk1 != nullptr);
    assert(seqTrk1->getStep(0).active); // Kick on step 0
    assert(seqTrk1->getStep(0).note == 36);
    assert(seqTrk1->getStep(4).active); // Snare on step 4
    assert(seqTrk1->getStep(4).note == 38);
    assert(seqTrk1->getStep(8).active); // Kick on step 8
    assert(seqTrk1->getStep(8).note == 36);
    assert(seqTrk1->getStep(12).active); // Snare on step 12
    assert(seqTrk1->getStep(12).note == 38);

    // Test moving clip in Arranger:
    // Move TR-909 clip from Bar 5 to Bar 1
    window.getModularArrangerView()->getTracks()[2].clips[0].startBar = 1;
    window.syncArrangerToSequencer();
    // Now Bar 1 (step 0) MUST have kick, and Bar 5 (step 64..79) MUST be silent!
    assert(seqTrk2->getStep(0).active);
    assert(seqTrk2->getStep(0).note == 36);
    assert(!seqTrk2->getStep(64).active); // Bar 5 is now silent!

    // Restore TR-909 back to Bar 5
    window.getModularArrangerView()->getTracks()[2].clips[0].startBar = 5;
    window.syncArrangerToSequencer();

    std::cout << "  [PASS] GuiWindow Modular Integration validated." << std::endl;
}

void testValueEditDialogAndBackdropBlur() {
    std::cout << "[Test 12/12] ValueEditDialog, Right-Click Editing & Backdrop Blur..." << std::endl;

    // 0. Test TextFieldState in isolation
    {
        TextFieldState tf("1.000", /*selectAllOnSet=*/ true);
        assert(tf.hasSelection());
        assert(tf.getSelectedText() == "1.000");
        assert(tf.getSelectionStart() == 0);
        assert(tf.getSelectionEnd() == 5);

        // Immediate typing replaces the selected text
        tf.insertChar('3');
        assert(!tf.hasSelection());
        assert(tf.getText() == "3");
        assert(tf.getCursor() == 1);

        // Append text
        tf.insertText(".75");
        assert(tf.getText() == "3.75");
        assert(tf.getCursor() == 4);

        // Select all
        tf.selectAll();
        assert(tf.hasSelection());
        assert(tf.getSelectedText() == "3.75");

        // Backspace removes selection
        tf.backspace();
        assert(tf.empty());
        assert(!tf.hasSelection());

        // Directional cursor and Shift-selection
        tf.setText("ABCDE", false);
        assert(!tf.hasSelection());
        assert(tf.getCursor() == 5);
        tf.moveLeft(/*select=*/ true);
        assert(tf.hasSelection());
        assert(tf.getSelectedText() == "E");
        tf.moveLeft(/*select=*/ true);
        assert(tf.getSelectedText() == "DE");
        tf.moveHome(/*select=*/ false);
        assert(!tf.hasSelection());
        assert(tf.getCursor() == 0);
    }

    // 0.5. Test unified TextFieldWidget
    {
        FocusManager fm;
        ViewContext ctx;
        ctx.focusManager = &fm;

        TextFieldWidget widget("My Track Name", "Enter name...");
        widget.setBounds(100.0f, 100.0f, 200.0f, 32.0f);
        assert(!widget.isFocused());
        assert(widget.getText() == "My Track Name");

        // Pointer click acquires focus
        PointerEvent click = makePointer(120.0f, 115.0f, PointerAction::Down);
        bool handled = widget.handlePointer(click, ctx);
        assert(handled);
        assert(widget.isFocused());
        assert(fm.hasFocus(&widget));

        // Typing via handleChar
        widget.handleChar(U'!');
        assert(widget.getText().find('!') != std::string::npos);

        // Defocus via clearFocus
        fm.clearFocus();
        assert(!widget.isFocused());
    }

    // 1. Direct ValueEditDialog tests
    ValueEditDialog dialog;
    assert(!dialog.isOpen());

    float committedVal = -999.0f;
    ValueEditRequest req;
    req.title = "MASTER VOLUME";
    req.paramName = "Volume";
    req.currentValue = 1.0f;
    req.minValue = 0.0f;
    req.maxValue = 2.0f;
    req.defaultValue = 1.0f;
    req.hasDefault = true;
    req.allowPercentage = true;
    req.unit = "dB";
    req.accentColor = Color(0.12f, 0.85f, 0.95f);
    req.onCommit = [&](float v) {
        committedVal = v;
    };

    dialog.open(req);
    assert(dialog.isOpen());
    assert(dialog.getRequest().title == "MASTER VOLUME");
    assert(!dialog.isPercentMode());
    assert(dialog.getInputText() == "1.000");

    // UX Feature: Existing value must be selected by default on open!
    assert(dialog.hasSelection());
    assert(dialog.getSelectedText() == "1.000");
    assert(dialog.getSelectionStart() == 0);
    assert(dialog.getSelectionEnd() == 5);

    // Immediate typing replaces the entire selected value without needing backspace
    dialog.handleKey(50 /* '2' */, 0, 1, 0);
    assert(dialog.getInputText() == "2");
    assert(!dialog.hasSelection());
    assert(dialog.getCursorPosition() == 1);

    // Continue typing '.' and '5'
    dialog.handleKey(46 /* '.' */, 0, 1, 0);
    dialog.handleKey(53 /* '5' */, 0, 1, 0);
    assert(dialog.getInputText() == "2.5");

    // Layout
    dialog.layout(1280.0f, 800.0f);
    Rect2D bounds = dialog.getDialogBounds();
    assert(bounds.w > 300.0f && bounds.h > 150.0f);
    // Dialog should be centered in display
    assertNear(bounds.x + bounds.w * 0.5f, 640.0f, 2.0f);
    assertNear(bounds.y + bounds.h * 0.5f, 400.0f, 2.0f);

    // Ctrl+A Select All
    dialog.handleKey(65 /* 'A' */, 0, 1, 2 /* GLFW_MOD_CONTROL */);
    assert(dialog.hasSelection());
    assert(dialog.getSelectedText() == "2.5");

    // Mouse click inside input field: click at index 0 clears selection and positions cursor
    Rect2D ifb = Rect2D(bounds.x + 20.0f, bounds.y + 68.0f, bounds.w - 40.0f, 40.0f);
    PointerEvent peClick = makePointer(ifb.x + 12.0f, ifb.y + 20.0f, PointerAction::Down);
    dialog.handlePointer(peClick);
    assert(!dialog.hasSelection());
    assert(dialog.getCursorPosition() == 0);

    // Mouse drag inside input field: drag to 2nd char positions cursor and expands selection
    float charW = getMonoCharAdvance(16.0f);
    PointerEvent peDrag = makePointer(ifb.x + 12.0f + 2.0f * charW, ifb.y + 20.0f, PointerAction::Move);
    dialog.handlePointer(peDrag);
    assert(dialog.hasSelection());
    assert(dialog.getSelectionStart() == 0);
    assert(dialog.getSelectionEnd() == 2);
    assert(dialog.getSelectedText() == "2.");

    PointerEvent peUp = makePointer(ifb.x + 12.0f + 2.0f * charW, ifb.y + 20.0f, PointerAction::Up);
    dialog.handlePointer(peUp);

    // Toggle to percentage mode
    dialog.togglePercentMode();
    assert(dialog.isPercentMode());
    // In percentage mode, new value is also selected by default for instant typing
    assert(dialog.hasSelection());

    // Toggle back to raw mode
    dialog.togglePercentMode();
    assert(!dialog.isPercentMode());
    assert(dialog.hasSelection());

    // Test text entry with %: "25%"
    dialog.setInputText("25%", /*selectAll=*/ false);
    // Commit via Enter key (GLFW_KEY_ENTER = 257, action = 1)
    dialog.handleKey(257, 0, 1, 0);
    assert(!dialog.isOpen());
    // 25% of [0, 2] is 0.0 + 0.25 * 2.0 = 0.5f
    assertNear(committedVal, 0.5f, 0.01f);

    // Reopen and test Cancel
    committedVal = -999.0f;
    dialog.open(req);
    assert(dialog.isOpen());
    // Press Escape (GLFW_KEY_ESCAPE = 256, action = 1)
    dialog.handleKey(256, 0, 1, 0);
    assert(!dialog.isOpen());
    assert(committedVal == -999.0f); // Callback was not invoked

    // Reopen and test Reset to default (also selected by default)
    dialog.open(req);
    dialog.setInputText("0.2", false);
    dialog.resetToDefault();
    assertNear(std::stof(dialog.getInputText()), 1.0f, 0.01f);
    assert(dialog.hasSelection());
    assert(dialog.getSelectedText() == "1.00");
    dialog.close();

    // 1b. Text Mode ValueEditDialog (Track Renaming & Icon Link)
    std::string committedText = "";
    bool actionLinkClicked = false;
    ValueEditRequest textReq;
    textReq.title = "EDIT TRACK PROPERTIES";
    textReq.paramName = "Track Name";
    textReq.isTextMode = true;
    textReq.initialText = "Audio 1";
    textReq.accentColor = Color(0.2f, 0.8f, 0.9f);
    textReq.actionLinkLabel = "🎨 Choose Track Icon...";
    textReq.onActionLink = [&]() {
        actionLinkClicked = true;
    };
    textReq.onCommitText = [&](const std::string& t) {
        committedText = t;
    };

    dialog.open(textReq);
    assert(dialog.isOpen());
    assert(dialog.isTextMode());
    assert(dialog.getInputText() == "Audio 1");
    assert(dialog.hasSelection()); // Selected by default

    // Immediate typing replaces text
    dialog.handleKey(83 /* 'S' */, 0, 1, 1 /* Shift */); // 'S'
    assert(dialog.getInputText() == "S");
    dialog.handleKey(89 /* 'Y' */, 0, 1, 0); // 'y'
    dialog.handleKey(78 /* 'N' */, 0, 1, 0); // 'n'
    dialog.handleKey(84 /* 'T' */, 0, 1, 0); // 't'
    dialog.handleKey(72 /* 'H' */, 0, 1, 0); // 'h'
    dialog.handleKey(32 /* ' ' */, 0, 1, 0); // ' '
    dialog.handleKey(49 /* '1' */, 0, 1, 0); // '1'
    assert(dialog.getInputText() == "Synth 1");

    // Layout
    dialog.layout(1280.0f, 800.0f);
    Rect2D tBounds = dialog.getDialogBounds();
    assert(tBounds.w == 400.0f);
    assert(tBounds.h >= 200.0f);

    // Commit via Enter
    dialog.handleKey(257 /* ENTER */, 0, 1, 0);
    assert(!dialog.isOpen());
    assert(committedText == "Synth 1");

    // Reopen and test action link click
    dialog.open(textReq);
    assert(dialog.isOpen());
    actionLinkClicked = false;
    PointerEvent peActionLink = makePointer(tBounds.x + 50.0f, tBounds.y + 120.0f, PointerAction::Down);
    dialog.handlePointer(peActionLink);
    assert(dialog.isOpen()); // Kept open so user can pick an icon and continue editing title!
    assert(actionLinkClicked);
    assert(committedText == "Audio 1"); // Preserved text before transitioning
    dialog.setIconRef("preset:inst_drums");
    assert(dialog.getIconRef() == "preset:inst_drums");
    dialog.close();
    assert(!dialog.isOpen());

    // Color lerp validation
    Color cA(0.0f, 0.0f, 0.0f, 1.0f);
    Color cB(1.0f, 1.0f, 1.0f, 1.0f);
    Color cMid = Color::lerp(cA, cB, 0.5f);
    assertNear(cMid.r, 0.5f, 0.01f);
    assertNear(cMid.g, 0.5f, 0.01f);
    assertNear(cMid.b, 0.5f, 0.01f);

    // 2. MixerView Right-Click & LCD Integration
    MixerView mixer;
    ViewContext ctx;
    ctx.logicalWidth = 1280.0f;
    ctx.logicalHeight = 800.0f;
    ValueEditRequest receivedReq;
    bool editRequested = false;
    ctx.onOpenValueEdit = [&](const ValueEditRequest& r) {
        editRequested = true;
        receivedReq = r;
    };

    mixer.layout(Rect2D(0.0f, 56.0f, 1280.0f, 696.0f), ctx);

    // Right-click on Master Volume Fader
    PointerEvent peMasterRight = makePointer(
        37.0f, 300.0f,
        PointerAction::Down, PointerType::Mouse, 0.0, PointerButton::Right);
    editRequested = false;
    bool handledMaster = mixer.handlePointer(peMasterRight, ctx);
    assert(handledMaster);
    assert(editRequested);
    assert(receivedReq.paramName == "Master Volume");
    assert(receivedReq.allowPercentage);

    // Right-click on Master Pan Knob (kx = 12 + 70 = 82, ky = 56 + 8 + 38 + 16 = 118)
    PointerEvent peMasterPanRight = makePointer(
        82.0f, 118.0f,
        PointerAction::Down, PointerType::Mouse, 0.0, PointerButton::Right);
    editRequested = false;
    bool handledMasterPan = mixer.handlePointer(peMasterPanRight, ctx);
    assert(handledMasterPan);
    assert(editRequested);
    assert(receivedReq.paramName == "Master Pan");

    // Right-click on Channel 0 Fader (cx = 195.0f, x = 220.0f, y = 300.0f)
    if (!mixer.getChannels().empty()) {
        PointerEvent peCh0Right = makePointer(
            220.0f, 300.0f,
            PointerAction::Down, PointerType::Mouse, 0.0, PointerButton::Right);
        editRequested = false;
        bool handledCh0 = mixer.handlePointer(peCh0Right, ctx);
        assert(handledCh0);
        assert(editRequested);
        assert(receivedReq.allowPercentage);

        // Right-click on Channel 0 Pan Knob (kx = 195 + 130/2 = 260, ky = 118)
        PointerEvent peCh0PanRight = makePointer(
            260.0f, 118.0f,
            PointerAction::Down, PointerType::Mouse, 0.0, PointerButton::Right);
        editRequested = false;
        bool handledCh0Pan = mixer.handlePointer(peCh0PanRight, ctx);
        assert(handledCh0Pan);
        assert(editRequested);

        // Left-click on Master LCD readout (mX + mW - 30.0f = 12 + 140 - 30 = 122, mY + 20 = 76)
        PointerEvent peMasterLcdLeft = makePointer(
            122.0f, 76.0f,
            PointerAction::Down, PointerType::Mouse, 0.0, PointerButton::Left);
        editRequested = false;
        bool handledMasterLcd = mixer.handlePointer(peMasterLcdLeft, ctx);
        assert(handledMasterLcd);
        assert(editRequested);
        assert(receivedReq.paramName == "Master Volume");
    }

    // 3. ArrangerView Track Header Right-Click
    ArrangerView arranger;
    arranger.layout(Rect2D(0.0f, 56.0f, 1280.0f, 696.0f), ctx);
    PointerEvent peArrangerRight = makePointer(
        50.0f, 85.0f,
        PointerAction::Down, PointerType::Mouse, 0.0, PointerButton::Right);
    editRequested = false;
    bool handledArranger = arranger.handlePointer(peArrangerRight, ctx);
    assert(handledArranger);
    assert(editRequested);
    assert(receivedReq.allowPercentage);

    // 4. TransportHeader BPM Edit
    TransportHeader transport;
    transport.layout(1280.0f);
    transport.onOpenValueEdit = ctx.onOpenValueEdit;
    Rect2D bpmBounds = transport.getBpmBounds();
    PointerEvent peBpm = makePointer(
        bpmBounds.x + 5.0f,
        bpmBounds.y + 5.0f,
        PointerAction::Down, PointerType::Mouse, 0.0, PointerButton::Left);
    editRequested = false;
    bool handledBpm = transport.handlePointer(peBpm, nullptr);
    assert(handledBpm);
    assert(editRequested);
    assert(receivedReq.title == "PROJECT TEMPO");
    assert(receivedReq.unit == "BPM");

    // 5. BatchRenderer2D Backdrop Blur
    BatchRenderer2D renderer;
    renderer.applyBackdropBlur(4.0f, 0.48f);
    renderer.flush();

    // 6. TrackPropertiesPanel Header Card Redesign: 'M', 'S', Edit, single-line title
    {
        TrackPropertiesPanel panel;
        ViewContext pCtx;
        pCtx.logicalWidth = 1280.0f;
        pCtx.logicalHeight = 800.0f;
        panel.layout(Rect2D(0.0f, 0.0f, 320.0f, 600.0f), pCtx);
        Rect2D headerBounds = panel.getHeaderCardBounds();
        assert(headerBounds.h == 38.0f); // Reclaimed vertical space

        TrackPropertiesDrawerData drawerData;
        drawerData.trackName = "TR-808 Kit";
        drawerData.iconRef = "preset:inst_drums";
        drawerData.mute = false;
        drawerData.solo = false;

        float btnY = headerBounds.y + (38.0f - 24.0f) * 0.5f;
        // Track icon button: left side
        auto hitIcon = panel.hitTest(headerBounds.x + 15.0f, btnY + 12.0f, drawerData);
        assert(hitIcon.hit);
        assert(hitIcon.area == TrackPropertiesHitArea::TrackIcon);

        // Edit button: right-most button
        auto hitEdit = panel.hitTest(headerBounds.x + headerBounds.w - 15.0f, btnY + 12.0f, drawerData);
        assert(hitEdit.hit);
        assert(hitEdit.area == TrackPropertiesHitArea::RenameButton);

        // Header background click / right-click
        auto hitHeader = panel.hitTest(headerBounds.x + 80.0f, btnY + 12.0f, drawerData);
        assert(hitHeader.hit);
        assert(hitHeader.area == TrackPropertiesHitArea::RenameButton);
    }

    // 7. ArrangerView Track Header Right-Click & Icon Click Unified Dialog
    {
        ArrangerView arranger;
        ViewContext aCtx;
        aCtx.logicalWidth = 1280.0f;
        aCtx.logicalHeight = 800.0f;
        ValueEditRequest aReq;
        bool aEditOpened = false;
        aCtx.onOpenValueEdit = [&](const ValueEditRequest& r) {
            aEditOpened = true;
            aReq = r;
        };

        arranger.layout(Rect2D(0.0f, 56.0f, 1280.0f, 696.0f), aCtx);
        assert(!arranger.getTracks().empty());

        // Right-click track 0 header (tracksList starts at y=56 + 28 = 84, row 0 height 64)
        PointerEvent peHeaderRight = makePointer(80.0f, 100.0f, PointerAction::Down, PointerType::Mouse, 0.0, PointerButton::Right);
        aEditOpened = false;
        arranger.handlePointer(peHeaderRight, aCtx);
        assert(aEditOpened);
        assert(aReq.isTextMode);
        assert(aReq.paramName == "Track Name");
        assert(aReq.actionLinkLabel.empty());

        // Click track 0 icon glyph (iconBox at x = tracksList.x + 10 = 10, y = 84 + 7.5 = 91.5)
        // Icon click functionality removed from arranger track listing
        PointerEvent peIconClick = makePointer(12.0f, 92.0f, PointerAction::Down);
        aEditOpened = false;
        arranger.handlePointer(peIconClick, aCtx);
        assert(!aEditOpened);
    }

    std::cout << "  [PASS] ValueEditDialog, Right-Click Editing & Backdrop Blur validated." << std::endl;
}

void testCommandPaletteDialog() {
    std::cout << "[Test 14/14] Universal Quick Command Palette (Spotlight Runner)..." << std::endl;

    CommandPaletteDialog palette;
    assert(!palette.isOpen());
    assert(palette.getFilteredCount() == 0);

    // 1. Open / Toggle / Close State
    palette.open();
    assert(palette.isOpen());
    palette.toggle();
    assert(!palette.isOpen());
    palette.toggle();
    assert(palette.isOpen());

    // 2. Command Registration
    bool action1Triggered = false;
    bool action2Triggered = false;
    bool presetTriggered = false;
    bool viewTriggered = false;

    palette.registerCommand({
        "action.play", "Play / Pause", "Toggle transport playback",
        CommandCategory::Action, "Space", [&]() { action1Triggered = true; }
    });
    palette.registerCommand({
        "action.panic", "Panic / All Stop", "Kill all audio voices immediately",
        CommandCategory::Action, "Esc", [&]() { action2Triggered = true; }
    });
    palette.registerCommand({
        "view.arranger", "Arranger View", "Full timeline and arrangement clips",
        CommandCategory::View, "1", [&]() { viewTriggered = true; }
    });
    palette.registerCommand({
        "preset.tb303", "TB-303 Acid Bass", "Resonant acid squelch synth preset",
        CommandCategory::Preset, "", [&]() { presetTriggered = true; }
    });

    assert(palette.getFilteredCount() == 4);

    // 3. Category Filtering
    palette.setSelectedCategory(CommandCategory::Action);
    assert(palette.getSelectedCategory() == CommandCategory::Action);
    assert(palette.getFilteredCount() == 2);

    palette.setSelectedCategory(CommandCategory::View);
    assert(palette.getFilteredCount() == 1);

    palette.setSelectedCategory(CommandCategory::All);
    assert(palette.getFilteredCount() == 4);

    // 4. Search Query Filtering
    palette.setQuery("acid");
    assert(palette.getFilteredCount() == 1);
    assert(palette.getSelectedIndex() == 0);

    palette.setQuery("ALL"); // Should match "Panic / All Stop"
    assert(palette.getFilteredCount() == 1);

    palette.setQuery("nonexistent_command_xyz");
    assert(palette.getFilteredCount() == 0);

    palette.setQuery("");
    assert(palette.getFilteredCount() == 4);

    // 5. Keyboard Navigation & Execution
    // Down Arrow (264)
    palette.handleKey(264, 0, 1, 0); // Down
    assert(palette.getSelectedIndex() == 1);
    palette.handleKey(264, 0, 1, 0); // Down
    assert(palette.getSelectedIndex() == 2);
    palette.handleKey(265, 0, 1, 0); // Up
    assert(palette.getSelectedIndex() == 1);

    // Enter Key (257) executes selected command (action2: Panic)
    palette.handleKey(257, 0, 1, 0);
    assert(action2Triggered);
    assert(!palette.isOpen()); // Should close on execute

    // 6. Typographic Input (handleChar & Backspace)
    palette.open();
    palette.handleChar('p');
    palette.handleChar('l');
    palette.handleChar('a');
    palette.handleChar('y');
    assert(palette.getQuery() == "play");
    assert(palette.getFilteredCount() == 1);

    palette.handleKey(259, 0, 1, 0); // Backspace
    assert(palette.getQuery() == "pla");

    // Escape (256) closes
    palette.handleKey(256, 0, 1, 0);
    assert(!palette.isOpen());

    // 7. Layout & Batch Rendering validation
    palette.open();
    palette.layout(1280.0f, 800.0f);
    palette.update(0.016f);

    BatchRenderer2D renderer;
    palette.render(renderer, Theme::current());
    renderer.flush();

    // 8. Integration with GuiWindow
    audio::AudioEngine engine;
    engine.initialize();
    GuiWindow window(1280, 800, "Test Window");
    bool ok = window.initialize(engine);
    assert(ok);

    assert(!window.isCommandPaletteOpen());
    window.toggleCommandPalette();
    assert(window.isCommandPaletteOpen());
    assert(window.getCommandPaletteDialog().getFilteredCount() > 10); // Standard commands loaded

    // Test Ctrl+P shortcut through window.onKeyDown
    window.onKeyDown(80, 2); // 'P' with GLFW_MOD_CONTROL (2) -> toggles closed
    assert(!window.isCommandPaletteOpen());
    window.onKeyDown(75, 2); // 'K' with GLFW_MOD_CONTROL (2) -> toggles open
    assert(window.isCommandPaletteOpen());

    std::cout << "  [PASS] CommandPaletteDialog & Spotlight Runner validated." << std::endl;
}

void testFileDragAndDrop() {
    std::cout << "[Test 15/15] Cross-Platform File Drag and Drop Subsystem..." << std::endl;

    ArrangerView arranger;
    ViewContext ctx;
    ctx.screenWidth = 1280.0f;
    ctx.screenHeight = 800.0f;
    std::string notifiedMsg;
    ctx.onShowNotification = [&](const std::string& msg) { notifiedMsg = msg; };

    arranger.layout(Rect2D{0.0f, 60.0f, 1280.0f, 740.0f}, ctx);
    size_t initialTracks = arranger.getTracks().size();
    assert(initialTracks >= 5);

    // 1. Drop WAV file onto Track 0 at Bar 3 (grid starts at x: 190, y: 60 + 28 + 16 = 104)
    size_t trk0InitialClips = arranger.getTracks()[0].clips.size();
    bool handledAudio = arranger.handleFileDrop({"C:/audio/samples/drum_break.wav"}, 190.0f + 2.0f * 64.0f + 10.0f, 120.0f, ctx);
    assert(handledAudio);
    assert(arranger.getTracks()[0].clips.size() == trk0InitialClips + 1);
    const auto& newClip = arranger.getTracks()[0].clips.back();
    assert(newClip.name == "drum_break");
    assert(newClip.startBar == 3);
    assert(newClip.isAudio == true);
    assert(notifiedMsg.find("drum_break.wav") != std::string::npos);

    // 2. Drop MIDI file onto Track 1 at Bar 5
    size_t trk1InitialClips = arranger.getTracks()[1].clips.size();
    bool handledMidi = arranger.handleFileDrop({"C:/midi/melodies/lead_riff.mid"}, 190.0f + 4.0f * 64.0f + 10.0f, 120.0f + 64.0f, ctx);
    assert(handledMidi);
    assert(arranger.getTracks()[1].clips.size() == trk1InitialClips + 1);
    const auto& midiClip = arranger.getTracks()[1].clips.back();
    assert(midiClip.name == "lead_riff");
    assert(midiClip.startBar == 5);
    assert(midiClip.isAudio == false);

    // 3. Drop WAV file into empty arranger space below tracks -> Creates New Sampler Track
    float belowTracksY = 120.0f + static_cast<float>(arranger.getTracks().size()) * 64.0f + 30.0f;
    bool handledNewTrack = arranger.handleFileDrop({"C:/audio/stems/synth_pad.flac"}, 190.0f + 1.0f * 64.0f + 10.0f, belowTracksY, ctx);
    assert(handledNewTrack);
    assert(arranger.getTracks().size() == initialTracks + 1);
    const auto& createdTrack = arranger.getTracks().back();
    assert(createdTrack.name == "synth_pad");
    assert(createdTrack.instrumentEngine == "sampler");
    assert(!createdTrack.clips.empty());
    assert(createdTrack.clips[0].startBar == 2);
    assert(createdTrack.clips[0].isAudio == true);

    // 4. GuiWindow Integration with Drop Callback
    audio::AudioEngine engine;
    engine.initialize();
    GuiWindow window(1280, 800, "Drop Test Window");
    bool ok = window.initialize(engine);
    assert(ok);

    bool externalHookFired = false;
    window.onExternalFilesDropped = [&](const std::vector<std::string>& files, float /*x*/, float /*y*/) {
        externalHookFired = true;
        assert(!files.empty());
        assert(files[0] == "test_drop.wav");
    };

    window.onFilesDropped({"test_drop.wav"}, 400.0f, 300.0f);
    assert(externalHookFired);

    // 5. Audio-to-MIDI Dialog routing when open
    window.getAudioToMidiDialog().open();
    assert(window.getAudioToMidiDialog().isOpen());
    window.onFilesDropped({"vocal_recording.wav"}, 500.0f, 400.0f);
    assert(window.getLastStatusMessage().find("Audio-to-MIDI") != std::string::npos ||
           window.getStatusToastText().find("Audio-to-MIDI") != std::string::npos);
    window.getAudioToMidiDialog().close();

    // 6. Test dropping .eats song file into GuiWindow: loads graph, tracks, clips, and produces sound!
    audio::AudioGraph testGraph;
    testGraph.prepare(48000.0, 128);
    auto tbNode = std::make_shared<audio::Tb303Node>("Tb303");
    auto d9Node = std::make_shared<audio::DrumKitNode>("Drums909");
    auto mGain = std::make_shared<audio::GainNode>("MasterOut");
    mGain->setVolume(0.85f);
    audio::NodeId tbId = testGraph.addNode(tbNode);
    audio::NodeId d9Id = testGraph.addNode(d9Node);
    audio::NodeId mgId = testGraph.addNode(mGain);
    testGraph.connect(tbId, 0, mgId, 0);
    testGraph.connect(d9Id, 0, mgId, 0);
    testGraph.setOutputNode(mgId, 0);
    testGraph.compile();

    sequencer::StepSequencer testSeq;
    testSeq.setBpm(142.0);
    testSeq.setSwing(0.58);
    size_t trk0 = testSeq.addTrack("Acid Synth", tbId, 16);
    size_t trk1 = testSeq.addTrack("Techno 909", d9Id, 16);
    testSeq.getTrack(trk0)->setStep(0, {true, 36, 0.9f, 0.8f, false, true});
    testSeq.getTrack(trk1)->setStep(0, {true, 36, 1.0f, 0.5f, false, false});

    const std::string dropEatsPath = "dropped_project.eats";
    bool saved = project::ProjectFile::saveToFile(dropEatsPath, testGraph, testSeq, "Cyber Acid Rave", 142.0, 0.58);
    assert(saved);

    // Drop into window
    window.onFilesDropped({dropEatsPath}, 200.0f, 200.0f);
    assert(window.getLastStatusMessage().find("LOADED PROJECT") != std::string::npos ||
           window.getLastStatusMessage().find("Loaded project") != std::string::npos);

    // Verify modular arranger tracks updated from loaded song
    assert(window.getModularArrangerView() != nullptr);
    const auto& arrTracks = window.getModularArrangerView()->getTracks();
    assert(arrTracks.size() == 2);
    assert(arrTracks[0].name == "Acid Synth");
    assert(arrTracks[1].name == "Techno 909");
    assert(!arrTracks[0].clips.empty());
    assert(arrTracks[0].clips[0].notes.size() >= 1);
    assert(arrTracks[0].clips[0].notes[0].pitch == 36);

    // Verify audio engine state
    assert(engine.getSequencer().getBpm() == 142.0);
    assert(engine.getSequencer().getSwing() == 0.58);

    // Test audio playback produces sound!
    engine.getSequencer().start();
    float testPeak = 0.0f;
    alignas(64) float pL[128]{};
    alignas(64) float pR[128]{};
    for (int b = 0; b < 10; ++b) {
        engine.getSequencer().processBlock(128, engine.getGraph());
        engine.getGraph().process(pL, pR, 128);
        for (int i = 0; i < 128; ++i) {
            testPeak = std::max(testPeak, std::abs(pL[i]));
            testPeak = std::max(testPeak, std::abs(pR[i]));
        }
    }
    assert(testPeak > 0.01f); // Sound is alive and kicking!

    std::filesystem::remove(dropEatsPath);

    std::cout << "  [PASS] Cross-Platform File Drag and Drop Subsystem validated." << std::endl;
}

void testCircularGradients() {
    std::cout << "[Test 16/16] Circular & Radial Gradient Tessellation..." << std::endl;

    BatchRenderer2D r;
    assert(r.getVertexCount() == 0);

    // 1. Two-stop radial gradient with off-center specular focal point
    r.drawCircleRadialGradient(100.0f, 100.0f, 20.0f,
                               1.0f, 1.0f, 1.0f, 1.0f,
                               0.1f, 0.1f, 0.1f, 1.0f,
                               -5.0f, -5.0f, 36);
    // 6 vertices (analytical base disc) + 36 fan segments * 3 = 114 vertices
    assert(r.getVertexCount() == 114);

    // 2. Three-stop radial gradient with concentric ring tessellation
    r.drawCircleRadial3StopGradient(100.0f, 100.0f, 20.0f,
                                   1.0f, 1.0f, 1.0f, 1.0f,  // inner specular
                                   0.5f, 0.5f, 0.5f, 1.0f,  // mid body
                                   0.1f, 0.1f, 0.1f, 1.0f,  // outer shadow
                                   -4.0f, -4.0f, 0.50f, 36);
    // 6 (base disc) + 108 (inner fan) + 216 (outer quad strip: 36 * 6) = 330 vertices
    assert(r.getVertexCount() == 114 + 330);

    // 3. Linear gradient across circle
    r.drawCircleLinearGradient(100.0f, 100.0f, 20.0f,
                               1.0f, 0.0f, 0.0f, 1.0f,
                               0.0f, 0.0f, 1.0f, 1.0f,
                               1.5707963f, 36);
    // 6 (base disc) + 108 (fan) = 114 vertices
    assert(r.getVertexCount() == 114 + 330 + 114);

    // 4. draw_utils convenience wrappers
    drawRectGradient(r, 0.0f, 0.0f, 50.0f, 50.0f, Color(1, 0, 0, 1), Color(0, 1, 0, 1));
    drawRoundedRectGradient(r, 0.0f, 0.0f, 50.0f, 50.0f, 8.0f, Color(1, 0, 0, 1), Color(0, 1, 0, 1));
    drawCircleRadialGradient(r, 50.0f, 50.0f, 15.0f, Color(1, 1, 1, 1), Color(0, 0, 0, 1), -2.0f, -2.0f);
    drawCircleRadial3StopGradient(r, 50.0f, 50.0f, 15.0f,
                                  Color(1, 1, 1, 1), Color(0.5f, 0.5f, 0.5f, 1), Color(0, 0, 0, 1),
                                  -2.0f, -2.0f, 0.5f);
    drawCircleLinearGradient(r, 50.0f, 50.0f, 15.0f, Color(1, 1, 1, 1), Color(0, 0, 0, 1));

    std::cout << "  [PASS] Circular & Radial Gradient Tessellation validated." << std::endl;
}

void testTextEditorWidgetAndMinimap() {
    std::cout << "[Test 17/17] TextEditorWidget, Code Minimap & Script Workstations..." << std::endl;

    const ThemeTokens& theme = Theme::current();
    BatchRenderer2D renderer;
    FocusManager focusManager;
    ViewContext ctx;
    ctx.renderer = &renderer;
    ctx.theme = &theme;
    ctx.focusManager = &focusManager;

    // 1. TextEditorWidget Layout and Bounds
    TextEditorWidget editor;
    Rect2D edBounds(0.0f, 0.0f, 800.0f, 600.0f);
    editor.layout(edBounds, ctx);

    assert(editor.getBounds().w == 800.0f);
    assert(editor.getBounds().h == 600.0f);
    assert(editor.getGutterBounds().w == 46.0f);
    assert(editor.getMinimapBounds().w == 56.0f);
    assertNear(editor.getTextAreaBounds().w, 800.0f - 46.0f - 56.0f);
    assertNear(editor.getCharWidth(), getMonoCharAdvance(10.0f), 0.01f);
    assertNear(editor.getCharWidth(), 7.01f, 0.02f);

    // 2. Multiline Document Editing, Input Focus & Hit-Testing Alignment
    std::string sampleCode =
        "# --- Eatscript Bass Synth ---\n"
        "import math\n\n"
        "def init():\n"
        "    eat.param(\"Cutoff\", 850.0)\n\n"
        "def process():\n"
        "    return 1.0\n";
    editor.setText(sampleCode);
    editor.layout(edBounds, ctx);

    assert(editor.getPresenter().getDocument().getLineCount() >= 7);
    assert(!editor.isFocused());

    // Verify click-to-column hit testing (coordFromPoint) matches exact font metrics without drift
    float textStartX = editor.getTextAreaBounds().x + 6.0f;
    float textLine0Y = editor.getTextAreaBounds().y + 5.0f;
    auto ptCol10 = editor.coordFromPoint(textStartX + 10.0f * editor.getCharWidth(), textLine0Y);
    assert(ptCol10.line == 0);
    assert(ptCol10.column == 10);

    auto ptCol25 = editor.coordFromPoint(textStartX + 25.0f * editor.getCharWidth(), textLine0Y);
    assert(ptCol25.line == 0);
    assert(ptCol25.column == 25);

    // Verify end-of-line hit testing has zero phantom space drift
    int line0Len = static_cast<int>(editor.getPresenter().getDocument().getLineLength(0));
    auto ptEndOfLine = editor.coordFromPoint(textStartX + static_cast<float>(line0Len) * editor.getCharWidth(), textLine0Y);
    assert(ptEndOfLine.line == 0);
    assert(ptEndOfLine.column == line0Len);

    // Focus acquisition via pointer click
    PointerEvent clickEd = makePointer(100.0f, 100.0f, PointerAction::Down);
    editor.handlePointer(clickEd, ctx);
    assert(editor.isFocused());
    assert(focusManager.hasFocus(&editor));

    // Type character
    editor.handleChar(U'#', ctx);
    assert(editor.getText().find('#') != std::string::npos);

    // Enter key creates indented newline
    editor.handleKey(257, 0, 1, 0, ctx); // Enter
    assert(editor.getPresenter().getDocument().getLineCount() >= 8);

    // Tab key indents
    size_t lenBeforeTab = editor.getText().length();
    editor.handleKey(258, 0, 1, 0, ctx); // Tab
    assert(editor.getText().length() == lenBeforeTab + 4);

    // Undo via Ctrl+Z
    editor.handleKey(90, 0, 1, 2, ctx); // Ctrl+Z
    assert(editor.getText().length() == lenBeforeTab);

    // 3. Code Minimap Scrubber Navigation
    // Create a 150-line script to test scrolling and minimap lens
    std::string longScript = "# Start of long script\n";
    for (int i = 0; i < 150; ++i) {
        longScript += "    eat.param(\"Param_" + std::to_string(i) + "\", " + std::to_string(i * 10) + ")\n";
    }
    longScript += "# End of script\n";
    editor.setText(longScript);
    editor.layout(edBounds, ctx);

    assert(editor.getPresenter().getMaxScrollY() > 0.0f);
    assertNear(editor.getPresenter().getScrollY(), 0.0f);

    // Click on Code Minimap at 60% of height (touch scrubber navigation)
    const auto& mmb = editor.getMinimapBounds();
    PointerEvent mapDown = makePointer(mmb.x + 20.0f, mmb.y + mmb.h * 0.60f, PointerAction::Down);
    bool mapHandled = editor.handlePointer(mapDown, ctx);
    assert(mapHandled);
    float scrubbedScrollY = editor.getPresenter().getScrollY();
    assert(scrubbedScrollY > 0.0f);

    // Drag minimap further down to 90%
    PointerEvent mapMove = makePointer(mmb.x + 20.0f, mmb.y + mmb.h * 0.90f, PointerAction::Move);
    editor.handlePointer(mapMove, ctx);
    assert(editor.getPresenter().getScrollY() > scrubbedScrollY);

    // Release minimap scrubber
    PointerEvent mapUp = makePointer(mmb.x + 20.0f, mmb.y + mmb.h * 0.90f, PointerAction::Up);
    editor.handlePointer(mapUp, ctx);

    // 4. Mouse Wheel Scrolling on Native & Web
    editor.getPresenter().setScrollY(0.0f);
    PointerEvent scrollDown = makePointer(100.0f, 100.0f, PointerAction::Scroll);
    scrollDown.scrollY = -2.0f; // Wheel down
    bool scrollHandled = editor.handlePointer(scrollDown, ctx);
    assert(scrollHandled);
    assert(editor.getPresenter().getScrollY() > 0.0f);

    float scrolledY = editor.getPresenter().getScrollY();
    PointerEvent scrollUp = makePointer(100.0f, 100.0f, PointerAction::Scroll);
    scrollUp.scrollY = 1.0f; // Wheel up
    editor.handlePointer(scrollUp, ctx);
    assert(editor.getPresenter().getScrollY() < scrolledY);

    // Horizontal scroll with Shift modifier
    float initScrollX = editor.getPresenter().getScrollX();
    PointerEvent shiftScroll = makePointer(100.0f, 100.0f, PointerAction::Scroll);
    shiftScroll.scrollY = -2.0f;
    shiftScroll.scrollX = 0.0f;
    shiftScroll.mods.shift = true;
    editor.handlePointer(shiftScroll, ctx);
    assert(editor.getPresenter().getScrollX() > initScrollX);

    // 5. Verification in DesignView (DESIGN > Code)
    DesignView designView;
    designView.setSubMode(DesignSubMode::Code);
    designView.layout(Rect2D(0.0f, 56.0f, 1280.0f, 700.0f), ctx);

    assert(!designView.getTextEditor().getText().empty());
    assert(designView.getTextEditor().getPresenter().getDocument().getLineCount() > 5);

    // Mouse wheel scroll through DesignView in Code mode
    const auto& desEdBounds = designView.getTextEditor().getBounds();
    PointerEvent desScroll = makePointer(desEdBounds.x + 50.0f, desEdBounds.y + 50.0f, PointerAction::Scroll);
    desScroll.scrollY = -2.0f;
    bool desScrollHandled = designView.handlePointer(desScroll, ctx);
    assert(desScrollHandled);

    // Switch active target to TB-303
    designView.selectTargetById("eats_303");
    assert(designView.getTextEditor().getText().find("TB-303") != std::string::npos);

    // 6. Verification in EditView (EDIT > Script)
    EditView editView;
    editView.setSubView(EditSubViewMode::Script);
    editView.layout(Rect2D(0.0f, 56.0f, 1280.0f, 700.0f), ctx);

    assert(!editView.getScriptEditor().getText().empty());
    assert(editView.getScriptEditor().getText().find("Clip:") != std::string::npos);

    // Mouse wheel scroll through EditView in Script mode
    const auto& editEdBounds = editView.getScriptEditor().getBounds();
    PointerEvent editScroll = makePointer(editEdBounds.x + 50.0f, editEdBounds.y + 50.0f, PointerAction::Scroll);
    editScroll.scrollY = -2.0f;
    bool editScrollHandled = editView.handlePointer(editScroll, ctx);
    assert(editScrollHandled);

    // 7. Test BatchRenderer2D rendering pass (validating gutter and line number scissoring)
    renderer.beginFrame(800.0f, 600.0f);
    editor.render(ctx);
    renderer.endFrame();

    // 8. Mobile Code Accessory Toolbar
    TextEditorWidget mobileEditor;
    ViewContext mobileCtx = ctx;
    mobileCtx.isMobile = true;
    mobileEditor.setText("x = 10\n");
    mobileEditor.layout(Rect2D(0.0f, 0.0f, 400.0f, 500.0f), mobileCtx);

    // Verify toolbar layout when mobile
    assert(mobileEditor.getAccessoryToolbarBounds().h == 36.0f);
    assert(mobileEditor.getAccessoryToolbarBounds().y == 500.0f - 36.0f);
    assert(mobileEditor.getGutterBounds().h == 500.0f - 36.0f);
    assert(mobileEditor.getTextAreaBounds().h == 500.0f - 36.0f);

    // Test tap on Tab/Indent button (first button at x ~ tb.x + 15, y ~ tb.y + 15)
    const auto& tb = mobileEditor.getAccessoryToolbarBounds();
    PointerEvent tapIndent = makePointer(tb.x + 15.0f, tb.y + 15.0f, PointerAction::Down);
    bool indentHandled = mobileEditor.handlePointer(tapIndent, mobileCtx);
    assert(indentHandled);
    assert(mobileEditor.getText().find("    x = 10") != std::string::npos);

    // Test tap on Parens button (third button at x ~ tb.x + 95)
    mobileEditor.setText("");
    PointerEvent tapParens = makePointer(tb.x + 95.0f, tb.y + 15.0f, PointerAction::Down);
    mobileEditor.handlePointer(tapParens, mobileCtx);
    assert(mobileEditor.getText() == "()");
    // Cursor placed inside parens at column 1
    assert(mobileEditor.getPresenter().getCursor().column == 1);

    // Test selection auto-wrapping with Parens
    mobileEditor.setText("signal");
    mobileEditor.getPresenter().setSelection({0, 0}, {0, 6});
    mobileEditor.handlePointer(tapParens, mobileCtx);
    assert(mobileEditor.getText() == "(signal)");

    // Test tap on Plus button (tenth button at x ~ tb.x + 343)
    mobileEditor.setText("out = a ");
    mobileEditor.getPresenter().setCursor({0, 8}, false);
    PointerEvent tapPlus = makePointer(tb.x + 343.0f, tb.y + 15.0f, PointerAction::Down);
    mobileEditor.handlePointer(tapPlus, mobileCtx);
    assert(mobileEditor.getText() == "out = a +");

    // Test compile callback triggered from toolbar
    bool compileTriggered = false;
    mobileEditor.onCompileTriggered = [&]() { compileTriggered = true; };
    // Scroll toolbar to reveal RunCompile at the end
    PointerEvent scrollEv;
    scrollEv.action = PointerAction::Scroll;
    scrollEv.x = tb.x + 50.0f;
    scrollEv.y = tb.y + 15.0f;
    scrollEv.scrollX = -20.0f; // Scroll right
    mobileEditor.handlePointer(scrollEv, mobileCtx);

    // Test focus gain & lost
    assert(!mobileEditor.isFocused());
    mobileEditor.onFocusGained();
    assert(mobileEditor.isFocused());
    mobileEditor.onFocusLost();
    assert(!mobileEditor.isFocused());

    // Test rendering pass of mobile editor with accessory toolbar
    renderer.beginFrame(400.0f, 500.0f);
    mobileEditor.render(mobileCtx);
    renderer.endFrame();

    std::cout << "  [PASS] TextEditorWidget, Code Minimap & Script Workstations validated." << std::endl;
}

void testArrangerMixerDrawer() {
    std::cout << "[Test 18] ArrangerMixerDrawer Docked Bottom Console & Composition Parity..." << std::endl;

    ArrangerMixerDrawer drawer;
    assert(!drawer.isExpanded());
    assert(drawer.getAnimProgress() == 0.0f);

    Rect2D bounds(0.0f, 56.0f, 1280.0f, 720.0f - 56.0f - 48.0f);
    drawer.layout(bounds, 0.0f);

    assert(drawer.getPullTabBounds().w == ArrangerMixerDrawer::kPullTabWidth);
    assert(drawer.getPullTabBounds().h == ArrangerMixerDrawer::kPullTabHeight);

    // Toggle expansion
    drawer.toggle();
    assert(drawer.isExpanded());

    // Step animation to completion
    for (int i = 0; i < 30; ++i) {
        drawer.update(0.016f);
    }
    assertNear(drawer.getAnimProgress(), 1.0f, 0.01f);
    drawer.layout(bounds, 0.0f);

    assert(drawer.getDrawerBounds().h > 100.0f);
    assert(drawer.getCloseButtonBounds().w > 0.0f);

    // Setup mock tracks
    std::vector<ArrangerTimelineTrack> tracks;
    ArrangerTimelineTrack t1;
    t1.name = "Kick"; t1.volume = 0.8f; t1.pan = 0.0f; t1.mute = false; t1.solo = false;
    ArrangerTimelineTrack t2;
    t2.name = "Bass"; t2.volume = 0.7f; t2.pan = -0.3f; t2.mute = false; t2.solo = false;
    tracks.push_back(t1);
    tracks.push_back(t2);

    uint32_t activeTrack = 0;
    float masterVol = 1.0f;
    float masterPan = 0.0f;
    bool masterMute = false;

    BatchRenderer2D renderer;
    ThemeTokens theme = Theme::current();
    ViewContext ctx;
    ctx.renderer = &renderer;
    ctx.theme = &theme;

    // Render pass without crashes
    renderer.beginFrame(1280.0f, 720.0f);
    float chPeaksL[2] = {0.5f, 0.3f};
    float chPeaksR[2] = {0.5f, 0.3f};
    drawer.render(renderer, theme, tracks, activeTrack, masterVol, masterPan, masterMute,
                  0.6f, 0.6f, chPeaksL, chPeaksR, 2);
    renderer.endFrame();

    // Hotkey 'M' toggles drawer
    bool handledM = drawer.handleKey('M', 0, 1, 0, ctx);
    assert(handledM);
    assert(!drawer.isExpanded()); // Toggled closed

    drawer.handleKey('M', 0, 1, 0, ctx);
    assert(drawer.isExpanded()); // Toggled open

    // Hotkey Escape closes drawer
    bool handledEsc = drawer.handleKey(256, 0, 1, 0, ctx);
    assert(handledEsc);
    assert(!drawer.isExpanded()); // Closed via Escape

    drawer.setExpanded(true);
    for (int i = 0; i < 30; ++i) drawer.update(0.016f);
    drawer.layout(bounds, 0.0f);

    // Pull-tab click toggles closed
    PointerEvent tabClick = makePointer(drawer.getPullTabBounds().x + 10.0f, drawer.getPullTabBounds().y + 5.0f, PointerAction::Down);
    bool hitTab = drawer.handlePointer(tabClick, tracks, activeTrack, masterVol, masterPan, masterMute, ctx);
    assert(hitTab);
    assert(!drawer.isExpanded());

    // Pull-tab click toggles open
    hitTab = drawer.handlePointer(tabClick, tracks, activeTrack, masterVol, masterPan, masterMute, ctx);
    assert(hitTab);
    assert(drawer.isExpanded());

    // Test close button
    for (int i = 0; i < 30; ++i) drawer.update(0.016f);
    drawer.layout(bounds, 0.0f);
    PointerEvent closeClick = makePointer(drawer.getCloseButtonBounds().x + 5.0f, drawer.getCloseButtonBounds().y + 5.0f, PointerAction::Down);
    bool hitClose = drawer.handlePointer(closeClick, tracks, activeTrack, masterVol, masterPan, masterMute, ctx);
    assert(hitClose);
    assert(!drawer.isExpanded());

    // Re-open and test ArrangerView integration
    ArrangerView arranger;
    arranger.layout(bounds, ctx);
    assert(!arranger.isMixerDrawerOpen());

    arranger.toggleMixerDrawer();
    assert(arranger.isMixerDrawerOpen());

    arranger.handleKey('M', 0, 1, 0, ctx);
    assert(!arranger.isMixerDrawerOpen());

    std::cout << "  [PASS] ArrangerMixerDrawer Docked Bottom Console & Hotkeys verified." << std::endl;
}

void testPluginSearchDialog() {
    std::cout << "Running Test 19: PluginSearchDialog Refined Text Filtering & Scissored Scrolling..." << std::endl;

    PluginSearchDialog dialog;
    assert(!dialog.isOpen());

    // 1. Open dialog in AddInstrument mode
    dialog.open(PluginDialogMode::AddInstrument, "Lead 303", 2);
    assert(dialog.isOpen());
    assert(dialog.getMode() == PluginDialogMode::AddInstrument);
    assert(dialog.getTargetTrackIndex() == 2);
    assert(dialog.getSearchQuery().empty());

    size_t initialCount = dialog.getFilteredCount();
    assert(initialCount > 0);

    // 2. Layout calculations
    dialog.layout(1280.0f, 800.0f);

    // 3. Typing via handleChar
    dialog.handleChar(U'3');
    dialog.handleChar(U'0');
    dialog.handleChar(U'3');
    assert(dialog.getSearchQuery() == "303");
    size_t count303 = dialog.getFilteredCount();
    assert(count303 > 0);
    assert(count303 <= initialCount);

    // 4. Tokenized multi-word search ("303 bass" or "acid 303")
    dialog.clearSearch();
    assert(dialog.getSearchQuery().empty());
    assert(dialog.getFilteredCount() == initialCount);

    dialog.setSearchQuery("synth lead");
    assert(dialog.getSearchQuery() == "synth lead");
    size_t multiWordCount = dialog.getFilteredCount();
    assert(multiWordCount <= initialCount);

    // 5. Headless typing fallback via handleKey
    dialog.clearSearch();
    dialog.handleKey('a', 0, 1, 0);
    dialog.handleKey('c', 0, 1, 0);
    dialog.handleKey('i', 0, 1, 0);
    dialog.handleKey('d', 0, 1, 0);
    assert(dialog.getSearchQuery() == "acid");

    // Backspace via handleKey
    dialog.handleKey(259, 0, 1, 0); // Backspace
    assert(dialog.getSearchQuery() == "aci");

    // 6. Escape behavior: first Esc clears search query, second Esc closes dialog
    dialog.handleKey(256, 0, 1, 0); // Escape
    assert(dialog.getSearchQuery().empty());
    assert(dialog.isOpen()); // Dialog stays open on query clear

    dialog.handleKey(256, 0, 1, 0); // Escape when query is empty
    assert(!dialog.isOpen()); // Dialog closes!

    // 7. Re-open and verify selection commit
    bool pluginSelected = false;
    std::string selectedName;
    uint32_t selectedTrack = 999;
    dialog.onPluginSelected = [&](PluginDialogMode, const PluginEntry& entry, uint32_t trk) {
        pluginSelected = true;
        selectedName = entry.name;
        selectedTrack = trk;
    };

    dialog.open(PluginDialogMode::AddInstrument, "Track 1", 1);
    dialog.setSearchQuery("303");
    dialog.layout(1280.0f, 800.0f);
    assert(dialog.getFilteredCount() > 0);

    // Arrow navigation
    dialog.handleKey(264, 0, 1, 0); // Down arrow
    dialog.handleKey(257, 0, 1, 0); // Enter (commit)
    assert(pluginSelected);
    assert(selectedTrack == 1);
    assert(!dialog.isOpen());

    // 8. Scrolling containment and bounds checks
    dialog.open(PluginDialogMode::SelectPreset, "All Presets", 0);
    dialog.layout(1280.0f, 800.0f);
    assert(dialog.getFilteredCount() >= 10);

    // Mouse scroll
    PointerEvent scrollEv;
    scrollEv.action = PointerAction::Scroll;
    scrollEv.scrollY = -4.0f;
    scrollEv.x = 640.0f;
    scrollEv.y = 350.0f;
    bool handledScroll = dialog.handlePointer(scrollEv);
    assert(handledScroll);

    // Clicks outside the visible card list (above search box) do NOT click cards
    PointerEvent clickHeader;
    clickHeader.action = PointerAction::Down;
    clickHeader.type = PointerType::Mouse;
    clickHeader.x = 640.0f;
    clickHeader.y = 150.0f; // in header region above list
    dialog.handlePointer(clickHeader);
    assert(dialog.isOpen()); // Did not commit a card

    // Clear button [X] interaction
    dialog.setSearchQuery("filtertest");
    assert(dialog.getFilteredCount() == 0); // Zero matches empty state
    dialog.clearSearch();
    assert(dialog.getFilteredCount() > 0);

    std::cout << "  [PASS] PluginSearchDialog Refined Text Filtering & Scissored Scrolling verified." << std::endl;
}

void testTooltips() {
    std::cout << "[Test 20/20] Header Transport & Track Properties Tooltip System..." << std::endl;

    // 1. GuiWindow Header Transport Tooltips
    GuiWindow window(1280, 800, "Tooltip Test Window");
    float wW = static_cast<float>(window.getWidth());
    assert(wW > 600.0f);

    // Test Logo tooltip
    std::string logoTip = window.getTransportTooltip(25.0f, 25.0f);
    assert(!logoTip.empty());
    assert(logoTip.find("Project Hub") != std::string::npos);

    // Test Play/Pause tooltip
    std::string playTip = window.getTransportTooltip(70.0f, 25.0f);
    assert(!playTip.empty());
    assert(playTip.find("Play") != std::string::npos || playTip.find("Pause") != std::string::npos);

    // Test Stop tooltip
    std::string stopTip = window.getTransportTooltip(110.0f, 25.0f);
    assert(!stopTip.empty());
    assert(stopTip.find("Stop") != std::string::npos);

    // Test Record tooltip
    std::string recTip = window.getTransportTooltip(150.0f, 25.0f);
    assert(!recTip.empty());
    assert(recTip.find("Record") != std::string::npos);

    // Test BPM tooltip
    std::string bpmTip = window.getTransportTooltip(220.0f, 25.0f);
    assert(!bpmTip.empty());
    assert(bpmTip.find("BPM") != std::string::npos || bpmTip.find("Tempo") != std::string::npos);

    // Test Timecode Position tooltip
    std::string timeTip = window.getTransportTooltip(wW - 180.0f, 25.0f);
    assert(!timeTip.empty());
    assert(timeTip.find("Song Position") != std::string::npos || timeTip.find("Position") != std::string::npos);

    // Test Lock tooltip
    std::string lockTip = window.getTransportTooltip(wW - 105.0f, 25.0f);
    assert(!lockTip.empty());
    assert(lockTip.find("Lock") != std::string::npos);

    // Test Fullscreen tooltip
    std::string fsTip = window.getTransportTooltip(wW - 75.0f, 25.0f);
    assert(!fsTip.empty());
    assert(fsTip.find("Fullscreen") != std::string::npos);

    // Test Browser tooltip
    std::string brwTip = window.getTransportTooltip(wW - 25.0f, 25.0f);
    assert(!brwTip.empty());
    assert(brwTip.find("Browser") != std::string::npos);

    // Outside header bounds should return empty
    std::string outsideTip = window.getTransportTooltip(100.0f, 200.0f);
    assert(outsideTip.empty());

    // 2. TransportHeader Widget Tooltips
    TransportHeader header;
    header.layout(1280.0f, 48.0f);
    assert(header.getTooltip(25.0f, 24.0f).find("Project Hub") != std::string::npos);
    assert(header.getTooltip(75.0f, 24.0f).find("Play") != std::string::npos);
    assert(header.getTooltip(115.0f, 24.0f).find("Stop") != std::string::npos);
    assert(header.getTooltip(180.0f, 24.0f).find("Position") != std::string::npos);
    assert(header.getTooltip(280.0f, 24.0f).find("BPM") != std::string::npos || header.getTooltip(280.0f, 24.0f).find("Tempo") != std::string::npos);
    assert(header.getTooltip(360.0f, 24.0f).find("Snap") != std::string::npos);
    assert(header.getTooltip(420.0f, 24.0f).find("Loop") != std::string::npos);
    assert(header.getTooltip(480.0f, 24.0f).find("Metronome") != std::string::npos);
    assert(header.getTooltip(1220.0f, 24.0f).find("Browser") != std::string::npos);

    // 3. TrackPropertiesPanel Element Tooltips
    TrackPropertiesPanel panel;
    ViewContext pCtx;
    pCtx.logicalWidth = 1280.0f;
    pCtx.logicalHeight = 800.0f;
    panel.layout(Rect2D(0.0f, 0.0f, 360.0f, 800.0f), pCtx);

    TrackPropertiesDrawerData data;
    data.trackName = "Retro Lead";
    data.instrument = "TB-303";
    data.instrumentExpanded = true;
    data.volume = 0.85f;
    data.pan = -0.30f;
    data.midiFx.emplace_back("Arpeggiator", "ARP", true);
    data.audioFx.emplace_back("8-Bit Crusher", "BITCRUSHER", 0.5f, 0.8f, true);
    data.syncKnobsIfEmpty();

    Rect2D headBounds = panel.getHeaderCardBounds();
    float headY = headBounds.y + headBounds.h * 0.5f;

    // Track icon button
    std::string iconTip = panel.getTooltip(headBounds.x + 18.0f, headY, data);
    assert(!iconTip.empty());
    assert(iconTip.find("Icon") != std::string::npos);

    // Edit button on right
    std::string editTip = panel.getTooltip(headBounds.x + headBounds.w - 18.0f, headY, data);
    assert(!editTip.empty());
    assert(editTip.find("Edit Track Name") != std::string::npos);

    // Color swatches card
    std::string colorTip = panel.getTooltip(headBounds.x + 150.0f, headBounds.y + headBounds.h + 20.0f, data);
    assert(!colorTip.empty());
    assert(colorTip.find("Color") != std::string::npos);

    // 4. TrackPropertiesDrawer Tooltips
    TrackPropertiesDrawer drawer;
    drawer.layout(Rect2D(0.0f, 0.0f, 1280.0f, 800.0f), 0.0f);

    // Collapsed pull tab
    Rect2D ptBounds = drawer.getPullTabBounds();
    std::string ptTip = drawer.getTooltip(ptBounds.x + ptBounds.w * 0.5f, ptBounds.y + ptBounds.h * 0.5f, data);
    assert(!ptTip.empty());
    assert(ptTip.find("Properties Drawer") != std::string::npos);

    // Expand drawer and check close button and tabs
    drawer.setExpanded(true);
    drawer.update(1.0f);
    drawer.layout(Rect2D(0.0f, 0.0f, 1280.0f, 800.0f), 0.0f);

    Rect2D clBounds = drawer.getCloseButtonBounds();
    std::string clTip = drawer.getTooltip(clBounds.x + clBounds.w * 0.5f, clBounds.y + clBounds.h * 0.5f, data);
    assert(!clTip.empty());
    assert(clTip.find("Close") != std::string::npos);

    // Top track vs clip tab
    Rect2D drBounds = drawer.getDrawerBounds();
    std::string tabTrkTip = drawer.getTooltip(drBounds.x + 40.0f, drBounds.y + 15.0f, data);
    assert(!tabTrkTip.empty());
    assert(tabTrkTip.find("Track") != std::string::npos);

    std::cout << "  [PASS] Header Transport & Track Properties Tooltip System verified." << std::endl;
}

void testTrackPropertiesActionsAndMuteSoloFix() {
    std::cout << "[Test 21] Track Properties Parity (+ Add Clip, Duplicate, Delete) & Mute/Solo Bug Fix..." << std::endl;

    audio::AudioEngine engine;
    GuiWindow window(1280, 800);
    bool initOk = window.initialize(engine);
    assert(initOk);

    size_t initialTracks = window.getArrangerTracks().size();
    assert(initialTracks >= 5);
    assert(window.getMixerStrips().size() == initialTracks);
    assert(engine.getSequencer().getNumTracks() == initialTracks);

    // 1. Add new track to project -> clean track with zero default clips
    window.addTrackToProject("Synth Lead 2", "synth", 0.9f, 0.4f, 0.2f);
    uint32_t newTrackIdx = static_cast<uint32_t>(initialTracks);

    assert(window.getArrangerTracks().size() == initialTracks + 1);
    assert(window.getMixerStrips().size() == initialTracks + 1);
    assert(engine.getSequencer().getNumTracks() == initialTracks + 1);
    assert(window.getArrangerTracks()[newTrackIdx].clips.empty());

    // 2. Mute / Solo bug fix: newly added track can now be muted and soloed cleanly!
    assert(!window.getArrangerTracks()[newTrackIdx].mute);
    assert(!engine.getSequencer().isTrackMuted(newTrackIdx));

    window.setTrackMuteState(newTrackIdx, true);
    assert(window.getArrangerTracks()[newTrackIdx].mute);
    assert(window.getMixerStrips()[newTrackIdx].mute);
    assert(engine.getSequencer().isTrackMuted(newTrackIdx));

    window.setTrackMuteState(newTrackIdx, false);
    assert(!window.getArrangerTracks()[newTrackIdx].mute);
    assert(!engine.getSequencer().isTrackMuted(newTrackIdx));

    window.setTrackSoloState(newTrackIdx, true);
    assert(window.getArrangerTracks()[newTrackIdx].solo);
    assert(window.getMixerStrips()[newTrackIdx].solo);
    assert(engine.getSequencer().isTrackSoloed(newTrackIdx));

    window.setTrackSoloState(newTrackIdx, false);
    assert(!window.getArrangerTracks()[newTrackIdx].solo);
    assert(!engine.getSequencer().isTrackSoloed(newTrackIdx));

    // 3. Duplicate track
    size_t beforeDup = window.getArrangerTracks().size();
    window.duplicateTrack(newTrackIdx);
    assert(window.getArrangerTracks().size() == beforeDup + 1);
    assert(window.getMixerStrips().size() == beforeDup + 1);
    assert(engine.getSequencer().getNumTracks() == beforeDup + 1);

    // 4. Add clip to track
    size_t dupIdx = window.getArrangerTracks().size() - 1;
    window.addClipToTrack(static_cast<uint32_t>(dupIdx));
    assert(!window.getArrangerTracks()[dupIdx].clips.empty());

    // 5. Delete track: ensure mixer config, audio engine, and sequencer get cleanly updated
    size_t beforeDel = window.getArrangerTracks().size();
    window.deleteTrack(static_cast<uint32_t>(dupIdx));
    assert(window.getArrangerTracks().size() == beforeDel - 1);
    assert(window.getMixerStrips().size() == beforeDel - 1);
    assert(engine.getSequencer().getNumTracks() == beforeDel - 1);

    // 6. Test Track Properties Action Card hit handling
    TrackPropertiesPanel panel;
    TrackPropertiesDrawerData data;
    data.trackIndex = 0;
    data.totalTracks = 5;

    bool addClipHandled = false;
    panel.onAddClip = [&](uint32_t) { addClipHandled = true; };
    ViewContext dummyCtx;
    TrackPropertiesHitResult hitAddClip{true, TrackPropertiesHitArea::AddClipButton, 0, 0.0f};
    panel.executeHitAction(hitAddClip, data, dummyCtx);
    assert(addClipHandled);

    bool dupHandled = false;
    panel.onDuplicateTrack = [&](uint32_t) { dupHandled = true; };
    TrackPropertiesHitResult hitDup{true, TrackPropertiesHitArea::DuplicateTrackButton, 0, 0.0f};
    panel.executeHitAction(hitDup, data, dummyCtx);
    assert(dupHandled);

    bool delHandled = false;
    panel.onDeleteTrack = [&](uint32_t) { delHandled = true; };
    TrackPropertiesHitResult hitDel{true, TrackPropertiesHitArea::DeleteTrackButton, 0, 0.0f};
    panel.executeHitAction(hitDel, data, dummyCtx);
    assert(delHandled);

    std::cout << "  [PASS] Parity actions and mute/solo fix validated." << std::endl;
}

void testClipActionsAndHistoryRestoration() {
    std::cout << "[Test 22/22] Clip Duplicate/Delete Parity, Clean Empty Rendering & History Undo/Redo..." << std::endl;

    // 1. Verify ArrangerView empty clip behavior: zero notes, no phantom notes
    ArrangerView arranger;
    ViewContext ctx;
    arranger.layout(Rect2D{0.0f, 0.0f, 1280.0f, 800.0f}, ctx);
    assert(!arranger.getTracks().empty());

    // Add a clean track
    arranger.addTrack("Test Clean Track", "synth", 0.5f, 0.8f, 0.2f);
    uint32_t cleanTrkIdx = static_cast<uint32_t>(arranger.getTracks().size() - 1);
    assert(arranger.getTracks()[cleanTrkIdx].clips.empty());

    // Add empty clip
    arranger.addClipToTrack(cleanTrkIdx);
    assert(arranger.getTracks()[cleanTrkIdx].clips.size() == 1);
    const auto& emptyClip = arranger.getTracks()[cleanTrkIdx].clips[0];
    assert(emptyClip.notes.empty()); // Must be clean with zero phantom notes
    assert(!emptyClip.isAudio);

    // 2. Test Clip Duplicate in ArrangerView
    arranger.duplicateClip(cleanTrkIdx, 0);
    assert(arranger.getTracks()[cleanTrkIdx].clips.size() == 2);
    assert(arranger.getTracks()[cleanTrkIdx].clips[1].name.find("Copy") != std::string::npos);
    assert(arranger.getTracks()[cleanTrkIdx].clips[1].startBar == emptyClip.startBar + emptyClip.lengthBars);
    assert(arranger.getSelectedClipIndex() == 1);

    // 3. Test Clip Delete in ArrangerView
    arranger.deleteClip(cleanTrkIdx, 1);
    assert(arranger.getTracks()[cleanTrkIdx].clips.size() == 1);
    arranger.deleteClip(cleanTrkIdx, 0);
    assert(arranger.getTracks()[cleanTrkIdx].clips.empty());
    assert(arranger.getSelectedClipIndex() == -1);

    // 4. Test Track Deletion resets clip index and re-indexes remaining clips
    size_t trkCount = arranger.getTracks().size();
    arranger.deleteTrack(cleanTrkIdx);
    assert(arranger.getTracks().size() == trkCount - 1);
    assert(arranger.getSelectedClipIndex() == -1);

    // 5. Test TrackPropertiesPanel Clip Duplicate & Delete Hit Handling
    TrackPropertiesPanel panel;
    TrackPropertiesDrawerData data;
    data.tab = TrackPropertiesTab::Clip;
    data.trackIndex = 0;
    data.selectedClipIndex = 0;
    data.clipName = "Test Clip";

    bool clipDupHandled = false;
    uint32_t dupTrk = 99;
    int dupClip = -1;
    panel.onDuplicateClip = [&](uint32_t t, int c) {
        clipDupHandled = true;
        dupTrk = t;
        dupClip = c;
    };

    ViewContext dummyCtx;
    TrackPropertiesHitResult hitClipDup{true, TrackPropertiesHitArea::ClipDuplicate, 0, 0.0f};
    panel.executeHitAction(hitClipDup, data, dummyCtx);
    assert(clipDupHandled);
    assert(dupTrk == 0 && dupClip == 0);

    bool clipDelHandled = false;
    uint32_t delTrk = 99;
    int delClip = -1;
    panel.onDeleteClip = [&](uint32_t t, int c) {
        clipDelHandled = true;
        delTrk = t;
        delClip = c;
    };

    TrackPropertiesHitResult hitClipDel{true, TrackPropertiesHitArea::ClipDelete, 0, 0.0f};
    panel.executeHitAction(hitClipDel, data, dummyCtx);
    assert(clipDelHandled);
    assert(delTrk == 0 && delClip == 0);

    // 6. Test GuiWindow History Undo & Redo for Track Add/Delete and Clip Add/Duplicate/Delete
    audio::AudioEngine engine;
    GuiWindow window(1280, 800);
    bool ok = window.initialize(engine);
    assert(ok);

    size_t baseTracks = window.getArrangerTracks().size();

    // Add track
    window.addTrackToProject("Undoable Synth", "synth", 0.4f, 0.7f, 0.9f);
    assert(window.getArrangerTracks().size() == baseTracks + 1);

    // Undo track addition
    bool undone = window.undoHistory();
    assert(undone);
    assert(window.getArrangerTracks().size() == baseTracks);

    // Redo track addition
    bool redone = window.redoHistory();
    assert(redone);
    assert(window.getArrangerTracks().size() == baseTracks + 1);

    // Add clip to the new track
    uint32_t addedIdx = static_cast<uint32_t>(baseTracks);
    window.addClipToTrack(addedIdx);
    assert(window.getArrangerTracks()[addedIdx].clips.size() == 1);

    // Duplicate clip
    window.duplicateClip(addedIdx, 0);
    assert(window.getArrangerTracks()[addedIdx].clips.size() == 2);

    // Delete clip
    window.deleteClip(addedIdx, 1);
    assert(window.getArrangerTracks()[addedIdx].clips.size() == 1);

    // Delete track
    window.deleteTrack(addedIdx);
    assert(window.getArrangerTracks().size() == baseTracks);

    // Undo track deletion
    bool undoDelete = window.undoHistory();
    assert(undoDelete);
    assert(window.getArrangerTracks().size() == baseTracks + 1);

    std::cout << "  [PASS] Clip Duplicate/Delete, clean empty rendering and History Undo/Redo validated." << std::endl;
}

int main() {
    std::cout << "=====================================================" << std::endl;
    std::cout << "   Eatsbits Modular UI/UX Architecture Test Suite   " << std::endl;
    std::cout << "=====================================================" << std::endl;

    testPointerAndGestures();
    testArrangerView();
    testEditView();
    testTrackInspectorView();
    testMixerView();
    testDesignView();
    testVirtualKeyboardDrawer();
    testDrumPadGridWidget();
    testProjectBrowserDrawer();
    testTransportHeader();
    testBottomNavBar();
    testGuiWindowIntegration();
    testValueEditDialogAndBackdropBlur();
    testCommandPaletteDialog();
    testFileDragAndDrop();
    testCircularGradients();
    testTextEditorWidgetAndMinimap();
    testArrangerMixerDrawer();
    testPluginSearchDialog();
    testTooltips();
    testTrackPropertiesActionsAndMuteSoloFix();
    testClipActionsAndHistoryRestoration();

    std::cout << "\n>>> ALL 22 MODULAR UI/UX TEST SUITES PASSED CLEANLY! <<<\n" << std::endl;
    return 0;
}

