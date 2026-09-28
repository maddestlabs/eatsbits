#ifndef EATS_AI_MIXING_ENGINE_HPP
#define EATS_AI_MIXING_ENGINE_HPP

#include <string>
#include <vector>
#include <map>
#include "../sequencer/step_sequencer.hpp"
#include "../project/json_parser.hpp"
#include "gemini_client.hpp"

namespace eatsbits::ai {

struct TrackChannelTelemetry {
    std::string trackId{""};
    std::string name{""};
    float volume{0.85f};
    float pan{0.0f};
    size_t activeNotes{0};
    bool isDrumTrack{false};
    bool isBassTrack{false};
    bool isLeadTrack{false};
    bool isPadTrack{false};
};

struct MixTelemetry {
    std::string genre{"Synthwave"};
    double bpm{124.0};
    float targetLufs{-14.0f};
    std::vector<TrackChannelTelemetry> tracks;
};

struct TrackEqAdjustment {
    bool enabled{true};
    float hpfHz{30.0f};
    float lowGainDb{0.0f};
    float midFreqHz{1000.0f};
    float midGainDb{0.0f};
    float midQ{1.0f};
    float highGainDb{0.0f};
};

struct TrackMixAdjustment {
    std::string trackId{""};
    float volume{0.85f};
    float pan{0.0f};
    TrackEqAdjustment eq;
};

struct MasterMixAdjustment {
    float subCutHz{28.0f};
    float lowGainDb{0.0f};
    float midFreqHz{2500.0f};
    float midGainDb{-0.5f};
    float highGainDb{1.0f};
    bool limiterEnabled{true};
    float ceilingDbfs{-0.3f};
    float limiterDriveDb{2.5f};
    float targetLufs{-14.0f};
};

struct AiMixResult {
    bool success{false};
    std::string summary{""};
    int tracksAdjusted{0};
    std::string rawPatchJson{""};
    std::string errorMessage{""};
    std::vector<TrackMixAdjustment> trackAdjustments;
    MasterMixAdjustment masterAdjustment;
};

/**
 * Intelligent DAW Auto-Mixing and Mastering Engine.
 * Formulates acoustic telemetry, queries Gemini API, applies algorithmic spectral unmasking,
 * and executes gain-staging on StepSequencer tracks.
 */
class AiMixingEngine {
public:
    static MixTelemetry extractTelemetry(
        const sequencer::StepSequencer& seq,
        const std::string& genre = "Synthwave",
        float targetLufs = -14.0f
    );

    /// Generates algorithmic gain staging and frequency unmasking without network dependency
    static json::Value computeOfflineMixPatch(const MixTelemetry& telemetry);

    /// Applies mix patch adjustments directly to sequencer tracks
    static int applyMixPatch(sequencer::StepSequencer& seq, const json::Value& patchRoot);

    /// Orchestrates end-to-end Auto-Mix & Master pass (using GeminiClient or offline fallback)
    static AiMixResult runAutoMixMaster(
        sequencer::StepSequencer& seq,
        GeminiClient& client,
        const std::string& genre = "Synthwave",
        float targetLufs = -14.0f,
        const std::string& customInstructions = ""
    );
};

} // namespace eatsbits::ai

#endif // EATS_AI_MIXING_ENGINE_HPP
