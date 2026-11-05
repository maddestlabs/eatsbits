#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "../geometry.hpp"

namespace eatsbits::eatscript {
    class Evaluator;
}

namespace eatsbits::ui {

enum class SelectionSidebarAction {
    None,
    Close,
    Delete,
    Transpose,             // Unified transpose with paramValue (-12, -1, 1, 12, etc.)
    Nudge,                 // Unified nudge with floatValue (-1.0, 1.0, etc.)
    ChangeDuration,        // Unified duration change with floatValue (-0.25, 0.25, etc.)
    SetDuration,           // Unified set duration with floatValue (0.25, 0.50, 0.75, 1.00)
    SetVelocity,           // Unified set velocity with floatValue (0.25, 0.50, 0.75, 1.00)
    Humanize,              // Humanize velocities (+- floatValue, default 0.15)
    ToggleSlide,           // Portamento slide toggle
    SetSlide,              // Set slide directly with paramValue (0 or 1)
    ToggleAccent,          // Dynamic accent toggle
    SetAccent,             // Set accent directly with paramValue (0 or 1)
    SetArticulation,       // Set articulation stringParam (normal, stacc, pizz, legato, etc.)
    Audition,              // Audition selected note
    SelectAll,             // Select all notes in clip
    Invert,                // Invert note selection
    Quantize,              // Quantize note start positions to grid
    Retrograde,            // Reverse note order in time
    InvertPitch,           // Invert pitch around median / scale
    Clear,                 // Clear selection
    Script,                // Custom Eatscript code execution
    Custom,                // Custom user action

    // Backward compatibility aliases
    TransposeOctDown,
    TransposeSemiDown,
    TransposeSemiUp,
    TransposeOctUp,
    NudgeLeft,
    NudgeRight,
    ShortenDuration,
    LengthenDuration,
    DurPreset25,
    DurPreset50,
    DurPreset75,
    DurPreset100,
    Vel25,
    Vel50,
    Vel75,
    Vel100
};

struct HitTestSelectionSidebarResult {
    bool hit{false};
    SelectionSidebarAction action{SelectionSidebarAction::None};
    int paramValue{0};
    float floatValue{0.0f};
    std::string stringParam;
    std::string scriptCode;
    std::string customActionId;
    std::string actionName;
};

enum class InspectorItemType {
    Button,
    Chip,
    Toggle,
    Badge,
    Label
};

struct InspectorItem {
    std::string id;
    std::string label;
    InspectorItemType type{InspectorItemType::Button};
    SelectionSidebarAction action{SelectionSidebarAction::None};
    int intParam{0};
    float floatParam{0.0f};
    std::string stringParam;
    std::string scriptCode;
    std::string customActionId;

    // Layout & Sizing (0 = flex / auto)
    float width{0.0f};
    float height{22.0f};

    // Styling
    uint32_t colorHex{0}; // 0 = theme default
    std::string visibility{"always"}; // "always", "single", "multi"
};

struct InspectorRow {
    std::vector<InspectorItem> items;
    float rowHeight{22.0f};
    float marginTop{0.0f};
    float marginBottom{6.0f};
};

struct InspectorSection {
    std::string id;
    std::string title;
    std::vector<InspectorRow> rows;
    std::string visibility{"always"}; // "always", "single", "multi"
};

class NoteInspectorRegistry {
public:
    static NoteInspectorRegistry& instance();

    void resetToDefaults();
    void clear();

    void addSection(const InspectorSection& section);
    bool removeSection(const std::string& sectionId);
    InspectorSection* findSection(const std::string& sectionId);

    [[nodiscard]] const std::vector<InspectorSection>& getSections() const noexcept { return sections_; }
    [[nodiscard]] std::vector<InspectorSection>& getSections() noexcept { return sections_; }

    void addCustomButton(const std::string& sectionTitle,
                         const std::string& label,
                         const std::string& scriptCode,
                         float width = 0.0f);

    void registerEatscriptApi(eatscript::Evaluator& evaluator);

private:
    NoteInspectorRegistry();
    std::vector<InspectorSection> sections_;
};

} // namespace eatsbits::ui
