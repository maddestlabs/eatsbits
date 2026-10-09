#include "eatsbits/audio/graph/nodes/daisy/daisy_filter_node.hpp"

namespace eatsbits::audio {

void DaisyFilterNode::processBlock(uint32_t numFrames) noexcept {
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
    const float* cvCutoff = getInputBuffer(1, 0);

    for (uint32_t i = 0; i < numFrames; ++i) {
        float f = cutoff_;
        if (cvCutoff) {
            f *= std::exp2(cvCutoff[i]);
            f = std::clamp(f, 10.0f, sampleRate_ * 0.48f);
        }

        float l = srcL[i];
        float r = srcR[i];

        switch (mode_) {
            case FilterMode::MoogLadder: {
                moogL_.SetFreq(f);
                moogR_.SetFreq(f);
                outL[i] = moogL_.Process(l);
                outR[i] = moogR_.Process(r);
                break;
            }
            case FilterMode::SvfLowpass: {
                float svfF = std::min(f, sampleRate_ / 3.0f);
                svfL_.SetFreq(svfF);
                svfR_.SetFreq(svfF);
                svfL_.Process(l);
                svfR_.Process(r);
                outL[i] = svfL_.Low();
                outR[i] = svfR_.Low();
                break;
            }
            case FilterMode::SvfHighpass: {
                float svfF = std::min(f, sampleRate_ / 3.0f);
                svfL_.SetFreq(svfF);
                svfR_.SetFreq(svfF);
                svfL_.Process(l);
                svfR_.Process(r);
                outL[i] = svfL_.High();
                outR[i] = svfR_.High();
                break;
            }
            case FilterMode::SvfBandpass: {
                float svfF = std::min(f, sampleRate_ / 3.0f);
                svfL_.SetFreq(svfF);
                svfR_.SetFreq(svfF);
                svfL_.Process(l);
                svfR_.Process(r);
                outL[i] = svfL_.Band();
                outR[i] = svfR_.Band();
                break;
            }
            case FilterMode::SvfNotch: {
                float svfF = std::min(f, sampleRate_ / 3.0f);
                svfL_.SetFreq(svfF);
                svfR_.SetFreq(svfF);
                svfL_.Process(l);
                svfR_.Process(r);
                outL[i] = svfL_.Notch();
                outR[i] = svfR_.Notch();
                break;
            }
        }
    }
}

} // namespace eatsbits::audio
