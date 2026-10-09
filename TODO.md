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

- [ ] **Fix Desktop CLAP Host GUI Lifecycle & Window Compatibility (Cardinal & DPF Plugins)**
  - **Context/Files**: `include/eatsbits/audio/graph/nodes/clap_audio_node.hpp`, `src/audio/graph/nodes/clap_audio_node.cpp`, `src/ui/gui_window.cpp`, `src/gui_main.cpp`, `tests/test_clap_host.cpp`
  - **Problem Statement**:
    While headless instantiation and audio rendering work for CLAP plugins (including Cardinal), opening the GUI for Cardinal triggers an immediate crash. Cardinal is built on DPF (DISTRHO Plugin Framework) and creates its own OpenGL context bound to the host Win32 `HWND`. Incompatibilities in window styles, parent-vs-floating mode negotiation, thread affinity, or the message pump cause `wglCreateContext` or DPF's event loop to segfault.
  - **Acceptance Criteria**:
    1. **Strict Window API & Mode Negotiation**:
       - Query `clap_plugin_gui->is_api_supported(plugin, CLAP_WINDOW_API_WIN32, false)` (embedded) and `true` (floating).
       - If embedded is supported, create a dedicated Win32 host window configured with `WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS` and valid pixel format attributes suitable for OpenGL/DirectX rendering.
       - If only floating is supported, create a top-level `WS_OVERLAPPEDWINDOW` shell or invoke `clap_plugin_gui->set_parent(NULL)` / `clap_plugin_gui->show()`.
    2. **Main-Thread Affection & Message Pump Isolation**:
       - Guarantee all `gui->create()`, `gui->set_parent()`, `gui->show()`, `gui->hide()`, and `gui->destroy()` calls happen strictly on the designated main thread (`ClapAudioNode::getMainThreadId()`).
       - Ensure `pollAllMainThreadCallbacks()` processes plugin UI timer requests and host resize callbacks (`request_resize`, `request_show`) without deadlocking GLFW or the WebGPU render loop.
    3. **Dimension Negotiation & HiDPI Scaling**:
       - Call `gui->get_size(&w, &h)` and `gui->set_size(w, h)` to properly size the container before invoking `gui->show()`.
       - Handle plugin dynamic resizing via `clap_host_gui::request_resize`.
    4. **Safety Wrappers & Generic Parameter Inspector Fallback**:
       - Add structured exception / guard checks around `gui->show()` and window creation.
       - If a plugin's GUI fails to load, gracefully catch the error and fall back to Eatsbits' generic parameter slider UI in `TrackPropertiesPanel` instead of terminating the host process.
    5. **Automated Verification**:
       - Update `tests/test_clap_host.cpp` to test Cardinal GUI lifecycle (`create` -> `show` -> `hide` -> `destroy`) headlessly and in windowed mode.

- [ ] **DaisySP Integration: Permissive (MIT), WebAssembly-Ready Modular DSP Engine & FX Suite**
  - **Context/Files**: `third_party/daisysp/` (new), `include/eatsbits/audio/graph/nodes/daisy/`, `src/audio/graph/nodes/daisy/`, `include/eatsbits/audio/audio_engine.hpp`, `src/audio/audio_engine.cpp`, `include/eatsbits/eatscript/api_manifest.hpp`, `CMakeLists.txt`, `tests/test_daisysp_nodes.cpp` (new)
  - **Rationale**:
    Eatsbits requires a rich collection of modular synthesis and audio effects that run with desktop/web/mobile parity. External plugins (CLAP) do not function in WebAssembly, and Surge XT/VCV Rack code is copyleft GPLv3. DaisySP (by Electro-Smith) is a battle-tested, high-performance C++ DSP library used in Eurorack hardware, licensed under **100% MIT**, with zero runtime memory allocations and zero OS dependencies.
  - **Acceptance Criteria**:
    1. **Vendor DaisySP into Codebase**:
       - Vendor official MIT-licensed `DaisySP` source/headers into `third_party/daisysp/`.
       - Configure `CMakeLists.txt` to compile DaisySP as an internal static library linked into `eats_audio`, ensuring compatibility with MSVC, Clang, GCC, and Emscripten (WebAssembly).
    2. **Implement Core Modular `GraphNode` Wrappers**:
       - **Oscillators (`DaisyOscillatorNode`)**:
         - Wrap `daisysp::Oscillator` (band-limited blip saw, square, triangle, sine, variable saw) and `daisysp::Wavetable` with sample-rate aware initialization and 1V/Oct CV / pitch modulation.
       - **Filters (`DaisyFilterNode`)**:
         - Wrap `daisysp::MoogLadder` (analog-modeled 24dB 4-pole lowpass) and `daisysp::Svf` (State Variable Filter: LP, HP, BP, Notch) with smooth cutoff, resonance, and drive controls.
       - **Reverb & Delay (`DaisyReverbNode`, `DaisyDelayNode`)**:
         - Wrap `daisysp::ReverbSc` (lush stereo algorithmic plate/hall reverb) and `daisysp::DelayLine` with damping, feedback, and stereo width.
       - **Modulation & Dynamics (`DaisyModulationNode`, `DaisyCompressorNode`)**:
         - Wrap `daisysp::Chorus`, `daisysp::Flanger`, `daisysp::Overdrive`, and `daisysp::Compressor`.
    3. **AudioEngine & Track FX Rack Integration**:
       - Wire new DaisySP nodes into `AudioEngine::rebuildTrackAudioFx` to populate empty effect categories (`REVERB`, `DELAY`, `CHORUS`, `FLANGER`, `MOOG_FILTER`, `COMPRESSOR`), mapping UI knob values to normalized parameters.
    4. **Eatscript & AOT Transpiler Manifest Binding**:
       - Register the new nodes in `include/eatsbits/eatscript/api_manifest.hpp` under `eats.dsp.*` and `eats.fx.*`.
       - Enable programmatic live scripting in Eatscript (e.g. `track.add_fx(eats.fx.reverb_sc(decay=0.85, damp=0.5))`).
    5. **Automated Unit Testing & Web Build Validation**:
       - Create `tests/test_daisysp_nodes.cpp` verifying:
         - Zero runtime allocations in `processBlock()`.
         - Valid float output ranges (no NaNs, no infinities) across extreme parameter sweeps.
         - Impulse responses for reverb and filter cutoff sweeps.
       - Verify compilation with `.\build.ps1 -Test -NoRun` and validate WebAssembly build target compatibility.

---

## 🕒 Recent Completions (Reference Context)

<!-- Keep the 5 most recent completed items here for immediate agent context; older items are archived in TODO-ARCHIVE.md -->

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
    1. **Unify Multi-View Chord Follow State**:
       - Wired `modularTrackInspectorView_->onChordFollowChanged` in `GuiWindow::initModularViews()`: mode changes update both `arrangerTracks_[idx].chordFollowMode` and `modularArrangerView_->getTracks()[idx].chordFollowMode`, and invoke `engine_->getSequencer().getTrack(idx)->setChordFollowMode(mode)`.
       - Updated `GuiWindow::handleTrackInspectorInteraction(TrackInspectorHitArea::ChordFollowChip)` to propagate mode changes to `modularArrangerView_`, `modularTrackInspectorView_`, and `SequencerTrack`.
       - Added `trackChordFollowModes` synchronization to `TrackInspectorView::syncFromWindow`.
    2. **Synchronize Arranger Chord Track Lane to StepSequencer**:
       - Added `onChordTrackChanged` callback to `ArrangerView`, invoked on chord adding, editing, deleting, progression preset application, and chord dragging.
       - Wired `modularArrangerView_->onChordTrackChanged` in `GuiWindow` to immediately push `modularArrangerView_->getChordTrack()` to `engine_->getSequencer().setChordTrack(...)`, ensuring `StepSequencer::getActiveChordAtBar()` always has the latest progression.
    3. **Auditioning & Note Preview Harmony Support**:
       - In `AudioEngine::postNoteOn` and `postNoteOff`, check if the destination track has `chordFollowMode != ChordFollowMode::Off`. If an active chord is present on the chord track, remap preview pitch through `theory::ChordTheory::remapPitchForChord` so Piano Roll, Virtual Keyboard, and Tracker auditioning audibly reflect the configured mode.
    4. **Automated Verification**:
       - Added unit test coverage in `tests/test_chord_track.cpp` (`testChordFollowPipelineSyncAndAuditioning`) and `tests/test_modular_ui.cpp` (`testGuiWindowIntegration`), validating 100% test pass across all 48 test suites.

- [x] **Vector Graphics UI Modernization & Procedural Texture Removal** (2026-10-09)
  - **Context/Files**: `include/eatsbits/ui/svg_path.hpp`, `src/ui/svg_path.cpp`, `src/ui/gui_window.cpp`, `src/ui/gui_panel_def.cpp`, `include/eatsbits/ui/procedural_texture_system.hpp` (deleted), `src/ui/procedural_texture_system.cpp` (deleted), `tests/test_vector_surfaces.cpp`, `tests/test_procedural_textures.cpp` (deleted), `CMakeLists.txt`, `src/ui/views/mixer_view.cpp`
  - **Acceptance Criteria**:
    1. **Eliminated Procedural Texture Cruft**: Completely removed `ProceduralTextureSystem` and `ChassisTextureSystem`, eliminating per-pixel CPU noise generators, redundant 256x256 GPU texture allocations, and heavy per-frame `wgpuQueueWriteTexture` uploads for chassis and buttons.
    2. **Pure Vector SVG Triangulation**: Removed `SvgTextureCache` raster caching. Triangulated SVG paths directly to vector triangle vertex batches via `SvgPath::triangulate()` and `BatchRenderer::drawTriangle()`, producing crisp resolution-independent vector rendering with 0 GPU texture uploads.
    3. **Modern Vector DAW Aesthetic**: Replaced tiled noise chassis plates and button backgrounds with sleek vector gradients (`drawRoundedRectGradient`), crisp 1px specular bevel highlights, and hairline divider lines (inspired by Ableton Live, FL Studio, and Bitwig Studio).
    4. **Mixer View Hit-Test Ordering Fix**: Fixed hit-test priority in `MixerView` where pan knob hit-testing was intercepting clicks before Mute/Solo button checks.
    5. **Automated Unit Testing**: Created `test_vector_surfaces.cpp` verifying all 14 `GuiChassisStyle` surfaces, custom color tints, wood cheeks, and minimal white vector faceplates across full headless passes.
  - **Test/Validation**: `test_vector_surfaces.exe` (10/10 tests pass), all 48 test suites passing (`.\build.ps1 -Test -NoRun`), 0 compile warnings or errors.

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
- [ ] **Phase 4: Desktop CLAP Host Node in AudioGraph**
  - Implement `ClapAudioNode` for loading 3rd-party instruments/FX into track mixer channels.
  - Connect to `PluginSearchDialog` and support floating native OS windows and generic parameter inspector.
