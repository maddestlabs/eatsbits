# Unified GUI Control Architecture & Refinement Plan (`gui-refine.md`)

## 1. Executive Summary & Goals

The goal of this architectural refactoring is to unify all continuous interactive controls (knobs, sliders, and faders) across the entire **Eatsbits** DAW. 

Currently, knob and slider rendering is fragmented across multiple independent implementations:
* High-detail skeuomorphic knobs in `gui_panel_def.cpp` (TB-303, Moog, Davies).
* Hardcoded custom volume pill sliders and pan dials in `arranger_view.cpp` track headers.
* Ad-hoc `drawKnobMini` lambdas in `track_properties_panel.cpp`.
* Custom fader geometry in `arranger_mixer_drawer.cpp` and `mixer_view.cpp`.

This document outlines the migration to a **Unified Composable Control Core** that:
1. **Natively Supports Modern Minimalist Styles** (Bitwig/Ableton/`gui-minimal`) as well as **Studio Hybrid Styles** (SMAOLAB/`gui-variations`) and **Skeuomorphic Vintage Hardware**.
2. **Introduces a Global User Setting (`Minimalist GUI Mode`)** in Settings/Preferences, allowing users to override all skeuomorphic rendering and enforce a clean, ultra-fast vector GUI DAW-wide.
3. **Eliminates Code Duplication** by replacing one-off implementations in the Arranger track list, Inspector, Mixer, and Dialogs with reusable, high-performance primitives.

---

## 2. Current Architecture Audit vs. Target State

### Current Implementation Bottlenecks

| Location | Control Type | Current Implementation | Issues |
| :--- | :--- | :--- | :--- |
| `src/ui/gui_panel_def.cpp` | Synthesizer & Effect Knobs | 1500+ lines of monolithic switch/case logic (`drawKnobWidget`) | Deeply branchy; tightly coupled to specific hardware presets; rigid vertical layout. |
| `src/ui/views/arranger_view.cpp` | Track Vol & Pan | Custom lines, circles, and rounded rects drawn directly in `renderTrackHeaders` (L1597-1625) | Inconsistent with main knobs; no drag-taper standardization; duplicated hit-test logic. |
| `src/ui/widgets/track_properties_panel.cpp` | Quick EQ & Limiter Knobs | Embedded lambda `drawKnobMini` (L1733) | Duplicated arc calculations; disconnected from central theme tokens. |
| `src/ui/widgets/arranger_mixer_drawer.cpp` | Track & Master Faders | Custom `computeFaderGeometry` and canvas drawing | Slider physics and aesthetics diverge from standard panel sliders. |

### The Target State: A Unified 5-Layer Core

Every continuous rotary or linear control in the DAW is rendered through a standardized 5-layer pipeline:

```
[Layer 1: Layout & Typography]  ---> (Vertical Stack, Horizontal Inline, or Micro)
[Layer 2: Scale, Ticks & Arc]   ---> (Unipolar Arc, Bipolar Center-Zero, or Stepped)
[Layer 3: Rotor / Track Body]   ---> (Flat Disc, Subtle Bevel, or Skeuomorphic Fluted)
[Layer 4: Cap / Inlay / Thumb]  ---> (Seamless, Lathe Aluminum, Scoop, or Fader Cap)
[Layer 5: Indicator & Pointer]  ---> (Vector Needle, Etched Notch, or Pip Dot)
```

---

## 3. Global User Setting: `Minimal GUI Mode`

Users frequently prefer minimalist, high-framerate interfaces (resembling Ableton Live, Bitwig, or Reaper) over heavy vintage recreations, especially on laptops or dense multi-track projects.

### Configuration Token
Add a global control mode setting to `ThemeTokens` / user configuration:

```cpp
enum class ControlAestheticMode : uint8_t {
    AutoThemeDefault = 0, // Respects plugin/preset skeuomorphic settings
    ForceMinimal     = 1, // Global override: clean vector aesthetics DAW-wide
    HighContrastFlat = 2  // Pure flat lines & high contrast for low-vision/performance
};

// In ThemeTokens (include/eatsbits/ui/theme.hpp):
struct ThemeTokens {
    // ... existing tokens ...
    ControlAestheticMode controlMode{ControlAestheticMode::AutoThemeDefault};
    bool showControlGradients{true};
    bool showControlDropShadows{true};
};
```

### The Zero-Cost Fast Path
When `controlMode == ControlAestheticMode::ForceMinimal`:
* All skeuomorphic passes (fluting, 3D shadow meshes, lathe highlights, skirt tick loops) are **completely bypassed** via a single branch at the top of the draw routine.
* Rendering falls back to:
  1. Subtle recessed track outline / well.
  2. Clean progress arc (`1.5px` to `2.5px`, unipolar or bipolar).
  3. Single-color rotor disc.
  4. Crisp hairline pointer needle.
* **Performance Gain:** Reduces vertex and triangle count per knob from ~300+ down to ~30, allowing 500+ animated controls to render simultaneously within a single `BatchRenderer2D` draw call at 144+ FPS.

---

## 4. Universal Widget Structure & API Specification

### 4.1 Composable Knob Descriptor (`KnobStyleCore`)

Replace monolithic `KnobShapeDef` in `gui_panel_def.hpp` with composable descriptors:

```cpp
namespace eatsbits::ui {

enum class KnobLayoutMode : uint8_t {
    VerticalStacked,    // Standard: Title Top, Knob Center, Value Bottom
    InlineRight,        // SMAOLAB style: Knob Left, Value & Title stacked to the right
    MicroInline,        // Arranger track header: compact 16-24px dial, no text or inline label
    OverlayOnHover      // Ableton style: Value swaps with Title inside rotor on hover
};

enum class KnobArcType : uint8_t {
    None,
    Unipolar,           // 0.0 to 1.0 (Volume, Cutoff, Drive)
    BipolarCenterZero,  // -1.0 to +1.0 with center detent (Pan, EQ Cut/Boost, Pitch Bend)
    SteppedGraduations  // Discrete positions (Filter types, octave selectors)
};

enum class RotorSurface : uint8_t {
    FlatVector,         // Pure minimalist flat circle (Bitwig/Ableton)
    SubtleGradient,     // Clean modern studio (SMAOLAB, FabFilter)
    BrushedAluminum,    // Lathe metallic disc
    FlutedHardware      // Skeuomorphic scalloped / knurled body
};

struct KnobStyleCore {
    KnobLayoutMode layout{KnobLayoutMode::VerticalStacked};
    KnobArcType arcType{KnobArcType::Unipolar};
    RotorSurface surface{RotorSurface::SubtleGradient};
    
    // Geometry & Sizing
    float arcThickness{2.2f};
    float arcRadiusOffset{3.5f};
    float pointerLengthRatio{0.80f};
    float pointerWidth{1.8f};
    
    // Ticks and Detents
    int tickCount{0};
    bool hasCenterDetent{false};
    
    // Colors (can fall back to ThemeTokens)
    std::optional<Color> trackColor;
    std::optional<Color> arcColor;
    std::optional<Color> bodyColor;
    std::optional<Color> pointerColor;
};

} // namespace eatsbits::ui
```

### 4.2 Universal Slider Descriptor (`SliderStyleCore`)

Unifies track volume faders, send bars, and property sliders:

```cpp
namespace eatsbits::ui {

enum class SliderOrientation : uint8_t {
    Horizontal,
    Vertical
};

enum class SliderThumbStyle : uint8_t {
    None,           // Fill-only progress bar
    PillThumb,      // Rounded modern pill (Arranger track style)
    HardwareCap     // Notched console fader cap (Mixer desk style)
};

struct SliderStyleCore {
    SliderOrientation orientation{SliderOrientation::Horizontal};
    SliderThumbStyle thumbStyle{SliderThumbStyle::PillThumb};
    bool bipolar{false}; // Center-zero for pan sliders
    float trackHeight{4.0f};
    float thumbWidth{10.0f};
    float thumbHeight{14.0f};
    bool showValueBadge{false};
};

} // namespace eatsbits::ui
```

---

## 5. Elimination of Ad-Hoc Implementations

### A. Arranger View Track Header (`arranger_view.cpp`)
* **Current:** Custom lines and circles in `renderTrackHeaders` (lines 1597–1625).
* **Replacement:** Call the unified `drawSlider()` and `drawKnob()` with:
  ```cpp
  // Unified Volume Slider in Arranger Header
  SliderStyleCore volStyle;
  volStyle.orientation = SliderOrientation::Horizontal;
  volStyle.thumbStyle = SliderThumbStyle::PillThumb;
  volStyle.trackHeight = 4.0f;
  drawSlider(r, Rect2D{sliderX, sliderY, sliderW, sliderH}, normV, volStyle, trackColor);

  // Unified Pan Micro-Knob in Arranger Header
  KnobStyleCore panStyle;
  panStyle.layout = KnobLayoutMode::MicroInline;
  panStyle.arcType = KnobArcType::BipolarCenterZero;
  panStyle.surface = RotorSurface::FlatVector;
  drawKnob(r, knobX, knobY, 9.0f, track.pan * 0.5f + 0.5f, panStyle, trackColor);
  ```

### B. Track Properties Panel (`track_properties_panel.cpp`)
* **Current:** Local lambda `drawKnobMini` at line 1733.
* **Replacement:** Replace `drawKnobMini` with calls to standard `drawKnob()` using `KnobLayoutMode::VerticalStacked` and `RotorSurface::SubtleGradient`.

### C. Fullscreen Device Modal & Synthesizer Panels
* **Current:** Branchy `drawKnobWidget` with hardcoded styling.
* **Replacement:** All custom presets (TB-303, Moog, Davies) compile down to a `KnobStyleCore`. When `ForceMinimal` is enabled, they automatically render as high-density vector controls without changing plugin logic.

---

## 6. Implementation Phasing & Roadmap

### Phase 1: Core Primitives & Declarative Descriptors
1. **Define Core Structs:** Add `KnobStyleCore` and `SliderStyleCore` in `include/eatsbits/ui/draw_utils.hpp`.
2. **Implement Unified Draw Functions:**
   * `void drawUnifiedKnob(BatchRenderer2D& r, float cx, float cy, float radius, float normVal, const KnobStyleCore& style, ...)`
   * `void drawUnifiedSlider(BatchRenderer2D& r, const Rect2D& bounds, float normVal, const SliderStyleCore& style, ...)`
3. **Support `gui-minimal` and `gui-variations` Presets:**
   * `KnobStyleCore::MinimalVector(Color accent)`
   * `KnobStyleCore::StudioModern(Color accent, bool bipolar)`
   * `KnobStyleCore::HardwareVintage(GuiKnobStyle vintageStyle)`

### Phase 2: Global Setting Integration
1. Add `controlMode` to `ThemeTokens` in `include/eatsbits/ui/theme.hpp`.
2. Add toggle in `Settings` / `CommandPaletteDialog`: `"UI: Toggle Minimalist Controls (Performance Mode)"`.
3. Wire the check in `drawUnifiedKnob` and `drawUnifiedSlider` to bypass 3D/skeuomorphic passes when active.

### Phase 3: DAW-Wide Migration
1. **Arranger View (`arranger_view.cpp`):** Refactor `renderTrackHeaders` to replace custom volume slider & pan knob with the unified calls.
2. **Track Properties Panel (`track_properties_panel.cpp`):** Replace `drawKnobMini` and custom faders.
3. **Mixer View (`mixer_view.cpp` & `arranger_mixer_drawer.cpp`):** Standardize fader wells and send/pan pots.
4. **GUI Window & Device Modals:** Route `drawKnobWidget` directly through `drawUnifiedKnob`.

### Phase 4: Verification & Automated Tests
1. **Visual Regression / Unit Tests:** Expand `tests/test_ui_widgets.cpp` to verify:
   * Value clamping and detent snapping on bipolar vs. unipolar controls.
   * Vertex allocation count under `ForceMinimal` mode vs. full skeuomorphic mode.
2. **Interactive Layout Test:** In `tests/test_modular_ui.cpp`, verify that `StackedVertical`, `InlineRight`, and `MicroInline` produce exact hit-test boundaries.

---

## 7. Deliverables Checklist

- [ ] `gui-refine.md` created in project root (this document).
- [ ] `KnobStyleCore` & `SliderStyleCore` declared in `include/eatsbits/ui/draw_utils.hpp`.
- [ ] `ThemeTokens::controlMode` added to `include/eatsbits/ui/theme.hpp`.
- [ ] Unified rendering logic implemented in `src/ui/draw_utils.cpp`.
- [ ] Arranger track header volume and pan migrated to unified functions.
- [ ] Track properties panel migrated to unified functions.
- [ ] Settings/palette command exposed for `Minimalist Controls`.
