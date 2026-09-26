#ifndef EATS_EATSCRIPT_NODE_HPP
#define EATS_EATSCRIPT_NODE_HPP

#include <cmath>
#include <string>
#include <array>
#include <mutex>
#include "../graph_node.hpp"
#include "../../../eatscript/vm.hpp"

namespace eatsbits::audio {

/**
 * Modular Audio Graph Node powered by the Eatscript bytecode Virtual Machine.
 * Enables live-coding DSP synthesis, modulation, and effects directly in the audio graph.
 */
class EatscriptNode : public GraphNode {
public:
    explicit EatscriptNode(std::string name = "EatscriptNode")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Stereo input (e.g. for audio effects / waveshapers)
        addOutputPort(2); // Stereo output
        params_.fill(1.0f);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        timeStep_ = 1.0 / (sampleRate > 0.0 ? sampleRate : 48000.0);
        reset();
    }

    void reset() noexcept override {
        time_ = 0.0;
        gate_ = false;
        currentNote_ = 60;
        currentFreq_ = 261.63;
        velocity_ = 0.0f;
        ampEnvelope_ = 0.0f;
    }

    /**
     * Compile and hot-reload Eatscript code into the node.
     * Thread-safe; offline compilation with atomic execution handover.
     */
    bool setScript(const std::string& scriptSource) {
        std::lock_guard<std::mutex> lock(vmMutex_);
        scriptCode_ = scriptSource;
        return vm_.compileSource(scriptSource);
    }

    [[nodiscard]] const std::string& getScript() const noexcept {
        return scriptCode_;
    }

    [[nodiscard]] bool hasCompiledProcess() const noexcept {
        return vm_.hasCompiledProcess();
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        if (paramId < params_.size()) {
            params_[paramId] = value;
            vm_.setParam(paramId, value);
        }
    }

    void handleEvent(const AudioEvent& event) noexcept override {
        switch (event.type) {
            case AudioEventType::NoteOn:
                currentNote_ = event.note;
                currentFreq_ = 440.0 * std::pow(2.0, (static_cast<double>(currentNote_) - 69.0) / 12.0);
                velocity_ = event.velocity;
                gate_ = true;
                break;
            case AudioEventType::NoteOff:
                if (event.note == currentNote_) {
                    gate_ = false;
                }
                break;
            case AudioEventType::AllNotesOff:
                gate_ = false;
                ampEnvelope_ = 0.0f;
                break;
            case AudioEventType::SetParameter:
                setParameter(event.paramId, event.paramValue);
                break;
            default:
                break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override {
        float* outL = getOutputBuffer(0, 0);
        float* outR = getOutputBuffer(0, 1);
        const float* inL = getInputBuffer(0, 0);
        const float* inR = getInputBuffer(0, 1);

        if (!outL || !outR) return;

        if (!enabled_ || !vm_.hasCompiledProcess()) {
            if (inL && inR) {
                std::copy_n(inL, numFrames, outL);
                std::copy_n(inR, numFrames, outR);
            } else {
                std::fill_n(outL, numFrames, 0.0f);
                std::fill_n(outR, numFrames, 0.0f);
            }
            return;
        }

        if (vm_.isStereoEffect()) {
            for (uint32_t i = 0; i < numFrames; ++i) {
                double l = inL ? static_cast<double>(inL[i]) : 0.0;
                double r = inR ? static_cast<double>(inR[i]) : 0.0;

                auto [resL, resR] = vm_.executeStereoProcess(l, r, params_.data(), params_.size());

                if (std::isnan(resL) || std::isinf(resL)) resL = 0.0;
                if (std::isnan(resR) || std::isinf(resR)) resR = 0.0;

                outL[i] = static_cast<float>(resL);
                outR[i] = static_cast<float>(resR);
            }
            return;
        }

        const float attackCoeff = 0.05f;
        const float releaseCoeff = 0.998f;

        for (uint32_t i = 0; i < numFrames; ++i) {
            // Smooth simple gate envelope
            if (gate_) {
                ampEnvelope_ += (velocity_ - ampEnvelope_) * attackCoeff;
            } else {
                ampEnvelope_ *= releaseCoeff;
            }

            // Execute Eatscript VM process callback (zero dynamic allocations)
            double sample = vm_.executeProcess(time_, currentFreq_, static_cast<double>(currentNote_),
                                               params_.data(), params_.size());

            // Check if NaN or Inf
            if (std::isnan(sample) || std::isinf(sample)) {
                sample = 0.0;
            }

            float finalSample = static_cast<float>(sample) * ampEnvelope_;

            // If input is connected, sum with input signal
            if (inL && inR) {
                outL[i] = inL[i] + finalSample;
                outR[i] = inR[i] + finalSample;
            } else {
                outL[i] = finalSample;
                outR[i] = finalSample;
            }

            time_ += timeStep_;
        }
    }

private:
    eatscript::VM vm_;
    std::mutex vmMutex_;
    std::string scriptCode_;

    double timeStep_{1.0 / 48000.0};
    double time_{0.0};
    double currentFreq_{261.63};
    uint8_t currentNote_{60};
    float velocity_{0.0f};
    bool gate_{false};
    float ampEnvelope_{0.0f};

    std::array<float, 16> params_{};
};

} // namespace eatsbits::audio

#endif // EATS_EATSCRIPT_NODE_HPP
