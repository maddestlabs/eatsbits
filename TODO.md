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

- [x] **Implement DAW-Standard Infinite Rotary Delta Drag & Fine Adjustment Modifiers** (commit `a8ef98c`, 2026-10-11)
  - **Context/Files**: [include/eatsbits/presenter/scalar_drag_presenter.hpp](file:///c:/git/eatsbits/include/eatsbits/presenter/scalar_drag_presenter.hpp), [src/presenter/scalar_drag_presenter.cpp](file:///c:/git/eatsbits/src/presenter/scalar_drag_presenter.cpp), [include/eatsbits/ui/widgets/skeuomorphic_knob.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/skeuomorphic_knob.hpp), [include/eatsbits/ui/widgets/skeuomorphic_slider.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/skeuomorphic_slider.hpp), [include/eatsbits/presenter/drag_handler.hpp](file:///c:/git/eatsbits/include/eatsbits/presenter/drag_handler.hpp), [include/eatsbits/ui/widgets/track_properties_panel.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/track_properties_panel.hpp), [src/ui/gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp), [src/ui/views/mixer_view.cpp](file:///c:/git/eatsbits/src/ui/views/mixer_view.cpp), [src/ui/widgets/track_properties_panel.cpp](file:///c:/git/eatsbits/src/ui/widgets/track_properties_panel.cpp), [tests/test_presenters.cpp](file:///c:/git/eatsbits/tests/test_presenters.cpp)
  - **Acceptance Criteria**:
    1. **Virtual Relative Delta Drag Mode**:
       - Implemented relative mouse drag for knobs, sliders, and faders in `ScalarDragPresenter`: captures cursor via GLFW on drag start, accumulates unbounded relative deltas without border clamping, clamps `accumulatedDelta_` to parameter travel limits to ensure instantaneous reversal without deadzone lag, and restores original cursor coordinates on drag release.
    2. **Industry-Standard Gesture Modifiers**:
       - Implemented `Shift` modifier precision scaling across presenter, widgets, MixerView, and TrackPropertiesPanel with standard $0.10\times$ sensitivity ratio.
       - Implemented `Ctrl` / `Cmd` + click instant default reset: restores volume/faders to 1.0 (0 dB unity gain), pan to 0.0 (center), and EQ/knobs to their default parameters.
       - Implemented double-click detection (<350ms, <6px) routing to `ValueEditDialog` for direct numeric and keyboard input across presenters and views.
    3. **Automated Verification**:
       - Added unit test `testInfiniteRotaryDeltaDragAndFineAdjustment()` in `tests/test_presenters.cpp` verifying relative deltas, unbounded motion, GLFW capture callbacks, shift modifier $0.10\times$ scaling, Ctrl+click reset, and double-click dispatch.
       - Verified 100% test pass (48/48 test suites) via `ctest`.

- [x] **Eliminate Per-Frame Heap Allocations in GUI Render Loop (`std::span` & Direct Model References)** (commit `39deacb`, 2026-10-11)
  - **Context/Files**: [src/ui/gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp), [include/eatsbits/ui/views/mixer_view.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/views/mixer_view.hpp), [src/ui/views/mixer_view.cpp](file:///c:/git/eatsbits/src/ui/views/mixer_view.cpp), [include/eatsbits/ui/views/track_inspector_view.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/views/track_inspector_view.hpp), [src/ui/views/track_inspector_view.cpp](file:///c:/git/eatsbits/src/ui/views/track_inspector_view.cpp), [src/ui/views/arranger_view.cpp](file:///c:/git/eatsbits/src/ui/views/arranger_view.cpp), [src/ui/views/design_view.cpp](file:///c:/git/eatsbits/src/ui/views/design_view.cpp), [include/eatsbits/ui/draw_utils.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/draw_utils.hpp), [src/ui/draw_utils.cpp](file:///c:/git/eatsbits/src/ui/draw_utils.cpp), [include/eatsbits/ui/icon_registry.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/icon_registry.hpp), [src/ui/icon_registry.cpp](file:///c:/git/eatsbits/src/ui/icon_registry.cpp), [src/ui/dawn_bridge.cpp](file:///c:/git/eatsbits/src/ui/dawn_bridge.cpp), [tests/test_gui_interaction.cpp](file:///c:/git/eatsbits/tests/test_gui_interaction.cpp)
  - **Acceptance Criteria**:
    1. **Zero Heap Allocations in `renderFrame()`**:
       - Removed temporary `std::vector<std::string>`, `std::vector<float>`, and `std::vector<Color>` constructions in `GuiWindow::renderFrame` across Mixer and Track Inspector view rendering paths.
       - Refactored `syncFromWindow(...)` and view layout calls to directly consume existing track data via `std::span` and direct read-only references to `arrangerTracks_` / sequencer models.
       - Replaced Win32/CRT `std::filesystem::last_write_time` shader polling in `DawnBridge` with `GetFileAttributesExA` / `stat` to avoid per-second heap allocations.
       - Replaced icon registry lookups with transparent hash/equality `std::string_view` lookups and mesh caching.
       - Optimized `drawMonoText`, `drawMonoTextClipped`, and `drawTooltipBadge` to take `std::string_view`, eliminating per-frame temporary string allocations in Design view search bars, headers, knob badges, and track properties drawer.
    2. **Telemetry & Audio Scope Buffer Passing**:
       - Ensured scope and meter buffers are passed as lightweight non-owning views (`std::span<const float>`) rather than copying sample vectors.
    3. **Automated Verification**:
       - Added verification test in `tests/test_gui_interaction.cpp` confirming that invoking `renderFrame()` across Arranger (View 0), Mixer (View 3), Track Inspector (View 2), and Design (View 4) views during live playback results in **0 heap allocations** across 20 rendered frames per view.
       - Verified 100% test pass (48/48 test suites) via `ctest`.

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
