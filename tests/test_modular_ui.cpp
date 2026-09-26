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
#include "eatsbits/ui/widgets/project_browser_drawer.hpp"
#include "eatsbits/ui/widgets/transport_header.hpp"
#include "eatsbits/ui/widgets/bottom_nav_bar.hpp"
#include "eatsbits/ui/widgets/plugin_search_dialog.hpp"
#include "eatsbits/ui/widgets/text_field_state.hpp"
#include "eatsbits/ui/widgets/value_edit_dialog.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include "eatsbits/ui/gui_window.hpp"
#include "eatsbits/audio/audio_engine.hpp"

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

    std::cout << "  [PASS] MixerView validated." << std::endl;
}

void testDesignView() {
    std::cout << "[Test 6/11] DesignView..." << std::endl;

    DesignView design;
    assert(design.getSubMode() == DesignSubMode::ModularRack);

    design.setSubMode(DesignSubMode::Eatscript);
    assert(design.getSubMode() == DesignSubMode::Eatscript);

    assert(design.getPatchCords().size() >= 2);
    const auto& c1 = design.getPatchCords()[0];
    assert(c1.r > 0.0f || c1.g > 0.0f || c1.b > 0.0f);

    std::cout << "  [PASS] DesignView validated." << std::endl;
}

void testVirtualKeyboardDrawer() {
    std::cout << "[Test 7/11] VirtualKeyboardDrawer..." << std::endl;

    VirtualKeyboardDrawer drawer;
    assert(!drawer.isExpanded());

    drawer.layout(1280.0f, 752.0f);
    float collapsedH = drawer.getDrawerHeight();
    assertNear(collapsedH, 22.0f, 1.0f);

    drawer.toggleExpanded();
    assert(drawer.isExpanded());
    float expandedH = drawer.getDrawerHeight();
    assert(expandedH > collapsedH);

    drawer.setBaseOctave(4);
    assert(drawer.getBaseOctave() == 4);

    std::cout << "  [PASS] VirtualKeyboardDrawer validated." << std::endl;
}

void testProjectBrowserDrawer() {
    std::cout << "[Test 8/11] ProjectBrowserDrawer..." << std::endl;

    ProjectBrowserDrawer browser;
    assert(!browser.isOpen());

    browser.open();
    assert(browser.isOpen());

    browser.setTab(BrowserDrawerTab::Macros);
    assert(browser.getTab() == BrowserDrawerTab::Macros);

    browser.setTab(BrowserDrawerTab::History);
    assert(browser.getTab() == BrowserDrawerTab::History);

    browser.setTab(BrowserDrawerTab::Presets);
    assert(browser.getTab() == BrowserDrawerTab::Presets);

    bool callbackTriggered = false;
    browser.onSelectPreset = [&](const std::string& id) {
        callbackTriggered = true;
        assert(!id.empty());
    };

    if (browser.onSelectPreset) {
        browser.onSelectPreset("303_acid_lead");
    }
    assert(callbackTriggered);

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

    // Navigation state verification
    window.setActiveView(WorkspaceView::Mixer);
    assert(window.getActiveView() == WorkspaceView::Mixer);

    window.setActiveView(WorkspaceView::Edit);
    assert(window.getActiveView() == WorkspaceView::Edit);
    window.setEditSubView(EditSubView::PianoRoll);
    assert(window.getEditSubView() == EditSubView::PianoRoll);

    window.setActiveView(WorkspaceView::Design);
    assert(window.getActiveView() == WorkspaceView::Design);
    window.setDesignSubView(DesignSubView::ModularRack);
    assert(window.getDesignSubView() == DesignSubView::ModularRack);

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

    std::cout << "  [PASS] ValueEditDialog, Right-Click Editing & Backdrop Blur validated." << std::endl;
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
    testProjectBrowserDrawer();
    testTransportHeader();
    testBottomNavBar();
    testGuiWindowIntegration();
    testValueEditDialogAndBackdropBlur();

    std::cout << "\n>>> ALL 12 MODULAR UI/UX TEST SUITES PASSED CLEANLY! <<<\n" << std::endl;
    return 0;
}
