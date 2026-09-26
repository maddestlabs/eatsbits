#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "../geometry.hpp"

namespace eatsbits::ui {

enum class SelectionSidebarAction {
    None,
    Close,
    Delete,
    TransposeOctDown,      // -12 semitones
    TransposeSemiDown,     // -1 semitone
    TransposeSemiUp,       // +1 semitone
    TransposeOctUp,        // +12 semitones
    NudgeLeft,             // -1 step
    NudgeRight,            // +1 step
    ShortenDuration,       // -0.25 length
    LengthenDuration,      // +0.25 length
    DurPreset25,           // 0.25 step
    DurPreset50,           // 0.50 step
    DurPreset75,           // 0.75 step
    DurPreset100,          // 1.00 step
    Vel25,                 // 25%
    Vel50,                 // 50%
    Vel75,                 // 75%
    Vel100,                // 100%
    Humanize,              // Humanize velocities (±15%)
    ToggleSlide,           // Portamento slide toggle
    ToggleAccent,          // Dynamic accent toggle
    SelectAll,             // Select all notes in clip
    Invert,                // Invert note selection
    Quantize,              // Quantize note start positions to grid
    Clear                  // Clear selection
};

struct HitTestSelectionSidebarResult {
    bool hit{false};
    SelectionSidebarAction action{SelectionSidebarAction::None};
    int paramValue{0};
    float floatValue{0.0f};
};

} // namespace eatsbits::ui
