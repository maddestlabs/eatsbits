---
description: Core architectural rules and development guidelines for the Eatsbits native C++ DAW.
always_on: true
---

# Eatsbits Development Rules

## 1. Zero-Allocation Real-Time Audio Core
- **Strictly No Dynamic Allocations**: Never call `malloc`, `free`, `new`, `delete`, or allocate memory inside real-time audio threads (`src/audio/`, DSP processing blocks, or sample-rendering loops).
- **No Vector/String Resizing**: Avoid `std::vector::push_back`, `std::string` concatenation, or resizing operations on audio paths. Use fixed buffers, pre-allocated rings, and lock-free queues.
- **No Blocking Primitives**: Avoid mutex locks or file I/O within the audio callback.

## 2. Modular Subsystem Pattern
- Keep view-specific logic inside modular classes in `src/ui/views/` and `src/ui/widgets/`.
- Do NOT inline view-specific drawing or event branches directly into `src/ui/gui_window.cpp`.
- Render views via the `ViewContext` and `BatchRenderer2D`.

## 3. Theme & Aesthetics
- Use `ThemeTokens` (`theme.primaryAccent`, `theme.controlBackground`, etc.) instead of hardcoded magic colors.
- Preserve diegetic analog aesthetics and high-DPI scaling.

## 4. Verification & Testing
- Always verify changes via `.\build.ps1 -Test` or CTest before finishing tasks.
- Keep test pass rates at 100%.
