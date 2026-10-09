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

- [ ] **Fix Live MIDI FX Pipeline Synchronization, Parameter Binding & Inspection**
  - **Context/Files**: `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `include/eatsbits/ui/views/track_inspector_view.hpp`, `src/ui/views/track_inspector_view.cpp`, `include/eatsbits/ui/views/arranger_view.hpp`, `src/ui/views/arranger_view.cpp`, `include/eatsbits/eatscript/midi_fx_pipeline.hpp`, `src/sequencer/step_sequencer.cpp`, `tests/test_midi_fx.cpp`
  - **Acceptance Criteria**:
    1. **Bridge Inspector and Arranger MIDI FX Model Lists**:
       - Resolve the disconnect between `TrackInspectorView::tracks_[trackIndex].midiFxList` and `ArrangerView::tracks_[trackIndex].midiFx`. When a MIDI FX is added, removed, reordered, or toggled in Track Inspector, mirror changes to `modularArrangerView_->getTracks()[trackIndex].midiFx` before calling `syncTrackMidiFxToEngine(trackIndex)` (preventing the engine from receiving an empty rack).
    2. **Populate Complete MIDI FX Parameters in `syncTrackMidiFxToEngine`**:
       - Update `GuiWindow::syncTrackMidiFxToEngine` to serialize the full set of parameters from `TrackMidiFxItem::knobs` into `insert.params` (e.g. `Octaves`, `semitones`, `rate`, `gate`, `swing`, `humanizeVelocity`, `humanizeTiming`), enabling `StepSequencer` to read them during playback.
    3. **Wire Parameter Updates in GuiWindow**:
       - Wire `modularTrackInspectorView_->onMidiFxParamChanged` in `GuiWindow::initModularViews()` to update the active rack on `engine_->getSequencer().getTrack(idx)`.
       - Update `GuiWindow::handleTrackInspectorInteraction` (knobs for Arp, Scale, Humanize) to call `syncTrackMidiFxToEngine(tIdx)`.
       - Connect `fullscreenDeviceModal_.onMidiFxParamChanged` to dispatch parameter values directly to the active `SequencerTrack::getMidiFxRack()`.
    4. **Automated Verification**:
       - Add regression tests in `tests/test_midi_fx.cpp` ensuring that adding an Arpeggiator or Scale Snap effect in the Inspector audibly alters playback notes in `StepSequencer::processBlock`.

- [ ] **Audio FX Engine Overhaul: Multi-Insert Routing, Distortion Calibration & Node Restoration**
  - **Context/Files**: `include/eatsbits/audio/audio_engine.hpp`, `src/audio/audio_engine.cpp`, `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `src/ui/views/track_inspector_view.cpp`, `include/eatsbits/audio/graph/nodes/waveshaper_node.hpp`, `include/eatsbits/audio/graph/nodes/biquad_node.hpp`, `include/eatsbits/audio/graph/nodes/eatscript_node.hpp`, `tests/test_studio_fx.cpp`
  - **Acceptance Criteria**:
    1. **Bridge Inspector Audio FX to Engine & Support Multi-Insert Param Dispatch**:
       - Mirror `TrackInspectorView::tracks_[trackIndex].audioFxList` into `modularArrangerView_->getTracks()[trackIndex].audioFx` so `syncTrackAudioFxToEngine` builds the actual FX chain rather than an empty list.
       - Replace hardcoded `fxIndex = 0` in `modularTrackInspectorView_->onAudioFxParamChanged` with the actual index of the edited effect.
    2. **Fix Track 1 Dual Audio Path & Initial Delay Disconnect**:
       - In `AudioEngine::rebuildTrackAudioFx`, safely disconnect any existing default intermediate nodes between `srcId` and `gainId` (including default `AcidEcho` delay on Track 1). Ensure `gainId` has only a single clean input path, eliminating parallel summing, phase cancellation, volume boost, and clipping distortion on Track 1.
    3. **Stop Defaulting Unrecognized Effects to Tube Distortion**:
       - Instantiate `BiquadNode` in `rebuildTrackAudioFx` for all `FILTER`, `MULTIMODE_FILTER`, `SVF`, `LOWPASS`, `HIGHPASS`, `BANDPASS`, and `VCF` effects, mapping `cutoff`, `resonance`, and `type` in `setTrackAudioFxParam`.
       - Instantiate `EatscriptNode` for script-based effects (`.eats` FX), loading and compiling the DSP script.
       - Restrict `WaveShaperNode` instantiation strictly to explicit distortion, overdrive, saturation, or bitcrusher types.
    4. **Calibrate `WaveShaperNode` Transfer Curve & Gain Staging**:
       - Replace the foldback curve in the negative quadrant `d / (1.0f + d * d)` with monotonic soft saturation (e.g., dual tanh or asymmetric cubic curve) to eliminate unmusical wavefolding distortion when driven with hot signals.
       - Normalize default pre-gain/drive in `TrackAudioFxItem` and `rebuildTrackAudioFx` so effects default to subtle warmth/color rather than aggressive overdrive.
    5. **Automated Verification**:
       - Expand `tests/test_studio_fx.cpp` to verify filter node creation, multi-insert parameter dispatch (insert index > 0), and clean distortion-free routing on Track 1.

- [ ] **Remove Eatsbeats Settings & Project Hub Texture Options**
  - **Context/Files**: `include/eatsbits/ui/gui_window.hpp`, `src/ui/gui_window.cpp`, `include/eatsbits/ui/views/design_view.hpp`, `src/ui/views/design_view.cpp`, `tests/test_gui_interaction.cpp`, `tests/test_vector_surfaces.cpp`
  - **Acceptance Criteria**:
    1. **Eliminate Obsolete Texture Settings from Project Hub**:
       - Remove the legacy texture options in Project Hub / Settings (Section 3), including the material texture chips (Dark Chassis, Brushed Metal, Powder Coat, Crinkle, Bakelite, Walnut, Rosewood, PCB Green, Carbon), Chassis Rotation (0° vs 90° Vertical), and Surface Wear & Patina levels (Pristine, Mint, Patina, Vintage).
       - Streamline the Project Hub drawer sections from 7 to 6 (or repurpose Section 3 into a clean UI Accent & Color Theme selector), updating `secTitles`, drawer layout calculations, and section icons.
    2. **Purge Texture Action Enums & State**:
       - Remove obsolete enum values from `ProjectHubAction` (`SetPanelTexture`, `SetPanelWear`, `SetPanelRotation`).
       - Remove unused chassis texture state variables from `GuiWindow` (`panelChassisTexture_`, `panelChassisWear_`, `panelChassisRotation_`) and their associated setters (`setPanelChassisTexture`, `setPanelChassisWear`, `setPanelChassisRotation`).
    3. **Simplify Chassis Plate Rendering**:
       - Streamline `drawChassisPlate` signatures in `GuiWindow` to remove unused `panelChassisTexture_`, `panelChassisWear_`, and `panelChassisRotation_` parameters, rendering sleek, uniform vector gradients and 1px specular bevels directly.
    4. **Clean Up Hit Testing & Event Handlers**:
       - Excised all material chip, rotation toggle, and patina level hit-testing blocks from `hitTestProjectHub()` and `onMouseDown()` in `GuiWindow`.
    5. **Test Suite Verification**:
       - Update `tests/test_gui_interaction.cpp` to align with the streamlined Project Hub section indices and actions.
       - Verify full compilation and run the test suite via `.\build.ps1 -Test -NoRun` (all 48 suites passing).

---

## 🕒 Recent Completions (Reference Context)

<!-- Keep the 5 most recent completed items here for immediate agent context; older items are archived in TODO-ARCHIVE.md -->

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
