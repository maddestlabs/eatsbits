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
    DrawerConfig drawerCfg;
    drawerCfg.edge = DockEdge::Left;
    drawerCfg.tab.title = "PROJECT";
    drawerCfg.tab.tabWidth = 22.0f;
    drawerCfg.tab.tabHeight = 84.0f;
    drawerCfg.tab.preferredExtent = 240.0f;
    drawerCfg.tab.minExtent = 180.0f;
    drawerCfg.tab.maxExtent = 480.0f;
    drawerCfg.tab.resizable = true;
    drawerCfg.tab.showCloseButton = true;
    drawerCfg.tab.showHeaderBar = false;
    drawerCfg.tab.tabPlacement = DockTabPlacement::Center;
    drawerCfg.animationDamping = 18.0f;
    projectDrawer_.setConfig(drawerCfg);
    projectDrawer_.setExpanded(true);
    projectDrawer_.setAnimProgress(1.0f);

    initDefaultTargetsAndCode();

    // Default patch cords (preserving unit test invariants)
    patchCords_.push_back({0, 0, 1, 0, 1.0f, 0.55f, 0.0f});
    patchCords_.push_back({1, 1, 2, 0, 0.13f, 0.96f, 0.91f});

    // Default Eurorack Modular rack modules (Rivet-aligned: 1 HP = 15px, row height = 380px)
    modules_ = {
        {"OSC 1: TB-303 CORE", "OSC", 30.0f, 80.0f, 240.0f, 320.0f, 0.20f, 0.21f, 0.24f,
         {{"TUNE", 0.50f, -24.0f, 24.0f, "st"}, {"WAVE", 0.0f, 0.0f, 1.0f, ""}, {"GLIDE", 0.35f, 0.0f, 1.0f, "s"}},
         {"CV IN", "GATE"}, {"OUT", "SUB"}},

        {"FILTER: 24dB DIODE", "VCF", 270.0f, 80.0f, 240.0f, 320.0f, 0.22f, 0.23f, 0.26f,
         {{"CUTOFF", 0.65f, 20.0f, 20000.0f, "Hz"}, {"RESON", 0.78f, 0.0f, 1.0f, ""}, {"ENV MOD", 0.60f, 0.0f, 1.0f, ""}, {"ACCENT", 0.70f, 0.0f, 1.0f, ""}},
         {"AUDIO IN", "FM IN"}, {"LPF OUT", "ENV OUT"}},

        {"VCA & SATURATION", "VCA", 510.0f, 80.0f, 210.0f, 320.0f, 0.19f, 0.20f, 0.23f,
         {{"DRIVE", 0.45f, 1.0f, 10.0f, "x"}, {"ATTACK", 0.05f, 0.001f, 1.0f, "s"}, {"DECAY", 0.45f, 0.01f, 2.0f, "s"}},
         {"IN", "CV"}, {"OUT", "MON"}},

        {"OUT: MASTER BUS", "OUT", 720.0f, 80.0f, 210.0f, 320.0f, 0.18f, 0.19f, 0.22f,
         {{"VOLUME", 0.85f, 0.0f, 1.0f, ""}, {"PAN", 0.0f, -1.0f, 1.0f, ""}, {"LIMIT", 0.90f, 0.0f, 1.0f, ""}},
         {"L IN", "R IN"}, {"MAIN L", "MAIN R"}}
    };

    initDefaultGuiPanel();

    textEditor_.onCompileTriggered = [this]() {
        ViewContext dummyCtx;
        compileCurrentScript(dummyCtx);
    };
    textEditor_.onTextChanged = [this](const std::string& code) {
        currentScriptCode_ = code;
    };
    textEditor_.setText(currentScriptCode_);

    // Configure 2D Modular Zoom & Pan
    modularZoomPan_.setUniformZoomLimits(0.40f, 2.0f);

    // Configure Modular Search Dialog
    moduleSearchDialog_.onPluginSelected = [this](PluginDialogMode, const PluginEntry& entry, uint32_t) {
        ModularModuleDef newMod;
        newMod.title = entry.name;
        newMod.type = entry.engineTag;
        newMod.w = 210.0f;
        newMod.h = 340.0f;
        newMod.r = std::clamp(entry.r * 0.35f, 0.12f, 0.45f);
        newMod.g = std::clamp(entry.g * 0.35f, 0.12f, 0.45f);
        newMod.b = std::clamp(entry.b * 0.35f, 0.12f, 0.45f);

        std::string tag = entry.engineTag;
        for (char& c : tag) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (tag.find("VCO") != std::string::npos || tag.find("OSC") != std::string::npos || tag.find("POLYBLEP") != std::string::npos) {
            newMod.knobs = {{"TUNE", 0.50f, -24.0f, 24.0f, "st"}, {"FINE", 0.50f, -50.0f, 50.0f, "ct"}, {"WAVE", 0.25f, 0.0f, 1.0f, ""}, {"PW", 0.50f, 0.0f, 1.0f, "%"}};
            newMod.inputs = {"1V/OCT", "PW CV", "FM"};
            newMod.outputs = {"OUT", "SUB"};
        } else if (tag.find("VCF") != std::string::npos || tag.find("FILTER") != std::string::npos || tag.find("LADDER") != std::string::npos || tag.find("SVF") != std::string::npos) {
            newMod.knobs = {{"CUTOFF", 0.65f, 20.0f, 20000.0f, "Hz"}, {"RESO", 0.50f, 0.0f, 1.0f, ""}, {"DRIVE", 0.25f, 0.0f, 1.0f, "dB"}, {"KEYTRK", 0.50f, 0.0f, 1.0f, ""}};
            newMod.inputs = {"AUDIO IN", "CUTOFF CV", "FM"};
            newMod.outputs = {"LP OUT", "HP OUT"};
        } else if (tag.find("REVERB") != std::string::npos) {
            newMod.knobs = {{"DECAY", 0.60f, 0.1f, 10.0f, "s"}, {"DAMP", 0.40f, 0.0f, 1.0f, ""}, {"MIX", 0.45f, 0.0f, 1.0f, "%"}, {"SIZE", 0.70f, 0.0f, 1.0f, ""}};
            newMod.inputs = {"IN L", "IN R"};
            newMod.outputs = {"OUT L", "OUT R"};
        } else if (tag.find("DELAY") != std::string::npos) {
            newMod.knobs = {{"TIME", 0.50f, 10.0f, 2000.0f, "ms"}, {"FEEDBK", 0.45f, 0.0f, 1.0f, "%"}, {"DAMP", 0.30f, 0.0f, 1.0f, ""}, {"MIX", 0.40f, 0.0f, 1.0f, "%"}};
            newMod.inputs = {"IN L", "IN R", "TIME CV"};
            newMod.outputs = {"OUT L", "OUT R"};
        } else if (tag.find("CHORUS") != std::string::npos) {
            newMod.knobs = {{"RATE", 0.35f, 0.05f, 10.0f, "Hz"}, {"DEPTH", 0.60f, 0.0f, 1.0f, "%"}, {"FEEDBK", 0.30f, 0.0f, 1.0f, "%"}, {"MIX", 0.50f, 0.0f, 1.0f, "%"}};
            newMod.inputs = {"IN L", "IN R"};
            newMod.outputs = {"OUT L", "OUT R"};
        } else if (tag.find("COMP") != std::string::npos) {
            newMod.knobs = {{"THRESH", 0.60f, -60.0f, 0.0f, "dB"}, {"RATIO", 0.40f, 1.0f, 20.0f, ":1"}, {"ATTACK", 0.20f, 0.1f, 100.0f, "ms"}, {"RELEASE", 0.50f, 10.0f, 1000.0f, "ms"}};
            newMod.inputs = {"IN L", "IN R", "SIDECHAIN"};
            newMod.outputs = {"OUT L", "OUT R"};
        } else if (tag.find("ADSR") != std::string::npos || tag.find("ENV") != std::string::npos) {
            newMod.knobs = {{"ATTACK", 0.10f, 1.0f, 2000.0f, "ms"}, {"DECAY", 0.35f, 1.0f, 3000.0f, "ms"}, {"SUSTAIN", 0.70f, 0.0f, 1.0f, "%"}, {"RELEASE", 0.40f, 5.0f, 5000.0f, "ms"}};
            newMod.inputs = {"GATE", "RETRIG"};
            newMod.outputs = {"ENV OUT", "INV OUT"};
        } else if (tag.find("LFO") != std::string::npos) {
            newMod.knobs = {{"RATE", 0.50f, 0.05f, 20.0f, "Hz"}, {"DEPTH", 0.75f, 0.0f, 1.0f, "%"}, {"SHAPE", 0.0f, 0.0f, 1.0f, ""}, {"PHASE", 0.0f, 0.0f, 360.0f, "deg"}};
            newMod.inputs = {"SYNC", "RESET"};
            newMod.outputs = {"TRI", "SQR", "SAW"};
        } else if (tag.find("MIXER") != std::string::npos) {
            newMod.knobs = {{"CH 1", 0.75f, 0.0f, 1.0f, ""}, {"CH 2", 0.75f, 0.0f, 1.0f, ""}, {"CH 3", 0.50f, 0.0f, 1.0f, ""}, {"CH 4", 0.50f, 0.0f, 1.0f, ""}};
            newMod.inputs = {"IN 1", "IN 2", "IN 3", "IN 4"};
            newMod.outputs = {"MIX L", "MIX R"};
        } else if (tag.find("OUT") != std::string::npos || tag.find("MASTER") != std::string::npos) {
            newMod.knobs = {{"VOLUME", 0.85f, 0.0f, 1.0f, ""}, {"PAN", 0.0f, -1.0f, 1.0f, ""}, {"LIMIT", 0.90f, 0.0f, 1.0f, ""}, {"PHONES", 0.70f, 0.0f, 1.0f, ""}};
            newMod.inputs = {"MAIN L", "MAIN R"};
            newMod.outputs = {"MON L", "MON R"};
        } else {
            newMod.knobs = {{"PARAM 1", 0.50f, 0.0f, 1.0f, ""}, {"PARAM 2", 0.50f, 0.0f, 1.0f, ""}, {"PARAM 3", 0.50f, 0.0f, 1.0f, ""}, {"PARAM 4", 0.50f, 0.0f, 1.0f, ""}};
            newMod.inputs = {"IN 1", "IN 2", "CV IN"};
            newMod.outputs = {"OUT 1", "OUT 2"};
        }

        newMod.x = pendingModuleSpawnX_;
        newMod.y = pendingModuleSpawnY_;
        addModule(newMod);
    };
}

void DesignView::setSubMode(DesignSubMode mode) noexcept {
    mode_ = mode;
}

void DesignView::setExplorerOpen(bool open) noexcept {
    isExplorerOpen_ = open;
    projectDrawer_.setExpanded(open);
}

void DesignView::resetPatch() {
    const auto& t = getActiveTarget();
    std::string defaultCode = getDefaultScriptForTarget(t);
    targetCodeMap_.erase(t.id);
    targetParamsMap_.erase(t.id);
    setScriptCode(defaultCode);
    initModularRackFromScript(defaultCode);
    initDefaultGuiPanel();
}

std::string DesignView::getDefaultScriptForTarget(const ScriptTarget& target) const {
    std::string id = target.id;
    std::string key = id + " " + target.title + " " + target.subtitle;
    for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (key.find("303") != std::string::npos || id == "eats_303") {
        return
            "# --- TB-303 Acid Bassline Synthesizer (Eatscript) ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"Cutoff\", min=200.0, max=4000.0, default=850.0)\n"
            "    eat.param(\"Resonance\", min=0.1, max=0.98, default=0.78)\n"
            "    eat.param(\"EnvMod\", min=0.0, max=1.0, default=0.65)\n"
            "    eat.param(\"Decay\", min=0.05, max=1.5, default=0.45)\n"
            "    eat.param(\"Accent\", min=0.0, max=1.0, default=0.70)\n"
            "    eat.param(\"Drive\", min=0.0, max=1.0, default=0.25)\n\n"
            "def gui():\n"
            "    return {\n"
            "        \"title\": \"Eats-303 (Synth DSP)\",\n"
            "        \"subtitle\": \"Acid Diode Ladder Synth\",\n"
            "        \"chassis\": 1,\n"
            "        \"woodCheeks\": False,\n"
            "        \"rows\": [\n"
            "            [{\"id\": \"w_waveform\", \"type\": 1, \"param\": \"Waveform\", \"label\": \"WAVEFORM\", \"style\": 3},\n"
            "             {\"id\": \"w_pitch\", \"type\": 1, \"param\": \"Pitch\", \"label\": \"PITCH\", \"style\": 1},\n"
            "             {\"id\": \"w_cutoff\", \"type\": 1, \"param\": \"Cutoff\", \"label\": \"CUTOFF\", \"style\": 1},\n"
            "             {\"id\": \"w_res\", \"type\": 1, \"param\": \"Resonance\", \"label\": \"RESONANCE\", \"style\": 1},\n"
            "             {\"id\": \"w_envmod\", \"type\": 1, \"param\": \"EnvMod\", \"label\": \"ENV MOD\", \"style\": 1},\n"
            "             {\"id\": \"w_decay\", \"type\": 1, \"param\": \"Decay\", \"label\": \"DECAY\", \"style\": 1},\n"
            "             {\"id\": \"w_accent\", \"type\": 1, \"param\": \"Accent\", \"label\": \"ACCENT\", \"style\": 1}],\n"
            "            [{\"id\": \"w_octave\", \"type\": 1, \"param\": \"Octave\", \"label\": \"OCTAVE\", \"style\": 2},\n"
            "             {\"id\": \"w_subosc\", \"type\": 3, \"param\": \"SubWaveform\", \"label\": \"SUB OSC\", \"style\": 0},\n"
            "             {\"id\": \"w_subvol\", \"type\": 1, \"param\": \"SubVolume\", \"label\": \"SUB VOL\", \"style\": 4},\n"
            "             {\"id\": \"w_glide\", \"type\": 1, \"param\": \"GlideCurve\", \"label\": \"GLIDE CURVE\", \"style\": 4},\n"
            "             {\"id\": \"w_drive\", \"type\": 1, \"param\": \"Drive\", \"label\": \"DRIVE\", \"style\": 4}]\n"
            "        ]\n"
            "    }\n\n"
            "def graph():\n"
            "    osc = eat.node.osc(type=\"TB-303 CORE\", tune=0.50, wave=0.0, glide=0.35)\n"
            "    filter = eat.node.vcf(type=\"24dB DIODE\", cutoff=0.65, resonance=0.78, env_mod=0.60, accent=0.70)\n"
            "    vca = eat.node.vca(type=\"VCA & SATURATION\", drive=0.45, attack=0.05, decay=0.45)\n"
            "    out = eat.node.out(type=\"MASTER BUS\", volume=0.85, pan=0.0, limit=0.90)\n"
            "    osc >> filter\n"
            "    filter >> vca\n"
            "    vca >> out\n"
            "    return out\n\n"
            "def process(time, freq, note, params):\n"
            "    saw = 2.0 * (time * freq - math.floor(time * freq + 0.5))\n"
            "    cutoff = params.get(\"Cutoff\", 850.0)\n"
            "    res = params.get(\"Resonance\", 0.78)\n"
            "    return tb303_diode_ladder(saw, cutoff, res)\n";
    }

    if (id == "eats_kick") {
        return
            "# --- Procedural Sub Kick Drum (Eatscript) ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"StartFreq\", min=100.0, max=300.0, default=160.0)\n"
            "    eat.param(\"EndFreq\", min=30.0, max=60.0, default=42.0)\n"
            "    eat.param(\"PitchDecay\", min=0.01, max=0.2, default=0.035)\n"
            "    eat.param(\"AmpDecay\", min=0.05, max=4.0, default=0.35)\n"
            "    eat.param(\"Click\", min=0.0, max=1.0, default=0.0)\n\n"
            "def gui():\n"
            "    return {\n"
            "        \"title\": \"Eats Kick (Synth DSP)\",\n"
            "        \"subtitle\": \"Procedural Sub Kick Drum\",\n"
            "        \"chassis\": 2,\n"
            "        \"woodCheeks\": True,\n"
            "        \"rows\": [\n"
            "            [{\"id\": \"w_start_f\", \"type\": 1, \"param\": \"StartFreq\", \"label\": \"START FREQ\", \"style\": 5},\n"
            "             {\"id\": \"w_end_f\", \"type\": 1, \"param\": \"EndFreq\", \"label\": \"END FREQ\", \"style\": 4},\n"
            "             {\"id\": \"w_pdecay\", \"type\": 1, \"param\": \"PitchDecay\", \"label\": \"PITCH DECAY\", \"style\": 6}],\n"
            "            [{\"id\": \"w_ampdecay\", \"type\": 1, \"param\": \"AmpDecay\", \"label\": \"AMP DECAY\", \"style\": 7},\n"
            "             {\"id\": \"w_click\", \"type\": 3, \"param\": \"Click\", \"label\": \"CLICK TRANSIENT\", \"style\": 0},\n"
            "             {\"id\": \"w_vu\", \"type\": 5, \"param\": \"Volume\", \"label\": \"OUTPUT LEVEL\", \"style\": 0}]\n"
            "        ]\n"
            "    }\n\n"
            "def graph():\n"
            "    osc = eat.node.osc(type=\"SUB OSC\", start_f=160.0, end_f=42.0, pitch_decay=0.035)\n"
            "    env = eat.node.env(type=\"AMP ENV\", decay=0.35, click=0.0)\n"
            "    out = eat.node.out(type=\"MASTER OUT\", volume=0.85)\n"
            "    osc >> env\n"
            "    env >> out\n"
            "    return out\n\n"
            "def process(time, freq, note, params):\n"
            "    startF = params.get(\"StartFreq\", 160.0)\n"
            "    endF = params.get(\"EndFreq\", 42.0)\n"
            "    pDecay = params.get(\"PitchDecay\", 0.035)\n"
            "    aDecay = params.get(\"AmpDecay\", 0.35)\n"
            "    click = params.get(\"Click\", 0.0)\n"
            "    curFreq = endF + (startF - endF) * math.exp(-time / max(0.005, pDecay))\n"
            "    phase = 2.0 * math.pi * curFreq * time\n"
            "    body = math.sin(phase) * math.exp(-time / max(0.01, aDecay))\n"
            "    clickNoise = (math.sin(time * 8421.0) * math.exp(-time / 0.005)) * click\n"
            "    return body + clickNoise\n";
    }

    if (id == "eats_snare") {
        return
            "# --- Procedural Analog Snare Drum (Eatscript) ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"Tone\", min=100.0, max=350.0, default=180.0)\n"
            "    eat.param(\"Snappy\", min=0.0, max=1.0, default=0.65)\n"
            "    eat.param(\"Decay\", min=0.05, max=1.0, default=0.25)\n"
            "    eat.param(\"Tune\", min=0.5, max=2.0, default=1.0)\n\n"
            "def gui():\n"
            "    return {\n"
            "        \"title\": \"Eats Snare (Synth DSP)\",\n"
            "        \"subtitle\": \"Analog Filtered Noise Snare\",\n"
            "        \"chassis\": 0,\n"
            "        \"woodCheeks\": True,\n"
            "        \"rows\": [\n"
            "            [{\"id\": \"w_tone\", \"type\": 1, \"param\": \"Tone\", \"label\": \"TONE\", \"style\": 4},\n"
            "             {\"id\": \"w_snappy\", \"type\": 1, \"param\": \"Snappy\", \"label\": \"SNAPPY\", \"style\": 5},\n"
            "             {\"id\": \"w_decay\", \"type\": 1, \"param\": \"Decay\", \"label\": \"DECAY\", \"style\": 6},\n"
            "             {\"id\": \"w_tune\", \"type\": 1, \"param\": \"Tune\", \"label\": \"TUNE\", \"style\": 4}]\n"
            "        ]\n"
            "    }\n\n"
            "def graph():\n"
            "    osc = eat.node.osc(type=\"TONE GENERATOR\", tone=180.0, tune=1.0)\n"
            "    noise = eat.node.noise(type=\"FILTERED NOISE\", snappy=0.65, decay=0.25)\n"
            "    out = eat.node.out(type=\"MASTER OUT\", volume=0.85)\n"
            "    osc >> out\n"
            "    noise >> out\n"
            "    return out\n\n"
            "def process(time, freq, note, params):\n"
            "    toneF = params.get(\"Tone\", 180.0) * params.get(\"Tune\", 1.0)\n"
            "    decay = params.get(\"Decay\", 0.25)\n"
            "    snappy = params.get(\"Snappy\", 0.65)\n"
            "    tone = math.sin(2.0 * math.pi * toneF * time) * math.exp(-time / (decay * 0.7))\n"
            "    noise = (math.sin(time * 12345.67) % 1.0 - 0.5) * 2.0 * math.exp(-time / decay) * snappy\n"
            "    return tone * (1.0 - snappy * 0.5) + noise\n";
    }

    if (id == "eats_hats") {
        return
            "# --- Procedural Metallic Hi-Hat (Eatscript) ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"Decay\", min=0.01, max=0.8, default=0.08)\n"
            "    eat.param(\"Metallic\", min=0.1, max=1.0, default=0.75)\n"
            "    eat.param(\"Pitch\", min=4000.0, max=12000.0, default=8500.0)\n\n"
            "def gui():\n"
            "    return {\n"
            "        \"title\": \"Eats Hats (Synth DSP)\",\n"
            "        \"subtitle\": \"Metallic Phase Hi-Hat\",\n"
            "        \"chassis\": 0,\n"
            "        \"woodCheeks\": True,\n"
            "        \"rows\": [\n"
            "            [{\"id\": \"w_decay\", \"type\": 1, \"param\": \"Decay\", \"label\": \"DECAY\", \"style\": 4},\n"
            "             {\"id\": \"w_metal\", \"type\": 1, \"param\": \"Metallic\", \"label\": \"METALLIC\", \"style\": 6},\n"
            "             {\"id\": \"w_pitch\", \"type\": 1, \"param\": \"Pitch\", \"label\": \"PITCH\", \"style\": 5}]\n"
            "        ]\n"
            "    }\n\n"
            "def graph():\n"
            "    osc = eat.node.osc(type=\"METALLIC OSC\", pitch=8500.0, metallic=0.75)\n"
            "    env = eat.node.env(type=\"DECAY ENV\", decay=0.08)\n"
            "    out = eat.node.out(type=\"MASTER OUT\", volume=0.85)\n"
            "    osc >> env\n"
            "    env >> out\n"
            "    return out\n\n"
            "def process(time, freq, note, params):\n"
            "    decay = params.get(\"Decay\", 0.08)\n"
            "    pitch = params.get(\"Pitch\", 8500.0)\n"
            "    osc1 = math.sin(2.0 * math.pi * pitch * time)\n"
            "    osc2 = math.sin(2.0 * math.pi * (pitch * 1.414) * time)\n"
            "    metallic = (1.0 if osc1 * osc2 > 0.0 else -1.0) * math.exp(-time / decay)\n"
            "    return metallic * 0.4\n";
    }

    if (target.type == ScriptTargetType::AudioFx) {
        if (key.find("crush") != std::string::npos || key.find("8-bit") != std::string::npos || key.find("bitcrusher") != std::string::npos) {
            return
                "# --- 8-Bit Digital Bitcrusher & Sample Reducer (Eatscript FX) ---\n"
                "import math\n\n"
                "def init():\n"
                "    eat.param(\"Bits\", min=1.0, max=16.0, default=8.0)\n"
                "    eat.param(\"Downsample\", min=1.0, max=32.0, default=4.0)\n"
                "    eat.param(\"Drive\", min=0.5, max=4.0, default=1.0)\n"
                "    eat.param(\"Mix\", min=0.0, max=1.0, default=0.85)\n\n"
                "def gui():\n"
                "    return {\n"
                "        \"title\": \"Bitcrusher FX\",\n"
                "        \"subtitle\": \"Lo-Fi Sample Reducer\",\n"
                "        \"chassis\": 3,\n"
                "        \"woodCheeks\": False,\n"
                "        \"rows\": [\n"
                "            [{\"id\": \"w_bits\", \"type\": 1, \"param\": \"Bits\", \"label\": \"BIT DEPTH\", \"style\": 6},\n"
                "             {\"id\": \"w_down\", \"type\": 1, \"param\": \"Downsample\", \"label\": \"DOWNSAMPLE\", \"style\": 4},\n"
                "             {\"id\": \"w_drive\", \"type\": 1, \"param\": \"Drive\", \"label\": \"PRE-DRIVE\", \"style\": 5},\n"
                "             {\"id\": \"w_mix\", \"type\": 1, \"param\": \"Mix\", \"label\": \"DRY / WET\", \"style\": 7}]\n"
                "        ]\n"
                "    }\n\n"
                "def graph():\n"
                "    in_node = eat.node.input(type=\"AUDIO IN\")\n"
                "    crush = eat.node.fx(type=\"CRUSHER\", bits=8.0, downsample=4.0, drive=1.0)\n"
                "    out = eat.node.out(type=\"AUDIO OUT\", volume=1.0)\n"
                "    in_node >> crush\n"
                "    crush >> out\n"
                "    return out\n\n"
                "def process(in_l, in_r, params):\n"
                "    bits = params.get(\"Bits\", 8.0)\n"
                "    crush = params.get(\"Downsample\", 4.0)\n"
                "    mix = params.get(\"Mix\", 0.85)\n"
                "    levels = math.pow(2.0, bits)\n"
                "    out_l = math.floor(in_l * levels) / levels\n"
                "    out_r = math.floor(in_r * levels) / levels\n"
                "    return [out_l * mix + in_l * (1.0 - mix), out_r * mix + in_r * (1.0 - mix)]\n";
        }

        return
            "# --- Warm Analog Tube Distortion (Eatscript FX) ---\n"
            "import math\n\n"
            "def init():\n"
            "    eat.param(\"Drive\", min=1.0, max=20.0, default=4.5)\n"
            "    eat.param(\"Warmth\", min=0.0, max=1.0, default=0.70)\n"
            "    eat.param(\"Mix\", min=0.0, max=1.0, default=0.85)\n\n"
            "def gui():\n"
            "    return {\n"
            "        \"title\": \"Tube Distortion FX\",\n"
            "        \"subtitle\": \"Harmonic Saturation\",\n"
            "        \"chassis\": 4,\n"
            "        \"woodCheeks\": True,\n"
            "        \"rows\": [\n"
            "            [{\"id\": \"w_drive\", \"type\": 1, \"param\": \"Drive\", \"label\": \"SATURATION DRIVE\", \"style\": 5},\n"
            "             {\"id\": \"w_warmth\", \"type\": 1, \"param\": \"Warmth\", \"label\": \"WARMTH\", \"style\": 4}],\n"
            "            [{\"id\": \"w_mix\", \"type\": 2, \"param\": \"Mix\", \"label\": \"DRY / WET MIX\", \"style\": 0},\n"
            "             {\"id\": \"w_vu\", \"type\": 5, \"param\": \"Level\", \"label\": \"DISTORTION PEAK\", \"style\": 0}]\n"
            "        ]\n"
            "    }\n\n"
            "def graph():\n"
            "    in_node = eat.node.input(type=\"AUDIO IN\")\n"
            "    dist = eat.node.fx(type=\"TUBE SAT\", drive=4.5, warmth=0.70)\n"
            "    out = eat.node.out(type=\"AUDIO OUT\", volume=1.0)\n"
            "    in_node >> dist\n"
            "    dist >> out\n"
            "    return out\n\n"
            "def process(in_l, in_r, params):\n"
            "    drive = params.get(\"Drive\", 4.5)\n"
            "    mix = params.get(\"Mix\", 0.85)\n"
            "    out_l = math.tanh(in_l * drive) * mix + in_l * (1.0 - mix)\n"
            "    out_r = math.tanh(in_r * drive) * mix + in_r * (1.0 - mix)\n"
            "    return [out_l, out_r]\n";
    }

    if (target.type == ScriptTargetType::MidiFx) {
        if (key.find("arp") != std::string::npos) {
            return
                "# --- Algorithmic 16-Step Arpeggiator (Eatscript MIDI FX) ---\n"
                "def init():\n"
                "    eat.param(\"Rate\", min=1.0, max=8.0, default=4.0)\n"
                "    eat.param(\"Octaves\", min=1.0, max=4.0, default=2.0)\n"
                "    eat.param(\"Gate\", min=0.1, max=1.0, default=0.75)\n"
                "    eat.param(\"Pattern\", min=0.0, max=4.0, default=0.0)\n\n"
                "def gui():\n"
                "    return {\n"
                "        \"title\": \"Arpeggiator MIDI FX\",\n"
                "        \"subtitle\": \"16th UpDown Generator\",\n"
                "        \"chassis\": 0,\n"
                "        \"woodCheeks\": True,\n"
                "        \"rows\": [\n"
                "            [{\"id\": \"w_rate\", \"type\": 1, \"param\": \"Rate\", \"label\": \"RATE\", \"style\": 4},\n"
                "             {\"id\": \"w_oct\", \"type\": 1, \"param\": \"Octaves\", \"label\": \"OCTAVES\", \"style\": 5},\n"
                "             {\"id\": \"w_gate\", \"type\": 1, \"param\": \"Gate\", \"label\": \"GATE\", \"style\": 6},\n"
                "             {\"id\": \"w_pat\", \"type\": 1, \"param\": \"Pattern\", \"label\": \"PATTERN\", \"style\": 4}]\n"
                "        ]\n"
                "    }\n\n"
                "def graph():\n"
                "    midi_in = eat.node.midi_in(type=\"MIDI IN\")\n"
                "    arp = eat.node.mfx(type=\"ARPEGGIATOR\", rate=4.0, octaves=2.0)\n"
                "    midi_out = eat.node.midi_out(type=\"MIDI OUT\")\n"
                "    midi_in >> arp\n"
                "    arp >> midi_out\n"
                "    return midi_out\n\n"
                "def process_notes(notes, context):\n"
                "    return notes\n";
        }

        return
            "# --- Real-Time MIDI Scale Quantizer (Eatscript MIDI FX) ---\n"
            "def init():\n"
            "    eat.param(\"RootKey\", min=0, max=11, default=0)\n"
            "    eat.param(\"ScaleMode\", min=0, max=6, default=0)\n"
            "    eat.param(\"HumanizeVel\", min=0.0, max=0.5, default=0.15)\n"
            "    eat.param(\"Transpose\", min=-24.0, max=24.0, default=0.0)\n\n"
            "def gui():\n"
            "    return {\n"
            "        \"title\": \"Scale Snap MIDI FX\",\n"
            "        \"subtitle\": \"C Natural Minor Quantize\",\n"
            "        \"chassis\": 0,\n"
            "        \"woodCheeks\": True,\n"
            "        \"rows\": [\n"
            "            [{\"id\": \"w_root\", \"type\": 1, \"param\": \"RootKey\", \"label\": \"ROOT KEY\", \"style\": 4},\n"
            "             {\"id\": \"w_mode\", \"type\": 1, \"param\": \"ScaleMode\", \"label\": \"SCALE MODE\", \"style\": 5},\n"
            "             {\"id\": \"w_hum\", \"type\": 1, \"param\": \"HumanizeVel\", \"label\": \"HUMANIZE\", \"style\": 6}]\n"
            "        ]\n"
            "    }\n\n"
            "def graph():\n"
            "    midi_in = eat.node.midi_in(type=\"MIDI IN\")\n"
            "    snap = eat.node.mfx(type=\"SCALE SNAP\", root=0, mode=0)\n"
            "    midi_out = eat.node.midi_out(type=\"MIDI OUT\")\n"
            "    midi_in >> snap\n"
            "    snap >> midi_out\n"
            "    return midi_out\n\n"
            "def process_notes(notes, context):\n"
            "    out = []\n"
            "    for n in notes:\n"
            "        snapped = eat.snap_to_scale(n.pitch, root=0, minor=True)\n"
            "        out.append(eat.Note(snapped, n.velocity, n.length))\n"
            "    return out\n";
    }

    return
        "# --- Procedural Synthesizer Engine (Eatscript) ---\n"
        "import math\n\n"
        "def init():\n"
        "    eat.param(\"Cutoff\", min=50.0, max=15000.0, default=2500.0)\n"
        "    eat.param(\"Resonance\", min=0.1, max=0.95, default=0.50)\n"
        "    eat.param(\"Attack\", min=0.001, max=1.0, default=0.05)\n"
        "    eat.param(\"Decay\", min=0.05, max=3.0, default=0.40)\n\n"
        "def gui():\n"
        "    return {\n"
        "        \"title\": \"" + target.title + "\",\n"
        "        \"subtitle\": \"" + target.subtitle + "\",\n"
        "        \"chassis\": 0,\n"
        "        \"woodCheeks\": True,\n"
        "        \"rows\": [\n"
        "            [{\"id\": \"w_cutoff\", \"type\": 1, \"param\": \"Cutoff\", \"label\": \"CUTOFF\", \"style\": 4},\n"
        "             {\"id\": \"w_res\", \"type\": 1, \"param\": \"Resonance\", \"label\": \"RESONANCE\", \"style\": 5},\n"
        "             {\"id\": \"w_att\", \"type\": 1, \"param\": \"Attack\", \"label\": \"ATTACK\", \"style\": 6},\n"
        "             {\"id\": \"w_dec\", \"type\": 1, \"param\": \"Decay\", \"label\": \"DECAY\", \"style\": 7}]\n"
        "        ]\n"
        "    }\n\n"
        "def graph():\n"
        "    osc = eat.node.osc(type=\"SYNTH OSC\", wave=0.0, tune=0.0)\n"
        "    vcf = eat.node.vcf(type=\"LADDER FILTER\", cutoff=0.6, resonance=0.5)\n"
        "    vca = eat.node.vca(type=\"VCA AMPLIFIER\", volume=0.8)\n"
        "    out = eat.node.out(type=\"MASTER OUT\", volume=0.85)\n"
        "    osc >> vcf\n"
        "    vcf >> vca\n"
        "    vca >> out\n"
        "    return out\n\n"
        "def process(time, freq, note, params):\n"
        "    saw = 2.0 * (time * freq - math.floor(time * freq + 0.5))\n"
        "    return saw * 0.5\n";
}

void DesignView::initModularRackFromScript(const std::string& code) {
    modules_.clear();
    patchCords_.clear();

    std::string lowerCode = code;
    for (char& c : lowerCode) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    size_t graphPos = lowerCode.find("def graph():");
    if (graphPos == std::string::npos) graphPos = lowerCode.find("def patch():");
    if (graphPos == std::string::npos) graphPos = lowerCode.find("def modular():");

    bool is303 = (lowerCode.find("303") != std::string::npos) || (getActiveTarget().id == "eats_303");
    if (is303 && (graphPos == std::string::npos || lowerCode.find("tb-303 core") != std::string::npos || lowerCode.find("diode") != std::string::npos)) {
        modules_ = {
            {"OSC 1: TB-303 CORE", "OSC", 30.0f, 80.0f, 240.0f, 320.0f, 0.20f, 0.21f, 0.24f,
             {{"TUNE", 0.50f, -24.0f, 24.0f, "st"}, {"WAVE", 0.0f, 0.0f, 1.0f, ""}, {"GLIDE", 0.35f, 0.0f, 1.0f, "s"}},
             {"CV IN", "GATE"}, {"OUT", "SUB"}},

            {"FILTER: 24dB DIODE", "VCF", 270.0f, 80.0f, 240.0f, 320.0f, 0.22f, 0.23f, 0.26f,
             {{"CUTOFF", 0.65f, 20.0f, 20000.0f, "Hz"}, {"RESON", 0.78f, 0.0f, 1.0f, ""}, {"ENV MOD", 0.60f, 0.0f, 1.0f, ""}, {"ACCENT", 0.70f, 0.0f, 1.0f, ""}},
             {"AUDIO IN", "FM IN"}, {"LPF OUT", "ENV OUT"}},

            {"VCA & SATURATION", "VCA", 510.0f, 80.0f, 210.0f, 320.0f, 0.19f, 0.20f, 0.23f,
             {{"DRIVE", 0.45f, 1.0f, 10.0f, "x"}, {"ATTACK", 0.05f, 0.001f, 1.0f, "s"}, {"DECAY", 0.45f, 0.01f, 2.0f, "s"}},
             {"IN", "CV"}, {"OUT", "MON"}},

            {"OUT: MASTER BUS", "OUT", 720.0f, 80.0f, 210.0f, 320.0f, 0.18f, 0.19f, 0.22f,
             {{"VOLUME", 0.85f, 0.0f, 1.0f, ""}, {"PAN", 0.0f, -1.0f, 1.0f, ""}, {"LIMIT", 0.90f, 0.0f, 1.0f, ""}},
             {"L IN", "R IN"}, {"MAIN L", "MAIN R"}}
        };
        patchCords_.push_back({0, 0, 1, 0, 1.0f, 0.55f, 0.0f});
        patchCords_.push_back({1, 1, 2, 0, 0.13f, 0.96f, 0.91f});
        modularLayoutDone_ = true;
        return;
    }

    std::istringstream stream(code);
    std::string line;
    bool inGraph = false;
    std::vector<std::string> nodeVarNames;
    float currentX = 30.0f;
    float row0Y = modularZoomPan_.getViewportOriginY();
    float modY = row0Y + kModuleTopRailOffset;
    std::vector<std::pair<std::string, std::string>> connections;

    while (std::getline(stream, line)) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

        if (trimmed.starts_with("def graph():") || trimmed.starts_with("def patch():") || trimmed.starts_with("def modular():")) {
            inGraph = true;
            continue;
        }
        if (inGraph && trimmed.starts_with("def ") && !trimmed.starts_with("def graph") && !trimmed.starts_with("def patch") && !trimmed.starts_with("def modular")) {
            inGraph = false;
        }

        if (inGraph) {
            size_t pipePos = trimmed.find(">>");
            if (pipePos != std::string::npos) {
                std::string from = trimmed.substr(0, pipePos);
                std::string to = trimmed.substr(pipePos + 2);
                from.erase(0, from.find_first_not_of(" \t"));
                from.erase(from.find_last_not_of(" \t") + 1);
                to.erase(0, to.find_first_not_of(" \t"));
                to.erase(to.find_last_not_of(" \t") + 1);
                if (!from.empty() && !to.empty()) {
                    connections.push_back({from, to});
                }
                continue;
            }

            size_t nodePos = trimmed.find("eat.node.");
            if (nodePos != std::string::npos) {
                size_t eqPos = trimmed.find('=');
                std::string varName = "node_" + std::to_string(modules_.size());
                if (eqPos != std::string::npos && eqPos < nodePos) {
                    varName = trimmed.substr(0, eqPos);
                    varName.erase(0, varName.find_first_not_of(" \t"));
                    varName.erase(varName.find_last_not_of(" \t") + 1);
                }

                size_t typeStart = nodePos + 9;
                size_t openParen = trimmed.find('(', typeStart);
                if (openParen != std::string::npos) {
                    std::string nodeType = trimmed.substr(typeStart, openParen - typeStart);
                    for (char& c : nodeType) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

                    ModularModuleDef modDef;
                    modDef.type = nodeType;
                    modDef.title = varName;
                    for (char& c : modDef.title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    modDef.w = 210.0f;
                    modDef.h = 320.0f;
                    modDef.x = currentX;
                    modDef.y = modY;

                    size_t typeKwarg = trimmed.find("type=\"");
                    if (typeKwarg != std::string::npos) {
                        size_t endQuote = trimmed.find('"', typeKwarg + 6);
                        if (endQuote != std::string::npos) {
                            modDef.title = trimmed.substr(typeKwarg + 6, endQuote - (typeKwarg + 6));
                        }
                    }

                    if (nodeType.find("OSC") != std::string::npos || nodeType.find("VCO") != std::string::npos) {
                        modDef.r = 0.20f; modDef.g = 0.21f; modDef.b = 0.24f;
                        modDef.knobs = {{"TUNE", 0.50f, -24.0f, 24.0f, "st"}, {"WAVE", 0.0f, 0.0f, 1.0f, ""}, {"GLIDE", 0.35f, 0.0f, 1.0f, "s"}};
                        modDef.inputs = {"CV IN", "GATE"};
                        modDef.outputs = {"OUT", "SUB"};
                    } else if (nodeType.find("VCF") != std::string::npos || nodeType.find("FILTER") != std::string::npos || nodeType.find("SVF") != std::string::npos) {
                        modDef.r = 0.22f; modDef.g = 0.23f; modDef.b = 0.26f;
                        modDef.knobs = {{"CUTOFF", 0.65f, 20.0f, 20000.0f, "Hz"}, {"RESON", 0.78f, 0.0f, 1.0f, ""}, {"ENV MOD", 0.60f, 0.0f, 1.0f, ""}, {"ACCENT", 0.70f, 0.0f, 1.0f, ""}};
                        modDef.inputs = {"AUDIO IN", "FM IN"};
                        modDef.outputs = {"LPF OUT", "ENV OUT"};
                    } else if (nodeType.find("VCA") != std::string::npos || nodeType.find("SAT") != std::string::npos) {
                        modDef.r = 0.19f; modDef.g = 0.20f; modDef.b = 0.23f;
                        modDef.knobs = {{"DRIVE", 0.45f, 1.0f, 10.0f, "x"}, {"ATTACK", 0.05f, 0.001f, 1.0f, "s"}, {"DECAY", 0.45f, 0.01f, 2.0f, "s"}};
                        modDef.inputs = {"IN", "CV"};
                        modDef.outputs = {"OUT", "MON"};
                    } else if (nodeType.find("OUT") != std::string::npos || nodeType.find("MASTER") != std::string::npos) {
                        modDef.r = 0.18f; modDef.g = 0.19f; modDef.b = 0.22f;
                        modDef.knobs = {{"VOLUME", 0.85f, 0.0f, 1.0f, ""}, {"PAN", 0.0f, -1.0f, 1.0f, ""}, {"LIMIT", 0.90f, 0.0f, 1.0f, ""}};
                        modDef.inputs = {"L IN", "R IN"};
                        modDef.outputs = {"MAIN L", "MAIN R"};
                    } else if (nodeType.find("ENV") != std::string::npos || nodeType.find("ADSR") != std::string::npos) {
                        modDef.r = 0.24f; modDef.g = 0.22f; modDef.b = 0.19f;
                        modDef.knobs = {{"ATTACK", 0.05f, 0.001f, 1.0f, "s"}, {"DECAY", 0.35f, 0.01f, 2.0f, "s"}, {"SUSTAIN", 0.70f, 0.0f, 1.0f, "%"}, {"RELEASE", 0.30f, 0.01f, 3.0f, "s"}};
                        modDef.inputs = {"GATE", "RETRIG"};
                        modDef.outputs = {"ENV OUT", "INV OUT"};
                    } else if (nodeType.find("NOISE") != std::string::npos) {
                        modDef.r = 0.23f; modDef.g = 0.20f; modDef.b = 0.22f;
                        modDef.knobs = {{"SNAPPY", 0.65f, 0.0f, 1.0f, ""}, {"DECAY", 0.25f, 0.01f, 1.0f, "s"}};
                        modDef.inputs = {"CV IN"};
                        modDef.outputs = {"NOISE OUT"};
                    } else {
                        modDef.r = 0.20f; modDef.g = 0.22f; modDef.b = 0.25f;
                        modDef.knobs = {{"PARAM 1", 0.50f, 0.0f, 1.0f, ""}, {"PARAM 2", 0.50f, 0.0f, 1.0f, ""}};
                        modDef.inputs = {"IN 1", "CV IN"};
                        modDef.outputs = {"OUT 1", "OUT 2"};
                    }

                    int wHp = std::max(1, static_cast<int>(std::round(modDef.w / kHpWidth)));
                    modDef.w = wHp * kHpWidth;
                    currentX += modDef.w;
                    nodeVarNames.push_back(varName);
                    modules_.push_back(modDef);
                }
            }
        }
    }

    if (modules_.empty()) {
        modules_ = {
            {"OSC 1: TB-303 CORE", "OSC", 30.0f, 80.0f, 240.0f, 320.0f, 0.20f, 0.21f, 0.24f,
             {{"TUNE", 0.50f, -24.0f, 24.0f, "st"}, {"WAVE", 0.0f, 0.0f, 1.0f, ""}, {"GLIDE", 0.35f, 0.0f, 1.0f, "s"}},
             {"CV IN", "GATE"}, {"OUT", "SUB"}},

            {"FILTER: 24dB DIODE", "VCF", 270.0f, 80.0f, 240.0f, 320.0f, 0.22f, 0.23f, 0.26f,
             {{"CUTOFF", 0.65f, 20.0f, 20000.0f, "Hz"}, {"RESON", 0.78f, 0.0f, 1.0f, ""}, {"ENV MOD", 0.60f, 0.0f, 1.0f, ""}, {"ACCENT", 0.70f, 0.0f, 1.0f, ""}},
             {"AUDIO IN", "FM IN"}, {"LPF OUT", "ENV OUT"}},

            {"VCA & SATURATION", "VCA", 510.0f, 80.0f, 210.0f, 320.0f, 0.19f, 0.20f, 0.23f,
             {{"DRIVE", 0.45f, 1.0f, 10.0f, "x"}, {"ATTACK", 0.05f, 0.001f, 1.0f, "s"}, {"DECAY", 0.45f, 0.01f, 2.0f, "s"}},
             {"IN", "CV"}, {"OUT", "MON"}},

            {"OUT: MASTER BUS", "OUT", 720.0f, 80.0f, 210.0f, 320.0f, 0.18f, 0.19f, 0.22f,
             {{"VOLUME", 0.85f, 0.0f, 1.0f, ""}, {"PAN", 0.0f, -1.0f, 1.0f, ""}, {"LIMIT", 0.90f, 0.0f, 1.0f, ""}},
             {"L IN", "R IN"}, {"MAIN L", "MAIN R"}}
        };
        patchCords_.push_back({0, 0, 1, 0, 1.0f, 0.55f, 0.0f});
        patchCords_.push_back({1, 1, 2, 0, 0.13f, 0.96f, 0.91f});
    } else {
        static const float cordColors[][3] = {
            {1.0f, 0.55f, 0.0f},
            {0.13f, 0.96f, 0.91f},
            {1.0f, 0.85f, 0.0f},
            {1.0f, 0.16f, 0.55f},
            {0.2f, 1.0f, 0.45f}
        };
        int cordIdx = 0;
        for (const auto& conn : connections) {
            int srcIdx = -1, dstIdx = -1;
            for (size_t i = 0; i < nodeVarNames.size(); ++i) {
                if (nodeVarNames[i] == conn.first) srcIdx = static_cast<int>(i);
                if (nodeVarNames[i] == conn.second) dstIdx = static_cast<int>(i);
            }
            if (srcIdx >= 0 && dstIdx >= 0 && srcIdx < static_cast<int>(modules_.size()) && dstIdx < static_cast<int>(modules_.size())) {
                const auto& col = cordColors[cordIdx % 5];
                patchCords_.push_back({srcIdx, 0, dstIdx, 0, col[0], col[1], col[2]});
                cordIdx++;
            }
        }
        if (patchCords_.empty() && modules_.size() >= 2) {
            patchCords_.push_back({0, 0, 1, 0, 1.0f, 0.55f, 0.0f});
            if (modules_.size() >= 3) {
                patchCords_.push_back({1, 0, 2, 0, 0.13f, 0.96f, 0.91f});
            }
        }
    }
    modularLayoutDone_ = true;
}

void DesignView::addModule(const ModularModuleDef& mod) {
    ModularModuleDef m = mod;
    int widthHp = std::max(1, static_cast<int>(std::round(m.w / kHpWidth)));
    m.w = widthHp * kHpWidth;
    if (m.h <= 0.0f) m.h = 320.0f;

    float row0Y = modularZoomPan_.getViewportOriginY();
    if (m.x == 0.0f && m.y == 0.0f && !modules_.empty()) {
        float rightmostX = 30.0f;
        for (const auto& existing : modules_) {
            if (existing.x + existing.w > rightmostX) {
                rightmostX = existing.x + existing.w;
            }
        }
        m.x = std::round(rightmostX / kHpWidth) * kHpWidth;
        m.y = row0Y + kModuleTopRailOffset;
    } else {
        m.x = std::round(m.x / kHpWidth) * kHpWidth;
        int targetRow = static_cast<int>(std::round((m.y - (row0Y + kModuleTopRailOffset)) / kRowHeight));
        m.y = row0Y + targetRow * kRowHeight + kModuleTopRailOffset;
    }
    modules_.push_back(m);
}

void DesignView::openModuleSearchDialog(float worldSpawnX, float worldSpawnY) {
    float row0Y = modularZoomPan_.getViewportOriginY();
    if (worldSpawnX == 0.0f && worldSpawnY == 0.0f) {
        float nextX = 30.0f;
        for (const auto& mod : modules_) {
            if (mod.x + mod.w > nextX) {
                nextX = mod.x + mod.w;
            }
        }
        pendingModuleSpawnX_ = std::round(nextX / kHpWidth) * kHpWidth;
        pendingModuleSpawnY_ = row0Y + kModuleTopRailOffset;
    } else {
        float snappedX = std::round(worldSpawnX / kHpWidth) * kHpWidth;
        int targetRow = static_cast<int>(std::round((worldSpawnY - (row0Y + kModuleTopRailOffset)) / kRowHeight));
        pendingModuleSpawnX_ = snappedX;
        pendingModuleSpawnY_ = row0Y + targetRow * kRowHeight + kModuleTopRailOffset;
    }
    moduleSearchDialog_.open(PluginDialogMode::AddModule, "Modular Rack");
}

void DesignView::applyModuleBumping(int draggedIdx, int targetRow, int targetHp) {
    if (draggedIdx < 0 || draggedIdx >= static_cast<int>(modules_.size())) return;
    if (initialModuleStates_.empty()) return;

    float row0Y = modularZoomPan_.getViewportOriginY();
    auto& draggedMod = modules_[draggedIdx];
    int widthHpD = std::max(1, static_cast<int>(std::round(draggedMod.w / kHpWidth)));

    // Update dragged module position strictly snapped to discrete HP rivet and row
    draggedMod.x = targetHp * kHpWidth;
    draggedMod.y = row0Y + targetRow * kRowHeight + kModuleTopRailOffset;

    // Reset all other modules to their initial recorded positions
    for (size_t i = 0; i < modules_.size(); ++i) {
        if (static_cast<int>(i) == draggedIdx) continue;
        modules_[i].x = initialModuleStates_[i].x;
        modules_[i].y = initialModuleStates_[i].y;
    }

    // Collect all other modules originally in targetRow
    struct RowItem {
        int index;
        int initialHp;
        int widthHp;
        int currentHp;
    };
    std::vector<RowItem> rowItems;
    rowItems.reserve(modules_.size());

    for (size_t i = 0; i < initialModuleStates_.size(); ++i) {
        if (static_cast<int>(i) == draggedIdx) continue;
        if (initialModuleStates_[i].row == targetRow) {
            rowItems.push_back({
                static_cast<int>(i),
                initialModuleStates_[i].hp,
                initialModuleStates_[i].widthHp,
                initialModuleStates_[i].hp
            });
        }
    }

    if (rowItems.empty()) return;

    // Sort by initial HP ascending
    std::sort(rowItems.begin(), rowItems.end(), [](const RowItem& a, const RowItem& b) {
        return a.initialHp < b.initialHp;
    });

    // Check if dragged module overlaps with any module in targetRow
    bool hasOverlap = false;
    for (const auto& item : rowItems) {
        int overlapStart = std::max(item.initialHp, targetHp);
        int overlapEnd = std::min(item.initialHp + item.widthHp, targetHp + widthHpD);
        if (overlapStart < overlapEnd) {
            hasOverlap = true;
            break;
        }
    }

    // If no overlap, every module in targetRow stays at its initial position
    if (!hasOverlap) return;

    // Separate modules into left group (bumped to left of D) and right group (bumped to right of D)
    // based on whether the module's initial center is left or right of the dragged module's center.
    float centerD = targetHp + (widthHpD * 0.5f);

    std::vector<RowItem> leftGroup;
    std::vector<RowItem> rightGroup;

    for (const auto& item : rowItems) {
        float centerItem = item.initialHp + (item.widthHp * 0.5f);
        if (centerItem < centerD) {
            leftGroup.push_back(item);
        } else {
            rightGroup.push_back(item);
        }
    }

    // Resolve left group from right to left (closest to D first)
    int maxRightEdge = targetHp;
    for (int j = static_cast<int>(leftGroup.size()) - 1; j >= 0; --j) {
        auto& item = leftGroup[j];
        int desiredHp = item.initialHp;
        if (desiredHp + item.widthHp > maxRightEdge) {
            desiredHp = maxRightEdge - item.widthHp;
        }
        item.currentHp = desiredHp;
        maxRightEdge = desiredHp;
    }

    // Resolve right group from left to right (closest to D first)
    int minLeftEdge = targetHp + widthHpD;
    for (size_t j = 0; j < rightGroup.size(); ++j) {
        auto& item = rightGroup[j];
        int desiredHp = item.initialHp;
        if (desiredHp < minLeftEdge) {
            desiredHp = minLeftEdge;
        }
        item.currentHp = desiredHp;
        minLeftEdge = desiredHp + item.widthHp;
    }

    // Apply computed positions to modules
    for (const auto& item : leftGroup) {
        modules_[item.index].x = item.currentHp * kHpWidth;
        modules_[item.index].y = row0Y + targetRow * kRowHeight + kModuleTopRailOffset;
    }
    for (const auto& item : rightGroup) {
        modules_[item.index].x = item.currentHp * kHpWidth;
        modules_[item.index].y = row0Y + targetRow * kRowHeight + kModuleTopRailOffset;
    }
}

void DesignView::initDefaultTargetsAndCode() {
    allTargets_.clear();

    // 1. Synths & DSP (4 tracks)
    allTargets_.push_back({"eats_kick", "Eats Kick (Synth DSP)", "Procedural Sub Kick Drum", ScriptTargetType::TrackDsp, "SYNTH DSP", {0.0f, 0.95f, 1.0f}, {1.0f, 0.55f, 0.0f}, 0, -1, -1});
    allTargets_.push_back({"eats_snare", "Eats Snare (Synth DSP)", "Analog Filtered Noise Snare", ScriptTargetType::TrackDsp, "SYNTH DSP", {0.0f, 0.95f, 1.0f}, {1.0f, 0.85f, 0.0f}, 1, -1, -1});
    allTargets_.push_back({"eats_hats", "Eats Hats (Synth DSP)", "Metallic Phase Hi-Hat", ScriptTargetType::TrackDsp, "SYNTH DSP", {0.0f, 0.95f, 1.0f}, {0.13f, 0.96f, 0.91f}, 2, -1, -1});
    allTargets_.push_back({"eats_303", "Eats-303 (Synth DSP)", "Acid Diode Ladder Synth", ScriptTargetType::TrackDsp, "SYNTH DSP", {0.0f, 0.95f, 1.0f}, {0.2f, 1.0f, 0.45f}, 3, -1, -1});

    // 2. Audio FX Inserts (3 tracks)
    allTargets_.push_back({"fx_kick_dist", "TubeDistortion (Eats Kick)", "Warm Harmonic Saturation", ScriptTargetType::AudioFx, "AUDIO FX", {1.0f, 0.16f, 0.55f}, {1.0f, 0.55f, 0.0f}, 0, -1, 0});
    allTargets_.push_back({"fx_snare_dist", "TubeDistortion (Eats Snare)", "Warm Harmonic Saturation", ScriptTargetType::AudioFx, "AUDIO FX", {1.0f, 0.16f, 0.55f}, {1.0f, 0.85f, 0.0f}, 1, -1, 0});
    allTargets_.push_back({"fx_303_dist", "TubeDistortion (Eats-303)", "Warm Harmonic Saturation", ScriptTargetType::AudioFx, "AUDIO FX", {1.0f, 0.16f, 0.55f}, {0.2f, 1.0f, 0.45f}, 3, -1, 0});

    // 2b. MIDI FX Processors
    allTargets_.push_back({"mfx_scale_snap", "ScaleSnap (C Natural Minor)", "Quantize Pitch to Active Key", ScriptTargetType::MidiFx, "MIDI FX", {1.0f, 0.85f, 0.0f}, {1.0f, 0.55f, 0.0f}, 0, -1, 0});
    allTargets_.push_back({"mfx_arpeggiator", "Arpeggiator (16th UpDown)", "Algorithmic Pattern Generator", ScriptTargetType::MidiFx, "MIDI FX", {1.0f, 0.85f, 0.0f}, {0.13f, 0.96f, 0.91f}, 1, -1, 0});
    allTargets_.push_back({"mfx_chord_follow", "Chord Follower (Triad Lock)", "Harmonic Progression Tracking", ScriptTargetType::MidiFx, "MIDI FX", {1.0f, 0.85f, 0.0f}, {0.2f, 1.0f, 0.45f}, 3, -1, 0});

    activeTargetIndex_ = 3;
    updateActiveTargetCodeAndParams();
}

void DesignView::syncWithProject(const std::vector<ArrangerTimelineTrack>& tracks) {
    std::string prevSelectedId = (activeTargetIndex_ >= 0 && activeTargetIndex_ < static_cast<int>(allTargets_.size()))
                                     ? allTargets_[activeTargetIndex_].id : "";
    int prevTrack = (activeTargetIndex_ >= 0 && activeTargetIndex_ < static_cast<int>(allTargets_.size()))
                        ? allTargets_[activeTargetIndex_].trackIndex : -1;
    ScriptTargetType prevType = (activeTargetIndex_ >= 0 && activeTargetIndex_ < static_cast<int>(allTargets_.size()))
                                    ? allTargets_[activeTargetIndex_].type : ScriptTargetType::TrackDsp;
    int prevFx = (activeTargetIndex_ >= 0 && activeTargetIndex_ < static_cast<int>(allTargets_.size()))
                     ? allTargets_[activeTargetIndex_].fxIndex : -1;

    cachedTracks_ = tracks;
    if (cachedTracks_.empty()) {
        initDefaultTargetsAndCode();
        return;
    }

    allTargets_.clear();

    // 1. Synths & DSP (Active tracks)
    for (size_t t = 0; t < cachedTracks_.size(); ++t) {
        const auto& trk = cachedTracks_[t];
        std::string engName = trk.instrument.empty() ? (trk.instrumentEngine.empty() ? "Synth DSP" : trk.instrumentEngine) : trk.instrument;
        std::string sub = trk.instrumentEngine.empty() ? "Procedural Synthesizer" : (trk.instrumentEngine + " DSP Engine");
        std::string id = "track_" + std::to_string(t) + "_dsp";
        Color trkCol{trk.r, trk.g, trk.b};
        allTargets_.push_back({id, trk.name + " (" + engName + ")", sub, ScriptTargetType::TrackDsp,
                               "SYNTH DSP", Color{0.0f, 0.95f, 1.0f}, trkCol, static_cast<int>(t), -1, -1});
    }

    // 2. Audio FX Inserts
    for (size_t t = 0; t < cachedTracks_.size(); ++t) {
        const auto& trk = cachedTracks_[t];
        Color trkCol{trk.r, trk.g, trk.b};
        for (size_t f = 0; f < trk.audioFx.size(); ++f) {
            const auto& afx = trk.audioFx[f];
            std::string id = "track_" + std::to_string(t) + "_afx_" + std::to_string(f);
            std::string sub = afx.type.empty() ? "Audio FX Insert" : (afx.type + " Insert");
            allTargets_.push_back({id, afx.name + " (" + trk.name + ")", sub, ScriptTargetType::AudioFx,
                                   "AUDIO FX", Color{1.0f, 0.16f, 0.55f}, trkCol, static_cast<int>(t), -1, static_cast<int>(f)});
        }
    }

    // 3. Track MIDI FX Processors
    for (size_t t = 0; t < cachedTracks_.size(); ++t) {
        const auto& trk = cachedTracks_[t];
        Color trkCol{trk.r, trk.g, trk.b};
        for (size_t m = 0; m < trk.midiFx.size(); ++m) {
            const auto& mfx = trk.midiFx[m];
            std::string id = "track_" + std::to_string(t) + "_mfx_" + std::to_string(m);
            std::string sub = mfx.type.empty() ? "MIDI FX Processor" : (mfx.type + " Processor");
            allTargets_.push_back({id, mfx.name + " (" + trk.name + ")", sub, ScriptTargetType::MidiFx,
                                   "MIDI FX", Color{1.0f, 0.85f, 0.0f}, trkCol, static_cast<int>(t), -1, static_cast<int>(m)});
        }
    }

    // Restore selection or select best matching target
    int targetToSelect = 0;
    if (!prevSelectedId.empty()) {
        for (size_t i = 0; i < allTargets_.size(); ++i) {
            if (allTargets_[i].id == prevSelectedId) {
                targetToSelect = static_cast<int>(i);
                break;
            }
        }
    } else if (prevTrack >= 0) {
        for (size_t i = 0; i < allTargets_.size(); ++i) {
            if (allTargets_[i].type == prevType && allTargets_[i].trackIndex == prevTrack &&
                (prevFx < 0 || allTargets_[i].fxIndex == prevFx)) {
                targetToSelect = static_cast<int>(i);
                break;
            }
        }
    }

    activeTargetIndex_ = targetToSelect;
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
        std::string nameUpper = activeT.title + " " + activeT.subtitle;
        for (char& c : nameUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (nameUpper.find("CRUSH") != std::string::npos || nameUpper.find("8-BIT") != std::string::npos || nameUpper.find("BITCRUSHER") != std::string::npos) {
            guiPanel_.chassisStyle = GuiChassisStyle::PcbGreen;
            GuiRowDef r1;
            r1.widgets.push_back({"w_bits", GuiWidgetType::Knob, "BIT DEPTH", "Bits", GuiKnobStyle::AnodizedKnurled, 56.0f, 0.5f, 1.0f, 16.0f, "bit", {0.13f, 0.96f, 0.91f}});
            r1.widgets.push_back({"w_downsample", GuiWidgetType::Knob, "DOWNSAMPLE", "Downsample", GuiKnobStyle::CreamFluted, 56.0f, 0.2f, 1.0f, 32.0f, "x", {1.0f, 0.55f, 0.0f}});
            r1.widgets.push_back({"w_drive", GuiWidgetType::Knob, "PRE-DRIVE", "Drive", GuiKnobStyle::BakeliteSkirt, 56.0f, 0.25f, 0.5f, 4.0f, "x", {1.0f, 0.16f, 0.55f}});
            r1.widgets.push_back({"w_mix", GuiWidgetType::Knob, "DRY / WET", "Mix", GuiKnobStyle::TwoToneStepped, 56.0f, 0.85f, 0.0f, 1.0f, "%", {0.0f, 0.95f, 1.0f}});
            guiPanel_.rows.push_back(r1);
        } else if (nameUpper.find("DELAY") != std::string::npos || nameUpper.find("ECHO") != std::string::npos) {
            guiPanel_.chassisStyle = GuiChassisStyle::Silver;
            GuiRowDef r1;
            r1.widgets.push_back({"w_time", GuiWidgetType::Knob, "DELAY TIME", "Time", GuiKnobStyle::CreamFluted, 56.0f, 0.375f, 10.0f, 1000.0f, "ms", {0.0f, 0.95f, 1.0f}});
            r1.widgets.push_back({"w_fb", GuiWidgetType::Knob, "FEEDBACK", "Feedback", GuiKnobStyle::AnodizedKnurled, 56.0f, 0.45f, 0.0f, 0.95f, "%", {1.0f, 0.55f, 0.0f}});
            r1.widgets.push_back({"w_damp", GuiWidgetType::Knob, "HIGH DAMP", "Damp", GuiKnobStyle::BakeliteSkirt, 56.0f, 0.4f, 1000.0f, 20000.0f, "Hz", {1.0f, 0.85f, 0.0f}});
            r1.widgets.push_back({"w_mix", GuiWidgetType::Knob, "WET MIX", "Mix", GuiKnobStyle::TwoToneStepped, 56.0f, 0.40f, 0.0f, 1.0f, "%", {1.0f, 0.16f, 0.55f}});
            guiPanel_.rows.push_back(r1);
        } else if (nameUpper.find("EQ") != std::string::npos) {
            guiPanel_.chassisStyle = GuiChassisStyle::MinimalWhite;
            GuiRowDef r1;
            r1.widgets.push_back({"w_low", GuiWidgetType::Knob, "LOW GAIN", "LowGain", GuiKnobStyle::Standard, 56.0f, 0.5f, -12.0f, 12.0f, "dB", {1.0f, 0.55f, 0.0f}});
            r1.widgets.push_back({"w_mid", GuiWidgetType::Knob, "MID GAIN", "MidGain", GuiKnobStyle::Standard, 56.0f, 0.5f, -12.0f, 12.0f, "dB", {1.0f, 0.85f, 0.0f}});
            r1.widgets.push_back({"w_high", GuiWidgetType::Knob, "HIGH GAIN", "HighGain", GuiKnobStyle::Standard, 56.0f, 0.5f, -12.0f, 12.0f, "dB", {0.13f, 0.96f, 0.91f}});
            r1.widgets.push_back({"w_midf", GuiWidgetType::Knob, "MID FREQ", "MidFreq", GuiKnobStyle::Standard, 56.0f, 0.3f, 200.0f, 5000.0f, "Hz", {0.0f, 0.95f, 1.0f}});
            guiPanel_.rows.push_back(r1);
        } else if (nameUpper.find("LIMIT") != std::string::npos) {
            guiPanel_.chassisStyle = GuiChassisStyle::Grunge;
            GuiRowDef r1;
            r1.widgets.push_back({"w_ceil", GuiWidgetType::Knob, "CEILING", "Ceiling", GuiKnobStyle::BakeliteSkirt, 56.0f, 0.99f, -12.0f, 0.0f, "dB", {1.0f, 0.16f, 0.55f}});
            r1.widgets.push_back({"w_thresh", GuiWidgetType::Knob, "THRESHOLD", "Threshold", GuiKnobStyle::TwoToneStepped, 56.0f, 0.85f, -24.0f, 0.0f, "dB", {1.0f, 0.55f, 0.0f}});
            r1.widgets.push_back({"w_rel", GuiWidgetType::Knob, "RELEASE", "Release", GuiKnobStyle::CreamFluted, 56.0f, 0.2f, 10.0f, 500.0f, "ms", {0.0f, 0.95f, 1.0f}});
            r1.widgets.push_back({"w_vu", GuiWidgetType::VuMeter, "GAIN REDUCT", "GR", GuiKnobStyle::Standard, 60.0f, 0.2f, 0.0f, 1.0f, "dB", {1.0f, 0.85f, 0.0f}});
            guiPanel_.rows.push_back(r1);
        } else {
            guiPanel_.chassisStyle = GuiChassisStyle::Walnut;
            GuiRowDef r1;
            r1.widgets.push_back({"w_drive", GuiWidgetType::Knob, "SATURATION DRIVE", "Drive", GuiKnobStyle::BakeliteSkirt, 56.0f, 0.60f, 1.0f, 20.0f, "x", {1.0f, 0.16f, 0.55f}});
            r1.widgets.push_back({"w_warmth", GuiWidgetType::Knob, "WARMTH HARMONICS", "Warmth", GuiKnobStyle::CreamFluted, 56.0f, 0.70f, 0.0f, 1.0f, "", {1.0f, 0.85f, 0.0f}});
            guiPanel_.rows.push_back(r1);

            GuiRowDef r2;
            r2.widgets.push_back({"w_mix", GuiWidgetType::Slider, "DRY / WET MIX", "Mix", GuiKnobStyle::Standard, 140.0f, 0.85f, 0.0f, 1.0f, "%", {0.0f, 0.95f, 1.0f}});
            r2.widgets.push_back({"w_vu", GuiWidgetType::VuMeter, "DISTORTION PEAK", "Level", GuiKnobStyle::Standard, 60.0f, 0.75f, 0.0f, 1.0f, "dB", {1.0f, 0.16f, 0.55f}});
            guiPanel_.rows.push_back(r2);
        }
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
        if (currentParams_.size() > 4) {
            GuiRowDef r2;
            for (size_t i = 4; i < currentParams_.size() && i < 8; ++i) {
                r2.widgets.push_back({"w_" + std::to_string(i), GuiWidgetType::Knob, currentParams_[i].name, currentParams_[i].name,
                                      static_cast<GuiKnobStyle>(i % 5), 56.0f, currentParams_[i].currentVal,
                                      currentParams_[i].minVal, currentParams_[i].maxVal, currentParams_[i].unit,
                                      activeT.badgeColor});
            }
            if (!r2.widgets.empty()) {
                guiPanel_.rows.push_back(r2);
            }
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
    if (guiPanel_.chassisTint.has_value()) {
        ss << "        \"chassisTint\": " << guiPanel_.chassisTint->toRgba8() << ",\n";
    }
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

    std::string engKey = t.id + " " + t.title + " " + t.subtitle;
    if (t.trackIndex >= 0 && t.trackIndex < static_cast<int>(cachedTracks_.size())) {
        const auto& trk = cachedTracks_[t.trackIndex];
        engKey += " " + trk.instrument + " " + trk.instrumentEngine;
    }
    for (auto& c : engKey) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // Check if user or session already has cached/edited script code
    auto itCode = targetCodeMap_.find(t.id);
    if (itCode != targetCodeMap_.end()) {
        currentScriptCode_ = itCode->second;
        auto itParams = targetParamsMap_.find(t.id);
        if (itParams != targetParamsMap_.end()) {
            currentParams_ = itParams->second;
        }
        setScriptCode(currentScriptCode_);
        initDefaultGuiPanel();
        return;
    }

    currentScriptCode_ = getDefaultScriptForTarget(t);

    if (t.id == "eats_kick") {
        currentParams_ = {
            {"StartFreq", 100.0f, 300.0f, 160.0f, 120.2f, "Hz"},
            {"EndFreq", 30.0f, 60.0f, 42.0f, 56.4f, "Hz"},
            {"PitchDecay", 0.01f, 0.20f, 0.035f, 0.035f, "s"},
            {"AmpDecay", 0.05f, 4.00f, 0.35f, 0.80f, "s"},
            {"Click", 0.0f, 1.0f, 0.0f, 0.0f, ""}
        };
    } else if (t.id == "eats_snare") {
        currentParams_ = {
            {"Tone", 100.0f, 350.0f, 180.0f, 185.0f, "Hz"},
            {"Snappy", 0.0f, 1.0f, 0.65f, 0.72f, ""},
            {"Decay", 0.05f, 1.0f, 0.25f, 0.28f, "s"},
            {"Tune", 0.5f, 2.0f, 1.0f, 1.05f, "x"}
        };
    } else if (t.id == "eats_hats") {
        currentParams_ = {
            {"Decay", 0.01f, 0.8f, 0.08f, 0.08f, "s"},
            {"Metallic", 0.1f, 1.0f, 0.75f, 0.80f, ""},
            {"Pitch", 4000.0f, 12000.0f, 8500.0f, 8600.0f, "Hz"}
        };
    } else if (engKey.find("303") != std::string::npos || t.id == "eats_303") {
        currentParams_ = {
            {"Cutoff", 200.0f, 4000.0f, 850.0f, 920.0f, "Hz"},
            {"Resonance", 0.1f, 0.98f, 0.78f, 0.85f, ""},
            {"EnvMod", 0.0f, 1.0f, 0.65f, 0.60f, ""},
            {"Decay", 0.05f, 1.5f, 0.45f, 0.45f, "s"},
            {"Accent", 0.0f, 1.0f, 0.70f, 0.75f, ""},
            {"Drive", 0.0f, 1.0f, 0.25f, 0.25f, ""}
        };
    } else if (engKey.find("dx7") != std::string::npos || engKey.find("fm") != std::string::npos) {
        currentParams_ = {
            {"Algorithm", 1.0f, 32.0f, 5.0f, 5.0f, ""},
            {"Feedback", 0.0f, 7.0f, 6.0f, 6.0f, ""},
            {"Brightness", 0.0f, 2.0f, 1.0f, 1.0f, ""},
            {"TineBell", 0.0f, 2.0f, 0.85f, 0.85f, ""},
            {"BodyWarmth", 0.0f, 2.0f, 1.0f, 1.0f, ""}
        };
    } else if (engKey.find("sid") != std::string::npos || engKey.find("c64") != std::string::npos) {
        currentParams_ = {
            {"Waveform", 0.0f, 3.0f, 0.0f, 0.0f, ""},
            {"Cutoff", 30.0f, 12000.0f, 1200.0f, 1200.0f, "Hz"},
            {"Resonance", 0.0f, 15.0f, 9.0f, 9.0f, ""},
            {"Overdrive", 1.0f, 4.0f, 1.2f, 1.2f, "x"}
        };
    } else if (t.trackIndex >= 0 && t.trackIndex < static_cast<int>(cachedTracks_.size())) {
        const auto& trk = cachedTracks_[t.trackIndex];
        std::string k1 = trk.knob1Name.empty() ? "Tone" : trk.knob1Name;
        std::string k2 = trk.knob2Name.empty() ? "Snappy" : trk.knob2Name;
        std::string k3 = trk.knob3Name.empty() ? "Decay" : trk.knob3Name;
        std::string k4 = trk.knob4Name.empty() ? "Var" : trk.knob4Name;
        currentParams_ = {
            {k1, 0.0f, 1.0f, trk.knob1, trk.knob1, ""},
            {k2, 0.0f, 1.0f, trk.knob2, trk.knob2, ""},
            {k3, 0.0f, 1.0f, trk.knob3, trk.knob3, ""},
            {k4, 0.0f, 1.0f, trk.knob4, trk.knob4, ""}
        };
    } else if (t.type == ScriptTargetType::AudioFx) {
        if (engKey.find("crush") != std::string::npos || engKey.find("8-bit") != std::string::npos || engKey.find("bitcrusher") != std::string::npos) {
            currentParams_ = {
                {"Bits", 1.0f, 16.0f, 8.0f, 8.0f, "bit"},
                {"Downsample", 1.0f, 32.0f, 4.0f, 4.0f, "x"},
                {"Drive", 0.5f, 4.0f, 1.0f, 1.0f, "x"},
                {"Mix", 0.0f, 1.0f, 0.85f, 0.85f, "%"}
            };
        } else {
            currentParams_ = {
                {"Drive", 1.0f, 20.0f, 4.5f, 6.2f, "x"},
                {"Warmth", 0.0f, 1.0f, 0.70f, 0.75f, ""},
                {"Mix", 0.0f, 1.0f, 0.85f, 0.90f, "%"}
            };
        }
    } else if (t.type == ScriptTargetType::MidiFx) {
        if (engKey.find("arp") != std::string::npos) {
            currentParams_ = {
                {"Rate", 1.0f, 8.0f, 4.0f, 4.0f, "st"},
                {"Octaves", 1.0f, 4.0f, 2.0f, 2.0f, "oct"},
                {"Gate", 0.1f, 1.0f, 0.75f, 0.75f, "%"},
                {"Pattern", 0.0f, 4.0f, 0.0f, 0.0f, ""}
            };
        } else {
            currentParams_ = {
                {"RootKey", 0.0f, 11.0f, 0.0f, 0.0f, "st"},
                {"ScaleMode", 0.0f, 6.0f, 0.0f, 0.0f, ""},
                {"HumanizeVel", 0.0f, 0.50f, 0.15f, 0.20f, "%"},
                {"Transpose", -24.0f, 24.0f, 0.0f, 0.0f, "st"}
            };
        }
    } else {
        currentParams_ = {
            {"Cutoff", 50.0f, 15000.0f, 2500.0f, 2500.0f, "Hz"},
            {"Resonance", 0.1f, 0.95f, 0.50f, 0.50f, ""},
            {"Attack", 0.001f, 1.0f, 0.05f, 0.05f, "s"},
            {"Decay", 0.05f, 3.0f, 0.40f, 0.40f, "s"}
        };
    }

    targetCodeMap_[t.id] = currentScriptCode_;
    targetParamsMap_[t.id] = currentParams_;

    setScriptCode(currentScriptCode_);
    initModularRackFromScript(currentScriptCode_);
    initDefaultGuiPanel();
}

void DesignView::setScriptCode(const std::string& code) {
    currentScriptCode_ = code;
    if (activeTargetIndex_ >= 0 && activeTargetIndex_ < static_cast<int>(allTargets_.size())) {
        targetCodeMap_[allTargets_[activeTargetIndex_].id] = code;
    }
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
        if (activeTargetIndex_ >= 0 && activeTargetIndex_ < static_cast<int>(allTargets_.size())) {
            targetCodeMap_[allTargets_[activeTargetIndex_].id] = textEditor_.getText();
            targetParamsMap_[allTargets_[activeTargetIndex_].id] = currentParams_;
        }
        activeTargetIndex_ = index;
        updateActiveTargetCodeAndParams();
    }
}

void DesignView::selectTargetById(const std::string& id) {
    // 1. Exact match on target id
    for (size_t i = 0; i < allTargets_.size(); ++i) {
        if (allTargets_[i].id == id) {
            selectTargetByIndex(static_cast<int>(i));
            return;
        }
    }
    // 2. Exact match on track engine / instrument or preset name
    for (size_t i = 0; i < allTargets_.size(); ++i) {
        if (allTargets_[i].type == ScriptTargetType::TrackDsp && allTargets_[i].trackIndex >= 0 &&
            allTargets_[i].trackIndex < static_cast<int>(cachedTracks_.size())) {
            const auto& trk = cachedTracks_[allTargets_[i].trackIndex];
            if (trk.instrumentEngine == id || trk.instrument == id ||
                (id == "eats_303" && (trk.instrumentEngine == "tb303" || trk.instrument.find("303") != std::string::npos)) ||
                (id == "eats_kick" && (trk.instrumentEngine == "kick" || trk.instrumentEngine == "tr808")) ||
                (id == "eats_snare" && (trk.instrumentEngine == "snare" || trk.instrumentEngine == "tr909")) ||
                (id == "eats_hats" && (trk.instrumentEngine == "hats" || trk.instrument.find("Hat") != std::string::npos))) {
                selectTargetByIndex(static_cast<int>(i));
                return;
            }
        }
    }
    // 3. Substring match on title or subtitle
    for (size_t i = 0; i < allTargets_.size(); ++i) {
        if (allTargets_[i].title.find(id) != std::string::npos || allTargets_[i].subtitle.find(id) != std::string::npos) {
            selectTargetByIndex(static_cast<int>(i));
            return;
        }
    }
}

void DesignView::selectTargetByTrackAndType(int trackIndex, ScriptTargetType type, int fxIndex) {
    // 1. If fxIndex >= 0, look for exact trackIndex + type + fxIndex match
    if (fxIndex >= 0) {
        for (size_t i = 0; i < allTargets_.size(); ++i) {
            if (allTargets_[i].type == type && allTargets_[i].trackIndex == trackIndex && allTargets_[i].fxIndex == fxIndex) {
                selectTargetByIndex(static_cast<int>(i));
                return;
            }
        }
    }

    // 2. Exact trackIndex + type match
    for (size_t i = 0; i < allTargets_.size(); ++i) {
        if (allTargets_[i].type == type && (trackIndex < 0 || allTargets_[i].trackIndex == trackIndex)) {
            selectTargetByIndex(static_cast<int>(i));
            return;
        }
    }

    // 3. Fallback to any target of matching type
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
    static ScriptTarget fallback{"unknown", "Unknown Target", "", ScriptTargetType::TrackDsp, "SYNTH DSP", {0.0f, 0.95f, 1.0f}, {1.0f, 0.55f, 0.0f}, 0, -1, -1};
    return fallback;
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

    // Project Sidebar Unified Drawer Layout
    projectDrawer_.layout(contentBounds_, 0.0f);
    float drawerW = projectDrawer_.isExpanded() ? (projectDrawer_.getExtent() * projectDrawer_.getAnimProgress()) : 0.0f;
    explorerBounds_ = projectDrawer_.getContentBounds();
    studioBodyBounds_ = Rect2D(contentBounds_.x + drawerW, contentBounds_.y, contentBounds_.w - drawerW, contentBounds_.h);

    // Code Editor layout inside studioBodyBounds_ (oscilloscope removed, extra height to editor)
    float pad = 12.0f;
    float edHeaderY = studioBodyBounds_.y + pad;
    editorHeaderBounds_ = Rect2D(studioBodyBounds_.x + pad, edHeaderY, studioBodyBounds_.w - (pad * 2.0f), 28.0f);

    float linkY = edHeaderY + 6.0f;
    float linkRight = editorHeaderBounds_.x + editorHeaderBounds_.w - 10.0f;
    btnApiDocs_ = Rect2D(linkRight - 65.0f, linkY, 65.0f, 18.0f);
    btnSubmitPr_ = Rect2D(btnApiDocs_.x - 70.0f, linkY, 65.0f, 18.0f);
    btnCopy_ = Rect2D(btnSubmitPr_.x - 48.0f, linkY, 44.0f, 18.0f);
    btnSelectAll_ = Rect2D(btnCopy_.x - 75.0f, linkY, 70.0f, 18.0f);

    float edCanvasY = edHeaderY + 28.0f;
    float edCanvasH = std::max(180.0f, studioBodyBounds_.h - 260.0f);
    editorCanvasBounds_ = Rect2D(editorHeaderBounds_.x, edCanvasY, editorHeaderBounds_.w, edCanvasH);
    textEditor_.layout(editorCanvasBounds_, ctx);

    float statusY = edCanvasY + edCanvasH + 8.0f;
    statusBannerBounds_ = Rect2D(editorHeaderBounds_.x, statusY, editorHeaderBounds_.w, 26.0f);

    float liveParamsY = statusY + 32.0f;
    float liveParamsH = std::max(80.0f, studioBodyBounds_.y + studioBodyBounds_.h - liveParamsY - pad);
    liveParamsSectionBounds_ = Rect2D(editorHeaderBounds_.x, liveParamsY, editorHeaderBounds_.w, liveParamsH);

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
    btnModularZoomIn_ = Rect2D(btnModularAddModule_.x - 30.0f, modularToolbarBounds_.y + 5.0f, 24.0f, 24.0f);
    btnModularZoomOut_ = Rect2D(btnModularZoomIn_.x - 28.0f, modularToolbarBounds_.y + 5.0f, 24.0f, 24.0f);
    btnModularZoomReset_ = Rect2D(btnModularZoomOut_.x - 60.0f, modularToolbarBounds_.y + 5.0f, 54.0f, 24.0f);

    // GUI Designer layout
    designerToolbarBounds_ = Rect2D(studioBodyBounds_.x, studioBodyBounds_.y + guiSubBarBounds_.h, studioBodyBounds_.w, 34.0f);
    btnTogglePalette_ = Rect2D(designerToolbarBounds_.x + 8.0f, designerToolbarBounds_.y + 5.0f, 82.0f, 24.0f);
    btnAddRow_ = Rect2D(btnTogglePalette_.x + 90.0f, designerToolbarBounds_.y + 5.0f, 88.0f, 24.0f);
    btnDeleteRow_ = Rect2D(btnAddRow_.x + 94.0f, designerToolbarBounds_.y + 5.0f, 88.0f, 24.0f);
    btnDuplicateWidget_ = Rect2D(btnDeleteRow_.x + 94.0f, designerToolbarBounds_.y + 5.0f, 88.0f, 24.0f);
    btnDeleteWidget_ = Rect2D(btnDuplicateWidget_.x + 94.0f, designerToolbarBounds_.y + 5.0f, 88.0f, 24.0f);
    btnToggleInspector_ = Rect2D(designerToolbarBounds_.x + designerToolbarBounds_.w - 96.0f, designerToolbarBounds_.y + 5.0f, 88.0f, 24.0f);

    // Modular Rack layout inside studioBodyBounds_
    float rackBodyY = studioBodyBounds_.y + modularToolbarBounds_.h;
    float rackBodyH = std::max(200.0f, studioBodyBounds_.h - modularToolbarBounds_.h);

    // Configure 2D Viewport
    Rect2D rackViewport(studioBodyBounds_.x, rackBodyY, studioBodyBounds_.w, rackBodyH);
    modularZoomPan_.setViewport(rackViewport.x, rackViewport.y, rackViewport.w, rackViewport.h);
    if (!modularPanInitialized_) {
        modularZoomPan_.setPan(rackViewport.x, rackViewport.y);
        modularPanInitialized_ = true;
    }

    // Modal dialog layout
    moduleSearchDialog_.layout(bounds_.w, bounds_.h);

    if (!modularLayoutDone_) {
        float modX = 30.0f; // 2 HP starting offset
        float modY = rackBodyY + kModuleTopRailOffset;
        float modH = 320.0f;

        for (size_t m = 0; m < modules_.size(); ++m) {
            int wHp = std::max(1, static_cast<int>(std::round(modules_[m].w / kHpWidth)));
            modules_[m].w = wHp * kHpWidth;
            modules_[m].x = modX;
            modules_[m].y = modY;
            modules_[m].h = modH;
            modX += modules_[m].w;
        }
        modularLayoutDone_ = true;
    }
}

void DesignView::render(const ViewContext& ctx) {
    if (!ctx.renderer || !ctx.theme) return;

    renderSubNavHeader(ctx);

    projectDrawer_.update(0.016f);
    projectDrawer_.render(*ctx.renderer, *ctx.theme, ctx);

    if (projectDrawer_.getAnimProgress() > 0.001f) {
        ScissorScope scissor(*ctx.renderer, projectDrawer_.getContentBounds().x, projectDrawer_.getContentBounds().y,
                             projectDrawer_.getContentBounds().w, projectDrawer_.getContentBounds().h);
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

    auto drawModeBtn = [&](const Rect2D& b, std::string_view label, bool active, const Color& accent) {
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

    // Sidebar background & separator
    drawRect(r, explorerBounds_.x, explorerBounds_.y, explorerBounds_.w, explorerBounds_.h,
             0.09f, 0.10f, 0.13f, 1.0f);
    drawLine(r, explorerBounds_.x + explorerBounds_.w, explorerBounds_.y,
             explorerBounds_.x + explorerBounds_.w, explorerBounds_.y + explorerBounds_.h,
             0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

    // 1. Sidebar Header with active project indicator
    float ey = explorerBounds_.y + 8.0f;
    drawText(r, "PROJECT SCRIPTS", explorerBounds_.x + 10.0f, ey + 1.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // Active project pulse dot + count badge
    float badgeW = 62.0f;
    float badgeH = 16.0f;
    float badgeX = explorerBounds_.x + explorerBounds_.w - badgeW - 8.0f;
    drawRoundedRect(r, badgeX, ey - 1.0f, badgeW, badgeH, 3.0f,
                    0.05f, 0.20f, 0.12f, 0.85f);
    drawRoundedRectOutline(r, badgeX, ey - 1.0f, badgeW, badgeH, 3.0f,
                           0.0f, 0.95f, 0.45f, 0.70f, 1.0f);
    drawCircle(r, badgeX + 7.0f, ey + 7.0f, 2.5f, 0.0f, 1.0f, 0.55f, 1.0f);
    char badgeLabel[32];
    std::snprintf(badgeLabel, sizeof(badgeLabel), "%zu LOADED", allTargets_.size());
    drawText(r, badgeLabel, badgeX + 13.0f, ey + 2.0f, 8.0f, 0.0f, 1.0f, 0.55f, 1.0f);

    ey += 22.0f;

    // 2. Filter Pills: [ALL] [INST] [FX] [MIDI]
    float pillGap = 3.0f;
    float pillAreaW = explorerBounds_.w - 16.0f;
    float pillW = std::floor((pillAreaW - (pillGap * 3.0f)) / 4.0f);
    float pillH = 20.0f;
    float px = explorerBounds_.x + 8.0f;

    btnFilterAll_ = Rect2D(px, ey, pillW, pillH);
    btnFilterInst_ = Rect2D(px + pillW + pillGap, ey, pillW, pillH);
    btnFilterFx_ = Rect2D(px + (pillW + pillGap) * 2.0f, ey, pillW, pillH);
    btnFilterMidi_ = Rect2D(px + (pillW + pillGap) * 3.0f, ey, pillW, pillH);

    auto drawPill = [&](const Rect2D& b, std::string_view label, bool active, const Color& accent) {
        if (active) {
            drawRoundedRect(r, b.x, b.y, b.w, b.h, 3.0f, accent.r * 0.25f, accent.g * 0.25f, accent.b * 0.25f, 0.95f);
            drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 3.0f, accent.r, accent.g, accent.b, 0.90f, 1.0f);
            drawText(r, label, b.x + (b.w - static_cast<float>(label.length()) * 5.2f) * 0.5f, b.y + 4.5f, 8.5f, accent.r, accent.g, accent.b, 1.0f);
        } else {
            drawRoundedRect(r, b.x, b.y, b.w, b.h, 3.0f, 0.12f, 0.13f, 0.17f, 0.85f);
            drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 3.0f, 0.20f, 0.22f, 0.28f, 0.60f, 1.0f);
            drawText(r, label, b.x + (b.w - static_cast<float>(label.length()) * 5.2f) * 0.5f, b.y + 4.5f, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
        }
    };

    drawPill(btnFilterAll_, "ALL", explorerFilter_ == ExplorerFilterMode::All, theme.primaryAccent);
    drawPill(btnFilterInst_, "INST", explorerFilter_ == ExplorerFilterMode::Instruments, Color{0.0f, 0.95f, 1.0f});
    drawPill(btnFilterFx_, "FX", explorerFilter_ == ExplorerFilterMode::AudioFx, Color{1.0f, 0.16f, 0.55f});
    drawPill(btnFilterMidi_, "MIDI", explorerFilter_ == ExplorerFilterMode::MidiFx, Color{1.0f, 0.85f, 0.0f});

    ey += pillH + 6.0f;

    // 3. Search / Filter box
    explorerSearchBoxBounds_ = Rect2D(explorerBounds_.x + 8.0f, ey, explorerBounds_.w - 16.0f, 22.0f);
    drawRoundedRect(r, explorerSearchBoxBounds_.x, explorerSearchBoxBounds_.y,
                    explorerSearchBoxBounds_.w, explorerSearchBoxBounds_.h, 3.0f,
                    0.05f, 0.06f, 0.08f, 0.95f);
    drawRoundedRectOutline(r, explorerSearchBoxBounds_.x, explorerSearchBoxBounds_.y,
                           explorerSearchBoxBounds_.w, explorerSearchBoxBounds_.h, 3.0f,
                           0.18f, 0.20f, 0.25f, 0.60f, 1.0f);
    std::string_view searchPlaceholder = scriptFilterQuery_.empty() ? std::string_view("Filter loaded scripts...") : std::string_view(scriptFilterQuery_);
    drawMonoText(r, searchPlaceholder,
                 explorerSearchBoxBounds_.x + 8.0f, explorerSearchBoxBounds_.y + 5.0f, 9.0f,
                 scriptFilterQuery_.empty() ? theme.textMuted.r : 0.95f,
                 scriptFilterQuery_.empty() ? theme.textMuted.g : 0.95f,
                 scriptFilterQuery_.empty() ? theme.textMuted.b : 0.95f,
                 scriptFilterQuery_.empty() ? 0.55f : 1.0f);

    ey += 28.0f;

    // 4. Scrollable List of Loaded Project Scripts
    float listTopY = ey;
    float listBottomY = explorerBounds_.y + explorerBounds_.h - 6.0f;
    float listH = std::max(40.0f, listBottomY - listTopY);

    explorerItemBounds_.clear();

    // Group counts
    size_t countDsp = 0, countAfx = 0, countMfx = 0;
    for (const auto& t : allTargets_) {
        if (t.type == ScriptTargetType::TrackDsp) countDsp++;
        else if (t.type == ScriptTargetType::AudioFx) countAfx++;
        else if (t.type == ScriptTargetType::MidiFx) countMfx++;
    }

    struct GroupInfo {
        std::string_view title;
        std::string_view shortTag;
        Color accent;
        ScriptTargetType type;
        size_t count;
        bool visible;
    };

    const GroupInfo groups[] = {
        {"INSTRUMENTS & DSP", "SYNTH", Color{0.0f, 0.95f, 1.0f}, ScriptTargetType::TrackDsp, countDsp,
         (explorerFilter_ == ExplorerFilterMode::All || explorerFilter_ == ExplorerFilterMode::Instruments)},
        {"AUDIO FX INSERTS", "FX", Color{1.0f, 0.16f, 0.55f}, ScriptTargetType::AudioFx, countAfx,
         (explorerFilter_ == ExplorerFilterMode::All || explorerFilter_ == ExplorerFilterMode::AudioFx)},
        {"MIDI FX PROCESSORS", "MIDI", Color{1.0f, 0.85f, 0.0f}, ScriptTargetType::MidiFx, countMfx,
         (explorerFilter_ == ExplorerFilterMode::All || explorerFilter_ == ExplorerFilterMode::MidiFx)}
    };

    float currentContentY = listTopY;

    for (size_t g = 0; g < 3; ++g) {
        const auto& grp = groups[g];
        if (!grp.visible) continue;

        // Group header
        float headerY = currentContentY - explorerScrollY_;
        if (headerY + 16.0f >= listTopY && headerY <= listBottomY) {
            char grpHeader[64];
            std::snprintf(grpHeader, sizeof(grpHeader), "%.*s (%zu)",
                          static_cast<int>(grp.title.size()), grp.title.data(), grp.count);
            drawText(r, grpHeader, explorerBounds_.x + 10.0f, headerY, 9.0f,
                     grp.accent.r, grp.accent.g, grp.accent.b, 1.0f);
        }
        currentContentY += 16.0f;

        int itemsRendered = 0;
        for (size_t i = 0; i < allTargets_.size(); ++i) {
            if (allTargets_[i].type != grp.type) continue;

            // Search filter query
            if (!scriptFilterQuery_.empty()) {
                std::string q = scriptFilterQuery_;
                for (char& c : q) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                std::string matchStr = allTargets_[i].title + " " + allTargets_[i].subtitle + " " + allTargets_[i].id;
                for (char& c : matchStr) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (matchStr.find(q) == std::string::npos) continue;
            }

            itemsRendered++;
            float cardH = 32.0f;
            float itemY = currentContentY - explorerScrollY_;
            Rect2D itemR(explorerBounds_.x + 6.0f, itemY, explorerBounds_.w - 12.0f, cardH);
            explorerItemBounds_.push_back({static_cast<int>(i), itemR});

            if (itemY + cardH >= listTopY && itemY <= listBottomY) {
                bool isSel = (static_cast<int>(i) == activeTargetIndex_);
                if (isSel) {
                    drawRoundedRect(r, itemR.x, itemR.y, itemR.w, itemR.h, 4.0f,
                                    grp.accent.r * 0.20f, grp.accent.g * 0.20f, grp.accent.b * 0.20f, 0.90f);
                    drawRoundedRectOutline(r, itemR.x, itemR.y, itemR.w, itemR.h, 4.0f,
                                           grp.accent.r, grp.accent.g, grp.accent.b, 0.85f, 1.0f);
                    // Left vertical accent bar
                    drawRoundedRect(r, itemR.x + 1.0f, itemR.y + 2.0f, 3.0f, itemR.h - 4.0f, 1.5f,
                                    grp.accent.r, grp.accent.g, grp.accent.b, 1.0f);
                } else {
                    drawRoundedRect(r, itemR.x, itemR.y, itemR.w, itemR.h, 4.0f,
                                    0.11f, 0.12f, 0.16f, 0.70f);
                    drawRoundedRectOutline(r, itemR.x, itemR.y, itemR.w, itemR.h, 4.0f,
                                           0.18f, 0.20f, 0.25f, 0.50f, 1.0f);
                }

                // Track color indicator
                drawCircle(r, itemR.x + 12.0f, itemR.y + 11.0f, 3.5f,
                           allTargets_[i].trackColor.r, allTargets_[i].trackColor.g, allTargets_[i].trackColor.b, 1.0f);

                // Type badge on right
                float badgeW_ = 32.0f;
                float badgeH_ = 13.0f;
                float bx = itemR.x + itemR.w - badgeW_ - 4.0f;
                float by = itemR.y + 4.0f;
                drawRoundedRect(r, bx, by, badgeW_, badgeH_, 2.0f,
                                grp.accent.r * 0.25f, grp.accent.g * 0.25f, grp.accent.b * 0.25f, 0.80f);
                drawText(r, grp.shortTag, bx + (badgeW_ - grp.shortTag.length() * 4.8f) * 0.5f, by + 2.5f, 7.5f,
                         grp.accent.r, grp.accent.g, grp.accent.b, 1.0f);

                // Title
                drawText(r, allTargets_[i].title, itemR.x + 20.0f, itemR.y + 4.0f, 9.5f,
                         isSel ? 1.0f : 0.88f, isSel ? 1.0f : 0.88f, isSel ? 1.0f : 0.88f, 1.0f);

                // Subtitle
                drawText(r, allTargets_[i].subtitle, itemR.x + 20.0f, itemR.y + 17.0f, 8.0f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.75f);
            }

            currentContentY += cardH + 3.0f;
        }

        if (itemsRendered == 0) {
            float emptyY = currentContentY - explorerScrollY_;
            if (emptyY + 16.0f >= listTopY && emptyY <= listBottomY) {
                char emptyMsg[64];
                std::snprintf(emptyMsg, sizeof(emptyMsg), "No %.*s in project",
                              static_cast<int>(grp.title.size()), grp.title.data());
                drawText(r, emptyMsg, explorerBounds_.x + 16.0f, emptyY, 8.5f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.55f);
            }
            currentContentY += 16.0f;
        }

        // Section divider line
        float divY = currentContentY - explorerScrollY_;
        if (divY >= listTopY && divY <= listBottomY) {
            drawLine(r, explorerBounds_.x + 8.0f, divY, explorerBounds_.x + explorerBounds_.w - 8.0f, divY,
                     0.16f, 0.18f, 0.24f, 0.70f, 1.0f);
        }
        currentContentY += 6.0f;
    }

    // Calculate max scroll
    float totalListH = currentContentY - listTopY;
    explorerMaxScrollY_ = std::max(0.0f, totalListH - listH);

    // Scrollbar if needed
    if (explorerMaxScrollY_ > 0.0f) {
        float scrollTrackW = 3.0f;
        float trackX = explorerBounds_.x + explorerBounds_.w - scrollTrackW - 2.0f;
        float trackH = listH;
        float thumbH = std::max(18.0f, trackH * (listH / totalListH));
        float thumbY = listTopY + (explorerScrollY_ / explorerMaxScrollY_) * (trackH - thumbH);

        drawRoundedRect(r, trackX, listTopY, scrollTrackW, trackH, 1.5f,
                        0.07f, 0.08f, 0.10f, 0.50f);
        drawRoundedRect(r, trackX, thumbY, scrollTrackW, thumbH, 1.5f,
                        0.30f, 0.35f, 0.45f, 0.80f);
    }
}

void DesignView::renderCodeEditorCanvas(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;
    const auto& activeT = getActiveTarget();

    drawRect(r, editorHeaderBounds_.x, editorHeaderBounds_.y, editorHeaderBounds_.w, editorHeaderBounds_.h,
             0.13f, 0.15f, 0.19f, 1.0f);
    drawLine(r, editorHeaderBounds_.x, editorHeaderBounds_.y + editorHeaderBounds_.h,
             editorHeaderBounds_.x + editorHeaderBounds_.w, editorHeaderBounds_.y + editorHeaderBounds_.h,
             0.22f, 0.25f, 0.32f, 1.0f, 1.0f);

    char targetBuf[128];
    std::snprintf(targetBuf, sizeof(targetBuf), "TARGET: %s", activeT.title.c_str());
    drawText(r, targetBuf, editorHeaderBounds_.x + 10.0f, editorHeaderBounds_.y + 8.0f, 10.5f,
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
    char statusBuf[128];
    std::snprintf(statusBuf, sizeof(statusBuf), "[OK] %s", statusMsg_.c_str());
    drawText(r, statusBuf, statusBannerBounds_.x + 10.0f, statusBannerBounds_.y + 6.0f, 9.5f,
             0.0f, 1.0f, 0.55f, 1.0f);

    float titleY = liveParamsSectionBounds_.y;
    char paramHdrBuf[128];
    std::snprintf(paramHdrBuf, sizeof(paramHdrBuf), "LIVE SCRIPT PARAMETERS (%s)", activeT.title.c_str());
    drawText(r, paramHdrBuf, liveParamsSectionBounds_.x, titleY, 10.5f,
             activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);

    for (size_t i = 0; i < currentParams_.size(); ++i) {
        const auto& p = currentParams_[i];
        const auto& pb = p.bounds;

        drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 4.0f, 0.12f, 0.14f, 0.18f, 0.95f);
        drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 4.0f,
                               activeT.badgeColor.r * 0.4f, activeT.badgeColor.g * 0.4f, activeT.badgeColor.b * 0.4f, 0.8f, 1.0f);

        drawText(r, p.name, pb.x + 8.0f, pb.y + 6.0f, 10.0f, 0.95f, 0.95f, 0.95f, 1.0f);

        char valBuf[32];
        std::snprintf(valBuf, sizeof(valBuf), "%.1f", p.currentVal);
        drawText(r, valBuf, pb.x + pb.w - 36.0f, pb.y + 6.0f, 10.0f,
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

    char rackTitle[96];
    std::string_view activeTTitle = getActiveTarget().title;
    std::snprintf(rackTitle, sizeof(rackTitle), "MODULAR RACK: %.*s",
                  static_cast<int>(activeTTitle.size()), activeTTitle.data());
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
    btnModularZoomIn_ = Rect2D(btnModularAddModule_.x - 30.0f, modularToolbarBounds_.y + 6.0f, 24.0f, 22.0f);
    btnModularZoomOut_ = Rect2D(btnModularZoomIn_.x - 28.0f, modularToolbarBounds_.y + 6.0f, 24.0f, 22.0f);
    btnModularZoomReset_ = Rect2D(btnModularZoomOut_.x - 60.0f, modularToolbarBounds_.y + 6.0f, 54.0f, 22.0f);

    // Draw Zoom buttons
    char zoomPct[16];
    std::snprintf(zoomPct, sizeof(zoomPct), "%d%%", static_cast<int>(std::round(modularZoomPan_.getZoomX() * 100.0f)));
    drawButton(r, btnModularZoomReset_, zoomPct,
               Color{0.14f, 0.17f, 0.23f, 0.9f}, Color{0.25f, 0.28f, 0.35f, 0.8f}, Color{0.8f, 0.85f, 0.95f, 1.0f}, 9.0f, 3.0f, 1.0f);
    drawButton(r, btnModularZoomOut_, "-",
               Color{0.14f, 0.17f, 0.23f, 0.9f}, Color{0.25f, 0.28f, 0.35f, 0.8f}, Color{0.8f, 0.85f, 0.95f, 1.0f}, 11.0f, 3.0f, 1.0f);
    drawButton(r, btnModularZoomIn_, "+",
               Color{0.14f, 0.17f, 0.23f, 0.9f}, Color{0.25f, 0.28f, 0.35f, 0.8f}, Color{0.8f, 0.85f, 0.95f, 1.0f}, 11.0f, 3.0f, 1.0f);

    drawButton(r, btnModularAddModule_, "+ ADD MODULE",
               Color{0.14f, 0.17f, 0.23f, 0.9f}, Color{1.0f, 0.85f, 0.0f, 0.6f}, Color{1.0f, 0.85f, 0.0f, 1.0f}, 9.5f, 3.0f, 1.0f);

    drawButton(r, btnModularResetPatch_, "RESET PATCH",
               Color{0.14f, 0.17f, 0.23f, 0.9f}, Color{0.25f, 0.28f, 0.35f, 0.8f}, Color{0.8f, 0.85f, 0.95f, 1.0f}, 9.5f, 3.0f, 1.0f);

    // Canvas Viewport & Scissoring
    float rackBodyY = rect.y + topBarH;
    float rackBodyH = rect.h - topBarH;
    modularZoomPan_.setViewport(rect.x, rackBodyY, rect.w, rackBodyH);

    ScissorScope scissorScope(r, rect.x, rackBodyY, rect.w, rackBodyH);

    // Canvas background
    drawRect(r, rect.x, rackBodyY, rect.w, rackBodyH, 0.07f, 0.08f, 0.10f, 1.0f);

    // Multi-Row Eurorack Rails (Standard 3U = 380px)
    constexpr float kRowHeight = 380.0f;
    constexpr float kRailH = 16.0f;
    float row0Y = rackBodyY;
    float minWorldY = modularZoomPan_.screenToWorldY(rackBodyY);
    float maxWorldY = modularZoomPan_.screenToWorldY(rackBodyY + rackBodyH);
    float minWorldX = modularZoomPan_.screenToWorldX(rect.x);
    float maxWorldX = modularZoomPan_.screenToWorldX(rect.x + rect.w);

    int startRow = static_cast<int>(std::floor((minWorldY - row0Y) / kRowHeight)) - 1;
    int endRow = static_cast<int>(std::ceil((maxWorldY - row0Y) / kRowHeight)) + 1;
    if (startRow > 0) startRow = 0;
    if (endRow < 2) endRow = 2;

    for (int row = startRow; row <= endRow; ++row) {
        float rowWorldY = row0Y + row * kRowHeight;
        float rowScreenY = modularZoomPan_.worldToScreenY(rowWorldY);
        float rowScreenH = modularZoomPan_.worldDistanceToScreenY(kRowHeight);

        // Dark backplane
        drawRect(r, rect.x, rowScreenY, rect.w, rowScreenH, 0.06f, 0.07f, 0.09f, 1.0f);

        // Power distribution bus bar channel (VCV Rack / Cardinal aesthetic)
        float busWorldY = rowWorldY + (kRowHeight * 0.5f) - 14.0f;
        float busScreenY = modularZoomPan_.worldToScreenY(busWorldY);
        float busScreenH = modularZoomPan_.worldDistanceToScreenY(28.0f);
        drawRect(r, rect.x, busScreenY, rect.w, busScreenH, 0.10f, 0.11f, 0.14f, 1.0f);
        // Red stripe on ribbon bus cable
        drawRect(r, rect.x, busScreenY + 2.0f, rect.w, 3.0f, 0.75f, 0.18f, 0.18f, 0.55f);

        // Eurorack 16-pin gold power connector headers spaced every 240px
        float firstHeaderX = std::floor(minWorldX / 240.0f) * 240.0f + 60.0f;
        for (float hx = firstHeaderX; hx < maxWorldX + 240.0f; hx += 240.0f) {
            float shx = modularZoomPan_.worldToScreenX(hx);
            float shy = busScreenY + 4.0f;
            drawRect(r, shx, shy, 54.0f * modularZoomPan_.getZoomX(), busScreenH - 8.0f, 0.15f, 0.16f, 0.20f, 1.0f);
            drawRoundedRectOutline(r, shx, shy, 54.0f * modularZoomPan_.getZoomX(), busScreenH - 8.0f, 2.0f, 0.24f, 0.26f, 0.32f, 0.8f, 1.0f);
            // Gold pins
            for (int p = 0; p < 8; ++p) {
                float pinX = shx + (6.0f + p * 5.5f) * modularZoomPan_.getZoomX();
                drawCircle(r, pinX, shy + 5.0f, 1.4f * modularZoomPan_.getZoomX(), 0.85f, 0.70f, 0.20f, 0.95f);
                drawCircle(r, pinX, shy + busScreenH - 13.0f, 1.4f * modularZoomPan_.getZoomX(), 0.85f, 0.70f, 0.20f, 0.95f);
            }
        }

        // Top Mounting Rail
        float topRailScreenY = modularZoomPan_.worldToScreenY(rowWorldY);
        float railScreenH = modularZoomPan_.worldDistanceToScreenY(kRailH);
        drawRect(r, rect.x, topRailScreenY, rect.w, railScreenH, 0.22f, 0.24f, 0.28f, 1.0f);
        drawLine(r, rect.x, topRailScreenY + 1.0f, rect.x + rect.w, topRailScreenY + 1.0f, 0.38f, 0.40f, 0.46f, 0.9f, 1.0f);
        drawLine(r, rect.x, topRailScreenY + railScreenH, rect.x + rect.w, topRailScreenY + railScreenH, 0.12f, 0.13f, 0.15f, 1.0f, 1.0f);

        // Bottom Mounting Rail
        float btmRailWorldY = rowWorldY + kRowHeight - kRailH;
        float btmRailScreenY = modularZoomPan_.worldToScreenY(btmRailWorldY);
        drawRect(r, rect.x, btmRailScreenY, rect.w, railScreenH, 0.22f, 0.24f, 0.28f, 1.0f);
        drawLine(r, rect.x, btmRailScreenY + 1.0f, rect.x + rect.w, btmRailScreenY + 1.0f, 0.38f, 0.40f, 0.46f, 0.9f, 1.0f);
        drawLine(r, rect.x, btmRailScreenY + railScreenH, rect.x + rect.w, btmRailScreenY + railScreenH, 0.12f, 0.13f, 0.15f, 1.0f, 1.0f);

        // Threaded screw / rivet holes (1 HP = 15px increments)
        float firstScrewX = std::floor(minWorldX / kHpWidth) * kHpWidth;
        for (float sx = firstScrewX; sx < maxWorldX + kHpWidth; sx += kHpWidth) {
            float ssx = modularZoomPan_.worldToScreenX(sx);
            float topHoleY = topRailScreenY + railScreenH * 0.5f;
            float btmHoleY = btmRailScreenY + railScreenH * 0.5f;
            drawCircle(r, ssx, topHoleY, 2.2f * modularZoomPan_.getZoomX(), 0.14f, 0.15f, 0.18f, 1.0f);
            drawCircle(r, ssx, topHoleY, 1.0f * modularZoomPan_.getZoomX(), 0.04f, 0.05f, 0.06f, 1.0f);
            drawCircle(r, ssx, btmHoleY, 2.2f * modularZoomPan_.getZoomX(), 0.14f, 0.15f, 0.18f, 1.0f);
            drawCircle(r, ssx, btmHoleY, 1.0f * modularZoomPan_.getZoomX(), 0.04f, 0.05f, 0.06f, 1.0f);
        }
    }

    // Render Modules
    float zoom = modularZoomPan_.getZoomX();

    for (size_t m = 0; m < modules_.size(); ++m) {
        auto& mod = modules_[m];
        float sx = modularZoomPan_.worldToScreenX(mod.x);
        float sy = modularZoomPan_.worldToScreenY(mod.y);
        float sw = modularZoomPan_.worldDistanceToScreenX(mod.w);
        float sh = modularZoomPan_.worldDistanceToScreenY(mod.h);

        // Culling if entirely outside visible canvas
        if (sx + sw < rect.x || sx > rect.x + rect.w || sy + sh < rackBodyY || sy > rackBodyY + rackBodyH) {
            continue;
        }

        bool isDragging = (draggingModuleIdx_ == static_cast<int>(m));

        // Faceplate chassis
        drawRoundedRect(r, sx, sy, sw, sh, 4.0f * zoom, mod.r, mod.g, mod.b, 1.0f);
        if (isDragging) {
            drawRoundedRectOutline(r, sx - 2.0f, sy - 2.0f, sw + 4.0f, sh + 4.0f, 5.0f * zoom,
                                   1.0f, 0.85f, 0.0f, 0.85f, 2.0f);
        } else {
            drawRoundedRectOutline(r, sx, sy, sw, sh, 4.0f * zoom, 0.42f, 0.45f, 0.52f, 1.0f, 1.5f * zoom);
        }

        // Corner mounting thumbscrews (knurled silver with screw driver notch, aligned to HP rivet pitch)
        auto drawThumbscrew = [&](float worldX, float worldY) {
            float scX = modularZoomPan_.worldToScreenX(worldX);
            float scY = modularZoomPan_.worldToScreenY(worldY);
            drawCircle(r, scX, scY, 3.5f * zoom, 0.65f, 0.68f, 0.75f, 1.0f);
            drawCircle(r, scX, scY, 2.2f * zoom, 0.48f, 0.50f, 0.58f, 1.0f);
            drawLine(r, scX - 2.0f * zoom, scY, scX + 2.0f * zoom, scY, 0.15f, 0.16f, 0.19f, 0.9f, 1.0f * zoom);
        };
        drawThumbscrew(mod.x + 7.5f, mod.y + 8.0f);
        drawThumbscrew(mod.x + mod.w - 7.5f, mod.y + 8.0f);
        drawThumbscrew(mod.x + 7.5f, mod.y + mod.h - 8.0f);
        drawThumbscrew(mod.x + mod.w - 7.5f, mod.y + mod.h - 8.0f);

        // Title banner
        float banH = 24.0f * zoom;
        drawRect(r, sx, sy, sw, banH, 0.14f, 0.15f, 0.18f, 1.0f);
        drawText(r, mod.title, sx + 12.0f * zoom, sy + 6.0f * zoom, 10.0f * zoom, 0.0f, 0.95f, 1.0f, 1.0f);

        // Drag handle grip dots on top right of title banner
        float gripX = sx + sw - 14.0f * zoom;
        drawCircle(r, gripX, sy + 7.0f * zoom, 1.5f * zoom, 0.55f, 0.60f, 0.70f, 0.75f);
        drawCircle(r, gripX, sy + 12.0f * zoom, 1.5f * zoom, 0.55f, 0.60f, 0.70f, 0.75f);
        drawCircle(r, gripX, sy + 17.0f * zoom, 1.5f * zoom, 0.55f, 0.60f, 0.70f, 0.75f);

        // Knobs
        for (size_t k = 0; k < mod.knobs.size(); ++k) {
            float kx = mod.x + 40.0f + ((k % 2) * 85.0f);
            float ky = mod.y + 65.0f + ((k / 2) * 70.0f);
            float skx = modularZoomPan_.worldToScreenX(kx);
            float sky = modularZoomPan_.worldToScreenY(ky);

            bool isKnobDragging = (draggingModuleKnobMod_ == static_cast<int>(m) &&
                                   draggingModuleKnobIdx_ == static_cast<int>(k));

            if (isKnobDragging) {
                drawCircle(r, skx, sky, 20.0f * zoom, 1.0f, 0.85f, 0.0f, 0.35f);
            }

            drawCircle(r, skx, sky, 16.0f * zoom, 0.08f, 0.09f, 0.11f, 1.0f);
            drawCircleOutline(r, skx, sky, 16.0f * zoom, 0.20f, 0.22f, 0.26f, 0.7f, 1.0f * zoom);

            float offX = -0.25f * 13.0f * zoom;
            float offY = -0.30f * 13.0f * zoom;
            drawCircleRadial3StopGradient(r, skx, sky, 13.0f * zoom,
                                          Color(0.42f, 0.45f, 0.52f, 1.0f),
                                          Color(0.24f, 0.26f, 0.31f, 1.0f),
                                          Color(0.12f, 0.13f, 0.16f, 1.0f),
                                          offX, offY, 0.50f, 32);
            drawCircleOutline(r, skx, sky, 13.0f * zoom, 0.08f, 0.09f, 0.12f, 0.6f, 1.0f * zoom);

            float ang = -2.356f + (mod.knobs[k].value * 4.712f);
            float nx = skx + std::sin(ang) * 11.0f * zoom;
            float ny = sky - std::cos(ang) * 11.0f * zoom;
            drawLine(r, skx, sky, nx, ny, isKnobDragging ? 0.0f : 1.0f, isKnobDragging ? 1.0f : 0.85f, 0.0f, 1.0f, 2.5f * zoom);

            drawText(r, mod.knobs[k].label, skx - 16.0f * zoom, sky + 18.0f * zoom, 8.5f * zoom, 0.8f, 0.85f, 0.9f, 0.9f);

            char valBuf[16];
            std::snprintf(valBuf, sizeof(valBuf), "%.2f", mod.knobs[k].value);
            drawText(r, valBuf, skx - 12.0f * zoom, sky + 29.0f * zoom, 7.5f * zoom, 0.6f, 0.7f, 0.8f, 0.8f);
        }

        // Jacks: Inputs (Cyan) on Left, Outputs (Gold) on Right
        float jackWorldY = mod.y + mod.h - 45.0f;
        float sjy = modularZoomPan_.worldToScreenY(jackWorldY);

        for (size_t i = 0; i < mod.inputs.size(); ++i) {
            float jx = mod.x + 30.0f + (i * 45.0f);
            float sjx = modularZoomPan_.worldToScreenX(jx);

            bool isConnected = false;
            Color wireColor{0.13f, 0.96f, 0.91f};
            for (const auto& c : patchCords_) {
                if (c.destModule == static_cast<int>(m) && c.destJack == static_cast<int>(i)) {
                    isConnected = true;
                    wireColor = Color{c.r, c.g, c.b};
                    break;
                }
            }

            drawCircle(r, sjx, sjy, 12.0f * zoom, 0.12f, 0.13f, 0.16f, 1.0f);
            drawCircle(r, sjx, sjy, 8.0f * zoom, 0.0f, 0.95f, 1.0f, 1.0f);
            drawCircle(r, sjx, sjy, 4.0f * zoom, 0.02f, 0.02f, 0.03f, 1.0f);

            if (isConnected) {
                drawCircle(r, sjx, sjy, 14.0f * zoom, wireColor.r, wireColor.g, wireColor.b, 0.45f);
            }

            drawText(r, mod.inputs[i], sjx - 12.0f * zoom, sjy - 22.0f * zoom, 7.5f * zoom, 0.0f, 0.95f, 1.0f, 0.9f);
        }

        for (size_t o = 0; o < mod.outputs.size(); ++o) {
            float jx = mod.x + mod.w - 30.0f - (o * 45.0f);
            float sjx = modularZoomPan_.worldToScreenX(jx);

            bool isConnected = false;
            Color wireColor{1.0f, 0.55f, 0.0f};
            for (const auto& c : patchCords_) {
                if (c.sourceModule == static_cast<int>(m) && c.sourceJack == static_cast<int>(o)) {
                    isConnected = true;
                    wireColor = Color{c.r, c.g, c.b};
                    break;
                }
            }

            drawCircle(r, sjx, sjy, 12.0f * zoom, 0.12f, 0.13f, 0.16f, 1.0f);
            drawCircle(r, sjx, sjy, 8.0f * zoom, 1.0f, 0.55f, 0.0f, 1.0f);
            drawCircle(r, sjx, sjy, 4.0f * zoom, 0.02f, 0.02f, 0.03f, 1.0f);

            if (isConnected) {
                drawCircle(r, sjx, sjy, 14.0f * zoom, wireColor.r, wireColor.g, wireColor.b, 0.45f);
            }

            drawText(r, mod.outputs[o], sjx - 10.0f * zoom, sjy - 22.0f * zoom, 7.5f * zoom, 1.0f, 0.55f, 0.0f, 0.9f);
        }
    }

    // Draw Catenary Patch Cords
    for (const auto& cord : patchCords_) {
        if (cord.sourceModule >= static_cast<int>(modules_.size()) ||
            cord.destModule >= static_cast<int>(modules_.size())) continue;

        const auto& sm = modules_[cord.sourceModule];
        const auto& dm = modules_[cord.destModule];

        float x1w = sm.x + sm.w - 30.0f - (cord.sourceJack * 45.0f);
        float y1w = sm.y + sm.h - 45.0f;
        float x2w = dm.x + 30.0f + (cord.destJack * 45.0f);
        float y2w = dm.y + dm.h - 45.0f;

        float x1 = modularZoomPan_.worldToScreenX(x1w);
        float y1 = modularZoomPan_.worldToScreenY(y1w);
        float x2 = modularZoomPan_.worldToScreenX(x2w);
        float y2 = modularZoomPan_.worldToScreenY(y2w);

        renderCatenaryCable(r, x1, y1, x2, y2, cord.r, cord.g, cord.b);
    }

    // Active live dragged cord
    if (isDraggingCord_) {
        float x1 = modularZoomPan_.worldToScreenX(cordStartX_);
        float y1 = modularZoomPan_.worldToScreenY(cordStartY_);
        renderCatenaryCable(r, x1, y1, cordCurrentX_, cordCurrentY_, dragCordR_, dragCordG_, dragCordB_);
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

        drawText(r, "HARDWARE CONSOLE (10)", palR.x + 10.0f, py, 8.5f, 1.0f, 0.85f, 0.0f, 1.0f);
        py += 15.0f;
        addPaletteItem("hw_cream", "Cream Fluted Dial", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::CreamFluted);
        addPaletteItem("hw_davies", "Davies 1900H Fluted", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::DaviesFluted);
        addPaletteItem("hw_rogan", "Rogan Moog Skirted", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::RoganMoog);
        addPaletteItem("hw_chicken", "Chicken-Head Pointer", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::ChickenHead);
        addPaletteItem("hw_pastel", "Pastel Dimple Touch", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::PastelDimple);
        addPaletteItem("hw_encoder", "Modern LED Encoder", "CONSOLE", GuiWidgetType::Knob, GuiKnobStyle::ModernEncoder);
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
    inspectorChassisTintBtns_.clear();
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

            drawText(r, "BACKGROUND MATERIAL", inspR.x + 12.0f, iy, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
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
            addThemeChip("Brushed Aluminum", GuiChassisStyle::BrushedAluminum);
            addThemeChip("Brushed Steel", GuiChassisStyle::BrushedSteel);
            addThemeChip("Matte Powder-Coat", GuiChassisStyle::MattePowderCoat);
            addThemeChip("Crinkle Wrinkle", GuiChassisStyle::CrinklePaint);
            addThemeChip("Bakelite Swirl", GuiChassisStyle::Bakelite);
            addThemeChip("Walnut Wood", GuiChassisStyle::Walnut);
            addThemeChip("Rosewood", GuiChassisStyle::Rosewood);
            addThemeChip("PCB Green", GuiChassisStyle::PcbGreen);
            addThemeChip("Silver Console", GuiChassisStyle::Silver);
            addThemeChip("SNES Vintage", GuiChassisStyle::Snes);
            addThemeChip("Aged Grunge", GuiChassisStyle::Grunge);
            addThemeChip("Carbon Fibre", GuiChassisStyle::Carbon);

            iy += 6.0f;
            drawText(r, "CHASSIS TINT / FINISH", inspR.x + 12.0f, iy, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
            iy += 14.0f;

            struct TintPreset {
                std::optional<Color> color;
                std::string label;
            };
            TintPreset tints[6] = {
                {std::nullopt, "NAT"},
                {Color(0.12f, 0.45f, 0.95f), "BLU"},
                {Color(0.85f, 0.72f, 0.45f), "GLD"},
                {Color(0.08f, 0.42f, 0.18f), "GRN"},
                {Color(0.55f, 0.08f, 0.12f), "RED"},
                {Color(0.18f, 0.20f, 0.24f), "SLT"}
            };
            for (int t = 0; t < 6; ++t) {
                Rect2D tintR(inspR.x + 8.0f + (t * 31.0f), iy, 27.0f, 20.0f);
                inspectorChassisTintBtns_.push_back({tints[t].color, tintR});
                bool isSel = (tints[t].color == guiPanel_.chassisTint);
                Color btnBg = tints[t].color.has_value() ? *tints[t].color : Color(0.25f, 0.26f, 0.30f);
                drawRoundedRect(r, tintR.x, tintR.y, tintR.w, tintR.h, 3.0f, btnBg.r, btnBg.g, btnBg.b, 1.0f);
                if (isSel) {
                    drawRoundedRectOutline(r, tintR.x, tintR.y, tintR.w, tintR.h, 3.0f, 1.0f, 1.0f, 1.0f, 0.95f, 1.5f);
                }
                drawText(r, tints[t].label, tintR.x + 4.0f, tintR.y + 6.0f, 7.5f, 1.0f, 1.0f, 1.0f, 0.9f);
            }
            iy += 30.0f;

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
                    addKStyle("Davies 1900H Fluted", GuiKnobStyle::DaviesFluted);
                    addKStyle("Rogan Moog Skirted", GuiKnobStyle::RoganMoog);
                    addKStyle("Chicken-Head Pointer", GuiKnobStyle::ChickenHead);
                    addKStyle("Pastel Dimple Touch", GuiKnobStyle::PastelDimple);
                    addKStyle("Modern LED Encoder", GuiKnobStyle::ModernEncoder);
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
    if (projectDrawer_.handlePointer(ev, ctx)) {
        isExplorerOpen_ = projectDrawer_.isExpanded();
        return true;
    }

    if (ev.action == PointerAction::Down) {
        // 1. Check Sub-Nav Header clicks
        if (explorerToggleBtn_.contains(ev.x, ev.y)) {
            projectDrawer_.toggleExpanded();
            isExplorerOpen_ = projectDrawer_.isExpanded();
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

        // Explorer Sidebar interactions
        if (isExplorerOpen_) {
            if (btnFilterAll_.contains(ev.x, ev.y)) {
                explorerFilter_ = ExplorerFilterMode::All;
                explorerScrollY_ = 0.0f;
                return true;
            }
            if (btnFilterInst_.contains(ev.x, ev.y)) {
                explorerFilter_ = ExplorerFilterMode::Instruments;
                explorerScrollY_ = 0.0f;
                return true;
            }
            if (btnFilterFx_.contains(ev.x, ev.y)) {
                explorerFilter_ = ExplorerFilterMode::AudioFx;
                explorerScrollY_ = 0.0f;
                return true;
            }
            if (btnFilterMidi_.contains(ev.x, ev.y)) {
                explorerFilter_ = ExplorerFilterMode::MidiFx;
                explorerScrollY_ = 0.0f;
                return true;
            }

            for (const auto& item : explorerItemBounds_) {
                if (item.second.contains(ev.x, ev.y)) {
                    selectTargetByIndex(item.first);
                    layout(bounds_, ctx);
                    return true;
                }
            }
        }

        // 0. Forward to embedded Module Search Dialog if open
        if (moduleSearchDialog_.isOpen()) {
            if (moduleSearchDialog_.handlePointer(ev)) {
                return true;
            }
        }

        // 2. Modular Rack interaction
        if (mode_ == DesignSubMode::ModularRack || (mode_ == DesignSubMode::Split && !splitShowsGui_)) {
            // Mobile multi-touch tracking
            if (ev.isTouch()) {
                auto it = std::find_if(activeTouches_.begin(), activeTouches_.end(),
                                       [&](const ActiveTouch& t) { return t.id == ev.id; });
                if (it == activeTouches_.end()) {
                    activeTouches_.push_back({ev.id, ev.x, ev.y});
                } else {
                    it->x = ev.x;
                    it->y = ev.y;
                }
                if (activeTouches_.size() >= 2) {
                    isPinchZooming_ = true;
                    isTouchPanning_ = false;
                    isPanningCanvas_ = false;
                    initialPinchDistance_ = std::hypot(activeTouches_[0].x - activeTouches_[1].x,
                                                       activeTouches_[0].y - activeTouches_[1].y);
                    lastPinchDistance_ = initialPinchDistance_;
                    initialPinchZoom_ = modularZoomPan_.getZoomX();
                    return true;
                }
            }

            // Modular toolbar buttons
            if (btnModularResetPatch_.contains(ev.x, ev.y)) {
                resetPatch();
                if (ctx.onShowNotification) {
                    ctx.onShowNotification("Reset patch to default for " + getActiveTarget().title);
                }
                return true;
            }
            if (btnModularAddModule_.contains(ev.x, ev.y)) {
                openModuleSearchDialog(0.0f, 0.0f);
                return true;
            }
            if (btnModularZoomIn_.contains(ev.x, ev.y)) {
                modularZoomPan_.zoomAt(modularZoomPan_.getViewportOriginX() + modularZoomPan_.getViewportWidth() * 0.5f,
                                       modularZoomPan_.getViewportOriginY() + modularZoomPan_.getViewportHeight() * 0.5f, 1.15f);
                return true;
            }
            if (btnModularZoomOut_.contains(ev.x, ev.y)) {
                modularZoomPan_.zoomAt(modularZoomPan_.getViewportOriginX() + modularZoomPan_.getViewportWidth() * 0.5f,
                                       modularZoomPan_.getViewportOriginY() + modularZoomPan_.getViewportHeight() * 0.5f, 0.85f);
                return true;
            }
            if (btnModularZoomReset_.contains(ev.x, ev.y)) {
                modularZoomPan_.setZoom(1.0f, 1.0f);
                modularZoomPan_.setPan(modularZoomPan_.getViewportOriginX(), modularZoomPan_.getViewportOriginY());
                return true;
            }

            // Convert pointer event to Modular World Space for canvas hit testing
            float wx = modularZoomPan_.screenToWorldX(ev.x);
            float wy = modularZoomPan_.screenToWorldY(ev.y);

            // Check Module Knobs
            for (size_t m = 0; m < modules_.size(); ++m) {
                auto& mod = modules_[m];
                for (size_t k = 0; k < mod.knobs.size(); ++k) {
                    float kx = mod.x + 40.0f + ((k % 2) * 85.0f);
                    float ky = mod.y + 65.0f + ((k / 2) * 70.0f);
                    float dx = wx - kx;
                    float dy = wy - ky;
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
                    float dx = wx - jx;
                    float dy = wy - jackY;
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
                    float dx = wx - jx;
                    float dy = wy - jackY;
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

            // Check Module Chassis & Title Banner for Freeform Module Dragging
            for (int m = static_cast<int>(modules_.size()) - 1; m >= 0; --m) {
                const auto& mod = modules_[m];
                if (wx >= mod.x && wx <= mod.x + mod.w && wy >= mod.y && wy <= mod.y + mod.h) {
                    if (ev.button == PointerButton::Left) {
                        draggingModuleIdx_ = m;
                        dragModuleOffsetWorldX_ = wx - mod.x;
                        dragModuleOffsetWorldY_ = wy - mod.y;
                        dragPointerStartWorldX_ = wx;
                        dragPointerStartWorldY_ = wy;

                        // Snapshot initial states of all modules on rack for bumping & restoration
                        float row0Y = modularZoomPan_.getViewportOriginY();
                        initialModuleStates_.clear();
                        initialModuleStates_.reserve(modules_.size());
                        for (size_t i = 0; i < modules_.size(); ++i) {
                            int modHp = static_cast<int>(std::round(modules_[i].x / kHpWidth));
                            int modRow = static_cast<int>(std::round((modules_[i].y - (row0Y + kModuleTopRailOffset)) / kRowHeight));
                            int modW = std::max(1, static_cast<int>(std::round(modules_[i].w / kHpWidth)));
                            initialModuleStates_.push_back({
                                static_cast<int>(i),
                                modules_[i].x,
                                modules_[i].y,
                                modHp,
                                modRow,
                                modW
                            });
                        }
                        dragStartHp_ = initialModuleStates_[m].hp;
                        dragStartRow_ = initialModuleStates_[m].row;
                        return true;
                    }
                }
            }

            // Canvas Panning (Touch drag, Left click on empty canvas, Middle drag, or Right drag/click)
            if (ev.isTouch() || ev.button == PointerButton::Left || ev.button == PointerButton::Middle || ev.button == PointerButton::Right) {
                isPanningCanvas_ = true;
                lastPanPointerX_ = ev.x;
                lastPanPointerY_ = ev.y;
                touchPanStartX_ = ev.x;
                touchPanStartY_ = ev.y;
                touchDownTimeMs_ = ev.timestampMs > 0.0 ? ev.timestampMs : 1.0;
                touchDownWorldX_ = wx;
                touchDownWorldY_ = wy;
                touchMoved_ = false;
                if (ev.button == PointerButton::Right) {
                    isRightClickCandidate_ = true;
                    rightClickStartX_ = ev.x;
                    rightClickStartY_ = ev.y;
                }
                return true;
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
                        for (const auto& cBtn : inspectorChassisTintBtns_) {
                            if (cBtn.second.contains(ev.x, ev.y)) {
                                guiPanel_.chassisTint = cBtn.first;
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
        if (ev.isTouch()) {
            for (auto& t : activeTouches_) {
                if (t.id == ev.id) {
                    t.x = ev.x;
                    t.y = ev.y;
                    break;
                }
            }
            if (isPinchZooming_ && activeTouches_.size() >= 2) {
                float curDist = std::hypot(activeTouches_[0].x - activeTouches_[1].x,
                                           activeTouches_[0].y - activeTouches_[1].y);
                if (lastPinchDistance_ > 1.0f && curDist > 1.0f) {
                    float factor = curDist / lastPinchDistance_;
                    float midX = (activeTouches_[0].x + activeTouches_[1].x) * 0.5f;
                    float midY = (activeTouches_[0].y + activeTouches_[1].y) * 0.5f;
                    modularZoomPan_.zoomAt(midX, midY, factor);
                    lastPinchDistance_ = curDist;
                }
                return true;
            }
        }

        if (moduleSearchDialog_.isOpen()) {
            if (moduleSearchDialog_.handlePointer(ev)) return true;
        }

        if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
            textEditor_.handlePointer(ev, ctx);
        }

        // Modular rack canvas panning
        if (isPanningCanvas_) {
            float dx = ev.x - lastPanPointerX_;
            float dy = ev.y - lastPanPointerY_;
            modularZoomPan_.panByScreen(dx, dy);
            lastPanPointerX_ = ev.x;
            lastPanPointerY_ = ev.y;
            if (std::hypot(ev.x - touchPanStartX_, ev.y - touchPanStartY_) > 8.0f) {
                touchMoved_ = true;
                isRightClickCandidate_ = false;
            }
            return true;
        }

        // Modular rack freeform module dragging (VCV Rack / Cardinal HP Rivet Increments & Bumping)
        if (draggingModuleIdx_ >= 0 && draggingModuleIdx_ < static_cast<int>(modules_.size())) {
            float wx = modularZoomPan_.screenToWorldX(ev.x);
            float wy = modularZoomPan_.screenToWorldY(ev.y);

            float deltaX = wx - dragPointerStartWorldX_;
            float deltaY = wy - dragPointerStartWorldY_;

            int deltaHp = static_cast<int>(std::round(deltaX / kHpWidth));
            int deltaRow = static_cast<int>(std::round(deltaY / kRowHeight));

            int targetHp = dragStartHp_ + deltaHp;
            int targetRow = dragStartRow_ + deltaRow;

            applyModuleBumping(draggingModuleIdx_, targetRow, targetHp);
            return true;
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
        if (ev.isTouch()) {
            activeTouches_.erase(std::remove_if(activeTouches_.begin(), activeTouches_.end(),
                                                [&](const ActiveTouch& t) { return t.id == ev.id; }),
                                 activeTouches_.end());
            if (activeTouches_.size() < 2) {
                isPinchZooming_ = false;
            }
        }

        if (moduleSearchDialog_.isOpen()) {
            if (moduleSearchDialog_.handlePointer(ev)) return true;
        }

        if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
            textEditor_.handlePointer(ev, ctx);
        }

        if (isPanningCanvas_) {
            isPanningCanvas_ = false;
            if (isRightClickCandidate_) {
                isRightClickCandidate_ = false;
                openModuleSearchDialog(modularZoomPan_.screenToWorldX(ev.x), modularZoomPan_.screenToWorldY(ev.y));
                return true;
            }
            double touchDuration = (ev.timestampMs > 0.0 && touchDownTimeMs_ > 0.0) ? (ev.timestampMs - touchDownTimeMs_) : 0.0;
            if ((ev.isTouch() || ctx.isMobile) && !touchMoved_ && touchDuration >= 450.0) {
                openModuleSearchDialog(touchDownWorldX_, touchDownWorldY_);
                return true;
            }
            return true;
        }

        if (draggingModuleIdx_ >= 0 && draggingModuleIdx_ < static_cast<int>(modules_.size())) {
            draggingModuleIdx_ = -1;
            initialModuleStates_.clear();
            return true;
        }

        if (draggingModuleKnobMod_ >= 0) {
            draggingModuleKnobMod_ = -1;
            draggingModuleKnobIdx_ = -1;
            return true;
        }

        if (isDraggingCord_) {
            // Find closest jack within 26px in world units
            float cwx = modularZoomPan_.screenToWorldX(cordCurrentX_);
            float cwy = modularZoomPan_.screenToWorldY(cordCurrentY_);
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
                        float dx = cwx - jx;
                        float dy = cwy - jackY;
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
                        float dx = cwx - jx;
                        float dy = cwy - jackY;
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
        if (moduleSearchDialog_.isOpen()) {
            if (moduleSearchDialog_.handlePointer(ev)) return true;
        }
        if (isExplorerOpen_ && explorerBounds_.contains(ev.x, ev.y)) {
            explorerScrollY_ = std::clamp(explorerScrollY_ - ev.scrollY * 24.0f, 0.0f, explorerMaxScrollY_);
            return true;
        }
        if (mode_ == DesignSubMode::ModularRack || (mode_ == DesignSubMode::Split && !splitShowsGui_)) {
            if (ev.mods.ctrl) {
                // Ctrl + mousewheel: zoom in / out anchored to mouse cursor
                float factor = (ev.scrollY > 0.0f) ? 1.10f : 0.90f;
                modularZoomPan_.zoomAt(ev.x, ev.y, factor);
                return true;
            } else {
                // Bare mousewheel: pan canvas vertically (or horizontally if shift is held)
                float panDeltaX = ev.scrollX * 30.0f;
                float panDeltaY = ev.scrollY * 30.0f;
                if (ev.mods.shift) {
                    panDeltaX = ev.scrollY * 30.0f;
                    panDeltaY = 0.0f;
                }
                modularZoomPan_.panByScreen(panDeltaX, panDeltaY);
                return true;
            }
        }
        if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
            if (textEditor_.handlePointer(ev, ctx)) return true;
        }
    }

    return false;
}

bool DesignView::handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
    if (moduleSearchDialog_.isOpen()) {
        if (moduleSearchDialog_.handleKey(key, scancode, action, mods)) {
            return true;
        }
    }

    if (action == 1) { // GLFW_PRESS
        // Ctrl+Enter: Compile current script
        if (key == 257 && (mods & 2)) {
            compileCurrentScript(ctx);
            return true;
        }

        if (mode_ == DesignSubMode::ModularRack) {
            // Space or Enter opens Module Search Dialog
            if (key == 32 || (key == 257 && !(mods & 2))) {
                openModuleSearchDialog(0.0f, 0.0f);
                return true;
            }
            // Zoom In (+ or =)
            if (key == 61 || key == 334) {
                modularZoomPan_.zoomAt(modularZoomPan_.getViewportOriginX() + modularZoomPan_.getViewportWidth() * 0.5f,
                                       modularZoomPan_.getViewportOriginY() + modularZoomPan_.getViewportHeight() * 0.5f, 1.15f);
                return true;
            }
            // Zoom Out (-)
            if (key == 45 || key == 333) {
                modularZoomPan_.zoomAt(modularZoomPan_.getViewportOriginX() + modularZoomPan_.getViewportWidth() * 0.5f,
                                       modularZoomPan_.getViewportOriginY() + modularZoomPan_.getViewportHeight() * 0.5f, 0.85f);
                return true;
            }
            // Reset Zoom (0)
            if (key == 48 || key == 320) {
                modularZoomPan_.setZoom(1.0f, 1.0f);
                modularZoomPan_.setPan(modularZoomPan_.getViewportOriginX(), modularZoomPan_.getViewportOriginY());
                return true;
            }
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
    if (moduleSearchDialog_.isOpen()) {
        if (moduleSearchDialog_.handleChar(codepoint)) {
            return true;
        }
    }
    if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
        return textEditor_.handleChar(codepoint, ctx);
    }
    return false;
}

} // namespace eatsbits::ui
