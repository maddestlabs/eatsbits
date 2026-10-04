# Eatsbits Refinement & Fix Backlog

This backlog powers the autonomous refinement workflow in Antigravity.
You can add bugs, UI tweaks, DSP optimizations, and features below.

To execute items automatically, run in Antigravity:
```text
/goal Work through the unchecked items in TODO.md using the todo-runner skill.
```

---

## 🚀 Active Queue (Prioritized)

<!-- Add your todo notes and improvements here. Items at the top are processed first. -->

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

- [ ] **Web Audio Optimization Phase 4: AudioWorklet & Dedicated Wasm Worker Threading**
  - **Context/Files**: `CMakeLists.txt`, `src/audio/audio_engine.cpp`, `build-web.ps1`
  - **Acceptance Criteria**: Configure miniaudio AudioWorklet integration with `-DMA_ENABLE_AUDIO_WORKLETS`, `-sAUDIO_WORKLET=1`, `-sWASM_WORKERS=1`, and `-sASYNCIFY` (leveraging existing COOP/COEP headers in `build-web.ps1`); verify audio thread decoupling so real-time DSP callback execution runs entirely on a dedicated audio worklet thread independent of the main JavaScript thread.
  - **Test/Validation**: `.\build-web.ps1 -NoServe`

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

---

## 📋 Backlog / Ideas

- [x] **Icon Design Review**: Improve icon readability, consistency, and aesthetic appeal. (commit `e860f33`)
  - **Context/Files**: `src/ui/icon_registry.cpp`, `src/ui/widgets/icon_search_dialog.cpp`, `tests/test_icon_system.cpp`
  - **Acceptance Criteria**: Icons are clear, follow the design system, and work well at small sizes.
  - **Test/Validation**: `.\build.ps1 -Test`
- [x] Add tooltips to header transport elements and Track Properties elements. (commit `cd2f33e`)
- [x] Refine '+ ADD' dialogs for instruments, FX, MIDI FX, etc. Scrolling needs to be confined within its vertical layout bounds (it currently extends a bit past top and bottom of its bounds). Filter text box needs to be implemented and needs to be able to filter the list of items below. List needs to be filtered based on that text. (commit `6245673`)
---

## ✅ Completed Archive

<!-- Completed items will be logged here with commit hashes and timestamps -->
- [x] Initialized autonomous refinement pipeline & todo-runner skill
