#include "eatsbits/tui/audio_telemetry.hpp"

namespace eatsbits::tui {

AudioTelemetryBridge::AudioTelemetryBridge(size_t scopeCapacity)
    : scopeCapacity_(scopeCapacity), rawScopeBuf_(scopeCapacity, 0.0f) {
    snapshot_.scopeSamples.assign(scopeCapacity, 0.0f);
    snapshot_.spectrumBands.fill(0.0f);
}

void AudioTelemetryBridge::update(audio::AudioEngine& engine) {
    // 1. Fetch Oscilloscope PCM samples
    size_t count = engine.getScopeSamples(rawScopeBuf_.data(), scopeCapacity_);
    if (count > 0) {
        snapshot_.scopeSamples.assign(rawScopeBuf_.begin(), rawScopeBuf_.begin() + count);
    }

    // 2. Fetch Meter Feedback
    MeterFeedback fb{};
    if (engine.pollMeterFeedback(fb)) {
        snapshot_.peakL = fb.peakLeft;
        snapshot_.peakR = fb.peakRight;

        // Peak hold decay
        if (fb.peakLeft > snapshot_.peakHoldL) {
            snapshot_.peakHoldL = fb.peakLeft;
            peakHoldTimerL_ = 1.0f;
        } else {
            peakHoldTimerL_ -= 0.05f;
            if (peakHoldTimerL_ <= 0.0f) {
                snapshot_.peakHoldL = std::max(0.0f, snapshot_.peakHoldL - 0.03f);
            }
        }

        if (fb.peakRight > snapshot_.peakHoldR) {
            snapshot_.peakHoldR = fb.peakRight;
            peakHoldTimerR_ = 1.0f;
        } else {
            peakHoldTimerR_ -= 0.05f;
            if (peakHoldTimerR_ <= 0.0f) {
                snapshot_.peakHoldR = std::max(0.0f, snapshot_.peakHoldR - 0.03f);
            }
        }
    }

    // 3. Sequencer info
    const auto& seq = engine.getSequencer();
    snapshot_.isPlaying = seq.isPlaying();
    snapshot_.currentStep = seq.getTransport().getCurrentStep();
    snapshot_.bpm = seq.getBpm();

    // 4. Compute 16-band spectrum energy
    computeSpectrumBands();
}

void AudioTelemetryBridge::computeSpectrumBands() {
    if (snapshot_.scopeSamples.empty()) return;

    // Simple, efficient Discrete Fourier approximation across 16 log-spaced musical frequency bins
    const size_t N = snapshot_.scopeSamples.size();
    static const double kCenterFreqs[16] = {
        40.0, 65.0, 100.0, 160.0, 250.0, 400.0, 630.0, 1000.0,
        1600.0, 2500.0, 4000.0, 6300.0, 8000.0, 10000.0, 12500.0, 16000.0
    };
    const double sampleRate = 44100.0;

    for (size_t bin = 0; bin < 16; ++bin) {
        double freq = kCenterFreqs[bin];
        double omega = 2.0 * 3.141592653589793 * freq / sampleRate;
        double real = 0.0;
        double imag = 0.0;

        for (size_t n = 0; n < N; ++n) {
            // Hann windowing
            double w = 0.5 * (1.0 - std::cos(2.0 * 3.141592653589793 * n / (N - 1)));
            double sample = snapshot_.scopeSamples[n] * w;
            real += sample * std::cos(omega * n);
            imag -= sample * std::sin(omega * n);
        }

        double mag = std::sqrt(real * real + imag * imag) / static_cast<double>(N);
        float normalized = std::clamp(static_cast<float>(mag * 5.0), 0.0f, 1.0f);

        // Smooth decaying
        snapshot_.spectrumBands[bin] = std::max(normalized, snapshot_.spectrumBands[bin] * 0.75f);
    }
}

} // namespace eatsbits::tui
