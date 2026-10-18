#pragma once

#include "../geometry.hpp"
#include "../views/view_base.hpp"
#include "plugin_search_dialog.hpp"
#include "color_picker_dialog.hpp"
#include "../gui_panel_def.hpp"
#include "eatsbits/theory/chord_model.hpp"
#include "eatsbits/project/preset_loader.hpp"
#include "eatsbits/ui/clay/clay_context.hpp"
#include "eatsbits/ui/clay/components.hpp"

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <algorithm>
#include <cmath>
#include <memory>
#include <chrono>

namespace eatsbits::ui {

using ChordFollowMode = eatsbits::theory::ChordFollowMode;

enum class TrackPropertiesTab {
    Track,
    Clip,
    Master
};

struct TrackPropertiesKnob {
    std::string name;
    std::string label;
    float value{0.5f};
    std::string display{"50%"};
    std::string unit{""};
};

using InspectorKnobDef = TrackPropertiesKnob;

struct TrackMidiFxItem {
    std::string id{"scale_snap"};
    std::string name{"Scale Snap"};
    std::string type{"SCALE_SNAP"};
    int rootKey{0};
    int scaleMode{0}; // 0 = Major, 1 = Natural Minor, etc.
    bool enabled{true};
    bool isExpanded{true};
    std::string background{"dark"};
    float accentR{1.0f}, accentG{0.75f}, accentB{0.2f};
    std::vector<TrackPropertiesKnob> knobs;
    std::optional<GuiPanelDef> customGuiPanel;

    TrackMidiFxItem() = default;
    TrackMidiFxItem(std::string n, std::string t, bool en = true)
        : id(t), name(std::move(n)), type(std::move(t)), enabled(en) {
        ensureDefaultKnobs();
    }
    TrackMidiFxItem(std::string n, std::string t, int rk, int sm, bool en = true)
        : id(t), name(std::move(n)), type(std::move(t)), rootKey(rk), scaleMode(sm), enabled(en) {
        ensureDefaultKnobs();
    }

    void ensureDefaultKnobs() {
        if (!knobs.empty()) return;
        std::string tUpper = type + " " + name;
        for (char& c : tUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (tUpper.find("ARP") != std::string::npos) {
            knobs = {
                {"rate", "RATE", 0.5f, "1/16", ""},
                {"pattern", "PATTERN", 0.0f, "UP", ""},
                {"octaves", "OCTAVES", 0.33f, "2", "oct"},
                {"gate", "GATE", 0.75f, "75%", ""}
            };
        } else if (tUpper.find("HUMANIZE") != std::string::npos || tUpper.find("GROOVE") != std::string::npos) {
            knobs = {
                {"timing", "TIMING", 0.25f, "15 ms", "ms"},
                {"velocity", "VELOCITY", 0.35f, "20%", "%"},
                {"swing", "SWING", 0.50f, "50%", "%"},
                {"chance", "CHANCE", 1.0f, "100%", "%"}
            };
        } else if (tUpper.find("TRANSPOSE") != std::string::npos || tUpper.find("PITCH") != std::string::npos) {
            knobs = {
                {"semitones", "SEMITONES", 0.5f, "0 st", "st"},
                {"octaves", "OCTAVES", 0.5f, "0 oct", "oct"},
                {"fine", "FINE", 0.5f, "0 ct", "ct"}
            };
        } else if (tUpper.find("CHORD") != std::string::npos || tUpper.find("VOICING") != std::string::npos || tUpper.find("STAB") != std::string::npos) {
            knobs = {
                {"voicing", "VOICING", 0.0f, "Triad", ""},
                {"inversion", "INVERSION", 0.0f, "Root", ""},
                {"spread", "SPREAD", 0.0f, "Close", ""},
                {"velocity", "VELOCITY", 0.85f, "85%", "%"}
            };
        } else { // SCALE_SNAP / generic
            knobs = {
                {"root", "ROOT", static_cast<float>(rootKey) / 11.0f, "C", ""},
                {"scale", "SCALE", static_cast<float>(scaleMode) / 6.0f, "Minor", ""},
                {"snap", "SNAP", 1.0f, "100%", "%"},
                {"transpose", "TRANS", 0.5f, "0 st", "st"}
            };
        }
    }
};

struct TrackAudioFxItem {
    std::string id{"bitcrusher"};
    std::string name{"8-Bit Crusher"};
    std::string type{"BITCRUSHER"};
    float drive{0.5f};
    float mix{0.8f};
    bool enabled{true};
    bool isExpanded{true};
    std::string background{"snes"}; // "snes", "dark", "grunge", "silver", "minimal_white"
    float accentR{0.13f}, accentG{0.75f}, accentB{1.0f};
    std::vector<TrackPropertiesKnob> knobs;
    std::optional<GuiPanelDef> customGuiPanel;

    TrackAudioFxItem() = default;
    TrackAudioFxItem(std::string n, std::string t, float d = 0.5f, float m = 0.8f, bool en = true)
        : id(t), name(std::move(n)), type(std::move(t)), drive(d), mix(m), enabled(en) {
        ensureDefaultKnobs();
    }

    void ensureDefaultKnobs() {
        if (!knobs.empty()) return;
        std::string tUpper = type + " " + name;
        for (char& c : tUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (tUpper.find("CRUSH") != std::string::npos || tUpper.find("8-BIT") != std::string::npos || tUpper.find("BITCRUSHER") != std::string::npos) {
            background = "snes";
            knobs = {
                {"Bits", "BITS", (8.0f - 1.0f) / 15.0f, "8 bit", "bit"},
                {"Downsample", "CRUSH", 0.0f, "1x", "x"},
                {"Drive", "DRIVE", (1.0f - 0.5f) / 3.5f, "1.0 x", "x"},
                {"Mix", "MIX", mix, "1.0", ""}
            };
        } else if (tUpper.find("CAB") != std::string::npos) {
            background = "grunge";
            knobs = {
                {"Width", "WIDTH", 0.5f, "0.76 m", "m"},
                {"Length", "LENGTH", 0.5f, "0.76 m", "m"},
                {"DryLevel", "DRY", 0.0f, "0.0", ""},
                {"WetLevel", "WET", 0.8f, "1.0", ""}
            };
        } else if (tUpper.find("DELAY") != std::string::npos || tUpper.find("ECHO") != std::string::npos) {
            background = "silver";
            knobs = {
                {"time", "TIME", drive, "375 ms", "ms"},
                {"feedback", "FEEDBK", 0.45f, "45%", "%"},
                {"filter", "DAMP", 0.30f, "3.2 kHz", "kHz"},
                {"mix", "MIX", mix, "30%", "%"}
            };
        } else if (tUpper.find("CHORUS") != std::string::npos || tUpper.find("FLANGER") != std::string::npos) {
            background = "snes";
            knobs = {
                {"rate", "RATE", 0.35f, "1.2 Hz", "Hz"},
                {"depth", "DEPTH", drive, "40%", "%"},
                {"feedback", "FEEDBK", 0.20f, "20%", "%"},
                {"mix", "MIX", mix, "35%", "%"}
            };
        } else if (tUpper.find("CONVOLVER") != std::string::npos || tUpper.find("REVERB") != std::string::npos) {
            background = "dark";
            knobs = {
                {"size", "SIZE", drive, "Medium", ""},
                {"decay", "DECAY", 0.5f, "2.4 s", "s"},
                {"predelay", "PREDLY", 0.2f, "25 ms", "ms"},
                {"mix", "MIX", mix, "35%", "%"}
            };
        } else if (tUpper.find("COMP") != std::string::npos || tUpper.find("DYNAMICS") != std::string::npos) {
            background = "dark";
            knobs = {
                {"thresh", "THRESH", 0.65f, "-18 dB", "dB"},
                {"ratio", "RATIO", 0.45f, "4:1", ""},
                {"attack", "ATTACK", 0.20f, "12 ms", "ms"},
                {"gain", "GAIN", mix, "+2.5 dB", "dB"}
            };
        } else if (tUpper.find("EQ") != std::string::npos || tUpper.find("PARAMETRIC") != std::string::npos || tUpper.find("EQUALIZER") != std::string::npos) {
            background = "dark";
            accentR = 0.0f; accentG = 0.90f; accentB = 1.0f;
            knobs = {
                {"low", "LOW", 0.5f, "0.0 dB", "dB"},
                {"mid", "MID", 0.5f, "0.0 dB", "dB"},
                {"high", "HIGH", 0.5f, "0.0 dB", "dB"},
                {"q", "Q", 0.35f, "1.2", "Q"},
                {"gain", "GAIN", 0.5f, "0.0 dB", "dB"}
            };
        } else if (tUpper.find("LIMIT") != std::string::npos) {
            background = "silver";
            accentR = 1.0f; accentG = 0.20f; accentB = 0.40f;
            knobs = {
                {"ceiling", "CEIL", 0.95f, "-0.1 dB", "dB"},
                {"release", "RELEASE", 0.25f, "50 ms", "ms"},
                {"gain", "GAIN", 0.5f, "0.0 dB", "dB"}
            };
        } else if (tUpper.find("FILTER") != std::string::npos || tUpper.find("SVF") != std::string::npos || tUpper.find("LOWPASS") != std::string::npos || tUpper.find("HIGHPASS") != std::string::npos || tUpper.find("VCF") != std::string::npos) {
            background = "carbon";
            accentR = 1.0f; accentG = 0.0f; accentB = 0.48f;
            knobs = {
                {"cutoff", "CUTOFF", 0.70f, "2.4 kHz", "Hz"},
                {"reso", "RESO", 0.30f, "1.5", "Q"},
                {"type", "TYPE", 0.0f, "LP", ""},
                {"drive", "DRIVE", drive, "1.0x", "x"}
            };
        } else if (tUpper.find("CLAP") != std::string::npos || id.rfind("clap:", 0) == 0) {
            background = "silver";
            accentR = 0.85f; accentG = 0.25f; accentB = 0.95f;
            knobs = {
                {"param1", "PARAM 1", drive, "50%", "%"},
                {"param2", "PARAM 2", 0.50f, "50%", "%"},
                {"mix", "DRY/WET", mix, "80%", "%"},
                {"output", "OUTPUT", 0.80f, "0.0 dB", "dB"}
            };
        } else { // DISTORTION / TUBE_DISTORTION / SHAPER / generic
            background = "grunge";
            accentR = 0.95f; accentG = 0.60f; accentB = 0.10f;
            knobs = {
                {"drive", "DRIVE", drive, "50%", "%"},
                {"tone", "TONE", 0.60f, "60%", "%"},
                {"bias", "BIAS", 0.40f, "40%", "%"},
                {"mix", "MIX", mix, "80%", "%"}
            };
        }
    }

    void populateFromPresetDefinition(const eatsbits::project::PresetDefinition& def) {
        if (!def.metadata.id.empty()) id = def.metadata.id;
        if (!def.metadata.name.empty()) name = def.metadata.name;
        if (!def.metadata.engineId.empty()) type = def.metadata.engineId;

        if (!def.guiRoot.background.empty()) {
            background = def.guiRoot.background;
        }
        if (!def.guiRoot.accent.empty() && def.guiRoot.accent[0] == '#' && def.guiRoot.accent.size() >= 7) {
            try {
                unsigned int hexVal = std::stoul(def.guiRoot.accent.substr(1, 6), nullptr, 16);
                accentR = static_cast<float>((hexVal >> 16) & 0xFF) / 255.0f;
                accentG = static_cast<float>((hexVal >> 8) & 0xFF) / 255.0f;
                accentB = static_cast<float>(hexVal & 0xFF) / 255.0f;
            } catch (...) {}
        }

        std::vector<TrackPropertiesKnob> extractedKnobs;
        auto collectKnobs = [&](auto& self, const eatsbits::project::GuiLayoutNode& node) -> void {
            if (node.type == eatsbits::project::GuiNodeType::Knob || node.type == eatsbits::project::GuiNodeType::Slider) {
                TrackPropertiesKnob k;
                k.name = node.paramName.empty() ? node.label : node.paramName;
                k.label = node.label.empty() ? node.paramName : node.label;
                k.unit = node.unit;

                const auto* param = def.findParam(node.paramName);
                if (param) {
                    float range = param->maxVal - param->minVal;
                    k.value = (range > 0.0001f) ? std::clamp((param->defaultVal - param->minVal) / range, 0.0f, 1.0f) : 0.5f;
                    k.display = param->getFormatted();
                    if (k.unit.empty()) k.unit = param->unit;
                } else {
                    k.value = 0.5f;
                    k.display = "0.5";
                }
                extractedKnobs.push_back(k);
            }
            for (const auto& child : node.children) {
                self(self, child);
            }
        };
        collectKnobs(collectKnobs, def.guiRoot);

        if (!extractedKnobs.empty()) {
            knobs = std::move(extractedKnobs);
        } else {
            ensureDefaultKnobs();
        }
    }
};

struct TrackMidiFxData {
    bool arpEnabled{true};
    int arpPattern{0};    // 0: Up, 1: Down, 2: UpDown, 3: Random, 4: Chord
    float arpRate{1.0f};  // 1.0 = 16th, 0.5 = 8th, 2.0 = 32nd
    int arpOctaves{2};
    float arpGate{0.85f};
    float arpSwing{0.0f};

    bool scaleSnapEnabled{true};
    int scaleRoot{0};     // 0 = C
    bool scaleMinor{true};

    bool humanizeEnabled{true};
    float humanizeTiming{0.15f};    // ±15ms
    float humanizeVelocity{0.20f};  // ±20%
};

struct TrackAudioFxData {
    bool delayEnabled{true};
    float delayTime{0.375f};
    float delayFeedback{0.45f};
    float delayMix{0.30f};

    bool chorusEnabled{false};
    float chorusRate{0.5f};
    float chorusDepth{0.4f};
    float chorusMix{0.35f};

    bool eqEnabled{true};
    float eqLow{0.6f};
    float eqMid{0.5f};
    float eqHigh{0.5f};

    bool compEnabled{true};
    float compThreshold{0.7f};
    float compRatio{0.4f};
    float compGain{0.5f};

    bool convolverEnabled{true};
    int convolverPreset{0}; // 0: Stone Cathedral, 1: Vintage Plate, 2: Warm Hall, etc.
    float convolverMix{0.35f};
};

struct TrackPropertiesDrawerData {
    // Mode
    TrackPropertiesTab tab{TrackPropertiesTab::Track};
    bool isMixerMode{false};
    bool isMasterSelected{false};

    // Track Identity
    uint32_t trackIndex{0};
    uint32_t totalTracks{1};
    std::string trackName{"Track"};
    std::string trackType{"SYNTH"};
    std::string iconRef{""};
    float r{0.0f}, g{0.90f}, b{1.0f};

    // Mix Parameters
    float volume{0.8f};
    float pan{0.0f};
    bool mute{false};
    bool solo{false};
    bool freeze{false};

    // 3-Band Parametric Channel EQ (Mixer specific)
    bool eqEnabled{true};
    float eqHpf{20.0f};        // 20..500 Hz
    float eqLowGain{0.0f};     // -18..+18 dB
    float eqMidFreq{1000.0f};  // 200..8000 Hz
    float eqMidGain{0.0f};     // -18..+18 dB
    float eqMidQ{1.0f};        // 0.3..10.0
    float eqHighGain{0.0f};    // -18..+18 dB

    // Dynamic Instrument Parameters
    std::string instrument{""};
    std::string instrumentEngine{""}; // "tb303", "tr808", "tr909", "dx7", "piano", "snes", "c64", "convolver", "synth"
    std::vector<TrackPropertiesKnob> knobs;
    size_t activePresetIdx{0};
    size_t totalPresets{1};
    std::string presetTitle{""};
    std::string presetSubtitle{""};
    bool instrumentExpanded{true};
    std::optional<GuiPanelDef> customGuiPanel;

    // Harmonic Chord Track Follow
    ChordFollowMode chordFollowMode{ChordFollowMode::Off};

    // Live Oscilloscope Audio Buffer
    const float* scopeBuffer{nullptr};
    size_t scopeBufferCount{0};

    // MIDI & Audio FX Pipeline Data
    TrackMidiFxData midiFxData;
    TrackAudioFxData audioFxData;

    // Legacy fallback parameters
    float knob1{0.55f};
    float knob2{0.75f};
    float knob3{0.45f};
    float knob4{0.80f};
    std::string knob1Name{"TONE"};
    std::string knob2Name{"SNAPPY"};
    std::string knob3Name{"DECAY"};
    std::string knob4Name{"VAR"};

    // Custom MIDI & Audio FX Racks
    std::vector<TrackMidiFxItem> midiFx;
    std::vector<TrackAudioFxItem> audioFx;

    // Master Bus Parameters (Mixer Master Console)
    bool masterEqEnabled{true};
    float masterSubCut{25.0f};    // 20..45 Hz
    float masterLowGain{0.0f};   // -12..+12 dB
    float masterMidGain{0.0f};   // -12..+12 dB
    float masterHighGain{0.0f};  // -12..+12 dB

    bool masterLimiterEnabled{true};
    float masterCeilingDbfs{-0.3f}; // -2.0..0.0 dB
    float masterLimiterDrive{0.0f}; // 0..12 dB
    float masterTargetLufs{-14.0f}; // -24..-6 LUFS

    std::vector<TrackAudioFxItem> masterAudioFx{
        {"Convolver Reverb", "CONVOLVER", 0.35f, 0.40f, true},
        {"Dynamic Limiter", "BRICKWALL_LIMITER", 0.80f, 1.00f, true}
    };

    // Clip Parameters (Arranger Mode)
    int selectedClipIndex{-1};
    std::string clipName{"Clip 1"};
    uint32_t clipStartBar{1};
    uint32_t clipLengthBars{4};
    bool clipLooped{false};
    uint32_t clipLoopLengthBars{4};
    int clipTranspose{0};

    // Multi-track info for top ribbon (when showRibbon is active)
    std::vector<std::string> allTrackNames;
    std::vector<Color> allTrackColors;

    // CLAP Native Plugin Integration
    bool isClapPlugin{false};
    bool hasNativeGui{false};
    bool isGuiOpen{false};

    void setKnobValue(const std::string& paramName, float val, const std::string& disp = "") {
        for (auto& k : knobs) {
            if (stringEqualsIgnoreCase(k.name, paramName) || stringEqualsIgnoreCase(k.label, paramName)) {
                k.value = val;
                if (!disp.empty()) k.display = disp;
                return;
            }
        }
        knobs.push_back({paramName, paramName, val, disp.empty() ? std::to_string(static_cast<int>(std::round(val * 100.0f))) + "%" : disp, ""});
    }

    void syncKnobsIfEmpty() {
        for (auto& fx : audioFx) fx.ensureDefaultKnobs();
        for (auto& fx : midiFx) fx.ensureDefaultKnobs();
        for (auto& fx : masterAudioFx) fx.ensureDefaultKnobs();
        if (instrumentEngine.rfind("clap:", 0) == 0 || instrumentEngine == "CLAP" || instrument.find(".clap") != std::string::npos) {
            isClapPlugin = true;
            hasNativeGui = true;
        }
        if (!knobs.empty()) return;
        if (isClapPlugin || instrumentEngine.rfind("clap:", 0) == 0 || instrumentEngine == "CLAP" || instrument.find(".clap") != std::string::npos) {
            knobs = {
                {"cutoff", "CUTOFF", 0.65f, "65%"},
                {"resonance", "RESONANCE", 0.50f, "50%"},
                {"attack", "ATTACK", 0.20f, "20%"},
                {"decay", "DECAY", 0.60f, "60%"},
                {"sustain", "SUSTAIN", 0.75f, "75%"},
                {"release", "RELEASE", 0.40f, "40%"}
            };
        } else if (instrumentEngine == "tb303" || instrument.find("303") != std::string::npos || presetTitle.find("303") != std::string::npos) {
            knobs = {
                {"waveform", "WAVEFORM", 0.0f, "Saw"},
                {"pitch", "PITCH", 0.50f, "0 st"},
                {"cutoff", "CUTOFF", knob1, std::to_string(static_cast<int>(std::round(knob1 * 100.0f))) + "%"},
                {"resonance", "RESONANCE", knob2, std::to_string(static_cast<int>(std::round(knob2 * 100.0f))) + "%"},
                {"envMod", "ENV MOD", 0.60f, "+2.4 oct"},
                {"decay", "DECAY", knob3, std::to_string(static_cast<int>(std::round(knob3 * 500.0f))) + " ms"},
                {"accent", "ACCENT", knob4, std::to_string(static_cast<int>(std::round(knob4 * 100.0f))) + "%"},
                {"octave", "OCTAVE", 0.50f, "0"},
                {"subOsc", "SUB OSC", 0.0f, "Off"},
                {"subVol", "SUB VOL", 0.35f, "35%"},
                {"glideCurve", "GLIDE CURVE", 0.40f, "40%"},
                {"drive", "DRIVE", 0.25f, "25%"}
            };
        } else if (instrumentEngine == "tr808") {
            knobs = {
                {"tone", "TONE", knob1, std::to_string(static_cast<int>(std::round(knob1 * 100.0f))) + "%"},
                {"snappy", "SNAPPY", knob2, std::to_string(static_cast<int>(std::round(knob2 * 100.0f))) + "%"},
                {"decay", "DECAY", knob3, std::to_string(static_cast<int>(std::round(knob3 * 1000.0f))) + " ms"},
                {"tuning", "TUNING", knob4, "55 Hz"},
                {"drive", "DRIVE", 0.35f, "15%"},
                {"punch", "PUNCH", 0.75f, "+3 dB"}
            };
        } else if (instrumentEngine == "tr909") {
            knobs = {
                {"attack", "ATTACK", knob1, "12 ms"},
                {"punch", "PUNCH", knob2, "+4 dB"},
                {"tune", "TUNE", knob3, "62 Hz"},
                {"crack", "CRACK", knob4, "60%"},
                {"decay", "DECAY", 0.55f, "450 ms"},
                {"snap", "SNAP", 0.75f, "75%"}
            };
        } else if (instrumentEngine == "dx7") {
            knobs = {
                {"algo", "ALGO", 0.35f, "Algo 5"},
                {"feedback", "FEEDBACK", 0.65f, "Lvl 6"},
                {"attack", "ATTACK", knob1, "8 ms"},
                {"decay", "DECAY", knob2, "1.4 s"},
                {"bright", "BRIGHT", knob3, "60%"},
                {"detune", "DETUNE", knob4, "+12 ct"}
            };
        } else if (instrumentEngine == "piano" || instrumentEngine == "piano_physical") {
            knobs = {
                {"stiffness", "STIFF", 0.50f, "50%"},
                {"hammer", "HAMMER", 0.65f, "Hard"},
                {"decay", "DECAY", 0.70f, "2.2 s"},
                {"damping", "DAMP", 0.30f, "30%"},
                {"pedal", "PEDAL", 0.0f, "Off"},
                {"reverb", "REVERB", 0.40f, "40%"}
            };
        } else if (instrumentEngine == "sfxr" || instrument.find("sfxr") != std::string::npos || presetTitle.find("Sfxr") != std::string::npos) {
            knobs = {
                {"SFXType", "SFX TYPE", 0.0f, "Laser"},
                {"Seed", "RNG SEED", 42.0f / 9999.0f, "42"},
                {"Waveform", "WAVETABLE", 1.0f / 11.0f, "Square"},
                {"Attack", "ATTACK", 0.005f / 0.5f, "5 ms"},
                {"Decay", "DECAY", 0.35f / 2.0f, "350 ms"},
                {"Sustain", "SUSTAIN", 0.20f, "20%"},
                {"Release", "RELEASE", 0.20f / 2.0f, "200 ms"},
                {"PitchSweep", "SWEEP", (-0.45f + 2.0f) / 4.0f, "-0.45"},
                {"VibratoRate", "VIB RATE", 0.0f, "0 Hz"},
                {"VibratoDepth", "VIB DEPTH", 0.0f, "0%"},
                {"EchoDelay", "ECHO MS", 120.0f / 480.0f, "120 ms"},
                {"EchoVolume", "ECHO VOL", 0.35f, "35%"}
            };
        } else if (instrumentEngine == "snes" || instrument.find("snes") != std::string::npos || presetTitle.find("SNES") != std::string::npos) {
            knobs = {
                {"Waveform", "WAVETABLE", 0.0f, "Square"},
                {"Attack", "ATTACK", 0.005f / 0.5f, "5 ms"},
                {"Decay", "DECAY", 0.30f / 2.0f, "300 ms"},
                {"Sustain", "SUSTAIN", 0.50f, "50%"},
                {"Release", "RELEASE", 0.25f / 2.0f, "250 ms"},
                {"VibratoRate", "VIB RATE", 5.5f / 20.0f, "5.5 Hz"},
                {"VibratoDepth", "VIB DEPTH", 0.10f, "10%"},
                {"EchoDelay", "ECHO MS", 140.0f / 480.0f, "140 ms"},
                {"EchoFeedback", "ECHO FDBK", 0.55f / 0.90f, "55%"},
                {"EchoVolume", "ECHO VOL", 0.40f, "40%"}
            };
        } else if (instrumentEngine == "clavinet" || instrument.find("clav") != std::string::npos ||
                   instrument.find("Clav") != std::string::npos || presetTitle.find("Clav") != std::string::npos ||
                   presetTitle.find("clav") != std::string::npos || instrument.find("d6") != std::string::npos ||
                   instrument.find("D6") != std::string::npos) {
            knobs = {
                {"Brilliant", "BRILLIANT", 1.0f, "On"},
                {"Treble", "TREBLE", 1.0f, "On"},
                {"Medium", "MEDIUM", 0.0f, "Off"},
                {"Soft", "SOFT", 0.0f, "Off"},
                {"PickupSelect", "PICKUP A/B", 0.5f, "A+B"},
                {"PhaseInvert", "PHASE (A-B)", 0.0f, "In Phase"},
                {"HammerHardness", "HAMMER", 1.4f / 3.0f, "1.4"},
                {"DamperThump", "DAMPER", 0.45f, "45%"},
                {"Drive", "DRIVE", (1.08f - 0.5f) / 2.5f, "1.08x"},
                {"Sustain", "SUSTAIN", (0.993f - 0.8f) / (0.999f - 0.8f), "99.3%"}
            };
        } else if (instrumentEngine == "ambient_pad" || instrumentEngine == "astral_pad" || instrumentEngine == "astral_shimmer_pad" ||
                   instrument.find("astral") != std::string::npos || instrument.find("Astral") != std::string::npos ||
                   presetTitle.find("Astral") != std::string::npos || presetTitle.find("astral") != std::string::npos ||
                   instrument.find("shimmer") != std::string::npos || presetTitle.find("Shimmer") != std::string::npos) {
            knobs = {
                {"Cutoff", "CUTOFF", (2400.0f - 100.0f) / 15900.0f, "2400.0 Hz"},
                {"Resonance", "RESO", (0.55f - 0.1f) / 4.9f, "0.55"},
                {"Detune", "DETUNE", (14.0f - 1.0f) / 34.0f, "14.0 ct"},
                {"Warmth", "WARMTH", 0.65f, "65%"},
                {"Attack", "ATTACK", (0.85f - 0.05f) / 3.95f, "0.85 s"},
                {"Decay", "DECAY", (1.5f - 0.1f) / 5.9f, "1.50 s"},
                {"Sustain", "SUSTAIN", 0.85f, "85%"},
                {"Release", "RELEASE", (2.5f - 0.1f) / 7.9f, "2.50 s"}
            };
        } else {
            knobs = {
                {"param1", knob1Name.empty() ? "PARAM 1" : knob1Name, knob1, std::to_string(static_cast<int>(std::round(knob1 * 100.0f))) + "%"},
                {"param2", knob2Name.empty() ? "PARAM 2" : knob2Name, knob2, std::to_string(static_cast<int>(std::round(knob2 * 100.0f))) + "%"},
                {"param3", knob3Name.empty() ? "PARAM 3" : knob3Name, knob3, std::to_string(static_cast<int>(std::round(knob3 * 100.0f))) + "%"},
                {"param4", knob4Name.empty() ? "PARAM 4" : knob4Name, knob4, std::to_string(static_cast<int>(std::round(knob4 * 100.0f))) + "%"},
                {"param5", "COLOR", 0.50f, "50%"},
                {"param6", "OUTPUT", 0.80f, "80%"}
            };
        }
    }
};

/**
 * Canonical Instrument GUI Panel Builder
 *
 * Populates a GuiPanelDef with the authentic hardware faceplate layout matching
 * Eatsbeats for the given instrument data.
 * Used identically across:
 * - Track Properties sidebar drawer (TrackPropertiesPanel)
 * - Fullscreen Dedicated Device Modal (FullscreenDeviceModal)
 * - Track Tab Hardware Inspector (TrackInspectorView)
 * - Design > GUI Tab Faceplate Preview (DesignView)
 * - Floating Detached Windows (FloatingWindowManager)
 */
void populateCanonicalInstrumentGuiPanel(GuiPanelDef& guiPanel,
                                         const TrackPropertiesDrawerData& data,
                                         int draggingRow = -1,
                                         int draggingWidget = -1);

enum class TrackPropertiesHitArea {
    None,
    PullTab,
    CloseButton,
    TabTrack,
    TabClip,
    MuteButton,
    SoloButton,
    FreezeButton,
    RenameButton,
    TrackIcon,
    DesignButton,
    PresetButton,
    VolumeSlider,
    PanKnob,
    PanSlider = PanKnob,
    CodeButton,
    ColorSwatch,
    CustomColorPicker,
    EqToggle,
    EqReset,
    EqHpf,
    EqLowGain,
    EqHighGain,
    EqMidFreq,
    EqMidGain,
    EqMidQ,
    InstrumentPrevPreset,
    InstrumentNextPreset,
    InstrumentKnob,
    ChangeInstrument,
    FullscreenInstrument,
    DetachInstrumentWindow,
    OpenPluginGui,
    ToggleInstrumentExpand,
    ChordFollowChip,
    BakeChords,
    AddMidiFx,
    AddAudioFx,
    FullscreenMidiFx,
    FullscreenAudioFx,
    ToggleMidiFx,
    ToggleAudioFx,
    ToggleAudioFxExpand,
    MoveAudioFxUp,
    MoveAudioFxDown,
    ToggleMidiFxExpand,
    MoveMidiFxUp,
    MoveMidiFxDown,
    RemoveMidiFx,
    RemoveAudioFx,
    AudioFxKnob,
    MidiFxKnob,
    ReorderLeft,
    ReorderRight,
    MasterEqSubCut,
    MasterEqLow,
    MasterEqMid,
    MasterEqHigh,
    MasterLimiterToggle,
    MasterCeiling,
    MasterDrive,
    MasterLufs,
    AutoMasterButton,
    ClipLoopToggle,
    ClipTransposeUp,
    ClipTransposeDown,
    ClipEditInPianoRoll,
    ClipDuplicate,
    ClipDelete,
    AddClipButton,
    DuplicateTrackButton,
    DeleteTrackButton,
    ScrollbarThumb,
    ScrollbarTrack
};

struct TrackPropertiesHitResult {
    bool hit{false};
    TrackPropertiesHitArea area{TrackPropertiesHitArea::None};
    int index{0};
    float normVal{0.0f};
};

/**
 * TrackPropertiesPanel: Canonical unified component for Track Channel Hardware Faceplate,
 * Mixer Settings, Parametric EQ, Harmonic Chord Follow, and Audio/MIDI FX Pipeline Rack.
 *
 * Shared identically across:
 * 1. Arranger View (Sliding sidebar drawer)
 * 2. Mixer View (Sliding sidebar drawer)
 * 3. Track Tab (Dedicated full-screen workspace)
 */
class TrackPropertiesPanel {
public:
    TrackPropertiesPanel();
    ~TrackPropertiesPanel() = default;

    void layout(const Rect2D& bounds, const ViewContext& ctx);
    void render(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                float mouseX = -1.0f, float mouseY = -1.0f);
    [[nodiscard]] TrackPropertiesHitResult hitTest(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept;
    bool handlePointer(const PointerEvent& ev, TrackPropertiesDrawerData& data, const ViewContext& ctx);
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx);
    bool handleChar(char32_t codepoint) {
        if (pluginDialog_.isOpen()) {
            return pluginDialog_.handleChar(codepoint);
        }
        return false;
    }

    void setShowTrackRibbon(bool show) noexcept { showTrackRibbon_ = show; }
    [[nodiscard]] bool getShowTrackRibbon() const noexcept { return showTrackRibbon_; }

    [[nodiscard]] float getScrollY() const noexcept { return scrollY_; }
    void setScrollY(float sy) noexcept { scrollY_ = std::max(0.0f, sy); }
    [[nodiscard]] float getTotalContentHeight() const noexcept { return totalContentHeight_; }

    [[nodiscard]] const Rect2D& getBounds() const noexcept { return bounds_; }
    [[nodiscard]] const Rect2D& getHeaderCardBounds() const noexcept { return headerCardBounds_; }
    [[nodiscard]] const Rect2D& getFaceplateBounds() const noexcept { return faceplateBounds_; }
    [[nodiscard]] bool isDragging() const noexcept { return dragMode_ != DragMode::None; }

    void syncGuiPanelFromTrackData(const TrackPropertiesDrawerData& data);
    [[nodiscard]] const GuiPanelDef& getGuiPanel() const noexcept { return guiPanel_; }
    [[nodiscard]] GuiPanelDef& getGuiPanel() noexcept { return guiPanel_; }
    [[nodiscard]] std::string getTooltip(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept;
    void setIsInsideDrawer(bool inside) noexcept { isInsideDrawer_ = inside; }
    [[nodiscard]] bool isInsideDrawer() const noexcept { return isInsideDrawer_; }

    // Embedded PluginSearchDialog modal overlay
    [[nodiscard]] PluginSearchDialog& getPluginSearchDialog() noexcept { return pluginDialog_; }
    [[nodiscard]] const PluginSearchDialog& getPluginSearchDialog() const noexcept { return pluginDialog_; }

    // Embedded ColorPickerDialog modal overlay
    [[nodiscard]] bool isColorPickerOpen() const noexcept { return colorPickerDialog_.isOpen(); }
    [[nodiscard]] ColorPickerDialog& getColorPickerDialog() noexcept { return colorPickerDialog_; }
    [[nodiscard]] const ColorPickerDialog& getColorPickerDialog() const noexcept { return colorPickerDialog_; }

    // Synchronized Callbacks
    std::function<void(uint32_t trackIndex)> onTrackSelected;
    std::function<void(uint32_t trackIndex)> onTrackRename;
    std::function<void(uint32_t trackIndex, const std::string& newName)> onTrackRenameWithText;
    std::function<void(uint32_t trackIndex)> onChooseTrackIcon;
    std::function<void(uint32_t trackIndex, float volume)> onVolumeChanged;
    std::function<void(uint32_t trackIndex, float pan)> onPanChanged;
    std::function<void(uint32_t trackIndex, bool mute)> onMuteToggled;
    std::function<void(uint32_t trackIndex, bool solo)> onSoloToggled;
    std::function<void(uint32_t trackIndex, bool freeze)> onFreezeToggled;
    std::function<void(uint32_t trackIndex, float r, float g, float b)> onColorChanged;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onParamChanged;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onMidiFxParamChanged;
    std::function<void(uint32_t trackIndex, size_t fxIndex, const std::string& paramName, float normVal)> onAudioFxParamChanged;
    std::function<void(uint32_t trackIndex, uint8_t note, float velocity)> onAuditionNote;
    std::function<void(uint32_t trackIndex)> onChangeInstrument;
    std::function<void(uint32_t trackIndex)> onOpenDesign;
    std::function<void(uint32_t trackIndex)> onOpenPresets;
    std::function<void()> onPrevPreset;
    std::function<void()> onNextPreset;
    std::function<void(uint32_t trackIndex, ChordFollowMode mode)> onChordFollowChanged;
    std::function<void(uint32_t trackIndex)> onBakeChords;
    std::function<void(TrackPropertiesTab newTab)> onTabSelected;
    std::function<void(uint32_t trackIndex, int clipIndex)> onEditInPianoRoll;
    std::function<void(uint32_t trackIndex)> onOpenCodeEditor;
    std::function<void(uint32_t trackIndex)> onOpenFullscreenDevice;
    std::function<void(uint32_t trackIndex)> onOpenDetachedWindow;
    std::function<void(uint32_t trackIndex)> onOpenPluginGui;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onOpenFullscreenAudioFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onOpenFullscreenMidiFx;
    std::function<void(uint32_t trackIndex)> onAddMidiFx;
    std::function<void(uint32_t trackIndex)> onAddAudioFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onRemoveMidiFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onRemoveAudioFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex, bool enabled)> onToggleMidiFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex, bool enabled)> onToggleAudioFx;
    std::function<void(uint32_t trackIndex, size_t fromIdx, size_t toIdx)> onReorderMidiFx;
    std::function<void(uint32_t trackIndex, size_t fromIdx, size_t toIdx)> onReorderAudioFx;
    std::function<void(uint32_t trackIndex)> onMidiFxChanged;
    std::function<void(uint32_t trackIndex)> onAudioFxChanged;
    std::function<void(float scrollY)> onScrollChanged;
    std::function<void(uint32_t trackIndex)> onAddClip;
    std::function<void(uint32_t trackIndex)> onDeleteTrack;
    std::function<void(uint32_t trackIndex)> onDuplicateTrack;
    std::function<void(uint32_t trackIndex, int clipIndex)> onDuplicateClip;
    std::function<void(uint32_t trackIndex, int clipIndex)> onDeleteClip;

    bool executeHitAction(const TrackPropertiesHitResult& hit, TrackPropertiesDrawerData& data, const ViewContext& ctx);

private:
    void renderTrackSelectorRibbon(BatchRenderer2D& r, const ThemeTokens& theme, const TrackPropertiesDrawerData& data);
    void renderHeaderCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                          float cx, float cy, float cw, bool isWide, float mouseX = -1.0f, float mouseY = -1.0f);
    void renderMixerControlsCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                                 float cx, float cy, float cw, bool isWide);
    void renderEqCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                      float cx, float cy, float cw);
    void renderFaceplateCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                             float cx, float cy, float cw, bool isWide);
    void renderChordFollowCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                               float cx, float cy, float cw);
    void renderMidiFxCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                          float cx, float cy, float cw, bool isWide);
    void renderAudioFxCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                           float cx, float cy, float cw, bool isWide);
    void renderTrackActionsCard(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                                float cx, float cy, float cw, bool isWide, float mouseX = -1.0f, float mouseY = -1.0f);
    void renderColorPalette(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                            float cx, float cy, float cw);
    void renderMasterSection(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                             float cx, float cy, float cw, float mouseX, float mouseY);
    void renderClipSection(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                           float cx, float cy, float cw, float mouseX, float mouseY);
    void renderScrollbar(BatchRenderer2D& r, const ThemeTokens& theme);
    bool openValueEditForHit(const TrackPropertiesHitResult& hit, TrackPropertiesDrawerData& data,
                             const std::function<void(const ValueEditRequest&)>& onOpenValueEdit);
    void update(float dt) noexcept;

    enum class DragMode {
        None,
        VolumeSlider,
        PanKnob,
        PanSlider = PanKnob,
        InstrumentKnob,
        EqKnob,
        AudioFxKnob,
        MidiFxKnob,
        Scrollbar,
        TouchScroll
    } dragMode_{DragMode::None};

    KineticScroller scroller_;
    float touchStartY_{0.0f};
    float touchStartX_{0.0f};
    float touchStartScrollY_{0.0f};
    bool touchDragCommitted_{false};
    double touchStartTimeMs_{0.0};
    TrackPropertiesHitResult pendingHitResult_;

    // Mobile Long-Press / Touch State
    bool isLongPressActive_{false};
    float longPressTimer_{0.0f};
    std::chrono::steady_clock::time_point touchDownTimePoint_{};
    Point2D longPressPos_{0.0f, 0.0f};
    TrackPropertiesHitResult longPressHit_{};
    std::function<void(const ValueEditRequest& req)> longPressOpenValueEdit_;

    bool showTrackRibbon_{false};
    Rect2D bounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D ribbonBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D scrollbarBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    float scrollY_{0.0f};
    float totalContentHeight_{850.0f};
    bool needScrollbar_{false};

    // Sub-section layout bounds for hit-testing
    Rect2D headerCardBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D colorCardBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D mixerCardBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D eqCardBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D faceplateBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D chordFollowBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D midiFxBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D audioFxBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D colorPaletteBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D trackActionsBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D headerMuteBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D headerSoloBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Active drag interaction state
    float dragStartY_{0.0f};
    float dragStartX_{0.0f};
    float dragStartVal_{0.0f};
    float dragStartScrollY_{0.0f};
    int activeKnobIndex_{-1};
    std::string activeParamName_{""};
    size_t activeFxIndex_{0};
    std::string activeFxParam_{""};

    GuiPanelDef guiPanel_;
    int draggingRow_{-1};
    int draggingWidget_{-1};
    std::string focusedWidgetId_{""};
    uint32_t lastTrackIndex_{0xFFFFFFFF};
    std::string lastInstrumentEngine_;
    std::string lastInstrument_;

    void selectListBoxOption(GuiWidgetDef& w, int newIdx, TrackPropertiesDrawerData& data);

    PluginSearchDialog pluginDialog_;
    ColorPickerDialog colorPickerDialog_;
    bool isInsideDrawer_{false};
    std::unique_ptr<clay::ClayContext> clayContext_;

    std::chrono::steady_clock::time_point lastClickTime_{};
    float lastClickX_{0.0f};
    float lastClickY_{0.0f};
    TrackPropertiesHitArea lastClickArea_{TrackPropertiesHitArea::None};
};

} // namespace eatsbits::ui
