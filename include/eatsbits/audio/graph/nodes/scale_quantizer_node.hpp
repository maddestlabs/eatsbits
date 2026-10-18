#ifndef EATS_SCALE_QUANTIZER_NODE_HPP
#define EATS_SCALE_QUANTIZER_NODE_HPP

#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include "../graph_node.hpp"

namespace eatsbits::audio {

/**
 * Modular Scale Quantizer Graph Node.
 * Quantizes continuous or unquantized Pitch CV (1V/Oct) to musical scale intervals.
 * 
 * Input Port 0: Pitch CV In (1.0V/Oct, continuous or from LFO/sequencer)
 * Output Port 0: Quantized Pitch CV Out (1.0V/Oct snapped to scale)
 * Output Port 1: Trigger / Gate Out (1.0 pulse on note changes, 0.0 otherwise)
 */
class ScaleQuantizerNode : public GraphNode {
public:
    enum class ScaleMode {
        Major = 0,
        NaturalMinor = 1,
        HarmonicMinor = 2,
        MelodicMinor = 3,
        Dorian = 4,
        Mixolydian = 5,
        PentatonicMajor = 6,
        PentatonicMinor = 7,
        Blues = 8,
        Chromatic = 9
    };

    explicit ScaleQuantizerNode(std::string name = "ScaleQuantizer")
        : GraphNode(std::move(name)) {
        addInputPort(1);  // Port 0: CV In (1 channel)
        addOutputPort(1); // Port 0: Quantized Pitch CV Out (1 channel)
        addOutputPort(1); // Port 1: Trigger Pulse Out (1 channel)
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = static_cast<float>(sampleRate);
        maxBlockSize_ = maxBlockSize;
        reset();
    }

    void reset() noexcept override {
        lastQuantizedSemitone_ = -9999;
        triggerCounter_ = 0;
    }

    void setRootKey(int rootKey) noexcept {
        rootKey_ = std::clamp(rootKey, 0, 11);
    }
    [[nodiscard]] int getRootKey() const noexcept { return rootKey_; }

    void setScaleMode(ScaleMode mode) noexcept {
        scaleMode_ = mode;
    }
    [[nodiscard]] ScaleMode getScaleMode() const noexcept { return scaleMode_; }

    void setSnapStrength(float str) noexcept {
        snapStrength_ = std::clamp(str, 0.0f, 1.0f);
    }
    [[nodiscard]] float getSnapStrength() const noexcept { return snapStrength_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setRootKey(static_cast<int>(std::round(value))); break;
            case 1: setScaleMode(static_cast<ScaleMode>(static_cast<int>(value))); break;
            case 2: setSnapStrength(value); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override {
        float* outCv = getOutputBuffer(0, 0);
        float* outTrig = getOutputBuffer(1, 0);
        if (!outCv) return;

        const float* inCv = getInputBuffer(0, 0);
        if (!enabled_ || !inCv) {
            if (inCv) {
                std::copy_n(inCv, numFrames, outCv);
            } else {
                std::fill_n(outCv, numFrames, 0.0f);
            }
            if (outTrig) std::fill_n(outTrig, numFrames, 0.0f);
            return;
        }

        // Scale pitch-class intervals (semitones from root)
        const auto scaleMask = getScaleMask(scaleMode_);

        for (uint32_t i = 0; i < numFrames; ++i) {
            float rawVolts = inCv[i];
            float rawSemitones = rawVolts * 12.0f;

            // Quantize to nearest scale degree
            int nearestSemi = quantizeSemitone(rawSemitones, rootKey_, scaleMask);
            float snappedVolts = static_cast<float>(nearestSemi) / 12.0f;

            // Blend based on snap strength
            float resultVolts = rawVolts + snapStrength_ * (snappedVolts - rawVolts);
            outCv[i] = resultVolts;

            // Trigger generation on note step transitions
            float trigVal = 0.0f;
            if (nearestSemi != lastQuantizedSemitone_) {
                lastQuantizedSemitone_ = nearestSemi;
                triggerCounter_ = static_cast<uint32_t>(sampleRate_ * 0.005f); // 5ms trigger pulse
            }
            if (triggerCounter_ > 0) {
                trigVal = 1.0f;
                --triggerCounter_;
            }
            if (outTrig) outTrig[i] = trigVal;
        }
    }

private:
    static uint16_t getScaleMask(ScaleMode mode) noexcept {
        // 12-bit mask representing active intervals in octave: bit 0 = root
        switch (mode) {
            case ScaleMode::Major:           return 0b101010110101; // 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 (W W H W W W H)
            case ScaleMode::NaturalMinor:    return 0b010110101101; // W H W W H W W
            case ScaleMode::HarmonicMinor:   return 0b100110101101; // W H W W H 3H H
            case ScaleMode::MelodicMinor:    return 0b101010101101; // W H W W W W H
            case ScaleMode::Dorian:          return 0b010101101101; // W H W W W H W
            case ScaleMode::Mixolydian:      return 0b011010110101; // W W H W W H W
            case ScaleMode::PentatonicMajor: return 0b001001010001; // 0, 2, 4, 7, 9
            case ScaleMode::PentatonicMinor: return 0b010010001001; // 0, 3, 5, 7, 10
            case ScaleMode::Blues:           return 0b010011001001; // 0, 3, 5, 6, 7, 10
            case ScaleMode::Chromatic:       return 0x0FFF;
            default:                         return 0x0FFF;
        }
    }

    static int quantizeSemitone(float rawSemi, int rootKey, uint16_t mask) noexcept {
        int rounded = static_cast<int>(std::round(rawSemi));
        int bestSemi = rounded;
        int minDistance = 999;

        // Search within +/- 6 semitones for nearest valid scale note
        for (int offset = -6; offset <= 6; ++offset) {
            int candidate = rounded + offset;
            int pitchClass = ((candidate - rootKey) % 12 + 12) % 12;
            if ((mask & (1 << pitchClass)) != 0) {
                int dist = std::abs(candidate - rounded);
                if (dist < minDistance) {
                    minDistance = dist;
                    bestSemi = candidate;
                }
            }
        }
        return bestSemi;
    }

    float sampleRate_{48000.0f};
    int rootKey_{0}; // 0 = C
    ScaleMode scaleMode_{ScaleMode::Major};
    float snapStrength_{1.0f};
    int lastQuantizedSemitone_{-9999};
    uint32_t triggerCounter_{0};
};

} // namespace eatsbits::audio

#endif // EATS_SCALE_QUANTIZER_NODE_HPP
