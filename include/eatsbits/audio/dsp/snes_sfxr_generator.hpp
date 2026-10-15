#pragma once

#include "snes_dsp_core.hpp"
#include "prng.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace eatsbits::audio {

/// Procedural Sound Effect Generator Suite built on the SNES S-DSP / SPC700 architecture.
/// Replicates the 10 iconic chiptune/console archetypes with deterministic PRNG seed variation.
class SNESSFXRGenerator {
public:
    static constexpr const char* kSfxTypeNames[11] = {
        "Laser", "Explosion", "Powerup", "Coin", "Jump",
        "Hurt", "Lose", "Button", "Warp", "Mutate", "Custom SNES"
    };

    static constexpr const char* kWaveformNames[12] = {
        "Sine", "Square", "Pulse 25%", "Pulse 12%", "Sawtooth",
        "Triangle", "Organ", "Strings", "Flute", "Slap Bass",
        "Chime", "Noise"
    };

    /// Returns the curated list of suitable SNES BRR wavetables for each SFX archetype.
    static std::vector<SNESWaveform> getCandidateWaveformsForType(int sfxType) {
        switch (sfxType) {
            case 0: // Laser / Zap
                return {
                    SNESWaveform::Sawtooth,
                    SNESWaveform::Pulse12,
                    SNESWaveform::Pulse25,
                    SNESWaveform::Square,
                    SNESWaveform::Sine,
                    SNESWaveform::Triangle
                };
            case 1: // Explosion
                return {
                    SNESWaveform::Noise,
                    SNESWaveform::Triangle,
                    SNESWaveform::Square,
                    SNESWaveform::Sawtooth,
                    SNESWaveform::SlapBass
                };
            case 2: // Powerup / 1-Up
                return {
                    SNESWaveform::Chime,
                    SNESWaveform::Sine,
                    SNESWaveform::Triangle,
                    SNESWaveform::Flute,
                    SNESWaveform::Square,
                    SNESWaveform::Pulse25,
                    SNESWaveform::Organ
                };
            case 3: // Coin
                return {
                    SNESWaveform::Pulse25,
                    SNESWaveform::Pulse12,
                    SNESWaveform::Chime,
                    SNESWaveform::Sine,
                    SNESWaveform::Triangle,
                    SNESWaveform::Square
                };
            case 4: // Jump
                return {
                    SNESWaveform::Triangle,
                    SNESWaveform::Square,
                    SNESWaveform::Pulse25,
                    SNESWaveform::Pulse12,
                    SNESWaveform::Sine,
                    SNESWaveform::SlapBass
                };
            case 5: // Hurt / Damage
                return {
                    SNESWaveform::Square,
                    SNESWaveform::Noise,
                    SNESWaveform::Sawtooth,
                    SNESWaveform::Triangle,
                    SNESWaveform::Pulse12,
                    SNESWaveform::SlapBass
                };
            case 6: // Lose / Game Over
                return {
                    SNESWaveform::Sawtooth,
                    SNESWaveform::Triangle,
                    SNESWaveform::Organ,
                    SNESWaveform::SlapBass,
                    SNESWaveform::Noise,
                    SNESWaveform::Strings
                };
            case 7: // Button / Click / Beep
                return {
                    SNESWaveform::Pulse25,
                    SNESWaveform::Pulse12,
                    SNESWaveform::Sine,
                    SNESWaveform::Triangle,
                    SNESWaveform::Square,
                    SNESWaveform::Chime
                };
            case 8: // Warp / Teleport
                return {
                    SNESWaveform::Chime,
                    SNESWaveform::Organ,
                    SNESWaveform::Strings,
                    SNESWaveform::Flute,
                    SNESWaveform::Sine,
                    SNESWaveform::Triangle
                };
            default: // Mutate (9) / Custom SNES (10)
                return {
                    SNESWaveform::Sine,
                    SNESWaveform::Square,
                    SNESWaveform::Pulse25,
                    SNESWaveform::Pulse12,
                    SNESWaveform::Sawtooth,
                    SNESWaveform::Triangle,
                    SNESWaveform::Organ,
                    SNESWaveform::Strings,
                    SNESWaveform::Flute,
                    SNESWaveform::SlapBass,
                    SNESWaveform::Chime,
                    SNESWaveform::Noise
                };
        }
    }

    /// Generates a complete parameter map corresponding to a specific archetype and seed.
    static std::unordered_map<std::string, float> generateParamsForType(int sfxType, uint32_t seed = 42) {
        DeterministicPRNG prng(seed);
        auto candidates = getCandidateWaveformsForType(sfxType);
        int wIdx = prng.nextInt(0, static_cast<int>(candidates.size()) - 1);
        SNESWaveform chosenWaveform = candidates[wIdx];

        std::unordered_map<std::string, float> p;
        p["SFXType"] = static_cast<float>(sfxType);
        p["Seed"] = static_cast<float>(seed);
        p["Waveform"] = static_cast<float>(chosenWaveform);
        p["Attack"] = 0.005f;
        p["Decay"] = 0.25f;
        p["Sustain"] = 0.1f;
        p["Release"] = 0.2f;
        p["PitchSweep"] = 0.0f;
        p["SweepSpeed"] = 0.16f;
        p["VibratoRate"] = 0.0f;
        p["VibratoDepth"] = 0.0f;
        p["EchoDelay"] = 120.0f;
        p["EchoFeedback"] = 0.45f;
        p["EchoVolume"] = 0.0f;
        p["NoiseMix"] = 0.0f;

        switch (sfxType) {
            case 0: // Laser / Zap
                p["Attack"] = 0.001f;
                p["Decay"] = prng.nextRange(0.08f, 0.16f);
                p["Sustain"] = 0.0f;
                p["Release"] = 0.02f;
                p["PitchSweep"] = prng.nextRange(-1.8f, -0.9f);
                p["SweepSpeed"] = prng.nextRange(0.08f, 0.15f);
                p["EchoVolume"] = 0.0f;
                break;

            case 1: // Explosion
                p["Attack"] = 0.002f;
                p["Decay"] = prng.nextRange(0.35f, 0.65f);
                p["Sustain"] = 0.0f;
                p["Release"] = 0.15f;
                p["PitchSweep"] = prng.nextRange(-1.2f, -0.4f);
                p["NoiseMix"] = (chosenWaveform == SNESWaveform::Noise) ? 1.0f : prng.nextRange(0.4f, 0.85f);
                p["EchoDelay"] = 160.0f;
                p["EchoFeedback"] = 0.55f;
                p["EchoVolume"] = 0.45f;
                break;

            case 2: // Powerup / 1-Up
                p["Attack"] = 0.002f;
                p["Decay"] = prng.nextRange(0.28f, 0.42f);
                p["Sustain"] = 0.2f;
                p["Release"] = 0.25f;
                p["PitchSweep"] = 0.0f;
                p["EchoDelay"] = 128.0f;
                p["EchoFeedback"] = 0.50f;
                p["EchoVolume"] = 0.50f;
                break;

            case 3: // Coin
                p["Attack"] = 0.001f;
                p["Decay"] = prng.nextRange(0.16f, 0.26f);
                p["Sustain"] = 0.0f;
                p["Release"] = 0.05f;
                p["PitchSweep"] = 0.0f;
                p["EchoVolume"] = 0.0f;
                break;

            case 4: // Jump
                p["Attack"] = 0.002f;
                p["Decay"] = prng.nextRange(0.14f, 0.22f);
                p["Sustain"] = 0.0f;
                p["Release"] = 0.02f;
                p["PitchSweep"] = prng.nextRange(0.6f, 1.4f);
                p["SweepSpeed"] = prng.nextRange(0.12f, 0.20f);
                p["EchoVolume"] = 0.0f;
                break;

            case 5: // Hurt / Damage
                p["Attack"] = 0.001f;
                p["Decay"] = prng.nextRange(0.09f, 0.16f);
                p["Sustain"] = 0.0f;
                p["Release"] = 0.02f;
                p["PitchSweep"] = prng.nextRange(-1.4f, -0.6f);
                p["NoiseMix"] = prng.nextRange(0.3f, 0.6f);
                p["EchoVolume"] = 0.0f;
                break;

            case 6: // Lose / Game Over
                p["Attack"] = 0.005f;
                p["Decay"] = prng.nextRange(0.38f, 0.55f);
                p["Sustain"] = 0.1f;
                p["Release"] = 0.25f;
                p["VibratoRate"] = 6.5f;
                p["VibratoDepth"] = prng.nextRange(0.1f, 0.25f);
                p["EchoDelay"] = 180.0f;
                p["EchoFeedback"] = 0.60f;
                p["EchoVolume"] = 0.55f;
                break;

            case 7: // Button / Click / Beep
                p["Attack"] = 0.0005f;
                p["Decay"] = prng.nextRange(0.04f, 0.07f);
                p["Sustain"] = 0.0f;
                p["Release"] = 0.005f;
                p["EchoVolume"] = 0.0f;
                break;

            case 8: // Warp / Teleport
                p["Attack"] = 0.01f;
                p["Decay"] = prng.nextRange(0.28f, 0.42f);
                p["Sustain"] = 0.2f;
                p["Release"] = 0.2f;
                p["VibratoRate"] = prng.nextRange(18.0f, 28.0f);
                p["VibratoDepth"] = 0.4f;
                p["EchoDelay"] = 120.0f;
                p["EchoFeedback"] = 0.50f;
                p["EchoVolume"] = 0.40f;
                break;

            case 9: // Mutate
            default:
                p["Waveform"] = static_cast<float>(chosenWaveform);
                p["Attack"] = prng.nextRange(0.001f, 0.15f);
                p["Decay"] = prng.nextRange(0.08f, 0.8f);
                p["Sustain"] = prng.nextRange(0.0f, 0.6f);
                p["Release"] = prng.nextRange(0.02f, 0.5f);
                p["PitchSweep"] = prng.nextRange(-1.5f, 1.2f);
                p["EchoVolume"] = prng.nextRange(0.0f, 0.55f);
                break;
        }

        return p;
    }

    /// Configures an S-DSP engine instance directly according to an archetype.
    static void configureFromType(SNESDSPEngine& dsp, int sfxType, uint32_t seed = 42) {
        dsp.setSeed(seed);
        DeterministicPRNG prng(seed);
        auto& v = dsp.voices[0];

        switch (sfxType) {
            case 0: { // Laser / Zap
                dsp.reset();
                v.waveform = SNESWaveform::Sawtooth;
                v.startFreqMult = prng.nextRange(2.6f, 3.6f);
                v.endFreqMult = prng.nextRange(0.1f, 0.22f);
                v.sweepDuration = prng.nextRange(0.10f, 0.16f);
                v.attack = 0.001f;
                v.decay = prng.nextRange(0.10f, 0.16f);
                v.sustain = 0.0f;
                v.release = 0.01f;
                dsp.echo.enabled = false;
                dsp.echo.volume = 0.0f;
                break;
            }
            case 1: { // Explosion
                dsp.reset();
                v.noiseEnabled = true;
                v.noiseRate = prng.nextInt(4, 9);
                v.startFreqMult = 1.4f;
                v.endFreqMult = 0.2f;
                v.sweepDuration = 0.4f;
                v.envMode = SNESEnvelopeMode::GainExpDecrease;
                v.decay = prng.nextRange(0.35f, 0.6f);
                dsp.echo.enabled = true;
                dsp.echo.volume = 0.45f;
                dsp.echo.setDelayMs(160);
                dsp.echo.feedback = 0.55f;
                dsp.echo.setFIRProfile("dark_hall");
                break;
            }
            case 2: { // Powerup
                dsp.reset();
                v.waveform = SNESWaveform::Chime;
                v.arpeggioNotes = {0, 4, 7, 12, 16, 0, 0, 0};
                v.arpeggioCount = 5;
                v.arpeggioSpeed = prng.nextRange(0.035f, 0.055f);
                v.attack = 0.002f;
                v.decay = 0.35f;
                v.sustain = 0.2f;
                v.release = 0.25f;
                dsp.echo.enabled = true;
                dsp.echo.volume = 0.5f;
                dsp.echo.setDelayMs(128);
                dsp.echo.feedback = 0.5f;
                dsp.echo.setFIRProfile("surround_reverb");
                break;
            }
            case 3: { // Coin
                dsp.reset();
                v.waveform = SNESWaveform::Pulse25;
                v.arpeggioNotes = {0, 12, 0, 0, 0, 0, 0, 0};
                v.arpeggioCount = 2;
                v.arpeggioSpeed = 0.04f;
                v.attack = 0.001f;
                v.decay = prng.nextRange(0.18f, 0.28f);
                v.sustain = 0.0f;
                v.release = 0.05f;
                dsp.echo.enabled = false;
                dsp.echo.volume = 0.0f;
                break;
            }
            case 4: { // Jump
                dsp.reset();
                v.waveform = SNESWaveform::Triangle;
                v.startFreqMult = prng.nextRange(0.4f, 0.55f);
                v.endFreqMult = prng.nextRange(1.5f, 1.9f);
                v.sweepDuration = prng.nextRange(0.12f, 0.20f);
                v.attack = 0.002f;
                v.decay = 0.18f;
                v.sustain = 0.0f;
                v.release = 0.02f;
                dsp.echo.enabled = false;
                dsp.echo.volume = 0.0f;
                break;
            }
            case 5: { // Hurt
                dsp.reset();
                v.waveform = SNESWaveform::Square;
                v.startFreqMult = 1.8f;
                v.endFreqMult = 0.4f;
                v.sweepDuration = 0.09f;
                v.attack = 0.001f;
                v.decay = 0.12f;
                v.sustain = 0.0f;
                v.noiseMix = prng.nextRange(0.3f, 0.6f);
                dsp.echo.enabled = false;
                dsp.echo.volume = 0.0f;
                break;
            }
            case 6: { // Lose
                dsp.reset();
                v.waveform = SNESWaveform::Sawtooth;
                v.arpeggioNotes = {0, -3, -7, -12, 0, 0, 0, 0};
                v.arpeggioCount = 4;
                v.arpeggioSpeed = prng.nextRange(0.06f, 0.09f);
                v.vibratoRate = 6.5f;
                v.vibratoDepth = 0.15f;
                v.attack = 0.005f;
                v.decay = 0.45f;
                v.sustain = 0.1f;
                v.release = 0.25f;
                dsp.echo.enabled = true;
                dsp.echo.volume = 0.55f;
                dsp.echo.setDelayMs(180);
                dsp.echo.feedback = 0.6f;
                dsp.echo.setFIRProfile("dark_hall");
                break;
            }
            case 7: { // Button
                dsp.reset();
                v.waveform = SNESWaveform::Pulse25;
                v.arpeggioNotes = {0, 7, 0, 0, 0, 0, 0, 0};
                v.arpeggioCount = 2;
                v.arpeggioSpeed = prng.nextRange(0.020f, 0.026f);
                v.startFreqMult = 1.0f;
                v.endFreqMult = 1.15f;
                v.sweepDuration = 0.05f;
                v.attack = 0.0005f;
                v.decay = 0.055f;
                v.sustain = 0.0f;
                v.release = 0.005f;
                dsp.echo.enabled = false;
                dsp.echo.volume = 0.0f;
                break;
            }
            case 8: { // Warp
                dsp.reset();
                v.waveform = SNESWaveform::Chime;
                v.vibratoRate = prng.nextRange(18.0f, 28.0f);
                v.vibratoDepth = 0.4f;
                v.attack = 0.01f;
                v.decay = 0.35f;
                v.sustain = 0.2f;
                v.release = 0.2f;
                dsp.echo.enabled = true;
                dsp.echo.volume = 0.4f;
                dsp.echo.setDelayMs(120);
                dsp.echo.setFIRProfile("metallic_chorus");
                break;
            }
            default: { // Mutate / Custom
                dsp.reset();
                auto p = generateParamsForType(sfxType, seed);
                v.waveform = static_cast<SNESWaveform>(static_cast<int>(p["Waveform"]));
                v.attack = p["Attack"];
                v.decay = p["Decay"];
                v.sustain = p["Sustain"];
                v.release = p["Release"];
                float sweep = p["PitchSweep"];
                if (std::abs(sweep) > 0.05f) {
                    if (sweep < 0.0f) {
                        v.startFreqMult = 1.0f - sweep * 1.5f;
                        v.endFreqMult = 0.2f;
                    } else {
                        v.startFreqMult = 0.4f;
                        v.endFreqMult = 1.0f + sweep * 1.5f;
                    }
                    v.sweepDuration = p["SweepSpeed"];
                }
                v.vibratoRate = p["VibratoRate"];
                v.vibratoDepth = p["VibratoDepth"];
                dsp.echo.enabled = (p["EchoVolume"] > 0.02f);
                dsp.echo.volume = p["EchoVolume"];
                dsp.echo.setDelayMs(static_cast<int>(p["EchoDelay"]));
                dsp.echo.feedback = p["EchoFeedback"];
                break;
            }
        }
    }
};

} // namespace eatsbits::audio
