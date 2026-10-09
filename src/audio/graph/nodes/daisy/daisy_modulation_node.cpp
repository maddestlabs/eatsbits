#include "eatsbits/audio/graph/nodes/daisy/daisy_modulation_node.hpp"

namespace eatsbits::audio {

void DaisyModulationNode::processBlock(uint32_t numFrames) noexcept {
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
        float l = srcL[i];
        float r = srcR[i];
        float modL = 0.0f;
        float modR = 0.0f;

        switch (type_) {
            case ModType::Chorus: {
                chorus_.Process(0.5f * (l + r));
                modL = chorus_.GetLeft();
                modR = chorus_.GetRight();
                break;
            }
            case ModType::Flanger: {
                modL = flangerL_.Process(l);
                modR = flangerR_.Process(r);
                break;
            }
            case ModType::Overdrive: {
                modL = overdriveL_.Process(l);
                modR = overdriveR_.Process(r);
                break;
            }
        }

        outL[i] = l * dry + modL * wet;
        outR[i] = r * dry + modR * wet;
    }
}

} // namespace eatsbits::audio
