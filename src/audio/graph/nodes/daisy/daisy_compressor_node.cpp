#include "eatsbits/audio/graph/nodes/daisy/daisy_compressor_node.hpp"

namespace eatsbits::audio {

void DaisyCompressorNode::processBlock(uint32_t numFrames) noexcept {
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
    const float* key = getInputBuffer(1, 0);

    float wet = dryWet_;
    float dry = 1.0f - wet;

    for (uint32_t i = 0; i < numFrames; ++i) {
        float l = srcL[i];
        float r = srcR[i];
        float cL = 0.0f;
        float cR = 0.0f;

        if (key) {
            float k = key[i];
            cL = compL_.Process(l, k);
            cR = compR_.Process(r, k);
        } else {
            cL = compL_.Process(l);
            cR = compR_.Process(r);
        }

        outL[i] = l * dry + cL * wet;
        outR[i] = r * dry + cR * wet;
    }
}

} // namespace eatsbits::audio
