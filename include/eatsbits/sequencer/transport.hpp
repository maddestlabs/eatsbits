#ifndef EATS_TRANSPORT_HPP
#define EATS_TRANSPORT_HPP

#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>

namespace eatsbits::sequencer {

enum class PlayState {
    Stopped,
    Playing,
    Paused
};

struct StepTick {
    uint32_t stepIndex{0};           // Step index (e.g. 0 to 63)
    uint32_t frameOffset{0};         // Exact frame index within the current audio block (0 <= offset < numFrames)
    double stepDurationSamples{0.0}; // Expected length of this step in samples (taking swing into account)
};

/**
 * Sample-accurate transport clock with fractional sample tracking and groove swing.
 * Real-time safe (zero dynamic allocations).
 */
class Transport {
public:
    explicit Transport(uint32_t sampleRate = 48000) noexcept
        : sampleRate_(sampleRate) {
        updateStepLengths();
    }

    void setSampleRate(uint32_t sampleRate) noexcept {
        if (sampleRate > 0) {
            sampleRate_ = sampleRate;
            updateStepLengths();
        }
    }

    [[nodiscard]] uint32_t getSampleRate() const noexcept { return sampleRate_; }

    void setBpm(double bpm) noexcept {
        bpm_ = std::clamp(bpm, 20.0, 300.0);
        updateStepLengths();
    }

    [[nodiscard]] double getBpm() const noexcept { return bpm_; }

    /**
     * Swing factor:
     * 0.50 = Straight (no swing)
     * 0.60 = Light groove
     * 0.667 = Triplet / Shuffle
     * Range: [0.50, 0.80]
     */
    void setSwing(double swing) noexcept {
        swing_ = std::clamp(swing, 0.50, 0.80);
        updateStepLengths();
    }

    [[nodiscard]] double getSwing() const noexcept { return swing_; }

    void start() noexcept {
        state_ = PlayState::Playing;
        // If starting fresh from 0, set up to trigger step 0 on the first frame
        stepSampleCounter_ = 0.0;
        triggeredFirstStep_ = false;
    }

    void stop() noexcept {
        state_ = PlayState::Stopped;
        currentStep_ = 0;
        stepSampleCounter_ = 0.0;
        totalSamplesElapsed_ = 0;
        triggeredFirstStep_ = false;
    }

    void pause() noexcept {
        state_ = PlayState::Paused;
    }

    void setPosition(uint32_t stepIndex) noexcept {
        currentStep_ = stepIndex % maxSteps_;
        stepSampleCounter_ = 0.0;
        triggeredFirstStep_ = false;
    }

    [[nodiscard]] PlayState getState() const noexcept { return state_; }
    [[nodiscard]] bool isPlaying() const noexcept { return state_ == PlayState::Playing; }
    [[nodiscard]] uint32_t getCurrentStep() const noexcept { return currentStep_; }
    [[nodiscard]] uint64_t getTotalSamplesElapsed() const noexcept { return totalSamplesElapsed_; }

    void setMaxSteps(uint32_t maxSteps) noexcept {
        if (maxSteps > 0) {
            maxSteps_ = maxSteps;
        }
    }
    [[nodiscard]] uint32_t getMaxSteps() const noexcept { return maxSteps_; }

    /**
     * Advances the clock by numFrames, reporting any step boundaries crossed.
     * Guaranteed zero heap allocations.
     * Returns the number of step ticks filled into outTicks (up to maxTicks).
     */
    template <size_t N>
    size_t advance(uint32_t numFrames, std::array<StepTick, N>& outTicks) noexcept {
        if (state_ != PlayState::Playing || numFrames == 0) {
            return 0;
        }

        size_t tickCount = 0;
        uint32_t framesProcessed = 0;

        // If just started, trigger current step immediately at frame 0
        if (!triggeredFirstStep_) {
            triggeredFirstStep_ = true;
            if (tickCount < N) {
                outTicks[tickCount] = StepTick{
                    currentStep_,
                    0,
                    getStepLength(currentStep_)
                };
                tickCount++;
            }
        }

        while (framesProcessed < numFrames) {
            double currentStepLength = getStepLength(currentStep_);
            double remainingInStep = currentStepLength - stepSampleCounter_;
            uint32_t framesLeftInBlock = numFrames - framesProcessed;

            if (static_cast<double>(framesLeftInBlock) < remainingInStep) {
                // Whole remaining block fits inside current step
                stepSampleCounter_ += framesLeftInBlock;
                totalSamplesElapsed_ += framesLeftInBlock;
                framesProcessed += framesLeftInBlock;
            } else {
                // Step boundary is crossed at framesProcessed + framesToBoundary
                uint32_t framesToBoundary = static_cast<uint32_t>(std::max(0.0, remainingInStep));
                framesProcessed += framesToBoundary;
                totalSamplesElapsed_ += framesToBoundary;

                // Advance to next step
                currentStep_ = (currentStep_ + 1) % maxSteps_;
                stepSampleCounter_ = 0.0;

                if (tickCount < N) {
                    outTicks[tickCount] = StepTick{
                        currentStep_,
                        framesProcessed < numFrames ? framesProcessed : (numFrames - 1),
                        getStepLength(currentStep_)
                    };
                    tickCount++;
                }
            }
        }

        return tickCount;
    }

    [[nodiscard]] double getStepLength(uint32_t stepIdx) const noexcept {
        // Even 16th step is scaled by (swing * 2.0)
        // Odd 16th step is scaled by ((1.0 - swing) * 2.0)
        bool isEven = (stepIdx % 2 == 0);
        return isEven ? evenStepSamples_ : oddStepSamples_;
    }

private:
    void updateStepLengths() noexcept {
        // 16th notes: 4 steps per quarter note beat
        double straightStepSamples = (static_cast<double>(sampleRate_) * 60.0) / (bpm_ * 4.0);
        evenStepSamples_ = straightStepSamples * (swing_ * 2.0);
        oddStepSamples_ = straightStepSamples * ((1.0 - swing_) * 2.0);
    }

    uint32_t sampleRate_{48000};
    double bpm_{135.0};
    double swing_{0.50}; // 0.50 = straight 50/50
    PlayState state_{PlayState::Stopped};

    uint32_t currentStep_{0};
    uint32_t maxSteps_{16};
    double stepSampleCounter_{0.0};
    uint64_t totalSamplesElapsed_{0};
    bool triggeredFirstStep_{false};

    double evenStepSamples_{6000.0};
    double oddStepSamples_{6000.0};
};

} // namespace eatsbits::sequencer

#endif // EATS_TRANSPORT_HPP
