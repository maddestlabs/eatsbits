#ifndef EATS_TB303_CLAP_ADAPTER_HPP
#define EATS_TB303_CLAP_ADAPTER_HPP

#include "clap_plugin_shell.hpp"
#include "gui/instrument_faceplate.hpp"
#include "gui/instrument_gui_desc.hpp"
#include "gui/plugin_gui.hpp"
#include "gui/pugl_bridge.hpp"
#include "../../audio/dsp/tb303_core.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace eatsbits::export_clap {

/**
 * Eats-303 Reference CLAP Adapter.
 * Bridges Tb303Core to ClapPluginShell and PuglBridge with the full 12-parameter
 * hardware GUI matching presets/instruments/eats_303.eats.
 */
class Eats303ClapAdapter : public gui::IParamProvider {
public:
    Eats303ClapAdapter() {
        // Initialize DSP core to match eats_303.eats default preset
        tb303_.setWaveform(0.0f);
        tb303_.setPitch(0.0f);
        tb303_.setCutoff(1400.0f);
        tb303_.setResonance(9.2f);
        tb303_.setEnvMod(0.75f);
        tb303_.setDecay(0.28f);
        tb303_.setAccent(0.78f);
        tb303_.setOctave(0);
        tb303_.setSubWaveform(0.0f);
        tb303_.setSubVolume(0.0f);
        tb303_.setDrive(0.25f);
        tb303_.setGlideCurve(0.0f);
    }
    ~Eats303ClapAdapter() override = default;

    Eats303ClapAdapter(const Eats303ClapAdapter&) = delete;
    Eats303ClapAdapter& operator=(const Eats303ClapAdapter&) = delete;
    Eats303ClapAdapter(Eats303ClapAdapter&&) = delete;
    Eats303ClapAdapter& operator=(Eats303ClapAdapter&&) = delete;

    static const clap_plugin_descriptor_t* getDescriptor() noexcept {
        static const char* const s_features[] = {
            CLAP_PLUGIN_FEATURE_INSTRUMENT,
            CLAP_PLUGIN_FEATURE_SYNTHESIZER,
            CLAP_PLUGIN_FEATURE_MONO,
            nullptr
        };

        static const clap_plugin_descriptor_t s_desc = {
            .clap_version = CLAP_VERSION,
            .id = "com.maddestlabs.eatsbits.eats303",
            .name = "Eats-303",
            .vendor = "Maddest Labs",
            .url = "https://github.com/maddestlabs/eatsbits",
            .manual_url = "",
            .support_url = "https://github.com/maddestlabs/eatsbits/issues",
            .version = "1.0.0",
            .description = "Authentic Eats-303 Acid Bassline Synthesizer",
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

    [[nodiscard]] static constexpr bool hasCustomGui() noexcept {
        return true;
    }

    static gui::InstrumentGuiDesc getGuiDesc() noexcept {
        gui::InstrumentGuiDesc d;
        d.title = "EATS-303 ACID BASSLINE";
        d.subtitle = "Eats-303 Acid Synth • (JC-303 & Open303 DSP)";
        d.background = "minimal_white";

        // Row 1: WAVEFORM (selector), Divider, PITCH, CUTOFF, RESONANCE, ENV MOD, DECAY, ACCENT
        gui::RowDesc r1;
        r1.widgets.push_back({gui::WidgetType::Knob, 0, "Waveform", "WAVEFORM", gui::KnobStyle::Selector, 0.0, 1.0, 0.0, true, 44.0f});
        r1.widgets.push_back({gui::WidgetType::Divider, 0, "", "", gui::KnobStyle::Standard, 0.0, 1.0, 0.0, false, 0.0f});
        r1.widgets.push_back({gui::WidgetType::Knob, 1, "Pitch", "PITCH", gui::KnobStyle::Potentiometer, -12.0, 12.0, 0.0, true, 44.0f});
        r1.widgets.push_back({gui::WidgetType::Knob, 2, "Cutoff", "CUTOFF", gui::KnobStyle::Potentiometer, 200.0, 4500.0, 1400.0, false, 44.0f});
        r1.widgets.push_back({gui::WidgetType::Knob, 3, "Resonance", "RESONANCE", gui::KnobStyle::Potentiometer, 0.5, 16.0, 9.2, false, 44.0f});
        r1.widgets.push_back({gui::WidgetType::Knob, 4, "EnvMod", "ENV MOD", gui::KnobStyle::Potentiometer, 0.0, 1.0, 0.75, false, 44.0f});
        r1.widgets.push_back({gui::WidgetType::Knob, 5, "Decay", "DECAY", gui::KnobStyle::Potentiometer, 0.05, 1.2, 0.28, false, 44.0f});
        r1.widgets.push_back({gui::WidgetType::Knob, 6, "Accent", "ACCENT", gui::KnobStyle::Potentiometer, 0.0, 1.0, 0.78, false, 44.0f});
        d.rows.push_back(std::move(r1));

        // Row 2: OCTAVE (selector), Divider, SUB OSC (switch), SUB VOL, Divider, GLIDE CURVE, Divider, DRIVE
        gui::RowDesc r2;
        r2.widgets.push_back({gui::WidgetType::Knob, 7, "Octave", "OCTAVE", gui::KnobStyle::Selector, -2.0, 0.0, 0.0, true, 44.0f});
        r2.widgets.push_back({gui::WidgetType::Divider, 0, "", "", gui::KnobStyle::Standard, 0.0, 1.0, 0.0, false, 0.0f});
        r2.widgets.push_back({gui::WidgetType::Switch, 8, "SubWaveform", "SUB OSC", gui::KnobStyle::Standard, 0.0, 1.0, 0.0, true, 44.0f});
        r2.widgets.push_back({gui::WidgetType::Knob, 9, "SubVolume", "SUB VOL", gui::KnobStyle::CreamFluted, 0.0, 1.0, 0.0, false, 44.0f});
        r2.widgets.push_back({gui::WidgetType::Divider, 0, "", "", gui::KnobStyle::Standard, 0.0, 1.0, 0.0, false, 0.0f});
        r2.widgets.push_back({gui::WidgetType::Knob, 11, "GlideCurve", "GLIDE CURVE", gui::KnobStyle::CreamFluted, 0.0, 2.0, 0.0, true, 44.0f});
        r2.widgets.push_back({gui::WidgetType::Divider, 0, "", "", gui::KnobStyle::Standard, 0.0, 1.0, 0.0, false, 0.0f});
        r2.widgets.push_back({gui::WidgetType::Knob, 10, "Drive", "DRIVE", gui::KnobStyle::CreamFluted, 0.0, 1.0, 0.25, false, 44.0f});
        d.rows.push_back(std::move(r2));

        return d;
    }

    [[nodiscard]] std::unique_ptr<gui::IPluginGui> createGui(const clap_host* host) noexcept {
        return std::make_unique<gui::PuglBridge>(this, host, getGuiDesc(), 800, 360);
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

    [[nodiscard]] double getParamValue(uint32_t id) const noexcept override {
        switch (id) {
            case 0: return waveform_.load(std::memory_order_relaxed);
            case 1: return pitch_.load(std::memory_order_relaxed);
            case 2: return cutoffHz_.load(std::memory_order_relaxed);
            case 3: return resonance_.load(std::memory_order_relaxed);
            case 4: return envMod_.load(std::memory_order_relaxed);
            case 5: return decay_.load(std::memory_order_relaxed);
            case 6: return accent_.load(std::memory_order_relaxed);
            case 7: return octave_.load(std::memory_order_relaxed);
            case 8: return subWaveform_.load(std::memory_order_relaxed);
            case 9: return subVolume_.load(std::memory_order_relaxed);
            case 10: return drive_.load(std::memory_order_relaxed);
            case 11: return glideCurve_.load(std::memory_order_relaxed);
            default: return 0.0;
        }
    }

    void setParamValue(uint32_t id, double val) noexcept override {
        switch (id) {
            case 0: {
                const float w = static_cast<float>(std::clamp(val, 0.0, 1.0));
                waveform_.store(w, std::memory_order_relaxed);
                tb303_.setWaveform(w);
                break;
            }
            case 1: {
                const float p = static_cast<float>(std::clamp(val, -12.0, 12.0));
                pitch_.store(p, std::memory_order_relaxed);
                tb303_.setPitch(p);
                break;
            }
            case 2: {
                const float c = static_cast<float>(std::clamp(val, 200.0, 4500.0));
                cutoffHz_.store(c, std::memory_order_relaxed);
                tb303_.setCutoff(c);
                break;
            }
            case 3: {
                const float r = static_cast<float>(std::clamp(val, 0.5, 16.0));
                resonance_.store(r, std::memory_order_relaxed);
                tb303_.setResonance(r);
                break;
            }
            case 4: {
                const float e = static_cast<float>(std::clamp(val, 0.0, 1.0));
                envMod_.store(e, std::memory_order_relaxed);
                tb303_.setEnvMod(e);
                break;
            }
            case 5: {
                const float d = static_cast<float>(std::clamp(val, 0.05, 1.2));
                decay_.store(d, std::memory_order_relaxed);
                tb303_.setDecay(d);
                break;
            }
            case 6: {
                const float a = static_cast<float>(std::clamp(val, 0.0, 1.0));
                accent_.store(a, std::memory_order_relaxed);
                tb303_.setAccent(a);
                break;
            }
            case 7: {
                const int o = static_cast<int>(std::round(std::clamp(val, -2.0, 0.0)));
                octave_.store(static_cast<float>(o), std::memory_order_relaxed);
                tb303_.setOctave(o);
                break;
            }
            case 8: {
                const float sw = static_cast<float>(std::clamp(val, 0.0, 1.0));
                subWaveform_.store(sw, std::memory_order_relaxed);
                tb303_.setSubWaveform(sw);
                break;
            }
            case 9: {
                const float sv = static_cast<float>(std::clamp(val, 0.0, 1.0));
                subVolume_.store(sv, std::memory_order_relaxed);
                tb303_.setSubVolume(sv);
                break;
            }
            case 10: {
                const float dr = static_cast<float>(std::clamp(val, 0.0, 1.0));
                drive_.store(dr, std::memory_order_relaxed);
                tb303_.setDrive(dr);
                break;
            }
            case 11: {
                const float gc = static_cast<float>(std::clamp(val, 0.0, 2.0));
                glideCurve_.store(gc, std::memory_order_relaxed);
                tb303_.setGlideCurve(gc);
                break;
            }
            default:
                break;
        }
    }

    bool valueToText(uint32_t id, double val, char* buf, uint32_t cap) const noexcept override {
        if (!buf || cap == 0) return false;
        switch (id) {
            case 0:
                std::snprintf(buf, cap, "%s", (val < 0.5) ? "Saw" : "Square");
                return true;
            case 1:
                std::snprintf(buf, cap, "%+.0f st", val);
                return true;
            case 2:
                std::snprintf(buf, cap, "%.0f Hz", val);
                return true;
            case 3:
                std::snprintf(buf, cap, "%.1f", val);
                return true;
            case 4:
                std::snprintf(buf, cap, "%.0f %%", val * 100.0);
                return true;
            case 5:
                std::snprintf(buf, cap, "%.2f s", val);
                return true;
            case 6:
                std::snprintf(buf, cap, "%.0f %%", val * 100.0);
                return true;
            case 7:
                std::snprintf(buf, cap, "%.0f oct", val);
                return true;
            case 8:
                std::snprintf(buf, cap, "%s", (val < 0.5) ? "Sine" : "Square");
                return true;
            case 9:
                std::snprintf(buf, cap, "%.0f %%", val * 100.0);
                return true;
            case 10:
                std::snprintf(buf, cap, "%.0f %%", val * 100.0);
                return true;
            case 11: {
                const char* desc = (val < 0.5) ? "Exp" : ((val < 1.5) ? "Lin" : "S-Curve");
                std::snprintf(buf, cap, "%s", desc);
                return true;
            }
            default:
                std::snprintf(buf, cap, "%.2f", val);
                return true;
        }
    }

    bool textToValue(clap_id id, const char* text, double* out) const noexcept {
        if (!text || !out) return false;
        if (id == 0) {
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
            const float s = tb303_.processSample();
            outL[i] = s;
            outR[i] = s;
        }
    }

    dsp::Tb303Core& core() noexcept { return tb303_; }
    const dsp::Tb303Core& core() const noexcept { return tb303_; }

private:
    static const std::array<ClapParamDef, 12>& paramDefs() noexcept {
        static const std::array<ClapParamDef, 12> s_defs = {{
            {0, "Waveform", "Oscillator", 0.0, 1.0, 0.0, CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_AUTOMATABLE, true, 0},
            {1, "Pitch", "Oscillator", -12.0, 12.0, 0.0, CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_AUTOMATABLE, true, 1},
            {2, "Cutoff", "Filter", 200.0, 4500.0, 1400.0, CLAP_PARAM_IS_AUTOMATABLE, true, 2},
            {3, "Resonance", "Filter", 0.5, 16.0, 9.2, CLAP_PARAM_IS_AUTOMATABLE, true, 3},
            {4, "EnvMod", "Filter", 0.0, 1.0, 0.75, CLAP_PARAM_IS_AUTOMATABLE, true, 4},
            {5, "Decay", "Envelope", 0.05, 1.2, 0.28, CLAP_PARAM_IS_AUTOMATABLE, true, 5},
            {6, "Accent", "Filter", 0.0, 1.0, 0.78, CLAP_PARAM_IS_AUTOMATABLE, true, 6},
            {7, "Octave", "Oscillator", -2.0, 0.0, 0.0, CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_AUTOMATABLE, true, 7},
            {8, "SubWaveform", "SubOsc", 0.0, 1.0, 0.0, CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_AUTOMATABLE, false, 8},
            {9, "SubVolume", "SubOsc", 0.0, 1.0, 0.0, CLAP_PARAM_IS_AUTOMATABLE, false, 9},
            {10, "Drive", "Distortion", 0.0, 1.0, 0.25, CLAP_PARAM_IS_AUTOMATABLE, false, 10},
            {11, "GlideCurve", "Envelope", 0.0, 2.0, 0.0, CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_AUTOMATABLE, false, 11}
        }};
        return s_defs;
    }

    dsp::Tb303Core tb303_;
    std::atomic<float> waveform_{0.0f};
    std::atomic<float> pitch_{0.0f};
    std::atomic<float> cutoffHz_{1400.0f};
    std::atomic<float> resonance_{9.2f};
    std::atomic<float> envMod_{0.75f};
    std::atomic<float> decay_{0.28f};
    std::atomic<float> accent_{0.78f};
    std::atomic<float> octave_{0.0f};
    std::atomic<float> subWaveform_{0.0f};
    std::atomic<float> subVolume_{0.0f};
    std::atomic<float> drive_{0.25f};
    std::atomic<float> glideCurve_{0.0f};
};

// Aliases for compatibility
using Tb303ClapAdapter = Eats303ClapAdapter;

} // namespace eatsbits::export_clap

#endif // EATS_TB303_CLAP_ADAPTER_HPP
