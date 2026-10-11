# Eatsbits Completed Task Archive

This file archives completed tasks, architectural acceptance criteria, and verification test recipes from [TODO.md](file:///c:/git/eatsbits/TODO.md).
It serves as historical context and regression-prevention reference for developers and AI agents.

---

## 📦 Archived Tasks

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

- [x] **Audio FX Engine Overhaul: Multi-Insert Routing, Distortion Calibration & Node Restoration** (commit `7648c86`, 2026-10-09)
  - **Context/Files**: `include/eatsbits/audio/audio_engine.hpp`, `src/audio/audio_engine.cpp`, `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `include/eatsbits/ui/views/track_inspector_view.hpp`, `src/ui/views/track_inspector_view.cpp`, `include/eatsbits/ui/views/arranger_view.hpp`, `src/ui/views/arranger_view.cpp`, `include/eatsbits/ui/widgets/track_properties_panel.hpp`, `src/ui/widgets/track_properties_panel.cpp`, `include/eatsbits/ui/widgets/track_properties_drawer.hpp`, `src/ui/widgets/track_properties_drawer.cpp`, `include/eatsbits/ui/widgets/fullscreen_device_modal.hpp`, `src/ui/widgets/fullscreen_device_modal.cpp`, `include/eatsbits/audio/graph/nodes/waveshaper_node.hpp`, `tests/test_studio_fx.cpp`
  - **Acceptance Criteria**:
    1. **Bridge Inspector Audio FX to Engine & Support Multi-Insert Param Dispatch**:
       - Bridged `TrackInspectorView::tracks_[trackIndex].audioFxList` and `ArrangerView::tracks_[trackIndex].audioFx` in `GuiWindow::syncTrackAudioFxToEngine` and across all inspector audio FX callbacks (`onAddAudioFx`, `onToggleAudioFx`, `onReorderAudioFx`, `onRemoveAudioFx`, `onAudioFxChanged`), ensuring engine builds the actual audio FX chain rather than an empty list.
       - Replaced hardcoded `fxIndex = 0` with actual `fxIdx` in `TrackPropertiesPanel`, `TrackPropertiesDrawer`, `TrackInspectorView`, `ArrangerView`, and `FullscreenDeviceModal` callbacks, dispatching parameters directly to specific insert indices via `engine_->setTrackAudioFxParam(trackIndex, fxIndex, paramName, normVal)`.
    2. **Fix Track 1 Dual Audio Path & Initial Delay Disconnect**:
       - In `AudioEngine::rebuildTrackAudioFx`, disconnected all intermediate nodes feeding into `gainId` port 0 and cleanly removed default `AcidEcho` node on Track 1. Guaranteed `gainId` receives only a single clean input path, eliminating parallel summing, phase cancellation, volume boost, and clipping distortion on Track 1.
    3. **Stop Defaulting Unrecognized Effects to Tube Distortion**:
       - Instantiated `BiquadNode` in `rebuildTrackAudioFx` for all `FILTER`, `MULTIMODE_FILTER`, `SVF`, `LOWPASS`, `HIGHPASS`, `BANDPASS`, and `VCF` effects, mapping `cutoff`, `resonance`, `type`, and `gain` in `setTrackAudioFxParam`.
       - Instantiated `EatscriptNode` for script-based effects (`.eats` FX), loading and compiling DSP scripts.
       - Restricted `WaveShaperNode` instantiation strictly to explicit distortion, overdrive, saturation, or bitcrusher types.
    4. **Calibrate `WaveShaperNode` Transfer Curve & Gain Staging**:
       - Replaced foldback curve `d / (1.0f + d * d)` with monotonic dual tanh saturation curve `(d > 0.0f) ? std::tanh(d) : (0.85f * std::tanh(d * 0.90f))` to eliminate unmusical wavefolding distortion when driven with hot signals.
       - Calibrated pre-gain/drive in `rebuildTrackAudioFx` to `std::clamp(fx.drive * 2.0f, 0.5f, 4.0f)` for subtle harmonic warmth and color.
    5. **Automated Verification**:
       - Expanded `tests/test_studio_fx.cpp` with `testAudioEngineFilterNodeAndMultiInsertRouting` validating `BiquadNode` filter creation, multi-insert parameter dispatch (insert index > 0), and clean single-path routing on Track 1. All 48 test suites passing.

- [x] **Fix Live MIDI FX Pipeline Synchronization, Parameter Binding & Inspection** (commit `2c1824f`, 2026-10-09)
  - **Context/Files**: `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `include/eatsbits/ui/views/track_inspector_view.hpp`, `src/ui/views/track_inspector_view.cpp`, `include/eatsbits/ui/views/arranger_view.hpp`, `src/ui/views/arranger_view.cpp`, `include/eatsbits/eatscript/midi_fx_pipeline.hpp`, `src/sequencer/step_sequencer.cpp`, `tests/test_midi_fx.cpp`
  - **Acceptance Criteria**:
    1. **Bridge Inspector and Arranger MIDI FX Model Lists**:
       - Resolved the disconnect between `TrackInspectorView::tracks_[trackIndex].midiFxList` and `ArrangerView::tracks_[trackIndex].midiFx`. When a MIDI FX is added, removed, reordered, or toggled in Track Inspector, mirror changes to `modularArrangerView_->getTracks()[trackIndex].midiFx` before calling `syncTrackMidiFxToEngine(trackIndex)`, preventing the engine from receiving an empty rack while preserving existing project insert items.
    2. **Populate Complete MIDI FX Parameters in `syncTrackMidiFxToEngine`**:
       - Updated `GuiWindow::syncTrackMidiFxToEngine` to serialize the full set of parameters from `TrackMidiFxItem::knobs` into `insert.params` (`Octaves`, `octaves`, `semitones`, `transpose`, `rate`, `gate`, `swing`, `humanizeVelocity`, `humanizeTiming`, `rootKey`, `scaleMode`), enabling `StepSequencer` to read them during playback.
    3. **Wire Parameter Updates in GuiWindow**:
       - Wired `modularTrackInspectorView_->onMidiFxParamChanged` in `GuiWindow::initModularViews()` to update the active rack on `engine_->getSequencer().getTrack(idx)`.
       - Updated `GuiWindow::handleTrackInspectorInteraction` (toggles and knobs for Arp, Scale, Humanize) to update insert items, mirror between Arranger and Track Inspector, and call `syncTrackMidiFxToEngine(tIdx)`.
       - Connected `fullscreenDeviceModal_.onMidiFxParamChanged` to dispatch parameter values directly to the active `SequencerTrack::getMidiFxRack()`, supporting normalized and case-insensitive keys, and mirroring values to UI views.
    4. **Automated Verification**:
       - Added regression tests in `tests/test_midi_fx.cpp` (`TestCaptureNode`, `testInspectorMidiFxBridgeAndLivePlayback`) verifying that adding an Arpeggiator, Transpose, or Scale Snap effect audibly alters playback notes in `StepSequencer::processBlock`. Verified all 48 test suites passing.

- [x] **Fix Chord Follow & Project Harmony Playback Pipeline Desynchronization** (commit `33a8fb6`, 2026-10-09)
  - **Context/Files**: `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `include/eatsbits/ui/views/track_inspector_view.hpp`, `src/ui/views/track_inspector_view.cpp`, `include/eatsbits/ui/views/arranger_view.hpp`, `src/ui/views/arranger_view.cpp`, `include/eatsbits/sequencer/step_sequencer.hpp`, `src/sequencer/step_sequencer.cpp`, `include/eatsbits/audio/audio_engine.hpp`, `src/audio/audio_engine.cpp`, `tests/test_chord_track.cpp`, `tests/test_modular_ui.cpp`
  - **Acceptance Criteria**:
    1. **Unify Multi-View Chord Follow State**: Wired `modularTrackInspectorView_->onChordFollowChanged` in `GuiWindow::initModularViews()` to sync `arrangerTracks_`, `modularArrangerView_`, and `engine_->getSequencer().getTrack(idx)->setChordFollowMode(mode)`. Propagated changes across views and synced on window state restore.
    2. **Synchronize Arranger Chord Track Lane to StepSequencer**: Added `onChordTrackChanged` callback to `ArrangerView` and wired it in `GuiWindow` to push `modularArrangerView_->getChordTrack()` to `StepSequencer`, ensuring progressions are immediately evaluated during playback.
    3. **Auditioning & Note Preview Harmony Support**: In `AudioEngine::postNoteOn` and `postNoteOff`, remapped preview pitches through `theory::ChordTheory::remapPitchForChord` so auditioning in Piano Roll, Virtual Keyboard, and Tracker reflects chord harmonization.
    4. **Automated Verification**: Added unit test coverage in `tests/test_chord_track.cpp` (`testChordFollowPipelineSyncAndAuditioning`) and `tests/test_modular_ui.cpp` (`testGuiWindowIntegration`), validating 100% test pass across all 48 test suites.

- [x] **Vector Graphics UI Modernization & Procedural Texture Removal** (commit `187fedc`, 2026-10-09)
  - **Context/Files**: `include/eatsbits/ui/svg_path.hpp`, `src/ui/svg_path.cpp`, `src/ui/gui_window.cpp`, `src/ui/gui_panel_def.cpp`, `include/eatsbits/ui/procedural_texture_system.hpp` (deleted), `src/ui/procedural_texture_system.cpp` (deleted), `tests/test_vector_surfaces.cpp`, `tests/test_procedural_textures.cpp` (deleted), `CMakeLists.txt`, `src/ui/views/mixer_view.cpp`
  - **Acceptance Criteria**:
    1. **Eliminated Procedural Texture Cruft**: Completely removed `ProceduralTextureSystem` and `ChassisTextureSystem`, eliminating per-pixel CPU noise generators, redundant 256x256 GPU texture allocations, and heavy per-frame `wgpuQueueWriteTexture` uploads for chassis and buttons.
    2. **Pure Vector SVG Triangulation**: Removed `SvgTextureCache` raster caching. Triangulated SVG paths directly to vector triangle vertex batches via `SvgPath::triangulate()` and `BatchRenderer::drawTriangle()`, producing crisp resolution-independent vector rendering with 0 GPU texture uploads.
    3. **Modern Vector DAW Aesthetic**: Replaced tiled noise chassis plates and button backgrounds with sleek vector gradients (`drawRoundedRectGradient`), crisp 1px specular bevel highlights, and hairline divider lines (inspired by Ableton Live, FL Studio, and Bitwig Studio).
    4. **Mixer View Hit-Test Ordering Fix**: Fixed hit-test priority in `MixerView` where pan knob hit-testing was intercepting clicks before Mute/Solo button checks.
    5. **Automated Unit Testing**: Created `test_vector_surfaces.cpp` verifying all 14 `GuiChassisStyle` surfaces, custom color tints, wood cheeks, and minimal white vector faceplates across full headless passes.
  - **Test/Validation**: `test_vector_surfaces.exe` (10/10 tests pass), all 48 test suites passing (`.\build.ps1 -Test -NoRun`), 0 compile warnings or errors.

- [x] **Design Tab Live Project Track, Instrument & Insert FX Synchronization** (commit `6f832a3`, 2026-10-05)
  - **Context/Files**: `include/eatsbits/ui/views/design_view.hpp`, `src/ui/views/design_view.cpp`, `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `tests/test_modular_ui.cpp`
  - **Acceptance Criteria**:
    1. Replaced hardcoded prototype targets in `DesignView::initDefaultTargetsAndCode()` with dynamic project synchronization.
    2. Implemented `DesignView::syncWithProject(const std::vector<ArrangerTimelineTrack>& tracks)` called on project initialization, track creation/deletion, track duplication, instrument replacement, FX insertion/removal, and upon switching to the Design Tab via bottom bar or modal navigation.
    3. Ensured Design Tab left sidebar accurately reflects real active tracks, DSP engines, actual Audio FX and MIDI FX inserts (with authentic Eatscript code templates and parameters), and clip scripts.
    4. Enhanced target code caching (`targetCodeMap_` and `targetParamsMap_`) so user modifications in the Eatscript Code Tab persist across target switching.
    5. Updated `selectTargetByTrackAndType` with `fxIndex` support so clicking the "Design Chip" icon from `FullscreenDeviceModal` navigates directly to that device's actual code/preset script rather than falling back to an unrelated Tube Distortion template.
  - **Test/Validation**: `test_modular_ui.exe` Test 6b (`testDesignViewProjectSync`) validated dynamic updates, FX insert selection, code persistence, and track addition. `.\build.ps1 -Test` (45/45 suites pass), and `.\build-web.ps1 -NoServe` (clean Wasm build).

- [x] **Audio FX Preset Binding, Node Routing & WaveShaper Distortion Calibration** (commit `20fe776`, 2026-10-05)
  - **Context/Files**: `include/eatsbits/ui/widgets/track_properties_panel.hpp`, `src/ui/widgets/fullscreen_device_modal.cpp`, `src/ui/gui_window.cpp`, `src/audio/audio_engine.cpp`, `include/eatsbits/audio/graph/nodes/waveshaper_node.hpp`, `src/ui/widgets/plugin_search_dialog.cpp`, `src/ui/widgets/project_browser_drawer.cpp`, `tests/test_studio_fx.cpp`
  - **Acceptance Criteria**:
    1. Fix Audio FX Preset UI extraction: When adding an Audio FX from `PluginSearchDialog` or `ProjectBrowserDrawer`, load its `PresetDefinition` via `PresetManager::loadPresetDefinition` (matching instrument preset loading) and populate `TrackAudioFxItem::knobs` from the script's `def init()` and `def gui()`.
    2. Expand `TrackAudioFxItem::ensureDefaultKnobs()` with dedicated fallback layouts and knob definitions for `EQ` / `PARAMETRIC` (Low, Mid, High, Q, Gain), `LIMITER` (Ceiling, Release, Gain), `FILTER` / `SVF` (Cutoff, Reso, Type, Drive), and ensure devices do not inappropriately fall through to the Tube Distortion fallback (`drive`, `tone`, `bias`, `mix`).
    3. Wire parameter dispatch in `AudioEngine::setTrackAudioFxParam`:
       - Add routing for `ParametricEqNode` (frequency, gain, Q) and `LimiterNode` (ceiling, release, gain).
       - Add `tone` and `bias` parameter handling to `WaveShaperNode` (implementing a tilt/lowpass filter and DC bias offset in DSP).
    4. Fix WaveShaper saturation & volume ramp-up: In `WaveShaperNode::processBlock`, recalibrate input pre-gain boost and implement output makeup/wet gain compensation so that turning the Drive knob produces rich harmonic saturation/overdrive rather than behaving as a linear volume amplifier.
    5. Wire `projectBrowserDrawerWidget_->onAddAudioFx` in `GuiWindow` so adding FX cards from the Project Browser drawer inserts the effect into the active track.
  - **Test/Validation**: Unit tests in `test_studio_fx.cpp` and `test_modular_ui.cpp` verifying Audio FX knob generation, parameter routing, and distortion wave shaping.

- [x] **Web Audio Optimization Phase 4: AudioWorklet & Dedicated Wasm Worker Threading** (commit `2b441d9`, 2026-10-05)
  - **Context/Files**: `CMakeLists.txt`, `src/audio/audio_engine.cpp`, `build-web.ps1`
  - **Acceptance Criteria**:
    1. Configured miniaudio AudioWorklet integration with `-DMA_ENABLE_AUDIO_WORKLETS`, `-sAUDIO_WORKLET=1`, `-sWASM_WORKERS=1`, and `-sASYNCIFY` in `CMakeLists.txt`.
    2. Enabled `-sWASM_WORKERS=1` and `-fexceptions` across all compilation units, ensuring SharedArrayBuffer, atomics, and bulk-memory features are universally enabled.
    3. Provided pthread and sleep bridge stubs in `audio_engine.cpp` for Wasm Worker libc compatibility (`-lc-ww`).
    4. Verified real-time audio callback execution runs on a dedicated Web Audio worklet worker thread independent of the main JavaScript UI/WebGPU thread.
  - **Test/Validation**: Clean Web target build via `.\build-web.ps1 -NoServe` (8.16 MB raw, 2.25 MB gzip payload), and verified desktop test suite with `test_audio_realtime.exe`.

- [x] **Eatsbeats Parity: Dedicated Single Chord Track, Harmonic Track Follow Modes (BASS, CHORD, SCALE, COLOR) Playback Integration, MIDI FX Pipeline Synchronization & Piano Roll Chord Strip Removal** (commit `b8774cf`, 2026-10-05)
  - **Context/Files**: `include/eatsbits/ui/views/arranger_view.hpp`, `src/ui/views/arranger_view.cpp`, `include/eatsbits/ui/views/edit_view.hpp`, `src/ui/views/edit_view.cpp`, `include/eatsbits/sequencer/step_sequencer.hpp`, `src/sequencer/step_sequencer.cpp`, `include/eatsbits/audio/audio_engine.hpp`, `src/audio/audio_engine.cpp`, `include/eatsbits/eatscript/midi_fx_pipeline.hpp`, `include/eatsbits/ui/widgets/track_properties_panel.hpp`, `src/ui/widgets/track_properties_panel.cpp`, `tests/test_chord_track.cpp`, `tests/test_modular_ui.cpp`
  - **Acceptance Criteria**:
    1. **Dedicated Single Chord Track Parity (Eatsbeats Parity)**:
       - Eliminated arbitrary track chord leader / dynamic fallback routing in `ArrangerView` (`isChordLeader`, `cl.detectedChords` search in `getHarmonicOverviewChords` and `getActiveChordAtBar`).
       - Reverted to the single authoritative project `chordTrack_` lane on the Arranger timeline as the sole harmonic reference, matching original `eatsbeats/lib/models/daw_state.dart` and `eatsbeats/lib/ui/arranger_view.dart`.
       - Retained chord detection strictly as an explicit tool/action (`"Extract Chords to Chord Track"` from Audio-to-MIDI and clips) that writes directly to the canonical Chord Track lane.
    2. **Harmonic Track Follow Modes Playback Integration (`BASS`, `CHORD`, `SCALE`, `COLOR`)**:
       - Wired `track.chordFollowMode` into playback in `StepSequencer::processBlock()`.
       - When `track.chordFollowMode != ChordFollowMode::Off`, sampled `getActiveChordAtBar(currentBar)` from the project Chord Track. Dynamically remapped note and step pitches via `theory::ChordTheory::remapPitchForChord(pitch, *activeChord, mode)` during playback (matching `eatsbeats/lib/models/daw_state.dart:3295`).
       - Verified `[ ⚡ BAKE TO MIDI ]` commits the transformed pitches to clip notes and resets follow mode to `OFF`.
    3. **Active Chord Synchronization with Live MIDI FX Pipeline**:
       - Updated `timeContext_.activeChordRoot`, `quality`, `bass`, and `chordPitchClasses` in `StepSequencer` each step from the active chord on the project Chord Track.
       - Verified that `eatscript::MidiFxType::ChordFollow`, `ChordStabs`, `ChordArp`, and `.eats` MIDI FX scripts audibly modify and conform playback notes in real time.
    4. **Piano Roll Chord Strip & Selection Removal**:
       - Removed `chordStripBounds_`, `chordHeaderBadgeBounds_`, `detectedChords_`, and associated click/dialog handlers from `EditView` (Piano Roll), giving full vertical canvas height to piano keys gutter and note grid.
       - Centralized chord creation, selection, auditioning, and editing in the Arranger Chord Track lane and Circle of Fifths dialog.
  - **Test/Validation**: `test_chord_track.exe` (11/11 tests pass), `test_modular_ui.exe` (22/22 tests pass), `.\build.ps1 -Test` (45/45 suites pass), and `.\build-web.ps1 -NoServe` (clean Wasm build).

- [x] **Clip Rendering Parity (No Phantom Notes), Clip Duplicate & Delete Parity, Track Deletion Sidebar Fix & Project History Undo/Redo Integration**
  - **Context/Files**: `include/eatsbits/ui/widgets/track_properties_panel.hpp`, `src/ui/widgets/track_properties_panel.cpp`, `include/eatsbits/ui/widgets/track_properties_drawer.hpp`, `src/ui/widgets/track_properties_drawer.cpp`, `include/eatsbits/ui/views/arranger_view.hpp`, `src/ui/views/arranger_view.cpp`, `include/eatsbits/ui/views/track_inspector_view.hpp`, `src/ui/views/track_inspector_view.cpp`, `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `tests/test_modular_ui.cpp`
  - **Acceptance Criteria**:
    1. **Eliminate Phantom Note Rendering**: Removed default/fake note quad loop in `ArrangerView::renderClips`. Empty MIDI clips render cleanly with zero notes. Audio clips render stylized audio waveforms.
    2. **Track Deletion Clip Properties Fix**: Fixed bug where deleting a track caused Clip properties in Track Properties sidebar to disappear or stay blank. Resolved double track deletion bug between ArrangerView and GuiWindow, re-indexed all remaining clips' `trackIndex`, reset inspector tab to Track if selected clip becomes invalid, dynamically refreshed track list and clip fields, and auto-selected clip 0 upon entering Clip tab.
    3. **Clip Action Parity (`DUPLICATE` & `DELETE`)**: Added `DUPLICATE` and `DELETE` action buttons in Track Properties sidebar Clip section matching original Eatsbeats (`arranger_context_inspector.dart`). Implemented `duplicateClip` and `deleteClip` in `ArrangerView` and `GuiWindow`, and mapped Ctrl+D and Delete/Backspace hotkeys.
    4. **History Manager Integration**: Connected all Track Properties and Arranger operations to `diffHistory_.recordState(...)` via `recordProjectHistory` (track add, track delete, track duplicate, clip add, clip duplicate, clip delete, instrument selection, and FX changes). Updated `undoHistory()`, `redoHistory()`, and `jumpToHistoryIndex()` to call `syncArrangerFromSequencer()`, fully restoring Arranger tracks/clips, Mixer channel strips, and Track Inspector state.
  - **Test/Validation**: `test_modular_ui.exe` Test 22 validated all 6 sub-cases cleanly, and `.\build.ps1` completed with 0 errors.

- [x] **Project Browser Scroll Stability Fix & Script Sub-Sections Filter Parity with Flutter Icons**
  - **Issue Resolved**:
    1. **Arbitrary Scroll Jitter on Mouse Hover**: Clicking any tab or button in `ProjectBrowserDrawer` set `isDraggingScroll_ = true` unconditionally, and `GuiWindow::onMouseUp` did not dispatch `PointerAction::Up` to `projectBrowserDrawerWidget_`. As a consequence, `isDraggingScroll_` remained permanently active after clicking, causing every mouse hover/move over the drawer to compute delta from `dragStartY_` and arbitrarily shift `scrollOffset_`.
    2. **Flutter Eatsbeats Parity for Script Sub-sections**: Ported script category filter chips (`ALL`, `SYNTH` [Instruments], `AUDIO_FX` [FX], `MIDI_FX` [MIDI FX], `MIDI_SEQ` [Sequences], and `MACRO` [Macros]) with authentic category color accents and icons.
    3. **Flutter Stock Vector Icons**: Created 12 custom vector icons mimicking Flutter's built-in Material icons (`ui_tab_assets`, `ui_tab_script`, `ui_tab_preset`, `ui_tab_packs`, `ui_tab_projects`, `ui_tab_history`, `ui_cat_all`, `ui_cat_instruments`, `ui_cat_fx`, `ui_cat_midifx`, `ui_cat_seq`, `ui_cat_macro`).
    4. **Responsive Mobile Mode**: Added `isMobile` support (`screenWidth < 768px`) where tab buttons and filter chips display icon-only for maximum screen space efficiency.
  - **Test/Validation**: All 44 vector icons pass in `test_icon_system.exe`, and all 24 modular UI test suites in `test_modular_ui.exe` pass cleanly.

- [x] **Project Browser & Track Properties Side-by-Side Layout, Pointer Pass-Through Prevention & Mixer Top Bar Parity**
  - **Issue Resolved**:
    1. **Side-by-Side Panel Layout Parity**: In original Eatsbeats (`arranger_view.dart:2106`, `mixer_view.dart:124`), the Track Properties sidebar renders immediately to the left of the Project Browser (`right: isBrowserOpen ? 320.0 : 0.0`) rather than being rendered underneath. Populated `ctx.browserOffset` in `ViewContext`, fed it into `propertiesDrawer_.layout(bounds, ctx.browserOffset)` across `ArrangerView` and `MixerView`, and adjusted arranger timeline and mixer channel scroll bounds so that they cleanly terminate to the left of Track Properties and Project Browser.
    2. **Removed Fullscreen Dark Backdrop Shadow**: Removed the modal 40% black backdrop shadow overlay in `ProjectBrowserDrawer::render`, ensuring Project Browser and Track Properties function seamlessly as side-by-side workspace panels.
    3. **Project Browser Interaction Absorption**: In `GuiWindow::onMouseMove`, `onMouseDown`, `onMouseUp`, and `onMouseScroll`, guaranteed that whenever pointer events fall within `projectBrowserDrawerWidget_->getDrawerBounds()`, events are dispatched to the drawer and immediately absorbed (`return`), preventing any hover, click, or scroll events from passing through to underlying components. Also removed the click-outside-to-close behavior so the user can interact with the Track Properties sidebar to the left while keeping the Project Browser open.
    4. **Track Properties Interaction Absorption**: Ensured `TrackPropertiesDrawer::handlePointer` absorbs all clicks, drags, and hovers within `drawerBounds_` and `pullTabBounds_`, returning `true` so interaction never passes through to underlying arranger clips, track grids, or mixer channel strips.
    5. **Mixer View Top Bar Always Visible**: Enabled `showToolbar_ = true` by default in `MixerView`, rendered the top toolbar with a solid background and crisp divider line above channel strips, and ensured that clicking anywhere on the Mixer top bar absorbs the event without leaking through to underlying mixer faders or mute buttons.
  - **Test/Validation**: Verified with automated unit tests in `tests/test_modular_ui.cpp` checking side-by-side positioning with `browserOffset`, event absorption on `propertiesDrawer_`, `toolbarBounds_`, and `projectBrowserDrawerWidget_`, and permanent visibility of the Mixer top toolbar. All 24 modular UI test suites in `test_modular_ui.exe` passed cleanly.

- [x] **Track Creation Without Default Clips, Empty Grid Double Click Clip Creation, Mute/Solo Desync Fix & Track Properties Actions Parity** (commit `a92ec21`, 2026-10-05)
  - **Context/Files**: `include/eatsbits/audio/audio_engine.hpp`, `src/audio/audio_engine.cpp`, `include/eatsbits/sequencer/step_sequencer.hpp`, `src/sequencer/step_sequencer.cpp`, `include/eatsbits/ui/widgets/track_properties_panel.hpp`, `src/ui/widgets/track_properties_panel.cpp`, `include/eatsbits/ui/widgets/track_properties_drawer.hpp`, `src/ui/widgets/track_properties_drawer.cpp`, `include/eatsbits/ui/views/arranger_view.hpp`, `src/ui/views/arranger_view.cpp`, `include/eatsbits/ui/views/mixer_view.hpp`, `src/ui/views/mixer_view.cpp`, `include/eatsbits/ui/views/track_inspector_view.hpp`, `src/ui/views/track_inspector_view.cpp`, `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `tests/test_modular_ui.cpp`
  - **Acceptance Criteria**:
    1. New track/instrument creation creates a clean, clip-free track so users can place audio clips or MIDI clips freely.
    2. Double-click or double-touch on any empty area within a track row on the Arranger timeline creates an empty 4-bar clip aligned to that bar position.
    3. Fix mute/solo bug where newly created tracks could not be muted or soloed due to local ArrangerView tracks desynchronizing from `GuiWindow::arrangerTracks_`, `mixerStrips_`, and `AudioEngine::sequencer_`.
    4. Restore Eatsbeats parity action card at bottom of Track Properties panel (`+ ADD CLIP`, `DUPLICATE`, `DELETE` with 1-track minimum guard and header mute/solo buttons), functional across Arranger View drawer, Mixer View drawer, and Track Inspector View.
    5. Track deletion automatically cascades and updates/shrinks the mixer channel strips, audio engine sequencer patterns, audio channel nodes, and inspector selection.
  - **Test/Validation**: `test_modular_ui.exe` Test 21 validated clean end-to-end.

- [x] **Script Editor Scrolling, Gutter Scissoring, Immediate Cursor Blink Reset & Mobile Input Roadmap** (commit `7c134c4`, 2026-10-05)
  - **Context/Files**: `include/eatsbits/ui/widgets/text_editor_widget.hpp`, `src/ui/widgets/text_editor_widget.cpp`, `src/ui/gui_window.cpp`, `include/eatsbits/presenter/text_presenter.hpp`, `tests/test_modular_ui.cpp`, `web/index.html`
  - **Acceptance Criteria**:
    1. Wire mouse wheel scroll event routing in `GuiWindow::onMouseScroll` to `modularDesignView_` (`WorkspaceView::Design` / `ModularRack`) and `fullscreenDeviceModal_`, and support horizontal Shift-scroll in `TextEditorWidget::handlePointer`.
    2. Enforce strict scissoring (`r.pushScissor(gutterBounds_)` / `r.popScissor()`) on gutter line numbers and active line highlight in `TextEditorWidget` to eliminate line numbers bleeding above or below content borders during fractional scrolls.
    3. Implement `resetCursorBlink()` and track `lastCursor_` so that moving the cursor, clicking, selecting text, or typing immediately resets the blink timer to $t=0$ (solid 100% visibility) without lagging in the invisible phase.
    4. Implement the optimal Mobile Code Input Roadmap architecture:
       - In-engine `CodeAccessoryToolbar` with 27 touch-friendly coding buttons: `⇥` (Indent), `⇤` (Outdent), `( )`, `[ ]`, `{ }`, `"`, `'`, `:`, `=`, `+`, `-`, `*`, `/`, `.`, `,`, `_`, `#`, `->`, `param`, `ret`, `◀`, `▶`, `▲`, `▼`, `↶` (Undo), `↷` (Redo), `▶ RUN` (Compile & Run).
       - Smart auto-pairing and selection wrapping for `()`, `[]`, `{}`, `""`, `''`.
       - WebAssembly soft-keyboard bridge in `web/index.html` using a hidden proxy `<textarea>` with full `input` and `keydown` event streaming to `eats_on_web_char_input` and `eats_on_web_key_input`.
       - Automatic mobile context detection (`ctx.isMobile`) on Web (touch/agent/screen detection) and desktop.
  - **Test/Validation**: `.\build.ps1 -Test` (44/44 test suites passing) and `.\build-web.ps1 -NoServe` (clean Wasm build)

- [x] **Theme System Syntax Foundation, Micro-Lexer & Theme Browser Dialog** (commit `6033a26`)
  - **Context/Files**: `include/eatsbits/ui/theme.hpp`, `src/ui/theme.cpp`, `src/ui/widgets/text_editor_widget.cpp`, `include/eatsbits/ui/widgets/theme_browser_dialog.hpp`, `src/ui/widgets/theme_browser_dialog.cpp`, `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `tests/test_theme.cpp`
  - **Acceptance Criteria**:
    1. Expand `ThemeTokens` and `ThemeSeed` with full syntax highlighting token definitions (`syntaxKeyword`, `syntaxString`, `syntaxNumber`, `syntaxComment`, `syntaxFunction`, `syntaxIdentifier`, `syntaxOperator`, `syntaxType`) with automatic fallback derivation from primary/accent/text colors.
    2. Expand theme presets to 10 curated themes with rich culinary/Eatsbits naming inspired by iconic syntax themes: `AteTrack` (1980s Hardware), `MidnightBites` (Synthwave), `LightSnack` (Studio Daylight), `Breakfast` (Solarized Light), `Dinner` (Solarized Dark), `CountsBite` (Dracula), `NordicFrost` (Nord), `Catppuccino` (Catppuccin Mocha), `DarkRoast` (Monochrome Dark), and `SeaAndSalt` (DuoTone Dark).
    3. Implement zero-allocation in-place Eatscript micro-lexer in `TextEditorWidget` for keywords, types, strings, numbers, operators, comments, and functions, coloring code and minimap seamlessly with active theme tokens.
    4. Build modal `ThemeBrowserDialog` with categorized tabs (`ALL`, `DARK`, `LIGHT`, `SYNTAX PORTS`, `MINIMAL / DUOTONE`, `HARDWARE`), real-time search, 6-disc swatch palette previews, live theme preview on navigation, and escape-to-revert / enter-to-apply semantics.
    5. Integrate `+ BROWSE ALL THEMES` button into Settings dialog with full pointer and keyboard navigation routing.
  - **Test/Validation**: `ctest -C Release` (43/43 tests pass) and `.\build-web.ps1 -NoServe` (clean Wasm build)


- [x] **Terminal CLI Keyboard Focus Isolation & Monospace Text Editor Parity** (commit `699ae8e`)
  - **Context/Files**: `include/eatsbits/ui/widgets/terminal_console_drawer.hpp`, `src/ui/widgets/terminal_console_drawer.cpp`, `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `src/ui/widgets/text_editor_widget.cpp`, `tests/test_terminal_console.cpp`, `tests/test_modular_ui.cpp`
  - **Acceptance Criteria**:
    1. Implement `IFocusable` on `TerminalConsoleDrawer` and synchronize focus state with `FocusManager` upon expanding, collapsing, or pointer clicking in drawer bounds.
    2. Absorb non-universal keystrokes in `TerminalConsoleDrawer::handleKey` when focused/expanded to shield terminal typing from triggering DAW shortcuts (such as 'p' for Preset Dialog, 'b' for Browser, and Space for playback), while strictly preserving DAW universals (Escape to dismiss drawer, F11 for fullscreen).
    3. Transition `TextEditorWidget` text line and gutter rendering to `BatchRenderer2D::drawMonospaceText`, establishing 1:1 cell metric parity with the CLI tool and eliminating font metric cumulative drift and phantom whitespace at ends of lines.
    4. Validate unit test suites across `test_terminal_console.exe`, `test_modular_ui.exe`, and ensure WebAssembly clean compilation with `build-web.ps1 -NoServe`.
  - **Test/Validation**: `.\build.ps1 -Test` and `.\build-web.ps1 -NoServe`


- [x] **Text Editor Widget Refinement: Cursor Advance Alignment & Minimap Vertical Spacing** (commit `fa33fa6`)
  - **Context/Files**: `include/eatsbits/ui/widgets/text_editor_widget.hpp`, `src/ui/widgets/text_editor_widget.cpp`, `tests/test_modular_ui.cpp`
  - **Acceptance Criteria**:
    1. Align `charWidth_` in `TextEditorWidget` with actual font metrics (`getMonoCharAdvance(10.0f)` $\approx 7.01\text{px}$) instead of hardcoded `8.5f`, eliminating the cumulative drift where the cursor renders ~3 chars to the right and creates phantom trailing whitespace at the ends of lines.
    2. Refactor minimap line layout in `TextEditorWidget::renderMinimap` from full-height slot stretching (`minimapBounds_.h / lineCount`) to a compact fixed line pitch (e.g., 2.0px bar with 1.0px separator) anchored from the top, only scaling slot height down when document lines exceed minimap bounds, eliminating double-spaced gaps on shorter scripts.
    3. Verify cursor positioning, click-to-column hit testing (`coordFromPoint`), selection highlight bounds, and minimap lens scrubber alignment remain pixel-accurate in both `DESIGN > Code` and `EDIT > Script`.
  - **Test/Validation**: `.\build.ps1 -Test`

- [x] **Web Audio Optimization Phase 1: Callback Chunking & Buffer Frame Size Tuning** (commit `bc91f5f`)
  - **Context/Files**: `src/audio/audio_engine.cpp`, `src/gui_main.cpp`, `include/eatsbits/audio/audio_engine.hpp`
  - **Acceptance Criteria**: Refactor `AudioEngine::audioCallbackInternal` to process arbitrary `frameCount` requests in a loop of chunks up to `MAX_BLOCK_SIZE` so buffer sizes >= 2048 or odd period counts never truncate or leave silence in the output buffer; increase Emscripten default buffer frame size in `gui_main.cpp` from 512 to 1024 or 2048 to prevent audio scheduler underruns.
  - **Test/Validation**: `.\build.ps1 -Test` and `.\build-web.ps1 -NoServe`

- [x] **Web Audio Optimization Phase 2: WebAssembly SIMD & Compiler Optimization Flags** (commit `333edd5`)
  - **Context/Files**: `CMakeLists.txt`
  - **Acceptance Criteria**: Add `-msimd128` to Emscripten compile and link options for `eatsbits_web` and DSP core; strip debug flags and assertions (`-g -sASSERTIONS=1`) in release link flags, ensuring `-O3` and `-DNDEBUG` are applied to eliminate scalar math and validation overhead across voice engines and effects.
  - **Test/Validation**: `.\build-web.ps1 -NoServe`

- [x] **Web Audio Optimization Phase 3: Hardware Sample Rate Negotiation & Audio Unlock Fix** (commit `cb39c21`)
  - **Context/Files**: `src/audio/audio_engine.cpp`, `web/index.html`
  - **Acceptance Criteria**: Update `AudioEngine::initialize` so `polySynth_`, `tb303_`, and `masterMixer_` properly re-align when `impl_->device.sampleRate` negotiates the native browser hardware rate (e.g. 44.1 kHz vs 48 kHz), eliminating browser resampler jitter; fix `unlockAudio()` in `web/index.html` to iterate `window.miniaudio.devices` and call `resume()` on `dev.webaudio` instead of querying non-existent `device_instances`.
  - **Test/Validation**: `.\build.ps1 -Test` and `.\build-web.ps1 -NoServe`

- [x] **Mixer Architecture Phase 1: Pointer Delegation & Precedence Alignment**
  - **Context/Files**: `src/ui/gui_window.cpp`, `src/ui/views/mixer_view.cpp`, `include/eatsbits/ui/views/mixer_view.hpp`
  - **Acceptance Criteria**: Remove legacy `hitTestMixer` interception in `GuiWindow::onMouseButton`; delegate all mouse/pointer events directly to `modularMixerView_->handlePointer(pev, ctx)`; enforce strict sidebar drawer bounds isolation so clicks on `TrackPropertiesDrawer` never fall through to channels underneath; clip channel strip hit boundaries to `propertiesDrawer_.getPullTabBounds().x`.
  - **Test/Validation**: `.\build.ps1 -Test`

- [x] **Mixer Architecture Phase 2: Decouple Audio Graph DSP Nodes from Mixer Channels**
  - **Context/Files**: `src/ui/gui_window.cpp`, `src/ui/views/mixer_view.cpp`, `src/audio/audio_engine.cpp`
  - **Acceptance Criteria**: Remove canvas module scanning in `GuiWindow::updateMixerStrips()`; lock mixer channel strips strictly 1:1 with Arranger Tracks + Busses + Master Bus; eliminate phantom channels from internal/canvas `GainNode`s; verify Gain nodes remain fully functional as Track Audio FX inserts / utility devices.
  - **Test/Validation**: `.\build.ps1 -Test`

- [x] **Mixer Architecture Phase 3: Systematized Fader Presenters & Logarithmic dB Taper**
  - **Context/Files**: `src/ui/views/mixer_view.cpp`, `include/eatsbits/presenter/scalar_drag_presenter.hpp`, `src/presenter/scalar_drag_presenter.cpp`
  - **Acceptance Criteria**: Replace hardcoded magic-number pixel boxes with structured Thumb & Well hit geometry; standardize fader dragging with audio taper dB scaling (unity 0 dB at 0.75, +6 dB at 1.0, logarithmic down to -inf); support double-click to reset to unity, Shift+Drag for fine 0.1 dB trim, and right-click manual value entry dialog.
  - **Test/Validation**: `.\build.ps1 -Test`

- [x] **Mixer Architecture Phase 4: Mobile Responsive Density & Section Options Toolbar**
  - **Context/Files**: `include/eatsbits/ui/views/mixer_view.hpp`, `src/ui/views/mixer_view.cpp`
  - **Acceptance Criteria**: Implement modular density modes (`Comfortable` ~130px, `Compact` ~75px, `Micro` ~50px); add collapsible top toolbar with toggle pills for `[Meters]`, `[Routing/Inserts]`, `[Pan]`, `[Readouts]`; auto-switch to Micro/Compact mode when screen width < 700px (portrait) so at least 5-6 channels fit cleanly without clutter.
  - **Test/Validation**: `.\build.ps1 -Test`

- [x] **Mixer Architecture Phase 5: Arranger Docked Bottom Mixer Drawer**
  - **Context/Files**: `include/eatsbits/ui/widgets/arranger_mixer_drawer.hpp`, `src/ui/widgets/arranger_mixer_drawer.cpp`, `src/ui/views/arranger_view.cpp`, `src/ui/gui_window.cpp`
  - **Acceptance Criteria**: Add a collapsible sliding bottom mixer drawer to the Arranger tab (toggled via hotkey `M` or bottom nav/transport button, matching `VirtualKeyboardDrawer` pattern); channels align with tracks or scroll horizontally to let users mix and balance levels without leaving composition view.
  - **Test/Validation**: `.\build.ps1 -Test`

- [x] **Text Engine Phase 1: Headless TextDocument, Presenters & FocusManager**
  - **Context/Files**: `include/eatsbits/core/text_document.hpp`, `include/eatsbits/presenter/text_presenter.hpp`, `include/eatsbits/ui/input/focus_manager.hpp`, `tests/test_text_engine.cpp`
  - **Acceptance Criteria**: Multi-line buffer edits, undo/redo stack, selection range math, word navigation (`Ctrl+Arrows`, `Ctrl+Backspace`), and focus state transitions pass all unit assertions headlessly.
  - **Test/Validation**: `.\build.ps1 -Test` or running `ctest -R test_text_engine`

- [x] **Text Engine Phase 2: Core Platform Services, Scissoring & Clipboard**
  - **Context/Files**: `include/eatsbits/ui/views/view_base.hpp`, `include/eatsbits/ui/batch_renderer_2d.hpp`, `src/ui/gui_window.cpp`
  - **Acceptance Criteria**: `IClipboard` added to `ViewContext` with GLFW bridge, `pushScissor`/`popScissor` clipping in `BatchRenderer2D`, `handleChar` added to `ViewBase`, and input routed through `FocusManager` so typing never triggers DAW global shortcuts (like Spacebar playback).
  - **Test/Validation**: `.\build.ps1 -Test`

- [x] **Text Engine Phase 3: TextFieldWidget & Terminal Prompt Integration**
  - **Context/Files**: `include/eatsbits/ui/widgets/text_field_widget.hpp`, `src/ui/widgets/value_edit_dialog.cpp`, `src/ui/widgets/terminal_console_drawer.cpp`
  - **Acceptance Criteria**: Reusable single-line `TextFieldWidget` with horizontal auto-scroll containment (no text overflow outside bounds), drag/double-click selection, native clipboard (`Ctrl+C/V/X`), replacing raw text fields in `ValueEditDialog` and giving `TerminalConsoleDrawer` in-line cursor navigation and clipboard paste.
  - **Test/Validation**: `.\build.ps1 -Test`

- [x] **Text Engine Phase 4: TextEditorWidget with High-Performance Code Minimap**
  - **Context/Files**: `include/eatsbits/ui/widgets/text_editor_widget.hpp`, `src/ui/views/design_view.cpp`, `src/ui/views/edit_view.cpp`
  - **Acceptance Criteria**: Full multi-line script editor with line number gutter, active line highlight, syntax micro-bar minimap with wide touch/scrub lens (replacing thin vertical scrollbar), multi-line selection, block indentation, and `Ctrl+Enter` compile trigger wired into `DESIGN > Code` and `EDIT > Script`.
  - **Test/Validation**: `.\build.ps1 -Test`

- [x] **Icon Design Review**: Improve icon readability, consistency, and aesthetic appeal. (commit `e860f33`)
  - **Context/Files**: `src/ui/icon_registry.cpp`, `src/ui/widgets/icon_search_dialog.cpp`, `tests/test_icon_system.cpp`
  - **Acceptance Criteria**: Icons are clear, follow the design system, and work well at small sizes.
  - **Test/Validation**: `.\build.ps1 -Test`

- [x] **Transport and Track Properties Tooltips**: Add tooltips to header transport elements and Track Properties elements. (commit `cd2f33e`)

- [x] **'+ ADD' Dialog Scrolling and Filtering**: Refine '+ ADD' dialogs for instruments, FX, MIDI FX, etc. Scrolling confined within vertical layout bounds; filter text box implemented to search list in real time. (commit `6245673`)

- [x] **Autonomous Pipeline Setup**: Initialized autonomous refinement pipeline & todo-runner skill.
