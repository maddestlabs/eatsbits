#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <span>
#include "piano_physical_tables.hpp"

namespace eatsbits::dsp {

/// Neoprene / Felt Hammer Impact Transient Exciter.
/// Synthesizes the mechanical strike transient of a piano/EP hammer against
/// a tuning fork/tine/string with velocity-dependent pulse width and hardness.
class HammerExciter {
public:
    static void generate(float* outBuffer, size_t numFrames, float sampleRate, int midiNote,
                        float velocity, float hardness = 1.0f, float clickLevel = 1.0f) {
        if (!outBuffer || numFrames == 0) return;

        const float h = std::clamp(hardness, 0.1f, 4.0f);
        const float click = std::clamp(clickLevel, 0.0f, 3.0f);
        const float vel = std::clamp(velocity, 0.01f, 1.0f);

        // Contact duration shortens with higher velocity and harder hammer (0.8ms to 4.5ms)
        const float contactSec = std::clamp(0.0035f / (std::pow(vel, 0.4f) * h), 0.0004f, 0.010f);
        const size_t contactSamples = std::clamp(static_cast<size_t>(contactSec * sampleRate), static_cast<size_t>(4), numFrames / 2);

        std::fill(outBuffer, outBuffer + numFrames, 0.0f);

        // 1. Raised-cosine force pulse (Hertzian contact model)
        const float pi = 3.14159265358979323846f;
        const float forceExponent = 1.5f + (1.0f - vel) * 0.8f;
        for (size_t i = 0; i < contactSamples && i < numFrames; ++i) {
            float phase = (static_cast<float>(i) / contactSamples) * pi;
            float force = std::pow(std::sin(phase), forceExponent);
            outBuffer[i] = force * vel;
        }

        // 2. High-frequency micro-click transient (neoprene tip friction)
        if (click > 0.01f) {
            uint32_t state = 0x5A5A5A5Au ^ static_cast<uint32_t>(midiNote * 73);
            const size_t clickLen = std::min(numFrames, static_cast<size_t>(0.004f * sampleRate));
            const float clickDecay = sampleRate * 0.0008f;
            for (size_t i = 0; i < clickLen; ++i) {
                state ^= (state << 13);
                state ^= (state >> 17);
                state ^= (state << 5);
                float noise = (static_cast<float>(state & 0xFFFFFFu) / 8388607.5f) - 1.0f;
                float env = std::exp(-static_cast<float>(i) / clickDecay);
                outBuffer[i] += noise * env * click * 0.25f * vel;
            }
        }
    }
};

/// Plectrum / Guitar Pick Strum Exciter.
/// Models the rapid micro-brush of a guitar pick across multiple strings.
/// Produces a multi-tap comb impulse with adjustable strum spread, pick scrape noise, and velocity dynamics.
class PlectrumStrumExciter {
public:
    static void generate(float* outBuffer, size_t numFrames, float sampleRate, int midiNote,
                        float velocity, float strumSpreadMs = 8.0f, float pickBite = 1.0f) {
        if (!outBuffer || numFrames == 0) return;

        const float spreadMs = std::clamp(strumSpreadMs, 1.0f, 40.0f);
        const float bite = std::clamp(pickBite, 0.0f, 3.0f);
        const float vel = std::clamp(velocity, 0.05f, 1.0f);
        const float pi = 3.14159265358979323846f;

        std::fill(outBuffer, outBuffer + numFrames, 0.0f);

        constexpr int numTaps = 4;
        const float tapSpacingSec = (spreadMs / 1000.0f) / (numTaps - 1);

        for (int t = 0; t < numTaps; ++t) {
            const float tapTime = t * tapSpacingSec;
            const size_t startSample = static_cast<size_t>(tapTime * sampleRate);
            if (startSample >= numFrames) break;

            const size_t pulseLen = std::clamp(static_cast<size_t>((0.0015f / (vel * bite + 0.2f)) * sampleRate), static_cast<size_t>(3), static_cast<size_t>(120));
            const float tapGain = (0.7f + 0.3f * (static_cast<float>(t) / numTaps)) * vel;

            for (size_t i = 0; i < pulseLen && (startSample + i) < numFrames; ++i) {
                float phase = (static_cast<float>(i) / pulseLen) * pi;
                outBuffer[startSample + i] += std::sin(phase) * tapGain;
            }

            // Pick scrape noise
            uint32_t state = 0x1337BEEFu ^ static_cast<uint32_t>(midiNote * 37 + t * 91);
            const size_t scrapeLen = static_cast<size_t>(0.0025f * sampleRate);
            const float scrapeDecay = sampleRate * 0.0006f;
            for (size_t i = 0; i < scrapeLen && (startSample + i) < numFrames; ++i) {
                state ^= (state << 13);
                state ^= (state >> 17);
                state ^= (state << 5);
                float noise = (static_cast<float>(state & 0xFFFFFFu) / 8388607.5f) - 1.0f;
                float env = std::exp(-static_cast<float>(i) / scrapeDecay);
                outBuffer[startSample + i] += noise * env * bite * 0.35f * vel;
            }
        }
    }
};

/// Commuted Soundboard Exciter (Bank-Bensa Physical Piano).
/// Synthesizes dual dry-tap and sympathetic sustain-pedal noise bursts shaped by
/// empirical T60 time constants per MIDI note.
class CommutedSoundboardExciter {
public:
    static void generate(float* outBuffer, size_t numFrames, float sampleRate, int midiNote,
                        float velocity, float hammerHardness = 0.85f, float pedalResonance = 0.55f,
                        float soundboardGain = 1.0f) {
        if (!outBuffer || numFrames == 0) return;

        const float note = static_cast<float>(midiNote);
        const float vel = std::clamp(velocity, 0.01f, 1.0f);
        const float hardness = std::clamp(hammerHardness, 0.05f, 2.0f);
        const float pReso = std::clamp(pedalResonance, 0.0f, 2.0f);
        const float sbGain = std::clamp(soundboardGain, 0.0f, 2.0f);

        const float noteCutoffT60 = static_cast<float>(PianoPhysicalTables::dryTapAmpT60.lookup(note)) * (0.8f + 0.4f * vel);
        const float pedalEnvValue = static_cast<float>(PianoPhysicalTables::sustainPedalLevel.lookup(note)) * 0.2f * pReso;
        constexpr float pedalCutoffT60 = 1.4f;

        const float attDurSec = std::max(0.0005f, 0.015f / hardness);
        const float factorNote = std::exp(-7.0f / (noteCutoffT60 * sampleRate));
        const float factorPedal = std::exp(-7.0f / (pedalCutoffT60 * sampleRate));

        float envDry = 0.85f * vel;
        float envPedal = pedalEnvValue * vel * 2.0f;
        uint32_t seed = 0x1A2B3Cu ^ static_cast<uint32_t>(midiNote * 37);

        for (size_t i = 0; i < numFrames; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            seed ^= (seed << 13);
            seed ^= (seed >> 17);
            seed ^= (seed << 5);
            float noise = (static_cast<float>(seed & 0xFFFFFFu) / 8388607.5f) - 1.0f;

            float curEnvDry;
            if (t < attDurSec) {
                curEnvDry = (t / attDurSec) * envDry;
            } else {
                curEnvDry = envDry;
                envDry *= factorNote;
            }

            float curEnvPedal = envPedal;
            envPedal *= factorPedal;

            outBuffer[i] = (noise * curEnvDry + noise * curEnvPedal) * sbGain;
        }
    }
};

/// 4-Stage 1-Pole Non-Linear Hammer Filter Cascade.
/// Models dynamic felt compression where velocity and brightness shift the filter
/// poles and gains according to Bank-Bensa empirical measurements.
class CommutedHammerFilterCascade {
public:
    void reset() {
        s_[0] = s_[1] = s_[2] = s_[3] = 0.0;
    }

    void process(float* inOutBuffer, size_t numFrames, int midiNote,
                float velocity, float brightness = 0.50f, float hardnessScale = 1.0f) {
        if (!inOutBuffer || numFrames == 0) return;

        const double note = static_cast<double>(midiNote);
        const double vel = std::clamp(static_cast<double>(velocity), 0.01, 1.0);
        const double bright = std::clamp(static_cast<double>(brightness), 0.0, 1.0);
        const double hScale = std::clamp(static_cast<double>(hardnessScale), 0.1, 3.0);

        const double baseLoudP = PianoPhysicalTables::loudPole.lookup(note);
        const double baseSoftP = PianoPhysicalTables::softPole.lookup(note);
        const double hardOffset = (hScale - 1.0) * 0.08;
        const double brightOffset = (bright - 0.5) * 0.10;

        const double loudP = std::clamp(baseLoudP - hardOffset - brightOffset, 0.40, 0.92);
        const double softP = std::clamp(baseSoftP - hardOffset * 0.5, 0.70, 0.98);

        const double baseLoudG = PianoPhysicalTables::loudGain.lookup(note);
        const double baseSoftG = PianoPhysicalTables::softGain.lookup(note);
        const double gainScale = std::clamp(std::sqrt(hScale), 0.70, 2.0);

        const double loudG = baseLoudG * gainScale;
        const double softG = baseSoftG * std::sqrt(gainScale);

        const double normVel = std::clamp(std::pow(vel, 0.65), 0.0, 1.0);
        const double hammerPole = std::clamp(softP + (loudP - softP) * normVel, 0.40, 0.985);
        const double hammerGain = std::clamp(softG + (loudG - softG) * normVel, 0.40, 5.0);

        const double b0 = 1.0 - hammerPole;
        const double a1 = -hammerPole;

        for (size_t i = 0; i < numFrames; ++i) {
            double x = static_cast<double>(inOutBuffer[i]) * hammerGain * 1.5;
            for (int stage = 0; stage < 4; ++stage) {
                double y = b0 * x - a1 * s_[stage];
                s_[stage] = y;
                x = y;
            }
            inOutBuffer[i] = static_cast<float>(x);
        }
    }

private:
    double s_[4] = {0.0, 0.0, 0.0, 0.0};
};

/// Strike Position Comb Filter EQ.
/// Models the notch transfer function at the physical hammer strike position along the string.
class CommutedStrikeComb {
public:
    void reset() {
        s1_ = s2_ = 0.0;
    }

    void process(float* inOutBuffer, size_t numFrames, float sampleRate, int midiNote, float freq) {
        if (!inOutBuffer || numFrames == 0) return;

        const double sr = sampleRate;
        const double note = static_cast<double>(midiNote);
        const double f0 = freq > 10.0f ? static_cast<double>(freq) : 440.0;

        const double strikePos = std::clamp(PianoPhysicalTables::strikePosition.lookup(note), 0.02, 0.5);
        const double bwFactor = PianoPhysicalTables::eqBandwidthFactor.lookup(note);
        const double eqG = PianoPhysicalTables::eqGain.lookup(note);

        const double eqTuning = std::clamp(f0 / strikePos, 20.0, sr * 0.45);
        const double eqBw = std::clamp(bwFactor * f0, 10.0, sr * 0.45);
        const double pi = 3.14159265358979323846;

        const double a2 = std::clamp(std::pow(eqBw / sr, 2.0), 0.0, 0.999);
        const double a1 = -2.0 * (eqBw / sr) * std::cos(2.0 * pi * eqTuning / sr);
        const double b0 = (0.5 - 0.5 * a2) * eqG;
        const double b1 = 0.0;
        const double b2 = -b0;

        for (size_t i = 0; i < numFrames; ++i) {
            double x = static_cast<double>(inOutBuffer[i]);
            double y = b0 * x + b1 * s1_ + b2 * s2_ - a1 * s1_ - a2 * s2_;
            s2_ = s1_;
            s1_ = y;
            inOutBuffer[i] = static_cast<float>(x + y * 0.5);
        }
    }

private:
    double s1_ = 0.0;
    double s2_ = 0.0;
};

/// Upright Pluck & Slap Exciter.
/// Models the warm, elastodynamic side-finger flesh pull on a heavy acoustic double bass string.
class UprightPluckSlapExciter {
public:
    static void generate(float* outBuffer, size_t numFrames, float sampleRate, int midiNote,
                        float velocity, float pluckMass = 2.2f, float slapClick = 0.35f, float fingerFlesh = 0.70f) {
        if (!outBuffer || numFrames == 0) return;

        const float mass = std::clamp(pluckMass, 0.5f, 4.0f);
        const float slap = std::clamp(slapClick, 0.0f, 2.0f);
        const float flesh = std::clamp(fingerFlesh, 0.1f, 1.0f);
        const float vel = std::clamp(velocity, 0.05f, 1.0f);
        const float pi = 3.14159265358979323846f;

        std::fill(outBuffer, outBuffer + numFrames, 0.0f);

        const float pulseSec = std::clamp(0.012f + flesh * 0.014f, 0.010f, 0.026f);
        const size_t pluckSamples = std::clamp(static_cast<size_t>(pulseSec * sampleRate), static_cast<size_t>(16), numFrames / 2);

        for (size_t i = 0; i < pluckSamples && i < numFrames; ++i) {
            float tNorm = static_cast<float>(i) / pluckSamples;
            float attackNorm = std::clamp(tNorm / 0.55f, 0.0f, 1.0f);
            float smoothRamp = attackNorm * attackNorm * (3.0f - 2.0f * attackNorm);
            float decayFactor = std::exp(-std::max(0.0f, tNorm - 0.55f) * (3.0f + (1.0f - flesh) * 4.0f));
            float fleshPulse = smoothRamp * decayFactor;

            float woodThump = std::sin(2.0f * pi * 110.0f * (static_cast<float>(i) / sampleRate)) * std::exp(-tNorm * 3.5f);
            float exciterGain = 0.38f * (0.8f + mass * 0.15f) * vel;
            outBuffer[i] = (fleshPulse * 0.70f + woodThump * 0.30f) * exciterGain;
        }

        // Fingerboard wood slap transient
        if (slap > 0.02f && vel >= 0.65f) {
            const size_t slapLen = std::clamp(static_cast<size_t>(0.014f * sampleRate), static_cast<size_t>(10), pluckSamples);
            uint32_t rng = 0x44424153u ^ static_cast<uint32_t>(midiNote * 67);
            const float velScale = (vel - 0.65f) / 0.35f;
            for (size_t i = 0; i < slapLen && i < numFrames; ++i) {
                float tNorm = static_cast<float>(i) / slapLen;
                rng ^= (rng << 13);
                rng ^= (rng >> 17);
                rng ^= (rng << 5);
                float noise = (static_cast<float>(rng & 0xFFFFFFu) / 8388607.5f) - 1.0f;
                float slapEnv = std::exp(-tNorm * 5.5f) * (1.0f - tNorm);
                outBuffer[i] += noise * slapEnv * slap * 0.28f * velScale;
            }
        }
    }
};

} // namespace eatsbits::dsp
