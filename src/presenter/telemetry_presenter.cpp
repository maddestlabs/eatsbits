#include "eatsbits/presenter/telemetry_presenter.hpp"
#include "eatsbits/audio/audio_engine.hpp"

namespace eatsbits::presenter {

TelemetryPresenter::TelemetryPresenter(size_t scopeCapacity, size_t maxTracks)
    : scopeCapacity_(scopeCapacity), rawScopeBuf_(scopeCapacity, 0.0f) {
    snapshot_.scopeSamples.assign(scopeCapacity, 0.0f);
    snapshot_.spectrumBands.fill(0.0f);
    snapshot_.trackMeters.resize(maxTracks, ChannelMeterData{});
    trackPeakHoldTimersL_.resize(maxTracks, 0.0f);
    trackPeakHoldTimersR_.resize(maxTracks, 0.0f);
    syncFlatFields();
}

void TelemetryPresenter::update(audio::AudioEngine& engine, float dt) {
    if (dt <= 0.0001f || dt > 0.2f) {
        dt = 0.016666f;
    }

    // 1. Fetch Oscilloscope PCM samples
    size_t count = engine.getScopeSamples(rawScopeBuf_.data(), scopeCapacity_);
    if (count > 0) {
        snapshot_.scopeSamples.assign(rawScopeBuf_.begin(), rawScopeBuf_.begin() + count);
    } else if (snapshot_.transport.isPlaying == false) {
        // Decay scope samples towards zero when stopped
        for (float& s : snapshot_.scopeSamples) {
            s *= 0.85f;
            if (std::abs(s) < silenceThreshold_) s = 0.0f;
        }
    }

    // 2. Fetch Master Meter Feedback
    MeterFeedback fb{};
    bool gotFb = engine.pollMeterFeedback(fb);
    applyBallistics(snapshot_.masterMeter,
                    gotFb ? fb.peakLeft : 0.0f, gotFb ? fb.peakRight : 0.0f,
                    gotFb ? fb.rmsLeft : 0.0f, gotFb ? fb.rmsRight : 0.0f,
                    masterPeakHoldTimerL_, masterPeakHoldTimerR_, dt);

    // 3. Fetch Per-Track Feedback
    for (size_t i = 0; i < snapshot_.trackMeters.size(); ++i) {
        MeterFeedback trFb{};
        bool gotTr = engine.getTrackMeterFeedback(static_cast<uint32_t>(i), trFb);
        applyBallistics(snapshot_.trackMeters[i],
                        gotTr ? trFb.peakLeft : 0.0f, gotTr ? trFb.peakRight : 0.0f,
                        gotTr ? trFb.rmsLeft : 0.0f, gotTr ? trFb.rmsRight : 0.0f,
                        trackPeakHoldTimersL_[i], trackPeakHoldTimersR_[i], dt);
    }

    // 4. Transport & Sequencer synchronization
    const auto& seq = engine.getSequencer();
    const auto& trans = seq.getTransport();
    snapshot_.transport.isPlaying = seq.isPlaying();
    snapshot_.transport.currentStep = trans.getCurrentStep();
    snapshot_.transport.bpm = seq.getBpm();
    uint32_t sr = trans.getSampleRate();
    if (sr == 0) sr = 48000;
    snapshot_.transport.songTimeSeconds = static_cast<double>(trans.getTotalSamplesElapsed()) / static_cast<double>(sr);
    snapshot_.transport.bar = (snapshot_.transport.currentStep / 16) + 1;
    snapshot_.transport.beat = ((snapshot_.transport.currentStep % 16) / 4) + 1;
    snapshot_.transport.sixteenth = (snapshot_.transport.currentStep % 4) + 1;

    // 5. Compute 16-band log spectrum energy
    computeSpectrumBands();

    // 6. Synchronize flat fields
    syncFlatFields();

    // 7. Evaluate fine-grained dirty tracking and settled status
    evaluateSettledState();
}

void TelemetryPresenter::feedCustomAudio(const float* samples, size_t count,
                                         float peakL, float peakR,
                                         float rmsL, float rmsR,
                                         float dt) {
    if (dt <= 0.0001f || dt > 0.2f) dt = 0.016666f;

    if (samples && count > 0) {
        size_t n = std::min(count, scopeCapacity_);
        snapshot_.scopeSamples.assign(samples, samples + n);
    }

    applyBallistics(snapshot_.masterMeter, peakL, peakR, rmsL, rmsR,
                    masterPeakHoldTimerL_, masterPeakHoldTimerR_, dt);

    computeSpectrumBands();
    syncFlatFields();
    evaluateSettledState();
}

void TelemetryPresenter::setTransport(bool isPlaying, uint32_t currentStep, double bpm,
                                      double songTimeSeconds, bool isRecording, bool isLooping) {
    snapshot_.transport.isPlaying = isPlaying;
    snapshot_.transport.currentStep = currentStep;
    snapshot_.transport.bpm = bpm;
    snapshot_.transport.songTimeSeconds = songTimeSeconds;
    snapshot_.transport.isRecording = isRecording;
    snapshot_.transport.isLooping = isLooping;
    snapshot_.transport.bar = (currentStep / 16) + 1;
    snapshot_.transport.beat = ((currentStep % 16) / 4) + 1;
    snapshot_.transport.sixteenth = (currentStep % 4) + 1;
    syncFlatFields();
    evaluateSettledState();
}

void TelemetryPresenter::reset() noexcept {
    snapshot_.scopeSamples.assign(scopeCapacity_, 0.0f);
    snapshot_.spectrumBands.fill(0.0f);
    snapshot_.masterMeter = ChannelMeterData{};
    for (auto& tr : snapshot_.trackMeters) {
        tr = ChannelMeterData{};
    }
    masterPeakHoldTimerL_ = 0.0f;
    masterPeakHoldTimerR_ = 0.0f;
    std::fill(trackPeakHoldTimersL_.begin(), trackPeakHoldTimersL_.end(), 0.0f);
    std::fill(trackPeakHoldTimersR_.begin(), trackPeakHoldTimersR_.end(), 0.0f);
    snapshot_.transport = TransportTelemetry{};
    isSettled_ = true;
    snapshot_.isSettled = true;
    syncFlatFields();
    clearDirty();
}

void TelemetryPresenter::applyBallistics(ChannelMeterData& meter, float inPeakL, float inPeakR,
                                         float inRmsL, float inRmsR,
                                         float& timerL, float& timerR, float dt) {
    float decayFactor = std::pow(releaseCoeff_, dt / 0.016666f);

    // Left Peak
    if (inPeakL > meter.peakL) {
        meter.peakL = inPeakL; // Instant attack
    } else {
        meter.peakL *= decayFactor;
    }
    if (meter.peakL < silenceThreshold_) meter.peakL = 0.0f;

    // Left Peak Hold
    if (inPeakL >= meter.peakHoldL) {
        meter.peakHoldL = inPeakL;
        timerL = peakHoldDuration_;
    } else {
        timerL -= dt;
        if (timerL <= 0.0f) {
            meter.peakHoldL = std::max(0.0f, meter.peakHoldL - 1.8f * dt);
        }
    }
    if (meter.peakHoldL < silenceThreshold_) meter.peakHoldL = 0.0f;

    // Right Peak
    if (inPeakR > meter.peakR) {
        meter.peakR = inPeakR;
    } else {
        meter.peakR *= decayFactor;
    }
    if (meter.peakR < silenceThreshold_) meter.peakR = 0.0f;

    // Right Peak Hold
    if (inPeakR >= meter.peakHoldR) {
        meter.peakHoldR = inPeakR;
        timerR = peakHoldDuration_;
    } else {
        timerR -= dt;
        if (timerR <= 0.0f) {
            meter.peakHoldR = std::max(0.0f, meter.peakHoldR - 1.8f * dt);
        }
    }
    if (meter.peakHoldR < silenceThreshold_) meter.peakHoldR = 0.0f;

    // RMS Smoothing
    if (inRmsL > 0.0f) {
        meter.rmsL = inRmsL;
    } else {
        meter.rmsL *= decayFactor;
    }
    if (meter.rmsL < silenceThreshold_) meter.rmsL = 0.0f;

    if (inRmsR > 0.0f) {
        meter.rmsR = inRmsR;
    } else {
        meter.rmsR *= decayFactor;
    }
    if (meter.rmsR < silenceThreshold_) meter.rmsR = 0.0f;
}

void TelemetryPresenter::computeSpectrumBands() {
    if (snapshot_.scopeSamples.empty()) return;

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
        if (snapshot_.spectrumBands[bin] < silenceThreshold_) {
            snapshot_.spectrumBands[bin] = 0.0f;
        }
    }
}

void TelemetryPresenter::syncFlatFields() {
    snapshot_.peakL = snapshot_.masterMeter.peakL;
    snapshot_.peakR = snapshot_.masterMeter.peakR;
    snapshot_.peakHoldL = snapshot_.masterMeter.peakHoldL;
    snapshot_.peakHoldR = snapshot_.masterMeter.peakHoldR;
    snapshot_.rmsL = snapshot_.masterMeter.rmsL;
    snapshot_.rmsR = snapshot_.masterMeter.rmsR;

    snapshot_.currentStep = snapshot_.transport.currentStep;
    snapshot_.isPlaying = snapshot_.transport.isPlaying;
    snapshot_.bpm = snapshot_.transport.bpm;
    snapshot_.cpuUsage = snapshot_.transport.cpuUsage;
}

void TelemetryPresenter::evaluateSettledState() {
    bool isSilent = (snapshot_.masterMeter.peakL == 0.0f && snapshot_.masterMeter.peakR == 0.0f &&
                     snapshot_.masterMeter.peakHoldL == 0.0f && snapshot_.masterMeter.peakHoldR == 0.0f);
    if (isSilent) {
        for (const auto& tr : snapshot_.trackMeters) {
            if (tr.peakL > 0.0f || tr.peakR > 0.0f || tr.peakHoldL > 0.0f || tr.peakHoldR > 0.0f) {
                isSilent = false;
                break;
            }
        }
    }

    if (isSilent) {
        for (float sb : snapshot_.spectrumBands) {
            if (sb > 0.0f) {
                isSilent = false;
                break;
            }
        }
    }

    bool currentlySettled = (!snapshot_.transport.isPlaying && isSilent);
    snapshot_.isSettled = currentlySettled;

    if (currentlySettled) {
        if (!isSettled_) {
            // First frame transitioning to settled: notify listeners so UI displays clean zero
            isSettled_ = true;
            markDirty();
            clearDirty();
        }
        // While settled, keep isDirty false
    } else {
        isSettled_ = false;
        markDirty();
    }
}

} // namespace eatsbits::presenter
