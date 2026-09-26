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
        {"OSC 1: TB-303 CORE", "OSC", 0.0f, 0.0f, 230.0f, 320.0f, 0.20f, 0.21f, 0.24f,
         {{"TUNE", 0.50f, -24.0f, 24.0f, "st"}, {"WAVE", 0.0f, 0.0f, 1.0f, ""}, {"GLIDE", 0.35f, 0.0f, 1.0f, "s"}},
         {"CV IN", "GATE"}, {"OUT", "SUB"}},

        {"FILTER: 24dB DIODE", "VCF", 0.0f, 0.0f, 230.0f, 320.0f, 0.22f, 0.23f, 0.26f,
         {{"CUTOFF", 0.65f, 20.0f, 20000.0f, "Hz"}, {"RESON", 0.78f, 0.0f, 1.0f, ""}, {"ENV MOD", 0.60f, 0.0f, 1.0f, ""}, {"ACCENT", 0.70f, 0.0f, 1.0f, ""}},
         {"AUDIO IN", "FM IN"}, {"LPF OUT", "ENV OUT"}},

        {"VCA & SATURATION", "VCA", 0.0f, 0.0f, 210.0f, 320.0f, 0.19f, 0.20f, 0.23f,
         {{"DRIVE", 0.45f, 1.0f, 10.0f, "x"}, {"ATTACK", 0.05f, 0.001f, 1.0f, "s"}, {"DECAY", 0.45f, 0.01f, 2.0f, "s"}},
         {"IN", "CV"}, {"OUT", "MON"}},

        {"OUT: MASTER BUS", "OUT", 0.0f, 0.0f, 210.0f, 320.0f, 0.18f, 0.19f, 0.22f,
         {{"VOLUME", 0.85f, 0.0f, 1.0f, ""}, {"PAN", 0.0f, -1.0f, 1.0f, ""}, {"LIMIT", 0.90f, 0.0f, 1.0f, ""}},
         {"L IN", "R IN"}, {"MAIN L", "MAIN R"}}
    };

    // Synthesize scope buffer with animated waveform
    scopeSamples_.resize(128, 0.0f);
    for (size_t i = 0; i < scopeSamples_.size(); ++i) {
        float ph = static_cast<float>(i) / static_cast<float>(scopeSamples_.size()) * 6.2831853f * 2.0f;
        scopeSamples_[i] = std::sin(ph) * 0.75f + std::sin(ph * 2.0f) * 0.25f;
    }
}

void DesignView::setSubMode(DesignSubMode mode) noexcept {
    mode_ = mode;
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
}

void DesignView::render(const ViewContext& ctx) {
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

            // Track dot
            drawCircle(r, itemR.x + 10.0f, itemR.y + 13.0f, 3.5f,
                       allTargets_[i].trackColor.r, allTargets_[i].trackColor.g, allTargets_[i].trackColor.b, 1.0f);

            // Badge pill
            drawRoundedRect(r, itemR.x + 22.0f, itemR.y + 5.0f, 60.0f, 16.0f, 2.0f,
                            allTargets_[i].badgeColor.r * 0.2f, allTargets_[i].badgeColor.g * 0.2f, allTargets_[i].badgeColor.b * 0.2f, 0.8f);
            drawText(r, allTargets_[i].typeBadge, itemR.x + 26.0f, itemR.y + 7.0f, 8.0f,
                     allTargets_[i].badgeColor.r, allTargets_[i].badgeColor.g, allTargets_[i].badgeColor.b, 1.0f);

            // Title
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

    // Background & subtle border
    drawRect(r, headerBounds_.x, headerBounds_.y, headerBounds_.w, headerBounds_.h,
             theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 1.0f);
    drawLine(r, headerBounds_.x, headerBounds_.y + headerBounds_.h,
             headerBounds_.x + headerBounds_.w, headerBounds_.y + headerBounds_.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.2f);

    // Explorer Toggle Button (view_sidebar icon representation)
    drawRoundedRect(r, explorerToggleBtn_.x, explorerToggleBtn_.y, explorerToggleBtn_.w, explorerToggleBtn_.h, 4.0f,
                    0.15f, 0.17f, 0.22f, 0.8f);
    drawRoundedRectOutline(r, explorerToggleBtn_.x, explorerToggleBtn_.y, explorerToggleBtn_.w, explorerToggleBtn_.h, 4.0f,
                           isExplorerOpen_ ? theme.primaryAccent.r : theme.borderSubtle.r,
                           isExplorerOpen_ ? theme.primaryAccent.g : theme.borderSubtle.g,
                           isExplorerOpen_ ? theme.primaryAccent.b : theme.borderSubtle.b,
                           0.8f, 1.0f);

    // Sidebar icon lines
    float ix = explorerToggleBtn_.x + 8.0f;
    float iy = explorerToggleBtn_.y + 7.0f;
    drawRect(r, ix, iy, 4.0f, 14.0f, isExplorerOpen_ ? theme.primaryAccent.r : 0.5f, isExplorerOpen_ ? theme.primaryAccent.g : 0.5f, isExplorerOpen_ ? theme.primaryAccent.b : 0.5f, 1.0f);
    drawRect(r, ix + 6.0f, iy, 8.0f, 14.0f, 0.35f, 0.38f, 0.45f, 0.8f);

    // Target Badge
    drawRoundedRect(r, targetBadgeBounds_.x, targetBadgeBounds_.y, targetBadgeBounds_.w, targetBadgeBounds_.h, 3.0f,
                    activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);
    drawText(r, activeT.typeBadge, targetBadgeBounds_.x + 8.0f, targetBadgeBounds_.y + 5.0f, 9.0f,
             0.0f, 0.0f, 0.0f, 1.0f);

    // Target Dropdown Button
    drawRoundedRect(r, targetDropdownBtn_.x, targetDropdownBtn_.y, targetDropdownBtn_.w, targetDropdownBtn_.h, 4.0f,
                    0.12f, 0.14f, 0.18f, 0.9f);
    drawRoundedRectOutline(r, targetDropdownBtn_.x, targetDropdownBtn_.y, targetDropdownBtn_.w, targetDropdownBtn_.h, 4.0f,
                           activeT.badgeColor.r * 0.6f, activeT.badgeColor.g * 0.6f, activeT.badgeColor.b * 0.6f, 0.8f, 1.0f);

    // Track dot
    drawCircle(r, targetDropdownBtn_.x + 12.0f, targetDropdownBtn_.y + 14.0f, 3.5f,
               activeT.trackColor.r, activeT.trackColor.g, activeT.trackColor.b, 1.0f);

    // Target Title & Dropdown Arrow
    drawText(r, activeT.title, targetDropdownBtn_.x + 22.0f, targetDropdownBtn_.y + 8.0f, 10.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "v", targetDropdownBtn_.x + targetDropdownBtn_.w - 14.0f, targetDropdownBtn_.y + 9.0f, 8.0f,
             activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);

    // Mode Switcher Buttons: [CODE] [MODULAR] [SPLIT] [GUI]
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

    // COMPILE & RUN Button (Luminescent Green)
    drawButton(r, btnCompile_, "> COMPILE & RUN",
               Color{0.05f, 0.35f, 0.18f, 0.95f}, Color{0.0f, 0.95f, 0.45f, 1.0f}, Color{0.0f, 1.0f, 0.55f, 1.0f},
               10.5f, 4.0f, 1.2f);
}

void DesignView::renderExplorerSidebar(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // Sidebar background & separator
    drawRect(r, explorerBounds_.x, explorerBounds_.y, explorerBounds_.w, explorerBounds_.h,
             0.10f, 0.11f, 0.14f, 1.0f);
    drawLine(r, explorerBounds_.x + explorerBounds_.w, explorerBounds_.y,
             explorerBounds_.x + explorerBounds_.w, explorerBounds_.y + explorerBounds_.h,
             0.18f, 0.20f, 0.26f, 1.0f, 1.2f);

    // Explorer Header: PROJECT SCRIPTS + count
    float ey = explorerBounds_.y + 8.0f;
    drawText(r, "PROJECT SCRIPTS", explorerBounds_.x + 10.0f, ey, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    drawText(r, std::to_string(allTargets_.size()), explorerBounds_.x + explorerBounds_.w - 24.0f, ey, 10.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

    // Search filter input box
    ey += 20.0f;
    drawRoundedRect(r, explorerBounds_.x + 8.0f, ey, explorerBounds_.w - 16.0f, 22.0f, 3.0f,
                    0.05f, 0.06f, 0.08f, 0.9f);
    drawMonoText(r, scriptFilterQuery_.empty() ? "Filter scripts..." : scriptFilterQuery_,
                 explorerBounds_.x + 14.0f, ey + 5.0f, 9.5f,
                 scriptFilterQuery_.empty() ? theme.textMuted.r : 0.95f,
                 scriptFilterQuery_.empty() ? theme.textMuted.g : 0.95f,
                 scriptFilterQuery_.empty() ? theme.textMuted.b : 0.95f,
                 scriptFilterQuery_.empty() ? 0.6f : 1.0f);

    // Groups
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

    // 1. Mini Audio Oscilloscope at top
    renderOscilloscope(ctx, scopeBounds_);

    // 2. Editor Header
    drawRect(r, editorHeaderBounds_.x, editorHeaderBounds_.y, editorHeaderBounds_.w, editorHeaderBounds_.h,
             0.13f, 0.15f, 0.19f, 1.0f);
    drawLine(r, editorHeaderBounds_.x, editorHeaderBounds_.y + editorHeaderBounds_.h,
             editorHeaderBounds_.x + editorHeaderBounds_.w, editorHeaderBounds_.y + editorHeaderBounds_.h,
             0.22f, 0.25f, 0.32f, 1.0f, 1.0f);

    std::string targetLabel = "TARGET: " + activeT.title;
    drawText(r, targetLabel, editorHeaderBounds_.x + 10.0f, editorHeaderBounds_.y + 8.0f, 10.5f,
             activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);

    // Tools on right side: SELECT ALL, COPY, SUBMIT PR, API DOCS
    drawButton(r, btnSelectAll_, "SELECT ALL", Color{0.12f, 0.14f, 0.18f, 0.8f}, Color{0.25f, 0.28f, 0.35f, 0.8f}, Color{0.7f, 0.75f, 0.85f, 1.0f}, 9.0f, 3.0f, 1.0f);
    drawButton(r, btnCopy_, "COPY", Color{0.12f, 0.14f, 0.18f, 0.8f}, Color{0.25f, 0.28f, 0.35f, 0.8f}, Color{0.7f, 0.75f, 0.85f, 1.0f}, 9.0f, 3.0f, 1.0f);
    drawButton(r, btnSubmitPr_, "SUBMIT PR", Color{0.14f, 0.18f, 0.24f, 0.8f}, Color{theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.8f}, theme.primaryAccent, 9.0f, 3.0f, 1.0f);
    drawButton(r, btnApiDocs_, "API DOCS", Color{0.05f, 0.20f, 0.12f, 0.8f}, Color{0.0f, 0.95f, 0.45f, 0.8f}, Color{0.0f, 0.95f, 0.45f, 1.0f}, 9.0f, 3.0f, 1.0f);

    // 3. Code Editor Box
    drawRect(r, editorCanvasBounds_.x, editorCanvasBounds_.y, editorCanvasBounds_.w, editorCanvasBounds_.h,
             0.06f, 0.07f, 0.09f, 1.0f);
    drawRoundedRectOutline(r, editorCanvasBounds_.x, editorCanvasBounds_.y, editorCanvasBounds_.w, editorCanvasBounds_.h, 0.0f,
                           0.20f, 0.23f, 0.30f, 1.0f, 1.2f);

    // Gutter with line numbers (50px wide)
    float gutterW = 46.0f;
    drawRect(r, editorCanvasBounds_.x, editorCanvasBounds_.y, gutterW, editorCanvasBounds_.h,
             0.08f, 0.09f, 0.12f, 1.0f);
    drawLine(r, editorCanvasBounds_.x + gutterW, editorCanvasBounds_.y,
             editorCanvasBounds_.x + gutterW, editorCanvasBounds_.y + editorCanvasBounds_.h,
             0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

    float lineH = 18.0f;
    int maxLines = static_cast<int>((editorCanvasBounds_.h - 10.0f) / lineH);

    for (int i = 0; i < maxLines && i < static_cast<int>(codeLines_.size()); ++i) {
        float ly = editorCanvasBounds_.y + 8.0f + (i * lineH);

        // Line number
        std::string numStr = (i + 1 < 10 ? " " : "") + std::to_string(i + 1);
        drawMonoText(r, numStr, editorCanvasBounds_.x + 10.0f, ly, 9.5f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.65f);

        // Colorized line text
        const auto& line = codeLines_[i];
        float textX = editorCanvasBounds_.x + gutterW + 12.0f;

        if (line.starts_with("#")) {
            drawMonoText(r, line, textX, ly, 10.0f, 0.38f, 0.49f, 0.55f, 1.0f); // Muted comment
        } else if (line.find("def ") != std::string::npos || line.find("import ") != std::string::npos) {
            drawMonoText(r, line, textX, ly, 10.0f, 1.0f, 0.85f, 0.0f, 1.0f); // Gold keyword
        } else if (line.find("eat.param") != std::string::npos || line.find("params.get") != std::string::npos) {
            drawMonoText(r, line, textX, ly, 10.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f); // Cyan API
        } else {
            drawMonoText(r, line, textX, ly, 10.0f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.95f);
        }
    }

    // 4. Status Bar
    drawRoundedRect(r, statusBannerBounds_.x, statusBannerBounds_.y, statusBannerBounds_.w, statusBannerBounds_.h, 4.0f,
                    0.05f, 0.22f, 0.12f, 0.85f);
    drawRoundedRectOutline(r, statusBannerBounds_.x, statusBannerBounds_.y, statusBannerBounds_.w, statusBannerBounds_.h, 4.0f,
                           0.0f, 0.95f, 0.45f, 0.7f, 1.0f);
    drawText(r, "[OK] " + statusMsg_, statusBannerBounds_.x + 10.0f, statusBannerBounds_.y + 6.0f, 9.5f,
             0.0f, 1.0f, 0.55f, 1.0f);

    // 5. Live Parameters Panel
    float titleY = liveParamsSectionBounds_.y;
    std::string paramHeader = "LIVE SCRIPT PARAMETERS (" + activeT.title + ")";
    drawText(r, paramHeader, liveParamsSectionBounds_.x, titleY, 10.5f,
             activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);

    for (size_t i = 0; i < currentParams_.size(); ++i) {
        const auto& p = currentParams_[i];
        const auto& pb = p.bounds;

        // Card container
        drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 4.0f, 0.12f, 0.14f, 0.18f, 0.95f);
        drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 4.0f,
                               activeT.badgeColor.r * 0.4f, activeT.badgeColor.g * 0.4f, activeT.badgeColor.b * 0.4f, 0.8f, 1.0f);

        // Parameter Name & Numeric Value
        drawText(r, p.name, pb.x + 8.0f, pb.y + 6.0f, 10.0f, 0.95f, 0.95f, 0.95f, 1.0f);

        std::ostringstream valOss;
        valOss << std::fixed << std::setprecision(1) << p.currentVal;
        drawText(r, valOss.str(), pb.x + pb.w - 36.0f, pb.y + 6.0f, 10.0f,
                 activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);

        // Horizontal Slider Track & Knob Thumb
        float trackX = pb.x + 8.0f;
        float trackY = pb.y + 26.0f;
        float trackW = pb.w - 16.0f;
        float trackH = 6.0f;

        float norm = (p.maxVal > p.minVal) ? std::clamp((p.currentVal - p.minVal) / (p.maxVal - p.minVal), 0.0f, 1.0f) : 0.5f;

        drawRoundedRect(r, trackX, trackY, trackW, trackH, 3.0f, 0.06f, 0.07f, 0.09f, 1.0f);
        drawRoundedRect(r, trackX, trackY, trackW * norm, trackH, 3.0f,
                        activeT.badgeColor.r * 0.7f, activeT.badgeColor.g * 0.7f, activeT.badgeColor.b * 0.7f, 1.0f);

        // Thumb
        float thumbX = trackX + (trackW * norm);
        drawCircle(r, thumbX, trackY + 3.0f, 6.0f, 0.95f, 0.95f, 0.95f, 1.0f);
        drawCircle(r, thumbX, trackY + 3.0f, 3.0f, activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);
    }
}

void DesignView::renderOscilloscope(const ViewContext& ctx, const Rect2D& rect) {
    auto& r = *ctx.renderer;

    // Dark scope CRT screen
    drawRoundedRect(r, rect.x, rect.y, rect.w, rect.h, 4.0f, 0.02f, 0.04f, 0.03f, 1.0f);
    drawRoundedRectOutline(r, rect.x, rect.y, rect.w, rect.h, 4.0f, 0.0f, 0.65f, 0.35f, 0.8f, 1.2f);

    // Green CRT grid lines
    float gridStep = rect.w / 12.0f;
    for (float gx = rect.x + gridStep; gx < rect.x + rect.w; gx += gridStep) {
        drawLine(r, gx, rect.y, gx, rect.y + rect.h, 0.0f, 0.22f, 0.12f, 0.4f, 1.0f);
    }
    float midY = rect.y + (rect.h * 0.5f);
    drawLine(r, rect.x, midY, rect.x + rect.w, midY, 0.0f, 0.25f, 0.15f, 0.5f, 1.0f);

    // Audio Waveform
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

    // Oscilloscope Header
    std::string scopeTitle = "[*] OSCILLOSCOPE - " + getActiveTarget().title;
    drawText(r, scopeTitle, rect.x + 10.0f, rect.y + 6.0f, 9.0f, 0.0f, 1.0f, 0.55f, 1.0f);
}

void DesignView::renderModularRack(const ViewContext& ctx, const Rect2D& rect) {
    auto& r = *ctx.renderer;

    // Dark chassis background
    drawRect(r, rect.x, rect.y, rect.w, rect.h, 0.11f, 0.12f, 0.15f, 1.0f);

    // Steel rack mounting rails top and bottom with screw holes
    float railH = 14.0f;
    drawRect(r, rect.x, rect.y, rect.w, railH, 0.22f, 0.23f, 0.28f, 1.0f);
    drawRect(r, rect.x, rect.y + rect.h - railH, rect.w, railH, 0.22f, 0.23f, 0.28f, 1.0f);

    for (float sx = rect.x + 16.0f; sx < rect.x + rect.w; sx += 40.0f) {
        drawCircle(r, sx, rect.y + 7.0f, 2.5f, 0.45f, 0.48f, 0.55f, 1.0f);
        drawCircle(r, sx, rect.y + rect.h - 7.0f, 2.5f, 0.45f, 0.48f, 0.55f, 1.0f);
    }

    // Position modules horizontally
    float modX = rect.x + 24.0f;
    float modY = rect.y + railH + 12.0f;
    float modH = rect.h - (railH * 2.0f) - 24.0f;

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

            // Dial cap and needle
            drawCircle(r, kx, ky, 15.0f, 0.12f, 0.13f, 0.16f, 1.0f);
            drawCircle(r, kx, ky, 12.0f, 0.25f, 0.27f, 0.32f, 1.0f);

            float ang = -2.356f + (mod.knobs[k].value * 4.712f);
            float nx = kx + std::sin(ang) * 11.0f;
            float ny = ky - std::cos(ang) * 11.0f;
            drawLine(r, kx, ky, nx, ny, 1.0f, 0.85f, 0.0f, 1.0f, 2.0f);

            drawText(r, mod.knobs[k].label, kx - 16.0f, ky + 18.0f, 8.5f, 0.8f, 0.85f, 0.9f, 0.9f);
        }

        // Jacks: Inputs (Cyan) on Left, Outputs (Gold) on Right
        float jackY = mod.y + mod.h - 45.0f;

        for (size_t i = 0; i < mod.inputs.size(); ++i) {
            float jx = mod.x + 30.0f + (i * 45.0f);
            drawCircle(r, jx, jackY, 11.0f, 0.12f, 0.13f, 0.16f, 1.0f);
            drawCircle(r, jx, jackY, 7.0f, 0.0f, 0.95f, 1.0f, 1.0f);
            drawCircle(r, jx, jackY, 3.5f, 0.02f, 0.02f, 0.03f, 1.0f);
            drawText(r, mod.inputs[i], jx - 12.0f, jackY - 22.0f, 7.5f, 0.0f, 0.95f, 1.0f, 0.9f);
        }

        for (size_t o = 0; o < mod.outputs.size(); ++o) {
            float jx = mod.x + mod.w - 30.0f - (o * 45.0f);
            drawCircle(r, jx, jackY, 11.0f, 0.12f, 0.13f, 0.16f, 1.0f);
            drawCircle(r, jx, jackY, 7.0f, 1.0f, 0.55f, 0.0f, 1.0f);
            drawCircle(r, jx, jackY, 3.5f, 0.02f, 0.02f, 0.03f, 1.0f);
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
        renderCatenaryCable(r, cordStartX_, cordStartY_, cordCurrentX_, cordCurrentY_, 1.0f, 0.85f, 0.0f);
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

    // Left Half: Code Editor Canvas
    renderCodeEditorCanvas(ctx);

    // Center divider
    drawLine(r, rightBounds.x, rightBounds.y, rightBounds.x, rightBounds.y + rightBounds.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.9f, 2.0f);

    // Right Half Sub-bar: [FACEPLATE] vs [MODULAR]
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

    // Top Sub-bar for GUI Tab: Title + [● LIVE INTERACTION] vs [✏️ DESIGN MODE] + [SUBMIT PR]
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
    const auto& activeT = getActiveTarget();

    // Center hardware faceplate chassis (e.g. 520px x 340px)
    float fpW = std::min(rect.w - 40.0f, 600.0f);
    float fpH = std::min(rect.h - 40.0f, 380.0f);
    float fpX = rect.x + (rect.w - fpW) * 0.5f;
    float fpY = rect.y + (rect.h - fpH) * 0.5f;

    // Dark brushed aluminum faceplate
    drawRoundedRect(r, fpX, fpY, fpW, fpH, 8.0f, 0.14f, 0.16f, 0.20f, 1.0f);
    drawRoundedRectOutline(r, fpX, fpY, fpW, fpH, 8.0f, 0.35f, 0.38f, 0.46f, 1.0f, 2.0f);

    // Wood / Rosewood rack cheeks on the sides
    drawRoundedRect(r, fpX - 12.0f, fpY, 12.0f, fpH, 4.0f, 0.28f, 0.14f, 0.08f, 1.0f);
    drawRoundedRect(r, fpX + fpW, fpY, 12.0f, fpH, 4.0f, 0.28f, 0.14f, 0.08f, 1.0f);

    // Faceplate Header
    drawText(r, activeT.title, fpX + 24.0f, fpY + 20.0f, 14.0f, 0.95f, 0.95f, 0.95f, 1.0f);
    drawText(r, activeT.subtitle, fpX + 24.0f, fpY + 38.0f, 10.0f, activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 0.9f);

    // Interactive Knobs Grid
    float knobAreaY = fpY + 80.0f;
    for (size_t i = 0; i < currentParams_.size(); ++i) {
        float kx = fpX + 50.0f + ((i % 4) * 130.0f);
        float ky = knobAreaY + ((i / 4) * 110.0f);

        const auto& p = currentParams_[i];
        float norm = (p.maxVal > p.minVal) ? std::clamp((p.currentVal - p.minVal) / (p.maxVal - p.minVal), 0.0f, 1.0f) : 0.5f;

        // Dial shadow & body
        drawCircle(r, kx, ky, 24.0f, 0.08f, 0.09f, 0.11f, 1.0f);
        drawCircle(r, kx, ky, 20.0f, 0.22f, 0.24f, 0.28f, 1.0f);

        // Value arc
        float minA = -2.356f;
        float maxA = 2.356f;
        float curA = minA + (norm * (maxA - minA));

        float nx = kx + std::sin(curA) * 18.0f;
        float ny = ky - std::cos(curA) * 18.0f;
        drawLine(r, kx, ky, nx, ny, activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f, 2.5f);

        // Parameter label & readout
        drawText(r, p.name, kx - 22.0f, ky + 28.0f, 9.5f, 0.85f, 0.88f, 0.92f, 1.0f);

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << p.currentVal;
        drawText(r, ss.str(), kx - 14.0f, ky + 42.0f, 9.0f, activeT.badgeColor.r, activeT.badgeColor.g, activeT.badgeColor.b, 1.0f);
    }
}

void DesignView::renderGuiDesigner(const ViewContext& ctx, const Rect2D& rect) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // 1. Left Widget Palette (180px)
    float palW = 180.0f;
    Rect2D palR(rect.x, rect.y, palW, rect.h);
    drawRect(r, palR.x, palR.y, palR.w, palR.h, 0.10f, 0.11f, 0.14f, 1.0f);
    drawLine(r, palR.x + palR.w, palR.y, palR.x + palR.w, palR.y + palR.h, 0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

    drawText(r, "WIDGET PALETTE", palR.x + 12.0f, palR.y + 12.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    const char* widgets[6] = {"Rotary Knob", "Hardware Slider", "Toggle Switch", "Nixie Display", "VU Meter", "Action Button"};
    float wy = palR.y + 36.0f;
    for (int w = 0; w < 6; ++w) {
        drawRoundedRect(r, palR.x + 8.0f, wy, palR.w - 16.0f, 28.0f, 4.0f, 0.15f, 0.17f, 0.22f, 0.9f);
        drawRoundedRectOutline(r, palR.x + 8.0f, wy, palR.w - 16.0f, 28.0f, 4.0f, 0.25f, 0.28f, 0.35f, 0.8f, 1.0f);
        drawText(r, widgets[w], palR.x + 16.0f, wy + 8.0f, 9.5f, 0.9f, 0.9f, 0.9f, 0.95f);
        wy += 34.0f;
    }

    // 2. Right Property Inspector (220px)
    float inspW = 220.0f;
    Rect2D inspR(rect.x + rect.w - inspW, rect.y, inspW, rect.h);
    drawRect(r, inspR.x, inspR.y, inspR.w, inspR.h, 0.10f, 0.11f, 0.14f, 1.0f);
    drawLine(r, inspR.x, inspR.y, inspR.x, inspR.y + inspR.h, 0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

    drawText(r, "PROPERTY INSPECTOR", inspR.x + 12.0f, inspR.y + 12.0f, 10.5f,
             Color{1.0f, 0.85f, 0.0f}.r, Color{1.0f, 0.85f, 0.0f}.g, Color{1.0f, 0.85f, 0.0f}.b, 1.0f);

    float iy = inspR.y + 36.0f;
    drawText(r, "Panel: Eats Instrument", inspR.x + 14.0f, iy, 9.5f, 0.8f, 0.8f, 0.8f, 0.9f);
    iy += 22.0f;
    drawText(r, "Background: Dark Chassis", inspR.x + 14.0f, iy, 9.5f, 0.8f, 0.8f, 0.8f, 0.9f);
    iy += 22.0f;
    drawText(r, "Knob Style: Chrome Fluted", inspR.x + 14.0f, iy, 9.5f, 0.8f, 0.8f, 0.8f, 0.9f);
    iy += 22.0f;
    drawText(r, "Accent: Cyan (#00FFE0)", inspR.x + 14.0f, iy, 9.5f, 0.8f, 0.8f, 0.8f, 0.9f);

    // 3. Center Canvas Editor Area
    Rect2D centerCanvas(palR.x + palW, rect.y, rect.w - palW - inspW, rect.h);
    drawRect(r, centerCanvas.x, centerCanvas.y, centerCanvas.w, centerCanvas.h, 0.07f, 0.08f, 0.10f, 1.0f);

    // Render Faceplate preview inside center area
    renderHardwareFaceplate(ctx, centerCanvas);
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

        // Code Editor Tool links
        if (mode_ == DesignSubMode::Code || mode_ == DesignSubMode::Split) {
            if (btnCopy_.contains(ev.x, ev.y)) {
                if (onCopyToClipboard) {
                    onCopyToClipboard(currentScriptCode_);
                }
                if (ctx.onShowNotification) {
                    ctx.onShowNotification("Copied script to clipboard");
                }
                return true;
            }
            if (btnSubmitPr_.contains(ev.x, ev.y)) {
                if (ctx.onShowNotification) {
                    ctx.onShowNotification("Opening GitHub PR submission dialog...");
                }
                return true;
            }
            if (btnApiDocs_.contains(ev.x, ev.y)) {
                if (ctx.onShowNotification) {
                    ctx.onShowNotification("Opening Eatscript API documentation...");
                }
                return true;
            }

            // Live Parameter Slider interaction
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

        // Split Sub-bar toggles
        if (mode_ == DesignSubMode::Split) {
            if (btnSplitFaceplate_.contains(ev.x, ev.y)) {
                splitShowsGui_ = true;
                return true;
            }
            if (btnSplitModular_.contains(ev.x, ev.y)) {
                splitShowsGui_ = false;
                return true;
            }
        }

        // GUI Sub-bar toggles
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
        }

        // Interactive hardware faceplate rotary knobs
        if ((mode_ == DesignSubMode::GuiDesigner && !isGuiDesignMode_) ||
            (mode_ == DesignSubMode::Split && splitShowsGui_)) {
            Rect2D fpRect = (mode_ == DesignSubMode::GuiDesigner) ?
                Rect2D(studioBodyBounds_.x, studioBodyBounds_.y + guiSubBarBounds_.h, studioBodyBounds_.w, studioBodyBounds_.h - guiSubBarBounds_.h) :
                Rect2D(studioBodyBounds_.x + (studioBodyBounds_.w * 0.45f), studioBodyBounds_.y + splitSubBarBounds_.h,
                       studioBodyBounds_.w * 0.55f, studioBodyBounds_.h - splitSubBarBounds_.h);
            float fpW = std::min(fpRect.w - 40.0f, 600.0f);
            float fpH = std::min(fpRect.h - 40.0f, 380.0f);
            float fpX = fpRect.x + (fpRect.w - fpW) * 0.5f;
            float fpY = fpRect.y + (fpRect.h - fpH) * 0.5f;
            float knobAreaY = fpY + 80.0f;
            for (size_t i = 0; i < currentParams_.size(); ++i) {
                float kx = fpX + 50.0f + ((i % 4) * 130.0f);
                float ky = knobAreaY + ((i / 4) * 110.0f);
                float dx = ev.x - kx;
                float dy = ev.y - ky;
                if ((dx * dx + dy * dy) <= (30.0f * 30.0f)) {
                    draggingFaceplateKnobIndex_ = static_cast<int>(i);
                    lastDragY_ = ev.y;
                    return true;
                }
            }
        }

        // GUI Designer widget palette click
        if (mode_ == DesignSubMode::GuiDesigner && isGuiDesignMode_) {
            Rect2D guiBody(studioBodyBounds_.x, studioBodyBounds_.y + guiSubBarBounds_.h, studioBodyBounds_.w, studioBodyBounds_.h - guiSubBarBounds_.h);
            float palW = 180.0f;
            Rect2D palR(guiBody.x, guiBody.y, palW, guiBody.h);
            if (palR.contains(ev.x, ev.y)) {
                float wy = palR.y + 36.0f;
                const char* widgets[6] = {"Rotary Knob", "Hardware Slider", "Toggle Switch", "Nixie Display", "VU Meter", "Action Button"};
                for (int w = 0; w < 6; ++w) {
                    Rect2D wb(palR.x + 8.0f, wy, palR.w - 16.0f, 28.0f);
                    if (wb.contains(ev.x, ev.y)) {
                        if (ctx.onShowNotification) {
                            ctx.onShowNotification(std::string("Selected widget: ") + widgets[w]);
                        }
                        return true;
                    }
                    wy += 34.0f;
                }
            }
        }
    } else if (ev.action == PointerAction::Move) {
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
        if (draggingFaceplateKnobIndex_ >= 0 && draggingFaceplateKnobIndex_ < static_cast<int>(currentParams_.size())) {
            auto& p = currentParams_[draggingFaceplateKnobIndex_];
            float delta = (lastDragY_ - ev.y) / 100.0f; // Drag up to increase value
            lastDragY_ = ev.y;
            float range = p.maxVal - p.minVal;
            p.currentVal = std::clamp(p.currentVal + delta * range, p.minVal, p.maxVal);
            if (onParamChanged) {
                onParamChanged(getActiveTarget().id, p.name, p.currentVal);
            }
            return true;
        }
    } else if (ev.action == PointerAction::Up) {
        if (draggingParamIndex_ >= 0) {
            draggingParamIndex_ = -1;
            return true;
        }
        if (draggingFaceplateKnobIndex_ >= 0) {
            draggingFaceplateKnobIndex_ = -1;
            return true;
        }
    }

    return false;
}

bool DesignView::handleKey(int key, [[maybe_unused]] int scancode, int action, int mods, const ViewContext& ctx) {
    if (action == 1) { // GLFW_PRESS
        // Ctrl+Enter: Compile current script
        if (key == 257 && (mods & 2)) { // Enter with Ctrl
            compileCurrentScript(ctx);
            return true;
        }
    }
    return false;
}

} // namespace eatsbits::ui
