#pragma once

#include <string>
#include <vector>
#include "eatsbits/eatscript/note_script.hpp"

namespace eatsbits::project {

struct SequenceDefinition {
    std::string id;
    std::string name;
    std::string category;     // "MIDI_SEQ"
    std::string subCategory;  // e.g. "Acid", "Techno", "Kick", "Hi-Hat", "Snare", "Bassline", "Chords", "Arpeggio"
    std::string description;
    std::string tags;
    std::string eatscriptCode;
    float colorR{0.0f};
    float colorG{0.90f};
    float colorB{0.46f};
};

class SequenceCatalog {
public:
    [[nodiscard]] static const std::vector<SequenceDefinition>& getAllSequences();
    [[nodiscard]] static const SequenceDefinition* findSequence(const std::string& id);
    [[nodiscard]] static std::vector<eatscript::NoteScriptEngine::ClipNoteEvent> getSequenceEvents(const std::string& id);
};

} // namespace eatsbits::project
