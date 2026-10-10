#include "eatsbits/audio/graph/nodes/reverb_node.hpp"

namespace eatsbits::audio {

void ReverbNode::processBlock(uint32_t numFrames) noexcept {
    const float* inL = getInputBuffer(0, 0);
    const float* inR = getInputBuffer(0, 1);
    float* outL = getOutputBuffer(0, 0);
    float* outR = getOutputBuffer(0, 1);

    if (!outL || !outR) return;

    if (!enabled_ || (!inL && !inR)) {
        if (inL && outL != inL) std::copy_n(inL, numFrames, outL);
        else if (!inL) std::fill_n(outL, numFrames, 0.0f);

        if (inR && outR != inR) std::copy_n(inR, numFrames, outR);
        else if (!inR) std::fill_n(outR, numFrames, 0.0f);
        return;
    }

    const float* srcL = inL ? inL : inR;
    const float* srcR = inR ? inR : inL;

    float wet = dryWet_;
    float dry = 1.0f - wet;

    for (uint32_t i = 0; i < numFrames; ++i) {
        float outWetL = 0.0f;
        float outWetR = 0.0f;
        verb_.Process(srcL[i], srcR[i], &outWetL, &outWetR);

        outL[i] = srcL[i] * dry + outWetL * wet;
        outR[i] = srcR[i] * dry + outWetR * wet;
    }
}

} // namespace eatsbits::audio
