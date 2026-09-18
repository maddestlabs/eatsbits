#ifndef EATS_ADSR_HPP
#define EATS_ADSR_HPP

#include <cmath>
#include <algorithm>

namespace eatsbits::dsp {

enum class AdsrState {
    Idle,
    Attack,
    Decay,
    Sustain,
    Release
};

/**
 * Sample-accurate ADSR Envelope Generator with linear/exponential behavior.
 */
class AdsrEnvelope {
public:
    AdsrEnvelope() noexcept = default;

    void setSampleRate(float sampleRate) noexcept {
        sampleRate_ = (sampleRate > 0.0f) ? sampleRate : 48000.0f;
        updateRates();
    }

    void setParameters(float attackSec, float decaySec, float sustainLevel, float releaseSec) noexcept {
        attackSec_ = std::max(0.0005f, attackSec);
        decaySec_ = std::max(0.0005f, decaySec);
        sustainLevel_ = std::clamp(sustainLevel, 0.0f, 1.0f);
        releaseSec_ = std::max(0.0005f, releaseSec);
        updateRates();
    }

    void noteOn(float velocity = 1.0f) noexcept {
        velocity_ = std::clamp(velocity, 0.0f, 1.0f);
        state_ = AdsrState::Attack;
    }

    void noteOff() noexcept {
        if (state_ != AdsrState::Idle) {
            state_ = AdsrState::Release;
        }
    }

    void reset() noexcept {
        state_ = AdsrState::Idle;
        currentValue_ = 0.0f;
    }

    [[nodiscard]] inline float process() noexcept {
        switch (state_) {
            case AdsrState::Idle:
                currentValue_ = 0.0f;
                break;

            case AdsrState::Attack:
                currentValue_ += attackRate_;
                if (currentValue_ >= 1.0f) {
                    currentValue_ = 1.0f;
                    state_ = AdsrState::Decay;
                }
                break;

            case AdsrState::Decay:
                currentValue_ -= decayRate_;
                if (currentValue_ <= sustainLevel_) {
                    currentValue_ = sustainLevel_;
                    state_ = AdsrState::Sustain;
                }
                break;

            case AdsrState::Sustain:
                currentValue_ = sustainLevel_;
                break;

            case AdsrState::Release:
                currentValue_ -= releaseRate_;
                if (currentValue_ <= 0.0f) {
                    currentValue_ = 0.0f;
                    state_ = AdsrState::Idle;
                }
                break;
        }

        return currentValue_ * velocity_;
    }

    [[nodiscard]] bool isActive() const noexcept {
        return state_ != AdsrState::Idle;
    }

    [[nodiscard]] AdsrState getState() const noexcept {
        return state_;
    }

    [[nodiscard]] float getValue() const noexcept {
        return currentValue_ * velocity_;
    }

private:
    void updateRates() noexcept {
        attackRate_ = 1.0f / (attackSec_ * sampleRate_);
        decayRate_ = (1.0f - sustainLevel_) / (decaySec_ * sampleRate_);
        releaseRate_ = (sustainLevel_ > 0.001f ? sustainLevel_ : 1.0f) / (releaseSec_ * sampleRate_);
    }

    float sampleRate_{48000.0f};
    float attackSec_{0.01f};
    float decaySec_{0.1f};
    float sustainLevel_{0.7f};
    float releaseSec_{0.2f};

    float attackRate_{0.001f};
    float decayRate_{0.001f};
    float releaseRate_{0.001f};

    float currentValue_{0.0f};
    float velocity_{1.0f};
    AdsrState state_{AdsrState::Idle};
};

} // namespace eatsbits::dsp

#endif // EATS_ADSR_HPP
