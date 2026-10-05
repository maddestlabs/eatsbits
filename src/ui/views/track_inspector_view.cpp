#include "eatsbits/ui/views/track_inspector_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::ui {

TrackInspectorView::TrackInspectorView() {
    initDefaultTracks();
    panel_.setShowTrackRibbon(true);

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
        if (onParamChanged) onParamChanged(idx, paramName, normVal);
    };
    panel_.onMidiFxParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
        if (onMidiFxParamChanged) onMidiFxParamChanged(idx, paramName, normVal);
    };
    panel_.onAudioFxParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
        if (onAudioFxParamChanged) onAudioFxParamChanged(idx, paramName, normVal);
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

    // Configure embedded PluginSearchDialog callback to sync track identity
    panel_.getPluginSearchDialog().onPluginSelected = [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t trackIndex) {
        if (trackIndex < tracks_.size()) {
            auto& trk = tracks_[trackIndex];
            if (mode == PluginDialogMode::AddInstrument) {
                trk.instrument = entry.name;
                trk.instrumentEngine = entry.engineTag;
                trk.r = entry.r;
                trk.g = entry.g;
                trk.b = entry.b;
                syncKnobsForTrack(trk);
                if (onChangeInstrument) onChangeInstrument(trackIndex);
            } else if (mode == PluginDialogMode::AddMidiFx) {
                TrackMidiFxItem mfx;
                mfx.id = entry.id;
                mfx.name = entry.name;
                mfx.type = entry.engineTag.empty() ? entry.name : entry.engineTag;
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

    // 1. 303 Acid Bass
    InspectorTrackChannel t1;
    t1.name = "303 Acid Bass";
    t1.type = "SYNTH";
    t1.r = 1.0f; t1.g = 0.55f; t1.b = 0.0f;
    t1.volume = 0.85f;
    t1.pan = -0.10f;
    t1.mute = false;
    t1.solo = false;
    t1.freeze = false;
    t1.instrument = "Eats-303 Acid Bassline";
    t1.instrumentEngine = "tb303";
    t1.chordFollowMode = ChordFollowMode::Off;
    t1.knobs = {
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
    t1.midiFx.arpEnabled = true;
    t1.midiFx.arpPattern = 2; // UpDown
    t1.midiFx.scaleSnapEnabled = true;
    t1.midiFx.humanizeEnabled = true;
    t1.audioFx.delayEnabled = true;
    t1.audioFx.delayTime = 0.375f;
    t1.audioFx.convolverEnabled = true;
    t1.midiFxList.push_back({"Arpeggiator Pro", "arp", true});
    t1.midiFxList.push_back({"Scale Snap", "scale", true});
    t1.audioFxList.push_back({"Tape Echo Delay", "delay", 0.45f, 0.30f, true});
    t1.audioFxList.push_back({"Stone Cathedral Reverb", "convolver", 0.60f, 0.35f, true});
    tracks_.push_back(t1);

    // 2. TR-808 Kit
    InspectorTrackChannel t2;
    t2.name = "TR-808 Kit";
    t2.type = "DRUMS";
    t2.r = 0.13f; t2.g = 0.96f; t2.b = 0.91f;
    t2.volume = 0.90f;
    t2.pan = 0.0f;
    t2.instrument = "Analog 808 Rhythm";
    t2.instrumentEngine = "tr808";
    t2.knobs = {
        {"tone", "TONE", 0.70f, "70%"},
        {"snappy", "SNAPPY", 0.80f, "80%"},
        {"decay", "DECAY", 0.60f, "600 ms"},
        {"tuning", "TUNING", 0.50f, "55 Hz"},
        {"drive", "DRIVE", 0.35f, "15%"},
        {"punch", "PUNCH", 0.75f, "+3 dB"}
    };
    tracks_.push_back(t2);

    // 3. TR-909 Drive
    InspectorTrackChannel t3;
    t3.name = "TR-909 Drive";
    t3.type = "DRUMS";
    t3.r = 1.0f; t3.g = 0.20f; t3.b = 0.20f;
    t3.volume = 0.88f;
    t3.pan = 0.05f;
    t3.instrument = "TR-909 Rhythm Composer";
    t3.instrumentEngine = "tr909";
    t3.knobs = {
        {"attack", "ATTACK", 0.30f, "12 ms"},
        {"punch", "PUNCH", 0.85f, "+4 dB"},
        {"tune", "TUNE", 0.55f, "62 Hz"},
        {"crack", "CRACK", 0.60f, "60%"},
        {"decay", "DECAY", 0.50f, "450 ms"},
        {"snap", "SNAP", 0.75f, "75%"}
    };
    tracks_.push_back(t3);

    // 4. DX7 Rhodes
    InspectorTrackChannel t4;
    t4.name = "DX7 Rhodes";
    t4.type = "KEYS";
    t4.r = 0.74f; t4.g = 0.0f; t4.b = 1.0f;
    t4.volume = 0.75f;
    t4.pan = 0.15f;
    t4.instrument = "Yamaha DX7 FM";
    t4.instrumentEngine = "dx7";
    t4.knobs = {
        {"algo", "ALGO", 0.35f, "Algo 5"},
        {"feedback", "FEEDBACK", 0.65f, "Lvl 6"},
        {"attack", "ATTACK", 0.20f, "8 ms"},
        {"decay", "DECAY", 0.75f, "1.4 s"},
        {"bright", "BRIGHT", 0.60f, "60%"},
        {"detune", "DETUNE", 0.40f, "+12 ct"}
    };
    tracks_.push_back(t4);

    // 5. Concert Grand
    InspectorTrackChannel t5;
    t5.name = "Concert Grand";
    t5.type = "PIANO";
    t5.r = 0.20f; t5.g = 0.60f; t5.b = 1.0f;
    t5.volume = 0.80f;
    t5.pan = -0.05f;
    t5.instrument = "Physical Modeling Piano";
    t5.instrumentEngine = "piano";
    t5.knobs = {
        {"stiffness", "STIFF", 0.50f, "50%"},
        {"hammer", "HAMMER", 0.65f, "Hard"},
        {"decay", "DECAY", 0.70f, "2.2 s"},
        {"damping", "DAMP", 0.30f, "30%"},
        {"pedal", "PEDAL", 0.0f, "Off"},
        {"reverb", "REVERB", 0.40f, "40%"}
    };
    tracks_.push_back(t5);
}

void TrackInspectorView::syncKnobsForTrack(InspectorTrackChannel& trk) {
    if (trk.instrumentEngine == "tb303") {
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
    } else {
        trk.knobs = {
            {"param1", "TONE", 0.50f, "50%"},
            {"param2", "TIMBRE", 0.60f, "60%"},
            {"param3", "ATTACK", 0.30f, "30%"},
            {"param4", "DECAY", 0.55f, "55%"},
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
        d.presetTitle = trk.instrument.empty() ? trk.name : trk.instrument;
        d.presetSubtitle = trk.name + " • " + (trk.instrument.empty() ? "Track" : trk.instrument);
    } else {
        d.presetTitle = presetTitle_;
        d.presetSubtitle = presetSubtitle_;
    }

    d.activePresetIdx = activePresetIdx_;
    d.totalPresets = totalPresets_;
    d.scopeBuffer = scopeBuffer_;
    d.scopeBufferCount = scopeBufferCount_;
    return d;
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
    }
}

void TrackInspectorView::setActiveTrack(uint32_t idx) noexcept {
    if (idx < tracks_.size()) {
        selectedTrackIndex_ = idx;
        presetTitle_ = tracks_[idx].instrument.empty() ? tracks_[idx].name : tracks_[idx].instrument;
        presetSubtitle_ = tracks_[idx].name + " • " + (tracks_[idx].instrument.empty() ? "Track" : tracks_[idx].instrument);
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
    if (!buffer || count == 0) return;
    size_t copyCount = std::min(count, size_t{256});
    std::copy(buffer, buffer + copyCount, scopeBuffer_);
    scopeBufferCount_ = copyCount;
}

void TrackInspectorView::setPresetInfo(size_t activePresetIdx, size_t totalPresets,
                                      const std::string& presetTitle, const std::string& presetSubtitle) {
    activePresetIdx_ = activePresetIdx;
    totalPresets_ = totalPresets;
    presetTitle_ = presetTitle;
    presetSubtitle_ = presetSubtitle;
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
    float scrollY
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
    }
}

void TrackInspectorView::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;
    panel_.setShowTrackRibbon(true);
    panel_.layout(bounds, ctx);
}

void TrackInspectorView::render(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // Full container background fill
    drawRect(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h,
             theme.backgroundDark.r, theme.backgroundDark.g, theme.backgroundDark.b, 1.0f);

    auto data = buildDrawerData();
    panel_.render(r, theme, data, ctx.mouseX, ctx.mouseY);
}

bool TrackInspectorView::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    auto data = buildDrawerData();
    bool handled = panel_.handlePointer(ev, data, ctx);
    syncBackFromDrawerData(data);
    return handled;
}

bool TrackInspectorView::handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
    return panel_.handleKey(key, scancode, action, mods, ctx);
}

bool TrackInspectorView::handleChar(char32_t codepoint, [[maybe_unused]] const ViewContext& ctx) {
    return panel_.handleChar(codepoint);
}

} // namespace eatsbits::ui

