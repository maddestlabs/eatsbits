#pragma once

#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <span>
#include "piano_physical_tables.hpp"

namespace eatsbits::dsp {

/// Generic Digital Waveguide Core with 1-Pole Loop Loss Filter (Karplus-Strong Extension).
/// Suitable for acoustic nylon/steel guitars, harps, plucked strings, and resonant bars.
class DigitalWaveguideCore {
public:
    static constexpr size_t kMaxDelaySamples = 4096; // Supports down to ~11Hz at 44.1kHz

    DigitalWaveguideCore() {
        reset();
    }

    void reset() {
        std::fill(delayLine_.begin(), delayLine_.end(), 0.0f);
        writeIdx_ = 0;
        filterState_ = 0.0f;
    }

    /// Process in-place: outBuffer initially contains exciter impulses, and will be
    /// overwritten with the vibrating string output.
    void process(float* inOutBuffer, size_t numFrames, float sampleRate, float baseFreq,
                 float feedback = 0.995f, float damping = 0.25f, float pitchBendSemitones = 0.0f) {
        if (!inOutBuffer || numFrames == 0) return;

        const float f0 = baseFreq > 10.0f ? baseFreq : 440.0f;
        const float sr = sampleRate;
        const float fb = std::clamp(feedback, 0.80f, 0.9999f);
        const float damp = std::clamp(damping, 0.01f, 0.95f);

        const float curFreq = std::clamp(f0 * std::pow(2.0f, pitchBendSemitones / 12.0f), 20.0f, sr * 0.48f);
        const float delaySamples = std::clamp(sr / curFreq, 2.0f, static_cast<float>(kMaxDelaySamples - 4));
        const float frac = delaySamples - std::floor(delaySamples);

        for (size_t i = 0; i < numFrames; ++i) {
            const float inSample = inOutBuffer[i];

            // Read from delay line with fractional linear interpolation
            float readPos = static_cast<float>(writeIdx_) - delaySamples;
            if (readPos < 0.0f) readPos += static_cast<float>(kMaxDelaySamples);
            while (readPos >= static_cast<float>(kMaxDelaySamples)) readPos -= static_cast<float>(kMaxDelaySamples);

            const size_t i0 = static_cast<size_t>(readPos) % kMaxDelaySamples;
            const size_t i1 = (i0 + 1) % kMaxDelaySamples;
            const float delayedSample = delayLine_[i0] + frac * (delayLine_[i1] - delayLine_[i0]);

            // 1-Pole Lowpass loop damping filter
            filterState_ = (1.0f - damp) * delayedSample + damp * filterState_;
            const float loopSample = filterState_ * fb;

            // Write excitation + recirculating string wave
            delayLine_[writeIdx_] = inSample + loopSample;
            writeIdx_ = (writeIdx_ + 1) % kMaxDelaySamples;

            inOutBuffer[i] = inSample + loopSample;
        }
    }

private:
    std::array<float, kMaxDelaySamples> delayLine_{};
    size_t writeIdx_ = 0;
    float filterState_ = 0.0f;
};

/// Commuted Piano Coupled Waveguide with All-Pass Inharmonic Dispersion.
/// Based on Bank & Bensa (2005) / Stanford CCRMA Faust STK-4.3 commuted piano model.
/// Features dual parallel delay lines, 3-stage allpass string stiffness dispersion,
/// and bridge coupling matrix (double decay / singing tail).
class CommutedPianoWaveguideCore {
public:
    static constexpr size_t kMaxDelaySamples = 4096;

    CommutedPianoWaveguideCore() {
        reset();
    }

    void reset() {
        std::fill(delayLine1_.begin(), delayLine1_.end(), 0.0);
        std::fill(delayLine2_.begin(), delayLine2_.end(), 0.0);
        writeIdx_ = 0;
        for (int s = 0; s < 3; ++s) {
            ap1_x_[s] = ap1_y_[s] = 0.0;
            ap2_x_[s] = ap2_y_[s] = 0.0;
        }
        cFilter_x_ = cFilter_y_ = 0.0;
        bridgeFeedback1_ = bridgeFeedback2_ = 0.0;
    }

    void process(float* inOutBuffer, size_t numFrames, float sampleRate, int midiNote, float freq,
                 float stiffnessFactor = 1.0f, float detuningFactor = 1.0f, float sustainScale = 1.0f,
                 bool isNoteOff = false) {
        if (!inOutBuffer || numFrames == 0) return;

        const double sr = sampleRate;
        const double note = static_cast<double>(midiNote);
        const double f0 = freq > 10.0f ? static_cast<double>(freq) : 440.0;
        const double stiffMult = std::clamp(static_cast<double>(stiffnessFactor), 0.0, 4.0);
        const double detuneMult = std::clamp(static_cast<double>(detuningFactor), 0.0, 5.0);
        const double susMult = std::clamp(static_cast<double>(sustainScale), 0.5, 1.2);
        const double pi = 3.14159265358979323846;

        // Single-string decay & bridge coupling filter parameters
        const double singleDecay = PianoPhysicalTables::singleStringDecayRate.lookup(note);
        const double gAtt = std::clamp(std::pow(10.0, (singleDecay / f0) / 20.0), 0.90, 0.9999);
        const double bCoupl = PianoPhysicalTables::singleStringZero.lookup(note);
        const double aCoupl = PianoPhysicalTables::singleStringPole.lookup(note);

        const double tempD = 3.0 * (1.0 - bCoupl) - gAtt * (1.0 - aCoupl);
        const double b0C = (std::abs(tempD) > 0.0001) ? (2.0 * (gAtt * (1.0 - aCoupl) - (1.0 - bCoupl)) / tempD) : 0.0;
        const double b1C = (std::abs(tempD) > 0.0001) ? (2.0 * (aCoupl * (1.0 - bCoupl) - gAtt * (1.0 - aCoupl) * bCoupl) / tempD) : 0.0;
        const double a1C = (std::abs(tempD) > 0.0001) ? ((gAtt * (1.0 - aCoupl) * bCoupl - 3.0 * aCoupl * (1.0 - bCoupl)) / tempD) : 0.0;

        // String stiffness coefficient for allpass dispersion
        const double stiffness = std::clamp(stiffMult * PianoPhysicalTables::stiffnessCoefficient.lookup(note), -0.95, 0.95);

        // Detuning for 2 coupled string courses
        const double hzDetune = PianoPhysicalTables::detuningHz.lookup(note) * detuneMult;
        const double freq1 = std::max(20.0, f0 + 0.5 * hzDetune);
        const double freq2 = std::max(20.0, f0 - 0.5 * hzDetune);

        auto calcDelayLength = [&](double f) -> double {
            const double wT = std::clamp(f * 2.0 * pi / sr, 0.001, pi * 0.95);
            const double sinWT = std::sin(wT);
            const double cosWT = std::cos(wT);

            const double numAp = (stiffness * stiffness - 1.0) * sinWT;
            const double denAp = 2.0 * stiffness + (stiffness * stiffness + 1.0) * cosWT;
            const double apPhase = std::atan2(numAp, denAp);

            const double b0P = 1.0 + 2.0 * b0C;
            const double b1P = a1C + 2.0 * b1C;
            const double a1P = a1C;

            const double numPz = -b1P * sinWT * (1.0 + a1P * cosWT) + a1P * sinWT * (b0P + b1P * cosWT);
            const double denPz = (b0P + b1P * cosWT) * (1.0 + a1P * cosWT) + b1P * sinWT * a1P * sinWT;
            const double pzPhase = std::atan2(numPz, denPz);

            const double dLen = (2.0 * pi + 3.0 * apPhase + pzPhase) / wT;
            return std::clamp(dLen, 2.0, static_cast<double>(kMaxDelaySamples - 8));
        };

        const double dLen1 = calcDelayLength(freq1);
        const double dLen2 = calcDelayLength(freq2);

        double loopGain = std::clamp(0.9996 * susMult, 0.90, 0.9999);
        if (isNoteOff) {
            loopGain = std::min(loopGain, PianoPhysicalTables::releaseLoopGain.lookup(note));
        }

        for (size_t i = 0; i < numFrames; ++i) {
            const double exc = static_cast<double>(inOutBuffer[i]);

            double in1 = (exc + bridgeFeedback1_) * loopGain;
            double in2 = (exc + bridgeFeedback2_) * loopGain;

            // 3-stage allpass dispersion on string 1
            for (int s = 0; s < 3; ++s) {
                const double y = stiffness * in1 + ap1_x_[s] - stiffness * ap1_y_[s];
                ap1_x_[s] = in1;
                ap1_y_[s] = y;
                in1 = y;
            }

            // 3-stage allpass dispersion on string 2
            for (int s = 0; s < 3; ++s) {
                const double y = stiffness * in2 + ap2_x_[s] - stiffness * ap2_y_[s];
                ap2_x_[s] = in2;
                ap2_y_[s] = y;
                in2 = y;
            }

            delayLine1_[writeIdx_] = in1;
            delayLine2_[writeIdx_] = in2;

            // Fractional read string 1
            double rPos1 = static_cast<double>(writeIdx_) - dLen1;
            if (rPos1 < 0.0) rPos1 += static_cast<double>(kMaxDelaySamples);
            while (rPos1 >= static_cast<double>(kMaxDelaySamples)) rPos1 -= static_cast<double>(kMaxDelaySamples);

            const size_t i1_0 = static_cast<size_t>(rPos1) % kMaxDelaySamples;
            const size_t i1_1 = (i1_0 + 1) % kMaxDelaySamples;
            const double frac1 = rPos1 - std::floor(rPos1);
            const double out1 = delayLine1_[i1_0] * (1.0 - frac1) + delayLine1_[i1_1] * frac1;

            // Fractional read string 2
            double rPos2 = static_cast<double>(writeIdx_) - dLen2;
            if (rPos2 < 0.0) rPos2 += static_cast<double>(kMaxDelaySamples);
            while (rPos2 >= static_cast<double>(kMaxDelaySamples)) rPos2 -= static_cast<double>(kMaxDelaySamples);

            const size_t i2_0 = static_cast<size_t>(rPos2) % kMaxDelaySamples;
            const size_t i2_1 = (i2_0 + 1) % kMaxDelaySamples;
            const double frac2 = rPos2 - std::floor(rPos2);
            const double out2 = delayLine2_[i2_0] * (1.0 - frac2) + delayLine2_[i2_1] * frac2;

            // Bridge coupling
            const double sumOut = out1 + out2;
            const double bridgeCoupled = b0C * sumOut + b1C * cFilter_x_ - a1C * cFilter_y_;
            cFilter_x_ = sumOut;
            cFilter_y_ = bridgeCoupled;

            bridgeFeedback1_ = out1 + bridgeCoupled;
            bridgeFeedback2_ = out2 + bridgeCoupled;

            writeIdx_ = (writeIdx_ + 1) % kMaxDelaySamples;

            inOutBuffer[i] = static_cast<float>((out1 + out2) * 0.5);
        }
    }

private:
    std::array<double, kMaxDelaySamples> delayLine1_{};
    std::array<double, kMaxDelaySamples> delayLine2_{};
    size_t writeIdx_ = 0;

    double ap1_x_[3] = {0.0, 0.0, 0.0};
    double ap1_y_[3] = {0.0, 0.0, 0.0};
    double ap2_x_[3] = {0.0, 0.0, 0.0};
    double ap2_y_[3] = {0.0, 0.0, 0.0};

    double cFilter_x_ = 0.0;
    double cFilter_y_ = 0.0;

    double bridgeFeedback1_ = 0.0;
    double bridgeFeedback2_ = 0.0;
};

/// Upright Bass Waveguide Core with 4-Point Hermite Interpolation, Allpass Dispersion,
/// and Non-Linear Ebony Fingerboard Collision Solver (Hunt-Crossley contact mechanics).
class UprightBassWaveguideCore {
public:
    static constexpr size_t kMaxDelaySamples = 4096;

    UprightBassWaveguideCore() {
        reset();
    }

    void reset() {
        std::fill(delayLine_.begin(), delayLine_.end(), 0.0f);
        writeIdx_ = 0;
        lossFilterState_ = 0.0f;
        dispStateIn_ = 0.0f;
        dispStateOut_ = 0.0f;
    }

    void process(float* inOutBuffer, size_t numFrames, float sampleRate, float freq,
                 float sustain = 0.995f, float stringDamping = 0.28f, float dispersion = 0.22f,
                 float actionHeight = 3.2f, float fingerboardSlap = 0.40f) {
        if (!inOutBuffer || numFrames == 0) return;

        const float sr = sampleRate;
        const float curFreq = std::clamp(freq > 10.0f ? freq : 55.0f, 15.0f, 2000.0f);
        const float fb = std::clamp(sustain, 0.85f, 0.9998f);
        const float damp = std::clamp(stringDamping, 0.02f, 0.95f);
        const float disp = std::clamp(dispersion, 0.0f, 0.85f);
        const float action = std::clamp(actionHeight, 1.0f, 8.0f);
        const float slap = std::clamp(fingerboardSlap, 0.0f, 2.0f);

        const float delaySamples = std::clamp(sr / curFreq, 4.0f, static_cast<float>(kMaxDelaySamples - 8));

        // String stiffness & allpass dispersion:
        // Heavy low strings (C1, C2) exhibit higher inharmonicity B than lighter strings
        const float stringStiffness = std::clamp(std::sqrt(90.0f / curFreq), 0.3f, 2.4f);
        const float effectiveDisp = std::clamp(disp * stringStiffness, 0.01f, 0.85f);
        const float aDisp = -effectiveDisp * 0.38f;

        // Loop loss filter damping & decay rate:
        const float curDamp = std::clamp(damp * 0.08f * std::clamp(curFreq / 100.0f, 0.3f, 2.0f), 0.005f, 0.40f);
        const float decayRate = (0.75f + (1.0f - fb) * 14.0f) * std::pow(curFreq / 55.0f, 0.32f);
        const float exactLoopFb = std::clamp(std::exp(-decayRate / curFreq), 0.970f, 0.9985f);

        // Action clearance limit & collision stiffness for Hunt-Crossley collision:
        const float collisionThreshold = (action / 4.0f) * 0.42f;
        const float collisionStiffness = 1.6f + slap * 1.2f;
        const float excursionScale = std::clamp(std::sqrt(65.0f / curFreq), 0.7f, 1.8f);
        const float thresh = collisionThreshold * excursionScale * 1.4f;

        for (size_t i = 0; i < numFrames; ++i) {
            const float inSample = inOutBuffer[i];

            // 4-point Hermite cubic interpolation
            float readPos = static_cast<float>(writeIdx_) - delaySamples;
            if (readPos < 0.0f) readPos += static_cast<float>(kMaxDelaySamples);
            while (readPos >= static_cast<float>(kMaxDelaySamples)) readPos -= static_cast<float>(kMaxDelaySamples);

            const size_t i1 = static_cast<size_t>(readPos) % kMaxDelaySamples;
            const size_t i0 = (i1 - 1 + kMaxDelaySamples) % kMaxDelaySamples;
            const size_t i2 = (i1 + 1) % kMaxDelaySamples;
            const size_t i3 = (i1 + 2) % kMaxDelaySamples;
            const float frac = readPos - std::floor(readPos);

            const float y0 = delayLine_[i0];
            const float y1 = delayLine_[i1];
            const float y2 = delayLine_[i2];
            const float y3 = delayLine_[i3];
            const float c0 = y1;
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            const float delayedSample = ((c3 * frac + c2) * frac + c1) * frac + c0;

            // 1. Allpass dispersion filter
            const float dispOut = aDisp * delayedSample + dispStateIn_ - aDisp * dispStateOut_;
            dispStateIn_ = delayedSample;
            dispStateOut_ = dispOut;

            // 2. 1-pole loop loss damping filter
            lossFilterState_ = (1.0f - curDamp) * dispOut + curDamp * lossFilterState_;
            float loopSample = lossFilterState_ * exactLoopFb;

            // 3. Non-linear ebony fingerboard collision solver (fretless growl & slap)
            if (loopSample > thresh) {
                const float delta = loopSample - thresh;
                loopSample = thresh + delta * (0.65f / (1.0f + collisionStiffness * delta));
            } else if (loopSample < -thresh * 1.2f) {
                const float delta = -loopSample - thresh * 1.2f;
                loopSample = -(thresh * 1.2f + delta * (0.70f / (1.0f + collisionStiffness * 0.7f * delta)));
            }

            // 4. Waveguide feedback injection
            delayLine_[writeIdx_] = inSample + loopSample;
            writeIdx_ = (writeIdx_ + 1) % kMaxDelaySamples;

            inOutBuffer[i] = inSample * 0.5f + loopSample;
        }
    }

private:
    std::array<float, kMaxDelaySamples> delayLine_{};
    size_t writeIdx_ = 0;
    float lossFilterState_ = 0.0f;
    float dispStateIn_ = 0.0f;
    float dispStateOut_ = 0.0f;
};

} // namespace eatsbits::dsp
