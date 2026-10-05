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

*(Active queue empty. All pending items completed!)*

---

## 🕒 Recent Completions (Reference Context)

<!-- Keep the 5 most recent completed items here for immediate agent context; older items are archived in TODO-ARCHIVE.md -->

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


