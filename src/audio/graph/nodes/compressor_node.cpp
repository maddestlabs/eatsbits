#include "eatsbits/audio/graph/nodes/compressor_node.hpp"

namespace eatsbits::audio {

void CompressorNode::processBlock(uint32_t numFrames) noexcept {
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
    const float* keyL = getInputBuffer(1, 0);
    const float* keyR = getInputBuffer(1, 1);
    if (!keyR) keyR = keyL;

    float wet = dryWet_;
    float dry = 1.0f - wet;

    float maxAttenDb = 0.0f;

    for (uint32_t i = 0; i < numFrames; ++i) {
        float l = srcL[i];
        float r = srcR[i];
        float cL = 0.0f;
        float cR = 0.0f;

        if (keyL) {
            cL = compL_.Process(l, keyL[i]);
            cR = compR_.Process(r, keyR ? keyR[i] : keyL[i]);
        } else {
            cL = compL_.Process(l);
            cR = compR_.Process(r);
        }

        outL[i] = l * dry + cL * wet;
        outR[i] = r * dry + cR * wet;

        // Estimate gain reduction
        float inPeak = std::max(std::abs(l), std::abs(r));
        float outPeak = std::max(std::abs(cL), std::abs(cR));
        if (inPeak > 1e-4f && outPeak < inPeak) {
            float atten = 20.0f * std::log10(outPeak / inPeak);
            if (atten < maxAttenDb) {
                maxAttenDb = atten;
            }
        }
    }

    gainReductionDb_ = maxAttenDb;
}

} // namespace eatsbits::audio
