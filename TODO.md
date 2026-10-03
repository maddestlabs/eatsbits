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

- [ ] **Icon Design Review**: Improve icon readability, consistency, and aesthetic appeal.
  - **Context/Files**: `assets/icons/`, `src/eatsbits/gui/views/icon_font.*`
  - **Acceptance Criteria**: Icons are clear, follow the design system, and work well at small sizes.
  - **Test/Validation**: Visual inspection in the UI.
- [ ] Add tooltips to header transport elements and Track Properties elements.
- [x] Refine '+ ADD' dialogs for instruments, FX, MIDI FX, etc. Scrolling needs to be confined within its vertical layout bounds (it currently extends a bit past top and bottom of its bounds). Filter text box needs to be implemented and needs to be able to filter the list of items below. List needs to be filtered based on that text.
---

## ✅ Completed Archive

<!-- Completed items will be logged here with commit hashes and timestamps -->
- [x] Initialized autonomous refinement pipeline & todo-runner skill
