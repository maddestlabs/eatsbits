#include "eatsbits/ui/views/design_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/project/preset_loader.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace eatsbits::ui {

DesignView::DesignView() {
    initDefaultTargetsAndCode();

    // Default patch cords (preserving unit test invariants)
    patchCords_.push_back({0, 0, 1, 0, 1.0f, 0.55f, 0.0f});
    patchCords_.push_back({1, 1, 2, 0, 0.13f, 0.96f, 0.91f});

    // Default Eurorack Modular rack modules
    modules_ = {
        {"OSC 1: TB-303 CORE", "OSC", 24.0f, 80.0f, 230.0f, 320.0f, 0.20f, 0.21f, 0.24f,
         {{"TUNE", 0.50f, -24.0f, 24.0f, "st"}, {"WAVE", 0.0f, 0.0f, 1.0f, ""}, {"GLIDE", 0.35f, 0.0f, 1.0f, "s"}},
         {"CV IN", "GATE"}, {"OUT", "SUB"}},

        {"FILTER: 24dB DIODE", "VCF", 274.0f, 80.0f, 230.0f, 320.0f, 0.22f, 0.23f, 0.26f,
         {{"CUTOFF", 0.65f, 20.0f, 20000.0f, "Hz"}, {"RESON", 0.78f, 0.0f, 1.0f, ""}, {"ENV MOD", 0.60f, 0.0f, 1.0f, ""}, {"ACCENT", 0.70f, 0.0f, 1.0f, ""}},
         {"AUDIO IN", "FM IN"}, {"LPF OUT", "ENV OUT"}},

        {"VCA & SATURATION", "VCA", 524.0f, 80.0f, 210.0f, 320.0f, 0.19f, 0.20f, 0.23f,
         {{"DRIVE", 0.45f, 1.0f, 10.0f, "x"}, {"ATTACK", 0.05f, 0.001f, 1.0f, "s"}, {"DECAY", 0.45f, 0.01f, 2.0f, "s"}},
         {"IN", "CV"}, {"OUT", "MON"}},

        {"OUT: MASTER BUS", "OUT", 754.0f, 80.0f, 210.0f, 320.0f, 0.18f, 0.19f, 0.22f,
         {{"VOLUME", 0.85f, 0.0f, 1.0f, ""}, {"PAN", 0.0f, -1.0f, 1.0f, ""}, {"LIMIT", 0.90f, 0.0f, 1.0f, ""}},
         {"L IN", "R IN"}, {"MAIN L", "MAIN R"}}
    };

    // Synthesize scope buffer with animated waveform
    scopeSamples_.resize(128, 0.0f);
    for (size_t i = 0; i < scopeSamples_.size(); ++i) {
        float ph = static_cast<float>(i) / static_cast<float>(scopeSamples_.size()) * 6.2831853f * 2.0f;
        scopeSamples_[i] = std::sin(ph) * 0.75f + std::sin(ph * 2.0f) * 0.25f;
    }

    initDefaultGuiPanel();

    textEditor_.onCompileTriggered = [this]() {
        ViewContext dummyCtx;
        compileCurrentScript(dummyCtx);
    };
    textEditor_.onTextChanged = [this](const std::string& code) {
        currentScriptCode_ = code;
    };
    textEditor_.setText(currentScriptCode_);
}

void DesignView::setSubMode(DesignSubMode mode) noexcept {
    mode_ = mode;
}

void DesignView::resetPatch() {
    patchCords_.clear();
    patchCords_.push_back({0, 0, 1, 0, 1.0f, 0.55f, 0.0f});
    patchCords_.push_back({1, 1, 2, 0, 0.13f, 0.96f, 0.91f});
}

void DesignView::addModule(const ModularModuleDef& mod) {
    modules_.push_back(mod);
}

void DesignView::initDefaultTargetsAndCode() {
    allTargets_.clear();

    // 1. Synths & DSP (4 tracks)
    allTargets_.push_back({"eats_kick", "Eats Kick (Synth DSP)", "Procedural Sub Kick Drum", ScriptTargetType::TrackDsp, "SYNTH DSP", {0.0f, 0.95f, 1.0f}, {1.0f, 0.55f, 0.0f}, 0, -1});
    allTargets_.push_back({"eats_snare", "Eats Snare (Synth DSP)", "Analog Filtered Noise Snare", ScriptTargetType::TrackDsp, "SYNTH DSP", {0.0f, 0.95f, 1.0f}, {1.0f, 0.85f, 0.0f}, 1, -1});
    allTargets_.push_back({"eats_hats", "Eats Hats (Synth DSP)", "Metallic Phase Hi-Hat", ScriptTargetType::TrackDsp, "SYNTH DSP", {0.0f, 0.95f, 1.0f}, {0.13f, 0.96f, 0.91f}, 2, -1});
    allTargets_.push_back({"eats_303", "Eats-303 (Synth DSP)", "Acid Diode Ladder Synth", ScriptTargetType::TrackDsp, "SYNTH DSP", {0.0f, 0.95f, 1.0f}, {0.2f, 1.0f, 0.45f}, 3, -1});

    // 2. Audio FX Inserts (3 tracks)
    allTargets_.push_back({"fx_kick_dist", "TubeDistortion (Eats Kick)", "Warm Harmonic Saturation", ScriptTargetType::AudioFx, "AUDIO FX", {1.0f, 0.16f, 0.55f}, {1.0f, 0.55f, 0.0f}, 0, -1});
    allTargets_.push_back({"fx_snare_dist", "TubeDistortion (Eats Snare)", "Warm Harmonic Saturation", ScriptTargetType::AudioFx, "AUDIO FX", {1.0f, 0.16f, 0.55f}, {1.0f, 0.85f, 0.0f}, 1, -1});
    allTargets_.push_back({"fx_303_dist", "TubeDistortion (Eats-303)", "Warm Harmonic Saturation", ScriptTargetType::AudioFx, "AUDIO FX", {1.0f, 0.16f, 0.55f}, {0.2f, 1.0f, 0.45f}, 3, -1});

    // 2b. MIDI FX Processors
    allTargets_.push_back({"mfx_scale_snap", "ScaleSnap (C Natural Minor)", "Quantize Pitch to Active Key", ScriptTargetType::MidiFx, "MIDI FX", {1.0f, 0.85f, 0.0f}, {1.0f, 0.55f, 0.0f}, 0, -1});
    allTargets_.push_back({"mfx_arpeggiator", "Arpeggiator (16th UpDown)", "Algorithmic Pattern Generator", ScriptTargetType::MidiFx, "MIDI FX", {1.0f, 0.85f, 0.0f}, {0.13f, 0.96f, 0.91f}, 1, -1});
    allTargets_.push_back({"mfx_chord_follow", "Chord Follower (Triad Lock)", "Harmonic Progression Tracking", ScriptTargetType::MidiFx, "MIDI FX", {1.0f, 0.85f, 0.0f}, {0.2f, 1.0f, 0.45f}, 3, -1});

    // 3. Generative Clip Scripts (4 tracks)
    allTargets_.push_back({"clip_kick", "Kick Rest A (Eats Kick)", "Dynamic Four-on-the-Floor", ScriptTargetType::ClipScript, "CLIP SCRIPT", {0.0f, 1.0f, 0.62f}, {1.0f, 0.55f, 0.0f}, 0, 0});
    allTargets_.push_back({"clip_snare", "Snare Pattern (Eats Snare)", "Backbeat 2 & 4 with Ghosts", ScriptTargetType::ClipScript, "CLIP SCRIPT", {0.0f, 1.0f, 0.62f}, {1.0f, 0.85f, 0.0f}, 1, 0});
    allTargets_.push_back({"clip_hats", "Hi-Hat Groove (Eats Hats)", "16th-Note Swing Groove", ScriptTargetType::ClipScript, "CLIP SCRIPT", {0.0f, 1.0f, 0.62f}, {0.13f, 0.96f, 0.91f}, 2, 0});
    allTargets_.push_back({"clip_303", "Acid 303 Riff (Eats-303)", "16-Step Hypnotic Ostinato", ScriptTargetType::ClipScript, "CLIP SCRIPT", {0.0f, 1.0f, 0.62f}, {0.2f, 1.0f, 0.45f}, 3, 0});

    // 4. Built-in Preset Library Reference
    auto builtins = project::PresetLoader::getBuiltinPresets();
    for (const auto& bp : builtins) {
        allTargets_.push_back({bp.metadata.id, bp.metadata.name, bp.metadata.category, ScriptTargetType::BuiltinPreset,
                              bp.metadata.category == "instrument" ? "INSTRUMENT" : "AUDIO FX",
                              bp.metadata.category == "instrument" ? Color{0.0f, 0.95f, 1.0f} : Color{1.0f, 0.16f, 0.55f},
                              Color{0.7f, 0.75f, 0.85f}, -1, -1});
    }

    activeTargetIndex_ = 0;
    updateActiveTargetCodeAndParams();
}

void DesignView::initDefaultGuiPanel() {
    const auto& activeT = getActiveTarget();
    guiPanel_.title = activeT.title;
    guiPanel_.subtitle = activeT.subtitle;
    guiPanel_.accentColor = activeT.badgeColor;
    guiPanel_.woodCheeks = true;
    guiPanel_.cornerRadius = 8.0f;
    guiPanel_.rows.clear();

    if (activeT.id == "eats_303") {
        guiPanel_.chassisStyle = GuiChassisStyle::MinimalWhite;
        guiPanel_.woodCheeks = false;
        guiPanel_.hideHeader = true;
        guiPanel_.cornerRadius = 6.0f;

        // Row 1: WAVEFORM, divider, PITCH, CUTOFF, RESONANCE, ENV MOD, DECAY, ACCENT
        GuiRowDef r1;
        r1.widgets.push_back({"w_waveform", GuiWidgetType::Knob, "WAVEFORM", "Waveform", GuiKnobStyle::Tb303SelectorSilver, 52.0f, 0.0f, 0.0f, 1.0f, "", Color(0.12f, 0.14f, 0.18f)});
        r1.widgets.push_back({"div1", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
        r1.widgets.push_back({"w_pitch", GuiWidgetType::Knob, "PITCH", "Pitch", GuiKnobStyle::Tb303Potentiometer, 52.0f, 0.5f, -12.0f, 12.0f, "st", Color(0.12f, 0.14f, 0.18f)});
        r1.widgets.push_back({"w_cutoff", GuiWidgetType::Knob, "CUTOFF", "Cutoff", GuiKnobStyle::Tb303Potentiometer, 52.0f, 0.65f, 200.0f, 4500.0f, "Hz", Color(0.12f, 0.14f, 0.18f)});
        r1.widgets.push_back({"w_res", GuiWidgetType::Knob, "RESONANCE", "Resonance", GuiKnobStyle::Tb303Potentiometer, 52.0f, 0.78f, 0.5f, 16.0f, "", Color(0.12f, 0.14f, 0.18f)});
        r1.widgets.push_back({"w_envmod", GuiWidgetType::Knob, "ENV MOD", "EnvMod", GuiKnobStyle::Tb303Potentiometer, 52.0f, 0.60f, 0.0f, 1.0f, "", Color(0.12f, 0.14f, 0.18f)});
        r1.widgets.push_back({"w_decay", GuiWidgetType::Knob, "DECAY", "Decay", GuiKnobStyle::Tb303Potentiometer, 52.0f, 0.45f, 0.05f, 1.2f, "s", Color(0.12f, 0.14f, 0.18f)});
        r1.widgets.push_back({"w_accent", GuiWidgetType::Knob, "ACCENT", "Accent", GuiKnobStyle::Tb303Potentiometer, 52.0f, 0.78f, 0.0f, 1.0f, "", Color(0.12f, 0.14f, 0.18f)});
        guiPanel_.rows.push_back(r1);

        // Row 2: OCTAVE, divider, SUB OSC, SUB VOL, divider, GLIDE CURVE, divider, DRIVE
        GuiRowDef r2;
        r2.widgets.push_back({"w_octave", GuiWidgetType::Knob, "OCTAVE", "Octave", GuiKnobStyle::Tb303SelectorBlack, 52.0f, 0.5f, -2.0f, 0.0f, "", Color(0.12f, 0.14f, 0.18f)});
        r2.widgets.push_back({"div2", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
        r2.widgets.push_back({"w_subosc", GuiWidgetType::ToggleSwitch, "SUB OSC", "SubWaveform", GuiKnobStyle::Standard, 48.0f, 0.0f, 0.0f, 1.0f, "", Color(0.12f, 0.14f, 0.18f)});
        r2.widgets.push_back({"w_subvol", GuiWidgetType::Knob, "SUB VOL", "SubVolume", GuiKnobStyle::MiniPotCream, 48.0f, 0.35f, 0.0f, 1.0f, "", Color(0.12f, 0.14f, 0.18f)});
        r2.widgets.push_back({"div3", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
        r2.widgets.push_back({"w_glide", GuiWidgetType::Knob, "GLIDE CURVE", "GlideCurve", GuiKnobStyle::MiniPotCream, 48.0f, 0.40f, 0.0f, 2.0f, "", Color(0.12f, 0.14f, 0.18f)});
        r2.widgets.push_back({"div4", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
        r2.widgets.push_back({"w_drive", GuiWidgetType::Knob, "DRIVE", "Drive", GuiKnobStyle::MiniPotCream, 48.0f, 0.25f, 0.0f, 1.0f, "", Color(0.12f, 0.14f, 0.18f)});
        guiPanel_.rows.push_back(r2);
    } else if (activeT.id == "eats_kick") {
        guiPanel_.chassisStyle = GuiChassisStyle::Grunge;
        GuiRowDef r1;
        r1.widgets.push_back({"w_start_f", GuiWidgetType::Knob, "START FREQ", "StartFreq", GuiKnobStyle::BakeliteSkirt, 56.0f, 0.55f, 100.0f, 300.0f, "Hz", {1.0f, 0.55f, 0.0f}});
        r1.widgets.push_back({"w_end_f", GuiWidgetType::Knob, "END FREQ", "EndFreq", GuiKnobStyle::CreamFluted, 56.0f, 0.40f, 30.0f, 60.0f, "Hz", {0.0f, 0.95f, 1.0f}});
        r1.widgets.push_back({"w_pdecay", GuiWidgetType::Knob, "PITCH DECAY", "PitchDecay", GuiKnobStyle::AnodizedKnurled, 56.0f, 0.35f, 0.01f, 0.20f, "s", {1.0f, 0.85f, 0.0f}});
        guiPanel_.rows.push_back(r1);

        GuiRowDef r2;
        r2.widgets.push_back({"w_ampdecay", GuiWidgetType::Knob, "AMP DECAY", "AmpDecay", GuiKnobStyle::TwoToneStepped, 56.0f, 0.45f, 0.05f, 4.0f, "s", {0.0f, 1.0f, 0.55f}});
        r2.widgets.push_back({"w_click", GuiWidgetType::ToggleSwitch, "CLICK TRANSIENT", "Click", GuiKnobStyle::Standard, 48.0f, 0.0f, 0.0f, 1.0f, "", {1.0f, 0.55f, 0.0f}});
        r2.widgets.push_back({"w_vu", GuiWidgetType::VuMeter, "OUTPUT LEVEL", "Volume", GuiKnobStyle::Standard, 70.0f, 0.82f, 0.0f, 1.0f, "dB", {0.0f, 0.95f, 1.0f}});
        guiPanel_.rows.push_back(r2);
    } else if (activeT.type == ScriptTargetType::AudioFx) {
        guiPanel_.chassisStyle = GuiChassisStyle::Walnut;
        GuiRowDef r1;
        r1.widgets.push_back({"w_drive", GuiWidgetType::Knob, "SATURATION DRIVE", "Drive", GuiKnobStyle::BakeliteSkirt, 56.0f, 0.60f, 1.0f, 20.0f, "x", {1.0f, 0.16f, 0.55f}});
        r1.widgets.push_back({"w_warmth", GuiWidgetType::Knob, "WARMTH HARMONICS", "Warmth", GuiKnobStyle::CreamFluted, 56.0f, 0.70f, 0.0f, 1.0f, "", {1.0f, 0.85f, 0.0f}});
        guiPanel_.rows.push_back(r1);

        GuiRowDef r2;
        r2.widgets.push_back({"w_mix", GuiWidgetType::Slider, "DRY / WET MIX", "Mix", GuiKnobStyle::Standard, 140.0f, 0.85f, 0.0f, 1.0f, "%", {0.0f, 0.95f, 1.0f}});
        r2.widgets.push_back({"w_vu", GuiWidgetType::VuMeter, "DISTORTION PEAK", "Level", GuiKnobStyle::Standard, 60.0f, 0.75f, 0.0f, 1.0f, "dB", {1.0f, 0.16f, 0.55f}});
        guiPanel_.rows.push_back(r2);
    } else {
        guiPanel_.chassisStyle = GuiChassisStyle::Silver;
        GuiRowDef r1;
        for (size_t i = 0; i < currentParams_.size() && i < 4; ++i) {
            r1.widgets.push_back({"w_" + std::to_string(i), GuiWidgetType::Knob, currentParams_[i].name, currentParams_[i].name,
                                  static_cast<GuiKnobStyle>(i % 5), 56.0f, currentParams_[i].currentVal,
                                  currentParams_[i].minVal, currentParams_[i].maxVal, currentParams_[i].unit,
                                  activeT.badgeColor});
        }
        if (!r1.widgets.empty()) {
            guiPanel_.rows.push_back(r1);
        }
    }

    selectedDesignerRow_ = -1;
    selectedDesignerWidget_ = -1;
    isChassisSelected_ = false;
}

void DesignView::syncGuiPanelToScript() {
    std::ostringstream ss;
    ss << "# --- Hardware GUI Layout ---\n";
    ss << "def gui():\n";
    ss << "    return {\n";
    ss << "        \"title\": \"" << guiPanel_.title << "\",\n";
    ss << "        \"subtitle\": \"" << guiPanel_.subtitle << "\",\n";
    ss << "        \"chassis\": " << static_cast<int>(guiPanel_.chassisStyle) << ",\n";
    ss << "        \"woodCheeks\": " << (guiPanel_.woodCheeks ? "True" : "False") << ",\n";
    ss << "        \"rows\": [\n";

    for (const auto& r : guiPanel_.rows) {
        ss << "            [";
        for (size_t w = 0; w < r.widgets.size(); ++w) {
            const auto& wid = r.widgets[w];
            ss << "{\"id\": \"" << wid.id << "\", \"type\": " << static_cast<int>(wid.type)
               << ", \"param\": \"" << wid.param << "\", \"label\": \"" << wid.label
               << "\", \"style\": " << static_cast<int>(wid.knobStyle) << "}";
            if (w + 1 < r.widgets.size()) ss << ", ";
        }
        ss << "],\n";
    }
    ss << "        ]\n";
    ss << "    }\n";

    // If script already has gui() section, replace or append
    size_t guiPos = currentScriptCode_.find("# --- Hardware GUI Layout ---");
    if (guiPos != std::string::npos) {
        currentScriptCode_ = currentScriptCode_.substr(0, guiPos) + ss.str();
    } else {
        currentScriptCode_ += "\n" + ss.str();
    }
}

void DesignView::addGuiRow() {
    GuiRowDef newRow;
    guiPanel_.rows.push_back(newRow);
    selectedDesignerRow_ = static_cast<int>(guiPanel_.rows.size()) - 1;
    selectedDesignerWidget_ = -1;
    isChassisSelected_ = false;
    syncGuiPanelToScript();
}

void DesignView::deleteGuiRow(int rowIndex) {
    if (rowIndex >= 0 && rowIndex < static_cast<int>(guiPanel_.rows.size())) {
        guiPanel_.rows.erase(guiPanel_.rows.begin() + rowIndex);
        selectedDesignerRow_ = -1;
        selectedDesignerWidget_ = -1;
        syncGuiPanelToScript();
    }
}

void DesignView::addGuiWidget(GuiWidgetType type, GuiKnobStyle knobStyle, const std::string& label, const std::string& param) {
    if (guiPanel_.rows.empty()) {
        addGuiRow();
    }
    int targetRow = (selectedDesignerRow_ >= 0 && selectedDesignerRow_ < static_cast<int>(guiPanel_.rows.size())) ?
                    selectedDesignerRow_ : static_cast<int>(guiPanel_.rows.size()) - 1;

    std::string pName = param;
    if (pName.empty()) {
        for (const auto& p : currentParams_) {
            bool used = false;
            for (const auto& r : guiPanel_.rows) {
                for (const auto& w : r.widgets) {
                    if (w.param == p.name) { used = true; break; }
                }
            }
            if (!used) { pName = p.name; break; }
        }
        if (pName.empty() && !currentParams_.empty()) {
            pName = currentParams_[0].name;
        } else if (pName.empty()) {
            pName = "Param" + std::to_string(guiPanel_.rows[targetRow].widgets.size() + 1);
        }
    }

    std::string lbl = label.empty() ? pName : label;
    std::string uniqueId = "w_" + std::to_string(targetRow) + "_" + std::to_string(guiPanel_.rows[targetRow].widgets.size());

    GuiWidgetDef wDef{uniqueId, type, lbl, pName, knobStyle, 56.0f, 0.5f, 0.0f, 1.0f, "", guiPanel_.accentColor};
    guiPanel_.rows[targetRow].widgets.push_back(wDef);

    selectedDesignerRow_ = targetRow;
    selectedDesignerWidget_ = static_cast<int>(guiPanel_.rows[targetRow].widgets.size()) - 1;
    isChassisSelected_ = false;
    syncGuiPanelToScript();
}

void DesignView::deleteSelectedGuiWidget() {
    if (selectedDesignerRow_ >= 0 && selectedDesignerRow_ < static_cast<int>(guiPanel_.rows.size())) {
        auto& row = guiPanel_.rows[selectedDesignerRow_];
        if (selectedDesignerWidget_ >= 0 && selectedDesignerWidget_ < static_cast<int>(row.widgets.size())) {
            row.widgets.erase(row.widgets.begin() + selectedDesignerWidget_);
            selectedDesignerWidget_ = -1;
            syncGuiPanelToScript();
        }
    }
}

void DesignView::duplicateSelectedGuiWidget() {
    if (selectedDesignerRow_ >= 0 && selectedDesignerRow_ < static_cast<int>(guiPanel_.rows.size())) {
        auto& row = guiPanel_.rows[selectedDesignerRow_];
        if (selectedDesignerWidget_ >= 0 && selectedDesignerWidget_ < static_cast<int>(row.widgets.size())) {
            auto clone = row.widgets[selectedDesignerWidget_];
            clone.id = clone.id + "_copy";
            clone.label = clone.label + " *";
            row.widgets.insert(row.widgets.begin() + selectedDesignerWidget_ + 1, clone);
            selectedDesignerWidget_++;
            syncGuiPanelToScript();
        }
    }
}

void DesignView::selectDesignerWidget(int row, int widget) {
    selectedDesignerRow_ = row;
    selectedDesignerWidget_ = widget;
    isChassisSelected_ = false;
}

void DesignView::selectDesignerChassis() {
    isChassisSelected_ = true;
    selectedDesignerRow_ = -1;
    selectedDesignerWidget_ = -1;
}

void DesignView::updateActiveTargetCodeAndParams() {
    if (activeTargetIndex_ < 0 || activeTargetIndex_ >= static_cast<int>(allTargets_.size())) {
        activeTargetIndex_ = 0;
    }

    const auto& t = allTargets_[activeTargetIndex_];
    currentParams_.clear();

    if (t.id == "eats_kick") {
        currentScriptCode_ =
            "# --- Procedural Sub Kick Drum (Eatscript) ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"StartFreq\", min=100.0, max=300.0, default=160.0)\n"
            "    eat.param(\"EndFreq\", min=30.0, max=60.0, default=42.0)\n"
            "    eat.param(\"PitchDecay\", min=0.01, max=0.2, default=0.035)\n"
            "    eat.param(\"AmpDecay\", min=0.05, max=4.0, default=0.35)\n"
            "    eat.param(\"Click\", min=0.0, max=1.0, default=0.0)\n\n"
            "def process(time, freq, note, params):\n"
            "    startF = params.get(\"StartFreq\", 160.0)\n"
            "    endF = params.get(\"EndFreq\", 42.0)\n"
            "    pDecay = params.get(\"PitchDecay\", 0.035)\n"
            "    aDecay = params.get(\"AmpDecay\", 0.35)\n"
            "    click = params.get(\"Click\", 0.0)\n\n"
            "    curFreq = endF + (startF - endF) * math.exp(-time / max(0.005, pDecay))\n"
            "    phase = 2.0 * math.pi * curFreq * time\n"
            "    body = math.sin(phase) * math.exp(-time / max(0.01, aDecay))\n"
            "    clickNoise = (math.sin(time * 8421.0) * math.exp(-time / 0.005)) * click\n"
            "    return body + clickNoise\n";

        currentParams_ = {
            {"StartFreq", 100.0f, 300.0f, 160.0f, 120.2f, "Hz"},
            {"EndFreq", 30.0f, 60.0f, 42.0f, 56.4f, "Hz"},
            {"PitchDecay", 0.01f, 0.20f, 0.035f, 0.035f, "s"},
            {"AmpDecay", 0.05f, 4.00f, 0.35f, 0.80f, "s"},
            {"Click", 0.0f, 1.0f, 0.0f, 0.0f, ""}
        };
    } else if (t.id == "eats_snare") {
        currentScriptCode_ =
            "# --- Procedural Analog Snare Drum (Eatscript) ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"Tone\", min=100.0, max=350.0, default=180.0)\n"
            "    eat.param(\"Snappy\", min=0.0, max=1.0, default=0.65)\n"
            "    eat.param(\"Decay\", min=0.05, max=1.0, default=0.25)\n"
            "    eat.param(\"Tune\", min=0.5, max=2.0, default=1.0)\n\n"
            "def process(time, freq, note, params):\n"
            "    toneF = params.get(\"Tone\", 180.0) * params.get(\"Tune\", 1.0)\n"
            "    decay = params.get(\"Decay\", 0.25)\n"
            "    snappy = params.get(\"Snappy\", 0.65)\n\n"
            "    tone = math.sin(2.0 * math.pi * toneF * time) * math.exp(-time / (decay * 0.7))\n"
            "    noise = (math.sin(time * 12345.67) % 1.0 - 0.5) * 2.0 * math.exp(-time / decay) * snappy\n"
            "    return tone * (1.0 - snappy * 0.5) + noise\n";

        currentParams_ = {
            {"Tone", 100.0f, 350.0f, 180.0f, 185.0f, "Hz"},
            {"Snappy", 0.0f, 1.0f, 0.65f, 0.72f, ""},
            {"Decay", 0.05f, 1.0f, 0.25f, 0.28f, "s"},
            {"Tune", 0.5f, 2.0f, 1.0f, 1.05f, "x"}
        };
    } else if (t.id == "eats_hats") {
        currentScriptCode_ =
            "# --- Procedural Metallic Hi-Hat (Eatscript) ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"Decay\", min=0.01, max=0.8, default=0.08)\n"
            "    eat.param(\"Metallic\", min=0.1, max=1.0, default=0.75)\n"
            "    eat.param(\"Pitch\", min=4000.0, max=12000.0, default=8500.0)\n\n"
            "def process(time, freq, note, params):\n"
            "    decay = params.get(\"Decay\", 0.08)\n"
            "    pitch = params.get(\"Pitch\", 8500.0)\n"
            "    osc1 = math.sin(2.0 * math.pi * pitch * time)\n"
            "    osc2 = math.sin(2.0 * math.pi * (pitch * 1.414) * time)\n"
            "    metallic = (1.0 if osc1 * osc2 > 0.0 else -1.0) * math.exp(-time / decay)\n"
            "    return metallic * 0.4\n";

        currentParams_ = {
            {"Decay", 0.01f, 0.8f, 0.08f, 0.08f, "s"},
            {"Metallic", 0.1f, 1.0f, 0.75f, 0.80f, ""},
            {"Pitch", 4000.0f, 12000.0f, 8500.0f, 8600.0f, "Hz"}
        };
    } else if (t.id == "eats_303") {
        currentScriptCode_ =
            "# --- TB-303 Acid Bassline Synthesizer (Eatscript) ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"Cutoff\", min=200.0, max=4000.0, default=850.0)\n"
            "    eat.param(\"Resonance\", min=0.1, max=0.98, default=0.78)\n"
            "    eat.param(\"EnvMod\", min=0.0, max=1.0, default=0.65)\n"
            "    eat.param(\"Decay\", min=0.05, max=1.5, default=0.45)\n"
            "    eat.param(\"Accent\", min=0.0, max=1.0, default=0.70)\n\n"
            "def process(time, freq, note, params):\n"
            "    saw = 2.0 * (time * freq - math.floor(time * freq + 0.5))\n"
            "    cutoff = params.get(\"Cutoff\", 850.0)\n"
            "    res = params.get(\"Resonance\", 0.78)\n"
            "    return tb303_diode_ladder(saw, cutoff, res)\n";

        currentParams_ = {
            {"Cutoff", 200.0f, 4000.0f, 850.0f, 920.0f, "Hz"},
            {"Resonance", 0.1f, 0.98f, 0.78f, 0.85f, ""},
            {"EnvMod", 0.0f, 1.0f, 0.65f, 0.60f, ""},
            {"Decay", 0.05f, 1.5f, 0.45f, 0.45f, "s"},
            {"Accent", 0.0f, 1.0f, 0.70f, 0.75f, ""}
        };
    } else if (t.type == ScriptTargetType::AudioFx) {
        currentScriptCode_ =
            "# --- Warm Analog Tube Distortion (Eatscript FX) ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"Drive\", min=1.0, max=20.0, default=4.5)\n"
            "    eat.param(\"Warmth\", min=0.0, max=1.0, default=0.70)\n"
            "    eat.param(\"Mix\", min=0.0, max=1.0, default=0.85)\n\n"
            "def process(in_l, in_r, params):\n"
            "    drive = params.get(\"Drive\", 4.5)\n"
            "    mix = params.get(\"Mix\", 0.85)\n"
            "    out_l = math.tanh(in_l * drive) * mix + in_l * (1.0 - mix)\n"
            "    out_r = math.tanh(in_r * drive) * mix + in_r * (1.0 - mix)\n"
            "    return [out_l, out_r]\n";

        currentParams_ = {
            {"Drive", 1.0f, 20.0f, 4.5f, 6.2f, "x"},
            {"Warmth", 0.0f, 1.0f, 0.70f, 0.75f, ""},
            {"Mix", 0.0f, 1.0f, 0.85f, 0.90f, "%"}
        };
    } else if (t.type == ScriptTargetType::MidiFx) {
        currentScriptCode_ =
            "# --- Real-Time MIDI FX Pipeline (Eatscript) ---\n"
            "# Transforms note pitch, velocity, and timing before voice trigger\n"
            "def init():\n"
            "    eat.param(\"RootKey\", min=0, max=11, default=0) # 0 = C\n"
            "    eat.param(\"ScaleMode\", min=0, max=6, default=0) # 0 = Major, 1 = Minor\n"
            "    eat.param(\"HumanizeVel\", min=0.0, max=0.5, default=0.15)\n\n"
            "def process_notes(notes, context):\n"
            "    out = []\n"
            "    for n in notes:\n"
            "        snapped = eat.snap_to_scale(n.pitch, root=0, minor=True)\n"
            "        out.append(eat.Note(snapped, n.velocity, n.length))\n"
            "    return out\n";

        currentParams_ = {
            {"RootKey", 0.0f, 11.0f, 0.0f, 0.0f, "st"},
            {"ScaleMode", 0.0f, 6.0f, 0.0f, 0.0f, ""},
            {"HumanizeVel", 0.0f, 0.50f, 0.15f, 0.20f, "%"},
            {"Transpose", -24.0f, 24.0f, 0.0f, 0.0f, "st"}
        };
    } else {
        currentScriptCode_ =
            "# --- Eatscript Generative Model ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"Density\", min=0.1, max=1.0, default=0.75)\n"
            "    eat.param(\"Velocity\", min=0.1, max=1.0, default=0.90)\n\n"
            "def generate(bars, step):\n"
            "    return []\n";

        currentParams_ = {
            {"Density", 0.1f, 1.0f, 0.75f, 0.75f, ""},
            {"Velocity", 0.1f, 1.0f, 0.90f, 0.90f, ""}
        };
    }

    setScriptCode(currentScriptCode_);
    initDefaultGuiPanel();
}

void DesignView::setScriptCode(const std::string& code) {
    currentScriptCode_ = code;
    codeLines_.clear();
    std::stringstream ss(code);
    std::string line;
    while (std::getline(ss, line)) {
        codeLines_.push_back(line);
    }
    if (codeLines_.empty()) {
        codeLines_.push_back("");
    }
    textEditor_.setText(code);
}

void DesignView::selectTargetByIndex(int index) {
    if (index >= 0 && index < static_cast<int>(allTargets_.size())) {
        activeTargetIndex_ = index;
        updateActiveTargetCodeAndParams();
    }
}

void DesignView::selectTargetById(const std::string& id) {
    for (size_t i = 0; i < allTargets_.size(); ++i) {
        if (allTargets_[i].id == id) {
            selectTargetByIndex(static_cast<int>(i));
            return;
        }
    }
}

void DesignView::selectTargetByTrackAndType(int trackIndex, ScriptTargetType type) {
    for (size_t i = 0; i < allTargets_.size(); ++i) {
        if (allTargets_[i].type == type && (trackIndex < 0 || allTargets_[i].trackIndex == trackIndex)) {
            selectTargetByIndex(static_cast<int>(i));
            return;
        }
    }
    for (size_t i = 0; i < allTargets_.size(); ++i) {
        if (allTargets_[i].type == type) {
            selectTargetByIndex(static_cast<int>(i));
            return;
        }
    }
}

const ScriptTarget& DesignView::getActiveTarget() const {
    if (activeTargetIndex_ >= 0 && activeTargetIndex_ < static_cast<int>(allTargets_.size())) {
        return allTargets_[activeTargetIndex_];
    }
    static ScriptTarget fallback{"unknown", "Unknown Target", "", ScriptTargetType::TrackDsp, "SYNTH DSP", {0.0f, 0.95f, 1.0f}, {1.0f, 0.55f, 0.0f}, 0, -1};
    return fallback;
}

void DesignView::setAudioScopeBuffer(const float* buffer, size_t count) {
    if (!buffer || count == 0) return;
    scopeSamples_.resize(count);
    std::memcpy(scopeSamples_.data(), buffer, count * sizeof(float));
}

void DesignView::compileCurrentScript(const ViewContext& ctx) {
    isCompiled_ = true;
    statusMsg_ = "Compiled successfully (Eatscript Live Engine) Active parameters: " + std::to_string(currentParams_.size());
    errorMsg_.clear();

    if (onCompileScript) {
        onCompileScript(getActiveTarget().id, currentScriptCode_);
    }

    if (ctx.onShowNotification) {
        ctx.onShowNotification("Compiled & hot-swapped " + getActiveTarget().title + " (Real-time C++ Core active)");
    }
}

void DesignView::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;

    // Top Sub-Nav Header: H = 38px
    float headerH = 38.0f;
    headerBounds_ = Rect2D(bounds_.x, bounds_.y, bounds_.w, headerH);
    contentBounds_ = Rect2D(bounds_.x, bounds_.y + headerH, bounds_.w, bounds_.h - headerH);

    float btnY = headerBounds_.y + 5.0f;
    float btnH = 28.0f;

    // Header element sizing based on available width
    float availW = headerBounds_.w;
    float compileW = (availW < 900.0f) ? 95.0f : (ctx.isMobile ? 85.0f : 135.0f);
    float modeBtnW = (availW < 900.0f) ? 56.0f : (availW < 1100.0f ? 66.0f : (ctx.isMobile ? 48.0f : 76.0f));
    float targetDropdownW = (availW < 900.0f) ? 140.0f : (availW < 1100.0f ? 180.0f : (ctx.isMobile ? 140.0f : 240.0f));

    // Left elements: Explorer Toggle (30px), Target Badge (80px), Target Dropdown
    float curX = headerBounds_.x + 10.0f;
    explorerToggleBtn_ = Rect2D(curX, btnY, 30.0f, btnH);
    curX += 36.0f;

    targetBadgeBounds_ = Rect2D(curX, btnY + 3.0f, 80.0f, btnH - 6.0f);
    curX += 86.0f;

    targetDropdownBtn_ = Rect2D(curX, btnY, targetDropdownW, btnH);
    targetDropdownMenuBounds_ = Rect2D(curX, btnY + btnH + 4.0f, targetDropdownW + 80.0f, 260.0f);

    // Right elements: COMPILE & RUN, GUI, SPLIT, MODULAR, CODE
    float rightMargin = headerBounds_.x + headerBounds_.w - 12.0f;
    btnCompile_ = Rect2D(rightMargin - compileW, btnY, compileW, btnH);

    btnGui_ = Rect2D(btnCompile_.x - modeBtnW - 6.0f, btnY, modeBtnW, btnH);
    btnSplit_ = Rect2D(btnGui_.x - modeBtnW - 4.0f, btnY, modeBtnW, btnH);
    btnModular_ = Rect2D(btnSplit_.x - (modeBtnW + 8.0f) - 4.0f, btnY, modeBtnW + 8.0f, btnH);
    btnCode_ = Rect2D(btnModular_.x - modeBtnW - 4.0f, btnY, modeBtnW, btnH);

    // Explorer sidebar vs Studio body layout
    float explorerW = isExplorerOpen_ ? (ctx.isMobile ? 180.0f : 240.0f) : 0.0f;
    explorerBounds_ = Rect2D(contentBounds_.x, contentBounds_.y, explorerW, contentBounds_.h);
    studioBodyBounds_ = Rect2D(contentBounds_.x + explorerW, contentBounds_.y, contentBounds_.w - explorerW, contentBounds_.h);

    // Code Editor layout inside studioBodyBounds_
    float pad = 12.0f;
    float scopeH = 64.0f;
    scopeBounds_ = Rect2D(studioBodyBounds_.x + pad, studioBodyBounds_.y + pad, studioBodyBounds_.w - (pad * 2.0f), scopeH);

    float edHeaderY = scopeBounds_.y + scopeH + 10.0f;
    editorHeaderBounds_ = Rect2D(scopeBounds_.x, edHeaderY, scopeBounds_.w, 28.0f);

    float linkY = edHeaderY + 6.0f;
    float linkRight = editorHeaderBounds_.x + editorHeaderBounds_.w - 10.0f;
    btnApiDocs_ = Rect2D(linkRight - 65.0f, linkY, 65.0f, 18.0f);
    btnSubmitPr_ = Rect2D(btnApiDocs_.x - 70.0f, linkY, 65.0f, 18.0f);
    btnCopy_ = Rect2D(btnSubmitPr_.x - 48.0f, linkY, 44.0f, 18.0f);
    btnSelectAll_ = Rect2D(btnCopy_.x - 75.0f, linkY, 70.0f, 18.0f);

    float edCanvasY = edHeaderY + 28.0f;
    float edCanvasH = std::min(300.0f, std::max(160.0f, studioBodyBounds_.h - 280.0f));
    editorCanvasBounds_ = Rect2D(scopeBounds_.x, edCanvasY, scopeBounds_.w, edCanvasH);
    textEditor_.layout(editorCanvasBounds_, ctx);

    float statusY = edCanvasY + edCanvasH + 8.0f;
    statusBannerBounds_ = Rect2D(scopeBounds_.x, statusY, scopeBounds_.w, 26.0f);

    float liveParamsY = statusY + 32.0f;
    float liveParamsH = std::max(80.0f, studioBodyBounds_.y + studioBodyBounds_.h - liveParamsY - pad);
    liveParamsSectionBounds_ = Rect2D(scopeBounds_.x, liveParamsY, scopeBounds_.w, liveParamsH);

    // Compute parameter card bounds
    float cardW = 160.0f;
    float cardH = 50.0f;
    float cardX = liveParamsSectionBounds_.x;
    float cardY = liveParamsSectionBounds_.y + 20.0f;

    for (size_t i = 0; i < currentParams_.size(); ++i) {
        if (cardX + cardW > liveParamsSectionBounds_.x + liveParamsSectionBounds_.w) {
            cardX = liveParamsSectionBounds_.x;
            cardY += cardH + 8.0f;
        }
        currentParams_[i].bounds = Rect2D(cardX, cardY, cardW, cardH);
        cardX += cardW + 10.0f;
    }

    // Split mode sub-bar layout
    float splitSubH = 28.0f;
    splitSubBarBounds_ = Rect2D(studioBodyBounds_.x + (studioBodyBounds_.w * 0.45f), studioBodyBounds_.y, studioBodyBounds_.w * 0.55f, splitSubH);
    btnSplitModular_ = Rect2D(splitSubBarBounds_.x + splitSubBarBounds_.w - 85.0f, splitSubBarBounds_.y + 4.0f, 75.0f, 20.0f);
    btnSplitFaceplate_ = Rect2D(btnSplitModular_.x - 85.0f, splitSubBarBounds_.y + 4.0f, 80.0f, 20.0f);

    // GUI mode sub-bar layout
    guiSubBarBounds_ = Rect2D(studioBodyBounds_.x, studioBodyBounds_.y, studioBodyBounds_.w, 32.0f);
    btnGuiPr_ = Rect2D(guiSubBarBounds_.x + guiSubBarBounds_.w - 95.0f, guiSubBarBounds_.y + 5.0f, 85.0f, 22.0f);
    btnGuiDesign_ = Rect2D(btnGuiPr_.x - 110.0f, guiSubBarBounds_.y + 5.0f, 102.0f, 22.0f);
    btnGuiLive_ = Rect2D(btnGuiDesign_.x - 130.0f, guiSubBarBounds_.y + 5.0f, 122.0f, 22.0f);

    // Modular Toolbar layout inside studioBodyBounds_
    modularToolbarBounds_ = Rect2D(studioBodyBounds_.x, studioBodyBounds_.y, studioBodyBounds_.w, 34.0f);
    btnModularResetPatch_ = Rect2D(modularToolbarBounds_.x + modularToolbarBounds_.w - 110.0f, modularToolbarBounds_.y + 5.0f, 98.0f, 24.0f);
    btnModularAddModule_ = Rect2D(btnModularResetPatch_.x - 115.0f, modularToolbarBounds_.y + 5.0f, 105.0f, 24.0f);

    // GUI Designer layout
    designerToolbarBounds_ = Rect2D(studioBodyBounds_.x, studioBodyBounds_.y + guiSubBarBounds_.h, studioBodyBounds_.w, 34.0f);
    btnTogglePalette_ = Rect2D(designerToolbarBounds_.x + 8.0f, designerToolbarBounds_.y + 5.0f, 82.0f, 24.0f);
    btnAddRow_ = Rect2D(btnTogglePalette_.x + 90.0f, designerToolbarBounds_.y + 5.0f, 88.0f, 24.0f);
    btnDeleteRow_ = Rect2D(btnAddRow_.x + 94.0f, designerToolbarBounds_.y + 5.0f, 88.0f, 24.0f);
    btnDuplicateWidget_ = Rect2D(btnDeleteRow_.x + 94.0f, designerToolbarBounds_.y + 5.0f, 88.0f, 24.0f);
    btnDeleteWidget_ = Rect2D(btnDuplicateWidget_.x + 94.0f, designerToolbarBounds_.y + 5.0f, 88.0f, 24.0f);
    btnToggleInspector_ = Rect2D(designerToolbarBounds_.x + designerToolbarBounds_.w - 96.0f, designerToolbarBounds_.y + 5.0f, 88.0f, 24.0f);

    // Modular Rack layout inside studioBodyBounds_
    float railH = 14.0f;
    float rackBodyY = studioBodyBounds_.y + modularToolbarBounds_.h;
    float rackBodyH = std::max(200.0f, studioBodyBounds_.h - modularToolbarBounds_.h);
    float modX = studioBodyBounds_.x + 24.0f;
    float modY = rackBodyY + railH + 10.0f;
    float modH = std::max(260.0f, rackBodyH - (railH * 2.0f) - 20.0f);

    for (size_t m = 0; m < modules_.size(); ++m) {
        modules_[m].x = modX;
        modules_[m].y = modY;
        modules_[m].h = modH;
        modX += modules_[m].w + 20.0f;
    }
}

void DesignView::render(const ViewContext& ctx) {
    if (!ctx.renderer || !ctx.theme) return;

    renderSubNavHeader(ctx);

    if (isExplorerOpen_ && explorerBounds_.w > 0.0f) {
        renderExplorerSidebar(ctx);
    }

    switch (mode_) {
        case DesignSubMode::Code:
            renderCodeEditorCanvas(ctx);
            break;
        case DesignSubMode::ModularRack:
            renderModularRack(ctx, studioBodyBounds_);
            break;
        case DesignSubMode::Split:
            renderSplitView(ctx);
            break;
        case DesignSubMode::GuiDesigner:
            renderGuiPreview(ctx);
            break;
    }

    // Render Target Dropdown popup on top if open
    if (isTargetDropdownOpen_) {
        auto& r = *ctx.renderer;
        const auto& theme = *ctx.theme;

        drawRoundedRect(r, targetDropdownMenuBounds_.x, targetDropdownMenuBounds_.y,
                        targetDropdownMenuBounds_.w, targetDropdownMenuBounds_.h, 6.0f,
                        0.10f, 0.12f, 0.16f, 0.98f);
        drawRoundedRectOutline(r, targetDropdownMenuBounds_.x, targetDropdownMenuBounds_.y,
                               targetDropdownMenuBounds_.w, targetDropdownMenuBounds_.h, 6.0f,
                               theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.8f, 1.2f);

        targetDropdownItemBounds_.clear();
        float itemY = targetDropdownMenuBounds_.y + 6.0f;
        float itemH = 26.0f;

        for (size_t i = 0; i < allTargets_.size() && i < 9; ++i) {
            Rect2D itemR(targetDropdownMenuBounds_.x + 4.0f, itemY, targetDropdownMenuBounds_.w - 8.0f, itemH);
            targetDropdownItemBounds_.push_back(itemR);

            bool isSel = (static_cast<int>(i) == activeTargetIndex_);
            if (isSel) {
                drawRoundedRect(r, itemR.x, itemR.y, itemR.w, itemR.h, 3.0f,
                                allTargets_[i].badgeColor.r * 0.25f, allTargets_[i].badgeColor.g * 0.25f, allTargets_[i].badgeColor.b * 0.25f, 0.9f);
                drawRoundedRectOutline(r, itemR.x, itemR.y, itemR.w, itemR.h, 3.0f,
                                       allTargets_[i].badgeColor.r, allTargets_[i].badgeColor.g, allTargets_[i].badgeColor.b, 0.8f, 1.0f);
            }

            drawCircle(r, itemR.x + 10.0f, itemR.y + 13.0f, 3.5f,
                       allTargets_[i].trackColor.r, allTargets_[i].trackColor.g, allTargets_[i].trackColor.b, 1.0f);

            drawRoundedRect(r, itemR.x + 22.0f, itemR.y + 5.0f, 60.0f, 16.0f, 2.0f,
                            allTargets_[i].badgeColor.r * 0.2f, allTargets_[i].badgeColor.g * 0.2f, allTargets_[i].badgeColor.b * 0.2f, 0.8f);
            drawText(r, allTargets_[i].typeBadge, itemR.x + 26.0f, itemR.y + 7.0f, 8.0f,
                     allTargets_[i].badgeColor.r, allTargets_[i].badgeColor.g, allTargets_[i].badgeColor.b, 1.0f);

            drawText(r, allTargets_[i].title, itemR.x + 88.0f, itemR.y + 7.0f, 10.0f,
                     isSel ? 1.0f : 0.85f, isSel ? 1.0f : 0.85f, isSel ? 1.0f : 0.85f, 1.0f);

            itemY += itemH + 2.0f;
        }
    }
}

void DesignView::renderSubNavHeader(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;
    const auto& activeT = getActiveTarget();

    drawRect(r, headerBounds_.x, headerBounds_.y, headerBounds_.w, headerBounds_.h,
             theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 1.0f);
    drawLine(r, headerBounds_.x, headerBounds_.y + headerBounds_.h,
             headerBounds_.x + headerBounds_.w, headerBounds_.y + headerBounds_.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.2f);

    drawRoundedRect(r, explorerToggleBtn_.x, explorerToggleBtn_.y, explorerToggleBtn_.w, explorerToggleBtn_.h, 4.0f,
                    0.15f, 0.17f, 0.22f, 0.8f);
    drawRoundedRectOutline(r, explorerToggleBtn_.x, explorerToggleBtn_.y, explorerToggleBtn_.w, explorerToggleBtn_.h, 4.0f,
                           isExplorerOpen_ ? theme.primaryAccent.r : theme.borderSubtle.r,
                           isExplorerOpen_ ? theme.primaryAccent.g : theme.borderSubtle.g,
                           isExplorerOpen_ ? theme.primaryAccent.b : theme.borderSubtle.b,
                           0.8f, 1.0f);

    float ix = explorerToggleBtn_.x + 8.0f;
    float iy = explorerToggleBtn_.y + 7.0f;
    drawRect(r, ix, iy, 4.0f, 14.0f, isExplorerOpen_ ? theme.primaryAccent.r : 0.5f, isExplorerOpen_ ? theme.primaryAccent.g : 0.5f, isExplorerOpen_ ? theme.primaryAccent.b : 0.5f, 1.0f);
    drawRect(r, ix + 6.0f, iy, 8.0f, 14.0f, 0.35f, 0.38f, 0.45f, 0.8f);

    drawRoundedRect(r, targetBadgeBounds_.x, targetBadgeBounds_.y, targetBadgeBounds_.w, targetBadgeBounds_.h, 3.0f,
                    activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);
    drawText(r, activeT.typeBadge, targetBadgeBounds_.x + 8.0f, targetBadgeBounds_.y + 5.0f, 9.0f,
             0.0f, 0.0f, 0.0f, 1.0f);

    drawRoundedRect(r, targetDropdownBtn_.x, targetDropdownBtn_.y, targetDropdownBtn_.w, targetDropdownBtn_.h, 4.0f,
                    0.12f, 0.14f, 0.18f, 0.9f);
    drawRoundedRectOutline(r, targetDropdownBtn_.x, targetDropdownBtn_.y, targetDropdownBtn_.w, targetDropdownBtn_.h, 4.0f,
                           activeT.badgeColor.r * 0.6f, activeT.badgeColor.g * 0.6f, activeT.badgeColor.b * 0.6f, 0.8f, 1.0f);

    drawCircle(r, targetDropdownBtn_.x + 12.0f, targetDropdownBtn_.y + 14.0f, 3.5f,
               activeT.trackColor.r, activeT.trackColor.g, activeT.trackColor.b, 1.0f);

    drawText(r, activeT.title, targetDropdownBtn_.x + 22.0f, targetDropdownBtn_.y + 8.0f, 10.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "v", targetDropdownBtn_.x + targetDropdownBtn_.w - 14.0f, targetDropdownBtn_.y + 9.0f, 8.0f,
             activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);

    auto drawModeBtn = [&](const Rect2D& b, const std::string& label, bool active, const Color& accent) {
        if (active) {
            Color bg{accent.r * 0.35f, accent.g * 0.35f, accent.b * 0.35f, 0.95f};
            drawButton(r, b, label, bg, accent, accent, 10.0f, 3.0f, 1.2f);
        } else {
            drawButton(r, b, label, Color{0.14f, 0.15f, 0.19f, 0.85f}, Color{0.0f, 0.0f, 0.0f, 0.0f}, theme.textMuted, 10.0f, 3.0f, 0.0f);
        }
    };

    drawModeBtn(btnCode_, "<> CODE", mode_ == DesignSubMode::Code, theme.primaryAccent);
    drawModeBtn(btnModular_, "MODULAR", mode_ == DesignSubMode::ModularRack, Color{1.0f, 0.55f, 0.0f});
    drawModeBtn(btnSplit_, "SPLIT", mode_ == DesignSubMode::Split, Color{1.0f, 0.85f, 0.0f});
    drawModeBtn(btnGui_, "GUI", mode_ == DesignSubMode::GuiDesigner, Color{1.0f, 0.16f, 0.55f});

    drawButton(r, btnCompile_, "> COMPILE & RUN",
               Color{0.05f, 0.35f, 0.18f, 0.95f}, Color{0.0f, 0.95f, 0.45f, 1.0f}, Color{0.0f, 1.0f, 0.55f, 1.0f},
               10.5f, 4.0f, 1.2f);
}

void DesignView::renderExplorerSidebar(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    drawRect(r, explorerBounds_.x, explorerBounds_.y, explorerBounds_.w, explorerBounds_.h,
             0.10f, 0.11f, 0.14f, 1.0f);
    drawLine(r, explorerBounds_.x + explorerBounds_.w, explorerBounds_.y,
             explorerBounds_.x + explorerBounds_.w, explorerBounds_.y + explorerBounds_.h,
             0.18f, 0.20f, 0.26f, 1.0f, 1.2f);

    float ey = explorerBounds_.y + 8.0f;
    drawText(r, "PROJECT SCRIPTS", explorerBounds_.x + 10.0f, ey, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    drawText(r, std::to_string(allTargets_.size()), explorerBounds_.x + explorerBounds_.w - 24.0f, ey, 10.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

    ey += 20.0f;
    drawRoundedRect(r, explorerBounds_.x + 8.0f, ey, explorerBounds_.w - 16.0f, 22.0f, 3.0f,
                    0.05f, 0.06f, 0.08f, 0.9f);
    drawMonoText(r, scriptFilterQuery_.empty() ? "Filter scripts..." : scriptFilterQuery_,
                 explorerBounds_.x + 14.0f, ey + 5.0f, 9.5f,
                 scriptFilterQuery_.empty() ? theme.textMuted.r : 0.95f,
                 scriptFilterQuery_.empty() ? theme.textMuted.g : 0.95f,
                 scriptFilterQuery_.empty() ? theme.textMuted.b : 0.95f,
                 scriptFilterQuery_.empty() ? 0.6f : 1.0f);

    explorerItemBounds_.clear();
    ey += 30.0f;

    auto renderGroup = [&](const std::string& title, const Color& col, ScriptTargetType type) {
        drawText(r, title, explorerBounds_.x + 10.0f, ey, 9.0f, col.r, col.g, col.b, 1.0f);
        ey += 15.0f;

        int count = 0;
        for (size_t i = 0; i < allTargets_.size(); ++i) {
            if (allTargets_[i].type != type) continue;
            count++;

            Rect2D itemR(explorerBounds_.x + 6.0f, ey, explorerBounds_.w - 12.0f, 20.0f);
            explorerItemBounds_.push_back({static_cast<int>(i), itemR});

            bool isSel = (static_cast<int>(i) == activeTargetIndex_);
            if (isSel) {
                drawRoundedRect(r, itemR.x, itemR.y, itemR.w, itemR.h, 3.0f,
                                col.r * 0.22f, col.g * 0.22f, col.b * 0.22f, 0.85f);
                drawRoundedRectOutline(r, itemR.x, itemR.y, itemR.w, itemR.h, 3.0f,
                                       col.r, col.g, col.b, 0.8f, 1.0f);
            }

            drawCircle(r, itemR.x + 8.0f, itemR.y + 10.0f, 3.0f,
                       allTargets_[i].trackColor.r, allTargets_[i].trackColor.g, allTargets_[i].trackColor.b, 1.0f);
            drawText(r, allTargets_[i].title, itemR.x + 18.0f, itemR.y + 4.0f, 9.5f,
                     isSel ? 1.0f : 0.82f, isSel ? 1.0f : 0.82f, isSel ? 1.0f : 0.82f, 1.0f);

            ey += 22.0f;
        }

        if (count == 0) {
            drawText(r, "No modules in tracks", explorerBounds_.x + 16.0f, ey, 9.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.6f);
            ey += 18.0f;
        }

        drawLine(r, explorerBounds_.x + 10.0f, ey, explorerBounds_.x + explorerBounds_.w - 10.0f, ey,
                 0.18f, 0.20f, 0.26f, 0.7f, 1.0f);
        ey += 8.0f;
    };

    renderGroup("SYNTHS & DSP (4)", theme.primaryAccent, ScriptTargetType::TrackDsp);
    renderGroup("AUDIO FX INSERTS (3)", Color{1.0f, 0.16f, 0.55f}, ScriptTargetType::AudioFx);
    renderGroup("TRACK MIDI FX (0)", Color{1.0f, 0.85f, 0.0f}, ScriptTargetType::MidiFx);
    renderGroup("CLIP SCRIPTS (4)", Color{0.0f, 1.0f, 0.62f}, ScriptTargetType::ClipScript);
    renderGroup("BUILT-IN PRESETS (156)", Color{1.0f, 0.85f, 0.0f}, ScriptTargetType::BuiltinPreset);
}

void DesignView::renderCodeEditorCanvas(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;
    const auto& activeT = getActiveTarget();

    renderOscilloscope(ctx, scopeBounds_);

    drawRect(r, editorHeaderBounds_.x, editorHeaderBounds_.y, editorHeaderBounds_.w, editorHeaderBounds_.h,
             0.13f, 0.15f, 0.19f, 1.0f);
    drawLine(r, editorHeaderBounds_.x, editorHeaderBounds_.y + editorHeaderBounds_.h,
             editorHeaderBounds_.x + editorHeaderBounds_.w, editorHeaderBounds_.y + editorHeaderBounds_.h,
             0.22f, 0.25f, 0.32f, 1.0f, 1.0f);

    std::string targetLabel = "TARGET: " + activeT.title;
    drawText(r, targetLabel, editorHeaderBounds_.x + 10.0f, editorHeaderBounds_.y + 8.0f, 10.5f,
             activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);

    drawButton(r, btnSelectAll_, "SELECT ALL", Color{0.12f, 0.14f, 0.18f, 0.8f}, Color{0.25f, 0.28f, 0.35f, 0.8f}, Color{0.7f, 0.75f, 0.85f, 1.0f}, 9.0f, 3.0f, 1.0f);
    drawButton(r, btnCopy_, "COPY", Color{0.12f, 0.14f, 0.18f, 0.8f}, Color{0.25f, 0.28f, 0.35f, 0.8f}, Color{0.7f, 0.75f, 0.85f, 1.0f}, 9.0f, 3.0f, 1.0f);
    drawButton(r, btnSubmitPr_, "SUBMIT PR", Color{0.14f, 0.18f, 0.24f, 0.8f}, Color{theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.8f}, theme.primaryAccent, 9.0f, 3.0f, 1.0f);
    drawButton(r, btnApiDocs_, "API DOCS", Color{0.05f, 0.20f, 0.12f, 0.8f}, Color{0.0f, 0.95f, 0.45f, 0.8f}, Color{0.0f, 0.95f, 0.45f, 1.0f}, 9.0f, 3.0f, 1.0f);

    textEditor_.render(ctx);

    drawRoundedRect(r, statusBannerBounds_.x, statusBannerBounds_.y, statusBannerBounds_.w, statusBannerBounds_.h, 4.0f,
                    0.05f, 0.22f, 0.12f, 0.85f);
    drawRoundedRectOutline(r, statusBannerBounds_.x, statusBannerBounds_.y, statusBannerBounds_.w, statusBannerBounds_.h, 4.0f,
                           0.0f, 0.95f, 0.45f, 0.7f, 1.0f);
    drawText(r, "[OK] " + statusMsg_, statusBannerBounds_.x + 10.0f, statusBannerBounds_.y + 6.0f, 9.5f,
             0.0f, 1.0f, 0.55f, 1.0f);

    float titleY = liveParamsSectionBounds_.y;
    std::string paramHeader = "LIVE SCRIPT PARAMETERS (" + activeT.title + ")";
    drawText(r, paramHeader, liveParamsSectionBounds_.x, titleY, 10.5f,
             activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);

    for (size_t i = 0; i < currentParams_.size(); ++i) {
        const auto& p = currentParams_[i];
        const auto& pb = p.bounds;

        drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 4.0f, 0.12f, 0.14f, 0.18f, 0.95f);
        drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 4.0f,
                               activeT.badgeColor.r * 0.4f, activeT.badgeColor.g * 0.4f, activeT.badgeColor.b * 0.4f, 0.8f, 1.0f);

        drawText(r, p.name, pb.x + 8.0f, pb.y + 6.0f, 10.0f, 0.95f, 0.95f, 0.95f, 1.0f);

        std::ostringstream valOss;
        valOss << std::fixed << std::setprecision(1) << p.currentVal;
        drawText(r, valOss.str(), pb.x + pb.w - 36.0f, pb.y + 6.0f, 10.0f,
                 activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);

        float trackX = pb.x + 8.0f;
        float trackY = pb.y + 26.0f;
        float trackW = pb.w - 16.0f;
        float trackH = 6.0f;

        float norm = (p.maxVal > p.minVal) ? std::clamp((p.currentVal - p.minVal) / (p.maxVal - p.minVal), 0.0f, 1.0f) : 0.5f;

        drawRoundedRect(r, trackX, trackY, trackW, trackH, 3.0f, 0.06f, 0.07f, 0.09f, 1.0f);
        drawRoundedRect(r, trackX, trackY, trackW * norm, trackH, 3.0f,
                        activeT.badgeColor.r * 0.7f, activeT.badgeColor.g * 0.7f, activeT.badgeColor.b * 0.7f, 1.0f);

        float thumbX = trackX + (trackW * norm);
        drawCircle(r, thumbX, trackY + 3.0f, 6.0f, 0.95f, 0.95f, 0.95f, 1.0f);
        drawCircle(r, thumbX, trackY + 3.0f, 3.0f, activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);
    }
}

void DesignView::renderOscilloscope(const ViewContext& ctx, const Rect2D& rect) {
    auto& r = *ctx.renderer;

    drawRoundedRect(r, rect.x, rect.y, rect.w, rect.h, 4.0f, 0.02f, 0.04f, 0.03f, 1.0f);
    drawRoundedRectOutline(r, rect.x, rect.y, rect.w, rect.h, 4.0f, 0.0f, 0.65f, 0.35f, 0.8f, 1.2f);

    float gridStep = rect.w / 12.0f;
    for (float gx = rect.x + gridStep; gx < rect.x + rect.w; gx += gridStep) {
        drawLine(r, gx, rect.y, gx, rect.y + rect.h, 0.0f, 0.22f, 0.12f, 0.4f, 1.0f);
    }
    float midY = rect.y + (rect.h * 0.5f);
    drawLine(r, rect.x, midY, rect.x + rect.w, midY, 0.0f, 0.25f, 0.15f, 0.5f, 1.0f);

    size_t count = scopeSamples_.size();
    if (count > 1) {
        float prevX = rect.x;
        float prevY = midY - (scopeSamples_[0] * (rect.h * 0.42f));

        for (size_t s = 1; s < count; ++s) {
            float px = rect.x + (static_cast<float>(s) / (count - 1)) * rect.w;
            float py = midY - (scopeSamples_[s] * (rect.h * 0.42f));

            drawLine(r, prevX, prevY, px, py, 0.0f, 1.0f, 0.60f, 0.95f, 2.0f);
            prevX = px;
            prevY = py;
        }
    }

    std::string scopeTitle = "[*] OSCILLOSCOPE - " + getActiveTarget().title;
    drawText(r, scopeTitle, rect.x + 10.0f, rect.y + 6.0f, 9.0f, 0.0f, 1.0f, 0.55f, 1.0f);
}

void DesignView::renderModularRack(const ViewContext& ctx, const Rect2D& rect) {
    auto& r = *ctx.renderer;

    // Dark chassis background
    drawRect(r, rect.x, rect.y, rect.w, rect.h, 0.11f, 0.12f, 0.15f, 1.0f);

    // Top modular sub-toolbar
    float topBarH = 34.0f;
    modularToolbarBounds_ = Rect2D(rect.x, rect.y, rect.w, topBarH);
    drawRect(r, modularToolbarBounds_.x, modularToolbarBounds_.y, modularToolbarBounds_.w, modularToolbarBounds_.h,
             0.09f, 0.10f, 0.13f, 1.0f);
    drawLine(r, modularToolbarBounds_.x, modularToolbarBounds_.y + modularToolbarBounds_.h,
             modularToolbarBounds_.x + modularToolbarBounds_.w, modularToolbarBounds_.y + modularToolbarBounds_.h,
             0.18f, 0.20f, 0.26f, 1.0f, 1.2f);

    std::string rackTitle = "MODULAR RACK: " + getActiveTarget().title;
    drawText(r, rackTitle, modularToolbarBounds_.x + 14.0f, modularToolbarBounds_.y + 10.0f, 10.5f,
             1.0f, 0.55f, 0.0f, 1.0f);

    // DSP Sync indicator
    drawRoundedRect(r, modularToolbarBounds_.x + 280.0f, modularToolbarBounds_.y + 6.0f, 110.0f, 22.0f, 3.0f,
                    0.05f, 0.22f, 0.12f, 0.9f);
    drawRoundedRectOutline(r, modularToolbarBounds_.x + 280.0f, modularToolbarBounds_.y + 6.0f, 110.0f, 22.0f, 3.0f,
                           0.0f, 0.95f, 0.45f, 0.7f, 1.0f);
    drawText(r, "* DSP SYNC: OK", modularToolbarBounds_.x + 288.0f, modularToolbarBounds_.y + 11.0f, 9.0f,
             0.0f, 1.0f, 0.55f, 1.0f);

    // Add module & reset patch buttons
    btnModularResetPatch_ = Rect2D(modularToolbarBounds_.x + modularToolbarBounds_.w - 110.0f, modularToolbarBounds_.y + 6.0f, 98.0f, 22.0f);
    btnModularAddModule_ = Rect2D(btnModularResetPatch_.x - 118.0f, modularToolbarBounds_.y + 6.0f, 108.0f, 22.0f);

    drawButton(r, btnModularAddModule_, "+ ADD MODULE",
               Color{0.14f, 0.17f, 0.23f, 0.9f}, Color{1.0f, 0.85f, 0.0f, 0.6f}, Color{1.0f, 0.85f, 0.0f, 1.0f}, 9.5f, 3.0f, 1.0f);

    drawButton(r, btnModularResetPatch_, "RESET PATCH",
               Color{0.14f, 0.17f, 0.23f, 0.9f}, Color{0.25f, 0.28f, 0.35f, 0.8f}, Color{0.8f, 0.85f, 0.95f, 1.0f}, 9.5f, 3.0f, 1.0f);

    // Steel rack mounting rails top and bottom with screw holes
    float railH = 14.0f;
    float rackBodyY = rect.y + topBarH;
    float rackBodyH = rect.h - topBarH;
    drawRect(r, rect.x, rackBodyY, rect.w, railH, 0.22f, 0.23f, 0.28f, 1.0f);
    drawRect(r, rect.x, rackBodyY + rackBodyH - railH, rect.w, railH, 0.22f, 0.23f, 0.28f, 1.0f);

    for (float sx = rect.x + 16.0f; sx < rect.x + rect.w; sx += 40.0f) {
        drawCircle(r, sx, rackBodyY + 7.0f, 2.5f, 0.45f, 0.48f, 0.55f, 1.0f);
        drawCircle(r, sx, rackBodyY + rackBodyH - 7.0f, 2.5f, 0.45f, 0.48f, 0.55f, 1.0f);
    }

    // Position modules horizontally
    float modX = rect.x + 24.0f;
    float modY = rackBodyY + railH + 10.0f;
    float modH = rackBodyH - (railH * 2.0f) - 20.0f;

    for (size_t m = 0; m < modules_.size(); ++m) {
        auto& mod = modules_[m];
        mod.x = modX;
        mod.y = modY;
        mod.h = modH;

        // Faceplate chassis
        drawRoundedRect(r, mod.x, mod.y, mod.w, mod.h, 4.0f, mod.r, mod.g, mod.b, 1.0f);
        drawRoundedRectOutline(r, mod.x, mod.y, mod.w, mod.h, 4.0f, 0.42f, 0.45f, 0.52f, 1.0f, 1.5f);

        // Corner screws
        drawCircle(r, mod.x + 8.0f, mod.y + 8.0f, 3.0f, 0.6f, 0.62f, 0.7f, 1.0f);
        drawCircle(r, mod.x + mod.w - 8.0f, mod.y + 8.0f, 3.0f, 0.6f, 0.62f, 0.7f, 1.0f);
        drawCircle(r, mod.x + 8.0f, mod.y + mod.h - 8.0f, 3.0f, 0.6f, 0.62f, 0.7f, 1.0f);
        drawCircle(r, mod.x + mod.w - 8.0f, mod.y + mod.h - 8.0f, 3.0f, 0.6f, 0.62f, 0.7f, 1.0f);

        // Title banner
        drawRect(r, mod.x, mod.y, mod.w, 24.0f, 0.14f, 0.15f, 0.18f, 1.0f);
        drawText(r, mod.title, mod.x + 12.0f, mod.y + 6.0f, 10.0f, 0.0f, 0.95f, 1.0f, 1.0f);

        // Knobs
        for (size_t k = 0; k < mod.knobs.size(); ++k) {
            float kx = mod.x + 40.0f + ((k % 2) * 85.0f);
            float ky = mod.y + 65.0f + ((k / 2) * 70.0f);

            bool isDragging = (draggingModuleKnobMod_ == static_cast<int>(m) &&
                               draggingModuleKnobIdx_ == static_cast<int>(k));

            // Outer dial glow if dragging
            if (isDragging) {
                drawCircle(r, kx, ky, 20.0f, 1.0f, 0.85f, 0.0f, 0.35f);
            }

            // Authentic Eatsbeats 3D knob with radial gradient body & lighting
            // Skirt bevel
            drawCircle(r, kx, ky, 16.0f, 0.08f, 0.09f, 0.11f, 1.0f);
            drawCircleOutline(r, kx, ky, 16.0f, 0.20f, 0.22f, 0.26f, 0.7f, 1.0f);

            // Core cylinder body with top-left overhead 3D radial gradient
            float offX = -0.25f * 13.0f;
            float offY = -0.30f * 13.0f;
            drawCircleRadial3StopGradient(r, kx, ky, 13.0f,
                                          Color(0.42f, 0.45f, 0.52f, 1.0f),  // bodyLight
                                          Color(0.24f, 0.26f, 0.31f, 1.0f),  // bodyColor
                                          Color(0.12f, 0.13f, 0.16f, 1.0f),  // bodyDark
                                          offX, offY, 0.50f, 32);
            drawCircleOutline(r, kx, ky, 13.0f, 0.08f, 0.09f, 0.12f, 0.6f, 1.0f);

            float ang = -2.356f + (mod.knobs[k].value * 4.712f);
            float nx = kx + std::sin(ang) * 11.0f;
            float ny = ky - std::cos(ang) * 11.0f;
            drawLine(r, kx, ky, nx, ny, isDragging ? 0.0f : 1.0f, isDragging ? 1.0f : 0.85f, 0.0f, 1.0f, 2.5f);

            drawText(r, mod.knobs[k].label, kx - 16.0f, ky + 18.0f, 8.5f, 0.8f, 0.85f, 0.9f, 0.9f);

            // Readout
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << mod.knobs[k].value;
            drawText(r, ss.str(), kx - 12.0f, ky + 29.0f, 7.5f, 0.6f, 0.7f, 0.8f, 0.8f);
        }

        // Jacks: Inputs (Cyan) on Left, Outputs (Gold) on Right
        float jackY = mod.y + mod.h - 45.0f;

        for (size_t i = 0; i < mod.inputs.size(); ++i) {
            float jx = mod.x + 30.0f + (i * 45.0f);

            // Check if connected
            bool isConnected = false;
            Color wireColor{0.13f, 0.96f, 0.91f};
            for (const auto& c : patchCords_) {
                if (c.destModule == static_cast<int>(m) && c.destJack == static_cast<int>(i)) {
                    isConnected = true;
                    wireColor = Color{c.r, c.g, c.b};
                    break;
                }
            }

            drawCircle(r, jx, jackY, 12.0f, 0.12f, 0.13f, 0.16f, 1.0f);
            drawCircle(r, jx, jackY, 8.0f, 0.0f, 0.95f, 1.0f, 1.0f);
            drawCircle(r, jx, jackY, 4.0f, 0.02f, 0.02f, 0.03f, 1.0f);

            if (isConnected) {
                drawCircle(r, jx, jackY, 14.0f, wireColor.r, wireColor.g, wireColor.b, 0.4f);
            }

            drawText(r, mod.inputs[i], jx - 12.0f, jackY - 22.0f, 7.5f, 0.0f, 0.95f, 1.0f, 0.9f);
        }

        for (size_t o = 0; o < mod.outputs.size(); ++o) {
            float jx = mod.x + mod.w - 30.0f - (o * 45.0f);

            bool isConnected = false;
            Color wireColor{1.0f, 0.55f, 0.0f};
            for (const auto& c : patchCords_) {
                if (c.sourceModule == static_cast<int>(m) && c.sourceJack == static_cast<int>(o)) {
                    isConnected = true;
                    wireColor = Color{c.r, c.g, c.b};
                    break;
                }
            }

            drawCircle(r, jx, jackY, 12.0f, 0.12f, 0.13f, 0.16f, 1.0f);
            drawCircle(r, jx, jackY, 8.0f, 1.0f, 0.55f, 0.0f, 1.0f);
            drawCircle(r, jx, jackY, 4.0f, 0.02f, 0.02f, 0.03f, 1.0f);

            if (isConnected) {
                drawCircle(r, jx, jackY, 14.0f, wireColor.r, wireColor.g, wireColor.b, 0.4f);
            }

            drawText(r, mod.outputs[o], jx - 10.0f, jackY - 22.0f, 7.5f, 1.0f, 0.55f, 0.0f, 0.9f);
        }

        modX += mod.w + 16.0f;
    }

    // Draw Catenary Patch Cords
    for (const auto& cord : patchCords_) {
        if (cord.sourceModule >= static_cast<int>(modules_.size()) ||
            cord.destModule >= static_cast<int>(modules_.size())) continue;

        const auto& sm = modules_[cord.sourceModule];
        const auto& dm = modules_[cord.destModule];

        float x1 = sm.x + sm.w - 30.0f - (cord.sourceJack * 45.0f);
        float y1 = sm.y + sm.h - 45.0f;

        float x2 = dm.x + 30.0f + (cord.destJack * 45.0f);
        float y2 = dm.y + dm.h - 45.0f;

        renderCatenaryCable(r, x1, y1, x2, y2, cord.r, cord.g, cord.b);
    }

    // Active live dragged cord
    if (isDraggingCord_) {
        renderCatenaryCable(r, cordStartX_, cordStartY_, cordCurrentX_, cordCurrentY_, dragCordR_, dragCordG_, dragCordB_);
    }
}

void DesignView::renderCatenaryCable(BatchRenderer2D& r, float x1, float y1, float x2, float y2,
                                     float cr, float cg, float cb) {
    constexpr int kSteps = 24;
    float sag = 50.0f + std::abs(x2 - x1) * 0.12f;

    float prevX = x1;
    float prevY = y1;

    for (int i = 1; i <= kSteps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSteps);
        float px = x1 + (x2 - x1) * t;
        float py = y1 + (y2 - y1) * t + (4.0f * sag * t * (1.0f - t));

        // Drop shadow for 3D realism
        drawLine(r, prevX, prevY + 6.0f, px, py + 6.0f, 0.02f, 0.02f, 0.03f, 0.35f, 5.0f);
        // Core cable body
        drawLine(r, prevX, prevY, px, py, cr, cg, cb, 0.95f, 4.0f);
        // Specular highlight line
        drawLine(r, prevX, prevY - 1.0f, px, py - 1.0f, 1.0f, 1.0f, 1.0f, 0.40f, 1.5f);

        prevX = px;
        prevY = py;
    }
}

void DesignView::renderSplitView(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    float splitW = studioBodyBounds_.w * 0.45f;
    Rect2D leftBounds(studioBodyBounds_.x, studioBodyBounds_.y, splitW, studioBodyBounds_.h);
    Rect2D rightBounds(studioBodyBounds_.x + splitW, studioBodyBounds_.y, studioBodyBounds_.w - splitW, studioBodyBounds_.h);

    renderCodeEditorCanvas(ctx);

    drawLine(r, rightBounds.x, rightBounds.y, rightBounds.x, rightBounds.y + rightBounds.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.9f, 2.0f);

    drawRect(r, splitSubBarBounds_.x, splitSubBarBounds_.y, splitSubBarBounds_.w, splitSubBarBounds_.h,
             0.09f, 0.10f, 0.13f, 1.0f);

    auto drawSplitBtn = [&](const Rect2D& b, const std::string& label, bool active, const Color& accent) {
        if (active) {
            Color bg{accent.r * 0.3f, accent.g * 0.3f, accent.b * 0.3f, 0.9f};
            drawButton(r, b, label, bg, accent, accent, 9.0f, 3.0f, 1.0f);
        } else {
            drawButton(r, b, label, Color{0.14f, 0.15f, 0.18f, 0.8f}, Color{0.0f, 0.0f, 0.0f, 0.0f}, theme.textMuted, 9.0f, 3.0f, 0.0f);
        }
    };

    drawSplitBtn(btnSplitFaceplate_, "FACEPLATE", splitShowsGui_, Color{1.0f, 0.16f, 0.55f});
    drawSplitBtn(btnSplitModular_, "MODULAR", !splitShowsGui_, Color{1.0f, 0.55f, 0.0f});

    Rect2D rightContent(rightBounds.x, rightBounds.y + splitSubBarBounds_.h, rightBounds.w, rightBounds.h - splitSubBarBounds_.h);

    if (splitShowsGui_) {
        renderHardwareFaceplate(ctx, rightContent);
    } else {
        renderModularRack(ctx, rightContent);
    }
}

void DesignView::renderGuiPreview(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // Top Sub-bar for GUI Tab: Title + [● LIVE INTERACTION] vs [DESIGN MODE] + [SUBMIT PR]
    drawRect(r, guiSubBarBounds_.x, guiSubBarBounds_.y, guiSubBarBounds_.w, guiSubBarBounds_.h,
             0.09f, 0.10f, 0.13f, 1.0f);
    drawLine(r, guiSubBarBounds_.x, guiSubBarBounds_.y + guiSubBarBounds_.h,
             guiSubBarBounds_.x + guiSubBarBounds_.w, guiSubBarBounds_.y + guiSubBarBounds_.h,
             0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

    std::string guiTitle = "GUI INTERFACE: " + getActiveTarget().title;
    drawText(r, guiTitle, guiSubBarBounds_.x + 14.0f, guiSubBarBounds_.y + 9.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    auto drawSubBtn = [&](const Rect2D& b, const std::string& label, bool active, const Color& accent) {
        if (active) {
            Color bg{accent.r * 0.3f, accent.g * 0.3f, accent.b * 0.3f, 0.95f};
            drawButton(r, b, label, bg, accent, accent, 9.5f, 3.0f, 1.0f);
        } else {
            drawButton(r, b, label, Color{0.14f, 0.16f, 0.20f, 0.8f}, Color{0.0f, 0.0f, 0.0f, 0.0f}, theme.textMuted, 9.5f, 3.0f, 0.0f);
        }
    };

    drawSubBtn(btnGuiLive_, "LIVE INTERACTION", !isGuiDesignMode_, theme.primaryAccent);
    drawSubBtn(btnGuiDesign_, "DESIGN MODE", isGuiDesignMode_, Color{0.0f, 0.95f, 0.45f});

    Color prBg{theme.primaryAccent.r * 0.2f, theme.primaryAccent.g * 0.2f, theme.primaryAccent.b * 0.2f, 0.8f};
    Color prBorder{theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.6f};
    drawButton(r, btnGuiPr_, "SUBMIT PR", prBg, prBorder, theme.primaryAccent, 9.5f, 3.0f, 1.0f);

    Rect2D guiBody(studioBodyBounds_.x, studioBodyBounds_.y + guiSubBarBounds_.h, studioBodyBounds_.w, studioBodyBounds_.h - guiSubBarBounds_.h);

    if (isGuiDesignMode_) {
        renderGuiDesigner(ctx, guiBody);
    } else {
        renderHardwareFaceplate(ctx, guiBody);
    }
}

void DesignView::renderHardwareFaceplate(const ViewContext& ctx, const Rect2D& rect) {
    auto& r = *ctx.renderer;

    float fpW = std::min(rect.w - 48.0f, 720.0f);
    float fpH = std::min(rect.h - 40.0f, 440.0f);
    float fpX = rect.x + (rect.w - fpW) * 0.5f;
    float fpY = rect.y + (rect.h - fpH) * 0.5f;

    Rect2D fpRect{fpX, fpY, fpW, fpH};
    const auto& theme = *ctx.theme;
    drawGuiFaceplate(r, guiPanel_, fpRect, theme, nullptr, 0, draggingGuiWidgetRow_, draggingGuiWidgetIdx_);
}

void DesignView::renderGuiDesigner(const ViewContext& ctx, const Rect2D& rect) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // 1. Top Designer Toolbar
    drawRect(r, designerToolbarBounds_.x, designerToolbarBounds_.y, designerToolbarBounds_.w, designerToolbarBounds_.h,
             0.09f, 0.10f, 0.13f, 1.0f);
    drawLine(r, designerToolbarBounds_.x, designerToolbarBounds_.y + designerToolbarBounds_.h,
             designerToolbarBounds_.x + designerToolbarBounds_.w, designerToolbarBounds_.y + designerToolbarBounds_.h,
             0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

    // Designer buttons
    drawButton(r, btnTogglePalette_, isPaletteOpen_ ? "TOOLBOX <" : "TOOLBOX >",
               Color{0.14f, 0.16f, 0.20f, 0.8f}, isPaletteOpen_ ? theme.primaryAccent : Color{0.25f, 0.28f, 0.35f, 0.8f},
               isPaletteOpen_ ? theme.primaryAccent : theme.textMuted, 9.0f, 3.0f, 1.0f);

    drawButton(r, btnAddRow_, "+ ADD ROW",
               Color{0.12f, 0.24f, 0.18f, 0.85f}, Color{0.0f, 0.95f, 0.45f, 0.8f}, Color{0.0f, 1.0f, 0.55f, 1.0f},
               9.0f, 3.0f, 1.0f);

    if (selectedDesignerRow_ >= 0) {
        drawButton(r, btnDeleteRow_, "- DEL ROW",
                   Color{0.24f, 0.12f, 0.14f, 0.85f}, Color{1.0f, 0.35f, 0.35f, 0.8f}, Color{1.0f, 0.5f, 0.5f, 1.0f},
                   9.0f, 3.0f, 1.0f);
    }

    if (selectedDesignerWidget_ >= 0) {
        drawButton(r, btnDuplicateWidget_, "DUPLICATE",
                   Color{0.14f, 0.18f, 0.24f, 0.85f}, Color{1.0f, 0.85f, 0.0f, 0.8f}, Color{1.0f, 0.85f, 0.0f, 1.0f},
                   9.0f, 3.0f, 1.0f);

        drawButton(r, btnDeleteWidget_, "DELETE",
                   Color{0.28f, 0.10f, 0.12f, 0.9f}, Color{1.0f, 0.20f, 0.25f, 0.9f}, Color{1.0f, 0.30f, 0.35f, 1.0f},
                   9.0f, 3.0f, 1.0f);
    }

    drawButton(r, btnToggleInspector_, isInspectorOpen_ ? "INSPECTOR >" : "INSPECTOR <",
               Color{0.14f, 0.16f, 0.20f, 0.8f}, isInspectorOpen_ ? Color{1.0f, 0.85f, 0.0f, 0.8f} : Color{0.25f, 0.28f, 0.35f, 0.8f},
               isInspectorOpen_ ? Color{1.0f, 0.85f, 0.0f, 1.0f} : theme.textMuted, 9.0f, 3.0f, 1.0f);

    float bodyY = designerToolbarBounds_.y + designerToolbarBounds_.h;
    float bodyH = rect.h - designerToolbarBounds_.h;

    float palW = isPaletteOpen_ ? 190.0f : 0.0f;
    float inspW = isInspectorOpen_ ? 230.0f : 0.0f;

    // 2. Left Widget Palette Drawer
    paletteItems_.clear();
    if (isPaletteOpen_) {
        Rect2D palR(rect.x, bodyY, palW, bodyH);
        drawRect(r, palR.x, palR.y, palR.w, palR.h, 0.10f, 0.11f, 0.14f, 1.0f);
        drawLine(r, palR.x + palR.w, palR.y, palR.x + palR.w, palR.y + palR.h, 0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

        float py = palR.y + 10.0f;
        drawText(r, "WIDGET TOOLBOX", palR.x + 12.0f, py, 10.5f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
        py += 22.0f;

        auto addPaletteItem = [&](const std::string& id, const std::string& title, const std::string& cat, GuiWidgetType type, GuiKnobStyle kStyle) {
            Rect2D itemR(palR.x + 8.0f, py, palR.w - 16.0f, 26.0f);
            paletteItems_.push_back({id, title, cat, type, kStyle, itemR});

            drawRoundedRect(r, itemR.x, itemR.y, itemR.w, itemR.h, 4.0f, 0.14f, 0.16f, 0.20f, 0.9f);
            drawRoundedRectOutline(r, itemR.x, itemR.y, itemR.w, itemR.h, 4.0f, 0.22f, 0.25f, 0.32f, 0.8f, 1.0f);
            drawText(r, "+ " + title, itemR.x + 10.0f, itemR.y + 8.0f, 9.0f, 0.92f, 0.94f, 0.98f, 1.0f);

            py += 30.0f;
        };

        drawText(r, "HARDWARE CONSOLE (5)", palR.x + 10.0f, py, 8.5f, 1.0f, 0.85f, 0.0f, 1.0f);
        py += 15.0f;
        addPaletteItem("hw_cream", "Cream Fluted Dial", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::CreamFluted);
        addPaletteItem("hw_bakelite", "Bakelite Skirt Dial", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::BakeliteSkirt);
        addPaletteItem("hw_knurled", "Anodized Knurled Dial", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::AnodizedKnurled);
        addPaletteItem("hw_stepped", "Two-Tone Stepped Dial", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::TwoToneStepped);
        addPaletteItem("hw_halo", "TB-303 Acid Halo", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::Tb303Halo);

        drawText(r, "CONTROLS (4)", palR.x + 10.0f, py, 8.5f, 0.0f, 0.95f, 1.0f, 1.0f);
        py += 15.0f;
        addPaletteItem("ctl_knob", "Rotary Knob", "CONTROLS", GuiWidgetType::Knob, GuiKnobStyle::Standard);
        addPaletteItem("ctl_slider", "Hardware Slider", "CONTROLS", GuiWidgetType::Slider, GuiKnobStyle::Standard);
        addPaletteItem("ctl_switch", "Toggle Switch", "CONTROLS", GuiWidgetType::ToggleSwitch, GuiKnobStyle::Standard);
        addPaletteItem("ctl_button", "Action Button", "CONTROLS", GuiWidgetType::ActionButton, GuiKnobStyle::Standard);

        drawText(r, "DISPLAYS & METERS (3)", palR.x + 10.0f, py, 8.5f, 0.0f, 1.0f, 0.55f, 1.0f);
        py += 15.0f;
        addPaletteItem("dsp_nixie", "Nixie Tube Display", "DISPLAYS", GuiWidgetType::NixieDisplay, GuiKnobStyle::Standard);
        addPaletteItem("dsp_vu", "LED VU Level Meter", "DISPLAYS", GuiWidgetType::VuMeter, GuiKnobStyle::Standard);
        addPaletteItem("dsp_scope", "Waveform Scope", "DISPLAYS", GuiWidgetType::ScopeScreen, GuiKnobStyle::Standard);
    }

    // 3. Right Property Inspector Sidebar
    inspectorThemeBtns_.clear();
    inspectorAccentBtns_.clear();
    inspectorParamBtns_.clear();
    inspectorKnobStyleBtns_.clear();
    inspectorSizeBtns_.clear();

    if (isInspectorOpen_) {
        Rect2D inspR(rect.x + rect.w - inspW, bodyY, inspW, bodyH);
        drawRect(r, inspR.x, inspR.y, inspR.w, inspR.h, 0.10f, 0.11f, 0.14f, 1.0f);
        drawLine(r, inspR.x, inspR.y, inspR.x, inspR.y + inspR.h, 0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

        float iy = inspR.y + 10.0f;

        if (isChassisSelected_ || (selectedDesignerRow_ < 0 && selectedDesignerWidget_ < 0)) {
            // Chassis Properties
            drawText(r, "CHASSIS SETTINGS", inspR.x + 12.0f, iy, 10.5f, 1.0f, 0.85f, 0.0f, 1.0f);
            iy += 22.0f;

            drawText(r, "Title: " + guiPanel_.title, inspR.x + 12.0f, iy, 9.5f, 0.9f, 0.9f, 0.9f, 1.0f);
            iy += 16.0f;
            drawText(r, "Subtitle: " + guiPanel_.subtitle, inspR.x + 12.0f, iy, 8.5f, 0.7f, 0.75f, 0.85f, 0.9f);
            iy += 24.0f;

            drawText(r, "BACKGROUND THEME", inspR.x + 12.0f, iy, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
            iy += 14.0f;

            auto addThemeChip = [&](const std::string& name, GuiChassisStyle style) {
                Rect2D chipR(inspR.x + 8.0f, iy, inspR.w - 16.0f, 22.0f);
                inspectorThemeBtns_.push_back({style, chipR});
                bool isSel = (guiPanel_.chassisStyle == style);
                drawRoundedRect(r, chipR.x, chipR.y, chipR.w, chipR.h, 3.0f,
                                isSel ? 0.22f : 0.14f, isSel ? 0.26f : 0.16f, isSel ? 0.35f : 0.20f, 0.9f);
                drawRoundedRectOutline(r, chipR.x, chipR.y, chipR.w, chipR.h, 3.0f,
                                       isSel ? 0.0f : 0.2f, isSel ? 0.95f : 0.25f, isSel ? 1.0f : 0.3f, 0.8f, 1.0f);
                drawText(r, name, chipR.x + 8.0f, chipR.y + 6.0f, 8.5f, isSel ? 1.0f : 0.8f, isSel ? 1.0f : 0.8f, isSel ? 1.0f : 0.8f, 1.0f);
                iy += 25.0f;
            };

            addThemeChip("Dark Chassis", GuiChassisStyle::DarkChassis);
            addThemeChip("PCB Green", GuiChassisStyle::PcbGreen);
            addThemeChip("Silver Console", GuiChassisStyle::Silver);
            addThemeChip("SNES Vintage", GuiChassisStyle::Snes);
            addThemeChip("Aged Grunge", GuiChassisStyle::Grunge);
            addThemeChip("Walnut Wood", GuiChassisStyle::Walnut);
            addThemeChip("Rosewood", GuiChassisStyle::Rosewood);
            addThemeChip("Brushed Steel", GuiChassisStyle::BrushedSteel);
            addThemeChip("Carbon Fibre", GuiChassisStyle::Carbon);

            iy += 6.0f;
            drawText(r, "ACCENT COLOR", inspR.x + 12.0f, iy, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
            iy += 14.0f;

            Color accents[5] = {{0.0f, 0.95f, 1.0f}, {1.0f, 0.85f, 0.0f}, {1.0f, 0.16f, 0.55f}, {0.0f, 1.0f, 0.55f}, {1.0f, 0.55f, 0.0f}};
            for (int a = 0; a < 5; ++a) {
                Rect2D accR(inspR.x + 10.0f + (a * 38.0f), iy, 32.0f, 20.0f);
                inspectorAccentBtns_.push_back({accents[a], accR});
                drawRoundedRect(r, accR.x, accR.y, accR.w, accR.h, 3.0f, accents[a].r, accents[a].g, accents[a].b, 1.0f);
            }
            iy += 30.0f;

            inspectorWoodCheeksBtn_ = Rect2D(inspR.x + 10.0f, iy, inspR.w - 20.0f, 24.0f);
            drawButton(r, inspectorWoodCheeksBtn_, guiPanel_.woodCheeks ? "WOOD CHEEKS: ENABLED" : "WOOD CHEEKS: DISABLED",
                       Color{0.16f, 0.18f, 0.24f, 0.9f}, guiPanel_.woodCheeks ? Color{0.0f, 0.95f, 0.45f, 0.8f} : Color{0.3f, 0.3f, 0.3f, 0.8f},
                       guiPanel_.woodCheeks ? Color{0.0f, 1.0f, 0.55f, 1.0f} : Color{0.7f, 0.7f, 0.7f, 1.0f}, 8.5f, 3.0f, 1.0f);

        } else if (selectedDesignerRow_ >= 0 && selectedDesignerRow_ < static_cast<int>(guiPanel_.rows.size())) {
            auto& row = guiPanel_.rows[selectedDesignerRow_];
            if (selectedDesignerWidget_ >= 0 && selectedDesignerWidget_ < static_cast<int>(row.widgets.size())) {
                auto& w = row.widgets[selectedDesignerWidget_];

                drawText(r, "WIDGET PROPERTIES", inspR.x + 12.0f, iy, 10.5f, 0.0f, 0.95f, 1.0f, 1.0f);
                iy += 22.0f;

                drawText(r, "ID: " + w.id, inspR.x + 12.0f, iy, 9.0f, 0.7f, 0.75f, 0.85f, 0.8f);
                iy += 16.0f;
                drawText(r, "Label: " + w.label, inspR.x + 12.0f, iy, 9.5f, 0.95f, 0.95f, 0.95f, 1.0f);
                iy += 20.0f;

                // Parameter Binding Selector
                drawText(r, "BOUND PARAMETER", inspR.x + 12.0f, iy, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
                iy += 14.0f;

                for (const auto& p : currentParams_) {
                    Rect2D pR(inspR.x + 8.0f, iy, inspR.w - 16.0f, 20.0f);
                    inspectorParamBtns_.push_back({p.name, pR});
                    bool isBound = (w.param == p.name);
                    drawRoundedRect(r, pR.x, pR.y, pR.w, pR.h, 3.0f, isBound ? 0.0f : 0.14f, isBound ? 0.35f : 0.16f, isBound ? 0.45f : 0.20f, 0.9f);
                    drawText(r, p.name, pR.x + 8.0f, pR.y + 5.0f, 8.5f, isBound ? 1.0f : 0.8f, isBound ? 1.0f : 0.8f, isBound ? 1.0f : 0.8f, 1.0f);
                    iy += 23.0f;
                }

                // Knob Style selector
                if (w.type == GuiWidgetType::Knob) {
                    iy += 8.0f;
                    drawText(r, "DIAL STYLE", inspR.x + 12.0f, iy, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
                    iy += 14.0f;

                    auto addKStyle = [&](const std::string& name, GuiKnobStyle st) {
                        Rect2D sR(inspR.x + 8.0f, iy, inspR.w - 16.0f, 20.0f);
                        inspectorKnobStyleBtns_.push_back({st, sR});
                        bool isSel = (w.knobStyle == st);
                        drawRoundedRect(r, sR.x, sR.y, sR.w, sR.h, 3.0f, isSel ? 0.25f : 0.14f, isSel ? 0.22f : 0.16f, isSel ? 0.10f : 0.20f, 0.9f);
                        drawText(r, name, sR.x + 8.0f, sR.y + 5.0f, 8.5f, isSel ? 1.0f : 0.8f, isSel ? 0.85f : 0.8f, isSel ? 0.0f : 0.8f, 1.0f);
                        iy += 23.0f;
                    };

                    addKStyle("Cream Fluted", GuiKnobStyle::CreamFluted);
                    addKStyle("Bakelite Skirt", GuiKnobStyle::BakeliteSkirt);
                    addKStyle("Anodized Knurled", GuiKnobStyle::AnodizedKnurled);
                    addKStyle("Two-Tone Stepped", GuiKnobStyle::TwoToneStepped);
                    addKStyle("TB-303 Acid Halo", GuiKnobStyle::Tb303Halo);
                    addKStyle("Standard Dial", GuiKnobStyle::Standard);
                }

                // Size chips
                iy += 8.0f;
                drawText(r, "SIZE", inspR.x + 12.0f, iy, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
                iy += 14.0f;

                float sizes[3] = {44.0f, 56.0f, 68.0f};
                const char* sLabels[3] = {"S (44)", "M (56)", "L (68)"};
                for (int s = 0; s < 3; ++s) {
                    Rect2D szR(inspR.x + 10.0f + (s * 68.0f), iy, 62.0f, 22.0f);
                    inspectorSizeBtns_.push_back({sizes[s], szR});
                    bool isSel = (std::abs(w.size - sizes[s]) < 2.0f);
                    drawRoundedRect(r, szR.x, szR.y, szR.w, szR.h, 3.0f, isSel ? 0.22f : 0.14f, isSel ? 0.26f : 0.16f, isSel ? 0.35f : 0.20f, 0.9f);
                    drawCenteredText(r, sLabels[s], szR.x, szR.y + 4.0f, szR.w, 14.0f, 8.5f, isSel ? 1.0f : 0.8f, isSel ? 1.0f : 0.8f, isSel ? 1.0f : 0.8f, 1.0f);
                }
                iy += 32.0f;

                inspectorDuplicateBtn_ = Rect2D(inspR.x + 10.0f, iy, inspR.w - 20.0f, 24.0f);
                drawButton(r, inspectorDuplicateBtn_, "DUPLICATE WIDGET",
                           Color{0.14f, 0.18f, 0.24f, 0.9f}, Color{1.0f, 0.85f, 0.0f, 0.8f}, Color{1.0f, 0.85f, 0.0f, 1.0f}, 8.5f, 3.0f, 1.0f);
                iy += 28.0f;

                inspectorDeleteBtn_ = Rect2D(inspR.x + 10.0f, iy, inspR.w - 20.0f, 24.0f);
                drawButton(r, inspectorDeleteBtn_, "DELETE WIDGET",
                           Color{0.28f, 0.10f, 0.12f, 0.9f}, Color{1.0f, 0.20f, 0.25f, 0.9f}, Color{1.0f, 0.30f, 0.35f, 1.0f}, 8.5f, 3.0f, 1.0f);
            }
        }
    }

    // 4. Center Canvas Editor Area
    Rect2D centerCanvas(rect.x + palW, bodyY, rect.w - palW - inspW, bodyH);
    drawRect(r, centerCanvas.x, centerCanvas.y, centerCanvas.w, centerCanvas.h, 0.07f, 0.08f, 0.10f, 1.0f);

    // Render Faceplate preview inside center area
    renderHardwareFaceplate(ctx, centerCanvas);

    // In Design Mode: render selection highlights and dashed bounds
    if (isChassisSelected_) {
        drawRoundedRectOutline(r, guiPanel_.bounds.x - 3.0f, guiPanel_.bounds.y - 3.0f,
                               guiPanel_.bounds.w + 6.0f, guiPanel_.bounds.h + 6.0f, 10.0f,
                               1.0f, 0.85f, 0.0f, 0.95f, 2.5f);
    }

    for (size_t rIdx = 0; rIdx < guiPanel_.rows.size(); ++rIdx) {
        const auto& row = guiPanel_.rows[rIdx];
        bool isRowSel = (selectedDesignerRow_ == static_cast<int>(rIdx));

        drawRoundedRectOutline(r, row.bounds.x, row.bounds.y, row.bounds.w, row.bounds.h, 4.0f,
                               isRowSel ? 0.0f : 0.3f, isRowSel ? 0.95f : 0.35f, isRowSel ? 0.45f : 0.45f,
                               isRowSel ? 0.9f : 0.4f, isRowSel ? 1.5f : 1.0f);

        for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
            const auto& w = row.widgets[wIdx];
            bool isWidSel = (isRowSel && selectedDesignerWidget_ == static_cast<int>(wIdx));

            if (isWidSel) {
                drawRoundedRectOutline(r, w.bounds.x - 2.0f, w.bounds.y - 2.0f, w.bounds.w + 4.0f, w.bounds.h + 4.0f, 4.0f,
                                       0.0f, 0.95f, 1.0f, 1.0f, 2.0f);
            }
        }
    }
}

bool DesignView::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    if (ev.action == PointerAction::Down) {
        // 1. Check Sub-Nav Header clicks
        if (explorerToggleBtn_.contains(ev.x, ev.y)) {
            isExplorerOpen_ = !isExplorerOpen_;
            layout(bounds_, ctx);
            return true;
        }

        if (targetDropdownBtn_.contains(ev.x, ev.y)) {
            isTargetDropdownOpen_ = !isTargetDropdownOpen_;
            return true;
        }

        if (isTargetDropdownOpen_) {
            for (size_t i = 0; i < targetDropdownItemBounds_.size(); ++i) {
                if (targetDropdownItemBounds_[i].contains(ev.x, ev.y)) {
                    selectTargetByIndex(static_cast<int>(i));
                    isTargetDropdownOpen_ = false;
                    layout(bounds_, ctx);
                    return true;
                }
            }
            isTargetDropdownOpen_ = false;
        }

        // Mode switchers
        if (btnCode_.contains(ev.x, ev.y)) {
            setSubMode(DesignSubMode::Code);
            layout(bounds_, ctx);
            return true;
        }
        if (btnModular_.contains(ev.x, ev.y)) {
            setSubMode(DesignSubMode::ModularRack);
            layout(bounds_, ctx);
            return true;
        }
        if (btnSplit_.contains(ev.x, ev.y)) {
            setSubMode(DesignSubMode::Split);
            layout(bounds_, ctx);
            return true;
        }
        if (btnGui_.contains(ev.x, ev.y)) {
            setSubMode(DesignSubMode::GuiDesigner);
            layout(bounds_, ctx);
            return true;
        }

        // COMPILE & RUN
        if (btnCompile_.contains(ev.x, ev.y)) {
            compileCurrentScript(ctx);
            return true;
        }

        // Explorer Item selection
        if (isExplorerOpen_) {
            for (const auto& item : explorerItemBounds_) {
                if (item.second.contains(ev.x, ev.y)) {
                    selectTargetByIndex(item.first);
                    layout(bounds_, ctx);
                    return true;
                }
            }
        }

        // 2. Modular Rack interaction
        if (mode_ == DesignSubMode::ModularRack || (mode_ == DesignSubMode::Split && !splitShowsGui_)) {
            // Modular toolbar buttons
            if (btnModularResetPatch_.contains(ev.x, ev.y)) {
                resetPatch();
                if (ctx.onShowNotification) {
                    ctx.onShowNotification("Reset modular patch cables to default routing");
                }
                return true;
            }
            if (btnModularAddModule_.contains(ev.x, ev.y)) {
                ModularModuleDef newMod{
                    "LFO MODULATOR " + std::to_string(modules_.size() + 1), "LFO",
                    0.0f, 0.0f, 200.0f, 320.0f, 0.18f, 0.22f, 0.20f,
                    {{"RATE", 0.5f, 0.1f, 20.0f, "Hz"}, {"DEPTH", 0.7f, 0.0f, 1.0f, ""}},
                    {"SYNC"}, {"TRI", "SQR"}
                };
                addModule(newMod);
                layout(bounds_, ctx);
                if (ctx.onShowNotification) {
                    ctx.onShowNotification("Added module to modular rack: " + newMod.title);
                }
                return true;
            }

            // Check Module Knobs
            for (size_t m = 0; m < modules_.size(); ++m) {
                auto& mod = modules_[m];
                for (size_t k = 0; k < mod.knobs.size(); ++k) {
                    float kx = mod.x + 40.0f + ((k % 2) * 85.0f);
                    float ky = mod.y + 65.0f + ((k / 2) * 70.0f);
                    float dx = ev.x - kx;
                    float dy = ev.y - ky;
                    if ((dx * dx + dy * dy) <= (18.0f * 18.0f)) {
                        draggingModuleKnobMod_ = static_cast<int>(m);
                        draggingModuleKnobIdx_ = static_cast<int>(k);
                        knobDragStartY_ = ev.y;
                        knobDragStartVal_ = mod.knobs[k].value;
                        return true;
                    }
                }
            }

            // Check Module Jacks (Inputs on Left, Outputs on Right)
            for (size_t m = 0; m < modules_.size(); ++m) {
                const auto& mod = modules_[m];
                float jackY = mod.y + mod.h - 45.0f;

                // Inputs
                for (size_t i = 0; i < mod.inputs.size(); ++i) {
                    float jx = mod.x + 30.0f + (i * 45.0f);
                    float dx = ev.x - jx;
                    float dy = ev.y - jackY;
                    if ((dx * dx + dy * dy) <= (14.0f * 14.0f)) {
                        if (ev.button == PointerButton::Right) {
                            // Right click on input: cycle color or disconnect
                            for (auto it = patchCords_.begin(); it != patchCords_.end(); ++it) {
                                if (it->destModule == static_cast<int>(m) && it->destJack == static_cast<int>(i)) {
                                    patchCords_.erase(it);
                                    if (ctx.onShowNotification) {
                                        ctx.onShowNotification("Disconnected patch cable from " + mod.inputs[i]);
                                    }
                                    return true;
                                }
                            }
                        } else {
                            // Left click on input: start dragging patch cable
                            isDraggingCord_ = true;
                            dragCordSrcMod_ = static_cast<int>(m);
                            dragCordSrcJack_ = static_cast<int>(i);
                            dragCordSrcIsOutput_ = false;
                            cordStartX_ = jx;
                            cordStartY_ = jackY;
                            cordCurrentX_ = ev.x;
                            cordCurrentY_ = ev.y;
                            dragCordR_ = 0.13f; dragCordG_ = 0.96f; dragCordB_ = 0.91f;
                            return true;
                        }
                    }
                }

                // Outputs
                for (size_t o = 0; o < mod.outputs.size(); ++o) {
                    float jx = mod.x + mod.w - 30.0f - (o * 45.0f);
                    float dx = ev.x - jx;
                    float dy = ev.y - jackY;
                    if ((dx * dx + dy * dy) <= (14.0f * 14.0f)) {
                        if (ev.button == PointerButton::Right) {
                            // Right click on output: cycle color or disconnect
                            for (auto it = patchCords_.begin(); it != patchCords_.end(); ++it) {
                                if (it->sourceModule == static_cast<int>(m) && it->sourceJack == static_cast<int>(o)) {
                                    patchCords_.erase(it);
                                    if (ctx.onShowNotification) {
                                        ctx.onShowNotification("Disconnected patch cable from " + mod.outputs[o]);
                                    }
                                    return true;
                                }
                            }
                        } else {
                            // Left click on output: start dragging cable
                            isDraggingCord_ = true;
                            dragCordSrcMod_ = static_cast<int>(m);
                            dragCordSrcJack_ = static_cast<int>(o);
                            dragCordSrcIsOutput_ = true;
                            cordStartX_ = jx;
                            cordStartY_ = jackY;
                            cordCurrentX_ = ev.x;
                            cordCurrentY_ = ev.y;
                            dragCordR_ = 1.0f; dragCordG_ = 0.55f; dragCordB_ = 0.0f;
                            return true;
                        }
                    }
                }
            }
        }

        // 3. GUI Designer & Live Faceplate interaction
        if (mode_ == DesignSubMode::GuiDesigner) {
            if (btnGuiLive_.contains(ev.x, ev.y)) {
                isGuiDesignMode_ = false;
                return true;
            }
            if (btnGuiDesign_.contains(ev.x, ev.y)) {
                isGuiDesignMode_ = true;
                return true;
            }
            if (btnGuiPr_.contains(ev.x, ev.y)) {
                if (ctx.onShowNotification) {
                    ctx.onShowNotification("Preparing GitHub PR submission for GUI panel design...");
                }
                return true;
            }

            if (isGuiDesignMode_) {
                // Toolbar buttons
                if (btnTogglePalette_.contains(ev.x, ev.y)) {
                    isPaletteOpen_ = !isPaletteOpen_;
                    return true;
                }
                if (btnToggleInspector_.contains(ev.x, ev.y)) {
                    isInspectorOpen_ = !isInspectorOpen_;
                    return true;
                }
                if (btnAddRow_.contains(ev.x, ev.y)) {
                    addGuiRow();
                    if (ctx.onShowNotification) ctx.onShowNotification("Added row to faceplate");
                    return true;
                }
                if (selectedDesignerRow_ >= 0 && btnDeleteRow_.contains(ev.x, ev.y)) {
                    deleteGuiRow(selectedDesignerRow_);
                    if (ctx.onShowNotification) ctx.onShowNotification("Deleted row from faceplate");
                    return true;
                }
                if (selectedDesignerWidget_ >= 0 && btnDuplicateWidget_.contains(ev.x, ev.y)) {
                    duplicateSelectedGuiWidget();
                    if (ctx.onShowNotification) ctx.onShowNotification("Duplicated widget");
                    return true;
                }
                if (selectedDesignerWidget_ >= 0 && btnDeleteWidget_.contains(ev.x, ev.y)) {
                    deleteSelectedGuiWidget();
                    if (ctx.onShowNotification) ctx.onShowNotification("Deleted widget");
                    return true;
                }

                // Left Palette clicks
                if (isPaletteOpen_) {
                    for (const auto& item : paletteItems_) {
                        if (item.bounds.contains(ev.x, ev.y)) {
                            addGuiWidget(item.type, item.knobStyle, item.title, "");
                            if (ctx.onShowNotification) {
                                ctx.onShowNotification("Added " + item.title + " to faceplate");
                            }
                            return true;
                        }
                    }
                }

                // Right Inspector clicks
                if (isInspectorOpen_) {
                    if (isChassisSelected_) {
                        for (const auto& tBtn : inspectorThemeBtns_) {
                            if (tBtn.second.contains(ev.x, ev.y)) {
                                guiPanel_.chassisStyle = tBtn.first;
                                syncGuiPanelToScript();
                                return true;
                            }
                        }
                        for (const auto& aBtn : inspectorAccentBtns_) {
                            if (aBtn.second.contains(ev.x, ev.y)) {
                                guiPanel_.accentColor = aBtn.first;
                                syncGuiPanelToScript();
                                return true;
                            }
                        }
                        if (inspectorWoodCheeksBtn_.contains(ev.x, ev.y)) {
                            guiPanel_.woodCheeks = !guiPanel_.woodCheeks;
                            syncGuiPanelToScript();
                            return true;
                        }
                    } else if (selectedDesignerRow_ >= 0 && selectedDesignerWidget_ >= 0) {
                        for (const auto& pBtn : inspectorParamBtns_) {
                            if (pBtn.second.contains(ev.x, ev.y)) {
                                auto& w = guiPanel_.rows[selectedDesignerRow_].widgets[selectedDesignerWidget_];
                                w.param = pBtn.first;
                                w.label = pBtn.first;
                                syncGuiPanelToScript();
                                return true;
                            }
                        }
                        for (const auto& kBtn : inspectorKnobStyleBtns_) {
                            if (kBtn.second.contains(ev.x, ev.y)) {
                                auto& w = guiPanel_.rows[selectedDesignerRow_].widgets[selectedDesignerWidget_];
                                w.knobStyle = kBtn.first;
                                syncGuiPanelToScript();
                                return true;
                            }
                        }
                        for (const auto& sBtn : inspectorSizeBtns_) {
                            if (sBtn.second.contains(ev.x, ev.y)) {
                                auto& w = guiPanel_.rows[selectedDesignerRow_].widgets[selectedDesignerWidget_];
                                w.size = sBtn.first;
                                syncGuiPanelToScript();
                                return true;
                            }
                        }
                        if (inspectorDuplicateBtn_.contains(ev.x, ev.y)) {
                            duplicateSelectedGuiWidget();
                            return true;
                        }
                        if (inspectorDeleteBtn_.contains(ev.x, ev.y)) {
                            deleteSelectedGuiWidget();
                            return true;
                        }
                    }
                }

                // Center Faceplate clicks (Chassis header, rows, widgets)
                if (guiPanel_.bounds.contains(ev.x, ev.y)) {
                    // Check header (chassis selection)
                    Rect2D headerR(guiPanel_.bounds.x, guiPanel_.bounds.y, guiPanel_.bounds.w, 48.0f);
                    if (headerR.contains(ev.x, ev.y)) {
                        selectDesignerChassis();
                        return true;
                    }

                    // Check Widgets
                    for (size_t rIdx = 0; rIdx < guiPanel_.rows.size(); ++rIdx) {
                        const auto& row = guiPanel_.rows[rIdx];
                        for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                            if (row.widgets[wIdx].bounds.contains(ev.x, ev.y)) {
                                selectDesignerWidget(static_cast<int>(rIdx), static_cast<int>(wIdx));
                                return true;
                            }
                        }
                        if (row.bounds.contains(ev.x, ev.y)) {
                            selectedDesignerRow_ = static_cast<int>(rIdx);
                            selectedDesignerWidget_ = -1;
                            isChassisSelected_ = false;
                            return true;
                        }
                    }
                }
            } else {
                // Live Interaction Mode: interactive knobs, sliders, toggles
                for (size_t rIdx = 0; rIdx < guiPanel_.rows.size(); ++rIdx) {
                    auto& row = guiPanel_.rows[rIdx];
                    for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                        auto& w = row.widgets[wIdx];
                        if (w.bounds.contains(ev.x, ev.y)) {
                            if (w.type == GuiWidgetType::ToggleSwitch) {
                                w.currentVal = (w.currentVal > 0.5f) ? 0.0f : 1.0f;
                                if (onParamChanged) {
                                    onParamChanged(getActiveTarget().id, w.param, w.currentVal);
                                }
                                return true;
                            } else {
                                draggingGuiWidgetRow_ = static_cast<int>(rIdx);
                                draggingGuiWidgetIdx_ = static_cast<int>(wIdx);
                                guiDragStartY_ = ev.y;
                                guiDragStartVal_ = w.currentVal;
                                return true;
                            }
                        }
                    }
                }
            }
        }

        // 4. Code Editor live parameters & interactive text editor
        if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
            if (btnSelectAll_.contains(ev.x, ev.y)) {
                textEditor_.getPresenter().selectAll();
                if (ctx.focusManager) ctx.focusManager->requestFocus(&textEditor_);
                return true;
            }
            if (btnCopy_.contains(ev.x, ev.y)) {
                if (ctx.clipboard) ctx.clipboard->setText(textEditor_.getText());
                if (onCopyToClipboard) onCopyToClipboard(textEditor_.getText());
                if (ctx.onShowNotification) ctx.onShowNotification("Copied script to clipboard");
                return true;
            }
            if (btnSubmitPr_.contains(ev.x, ev.y)) {
                if (ctx.onShowNotification) ctx.onShowNotification("Opening GitHub PR submission dialog...");
                return true;
            }
            if (btnApiDocs_.contains(ev.x, ev.y)) {
                if (ctx.onShowNotification) ctx.onShowNotification("Opening Eatscript API documentation...");
                return true;
            }

            if (textEditor_.handlePointer(ev, ctx)) {
                return true;
            }

            for (size_t i = 0; i < currentParams_.size(); ++i) {
                if (currentParams_[i].bounds.contains(ev.x, ev.y)) {
                    draggingParamIndex_ = static_cast<int>(i);
                    float trackX = currentParams_[i].bounds.x + 8.0f;
                    float trackW = currentParams_[i].bounds.w - 16.0f;
                    float norm = std::clamp((ev.x - trackX) / trackW, 0.0f, 1.0f);
                    currentParams_[i].currentVal = currentParams_[i].minVal + norm * (currentParams_[i].maxVal - currentParams_[i].minVal);
                    if (onParamChanged) {
                        onParamChanged(getActiveTarget().id, currentParams_[i].name, currentParams_[i].currentVal);
                    }
                    return true;
                }
            }
        }
    } else if (ev.action == PointerAction::Move) {
        if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
            textEditor_.handlePointer(ev, ctx);
        }

        // Modular rack knob dragging
        if (draggingModuleKnobMod_ >= 0 && draggingModuleKnobMod_ < static_cast<int>(modules_.size())) {
            auto& mod = modules_[draggingModuleKnobMod_];
            if (draggingModuleKnobIdx_ >= 0 && draggingModuleKnobIdx_ < static_cast<int>(mod.knobs.size())) {
                float delta = (knobDragStartY_ - ev.y) / 120.0f;
                float newVal = std::clamp(knobDragStartVal_ + delta, 0.0f, 1.0f);
                mod.knobs[draggingModuleKnobIdx_].value = newVal;

                if (onParamChanged) {
                    onParamChanged(getActiveTarget().id, mod.knobs[draggingModuleKnobIdx_].label, newVal);
                }
                return true;
            }
        }

        // Modular rack patch cord dragging
        if (isDraggingCord_) {
            cordCurrentX_ = ev.x;
            cordCurrentY_ = ev.y;
            return true;
        }

        // GUI Live Widget dragging
        if (draggingGuiWidgetRow_ >= 0 && draggingGuiWidgetRow_ < static_cast<int>(guiPanel_.rows.size())) {
            auto& row = guiPanel_.rows[draggingGuiWidgetRow_];
            if (draggingGuiWidgetIdx_ >= 0 && draggingGuiWidgetIdx_ < static_cast<int>(row.widgets.size())) {
                auto& w = row.widgets[draggingGuiWidgetIdx_];
                float delta = (guiDragStartY_ - ev.y) / 120.0f;
                float range = w.maxVal - w.minVal;
                w.currentVal = std::clamp(guiDragStartVal_ + delta * range, w.minVal, w.maxVal);

                if (onParamChanged) {
                    onParamChanged(getActiveTarget().id, w.param, w.currentVal);
                }
                return true;
            }
        }

        // Live parameter slider dragging in Code tab
        if (draggingParamIndex_ >= 0 && draggingParamIndex_ < static_cast<int>(currentParams_.size())) {
            auto& p = currentParams_[draggingParamIndex_];
            float trackX = p.bounds.x + 8.0f;
            float trackW = p.bounds.w - 16.0f;
            float norm = std::clamp((ev.x - trackX) / trackW, 0.0f, 1.0f);
            p.currentVal = p.minVal + norm * (p.maxVal - p.minVal);
            if (onParamChanged) {
                onParamChanged(getActiveTarget().id, p.name, p.currentVal);
            }
            return true;
        }
    } else if (ev.action == PointerAction::Up) {
        if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
            textEditor_.handlePointer(ev, ctx);
        }

        if (draggingModuleKnobMod_ >= 0) {
            draggingModuleKnobMod_ = -1;
            draggingModuleKnobIdx_ = -1;
            return true;
        }

        if (isDraggingCord_) {
            // Find closest jack within 26px
            int targetMod = -1;
            int targetJack = -1;
            bool targetIsOutput = false;
            float bestDist = 26.0f * 26.0f;

            for (size_t m = 0; m < modules_.size(); ++m) {
                if (static_cast<int>(m) == dragCordSrcMod_) continue; // Can't connect to same module

                const auto& mod = modules_[m];
                float jackY = mod.y + mod.h - 45.0f;

                // Test input jacks if dragging from output (or vice versa)
                if (dragCordSrcIsOutput_) {
                    for (size_t i = 0; i < mod.inputs.size(); ++i) {
                        float jx = mod.x + 30.0f + (i * 45.0f);
                        float dx = cordCurrentX_ - jx;
                        float dy = cordCurrentY_ - jackY;
                        float distSq = dx * dx + dy * dy;
                        if (distSq < bestDist) {
                            bestDist = distSq;
                            targetMod = static_cast<int>(m);
                            targetJack = static_cast<int>(i);
                            targetIsOutput = false;
                        }
                    }
                } else {
                    for (size_t o = 0; o < mod.outputs.size(); ++o) {
                        float jx = mod.x + mod.w - 30.0f - (o * 45.0f);
                        float dx = cordCurrentX_ - jx;
                        float dy = cordCurrentY_ - jackY;
                        float distSq = dx * dx + dy * dy;
                        if (distSq < bestDist) {
                            bestDist = distSq;
                            targetMod = static_cast<int>(m);
                            targetJack = static_cast<int>(o);
                            targetIsOutput = true;
                        }
                    }
                }
            }

            if (targetMod >= 0 && targetJack >= 0) {
                int srcMod = dragCordSrcIsOutput_ ? dragCordSrcMod_ : targetMod;
                int srcJk = dragCordSrcIsOutput_ ? dragCordSrcJack_ : targetJack;
                int dstMod = dragCordSrcIsOutput_ ? targetMod : dragCordSrcMod_;
                int dstJk = dragCordSrcIsOutput_ ? targetJack : dragCordSrcJack_;

                // Prevent duplicate
                bool duplicate = false;
                for (const auto& c : patchCords_) {
                    if (c.sourceModule == srcMod && c.sourceJack == srcJk &&
                        c.destModule == dstMod && c.destJack == dstJk) {
                        duplicate = true;
                        break;
                    }
                }

                if (!duplicate) {
                    patchCords_.push_back({srcMod, srcJk, dstMod, dstJk, dragCordR_, dragCordG_, dragCordB_});
                    if (ctx.onShowNotification) {
                        ctx.onShowNotification("Patched " + modules_[srcMod].title + " -> " + modules_[dstMod].title);
                    }
                }
            }

            isDraggingCord_ = false;
            return true;
        }

        if (draggingGuiWidgetRow_ >= 0) {
            draggingGuiWidgetRow_ = -1;
            draggingGuiWidgetIdx_ = -1;
            return true;
        }

        if (draggingParamIndex_ >= 0) {
            draggingParamIndex_ = -1;
            return true;
        }
    } else if (ev.action == PointerAction::Scroll) {
        if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
            if (textEditor_.handlePointer(ev, ctx)) return true;
        }
    }

    return false;
}

bool DesignView::handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
    if (action == 1) { // GLFW_PRESS
        // Ctrl+Enter: Compile current script
        if (key == 257 && (mods & 2)) {
            compileCurrentScript(ctx);
            return true;
        }
    }
    if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
        if (textEditor_.handleKey(key, scancode, action, mods, ctx)) {
            return true;
        }
    }
    return false;
}

bool DesignView::handleChar(char32_t codepoint, const ViewContext& ctx) {
    if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
        return textEditor_.handleChar(codepoint, ctx);
    }
    return false;
}

} // namespace eatsbits::ui
