#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <cstring>
#include "procedural_ir.hpp"

namespace eatsbits::audio::dsp {

class ConvolverCore {
public:
    static constexpr size_t kHistorySize = 32768; // Power of 2 (32K samples ~ 740ms at 44.1kHz)
    static constexpr size_t kHistoryMask = kHistorySize - 1;
    static constexpr size_t kMaxIrLength = 8192;
    static constexpr size_t kDefaultIrLength = 3072; // Optimized for high fidelity + real-time performance

    ConvolverCore(float sampleRate = 44100.0f)
        : sampleRate_(sampleRate) {
        historyL_.assign(kHistorySize, 0.0f);
        historyR_.assign(kHistorySize, 0.0f);
        activeIrL_.assign(kMaxIrLength, 0.0f);
        activeIrR_.assign(kMaxIrLength, 0.0f);
        targetIrL_.assign(kMaxIrLength, 0.0f);
        targetIrR_.assign(kMaxIrLength, 0.0f);

        updateDampingCoeffs();
        // Load default Great Hall preset
        loadPreset("Great Hall");
    }

    void reset() {
        std::fill(historyL_.begin(), historyL_.end(), 0.0f);
        std::fill(historyR_.begin(), historyR_.end(), 0.0f);
        writePos_ = 0;
        lpfStateL_ = 0.0f;
        lpfStateR_ = 0.0f;
        hpfStateL_ = 0.0f;
        hpfStateR_ = 0.0f;
        hpfInPrevL_ = 0.0f;
        hpfInPrevR_ = 0.0f;
        crossfadeRemaining_ = 0;
    }

    void setSampleRate(float sr) {
        if (sr > 1000.0f) {
            sampleRate_ = sr;
            updateDampingCoeffs();
            regenerateCurrentSpace();
        }
    }

    float getSampleRate() const noexcept { return sampleRate_; }

    void setMix(float mix) noexcept { mix_ = std::clamp(mix, 0.0f, 1.0f); }
    float getMix() const noexcept { return mix_; }

    void setPreDelay(float ms) noexcept { preDelayMs_ = std::clamp(ms, 0.0f, 100.0f); }
    float getPreDelay() const noexcept { return preDelayMs_; }

    void setDecay(float decay) {
        decay_ = std::clamp(decay, 0.1f, 2.5f);
        regenerateCurrentSpace();
    }
    float getDecay() const noexcept { return decay_; }

    void setRoomSize(float scale) {
        roomSize_ = std::clamp(scale, 0.2f, 3.0f);
        regenerateCurrentSpace();
    }
    float getRoomSize() const noexcept { return roomSize_; }

    void setDamping(float damping) {
        currentParams_.damping = std::clamp(damping, 0.0f, 1.0f);
        regenerateCurrentSpace();
    }
    float getDamping() const noexcept { return currentParams_.damping; }

    void setGated(bool gated, float holdTimeMs = 180.0f, float releaseTimeMs = 20.0f) {
        currentParams_.isGated = gated;
        currentParams_.gateHoldMs = holdTimeMs;
        currentParams_.gateReleaseMs = releaseTimeMs;
        regenerateCurrentSpace();
    }
    bool isGated() const noexcept { return currentParams_.isGated; }

    void setHighCut(float hz) {
        highCutHz_ = std::clamp(hz, 500.0f, 20000.0f);
        updateDampingCoeffs();
    }
    float getHighCut() const noexcept { return highCutHz_; }

    void setLowCut(float hz) {
        lowCutHz_ = std::clamp(hz, 20.0f, 2000.0f);
        updateDampingCoeffs();
    }
    float getLowCut() const noexcept { return lowCutHz_; }

    size_t getIrLength() const noexcept { return irLength_; }
    const std::string& getCurrentPresetName() const noexcept { return currentPresetName_; }
    const AcousticSpaceParams& getCurrentSpaceParams() const noexcept { return currentParams_; }

    void loadPreset(const std::string& name) {
        const auto* preset = ProceduralIRGenerator::findPreset(name);
        if (preset) {
            currentParams_ = *preset;
            regenerateCurrentSpace();
        }
    }

    void loadPresetIndex(size_t index) {
        const auto& presets = ProceduralIRGenerator::getStockPresets();
        if (index < presets.size()) {
            loadPreset(presets[index].name);
        }
    }

    void setAcousticSpace(const AcousticSpaceParams& params) {
        currentParams_ = params;
        regenerateCurrentSpace();
    }

    StereoIRBuffer bakeCustomSpace(const AcousticSpaceParams& params) {
        ProceduralIRGenerator::registerCustomPreset(params);
        setAcousticSpace(params);
        StereoIRBuffer result;
        result.left.assign(targetIrL_.begin(), targetIrL_.begin() + targetLength_);
        result.right.assign(targetIrR_.begin(), targetIrR_.begin() + targetLength_);
        return result;
    }

    void loadImpulseResponse(const StereoIRBuffer& ir, const std::string& name = "Custom IR") {
        if (ir.empty()) return;

        size_t len = std::min(ir.size(), kMaxIrLength);
        targetLength_ = len;

        for (size_t i = 0; i < len; ++i) {
            targetIrL_[i] = ir.left[i];
            targetIrR_[i] = ir.right[i];
        }
        for (size_t i = len; i < kMaxIrLength; ++i) {
            targetIrL_[i] = 0.0f;
            targetIrR_[i] = 0.0f;
        }

        currentPresetName_ = name;

        // If not initialized yet, copy immediately
        if (irLength_ == 0) {
            std::copy(targetIrL_.begin(), targetIrL_.end(), activeIrL_.begin());
            std::copy(targetIrR_.begin(), targetIrR_.end(), activeIrR_.begin());
            irLength_ = targetLength_;
            crossfadeRemaining_ = 0;
        } else {
            // Initiate click-free crossfade over 128 samples
            crossfadeTotal_ = 128;
            crossfadeRemaining_ = 128;
        }
    }

    // Zero-allocation real-time stereo processing
    void processStereo(const float* inL, const float* inR, float* outL, float* outR, size_t numFrames) noexcept {
        if (numFrames == 0) return;

        // If fully dry, pass through
        if (mix_ <= 0.001f) {
            if (outL != inL) std::memcpy(outL, inL, numFrames * sizeof(float));
            if (outR != inR) std::memcpy(outR, inR, numFrames * sizeof(float));
            return;
        }

        const int preDelaySamples = static_cast<int>((preDelayMs_ * 0.001f) * sampleRate_);
        const size_t curIrLen = irLength_;
        const float wetGain = 1.6f; // Standard acoustic makeup gain

        const float* __restrict hL = historyL_.data();
        const float* __restrict hR = historyR_.data();
        const float* __restrict irL = activeIrL_.data();
        const float* __restrict irR = activeIrR_.data();

        for (size_t i = 0; i < numFrames; ++i) {
            // 1. Push into circular delay history
            historyL_[writePos_] = inL[i];
            historyR_[writePos_] = inR[i];

            // 2. Convolution with active IR
            int readHead = static_cast<int>(writePos_ - preDelaySamples);
            float convL = 0.0f;
            float convR = 0.0f;

            size_t k = 0;
            for (; k + 3 < curIrLen; k += 4) {
                size_t idx0 = static_cast<size_t>(readHead - static_cast<int>(k)) & kHistoryMask;
                size_t idx1 = static_cast<size_t>(readHead - static_cast<int>(k + 1)) & kHistoryMask;
                size_t idx2 = static_cast<size_t>(readHead - static_cast<int>(k + 2)) & kHistoryMask;
                size_t idx3 = static_cast<size_t>(readHead - static_cast<int>(k + 3)) & kHistoryMask;

                convL += (hL[idx0] * irL[k] + hL[idx1] * irL[k + 1]) + (hL[idx2] * irL[k + 2] + hL[idx3] * irL[k + 3]);
                convR += (hR[idx0] * irR[k] + hR[idx1] * irR[k + 1]) + (hR[idx2] * irR[k + 2] + hR[idx3] * irR[k + 3]);
            }
            for (; k < curIrLen; ++k) {
                size_t idx = static_cast<size_t>(readHead - static_cast<int>(k)) & kHistoryMask;
                convL += hL[idx] * irL[k];
                convR += hR[idx] * irR[k];
            }

            // 3. Handle hot-swap crossfade if active
            if (crossfadeRemaining_ > 0) {
                float convNextL = 0.0f;
                float convNextR = 0.0f;
                for (size_t ck = 0; ck < targetLength_; ++ck) {
                    size_t idx = static_cast<size_t>(readHead - static_cast<int>(ck)) & kHistoryMask;
                    convNextL += historyL_[idx] * targetIrL_[ck];
                    convNextR += historyR_[idx] * targetIrR_[ck];
                }

                float t = 1.0f - (static_cast<float>(crossfadeRemaining_) / static_cast<float>(crossfadeTotal_));
                convL = (1.0f - t) * convL + t * convNextL;
                convR = (1.0f - t) * convR + t * convNextR;

                --crossfadeRemaining_;
                if (crossfadeRemaining_ == 0) {
                    std::copy(targetIrL_.begin(), targetIrL_.end(), activeIrL_.begin());
                    std::copy(targetIrR_.begin(), targetIrR_.end(), activeIrR_.begin());
                    irLength_ = targetLength_;
                }
            }

            // 4. Low-Cut (High-Pass) Filter
            float hpL = hpfCoeff_ * (hpfStateL_ + convL - hpfInPrevL_);
            hpfInPrevL_ = convL;
            hpfStateL_ = hpL;

            float hpR = hpfCoeff_ * (hpfStateR_ + convR - hpfInPrevR_);
            hpfInPrevR_ = convR;
            hpfStateR_ = hpR;

            // 5. High-Cut (Low-Pass Damping) Filter
            lpfStateL_ += lpfCoeff_ * (hpL - lpfStateL_);
            lpfStateR_ += lpfCoeff_ * (hpR - lpfStateR_);

            float wetL = lpfStateL_ * wetGain;
            float wetR = lpfStateR_ * wetGain;

            // 6. Equal-power or linear mix blend
            outL[i] = inL[i] * (1.0f - mix_) + wetL * mix_;
            outR[i] = inR[i] * (1.0f - mix_) + wetR * mix_;

            writePos_ = (writePos_ + 1) & kHistoryMask;
        }
    }

private:
    void regenerateCurrentSpace() {
        AcousticSpaceParams mod = currentParams_;
        mod = mod.withRoomScale(roomSize_);
        mod.rt60 = std::clamp(mod.rt60 * decay_, 0.015f, 6.0f);
        size_t targetLen = mod.isCabinetMode ? 1024 : (mod.isGated ? 2048 : kDefaultIrLength);
        auto ir = ProceduralIRGenerator::generateStereo(
            mod,
            static_cast<int>(sampleRate_),
            static_cast<int>(targetLen)
        );
        loadImpulseResponse(ir, mod.name);
    }

    void updateDampingCoeffs() {
        // 1-pole Low-pass coefficient: alpha = dt / (RC + dt)
        float dt = 1.0f / sampleRate_;
        float rcLpf = 1.0f / (2.0f * 3.14159265359f * highCutHz_);
        lpfCoeff_ = std::clamp(dt / (rcLpf + dt), 0.01f, 1.0f);

        // 1-pole High-pass coefficient: alpha = RC / (RC + dt)
        float rcHpf = 1.0f / (2.0f * 3.14159265359f * lowCutHz_);
        hpfCoeff_ = std::clamp(rcHpf / (rcHpf + dt), 0.01f, 0.999f);
    }

    float sampleRate_{44100.0f};
    float mix_{0.35f};
    float preDelayMs_{10.0f};
    float decay_{1.0f};
    float roomSize_{1.0f};
    float highCutHz_{8500.0f};
    float lowCutHz_{80.0f};

    AcousticSpaceParams currentParams_;

    float lpfCoeff_{0.5f};
    float hpfCoeff_{0.99f};
    float lpfStateL_{0.0f};
    float lpfStateR_{0.0f};
    float hpfStateL_{0.0f};
    float hpfStateR_{0.0f};
    float hpfInPrevL_{0.0f};
    float hpfInPrevR_{0.0f};

    std::vector<float> historyL_;
    std::vector<float> historyR_;
    size_t writePos_{0};

    std::vector<float> activeIrL_;
    std::vector<float> activeIrR_;
    size_t irLength_{0};

    std::vector<float> targetIrL_;
    std::vector<float> targetIrR_;
    size_t targetLength_{0};

    int crossfadeRemaining_{0};
    int crossfadeTotal_{128};

    std::string currentPresetName_;
};

} // namespace eatsbits::audio::dsp
