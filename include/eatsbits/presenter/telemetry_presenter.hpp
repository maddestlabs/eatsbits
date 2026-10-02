#ifndef EATS_TELEMETRY_PRESENTER_HPP
#define EATS_TELEMETRY_PRESENTER_HPP

#include "eatsbits/presenter/presenter_base.hpp"
#include "eatsbits/presenter/frame_time_context.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <string>

namespace eatsbits::audio {
    class AudioEngine;
}

namespace eatsbits::presenter {

/**
 * @brief Channel audio telemetry feedback (peaks, RMS, peak hold).
 */
struct ChannelMeterData {
    float peakL{0.0f};
    float peakR{0.0f};
    float rmsL{0.0f};
    float rmsR{0.0f};
    float peakHoldL{0.0f};
    float peakHoldR{0.0f};
};

/**
 * @brief Sequencer transport & system telemetry.
 */
struct TransportTelemetry {
    bool isPlaying{false};
    bool isRecording{false};
    bool isLooping{false};
    uint32_t currentStep{0};
    uint32_t bar{1};
    uint32_t beat{1};
    uint32_t sixteenth{1};
    double songTimeSeconds{0.0};
    double bpm{120.0};
    double cpuUsage{0.0};
};

/**
 * @brief Complete snapshot of audio telemetry for GUI widgets, TUI screens,
 * and offline non-realtime video exporters.
 */
struct TelemetrySnapshot {
    std::vector<float> scopeSamples{};

    // Master bus metrics (direct flat fields for backward compatibility with TUI views)
    float peakL{0.0f};
    float peakR{0.0f};
    float peakHoldL{0.0f};
    float peakHoldR{0.0f};
    float rmsL{0.0f};
    float rmsR{0.0f};

    std::array<float, 16> spectrumBands{};
    uint32_t currentStep{0};
    bool isPlaying{false};
    double bpm{120.0};
    double cpuUsage{0.0};

    // Structured channel & transport data
    ChannelMeterData masterMeter{};
    std::vector<ChannelMeterData> trackMeters{};
    TransportTelemetry transport{};
    bool isSettled{false};
};

/**
 * @brief Decoupled, lock-free TelemetryPresenter bridge.
 * Owns meter ballistics (attack, exponential decay, peak-hold timers),
 * 16-band FFT spectral decomposition, oscilloscope sampling, and transport metrics.
 *
 * Implements fine-grained dirty tracking: when audio is stopped and meters settle to silence,
 * dirty marking is suspended and isSettled() becomes true, enabling event-driven zero-CPU idle loops.
 */
class TelemetryPresenter : public PresenterBase {
public:
    explicit TelemetryPresenter(size_t scopeCapacity = 256, size_t maxTracks = 16);
    ~TelemetryPresenter() override = default;

    /**
     * @brief Polls AudioEngine queues, updates ballistics, advances timers,
     * computes spectrum, and updates dirty tracking.
     */
    void update(audio::AudioEngine& engine, float dt = 0.016666f);
    void update(audio::AudioEngine& engine, const FrameTimeContext& time) {
        update(engine, time.dt());
    }

    /**
     * @brief Directly feed audio metrics without AudioEngine (for offline rendering,
     * unit testing, or custom headless pipelines).
     */
    void feedCustomAudio(const float* samples, size_t count,
                         float peakL, float peakR,
                         float rmsL = 0.0f, float rmsR = 0.0f,
                         float dt = 0.016666f);
    void feedCustomAudio(const float* samples, size_t count,
                         float peakL, float peakR,
                         float rmsL, float rmsR,
                         const FrameTimeContext& time) {
        feedCustomAudio(samples, count, peakL, peakR, rmsL, rmsR, time.dt());
    }

    /**
     * @brief Updates transport state explicitly.
     */
    void setTransport(bool isPlaying, uint32_t currentStep, double bpm = 120.0,
                      double songTimeSeconds = 0.0, bool isRecording = false, bool isLooping = false);

    [[nodiscard]] const TelemetrySnapshot& getSnapshot() const noexcept { return snapshot_; }
    [[nodiscard]] const ChannelMeterData& getMasterMeter() const noexcept { return snapshot_.masterMeter; }
    [[nodiscard]] const std::vector<ChannelMeterData>& getTrackMeters() const noexcept { return snapshot_.trackMeters; }
    [[nodiscard]] const std::vector<float>& getScopeSamples() const noexcept { return snapshot_.scopeSamples; }
    [[nodiscard]] const std::array<float, 16>& getSpectrumBands() const noexcept { return snapshot_.spectrumBands; }
    [[nodiscard]] const TransportTelemetry& getTransport() const noexcept { return snapshot_.transport; }

    [[nodiscard]] bool isSettled() const noexcept { return isSettled_; }

    void setReleaseCoeff(float coeff) noexcept { releaseCoeff_ = std::clamp(coeff, 0.50f, 0.999f); }
    [[nodiscard]] float getReleaseCoeff() const noexcept { return releaseCoeff_; }

    void setPeakHoldDuration(float durationSec) noexcept { peakHoldDuration_ = std::max(0.0f, durationSec); }
    [[nodiscard]] float getPeakHoldDuration() const noexcept { return peakHoldDuration_; }

    void setSilenceThreshold(float thresh) noexcept { silenceThreshold_ = std::max(0.0f, thresh); }
    [[nodiscard]] float getSilenceThreshold() const noexcept { return silenceThreshold_; }

    void reset() noexcept;

private:
    size_t scopeCapacity_{256};
    std::vector<float> rawScopeBuf_;
    TelemetrySnapshot snapshot_;

    float releaseCoeff_{0.91f};
    float peakHoldDuration_{1.0f};
    float silenceThreshold_{0.0005f}; // ~ -66 dB
    float masterPeakHoldTimerL_{0.0f};
    float masterPeakHoldTimerR_{0.0f};

    std::vector<float> trackPeakHoldTimersL_;
    std::vector<float> trackPeakHoldTimersR_;

    bool isSettled_{false};

    void computeSpectrumBands();
    void applyBallistics(ChannelMeterData& meter, float inPeakL, float inPeakR,
                         float inRmsL, float inRmsR, float& timerL, float& timerR, float dt);
    void syncFlatFields();
    void evaluateSettledState();
};

} // namespace eatsbits::presenter

#endif // EATS_TELEMETRY_PRESENTER_HPP
