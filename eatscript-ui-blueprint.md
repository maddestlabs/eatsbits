# Eatscript UI & Embedded Asset Blueprint (`eatscript-ui-blueprint.md`)

## 1. Vision & Architecture Overview

Eatscript is evolving beyond an audio-rate DSL into a **complete, self-contained language and runtime** for:
1. **Interactive Audio Instruments & FX** (CLAP/DAW plugins).
2. **Embedded GUI Frontpanels & Visualizers** (Modular rigs, oscilloscopes, step sequencers).
3. **Cross-Platform Standalone Apps & Games** that run with identical fidelity on Native Desktop (C++/OpenGL/WebGPU) and WebAssembly (Canvas/WebGPU).

To achieve this without the bloat of monolithic frameworks (like JUCE or iPlug2), Eatscript adopts two core architectural pillars:
1. **An Idiomatic Declarative UI DSL** backed by the micro-layout engine **Clay**.
2. **An In-Memory Embedded Asset System** storing textures, filmstrips, and fonts directly inside script source as compressed strings (Base64/QOI/PNG).

---

## 2. Declarative Eatscript UI DSL (Powered by Clay)

### 2.1 Why Not Expose Raw `CLAY` Macros?
Exposing C99 macros (`CLAY(...)`, `CLAY_LAYOUT({.sizing = ...})`) in Eatscript introduces syntax friction and domain confusion. Instead, Eatscript provides a clean, domain-specific `ui` module that compiles directly into Clay layout nodes under the hood.

### 2.2 Syntax Specification
```javascript
// Example: Complete Stereo Compressor Plugin in Eatscript
import { ui, audio } from "eatsbits";

export default {
    params: {
        threshold: { min: -60, max: 0, default: -20, unit: "dB" },
        ratio:     { min: 1,  max: 20, default: 4, unit: ":1" },
        attack:    { min: 0.1, max: 100, default: 10, unit: "ms" },
        release:   { min: 10, max: 1000, default: 100, unit: "ms" },
        makeup:    { min: 0,  max: 24, default: 0, unit: "dB" },
        mix:       { min: 0,  max: 100, default: 100, unit: "%" }
    },

    renderUI() {
        ui.panel({ title: "OPTO-COMP V2", padding: 12, gap: 10 }, () => {
            // Header Section: Status & Preset Selector
            ui.row({ align: "space-between", grow: true }, () => {
                ui.label("DYNAMIC OPTO-CELL", { color: "#8ca3ba", size: 9 });
                ui.badge("ACTIVE", { color: "#00ff66" });
            });

            // Main Knob Controls Row (using unified procedural studio controls)
            ui.row({ gap: 14, align: "center" }, () => {
                ui.knob("THRESH", this.params.threshold, { style: "studio", accent: "#ff8c00" });
                ui.knob("RATIO",  this.params.ratio,     { style: "studio", accent: "#ff8c00" });
                ui.knob("ATTACK", this.params.attack,    { style: "minimal", accent: "#21f4e8" });
                ui.knob("RELEASE",this.params.release,   { style: "minimal", accent: "#21f4e8" });
                ui.knob("MIX",    this.params.mix,       { style: "studio", accent: "#ff007a" });
            });

            // Gain Reduction Meter
            ui.row({ grow: true }, () => {
                ui.meter("GR", this.signals.gainReduction, { range: [-24, 0], color: "#ff3333" });
            });
        });
    }
};
```

### 2.3 How the DSL Maps to Clay C++ Engine
When Eatscript executes `ui.panel()`, `ui.row()`, and `ui.knob()`:
1. `ui.panel()` creates a top-level Clay container element with `CLAY_LAYOUT({.layoutDirection = CLAY_TOP_TO_BOTTOM, ...})`.
2. `ui.row()` creates a child container with `CLAY_LAYOUT({.layoutDirection = CLAY_LEFT_TO_RIGHT, ...})`.
3. `ui.knob()` allocates an interactive leaf node in Clay, computes its bounding box `Rect2D`, and passes the coordinates directly to `drawUnifiedKnob()` in `BatchRenderer2D`.

---

## 3. Embedded In-Memory Asset Architecture

### 3.1 The Problem with External Asset Files
* Traditional audio plugins require loose files: `knob_strip.png`, `background.jpg`, `custom_font.ttf`.
* When users share `.eats` scripts, presets, or source code across Discord, GitHub, or forums, relative file paths break, textures go missing, and dependencies fail to resolve.

### 3.2 The Solution: String-Embedded Assets
Eatscript allows all binary visual assets to be declared as **data URI strings** directly inside the script:

```javascript
// Embedded 64-frame 3D Knob Filmstrip (Rendered in Blender / Cinema4D)
const KNOB_SKIN = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAA..." ;

// Embedded Background Texture (Brushed Aluminum Chassis Plate)
const CHASSIS_BG = "data:image/png;base64,UklGRmIAAABXRUJQVlA4...";

export default {
    init() {
        // Loaded into GPU texture atlas once on instantiation
        this.knobTexture = ui.loadTexture(KNOB_SKIN);
        this.chassisTexture = ui.loadTexture(CHASSIS_BG);
    },

    renderUI() {
        ui.panel({ backgroundTexture: this.chassisTexture }, () => {
            ui.knob("FILTER", this.params.cutoff, {
                filmstrip: this.knobTexture,
                frames: 64
            });
        });
    }
};
```

### 3.3 The C++ Decompression & Upload Pipeline

```
[ Eatscript String: "data:image/png;base64,..." ]
                  │
                  ▼ (One-Time Execution on Plugin Instantiation)
┌────────────────────────────────────────────────────────┐
│ 1. Base64 Decoder (Fast 256-entry lookup table)        │
│    -> Extracts raw compressed PNG / QOI bytes in RAM   │
├────────────────────────────────────────────────────────┤
│ 2. stb_image Memory Loader (Already in third_party/)   │
│    -> stbi_load_from_memory(bytes, len, &w, &h, &c, 4) │
│    -> Produces raw RGBA8 pixel buffer in < 1ms         │
├────────────────────────────────────────────────────────┤
│ 3. BatchRenderer2D Texture Allocation                  │
│    -> Uploads to OpenGL / WebGPU texture slot or atlas │
│    -> Returns lightweight uint32_t TextureHandle       │
└────────────────────────────────────────────────────────┘
                  │
                  ▼ (Zero-Cost Runtime Run Loop)
[ BatchRenderer2D::drawTexturedQuad() - Pure GPU texture sampling at 144+ FPS ]
```

### 3.4 Supported Visual Formats:
1. **PNG (via `stbi_load_from_memory`):** High universal compatibility; supports lossless alpha channels.
2. **QOI (Quite OK Image):** Single-header C format; 20x faster decode than PNG with virtually identical compression ratios.
3. **SVG Vector Paths:** Can also be stored as raw strings and rasterized directly by our vector engine.

---

### 3.5 Audio Asset Embedding (Samples, Wavetables, Impulse Responses)

The exact same string-embedding architecture applies to **audio data**, unlocking true zero-dependency sound engines:

```javascript
// Embedded 808 Kick Drum Sample (16-bit 44.1kHz PCM WAV as Base64)
const KICK_WAV = "data:audio/wav;base64,UklGRiYAAABXQVZFZm10IBAAAA...";

// Embedded Guitar Cabinet Impulse Response (Convolution Reverb)
const CAB_IR_WAV = "data:audio/wav;base64,UklGRvgAAABXQVZFZm10IBAAAA...";

// Embedded Single-Cycle Wavetable (AKWF Cycle)
const SAW_CYCLE = "data:audio/wav;base64,UklGRmQAAABXQVZFZm10IBAAAA...";

export default {
    init() {
        // Decoded once into memory float buffers on instrument instantiation
        this.kickSample  = audio.loadSample(KICK_WAV);
        this.cabConvolver = audio.loadImpulseResponse(CAB_IR_WAV);
        this.wavetable   = audio.loadWavetable(SAW_CYCLE);
    },

    process(inputs, outputs) {
        // High-speed realtime playback & convolution with ZERO disk I/O
    }
};
```

#### Audio Decompression Pipeline:
1. **Decoder:** Powered by `ma_dr_wav` / `dr_wav` (already built into `third_party/miniaudio`).
2. **Execution:** On plugin instantiation, `base64Decode()` unpacks the bytes in RAM, and `ma_dr_wav_init_memory()` decodes raw PCM floats directly into an `AudioBuffer`.
3. **Zero Audio-Thread Allocations:** During audio playback, the sample is played back from pre-allocated memory buffers—**zero allocations and zero disk I/O in the realtime audio thread**.

#### Primary Audio Use Cases:
* **Self-Contained Drum Machines & Samplers:** An entire 808/909 drum kit or acoustic snare instrument inside a single `.eats` text file.
* **Cabinet Simulators & Convolution Reverbs:** Guitar amp modelers with their matching 4x12 cab IR embedded in the plugin.
* **Wavetable Synthesizers:** Rich morphing wavetables stored without requiring gigabytes of external sample folders.
* **Game Audio & App UI SFX:** UI clicks, game sound effects, and ambient loops packaged in a standalone script.

---

## 4. In-DAW Tooling: "Embed Asset into Script"

To make this seamless for artists and sound designers who do not want to manually run base64 CLI tools:

1. **Drag-and-Drop Asset Embedding:**
   * Dropping a `.png` or `.jpg` file onto the Eatscript IDE window automatically converts it into a `const ASSET_NAME = "data:image/png;base64,..."` declaration at the top of the file.
2. **Asset Inspector Panel:**
   * An in-editor drawer showing embedded assets, dimensions (e.g. `48x3072 px, 64 frames`), memory footprint, and live preview.

---

## 5. CLAP Plugin Export Synergy

When a user clicks **[ Export to CLAP ]**:
* The Eatscript transpiler (`src/eatscript/transpiler.cpp`) generates standalone C++ code wrapped by `EATS_CLAP_EXPORT_PLUGIN`.
* The embedded asset strings are compiled directly into the `.clap` binary as `static const char*` data.
* **Result:** A standalone `.clap` plugin file that contains **both DSP engine and entire graphical skin** in a single binary (< 2 MB) with zero external folder dependencies!

---

## 6. Implementation Phasing

* [x] **Phase 1: Memory Image Loader in C++**
  * Added `base64Decode()`, `base64Encode()`, `parseDataUri()`, and `generateEmbeddedAssetDeclaration()` in [`include/eatsbits/ui/base64.hpp`](file:///c:/git/eatsbits/include/eatsbits/ui/base64.hpp) and [`src/ui/base64.cpp`](file:///c:/git/eatsbits/src/ui/base64.cpp).
  * Added `BatchRenderer2D::createTextureFromMemory(const void* data, size_t len)` utilizing `stbi_load_from_memory` and in-memory texture handle caching in [`include/eatsbits/ui/batch_renderer_2d.hpp`](file:///c:/git/eatsbits/include/eatsbits/ui/batch_renderer_2d.hpp) and [`src/ui/batch_renderer_2d.cpp`](file:///c:/git/eatsbits/src/ui/batch_renderer_2d.cpp).
  * Added filmstrip quad rendering in `BatchRenderer2D` (`drawFilmstripFrame()`) and unified knob/slider filmstrip support in [`src/ui/draw_utils.cpp`](file:///c:/git/eatsbits/src/ui/draw_utils.cpp).
* [x] **Phase 2: Eatscript Host ABI Binding**
  * Implemented and exposed `ui.loadTexture(uri)` to Eatscript VM and Host ABI in [`src/ui/clay/eatscript_clay_bridge.cpp`](file:///c:/git/eatsbits/src/ui/clay/eatscript_clay_bridge.cpp).
  * Added `filmstrip` / `texture`, `frames`, `style`, and `horizontal` options to `ui.knob` and `ui.slider`.
* [x] **Phase 3: Declarative Clay DSL Wrapper**
  * Added Eatscript wrapper functions `ui.panel()`, `ui.row()`, `ui.column()`, `ui.stack()`, `ui.knob()`, `ui.slider()`, `ui.label()`, `ui.badge()`, `ui.meter()`, `ui.spacer()` mapping directly to Clay elements and `UiLayoutEngine`.
  * Verified nested closures and scoped ID resolution in [`tests/test_clay_layout.cpp`](file:///c:/git/eatsbits/tests/test_clay_layout.cpp).
* [x] **Phase 4: Drag & Drop In-Editor Asset Embedding**
  * Added `embedAssetFromFile(filePath, insertAtTop)` and `handleFileDrop(filePaths)` in [`include/eatsbits/ui/widgets/text_editor_widget.hpp`](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/text_editor_widget.hpp) and [`src/ui/widgets/text_editor_widget.cpp`](file:///c:/git/eatsbits/src/ui/widgets/text_editor_widget.cpp) to convert dropped image/audio assets directly into `const ASSET_NAME = "data:...;base64,...";` declarations.

