# Eatsbeats to Eatsbits C++20 Porting & Parity Tracker

This document provides a comprehensive inventory, architectural mapping, and live progress tracker for porting all functionality from the original Flutter DAW (**Eatsbeats**, `c:\git\eatsbeats`) into the high-performance native C++20 workstation (**Eatsbits**, `c:\git\eatsbits`).

---

## 1. Architectural Mission & Constraints

1. **Native Performance First**:
   - Zero-allocation audio render loop: Real-time audio processing must never allocate heap memory, acquire non-try mutex locks, or execute file I/O on the audio thread.
   - NanoVG + Google Dawn (WebGPU) vector rendering pipeline targeting 60+ FPS at 4K HiDPI.
2. **Modular Subsystem Architecture**:
   - Decoupled view base classes (`ViewBase`, `ArrangerView`, `EditView`, `TrackInspectorView`, `MixerView`, `DesignView`).
   - Reusable standalone widgets and dialogs (`ProjectBrowserDrawer`, `VirtualKeyboardDrawer`, `DrumPadGridWidget`, `CommandPaletteDialog`, `CircleOfFifthsDialog`, `ValueEditDialog`).
3. **Clean Process Hygiene**:
   - Strictly avoid orphan background server processes.
   - Synchronous, fast incremental compilation via `build.ps1 -NoRun`.
   - Comprehensive automated regression testing targeting 100% test assertion passes.

---

## 2. Completed Milestones & Subsystem Inventory

| Subsystem | Original Flutter File(s) | Native C++20 Implementation | Status | Validation Suite |
| :--- | :--- | :--- | :---: | :--- |
| **Arranger Timeline & Clips** | `lib/ui/arranger_view.dart`<br>`lib/models/daw_state.dart` | [`ArrangerView`](file:///c:/git/eatsbits/include/eatsbits/ui/views/arranger_view.hpp) | ✅ Complete | `testArrangerView` |
| **Piano Roll & Step Editor** | `lib/ui/piano_roll_view.dart`<br>`lib/models/track_model.dart` | [`EditView`](file:///c:/git/eatsbits/include/eatsbits/ui/views/edit_view.hpp) | ✅ Complete | `testEditView` |
| **Tracker Sub-View** | `lib/ui/tracker_view.dart` | [`EditView` (Tracker mode)](file:///c:/git/eatsbits/include/eatsbits/ui/views/edit_view.hpp) | ✅ Complete | `testEditView` |
| **Classical Score Notation** | `lib/ui/score/score_view.dart`<br>`score_vector_glyphs.dart` | [`EditView` (Score mode)](file:///c:/git/eatsbits/include/eatsbits/ui/views/edit_view.hpp) | ✅ Complete | `testEditView` |
| **Track Inspector & Routing** | `lib/ui/track_inspector_view.dart` | [`TrackInspectorView`](file:///c:/git/eatsbits/include/eatsbits/ui/views/track_inspector_view.hpp) | ✅ Complete | `testTrackInspectorView` |
| **Modular 8-Ch Studio Mixer** | `lib/ui/mixer_view.dart` | [`MixerView`](file:///c:/git/eatsbits/include/eatsbits/ui/views/mixer_view.hpp) | ✅ Complete | `testMixerView` |
| **Modular Node Graph & Rack** | `lib/ui/modular/modular_rack.dart` | [`DesignView`](file:///c:/git/eatsbits/include/eatsbits/ui/views/design_view.hpp) | ✅ Complete | `testDesignView` |
| **Eatscript Code IDE & VM** | `lib/ui/script_view.dart`<br>`lib/eatscript/eats_vm.dart` | [`EatscriptVm`](file:///c:/git/eatsbits/include/eatsbits/eatscript/eatscript_vm.hpp) | ✅ Complete | `test_eatscript_vm` |
| **Virtual Instrument Drawer** | `lib/ui/widgets/virtual_keyboard_drawer.dart` | [`VirtualKeyboardDrawer`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/virtual_keyboard_drawer.hpp) | ✅ Complete | `testVirtualKeyboardDrawer` |
| **16-Pad MPC/SP-1200 Matrix** | `lib/ui/widgets/drum_pad_grid_widget.dart` | [`DrumPadGridWidget`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/drum_pad_grid_widget.hpp) | ✅ Complete | `testDrumPadGridWidget` |
| **Project Browser Drawer** | `lib/ui/widgets/project_browser_drawer.dart` | [`ProjectBrowserDrawer`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/project_browser_drawer.hpp) | ✅ Complete | `testProjectBrowserDrawer` |
| **Command Palette Runner** | `lib/ui/widgets/command_palette_dialog.dart`<br>`command_palette_registry.dart` | [`CommandPaletteDialog`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/command_palette_dialog.hpp) | ✅ Complete | `testCommandPaletteDialog` |
| **Circle of Fifths Modal** | `lib/ui/widgets/circle_of_fifths_dialog.dart` | [`CircleOfFifthsDialog`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/circle_of_fifths_dialog.hpp) | ✅ Complete | `test_chord_track` |
| **Plugin Search & Library** | `lib/ui/widgets/plugin_search_dialog.dart` | [`PluginSearchDialog`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/plugin_search_dialog.hpp) | ✅ Complete | `test_modular_ui` |
| **Value Edit Numpad Modal** | `lib/ui/widgets/compact_value_dialog.dart` | [`ValueEditDialog`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/value_edit_dialog.hpp) | ✅ Complete | `testValueEditDialogAndBackdropBlur` |
| **GPU Backdrop Blur Filter** | Direct NanoVG / Framebuffer blur | [`BatchRenderer2D::applyBackdropBlur`](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp) | ✅ Complete | `testValueEditDialogAndBackdropBlur` |
| **Analog Synthesis Engines** | `lib/audio/acid303_worklet.dart`<br>`drum/*`, `dx7_fm_engine.dart` | TB-303, TR-808, TR-909, DX7, SNES, SID, YM2612 | ✅ Complete | `test_audio_graph`, `test_drum_synths` |
| **Master Convolution Reverb** | `lib/audio/convolver_engine.dart`<br>`procedural_ir_generator.dart` | [`ConvolverNode`](file:///c:/git/eatsbits/include/eatsbits/audio/graph/nodes/convolver_node.hpp)<br>[`ProceduralIRGenerator`](file:///c:/git/eatsbits/include/eatsbits/audio/procedural_ir_generator.hpp) | ✅ Complete | `test_convolver` |
| **Non-Destructive Diff History** | `lib/models/history_manager.dart` | [`DiffHistory`](file:///c:/git/eatsbits/include/eatsbits/sequencer/diff_history.hpp) | ✅ Complete | `test_diff_history` |
| **Master WAV Audio Bouncer** | `lib/audio/wav_exporter.dart` | [`WavWriter`](file:///c:/git/eatsbits/include/eatsbits/audio/wav_writer.hpp) | ✅ Complete | `test_wav_export` |
| **Audio-to-MIDI Transcriber** | `lib/audio/audio_to_midi_engine.dart`<br>`lib/ui/audio_to_midi_dialog.dart` | [`AudioToMidiEngine`](file:///c:/git/eatsbits/include/eatsbits/audio/audio_to_midi_engine.hpp)<br>[`AudioToMidiDialog`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/audio_to_midi_dialog.hpp) | ✅ Complete | `test_audio_to_midi` |
| **Procedural Music Architect** | `lib/audio/procgen/*`<br>`song_archetype.dart` | [`ProceduralSongEngine`](file:///c:/git/eatsbits/include/eatsbits/procgen/procedural_song_engine.hpp)<br>[`SongArchetypeRegistry`](file:///c:/git/eatsbits/include/eatsbits/procgen/song_archetypes.hpp) | ✅ Complete | `test_procgen` |
| **SoundFont2 Sample Engine** | `lib/audio/soundfont_decoder.dart`<br>`soundfont_engine.dart` | [`SoundFontDecoder`](file:///c:/git/eatsbits/include/eatsbits/audio/soundfont/soundfont_decoder.hpp)<br>[`SoundFontNode`](file:///c:/git/eatsbits/include/eatsbits/audio/graph/nodes/soundfont_node.hpp) | ✅ Complete | `test_soundfont` |
| **Track Freeze & Bounce Engine** | `lib/audio/track_freeze_engine.dart` | [`TrackFreezeEngine`](file:///c:/git/eatsbits/include/eatsbits/audio/track_freeze_engine.hpp) | ✅ Complete | `test_track_freeze` |

---

## 3. Remaining Feature Backlog & Porting Tracker

### Subsystem 1: Audio-to-MIDI Transcription Engine & Dialog
- [x] **1.1 Onset & Transient Detector (`audio_to_midi_engine.hpp`)**:
  - Energy difference onset detection across sub-bands.
  - Spectral flux and phase deviation analysis for percussive strike localization.
- [x] **1.2 Monophonic/Polyphonic Pitch Tracker**:
  - YIN / Autocorrelation pitch detection algorithm for melodic audio tracks.
  - Note threshold, minimum duration filtering, and octave snapping.
- [x] **1.3 Audio-to-MIDI Modal Dialog (`audio_to_midi_dialog.hpp`)**:
  - Modal window for audio file loading, waveform audition preview, detection sensitivity sliders, quantization grid selector, and target track assignment.
- [x] **1.4 UI & Project Integration**:
  - Connect "AUDIO TO MIDI CONVERTER" button in `ProjectBrowserDrawer` tab 4/6.
  - Add `action.audio_to_midi` to `CommandPaletteDialog`.

### Subsystem 2: Algorithmic Procedural Music & Song Generation Engine
- [x] **2.1 Song Archetypes & Blueprint Registry (`song_archetypes.hpp`)**:
  - Genre structures: Acid Techno, Synthwave, Lofi Hip Hop, Cyberpunk Electro, Ambient Drone, SNES 16-Bit Adventure, C64 SID Chiptune.
  - Form blueprints: Intro -> Verse -> Chorus -> Breakdown -> Drop -> Outro.
- [x] **2.2 Procedural Component Generators**:
  - `ProceduralAcidEngine`: 16-step TB-303 algorithmic sequence generator with accent, slide, and gate rules.
  - `ProceduralDrumEngine`: Polyrhythmic 808/909 drum groove generator with velocity humanization.
  - `ProceduralPianoEngine`: Voice-leading harmonic chord progressions, jazz voicings, and arpeggios.
- [x] **2.3 Multi-Track Procedural Ensemble Engine (`procedural_ensemble_engine.hpp`)**:
  - Orchestrates bassline, drums, chords, and lead simultaneously to guarantee scale and rhythm cohesion.
- [x] **2.4 Procedural Song Engine (`procedural_song_engine.hpp`)**:
  - Generates full arrangements and populates tracks and clips across the timeline.
  - Hook into `CommandPaletteDialog` and `ProjectBrowserDrawer` macros tab.

### Subsystem 3: Procedural Impulse Response (IR) Generator
- [x] **3.1 Synthetic Acoustic Space Generator (`procedural_ir_generator.hpp`)**:
  - Room, Hall, Plate, Spring, Cathedral, and Non-Linear Gated Reverb impulse synthesis.
  - Frequency-dependent damping, early reflections scattering, and late diffuse reverberation tails.
  - Eliminates the need for external static `.wav` impulse response sample dependencies.
- [x] **3.2 Convolver Node Integration**:
  - Real-time procedural parameter updates (decay time, room size, high damping, pre-delay) directly into `ConvolverNode`.

### Subsystem 4: SoundFont2 (SF2) Sample Engine
- [x] **4.1 SoundFont2 Parser (`soundfont_decoder.hpp`)**:
  - RIFF chunk parser for SF2 files: Hydrag chunks (`PHDR`, `PBAG`, `PMOD`, `PGEN`, `INST`, `IBAG`, `IMOD`, `IGEN`, `SHDR`) and sample PCM data.
- [x] **4.2 Multitimbral Sample Player Node (`soundfont_node.hpp`)**:
  - Multi-zone velocity and key split mapping.
  - Loop point interpolation and ADSR envelope modulation per zone.

### Subsystem 5: Track Freeze & Background Audio Bouncing
- [x] **5.1 Track Freeze Engine (`track_freeze_engine.hpp`)**:
  - Fast-render offline synthesis for individual tracks into stereo 32-bit float memory buffers.
  - Dynamic bypass of heavy synth and FX nodes when frozen to reclaim real-time CPU.
- [x] **5.2 Track Inspector & Header Integration**:
  - Freeze indicator icon and toggle button on track headers and Inspector sidebar.

### Subsystem 6: Specialized Creative UI Dialogs & Scopes
- [x] **6.1 Interactive Waveshaper Transfer Curve Editor (`waveshaper_dialog.hpp`)**:
  - Visual spline node canvas for designing non-linear distortion curves.
  - 5 transfer shapes (Soft Tanh, Tube Asymmetric, Sine Wavefold, Angry 1, Angry 2).
  - Real-time 8-band harmonic spectrum analyzer via fast Fourier transform probe.
- [x] **6.2 Note Splitter Modal Dialog & Engine (`note_splitter_engine.hpp`, `note_splitter_dialog.hpp`)**:
  - Chord voice distributor (splits polyphonic chords into separate tracks for Lead, Harmony, Bass).
  - 4 split modes: 3-Way Voice Skyline, 2-Way Piano Clefs, 4-Voice SATB polyphony, and GM Drum Demuxer.
  - Live output track preview and automated track creation in `StepSequencer`.
- [x] **6.3 Studio Track Color Picker (`color_picker_dialog.hpp`)**:
  - 4 curated Eatsbeats palette categories (Neon & Cyberpunk, Classic Synth & Studio, Vibrant Palette, Pastels & Subtle).
  - Interactive HSL sliders, hex format display (`#RRGGBB`), and Before/After preview swatches.
- [x] **6.4 Stereo Field Scope & Goniometer (`space_visualizer_widget.hpp`)**:
  - Real-time Lissajous phase correlation scope, $-1.0 \leftrightarrow +1.0$ meter, L/R balance, and M/S ratio readout.
  - 2.5D acoustic perspective room with draggable sound source/listener and reflection rays.

### Subsystem 7: AI Services & Intelligent DAW Assistant
- [x] **7.1 AI Assistant Chat Dialog (`ai_assistant_dialog.hpp`)**:
  - Floating natural language dialog with prompt history and code snippet injection.
- [x] **7.2 AI Auto-Mixing Engine (`ai_mixing_engine.hpp`)**:
  - Algorithmic gain-staging, LUFS target leveling, and spectral unmasking.
- [x] **7.3 Gemini API Client Bridge (`gemini_client.hpp`)**:
  - Native asynchronous HTTP/REST client for Google Gemini API integration.

### Subsystem 8: Synchronized Lyric Track & Speech Vocalizer
- [x] **8.1 Lyric Marker Model & Track (`lyric_track.hpp`)**:
  - Syllable and word timing markers anchored to sequencer bars/steps.
- [x] **8.2 Speech Synthesizer Node (`tts_synth_node.hpp`)**:
  - Formant speech synthesis engine for robotic and vocoder-style singing lines.

### Subsystem 9: Extended Physical Modeling Preset Library
- [x] **9.1 Preset Library Import (`eats_builtin_presets.hpp`)**:
  - Port complete catalog of pipe organs, woodwinds, brass, bowed strings, and bells from `lib/eatscript/eats_builtin_presets.g.dart`.

---

## 4. Implementation Priority Phasing

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 1: AUDIO INTELLIGENCE & DSP FOUNDATIONS                              │
│ • Subsystem 1: Audio-to-MIDI Transcription Engine & Modal Dialog           │
│ • Subsystem 3: Procedural Impulse Response (IR) Generator                   │
│ • Subsystem 5: Track Freeze / Fast Bounce Engine                            │
├─────────────────────────────────────────────────────────────────────────────┤
│ PHASE 2: ALGORITHMIC COMPOSITION & ARRANGEMENT ENGINES                      │
│ • Subsystem 2: Procedural Song Engine, Ensembles & Archetypes               │
│ • Subsystem 4: SoundFont2 (SF2) Decoder & Multitimbral Player               │
│ • Subsystem 9: Extended Physical Modeling Preset Catalog                    │
├─────────────────────────────────────────────────────────────────────────────┤
│ PHASE 3: SPECIALIZED CREATIVE UI & VISUALIZATION                            │
│ • Subsystem 6: Waveshaper Editor, Note Splitter, Color Picker, Goniometer   │
├─────────────────────────────────────────────────────────────────────────────┤
│ PHASE 4: AI ASSISTANT & VOCAL/LYRIC PIPELINE                                │
│ • Subsystem 7: AI Assistant Dialog, Auto-Mixer, Gemini Service              │
│ • Subsystem 8: Synchronized Lyric Track & Formant Speech Synthesizer        │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Verification Standards

Every completed feature must satisfy the following criteria:
1. **Compilation**: Clean MSVC C++20 build with zero errors and zero warnings under `/W4` / `Release`.
2. **Automated Testing**: Dedicated unit tests added to `tests/test_modular_ui.cpp` or standalone test binaries.
3. **Interactive Validation**: Full event routing verified in `GuiWindow` and automated interaction suites.
4. **Footprint**: Kept strictly within the lightweight native executable boundary without bloated external dependencies.
