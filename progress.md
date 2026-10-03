# Eatsbits Progress & Eatsbeats Alignment Guide

This document tracks the progress of aligning the **Eatsbits** native C++ DAW with the visual design, interactive aesthetics, and event handling logic of the original **Eatsbeats** (`c:\git\eatsbeats`), while maintaining the superior, zero-allocation C++20 real-time audio core.

---

## 1. Architectural Philosophy: The Modular Subsystem Pattern

In Eatsbeats, monoliths were broken down into clean, modular, purpose-driven tabs and controllers. Eatsbits adopts this exact principle in modern C++20 (Option B):

```
                        +---------------------------+
                        |        GuiWindow          |
                        |   (GLFW / Native Shell)   |
                        +-------------+-------------+
                                      |
                     createViewContext() [Viewport, Theme, AudioEngine]
                                      |
       +------------------+-----------+-----------+------------------+
       |                  |                       |                  |
+------v------+    +------v------+         +------v------+    +------v------+
| ArrangerView|    |  EditView   |         |  MixerView  |    |  DesignView |
| (Timeline,  |    | (Piano Roll,|         | (Strips,    |    | (Rack,      |
|  Clips,     |    |  Tracker,   |         |  Routing,   |    |  Cables,    |
|  Tracks)    |    |  Score)     |         |  Meters)    |    |  Eatscript) |
+------+------+    +-------------+         +-------------+    +-------------+
       |
       +---> PluginSearchDialog (Reusable Contextual Dialog for Instruments, MIDI FX, Audio FX)
       +---> Properties Drawer (Contextual: Track Properties vs. Clip Properties)
```

### Core Abstractions
- **`ViewBase`**: Interface requiring `layout()`, `render()`, `handlePointer()`, and `handleKey()`.
- **`ViewContext`**: Lightweight context object passed to views containing:
  - `BatchRenderer2D*`: GPU-accelerated drawing primitives (rectangles, rounded outlines, gradients, text, arcs, knobs, icons).
  - `const ThemeTokens*`: Active color palette and typography styles.
  - `AudioEngine*`: Real-time audio engine, sequencer, audio graph, and transport.
  - Navigation callbacks (`onNavigateTab`, `onJumpToClipEdit`, `onToggleBrowser`, `onShowNotification`).
  - Screen and logical coordinate metrics.
- **`PointerEvent`**: Device-agnostic pointer abstraction unifying mouse clicks, touch taps, mouse move deltas, and 2D trackpad/wheel scrolls.

---

## 2. Step-by-Step Alignment Process for Every View

To align any part of Eatsbits with original Eatsbeats, follow this systematic 6-step recipe:

### Step 1: Study Visual Layout & Palette in Eatsbeats
1. Inspect the corresponding tab in `c:\git\eatsbeats` (visual structure, padding, fonts, and responsive behavior).
2. Utilize curated `ThemeTokens` (`theme.primaryAccent`, `secondaryAccent`, `controlBackground`, `controlWell`, etc.) instead of generic hardcoded colors.

### Step 2: Implement Component State in View Class
1. Add state structs and members to the modular view header (e.g. `include/eatsbits/ui/views/<view>_view.hpp`).
2. Separate business/sequencer state from interactive UI state (e.g. drag modes, hover states, scroll offsets).

### Step 3: Implement 2D Layout & Layered Rendering
1. In `layout(bounds, ctx)`: Compute bounding boxes for headers, grids, scrollbars, and buttons.
2. In `render(ctx)`: Render from back to front:
   - Background lanes / chassis.
   - Grid lines and sub-beat markers.
   - Interactive content (clips, notes, modules, channel strips).
   - Overlays, badges, active indicators, and slide-out drawers.
   - Modal dialogs (rendered on the top layer when active).

### Step 4: Implement Unified Event Handling & Drag Modes
1. Define an explicit enum for interaction modes (e.g. `None`, `Move`, `Resize`, `Pan`, `Scrub`).
2. Check modal dialogs first (`if (modalDialog.isOpen()) return modalDialog.handlePointer(ev);`).
3. Handle active drags globally across `PointerAction::Move` and `PointerAction::Up` so rapid mouse movements never lose tracking when leaving widget boundaries.
4. Implement middle-click and wheel scrolling for 2D viewport navigation.

### Step 5: Wire into `GuiWindow`
1. In `GuiWindow::renderFrame()`: Delegate the active workspace directly to `modularView_->layout()` and `modularView_->render(ctx)`.
2. In `GuiWindow::onMouseDown`, `onMouseMove`, `onMouseUp`, `onMouseScroll`, `onKeyDown`: Route events directly to `modularView_->handlePointer()` / `handleKey()`.
3. Eliminate legacy inline drawing and duplicate state from `gui_window.cpp` to prevent dead cruft.

### Step 6: Automated Testing & Verification
1. Add unit and interaction tests in `tests/test_modular_ui.cpp` verifying bounds, clicks, drags, and modal toggles.
2. Verify all 27 unit tests pass with `ctest --test-dir build -C Release --output-on-failure`.
3. Test locally with `./build.ps1` to ensure instantaneous linking and running.

---

## 3. Arranger View: Completed Milestones & Eatsbeats Parity

| Feature | Eatsbeats Requirement | Implementation in Eatsbits | Status |
| :--- | :--- | :--- | :--- |
| **Contextual Inspector** | Clicking track opens Track Properties; clicking clip opens Clip Properties | Sliding drawer with auto-switching `ArrangerInspectorTab` (`Track` vs. `Clip`) | Complete |
| **No Master Bus Track** | Master bus exists only on Mixer console, never as an arranger track lane | Removed master track lane; pure 5-track instrument arrangement with dedicated Mixer master strip | Complete |
| **Clip Loop Resize Handle** | Upper-right corner loop icon (`↻`) for loop-based duration extension | 22x18px handle in top-right corner; extends loop cycles with visible cycle divider bars | Complete |
| **Cross-Track Clip Movement** | Piano-roll style dragging across tracks and horizontally | Smooth 2D drag snapping to bar grid and transferring clip seamlessly across track lanes | Complete |
| **'+ ADD TRACK' Option** | Dedicated row below the last track to add instruments | Clickable `+ ADD TRACK` row below track list opening `PluginSearchDialog` | Complete |
| **Reusable Plugin Dialog** | Unified modal dialog for Instruments, Audio FX, and MIDI FX | `PluginSearchDialog` with category pills (`SYNTHS`, `DRUMS`, `RETRO`, etc.), query filter, and selection callback | Complete |
| **Track Properties FX Racks** | Buttons to add FX directly from track drawer | `+ ADD MIDI FX` and `+ ADD FX` buttons opening `PluginSearchDialog` | Complete |
| **Hardware Rotary Knobs** | 4 sweep dials (`TONE`, `SNAPPY`, `DECAY`, `VAR`) with instrument swap | 4 rotary dial controls with arc sweeps, parameter labels, and `[⇄ CHANGE INSTRUMENT]` | Complete |
| **2D Viewport Panning** | Piano roll-style navigation of the timeline | Middle-click drag, scroll wheel, and timeline minimap overview scrubbing | Complete |
| **Code Hygiene** | Zero dead cruft in shell window | Removed 450+ lines of monolithic Arranger code from `gui_window.cpp`; automated process unlock in `build.ps1` | Complete |

---

## 4. Edit View: Completed Milestones & Eatsbeats Parity

| Feature | Eatsbeats Requirement | Implementation in Eatsbits | Status |
| :--- | :--- | :--- | :--- |
| **Typography & Fonts** | High-fidelity typography matching Mixer tab | Bundled TrueType fonts via FontStash: `assets/fonts/Oswald-Medium.ttf` for UI elements/labels/tags (`drawVectorString` / `drawText`), and `assets/fonts/ShareTechMono-Regular.ttf` for tracker matrices, values & code (`drawMonoString` / `drawMonoText`) | Complete |
| **Piano Roll** | Alternating accidental/natural stripes, velocity stalks, live auditioning, right-click pitch select | FL Studio metrics (C1..C6 clamped lower bound), note move/duration resize, **double-click note creation**, right-click delete, slide ramps & accent dots, key illumination during audition, velocity stalks, marquee selection, live audition | Complete |
| **Tracker** | FastTracker 2 / Renoise hexadecimal note/vol/sld/fx matrix | 4-channel matrix rows (00..63) rendered with bundled `ShareTechMono-Regular.ttf`, QWERTY note entry, arrow key navigation (Up/Down/Left/Right), octave switcher `[-]`/`[+]`, live playhead row highlight, delete key clearing | Complete |
| **Score** | Standard notation engraving with clefs and note tools | Treble & Bass staff engraving with G/F clef, note heads, full ledger lines (C4/A5/E2), accidentals (`#`), stems, measure bars every 16 steps, **note head click selection**, **right-click delete**, **marquee drag selection**, **double-click note creation**, live moving playhead | Complete |
| **Script (Eatscript)** | Bi-directional note notepad synchronized with clip events | Text editor with line number gutter, syntax coloring using bundled mono font, `[APPLY (Ctrl+Enter / F5)]`, `[REVERT / SYNC]`, status bar, declarative note syntax | Complete |
| **Selection Sidebar** | Auto-sliding note inspector sidebar for single & multi-note batch editing | 265px sliding inspector with Pitch transpose (`±1`, `±12`), Step position, Duration, Velocity presets (`25%`..`100%`, `Humanize`), Articulations (`Pizz`, `Stacc`, `Legato`, `Slap`, `Flam`), Gliss/Bend, Retrograde, Pitch Inversion, and Selection utilities | Complete |
| **Ghost Notes** | Background track overlay with opacity slider & toggle | Header toggle & percentage slider (`GHOST 35%`), rendering background tracks translucently on grid & score | Complete |
| **Navigation & Pan** | Seamless tab navigation & middle-mouse 2D navigation | Fixed "Edit in Piano Roll" selection persistence by replacing 60 FPS per-frame resyncs with event-driven state transitions; middle-mouse panning routed to `modularEditView_`; sub-view switcher buttons responsive at all window widths | Complete |
| **C++ Core Sync** | Real-time zero-allocation audio engine synchronization | Direct two-way sync with `sequencer::StepSequencer` & `SequencerTrack`, live MIDI noteOn/noteOff auditioning, Spacebar play/stop toggle | Complete |
| **Code Hygiene** | Modular Subsystem Pattern | Removed 980+ lines of monolithic Edit view cruft from `gui_window.cpp`; 100% test pass rate across all 27 unit tests | Complete |

---

## 5. Workstation Alignment Roadmap

### Phase 1: Edit View (Piano Roll, Tracker, Score, Script) [COMPLETED]
- [x] **Piano Roll**:
  - Align note selection, multi-selection marquee, and pitch row styling with Eatsbeats.
  - Implement note velocity stalks with gradient color-coded levels.
  - Implement 2D scrolling and middle-click pan matching Eatsbeats gestures, with vertical scrolling strictly bounded at C1 (pitch 24).
  - Implement **double-click note creation** on empty grid slots, right-click note deletion, and auditioning key illumination.
  - Implement vertical auto-centering on existing note content ([autoCenterOnNotesOrDefault](file:///c:/git/eatsbits/src/ui/views/edit_view.cpp#L315)) matching Eatsbeats, calculating bounding pitch range and centering viewport.
  - Fix note selection preservation when navigating from Arranger "Edit in Piano Roll" button.
  - Implement **double-click on clip in Arranger tab** to directly open the clip in Edit tab / Piano Roll.
- [x] **Tracker**:
  - FastTracker 2 / Renoise layout with authentic hexadecimal note/effect matrices and QWERTY input.
  - Render all tracker step and column figures using bundled `ShareTechMono-Regular.ttf`.
  - Arrow key step/channel navigation and delete/backspace note clearing.
  - Real-time playhead step row illumination across all 64 steps during playback.
- [x] **Score**:
  - Vector notation engraving with staff lines, clefs, and responsive note spacing.
  - Ledger lines for pitches outside the 5 staff lines (C4, A5, E2), sharp accidental flags.
  - **Note selection on note head click**, right-click note deletion, **marquee drag selection** across the staff, and **double-click note creation**.
  - Moving vertical playhead line synchronized with transport.
- [x] **Script (Eatscript IDE)**:
  - Declarative note script syntax, syntax coloring, status bar, and bi-directional compilation (`Ctrl+Enter` / `F5`).
- [x] **Note Inspector Sidebar**:
  - Single-note & multi-note batch transpositions, nudges, velocity presets, articulations (`Normal`, `Pizz`, `Stacc`, `Legato`, `Slap`, `Flam`), gliss/bend toggles.
  - Classical transformations: Retrograde (time reversal) and Pitch Inversion (melodic inversion around mean pitch).

### Phase 2: Mixer View [COMPLETED]
- [x] **Authentic Eatsbeats Visual Feel & Space-Efficient Layout**:
  - Pinned Master channel strip on the far left (`width = 140px`) with amber/gold outline, top backlit vintage amber LCD screen (`MASTER`, `st-out`, volume %), rotary Pan knob with "C" center button below, vertical fader with gold indicator line, and dual glass stereo peak LED meter.
  - Horizontally scrollable track channel strips (`width = 140px` each, gap = 10px). Selected track highlighted with vibrant glowing outline matching track color (e.g. magenta for Kick, green for 303, amber for Drums).
  - Top Backlit LCD: Vintage amber monospace matrix display (`EATS KICK`, `center`/`Lxx`/`Rxx`, volume percentage `95%`).
  - Rotary Pan dial with track-colored indicator needle and dedicated "C" center pill button.
  - Three-column lower body layout: Long vertical fader track with metallic silver cap and colored center groove, dual glass stereo peak LED meter with centered vertical dB scale markings (`-inf`, `-6`, `-12`, `-18`, `-24`, `-30`, `-36`, `-42`, `-48`, `-54`, `-60`), and compact vertical hardware button column (`M` - Mute Red, `S` - Solo Yellow, `*` - Freeze Cyan).
  - Floating tactile tooltip badge on hover/drag (e.g. `Eats Hats Volume: 0.75`).
  - Removed top bulky "Studio Mixing Console" header and option pills to maximize vertical screen real estate.
  - Strictly decoupled from Edit view: Channel strips and LCDs never switch to EDIT view; clicking selects the track and updates/expands the Track Properties sidebar.
- [x] **Decoupled Track Properties Sidebar Drawer**:
  - Reused [TrackPropertiesDrawer](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/track_properties_drawer.hpp) shared cleanly across both Arranger and Mixer tabs.
  - 24px pull tab, interactive drag-resize, 8 quick color swatches, track reorder `<`/`>`, 3-band parametric EQ with `RESET FLAT` and `ACTIVE`/`BYPASS`, Instrument Card with macro knobs, and MIDI/Audio FX racks.
  - Eliminated monolithic raw rendering cruft (445+ lines) from `gui_window.cpp`.
  - Maintained 100% test pass rate across all 27 unit tests.

### Phase 3: Design View & Modular Rack
- [x] **Full Eatsbeats Parity for Design Tab**:
  - Sub-navigation header with sidebar toggle, target selector badge & dropdown, mode switcher (`<> CODE`, `MODULAR`, `SPLIT`, `GUI`), and `> COMPILE & RUN`.
  - Collapsible **PROJECT SCRIPTS** Explorer sidebar categorized into Synths & DSP (4), Audio FX Inserts (3), Track MIDI FX, Clip Scripts (4), and Built-in Presets (156) with live filter search.
  - Real-time Audio Oscilloscope LCD Display with CRT grid and live waveform visualization.
  - High-performance Code Editor with line numbers, syntax highlighting (comments, keywords, API calls, strings), select all, copy, submit PR, and API docs links.
  - Live Script Parameters Panel with dynamic cards, value readouts, and interactive sliders for real-time DSP tuning.
  - Eurorack Modular Rack canvas with procedural Bezier catenary patch cables (gravitational sag, drop shadows, metallic jacks), inputs/outputs, and skeuomorphic module faceplates.
  - Split View with dual code editor and hardware faceplate / modular rack viewports.
  - GUI Interface with live skeuomorphic hardware faceplate interaction and integrated visual GUI Designer (widget palette, canvas editor, property inspector).
  - Strictly decoupled from `gui_window.cpp`: eliminated 445+ lines of monolithic raw inlined rendering into [DesignView](file:///c:/git/eatsbits/include/eatsbits/ui/views/design_view.hpp) and [design_view.cpp](file:///c:/git/eatsbits/src/ui/views/design_view.cpp).
  - Fixed hit testing interception in [gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp): removed obsolete hardcoded sub-bar switcher and gave dynamic `modularDesignView_->handlePointer()` priority so `<> CODE`, `MODULAR`, `SPLIT`, and `GUI` buttons work reliably regardless of window dimensions.
  - Verified and enhanced GUI mode: hardware faceplate live interaction with interactive rotary knob tweaking and visual GUI designer mode with widget palette, inspector, and PR submission.
  - Maintained 100% test pass rate across all 27 unit tests.

### Phase 4: Track Tab Alignment [COMPLETED]
- [x] **Full Eatsbeats Parity for Track Tab**:
  - Implemented modular [TrackInspectorView](file:///c:/git/eatsbits/include/eatsbits/ui/views/track_inspector_view.hpp) and [track_inspector_view.cpp](file:///c:/git/eatsbits/src/ui/views/track_inspector_view.cpp) completely decoupled from `gui_window.cpp`.
  - **Top Track Navigation Ribbon**: Quick channel switching pills with track color dots, uppercase track names, and glowing selection highlights.
  - **Track Identity Header Card**: Color-accented vertical indicator pill, uppercase track name, channel subtitle (`TRACK CHANNEL X • SYNTH/DRUMS`), 8 rapid color swatches, `[ < > CODE ]` navigation button, and tactile hardware buttons (`MUTE` Red, `SOLO` Yellow, `FREEZE` Cyan).
  - **Channel Mixer Settings**: Horizontal volume slider with exact percentage readout (e.g. `100%`) and centered pan slider with stereo readout (`CENTER`, `Lxx%`, `Rxx%`).
  - **Dynamic Instrument Hardware Faceplate**: Preset navigation (`< PREV`, `NEXT >`), `[ ⇄ CHANGE INSTRUMENT ]` button, authentic skeuomorphic chassis with subtle beveling, tactile knurled rotary dials with pointer needles and value badges, and embedded live real-time CRT audio oscilloscope.
  - **Harmonic Chord Track Follow Settings**: 5 choice mode chips (`OFF`, `CHORD`, `BASS`, `SCALE`, `COLOR LEAD`), `[ ⚡ BAKE TO MIDI ]` action button, and dynamic harmonic description readouts.
  - **MIDI FX Pipeline Rack**: Arpeggiator module (rate, gate, pattern selector), Scale Snap module (root key, scale mode, snap intensity), and Humanize module (timing and velocity drift dials).
  - **Audio FX Insert Rack**: 5-rack module pipeline (Stereo Ping-Pong Delay, Analog Chorus, 5-Band Parametric EQ, Studio Compressor, and Convolver Reverb) with individual bypass toggle switches and interactive rotary dials.
  - **Embedded PluginSearchDialog**: Modal overlay dialog for hot-swapping instruments and inserting MIDI and audio FX with category tabs, search filtering, and clean lifecycle management.
  - **Cruft-Free Integration**: Decoupled layout and pointer dispatch in [gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp), preserving full compatibility with legacy headless interaction tests.
  - **100% Test Pass Rate**: All 28 test suites passed with 0 failures (`test_modular_ui`, `test_gui_interaction`, audio engines, DSP, and graph tests).

### 4.7 Hardware WebGPU Acceleration & MaddestLabs Apocalypse CRT Parity
- **Offloaded from CPU to Physical GPU Hardware**:
  - Replaced legacy OpenMP CPU rasterizer and GDI `StretchDIBits` presentation with native WebGPU (`wgpu-native`) driving physical GPU hardware (NVIDIA GeForce GTX 1660 Ti via Vulkan/D3D12).
  - Post-processing fullscreen triangle render pass executing in pure WGSL with sub-millisecond execution times (< 0.2 ms).
  - Added fast 1-instruction bypass when CRT shader is toggled OFF (`crtEnabled < 0.5`).
- **Restored MaddestLabs Apocalypse CRT Reflective Frame Aesthetic**:
  - **Two-Tier Bezel Geometry**: Outer reflective chamfered chassis frame (`frameSize`) + inner dark overscan buffer (`borderSize`).
  - **5-Tap Frosted Reflection Blur**: Restored diffuse frosted reflection spill across glossy bevel using hardware linear sampler offsets.
  - **Authentic Chassis Micro-Grain**: Procedural hash grain `(rnd(suv * 2.0) - 0.5) * 0.12` providing vintage matte chassis texture.
  - **Deep Corner Occlusion Shadow**: Non-linear corner decay `lerp(frameLight, 0.0, minDist / max(nX, nY) * 4.0)` matching the original HLSL shader.
  - **Optimized Dynamic Uniforms**: Precomputed mechanical rumble micro-jitter, dynamic power sag dimming, horizontal sync scan waves, and swaying incandescent studio spotlight on CPU, eliminating per-pixel trigonometric overhead.
- **Verification**:
  - 100% test pass rate across `test_gui_interaction` and all GUI/audio suites running on GPU hardware.

### 4.8 Unified Track Properties & Track Tab Architecture (`TrackPropertiesPanel`)
- **Single Source of Truth**:
  - Extracted the unified [TrackPropertiesPanel](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/track_properties_panel.hpp) component, eliminating over 1,000 lines of duplicated logic across the Arranger sidebar, Mixer sidebar, and dedicated Track tab.
  - **Shared Responsive Visuals**:
    - **Adaptive Layout**: Automatically detects viewport width (`contentW >= 560px` expanded vs. `contentW < 560px` compact sidebar).
    - **Dynamic Hardware Faceplate**: Authentic skeuomorphic chassis (TB-303, TR-808, TR-909, DX7, Piano), rotary dials with pointer needles, and embedded live real-time CRT audio oscilloscope.
    - **Harmonic Chord Follow**: Mode selector chips (`OFF`, `CHORD`, `BASS`, `SCALE`, `COLOR`), `[ BAKE TO MIDI ]` button, and dynamic harmonic descriptions available in all three views.
    - **Integrated FX Pipelines**: MIDI FX Rack and Audio FX Rack with bypass toggles, dials, and embedded `PluginSearchDialog` modal dialog.
    - **Shared Smooth Scrolling**: Virtual scroll container with mouse wheel support, touch drag, and custom scrollbar across both narrow drawer and widescreen tab.
- **Consumer Alignment**:
  - [TrackPropertiesDrawer](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/track_properties_drawer.hpp): Streamlined to a thin sliding pull-tab and drag-resize shell hosting `TrackPropertiesPanel`.
  - [TrackInspectorView](file:///c:/git/eatsbits/include/eatsbits/ui/views/track_inspector_view.hpp): Streamlined to a full-screen workspace host with top track selector ribbon delegating to `TrackPropertiesPanel`.
- **Validation**: 100% test pass rate across all 29 test suites (`ModularUiTest`, `GuiInteractionTest`, DSP, and engine suites).

### 4.9 Track Properties Refinements: Rotary Pan Knob, Relocated Color Palette, Modular FX Racks & Scroll Isolation
- **Rotary Pan Knob & Extended Volume Slider**:
  - Replaced horizontal Pan slider with an authentic rotary dial knob equipped with a 12 o'clock center detent tick, needle pointer, top label, and "C" / "Lxx" / "Rxx" dynamic readout.
  - Sparing the horizontal space expanded the continuous Volume slider track width by over 2.5x, giving users significantly more precision and travel.
- **Arranger Sidebar Mouse Wheel Isolation**:
  - Explicitly routed `PointerAction::Scroll` events occurring within the expanded properties drawer sidebar to the sidebar drawer itself rather than bubbling through to the Arranger timeline/grid.
- **Restored Modular Eatsbeats FX & MIDI FX Pipeline**:
  - Removed static toggle pills (Arp, Delay, Chorus, Humanize, etc.) in favor of user-defined, modular dynamic racks (`data.midiFx` and `data.audioFx`).
  - Restored dynamic item rows with illuminated power status dots, item titles, status badges (`ACTIVE` / `BYPASS`), deletion `[X]` buttons, and `+ ADD MIDI FX` / `+ ADD FX` buttons launching `PluginSearchDialog`.
- **Relocated Track Color Swatches**:
  - Positioned the 8 track color options directly below the track identity/title section in both compact sidebar and expanded widescreen views.
- **Verification**: 100% test pass rate across all 29 test suites (including `GuiInteractionTest` and `ModularUiTest`).

### 4.10 Comprehensive Eatsbeats Project Browser Alignment & Modular Decoupling
- **Full 6-Hub Workstation Hub Parity**:
  - Ported all 6 browser tabs from the Flutter Eatsbeats project drawer (`c:\git\eatsbeats\lib\ui\widgets\project_browser_drawer.dart`) into the native C++20 [ProjectBrowserDrawer](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/project_browser_drawer.hpp):
    - **Tab 1: Project Assets**: Real-time project asset overview (active sequencer tracks with color strips, mute/solo badges, audio clips, active FX inserts, and SoundFonts).
    - **Tab 2: Script & Engine Library**: Eatscript algorithms, DSP engines, synth scripts, audio FX scripts, MIDI FX scripts, and generative sequencer routines.
    - **Tab 3: Preset Library**: Sound patches (Lead, Bass, Pad, Keys, Pluck), acoustic impulse responses, and guitar/bass amp cabinet simulations with load actions.
    - **Tab 4: Expansion Packs**: Community and factory expansion packs, bundled SoundFonts, and the Audio-to-MIDI neural tool.
    - **Tab 5: Saved Projects File Manager**: Native `<filesystem>` scanner of `./Projects/` and `*.eats` files with inline file metrics, creation timestamps, Save, Save As, Explore, Refresh, Load, and Delete controls.
    - **Tab 6: History & Time Travel**: Diff history inspection displaying checkpoints, timestamps, modified tracks/properties, and instantaneous rollback/redo actions.
- **Architectural Modernization & Decoupling**:
  - Completely removed 348 lines of monolithic inline browser drawing and ad-hoc event handling from [gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp).
  - Encapsulated all browser tab strip layout, search bar filtering, and dynamic content rendering inside [project_browser_drawer.cpp](file:///c:/git/eatsbits/src/ui/widgets/project_browser_drawer.cpp).
  - Integrated cleanly through `projectBrowserDrawerWidget_->render(*batchRenderer_, getTheme())` and `projectBrowserDrawerWidget_->handlePointer(ev)`.
- **Validation**:
  - Added full automated suite `testProjectBrowserDrawer()` in [tests/test_modular_ui.cpp](file:///c:/git/eatsbits/tests/test_modular_ui.cpp) validating all 6 tabs, item search filtering, and callback triggers.
  - 100% test pass rate across all test suites (`test_modular_ui`, `test_gui_interaction`, etc.) and clean build of `eatsbits_gui.exe`.

### 4.11 Interactive Drum Pad Matrix (16-Pad MPC/SP-1200 Grid) & Dual-Mode Virtual Instrument Drawer
- **Full Eatsbeats Drum Pad Parity**:
  - Ported [DrumPadGridWidget](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/drum_pad_grid_widget.hpp) from `c:\git\eatsbeats\lib\ui\widgets\drum_pad_grid_widget.dart` into native C++20:
    - **16-Pad MPC/SP-1200 Matrix**: Tactile rubber pads with chamfered borders, recessed shadows, dynamic accent colors (Cymbals gold, Toms purple, Hats cyan, Kicks red, Snares orange, Claps coral, Latin percussion azure/violet/green).
    - **Dual-Bank Switching**:
      - **Core Kit (16 Pads)**: Cymbals (Crash 1, Ride 1, Ride Bell, Splash), Toms (High, Low-Mid, Hi Floor, Low Floor), Hats (Closed, Pedal, Open, Elec Snare), Kicks & Snares (Kick 1, Ac. Snare, Side Stick, Hand Clap).
      - **Percussion (16 Pads)**: Latin Cymbals (Crash 2, China, Cowbell, Tambourine), Bongos & Timbales (Hi/Low Bongo, Hi/Low Timbale), Congas & Agogo (Mute/Open/Low Conga, Hi Agogo), Wood & Shakers (Claves, Hi Block, Maracas, Triangle).
    - **Dynamic Kit Profiles**: Automatic styling adaptation for `EATS-808` (amber/orange accents), `EATS-909` (sky-blue/cyan accents), and `MODULAR DRUM PADS` (gold/crimson accents).
    - **Y-Axis Velocity Sensitivity**: Top of pad hits trigger high strike velocity (~1.0), lower region hits trigger softer strike (~0.65).
    - **Active Playback & Flash LED Indicators**: Real-time pad flash and circular LED lighting dot decaying smoothly after hits (~140ms).
- **Dual-Mode Virtual Instrument Drawer Integration**:
  - Modernized [VirtualKeyboardDrawer](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/virtual_keyboard_drawer.hpp) with `KeyboardDrawerMode::Piano` vs `KeyboardDrawerMode::DrumPads`.
  - Added mode toggle buttons `[ KEYS ]` and `[ PADS ]` in the drawer toolbar with dynamic pull-tab labeling and height auto-adjustment (110px for Piano keys vs 164px for 16-pad MPC matrix).
  - Wired real-time sound auditioning via `AudioEngine::postNoteOn` and `postNoteOff`.
- **Validation**:
  - Added unit and interaction suite `testDrumPadGridWidget()` and updated `testVirtualKeyboardDrawer()` in [tests/test_modular_ui.cpp](file:///c:/git/eatsbits/tests/test_modular_ui.cpp).
  - 100% test pass rate across all 13 modular UI test suites and verified clean build of `eatsbits_gui.exe`.

### 4.12 Universal Quick Command Palette Dialog (`Ctrl+P` / `Ctrl+K` / Search)
- **Full Eatsbeats Command Palette Parity**:
  - Ported [CommandPaletteDialog](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/command_palette_dialog.hpp) and [command_palette_dialog.cpp](file:///c:/git/eatsbits/src/ui/widgets/command_palette_dialog.cpp) from `c:\git\eatsbeats\lib\ui\widgets\command_palette_dialog.dart` and `c:\git\eatsbeats\lib\models\command_palette_registry.dart` into native C++20:
    - **Floating Spotlight Overlay**: Centered modal chassis (600x480) with GPU backdrop blur (`applyBackdropBlur(4.0f, 0.60f)`), outer drop shadow, and glowing border.
    - **Categorized Command Taxonomy**:
      - `All`: Unified view of all registered workstation commands.
      - `View`: Neon-green category for Arranger, Edit View (Piano Roll/Tracker), Track Inspector, Modular Mixer, Modular Rack, and Drawers.
      - `Action`: Cyan category for Play/Pause, Panic/Stop, Loop, Metronome, Undo, Redo, Save, Save As, and Bounce WAV.
      - `Preset`: Warm amber category for quick-loading TB-303, TR-808, TR-909, DX7, and grand piano presets.
      - `Theme`: Purple category for switching color schemes (Cyberpunk Neon, Midnight Blue, Charcoal Studio, etc.).
      - `Macro`: Coral pink category for procedural acid basslines, 909 grooves, and note humanization.
    - **Live Substring Search**: Real-time filtering matching against command titles, subtitles, and IDs with instant result count badge and selected highlight row.
    - **Keyboard-First Workflow**: Arrow key navigation (`Up`/`Down`), `Enter` to execute, `Escape` to dismiss, typographic text entry (`onChar`), and backspace editing.
    - **Desktop Shortcut & Transport Search Bar Integration**:
      - Intercepts `Ctrl+P` and `Ctrl+K` in `GuiWindow::onKeyDown` to toggle the palette.
      - Connected `TransportAction::SearchToggle` directly to `GuiWindow::toggleCommandPalette()`.
- **Validation**:
  - Added comprehensive automated test suite `testCommandPaletteDialog()` in [tests/test_modular_ui.cpp](file:///c:/git/eatsbits/tests/test_modular_ui.cpp) testing open/toggle/close, command registration, category filtering, search queries, keyboard navigation, and `GuiWindow` integration.
  - 100% test pass rate across all 14 modular UI test suites and verified clean build of native binaries.

### 4.13 Subsystem 1: Audio-to-MIDI Transcription Engine & Interactive Dialog (`AudioToMidiEngine` & `AudioToMidiDialog`)
- **Full Eatsbeats Subsystem 1 Parity**:
  - Ported and enhanced the audio-to-MIDI transcription engine from `c:\git\eatsbeats` (`audio_to_midi_engine.dart` and `audio_to_midi_dialog.dart`) into high-performance, real-time native C++20:
    - **`AudioToMidiEngine` Core DSP** ([audio_to_midi_engine.hpp](file:///c:/git/eatsbits/include/eatsbits/audio/audio_to_midi_engine.hpp), [audio_to_midi_engine.cpp](file:///c:/git/eatsbits/src/audio/audio_to_midi_engine.cpp)):
      - **Decoded Audio & Formats**: Unified `miniaudio` decoder streaming supporting WAV, MP3, FLAC, OGG, and AIFF files, with robust RIFF/WAV fallback, automatic stereo-to-mono downmixing, and linear sample-rate conversion to 44.1 kHz.
      - **128/160-Point Waveform Preview**: RMS and peak energy extraction for instantaneous UI visualization.
      - **Multi-Band Transient & Onset Detection**: High-frequency spectral flux and short-term energy derivatives with dynamic thresholding and minimum inter-onset refractory periods.
      - **Monophonic YIN Pitch Tracker**: High-precision sub-sample parabolic interpolation, normalized difference function $d'(\tau)$, and absolute thresholding across musical frequencies (MIDI 21 to 108).
      - **Polyphonic Hybrid DSP CQT & Harmonic Matching**: Tone bank correlation across 88 chromatic pitches evaluating fundamental and harmonic series ($2f_0, 3f_0$), peak picking with dynamic thresholding, note re-articulation, and duration filtering.
      - **Percussive Drum Transcriber**: 3-band spectral flux partitioning isolating low-end punches (Kick, MIDI 36), mid-range snap (Snare, MIDI 38), and high-frequency metallic transients (Closed Hi-Hat, MIDI 42).
      - **Harmonic Chord Track Integration**: Direct coupling with `theory::ChordTheory::extractChordsFromNotes` to automatically detect chord progressions and emit `ChordTrackEvent` markers.
      - **Cooperative Cancellation**: Non-blocking asynchronous transcription with `CancellationToken` and progress reporter callbacks.
    - **`AudioToMidiDialog` Modal UI** ([audio_to_midi_dialog.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/audio_to_midi_dialog.hpp), [audio_to_midi_dialog.cpp](file:///c:/git/eatsbits/src/ui/widgets/audio_to_midi_dialog.cpp)):
      - Glassmorphic modal window (580x560) with GPU darkened backdrop blur and cyan glowing border.
      - Mode selection button strip (`[ HYBRID DSP ]`, `[ YIN PITCH ]`, `[ PERCUSSIVE ]`).
      - Interactive gradient waveform overview with animated audition playhead and `[ > AUDITION ]` toggle.
      - 4 precision drag sliders (`Onset Sensitivity`, `Frame Threshold`, `Min Duration ms`, `Velocity Scale`).
      - Checkbox toggles for `Create New Track` and `Extract Chords to Chord Track`, plus editable `TextFieldState` track name.
      - Progress bar display and non-blocking `[ TRANSCRIBE TO MIDI ]` background task execution.
    - **Workstation UI Wiring** ([gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp)):
      - Integrated native file chooser (`*.wav;*.mp3;*.flac;*.ogg;*.aif`) via `promptOpenAudioFile()`.
      - Connected `projectBrowserDrawerWidget_->onLaunchAudioToMidi` in the Packs tab directly to `openAudioToMidiConverter()`.
      - Registered global command `action.audio_to_midi` (`Ctrl+M`) in `CommandPaletteDialog`.
      - Event routing in `GuiWindow` for pointer actions, scroll wheels, text char inputs, and hotkeys.
- **Validation**:
  - Dedicated test suite [test_audio_to_midi.cpp](file:///c:/git/eatsbits/tests/test_audio_to_midi.cpp) covering 11 unit tests (monophonic A4, C4, polyphonic dyads, percussive transients, silence rejection, cancellation, chord extraction, waveform peaks, modal hit-testing, and command palette integration).
  - 100% test pass rate across all 31 test suites in the Eatsbits project.

### 4.14 Subsystem 2: Algorithmic Procedural Music & Song Generation Engine (`ProceduralSongEngine`, `SongArchetypeRegistry`, `ProceduralAcidEngine`, `ProceduralDrumEngine`, `ProceduralPianoEngine`, `ProceduralEnsembleEngine`)
- **Full Eatsbeats Subsystem 2 Parity**:
  - Ported and enhanced the entire procedural algorithmic music suite from `c:\git\eatsbeats` (`lib/audio/procgen/*` and `song_archetype.dart`) into high-performance, deterministic C++20:
    - **Deterministic Mulberry32 PRNG** ([mulberry32_rng.hpp](file:///c:/git/eatsbits/include/eatsbits/procgen/mulberry32_rng.hpp)):
      - 32-bit bitwise multiplication and rotation algorithms ensuring reproducible compositions across identical seeds.
      - Floating-point and integer interval ranges, percentage chance probabilities, array element picking, and weighted distributions.
    - **Song Archetypes & Blueprint Registry** ([song_archetypes.hpp](file:///c:/git/eatsbits/include/eatsbits/procgen/song_archetypes.hpp), [song_archetypes.cpp](file:///c:/git/eatsbits/src/procgen/song_archetypes.cpp)):
      - Functional Roles (`Rhythm`, `Foundation`, `HarmonicTexture`, `PrimaryMelody`, `Counterpoint`) and Texture Types (`Sustained`, `Strummed`, `Arpeggiated`, `Stabs`).
      - Section form blueprints: `Intro -> Verse -> Chorus -> Breakdown -> Drop -> Outro` across variable bar lengths (16, 24, 32 bars).
      - Built-in catalog of 7 exemplar song archetypes: Acid Techno (135 BPM, C Phrygian), Synthwave (124 BPM, A Minor), Lofi Hip Hop (84 BPM, D Minor), Cyberpunk Electro (130 BPM, E Minor), Ambient Drone (68 BPM, D Dorian), SNES 16-Bit Adventure (134 BPM, C Major), and C64 SID Chiptune (138 BPM, A Minor).
    - **`ProceduralAcidEngine` (Roland TB-303 Algorithmic Generator)** ([procedural_acid_engine.hpp](file:///c:/git/eatsbits/include/eatsbits/procgen/procedural_acid_engine.hpp), [procedural_acid_engine.cpp](file:///c:/git/eatsbits/src/procgen/procedural_acid_engine.cpp)):
      - Generates 16-step patterns with 6 distinct styles (Chicago 1987, Trance/Goa, Warehouse Techno, IDM Brain-Dance, Electro Funk, Minimalist).
      - Authentic TB-303 sequencing rules: constant-rate 60ms portamento glissando (+0.05 step overlap), tied note envelope extension, dynamic accents (0.95 vs 0.68 velocity), turnaround fills, and octave leap mutations.
    - **`ProceduralDrumEngine` (Polyrhythmic 808/909 & Acoustic Kit Generator)** ([procedural_drum_engine.hpp](file:///c:/git/eatsbits/include/eatsbits/procgen/procedural_drum_engine.hpp), [procedural_drum_engine.cpp](file:///c:/git/eatsbits/src/procgen/procedural_drum_engine.cpp)):
      - 8 genre groove algorithms: House/Disco (4-on-the-floor), Hip-Hop/Boom-Bap, Funk/Breakbeat, Trap/Halftime, Rock/Pop, Jazz/Swing, 16-Bit Console, and 8-Bit Chiptune.
      - Micro-timing swing jitter, velocity humanization, ghost notes, turnaround tom/snare rolls, and crash accents mapped to GM standard drum kit notes.
    - **`ProceduralPianoEngine` (Harmonic Voice Leading & Accompaniment)** ([procedural_piano_engine.hpp](file:///c:/git/eatsbits/include/eatsbits/procgen/procedural_piano_engine.hpp), [procedural_piano_engine.cpp](file:///c:/git/eatsbits/src/procgen/procedural_piano_engine.cpp)):
      - Voice leading algorithm minimizing soprano motion between chord transitions.
      - Neo-Soul 9th/11th jazz voicings, strum rubato spread (15-30ms offset), syncopated comping, Alberti bass, and cascading arpeggiators.
    - **`ProceduralEnsembleEngine` (Multi-Track Orchestration)** ([procedural_ensemble_engine.hpp](file:///c:/git/eatsbits/include/eatsbits/procgen/procedural_ensemble_engine.hpp), [procedural_ensemble_engine.cpp](file:///c:/git/eatsbits/src/procgen/procedural_ensemble_engine.cpp)):
      - Simultaneously renders Drums, Bassline, Harmonic Textures, Lead Melody, and Counterpoint across section plans.
      - Enforces global chord track consistency and section dynamic energies (tacit instrument rests in Intro/Outro).
    - **`ProceduralSongEngine` & Workstation Integration** ([procedural_song_engine.hpp](file:///c:/git/eatsbits/include/eatsbits/procgen/procedural_song_engine.hpp), [procedural_song_engine.cpp](file:///c:/git/eatsbits/src/procgen/procedural_song_engine.cpp)):
      - Top-level arrangement generator populating the Arranger timeline (`ArrangerTimelineTrack` and `ArrangerTimelineClip`), the Chord Track (`theory::ChordEvent`), and the `StepSequencer`.
      - Connected into `MacroRuntime` (`eat.daw.generate_song(style, bars, seed)`), `CommandPaletteDialog` (`macro.procedural_song` via `Ctrl+Shift+G`), and `ProjectBrowserDrawer` ("Scripts & Macros" tab).
- **Validation**:
  - Dedicated test suite [test_procgen.cpp](file:///c:/git/eatsbits/tests/test_procgen.cpp) covering 8 automated test suites verifying determinism, registry search, acid sequencing, drum grooves, piano voice leading, ensemble orchestration, song generation, and UI/macro integration.
  - 100% test pass rate across all 32 test suites in the Eatsbits project.

### 4.15 Subsystem 3: Procedural Impulse Response (IR) Generator & Acoustic Space Physics (`ProceduralIRGenerator`, `ConvolverNode`, `ConvolverCore`)
- **Full Eatsbeats Subsystem 3 Parity**:
  - Ported and enhanced the entire procedural impulse response generator from `c:\git\eatsbeats` (`lib/audio/procedural_ir_generator.dart` and `test/procedural_ir_and_cabinet_test.dart`) into high-performance, deterministic C++20:
    - **Acoustic Materials & Frequency-Dependent Absorption** ([procedural_ir_generator.hpp](file:///c:/git/eatsbits/include/eatsbits/audio/procedural_ir_generator.hpp), [procedural_ir_generator.cpp](file:///c:/git/eatsbits/src/audio/procedural_ir_generator.cpp)):
      - Physical boundaries for Birch Plywood, Pine Wood, Acoustic Foam, Hard Concrete/Marble, Wood Paneling, Heavy Velvet Drapes, Sheet Metal, and Carpet.
      - Multiband absorption coefficients ($\alpha_{\text{low}}$ 125-250 Hz, $\alpha_{\text{mid}}$ 500-1000 Hz, $\alpha_{\text{high}}$ 2000-4000 Hz) and scattering diffusion coefficients.
    - **3D Image Source Method (ISM) & Velvet Noise Reverb Synthesis**:
      - 3D virtual image reflections (order up to 3) modeling boundary geometry, spatial distance falloff ($1 / (d + 1)$), and diffusion jitter scattering.
      - Interaural Time Difference (ITD) direct arrival spikes for stereo binaural realism.
      - Dual-seed decorrelated velvet noise stochastic late diffuse tail (left seed 1337, right seed 7331) with material frequency damping.
    - **Non-Linear & Gated Reverbs**:
      - 80s Phil Collins non-linear gated chamber: sustained plate density with sharp cosine gate cutoff after configurable hold and release times.
      - Reverse snare reverb: exponential energy rise swell for explosive drum impacts.
    - **Speaker Cabinet & Enclosure Simulation**:
      - Modal enclosure bounce reflections (width, length, height dimensions).
      - Open-back combo cabinet rear dipole phase cancellation ($180^\circ$ inverted path).
      - 3-stage digital biquad filter chain: 2nd-order high-pass resonant bump, peaking presence EQ at 3.2 kHz, and 2nd-order low-pass speaker cone treble roll-off with off-axis angular damping.
    - **Comprehensive Preset Catalog (16 Stock Presets)**:
      - Acoustic Spaces: Stone Cathedral, Great Hall, Studio Live Room, Warm Room, Small Vocal Booth, Tile Bathroom, Plate Reverb, Spring Tank.
      - Non-Linear Spaces: 80s Gated Chamber, Non-Linear Reverse Snare.
      - Amp Cabinets: 4x12 Vintage Stack (Closed), 2x12 British Celestion, 1x12 Tweed Combo (Open-Back), Bass 8x10 Fridge, Small Radio Speaker, Acoustic Resonator Box.
    - **Convolver Node & Real-Time Core Integration** ([convolver_core.hpp](file:///c:/git/eatsbits/include/eatsbits/audio/dsp/convolver_core.hpp), [convolver_node.hpp](file:///c:/git/eatsbits/include/eatsbits/audio/graph/nodes/convolver_node.hpp)):
      - Real-time procedural parameter updates directly on `ConvolverNode`: `setRoomSize()`, `setDamping()`, `setGated()`, `setAcousticSpace()`, `bakeCustomSpace()`.
      - Click-free 128-sample crossfading when switching spaces or dynamically tweaking dimensions without audio thread allocation.
- **Validation**:
  - Expanded test suite [test_convolver.cpp](file:///c:/git/eatsbits/tests/test_convolver.cpp) covering 8 automated test suites verifying all 16 presets, gated sharp cutoff, reverse swell, multiband damping, diffusion scattering, cabinet open-back dipole, Dirac impulse convolution, 10ms pre-delay accuracy, dynamic parameters, and AudioGraph execution.
  - 100% test pass rate across all 32 test suites in the Eatsbits project.

### 4.16 Subsystem 4: SoundFont 2 (SF2) Sample Engine (`SoundFontDecoder`, `SoundFontNode`, `SoundFontData`)
- **Full Eatsbeats Subsystem 4 Parity**:
  - Ported and enhanced the entire SoundFont 2 parser and polyphonic sample player from `c:\git\eatsbeats` (`lib/audio/soundfont_decoder.dart`, `soundfont_engine.dart`, `soundfont_test.dart`):
    - **RIFF Chunk Hydrag Parser** ([soundfont_decoder.hpp](file:///c:/git/eatsbits/include/eatsbits/audio/soundfont/soundfont_decoder.hpp), [soundfont_decoder.cpp](file:///c:/git/eatsbits/src/audio/soundfont/soundfont_decoder.cpp)):
      - Robust RIFF parsing for SF2 soundbanks (`sfbk` form type).
      - Extracts 16-bit signed PCM from `sdta`/`smpl` into 32-bit normalized floats.
      - Full hydrag chunk reader: `phdr`, `pbag`, `pgen`, `inst`, `ibag`, `igen`, and `shdr`.
      - SoundFont 2.04 generator hierarchy assembly: key/velocity range intersections (gens 43, 44), tuning offsets (gens 51, 52, 58), pan (gen 17), sample modes (gen 54), loop offsets (gens 2, 3, 45, 50), and volume envelope timecents (gens 33-38).
      - 128 General MIDI standard instrument names lookup and formatted display names.
    - **Multitimbral Modular Sample Player Node** ([soundfont_node.hpp](file:///c:/git/eatsbits/include/eatsbits/audio/graph/nodes/soundfont_node.hpp), [soundfont_node.cpp](file:///c:/git/eatsbits/src/audio/graph/nodes/soundfont_node.cpp)):
      - 32-voice polyphonic real-time audio thread rendering with zero heap allocation.
      - Fractional sample pitch shifting with linear interpolation.
      - Continuous loop point wrapping (`startLoop` to `endLoop`) and anti-click boundary de-clicking.
      - Per-zone ADSR volume envelope calculation (Delay, Attack, Hold, Decay, Sustain, Release).
      - Dynamic voice stealing, master gain, and pitch bend control.
- **Validation**:
  - Dedicated test suite [test_soundfont.cpp](file:///c:/git/eatsbits/tests/test_soundfont.cpp) covering 7 automated tests (header validation, bundled `super_small_font.sf2` decoding, General MIDI names, real-time audio playback, 32-voice chords, AudioGraph event routing, and 10s benchmark with $160\times$ real-time speedup).
  - 100% test pass rate across all 33 test suites in the Eatsbits project.

### 4.17 Subsystem 5: Track Freeze & Background Audio Bouncing (`TrackFreezeEngine`, `SequencerTrack`, `AudioEngine`)
- **Full Eatsbeats Subsystem 5 Parity**:
  - Ported and enhanced the entire Track Freeze and offline bouncing system from `c:\git\eatsbeats` (`lib/audio/track_freeze_engine.dart`, `models/track_model.dart`, `test/track_freeze_test.dart`):
    - **Deterministic 64-bit FNV-1a Track Hash** ([track_freeze_engine.hpp](file:///c:/git/eatsbits/include/eatsbits/audio/track_freeze_engine.hpp), [track_freeze_engine.cpp](file:///c:/git/eatsbits/src/audio/track_freeze_engine.cpp)):
      - 64-bit FNV-1a hash algorithm formatted as a 16-hex-digit string (`computeTrackHash`).
      - Deeply hashes track ID, name, icon reference, target node ID, step sequence properties (notes, velocity, gate, slide, accent, probability, parameter locks, chords), EatScript code, connected graph nodes, BPM, and sample rate.
      - Invalidation detection: `isFreezeValid` ensures baked audio matches the current track state.
    - **Offline Fast Synthesis & Audio Bouncing**:
      - `renderTrackOffline`: Synthesizes individual tracks offline into contiguous stereo Float32 memory buffers.
      - Track isolation: automatically mutes other tracks during bake and evaluates the entire signal chain through master or stems.
      - Peak limiting & normalization: automatic soft peak attenuation when audio exceeds configurable peak ceiling (`peakLimit`).
      - Non-blocking progress reporting via `FreezeProgressCallback`.
      - Render speeds exceeding $40\times - 50\times$ faster than real-time.
      - Direct WAV file bouncing (`bounceTrackToWav`) and asynchronous background baking (`renderTrackAsync`).
    - **Zero-CPU Dynamic DSP Bypass & Real-Time Playback**:
      - `SequencerTrack`: Holds pre-rendered stereo audio buffers (`frozenBufferL`, `frozenBufferR`), content hash, and sample rate.
      - `StepSequencer`: In `processBlock`, skips live note dispatch and voice allocation for frozen tracks.
      - `AudioGraph`: In `process`, detects `!node->isEnabled()`, zeroes output buffers, and completely skips `processBlock`, reclaiming 100% CPU.
      - Real-time streaming: `mixFrozenTracks` sample-accurately streams baked audio aligned with transport clock without any real-time memory allocations.
    - **UI & Studio Window Integration**:
      - Integrated `setTrackFreezeState` in [gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp).
      - Wired `onFreezeToggled` in `TrackInspectorView`, `TrackPropertiesPanel`, Arranger track headers, and Mixer channel strips.
      - Project file persistence in [project_file.cpp](file:///c:/git/eatsbits/src/project/project_file.cpp): preserves `isFrozen` and `frozenContentHash` across save and load.
- **Validation**:
  - Dedicated test suite [test_track_freeze.cpp](file:///c:/git/eatsbits/tests/test_track_freeze.cpp) covering 6 comprehensive automated tests (deterministic hash, offline rendering, freeze/unfreeze lifecycle, real-time frozen streaming with mute handling, project JSON serialization, and WAV export).
### 4.18 Subsystem 6: Specialized Creative UI Dialogs & Scopes (`WaveshaperDialog`, `NoteSplitterEngine`, `ColorPickerDialog`, `SpaceVisualizerWidget`)
- **Full Eatsbeats Subsystem 6 Parity**:
  - Ported and enhanced all specialized creative visual widgets and modal dialogs from `c:\git\eatsbeats`:
    - **Interactive Waveshaper Transfer Curve Editor** ([waveshaper_dialog.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/waveshaper_dialog.hpp), [waveshaper_dialog.cpp](file:///c:/git/eatsbits/src/ui/widgets/waveshaper_dialog.cpp)):
      - 5 non-linear distortion curves: Soft Saturation (Tanh), Asymmetric Tube overdrive, Sine Wavefolder, Angry 1 multi-fold, and Angry 2 crunch.
      - Draggable curve tension canvas with visual spline node control.
      - Real-time 8-band harmonic spectrum analyzer computing harmonics 1 through 8 via discrete Fourier probe.
      - Pre-gain (drive), post-gain (level), DC highpass filter, dry/wet blending, and 1x/2x/4x oversampling toggles.
    - **Note & Chord Voice Splitter Engine & Dialog** ([note_splitter_engine.hpp](file:///c:/git/eatsbits/include/eatsbits/sequencer/note_splitter_engine.hpp), [note_splitter_engine.cpp](file:///c:/git/eatsbits/src/sequencer/note_splitter_engine.cpp), [note_splitter_dialog.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/note_splitter_dialog.hpp), [note_splitter_dialog.cpp](file:///c:/git/eatsbits/src/ui/widgets/note_splitter_dialog.cpp)):
      - Algorithmic polyphony distributor with 4 split modes:
        1. *3-Way Voice*: Separates Bassline, Chords/Harmony, and Skyline Lead melody.
        2. *Piano Clefs*: Splits left hand (< pivot) and right hand (>= pivot) at Middle C.
        3. *4-Voice SATB*: Chronologically clusters and distributes notes into Soprano, Alto, Tenor, and Bass stems.
        4. *Drum Demuxer*: Automatically demuxes General MIDI drum tracks into Kick, Snare & Claps, Hi-Hats & Cymbals, and Percussion/Toms.
      - Directly applies splits to `StepSequencer`, creating designated colored tracks with assigned presets.
    - **Studio Track & Clip Color Picker** ([color_picker_dialog.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/color_picker_dialog.hpp), [color_picker_dialog.cpp](file:///c:/git/eatsbits/src/ui/widgets/color_picker_dialog.cpp)):
      - 4 curated studio palettes: Neon & Cyberpunk, Classic Synth & Studio, Vibrant Palette, and Pastels & Subtle.
      - Interactive HSL sliders (Hue 0-360, Saturation 0-100%, Lightness 0-100%) with bidirectional RGB $\leftrightarrow$ HSL math.
      - Hex code display (`#RRGGBB`) and Before/After preview swatches.
    - **Stereo Field Scope & Lissajous Goniometer** ([space_visualizer_widget.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/space_visualizer_widget.hpp), [space_visualizer_widget.cpp](file:///c:/git/eatsbits/src/ui/widgets/space_visualizer_widget.cpp)):
      - Dual-mode visualization:
        1. *Real-Time Lissajous Goniometer*: $45^\circ$ rotated polar scope plotting Mid ($Y$) vs Side ($X$), phase correlation meter ($-1.0 \leftrightarrow +1.0$), L/R balance, and M/S ratio.
        2. *2.5D Acoustic Room Geometry*: Perspective room vanishing box with draggable Sound Source and Listener icons, direct sound rays, and wall reflection bounce paths.
      - Zero-allocation circular ring buffer for real-time live stereo audio feeding.
- **Validation**:
  - Dedicated test suite [test_creative_dialogs.cpp](file:///c:/git/eatsbits/tests/test_creative_dialogs.cpp) covering 7 automated tests (transfer curve evaluation, 3-way skyline separation, piano clef & SATB polyphony, drum demuxer, step sequencer integration, color picker HSL roundtrip, and Lissajous goniometer phase metrics).
### 4.19 Subsystem 7: AI Services & Intelligent DAW Assistant (`GeminiClient`, `AiMixingEngine`, `AiAssistantDialog`)
- **Full Eatsbeats Subsystem 7 Parity**:
  - Ported and enhanced the entire AI services and intelligent DAW assistant pipeline from `c:\git\eatsbeats` (`lib/ai/gemini_service.dart`, `lib/ai/ai_mixing_engine.dart`, `lib/ui/dialogs/ai_assistant_dialog.dart`, `lib/ui/dialogs/ai_mixing_dialog.dart`):
    - **Modern Shared JSON Engine** ([json_parser.hpp](file:///c:/git/eatsbits/include/eatsbits/project/json_parser.hpp)):
      - Clean, self-contained recursive-descent JSON parser and serializer with strongly-typed `json::Value`, `json::Object`, and `json::Array`.
      - Supports string escapes, numeric parsing, null, booleans, nested arrays, and objects.
      - Refactored [project_file.cpp](file:///c:/git/eatsbits/src/project/project_file.cpp) to eliminate ad-hoc parsing and leverage the unified parser.
    - **Google Gemini API Client Bridge** ([gemini_client.hpp](file:///c:/git/eatsbits/include/eatsbits/ai/gemini_client.hpp), [gemini_client.cpp](file:///c:/git/eatsbits/src/ai/gemini_client.cpp)):
      - BYOK (Bring Your Own Key) architecture with persistent local config and `GEMINI_API_KEY` environment variable discovery.
      - Native synchronous and asynchronous HTTPS REST client leveraging Windows WinHTTP (`winhttp.dll`).
      - Deterministic offline mock engine providing immediate responses without internet or API keys:
        - EatScript DSL code generation for synthesizers, drum machines, and audio effects.
        - Song blueprint generator parsing structured JSON for genre, tempo, key, scale, chords, and multi-track stems.
        - Natural language DAW chat completions and prompt history tracking.
    - **Intelligent AI Auto-Mixing Engine** ([ai_mixing_engine.hpp](file:///c:/git/eatsbits/include/eatsbits/ai/ai_mixing_engine.hpp), [ai_mixing_engine.cpp](file:///c:/git/eatsbits/src/ai/ai_mixing_engine.cpp)):
      - Mix telemetry extraction: inspects all `StepSequencer` tracks, active steps, polyphony, notes, and automations.
      - Instrument heuristic classification: automatically identifies Drums/Percussion, Bass/Sub, Leads, Chords/Pads, and Vocals.
      - Algorithmic gain-staging and spectral unmasking:
        - Target LUFS calibration (-14 LUFS streaming, -9 LUFS club/EDM, -18 LUFS dynamic acoustic).
        - Bass/Kick mono anchoring with sub-bass headroom protection.
        - Stereo width expansion for backing pads and textures.
        - High-pass filtering (HPF) on melodic stems (120-160 Hz) to eliminate muddy low-end buildup.
        - Master limiter ceiling (-0.3 dBFS) and 28 Hz subsonic roll-off.
      - Two execution modes:
        1. Cloud-powered Gemini LLM reasoning analyzing telemetry JSON and emitting mix adjustment patches.
        2. Fast local heuristic fallback engine that calculates complete studio mix patches offline in under 1 ms.
      - Direct patch applicator: modifies track volume, pan, EQ filters, and master bus parameters in `StepSequencer`.
    - **AI Assistant Chat & Mix Dialog** ([ai_assistant_dialog.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/ai_assistant_dialog.hpp), [ai_assistant_dialog.cpp](file:///c:/git/eatsbits/src/ui/widgets/ai_assistant_dialog.cpp)):
      - Modular tabbed interface:
        1. *Compose*: Generates songs, chords, patterns, and EatScript code with an "Apply to DAW" button.
        2. *Sound Design*: Generates synthesizer presets, FM algorithms, and filter configurations with code preview.
        3. *Auto-Mix & Master*: Configures target genre, loudness LUFS, and tonal character, triggers mix extraction, and displays per-track adjustment summaries.
        4. *Settings*: API key configuration, model selection (`gemini-2.5-flash`, `gemini-1.5-pro`), and mock mode toggle.
      - Rendered with modern studio aesthetics, tabs, responsive message history bubbles, and command prompt field.
### 4.20 Subsystem 8: Synchronized Lyric Track & Speech Vocalizer (`LyricTrack`, `LrcParser`, `TtsSynthNode`)
- **Full Eatsbeats Subsystem 8 Parity**:
  - Ported and enhanced the entire lyrics and speech vocalizer pipeline from `c:\git\eatsbeats` (`lib/models/lyric_model.dart`, `lib/audio/tts_engine.dart`, `presets/instruments/tts_voice_synth.eats`, `test/lyric_test.dart`):
    - **Lyric Marker Model & Timeline Track** ([lyric_track.hpp](file:///c:/git/eatsbits/include/eatsbits/lyrics/lyric_track.hpp), [lyric_track.cpp](file:///c:/git/eatsbits/src/lyrics/lyric_track.cpp)):
      - `LyricCue`: Represents a time-synchronized syllable, word, or phrase event with musical start step, duration in 16th steps, text, optional phonetic override, pitch multiplier, and rate multiplier.
      - Strong JSON serialization and deserialization via the shared `json_parser.hpp`.
      - High-precision `LrcParser`:
        - Parses standard line-synced `[mm:ss.xx] lyric text` LRC files with tempo-to-step mapping at given BPM.
        - Parses Enhanced word-synced `<mm:ss.xx> word <mm:ss.xx> word2` LRC files with sub-beat word duration interpolation.
        - Exports cues back to standard LRC string format with millisecond accuracy.
      - `LyricTrack`: Timeline container with chronological sorting, step-accurate lookup (`getCueAtStep`), and range windowing (`getCuesInRange`).
      - Sequencer integration: Added `lyric` field to `StepData` and `lyrics_` list to `SequencerTrack`, persisted via `ProjectFile` JSON save/load.
    - **Speech & Vocal Formant Synthesizer Node** ([tts_synth_node.hpp](file:///c:/git/eatsbits/include/eatsbits/audio/graph/nodes/tts_synth_node.hpp), [tts_synth_node.cpp](file:///c:/git/eatsbits/src/audio/graph/nodes/tts_synth_node.cpp)):
      - Physical acoustic vocal tract model with 3 resonant state-variable bandpass formant filters ($F_1, F_2, F_3$).
      - Glottal pulse oscillator with variable prosody and subtle human vibrato (5.5 Hz, Natural mode).
      - 3 vocal profiles:
        1. *Natural*: Singing voice with glottal pulse, formant resonance, and human vibrato.
        2. *Robot*: Quantized pulse carrier with ring-modulated harmonics.
        3. *Whisper*: 100% unvoiced filtered turbulence noise through vocal tract resonators.
      - Real-time zero-allocation processing loop (`processBlock`).
      - Syllable speech queuing with smooth formant interpolation.
### 4.21 Subsystem 9: Extended Physical Modeling Preset Library (`BuiltinPresetRegistry`, `PresetLoader`)
- **Full Eatsbeats Subsystem 9 Parity**:
  - Ported and enhanced the complete physical modeling preset library and EatScript catalog from `c:\git\eatsbeats` (`lib/eatscript/eats_builtin_presets.g.dart`, `presets/`):
    - **Physical Modeling Instrument Catalog** ([eats_builtin_presets.hpp](file:///c:/git/eatsbits/include/eatsbits/project/eats_builtin_presets.hpp), [eats_builtin_presets.cpp](file:///c:/git/eatsbits/src/project/eats_builtin_presets.cpp)):
      - Comprehensive registry with 33 registered presets organized into 8 physical modeling families:
        1. *Keyboards*: Concert Grand Piano, Felt Upright Piano, Honky-Tonk Piano, Harpsichord Cembalo, Clavinet D6.
        2. *Plucked Strings*: Spanish Nylon Guitar, Steel Acoustic Guitar, 12-String Acoustic Guitar, Renaissance Lute, Bluegrass Banjo, Folk Mandolin, Hawaiian Ukulele, Dobro Resonator.
        3. *Bowed Strings*: Solo Violin, Solo Viola, Solo Cello, Orchestral Double Bass, String Ensemble.
        4. *Bass*: Acoustic Upright Bass, Fretless Bass.
        5. *Wind & Organ*: Church Pipe Organ with flue and reed pipes.
        6. *Tuned Percussion & Bells*: Concert Glockenspiel, Tubular Bells, Concert Vibraphone, Orchestral Xylophone, Vintage Music Box, Latin Agogo Bell.
        7. *Vocal & Speech*: TTS Voice Synth with 3-column ceramic hardware layout.
        8. *Electronic Synths*: Eats-303, C64 SID, Yamaha DX7, SNES S-DSP, Sega Genesis YM2612.
      - Canonical ID lookup (`createPresetById`), family grouping (`getPresetsByFamily`), and category queries (`getPresetsByCategory`).
      - Each instrument generates a full declarative hardware GUI layout tree (`guiRoot`, `compactGuiRoot`) with styled knobs, sliders, meters, panels, and parameter metadata.
    - **Filesystem `.eats` Preset Library**:
      - Copied all 131 `.eats` presets across all 6 categories (`instruments`, `audio_fx`, `drums`, `midi_fx`, `midi_seq`, `utility`) into `presets/`.
      - Implemented recursive directory loading in `PresetLoader::loadDirectory` with C++20 `std::filesystem`.
- **Validation**:
  - Dedicated test suite [test_builtin_presets.cpp](file:///c:/git/eatsbits/tests/test_builtin_presets.cpp) covering 6 comprehensive automated tests (catalog metadata, physical modeling instantiation, family filtering, canonical ID lookup, hardware GUI bounds computation, and filesystem directory loading).
  - 100% test pass rate across all 38 test suites in the Eatsbits project.

### 4.22 Cross-Platform File Drag and Drop Subsystem (`glfwSetDropCallback`, HTML5 Drag & Drop MEMFS Bridge)
- **Cross-Platform Ingestion**:
  - **Desktop (GLFW)**: Hooked `glfwSetDropCallback` in `GuiWindow::initialize` with coordinate conversion via `windowToLogicalX/Y`, CRT curvature remapping, and 3D console matrices.
  - **Web (Emscripten / HTML5 Drag & Drop)**: Embedded drag-and-drop listener on `#canvas` via `EM_ASM`, writing dropped files asynchronously into Emscripten MEMFS (`/tmp/<filename>`) and dispatching to exported C++ `eats_on_file_dropped_web`.
- **Modular Dispatch & UI Routing**:
  - Extended `ViewBase` interface with `virtual bool handleFileDrop(const std::vector<std::string>& filePaths, float x, float y, const ViewContext& ctx)`.
  - Implemented `ArrangerView::handleFileDrop`:
    - Bar and track hit-testing: calculates target bar from horizontal drop coordinate and track lane index from vertical position.
    - Dropping audio (`.wav`, `.mp3`, `.ogg`, `.flac`, `.aif`, `.sf2`) on existing track adds an audio clip; dropping below tracks automatically creates a new Sampler track with the clip.
    - Dropping MIDI (`.mid`, `.midi`) imports melodic/drum clips at target timeline bar.
  - Contextual modal dialog interception: `AudioToMidiDialog::loadAudioFile` handles direct drop when the transcription modal is open.
  - Global fallback: automatically opens `.eats` / `.json` projects, imports samples/MIDI, and loads `.eatscript`.
- **Validation**:
  - Verified across 15 modular UI/UX tests in [test_modular_ui.cpp](file:///c:/git/eatsbits/tests/test_modular_ui.cpp) (`testFileDragAndDrop`), including track placement, bar calculation, new sampler track creation, dialog routing, and window integration hooks.

### 4.23 CRT Shader Curved Reflection Mirroring & Reverse Bezel Vignette
- **Diagnosis of Deviation Away from Center/Middle**:
  - The CRT active screen applies non-linear polynomial bulb curvature (`curvedTube`) and horizontal sync scan wave (`hWave`).
  - Previously, the bezel frame sampled screen content using uncurved coordinates (`tubeRel`) without `hWave`, causing reflections to drift away from the phosphor raster as coordinates moved away from the screen center.
  - Furthermore, using axis-aligned rectilinear checks (`tubeRel.x < 0.0`) failed inside the rounded corners because the corner arc of the CRT screen is curved inward (`cornerRadius_px = 20.0`), leaving corner pixels with positive coordinates where axis-aligned mirroring never triggered.
- **Normal-Based Curved Reflection Mirroring**:
  - Derived the analytical surface normal `normPixel` of the rounded CRT bezel aperture across both flat edges and radial corner quadrants (`normalize(q) * sgn`).
  - Mirrored frame coordinates continuously across the curved bezel boundary along the surface normal: `reflPixel = currentPixel - 2.0 * dTube * normPixel`.
  - Applied the identical polynomial CRT bulb curvature and `hWave` modulation to the reflected sample coordinate: guaranteed 1:1 pixel alignment across the entire perimeter, center, edges, and rounded corners.
  - Aligned environmental room reflection (`sampleFrostedReflection`) to sample the curved reflection space seamlessly.
- **Reverse Bezel Vignette & Rounded Corner Shading**:
  - Inverted the bezel cavity slope so the frame darkens as it progresses inward toward the CRT aperture junction (`dTube -> 0`), sinking into the recessed shadow cavity.
  - Evaluated the 2D conformal vignette (`vig.x * vig.y`) across the frame: along flat edges, the reverse vignette provides smooth bevel recession, while in the rounded corners both X and Y components converge, automatically deepening the vignette in the rounded corners.
  - Preserved the 2px chassis edge gap (`distToPanelEdge < 2.0px`) separating the bezel flush from the transport panel, bottom chin, and window borders with subtle outer glint.
- **Validation**:
  - Verified across `test_gui_interaction.exe` (Phase 6 GUI test suite) running on Dawn WebGPU / NVIDIA GeForce GTX 1660 Ti hardware pipeline with zero validation panics.

### 4.24 Hardware Panel Softness, Desaturation & Real-Time CRT Uniforms Tweaker HUD
- **Physical Panel Realism Enhancements**:
  - **Sub-Pixel 5-Tap Cross Blur (`panelSoftness`, default 0.75px)**: Softens the top transport header and bottom navigation chin in the CRT pass, eliminating razor-sharp vector anti-aliasing artifacts and blending GUI elements into the chassis plastic.
  - **Hardware Desaturation (`panelSaturation`, default 0.70)**: Reduces raw digital panel saturation to 70% using luminance weighting (`dot(col, vec3(0.299, 0.587, 0.114))`), matching analog broadcast hardware panels.
  - **Black Floor Lift (`panelBlackLift`, default +0.025)**: Lifts pitch-black UI borders and dividers (`col + panelBlackLift`), removing harsh digital contrast and simulating physical matte polycarbonate plastics.
- **Dynamic Shader Uniform Layout (96 Bytes)**:
  - Synchronized [crt_screen.wgsl](file:///c:/git/eatsbits/assets/shaders/crt_screen.wgsl), [dawn_bridge.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/dawn_bridge.hpp), and [dawn_bridge.cpp](file:///c:/git/eatsbits/src/ui/dawn_bridge.cpp) with strict 16-byte alignment (`static_assert(sizeof(GpuCrtUniforms) == 96)`).
  - Added uniform controls: `spotlightSize`, `spotlightIntensity` (0.0 = uniform flat light), `curvature`, `scanlineIntensity` (0.0 = completely removed for crisp text clarity), `frameReflectLevel`, `vignetteLevel`, `panelSoftness`, `panelSaturation`, and `panelBlackLift`.
- **Interactive CRT Shader & Chassis Tweaker HUD**:
  - **Access & Hotkeys**: Openable via **`F10`**, **`Shift+F9`**, or the **`[TWEAK HUD]`** button in Project Hub Settings (Alt+F). Dismissable with **`ESC`**, `[X]`, or clicking outside.
  - **Live Calibration Controls**: 9 real-time interactive sliders with dynamic numeric readouts, immediate WebGPU uniform updates, and master power switch (`[CRT: ON]` / `[CRT: OFF]`).
  - **Instant Presets**:
    - `[STUDIO REF]`: Calibrated reference console (0.38 scanlines, 0.85 curvature, 0.75px blur, 70% saturation, +0.025 black lift).
    - `[MAX CLARITY]`: Flat monitor profile (0 scanlines, 0 curvature, 0 blur, 100% saturation, flat uniform lighting).
    - `[WARM VINTAGE]`: Retro arcade console (0.55 scanlines, 1.15 curvature, 1.0px blur, 65% saturation, +0.035 black lift).
    - `[RESET]`: Restores factory default settings.
- **Validation**:
  - Validated with clean compilation across MSVC Release toolchain and 100% pass on [test_gui_interaction.exe](file:///c:/git/eatsbits/tests/test_gui_interaction.cpp) running on NVIDIA GeForce GTX 1660 Ti hardware pipeline.

### 4.25 CRT Tube Reflection, H-Sync Distortion Control, Top Panel Seam Revamp & Dedicated Settings Drawer
- **CRT Reflection & H-Sync Uniform Options**:
  - **CRT Tube Room Reflection (`crtReflectionLevel`, 0.0 to 1.0, offset 88)**:
    - Scales the room and window background reflection rendered onto the curved glass bulb (`reflectionGlow = reflSample * tubeEdgeFade * (0.125 * u.crtReflectionLevel)`).
    - Setting to `0.00` completely removes the room reflection image from the CRT tube while preserving phosphor emission, bloom, and raster scanlines.
  - **Horizontal Sync Wave Distortion (`hsyncDistortion`, 0.0 to 2.0, offset 92)**:
    - Configures the horizontal raster scanline wave distortion and wobbling (`hWave = sin(...) * (u.hWaveStrength * u.hsyncDistortion)`).
    - Setting to `0.00` completely eliminates horizontal raster wobble for a rock-solid, static raster picture.
- **Top Panel Bottom Edge Drawing Revamp**:
  - Revamped the top panel's bottom lip to be smooth, clean, and uniform, matching the bottom navigation panel's top edge:
  - Eliminated procedural noise and chipped brass erosion (`hash2D`, `edgeErosion`) in [gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp) `ChassisTextureSystem`.
  - Replaced pixelated `chipGlint` in [crt_screen.wgsl](file:///c:/git/eatsbits/assets/shaders/crt_screen.wgsl) with a uniform machined recession shadow (`distPx < 3.5px`, `smoothstep`).
  - Rendered clean machined bevel lines at Y = 54.5px and Y = 56.0px matching the bottom chin's top seam.
- **Dedicated Settings Drawer for CRT Shader Calibration**:
  - Added dedicated Section 3 ("CRT SHADER & DIEGETIC CONSOLE CALIBRATION") to the Project Hub Settings modal (`Alt+F`):
    - Full vertical accordion drawer with smooth kinetic scrolling (`projectHubScrollY_`).
    - Direct master switch toggle (`[CRT: ON]` / `[CRT: OFF]`).
    - `[LIVE HUD (F10)]` button to pop open the floating HUD overlay for real-time adjustments during audio playback.
    - Quick calibration presets: `[STUDIO REF]`, `[MAX CLARITY]`, `[WARM VINTAGE]`, `[RESET]`.
    - 11 interactive real-time calibration sliders with tracks, gradients, thumbs, and live status readouts:
      1. `SCANLINE INTENSITY (0 = NONE)`
      2. `CRT BULB CURVATURE`
      3. `CRT TUBE ROOM REFLECTION (0 = NONE)`
      4. `H-SYNC WAVE DISTORTION (0 = STATIC)`
      5. `SPOTLIGHT INTENSITY`
      6. `SPOTLIGHT BEAM SIZE`
      7. `VIGNETTE CORNER FALLOFF`
      8. `BEZEL FRAME REFLECTION`
      9. `PANEL SOFTNESS (SUBPIXEL BLUR)`
      10. `PANEL HARDWARE SATURATION`
      11. `PANEL BLACK FLOOR LIFT`
  - **Constrained Drawer Viewport & Nested Scrollbar UI/UX Architecture**:
    - **Dialog Space Budget & Header Anchoring**: Calculated dynamic available height `availableDrawerH = (footerY - topContentY) - (numSections * headerStep) - headerGap` (`272.0f` within a `580px` dialog). All 6 accordion section headers remain strictly anchored within the modal frame, preventing headers from pushing below the modal footer.
    - **Nested ScrollableArea**: Configured `projectHubScrollArea_.setViewport(drawerX, drawerY, drawerW, drawerH)` for Section 3 with total content height `586.0f` (`maxScroll = 314.0f`).
    - **Constrained Scrollbar Track & Thumb**: Scrollbar pill track and thumb are strictly confined within the right gutter of the CRT Shader drawer container well (`drawerW - 32px` slider span, leaving room for the scrollbar), with active hover and dragging states.
    - **Strict Geometry Culling & Subpixel Bleed Caps**: Every control (master switch, presets, 11 sliders) is culled against drawer boundaries (`drawerY` to `drawerY + drawerH`) with 6px background caps above and below the drawer viewport to eliminate subpixel raster bleed.
- **CRT Shader Mouse Coordinate Distortion & Hit Detection Calibration**:
  - Identified and resolved the root cause of off-center and edge mouse hit detection distortion across the DAW workspace when the CRT shader is active:
    - **Exact Shader Parity**: Realigned [remapCrtMouseCoords](file:///c:/git/eatsbits/src/ui/gui_window.cpp) with [crt_screen.wgsl](file:///c:/git/eatsbits/assets/shaders/crt_screen.wgsl) and [dawn_bridge.cpp](file:///c:/git/eatsbits/src/ui/dawn_bridge.cpp).
    - **Correct Transform Order**: Evaluated tube aperture bezel inset `tubeUV = (suv - frameFrac) / (1 - 2*frameFrac)` prior to applying CRT bulb polynomial curvature `curvedTube += dCenter * pow(dist, 2.6) * (curvature * 0.08)`, strictly matching the GPU fragment pipeline.
    - **Precise Bezel Dimensions**: Replaced mismatched hardcoded magic constants (`0.004f`, `0.008f`, `0.012f`) with physical FBO-matched constants (`frameWidthX_px = 22.0f`, `frameHeightY_px = 20.0f`).
    - **HiDPI / RenderScale Synchronization**: Passed `renderScale` through to [renderCrtScene](file:///c:/git/eatsbits/src/ui/dawn_bridge.cpp) and synchronized `topCut` and `bottomCut` calculations across standard and Retina/HiDPI resolutions, preventing boundary drift between the transport bar, DAW area, and bottom chin.
    - **Dynamic Diegetic Offset Tracking**: Integrated real-time tracking of `rumbleOffset` (audio-reactive sub-bass chassis shake) and `hWave` (horizontal sync raster scan wave) into `DawnBridge`, keeping hit detection synchronized with active visual displacements.
    - **Automated Verification**: Added comprehensive unit test suite `testCrtMouseCoordinateRemapping()` in [test_gui_interaction.cpp](file:///c:/git/eatsbits/tests/test_gui_interaction.cpp) validating 1:1 rectilinear pass-through, transport/chin boundary protection, and exact center/off-center mapping.
- **Comprehensive Touch & Multi-Touch Support (Flutter Eatsbeats Parity)**:
  - **TrackPropertiesPanel Kinetic Touch Drag-to-Scroll**:
    - Integrated `KineticScroller` with 14px touch slop deadzone distinguishing intended taps on track property controls from vertical scroll gestures.
    - Implemented smooth exponential deceleration glide (`friction = 0.92f`) and inertia momentum tracking on touch release.
    - Added reactive scrollbar indicator with active dragging feedback and smooth track clamping.
  - **Piano Roll & Arranger Grid Touch Panning & Desktop Selection Parity**:
    - Preserved instant desktop mouse left-click drag marquee selection without delay.
    - On touchscreens (`PointerType::Touch`), touch-down on empty grid space begins in pending state; moving beyond 14px commits immediately to smooth kinetic 2D panning (`TouchPan`).
    - Stationary hold on touchscreens for >= 380-400ms seamlessly arms `MarqueeSelect` box selection mode with haptic visual cues, matching Eatsbeats mobile UX.
  - **VirtualKeyboardDrawer Multi-Touch Polyphony**:
    - Migrated single-touch tracking to `std::unordered_map<int, ActiveKeyTouch> activeTouches_` keyed by `ev.id`.
    - Enables simultaneous multi-finger chords, independent finger glissando, and selective note release without dropping held chords.
  - **DrumPadGridWidget Multi-Touch Matrix**:
    - Replaced single-pointer pad state with `std::unordered_map<int, int> activePadPointers_` keyed by `ev.id`.
    - Allows simultaneous multi-finger finger-drumming across the 4x4 MPC/SP-1200 pad matrix with vertical velocity dynamics per finger.
  - **MixerView Multi-Fader Simultaneous Mixing**:
    - Implemented `std::unordered_map<int, ActiveFaderSession> activeFaderSessions_` keyed by `ev.id`.
    - Supports multi-finger volume fader adjustments across channel strips and master fader simultaneously.
- **Mixer Architecture Phases 1-5: Pointer Delegation, DSP Decoupling, Audio Taper Presenters, Responsive Toolbar & Arranger Docked Bottom Mixer Drawer**:
  - **Phase 1: Pointer Delegation & Precedence Alignment**:
    - Removed legacy `hitTestMixer` interception in [gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp), delegating all mouse/pointer events directly to `modularMixerView_->handlePointer(pev, ctx)`.
    - Enforced strict sidebar drawer bounds isolation so clicks and gestures on `TrackPropertiesDrawer` never bleed or fall through into underlying mixer channels.
    - Clipped channel strip hit boundaries dynamically to `propertiesDrawer_.getPullTabBounds().x`.
  - **Phase 2: Decoupled Audio Graph DSP Nodes from Mixer Strips**:
    - Removed raw canvas module scanning in `GuiWindow::updateMixerStrips()`.
    - Locked mixer channel strips strictly 1:1 with Arranger Tracks + Busses + Master Bus, eliminating phantom channels created by internal/canvas `GainNode`s.
    - Preserved Gain nodes as utility devices and track insert FX.
  - **Phase 3: Systematized Fader Presenters & Logarithmic dB Taper**:
    - Added structured Thumb & Well hit geometry (`getMasterFaderGeometry()`, `getChannelFaderGeometry()`, `hitTestMasterFader()`, `hitTestChannelFader()`), replacing hardcoded magic numbers while preserving legacy test hit box boundaries.
    - Standardized fader dragging with audio taper dB scaling via `presenter::audio_taper`: unity 0 dB at 0.75 travel (1.0f gain), +6 dB at 1.0 travel (up to 1.5f max gain), cubic logarithmic taper down to $-\infty$ at 0.0 travel.
    - Added double-click detection (<350ms) to reset fader directly to 0 dB unity gain (1.0f).
    - Added Shift+Drag fine trim scaling (0.15 ratio) for precise 0.1 dB trimming.
    - Standardized manual value edit entry dialogs to 1.0f (0 dB unity) default.
    - Added calibrated dB tick marks (`+6`, `0`, `-6`, `-12`, `-inf`) along fader slots and real-time formatted dB tooltips.
  - **Phase 4: Mobile Responsive Density & Section Options Toolbar**:
    - Introduced `MixerDensityMode` with three modular layouts: `Comfortable` (140px strip), `Compact` (78px strip), and `Micro` (50px strip).
    - Implemented a collapsible top options toolbar with interactive toggle pills:
      - Density Mode selectors: `[COMFORT]`, `[COMPACT]`, `[MICRO]`.
      - Modular section toggles: `[METERS]`, `[ROUTING]`, `[PAN]`, `[READOUT]`.
      - Master channel position toggle: `[MST: LEFT]` / `[MST: RIGHT]`.
      - Toolbar collapse toggle: `[^ HIDE]` and non-intrusive `[OPTS v]` trigger pill when collapsed.
    - Added intelligent responsive auto-density switching: automatically sets `Micro` when `ctx.isMobile || bounds.w < 600px` (allowing 5-6 channels to cleanly fit on mobile screens without horizontal congestion) and `Compact` when `bounds.w < 850px`.
    - Systematized channel strip element layout, scaling LED meters, LCD displays, pan dials, button columns (stacked, inline, and micro variants), and pointer hit boundaries across all density modes while strictly preserving backwards compatibility with legacy coordinates.
  - **Phase 5: Arranger Docked Bottom Mixer Drawer**:
    - Created `ArrangerMixerDrawer` widget (`include/eatsbits/ui/widgets/arranger_mixer_drawer.hpp`, `src/ui/widgets/arranger_mixer_drawer.cpp`) adhering to the modular sliding drawer pattern with exponential damping (`kDamping = 18.0f`).
    - Fixed Master Channel Strip pinned on the left with amber LCD badge, Mute button, audio-taper calibrated fader (unity 0 dB at 0.75, +6 dB at 1.0), and dual stereo peak meter.
    - Viewport scissored channel strips with smooth horizontal scrolling via mouse wheel and drag scrollbar, keeping arranger channels aligned and mixed without navigating away from the composition timeline.
    - Per-channel track color header pill, pan dial with center detent, mute & solo buttons, vertical fader well & thumb, stereo peak meter, and bottom dB readout.
    - Resizable top edge handle (`kMinHeight = 140px`, `kMaxHeight = 340px`) and collapsible pull tab with tune sliders icon and active status indicator.
    - Toggled via hotkey `M`, Command Palette (`view.drawer.arranger_mixer`), or pull-tab click; Escape dismisses drawer.
    - Added comprehensive unit and integration suite in `tests/test_modular_ui.cpp` (Test 18) validating drawer layout, animation states, rendering, pointer routing, hotkey actions, and master control hooks.
- **PluginSearchDialog Refinements (Scroll Scissoring, Real-Time Tokenized Filtering, & Interactive Text Box)**:
  - **Vertical Scissor Containment**:
    - Confined list rendering within `[listY, listY + listH]` using GPU scissoring (`r.pushScissor(dialogBounds_.x + 18.0f, listY, cardW + 4.0f, listH)` and `r.popScissor()`).
    - Fixed card bleed over top headers, category chips, search bar, and dialog bottom margins.
    - Clamped scrolling bounds to valid range `[0, maxScroll]` and strictly isolated pointer hits so scrolled-out cards cannot receive click events.
  - **Interactive Filter Text Box**:
    - Implemented focus states with glowing accent outline, blinking vertical cursor line, and scissored text rendering.
    - Integrated clear `[X]` button when search query is active.
    - Implemented whitespace-tokenized search matching across card `name`, `category`, `engineTag`, `description`, `author`, and `id`.
    - Added empty state card feedback ("No plugins match '<query>'") with clear query action hint when 0 matches found.
    - Added real-time match count pill in dialog header (e.g. `[12 PRESETS]` or `[0 MATCHES]`).
  - **Keyboard & Char Input Integration**:
    - Wired `handleChar(char32_t codepoint)` and `handleKey` for character input, caret navigation (`Left`/`Right`/`Home`/`End`), deletion (`Backspace`/`Delete`), clipboard paste (`Ctrl+V`), and select all (`Ctrl+A`).
    - Escape key clears search query first if non-empty, and dismisses dialog on second press.
    - Enter key commits active selection; Up/Down arrow keys navigate filtered items.
    - Forwarded character input through `ArrangerView`, `MixerView`, and `TrackInspectorView` up to `GuiWindow::onChar` and `GuiWindow::onKeyDown`.
  - **Automated Verification**:
    - Added comprehensive unit tests in `tests/test_modular_ui.cpp` (Test 19) verifying tokenized search, character input, backspace, escape behavior, selection navigation, scrolling containment, and outside click dismissal.
- **Header Transport & Track Properties Tooltip System**:
  - **Shared Badge Renderer (`drawTooltipBadge`)**:
    - Created `drawTooltipBadge` utility in `include/eatsbits/ui/draw_utils.hpp` and `src/ui/draw_utils.cpp`.
    - Features soft drop shadows, theme-aware high contrast styling (light-mode solid dark badge with clean white vector text, dark-mode creamy studio label tape badge with deep dark charcoal text), boundary clamping against screen edges, and configurable placement (above or below anchor point).
  - **Header Transport Tooltips**:
    - Supported hover tips for Logo ("Project Hub & Global Workspace Settings"), Play/Pause ("Play / Pause Playhead (Space)"), Stop ("Stop Transport & Return to Zero"), Record ("Toggle Real-Time Automation & Input Recording (R)"), BPM ("Tempo (BPM) - Click/Scroll to Adjust"), Song Position ("Song Position (Bar.Beat.Tick)"), Workspace Lock ("Lock / Unlock Workspace Layout"), Fullscreen Device Mode ("Dedicated Full-Screen Instrument & Device Mode"), Preset Browser ("Toggle Preset Browser & Project Library (B)"), Snap ("Snap to Grid (Off / 16th / 8th / Bar)"), Scale ("Scale & Harmonic Snapping"), Metronome ("Toggle Metronome Click"), and Loop ("Toggle Arrangement Loop Range").
    - Integrated in both `GuiWindow::getTransportTooltip` and modular `TransportHeader::getTooltip`.
  - **Track Properties Drawer & Panel Tooltips**:
    - Implemented `TrackPropertiesPanel::getTooltip` and `TrackPropertiesDrawer::getTooltip`.
    - Covers Track Icon ("Change Track Icon / Color"), Rename / Title ("Edit Track Name"), Color Swatch ("Set Track Accent Color"), Volume Slider ("Track Output Level"), Pan Dial ("Stereo Panning"), Fullscreen Mode ("Fullscreen Device GUI"), Preset Selector ("Cycle Previous / Next Preset"), Instrument ("Instrument Device"), Chord Follow Chips ("Harmonic Chord Following Mode"), Bake to MIDI ("Bake Chords to Sequencer MIDI"), MIDI FX & Audio FX Add Buttons ("Add MIDI FX Insert", "Add Audio FX Insert"), Bypass Toggles ("Bypass FX"), Reorder Up/Down buttons, Remove FX buttons, Fullscreen Device FX buttons, and Drawer Tabs ("Track Properties Inspector", "Clip Properties Inspector", "Close Properties Drawer").
  - **Automated Verification**:
    - Added Test 20 to `tests/test_modular_ui.cpp` verifying tooltip queries across GuiWindow, TransportHeader, TrackPropertiesPanel, and TrackPropertiesDrawer.
- **Icon Design Review & Vector Rendering Refinements**:
  - **Hole-Occlusion Fix & Disjoint Silhouette Architecture**:
    - Identified and eliminated contour hole-occlusion issue where nested contours inside solid bounds rendered as filled blobs (affecting `inst_synth`, `inst_piano`, `drum_machine`, `drum_kick`, and `hw_cassette`).
    - Redesigned all stock icons into crisp, disjoint geometric silhouettes with calibrated negative space gaps (1.5px - 2.5px), ensuring optimal sharpness and contrast at small sizes (16px, 20px, 24px, 32px).
  - **Enhanced Vector Icon Library (`src/ui/icon_registry.cpp`)**:
    - `inst_synth`: Sleek synthesizer chassis with LED rotary encoders and 5 white keys.
    - `inst_piano`: Classic Grand Piano silhouette with raised prop stick, curved rim, and lyre pedals.
    - `inst_bass`: High-amplitude sub-bass sine wave with punchy low-end sub pulses.
    - `inst_guitar`: Electric guitar silhouette with cutaway horns and angled headstock.
    - `inst_strings`: Classical violin/cello body with carved C-bout waist and scroll.
    - `inst_brass`: Trumpet with 3 vertical piston valves and flared acoustic bell.
    - `inst_vocal`: Studio condenser microphone capsule with shockmount cradle.
    - `drum_kick`: Bass drumhead with angled kick spurs and kick pedal beater mallet.
    - `drum_snare`: Dual-rim snare drum with tension lugs and crossed striking drumsticks.
    - `drum_hihat`: Acoustic cymbal pair on tripod stand with center pull rod and pedal.
    - `drum_clap`: Acoustic shockwave impact burst with radial energy rays.
    - `drum_tom`: Angled rack tom drum with dual rim beads.
    - `drum_machine`: 9-pad illuminated MPC grid with top display/encoder bar.
    - `fx_reverb`: Parabolic acoustic reflections expanding over studio floor plane.
    - `fx_delay`: Clock dial repeat loop with trailing echo taps.
    - `fx_filter`: 24 dB/oct resonant lowpass filter frequency response curve with rolloff slope.
    - `fx_distortion`: High-energy lightning drive bolt.
    - `fx_compressor`: Clamping dynamic limiter arrows and side rails.
    - `fx_eq`: Multi-band graphic parametric EQ sliders with calibrated fader caps.
    - `fx_modular`: Eurorack 3.5mm jack socket and patch cable plug.
    - `hw_speaker`: Studio monitor speaker with acoustic radiation wavefronts.
    - `hw_headphones`: Studio monitoring headphones with headband, pivots, and ear cushions.
    - `hw_cassette`: Retro cassette tape with twin hubs, tape bridge, and label strip.
  - **New Classic DAW Icon Categories**:
    - Added `inst_chiptune` (8-bit gamepad D-pad and action buttons for retro game audio tracks).
    - Added `inst_organ` (B3 tonewheel organ drawbars and dual manual keyboard).
    - Added `fx_chorus` (dual phase-shifted stereo chorus ribbons).
    - Added `hw_midi` (classic 5-pin DIN MIDI port).
  - **IconSearchDialog Scrolling Containment**:
    - Added GPU hardware scissoring (`r.pushScissor` / `r.popScissor`) to `gridBounds_` in `IconSearchDialog::render`, preventing icon cards from bleeding outside dialog borders during scrolling.
  - **Automated Verification**:
    - Updated `tests/test_icon_system.cpp` to verify parsing, contour validation, and triangulation for all 32 stock vector meshes and new categories.
- **Validation**:
  - 100% pass across all 44 test suites in `build.ps1 -Test` on Dawn WebGPU NVIDIA GeForce GTX 1660 Ti hardware pipeline.

---



## 5. Cruft Prevention & Code Hygiene Checklist

- **No Monolithic Inlining**: Never add raw OpenGL/GLFW rendering blocks directly into `gui_window.cpp`. All view-specific rendering belongs in its respective `ViewBase` subclass.
- **Single Source of Truth**: Keep track and clip definitions synchronized via the `ViewContext` and sequencer project data.
- **Process Lock Protection**: Keep `build.ps1` equipped with automatic termination of active debug instances prior to MSVC linking to avoid `LNK1104`.
- **Clean Repository Roots**: Keep test exports and logs out of git tracking via `.gitignore`.
- **100% Test Pass Rate**: Run `ctest` across all 27 unit tests after any architectural migration.
