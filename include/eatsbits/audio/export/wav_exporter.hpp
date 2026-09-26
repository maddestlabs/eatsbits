#ifndef EATS_WAV_EXPORTER_HPP
#define EATS_WAV_EXPORTER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "../../audio/graph/audio_graph.hpp"
#include "../../sequencer/step_sequencer.hpp"

namespace eatsbits::audio::exporting {

enum class WavFormat {
    Pcm16,
    Pcm24,
    Float32
};

struct BounceConfig {
    uint32_t sampleRate{48000};
    uint32_t numChannels{2};
    WavFormat format{WavFormat::Pcm24};
    bool enableDither{true};
    double durationSeconds{8.0};
};

struct BounceStats {
    double durationSeconds{0.0};
    double renderTimeMs{0.0};
    double speedMultiplier{0.0};
    float peakLeft{0.0f};
    float peakRight{0.0f};
    uint64_t totalFrames{0};
    uint64_t fileSizeBytes{0};
    std::string outputPath{""};
};

/**
 * High-Speed Studio Audio Bouncing & Stem Exporter Engine.
 * Renders offline projects at maximum CPU throughput to 16/24/32-bit WAV
 * with TPDF dithering and multi-track stem isolation.
 */
class WavExporter {
public:
    // Low-level buffer to WAV file writer
    static bool writeWavFile(const std::string& filePath,
                             const float* const* channelData,
                             uint32_t numChannels,
                             uint32_t numFrames,
                             uint32_t sampleRate,
                             WavFormat format,
                             bool enableDither = true);

    // Full project master bounce
    static BounceStats bounceMaster(audio::AudioGraph& graph,
                                   sequencer::StepSequencer& seq,
                                   const BounceConfig& config,
                                   const std::string& outputPath);

    // Isolated stems bounce (one WAV file per sequencer track + master)
    static std::vector<BounceStats> bounceStems(audio::AudioGraph& graph,
                                               sequencer::StepSequencer& seq,
                                               const BounceConfig& config,
                                               const std::string& outputDir);

private:
    // Fast high-quality TPDF (Triangular Probability Density Function) dither
    static float generateTpdfDither(uint32_t& rngState) noexcept;
};

} // namespace eatsbits::audio::exporting

#endif // EATS_WAV_EXPORTER_HPP
