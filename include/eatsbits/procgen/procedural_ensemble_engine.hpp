#ifndef EATS_PROCEDURAL_ENSEMBLE_ENGINE_HPP
#define EATS_PROCEDURAL_ENSEMBLE_ENGINE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "song_archetypes.hpp"
#include "procedural_acid_engine.hpp"
#include "procedural_drum_engine.hpp"
#include "procedural_piano_engine.hpp"
#include "../ui/views/arranger_view.hpp"
#include "../sequencer/step_sequencer.hpp"

namespace eatsbits::procgen {

/// Generated multi-track output containing populated timeline tracks and clips.
struct EnsembleRenderResult {
    bool success{true};
    std::string message;
    std::vector<ui::ArrangerTimelineTrack> tracks;
    std::vector<theory::ChordEvent> chordTrack;
    double bpm{120.0};
    std::string songKey{"C Minor"};
    uint32_t totalBars{16};
    uint32_t totalNotes{0};
};

/// Parameters guiding multi-track ensemble composition.
struct EnsembleRenderParams {
    uint32_t seed{42};
    double swing{0.15};
    double humanize{0.20};
    bool generateChordTrack{true};
};

/**
 * Procedural Ensemble Engine.
 * Multi-track orchestration engine coordinating Drums, Bassline, Harmonic Textures,
 * Primary Melody, and Counterpoint simultaneously across dynamic section plans.
 * Guarantees zero harmonic clashes and rhythm cohesion.
 */
class ProceduralEnsembleEngine {
public:
    /// Renders a complete SongStructureBlueprint into timeline tracks and clips.
    [[nodiscard]] static EnsembleRenderResult renderBlueprint(
        const SongStructureBlueprint& blueprint,
        const EnsembleRenderParams& params
    );

    /// Renders a SongArchetype by archetype ID.
    [[nodiscard]] static EnsembleRenderResult renderArchetype(
        const std::string& archetypeId,
        const EnsembleRenderParams& params
    );
};

} // namespace eatsbits::procgen

#endif // EATS_PROCEDURAL_ENSEMBLE_ENGINE_HPP
