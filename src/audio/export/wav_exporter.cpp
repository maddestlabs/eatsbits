#include "eatsbits/audio/export/wav_exporter.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace eatsbits::audio::exporting {

namespace {

inline uint32_t xorshift32(uint32_t& state) noexcept {
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

inline void writeUint32LE(std::ofstream& out, uint32_t value) {
    char bytes[4];
    bytes[0] = static_cast<char>(value & 0xFF);
    bytes[1] = static_cast<char>((value >> 8) & 0xFF);
    bytes[2] = static_cast<char>((value >> 16) & 0xFF);
    bytes[3] = static_cast<char>((value >> 24) & 0xFF);
    out.write(bytes, 4);
}

inline void writeUint16LE(std::ofstream& out, uint16_t value) {
    char bytes[2];
    bytes[0] = static_cast<char>(value & 0xFF);
    bytes[1] = static_cast<char>((value >> 8) & 0xFF);
    out.write(bytes, 2);
}

} // namespace

float WavExporter::generateTpdfDither(uint32_t& rngState) noexcept {
    // Generate two independent uniform random variables in [-1, 1]
    const float r1 = (static_cast<float>(xorshift32(rngState)) / 2147483648.0f) - 1.0f;
    const float r2 = (static_cast<float>(xorshift32(rngState)) / 2147483648.0f) - 1.0f;
    // Triangular distribution in [-1, 1]
    return 0.5f * (r1 - r2);
}

bool WavExporter::writeWavFile(const std::string& filePath,
                             const float* const* channelData,
                             uint32_t numChannels,
                             uint32_t numFrames,
                             uint32_t sampleRate,
                             WavFormat format,
                             bool enableDither) {
    if (!channelData || numChannels == 0 || numFrames == 0) return false;

    // Create parent directories if needed
    try {
        std::filesystem::path p(filePath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {}

    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) return false;

    uint16_t bitsPerSample = 16;
    uint16_t audioFormat = 1; // PCM

    if (format == WavFormat::Pcm16) {
        bitsPerSample = 16;
        audioFormat = 1;
    } else if (format == WavFormat::Pcm24) {
        bitsPerSample = 24;
        audioFormat = 1;
    } else if (format == WavFormat::Float32) {
        bitsPerSample = 32;
        audioFormat = 3; // IEEE Float
    }

    const uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    const uint32_t byteRate = sampleRate * blockAlign;
    const uint32_t subchunk2Size = numFrames * blockAlign;
    const uint32_t chunkSize = 36 + subchunk2Size;

    // 1. RIFF Chunk Descriptor
    out.write("RIFF", 4);
    writeUint32LE(out, chunkSize);
    out.write("WAVE", 4);

    // 2. fmt Sub-chunk
    out.write("fmt ", 4);
    writeUint32LE(out, 16); // Subchunk1Size for PCM/Float
    writeUint16LE(out, audioFormat);
    writeUint16LE(out, static_cast<uint16_t>(numChannels));
    writeUint32LE(out, sampleRate);
    writeUint32LE(out, byteRate);
    writeUint16LE(out, blockAlign);
    writeUint16LE(out, bitsPerSample);

    // 3. data Sub-chunk
    out.write("data", 4);
    writeUint32LE(out, subchunk2Size);

    // Sample payload writing
    uint32_t ditherRng = 0x12345678;

    if (format == WavFormat::Float32) {
        std::vector<float> interleaved(numChannels);
        for (uint32_t f = 0; f < numFrames; ++f) {
            for (uint32_t ch = 0; ch < numChannels; ++ch) {
                interleaved[ch] = channelData[ch][f];
            }
            out.write(reinterpret_cast<const char*>(interleaved.data()), numChannels * sizeof(float));
        }
    } else if (format == WavFormat::Pcm16) {
        std::vector<int16_t> interleaved(numChannels);
        const float lsb = 1.0f / 32768.0f;

        for (uint32_t f = 0; f < numFrames; ++f) {
            for (uint32_t ch = 0; ch < numChannels; ++ch) {
                float sample = channelData[ch][f];
                if (enableDither) {
                    sample += generateTpdfDither(ditherRng) * lsb;
                }
                int32_t quantized = static_cast<int32_t>(std::round(sample * 32767.0f));
                interleaved[ch] = static_cast<int16_t>(std::clamp(quantized, -32768, 32767));
            }
            out.write(reinterpret_cast<const char*>(interleaved.data()), numChannels * sizeof(int16_t));
        }
    } else if (format == WavFormat::Pcm24) {
        std::vector<uint8_t> frameBytes(numChannels * 3);
        const float lsb = 1.0f / 8388608.0f;

        for (uint32_t f = 0; f < numFrames; ++f) {
            for (uint32_t ch = 0; ch < numChannels; ++ch) {
                float sample = channelData[ch][f];
                if (enableDither) {
                    sample += generateTpdfDither(ditherRng) * lsb;
                }
                int32_t quantized = static_cast<int32_t>(std::round(sample * 8388607.0f));
                quantized = std::clamp(quantized, -8388608, 8388607);

                const size_t byteOffset = ch * 3;
                frameBytes[byteOffset + 0] = static_cast<uint8_t>(quantized & 0xFF);
                frameBytes[byteOffset + 1] = static_cast<uint8_t>((quantized >> 8) & 0xFF);
                frameBytes[byteOffset + 2] = static_cast<uint8_t>((quantized >> 16) & 0xFF);
            }
            out.write(reinterpret_cast<const char*>(frameBytes.data()), frameBytes.size());
        }
    }

    return out.good();
}

BounceStats WavExporter::bounceMaster(audio::AudioGraph& graph,
                                     sequencer::StepSequencer& seq,
                                     const BounceConfig& config,
                                     const std::string& outputPath) {
    BounceStats stats{};
    stats.outputPath = outputPath;
    stats.durationSeconds = config.durationSeconds;

    const uint32_t totalFrames = static_cast<uint32_t>(config.sampleRate * config.durationSeconds);
    stats.totalFrames = totalFrames;

    // Allocate continuous render buffers
    std::vector<float> masterL(totalFrames, 0.0f);
    std::vector<float> masterR(totalFrames, 0.0f);
    const float* channelPtrs[2] = {masterL.data(), masterR.data()};

    // Prepare graph & transport
    graph.prepare(config.sampleRate, 512);
    seq.getTransport().setSampleRate(config.sampleRate);
    seq.stop();
    seq.start();

    constexpr uint32_t CHUNK_SIZE = 512;
    alignas(64) float chunkL[CHUNK_SIZE];
    alignas(64) float chunkR[CHUNK_SIZE];

    const auto startTime = std::chrono::high_resolution_clock::now();

    uint32_t framesRendered = 0;
    float peakL = 0.0f;
    float peakR = 0.0f;

    while (framesRendered < totalFrames) {
        const uint32_t curFrames = std::min(CHUNK_SIZE, totalFrames - framesRendered);

        // Sequence and evaluate graph
        seq.processBlock(curFrames, graph);
        graph.process(chunkL, chunkR, curFrames);

        for (uint32_t i = 0; i < curFrames; ++i) {
            const float sL = chunkL[i];
            const float sR = chunkR[i];
            masterL[framesRendered + i] = sL;
            masterR[framesRendered + i] = sR;

            peakL = std::max(peakL, std::abs(sL));
            peakR = std::max(peakR, std::abs(sR));
        }

        framesRendered += curFrames;
    }

    seq.stop();

    const auto endTime = std::chrono::high_resolution_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    stats.renderTimeMs = elapsedMs;
    stats.speedMultiplier = (config.durationSeconds * 1000.0) / std::max(0.001, elapsedMs);
    stats.peakLeft = peakL;
    stats.peakRight = peakR;

    // Write WAV output
    bool written = writeWavFile(outputPath, channelPtrs, config.numChannels, totalFrames, config.sampleRate, config.format, config.enableDither);
    if (written) {
        try {
            stats.fileSizeBytes = std::filesystem::file_size(outputPath);
        } catch (...) {
            stats.fileSizeBytes = 0;
        }
    }

    return stats;
}

std::vector<BounceStats> WavExporter::bounceStems(audio::AudioGraph& graph,
                                                 sequencer::StepSequencer& seq,
                                                 const BounceConfig& config,
                                                 const std::string& outputDir) {
    std::vector<BounceStats> stemResults;

    try {
        std::filesystem::create_directories(outputDir);
    } catch (...) {}

    // 1. Render Master mix first
    std::string masterPath = outputDir + "/master.wav";
    stemResults.push_back(bounceMaster(graph, seq, config, masterPath));

    // 2. Render each isolated track stem
    const size_t numTracks = seq.getNumTracks();
    for (size_t t = 0; t < numTracks; ++t) {
        auto* targetTrack = seq.getTrack(t);
        if (!targetTrack) continue;

        std::string trackName = targetTrack->getName();
        // Sanitize track name for filename
        std::replace(trackName.begin(), trackName.end(), ' ', '_');
        std::replace(trackName.begin(), trackName.end(), '/', '_');
        std::replace(trackName.begin(), trackName.end(), '\\', '_');

        // Mute all other tracks
        std::vector<bool> origMuteStates(numTracks);
        for (size_t o = 0; o < numTracks; ++o) {
            auto* tr = seq.getTrack(o);
            origMuteStates[o] = tr->isMuted();
            tr->setMuted(o != t);
        }

        std::string stemPath = outputDir + "/stem_" + std::to_string(t + 1) + "_" + trackName + ".wav";
        BounceStats stemStats = bounceMaster(graph, seq, config, stemPath);
        stemResults.push_back(stemStats);

        // Restore mute states
        for (size_t o = 0; o < numTracks; ++o) {
            seq.getTrack(o)->setMuted(origMuteStates[o]);
        }
    }

    return stemResults;
}

} // namespace eatsbits::audio::exporting
