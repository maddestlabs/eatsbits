#ifndef EATS_MACRO_RUNTIME_HPP
#define EATS_MACRO_RUNTIME_HPP

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>
#include "../audio/audio_engine.hpp"
#include "../sequencer/step_sequencer.hpp"
#include "midi_fx_pipeline.hpp"

namespace eatsbits::eatscript {

/**
 * Result of a macro execution.
 */
struct MacroResult {
    bool success{true};
    std::string message;
    std::vector<std::string> logs;
};

/**
 * Macro Definition for generative composing and project automation.
 */
struct MacroDefinition {
    std::string id;
    std::string name;
    std::string category;     // "Generative", "Arrangement", "Mixing", "Music Theory"
    std::string description;
    std::string scriptCode;
};

/**
 * High-Level DAW Macro Runtime (`eat.daw` / `project` API).
 * Enables AI agents and human users to orchestrate the DAW, generate whole multi-track patterns,
 * apply procedural transformations, and automate project configuration.
 */
class MacroRuntime {
public:
    MacroRuntime();

    /// Returns a catalog of built-in generative and workflow macros.
    [[nodiscard]] static const std::vector<MacroDefinition>& getBuiltinMacros();

    /// Executes an Eatscript macro script against the active DAW state.
    MacroResult execute(const std::string& macroScript,
                        audio::AudioEngine& engine,
                        sequencer::StepSequencer& sequencer);

    /// Convenience helpers for common procedural compositions:
    static MacroResult generateAcidBassline(sequencer::SequencerTrack& track,
                                           int rootPitch = 36,
                                           uint32_t numSteps = 16,
                                           uint32_t seed = 42);

    static MacroResult generateDrumPattern(sequencer::StepSequencer& sequencer,
                                          const std::string& style = "Techno",
                                          uint32_t seed = 42);

    static MacroResult generateProceduralSong(sequencer::StepSequencer& sequencer,
                                             const std::string& style = "Lo-Fi Hip Hop",
                                             uint32_t bars = 16,
                                             uint32_t seed = 42);

    static MacroResult humanizeAllTracks(sequencer::StepSequencer& sequencer,
                                        float timingJitter = 0.03f,
                                        float velocityJitter = 0.12f,
                                        uint32_t seed = 42);

    static MacroResult arpeggiateTrack(sequencer::SequencerTrack& track,
                                      double rate = 1.0,
                                      int octaves = 2,
                                      const std::string& pattern = "up");
};

} // namespace eatsbits::eatscript

#endif // EATS_MACRO_RUNTIME_HPP
