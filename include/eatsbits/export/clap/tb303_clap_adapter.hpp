#ifndef EATS_TB303_CLAP_ADAPTER_HPP
#define EATS_TB303_CLAP_ADAPTER_HPP

#include "clap_plugin_shell.hpp"
#include "../../audio/dsp/tb303_core.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace eatsbits::export_clap {

/**
 * Adapter exposing Tb303Core through the ClapPluginShell interface.
 */
class Tb303ClapAdapter {
public:
    static const clap_plugin_descriptor_t* getDescriptor() noexcept {
        static const char* const s_features[] = {
            CLAP_PLUGIN_FEATURE_INSTRUMENT,
            CLAP_PLUGIN_FEATURE_SYNTHESIZER,
            CLAP_PLUGIN_FEATURE_MONO,
            nullptr
        };

        static const clap_plugin_descriptor_t s_desc = {
            .clap_version = CLAP_VERSION,
            .id = "com.maddestlabs.eatsbits.tb303",
            .name = "Eatsbits TB-303",
            .vendor = "Maddest Labs",
            .url = "https://github.com/maddestlabs/eatsbits",
            .manual_url = "",
            .support_url = "https://github.com/maddestlabs/eatsbits/issues",
            .version = "1.0.0",
            .description = "Authentic Roland TB-303 Acid Bassline Synthesizer",
            .features = s_features,
        };

        return &s_desc;
    }

    static uint32_t getParamCount() noexcept {
        return static_cast<uint32_t>(paramDefs().size());
    }

    static const ClapParamDef& getParamDef(uint32_t index) noexcept {
        return paramDefs()[index];
    }

    void setSampleRate(double sampleRate) noexcept {
        tb303_.setSampleRate(static_cast<float>(sampleRate));
    }

    void reset() noexcept {
        tb303_.reset();
    }

    void noteOn(uint8_t note, float velocity, bool isSlide = false, bool isAccent = false) noexcept {
        tb303_.noteOn(note, velocity, isSlide, isAccent);
    }

    void noteOff(uint8_t /*note*/ = 0, float /*velocity*/ = 0.0f) noexcept {
        tb303_.noteOff();
    }

    [[nodiscard]] bool isActive() const noexcept {
        return tb303_.isActive();
    }

    [[nodiscard]] double getParamValue(clap_id id) const noexcept {
        switch (id) {
            case 0: return cutoffHz_;
            case 1: return resonance_;
            case 2: return envMod_;
            case 3: return decay_;
            case 4: return accent_;
            case 5: return overdrive_;
            case 6: return waveform_;
            case 7: return volume_;
            default: return 0.0;
        }
    }

    void setParamValue(clap_id id, double val) noexcept {
        switch (id) {
            case 0:
                cutoffHz_ = static_cast<float>(std::clamp(val, 20.0, 18000.0));
                tb303_.setCutoff(cutoffHz_);
                break;
            case 1:
                resonance_ = static_cast<float>(std::clamp(val, 0.0, 1.0));
                tb303_.setResonance(resonance_);
                break;
            case 2:
                envMod_ = static_cast<float>(std::clamp(val, 0.0, 1.0));
                tb303_.setEnvMod(envMod_);
                break;
            case 3:
                decay_ = static_cast<float>(std::clamp(val, 0.0, 1.0));
                tb303_.setDecay(decay_);
                break;
            case 4:
                accent_ = static_cast<float>(std::clamp(val, 0.0, 1.0));
                tb303_.setAccent(accent_);
                break;
            case 5:
                overdrive_ = static_cast<float>(std::clamp(val, 0.0, 1.0));
                tb303_.setOverdrive(overdrive_);
                break;
            case 6:
                waveform_ = static_cast<float>(std::clamp(val, 0.0, 1.0));
                tb303_.setWaveform(waveform_);
                break;
            case 7:
                volume_ = static_cast<float>(std::clamp(val, 0.0, 2.0));
                break;
            default:
                break;
        }
    }

    bool valueToText(clap_id id, double val, char* buf, uint32_t cap) const noexcept {
        if (!buf || cap == 0) return false;
        switch (id) {
            case 0:
                std::snprintf(buf, cap, "%.1f Hz", val);
                return true;
            case 6:
                std::snprintf(buf, cap, "%s", (val < 0.5) ? "Saw" : "Square");
                return true;
            case 7:
                std::snprintf(buf, cap, "%.0f %%", val * 100.0);
                return true;
            default:
                std::snprintf(buf, cap, "%.1f %%", val * 100.0);
                return true;
        }
    }

    bool textToValue(clap_id id, const char* text, double* out) const noexcept {
        if (!text || !out) return false;
        if (id == 6) {
#if defined(_WIN32)
            if (_stricmp(text, "Square") == 0 || _stricmp(text, "Sqr") == 0) {
                *out = 1.0;
                return true;
            }
            if (_stricmp(text, "Saw") == 0 || _stricmp(text, "Sawtooth") == 0) {
                *out = 0.0;
                return true;
            }
#else
            if (strcasecmp(text, "Square") == 0 || strcasecmp(text, "Sqr") == 0) {
                *out = 1.0;
                return true;
            }
            if (strcasecmp(text, "Saw") == 0 || strcasecmp(text, "Sawtooth") == 0) {
                *out = 0.0;
                return true;
            }
#endif
        }
        char* end = nullptr;
        *out = std::strtod(text, &end);
        return (end != text);
    }

    void processBlock(float* outL, float* outR, size_t numFrames) noexcept {
        for (size_t i = 0; i < numFrames; ++i) {
            const float s = tb303_.processSample() * volume_;
            outL[i] = s;
            outR[i] = s;
        }
    }

    dsp::Tb303Core& core() noexcept { return tb303_; }
    const dsp::Tb303Core& core() const noexcept { return tb303_; }

private:
    static const std::array<ClapParamDef, 8>& paramDefs() noexcept {
        static const std::array<ClapParamDef, 8> s_defs = {{
            {0, "Cutoff", "Filter", 20.0, 18000.0, 1400.0, CLAP_PARAM_IS_AUTOMATABLE},
            {1, "Resonance", "Filter", 0.0, 1.0, 0.5, CLAP_PARAM_IS_AUTOMATABLE},
            {2, "Env Mod", "Filter", 0.0, 1.0, 0.75, CLAP_PARAM_IS_AUTOMATABLE},
            {3, "Decay", "Envelope", 0.0, 1.0, 0.5, CLAP_PARAM_IS_AUTOMATABLE},
            {4, "Accent", "Filter", 0.0, 1.0, 0.0, CLAP_PARAM_IS_AUTOMATABLE},
            {5, "Overdrive", "Distortion", 0.0, 1.0, 0.25, CLAP_PARAM_IS_AUTOMATABLE},
            {6, "Waveform", "Oscillator", 0.0, 1.0, 0.0, CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_AUTOMATABLE},
            {7, "Volume", "Output", 0.0, 2.0, 1.0, CLAP_PARAM_IS_AUTOMATABLE}
        }};
        return s_defs;
    }

    dsp::Tb303Core tb303_;
    float cutoffHz_{1400.0f};
    float resonance_{0.5f};
    float envMod_{0.75f};
    float decay_{0.5f};
    float accent_{0.0f};
    float overdrive_{0.25f};
    float waveform_{0.0f};
    float volume_{1.0f};
};

} // namespace eatsbits::export_clap

#endif // EATS_TB303_CLAP_ADAPTER_HPP
