#ifndef EATS_PROJECT_FILE_HPP
#define EATS_PROJECT_FILE_HPP

#include <string>
#include <memory>
#include "../audio/graph/audio_graph.hpp"
#include "../sequencer/step_sequencer.hpp"
#include "../theory/chord_model.hpp"

namespace eatsbits::project {

/**
 * Project Manager for the .eats JSON project file format.
 * Encapsulates full serialization and deserialization of:
 * - Project Metadata (Title, BPM, Swing, Song Key)
 * - Chord Track (Harmonic events, qualities, slash chords, duration)
 * - AudioGraph Topology (Nodes, parameters, cable patch routing)
 * - Multi-Track Step Sequencer (Patterns, tracks, steps, parameter locks)
 * - Embedded Eatscript live-coding sources
 */
class ProjectFile {
public:
    static bool saveToFile(const std::string& filePath,
                           const audio::AudioGraph& graph,
                           const sequencer::StepSequencer& sequencer,
                           const std::string& title = "Untitled Project",
                           double bpm = 135.0,
                           double swing = 0.50);

    static bool loadFromFile(const std::string& filePath,
                             audio::AudioGraph& graph,
                             sequencer::StepSequencer& sequencer,
                             std::string& outTitle,
                             double& outBpm,
                             double& outSwing);

    static std::string serializeJson(const audio::AudioGraph& graph,
                                     const sequencer::StepSequencer& sequencer,
                                     const std::string& title,
                                     double bpm,
                                     double swing,
                                     int songKeyRoot = 0,
                                     bool isSongKeyMinor = false,
                                     const std::vector<theory::ChordEvent>& chordTrack = {});

    static bool deserializeJson(const std::string& jsonStr,
                                audio::AudioGraph& graph,
                                sequencer::StepSequencer& sequencer,
                                std::string& outTitle,
                                double& outBpm,
                                double& outSwing);

    static bool deserializeJson(const std::string& jsonStr,
                                audio::AudioGraph& graph,
                                sequencer::StepSequencer& sequencer,
                                std::string& outTitle,
                                double& outBpm,
                                double& outSwing,
                                int& outSongKeyRoot,
                                bool& outIsSongKeyMinor,
                                std::vector<theory::ChordEvent>& outChordTrack);
};

} // namespace eatsbits::project

#endif // EATS_PROJECT_FILE_HPP
