#pragma once

#include "eatsbits/audio/audio_engine.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace eatsbits::tui {

struct TelemetrySnapshot {
    std::vector<float> scopeSamples;
    float peakL{0.0f};
    float peakR{0.0f};
    float peakHoldL{0.0f};
    float peakHoldR{0.0f};
    std::array<float, 16> spectrumBands{};
    uint32_t currentStep{0};
    bool isPlaying{false};
    double bpm{120.0};
    double cpuUsage{0.0};
};

class AudioTelemetryBridge {
public:
    AudioTelemetryBridge(size_t scopeCapacity = 256);
    ~AudioTelemetryBridge() = default;

    void update(audio::AudioEngine& engine);
    const TelemetrySnapshot& getSnapshot() const { return snapshot_; }

private:
    size_t scopeCapacity_{256};
    std::vector<float> rawScopeBuf_;
    TelemetrySnapshot snapshot_;

    float peakHoldTimerL_{0.0f};
    float peakHoldTimerR_{0.0f};

    void computeSpectrumBands();
};

} // namespace eatsbits::tui
