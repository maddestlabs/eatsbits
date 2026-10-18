#ifndef EATS_ARPEGGIATOR_NODE_HPP
#define EATS_ARPEGGIATOR_NODE_HPP

#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include "../graph_node.hpp"

namespace eatsbits::audio {

/**
 * Modular Arpeggiator Graph Node.
 * Audio-rate CV/Gate Arpeggiator with authentic C64 chiptune speed support (50Hz).
 * 
 * Input Port 0: Base Pitch CV In (1.0V/Oct, 0.0V = C4 = pitch 60)
 * Input Port 1: Clock / Gate In (Optional; advances on rising edge > 0.5V. If disconnected, uses internal clock)
 * Output Port 0: Arp Pitch CV Out (1.0V/Oct)
 * Output Port 1: Gate Out (1.0 during active gate, 0.0 during rest)
 */
class ArpeggiatorNode : public GraphNode {
public:
    enum class ArpPattern {
        Up = 0,
        Down = 1,
        UpDown = 2,
        Random = 3,
        ChiptuneCycle = 4
    };

    enum class ArpVoicing {
        MajorTriad = 0,     // [0, 4, 7]
        MinorTriad = 1,     // [0, 3, 7]
        Dominant7 = 2,      // [0, 4, 7, 10]
        Minor7 = 3,         // [0, 3, 7, 10]
        Major7 = 4,         // [0, 4, 7, 11]
        Suspended4 = 5,     // [0, 5, 7]
        OctavesOnly = 6     // [0, 12]
    };

    explicit ArpeggiatorNode(std::string name = "Arpeggiator")
        : GraphNode(std::move(name)) {
        addInputPort(1);  // Port 0: Base CV In
        addInputPort(1);  // Port 1: External Clock / Gate In
        addOutputPort(1); // Port 0: Arp Pitch CV Out
        addOutputPort(1); // Port 1: Gate Out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = static_cast<float>(sampleRate);
        maxBlockSize_ = maxBlockSize;
        updateTiming();
        reset();
    }

    void reset() noexcept override {
        currentStep_ = 0;
        stepDirection_ = 1;
        sampleCounter_ = 0;
        lastClockState_ = false;
        currentNoteOffset_ = 0;
        isGateHigh_ = false;
    }

    void setRateIndex(int rateIdx) noexcept {
        rateIndex_ = std::clamp(rateIdx, 0, 6);
        updateTiming();
    }
    [[nodiscard]] int getRateIndex() const noexcept { return rateIndex_; }

    void setPattern(ArpPattern pat) noexcept {
        pattern_ = pat;
    }
    [[nodiscard]] ArpPattern getPattern() const noexcept { return pattern_; }

    void setOctaves(int oct) noexcept {
        octaves_ = std::clamp(oct, 1, 4);
    }
    [[nodiscard]] int getOctaves() const noexcept { return octaves_; }

    void setGateLength(float gate) noexcept {
        gateLength_ = std::clamp(gate, 0.05f, 0.98f);
    }
    [[nodiscard]] float getGateLength() const noexcept { return gateLength_; }

    void setVoicing(ArpVoicing v) noexcept {
        voicing_ = v;
    }
    [[nodiscard]] ArpVoicing getVoicing() const noexcept { return voicing_; }

    void setBpm(float bpm) noexcept {
        bpm_ = std::clamp(bpm, 20.0f, 400.0f);
        updateTiming();
    }
    [[nodiscard]] float getBpm() const noexcept { return bpm_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setRateIndex(static_cast<int>(std::round(value))); break;
            case 1: setPattern(static_cast<ArpPattern>(static_cast<int>(value))); break;
            case 2: setOctaves(static_cast<int>(std::round(value))); break;
            case 3: setGateLength(value); break;
            case 4: setVoicing(static_cast<ArpVoicing>(static_cast<int>(value))); break;
            case 5: setBpm(value); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override {
        float* outPitch = getOutputBuffer(0, 0);
        float* outGate = getOutputBuffer(1, 0);
        if (!outPitch || !outGate) return;

        const float* inBasePitch = getInputBuffer(0, 0);
        const float* inClock = getInputBuffer(1, 0);

        if (!enabled_) {
            if (inBasePitch) {
                std::copy_n(inBasePitch, numFrames, outPitch);
            } else {
                std::fill_n(outPitch, numFrames, 0.0f);
            }
            std::fill_n(outGate, numFrames, 0.0f);
            return;
        }

        const auto intervals = getVoicingIntervals(voicing_);
        const size_t totalNotes = intervals.size() * static_cast<size_t>(octaves_);

        for (uint32_t i = 0; i < numFrames; ++i) {
            bool advanceStep = false;

            if (inClock) {
                bool clockHigh = (inClock[i] >= 0.5f);
                if (clockHigh && !lastClockState_) {
                    advanceStep = true;
                }
                lastClockState_ = clockHigh;
                isGateHigh_ = clockHigh;
            } else {
                // Internal sub-clock engine
                ++sampleCounter_;
                if (sampleCounter_ >= samplesPerStep_) {
                    sampleCounter_ = 0;
                    advanceStep = true;
                }
                isGateHigh_ = (sampleCounter_ < static_cast<uint32_t>(static_cast<float>(samplesPerStep_) * gateLength_));
            }

            if (advanceStep && totalNotes > 0) {
                stepNextNote(totalNotes);
            }

            // Calculate current pitch offset in semitones
            size_t noteIdx = currentStep_ % totalNotes;
            size_t baseIdx = noteIdx % intervals.size();
            int octOffset = static_cast<int>(noteIdx / intervals.size()) * 12;
            int semiOffset = intervals[baseIdx] + octOffset;

            float baseV = inBasePitch ? inBasePitch[i] : 0.0f;
            outPitch[i] = baseV + (static_cast<float>(semiOffset) / 12.0f);
            outGate[i] = isGateHigh_ ? 1.0f : 0.0f;
        }
    }

private:
    void updateTiming() noexcept {
        // Musical note speed divisions relative to 120 BPM quarter note:
        // 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32, 4 = 1/64, 5 = 1/128, 6 = 50Hz (C64)
        if (rateIndex_ == 6) {
            // Authentic Commodore 64 50Hz PAL VBLANK refresh (20 ms)
            samplesPerStep_ = static_cast<uint32_t>(sampleRate_ * 0.020f);
            return;
        }

        float beatSeconds = 60.0f / bpm_;
        float stepFactor = 1.0f;
        switch (rateIndex_) {
            case 0: stepFactor = 1.0f; break;      // 1/4
            case 1: stepFactor = 0.5f; break;      // 1/8
            case 2: stepFactor = 0.25f; break;     // 1/16
            case 3: stepFactor = 0.125f; break;    // 1/32
            case 4: stepFactor = 0.0625f; break;   // 1/64
            case 5: stepFactor = 0.03125f; break;  // 1/128
            default: stepFactor = 0.25f; break;
        }
        samplesPerStep_ = std::max(16u, static_cast<uint32_t>(sampleRate_ * beatSeconds * stepFactor));
    }

    void stepNextNote(size_t totalNotes) noexcept {
        if (totalNotes <= 1) {
            currentStep_ = 0;
            return;
        }

        switch (pattern_) {
            case ArpPattern::Up:
                currentStep_ = (currentStep_ + 1) % totalNotes;
                break;
            case ArpPattern::Down:
                currentStep_ = (currentStep_ + totalNotes - 1) % totalNotes;
                break;
            case ArpPattern::UpDown:
                if (stepDirection_ > 0) {
                    if (currentStep_ + 1 >= totalNotes) {
                        stepDirection_ = -1;
                        currentStep_ = totalNotes - 2;
                    } else {
                        currentStep_++;
                    }
                } else {
                    if (currentStep_ == 0) {
                        stepDirection_ = 1;
                        currentStep_ = 1;
                    } else {
                        currentStep_--;
                    }
                }
                break;
            case ArpPattern::Random:
                currentStep_ = static_cast<size_t>(std::rand()) % totalNotes;
                break;
            case ArpPattern::ChiptuneCycle:
                currentStep_ = (currentStep_ + 1) % totalNotes;
                break;
        }
    }

    static std::vector<int> getVoicingIntervals(ArpVoicing v) {
        switch (v) {
            case ArpVoicing::MajorTriad:  return {0, 4, 7};
            case ArpVoicing::MinorTriad:  return {0, 3, 7};
            case ArpVoicing::Dominant7:   return {0, 4, 7, 10};
            case ArpVoicing::Minor7:      return {0, 3, 7, 10};
            case ArpVoicing::Major7:      return {0, 4, 7, 11};
            case ArpVoicing::Suspended4:  return {0, 5, 7};
            case ArpVoicing::OctavesOnly: return {0, 12};
            default:                      return {0, 4, 7};
        }
    }

    float sampleRate_{48000.0f};
    int rateIndex_{2}; // Default 1/16
    ArpPattern pattern_{ArpPattern::Up};
    int octaves_{1};
    float gateLength_{0.75f};
    ArpVoicing voicing_{ArpVoicing::MajorTriad};
    float bpm_{120.0f};

    uint32_t samplesPerStep_{6000};
    uint32_t sampleCounter_{0};
    size_t currentStep_{0};
    int stepDirection_{1};
    bool lastClockState_{false};
    bool isGateHigh_{false};
    int currentNoteOffset_{0};
};

} // namespace eatsbits::audio

#endif // EATS_ARPEGGIATOR_NODE_HPP
