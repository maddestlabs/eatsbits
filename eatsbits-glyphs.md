# Eatsbits Modular Vector Glyph System

A zero-overhead, fontless procedural vector glyph and icon architecture engineered for high-performance C++20 applications, audio workstations, terminal emulators, code editors, and score engravers.

---

## 1. Overview & Motivation

Traditional GUI frameworks rely on large bundled TrueType or OpenType icon fonts (such as Nerd Fonts, FontAwesome, or Material Icons) for rendering symbols, box-drawing characters, and editor glyphs. While convenient, this approach introduces significant compromises:

1. **Binary Bloat**: A comprehensive Nerd Font or icon symbol font adds **2.5 MB to 15+ MB** to the application binary or asset bundle.
2. **GPU Pipeline Inefficiency**: Glyph fonts require rasterizing glyphs into CPU/GPU texture atlases with FreeType, managing atlas fragmentation, and performing texture binding swaps between vector UI passes and text passes.
3. **One-Size-Fits-None Overhead**: Auxiliary tools (such as standalone terminal apps or script editors) built on the core UI engine are forced to ship heavy musical or DSP symbols they never execute.
4. **Resolution & Styling Limitations**: Rasterized font glyphs suffer from hinting artifacts at fractional scaling factors and cannot easily participate in multi-pass gradient fills, dynamic glow shaders, or contextual skeuomorphic lighting.

### The Eatsbits Solution
The Eatsbits Modular Vector Glyph Architecture provides **100% procedural vector rendering** for all symbols, terminal box-drawing elements, code editor indicators, DSP units, music score glyphs, and a built-in ShareTech-style monospace debug font.

- **0 MB External Font Assets**: Completely eliminated `SymbolsNerdFont-Regular.ttf` (saving 2.64 MB).
- **Pay-For-What-You-Use Modularity**: Discrete functional glyph packs (`Terminal`, `Editor`, `DspMath`, `Score`, `VectorFont`) can be selectively enabled or isolated per-project.
- **Single-Pass GPU Batching**: All glyphs tessellate directly into the existing [BatchRenderer2D](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp) vertex stream alongside panels, buttons, and knobs in a single WebGPU/Vulkan draw call.
- **Microsecond Response**: Render functions emit lines, triangles, quads, and Bézier curves with zero string parsing or atlas lookups.

---

## 2. Architecture & Glyph Packs

The system is defined in [include/eatsbits/ui/glyph/glyph_packs.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/glyph/glyph_packs.hpp) and implemented in [src/ui/glyph/glyph_packs.cpp](file:///c:/git/eatsbits/src/ui/glyph/glyph_packs.cpp) and [src/ui/glyph/vector_font.cpp](file:///c:/git/eatsbits/src/ui/glyph/vector_font.cpp).

### The `GlyphPack` Bitmask

```cpp
namespace eatsbits::ui::glyph {

enum class GlyphPack : uint32_t {
    None       = 0,
    Terminal   = 1 << 0, // CP437, Box drawing (0x2500..0x257F), Block elements (0x2580..0x259F)
    Editor     = 1 << 1, // Code folding (▾, ▸), Breakpoints (●, ○), Diagnostics (✓, ✗, ⚠, ℹ), Pipeline arrows (→, ←)
    DspMath    = 1 << 2, // Audio math (π, τ, ±, °, ∞, μ, Ω, √, ≠, ≤, ≥, ≈), Waveforms (∿, ⊓, △, ⊿)
    Score      = 1 << 3, // Music score engraving (𝄞 Treble, 𝄢 Bass, ♭, ♯, ♮, 𝄽 rests, SMuFL noteheads)
    VectorFont = 1 << 4, // Procedural ShareTech-style monospace vector font (ASCII 32..126)
    All        = 0xFFFFFFFF
};

}
```

### Pack Breakdown

| Pack | Typical Use Cases | Included Unicode Ranges & Codepoints |
| :--- | :--- | :--- |
| **`Terminal`** | CLI grids, splitters, progress bars, TUI panels | `0x2500`─ `0x2502`│ `0x250C`┌ `0x2510`┐ `0x2514`└ `0x2518`┘ `0x251C`├ `0x2524`┤ `0x252C`┬ `0x2534`┴ `0x253C`┼<br>`0x2588`█ `0x2584`▄ `0x2580`▀ `0x258C`▌ `0x2590`▐ `0x2591`░ `0x2592`▒ `0x2593`▓ |
| **`Editor`** | Code editor gutters, folding margins, debugger, AST arrows | `0x25BE`▾ `0x25B8`▸ (Folding)<br>`0x25CF`● `0x25CB`○ `0x25B6`▶ (Breakpoints & execution pointer)<br>`0x2713`✓ `0x2717`✗ `0x26A0`⚠ `0x2139`ℹ (Linter diagnostics)<br>`0x2192`→ `0x2190`← (Pipeline & return arrows) |
| **`DspMath`** | Audio script DSL, DSP parameters, circuit schematics | `0x03C0`π `0x03C4`τ `0x00B1`± `0x00B0`° `0x221E`∞ `0x221A`√<br>`0x03BC`μ `0x03A9`Ω (Microseconds, Ohms)<br>`0x2260`≠ `0x2264`≤ `0x2265`≥ `0x2248`≈<br>`0x223F`∿ `0x2293`⊓ `0x25B3`△ `0x22BF`⊿ (Oscillator shapes) |
| **`Score`** | Music stave notation, MIDI score roll, sheet music | `0x1D11E`𝄞 (Treble G-Clef Bravura Bézier)<br>`0x1D122`𝄢 (Bass F-Clef Bravura Bézier)<br>`0x266D`♭ `0x266F`♯ `0x266E`♮ (Accidentals)<br>`0x1D13D`𝄽 (Quarter Rest)<br>`0x2669`♩ `0x266A`♪ `0x266B`♫ (Notes with SMuFL noteheads & stems) |
| **`VectorFont`** | Script editors, tracker grids, live IDE, debug reporting HUD | Printable ASCII `0x20`–`0x7E` (A–Z, a–z, 0–9, punctuation) in pure 45° chamfered vector strokes |

---

## 3. Built-In "ShareTech Vector" Debug & Script Font

### Comparison with SDL and Raylib Debug Fonts

Modern game development frameworks provide embedded debug text capabilities for reporting metrics during development:
- **SDL3** (`SDL_RenderDebugText`): Bundles a 1-bit-per-pixel 8×8 font bitmap (~760 bytes). When scaled on HiDPI or 4K displays, it renders as aliased retro pixel blocks.
- **Raylib** (`rdefaultFont`): Bundles an embedded 8×16 or 6×10 bitmap atlas (~3–5 KB). Requires texture atlas sampling and can look blurry or aliased.
- **Dear ImGui**: Embeds compressed TrueType (`ProggyClean.ttf`, ~17 KB), requiring FreeType or `stb_truetype` parsing and atlas generation.

### The Eatsbits Advantage
The Eatsbits `VectorFont` pack takes a fundamentally different route: **procedural vector stroke rendering** styled after the iconic **ShareTech Mono** font:
1. **Ultra-Low Memory Footprint**: All 95 printable ASCII glyphs are encoded in **~1.2 KB** of coordinate stroke data in `.rodata`.
2. **Infinite Scalability**: Evaluated as vector strokes with thickness dynamically proportional to cell size (`strokeW = h * 0.082f`). Remains razor-sharp at 10px, 14px, 48px, or zoomed 4K displays.
3. **True Zero-Dependency Fallback**: Works with zero disk font files and zero FreeType font handles. If `assets/fonts/` is missing or in minimal CLI builds, all text continues to render smoothly.
4. **Single-Pass GPU Batching**: Emits directly into `BatchRenderer2D` without texture atlas switches.

### Developer API: `drawDebugText` and `drawVectorText`

For development feedback, diagnostic overlays, and HUDs:

```cpp
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/glyph/vector_font.hpp"

// One-liner debug feedback HUD (like Raylib/SDL, but sharp vector)
drawDebugText(renderer, 10.0f, 10.0f, 12.0f, "FPS: 144 | Voices: 8 | Buffer: 10ms", Color(0.2f, 1.0f, 0.4f));

// Direct vector text drawing
eatsbits::ui::glyph::drawVectorText(renderer, x, y, 14.0f, "def synth_loop():", 0.4f, 0.8f, 1.0f, 1.0f);
```

### Script Editor (`TextEditorWidget`) Integration
The script editor in [TextEditorWidget](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/text_editor_widget.hpp) renders characters via `BatchRenderer2D::drawMonospaceText`. Through the window hook [setMonospaceGlyphHook](file:///c:/git/eatsbits/src/ui/gui_window.cpp#L7221), when `GlyphPack::VectorFont` is enabled, the editor bypasses FreeType and renders with the procedural ShareTech vector font.

### Typographic Refinement: The ShareTech Aesthetic
Unlike skeleton wireframe fonts that look like stick figures, the Eatsbits ShareTech vector font utilizes optical weight modulation and geometric corner fillets:
- **Optical Stem Modulation**: Vertical stems (`vStem`) are rendered with an optical grotesque ratio ($\approx 0.165 \times \text{cellWidth}$), while horizontal bars (`hBar`) are rendered at $\approx 0.082 \times \text{cellHeight}$ to compensate for human horizontal-vertical illusion.
- **Chamfer Fillets & Solid Corners**: Corner bevels (`chamferLoop`) employ solid triangle and quad fillets that eliminate microscopic gaps between orthogonal bars and diagonal strokes under hardware anti-aliasing.
- **Authentic Counter-Spaces**: Letters with enclosed counters (`0`, `8`, `B`, `D`, `O`, `P`, `Q`, `R`, `a`, `b`, `d`, `e`, `g`, `o`, `p`, `q`) maintain consistent inner counter apertures and 45-degree chamfers matching authentic ShareTech Mono.

---

## 4. Usage & Integration Guide

### Direct Standalone Invocation (Zero Overhead)
If an application only needs a specific pack, it can call that pack's renderer directly with zero dispatch overhead:

```cpp
#include "eatsbits/ui/glyph/glyph_packs.hpp"
#include "eatsbits/ui/glyph/vector_font.hpp"

// Render a terminal corner character directly
bool rendered = eatsbits::ui::glyph::renderTerminalGlyph(
    renderer, 0x250C, x, y, scale, 0.8f, 0.8f, 0.8f, 1.0f);

// Render an editor folding chevron
bool folded = eatsbits::ui::glyph::renderEditorGlyph(
    renderer, 0x25BE, x, y, scale, 0.3f, 0.8f, 1.0f, 1.0f);

// Render a single vector character in a cell
eatsbits::ui::glyph::renderVectorFontGlyph(
    renderer, 'A', cellX, cellY, cellW, cellH, 1.0f, 1.0f, 1.0f, 1.0f);
```

### Centralized Dispatch with `GlyphRegistry`
For modular multi-purpose applications, use `GlyphRegistry`. Applications configure active packs at startup using bitwise operators:

```cpp
#include "eatsbits/ui/glyph/glyph_packs.hpp"

using namespace eatsbits::ui::glyph;

void initApp(AppType type) {
    auto& reg = GlyphRegistry::instance();

    switch (type) {
        case AppType::TerminalConsole:
            // Terminal app: ONLY box-drawing and blocks.
            // Completely ignores and skips Score, DSP Math, and Editor glyphs.
            reg.enablePacks(static_cast<uint32_t>(GlyphPack::Terminal));
            break;

        case AppType::CodeEditor:
            // Text editor: Terminal frames + Code folding, breakpoints, diagnostics + Vector font.
            reg.enablePacks(GlyphPack::Terminal | GlyphPack::Editor | GlyphPack::VectorFont);
            break;

        case AppType::AudioLiveCoder:
            // Live-coding environment: Terminal + Editor + DSP math & waveforms + Vector font.
            reg.enablePacks(GlyphPack::Terminal | GlyphPack::Editor | GlyphPack::DspMath | GlyphPack::VectorFont);
            break;

        case AppType::FullDAW:
        default:
            // Complete digital audio workstation: Enable all glyphs.
            reg.enablePacks(static_cast<uint32_t>(GlyphPack::All));
            break;
    }
}
```

---

## 5. Sub-Project Integration & Alignment Guide

When sharing this engine or porting it to downstream sub-projects (such as CLAP/VST audio plugins, standalone terminal emulators, code editors, or micro-tools), the revamped core provides decoupled modularity so each sub-project only compiles what it needs.

### 1. Architectural Decoupling: Runtime Rendering vs. Offline Baking

The typography and vector glyph stack is partitioned into two distinct domains:

1. **Runtime Vector Core (Zero External Dependencies)**:
   - **Headers**: [ebf_font.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/glyph/ebf_font.hpp), [glyph_packs.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/glyph/glyph_packs.hpp), [vector_font.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/glyph/vector_font.hpp), [oswald_embedded_data.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/glyph/oswald_embedded_data.hpp), [sharetech_embedded_data.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/glyph/sharetech_embedded_data.hpp).
   - **Sources**: `src/ui/glyph/ebf_font.cpp`, `src/ui/glyph/glyph_packs.cpp`, `src/ui/glyph/vector_font.cpp`.
   - **Dependencies**: None. Requires only standard C++20 and a 2D batch vertex sink (like [BatchRenderer2D](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp)).
   - **External Libs Needed**: **Zero**. Does NOT need FreeType, HarfBuzz, or disk font assets.
   - **Target Sub-Projects**: CLAP/VST3 audio plugins, micro-GUIs, WebAssembly web builds, embedded hardware.

2. **Offline / Build-Time Conversion Core (Asset Pipeline)**:
   - **Headers & Sources**: [ebf_converter.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/glyph/ebf_converter.hpp), [ebf_converter.cpp](file:///c:/git/eatsbits/src/ui/glyph/ebf_converter.cpp).
   - **Dependencies**: FreeType 2 (`FT_Init_FreeType`, `FT_Outline_Decompose`).
   - **Usage**: Used only during development or in build scripts to convert `.ttf`/`.otf` outlines into pre-tessellated `.ebf` files or `.hpp` headers. Sub-projects do not need to ship this in release binaries.

---

### 2. Integration Recipes for Sub-Projects

#### Recipe A: Embedded Zero-Dependency Sub-Project (Audio Plugins & Micro-Tools)
For a CLAP/VST3 plugin (like `eatsbits_tb303.clap`) or a standalone micro-app that must ship as a single self-contained binary:

```cmake
# CMake configuration for zero-dependency sub-project:
target_sources(my_plugin_ui PRIVATE
    src/ui/glyph/ebf_font.cpp
    src/ui/glyph/glyph_packs.cpp
    src/ui/glyph/vector_font.cpp
)
# Note: Do NOT link FreeType or HarfBuzz!
```

```cpp
#include "eatsbits/ui/glyph/ebf_font.hpp"

// Single-line access to embedded, pre-tessellated fonts in .rodata
auto& titleFont = eatsbits::ui::glyph::EbfFont::getEmbeddedOswaldCondensed();
auto& monoFont  = eatsbits::ui::glyph::EbfFont::getEmbeddedShareTechMono();

// Render directly into plugin canvas with zero file I/O:
titleFont.drawText(renderer, x, y, 16.0f, "TB-303 BASS SYNTHESIZER", 1.0f, 1.0f, 1.0f, 1.0f);
```

#### Recipe B: Desktop Tool Sub-Project with 3-Tier Font Fallback
For standalone desktop applications that want to support user-customizable `.ttf` or `.ebf` fonts with fallback to embedded ROM:

```cpp
#include "eatsbits/ui/glyph/ebf_font.hpp"
#include "eatsbits/ui/glyph/ebf_converter.hpp"

class SubProjectWindow {
    eatsbits::ui::glyph::EbfFont uiFont_;
    eatsbits::ui::glyph::EbfFont codeFont_;

public:
    void initFonts() {
        // 1. Try pre-compiled fast binary (.ebf)
        if (!uiFont_.loadFromEbfFile("assets/fonts/Oswald-Medium.ebf")) {
            // 2. Fall back to on-the-fly TTF conversion if file exists
            if (!uiFont_.loadFromTtfFile("assets/fonts/Oswald-Medium.ttf")) {
                // 3. Fall back to zero-dependency embedded ROM
                uiFont_ = eatsbits::ui::glyph::EbfFont::getEmbeddedOswaldCondensed();
            }
        }
    }
};
```

#### Recipe C: Standalone Terminal / CLI Tool (`eatsbits_term`, `eatsbits_tui`)
For TUI tools that only require terminal box-drawing frames and monospace text:

```cpp
#include "eatsbits/ui/glyph/glyph_packs.hpp"
#include "eatsbits/ui/glyph/vector_font.hpp"

void initTerminalSubProject() {
    // Only enable box-drawing and ShareTech vector font
    auto& reg = eatsbits::ui::glyph::GlyphRegistry::instance();
    reg.enablePacks(
        static_cast<uint32_t>(eatsbits::ui::glyph::GlyphPack::Terminal) |
        static_cast<uint32_t>(eatsbits::ui::glyph::GlyphPack::VectorFont)
    );
}
```

---

## 6. Score Notation Engraving (SMuFL Bravura Béziers)

The score editor in Eatsbits has been upgraded from raw text placeholders (`"[TREBLE G-CLEF]"`) to authentic SMuFL vector engraving powered by `ScoreVectorGlyphs`:

1. **Treble G-Clef (`0x1D11E`)**: Evaluates cubic Bézier splines matching the Bravura reference font:
   - Base loop with taper.
   - Central G-line intersection.
   - Upper loop apex and descending spiral tail.
2. **Bass F-Clef (`0x1D122`)**:
   - Spiral body anchored on the F-line (line 4 of staff).
   - Dual pitch reference dots at staff space 3 and space 4.
3. **Accidentals**:
   - Flat (`0x266D`): Ascending vertical stem with smooth lower bowl.
   - Sharp (`0x266F`): Dual slanted cross-bars and double vertical uprights.
   - Natural (`0x266E`): Paired L-bracket stems with optical weight compensation.
4. **Noteheads**:
   - Black notehead (`drawBlackNotehead`): Tilted ellipse (angle ~-20°) with standard SMuFL width-to-height ratio ($1.18 \times \text{sp}$).
   - Half notehead (`drawHalfNotehead`): Tilted hollow ellipse with thin inner cut.
   - Whole notehead (`drawWholeNotehead`): Wide oval with dual optical carve-outs.
5. **Staff Integration**:
   - Automatic ledger line computation for notes above/below the 5 standard lines.
   - Direction-aware note stems (upstem attached to right edge, downstem to left edge).

---

## 7. Coexistence: `IconRegistry` vs `GlyphRegistry`

Eatsbits maintains two complementary vector systems:

| System | Identification | Primary Purpose | Example Usages |
| :--- | :--- | :--- | :--- |
| **[IconRegistry](file:///c:/git/eatsbits/include/eatsbits/ui/draw_utils.hpp)** | Named strings (e.g. `"hw_speaker"`, `"gen_zap"`, `"inst_synth"`) | Skeuomorphic knobs, device headers, toolbar buttons, hardware chassis badges | Center glyphs of rotary knobs, DAW mixer track headers, modal device titles |
| **[GlyphRegistry](file:///c:/git/eatsbits/include/eatsbits/ui/glyph/glyph_packs.hpp)** | Unicode Codepoints (`uint32_t cp`, e.g. `0x250C`, `0x25BE`, `0x1D11E`) | Inline code text, monospace terminal grids, editor gutters, music score staves | Code folding chevrons, linter diagnostics, box drawing frames, sheet music |

The unified dispatcher `drawVectorGlyph(nameOrCodepoint)` in [src/ui/gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp) queries both systems seamlessly: if a string represents an `IconRegistry` ID, it draws the named vector asset; if it represents a UTF-8 character, it parses the codepoint and renders via `GlyphRegistry`.

---

## 8. Custom Eatsbits Font Format (`.ebf`) & Proportional Vector Typography

To enable derivative lightweight applications (and potentially Eatsbits itself) to eliminate heavy font rasterizers (FreeType) and shaping engines (HarfBuzz), Eatsbits introduces the **`.ebf` (Eatsbits Font) binary format** and proportional vector typography engine defined in [include/eatsbits/ui/glyph/ebf_font.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/glyph/ebf_font.hpp) and implemented in [src/ui/glyph/ebf_font.cpp](file:///c:/git/eatsbits/src/ui/glyph/ebf_font.cpp).

### The `.ebf` Binary Format Specification

The `.ebf` format is engineered for ultra-fast, zero-allocation memory mapping:

```
+-------------------------------------------------------+
|  EbfHeader (32 bytes)                                 |
|  - magic: "EBF1"                                      |
|  - unitsPerEm: 1000                                   |
|  - glyphCount: uint16                                 |
|  - ascender, descender, lineGap, capHeight, xHeight   |
+-------------------------------------------------------+
|  EbfGlyphRecord Table (16 bytes * glyphCount)         |
|  - codepoint: uint32                                  |
|  - advanceX, bearingX, bearingY                       |
|  - width, height                                      |
|  - commandOffset, commandLength                       |
+-------------------------------------------------------+
|  Contour Bytecode Buffer                              |
|  - Sequential stream of EbfOpcode instructions        |
+-------------------------------------------------------+
|  Kerning Table (Optional)                             |
|  - Array of (leftCp, rightCp, kerningAmount)          |
+-------------------------------------------------------+
```

### Compact Vector Bytecode Opcodes (`EbfOpcode`)

| Opcode | Arguments | Description |
| :--- | :--- | :--- |
| `0x01 MoveTo` | `int16 x, y` | Moves pen without drawing |
| `0x02 LineTo` | `int16 x, y` | Draws stroked vector line from last point |
| `0x03 QuadTo` | `int16 cx, cy, x, y` | Evaluates quadratic Bézier curve segment |
| `0x04 CubicTo` | `int16 c1x, c1y, c2x, c2y, x, y` | Evaluates cubic Bézier curve segment |
| `0x05 Close` | None | Closes active contour |
| `0x06 Rect` | `int16 x, y, w, h` | Emits filled rectangular block |
| `0x07 ChamferRect` | `int16 x, y, w, h, c` | Emits 45° chamfered loop with solid corner fillets |
| `0x08 Triangle` | `int16 x0, y0, x1, y1, x2, y2` | Emits filled triangle |
| `0x09 VStem` | `int16 x, y0, y1` | Emits vertical stem with optical grotesque weight |
| `0x0A HBar` | `int16 x0, x1, y` | Emits horizontal bar with optical weight |
| `0x0B Diag` | `int16 x0, y0, x1, y1` | Emits diagonal line connecting two coordinates |

---

## 9. Built-In Proportional Oswald Condensed Vector Font

While monospace fonts are ideal for code editors and terminals, GUI applications require **proportional condensed typography** for button labels, tabs, track headers, and value readouts.

`EbfFont` provides a built-in, pre-compiled implementation of **Oswald Condensed Grotesque**:
- **Zero File I/O**: Instantly accessible via `EbfFont::getEmbeddedOswaldCondensed()`.
- **Proportional Horizontal Advances**:
  - Punctuation & Narrow Letters (`.`, `:`, `;`, `!`, `I`, `l`, `i`): **220–260 em**
  - Narrow Uppercase & Lowercase (`J`, `f`, `t`, `r`): **310–380 em**
  - Standard Grotesque Uppercase (`B`, `C`, `D`, `E`, `F`, `G`, `H`, `K`, `L`, `N`, `O`, `P`, `R`, `S`, `T`, `U`, `V`, `X`, `Y`, `Z`): **450–530 em**
  - Standard Grotesque Lowercase (`a`, `b`, `c`, `d`, `e`, `g`, `h`, `k`, `n`, `o`, `p`, `q`, `s`, `u`, `v`, `x`, `y`, `z`): **400–470 em**
  - Broad Capitals (`M`, `W`): **640–660 em**
  - Tabular Figures (`0`–`9`): **500 em uniform**
- **Proportional Text Measurement**:
  ```cpp
  float width = font.measureText("SYNTHESIZER", 18.0f);
  auto metrics = font.measureTextDetailed("Multi-line\nLabel", 14.0f);
  ```
- **Single-Pass Vector Drawing**:
  ```cpp
  font.drawText(renderer, x, y, 16.0f, "OSWALD CONDENSED", 1.0f, 1.0f, 1.0f, 1.0f);
  ```

---

---

## 10. In-Engine TTF/OTF to `.ebf` Conversion (`EbfConverter`)

Eatsbits provides a built-in conversion engine defined in [include/eatsbits/ui/glyph/ebf_converter.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/glyph/ebf_converter.hpp) and implemented in [src/ui/glyph/ebf_converter.cpp](file:///c:/git/eatsbits/src/ui/glyph/ebf_converter.cpp).

This provides two flexible workflows for developers and end-users:

### Workflow A: On-The-Fly Startup Conversion (Maximum Ease of Use)
Users can simply drop standard `.ttf` or `.otf` font files into their project without any manual pre-processing. The engine converts the font in-memory at boot time in $< 5\,\text{ms}$:

```cpp
#include "eatsbits/ui/glyph/ebf_font.hpp"

eatsbits::ui::glyph::EbfFont customFont;

// Converts TTF outlines directly into in-memory .ebf bytecode on startup
if (customFont.loadFromTtfFile("assets/fonts/ShareTechMono-Regular.ttf")) {
    // All subsequent frame rendering runs 100% pure vector through BatchRenderer2D!
    customFont.drawText(renderer, x, y, 16.0f, "AUTHENTIC SHARETECH VECTORS", 1.0f, 1.0f, 1.0f);
}
```

### Workflow B: Offline / Build-Time Conversion (Zero-Dependency Shipping)
To ship an ultra-lightweight downstream binary that completely omits FreeType, convert the font once during development:

```cpp
#include "eatsbits/ui/glyph/ebf_converter.hpp"

// 1. Export to binary .ebf file on disk
eatsbits::ui::glyph::EbfConverter::convertFile(
    "assets/fonts/ShareTechMono-Regular.ttf",
    "assets/fonts/share_tech_mono.ebf"
);

// 2. Or generate a static C++ header for single-binary zero-file compilation
eatsbits::ui::glyph::EbfConverter::convertToCppHeader(
    "assets/fonts/ShareTechMono-Regular.ttf",
    "include/eatsbits/ui/glyph/share_tech_mono_data.hpp",
    "ShareTechMono"
);
```

### Typographic Fidelity: Exact Bézier Extraction & Pure Solid Polygon Triangulation
Unlike algorithmic vector approximations or hollow perimeter stroking, `EbfConverter` uses FreeType's `FT_Outline_Decompose` together with a high-performance **2D ear-clipping triangulator with bridge hole connection**:
1. **Solid Filled Geometry**: Decomposes glyph closed polylines into solid `Triangle` opcodes (`EbfOpcode::Triangle`), filling stems, bars, loops, and bowls completely with zero hollow/transparent gaps.
2. **Hole Bridge Connection**: For characters with interior apertures (e.g. 'O', 'A', 'B', 'P', 'R', '8', '0'), internal holes are automatically connected to their parent outer boundary via zero-width bridge cuts before ear-clipping, ensuring interior counters remain cleanly open while the surrounding glyph body is completely solid.
3. **Pure Triangle Rasterization (No Artificial Bolding)**: Triangles directly define the exact outer boundary and aperture contours of the font. Perimeter line stroking (`drawLine`) is omitted when solid triangles exist, preventing the "double stroke / artificial bold" effect where 1px boundary ribbons expanded stems outward and choked interior counters.
- **ShareTech Mono**: Preserves 100% of the authentic 45° corner chamfers, open counter apertures, and clean monospace geometry with sharp, delicate vector stems.
- **Oswald**: Preserves the condensed grotesque curves, high crossbars, and authentic 400-weight vertical proportions with exact kerning pairs.

---

## 11. Eliminating FreeType & HarfBuzz (Zero-Dependency Builds)

For lightweight derivative projects (such as standalone terminal consoles, compact text editors, micro-plugins, or embedded targets), the combination of `EbfFont` and `VectorFont` enables **completely purging FreeType and HarfBuzz**:

| Dependency | Standard Build Size | With Eatsbits Vector Typography |
| :--- | :--- | :--- |
| **FreeType Static Lib** | ~1.4 MB | **0 KB (Removed)** |
| **HarfBuzz Static Lib** | ~1.1 MB | **0 KB (Removed)** |
| **Font File Assets** (`Oswald.ttf`, `ShareTechMono.ttf`) | ~600 KB | **0 KB (Removed)** |
| **Runtime Texture Atlas** (`GlyphAtlas` 1024×1024) | 1.0 MB VRAM | **0 KB (No Atlas Switches)** |
| **Total Binary & Asset Savings** | — | **~3.1 MB eliminated** |

---

## 12. Verification & Performance

The modular glyph architecture, ShareTech monospace font, `.ebf` proportional vector engine, and `EbfConverter` are continuously validated in the automated test suite:

- **Unit Tests**:
  - `testModularGlyphPacks()` in [tests/test_ui_widgets.cpp](file:///c:/git/eatsbits/tests/test_ui_widgets.cpp)
  - `testEbfFontFormatAndOswald()` in [tests/test_ui_widgets.cpp](file:///c:/git/eatsbits/tests/test_ui_widgets.cpp)
  - `testEbfConverterAndShareTechConversion()` in [tests/test_ui_widgets.cpp](file:///c:/git/eatsbits/tests/test_ui_widgets.cpp)
- **Assertions Verified**:
  - Independent vertex generation for each pack (`Terminal`, `Editor`, `DspMath`, `Score`, `VectorFont`).
  - Bitmask filtering isolating Terminal app configurations from DSP and Score glyphs.
  - Printable ASCII coverage (uppercase, lowercase, numbers, brackets, punctuation).
  - ShareTech optical grotesque stem/bar compensation and solid chamfer loop filling.
  - Proportional metrics hierarchy (`advanceX('I') < advanceX('H') < advanceX('M')`).
  - Multi-line measurement and detailed bounds calculation.
  - Binary `.ebf` serialization to disk and byte-identical deserialization reload.
  - On-the-fly conversion of `ShareTechMono-Regular.ttf` $\rightarrow$ `.ebf` with verified monospace flags and uniform advances.
  - On-the-fly startup conversion of `Oswald-Medium.ttf` into `EbfFont` (`loadFromTtfFile`).
  - Single-pass vector drawing into `BatchRenderer2D` with zero heap allocation during frame rendering.

---

## 13. Live GUI & Script Editor Integration (`GuiWindow`)

The entire live user interface and script editor are now wired to the high-performance `EbfFont` vector engine in [src/ui/gui_window.cpp](file:///c:/git/eatsbits/src/ui/gui_window.cpp):

### 1. Three-Tier Typography Architecture
At startup, `GuiWindow::initFontRenderer()` initializes both `g_ebfOswald` (proportional UI) and `g_ebfShareTech` (monospace script editor & telemetry) using a resilient 3-tier loading hierarchy:
1. **Tier 1 (Bundled TrueType On-the-Fly Conversion - Default)**: Automatically parses bundled TTF files (`ShareTechMono-Regular.ttf`, `Oswald-Medium.ttf`, or user-specified custom fonts) on-the-fly at boot ($< 3\,\text{ms}$) via `EbfConverter::loadTtfIntoFont`, yielding zero-dependency vector geometry without shipping heavy pre-tessellated mesh files.
2. **Tier 2 (Pre-Compiled Binary `.ebf`)**: If pre-compiled `.ebf` files are provided by user or build cache, loads them in $< 0.1\,\text{ms}$.
3. **Tier 3 (Zero-Dependency Embedded Fallback)**: If no font files exist on disk, `getEffectiveOswaldFont()` and `getEffectiveShareTechFont()` transparently fall back to `EbfFont::getEmbeddedOswaldCondensed()` and `EbfFont::getEmbeddedShareTechMono()`.

### 2. Live Rendering Routes
- **Script Editor (`TextEditorWidget`)**: The monospace cell hook configured via [batchRenderer_->setMonospaceGlyphHook](file:///c:/git/eatsbits/src/ui/gui_window.cpp#L7340) directly resolves character codepoints through `g_ebfShareTech.findGlyph()`. Every character typed in the script editor renders using authentic converted ShareTech Mono vector contours aligned precisely to the cell baseline ($y + h \times 0.78$) and centered horizontally.
- **Syntax Highlighting & Telemetry**: [drawMonoString](file:///c:/git/eatsbits/src/ui/gui_window.cpp#L1702) and [drawSyntaxString](file:///c:/git/eatsbits/src/ui/gui_window.cpp#L1650) shape code and tracker strings directly through `g_ebfShareTech.drawText()`, accurately measuring widths via `measureText()`.
- **Primary UI Text & Buttons**: [drawVectorString](file:///c:/git/eatsbits/src/ui/gui_window.cpp#L1292), [drawVectorStringCentered](file:///c:/git/eatsbits/src/ui/gui_window.cpp#L1437), and [drawVectorStringLeftAlignedY](file:///c:/git/eatsbits/src/ui/gui_window.cpp#L1509) route through `g_ebfOswald`, featuring:
  - Full proportional tracking and kerning.
  - Optical visual bounding-box centering for single-character control badges and rotary knob centers.
  - Hybrid UTF-8 fallback for special UI navigation symbols (e.g. `⇆`, `⇄`, `♪`).
- **Runtime Control**: Can be toggled on or off programmatically or for benchmarking using `GuiWindow::setEbfTypographyEnabled(bool)` and `GuiWindow::isEbfTypographyEnabled()`.

---

## 14. Counter Aperture Preservation & Antialiasing Engine

### 1. Counter Aperture (Hole) Tessellation
Characters containing interior voids (e.g. `'O'`, `'D'`, `'0'`, `'8'`, `'A'`, `'P'`, `'R'`, `'B'`) require bridge cuts to merge inner hole contours into the outer polygon so ear-clipping can operate on a single simple polygon:
1. **Line-of-Sight Bridge Validation**: `EbfConverter::tessellatePolygonWithHoles` tests candidate outer vertices with `segmentsCross` to ensure the bridge cut between the hole's rightmost vertex and the outer edge has an unobstructed line of sight and does not cross any polygon boundary.
2. **Bridge Vertex Coincidence Skip**: Zero-width bridge cuts insert pairs of coincident vertices with identical $(x, y)$ coordinates. `earClip` skips coincident vertices (`|pk - a| < 1e-3`) during the interior-point test so ears touching the cut are not falsely rejected.
3. **Safe Centroid Fallback**: The fallback triangulation checks `pointInPolygon(mid, clean)` for each triangle candidate, preventing triangles from spanning across interior holes.
4. **Automated Aperture Assertion**: Automated tests verify that the center of glyph `'O'` is not inside any triangle mesh, ensuring counter apertures remain open.

### 2. FreeType vs. EBF Vector Antialiasing Mechanics
- **FreeType Atlas Rasterization**: FreeType draws font Bézier curves into an 8-bit alpha grayscale texture atlas on the CPU. It uses grid-fitting (stem hinting) to snap horizontal and vertical stems to exact integer display pixels, and calculates partial pixel area coverage for every texel on the edge.
- **EBF GPU Vector Tessellation**: EBF decomposes TrueType glyphs into triangle meshes and streams them directly into `BatchRenderer2D`. When rendered:
  - Without MSAA / supersampling, triangle coverage is binary (evaluated strictly at pixel centers).
  - With WebGPU/MSAA (or multi-sample anti-aliasing enabled), the GPU hardware performs subpixel multisampling at triangle edges.
  - At very small text sizes ($< 10\,\text{px}$), vector stems do not snap to integer pixel grids unless explicitly hinted, whereas at display sizes ($12\,\text{px} - 64\,\text{px}$), the true curved geometry scales cleanly at any zoom level without pixelation or texture memory limits.
- **Procedural Debug Font (`vector_font.cpp`)**: The procedural vector font uses single-stroke skeletons calibrated to whole- and half-pixel offsets, delivering high legibility at tiny sizes, and is retained as a user option.

### 3. Elimination of Needle Slivers & Crossing Artifacts ("X", "x", "O", "o")
- **The Needle Sliver Problem**: In naive ear-clipping, picking the first available ear creates fan triangulations from reflex corners. Across narrow diagonal strokes (such as in `'X'` and `'x'`), chord diagonals sliced across stems at acute $25^\circ - 30^\circ$ angles. At small font sizes, the needle tips were $< 0.2\,\text{px}$ wide, causing the GPU rasterizer to miss pixel centers and creating visible disconnections/notches in glyph limbs.
- **Best-Quality Delaunay Selection**: `earClip` now evaluates all candidate ears and clips the ear that maximizes the triangle aspect quality ratio:
  $$\text{quality} = \frac{\text{Area}}{d_0^2 + d_1^2 + d_2^2}$$
  This prioritizes fat, well-conditioned, equilateral-like triangles, breaking diagonal limbs into orthogonal rectangular segments and central crossing diamonds.
- **Chord and Centroid Containment**: Every ear candidate must satisfy:
  1. Centroid inside the polygon: `pointInPolygon(centroid, clean)` is strictly true.
  2. Chord non-intersection: The chord $a-c$ does not intersect any boundary edge of the polygon.
  This prevents triangles from jumping across the concave notches of `'X'` or bridge cuts in `'O'`.

---

## 15. Sub-Project Alignment Standards & Reference Matrix

To ensure visual consistency, pixel-perfect alignment, and zero regressions across all present and future sub-projects (DAW, CLAP plugins, terminal consoles, code editors, and mobile/web ports), all sub-projects must adhere to the following core typographic and layout standards.

### 1. Coordinate & Baseline Conventions

When placing text inside UI widgets, containers, or grid cells, use the standard baseline and bounding-box formulas:

| Widget Element | Standard Formula | Rationale |
| :--- | :--- | :--- |
| **Monospace Grid Cell** (Editor/Terminal) | `baselineY = cellY + cellH * 0.78f`<br>`charX = cellX + (cellW - advance) * 0.5f` | Aligns glyph x-height and descenders optical center to standard 8×16 or 10×20 terminal cells. |
| **Proportional Button Label** (Vertical Centering) | `baselineY = btnY + (btnH + font.getCapHeight() * (size / 1000.0f)) * 0.5f` | Centers capital letters optically within the button height rather than bottom-aligning. |
| **Single-Character Badge / Knob Center** | `cx = widgetX + (widgetW - bounds.width) * 0.5f - bounds.minX`<br>`cy = widgetY + (widgetH - bounds.height) * 0.5f - bounds.minY` | Uses `measureTextDetailed()` bounding box to achieve mathematical geometric centering. |
| **Multi-line Paragraph** | `lineStep = font.getLineHeight(fontSize)`<br>`nextBaselineY = currentY + lineStep` | Respects font metrics (`ascender - descender + lineGap`). |

### 2. Tessellation Quality Standard for Custom Glyphs
If a sub-project generates custom vector glyphs or tessellates arbitrary polygons, it must adhere to the **Best-Quality Delaunay Ear-Selection** rule:
- **Aspect Quality Metric**: Always clip candidate ears that maximize:
  $$\text{quality} = \frac{\text{Area}}{d_0^2 + d_1^2 + d_2^2}$$
- **Centroid Containment**: Verify `pointInPolygon(centroid, contour) == true` to prevent bridging concave notches.
- **Chord Crossing Check**: Verify `segmentsCross(chord, boundary) == false`.
- **Dropout Prevention**: Never emit acute fan triangles with tip widths below $0.5\,\text{px}$ across narrow strokes.

### 3. Hardware Antialiasing / MSAA Recommendation
- **Standard Pipeline Target**: Tessellated vector glyphs stream directly into GPU vertex buffers alongside UI quads.
- **Multisampling (MSAA)**: Sub-projects should configure their graphics context (WebGPU, DirectX, Vulkan, or OpenGL) with **4x MSAA**. Multi-sampling performs hardware subpixel evaluation along triangle edges, producing soft, beautiful font antialiasing matching FreeType while retaining 100% vector scalability.
- **Aliased Fallback**: If MSAA is disabled (e.g. low-power mode or software rasterizer), stems retain full geometric width without dropouts thanks to the Delaunay decomposition.

### 4. Offline Font Baking Workflow for Sub-Projects
When adding a new custom font to a sub-project, use the built-in generator to create embedded headers:

```cpp
#include "eatsbits/ui/glyph/ebf_converter.hpp"

// Convert TrueType outline into a single static C++ header for shipping
eatsbits::ui::glyph::EbfConverter::convertToCppHeader(
    "path/to/MyCustomFont.ttf",
    "include/my_subproject/my_font_data.hpp",
    "MyCustomFont"
);
```
Include `my_font_data.hpp` and instantiate `EbfFont(g_myCustomFontBytes, sizeof(g_myCustomFontBytes))` for instant, zero-file startup.

### 5. Architectural Alignment & Feature Matrix

| Sub-Project Type | Core Modules | Required Assets | FreeType Needed? | Memory Footprint |
| :--- | :--- | :--- | :--- | :--- |
| **Full DAW GUI** (`eatsbits_gui`) | `EbfFont`, `EbfConverter`, `GlyphRegistry`, `ScoreVectorGlyphs` | Bundled `.ttf` or `.ebf` | Optional (for TTF import) | Full feature set |
| **CLAP / VST Plugin** (`eatsbits_tb303`) | `EbfFont`, `VectorFont`, `IconRegistry` | Embedded ROM headers only | **No** | $< 150\,\text{KB}$ code overhead |
| **Interactive Terminal** (`eatsbits_term`) | `GlyphPack::Terminal`, `VectorFont` | None (pure math) | **No** | $< 25\,\text{KB}$ code overhead |
| **Script / Live Coder** (`TextEditorWidget`) | `EbfFont` (ShareTech), `GlyphPack::Editor` | Embedded ShareTech header | **No** | $< 40\,\text{KB}$ code overhead |
| **Headless Web / WASM** (`eatsbits.wasm`) | `EbfFont`, `BatchRenderer2D` | Embedded headers | **No** | Zero external network fetch |
