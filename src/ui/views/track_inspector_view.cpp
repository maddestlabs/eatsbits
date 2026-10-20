#include "eatsbits/ui/views/track_inspector_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/project/preset_loader.hpp"
#include "eatsbits/project/preset_manager.hpp"
#include "eatsbits/project/eats_builtin_presets.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::ui {

TrackInspectorView::TrackInspectorView() {
    initDefaultTracks();
    panel_.setShowTrackRibbon(false);

    // Bridge callbacks
    panel_.onTrackSelected = [this](uint32_t idx) {
        setActiveTrack(idx);
        if (onTrackSelected) onTrackSelected(selectedTrackIndex_);
    };
    panel_.onVolumeChanged = [this](uint32_t idx, float vol) {
        if (idx < tracks_.size()) tracks_[idx].volume = vol;
        if (onVolumeChanged) onVolumeChanged(idx, vol);
    };
    panel_.onPanChanged = [this](uint32_t idx, float pan) {
        if (idx < tracks_.size()) tracks_[idx].pan = pan;
        if (onPanChanged) onPanChanged(idx, pan);
    };
    panel_.onMuteToggled = [this](uint32_t idx, bool mute) {
        if (idx < tracks_.size()) tracks_[idx].mute = mute;
        if (onMuteToggled) onMuteToggled(idx, mute);
    };
    panel_.onSoloToggled = [this](uint32_t idx, bool solo) {
        if (idx < tracks_.size()) tracks_[idx].solo = solo;
        if (onSoloToggled) onSoloToggled(idx, solo);
    };
    panel_.onFreezeToggled = [this](uint32_t idx, bool freeze) {
        if (idx < tracks_.size()) tracks_[idx].freeze = freeze;
        if (onFreezeToggled) onFreezeToggled(idx, freeze);
    };
    panel_.onColorChanged = [this](uint32_t idx, float r, float g, float b) {
        if (idx < tracks_.size()) {
            tracks_[idx].r = r;
            tracks_[idx].g = g;
            tracks_[idx].b = b;
        }
        if (onColorChanged) onColorChanged(idx, r, g, b);
    };
    panel_.onOpenCodeEditor = [this](uint32_t idx) {
        if (onOpenCodeEditor) onOpenCodeEditor(idx);
    };
    panel_.onChangeInstrument = [this](uint32_t idx) {
        if (onChangeInstrument) onChangeInstrument(idx);
    };
    panel_.onPrevPreset = [this]() {
        if (onPrevPreset) onPrevPreset();
    };
    panel_.onNextPreset = [this]() {
        if (onNextPreset) onNextPreset();
    };
    panel_.onChordFollowChanged = [this](uint32_t idx, ChordFollowMode mode) {
        if (idx < tracks_.size()) tracks_[idx].chordFollowMode = mode;
        if (onChordFollowChanged) onChordFollowChanged(idx, mode);
    };
    panel_.onBakeChords = [this](uint32_t idx) {
        if (onBakeChords) onBakeChords(idx);
    };
    panel_.onParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
        if (idx < tracks_.size()) {
            tracks_[idx].setKnobValue(paramName, normVal);
            drawerData_.setKnobValue(paramName, normVal);
        }
        if (onParamChanged) onParamChanged(idx, paramName, normVal);
    };
    panel_.onMidiFxParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
        if (onMidiFxParamChanged) onMidiFxParamChanged(idx, paramName, normVal);
    };
    panel_.onAudioFxParamChanged = [this](uint32_t idx, size_t fxIdx, const std::string& paramName, float normVal) {
        if (onAudioFxParamChanged) onAudioFxParamChanged(idx, fxIdx, paramName, normVal);
    };
    panel_.onAddMidiFx = [this](uint32_t idx) {
        if (onAddMidiFx) onAddMidiFx(idx);
    };
    panel_.onAddAudioFx = [this](uint32_t idx) {
        if (onAddAudioFx) onAddAudioFx(idx);
    };
    panel_.onRemoveMidiFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (trackIdx < tracks_.size() && fxIdx < tracks_[trackIdx].midiFxList.size()) {
            tracks_[trackIdx].midiFxList.erase(tracks_[trackIdx].midiFxList.begin() + fxIdx);
            if (onRemoveMidiFx) onRemoveMidiFx(trackIdx, fxIdx);
            if (onMidiFxChanged) onMidiFxChanged(trackIdx);
        }
    };
    panel_.onRemoveAudioFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (trackIdx < tracks_.size() && fxIdx < tracks_[trackIdx].audioFxList.size()) {
            tracks_[trackIdx].audioFxList.erase(tracks_[trackIdx].audioFxList.begin() + fxIdx);
            if (onRemoveAudioFx) onRemoveAudioFx(trackIdx, fxIdx);
            if (onAudioFxChanged) onAudioFxChanged(trackIdx);
        }
    };
    panel_.onToggleMidiFx = [this](uint32_t trackIdx, size_t fxIdx, bool en) {
        if (trackIdx < tracks_.size() && fxIdx < tracks_[trackIdx].midiFxList.size()) {
            tracks_[trackIdx].midiFxList[fxIdx].enabled = en;
            if (onToggleMidiFx) onToggleMidiFx(trackIdx, fxIdx, en);
            if (onMidiFxChanged) onMidiFxChanged(trackIdx);
        }
    };
    panel_.onToggleAudioFx = [this](uint32_t trackIdx, size_t fxIdx, bool en) {
        if (trackIdx < tracks_.size() && fxIdx < tracks_[trackIdx].audioFxList.size()) {
            tracks_[trackIdx].audioFxList[fxIdx].enabled = en;
            if (onToggleAudioFx) onToggleAudioFx(trackIdx, fxIdx, en);
            if (onAudioFxChanged) onAudioFxChanged(trackIdx);
        }
    };
    panel_.onReorderAudioFx = [this](uint32_t trackIdx, size_t fromIdx, size_t toIdx) {
        if (trackIdx < tracks_.size()) {
            auto& list = tracks_[trackIdx].audioFxList;
            if (fromIdx < list.size() && toIdx < list.size() && fromIdx != toIdx) {
                auto item = list[fromIdx];
                list.erase(list.begin() + fromIdx);
                list.insert(list.begin() + toIdx, item);
                if (onReorderAudioFx) onReorderAudioFx(trackIdx, fromIdx, toIdx);
                if (onAudioFxChanged) onAudioFxChanged(trackIdx);
            }
        }
    };
    panel_.onReorderMidiFx = [this](uint32_t trackIdx, size_t fromIdx, size_t toIdx) {
        if (trackIdx < tracks_.size()) {
            auto& list = tracks_[trackIdx].midiFxList;
            if (fromIdx < list.size() && toIdx < list.size() && fromIdx != toIdx) {
                auto item = list[fromIdx];
                list.erase(list.begin() + fromIdx);
                list.insert(list.begin() + toIdx, item);
                if (onReorderMidiFx) onReorderMidiFx(trackIdx, fromIdx, toIdx);
                if (onMidiFxChanged) onMidiFxChanged(trackIdx);
            }
        }
    };
    panel_.onMidiFxChanged = [this](uint32_t trackIdx) {
        if (onMidiFxChanged) onMidiFxChanged(trackIdx);
    };
    panel_.onAudioFxChanged = [this](uint32_t trackIdx) {
        if (onAudioFxChanged) onAudioFxChanged(trackIdx);
    };
    panel_.onScrollChanged = [this](float sY) {
        if (onScrollChanged) onScrollChanged(sY);
    };
    panel_.onOpenFullscreenDevice = [this](uint32_t trackIdx) {
        if (onOpenFullscreenDevice) onOpenFullscreenDevice(trackIdx);
    };
    panel_.onOpenDetachedWindow = [this](uint32_t trackIdx) {
        if (onOpenDetachedWindow) onOpenDetachedWindow(trackIdx);
    };
    panel_.onOpenPluginGui = [this](uint32_t trackIdx) {
        if (onOpenPluginGui) onOpenPluginGui(trackIdx);
    };
    panel_.onOpenDesign = [this](uint32_t trackIdx) {
        if (onOpenDesign) onOpenDesign(trackIdx);
    };
    panel_.onOpenFullscreenAudioFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (onOpenFullscreenAudioFx) onOpenFullscreenAudioFx(trackIdx, fxIdx);
    };
    panel_.onOpenFullscreenMidiFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (onOpenFullscreenMidiFx) onOpenFullscreenMidiFx(trackIdx, fxIdx);
    };
    panel_.onAddClip = [this](uint32_t trackIdx) {
        if (onAddClip) onAddClip(trackIdx);
    };
    panel_.onDeleteTrack = [this](uint32_t trackIdx) {
        if (onDeleteTrack) onDeleteTrack(trackIdx);
    };
    panel_.onDuplicateTrack = [this](uint32_t trackIdx) {
        if (onDuplicateTrack) onDuplicateTrack(trackIdx);
    };
    panel_.onDuplicateClip = [this](uint32_t trackIdx, int clipIdx) {
        if (onDuplicateClip) onDuplicateClip(trackIdx, clipIdx);
    };
    panel_.onDeleteClip = [this](uint32_t trackIdx, int clipIdx) {
        if (onDeleteClip) onDeleteClip(trackIdx, clipIdx);
    };
    panel_.onAuditionNote = [this](uint32_t trackIdx, uint8_t note, float vel) {
        if (onAuditionNote) onAuditionNote(trackIdx, note, vel);
    };

    // Configure embedded PluginSearchDialog callback to sync track identity
    panel_.getPluginSearchDialog().onPluginSelected = [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t trackIndex) {
        if (trackIndex < tracks_.size()) {
            auto& trk = tracks_[trackIndex];
            if (mode == PluginDialogMode::AddInstrument) {
                trk.instrument = entry.name;
                trk.instrumentEngine = entry.id.empty() ? entry.engineTag : entry.id;
                trk.r = entry.r;
                trk.g = entry.g;
                trk.b = entry.b;
                trk.eatscriptCode.clear();
                trk.customGuiPanel.reset();
                syncKnobsForTrack(trk);
                updateDrawerData();
                if (onInstrumentChanged) onInstrumentChanged(trackIndex, trk.instrumentEngine, entry.name);
                if (onChangeInstrument) onChangeInstrument(trackIndex);
            } else if (mode == PluginDialogMode::AddMidiFx) {
                TrackMidiFxItem mfx;
                mfx.id = entry.id;
                mfx.name = entry.name;
                mfx.type = (entry.engineTag.empty() || entry.engineTag == "MIDI Processor" || entry.engineTag == "DSP Effect") ? entry.name : entry.engineTag;
                mfx.enabled = true;
                mfx.isExpanded = true;
                mfx.ensureDefaultKnobs();
                trk.midiFxList.push_back(mfx);
                trk.midiFx.arpEnabled = true;
                if (onAddMidiFx) onAddMidiFx(trackIndex);
                if (onMidiFxChanged) onMidiFxChanged(trackIndex);
            } else if (mode == PluginDialogMode::AddAudioFx) {
                TrackAudioFxItem afx;
                afx.id = entry.id;
                afx.name = entry.name;
                afx.type = entry.engineTag.empty() ? entry.name : entry.engineTag;
                afx.enabled = true;
                afx.isExpanded = true;
                afx.ensureDefaultKnobs();
                trk.audioFxList.push_back(afx);
                if (entry.engineTag == "delay") trk.audioFx.delayEnabled = true;
                else if (entry.engineTag == "chorus") trk.audioFx.chorusEnabled = true;
                else if (entry.engineTag == "convolver") trk.audioFx.convolverEnabled = true;
                else if (entry.engineTag == "eq") trk.audioFx.eqEnabled = true;
                else if (entry.engineTag == "comp") trk.audioFx.compEnabled = true;
                if (onAddAudioFx) onAddAudioFx(trackIndex);
                if (onAudioFxChanged) onAudioFxChanged(trackIndex);
            }
        }
    };

    // Initialize layout early for tests and headless validation
    layout(Rect2D{0.0f, 56.0f, 1280.0f, 744.0f}, ViewContext{});
}

void TrackInspectorView::initDefaultTracks() {
    tracks_.clear();

    // 1. DXVII E-Piano (Single default track)
    InspectorTrackChannel t1;
    t1.name = "DXVII E-Piano";
    t1.type = "KEYS";
    t1.r = 0.62f; t1.g = 0.31f; t1.b = 0.87f;
    t1.volume = 0.75f;
    t1.pan = 0.0f;
    t1.mute = false;
    t1.solo = false;
    t1.freeze = false;
    t1.instrument = "DXVII E-Piano";
    t1.instrumentEngine = "dxvii_epiano";
    t1.chordFollowMode = ChordFollowMode::Chord;

    if (auto p = eatsbits::project::BuiltinPresetRegistry::createPresetById("dxvii_epiano")) {
        t1.eatscriptCode = p->rawScript;
        t1.customGuiPanel = createGuiPanelFromPresetDefinition(*p);
        auto kList = extractKnobsFromPresetDefinition(*p);
        t1.knobs.clear();
        for (const auto& k : kList) {
            t1.knobs.push_back({k.name, k.label, k.value, k.display});
        }
    } else {
        t1.knobs = {
            {"algo", "ALGO", 0.15f, "Algo 5"},
            {"feedback", "FEEDBACK", 0.85f, "Lvl 6"},
            {"volume", "VOL", 0.85f, "85%"},
            {"chorus", "CHORUS", 0.35f, "35%"},
            {"drive", "DRIVE", 0.33f, "1.0x"},
            {"brightness", "BRIGHT", 0.33f, "1.0"}
        };
    }
    t1.audioFxList.push_back({"Chorus / Flanger", "chorus", 0.50f, 0.75f, true});
    tracks_.push_back(t1);
}

void TrackInspectorView::syncKnobsForTrack(InspectorTrackChannel& trk) {
    if (!trk.eatscriptCode.empty()) {
        try {
            auto def = eatsbits::project::PresetLoader::parseFromEatscript(trk.eatscriptCode);
            if (!def.params.empty()) {
                auto kList = extractKnobsFromPresetDefinition(def);
                trk.knobs.clear();
                for (const auto& k : kList) {
                    trk.knobs.push_back({k.name, k.label, k.value, k.display});
                }
                trk.customGuiPanel = createGuiPanelFromPresetDefinition(def);
                return;
            }
        } catch (...) {}
    }

    if (auto p = eatsbits::project::BuiltinPresetRegistry::createPresetById(trk.instrumentEngine)) {
        trk.eatscriptCode = p->rawScript;
        trk.customGuiPanel = createGuiPanelFromPresetDefinition(*p);
        auto kList = extractKnobsFromPresetDefinition(*p);
        trk.knobs.clear();
        for (const auto& k : kList) {
            trk.knobs.push_back({k.name, k.label, k.value, k.display});
        }
        return;
    }

    if (trk.instrumentEngine.rfind("clap:", 0) == 0 || trk.instrumentEngine == "CLAP" || trk.instrumentEngine == "clap") {
        trk.knobs = {
            {"cutoff", "CUTOFF", 0.50f, "1.2 kHz"},
            {"resonance", "RESONANCE", 0.30f, "30%"},
            {"attack", "ATTACK", 0.15f, "15 ms"},
            {"decay", "DECAY", 0.40f, "400 ms"},
            {"sustain", "SUSTAIN", 0.70f, "70%"},
            {"release", "RELEASE", 0.35f, "350 ms"}
        };
    } else if (trk.instrumentEngine == "tb303") {
        trk.knobs = {
            {"waveform", "WAVEFORM", 0.0f, "Saw"},
            {"pitch", "PITCH", 0.50f, "0 st"},
            {"cutoff", "CUTOFF", 0.65f, "1.8 kHz"},
            {"resonance", "RESONANCE", 0.80f, "80%"},
            {"envMod", "ENV MOD", 0.60f, "+2.4 oct"},
            {"decay", "DECAY", 0.45f, "240 ms"},
            {"accent", "ACCENT", 0.85f, "85%"},
            {"octave", "OCTAVE", 0.50f, "0"},
            {"subOsc", "SUB OSC", 0.0f, "Off"},
            {"subVol", "SUB VOL", 0.35f, "35%"},
            {"glideCurve", "GLIDE CURVE", 0.40f, "40%"},
            {"drive", "DRIVE", 0.25f, "25%"}
        };
    } else if (trk.instrumentEngine == "tr808") {
        trk.knobs = {
            {"tone", "TONE", 0.70f, "70%"},
            {"snappy", "SNAPPY", 0.80f, "80%"},
            {"decay", "DECAY", 0.60f, "600 ms"},
            {"tuning", "TUNING", 0.50f, "55 Hz"},
            {"drive", "DRIVE", 0.35f, "15%"},
            {"punch", "PUNCH", 0.75f, "+3 dB"}
        };
    } else if (trk.instrumentEngine == "tr909") {
        trk.knobs = {
            {"attack", "ATTACK", 0.30f, "12 ms"},
            {"punch", "PUNCH", 0.85f, "+4 dB"},
            {"tune", "TUNE", 0.55f, "62 Hz"},
            {"crack", "CRACK", 0.60f, "60%"},
            {"decay", "DECAY", 0.50f, "450 ms"},
            {"snap", "SNAP", 0.75f, "75%"}
        };
    } else if (trk.instrumentEngine == "dx7") {
        trk.knobs = {
            {"algo", "ALGO", 0.35f, "Algo 5"},
            {"feedback", "FEEDBACK", 0.65f, "Lvl 6"},
            {"attack", "ATTACK", 0.20f, "8 ms"},
            {"decay", "DECAY", 0.75f, "1.4 s"},
            {"bright", "BRIGHT", 0.60f, "60%"},
            {"detune", "DETUNE", 0.40f, "+12 ct"}
        };
    } else if (trk.instrumentEngine == "piano" || trk.instrumentEngine == "piano_physical") {
        trk.knobs = {
            {"stiffness", "STIFF", 0.50f, "50%"},
            {"hammer", "HAMMER", 0.65f, "Hard"},
            {"decay", "DECAY", 0.70f, "2.2 s"},
            {"damping", "DAMP", 0.30f, "30%"},
            {"pedal", "PEDAL", 0.0f, "Off"},
            {"reverb", "REVERB", 0.40f, "40%"}
        };
    } else if (trk.instrumentEngine == "sfxr" || trk.instrument.find("sfxr") != std::string::npos) {
        trk.knobs = {
            {"SFXType", "SFX TYPE", 0.0f, "Laser"},
            {"Seed", "RNG SEED", 42.0f, "42"},
            {"Waveform", "WAVETABLE", 0.0f, "Square"},
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
    } else if (trk.instrumentEngine == "snes" || trk.instrumentEngine == "snes_synth" || trk.instrumentEngine == "snes_console_synth" || trk.instrument.find("snes") != std::string::npos) {
        trk.knobs = {
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
    } else if (trk.instrumentEngine == "ambient_pad" || trk.instrumentEngine == "astral_pad" || trk.instrumentEngine == "astral_shimmer_pad" ||
               trk.instrument.find("astral") != std::string::npos || trk.instrument.find("Astral") != std::string::npos ||
               trk.name.find("Astral") != std::string::npos || trk.name.find("astral") != std::string::npos ||
               trk.instrument.find("shimmer") != std::string::npos || trk.name.find("Shimmer") != std::string::npos) {
        trk.knobs = {
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
        project::PresetDefinition def{};
        bool loadedDef = false;
        if (!trk.instrumentEngine.empty() && project::PresetManager::instance().loadPresetDefinition(trk.instrumentEngine, def)) {
            loadedDef = true;
        } else if (!trk.instrument.empty() && project::PresetManager::instance().loadPresetDefinition(trk.instrument, def)) {
            loadedDef = true;
        } else {
            auto opt = project::BuiltinPresetRegistry::createPresetById(trk.instrumentEngine);
            if (!opt) opt = project::BuiltinPresetRegistry::createPresetById(trk.instrument);
            if (opt) {
                def = *opt;
                loadedDef = true;
            }
        }
        if (loadedDef) {
            auto kList = extractKnobsFromPresetDefinition(def);
            trk.knobs.clear();
            for (const auto& k : kList) {
                trk.knobs.push_back({k.name, k.label, k.value, k.display});
            }
            trk.customGuiPanel = createGuiPanelFromPresetDefinition(def);
            if (trk.eatscriptCode.empty()) {
                trk.eatscriptCode = def.rawScript;
            }
            return;
        }

        trk.knobs = {
            {"param1", "PARAM 1", 0.50f, "50%"},
            {"param2", "PARAM 2", 0.60f, "60%"},
            {"param3", "PARAM 3", 0.30f, "30%"},
            {"param4", "PARAM 4", 0.55f, "55%"},
            {"param5", "COLOR", 0.40f, "40%"},
            {"param6", "OUTPUT", 0.80f, "80%"}
        };
    }
}

TrackPropertiesDrawerData TrackInspectorView::buildDrawerData() const {
    TrackPropertiesDrawerData d;
    d.tab = TrackPropertiesTab::Track;
    d.isMixerMode = false;
    d.isMasterSelected = false;
    d.trackIndex = selectedTrackIndex_;
    d.totalTracks = static_cast<uint32_t>(tracks_.size());

    for (const auto& trk : tracks_) {
        d.allTrackNames.push_back(trk.name);
        d.allTrackColors.push_back(Color(trk.r, trk.g, trk.b));
    }

    if (selectedTrackIndex_ < tracks_.size()) {
        const auto& trk = tracks_[selectedTrackIndex_];
        d.trackName = trk.name;
        d.trackType = trk.type;
        d.r = trk.r; d.g = trk.g; d.b = trk.b;
        d.volume = trk.volume;
        d.pan = trk.pan;
        d.mute = trk.mute;
        d.solo = trk.solo;
        d.freeze = trk.freeze;
        d.instrument = trk.instrument;
        d.instrumentEngine = trk.instrumentEngine;
        d.chordFollowMode = trk.chordFollowMode;
        d.knobs = trk.knobs;
        d.midiFxData = trk.midiFx;
        d.audioFxData = trk.audioFx;
        d.midiFx = trk.midiFxList;
        d.audioFx = trk.audioFxList;
        d.customGuiPanel = trk.customGuiPanel;
        d.eatscriptCode = trk.eatscriptCode;
        d.presetTitle = trk.instrument.empty() ? trk.name : trk.instrument;
        d.presetSubtitle = trk.name + " • " + (trk.instrument.empty() ? "Track" : trk.instrument);
    } else {
        d.presetTitle = presetTitle_;
        d.presetSubtitle = presetSubtitle_;
    }

    d.activePresetIdx = activePresetIdx_;
    d.totalPresets = totalPresets_;
    d.scopeBuffer = scopeSpan_.empty() ? scopeBuffer_ : scopeSpan_.data();
    d.scopeBufferCount = scopeSpan_.empty() ? scopeBufferCount_ : scopeSpan_.size();

    if (onPopulateClapData) {
        onPopulateClapData(selectedTrackIndex_, d);
    }

    return d;
}

void TrackInspectorView::updateDrawerData() {
    drawerData_.trackIndex = selectedTrackIndex_;
    drawerData_.totalTracks = static_cast<uint32_t>(tracks_.size());

    if (drawerData_.allTrackNames.size() != tracks_.size()) {
        drawerData_.allTrackNames.resize(tracks_.size());
        drawerData_.allTrackColors.resize(tracks_.size());
    }
    for (size_t i = 0; i < tracks_.size(); ++i) {
        if (drawerData_.allTrackNames[i] != tracks_[i].name) {
            drawerData_.allTrackNames[i] = tracks_[i].name;
        }
        drawerData_.allTrackColors[i] = Color(tracks_[i].r, tracks_[i].g, tracks_[i].b);
    }

    if (selectedTrackIndex_ < tracks_.size()) {
        const auto& trk = tracks_[selectedTrackIndex_];
        if (drawerData_.trackName != trk.name) drawerData_.trackName = trk.name;
        if (drawerData_.trackType != trk.type) drawerData_.trackType = trk.type;
        drawerData_.r = trk.r; drawerData_.g = trk.g; drawerData_.b = trk.b;
        drawerData_.volume = trk.volume;
        drawerData_.pan = trk.pan;
        drawerData_.mute = trk.mute;
        drawerData_.solo = trk.solo;
        drawerData_.freeze = trk.freeze;
        if (drawerData_.instrument != trk.instrument) drawerData_.instrument = trk.instrument;
        if (drawerData_.instrumentEngine != trk.instrumentEngine) drawerData_.instrumentEngine = trk.instrumentEngine;
        drawerData_.chordFollowMode = trk.chordFollowMode;
        if (drawerData_.knobs.size() != trk.knobs.size()) {
            drawerData_.knobs = trk.knobs;
        } else {
            for (size_t k = 0; k < trk.knobs.size(); ++k) {
                drawerData_.knobs[k].value = trk.knobs[k].value;
                if (drawerData_.knobs[k].display != trk.knobs[k].display) {
                    drawerData_.knobs[k].display = trk.knobs[k].display;
                }
            }
        }
        drawerData_.midiFxData = trk.midiFx;
        drawerData_.audioFxData = trk.audioFx;
        if (drawerData_.midiFx.size() != trk.midiFxList.size()) {
            drawerData_.midiFx = trk.midiFxList;
        }
        if (drawerData_.audioFx.size() != trk.audioFxList.size()) {
            drawerData_.audioFx = trk.audioFxList;
        }
        drawerData_.customGuiPanel = trk.customGuiPanel;
        drawerData_.eatscriptCode = trk.eatscriptCode;
        std::string_view desiredTitle = trk.instrument.empty() ? std::string_view(trk.name) : std::string_view(trk.instrument);
        if (drawerData_.presetTitle != desiredTitle) {
            drawerData_.presetTitle = desiredTitle;
        }

        std::string_view trkInst = trk.instrument.empty() ? std::string_view("Track") : std::string_view(trk.instrument);
        std::string_view sep = " • ";
        size_t neededLen = trk.name.size() + sep.size() + trkInst.size();
        bool matches = (drawerData_.presetSubtitle.size() == neededLen) &&
                       (drawerData_.presetSubtitle.compare(0, trk.name.size(), trk.name) == 0) &&
                       (drawerData_.presetSubtitle.compare(trk.name.size(), sep.size(), sep) == 0) &&
                       (drawerData_.presetSubtitle.compare(trk.name.size() + sep.size(), trkInst.size(), trkInst) == 0);
        if (!matches) {
            drawerData_.presetSubtitle.clear();
            drawerData_.presetSubtitle.reserve(neededLen);
            drawerData_.presetSubtitle.append(trk.name);
            drawerData_.presetSubtitle.append(sep);
            drawerData_.presetSubtitle.append(trkInst);
        }
    } else {
        if (drawerData_.presetTitle != presetTitle_) drawerData_.presetTitle = presetTitle_;
        if (drawerData_.presetSubtitle != presetSubtitle_) drawerData_.presetSubtitle = presetSubtitle_;
    }

    drawerData_.activePresetIdx = activePresetIdx_;
    drawerData_.totalPresets = totalPresets_;
    drawerData_.scopeBuffer = scopeSpan_.empty() ? scopeBuffer_ : scopeSpan_.data();
    drawerData_.scopeBufferCount = scopeSpan_.empty() ? scopeBufferCount_ : scopeSpan_.size();

    if (onPopulateClapData) {
        onPopulateClapData(selectedTrackIndex_, drawerData_);
    }
}

void TrackInspectorView::syncBackFromDrawerData(const TrackPropertiesDrawerData& d) {
    if (selectedTrackIndex_ < tracks_.size()) {
        auto& trk = tracks_[selectedTrackIndex_];
        trk.volume = d.volume;
        trk.pan = d.pan;
        trk.mute = d.mute;
        trk.solo = d.solo;
        trk.freeze = d.freeze;
        trk.r = d.r; trk.g = d.g; trk.b = d.b;
        trk.chordFollowMode = d.chordFollowMode;
        trk.knobs = d.knobs;
        trk.midiFx = d.midiFxData;
        trk.audioFx = d.audioFxData;
        trk.midiFxList = d.midiFx;
        trk.audioFxList = d.audioFx;
        if (d.customGuiPanel.has_value()) {
            trk.customGuiPanel = d.customGuiPanel;
        }
        if (!d.eatscriptCode.empty()) {
            trk.eatscriptCode = d.eatscriptCode;
        }
    }
}

void TrackInspectorView::setActiveTrack(uint32_t idx) noexcept {
    if (idx < tracks_.size()) {
        if (selectedTrackIndex_ != idx) {
            selectedTrackIndex_ = idx;
            std::string_view title = tracks_[idx].instrument.empty() ? std::string_view(tracks_[idx].name) : std::string_view(tracks_[idx].instrument);
            if (presetTitle_ != title) presetTitle_ = title;
            std::string_view trkInst = tracks_[idx].instrument.empty() ? std::string_view("Track") : std::string_view(tracks_[idx].instrument);
            std::string_view sep = " • ";
            size_t neededLen = tracks_[idx].name.size() + sep.size() + trkInst.size();
            presetSubtitle_.clear();
            presetSubtitle_.reserve(neededLen);
            presetSubtitle_.append(tracks_[idx].name);
            presetSubtitle_.append(sep);
            presetSubtitle_.append(trkInst);
        }
    }
}

const InspectorTrackChannel& TrackInspectorView::getTrack(size_t idx) const {
    static const InspectorTrackChannel kFallback;
    if (idx < tracks_.size()) return tracks_[idx];
    return kFallback;
}

InspectorTrackChannel& TrackInspectorView::getTrack(size_t idx) {
    if (idx < tracks_.size()) return tracks_[idx];
    return tracks_[0];
}

void TrackInspectorView::setAudioScopeBuffer(const float* buffer, size_t count) {
    if (!buffer || count == 0) {
        scopeSpan_ = {};
        return;
    }
    scopeSpan_ = std::span<const float>(buffer, count);
}

void TrackInspectorView::setAudioScopeBuffer(std::span<const float> buffer) {
    scopeSpan_ = buffer;
}

void TrackInspectorView::setPresetInfo(size_t activePresetIdx, size_t totalPresets,
                                      const std::string& presetTitle, const std::string& presetSubtitle) {
    activePresetIdx_ = activePresetIdx;
    totalPresets_ = totalPresets;
    if (presetTitle_ != presetTitle) presetTitle_ = presetTitle;
    if (presetSubtitle_ != presetSubtitle) presetSubtitle_ = presetSubtitle;
}

void TrackInspectorView::setPresetInfo(size_t activePresetIdx, size_t totalPresets,
                                      std::string_view presetTitle, std::string_view presetSubtitle) {
    activePresetIdx_ = activePresetIdx;
    totalPresets_ = totalPresets;
    if (presetTitle_ != presetTitle) presetTitle_ = presetTitle;
    if (presetSubtitle_ != presetSubtitle) presetSubtitle_ = presetSubtitle;
}

void TrackInspectorView::syncFromDrawerData(const TrackPropertiesDrawerData& data) {
    selectedTrackIndex_ = data.trackIndex;
    drawerData_ = data;
    drawerData_.tab = TrackPropertiesTab::Track;
    drawerData_.scopeBuffer = scopeSpan_.empty() ? scopeBuffer_ : scopeSpan_.data();
    drawerData_.scopeBufferCount = scopeSpan_.empty() ? scopeBufferCount_ : scopeSpan_.size();
    if (onPopulateClapData) {
        onPopulateClapData(selectedTrackIndex_, drawerData_);
    }
}

void TrackInspectorView::syncFromWindow(
    std::span<const ArrangerTrackData> tracks,
    uint32_t selectedTrackIdx,
    std::string_view activePresetTitle,
    std::string_view activePresetSubtitle,
    size_t activePresetIdx,
    size_t totalPresets,
    float scrollY
) {
    setActiveTrack(selectedTrackIdx);
    setPresetInfo(activePresetIdx, totalPresets, activePresetTitle, activePresetSubtitle);
    setScrollY(scrollY);

    size_t count = tracks.size();
    while (tracks_.size() < count) {
        InspectorTrackChannel nt;
        nt.name = "Track " + std::to_string(tracks_.size() + 1);
        syncKnobsForTrack(nt);
        tracks_.push_back(std::move(nt));
    }
    if (tracks_.size() > count) {
        tracks_.resize(count);
    }
    if (selectedTrackIndex_ >= tracks_.size() && !tracks_.empty()) {
        selectedTrackIndex_ = static_cast<uint32_t>(tracks_.size() - 1);
    }

    for (size_t i = 0; i < count; ++i) {
        const auto& src = tracks[i];
        auto& trk = tracks_[i];
        if (trk.name != src.name) trk.name = src.name;
        trk.volume = src.volume;
        trk.pan = src.pan;
        trk.mute = src.mute;
        trk.solo = src.solo;
        trk.freeze = src.freeze;
        trk.r = src.r;
        trk.g = src.g;
        trk.b = src.b;
        trk.chordFollowMode = src.chordFollowMode;

        if (trk.instrument != src.instrument && !src.instrument.empty()) {
            trk.instrument = src.instrument;
        }
        if (!src.instrumentEngine.empty() && trk.instrumentEngine != src.instrumentEngine) {
            trk.instrumentEngine = src.instrumentEngine;
            syncKnobsForTrack(trk);
        }
        if (src.customGuiPanel.has_value()) {
            trk.customGuiPanel = src.customGuiPanel;
        }
        if (!src.eatscriptCode.empty()) {
            trk.eatscriptCode = src.eatscriptCode;
        }
    }
}

void TrackInspectorView::syncFromWindow(
    const std::vector<std::string>& trackNames,
    const std::vector<float>& trackVols,
    const std::vector<float>& trackPans,
    const std::vector<bool>& trackMutes,
    const std::vector<bool>& trackSolos,
    const std::vector<bool>& trackFreezes,
    const std::vector<Color>& trackColors,
    uint32_t selectedTrackIdx,
    const std::string& activePresetTitle,
    const std::string& activePresetSubtitle,
    size_t activePresetIdx,
    size_t totalPresets,
    float scrollY,
    const std::vector<std::string>& trackInstruments,
    const std::vector<std::string>& trackInstrumentEngines,
    const std::vector<theory::ChordFollowMode>& trackChordFollowModes
) {
    setActiveTrack(selectedTrackIdx);
    setPresetInfo(activePresetIdx, totalPresets, activePresetTitle, activePresetSubtitle);
    setScrollY(scrollY);

    size_t count = std::min({
        trackNames.size(), trackVols.size(), trackPans.size(),
        trackMutes.size(), trackSolos.size(), trackFreezes.size(), trackColors.size()
    });

    while (tracks_.size() < count) {
        InspectorTrackChannel nt;
        nt.name = "Track " + std::to_string(tracks_.size() + 1);
        syncKnobsForTrack(nt);
        tracks_.push_back(nt);
    }
    if (tracks_.size() > count) {
        tracks_.resize(count);
    }
    if (selectedTrackIndex_ >= tracks_.size() && !tracks_.empty()) {
        selectedTrackIndex_ = static_cast<uint32_t>(tracks_.size() - 1);
    }

    for (size_t i = 0; i < count; ++i) {
        auto& trk = tracks_[i];
        trk.name = trackNames[i];
        trk.volume = trackVols[i];
        trk.pan = trackPans[i];
        trk.mute = trackMutes[i];
        trk.solo = trackSolos[i];
        trk.freeze = trackFreezes[i];
        trk.r = trackColors[i].r;
        trk.g = trackColors[i].g;
        trk.b = trackColors[i].b;

        if (i < trackChordFollowModes.size()) {
            trk.chordFollowMode = trackChordFollowModes[i];
        }

        if (i < trackInstruments.size() && !trackInstruments[i].empty()) {
            trk.instrument = trackInstruments[i];
        }
        if (i < trackInstrumentEngines.size() && !trackInstrumentEngines[i].empty()) {
            if (trk.instrumentEngine != trackInstrumentEngines[i]) {
                trk.instrumentEngine = trackInstrumentEngines[i];
                syncKnobsForTrack(trk);
            }
        }
    }
}

void TrackInspectorView::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;
    panel_.setShowTrackRibbon(false);
    panel_.layout(bounds, ctx);
}

void TrackInspectorView::render(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // Full container background fill
    drawRect(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h,
             theme.backgroundDark.r, theme.backgroundDark.g, theme.backgroundDark.b, 1.0f);

    if (drawerData_.totalTracks == 0 || drawerData_.instrumentEngine.empty()) {
        updateDrawerData();
    }
    panel_.render(r, theme, drawerData_, ctx.mouseX, ctx.mouseY);
}

bool TrackInspectorView::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    if (drawerData_.totalTracks == 0 || drawerData_.instrumentEngine.empty()) {
        updateDrawerData();
    }
    bool handled = panel_.handlePointer(ev, drawerData_, ctx);
    syncBackFromDrawerData(drawerData_);
    return handled;
}

bool TrackInspectorView::handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
    return panel_.handleKey(key, scancode, action, mods, ctx);
}

bool TrackInspectorView::handleChar(char32_t codepoint, [[maybe_unused]] const ViewContext& ctx) {
    return panel_.handleChar(codepoint);
}

} // namespace eatsbits::ui

