# Unified UI Layout Architecture: Clay Engine Unification

**Document Version:** 1.2.0 (Eatsbits Core Refinement & Full Parity Verification)  
**Target Projects:** `c:\git\eatscode` & `c:\git\eatsbits`  
**Status:** **Phase 1 & Phase 2 Complete** (Full Bi-directional Parity: Eatscode 145/145 Tests Passing, Eatsbits 10/10 Layout & 29/29 Modular UI Suites Passing)  
**Goal:** Establish a single, shared, high-performance immediate-mode layout core powering both the DAW (Eatsbits) and IDE (Eatscode) across WebGPU GUI and Terminal TUI surfaces.

---

## 1. Executive Summary & Verification Milestone

### 1.1 Eatscode Status (Clay-Powered)
Eatscode has integrated the Clay layout engine:
* **[CMakeLists.txt](file:///c:/git/eatscode/CMakeLists.txt):** Added `${EATSBITS_ROOT}/third_party/clay` to `EATSBITS_INCLUDE_DIRS` and linked against `eats_clay.lib`.
* **[ui_layout.hpp](file:///c:/git/eatscode/include/eatscode/ui_layout.hpp):** Enhanced `eatscode::UiLayoutEngine` with an integrated [`eatsbits::ui::clay::ClayContext`](file:///c:/git/eatsbits/include/eatsbits/ui/clay/clay_context.hpp).
* **Linear Arena Solver:** `UiLayoutEngine::solve()` constructs a Clay declaration hierarchy in a pre-allocated 1MB linear arena, executes `Clay_EndLayout()`, and synchronizes computed `Clay_BoundingBox` coordinates back to the application.
* **Fluent RAII Wrappers:** Introduced `IdScope`, `ContainerScope`, `Row::Fixed`, `Row::Flex`, `Column::Fixed`, `Column::Flex`, and `Stack::Begin`.
* **100% Backward Compatibility:** Scripted layouts in [init.eats](file:///c:/git/eatscode/init.eats) run without modifications, and **all 145 automated unit tests pass**.

### 1.2 Eatsbits Core Status (Refined & Unified)
Eatsbits core has adopted the Eatscode architectural improvements:
* **[clay_context.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/clay/clay_context.hpp) & [clay_context.cpp](file:///c:/git/eatsbits/src/ui/clay/clay_context.cpp):** Added thread-local context tracking (`ClayContext::getCurrent()`) and hierarchical ID scoping (`pushId`, `popId`, `getScopedId`, `makeId`).
* **[components.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/clay/components.hpp):** Added comprehensive fluent C++ RAII primitives: `IdScope`, `ContainerScope`, `Row::Fixed`, `Row::ScopedFixed`, `Row::Flex`, `Row::ScopedFlex`, `Column::Fixed`, `Column::ScopedFixed`, `Column::Flex`, `Column::ScopedFlex`, `Stack::Begin`, `Stack::ScopedBegin`, `UI_Spacer`, `UI_ID`, and `UI_IDI`.
* **[ui_layout.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/clay/ui_layout.hpp):** Created `eatsbits::ui::clay::UiLayoutEngine` providing a unified immediate-mode layout engine with explicit overlay isolation (`isExplicitRect` protection preventing flow displacement).
* **[bottom_nav_bar.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/bottom_nav_bar.hpp) & [transport_header.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/widgets/transport_header.hpp):** Dismantled the micro-arena anti-pattern. Removed isolated 512KB heap allocations per widget in favor of direct participation in the window's layout pass via `declareLayout(ClayContext&, ...)` and shared static fallback arenas.
* **[eatscript_clay_bridge.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/clay/eatscript_clay_bridge.hpp) & [eatscript_clay_bridge.cpp](file:///c:/git/eatsbits/src/ui/clay/eatscript_clay_bridge.cpp):** Implemented the full Eatscript host registry layout bridge (`ui_begin_column`, `ui_begin_row`, `ui_begin_fixed_row`, `ui_begin_flex_row`, `ui_begin_stack`, `ui_end`, `ui_spacer`, `ui_push_id`, `ui_pop_id`, `ui_set_min_width`, `ui_set_min_height`, `ui_set_fixed_size`).
* **Verification Suite:** Added Tests 7–10 to [test_clay_layout.cpp](file:///c:/git/eatsbits/tests/test_clay_layout.cpp) (10/10 tests pass). Ran [test_modular_ui.cpp](file:///c:/git/eatsbits/tests/test_modular_ui.cpp) (29/29 modular UI/UX suites pass with 0 regressions).

### 1.3 Architectural Parity Matrix

| Dimension | Eatscode (Current) | Eatsbits (Current) | Unified Shared Core Status |
| :--- | :--- | :--- | :--- |
| **Engine Foundation** | Clay (`clay.h`) via `ClayContext` | Clay (`clay.h`) via `ClayContext` | **Unified & Parity Verified** |
| **Memory Allocation** | Zero-allocation linear arena (1 MB) | Zero-allocation linear arena (1 MB) | **Unified & Parity Verified** |
| **Layout Tree Structure** | Single top-level tree pass | Single top-level tree pass (shared context) | **Unified & Parity Verified** |
| **Ergonomics & Syntax** | Fluent RAII + Semantic Primitives | Fluent RAII + Semantic Primitives (`components.hpp`) | **Unified & Parity Verified** |
| **Layering / Overlays** | First-class Stack (`Stack::Begin`) | First-class Stack (`Stack::Begin`) + `isExplicitRect` | **Unified & Parity Verified** |
| **ID Management** | Hierarchical `IdScope` (`push_id`/`pop_id`) | Hierarchical `IdScope` + thread-local context | **Unified & Parity Verified** |
| **Scriptability** | 100% Eatscript-driven (`init.eats`) | Eatscript Host Registry Bridge (`abi::HostRegistry`) | **Unified & Parity Verified** |
| **Dual-Surface Output** | `BatchRenderer2D` + `CellSurface` | `ClayBatchBridge` + `ClayTuiBridge` | **Unified & Parity Verified** |

---

## 2. Crucial Implementation Lessons Learned from Eatscode

During the integration in Eatscode, three subtle interactions were discovered and solved. **These have now been fully integrated into Eatsbits core:**

### 2.1 Preserving Explicit Overlays (`isExplicitRect`)
* Real-world applications mix auto-flexbox layouts with explicit coordinate widgets (e.g. title-bar logo buttons at fixed offsets `(14, 10, 36, 36)`, minimap scrubbers, or floating tooltips).
* **Gotcha:** If explicit nodes are added to Clay's flow tree, Clay's flexbox solver will displace them into flow order, overriding their explicit screen positions.
* **Solution in Eatsbits Core:** `UiLayoutEngine` checks `if (node->isExplicitRect) continue;` when building the Clay tree, ensuring explicit screen rectangles remain intact while flow nodes are flexed around them.

### 2.2 Invisible Containers vs Render Commands
* **Gotcha:** In Clay, pure layout containers (`CLAY_LAYOUT`) without `.backgroundColor`, `.border`, or text do **not** generate items in `Clay_RenderCommandArray`. They compute bounding boxes for `Clay_GetElementData()`, but `commands.length` remains 0.
* **Solution in Eatsbits Core:** Widgets and layout declarations assign visual styling (e.g. colors, borders, or custom render delegates) to generate render commands, while pure flex containers serve bounding box synchronization.

### 2.3 Single Arena vs Micro-Arena Anti-Pattern
* In Eatscode, a single `ClayContext` with a 1MB arena solves the **entire application** (header, sidebar, code editor, REPL dock, footer, overlays) in one pass (`< 40 µs`).
* In Eatsbits, multiple widgets previously allocated their own 512KB `ClayContext` instances.
* **Solution in Eatsbits Core:** Widgets now implement `declareLayout(ClayContext&, ...)` and accept an optional pointer to the window's shared `ClayContext`. Standalone executions fall back to a single shared static arena, eliminating repeated heap thrashing.

---

## 3. C++ Fluent Wrappers & Core API Reference (`components.hpp`)

Available in [include/eatsbits/ui/clay/components.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/clay/components.hpp):

```cpp
namespace eatsbits::ui::clay {

// --- RAII Hierarchical ID Scoping ---
{
    IdScope scope("channel_strip");
    Clay_ElementId id = UI_ID("fader");      // Hashed as "channel_strip.fader"
    Clay_ElementId indexed = UI_IDI("fx", 2); // Hashed as "channel_strip.fx[2]"
}

// --- Fluent Scoped Layout Primitives ---
{
    auto header = Row::ScopedFixed(48.0f, 8.0f);
    // Add header children...
}

{
    auto body = Row::ScopedFlex(1.0f, 4.0f);
    {
        auto sidebar = Column::ScopedFixed(240.0f, 4.0f);
        // Add sidebar items...
    }
    {
        auto mainContent = Column::ScopedFlex(1.0f, 4.0f);
        // Add track arrange view...
    }
}

{
    auto overlay = Stack::ScopedBegin();
    // Layered floating views...
}

// Spacers
UI_Spacer();               // Flexible spacer (grows to fill available space)
UI_Spacer(16.0f, false);   // Fixed 16px spacer in row
} // namespace eatsbits::ui::clay
```

---

## 4. Completed Milestones & Checklist

- [x] **CMake Integration in Eatscode:** Add `third_party/clay` and link `eats_clay.lib`.
- [x] **Clay Engine in UiLayoutEngine (Eatscode):** Integrate `ClayContext` into [ui_layout.hpp](file:///c:/git/eatscode/include/eatscode/ui_layout.hpp).
- [x] **Fluent C++ RAII Builders (Eatscode & Eatsbits):** Implement `IdScope`, `ContainerScope`, `Row`, `Column`, `Stack`, `UI_Spacer` in [components.hpp](file:///c:/git/eatsbits/include/eatsbits/ui/clay/components.hpp).
- [x] **Explicit Rect Protection (Both):** Guard `isExplicitRect` nodes from flow overrides in `UiLayoutEngine`.
- [x] **Dismantle Micro-Arenas (Eatsbits):** Refactor `BottomNavBar` and `TransportHeader` to participate in shared `ClayContext` passes.
- [x] **Eatscript Layout Bridge (Eatsbits):** Expose layout primitives to Eatscript via `registerClayLayoutHostFunctions` in [eatscript_clay_bridge.cpp](file:///c:/git/eatsbits/src/ui/clay/eatscript_clay_bridge.cpp).
- [x] **Automated Test Validation:**
  - **Eatscode:** 145/145 unit tests passing.
  - **Eatsbits Layout:** 10/10 tests in [test_clay_layout.exe](file:///c:/git/eatsbits/build/Release/test_clay_layout.exe) passing.
  - **Eatsbits Modular UI:** 29/29 suites in [test_modular_ui.exe](file:///c:/git/eatsbits/build/Release/test_modular_ui.exe) passing.

