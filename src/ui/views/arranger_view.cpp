#include "eatsbits/ui/views/arranger_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace eatsbits::ui {

static const float kQuickPalette[8][3] = {
    {0.13f, 0.96f, 0.91f}, // Neon Cyan (0xFF21F4E8)
    {1.0f,  0.0f,  0.48f}, // Hot Pink (0xFFFF007A)
    {0.0f,  1.0f,  0.40f}, // Acid Green (0xFF00FF66)
    {0.74f, 0.0f,  1.0f }, // Electric Purple (0xFFBD00FF)
    {1.0f,  0.55f, 0.0f }, // Neon Amber (0xFFFF8C00)
    {1.0f,  0.90f, 0.0f }, // Yellow (0xFFFFE600)
    {1.0f,  0.20f, 0.20f}, // Crimson Red (0xFFFF3333)
    {0.20f, 0.60f, 1.0f }  // Sky Blue (0xFF3399FF)
};

ArrangerView::ArrangerView() {
    // 1. 303 Acid Bass
    ArrangerTimelineTrack t1;
    t1.name = "303 Acid Bass";
    t1.instrument = "Roland TB-303";
    t1.instrumentEngine = "tb303";
    t1.iconRef = "preset:inst_synth";
    t1.r = 1.0f; t1.g = 0.55f; t1.b = 0.0f;
    t1.knob1 = 0.65f; t1.knob1Name = "CUTOFF";
    t1.knob2 = 0.85f; t1.knob2Name = "RESONANCE";
    t1.knob3 = 0.45f; t1.knob3Name = "DECAY";
    t1.knob4 = 0.80f; t1.knob4Name = "ACCENT";

    ArrangerTimelineClip c1{"c1", "Acid Pattern A", 0, 1, 4, 1.0f, 0.55f, 0.0f, false, false, true, 1, 0, 1.0f, false};
    c1.notes = {
        {36, 0.0f, 0.2f, 0.95f}, {36, 0.5f, 0.2f, 0.70f}, {48, 0.75f, 0.2f, 1.00f},
        {39, 1.0f, 0.2f, 0.80f}, {41, 1.5f, 0.2f, 0.90f}, {43, 2.0f, 0.2f, 0.85f},
        {36, 2.5f, 0.2f, 0.92f}, {46, 3.0f, 0.2f, 0.95f}, {48, 3.5f, 0.2f, 1.00f}
    };
    t1.clips.push_back(c1);

    ArrangerTimelineClip c2{"c2", "Acid Pattern B", 0, 5, 4, 1.0f, 0.55f, 0.0f, false, false, true, 1, 0, 1.0f, false};
    c2.notes = {
        {36, 0.0f, 0.2f, 0.85f}, {48, 0.25f, 0.2f, 1.00f}, {36, 0.5f, 0.2f, 0.65f},
        {41, 1.0f, 0.2f, 0.90f}, {44, 1.5f, 0.2f, 0.85f}, {46, 2.0f, 0.3f, 0.95f},
        {48, 2.75f, 0.2f, 1.00f}, {39, 3.0f, 0.2f, 0.75f}, {41, 3.5f, 0.2f, 0.85f}
    };
    t1.clips.push_back(c2);

    t1.midiFx.push_back({"Scale Snap", "SCALE_SNAP", 0, 0, true});
    t1.audioFx.push_back({"Tube Distortion", "TUBE_DISTORTION", 0.65f, 0.80f, true});

    // 2. TR-808 Rhythm Kit
    ArrangerTimelineTrack t2;
    t2.name = "TR-808 Kit";
    t2.instrument = "Analog 808";
    t2.instrumentEngine = "tr808";
    t2.iconRef = "preset:drum_machine";
    t2.r = 0.13f; t2.g = 0.96f; t2.b = 0.91f;
    t2.knob1 = 0.70f; t2.knob1Name = "TONE";
    t2.knob2 = 0.80f; t2.knob2Name = "SNAPPY";
    t2.knob3 = 0.60f; t2.knob3Name = "DECAY";
    t2.knob4 = 0.50f; t2.knob4Name = "TUNING";

    ArrangerTimelineClip c3{"c3", "808 Beat 01", 1, 1, 8, 0.13f, 0.96f, 0.91f, false, false, true, 1, 0, 1.0f, false};
    c3.notes = {
        {36, 0.0f, 0.2f, 1.00f}, {42, 0.0f, 0.1f, 0.75f}, {42, 0.25f, 0.1f, 0.45f},
        {42, 0.5f, 0.1f, 0.70f}, {42, 0.75f, 0.1f, 0.40f}, {38, 1.0f, 0.2f, 0.95f},
        {42, 1.0f, 0.1f, 0.80f}, {42, 1.25f, 0.1f, 0.50f}, {42, 1.5f, 0.1f, 0.70f},
        {36, 1.75f, 0.2f, 0.85f}, {36, 2.0f, 0.2f, 1.00f}, {42, 2.0f, 0.1f, 0.85f},
        {42, 2.25f, 0.1f, 0.45f}, {36, 2.5f, 0.2f, 0.75f}, {42, 2.5f, 0.1f, 0.70f},
        {38, 3.0f, 0.2f, 1.00f}, {42, 3.0f, 0.1f, 0.80f}, {42, 3.5f, 0.1f, 0.75f},
        {46, 3.75f, 0.2f, 0.65f}
    };
    t2.clips.push_back(c3);
    t2.audioFx.push_back({"Studio Dynamics", "DYNAMICS_COMP", 0.70f, 0.90f, true});

    // 3. TR-909 Groove Kit
    ArrangerTimelineTrack t3;
    t3.name = "TR-909 Drive";
    t3.instrument = "Analog 909";
    t3.instrumentEngine = "tr909";
    t3.iconRef = "preset:drum_kick";
    t3.r = 1.0f; t3.g = 0.16f; t3.b = 0.43f;
    t3.knob1 = 0.85f; t3.knob1Name = "ATTACK";
    t3.knob2 = 0.65f; t3.knob2Name = "PUNCH";
    t3.knob3 = 0.50f; t3.knob3Name = "TUNE";
    t3.knob4 = 0.60f; t3.knob4Name = "CRACK";

    ArrangerTimelineClip c4{"c4", "909 Groove", 2, 5, 4, 1.0f, 0.16f, 0.43f, false, false, true, 1, 0, 1.0f, false};
    c4.notes = {
        {36, 0.0f, 0.2f, 1.00f}, {42, 0.125f, 0.1f, 0.70f}, {36, 0.5f, 0.2f, 0.90f},
        {42, 0.625f, 0.1f, 0.65f}, {38, 1.0f, 0.2f, 0.95f}, {36, 1.0f, 0.2f, 1.00f},
        {46, 1.25f, 0.2f, 0.75f}, {36, 1.5f, 0.2f, 0.85f}, {36, 2.0f, 0.2f, 1.00f},
        {42, 2.125f, 0.1f, 0.70f}, {36, 2.5f, 0.2f, 0.90f}, {38, 3.0f, 0.2f, 1.00f},
        {46, 3.25f, 0.2f, 0.80f}, {36, 3.5f, 0.2f, 0.90f}, {42, 3.75f, 0.1f, 0.60f}
    };
    t3.clips.push_back(c4);

    // 4. DX7 Rhodes
    ArrangerTimelineTrack t4;
    t4.name = "DX7 Rhodes";
    t4.instrument = "Yamaha DX7 6-Op FM";
    t4.instrumentEngine = "dx7";
    t4.iconRef = "preset:inst_piano";
    t4.r = 0.62f; t4.g = 0.31f; t4.b = 0.87f;
    t4.knob1 = 0.50f; t4.knob1Name = "BRIGHT";
    t4.knob2 = 0.60f; t4.knob2Name = "VEL_SENS";
    t4.knob3 = 0.70f; t4.knob3Name = "CHORUS";
    t4.knob4 = 0.40f; t4.knob4Name = "FEEDBACK";

    ArrangerTimelineClip c5{"c5", "Chords A", 3, 1, 8, 0.62f, 0.31f, 0.87f, false, false, false, 8, 0, 1.0f, false};
    c5.notes = {
        {60, 0.0f, 7.5f, 0.70f}, {63, 0.0f, 7.5f, 0.65f}, {67, 0.0f, 7.5f, 0.80f}, {70, 0.0f, 7.5f, 0.75f},
        {58, 8.0f, 7.5f, 0.75f}, {62, 8.0f, 7.5f, 0.70f}, {65, 8.0f, 7.5f, 0.80f}, {69, 8.0f, 7.5f, 0.85f},
        {56, 16.0f, 7.5f, 0.70f}, {60, 16.0f, 7.5f, 0.65f}, {63, 16.0f, 7.5f, 0.75f}, {67, 16.0f, 7.5f, 0.80f},
        {58, 24.0f, 7.5f, 0.80f}, {62, 24.0f, 7.5f, 0.75f}, {65, 24.0f, 7.5f, 0.85f}, {68, 24.0f, 7.5f, 0.90f}
    };
    t4.clips.push_back(c5);
    t4.midiFx.push_back({"Arpeggiator Pro", "ARP_PRO", 0, 0, true});
    t4.audioFx.push_back({"Chorus / Flanger", "CHORUS_FLANGER", 0.50f, 0.75f, true});

    // Sub Bass follows project Chord Track in Bass mode
    t2.chordFollowMode = theory::ChordFollowMode::Bass;

    // 5. Concert Grand Piano (Waveguide Physical Modeling)
    ArrangerTimelineTrack t5;
    t5.name = "Concert Grand";
    t5.instrument = "Waveguide Grand Piano";
    t5.instrumentEngine = "piano";
    t5.iconRef = "preset:inst_piano";
    t5.r = 0.88f; t5.g = 0.66f; t5.b = 0.43f;
    t5.knob1 = 0.55f; t5.knob1Name = "HARDNESS";
    t5.knob2 = 0.60f; t5.knob2Name = "COUPLING";
    t5.knob3 = 0.80f; t5.knob3Name = "DECAY";
    t5.knob4 = 0.30f; t5.knob4Name = "PEDAL";

    ArrangerTimelineClip c6{"c6", "Piano Solo", 4, 9, 8, 0.88f, 0.66f, 0.43f, false, false, false, 8, 0, 1.0f, false};
    c6.notes = {
        {60, 0.0f, 2.0f, 0.85f}, {64, 2.0f, 2.0f, 0.75f}, {67, 4.0f, 3.5f, 0.90f},
        {65, 8.0f, 1.5f, 0.70f}, {64, 10.0f, 1.5f, 0.65f}, {62, 12.0f, 3.5f, 0.80f},
        {67, 16.0f, 2.0f, 0.95f}, {71, 18.0f, 2.0f, 0.85f}, {72, 20.0f, 3.5f, 1.00f},
        {71, 24.0f, 1.5f, 0.75f}, {69, 26.0f, 1.5f, 0.70f}, {67, 28.0f, 3.5f, 0.85f}
    };
    t5.clips.push_back(c6);
    t5.audioFx.push_back({"Partitioned Convolver", "CONVOLVER_REVERB", 0.40f, 0.50f, true});

    tracks_ = {t1, t2, t3, t4, t5};
    refreshAllClipChords();

    // Connect Reusable PluginSearchDialog callbacks
    pluginDialog_.onPluginSelected = [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t targetIdx) {
        if (mode == PluginDialogMode::AddInstrument) {
            if (targetIdx >= tracks_.size()) {
                // New track
                addTrack(entry.name, entry.engineTag, entry.r, entry.g, entry.b);
            } else {
                // Change instrument
                tracks_[targetIdx].instrument = entry.name;
                tracks_[targetIdx].r = entry.r;
                tracks_[targetIdx].g = entry.g;
                tracks_[targetIdx].b = entry.b;
                for (auto& cl : tracks_[targetIdx].clips) {
                    cl.r = entry.r; cl.g = entry.g; cl.b = entry.b;
                }
            }
        } else if (mode == PluginDialogMode::AddMidiFx) {
            addMidiFxToTrack(targetIdx, entry.name, entry.id);
        } else if (mode == PluginDialogMode::AddAudioFx) {
            addAudioFxToTrack(targetIdx, entry.name, entry.id);
        }
    };

    // Connect Reusable CircleOfFifthsDialog callbacks (Eatsbeats parity)
    circleOfFifthsDialog_.onChordApplied = [this](const theory::ChordEvent& chord) {
        addOrUpdateChord(chord);
    };
    circleOfFifthsDialog_.onChordDeleted = [this](const std::string& chordId) {
        removeChord(chordId);
    };
    circleOfFifthsDialog_.onProgressionApplied = [this](const theory::ChordProgressionPreset& preset, uint32_t startBar) {
        applyChordProgressionPreset(preset, startBar);
    };
    circleOfFifthsDialog_.onAuditionChord = [this](const theory::ChordEvent& chord) {
        if (onAuditionChord) onAuditionChord(chord);
    };
    circleOfFifthsDialog_.onExtractFromActiveTrack = [this]() {
        extractChordsFromTrack(activeTrackIndex_);
    };
    circleOfFifthsDialog_.onExtractFromActiveClip = [this]() {
        if (selectedClipIndex_ >= 0) {
            extractChordsFromClip(activeTrackIndex_, static_cast<uint32_t>(selectedClipIndex_));
        } else {
            extractChordsFromTrack(activeTrackIndex_);
        }
    };

    // Connect Reusable IconSearchDialog callbacks
    iconDialog_.onIconSelected = [this](const std::string& iconRef, uint32_t targetIdx) {
        if (targetIdx < tracks_.size()) {
            tracks_[targetIdx].iconRef = iconRef;
            drawerData_.iconRef = iconRef;
            if (onTrackIconChanged) {
                onTrackIconChanged(targetIdx, iconRef);
            }
        }
    };

    // Initialize Default Chord Track progression (Pop Classic / Synthwave: C - G - Am - F)
    chordTrack_ = {
        {"chord_0",  0,  2.0f, 0, theory::ChordQuality::Major, -1},  // Bar 1-2: C Major
        {"chord_2",  2,  2.0f, 7, theory::ChordQuality::Major, -1},  // Bar 3-4: G Major
        {"chord_4",  4,  2.0f, 9, theory::ChordQuality::Minor, -1},  // Bar 5-6: A Minor
        {"chord_6",  6,  2.0f, 5, theory::ChordQuality::Major, -1},  // Bar 7-8: F Major
        {"chord_8",  8,  2.0f, 0, theory::ChordQuality::Major, -1},  // Bar 9-10: C Major
        {"chord_10", 10, 2.0f, 7, theory::ChordQuality::Major, -1},  // Bar 11-12: G Major
        {"chord_12", 12, 2.0f, 9, theory::ChordQuality::Minor, -1},  // Bar 13-14: A Minor
        {"chord_14", 14, 2.0f, 5, theory::ChordQuality::Major, -1}   // Bar 15-16: F Major
    };

    // Shared TrackPropertiesDrawer callbacks across Arranger and Mixer
    propertiesDrawer_.onVolumeChanged = [this](uint32_t trackIdx, float vol) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].volume = vol;
            if (onVolumeChanged) onVolumeChanged(trackIdx, vol);
        }
    };
    propertiesDrawer_.onPanChanged = [this](uint32_t trackIdx, float pan) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].pan = pan;
            if (onPanChanged) onPanChanged(trackIdx, pan);
        }
    };
    propertiesDrawer_.onMuteToggled = [this](uint32_t trackIdx, bool mute) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].mute = mute;
            if (onMuteToggled) onMuteToggled(trackIdx, mute);
        }
    };
    propertiesDrawer_.onSoloToggled = [this](uint32_t trackIdx, bool solo) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].solo = solo;
            if (onSoloToggled) onSoloToggled(trackIdx, solo);
        }
    };
    propertiesDrawer_.onColorChanged = [this](uint32_t trackIdx, float r, float g, float b) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].r = r;
            tracks_[trackIdx].g = g;
            tracks_[trackIdx].b = b;
            for (auto& cl : tracks_[trackIdx].clips) {
                cl.r = r; cl.g = g; cl.b = b;
            }
        }
    };
    propertiesDrawer_.onParamChanged = [this](uint32_t trackIdx, const std::string& paramName, float normVal) {
        if (trackIdx < tracks_.size()) {
            auto& trk = tracks_[trackIdx];
            if (paramName == "cutoff" || paramName == "tone" || paramName == "attack" || paramName == "param1") {
                trk.knob1 = normVal;
            } else if (paramName == "resonance" || paramName == "snappy" || paramName == "punch" || paramName == "decay" || paramName == "param2") {
                trk.knob2 = normVal;
            } else if (paramName == "decay" || paramName == "tune" || paramName == "bright" || paramName == "param3") {
                trk.knob3 = normVal;
            } else if (paramName == "accent" || paramName == "tuning" || paramName == "crack" || paramName == "detune" || paramName == "param4") {
                trk.knob4 = normVal;
            }
        }
        if (onParamChanged) {
            onParamChanged(trackIdx, paramName, normVal);
        }
    };
    propertiesDrawer_.onTabSelected = [this](TrackPropertiesTab tab) {
        inspectorTab_ = (tab == TrackPropertiesTab::Clip) ? ArrangerInspectorTab::Clip : ArrangerInspectorTab::Track;
        drawerData_.tab = tab;
    };
    propertiesDrawer_.onEditInPianoRoll = [this](uint32_t trackIdx, int clipIdx) {
        if (onEditClipInPianoRoll) {
            onEditClipInPianoRoll(trackIdx, clipIdx);
        }
    };
    propertiesDrawer_.getPanel().getPluginSearchDialog().onPluginSelected =
        [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t targetIdx) {
            if (mode == PluginDialogMode::AddInstrument) {
                if (targetIdx < tracks_.size()) {
                    tracks_[targetIdx].instrument = entry.name;
                    tracks_[targetIdx].instrumentEngine = entry.engineTag;
                    tracks_[targetIdx].r = entry.r;
                    tracks_[targetIdx].g = entry.g;
                    tracks_[targetIdx].b = entry.b;
                    drawerData_.instrument = entry.name;
                    drawerData_.instrumentEngine = entry.engineTag;
                    drawerData_.r = entry.r;
                    drawerData_.g = entry.g;
                    drawerData_.b = entry.b;
                }
            } else if (mode == PluginDialogMode::AddMidiFx) {
                if (targetIdx < tracks_.size()) {
                    TrackMidiFxItem mfx;
                    mfx.id = entry.id;
                    mfx.name = entry.name;
                    mfx.type = entry.engineTag.empty() ? entry.name : entry.engineTag;
                    mfx.enabled = true;
                    mfx.isExpanded = true;
                    mfx.ensureDefaultKnobs();
                    tracks_[targetIdx].midiFx.push_back(mfx);
                    drawerData_.midiFx.push_back(mfx);
                    if (onMidiFxChanged) onMidiFxChanged(targetIdx);
                }
            } else if (mode == PluginDialogMode::AddAudioFx) {
                if (targetIdx < tracks_.size()) {
                    TrackAudioFxItem afx;
                    afx.id = entry.id;
                    afx.name = entry.name;
                    afx.type = entry.engineTag.empty() ? entry.name : entry.engineTag;
                    afx.enabled = true;
                    afx.isExpanded = true;
                    afx.ensureDefaultKnobs();
                    tracks_[targetIdx].audioFx.push_back(afx);
                    drawerData_.audioFx.push_back(afx);
                    if (onAudioFxChanged) onAudioFxChanged(targetIdx);
                }
            }
        };
    propertiesDrawer_.onRemoveMidiFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (trackIdx < tracks_.size() && fxIdx < tracks_[trackIdx].midiFx.size()) {
            tracks_[trackIdx].midiFx.erase(tracks_[trackIdx].midiFx.begin() + fxIdx);
            if (onMidiFxChanged) onMidiFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onRemoveAudioFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (trackIdx < tracks_.size() && fxIdx < tracks_[trackIdx].audioFx.size()) {
            tracks_[trackIdx].audioFx.erase(tracks_[trackIdx].audioFx.begin() + fxIdx);
            if (onAudioFxChanged) onAudioFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onToggleMidiFx = [this](uint32_t trackIdx, size_t fxIdx, bool en) {
        if (trackIdx < tracks_.size() && fxIdx < tracks_[trackIdx].midiFx.size()) {
            tracks_[trackIdx].midiFx[fxIdx].enabled = en;
            if (onToggleMidiFx) onToggleMidiFx(trackIdx, fxIdx, en);
            if (onMidiFxChanged) onMidiFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onToggleAudioFx = [this](uint32_t trackIdx, size_t fxIdx, bool en) {
        if (trackIdx < tracks_.size() && fxIdx < tracks_[trackIdx].audioFx.size()) {
            tracks_[trackIdx].audioFx[fxIdx].enabled = en;
            if (onToggleAudioFx) onToggleAudioFx(trackIdx, fxIdx, en);
            if (onAudioFxChanged) onAudioFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onReorderAudioFx = [this](uint32_t trackIdx, size_t fromIdx, size_t toIdx) {
        if (trackIdx < tracks_.size()) {
            auto& list = tracks_[trackIdx].audioFx;
            if (fromIdx < list.size() && toIdx < list.size() && fromIdx != toIdx) {
                auto item = list[fromIdx];
                list.erase(list.begin() + fromIdx);
                list.insert(list.begin() + toIdx, item);
                drawerData_.audioFx = list;
                if (onAudioFxChanged) onAudioFxChanged(trackIdx);
            }
        }
    };
    propertiesDrawer_.onReorderMidiFx = [this](uint32_t trackIdx, size_t fromIdx, size_t toIdx) {
        if (trackIdx < tracks_.size()) {
            auto& list = tracks_[trackIdx].midiFx;
            if (fromIdx < list.size() && toIdx < list.size() && fromIdx != toIdx) {
                auto item = list[fromIdx];
                list.erase(list.begin() + fromIdx);
                list.insert(list.begin() + toIdx, item);
                drawerData_.midiFx = list;
                if (onMidiFxChanged) onMidiFxChanged(trackIdx);
            }
        }
    };
    propertiesDrawer_.onAudioFxChanged = [this](uint32_t trackIdx) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].audioFx = drawerData_.audioFx;
            if (onAudioFxChanged) onAudioFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onMidiFxChanged = [this](uint32_t trackIdx) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].midiFx = drawerData_.midiFx;
            if (onMidiFxChanged) onMidiFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onAudioFxParamChanged = [this](uint32_t trackIdx, const std::string& paramName, float normVal) {
        if (onAudioFxParamChanged) {
            onAudioFxParamChanged(trackIdx, 0, paramName, normVal);
        }
    };
    propertiesDrawer_.onChooseTrackIcon = [this](uint32_t trackIdx) {
        if (trackIdx < tracks_.size()) {
            iconDialog_.open(tracks_[trackIdx].name, trackIdx, tracks_[trackIdx].iconRef);
        }
    };
    propertiesDrawer_.onOpenPresets = [this](uint32_t trackIdx) {
        if (trackIdx < tracks_.size()) {
            pluginDialog_.open(PluginDialogMode::SelectPreset, tracks_[trackIdx].name, trackIdx);
        }
    };
    propertiesDrawer_.onOpenFullscreenDevice = [this](uint32_t trackIdx) {
        if (onOpenFullscreenDevice) onOpenFullscreenDevice(trackIdx);
    };
    propertiesDrawer_.onOpenFullscreenAudioFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (onOpenFullscreenAudioFx) onOpenFullscreenAudioFx(trackIdx, fxIdx);
    };
    propertiesDrawer_.onOpenFullscreenMidiFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (onOpenFullscreenMidiFx) onOpenFullscreenMidiFx(trackIdx, fxIdx);
    };
    propertiesDrawer_.onTrackRenameWithText = [this](uint32_t trackIdx, const std::string& newName) {
        if (trackIdx < tracks_.size() && !newName.empty()) {
            tracks_[trackIdx].name = newName;
            drawerData_.trackName = newName;
            if (trackIdx < drawerData_.allTrackNames.size()) {
                drawerData_.allTrackNames[trackIdx] = newName;
            }
            if (onTrackRename) {
                onTrackRename(trackIdx, newName);
            }
        }
    };
    propertiesDrawer_.onAddClip = [this](uint32_t trackIdx) {
        addClipToTrack(trackIdx);
        if (onAddClip) onAddClip(trackIdx);
    };
    propertiesDrawer_.onDeleteTrack = [this](uint32_t trackIdx) {
        if (onTrackDeleted) {
            onTrackDeleted(trackIdx);
        } else {
            deleteTrack(trackIdx);
        }
    };
    propertiesDrawer_.onDuplicateTrack = [this](uint32_t trackIdx) {
        if (onTrackDuplicated) {
            onTrackDuplicated(trackIdx);
        } else {
            duplicateTrack(trackIdx);
        }
    };
    propertiesDrawer_.onDuplicateClip = [this](uint32_t trackIdx, int clipIdx) {
        duplicateClip(trackIdx, clipIdx);
        if (onDuplicateClip) onDuplicateClip(trackIdx, clipIdx);
    };
    propertiesDrawer_.onDeleteClip = [this](uint32_t trackIdx, int clipIdx) {
        deleteClip(trackIdx, clipIdx);
        if (onDeleteClip) onDeleteClip(trackIdx, clipIdx);
    };

    // Docked ArrangerMixerDrawer callbacks
    mixerDrawer_.onTrackSelected = [this](uint32_t trackIdx) {
        setActiveTrack(trackIdx);
        if (onTrackSelected) onTrackSelected(trackIdx);
    };
    mixerDrawer_.onVolumeChanged = [this](uint32_t trackIdx, float vol) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].volume = vol;
            if (onVolumeChanged) onVolumeChanged(trackIdx, vol);
        }
    };
    mixerDrawer_.onPanChanged = [this](uint32_t trackIdx, float pan) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].pan = pan;
            if (onPanChanged) onPanChanged(trackIdx, pan);
        }
    };
    mixerDrawer_.onMuteToggled = [this](uint32_t trackIdx, bool mute) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].mute = mute;
            if (onMuteToggled) onMuteToggled(trackIdx, mute);
        }
    };
    mixerDrawer_.onSoloToggled = [this](uint32_t trackIdx, bool solo) {
        if (trackIdx < tracks_.size()) {
            tracks_[trackIdx].solo = solo;
            if (onSoloToggled) onSoloToggled(trackIdx, solo);
        }
    };
    mixerDrawer_.onMasterVolumeChanged = [this](float vol) {
        masterVolume_ = vol;
        if (onMasterVolumeChanged) onMasterVolumeChanged(vol);
    };
    mixerDrawer_.onMasterPanChanged = [this](float pan) {
        masterPan_ = pan;
        if (onMasterPanChanged) onMasterPanChanged(pan);
    };
    mixerDrawer_.onMasterMuteToggled = [this](bool mute) {
        masterMute_ = mute;
        if (onMasterMuteToggled) onMasterMuteToggled(mute);
    };
}

void ArrangerView::setMasterLevels(float masterVol, float masterPan, bool masterMute,
                                   float masterPeakL, float masterPeakR,
                                   const float* chPeaksL, const float* chPeaksR, size_t numPeaks) {
    masterVolume_ = masterVol;
    masterPan_ = masterPan;
    masterMute_ = masterMute;
    masterPeakL_ = masterPeakL;
    masterPeakR_ = masterPeakR;
    if (chPeaksL && chPeaksR && numPeaks > 0) {
        chPeaksL_.assign(chPeaksL, chPeaksL + numPeaks);
        chPeaksR_.assign(chPeaksR, chPeaksR + numPeaks);
    }
}

void ArrangerView::setActiveTrack(uint32_t idx) noexcept {
    if (idx < tracks_.size()) {
        activeTrackIndex_ = idx;
    }
}

void ArrangerView::setSelectedClip(int idx) noexcept {
    selectedClipIndex_ = idx;
}

void ArrangerView::addTrack(const std::string& name, const std::string& instrument, float r, float g, float b) {
    ArrangerTimelineTrack t;
    t.name = name;
    t.instrument = instrument;
    t.instrumentEngine = "synth";
    t.iconRef = "preset:inst_synth";
    t.r = r; t.g = g; t.b = b;
    // Clean track ready for audio or MIDI clips - no default clip inserted
    t.clips.clear();
    tracks_.push_back(t);
    activeTrackIndex_ = static_cast<uint32_t>(tracks_.size() - 1);
    selectedClipIndex_ = -1;
    inspectorTab_ = ArrangerInspectorTab::Track;
    inspectorOpen_ = true;
    if (onTrackAdded) {
        onTrackAdded(activeTrackIndex_);
    }
    if (onTrackSelected) {
        onTrackSelected(activeTrackIndex_);
    }
}

void ArrangerView::addClipToTrack(uint32_t trackIdx, uint32_t startBar, uint32_t lengthBars) {
    if (trackIdx >= tracks_.size()) return;
    auto& track = tracks_[trackIdx];
    if (startBar == 1 && !track.clips.empty()) {
        uint32_t endB = 1;
        for (const auto& c : track.clips) {
            endB = std::max(endB, c.startBar + c.lengthBars);
        }
        startBar = endB;
    }
    createEmptyClipAt(trackIdx, startBar, lengthBars);
}

void ArrangerView::createEmptyClipAt(uint32_t trackIdx, uint32_t startBar, uint32_t lengthBars) {
    if (trackIdx >= tracks_.size()) return;
    auto& track = tracks_[trackIdx];
    std::string cid = "clip_" + std::to_string(trackIdx + 1) + "_" + std::to_string(startBar);
    ArrangerTimelineClip cl;
    cl.id = cid;
    cl.name = track.name + " Clip " + std::to_string(track.clips.size() + 1);
    cl.trackIndex = trackIdx;
    cl.startBar = startBar;
    cl.lengthBars = lengthBars;
    cl.r = track.r;
    cl.g = track.g;
    cl.b = track.b;
    cl.isAudio = false;
    cl.isLooped = false;
    cl.loopLengthBars = lengthBars;
    cl.volumeScale = 1.0f;
    cl.mute = false;
    track.clips.push_back(cl);
    selectedClipIndex_ = static_cast<int>(track.clips.size() - 1);
    activeTrackIndex_ = trackIdx;
    inspectorTab_ = ArrangerInspectorTab::Clip;
    inspectorOpen_ = true;
    if (onClipsChanged) onClipsChanged();
    if (onTrackSelected) onTrackSelected(activeTrackIndex_);
}

void ArrangerView::duplicateClip(uint32_t trackIdx, int clipIdx) {
    if (trackIdx >= tracks_.size()) return;
    auto& track = tracks_[trackIdx];
    if (clipIdx < 0 || clipIdx >= static_cast<int>(track.clips.size())) return;

    const auto& src = track.clips[clipIdx];
    ArrangerTimelineClip dup = src;
    dup.id = "clip_" + std::to_string(trackIdx + 1) + "_" + std::to_string(track.clips.size() + 1);
    dup.name = src.name + " (Copy)";
    dup.startBar = src.startBar + src.lengthBars;
    track.clips.push_back(dup);

    activeTrackIndex_ = trackIdx;
    selectedClipIndex_ = static_cast<int>(track.clips.size() - 1);
    inspectorTab_ = ArrangerInspectorTab::Clip;
    inspectorOpen_ = true;
    if (onClipsChanged) onClipsChanged();
    if (onTrackSelected) onTrackSelected(activeTrackIndex_);
}

void ArrangerView::deleteClip(uint32_t trackIdx, int clipIdx) {
    if (trackIdx >= tracks_.size()) return;
    auto& track = tracks_[trackIdx];
    if (clipIdx < 0 || clipIdx >= static_cast<int>(track.clips.size())) return;

    track.clips.erase(track.clips.begin() + clipIdx);
    if (track.clips.empty()) {
        selectedClipIndex_ = -1;
        inspectorTab_ = ArrangerInspectorTab::Track;
    } else {
        selectedClipIndex_ = std::min(clipIdx, static_cast<int>(track.clips.size() - 1));
        inspectorTab_ = ArrangerInspectorTab::Clip;
    }
    activeTrackIndex_ = trackIdx;
    if (onClipsChanged) onClipsChanged();
    if (onTrackSelected) onTrackSelected(activeTrackIndex_);
}

void ArrangerView::deleteTrack(uint32_t trackIdx) {
    if (tracks_.size() <= 1 || trackIdx >= tracks_.size()) return;
    tracks_.erase(tracks_.begin() + trackIdx);
    if (activeTrackIndex_ >= tracks_.size()) {
        activeTrackIndex_ = static_cast<uint32_t>(tracks_.size() - 1);
    }
    for (size_t t = 0; t < tracks_.size(); ++t) {
        for (auto& cl : tracks_[t].clips) {
            cl.trackIndex = static_cast<uint32_t>(t);
        }
    }
    selectedClipIndex_ = -1;
    inspectorTab_ = ArrangerInspectorTab::Track;
    drawerData_.tab = TrackPropertiesTab::Track;
    drawerData_.selectedClipIndex = -1;
    drawerData_.totalTracks = static_cast<uint32_t>(tracks_.size());
    drawerData_.trackIndex = activeTrackIndex_;
    drawerData_.allTrackNames.clear();
    for (const auto& trk : tracks_) {
        drawerData_.allTrackNames.push_back(trk.name);
    }
    if (onTrackDeleted) onTrackDeleted(trackIdx);
    if (onClipsChanged) onClipsChanged();
    if (onTrackSelected) onTrackSelected(activeTrackIndex_);
}

void ArrangerView::duplicateTrack(uint32_t trackIdx) {
    if (trackIdx >= tracks_.size()) return;
    ArrangerTimelineTrack dup = tracks_[trackIdx];
    dup.name += " (Copy)";
    uint32_t newTrackIdx = static_cast<uint32_t>(tracks_.size());
    for (size_t i = 0; i < dup.clips.size(); ++i) {
        dup.clips[i].id = "clip_" + std::to_string(newTrackIdx + 1) + "_" + std::to_string(i + 1);
        dup.clips[i].trackIndex = newTrackIdx;
    }
    tracks_.push_back(dup);
    activeTrackIndex_ = newTrackIdx;
    selectedClipIndex_ = -1;
    drawerData_.totalTracks = static_cast<uint32_t>(tracks_.size());
    drawerData_.trackIndex = activeTrackIndex_;
    if (onTrackDuplicated) onTrackDuplicated(trackIdx);
    if (onTrackAdded) onTrackAdded(activeTrackIndex_);
    if (onClipsChanged) onClipsChanged();
    if (onTrackSelected) onTrackSelected(activeTrackIndex_);
}

void ArrangerView::addMidiFxToTrack(uint32_t trackIdx, const std::string& name, const std::string& type) {
    if (trackIdx < tracks_.size()) {
        tracks_[trackIdx].midiFx.push_back({name, type, 0, 0, true});
        if (onMidiFxChanged) onMidiFxChanged(trackIdx);
    }
}

void ArrangerView::addAudioFxToTrack(uint32_t trackIdx, const std::string& name, const std::string& type) {
    if (trackIdx < tracks_.size()) {
        tracks_[trackIdx].audioFx.push_back({name, type, 0.5f, 0.5f, true});
        if (onAudioFxChanged) onAudioFxChanged(trackIdx);
    }
}

// -------------------------------------------------------------------------
// Chord Track & Music Theory APIs
// -------------------------------------------------------------------------

void ArrangerView::addOrUpdateChord(const theory::ChordEvent& chord) {
    auto it = std::find_if(chordTrack_.begin(), chordTrack_.end(), [&](const theory::ChordEvent& c) {
        return c.id == chord.id;
    });

    if (it != chordTrack_.end()) {
        *it = chord;
    } else {
        // Remove any chord starting at the exact same bar to prevent exact overlap
        chordTrack_.erase(
            std::remove_if(chordTrack_.begin(), chordTrack_.end(), [&](const theory::ChordEvent& c) {
                return c.startBar == chord.startBar;
            }),
            chordTrack_.end()
        );
        chordTrack_.push_back(chord);
    }

    std::sort(chordTrack_.begin(), chordTrack_.end(), [](const theory::ChordEvent& a, const theory::ChordEvent& b) {
        return a.startBar < b.startBar;
    });
}

void ArrangerView::removeChord(const std::string& chordId) {
    chordTrack_.erase(
        std::remove_if(chordTrack_.begin(), chordTrack_.end(), [&](const theory::ChordEvent& c) {
            return c.id == chordId;
        }),
        chordTrack_.end()
    );
}

const theory::ChordEvent* ArrangerView::getActiveChordAtBar(float bar) const noexcept {
    for (const auto& chord : chordTrack_) {
        if (bar >= static_cast<float>(chord.startBar) && bar < static_cast<float>(chord.startBar) + chord.barLength) {
            return &chord;
        }
    }
    return nullptr;
}

const theory::ChordEvent* ArrangerView::getActiveChordAtStep(float step) const noexcept {
    return getActiveChordAtBar(step / 16.0f);
}

void ArrangerView::setSongKey(int rootPitchClass, bool isMinor) noexcept {
    songKeyRoot_ = (rootPitchClass % 12 + 12) % 12;
    isSongKeyMinor_ = isMinor;
}

std::string ArrangerView::getSongKeyName() const {
    return std::string(theory::ChordTheory::pitchClassNames[songKeyRoot_]) +
           (isSongKeyMinor_ ? " Minor" : " Major");
}

void ArrangerView::applyChordProgressionPreset(const theory::ChordProgressionPreset& preset, uint32_t startBar) {
    uint32_t curBar = startBar;
    for (const auto& chordDef : preset.chords) {
        int rootPc = (songKeyRoot_ + chordDef.rootOffset) % 12;

        // Clear overlapping
        chordTrack_.erase(
            std::remove_if(chordTrack_.begin(), chordTrack_.end(), [&](const theory::ChordEvent& c) {
                return c.startBar >= curBar && c.startBar < curBar + static_cast<uint32_t>(std::ceil(chordDef.barLength));
            }),
            chordTrack_.end()
        );

        theory::ChordEvent ev;
        ev.id = "chord_" + std::to_string(curBar) + "_" + std::to_string(rootPc);
        ev.startBar = curBar;
        ev.barLength = chordDef.barLength;
        ev.rootPitchClass = rootPc;
        ev.quality = chordDef.quality;
        ev.bassPitchClass = -1;
        chordTrack_.push_back(ev);

        curBar += std::max(1u, static_cast<uint32_t>(std::round(chordDef.barLength)));
    }

    std::sort(chordTrack_.begin(), chordTrack_.end(), [](const theory::ChordEvent& a, const theory::ChordEvent& b) {
        return a.startBar < b.startBar;
    });
}

uint32_t ArrangerView::extractChordsFromTrack(uint32_t trackIdx) {
    if (trackIdx >= tracks_.size()) return 0;
    const auto& track = tracks_[trackIdx];

    std::vector<theory::TheoryNote> allNotes;
    for (const auto& clip : track.clips) {
        float clipStartStep = static_cast<float>(clip.startBar - 1) * 16.0f;
        for (const auto& n : clip.notes) {
            theory::TheoryNote tn;
            tn.pitch = n.pitch;
            tn.startStep = clipStartStep + (n.startBeat * 4.0f);
            tn.durationSteps = n.lengthBeats * 4.0f;
            tn.velocity = n.velocity;
            allNotes.push_back(tn);
        }
    }

    auto extracted = theory::ChordTheory::extractChordsFromNotes(allNotes, 0, totalBars_, 16);
    uint32_t count = static_cast<uint32_t>(extracted.size());
    for (const auto& c : extracted) {
        addOrUpdateChord(c);
    }
    return count;
}

uint32_t ArrangerView::extractChordsFromClip(uint32_t trackIdx, uint32_t clipIdx) {
    if (trackIdx >= tracks_.size() || clipIdx >= tracks_[trackIdx].clips.size()) return 0;
    const auto& clip = tracks_[trackIdx].clips[clipIdx];

    std::vector<theory::TheoryNote> notes;
    for (const auto& n : clip.notes) {
        theory::TheoryNote tn;
        tn.pitch = n.pitch;
        tn.startStep = n.startBeat * 4.0f;
        tn.durationSteps = n.lengthBeats * 4.0f;
        tn.velocity = n.velocity;
        notes.push_back(tn);
    }

    uint32_t clipStartBar0 = (clip.startBar > 0) ? (clip.startBar - 1) : 0;
    auto extracted = theory::ChordTheory::extractChordsFromNotes(notes, clipStartBar0, clip.lengthBars, 16);
    uint32_t count = static_cast<uint32_t>(extracted.size());
    for (const auto& c : extracted) {
        addOrUpdateChord(c);
    }
    return count;
}

void ArrangerView::updateClipDetectedChords(ArrangerTimelineClip& clip) {
    if (clip.notes.empty()) {
        clip.detectedChords.clear();
        return;
    }
    std::vector<theory::TheoryNote> tnotes;
    tnotes.reserve(clip.notes.size());
    for (const auto& n : clip.notes) {
        theory::TheoryNote tn;
        tn.pitch = n.pitch;
        tn.startStep = n.startBeat * 4.0f; // 4 steps per beat
        tn.durationSteps = n.lengthBeats * 4.0f;
        tn.velocity = n.velocity;
        tnotes.push_back(tn);
    }
    uint32_t effBars = clip.isLooped ? clip.loopLengthBars : clip.lengthBars;
    if (effBars == 0) effBars = 1;
    clip.detectedChords = theory::ChordTheory::extractChordsFromNotes(tnotes, 0, effBars, 16);
}

void ArrangerView::refreshAllClipChords() {
    for (auto& track : tracks_) {
        for (auto& clip : track.clips) {
            updateClipDetectedChords(clip);
        }
    }
}

const theory::ChordEvent* ArrangerView::getActiveChordForTrackAtBar([[maybe_unused]] uint32_t trackIdx, float bar) const noexcept {
    return getActiveChordAtBar(bar);
}

std::vector<ArrangerView::OverviewChordInfo> ArrangerView::getHarmonicOverviewChords() const {
    std::vector<OverviewChordInfo> result;
    result.reserve(chordTrack_.size());
    for (const auto& ch : chordTrack_) {
        OverviewChordInfo info;
        info.chord = ch;
        info.sourceTrackIdx = -1;
        info.sourceTrackName = "Chord Track";
        info.sourceClipIdx = -1;
        info.startBar = static_cast<float>(ch.startBar);
        info.barLength = ch.barLength;
        result.push_back(info);
    }
    return result;
}

void ArrangerView::bakeChordsToTrack(uint32_t trackIdx) {
    if (trackIdx >= tracks_.size()) return;
    auto& track = tracks_[trackIdx];
    if (track.chordFollowMode == theory::ChordFollowMode::Off) return;

    for (auto& clip : track.clips) {
        float clipStartBar = static_cast<float>(clip.startBar - 1);
        for (auto& note : clip.notes) {
            float noteBar = clipStartBar + (note.startBeat / 4.0f);
            const auto* chord = getActiveChordAtBar(noteBar);
            if (chord) {
                note.pitch = static_cast<uint8_t>(theory::ChordTheory::remapPitchForChord(
                    note.pitch, *chord, track.chordFollowMode));
            }
        }
        updateClipDetectedChords(clip);
    }
    track.chordFollowMode = theory::ChordFollowMode::Off;
    if (onTrackChordFollowChanged) {
        onTrackChordFollowChanged(trackIdx, theory::ChordFollowMode::Off);
    }
}

void ArrangerView::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;
    trackHeaderWidth_ = ctx.isMobile ? 130.0f : 190.0f;
    inspectorWidth_ = ctx.isMobile ? 260.0f : 320.0f;

    propertiesDrawer_.layout(bounds, 0.0f);
    inspectorOpen_ = propertiesDrawer_.isExpanded();

    float inspW = propertiesDrawer_.getEffectiveWidth();
    float mainW = bounds_.w - inspW;

    rulerBounds_ = Rect2D(bounds_.x + trackHeaderWidth_, bounds_.y, mainW - trackHeaderWidth_, rulerHeight_);
    minimapBounds_ = Rect2D(bounds_.x + trackHeaderWidth_, bounds_.y + rulerHeight_, mainW - trackHeaderWidth_, minimapHeight_);

    // Dedicated Chord Track Lane & Chord Header Card (Eatsbeats parity)
    chordLaneBounds_ = Rect2D(bounds_.x + trackHeaderWidth_, bounds_.y + rulerHeight_ + minimapHeight_, mainW - trackHeaderWidth_, chordLaneHeight_);
    chordHeaderBounds_ = Rect2D(bounds_.x, bounds_.y + rulerHeight_ + minimapHeight_, trackHeaderWidth_, chordLaneHeight_);

    float gridTopY = bounds_.y + rulerHeight_ + minimapHeight_ + chordLaneHeight_;
    float gridH = bounds_.h - rulerHeight_ - minimapHeight_ - chordLaneHeight_;

    tracksListBounds_ = Rect2D(bounds_.x, gridTopY, trackHeaderWidth_, gridH);
    gridBounds_ = Rect2D(bounds_.x + trackHeaderWidth_, gridTopY, mainW - trackHeaderWidth_, gridH);
    inspectorBounds_ = propertiesDrawer_.getDrawerBounds();

    // + ADD track row directly below the last track
    float addRowY = tracksListBounds_.y + static_cast<float>(tracks_.size()) * trackRowHeight_ - scrollY_;
    addTrackRowBounds_ = Rect2D(tracksListBounds_.x + 10.0f, addRowY + 8.0f, trackHeaderWidth_ - 20.0f, 32.0f);

    pluginDialog_.layout(bounds_.w, bounds_.h);
    circleOfFifthsDialog_.layout(bounds_.w, bounds_.h);
    iconDialog_.layout(bounds_.w, bounds_.h);
    mixerDrawer_.layout(bounds, inspW);
}

void ArrangerView::render(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    float frameDt = ctx.dt > 0.0f ? ctx.dt : 0.016f;
    propertiesDrawer_.update(frameDt);
    propertiesDrawer_.layout(bounds_, 0.0f);
    mixerDrawer_.update(frameDt);
    mixerDrawer_.layout(bounds_, propertiesDrawer_.getEffectiveWidth());

    if (kineticScroller_.isGliding()) {
        float dx = 0.0f, dy = 0.0f;
        kineticScroller_.step(ctx.dt > 0.0f ? ctx.dt : 0.016f, dx, dy);
        scrollX_ = (std::max)(0.0f, scrollX_ - dx);
        float maxScrollY = (std::max)(0.0f, static_cast<float>(tracks_.size()) * trackRowHeight_ - gridBounds_.h);
        scrollY_ = std::clamp(scrollY_ - dy, 0.0f, maxScrollY);
    }

    // Follow playback if enabled
    if (followPlayback_ && ctx.audioEngine && ctx.audioEngine->getSequencer().isPlaying()) {
        float curStep = static_cast<float>(ctx.audioEngine->getSequencer().getTransport().getCurrentStep());
        float playheadX = (curStep / 16.0f) * barWidth_;
        float targetScroll = playheadX - (gridBounds_.w * 0.4f);
        if (targetScroll < 0.0f) targetScroll = 0.0f;
        scrollX_ += (targetScroll - scrollX_) * 0.15f;
    }

    renderGrid(ctx);
    renderChordLane(ctx);
    renderClips(ctx);
    renderTrackHeaders(ctx);
    renderRulerAndMinimap(ctx);
    renderPropertiesDrawer(ctx);

    // 6. Playhead Indicator
    if (ctx.audioEngine) {
        float curStep = static_cast<float>(ctx.audioEngine->getSequencer().getTransport().getCurrentStep());
        float playheadX = gridBounds_.x + (curStep / 16.0f) * barWidth_ - scrollX_;
        if (playheadX >= gridBounds_.x && playheadX <= gridBounds_.x + gridBounds_.w) {
            drawLine(r, playheadX, rulerBounds_.y, playheadX, gridBounds_.y + gridBounds_.h,
                     theme.playhead.r, theme.playhead.g, theme.playhead.b, 0.95f, 2.0f);
            drawTriangle(r, playheadX - 6.0f, rulerBounds_.y,
                         playheadX + 6.0f, rulerBounds_.y,
                         playheadX, rulerBounds_.y + 10.0f,
                         theme.playhead.r, theme.playhead.g, theme.playhead.b, 1.0f);
        }
    }

    // 7. Follow Indicator Badge
    if (followPlayback_) {
        drawRoundedRect(r, gridBounds_.x + gridBounds_.w - 86.0f, rulerBounds_.y + 4.0f, 78.0f, 20.0f, 3.0f,
                        theme.secondaryAccent.r * 0.25f, theme.secondaryAccent.g * 0.25f, theme.secondaryAccent.b * 0.25f, 0.85f);
        drawText(r, "[F] FOLLOW", gridBounds_.x + gridBounds_.w - 80.0f, rulerBounds_.y + 6.5f, 9.5f,
                 theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);
    }

    // 8. Reusable Contextual Dialogs (Rendered on top if open)
    // Note: pluginDialog_ is rendered at the top-level overlay pass in GuiWindow for full-screen backdrop coverage
    if (circleOfFifthsDialog_.isOpen()) {
        circleOfFifthsDialog_.render(r, theme);
    }
    if (iconDialog_.isOpen()) {
        iconDialog_.render(r, theme);
    }
}

void ArrangerView::renderGrid(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // Grid Background
    drawRect(r, gridBounds_.x, gridBounds_.y, gridBounds_.w, gridBounds_.h,
             theme.backgroundDark.r, theme.backgroundDark.g, theme.backgroundDark.b, 1.0f);

    // Bar vertical lines & alternating subtle shading
    for (uint32_t bar = 1; bar <= totalBars_; ++bar) {
        float bx = gridBounds_.x + static_cast<float>(bar - 1) * barWidth_ - scrollX_;
        if (bx + barWidth_ < gridBounds_.x || bx > gridBounds_.x + gridBounds_.w) continue;

        if (bar % 2 == 0) {
            drawRect(r, bx, gridBounds_.y, barWidth_, gridBounds_.h,
                     theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.22f);
        }
        drawLine(r, bx, gridBounds_.y, bx, gridBounds_.y + gridBounds_.h,
                 theme.gridLineMinor.r, theme.gridLineMinor.g, theme.gridLineMinor.b, 0.35f, 1.0f);
    }

    // Horizontal track row separators
    for (size_t t = 0; t <= tracks_.size(); ++t) {
        float rowY = gridBounds_.y + static_cast<float>(t) * trackRowHeight_ - scrollY_;
        if (rowY < gridBounds_.y || rowY > gridBounds_.y + gridBounds_.h) continue;

        drawLine(r, gridBounds_.x, rowY, gridBounds_.x + gridBounds_.w, rowY,
                 theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.40f, 1.0f);
    }
}

void ArrangerView::renderChordLane(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // 1. Background strip for Harmonic Overview Lane
    drawRect(r, chordLaneBounds_.x, chordLaneBounds_.y, chordLaneBounds_.w, chordLaneBounds_.h,
             0.075f, 0.080f, 0.095f, 0.98f);
    drawLine(r, chordLaneBounds_.x, chordLaneBounds_.y + chordLaneBounds_.h,
             chordLaneBounds_.x + chordLaneBounds_.w, chordLaneBounds_.y + chordLaneBounds_.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.80f, 1.5f);

    // 2. Bar vertical marker ticks
    for (uint32_t bar = 1; bar <= totalBars_; ++bar) {
        float bx = chordLaneBounds_.x + static_cast<float>(bar - 1) * barWidth_ - scrollX_;
        if (bx < chordLaneBounds_.x - 10.0f || bx > chordLaneBounds_.x + chordLaneBounds_.w) continue;

        drawLine(r, bx, chordLaneBounds_.y, bx, chordLaneBounds_.y + chordLaneBounds_.h,
                 theme.gridLineMinor.r, theme.gridLineMinor.g, theme.gridLineMinor.b, 0.40f, 1.0f);
    }

    // 3. Render Chord Blocks
    if (chordTrack_.empty()) {
        drawText(r, "♫ CHORD TRACK • Click + or double-click to add chords / open Circle of Fifths",
                 chordLaneBounds_.x + 20.0f, chordLaneBounds_.y + 9.0f, 10.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.75f);
        return;
    }

    // Right-hand header hint
    float hintX = chordLaneBounds_.x + chordLaneBounds_.w - 240.0f;
    if (hintX > chordLaneBounds_.x + 200.0f) {
        drawText(r, "[ + ] CIRCLE OF FIFTHS • CLICK TO AUDITION", hintX, chordLaneBounds_.y + 9.0f, 8.0f,
                 0.50f, 0.55f, 0.65f, 0.70f);
    }

    for (size_t i = 0; i < chordTrack_.size(); ++i) {
        const auto& chord = chordTrack_[i];
        float cx = chordLaneBounds_.x + static_cast<float>(chord.startBar) * barWidth_ - scrollX_;
        float cw = chord.barLength * barWidth_;
        float cy = chordLaneBounds_.y + 2.0f;
        float ch = chordLaneBounds_.h - 4.0f;

        // Culling
        if (cx + cw < chordLaneBounds_.x || cx > chordLaneBounds_.x + chordLaneBounds_.w) continue;

        bool isSelected = (selectedChordIndex_ == static_cast<int>(i));

        // Harmonic Color Palette (Eatsbeats visual aesthetic)
        float bgR = 0.12f, bgG = 0.22f, bgB = 0.32f;
        float edgeR = 0.18f, edgeG = 0.75f, edgeB = 0.90f;

        if (chord.quality == theory::ChordQuality::Minor || chord.quality == theory::ChordQuality::Minor7 || chord.quality == theory::ChordQuality::Min9) {
            bgR = 0.26f; bgG = 0.12f; bgB = 0.30f;
            edgeR = 0.85f; edgeG = 0.30f; edgeB = 0.80f;
        } else if (chord.quality == theory::ChordQuality::Dominant7 || chord.quality == theory::ChordQuality::Dom9) {
            bgR = 0.30f; bgG = 0.20f; bgB = 0.10f;
            edgeR = 0.98f; edgeG = 0.65f; edgeB = 0.20f;
        } else if (chord.quality == theory::ChordQuality::Diminished || chord.quality == theory::ChordQuality::HalfDiminished7) {
            bgR = 0.28f; bgG = 0.10f; bgB = 0.14f;
            edgeR = 0.95f; edgeG = 0.25f; edgeB = 0.35f;
        } else if (chord.quality == theory::ChordQuality::Sus2 || chord.quality == theory::ChordQuality::Sus4) {
            bgR = 0.10f; bgG = 0.25f; bgB = 0.22f;
            edgeR = 0.25f; edgeG = 0.88f; edgeB = 0.65f;
        }

        if (isSelected) {
            bgR = std::min(1.0f, bgR * 1.5f + 0.1f);
            bgG = std::min(1.0f, bgG * 1.5f + 0.1f);
            bgB = std::min(1.0f, bgB * 1.5f + 0.1f);
            edgeR = 1.0f; edgeG = 0.92f; edgeB = 0.40f; // Glowing Gold outline
        }

        // Draw block body
        drawRoundedRect(r, cx + 1.0f, cy, std::max(4.0f, cw - 2.0f), ch, 4.0f, bgR, bgG, bgB, 0.92f);

        // Subtle gradient sheen on top half
        drawRoundedRect(r, cx + 1.0f, cy, std::max(4.0f, cw - 2.0f), ch * 0.45f, 4.0f,
                        1.0f, 1.0f, 1.0f, 0.08f);

        // Glowing outline
        drawRoundedRectOutline(r, cx + 1.0f, cy, std::max(4.0f, cw - 2.0f), ch, 4.0f,
                               edgeR, edgeG, edgeB, isSelected ? 1.0f : 0.75f, isSelected ? 2.0f : 1.2f);

        // Roman numeral badge (e.g. I, vi, V, IV)
        std::string roman = theory::ChordTheory::getRomanNumeral(
            songKeyRoot_, isSongKeyMinor_, chord.rootPitchClass, chord.quality);
        float badgeW = std::clamp(static_cast<float>(roman.length()) * 7.5f + 8.0f, 18.0f, 34.0f);
        float badgeH = 14.0f;
        float badgeX = cx + 5.0f;
        float badgeY = cy + (ch - badgeH) * 0.5f;

        if (cw >= 40.0f) {
            drawRoundedRect(r, badgeX, badgeY, badgeW, badgeH, 3.0f, 0.05f, 0.05f, 0.07f, 0.85f);
            drawRoundedRectOutline(r, badgeX, badgeY, badgeW, badgeH, 3.0f, edgeR, edgeG, edgeB, 0.8f, 1.0f);
            drawText(r, roman, badgeX + (badgeW - static_cast<float>(roman.length()) * 5.5f) * 0.5f,
                     badgeY + 2.5f, 8.5f, edgeR, edgeG, edgeB, 1.0f);
        }

        // Chord Name Text (e.g. "C", "Am", "G7", "F/A")
        std::string chordName = chord.getDisplayName();
        float textX = (cw >= 40.0f) ? (badgeX + badgeW + 6.0f) : (cx + 5.0f);
        if (textX + 16.0f <= cx + cw) {
            drawText(r, chordName, textX, cy + 6.5f, 11.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    }
}

void ArrangerView::renderClips(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    for (size_t t = 0; t < tracks_.size(); ++t) {
        float rowY = gridBounds_.y + static_cast<float>(t) * trackRowHeight_ - scrollY_;
        if (rowY + trackRowHeight_ < gridBounds_.y || rowY > gridBounds_.y + gridBounds_.h) continue;

        const auto& track = tracks_[t];
        for (size_t c = 0; c < track.clips.size(); ++c) {
            const auto& clip = track.clips[c];
            float cx = gridBounds_.x + static_cast<float>(clip.startBar - 1) * barWidth_ - scrollX_;
            float cw = static_cast<float>(clip.lengthBars) * barWidth_ - 2.0f;
            float cy = rowY + 3.0f;
            float ch = trackRowHeight_ - 6.0f;

            if (cx + cw < gridBounds_.x || cx > gridBounds_.x + gridBounds_.w) continue;

            bool isSelected = (static_cast<int>(t) == static_cast<int>(activeTrackIndex_) &&
                               static_cast<int>(c) == selectedClipIndex_);

            // 1. Clip Body Base (dark translucent well tinted with track color)
            drawRoundedRect(r, cx, cy, cw, ch, 4.0f,
                            0.06f + clip.r * 0.10f, 0.06f + clip.g * 0.10f, 0.06f + clip.b * 0.10f, 0.94f);

            // 2. Header Bar (vibrant track-tinted banner)
            drawRoundedRect(r, cx, cy, cw, 18.0f, 3.5f,
                            0.12f + clip.r * 0.30f, 0.12f + clip.g * 0.30f, 0.12f + clip.b * 0.30f, 0.96f);

            // 3. Gold Index Badge '00' on left
            drawRoundedRect(r, cx + 4.0f, cy + 3.0f, 16.0f, 12.0f, 2.0f,
                            1.0f, 0.75f, 0.0f, 0.90f);
            drawText(r, "00", cx + 6.0f, cy + 4.0f, 8.5f,
                     0.05f, 0.05f, 0.05f, 1.0f);

            // 4. Clip Name
            drawText(r, clip.name, cx + 24.0f, cy + 4.0f, 10.0f,
                     1.0f, 1.0f, 1.0f, 0.95f);

            // 5. Top-Right Loop Resize Handle & Icon (Width: 22px, Height: 18px)
            float loopBtnX = cx + cw - 20.0f;
            float loopBtnY = cy + 2.0f;
            drawCircle(r, loopBtnX + 7.0f, loopBtnY + 7.0f, 6.0f,
                       clip.isLooped ? 1.0f : 0.25f, clip.isLooped ? 0.75f : 0.25f, clip.isLooped ? 0.0f : 0.30f, 0.8f);
            drawCircle(r, loopBtnX + 7.0f, loopBtnY + 7.0f, 4.5f,
                       0.08f, 0.08f, 0.10f, 1.0f);
            drawText(r, "R", loopBtnX + 4.5f, loopBtnY + 3.0f, 8.0f,
                     clip.isLooped ? 1.0f : 0.8f, clip.isLooped ? 0.75f : 0.8f, clip.isLooped ? 0.0f : 0.8f, 1.0f);

            // 6. Bottom-Right Standard Resize Handle (< >)
            drawText(r, "<>", cx + cw - 16.0f, cy + ch - 16.0f, 9.0f,
                     1.0f, 1.0f, 1.0f, 0.75f);

            // 7. Note preview blocks inside clip body (sharp rectangles matching track color with velocity intensity)
            float noteAreaY = cy + 20.0f;
            float noteAreaH = clip.detectedChords.empty() ? (ch - 22.0f) : (ch - 37.0f);
            float patternW = static_cast<float>(clip.isLooped ? clip.loopLengthBars : clip.lengthBars) * barWidth_;
            int cycles = clip.isLooped ? static_cast<int>(std::ceil(static_cast<float>(clip.lengthBars) / static_cast<float>(clip.loopLengthBars))) : 1;
            float clipBeats = static_cast<float>(clip.isLooped ? clip.loopLengthBars : clip.lengthBars) * 4.0f;

            for (int cyc = 0; cyc < cycles; ++cyc) {
                float cycleStartX = cx + static_cast<float>(cyc) * patternW;

                // Loop cycle vertical separator
                if (cyc > 0 && cycleStartX < cx + cw) {
                    drawLine(r, cycleStartX, cy + 18.0f, cycleStartX, cy + ch,
                             0.1f, 0.1f, 0.15f, 0.8f, 1.5f);
                }

                if (!clip.notes.empty()) {
                    for (const auto& note : clip.notes) {
                        float nx = cycleStartX + (note.startBeat / clipBeats) * patternW;
                        float nw = std::max(3.0f, (note.lengthBeats / clipBeats) * patternW - 1.5f);
                        if (nx + nw > cx + cw || nx < cx) continue;

                        float pitchNorm = std::clamp((static_cast<float>(note.pitch) - 34.0f) / 44.0f, 0.0f, 1.0f);
                        float ny = (noteAreaY + noteAreaH - 6.0f) - pitchNorm * (noteAreaH - 8.0f);
                        float nh = 5.0f;

                        // Notes match track color with intensity varied by velocity
                        float v = std::clamp(note.velocity, 0.10f, 1.0f);
                        float intensity = 0.35f + 0.65f * v;
                        float nr = std::clamp(clip.r * intensity, 0.0f, 1.0f);
                        float ng = std::clamp(clip.g * intensity, 0.0f, 1.0f);
                        float nb = std::clamp(clip.b * intensity, 0.0f, 1.0f);
                        float na = 0.50f + 0.50f * v;

                        // Pure sharp rectangle (performant single-quad draw)
                        drawRect(r, nx, ny, nw, nh, nr, ng, nb, na);
                    }
                } else if (clip.isAudio) {
                    // Stylized audio waveform preview for audio clips
                    int waveSegments = static_cast<int>(patternW / 4.0f);
                    for (int s = 0; s < waveSegments; ++s) {
                        float wx = cycleStartX + static_cast<float>(s) * 4.0f;
                        if (wx > cx + cw) break;
                        float phase = static_cast<float>(s) * 0.28f;
                        float amp = std::abs(std::sin(phase) * 0.65f + std::sin(phase * 2.3f) * 0.35f);
                        float wh = std::max(2.0f, amp * (noteAreaH - 6.0f));
                        float wy = noteAreaY + (noteAreaH - wh) * 0.5f;
                        drawRect(r, wx, wy, 2.5f, wh, clip.r * 0.75f, clip.g * 0.75f, clip.b * 0.75f, 0.65f);
                    }
                }
            }

            // 7b. Clip Chord Progression Text (render raw detected chord names & Roman numerals directly over clip)
            if (!clip.detectedChords.empty()) {
                float chordTextY = cy + ch - 15.0f;
                float effBars = static_cast<float>(clip.isLooped ? clip.loopLengthBars : clip.lengthBars);
                if (effBars <= 0.0f) effBars = 1.0f;

                // Color: match notes in the respective clip (brightened slightly for crisp visibility against dark clip body)
                float noteTextR = std::clamp(clip.r * 1.35f, 0.40f, 1.0f);
                float noteTextG = std::clamp(clip.g * 1.35f, 0.40f, 1.0f);
                float noteTextB = std::clamp(clip.b * 1.35f, 0.40f, 1.0f);

                for (int cyc = 0; cyc < cycles; ++cyc) {
                    float cycleStartX = cx + static_cast<float>(cyc) * patternW;

                    for (const auto& chord : clip.detectedChords) {
                        float chX = cycleStartX + (static_cast<float>(chord.startBar) / effBars) * patternW + 4.0f;
                        float chW = (chord.barLength / effBars) * patternW - 4.0f;
                        if (chX + 10.0f > cx + cw || chX < cx) continue;
                        if (chW < 12.0f) continue;

                        std::string cname = chord.getDisplayName();
                        std::string roman = theory::ChordTheory::getRomanNumeral(
                            songKeyRoot_, isSongKeyMinor_, chord.rootPitchClass, chord.quality);

                        if (chW > 38.0f) {
                            // Roman numeral in theme highlight color
                            drawText(r, roman, chX, chordTextY, 8.5f,
                                     theme.highlight.r, theme.highlight.g, theme.highlight.b, 0.95f);
                            float romanOffset = static_cast<float>(roman.length()) * 6.5f + 4.0f;
                            // Chord name in note color
                            drawText(r, cname, chX + romanOffset, chordTextY, 9.5f,
                                     noteTextR, noteTextG, noteTextB, 1.0f);
                        } else {
                            // Just chord name when width is compact
                            drawText(r, cname, chX, chordTextY, 8.5f,
                                     noteTextR, noteTextG, noteTextB, 1.0f);
                        }
                    }
                }
            }

            // 8. Selection outline with glowing accent
            if (isSelected) {
                drawRoundedRectOutline(r, cx, cy, cw, ch, 4.0f,
                                       theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f, 2.0f);
            } else {
                drawRoundedRectOutline(r, cx, cy, cw, ch, 4.0f,
                                       clip.r * 0.5f, clip.g * 0.5f, clip.b * 0.5f, 0.6f, 1.0f);
            }
        }
    }
}

void ArrangerView::renderTrackHeaders(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // -------------------------------------------------------------------------
    // Chord Track Lane Header Card (Eatsbeats Parity)
    // -------------------------------------------------------------------------
    drawRect(r, chordHeaderBounds_.x, chordHeaderBounds_.y, chordHeaderBounds_.w, chordHeaderBounds_.h,
             0.09f, 0.10f, 0.13f, 0.98f);
    drawLine(r, chordHeaderBounds_.x, chordHeaderBounds_.y + chordHeaderBounds_.h,
             chordHeaderBounds_.x + chordHeaderBounds_.w, chordHeaderBounds_.y + chordHeaderBounds_.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.80f, 1.5f);
    drawLine(r, chordHeaderBounds_.x + chordHeaderBounds_.w, chordHeaderBounds_.y,
             chordHeaderBounds_.x + chordHeaderBounds_.w, chordHeaderBounds_.y + chordHeaderBounds_.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.85f, 2.0f);

    // Cyan harmonic stripe on left edge
    drawRect(r, chordHeaderBounds_.x, chordHeaderBounds_.y, 4.0f, chordHeaderBounds_.h,
             0.18f, 0.85f, 0.95f, 1.0f);

    // "CHORD TRACK" Title
    drawText(r, "CHORD TRACK", chordHeaderBounds_.x + 10.0f, chordHeaderBounds_.y + 8.5f, 10.0f,
             0.18f, 0.85f, 0.95f, 1.0f);

    // Song Key badge
    std::string keyText = std::string(theory::ChordTheory::pitchClassNames[songKeyRoot_]) +
                          (isSongKeyMinor_ ? "m" : " maj");
    float keyBadgeW = static_cast<float>(keyText.length()) * 6.5f + 10.0f;
    float keyBadgeX = chordHeaderBounds_.x + chordHeaderBounds_.w - keyBadgeW - 28.0f;
    drawRoundedRect(r, keyBadgeX, chordHeaderBounds_.y + 4.0f, keyBadgeW, chordHeaderBounds_.h - 8.0f, 3.0f,
                    0.14f, 0.16f, 0.22f, 0.9f);
    drawRoundedRectOutline(r, keyBadgeX, chordHeaderBounds_.y + 4.0f, keyBadgeW, chordHeaderBounds_.h - 8.0f, 3.0f,
                           0.28f, 0.35f, 0.48f, 0.8f, 1.0f);
    drawText(r, keyText, keyBadgeX + 5.0f, chordHeaderBounds_.y + 8.0f, 8.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // [ + ] button to open Circle of Fifths
    float addBtnX = chordHeaderBounds_.x + chordHeaderBounds_.w - 22.0f;
    float addBtnY = chordHeaderBounds_.y + 4.0f;
    float addBtnW = 16.0f;
    float addBtnH = chordHeaderBounds_.h - 8.0f;
    drawRoundedRect(r, addBtnX, addBtnY, addBtnW, addBtnH, 3.0f, 0.18f * 0.3f, 0.85f * 0.3f, 0.95f * 0.3f, 0.9f);
    drawRoundedRectOutline(r, addBtnX, addBtnY, addBtnW, addBtnH, 3.0f, 0.18f, 0.85f, 0.95f, 0.8f, 1.0f);
    drawText(r, "+", addBtnX + 4.5f, addBtnY + 4.0f, 9.5f, 0.18f, 0.85f, 0.95f, 1.0f);

    // Header strip container
    drawRect(r, tracksListBounds_.x, tracksListBounds_.y, tracksListBounds_.w, tracksListBounds_.h,
             theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawLine(r, tracksListBounds_.x + tracksListBounds_.w, tracksListBounds_.y,
             tracksListBounds_.x + tracksListBounds_.w, tracksListBounds_.y + tracksListBounds_.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.85f, 2.0f);

    for (size_t t = 0; t < tracks_.size(); ++t) {
        float rowY = tracksListBounds_.y + static_cast<float>(t) * trackRowHeight_ - scrollY_;
        if (rowY + trackRowHeight_ < tracksListBounds_.y || rowY > tracksListBounds_.y + tracksListBounds_.h) continue;

        const auto& track = tracks_[t];
        bool isActive = (activeTrackIndex_ == t);

        // Track header card body
        drawRect(r, tracksListBounds_.x, rowY, tracksListBounds_.w, trackRowHeight_,
                 isActive ? theme.panelBackground.r * 1.35f : theme.panelBackground.r,
                 isActive ? theme.panelBackground.g * 1.35f : theme.panelBackground.g,
                 isActive ? theme.panelBackground.b * 1.35f : theme.panelBackground.b, 0.95f);

        // Left vertical colored track stripe
        drawRect(r, tracksListBounds_.x, rowY, 6.0f, trackRowHeight_,
                 track.r, track.g, track.b, 1.0f);

        // Track Vector Icon Glyph
        float iconBoxX = tracksListBounds_.x + 10.0f;
        float iconBoxY = rowY + 7.5f;
        float iconBoxSize = 15.0f;
        Color iconCol = isActive ? theme.primaryAccent : Color{track.r, track.g, track.b, 1.0f};
        IconRegistry::instance().renderIcon(r, track.iconRef.empty() ? "preset:inst_synth" : track.iconRef,
                                            iconBoxX, iconBoxY, iconBoxSize, iconCol);

        // Track Name (size 10.0f matching clip title text)
        drawText(r, track.name, iconBoxX + iconBoxSize + 6.0f, rowY + 9.5f, 10.0f,
                 isActive ? theme.primaryAccent.r : theme.textPrimary.r,
                 isActive ? theme.primaryAccent.g : theme.textPrimary.g,
                 isActive ? theme.primaryAccent.b : theme.textPrimary.b, 1.0f);

        // MUTE, SOLO Buttons
        float btnW = 16.0f;
        float btnH = 13.0f;
        float btnY = rowY + 8.0f;

        // Mute
        drawRoundedRect(r, tracksListBounds_.x + tracksListBounds_.w - 42.0f, btnY, btnW, btnH, 2.0f,
                        track.mute ? 0.85f : 0.20f, 0.20f, 0.20f, 0.95f);
        drawText(r, "M", tracksListBounds_.x + tracksListBounds_.w - 38.0f, btnY + 2.5f, 8.0f, 1.0f, 1.0f, 1.0f, 1.0f);

        // Solo
        drawRoundedRect(r, tracksListBounds_.x + tracksListBounds_.w - 22.0f, btnY, btnW, btnH, 2.0f,
                        track.solo ? 0.95f : 0.20f, track.solo ? 0.80f : 0.20f, 0.10f, 0.95f);
        drawText(r, "S", tracksListBounds_.x + tracksListBounds_.w - 18.0f, btnY + 2.5f, 8.0f, 1.0f, 1.0f, 1.0f, 1.0f);

        // Volume Horizontal Pill Slider (VOL 85%) moved down by 10px
        drawText(r, "VOL", tracksListBounds_.x + 14.0f, rowY + 40.0f, 8.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

        float sliderW = tracksListBounds_.w - 75.0f;
        float sliderH = 4.0f;
        float sliderX = tracksListBounds_.x + 36.0f;
        float sliderY = rowY + 44.0f;

        // Slider track
        drawRoundedRect(r, sliderX, sliderY, sliderW, sliderH, 2.0f,
                        0.10f, 0.10f, 0.12f, 1.0f);
        // Colored volume progress fill
        float normV = std::clamp(track.volume / 1.5f, 0.0f, 1.0f);
        drawRoundedRect(r, sliderX, sliderY, sliderW * normV, sliderH, 2.0f,
                        track.r, track.g, track.b, 1.0f);
        // Slider pill thumb
        float thumbX = sliderX + sliderW * normV;
        drawCircle(r, thumbX, sliderY + 2.0f, 5.0f, 0.85f, 0.85f, 0.90f, 1.0f);

        // Pan Knob with center detent indicator line (moved down by 10px, text readout removed)
        float knobX = tracksListBounds_.x + tracksListBounds_.w - 22.0f;
        float knobY = rowY + 46.0f;
        drawCircle(r, knobX, knobY, 9.0f, 0.15f, 0.16f, 0.18f, 1.0f);
        drawCircle(r, knobX, knobY, 7.5f, 0.08f, 0.08f, 0.09f, 1.0f);
        float pAngle = -1.57f + track.pan * 2.2f;
        drawLine(r, knobX, knobY, knobX + std::cos(pAngle) * 6.5f, knobY + std::sin(pAngle) * 6.5f,
                 track.r, track.g, track.b, 1.0f, 1.5f);

        drawLine(r, tracksListBounds_.x, rowY + trackRowHeight_, tracksListBounds_.x + tracksListBounds_.w, rowY + trackRowHeight_,
                 theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.45f, 1.0f);
    }

    // 5. '+ ADD TRACK' Row directly below last track
    float addRowY = tracksListBounds_.y + static_cast<float>(tracks_.size()) * trackRowHeight_ - scrollY_;
    if (addRowY + 32.0f >= tracksListBounds_.y && addRowY <= tracksListBounds_.y + tracksListBounds_.h) {
        addTrackRowBounds_ = Rect2D(tracksListBounds_.x + 12.0f, addRowY + 6.0f, tracksListBounds_.w - 24.0f, 26.0f);
        drawRoundedRect(r, addTrackRowBounds_.x, addTrackRowBounds_.y, addTrackRowBounds_.w, addTrackRowBounds_.h, 5.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
        drawRoundedRectOutline(r, addTrackRowBounds_.x, addTrackRowBounds_.y, addTrackRowBounds_.w, addTrackRowBounds_.h, 5.0f,
                               theme.primaryAccent.r * 0.6f, theme.primaryAccent.g * 0.6f, theme.primaryAccent.b * 0.6f, 0.8f, 1.0f);
        drawText(r, "+ ADD", addTrackRowBounds_.x + (addTrackRowBounds_.w * 0.5f) - 18.0f, addTrackRowBounds_.y + 7.0f, 10.5f,
                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    }
}

void ArrangerView::renderRulerAndMinimap(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // 1. Time Ruler Bar
    drawRect(r, rulerBounds_.x, rulerBounds_.y, rulerBounds_.w, rulerBounds_.h,
             theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 1.0f);
    drawLine(r, rulerBounds_.x, rulerBounds_.y + rulerBounds_.h, rulerBounds_.x + rulerBounds_.w, rulerBounds_.y + rulerBounds_.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    for (uint32_t bar = 1; bar <= totalBars_; ++bar) {
        float bx = rulerBounds_.x + static_cast<float>(bar - 1) * barWidth_ - scrollX_;
        if (bx < rulerBounds_.x - 10.0f || bx > rulerBounds_.x + rulerBounds_.w) continue;

        drawLine(r, bx, rulerBounds_.y + rulerBounds_.h - 8.0f, bx, rulerBounds_.y + rulerBounds_.h,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.6f, 1.0f);

        std::string barStr = std::to_string(bar);
        drawText(r, barStr, bx + 4.0f, rulerBounds_.y + 7.0f, 10.0f,
                 theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.9f);
    }

    // 2. Timeline Minimap Scrollbar
    drawRect(r, minimapBounds_.x, minimapBounds_.y, minimapBounds_.w, minimapBounds_.h,
             0.05f, 0.05f, 0.07f, 0.95f);
    drawLine(r, minimapBounds_.x, minimapBounds_.y + minimapBounds_.h, minimapBounds_.x + minimapBounds_.w, minimapBounds_.y + minimapBounds_.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    float totalArrangerW = static_cast<float>(totalBars_) * barWidth_;
    float scaleFactor = minimapBounds_.w / totalArrangerW;

    // Render miniature thumbnail clips in minimap
    for (size_t t = 0; t < tracks_.size(); ++t) {
        for (const auto& cl : tracks_[t].clips) {
            float mcx = minimapBounds_.x + (static_cast<float>(cl.startBar - 1) * barWidth_) * scaleFactor;
            float mcw = (static_cast<float>(cl.lengthBars) * barWidth_) * scaleFactor;
            float mcy = minimapBounds_.y + 2.0f + static_cast<float>(t % 3) * 3.5f;
            drawRect(r, mcx, mcy, std::max(2.0f, mcw), 2.5f,
                     cl.r, cl.g, cl.b, 0.85f);
        }
    }

    // Render Draggable Viewport Window Thumb
    float thumbStartX = minimapBounds_.x + (scrollX_ * scaleFactor);
    float thumbW = std::clamp(gridBounds_.w * scaleFactor, 24.0f, minimapBounds_.w);

    drawRoundedRect(r, thumbStartX, minimapBounds_.y + 1.0f, thumbW, minimapBounds_.h - 2.0f, 2.0f,
                    theme.primaryAccent.r * 0.35f, theme.primaryAccent.g * 0.35f, theme.primaryAccent.b * 0.35f, 0.70f);
    drawRoundedRectOutline(r, thumbStartX, minimapBounds_.y + 1.0f, thumbW, minimapBounds_.h - 2.0f, 2.0f,
                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.95f, 1.0f);
}

void ArrangerView::renderPropertiesDrawer(const ViewContext& ctx) {
    if (activeTrackIndex_ < tracks_.size()) {
        auto& track = tracks_[activeTrackIndex_];
        bool trackChanged = (drawerData_.trackIndex != activeTrackIndex_ ||
                             drawerData_.instrument != track.instrument ||
                             drawerData_.instrumentEngine != track.instrumentEngine);
        if (trackChanged) {
            drawerData_.knobs.clear();
        }

        drawerData_.trackIndex = activeTrackIndex_;
        drawerData_.totalTracks = static_cast<uint32_t>(tracks_.size());
        drawerData_.trackName = track.name;
        drawerData_.instrument = track.instrument;
        drawerData_.instrumentEngine = track.instrumentEngine.empty() ? "synth" : track.instrumentEngine;
        drawerData_.presetTitle = track.instrument.empty() ? track.name : track.instrument;
        drawerData_.presetSubtitle = track.name + " • " + (track.instrument.empty() ? "Track" : track.instrument);
        drawerData_.iconRef = track.iconRef;
        drawerData_.r = track.r;
        drawerData_.g = track.g;
        drawerData_.b = track.b;
        drawerData_.volume = track.volume;
        drawerData_.pan = track.pan;
        drawerData_.mute = track.mute;
        drawerData_.solo = track.solo;
        drawerData_.isMasterSelected = false;
        drawerData_.isMixerMode = false;
        drawerData_.tab = (inspectorTab_ == ArrangerInspectorTab::Clip) ? TrackPropertiesTab::Clip : TrackPropertiesTab::Track;

        drawerData_.knob1 = track.knob1;
        drawerData_.knob2 = track.knob2;
        drawerData_.knob3 = track.knob3;
        drawerData_.knob4 = track.knob4;
        drawerData_.knob1Name = track.knob1Name;
        drawerData_.knob2Name = track.knob2Name;
        drawerData_.knob3Name = track.knob3Name;
        drawerData_.knob4Name = track.knob4Name;

        drawerData_.allTrackNames.clear();
        for (const auto& trk : tracks_) {
            drawerData_.allTrackNames.push_back(trk.name);
        }

        // If in Clip tab and no clip was selected, auto-select clip 0 if available
        if (inspectorTab_ == ArrangerInspectorTab::Clip && selectedClipIndex_ < 0 && !track.clips.empty()) {
            selectedClipIndex_ = 0;
        }

        drawerData_.selectedClipIndex = selectedClipIndex_;
        if (selectedClipIndex_ >= 0 && selectedClipIndex_ < static_cast<int>(track.clips.size())) {
            const auto& cl = track.clips[selectedClipIndex_];
            drawerData_.clipName = cl.name;
            drawerData_.clipStartBar = cl.startBar;
            drawerData_.clipLengthBars = cl.lengthBars;
            drawerData_.clipLooped = cl.isLooped;
            drawerData_.clipLoopLengthBars = cl.loopLengthBars;
            drawerData_.clipTranspose = cl.transposeSemitones;
        } else {
            drawerData_.clipName.clear();
            drawerData_.clipStartBar = 0;
            drawerData_.clipLengthBars = 0;
            drawerData_.clipLooped = false;
            drawerData_.clipLoopLengthBars = 0;
            drawerData_.clipTranspose = 0;
        }

        drawerData_.midiFx = track.midiFx;
        drawerData_.audioFx = track.audioFx;
        drawerData_.syncKnobsIfEmpty();
    }

    mixerDrawer_.render(*ctx.renderer, *ctx.theme, tracks_, activeTrackIndex_,
                        masterVolume_, masterPan_, masterMute_,
                        masterPeakL_, masterPeakR_,
                        chPeaksL_.empty() ? nullptr : chPeaksL_.data(),
                        chPeaksR_.empty() ? nullptr : chPeaksR_.data(),
                        chPeaksL_.size(),
                        ctx.mouseX, ctx.mouseY);

    propertiesDrawer_.render(*ctx.renderer, *ctx.theme, drawerData_, ctx.mouseX, ctx.mouseY);
}

bool ArrangerView::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    // 0. Forward to Reusable Contextual Dialog if open
    if (pluginDialog_.isOpen()) {
        return pluginDialog_.handlePointer(ev);
    }
    if (propertiesDrawer_.isPluginDialogOpen()) {
        if (propertiesDrawer_.handlePointer(ev, drawerData_, ctx)) {
            inspectorOpen_ = propertiesDrawer_.isExpanded();
            return true;
        }
        return true;
    }
    if (circleOfFifthsDialog_.isOpen()) {
        return circleOfFifthsDialog_.handlePointer(ev);
    }
    if (iconDialog_.isOpen()) {
        if (ev.action == PointerAction::Scroll) {
            return iconDialog_.handleScroll(ev.scrollY);
        }
        return iconDialog_.handlePointer(ev);
    }

    // 0a. Continuous active drag inside Properties Drawer (knobs, sliders, scrollbar)
    if (propertiesDrawer_.isDragging()) {
        propertiesDrawer_.handlePointer(ev, drawerData_, ctx);
        return true;
    }

    // 0b. Interaction or Scroll inside expanded Properties Drawer Sidebar
    if (propertiesDrawer_.isExpanded() && propertiesDrawer_.getDrawerBounds().contains(ev.x, ev.y)) {
        if (propertiesDrawer_.handlePointer(ev, drawerData_, ctx)) {
            inspectorOpen_ = propertiesDrawer_.isExpanded();
            return true;
        }
    }

    // 0c. Active Continuous Drag in Mixer Drawer (faders, pan, resize, scrollbar)
    if (mixerDrawer_.isDragging()) {
        if (mixerDrawer_.handlePointer(ev, tracks_, activeTrackIndex_, masterVolume_, masterPan_, masterMute_, ctx)) {
            return true;
        }
    }

    // 0d. Interaction with Mixer Drawer (Pull-Tab or expanded drawer body)
    if (mixerDrawer_.getPullTabBounds().contains(ev.x, ev.y) ||
        (mixerDrawer_.isExpanded() && mixerDrawer_.getDrawerBounds().contains(ev.x, ev.y))) {
        if (mixerDrawer_.handlePointer(ev, tracks_, activeTrackIndex_, masterVolume_, masterPan_, masterMute_, ctx)) {
            return true;
        }
    }

    // 0e. Mouse Wheel / 2D Scroll on Arranger Grid
    if (ev.action == PointerAction::Scroll) {
        float totalArrangerW = static_cast<float>(totalBars_) * barWidth_;
        scrollX_ = std::clamp(scrollX_ - ev.scrollX * 30.0f, 0.0f, std::max(0.0f, totalArrangerW - gridBounds_.w));
        scrollY_ = std::max(0.0f, scrollY_ - ev.scrollY * 30.0f);
        return true;
    }

    // 0c. Active Continuous Drags (Move, LoopResize, EdgeResize, ViewportPan, MinimapScrub, PlayheadScrub, ChordMove, ChordResize)
    if (dragMode_ != ClipDragMode::None) {
        if (ev.action == PointerAction::Move) {
            if (dragMode_ == ClipDragMode::ChordMove) {
                if (dragChordIdx_ >= 0 && dragChordIdx_ < static_cast<int>(chordTrack_.size())) {
                    float dx = ev.x - dragStartPointerX_;
                    int deltaBars = static_cast<int>(std::round(dx / barWidth_));
                    int targetBar = std::max(0, static_cast<int>(dragChordOrigStartBar_) + deltaBars);
                    chordTrack_[dragChordIdx_].startBar = static_cast<uint32_t>(targetBar);
                }
                return true;
            } else if (dragMode_ == ClipDragMode::ChordResize) {
                if (dragChordIdx_ >= 0 && dragChordIdx_ < static_cast<int>(chordTrack_.size())) {
                    float dx = ev.x - dragStartPointerX_;
                    int deltaBars = static_cast<int>(std::round(dx / barWidth_));
                    float newLen = std::max(0.5f, dragChordOrigLength_ + static_cast<float>(deltaBars));
                    chordTrack_[dragChordIdx_].barLength = newLen;
                }
                return true;
            } else if (dragMode_ == ClipDragMode::ViewportPan) {
                float totalArrangerW = static_cast<float>(totalBars_) * barWidth_;
                scrollX_ = std::clamp(dragStartScrollX_ - (ev.x - dragStartPointerX_), 0.0f, std::max(0.0f, totalArrangerW - gridBounds_.w));
                scrollY_ = std::max(0.0f, dragStartScrollY_ - (ev.y - dragStartPointerY_));
                return true;
            } else if (dragMode_ == ClipDragMode::MinimapScrub) {
                float totalArrangerW = static_cast<float>(totalBars_) * barWidth_;
                float normX = (ev.x - minimapBounds_.x) / std::max(1.0f, minimapBounds_.w);
                scrollX_ = std::clamp(normX * totalArrangerW - (gridBounds_.w * 0.5f), 0.0f, std::max(0.0f, totalArrangerW - gridBounds_.w));
                return true;
            } else if (dragMode_ == ClipDragMode::PlayheadScrub) {
                if (ctx.audioEngine) {
                    float localX = ev.x - rulerBounds_.x;
                    float bar = (localX + scrollX_) / barWidth_;
                    ctx.audioEngine->getSequencer().getTransport().setPosition(static_cast<uint32_t>(std::max(0.0f, bar * 16.0f)));
                }
                return true;
            } else if (dragMode_ == ClipDragMode::TrackVolume) {
                if (dragOrigTrackIdx_ < tracks_.size()) {
                    float dx = ev.x - dragStartPointerX_;
                    float sliderW = tracksListBounds_.w - 75.0f;
                    float norm = std::clamp((dragStartVal_ / 1.5f) + (dx / sliderW), 0.0f, 1.0f);
                    tracks_[dragOrigTrackIdx_].volume = norm * 1.5f;
                    if (onVolumeChanged) onVolumeChanged(dragOrigTrackIdx_, tracks_[dragOrigTrackIdx_].volume);
                }
                return true;
            } else if (dragMode_ == ClipDragMode::TrackPan) {
                if (dragOrigTrackIdx_ < tracks_.size()) {
                    float dx = ev.x - dragStartPointerX_;
                    tracks_[dragOrigTrackIdx_].pan = std::clamp(dragStartVal_ + (dx / 80.0f), -1.0f, 1.0f);
                    if (onPanChanged) onPanChanged(dragOrigTrackIdx_, tracks_[dragOrigTrackIdx_].pan);
                }
                return true;
            } else if (dragMode_ == ClipDragMode::InspectorVolume) {
                if (activeTrackIndex_ < tracks_.size()) {
                    float dx = ev.x - dragStartPointerX_;
                    float cardW = inspectorBounds_.w - 20.0f;
                    float sbVolW = cardW - 120.0f;
                    float norm = std::clamp((dragStartVal_ / 1.5f) + (dx / sbVolW), 0.0f, 1.0f);
                    tracks_[activeTrackIndex_].volume = norm * 1.5f;
                    if (onVolumeChanged) onVolumeChanged(activeTrackIndex_, tracks_[activeTrackIndex_].volume);
                }
                return true;
            } else if (dragMode_ == ClipDragMode::InspectorPan) {
                if (activeTrackIndex_ < tracks_.size()) {
                    float dx = ev.x - dragStartPointerX_;
                    tracks_[activeTrackIndex_].pan = std::clamp(dragStartVal_ + (dx / 80.0f), -1.0f, 1.0f);
                    if (onPanChanged) onPanChanged(activeTrackIndex_, tracks_[activeTrackIndex_].pan);
                }
                return true;
            } else if (dragMode_ == ClipDragMode::InspectorKnob) {
                if (activeTrackIndex_ < tracks_.size() && dragKnobIdx_ >= 0) {
                    float dy = dragStartPointerY_ - ev.y;
                    float newVal = std::clamp(dragStartVal_ + (dy / 120.0f), 0.0f, 1.0f);
                    auto& trk = tracks_[activeTrackIndex_];
                    std::string kname;
                    if (dragKnobIdx_ == 0) { trk.knob1 = newVal; kname = trk.knob1Name; }
                    else if (dragKnobIdx_ == 1) { trk.knob2 = newVal; kname = trk.knob2Name; }
                    else if (dragKnobIdx_ == 2) { trk.knob3 = newVal; kname = trk.knob3Name; }
                    else if (dragKnobIdx_ == 3) { trk.knob4 = newVal; kname = trk.knob4Name; }
                    if (onParamChanged) onParamChanged(activeTrackIndex_, kname, newVal);
                }
                return true;
            } else if (dragClipIdx_ >= 0 && dragOrigTrackIdx_ < tracks_.size()) {
                float dx = ev.x - dragStartPointerX_;
                float dy = ev.y - dragStartPointerY_;
                int deltaBars = static_cast<int>(std::round(dx / barWidth_));
                int deltaTracks = static_cast<int>(std::round(dy / trackRowHeight_));

                // Move Clip Horizontally and Vertically across tracks
                if (dragMode_ == ClipDragMode::Move) {
                    uint32_t targetTrackIdx = static_cast<uint32_t>(std::clamp(static_cast<int>(dragOrigTrackIdx_) + deltaTracks,
                                                                               0, static_cast<int>(tracks_.size()) - 1));
                    int targetStartBar = std::max(1, static_cast<int>(dragOrigStartBar_) + deltaBars);

                    if (targetTrackIdx != activeTrackIndex_) {
                        auto clipData = tracks_[activeTrackIndex_].clips[dragClipIdx_];
                        tracks_[activeTrackIndex_].clips.erase(tracks_[activeTrackIndex_].clips.begin() + dragClipIdx_);

                        clipData.trackIndex = targetTrackIdx;
                        clipData.startBar = static_cast<uint32_t>(targetStartBar);
                        tracks_[targetTrackIdx].clips.push_back(clipData);

                        activeTrackIndex_ = targetTrackIdx;
                        dragClipIdx_ = static_cast<int>(tracks_[targetTrackIdx].clips.size() - 1);
                        selectedClipIndex_ = dragClipIdx_;
                    } else {
                        tracks_[activeTrackIndex_].clips[dragClipIdx_].startBar = static_cast<uint32_t>(targetStartBar);
                    }
                    return true;
                }

                // Loop Resize (from upper right loop icon)
                if (dragMode_ == ClipDragMode::LoopResize) {
                    int newLen = std::max(1, static_cast<int>(dragOrigLengthBars_) + deltaBars);
                    auto& clip = tracks_[activeTrackIndex_].clips[dragClipIdx_];
                    clip.lengthBars = static_cast<uint32_t>(newLen);
                    if (clip.lengthBars > dragOrigLengthBars_) {
                        clip.isLooped = true;
                        clip.loopLengthBars = dragOrigLengthBars_;
                    } else {
                        clip.isLooped = false;
                    }
                    return true;
                }

                // Standard Edge Resize Right
                if (dragMode_ == ClipDragMode::ResizeRight) {
                    int newLen = std::max(1, static_cast<int>(dragOrigLengthBars_) + deltaBars);
                    auto& clip = tracks_[activeTrackIndex_].clips[dragClipIdx_];
                    clip.lengthBars = static_cast<uint32_t>(newLen);
                    return true;
                }
            }
        } else if (ev.action == PointerAction::Up) {
            bool clipDrag = (dragMode_ == ClipDragMode::Move ||
                             dragMode_ == ClipDragMode::LoopResize ||
                             dragMode_ == ClipDragMode::ResizeRight ||
                             dragMode_ == ClipDragMode::ResizeLeft);
            if (dragMode_ == ClipDragMode::ChordMove || dragMode_ == ClipDragMode::ChordResize) {
                std::sort(chordTrack_.begin(), chordTrack_.end(), [](const theory::ChordEvent& a, const theory::ChordEvent& b) {
                    return a.startBar < b.startBar;
                });
            }
            if (dragMode_ == ClipDragMode::TouchPan) {
                kineticScroller_.endDrag(ev.timestampMs);
            }
            dragMode_ = ClipDragMode::None;
            dragClipIdx_ = -1;
            dragKnobIdx_ = -1;
            dragChordIdx_ = -1;
            if (clipDrag && onClipsChanged) {
                onClipsChanged();
            }
            return true;
        }
    }

    // 0d. Middle-Click Pan anywhere
    if (ev.button == PointerButton::Middle && ev.action == PointerAction::Down) {
        dragMode_ = ClipDragMode::ViewportPan;
        dragStartPointerX_ = ev.x;
        dragStartPointerY_ = ev.y;
        dragStartScrollX_ = scrollX_;
        dragStartScrollY_ = scrollY_;
        return true;
    }

    // 1. Properties Drawer Pull-Tab & Panel Pointer Handling
    if (propertiesDrawer_.getPullTabBounds().contains(ev.x, ev.y) ||
        (propertiesDrawer_.isExpanded() && propertiesDrawer_.getDrawerBounds().contains(ev.x, ev.y))) {
        if (activeTrackIndex_ < tracks_.size()) {
            auto& track = tracks_[activeTrackIndex_];
            drawerData_.trackIndex = activeTrackIndex_;
            drawerData_.totalTracks = static_cast<uint32_t>(tracks_.size());
            drawerData_.trackName = track.name;
            drawerData_.instrument = track.instrument;
            drawerData_.instrumentEngine = track.instrumentEngine.empty() ? "synth" : track.instrumentEngine;
            drawerData_.r = track.r;
            drawerData_.g = track.g;
            drawerData_.b = track.b;
            drawerData_.volume = track.volume;
            drawerData_.pan = track.pan;
            drawerData_.mute = track.mute;
            drawerData_.solo = track.solo;
            drawerData_.isMasterSelected = false;
            drawerData_.isMixerMode = false;
            drawerData_.tab = (inspectorTab_ == ArrangerInspectorTab::Clip) ? TrackPropertiesTab::Clip : TrackPropertiesTab::Track;
            drawerData_.selectedClipIndex = selectedClipIndex_;
            if (selectedClipIndex_ >= 0 && selectedClipIndex_ < static_cast<int>(track.clips.size())) {
                const auto& cl = track.clips[selectedClipIndex_];
                drawerData_.clipName = cl.name;
                drawerData_.clipStartBar = cl.startBar;
                drawerData_.clipLengthBars = cl.lengthBars;
                drawerData_.clipLooped = cl.isLooped;
                drawerData_.clipLoopLengthBars = cl.loopLengthBars;
                drawerData_.clipTranspose = cl.transposeSemitones;
            }
            drawerData_.syncKnobsIfEmpty();
        }
        if (propertiesDrawer_.handlePointer(ev, drawerData_, ctx)) {
            inspectorOpen_ = propertiesDrawer_.isExpanded();
            return true;
        }
    }

    // 2. Timeline Minimap Scrubbing / Jump
    if (minimapBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down || (ev.action == PointerAction::Move && dragMode_ == ClipDragMode::MinimapScrub)) {
            dragMode_ = ClipDragMode::MinimapScrub;
            float totalArrangerW = static_cast<float>(totalBars_) * barWidth_;
            float normX = (ev.x - minimapBounds_.x) / minimapBounds_.w;
            scrollX_ = std::clamp(normX * totalArrangerW - (gridBounds_.w * 0.5f), 0.0f, totalArrangerW - gridBounds_.w);
            return true;
        } else if (ev.action == PointerAction::Up) {
            dragMode_ = ClipDragMode::None;
            return true;
        }
    }

    // 3. Time Ruler Scrubbing
    if (rulerBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down || (ev.action == PointerAction::Move && dragMode_ == ClipDragMode::PlayheadScrub)) {
            dragMode_ = ClipDragMode::PlayheadScrub;
            float localX = ev.x - rulerBounds_.x + scrollX_;
            float clickedStep = std::max(0.0f, (localX / barWidth_) * 16.0f);
            if (ctx.audioEngine) {
                ctx.audioEngine->getSequencer().getTransport().setPosition(static_cast<uint32_t>(clickedStep));
            }
            return true;
        } else if (ev.action == PointerAction::Up) {
            dragMode_ = ClipDragMode::None;
            return true;
        }
    }

    // 3b. Chord Track Lane Header Card Click (Opens Circle of Fifths dialog)
    if (chordHeaderBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            circleOfFifthsDialog_.open(0, songKeyRoot_, isSongKeyMinor_);
            return true;
        }
    }

    // 3c. Chord Track Lane Interaction (Audition on click, Edit in Circle of Fifths on double-click)
    if (chordLaneBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            float localX = ev.x - chordLaneBounds_.x + scrollX_;
            int clickedBar = static_cast<int>(localX / barWidth_);

            for (size_t i = 0; i < chordTrack_.size(); ++i) {
                const auto& chord = chordTrack_[i];
                float cx = static_cast<float>(chord.startBar) * barWidth_;
                float cw = chord.barLength * barWidth_;

                if (localX >= cx && localX <= cx + cw) {
                    selectedChordIndex_ = static_cast<int>(i);

                    // Audition chord
                    if (onAuditionChord) {
                        onAuditionChord(chord);
                    }

                    // Check double click -> Open Circle of Fifths for this chord
                    auto now = std::chrono::steady_clock::now();
                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastOverviewClickTime_).count();
                    if (lastClickedOverviewChordIdx_ == static_cast<int>(i) && elapsedMs < 400) {
                        lastOverviewClickTime_ = std::chrono::steady_clock::time_point{};
                        lastClickedOverviewChordIdx_ = -1;
                        circleOfFifthsDialog_.openForChord(chord, songKeyRoot_, isSongKeyMinor_);
                        return true;
                    }
                    lastOverviewClickTime_ = now;
                    lastClickedOverviewChordIdx_ = static_cast<int>(i);
                    return true;
                }
            }

            // Clicked empty lane space
            selectedChordIndex_ = -1;
            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastOverviewClickTime_).count();
            if (lastClickedOverviewChordIdx_ == -2 && elapsedMs < 400) {
                lastOverviewClickTime_ = std::chrono::steady_clock::time_point{};
                lastClickedOverviewChordIdx_ = -1;
                circleOfFifthsDialog_.open(static_cast<uint32_t>(std::max(0, clickedBar)), songKeyRoot_, isSongKeyMinor_);
                return true;
            }
            lastOverviewClickTime_ = now;
            lastClickedOverviewChordIdx_ = -2;
            return true;
        }
    }

    // 4. Track Headers Clicking & Volume/Pan Dragging
    if (tracksListBounds_.contains(ev.x, ev.y)) {
        // '+ ADD TRACK' Row Click
        if (addTrackRowBounds_.contains(ev.x, ev.y)) {
            if (ev.action == PointerAction::Down) {
                pluginDialog_.open(PluginDialogMode::AddInstrument, "", static_cast<uint32_t>(tracks_.size()));
                return true;
            }
        }

        if (ev.action == PointerAction::Down) {
            float localY = ev.y - tracksListBounds_.y + scrollY_;
            int clickedTrack = static_cast<int>(localY / trackRowHeight_);
            if (clickedTrack >= 0 && clickedTrack < static_cast<int>(tracks_.size())) {
                activeTrackIndex_ = static_cast<uint32_t>(clickedTrack);
                selectedClipIndex_ = -1;
                inspectorTab_ = ArrangerInspectorTab::Track; // Expose Track properties
                inspectorOpen_ = true;
                if (onTrackSelected) onTrackSelected(activeTrackIndex_);

                float rowY = tracksListBounds_.y + static_cast<float>(clickedTrack) * trackRowHeight_ - scrollY_;
                float btnY = rowY + 8.0f;

                auto openTrackTitleEdit = [this, clickedTrack, &ctx]() {
                    if (ctx.onOpenValueEdit) {
                        ValueEditRequest req;
                        req.title = "EDIT TRACK PROPERTIES";
                        req.paramName = "Track Name";
                        req.isTextMode = true;
                        req.initialText = tracks_[clickedTrack].name;
                        req.accentColor = Color(tracks_[clickedTrack].r, tracks_[clickedTrack].g, tracks_[clickedTrack].b, 1.0f);
                        req.onCommitText = [this, clickedTrack](const std::string& newName) {
                            if (!newName.empty()) {
                                tracks_[clickedTrack].name = newName;
                                drawerData_.trackName = newName;
                                if (clickedTrack < drawerData_.allTrackNames.size()) {
                                    drawerData_.allTrackNames[clickedTrack] = newName;
                                }
                                if (onTrackRename) {
                                    onTrackRename(clickedTrack, newName);
                                }
                            }
                        };
                        ctx.onOpenValueEdit(req);
                    }
                };

                Rect2D muteRect(tracksListBounds_.x + tracksListBounds_.w - 42.0f, btnY, 16.0f, 13.0f);
                Rect2D soloRect(tracksListBounds_.x + tracksListBounds_.w - 22.0f, btnY, 16.0f, 13.0f);

                if (muteRect.contains(ev.x, ev.y)) {
                    tracks_[clickedTrack].mute = !tracks_[clickedTrack].mute;
                    if (onMuteToggled) onMuteToggled(clickedTrack, tracks_[clickedTrack].mute);
                    return true;
                } else if (soloRect.contains(ev.x, ev.y)) {
                    tracks_[clickedTrack].solo = !tracks_[clickedTrack].solo;
                    if (onSoloToggled) onSoloToggled(clickedTrack, tracks_[clickedTrack].solo);
                    return true;
                }

                // Volume Slider (moved down by 10 pixels)
                float sliderW = tracksListBounds_.w - 75.0f;
                float sliderX = tracksListBounds_.x + 36.0f;
                float sliderY = rowY + 44.0f;
                Rect2D volBox(sliderX - 6.0f, sliderY - 6.0f, sliderW + 12.0f, 16.0f);

                // Pan Knob (moved down by 10 pixels)
                float knobX = tracksListBounds_.x + tracksListBounds_.w - 22.0f;
                float knobY = rowY + 46.0f;
                Rect2D panBox(knobX - 11.0f, knobY - 11.0f, 22.0f, 22.0f);

                if (ev.button == PointerButton::Right && !volBox.contains(ev.x, ev.y) && !panBox.contains(ev.x, ev.y)) {
                    openTrackTitleEdit();
                    return true;
                }

                if (volBox.contains(ev.x, ev.y)) {
                    if (ev.button == PointerButton::Right) {
                        if (ctx.onOpenValueEdit) {
                            ValueEditRequest req;
                            req.title = tracks_[clickedTrack].name + " VOLUME";
                            req.paramName = tracks_[clickedTrack].name + " Volume";
                            req.currentValue = tracks_[clickedTrack].volume;
                            req.minValue = 0.0f;
                            req.maxValue = 1.5f;
                            req.defaultValue = 1.0f;
                            req.hasDefault = true;
                            req.allowPercentage = true;
                            req.accentColor = Color(tracks_[clickedTrack].r, tracks_[clickedTrack].g, tracks_[clickedTrack].b, 1.0f);
                            req.onCommit = [this, clickedTrack](float val) {
                                tracks_[clickedTrack].volume = val;
                                if (onVolumeChanged) onVolumeChanged(clickedTrack, val);
                            };
                            ctx.onOpenValueEdit(req);
                        }
                        return true;
                    }
                    dragMode_ = ClipDragMode::TrackVolume;
                    dragOrigTrackIdx_ = clickedTrack;
                    dragStartPointerX_ = ev.x;
                    dragStartPointerY_ = ev.y;
                    dragStartVal_ = tracks_[clickedTrack].volume;
                    float norm = std::clamp((ev.x - sliderX) / sliderW, 0.0f, 1.0f);
                    tracks_[clickedTrack].volume = norm * 1.5f;
                    if (onVolumeChanged) onVolumeChanged(clickedTrack, tracks_[clickedTrack].volume);
                    return true;
                }

                if (panBox.contains(ev.x, ev.y)) {
                    if (ev.button == PointerButton::Right) {
                        if (ctx.onOpenValueEdit) {
                            ValueEditRequest req;
                            req.title = tracks_[clickedTrack].name + " PAN";
                            req.paramName = tracks_[clickedTrack].name + " Pan";
                            req.currentValue = tracks_[clickedTrack].pan;
                            req.minValue = -1.0f;
                            req.maxValue = 1.0f;
                            req.defaultValue = 0.0f;
                            req.hasDefault = true;
                            req.allowPercentage = false;
                            req.accentColor = Color(tracks_[clickedTrack].r, tracks_[clickedTrack].g, tracks_[clickedTrack].b, 1.0f);
                            req.onCommit = [this, clickedTrack](float val) {
                                tracks_[clickedTrack].pan = val;
                                if (onPanChanged) onPanChanged(clickedTrack, val);
                            };
                            ctx.onOpenValueEdit(req);
                        }
                        return true;
                    }
                    dragMode_ = ClipDragMode::TrackPan;
                    dragOrigTrackIdx_ = clickedTrack;
                    dragStartPointerX_ = ev.x;
                    dragStartPointerY_ = ev.y;
                    dragStartVal_ = tracks_[clickedTrack].pan;
                    return true;
                }

                if (ev.button == PointerButton::Right) {
                    openTrackTitleEdit();
                    return true;
                }

                return true;
            }
        }
    }

    // 5. Middle-Click Pan anywhere in Arranger Grid
    if (ev.button == PointerButton::Middle) {
        if (ev.action == PointerAction::Down) {
            dragMode_ = ClipDragMode::ViewportPan;
            dragStartPointerX_ = ev.x;
            dragStartPointerY_ = ev.y;
            dragStartScrollX_ = scrollX_;
            dragStartScrollY_ = scrollY_;
            return true;
        } else if (ev.action == PointerAction::Move && dragMode_ == ClipDragMode::ViewportPan) {
            scrollX_ = std::max(0.0f, dragStartScrollX_ - (ev.x - dragStartPointerX_));
            scrollY_ = std::max(0.0f, dragStartScrollY_ - (ev.y - dragStartPointerY_));
            return true;
        } else if (ev.action == PointerAction::Up && dragMode_ == ClipDragMode::ViewportPan) {
            dragMode_ = ClipDragMode::None;
            return true;
        }
    }

    // 6. Clips Interaction (Move across tracks & Loop Resize & Standard Resize)
    if (gridBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            float localX = ev.x - gridBounds_.x + scrollX_;
            float localY = ev.y - gridBounds_.y + scrollY_;
            int clickedTrack = static_cast<int>(localY / trackRowHeight_);

            if (clickedTrack >= 0 && clickedTrack < static_cast<int>(tracks_.size())) {
                auto& track = tracks_[clickedTrack];
                for (int c = static_cast<int>(track.clips.size()) - 1; c >= 0; --c) {
                    const auto& clip = track.clips[c];
                    float cx = static_cast<float>(clip.startBar - 1) * barWidth_;
                    float cw = static_cast<float>(clip.lengthBars) * barWidth_;
                    float cy = static_cast<float>(clickedTrack) * trackRowHeight_;
                    float ch = trackRowHeight_;

                    if (localX >= cx && localX <= cx + cw && localY >= cy && localY <= cy + ch) {
                        activeTrackIndex_ = static_cast<uint32_t>(clickedTrack);
                        selectedClipIndex_ = c;
                        inspectorTab_ = ArrangerInspectorTab::Clip; // Expose Clip properties!
                        inspectorOpen_ = true;
                        if (onTrackSelected) onTrackSelected(activeTrackIndex_);

                        // Check double click on clip body (not on resize handles)
                        bool onResizeHandle = (localX >= cx + cw - 22.0f);
                        if (!onResizeHandle) {
                            auto now = std::chrono::steady_clock::now();
                            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClipClickTime_).count();
                            if (lastClickedClipIdx_ == c && lastClickedClipTrack_ == clickedTrack && elapsedMs < 350) {
                                lastClipClickTime_ = std::chrono::steady_clock::time_point{};
                                lastClickedClipIdx_ = -1;
                                lastClickedClipTrack_ = -1;
                                dragMode_ = ClipDragMode::None;
                                if (ctx.onJumpToClipEdit) {
                                    ctx.onJumpToClipEdit(activeTrackIndex_, selectedClipIndex_);
                                }
                                return true;
                            }
                            lastClipClickTime_ = now;
                            lastClickedClipIdx_ = c;
                            lastClickedClipTrack_ = clickedTrack;
                        }

                        dragStartPointerX_ = ev.x;
                        dragStartPointerY_ = ev.y;
                        dragOrigStartBar_ = clip.startBar;
                        dragOrigLengthBars_ = clip.lengthBars;
                        dragOrigTrackIdx_ = static_cast<uint32_t>(clickedTrack);
                        dragClipIdx_ = c;

                        // Check Top-Right Loop Resize Handle (right 22px, top 18px)
                        if (localX >= cx + cw - 22.0f && localY <= cy + 20.0f) {
                            dragMode_ = ClipDragMode::LoopResize;
                        }
                        // Check Bottom-Right Standard Resize Handle (right 22px, bottom part)
                        else if (localX >= cx + cw - 22.0f) {
                            dragMode_ = ClipDragMode::ResizeRight;
                        }
                        // Otherwise: Move clip horizontally & vertically across tracks!
                        else {
                            dragMode_ = ClipDragMode::Move;
                        }
                        return true;
                    }
                }
            }

            // Clicked empty grid space -> deselect clip, switch to Track properties
            selectedClipIndex_ = -1;
            if (clickedTrack >= 0 && clickedTrack < static_cast<int>(tracks_.size())) {
                activeTrackIndex_ = static_cast<uint32_t>(clickedTrack);
                inspectorTab_ = ArrangerInspectorTab::Track;
                if (onTrackSelected) onTrackSelected(activeTrackIndex_);

                uint32_t clickedBar = std::max(1u, static_cast<uint32_t>(localX / barWidth_) + 1);
                auto now = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEmptyClickTime_).count();
                if (lastEmptyClickedTrack_ == clickedTrack &&
                    std::abs(static_cast<int>(lastEmptyClickedBar_) - static_cast<int>(clickedBar)) <= 1 &&
                    elapsedMs < 400) {
                    lastEmptyClickTime_ = std::chrono::steady_clock::time_point{};
                    lastEmptyClickedTrack_ = -1;
                    lastEmptyClickedBar_ = 0;
                    dragMode_ = ClipDragMode::None;
                    createEmptyClipAt(static_cast<uint32_t>(clickedTrack), clickedBar);
                    return true;
                }
                lastEmptyClickTime_ = now;
                lastEmptyClickedTrack_ = clickedTrack;
                lastEmptyClickedBar_ = clickedBar;
            }

            if (ev.type == PointerType::Touch) {
                dragMode_ = ClipDragMode::TouchGridPending;
                touchDownPos_ = Point2D{ev.x, ev.y};
                touchDownTimePoint_ = std::chrono::steady_clock::now();
                touchPanCommitted_ = false;
                dragStartPointerX_ = ev.x;
                dragStartPointerY_ = ev.y;
                dragStartScrollX_ = scrollX_;
                dragStartScrollY_ = scrollY_;
                kineticScroller_.reset();
                kineticScroller_.addSample(ev.x, ev.y, ev.timestampMs);
            }
            return true;
        }

        if (ev.action == PointerAction::Move) {
            if (dragMode_ == ClipDragMode::TouchGridPending) {
                float dist = std::hypot(ev.x - touchDownPos_.x, ev.y - touchDownPos_.y);
                if (dist > 14.0f) {
                    touchPanCommitted_ = true;
                    dragMode_ = ClipDragMode::TouchPan;
                    kineticScroller_.addSample(ev.x, ev.y, ev.timestampMs);
                }
            }

            if (dragMode_ == ClipDragMode::TouchPan) {
                kineticScroller_.addSample(ev.x, ev.y, ev.timestampMs);
                scrollX_ = (std::max)(0.0f, dragStartScrollX_ - (ev.x - dragStartPointerX_));
                float maxScrollY = (std::max)(0.0f, static_cast<float>(tracks_.size()) * trackRowHeight_ - gridBounds_.h);
                scrollY_ = std::clamp(dragStartScrollY_ - (ev.y - dragStartPointerY_), 0.0f, maxScrollY);
                return true;
            }
            if (dragClipIdx_ >= 0 && dragOrigTrackIdx_ < tracks_.size()) {
                float dx = ev.x - dragStartPointerX_;
                float dy = ev.y - dragStartPointerY_;
                int deltaBars = static_cast<int>(std::round(dx / barWidth_));
                int deltaTracks = static_cast<int>(std::round(dy / trackRowHeight_));

                // 6a. Move Clip Horizontally and Vertically across tracks
                if (dragMode_ == ClipDragMode::Move) {
                    uint32_t targetTrackIdx = static_cast<uint32_t>(std::clamp(static_cast<int>(dragOrigTrackIdx_) + deltaTracks,
                                                                               0, static_cast<int>(tracks_.size()) - 1));
                    int targetStartBar = std::max(1, static_cast<int>(dragOrigStartBar_) + deltaBars);

                    if (targetTrackIdx != activeTrackIndex_) {
                        // Move clip to new track
                        auto clipData = tracks_[activeTrackIndex_].clips[dragClipIdx_];
                        tracks_[activeTrackIndex_].clips.erase(tracks_[activeTrackIndex_].clips.begin() + dragClipIdx_);

                        clipData.trackIndex = targetTrackIdx;
                        clipData.startBar = static_cast<uint32_t>(targetStartBar);
                        tracks_[targetTrackIdx].clips.push_back(clipData);

                        activeTrackIndex_ = targetTrackIdx;
                        dragClipIdx_ = static_cast<int>(tracks_[targetTrackIdx].clips.size() - 1);
                        selectedClipIndex_ = dragClipIdx_;
                    } else {
                        tracks_[activeTrackIndex_].clips[dragClipIdx_].startBar = static_cast<uint32_t>(targetStartBar);
                    }
                    return true;
                }

                // 6b. Loop Resize (from upper right loop icon)
                if (dragMode_ == ClipDragMode::LoopResize) {
                    int newLen = std::max(1, static_cast<int>(dragOrigLengthBars_) + deltaBars);
                    auto& clip = tracks_[activeTrackIndex_].clips[dragClipIdx_];
                    clip.lengthBars = static_cast<uint32_t>(newLen);
                    if (clip.lengthBars > dragOrigLengthBars_) {
                        clip.isLooped = true;
                        clip.loopLengthBars = dragOrigLengthBars_;
                    } else {
                        clip.isLooped = false;
                    }
                    return true;
                }

                // 6c. Standard Edge Resize Right
                if (dragMode_ == ClipDragMode::ResizeRight) {
                    int newLen = std::max(1, static_cast<int>(dragOrigLengthBars_) + deltaBars);
                    auto& clip = tracks_[activeTrackIndex_].clips[dragClipIdx_];
                    clip.lengthBars = static_cast<uint32_t>(newLen);
                    return true;
                }
            }
        }

        if (ev.action == PointerAction::Up) {
            dragMode_ = ClipDragMode::None;
            dragClipIdx_ = -1;
            return true;
        }
    }

    return false;
}

bool ArrangerView::handleKey(int key, [[maybe_unused]] int scancode, int action, int mods, [[maybe_unused]] const ViewContext& ctx) {
    if (pluginDialog_.isOpen()) {
        return pluginDialog_.handleKey(key, scancode, action, mods);
    }
    if (propertiesDrawer_.isPluginDialogOpen()) {
        return propertiesDrawer_.handleKey(key, scancode, action, mods, ctx);
    }
    if (circleOfFifthsDialog_.isOpen()) {
        return circleOfFifthsDialog_.handleKey(key, scancode, action, mods);
    }
    if (iconDialog_.isOpen()) {
        return iconDialog_.handleKey(key, scancode, action, mods);
    }

    if (action != 1 && action != 2) return false;

    // 'M' -> Toggle docked bottom mixer drawer
    if (mixerDrawer_.handleKey(key, scancode, action, mods, ctx)) {
        return true;
    }

    // 'F' -> Toggle continuous playback follow mode
    if (key == 'F' || key == 'f') {
        toggleFollowPlayback();
        return true;
    }

    // Ctrl+D -> Duplicate Selected Clip
    if ((key == 'D' || key == 'd') && (mods & 2)) {
        if (activeTrackIndex_ < tracks_.size() && selectedClipIndex_ >= 0) {
            int cIdx = selectedClipIndex_;
            duplicateClip(activeTrackIndex_, cIdx);
            if (onDuplicateClip) onDuplicateClip(activeTrackIndex_, cIdx);
            return true;
        }
    }

    // Delete or Backspace -> Delete Selected Clip
    if (key == 261 || key == 259) {
        if (activeTrackIndex_ < tracks_.size() && selectedClipIndex_ >= 0) {
            int cIdx = selectedClipIndex_;
            deleteClip(activeTrackIndex_, cIdx);
            if (onDeleteClip) onDeleteClip(activeTrackIndex_, cIdx);
            return true;
        }
    }

    return false;
}

bool ArrangerView::handleChar(char32_t codepoint, [[maybe_unused]] const ViewContext& ctx) {
    if (pluginDialog_.isOpen()) {
        return pluginDialog_.handleChar(codepoint);
    }
    if (propertiesDrawer_.isPluginDialogOpen()) {
        return propertiesDrawer_.handleChar(codepoint);
    }
    return false;
}

bool ArrangerView::handleFileDrop(const std::vector<std::string>& filePaths, float x, float y, const ViewContext& ctx) {
    if (filePaths.empty()) return false;

    // Check if drop occurred within tracks list, timeline grid, or overall arranger bounds
    if (bounds_.w > 0.0f && bounds_.h > 0.0f && !bounds_.contains(x, y)) {
        return false;
    }

    bool handledAny = false;

    for (const auto& path : filePaths) {
        std::filesystem::path fsPath(path);
        std::string filename = fsPath.filename().string();
        std::string ext = fsPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        bool isAudio = (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".aif" || ext == ".aiff");
        bool isMidi = (ext == ".mid" || ext == ".midi");
        bool isSf2 = (ext == ".sf2");

        if (!isAudio && !isMidi && !isSf2) {
            continue;
        }

        // Determine drop location in timeline grid
        uint32_t targetBar = 1;
        if (gridBounds_.w > 0.0f && x >= gridBounds_.x) {
            float relX = x - gridBounds_.x + scrollX_;
            targetBar = static_cast<uint32_t>(std::max(1.0f, std::floor(relX / barWidth_) + 1.0f));
        }

        // Determine target track from Y position
        int targetTrackIdx = -1;
        if (gridBounds_.h > 0.0f && y >= gridBounds_.y) {
            float relY = y - gridBounds_.y + scrollY_;
            int idx = static_cast<int>(relY / trackRowHeight_);
            if (idx >= 0 && idx < static_cast<int>(tracks_.size())) {
                targetTrackIdx = idx;
            }
        }

        std::string stemName = fsPath.stem().string();

        if (isAudio || isSf2) {
            if (targetTrackIdx >= 0) {
                // Add an audio clip to existing track at dropped bar
                auto& track = tracks_[targetTrackIdx];
                ArrangerTimelineClip clip;
                clip.id = "clip_audio_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
                clip.name = stemName;
                clip.trackIndex = static_cast<uint32_t>(targetTrackIdx);
                clip.startBar = targetBar;
                clip.lengthBars = 4;
                clip.r = track.r; clip.g = track.g; clip.b = track.b;
                clip.isAudio = true;
                clip.isSelected = true;
                track.clips.push_back(clip);
                selectedClipIndex_ = static_cast<int>(track.clips.size() - 1);
                activeTrackIndex_ = static_cast<uint32_t>(targetTrackIdx);
            } else {
                // Create a new Audio / Sampler Track
                ArrangerTimelineTrack newTrack;
                newTrack.name = stemName;
                newTrack.instrument = isSf2 ? "SoundFont Sampler" : "Audio Sampler";
                newTrack.instrumentEngine = isSf2 ? "soundfont" : "sampler";
                newTrack.iconRef = isSf2 ? "preset:inst_keys" : "preset:inst_sampler";
                int colorIdx = static_cast<int>(tracks_.size()) % 8;
                newTrack.r = kQuickPalette[colorIdx][0];
                newTrack.g = kQuickPalette[colorIdx][1];
                newTrack.b = kQuickPalette[colorIdx][2];

                ArrangerTimelineClip clip;
                clip.id = "clip_audio_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
                clip.name = stemName;
                clip.trackIndex = static_cast<uint32_t>(tracks_.size());
                clip.startBar = targetBar;
                clip.lengthBars = 4;
                clip.r = newTrack.r; clip.g = newTrack.g; clip.b = newTrack.b;
                clip.isAudio = true;
                clip.isSelected = true;
                newTrack.clips.push_back(clip);

                tracks_.push_back(newTrack);
                activeTrackIndex_ = static_cast<uint32_t>(tracks_.size() - 1);
                selectedClipIndex_ = 0;
            }
            if (ctx.onShowNotification) {
                ctx.onShowNotification("Imported " + filename + " at Bar " + std::to_string(targetBar));
            }
            handledAny = true;
        } else if (isMidi) {
            if (targetTrackIdx >= 0) {
                auto& track = tracks_[targetTrackIdx];
                ArrangerTimelineClip clip;
                clip.id = "clip_midi_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
                clip.name = stemName;
                clip.trackIndex = static_cast<uint32_t>(targetTrackIdx);
                clip.startBar = targetBar;
                clip.lengthBars = 4;
                clip.r = track.r; clip.g = track.g; clip.b = track.b;
                clip.isAudio = false;
                clip.isSelected = true;
                track.clips.push_back(clip);
                selectedClipIndex_ = static_cast<int>(track.clips.size() - 1);
                activeTrackIndex_ = static_cast<uint32_t>(targetTrackIdx);
            } else {
                ArrangerTimelineTrack newTrack;
                newTrack.name = stemName;
                newTrack.instrument = "Polyphonic Synth";
                newTrack.instrumentEngine = "poly_synth";
                newTrack.iconRef = "preset:inst_synth";
                int colorIdx = static_cast<int>(tracks_.size()) % 8;
                newTrack.r = kQuickPalette[colorIdx][0];
                newTrack.g = kQuickPalette[colorIdx][1];
                newTrack.b = kQuickPalette[colorIdx][2];

                ArrangerTimelineClip clip;
                clip.id = "clip_midi_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
                clip.name = stemName;
                clip.trackIndex = static_cast<uint32_t>(tracks_.size());
                clip.startBar = targetBar;
                clip.lengthBars = 4;
                clip.r = newTrack.r; clip.g = newTrack.g; clip.b = newTrack.b;
                clip.isAudio = false;
                clip.isSelected = true;
                newTrack.clips.push_back(clip);

                tracks_.push_back(newTrack);
                activeTrackIndex_ = static_cast<uint32_t>(tracks_.size() - 1);
                selectedClipIndex_ = 0;
            }
            if (ctx.onShowNotification) {
                ctx.onShowNotification("Imported MIDI " + filename + " at Bar " + std::to_string(targetBar));
            }
            handledAny = true;
        }
    }

    if (handledAny) {
        if (onClipsChanged) onClipsChanged();
    }

    return handledAny;
}

} // namespace eatsbits::ui
