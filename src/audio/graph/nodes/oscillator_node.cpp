#include "eatsbits/audio/graph/nodes/oscillator_node.hpp"

namespace eatsbits::audio {

void OscillatorNode::processBlock(uint32_t numFrames) noexcept {
    float* outL = getOutputBuffer(0, 0);
    float* outR = getOutputBuffer(0, 1);
    if (!outL || !outR) return;

    if (!enabled_) {
        std::fill_n(outL, numFrames, 0.0f);
        std::fill_n(outR, numFrames, 0.0f);
        return;
    }

    const float* cvIn = getInputBuffer(0, 0);

    for (uint32_t i = 0; i < numFrames; ++i) {
        float f = frequency_;
        if (cvIn) {
            // 1V/Oct CV standard: f = f0 * 2^(cv)
            f *= std::exp2(cvIn[i]);
            f = std::clamp(f, 5.0f, 22000.0f);
        }

        float s = 0.0f;
        if (waveform_ == Waveform::VariableSaw) {
            varSaw_.SetFreq(f);
            s = varSaw_.Process() * amplitude_;
        } else {
            osc_.SetFreq(f);
            s = osc_.Process();
        }

        outL[i] = s;
        outR[i] = s;
    }
}

} // namespace eatsbits::audio
