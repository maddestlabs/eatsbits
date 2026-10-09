#include "eatsbits/audio/graph/nodes/daisy/daisy_delay_node.hpp"

namespace eatsbits::audio {

void DaisyDelayNode::processBlock(uint32_t numFrames) noexcept {
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
    float fb = feedback_;

    for (uint32_t i = 0; i < numFrames; ++i) {
        float readL = delayL_.Read();
        float readR = delayR_.Read();

        float filteredL = dampL_.Process(readL);
        float filteredR = dampR_.Process(readR);

        delayL_.Write(srcL[i] + filteredL * fb);
        delayR_.Write(srcR[i] + filteredR * fb);

        outL[i] = srcL[i] * dry + readL * wet;
        outR[i] = srcR[i] * dry + readR * wet;
    }
}

} // namespace eatsbits::audio
