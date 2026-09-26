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

---

## 5. Cruft Prevention & Code Hygiene Checklist

- **No Monolithic Inlining**: Never add raw OpenGL/GLFW rendering blocks directly into `gui_window.cpp`. All view-specific rendering belongs in its respective `ViewBase` subclass.
- **Single Source of Truth**: Keep track and clip definitions synchronized via the `ViewContext` and sequencer project data.
- **Process Lock Protection**: Keep `build.ps1` equipped with automatic termination of active debug instances prior to MSVC linking to avoid `LNK1104`.
- **Clean Repository Roots**: Keep test exports and logs out of git tracking via `.gitignore`.
- **100% Test Pass Rate**: Run `ctest` across all 27 unit tests after any architectural migration.
