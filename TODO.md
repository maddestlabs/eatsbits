# Eatsbits Refinement & Fix Backlog

This backlog powers the autonomous refinement workflow in Antigravity.
You can add bugs, UI tweaks, DSP optimizations, and features below.

> 📦 **Task Archive**: Completed tasks older than the 5 most recent are preserved in [TODO-ARCHIVE.md](file:///c:/git/eatsbits/TODO-ARCHIVE.md) for historical context, architectural reference, and test recipes.

To execute items automatically, run in Antigravity:
```text
/goal Work through the unchecked items in TODO.md using the todo-runner skill.
```

---

## 🚀 Active Queue (Prioritized)

<!-- Add your todo notes and improvements here. Items at the top are processed first. -->

- [ ] **Eliminate Per-Frame Heap Allocations in GUI Render Loop (`std::span` & Direct Model References)**
  - **Context/Files**: [src/ui/gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp), [include/eatsbits/ui/views/mixer_view.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/views/mixer_view.hpp), [src/ui/views/mixer_view.cpp](file:///c:/git/eatsbits/src/ui/views/mixer_view.cpp), [include/eatsbits/ui/views/track_inspector_view.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/views/track_inspector_view.hpp), [src/ui/views/track_inspector_view.cpp](file:///c:/git/eatsbits/src/ui/views/track_inspector_view.cpp), [tests/test_gui_interaction.cpp](file:///c:/git/eatsbits/tests/test_gui_interaction.cpp)
  - **Acceptance Criteria**:
    1. **Zero Heap Allocations in `renderFrame()`**:
       - Remove temporary `std::vector<std::string>`, `std::vector<float>`, and `std::vector<Color>` constructions in [`GuiWindow::renderFrame`](file:///c:/git/eatsbits/src/ui/gui_window.cpp#L6174-L6250) across Mixer and Track Inspector view rendering paths.
       - Refactor `syncFromWindow(...)` or view layout calls to directly consume existing track data via `std::span` or direct read-only references to `arrangerTracks_` / sequencer models.
    2. **Telemetry & Audio Scope Buffer Passing**:
       - Ensure scope and meter buffers are passed as lightweight non-owning views (`std::span<const float>`) rather than copying sample vectors.
    3. **Automated Verification**:
       - Add verification test in [`tests/test_gui_interaction.cpp`](file:///c:/git/eatsbits/tests/test_gui_interaction.cpp) confirming that invoking `renderFrame()` multiple times during playback causes 0 memory churn / allocations.
       - Verify all existing GUI interaction and modular UI test suites pass cleanly.

- [ ] **Implement DAW-Standard Infinite Rotary Delta Drag & Fine Adjustment Modifiers**
  - **Context/Files**: [include/eatsbits/presenter/scalar_drag_presenter.hpp](file:///c:/git/eatsbits/include/eatsbits/presenter/scalar_drag_presenter.hpp), [src/presenter/scalar_drag_presenter.cpp](file:///c:/git/eatsbits/src/presenter/scalar_drag_presenter.cpp), [include/eatsbits/ui/widgets/skeuomorphic_knob.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/skeuomorphic_knob.hpp), [include/eatsbits/ui/widgets/skeuomorphic_slider.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/skeuomorphic_slider.hpp), [include/eatsbits/ui/gui_window.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/gui_window.hpp), [src/ui/gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp), [tests/test_presenters.cpp](file:///c:/git/eatsbits/tests/test_presenters.cpp)
  - **Acceptance Criteria**:
    1. **Virtual Relative Delta Drag Mode**:
       - Support relative mouse drag for knobs and vertical faders: capture the mouse on drag start, track relative deltas without clamping to screen borders, and restore the mouse position on drag release.
    2. **Industry-Standard Gesture Modifiers**:
       - Support `Shift` modifier for precision adjustment ($0.1\times$ sensitivity multiplier).
       - Support `Ctrl` / `Cmd` + click to immediately restore control to its default value (e.g. center for pan, 0 dB for track faders).
       - Support double-click to trigger [`ValueEditDialog`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/value_edit_dialog.hpp) for direct numeric keyboard input.
    3. **Automated Verification**:
       - Add unit tests in [`tests/test_presenters.cpp`](file:///c:/git/eatsbits/tests/test_presenters.cpp) verifying modifier sensitivity scaling, double-click value edit routing, and relative delta math.

- [ ] **Add RAII Scissor Scope Management to `BatchRenderer2D`**
  - **Context/Files**: [include/eatsbits/ui/batch_renderer_2d.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp), [src/ui/batch_renderer_2d.cpp](file:///c:/git/eatsbits/src/ui/batch_renderer_2d.cpp), [include/eatsbits/ui/draw_utils.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/draw_utils.hpp), [include/eatsbits/ui/widgets/scrollable_area.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/scrollable_area.hpp), [tests/test_ui_canvas.cpp](file:///c:/git/eatsbits/tests/test_ui_canvas.cpp)
  - **Acceptance Criteria**:
    1. **RAII `ScissorScope`**:
       - Implement `ScissorScope` struct that pushes a scissor rect on construction and automatically clears or restores previous scissor state on scope exit.
       - Support nested clipping rects by calculating the intersection of parent and child clip rectangles.
    2. **Refactor Existing Scissor Calls**:
       - Replace manual, error-prone `setScissorRect` and `clearScissorRect` calls in [`ScrollableArea`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/scrollable_area.hpp), inspector panels, and modal dialogs with `ScissorScope`.
    3. **Automated Verification**:
       - Add unit test in [`tests/test_ui_canvas.cpp`](file:///c:/git/eatsbits/tests/test_ui_canvas.cpp) testing nested clipping rectangle calculation and automatic RAII unwinding.

- [ ] **Implement Static Chrome Offscreen Texture (FBO) Caching**
  - **Context/Files**: [include/eatsbits/ui/batch_renderer_2d.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp), [src/ui/batch_renderer_2d.cpp](file:///c:/git/eatsbits/src/ui/batch_renderer_2d.cpp), [include/eatsbits/ui/gui_window.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/gui_window.hpp), [src/ui/gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp)
  - **Acceptance Criteria**:
    1. **Offscreen Chrome Backing Store**:
       - Allocate a backing texture/render target in `BatchRenderer2D` for static background chrome (top transport metal faceplate, panel bevels, static track headers).
       - Render static chrome once; on subsequent frames blit the cached chrome and only redraw dynamic elements (playhead, meters, scope, active controls).
    2. **Dirty Invalidation Lifecycle**:
       - Automatically invalidate the chrome cache upon window resize, theme changes, or panel toggle events.
    3. **Automated Verification**:
       - Verify rendering correctness in headless mode and ensure visual parity with and without chrome caching enabled.

---

## 🕒 Recent Completions (Reference Context)

<!-- Keep the 5 most recent completed items here for immediate agent context; older items are archived in TODO-ARCHIVE.md -->

- [x] **Eliminate Startup Eatscript Parser Errors & Implement Subscript Indexing / Trailing Commas in DSL Grammar** (2026-10-11)
  - **Context/Files**: [include/eatsbits/eatscript/ast.hpp](file:///c:/git/eatsbits/include/eatsbits/eatscript/ast.hpp), [src/eatscript/parser.cpp](file:///c:/git/eatsbits/src/eatscript/parser.cpp), [src/eatscript/evaluator.cpp](file:///c:/git/eatsbits/src/eatscript/evaluator.cpp), [src/eatscript/transpiler.cpp](file:///c:/git/eatsbits/src/eatscript/transpiler.cpp), [src/eatscript/dispatch_scanner.cpp](file:///c:/git/eatsbits/src/eatscript/dispatch_scanner.cpp), [src/eatscript/vm.cpp](file:///c:/git/eatsbits/src/eatscript/vm.cpp), [src/project/preset_loader.cpp](file:///c:/git/eatsbits/src/project/preset_loader.cpp), [src/project/preset_manager.cpp](file:///c:/git/eatsbits/src/project/preset_manager.cpp), [presets/midi_fx/arpeggiator_midi_fx.eats](file:///c:/git/eatsbits/presets/midi_fx/arpeggiator_midi_fx.eats), [presets/midi_fx/euclidean_acid_generator.eats](file:///c:/git/eatsbits/presets/midi_fx/euclidean_acid_generator.eats), [tests/test_eatscript_vm.cpp](file:///c:/git/eatsbits/tests/test_eatscript_vm.cpp)
  - **Acceptance Criteria**:
    1. **Dictionary Subscript Indexing & Assignment (`params['Key'] = val`)**:
       - Added `ExprKind::Index` (`IndexExpr`) and `StmtKind::IndexAssign` (`IndexAssignStmt`) to AST.
       - Implemented parser lookahead in `parseExpressionOrAssignment()` to distinguish subscript assignment from standalone expressions.
       - Implemented AST Evaluator, VM Bytecode emission, AOT C++ Transpiler, and Dispatch Scanner support for subscript assignments and lookups.
    2. **Grammar Support for Trailing Commas**:
       - Updated `parseCall()` and `parseFunctionDef()` to gracefully handle trailing commas before `)` in argument lists and parameter lists.
    3. **Preset Directory Filtering**:
       - Added guards in `PresetManager::scanDirectory` and `PresetLoader::parseFromEatscript` to ignore non-preset INI/song project files (`[project]`, `return eatsbeats.song`, `"format": "eatsbits"`) so project files are never misparsed as Eatscript presets.
       - Removed stray INI project file `presets/default.eats`.
    4. **Preset Script Refactoring**:
       - Removed trailing comma in `presets/midi_fx/arpeggiator_midi_fx.eats`.
       - Updated `presets/midi_fx/euclidean_acid_generator.eats` to standard host processor call `eat.euclidean()`.
    5. **Automated Verification**:
       - Added unit test `testParserTrailingCommasAndSubscripts()` in `test_eatscript_vm.cpp`.
       - Verified all 37 preset files in `presets/` parse with 0 errors via `eatscript_cli.exe`.
       - Verified clean application startup with 0 parser errors.

- [x] **Optimize CLAP Plugin Scanning & Add Portable App / Binary Directory Setting (Reaper Mode)** (2026-10-11)
  - **Context/Files**: `include/eatsbits/audio/clap/clap_plugin_loader.hpp`, `src/audio/clap/clap_plugin_loader.cpp`, `include/eatsbits/ui/widgets/plugin_search_dialog.hpp`, `src/ui/widgets/plugin_search_dialog.cpp`, `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `tests/test_clap_host.cpp`
  - **Acceptance Criteria**:
    1. **Decoupled Dialog Open from Filesystem Scanning (Zero-Latency Opens)**:
       - Removed synchronous directory scanning and recursive DLL probing from `PluginSearchDialog::open()`. Presets and discovered CLAP plugins are served immediately from in-memory cache (`ClapPluginLoader::getDiscoveredPlugins()`), reducing dialog open latency to 0ms.
    2. **Added Header [ ↻ SCAN ] Button & Background Asynchronous Scanner**:
       - Added a dedicated `[ ↻ SCAN ]` button with scanning pulse indicators to `PluginSearchDialog` header.
       - Implemented `ClapPluginLoader::scanDirectoriesAsync()` to execute scans in background threads without blocking audio processing or the WebGPU render loop.
       - Auto-refreshes dialog cards when a scan completes.
    3. **Portable / Binary Directory Search Path Setting (Reaper-Style)**:
       - Added `includeBinaryPath` configuration (default `true`) allowing the DAW to run portably from any directory, scanning `<exe_dir>/plugins`, `<exe_dir>`, and `<cwd>/plugins` without requiring system-wide installation.
       - Added a toggle switch and `[ ↻ RESCAN PLUGINS ]` button in the System Settings modal (Section 5: "AUDIO & CLAP PLUGINS") with real-time discovered plugin counter.
    4. **Filesystem Traversal & Non-CLAP Library Probe Optimization**:
       - Added `disable_recursion_pending()` to skip traversing `.git`, `.agents`, `CMakeFiles`, `node_modules`, `.vs`, and IDE cache folders.
       - Maintained an in-memory cache of non-CLAP libraries (`nonClapLibraries_`) so failed DLLs are never repeatedly inspected with `LoadLibraryA`.
    5. **Automated Verification**:
       - Added tests in `tests/test_clap_host.cpp` verifying portable path toggle, in-memory cache retrieval, and async scanning. Verified 100% pass across all 48 test suites.

- [x] **Modular Canvas VCV Rack & Cardinal Alignment: Open 2D Canvas, Freeform Drag & Drop, Zoom/Pan & Module Search Dialog** (commits `560db9d`, `a2884de`, 2026-10-10)
  - **Context/Files**: `include/eatsbits/ui/views/design_view.hpp`, `src/ui/views/design_view.cpp`, `include/eatsbits/presenter/zoom_pan_presenter.hpp`, `src/presenter/zoom_pan_presenter.cpp`, `include/eatsbits/ui/widgets/plugin_search_dialog.hpp`, `src/ui/widgets/plugin_search_dialog.cpp`, `src/ui/gui_window.cpp`, `tests/test_modular_ui.cpp`
  - **Acceptance Criteria**:
    1. **Infinite 2D Zoom & Pan Engine**:
       - Integrated `ZoomPanPresenter` (`modularZoomPan_`) into `DesignView::renderModularRack` and input event handlers (`handlePointer`, `handleWheel`, `handleKey`).
       - Supported desktop middle-mouse button drag in `GuiWindow::onMouseDown` (`button == 2`) and right-mouse drag for smooth 2D canvas panning across the modular workspace.
       - Implemented `Ctrl+mousewheel` anchored zoom in/out with smooth bounds ($0.40\times$ to $2.0\times$ zoom) and bare mousewheel canvas panning (vertical / horizontal with Shift).
    2. **Eurorack Multi-Row Mounting Rails & Visual Structure**:
       - Rendered multi-row horizontal Eurorack rows with standard 3U height (380px), power distribution bus bar channel with red stripe & 16-pin gold headers, brushed metal mounting strips with discrete 1 HP (15px) screw/rivet holes, and knurled corner thumbscrews aligned to rivet pitch.
       - Implemented viewport culling and scissor clipping (`pushScissor`/`popScissor`) to ensure infinite canvas elements remain neatly framed within the rack body.
    3. **Discrete 1 HP Rivet Snapping, Reversible Module Bumping & Cable Physics**:
       - Implemented physical hardware feel: module movement strictly occurs in discrete 1 HP increments (15px) derived from rivet hole intervals, ignoring sub-HP movements to maintain hardware rack lock.
       - Snapshot initial module positions upon drag start: moving a module over a neighbor automatically bumps the neighbor left or right based on approach trajectory and non-overlapping interval packing. Moving the dragged module back to its starting location restores all neighboring modules to their exact initial positions.
       - Dynamically transformed patch cable endpoints to follow dragged and bumped modules in real-time with catenary physics droop and jack halo highlights.
    4. **Module Search & Add Dialog Integration**:
       - Connected `+ ADD MODULE` button, hotkeys (`Space`, `Enter`), and cursor click positions to open `PluginSearchDialog` in `PluginDialogMode::AddModule` mode.
       - Populated 22 Eurorack modules across 5 categories (`VCO / Synths`, `Filters`, `Effects`, `Modulation & Dynamics`, `Utilities`).
       - Instant module instantiation with matching knobs, inputs, and outputs placed under the cursor or in open rail slots.
    5. **Automated Verification**:
       - Added regression tests in `tests/test_modular_ui.cpp` (`testDesignView`) verifying coordinate transforms, middle-drag panning, bare wheel panning, Ctrl+wheel zooming, sub-HP deadband, 1 HP discrete step dragging, neighbor bumping on collision, and complete initial position restoration upon return.
       - Verified 100% test pass on native desktop (49/49 test suites via `.\build.ps1 -Test -NoRun`) and clean WebAssembly compilation (`.\build-web.ps1 -NoServe`).

- [x] **Consolidate Modular Nodes: Drop 'Daisy' Brand Prefix, Power Core Nodes with DaisySP & Unify Eatscript API** (commit `7f81637`, 2026-10-09)
  - **Context/Files**: `include/eatsbits/audio/graph/nodes/`, `src/audio/graph/nodes/`, `include/eatsbits/eatscript/api_manifest.hpp`, `src/audio/audio_engine.cpp`, `CMakeLists.txt`, `tests/test_daisysp_nodes.cpp`
  - **Acceptance Criteria**:
    1. **Unify C++ GraphNode Classes & Drop Vendor Prefix**:
       - Consolidated `DaisyOscillatorNode` $\to$ canonical `OscillatorNode` (`include/eatsbits/audio/graph/nodes/oscillator_node.hpp`), providing PolyBLEP bandlimited oscillator with 1V/Oct CV modulation.
       - Consolidated `DaisyFilterNode` $\to$ canonical `MoogFilterNode` (`include/eatsbits/audio/graph/nodes/moog_filter_node.hpp`) wrapping analog-modeled 24dB Moog ladder and SVF filters (LP/HP/BP/Notch).
       - Consolidated `DaisyReverbNode` $\to$ canonical `ReverbNode` (`include/eatsbits/audio/graph/nodes/reverb_node.hpp`) as algorithmic stereo plate/hall reverb (`daisysp::ReverbSc`).
       - Upgraded `ChorusNode`, `DelayNode`, and `CompressorNode` to internally encapsulate DaisySP engines (`daisysp::Chorus`/`daisysp::Flanger`/`daisysp::Overdrive`, `daisysp::DelayLine` + `daisysp::Tone`, `daisysp::Compressor`) with zero heap allocations and backwards-compatible parameter interfaces.
       - Created aggregation header `modular_nodes.hpp` and retained forwarders for all legacy `Daisy*` headers.
    2. **Clean, Prefix-Free Eatscript API Manifest**:
       - Updated `include/eatsbits/eatscript/api_manifest.hpp` with canonical names:
         - `eats.dsp.Oscillator` (aliases: `"osc"`, `"Oscillator"`)
         - `eats.dsp.MoogFilter` (aliases: `"moog"`, `"moog_ladder"`, `"Filter"`)
         - `eats.fx.Reverb` (aliases: `"reverb"`, `"reverb_sc"`, `"Reverb"`)
         - `eats.fx.Delay` (aliases: `"delay"`, `"Delay"`)
         - `eats.fx.Chorus` (aliases: `"chorus"`, `"flanger"`, `"Chorus"`)
         - `eats.fx.Compressor` (aliases: `"compressor"`, `"Compressor"`)
       - Maintained legacy `Daisy*` aliases so older scripts and reflection queries continue running without breakage.
    3. **AudioEngine & FX Rack Consolidation**:
       - Streamlined `AudioEngine::rebuildTrackAudioFx` and `AudioEngine::setTrackAudioFxParam` to instantiate and parameterize the unified canonical nodes (`ReverbNode`, `DelayNode`, `ChorusNode`, `CompressorNode`, `MoogFilterNode`).
    4. **Preserved Distinct Specialized & Retro Engines**:
       - Retained `BiquadNode` and `ParametricEqNode` for surgical parametric EQ (peaking/shelving/notch filters).
       - Retained `ConvolverNode` for FFT partitioned impulse responses (speaker cabinets, acoustic spaces).
       - Retained `WaveShaperNode` for custom tanh overdrive/distortion curves.
       - Retained vintage sound chip emulators (`Ym2612Node`, `Tb303Node`, `SidNode`, `SnesNode`, `Dx7Node`).
    5. **Automated Verification & Web Build Validation**:
       - Updated `tests/test_daisysp_nodes.cpp` verifying canonical nodes, parameter sweeps, and audio stability.
       - 100% test pass on native desktop (49/49 test suites) and clean WebAssembly compilation via `.\build-web.ps1 -NoServe`.

- [x] **DaisySP Integration: Permissive (MIT), WebAssembly-Ready Modular DSP Engine & FX Suite** (commit `14329da`, 2026-10-09)
  - **Context/Files**: `third_party/daisysp/`, `include/eatsbits/audio/graph/nodes/daisy/`, `src/audio/graph/nodes/daisy/`, `include/eatsbits/audio/audio_engine.hpp`, `src/audio/audio_engine.cpp`, `include/eatsbits/eatscript/api_manifest.hpp`, `CMakeLists.txt`, `tests/test_daisysp_nodes.cpp`
  - **Acceptance Criteria**:
    1. **Vendored DaisySP Suite**:
       - Vendored official `DaisySP` core (MIT) and `DaisySP-LGPL` into `third_party/daisysp/`.
       - Configured `eats_daisysp` static library in `CMakeLists.txt` linked into `eats_audio`, passing MSVC, Clang, GCC, and Emscripten (WebAssembly) with zero warnings or undefined symbols.
    2. **Implemented 6 Modular GraphNode Wrappers**:
       - `DaisyOscillatorNode`: `daisysp::Oscillator` (Saw, Square, Triangle, Sine, Variable Saw) with 1V/Oct CV and phase modulation.
       - `DaisyFilterNode`: Dual stereo `daisysp::MoogLadder` (24dB/oct resonant ladder) and `daisysp::Svf` (State Variable Filter).
       - `DaisyReverbNode`: Stereo `daisysp::ReverbSc` algorithmic plate/hall reverb with feedback/decay and LP damping.
       - `DaisyDelayNode`: Stereo `daisysp::DelayLine` with high-frequency damping tone filter and feedback.
       - `DaisyModulationNode`: Multi-FX processor wrapping `daisysp::Chorus`, `daisysp::Flanger`, and `daisysp::Overdrive`.
       - `DaisyCompressorNode`: Dual stereo dynamic range compressor wrapping `daisysp::Compressor` with attack, release, ratio, threshold, and makeup gain.
    3. **AudioEngine & Track FX Rack Integration**:
       - Fully integrated in `AudioEngine::rebuildTrackAudioFx` and `AudioEngine::setTrackAudioFxParam`, mapping UI knob values to normalized parameters across Reverb, Delay, Chorus, Flanger, Moog Filter, and Compressor effect inserts.
    4. **Eatscript & API Manifest Binding**:
       - Bound all 6 nodes into `include/eatsbits/eatscript/api_manifest.hpp` under `eats.dsp.*` and `eats.fx.*` for AST dependency scanning and AOT export.
    5. **Automated Verification**:
       - Added `tests/test_daisysp_nodes.cpp` validating parameter sweeps, impulse responses, and zero runtime heap allocations.
       - 100% test pass on native desktop (49/49 test suites) and clean WebAssembly compilation via `.\build-web.ps1 -NoServe`.

- [x] **Fix Desktop CLAP Host GUI Lifecycle & Window Compatibility (Cardinal & DPF Plugins)** (commit `e84ef3e`, 2026-10-09)
  - **Context/Files**: `include/eatsbits/audio/graph/nodes/clap_audio_node.hpp`, `src/audio/graph/nodes/clap_audio_node.cpp`, `src/ui/gui_window.cpp`, `src/gui_main.cpp`, `tests/test_clap_host.cpp`
  - **Acceptance Criteria**:
    1. **Strict Window API & Mode Negotiation**:
       - Query `clap_plugin_gui->is_api_supported(plugin, CLAP_WINDOW_API_WIN32, false)` (embedded) and `true` (floating).
       - Create dedicated Win32 host window configured with `WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS`, `CS_OWNDC`, and valid double-buffered pixel format attributes for embedded DPF/OpenGL child windows.
       - Support both floating native `WS_OVERLAPPEDWINDOW` shell and embedded docking.
    2. **Main-Thread Affinity & Message Pump Isolation**:
       - Guaranteed all `gui->create()`, `gui->set_parent()`, `gui->show()`, `gui->hide()`, and `gui->destroy()` calls happen strictly on the designated main thread (`ClapAudioNode::getMainThreadId()`).
       - Drained Win32 message queue in `pollAllMainThreadCallbacks()`, providing plugin UI timer dispatch via `CLAP_EXT_TIMER_SUPPORT` without deadlocking GLFW or the WebGPU render loop.
    3. **Dimension Negotiation & HiDPI Scaling**:
       - Initialized GUI with `gui->create()` prior to size queries, honoring DPF requirements (`gui != nullptr`).
       - Handled plugin dynamic resizing and dimension queries via `clap_host_gui::request_resize` and `resize_hints_changed`.
    4. **Safety Wrappers & Generic Parameter Inspector Fallback**:
       - Implemented SEH safe wrappers (`SafeClapGuiCreate`, `SafeClapGuiSetParent`, `SafeClapGuiShow`, `SafeClapGuiHide`, `SafeClapGuiDestroy`) to protect host stability.
       - In `GuiWindow`, gracefully catch windowing or runtime failures and fall back to generic parameter slider UI in `TrackPropertiesPanel`.
    5. **Automated Verification**:
       - Comprehensive testing in `tests/test_clap_host.cpp` verifying full Cardinal floating and embedded GUI lifecycle (`create` -> `show` -> `hide` -> `destroy`, bounds resizing, and child window destruction). Verified 100% pass across all 47 test suites.

- [x] **Remove Eatsbeats Settings & Project Hub Texture Options** (commit `e3d4d8a`, 2026-10-09)
  - **Context/Files**: `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `tests/test_gui_interaction.cpp`, `tests/test_vector_surfaces.cpp`
  - **Acceptance Criteria**:
    1. **Eliminated Obsolete Texture Settings from Project Hub**:
       - Removed legacy texture options in Project Hub (Section 3), including 9 material texture chips (Dark Chassis, Brushed Metal, Powder Coat, Crinkle, Bakelite, Walnut, Rosewood, PCB Green, Carbon), Chassis Rotation (0° vs 90° Vertical), and Surface Wear & Patina levels (Pristine, Mint, Patina, Vintage).
       - Streamlined Section 3 into a clean `"CHASSIS COLOR ACCENTS & TINTS"` drawer with 8 anodized harmonic color swatches and a live vector chassis plate preview, updating drawer height calculations (from 268.0f to 100.0f) and section header icons.
    2. **Purged Texture Action Enums & State**:
       - Removed obsolete enum values from `ProjectHubAction` (`SetPanelTexture`, `SetPanelWear`, `SetPanelRotation`).
       - Removed unused chassis texture state variables from `GuiWindow` (`panelChassisTexture_`, `panelChassisWear_`, `panelChassisRotation_`) and their setters (`setPanelChassisTexture`, `setPanelChassisWear`, `setPanelChassisRotation`). Kept `panelChassisTint_` and `setPanelChassisTint()`.
    3. **Simplified Chassis Plate Rendering**:
       - Streamlined `drawChassisPlate` signatures in `GuiWindow` to eliminate unused style, wear, and rotation parameters, rendering crisp resolution-independent vector gradients and specular bevels directly.
    4. **Cleaned Up Hit Testing & Event Handlers**:
       - Excised all material chip, rotation toggle, and patina level hit-testing loops and click-action handling in `hitTestProjectHub()` and `onMouseDown()`.
    5. **Test Suite Verification**:
       - Updated `tests/test_gui_interaction.cpp` to verify the streamlined Chassis Color Accents & Tints swatch selection. Verified 100% pass across all 48 test suites with `.\build.ps1 -Test -NoRun`.

## Planned / Up Next: CLAP Plugin Export & Hosting (See clap-blueprint.md)
- [x] **Phase 1: Header-Only CLAP Shell Adapter & Static DSP Export Reference**
  - Vendored official `clap/clap.h` (v1.2.10) C-ABI headers into `third_party/clap/include/clap/`.
  - Implemented `include/eatsbits/export/clap/clap_plugin_shell.hpp` template bridging `clap_plugin_t` to planar float buffers, sample-accurate note/parameter events, and state persistence with zero runtime memory allocations.
  - Implemented `include/eatsbits/export/clap/tb303_clap_adapter.hpp` exposing `Tb303Core` with 8 automatable parameters, bidirectional text formatting, and note-on/note-off dispatch with Roland glide and accents.
  - Compiled self-contained reference `.clap` binary `eatsbits_tb303.clap` (26 KB) via `src/export/clap/tb303_clap_plugin.cpp`.
  - Verified with comprehensive automated test suite in `tests/test_clap_shell.cpp` (`ClapShellTest`), validating port descriptors, in-process block slicing, sample-accurate event timing, parameter automation sweep, state save/load roundtrips, and dynamic DSO runtime loading via `LoadLibraryA`.
- [x] **Phase 2: API Dependency Manifest & AST Tree-Shaking Scanner**
  - Defined declarative `include/eatsbits/eatscript/api_manifest.hpp` mapping modules (`eats.dsp.*`, `eats.fx.*`) to exact C++ headers and types.
  - Upgraded `DependencyScanner` with AST traversal, comment directive harvesting (`# @id`, `# @name`, `# @engine`, etc.), symbol dependency resolution, and `generateClapParamDefs()`.
  - Added automated test suite `tests/test_dispatch_scanner.cpp` (`DispatchScannerTest`), validating 100% detection and reflection.
- [x] **Phase 3: Eatscript AOT Transpiler CLAP Target & CLI Export**
  - Extended `Transpiler` with `emitClapPluginEntry()` and `transpileClap()`, generating standalone C++ compilation units embedding `ClapPluginShell<GeneratedPatchAdapter>`.
  - Added CLI `export --format=clap <script.eats> -o <output.clap>` to `eatscript_cli` producing micro-runtime `.clap` binaries (<200KB) with zero VM overhead.
  - Added automated test suite `tests/test_clap_export.cpp` (`ClapExportTest`), validating AST math emission, native engine wrapping, CLI tool execution, parameter reflection, and dynamic host loading with audio block processing.
- [x] **Phase 4: Desktop CLAP Host Node in AudioGraph**
  - Implement `ClapAudioNode` for loading 3rd-party instruments/FX into track mixer channels.
  - Connect to `PluginSearchDialog` and support floating native OS windows and generic parameter inspector.
  - Verified Cardinal & DPF plugin GUI lifecycle, Win32 embedded container docking, UI timer dispatch, and crash-resilient SEH wrappers.
