#ifndef EATS_PROJECT_SERIALIZER_HPP
#define EATS_PROJECT_SERIALIZER_HPP

#include <string>
#include <vector>
#include <memory>
#include "../audio/graph/audio_graph.hpp"
#include "../sequencer/step_sequencer.hpp"

namespace eatsbits::project {

/**
 * Authentic .eats Song Container Format Serializer and Parser.
 * 100% interoperable with original Eatsbeats DAW project files.
 * Formats projects as declarative Eatscript/Lua tables with complete track, note,
 * instrument script, step data, and metadata encapsulation.
 */
class EatsProjectSerializer {
public:
    /// Serializes project state into an authentic .eats script string.
    [[nodiscard]] static std::string serialize(
        const audio::AudioGraph& graph,
        const sequencer::StepSequencer& sequencer,
        const std::string& title = "Untitled Song",
        double bpm = 135.0,
        double swing = 0.50);

    /// Parses an authentic .eats script string and reconstructs project state.
    static bool deserialize(
        const std::string& eatsScript,
        audio::AudioGraph& graph,
        sequencer::StepSequencer& sequencer,
        std::string& outTitle,
        double& outBpm,
        double& outSwing);

    /// Helper to test if a file content represents authentic .eats format.
    [[nodiscard]] static bool isEatsScriptFormat(const std::string& content) noexcept;
};

} // namespace eatsbits::project

#endif // EATS_PROJECT_SERIALIZER_HPP
