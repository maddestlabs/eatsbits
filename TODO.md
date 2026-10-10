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

---

## 🕒 Recent Completions (Reference Context)

<!-- Keep the 5 most recent completed items here for immediate agent context; older items are archived in TODO-ARCHIVE.md -->

- [x] **Modular Canvas VCV Rack & Cardinal Alignment: Open 2D Canvas, Freeform Drag & Drop, Zoom/Pan & Module Search Dialog** (commit `560db9d`, 2026-10-09)
  - **Context/Files**: `include/eatsbits/ui/views/design_view.hpp`, `src/ui/views/design_view.cpp`, `include/eatsbits/presenter/zoom_pan_presenter.hpp`, `src/presenter/zoom_pan_presenter.cpp`, `include/eatsbits/ui/widgets/plugin_search_dialog.hpp`, `src/ui/widgets/plugin_search_dialog.cpp`, `src/ui/gui_window.cpp`, `tests/test_modular_ui.cpp`
  - **Acceptance Criteria**:
    1. **Infinite 2D Zoom & Pan Engine**:
       - Integrated `ZoomPanPresenter` (`modularZoomPan_`) into `DesignView::renderModularRack` and input event handlers (`handlePointer`, `handleWheel`, `handleKey`).
       - Supported middle-mouse drag and right-mouse drag for smooth 2D canvas panning across the modular workspace.
       - Supported mouse wheel and trackpad pinch-to-zoom (anchored to cursor position) with smooth bounds (0.40x to 2.0x zoom) and keyboard zoom shortcuts (`+`, `-`, `0`).
    2. **Eurorack Multi-Row Mounting Rails & Visual Structure**:
       - Rendered multi-row horizontal Eurorack rows with standard 3U height (380px), power distribution bus bar channel with red stripe & 16-pin gold headers, brushed metal mounting strips with 15px HP screw holes, and corner thumbscrews on module faceplates.
       - Implemented viewport culling and scissor clipping (`pushScissor`/`popScissor`) to ensure infinite canvas elements remain neatly framed within the rack body.
    3. **Freeform Module Drag-and-Drop & Reordering**:
       - Enabled dragging modules by their title banners across rows and horizontal positions with live 15px HP snap-to-grid on release.
       - Dynamically transformed patch cable endpoints to follow dragged modules in real-time with catenary physics droop and jack halo highlights.
    4. **Module Search & Add Dialog Integration**:
       - Connected `+ ADD MODULE` button, hotkeys (`Space`, `Enter`), and cursor click positions to open `PluginSearchDialog` in `PluginDialogMode::AddModule` mode.
       - Populated 22 Eurorack modules across 5 categories (`VCO / Synths`, `Filters`, `Effects`, `Modulation & Dynamics`, `Utilities`).
       - Instant module instantiation with matching knobs, inputs, and outputs placed under the cursor or in open rail slots.
    5. **Automated Verification**:
       - Added regression tests in `tests/test_modular_ui.cpp` (`testDesignView`) verifying coordinate transforms, middle-drag panning, zoom bounds, freeform module dragging, HP snapping, and search dialog module insertion.
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
