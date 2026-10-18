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

*All prioritized backlog tasks are complete! Add new items here to process in the autonomous refinement loop.*

---

## 🕒 Recent Completions (Reference Context)

<!-- Keep the 5 most recent completed items here for immediate agent context; older items are archived in TODO-ARCHIVE.md -->

- [x] **Implement Eatscript Declarative Clay UI DSL & In-Memory Embedded Asset System** (2026-10-17)
  - **Context/Files**: [eatscript-ui-blueprint.md](file:///c:/git/eatsbits/eatscript-ui-blueprint.md), [include/eatsbits/ui/base64.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/base64.hpp), [src/ui/base64.cpp](file:///c:/git/eatsbits/src/ui/base64.cpp), [include/eatsbits/ui/batch_renderer_2d.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp), [src/ui/batch_renderer_2d.cpp](file:///c:/git/eatsbits/src/ui/batch_renderer_2d.cpp), [include/eatsbits/ui/clay/eatscript_clay_bridge.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/clay/eatscript_clay_bridge.hpp), [src/ui/clay/eatscript_clay_bridge.cpp](file:///c:/git/eatsbits/src/ui/clay/eatscript_clay_bridge.cpp), [include/eatsbits/ui/widgets/text_editor_widget.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/text_editor_widget.hpp), [src/ui/widgets/text_editor_widget.cpp](file:///c:/git/eatsbits/src/ui/widgets/text_editor_widget.cpp), [tests/test_clay_layout.cpp](file:///c:/git/eatsbits/tests/test_clay_layout.cpp)
  - **Summary**:
    1. **Base64 & Data URI Parser (`base64.hpp` / `base64.cpp`)**: High-performance 256-lookup-table Base64 encoder/decoder, RFC 2397 Data URI parser, and automated code generator.
    2. **In-Memory Texture Loading & Filmstrip Rendering (`BatchRenderer2D`)**: Added `createTextureFromMemory` using `stbi_load_from_memory`, texture handle caching, `drawFilmstripFrame()`, and wired filmstrip texture rendering into `drawUnifiedKnob` and `drawUnifiedSlider`.
    3. **Declarative Clay UI DSL (`eatscript_clay_bridge.hpp` / `.cpp`)**: Implemented `registerEatscriptUiModule`, exposing `ui.panel`, `ui.row`, `ui.column`, `ui.stack`, `ui.knob`, `ui.slider`, `ui.label`, `ui.badge`, `ui.meter`, `ui.spacer`, and `ui.loadTexture` to Eatscript. Added identifier keys in Eatscript dictionary literals and closure invocation.
    4. **In-Editor Drag-and-Drop Embedding (`TextEditorWidget`)**: Added `handleFileDrop` and `embedAssetFromFile` to automatically convert dropped PNG/JPEG/QOI/WAV files into `const ASSET = "data:...;base64,..."` string literals.
    5. **Automated Verification**: Added Test 11 to `tests/test_clay_layout.cpp` validating Base64 roundtrip, texture memory loading, nested declarative layout evaluation, node parameter binding, and Clay flexbox resolution. Verified 51/51 test suites pass in `ctest`.

- [x] **Implement Monospace Terminal Grid Invariants (Ligature Cell Slicing & Box Primitives)** (commit `81a0b16`, 2026-10-15)
  - **Context/Files**: [include/eatsbits/text/monospace_grid_layout.hpp](file:///c:/git/eatsbits/include/eatsbits/text/monospace_grid_layout.hpp), [src/text/monospace_grid_layout.cpp](file:///c:/git/eatsbits/src/text/monospace_grid_layout.cpp), [include/eatsbits/terminal/terminal_grid.hpp](file:///c:/git/eatsbits/include/eatsbits/terminal/terminal_grid.hpp), [src/terminal/terminal_grid.cpp](file:///c:/git/eatsbits/src/terminal/terminal_grid.cpp), [tests/test_terminal_grid.cpp](file:///c:/git/eatsbits/tests/test_terminal_grid.cpp), [src/ui/batch_renderer_2d.cpp](file:///c:/git/eatsbits/src/ui/batch_renderer_2d.cpp), [include/eatsbits/ui/batch_renderer_2d.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp), [include/eatsbits/tui/types.hpp](file:///c:/git/eatsbits/include/eatsbits/tui/types.hpp)
  - **Acceptance Criteria**:
    1. **Ligature Cell Slicing (`MonospaceGridLayout`)**:
       - Implemented HarfBuzz and rule-based cell slicing in `MonospaceGridLayout`. When HarfBuzz collapses $N$ ASCII characters into a single compound ligature (e.g. `==` -> 2 cells, `===` -> 3 cells, `<!--` -> 4 cells), maps primary glyph to cell 0 and marks cells $1 \dots N-1$ as non-rendering continuation cells (`isContinuation = true`).
       - Added `TextAttr::Continuation` attribute in `tui::types.hpp` and updated `TerminalGrid` with `sliceLigature`, `writeLigature`, `isContinuationCell`, and `getCellSpan`.
    2. **Sub-Ligature Caret Navigation & Hit-Testing**:
       - Implemented `columnToByteOffset` and `byteOffsetToColumn`, mapping cursor column indices directly to sub-character UTF-8 cluster byte offsets without breaking visual shaping.
       - Implemented `hitTestColumn` with subpixel cell width clamping.
    3. **Analytical Zero-Seam Box & Block Mesh Generation**:
       - Implemented exact vector quad generation (`VertexMode::Color` / `OpaqueMeshNoAA`) in `BatchRenderer2D::drawMonospaceCell` covering all Unicode Box Drawing (`U+2500`–`U+257F`, including light, heavy, double, arc, and half lines) and Block Elements (`U+2580`–`U+259F`, including fractional 1/8 to 7/8 horizontal and vertical steps and quadrants).
       - Connected adjacent cells with bit-accurate integer boundaries, completely eliminating font-rounding hairline seams.
       - Early-exited glyph drawing on continuation cells to guarantee zero glyph overdraw.
    4. **Automated Verification**:
       - Added automated test suite `tests/test_terminal_grid.cpp` (`TerminalGridTest`) verifying cell slicing, caret hit-testing, terminal grid dual buffers, and zero-seam quad mesh generation.
       - Verified 100% test pass (50/50 test suites) via `ctest`.

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
