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

- [ ] **Implement Modular `eatsbits::text` Core (`FontManager`, `TextShaper`, and `GlyphAtlas`)**
  - **Context/Files**: [include/eatsbits/text/font_manager.hpp](file:///c:/git/eatsbits/include/eatsbits/text/font_manager.hpp), [src/text/font_manager.cpp](file:///c:/git/eatsbits/src/text/font_manager.cpp), [include/eatsbits/text/text_shaper.hpp](file:///c:/git/eatsbits/include/eatsbits/text/text_shaper.hpp), [src/text/text_shaper.cpp](file:///c:/git/eatsbits/src/text/text_shaper.cpp), [include/eatsbits/text/glyph_atlas.hpp](file:///c:/git/eatsbits/include/eatsbits/text/glyph_atlas.hpp), [src/text/glyph_atlas.cpp](file:///c:/git/eatsbits/src/text/glyph_atlas.cpp), [include/eatsbits/text/embedded_fonts.hpp](file:///c:/git/eatsbits/include/eatsbits/text/embedded_fonts.hpp), [tests/test_text_shaping.cpp](file:///c:/git/eatsbits/tests/test_text_shaping.cpp)
  - **Scope & Objectives**:
    1. **`FontManager`**: Thread-safe manager holding `FT_Library`, loading TTF/OTF files from disk or memory buffers, creating `FT_Face` and `hb_font_t` handles, and providing fallback font chains. Include embedded compressed fallback monospace font data in [embedded_fonts.hpp](file:///c:/git/eatsbits/include/eatsbits/text/embedded_fonts.hpp) for zero-file-dependency execution on both desktop and WebAssembly.
    2. **`TextShaper`**: Wrapper around HarfBuzz shaping engine. Takes UTF-8 text, font ID, font size, and feature flags (`liga`, `calt`, `dlig`). Returns `ShapedRun` with vector of `ShapedGlyph` containing `glyphIndex`, `cluster` (byte offset in UTF-8 string for cursor mapping), `xOffset`, `yOffset`, `xAdvance`, and `yAdvance`.
    3. **`GlyphAtlas`**: Dynamic 2048x2048 GPU texture atlas with 2D shelf/bin packing. Features LRU cache keyed by `(fontId, glyphIndex, pixelSize, subpixelBin)`. Supports 4 subpixel horizontal bins (`x % 0.25f`) for razor-sharp rendering at small font sizes (11px–14px). Rasterizes glyphs using FreeType LCD subpixel or antialiased modes, streaming dirty bounding boxes to [IBatchRenderBackend::updateFontTexture](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp#L59).
  - **Verification**: Expand `tests/test_text_shaping.cpp` to verify:
    - Accurate cluster byte offset mapping for multi-byte UTF-8 and multi-character ligatures.
    - Glyph atlas packing, cache hits vs misses, and dirty rect eviction.
    - Zero heap allocations during repeated shaping runs of cached text.

- [ ] **Migrate GUI Window & Drawing Utilities from FontStash to `eatsbits::text`**
  - **Context/Files**: [src/ui/gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp), [include/eatsbits/ui/gui_window.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/gui_window.hpp), [src/ui/draw_utils.cpp](file:///c:/git/eatsbits/src/ui/draw_utils.cpp), [include/eatsbits/ui/draw_utils.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/draw_utils.hpp), [third_party/fontstash/](file:///c:/git/eatsbits/third_party/fontstash/)
  - **Scope & Objectives**:
    1. **Replace `FontRendererState`**: Replace FontStash context in `GuiWindow` with `eatsbits::text::FontManager`, `TextShaper`, and `GlyphAtlas`.
    2. **Refactor Text Drawing Functions**: Update `drawVectorString`, `drawVectorStringCentered`, `drawMonoString`, and tooltip helpers to shape via `TextShaper` and emit `Vertex2D` quads (`VertexMode::FontAtlas`) directly into `BatchRenderer2D`.
    3. **Remove FontStash Dependency**: Delete `third_party/fontstash/` entirely from the codebase once all rendering is routed through `eatsbits::text`.
    4. **Visual Parity & Crispness**: Ensure UI typography across Arranger, Mixer, Track Inspector, and modal dialogs renders with enhanced subpixel clarity and accurate baseline alignment.
  - **Verification**: Run `.\build.ps1 -Test`. Verify that `test_gui_interaction` passes with zero heap allocations during `renderFrame()` across all 4 primary DAW views.

- [ ] **Implement Monospace Terminal Grid Invariants (Ligature Cell Slicing & Box Primitives)**
  - **Context/Files**: [include/eatsbits/text/monospace_grid_layout.hpp](file:///c:/git/eatsbits/include/eatsbits/text/monospace_grid_layout.hpp), [src/text/monospace_grid_layout.cpp](file:///c:/git/eatsbits/src/text/monospace_grid_layout.cpp), [include/eatsbits/terminal/terminal_grid.hpp](file:///c:/git/eatsbits/include/eatsbits/terminal/terminal_grid.hpp), [src/terminal/terminal_grid.cpp](file:///c:/git/eatsbits/src/terminal/terminal_grid.cpp), [tests/test_terminal_grid.cpp](file:///c:/git/eatsbits/tests/test_terminal_grid.cpp)
  - **Scope & Objectives**:
    1. **Ligature Cell Slicing**: Implement monospace grid layout helper for the terminal (`eatsterm`) and code editor (`eatscode`). When HarfBuzz collapses $N$ ASCII characters into a single compound ligature (e.g. `==` -> 2 cells, `===` -> 3 cells, `<!--` -> 4 cells), map the primary glyph to cell 0 and mark cells $1 \dots N-1$ as non-rendering continuation cells with proper cursor hit testing.
    2. **Sub-Ligature Caret Navigation**: Map cursor column index directly to the UTF-8 cluster offset, allowing the editor/terminal cursor to seamlessly navigate inside multi-character ligatures without breaking visual shaping.
    3. **Analytical Zero-Seam Box & Block Geometry**: Route Unicode box drawing (`U+2500`–`U+257F`) and block characters (`U+2580`–`U+259F`) to analytical vector quad rendering (`VertexMode::OpaqueMeshNoAA` / `Color`) in [BatchRenderer2D](file:///c:/git/eatsbits/src/ui/batch_renderer_2d.cpp), completely eliminating font-rounding pixel seams in terminal windows and TUI frames.
  - **Verification**: Add comprehensive unit tests in `tests/test_terminal_grid.cpp` testing ligature cell spans, cluster caret hit-testing, and analytical box-drawing mesh generation.


---

## 🕒 Recent Completions (Reference Context)

<!-- Keep the 5 most recent completed items here for immediate agent context; older items are archived in TODO-ARCHIVE.md -->

- [x] **Integrate FreeType and HarfBuzz into CMake Build Pipeline (Native + Wasm)** (commit `a1ae617`, 2026-10-15)
  - **Context/Files**: [CMakeLists.txt](file:///c:/git/eatsbits/CMakeLists.txt), [include/eatsbits/text/embedded_fonts.hpp](file:///c:/git/eatsbits/include/eatsbits/text/embedded_fonts.hpp), [src/text/text_engine.cpp](file:///c:/git/eatsbits/src/text/text_engine.cpp), [tests/test_text_shaping.cpp](file:///c:/git/eatsbits/tests/test_text_shaping.cpp), [src/ui/batch_renderer_2d.cpp](file:///c:/git/eatsbits/src/ui/batch_renderer_2d.cpp), [src/ui/icon_registry.cpp](file:///c:/git/eatsbits/src/ui/icon_registry.cpp)
  - **Acceptance Criteria**:
    1. **CMake FetchContent Configuration**:
       - Configured `FetchContent` for `freetype` (`VER-2-13-3`) and `harfbuzz` (`10.1.0`) with minimal dependency flags (brotli/bzip2/png/zlib and extra utilities disabled).
       - Created `eats_text` static library target linking FreeType, HarfBuzz, and `eats_core`.
       - Linked `eats_text` into `eatsbits_gui_core`.
    2. **Embedded Fallback Font Asset**:
       - Embedded `ShareTechMono-Regular.ttf` as binary hex byte array in `embedded_fonts.hpp` for zero-file-dependency runtime execution across desktop and WebAssembly.
    3. **Automated Smoke Test (`TextShapingTest`)**:
       - Added `tests/test_text_shaping.cpp` verifying `FT_Init_FreeType`, memory font face instantiation, `hb_ft_font_create`, and shaping strings with valid glyph indices and advances.
    4. **Zero-Allocation GUI Render Invariant Protection**:
       - Fixed `BatchRenderer2D::drawFlutedRing` to compute point-sampled fluted quad strips on the fly, eliminating `std::vector` heap allocations per knob per frame.
       - Cached contours in `IconRegistry` fallback path to prevent repetitive SVG parsing on stroke icons.
       - Verified 0 allocations across 20 consecutive frames in all 4 DAW views during active playback in `test_gui_interaction`.
    5. **Automated Verification**:
       - 100% pass across all 49 test suites via `ctest`.

- [x] **Purge Dead NanoVG Engine & Decouple Legacy Backend Interfaces** (commit `b21c403`, 2026-10-15)
  - **Context/Files**: [CMakeLists.txt](file:///c:/git/eatsbits/CMakeLists.txt), [README.md](file:///c:/git/eatsbits/README.md), [include/eatsbits/ui/nanovg_backend.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/nanovg_backend.hpp), [include/eatsbits/ui/dawn_bridge.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/dawn_bridge.hpp), [src/ui/gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp), [src/gui_main.cpp](file:///c:/git/eatsbits/src/gui_main.cpp)
  - **Acceptance Criteria**:
    1. **Removed NanoVG C Code from Build**:
       - Purged `third_party/nanovg/nanovg.c` and `third_party/nanovg/nanovg.h` from repository and CMake build system.
       - Removed `${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanovg` from global `include_directories`.
    2. **Isolated Interim FontStash & STB**:
       - Moved `fontstash.h` and `stb_truetype.h` into dedicated `third_party/fontstash/` directory, backed by a clean `fontstash.c` translation unit.
       - Moved `stb_image.h` into dedicated `third_party/stb/` directory backed by `stb_image.c`.
       - Completely deleted `third_party/nanovg/` directory.
    3. **Decoupled Backend Interfaces**:
       - Renamed legacy `INanoVGBackend` to `IRenderBatchBackend` in `nanovg_backend.hpp` and `dawn_bridge.hpp`, maintaining `using INanoVGBackend = IRenderBatchBackend` alias for zero-regression compatibility.
    4. **Refined Diagnostic Banners & About Dialog**:
       - Updated GUI startup console banner and About modal text to reflect native WebGPU / BatchRenderer2D architecture.
    5. **Automated Verification**:
       - Verified 100% build pass across all static libraries and executables.
       - Verified 100% test pass (48/48 test suites) via `ctest`.

- [x] **Implement Static Chrome Offscreen Texture (FBO) Caching** (commit `0f5d700`, 2026-10-11)
  - **Context/Files**: [include/eatsbits/ui/batch_renderer_2d.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp), [src/ui/batch_renderer_2d.cpp](file:///c:/git/eatsbits/src/ui/batch_renderer_2d.cpp), [include/eatsbits/ui/gui_window.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/gui_window.hpp), [src/ui/gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp), [tests/test_gui_interaction.cpp](file:///c:/git/eatsbits/tests/test_gui_interaction.cpp)
  - **Acceptance Criteria**:
    1. **Offscreen Chrome Backing Store & Blit Pipeline**:
       - Added `snapshotFramebuffer`, `restoreFramebuffer`, and `restoreFramebufferRegion` to `IBatchRenderBackend` and implemented high-throughput CPU/GPU raster snapshotting in `FilamentBatchBackend`.
       - Implemented `BatchRenderer2D::beginChromeRecording()`, `endChromeRecording()`, `drawCachedChrome()`, and `drawCachedChromeRegion()`, enabling static background chrome (top transport metal faceplate, machined bevels, hex fasteners, bottom navigation strip) to be rendered once and restored in $O(1)$ block copies.
       - Wired region blitting into `drawTopTransportBar()` and chin chassis in `GuiWindow::renderFrame()`, avoiding redundant procedural draw calls for static chrome on subsequent frames.
    2. **Dirty Invalidation Lifecycle**:
       - Added automatic chrome cache invalidation in `BatchRenderer2D::resize()` and `BatchRenderer2D::shutdown()`.
       - Integrated cache invalidation into `GuiWindow` upon window resize (`onWindowResize`), theme changes (`setActiveThemePreset`), view changes (`setActiveView`), side browser toggle (`toggleBrowser`, `setBrowserOpen`), and project hub toggle (`toggleProjectHub`, `setProjectHubOpen`).
    3. **Automated Verification**:
       - Added automated unit test `testStaticChromeFboCachingAndVisualParity()` to `tests/test_gui_interaction.cpp` verifying standalone `BatchRenderer2D` recording, cache metrics (`getChromeDrawCallsAvoided`), bit-for-bit parity (`std::memcmp == 0`), sub-region blitting, and `GuiWindow` event lifecycle invalidation across resize, view, theme, and drawer events.
       - Verified 100% test pass (48/48 test suites) via `ctest`.

- [x] **Add RAII Scissor Scope Management to `BatchRenderer2D`** (commit `f6d3139`, 2026-10-11)
  - **Context/Files**: [include/eatsbits/ui/batch_renderer_2d.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp), [src/ui/batch_renderer_2d.cpp](file:///c:/git/eatsbits/src/ui/batch_renderer_2d.cpp), [include/eatsbits/ui/draw_utils.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/draw_utils.hpp), [include/eatsbits/ui/widgets/scrollable_area.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/scrollable_area.hpp), [tests/test_ui_canvas.cpp](file:///c:/git/eatsbits/tests/test_ui_canvas.cpp)
  - **Acceptance Criteria**:
    1. **RAII `ScissorScope`**:
       - Implemented `ScissorScope` class in `BatchRenderer2D` with move semantics, `reset()`, and constructor variants taking `(x, y, w, h)` or `const Rect2D&`.
       - ScissorScope pushes scissor clipping rectangle on construction, computes hardware intersection with parent rectangles in the scissor stack, and automatically pops/restores previous scissor state on scope destruction or early `reset()`.
    2. **Refactored Scissor Calls Across Views and Dialogs**:
       - Added `makeScissorScope(BatchRenderer2D&)` helper to `ScrollableArea` to bind viewport clipping directly into RAII scopes.
       - Refactored manual `pushScissor`/`popScissor` calls across `DesignView`, `ArrangerMixerDrawer`, `IconSearchDialog`, `PluginSearchDialog`, `ThemeBrowserDialog`, `TrackPropertiesDrawer`, and `TextEditorWidget` to use `ScissorScope`.
    3. **Automated Verification**:
       - Added comprehensive unit test `testScissorScopeNestedClippingAndRaii()` in `tests/test_ui_canvas.cpp` testing single scope push/pop, nested parent/child rectangle intersection, empty intersection handling, RAII automatic unwinding, move semantics, early `reset()`, and `ScrollableArea::makeScissorScope()`.
       - Verified 100% test pass (48/48 test suites) via `ctest`.

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
