#include "eatsbits/ui/views/arranger_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <algorithm>
#include <cmath>

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
    t1.iconRef = "preset:inst_synth";
    t1.r = 1.0f; t1.g = 0.55f; t1.b = 0.0f;
    t1.knob1 = 0.65f; t1.knob1Name = "CUTOFF";
    t1.knob2 = 0.85f; t1.knob2Name = "RESONANCE";
    t1.knob3 = 0.45f; t1.knob3Name = "DECAY";
    t1.knob4 = 0.80f; t1.knob4Name = "ACCENT";

    ArrangerTimelineClip c1{"c1", "Acid Pattern A", 0, 1, 4, 1.0f, 0.55f, 0.0f, false, false, false, 4, 0, 1.0f, false};
    c1.notes = {
        {36, 0.0f, 1.0f, 0.95f}, {36, 2.0f, 1.0f, 0.70f}, {48, 3.0f, 0.75f, 1.00f},
        {39, 4.0f, 1.0f, 0.80f}, {41, 6.0f, 1.5f, 0.90f}, {43, 8.0f, 1.0f, 0.85f},
        {36, 10.0f, 1.0f, 0.92f}, {46, 12.0f, 0.5f, 0.95f}, {48, 14.0f, 1.5f, 1.00f}
    };
    t1.clips.push_back(c1);

    ArrangerTimelineClip c2{"c2", "Acid Pattern B", 0, 5, 4, 1.0f, 0.55f, 0.0f, false, false, false, 4, 0, 1.0f, false};
    c2.notes = {
        {36, 0.0f, 0.75f, 0.85f}, {48, 1.0f, 0.5f, 1.00f}, {36, 2.0f, 1.0f, 0.65f},
        {41, 4.0f, 1.0f, 0.90f}, {44, 6.0f, 1.0f, 0.85f}, {46, 8.0f, 1.5f, 0.95f},
        {48, 11.0f, 0.5f, 1.00f}, {39, 12.0f, 1.0f, 0.75f}, {41, 14.0f, 1.0f, 0.85f}
    };
    t1.clips.push_back(c2);

    t1.midiFx.push_back({"Scale Snap", "SCALE_SNAP", 0, 0, true});
    t1.audioFx.push_back({"Tube Distortion", "TUBE_DISTORTION", 0.65f, 0.80f, true});

    // 2. TR-808 Rhythm Kit
    ArrangerTimelineTrack t2;
    t2.name = "TR-808 Kit";
    t2.instrument = "Analog 808";
    t2.iconRef = "preset:drum_machine";
    t2.r = 0.13f; t2.g = 0.96f; t2.b = 0.91f;
    t2.knob1 = 0.70f; t2.knob1Name = "TONE";
    t2.knob2 = 0.80f; t2.knob2Name = "SNAPPY";
    t2.knob3 = 0.60f; t2.knob3Name = "DECAY";
    t2.knob4 = 0.50f; t2.knob4Name = "TUNING";

    ArrangerTimelineClip c3{"c3", "808 Beat 01", 1, 1, 8, 0.13f, 0.96f, 0.91f, false, false, true, 4, 0, 1.0f, false};
    c3.notes = {
        {36, 0.0f, 0.75f, 1.00f}, {42, 0.0f, 0.4f, 0.75f}, {42, 1.0f, 0.4f, 0.45f},
        {42, 2.0f, 0.4f, 0.70f}, {42, 3.0f, 0.4f, 0.40f}, {38, 4.0f, 0.8f, 0.95f},
        {42, 4.0f, 0.4f, 0.80f}, {42, 5.0f, 0.4f, 0.50f}, {42, 6.0f, 0.4f, 0.70f},
        {36, 7.0f, 0.6f, 0.85f}, {36, 8.0f, 0.75f, 1.00f}, {42, 8.0f, 0.4f, 0.85f},
        {42, 9.0f, 0.4f, 0.45f}, {36, 10.5f, 0.5f, 0.75f}, {42, 10.0f, 0.4f, 0.70f},
        {38, 12.0f, 0.8f, 1.00f}, {42, 12.0f, 0.4f, 0.80f}, {42, 14.0f, 0.4f, 0.75f},
        {46, 15.0f, 0.8f, 0.65f}
    };
    t2.clips.push_back(c3);
    t2.audioFx.push_back({"Studio Dynamics", "DYNAMICS_COMP", 0.70f, 0.90f, true});

    // 3. TR-909 Groove Kit
    ArrangerTimelineTrack t3;
    t3.name = "TR-909 Drive";
    t3.instrument = "Analog 909";
    t3.iconRef = "preset:drum_kick";
    t3.r = 1.0f; t3.g = 0.16f; t3.b = 0.43f;
    t3.knob1 = 0.85f; t3.knob1Name = "ATTACK";
    t3.knob2 = 0.65f; t3.knob2Name = "PUNCH";
    t3.knob3 = 0.50f; t3.knob3Name = "TUNE";
    t3.knob4 = 0.60f; t3.knob4Name = "CRACK";

    ArrangerTimelineClip c4{"c4", "909 Groove", 2, 5, 4, 1.0f, 0.16f, 0.43f, false, false, false, 4, 0, 1.0f, false};
    c4.notes = {
        {36, 0.0f, 0.8f, 1.00f}, {42, 0.5f, 0.4f, 0.70f}, {36, 2.0f, 0.8f, 0.90f},
        {42, 2.5f, 0.4f, 0.65f}, {38, 4.0f, 0.8f, 0.95f}, {36, 4.0f, 0.8f, 1.00f},
        {46, 5.0f, 0.8f, 0.75f}, {36, 6.0f, 0.8f, 0.85f}, {36, 8.0f, 0.8f, 1.00f},
        {42, 8.5f, 0.4f, 0.70f}, {36, 10.0f, 0.8f, 0.90f}, {38, 12.0f, 0.8f, 1.00f},
        {46, 13.0f, 0.8f, 0.80f}, {36, 14.0f, 0.8f, 0.90f}, {42, 15.0f, 0.4f, 0.60f}
    };
    t3.clips.push_back(c4);

    // 4. DX7 Rhodes
    ArrangerTimelineTrack t4;
    t4.name = "DX7 Rhodes";
    t4.instrument = "Yamaha DX7 6-Op FM";
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

    // 5. Concert Grand Piano (Waveguide Physical Modeling)
    ArrangerTimelineTrack t5;
    t5.name = "Concert Grand";
    t5.instrument = "Waveguide Grand Piano";
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
    t.iconRef = "preset:inst_synth";
    t.r = r; t.g = g; t.b = b;
    // Add default initial clip
    std::string cid = "clip_" + std::to_string(tracks_.size() + 1);
    t.clips.push_back({cid, name + " Clip", static_cast<uint32_t>(tracks_.size()), 1, 4, r, g, b, false, false, false, 4, 0, 1.0f, false});
    tracks_.push_back(t);
    activeTrackIndex_ = static_cast<uint32_t>(tracks_.size() - 1);
    selectedClipIndex_ = -1;
    inspectorTab_ = ArrangerInspectorTab::Track;
    inspectorOpen_ = true;
}

void ArrangerView::addMidiFxToTrack(uint32_t trackIdx, const std::string& name, const std::string& type) {
    if (trackIdx < tracks_.size()) {
        tracks_[trackIdx].midiFx.push_back({name, type, 0, 0, true});
    }
}

void ArrangerView::addAudioFxToTrack(uint32_t trackIdx, const std::string& name, const std::string& type) {
    if (trackIdx < tracks_.size()) {
        tracks_[trackIdx].audioFx.push_back({name, type, 0.5f, 0.5f, true});
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
    }
    track.chordFollowMode = theory::ChordFollowMode::Off;
}

void ArrangerView::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;
    trackHeaderWidth_ = ctx.isMobile ? 130.0f : 190.0f;
    inspectorWidth_ = ctx.isMobile ? 260.0f : 320.0f;

    float inspW = inspectorOpen_ ? inspectorWidth_ : 24.0f;
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
    inspectorBounds_ = Rect2D(bounds_.x + mainW, bounds_.y, inspW, bounds_.h);

    // + ADD track row directly below the last track
    float addRowY = tracksListBounds_.y + static_cast<float>(tracks_.size()) * trackRowHeight_ - scrollY_;
    addTrackRowBounds_ = Rect2D(tracksListBounds_.x + 10.0f, addRowY + 8.0f, trackHeaderWidth_ - 20.0f, 32.0f);

    pluginDialog_.layout(bounds_.w, bounds_.h);
    circleOfFifthsDialog_.layout(bounds_.w, bounds_.h);
    iconDialog_.layout(bounds_.w, bounds_.h);
}

void ArrangerView::render(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

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
    if (pluginDialog_.isOpen()) {
        pluginDialog_.render(r, theme);
    }
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

    // 1. Background strip for Chord Lane
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
        drawText(r, "+ Double-click or click [WHEEL] to insert chords / explore Circle of Fifths",
                 chordLaneBounds_.x + 20.0f, chordLaneBounds_.y + 9.0f, 10.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.65f);
        return;
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
        // Major: Vibrant Cyan / Teal, Minor: Purple / Magenta, Dominant: Amber, Dim/Aug: Crimson, Sus: Emerald
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

        // Right resize grip handle
        if (cw >= 24.0f) {
            float gripX = cx + cw - 7.0f;
            float gripH = ch * 0.4f;
            float gripY = cy + (ch - gripH) * 0.5f;
            drawLine(r, gripX, gripY, gripX, gripY + gripH, 1.0f, 1.0f, 1.0f, 0.35f, 1.5f);
            drawLine(r, gripX + 3.0f, gripY, gripX + 3.0f, gripY + gripH, 1.0f, 1.0f, 1.0f, 0.35f, 1.5f);
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
            float noteAreaH = ch - 22.0f;
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
                } else {
                    for (int step = 0; step < 16; ++step) {
                        if ((step % 2 == 0) || (step == 7) || (step == 11)) {
                            float nx = cycleStartX + static_cast<float>(step) * (patternW / 16.0f) + 1.5f;
                            float nw = std::max(4.0f, (patternW / 16.0f) - 3.0f);
                            if (nx + nw > cx + cw) continue;

                            float ny = noteAreaY + static_cast<float>((step * 3) % static_cast<int>(std::max(1.0f, noteAreaH - 6.0f)));
                            float vel = (step % 4 == 0) ? 1.0f : ((step == 7 || step == 11) ? 0.85f : 0.60f);
                            float intensity = 0.35f + 0.65f * vel;
                            float nr = std::clamp(clip.r * intensity, 0.0f, 1.0f);
                            float ng = std::clamp(clip.g * intensity, 0.0f, 1.0f);
                            float nb = std::clamp(clip.b * intensity, 0.0f, 1.0f);
                            float na = 0.50f + 0.50f * vel;

                            drawRect(r, nx, ny, nw, 5.0f, nr, ng, nb, na);
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

    // "CHORDS" Title
    drawText(r, "CHORDS", chordHeaderBounds_.x + 10.0f, chordHeaderBounds_.y + 8.5f, 10.5f,
             0.18f, 0.85f, 0.95f, 1.0f);

    // Song Key Badge (e.g. "C", "Am")
    std::string keyShort = std::string(theory::ChordTheory::pitchClassNames[songKeyRoot_]) +
                           (isSongKeyMinor_ ? "m" : "");
    float keyBadgeW = 34.0f;
    float keyBadgeH = 16.0f;
    float keyBadgeX = chordHeaderBounds_.x + 64.0f;
    float keyBadgeY = chordHeaderBounds_.y + 7.0f;
    drawRoundedRect(r, keyBadgeX, keyBadgeY, keyBadgeW, keyBadgeH, 3.0f, 0.14f, 0.16f, 0.22f, 0.9f);
    drawRoundedRectOutline(r, keyBadgeX, keyBadgeY, keyBadgeW, keyBadgeH, 3.0f, 0.18f, 0.85f, 0.95f, 0.7f, 1.0f);
    drawText(r, keyShort, keyBadgeX + (keyBadgeW - static_cast<float>(keyShort.length()) * 6.0f) * 0.5f,
             keyBadgeY + 3.0f, 8.5f, 1.0f, 1.0f, 1.0f, 1.0f);

    // "WHEEL" Button (Opens Circle of Fifths modal)
    float wheelBtnW = 46.0f;
    float wheelBtnH = 16.0f;
    float wheelBtnX = chordHeaderBounds_.x + chordHeaderBounds_.w - wheelBtnW - 8.0f;
    float wheelBtnY = chordHeaderBounds_.y + 7.0f;
    drawRoundedRect(r, wheelBtnX, wheelBtnY, wheelBtnW, wheelBtnH, 3.0f,
                    0.18f * 0.35f, 0.85f * 0.35f, 0.95f * 0.35f, 0.9f);
    drawRoundedRectOutline(r, wheelBtnX, wheelBtnY, wheelBtnW, wheelBtnH, 3.0f,
                           0.18f, 0.85f, 0.95f, 0.9f, 1.0f);
    drawText(r, "WHEEL", wheelBtnX + 6.0f, wheelBtnY + 3.0f, 8.5f,
             0.18f, 0.85f, 0.95f, 1.0f);

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

        // Track Name
        drawText(r, track.name, iconBoxX + iconBoxSize + 6.0f, rowY + 9.0f, 11.5f,
                 isActive ? theme.primaryAccent.r : theme.textPrimary.r,
                 isActive ? theme.primaryAccent.g : theme.textPrimary.g,
                 isActive ? theme.primaryAccent.b : theme.textPrimary.b, 1.0f);

        // CHORD FOLLOW, MUTE, SOLO Buttons
        float btnW = 16.0f;
        float btnH = 13.0f;
        float btnY = rowY + 8.0f;

        // Chord Follow Mode Button Chip (Off, Chord, Bass, Scale, ColorLead)
        float flwBtnW = 34.0f;
        float flwBtnH = 13.0f;
        float flwBtnX = tracksListBounds_.x + tracksListBounds_.w - 82.0f;
        bool hasFollow = (track.chordFollowMode != theory::ChordFollowMode::Off);
        const char* flwLabel = "OFF";
        float flwR = 0.20f, flwG = 0.20f, flwB = 0.22f;
        float flwTextR = theme.textMuted.r, flwTextG = theme.textMuted.g, flwTextB = theme.textMuted.b;
        if (track.chordFollowMode == theory::ChordFollowMode::Chord) {
            flwLabel = "CHRD";
            flwR = 0.15f; flwG = 0.45f; flwB = 0.65f;
            flwTextR = 0.30f; flwTextG = 0.85f; flwTextB = 1.0f;
        } else if (track.chordFollowMode == theory::ChordFollowMode::Bass) {
            flwLabel = "BASS";
            flwR = 0.45f; flwG = 0.25f; flwB = 0.60f;
            flwTextR = 0.85f; flwTextG = 0.60f; flwTextB = 1.0f;
        } else if (track.chordFollowMode == theory::ChordFollowMode::Scale) {
            flwLabel = "SCAL";
            flwR = 0.15f; flwG = 0.50f; flwB = 0.35f;
            flwTextR = 0.40f; flwTextG = 0.95f; flwTextB = 0.65f;
        } else if (track.chordFollowMode == theory::ChordFollowMode::ColorLead) {
            flwLabel = "LEAD";
            flwR = 0.55f; flwG = 0.35f; flwB = 0.12f;
            flwTextR = 1.0f; flwTextG = 0.80f; flwTextB = 0.30f;
        }

        drawRoundedRect(r, flwBtnX, btnY, flwBtnW, flwBtnH, 2.0f, flwR, flwG, flwB, 0.95f);
        if (hasFollow) {
            drawRoundedRectOutline(r, flwBtnX, btnY, flwBtnW, flwBtnH, 2.0f, flwTextR, flwTextG, flwTextB, 0.8f, 1.0f);
        }
        drawText(r, flwLabel, flwBtnX + 3.0f, btnY + 2.5f, 7.5f, flwTextR, flwTextG, flwTextB, 1.0f);

        // Mute
        drawRoundedRect(r, tracksListBounds_.x + tracksListBounds_.w - 42.0f, btnY, btnW, btnH, 2.0f,
                        track.mute ? 0.85f : 0.20f, 0.20f, 0.20f, 0.95f);
        drawText(r, "M", tracksListBounds_.x + tracksListBounds_.w - 38.0f, btnY + 2.5f, 8.0f, 1.0f, 1.0f, 1.0f, 1.0f);

        // Solo
        drawRoundedRect(r, tracksListBounds_.x + tracksListBounds_.w - 22.0f, btnY, btnW, btnH, 2.0f,
                        track.solo ? 0.95f : 0.20f, track.solo ? 0.80f : 0.20f, 0.10f, 0.95f);
        drawText(r, "S", tracksListBounds_.x + tracksListBounds_.w - 18.0f, btnY + 2.5f, 8.0f, 1.0f, 1.0f, 1.0f, 1.0f);

        // Volume Horizontal Pill Slider (VOL 85%)
        drawText(r, "VOL", tracksListBounds_.x + 14.0f, rowY + 30.0f, 8.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

        float sliderW = tracksListBounds_.w - 75.0f;
        float sliderH = 4.0f;
        float sliderX = tracksListBounds_.x + 36.0f;
        float sliderY = rowY + 34.0f;

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

        // Pan Knob with center detent 'C' indicator on right
        float knobX = tracksListBounds_.x + tracksListBounds_.w - 22.0f;
        float knobY = rowY + 36.0f;
        drawCircle(r, knobX, knobY, 9.0f, 0.15f, 0.16f, 0.18f, 1.0f);
        drawCircle(r, knobX, knobY, 7.5f, 0.08f, 0.08f, 0.09f, 1.0f);
        float pAngle = -1.57f + track.pan * 2.2f;
        drawLine(r, knobX, knobY, knobX + std::cos(pAngle) * 6.5f, knobY + std::sin(pAngle) * 6.5f,
                 track.r, track.g, track.b, 1.0f, 1.5f);
        drawText(r, "C", knobX - 3.0f, knobY + 11.0f, 8.0f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

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
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // Pull Tab
    float pullTabX = inspectorBounds_.x;
    float pullTabW = 22.0f;
    float pullTabH = 96.0f;
    float pullTabY = inspectorBounds_.y + 60.0f;
    drawRoundedRect(r, pullTabX - pullTabW, pullTabY, pullTabW, pullTabH, 4.0f,
                    theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.95f);
    drawRoundedRectOutline(r, pullTabX - pullTabW, pullTabY, pullTabW, pullTabH, 4.0f,
                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.8f, 1.0f);

    // Properties Icon at top of pull tab
    float iconW = 12.0f;
    float iconH = 12.0f;
    float iconX = pullTabX - pullTabW + (pullTabW - iconW) * 0.5f;
    float iconY = pullTabY + 9.0f;
    drawPropertiesIcon(r, iconX, iconY, iconW, iconH,
                       inspectorOpen_ ? theme.secondaryAccent : theme.primaryAccent);

    // "PROPERTIES" text rendered sideways (-90 deg: reads cleanly from bottom to top)
    float textCx = pullTabX - (pullTabW * 0.5f);
    float textCy = pullTabY + 56.0f;
    drawRotatedText(r, "PROPERTIES", textCx, textCy, -90.0f, 9.5f,
                    inspectorOpen_ ? theme.secondaryAccent : theme.primaryAccent);

    if (!inspectorOpen_) return;

    // Drawer Container Background
    drawRect(r, inspectorBounds_.x, inspectorBounds_.y, inspectorBounds_.w, inspectorBounds_.h,
             theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawLine(r, inspectorBounds_.x, inspectorBounds_.y, inspectorBounds_.x, inspectorBounds_.y + inspectorBounds_.h,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.85f, 1.5f);

    if (activeTrackIndex_ >= tracks_.size()) return;
    auto& track = tracks_[activeTrackIndex_];

    // Toggle Tabs Header: TRACK vs CLIP
    float tabW = (inspectorBounds_.w - 24.0f) * 0.5f;
    float tabY = inspectorBounds_.y + 10.0f;

    // Track Tab
    bool isTrk = (inspectorTab_ == ArrangerInspectorTab::Track);
    drawRoundedRect(r, inspectorBounds_.x + 10.0f, tabY, tabW, 26.0f, 4.0f,
                    isTrk ? theme.primaryAccent.r * 0.25f : theme.controlBackground.r,
                    isTrk ? theme.primaryAccent.g * 0.25f : theme.controlBackground.g,
                    isTrk ? theme.primaryAccent.b * 0.25f : theme.controlBackground.b, 0.9f);
    drawText(r, "TRACK PROPERTIES", inspectorBounds_.x + 16.0f, tabY + 7.0f, 9.5f,
             isTrk ? theme.primaryAccent.r : theme.textMuted.r,
             isTrk ? theme.primaryAccent.g : theme.textMuted.g,
             isTrk ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);

    // Clip Tab
    bool isClp = (inspectorTab_ == ArrangerInspectorTab::Clip);
    drawRoundedRect(r, inspectorBounds_.x + 14.0f + tabW, tabY, tabW, 26.0f, 4.0f,
                    isClp ? theme.primaryAccent.r * 0.25f : theme.controlBackground.r,
                    isClp ? theme.primaryAccent.g * 0.25f : theme.controlBackground.g,
                    isClp ? theme.primaryAccent.b * 0.25f : theme.controlBackground.b, 0.9f);
    drawText(r, "CLIP PROPERTIES", inspectorBounds_.x + 20.0f + tabW, tabY + 7.0f, 9.5f,
             isClp ? theme.primaryAccent.r : theme.textMuted.r,
             isClp ? theme.primaryAccent.g : theme.textMuted.g,
             isClp ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);

    drawLine(r, inspectorBounds_.x, tabY + 34.0f, inspectorBounds_.x + inspectorBounds_.w, tabY + 34.0f,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);

    if (isTrk) {
        renderTrackProperties(ctx, track);
    } else {
        if (selectedClipIndex_ >= 0 && selectedClipIndex_ < static_cast<int>(track.clips.size())) {
            renderClipProperties(ctx, track, track.clips[selectedClipIndex_]);
        } else {
            drawText(r, "Select a clip to view properties", inspectorBounds_.x + 20.0f, tabY + 60.0f, 11.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);
        }
    }
}

void ArrangerView::renderTrackProperties(const ViewContext& ctx, ArrangerTimelineTrack& track) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;
    float startY = inspectorBounds_.y + 50.0f;
    float cardW = inspectorBounds_.w - 20.0f;
    float cardX = inspectorBounds_.x + 10.0f;

    // 1. Track Title Card
    drawRoundedRect(r, cardX, startY, cardW, 38.0f, 5.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cardX, startY, cardW, 38.0f, 5.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
    drawText(r, track.name, cardX + 14.0f, startY + 11.0f, 13.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "[PENCIL]", cardX + cardW - 32.0f, startY + 11.0f, 10.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f);

    // 1b. Track Volume & Pan Card (Quick Controls)
    float mixCardY = startY + 44.0f;
    float mixCardH = 46.0f;
    drawRoundedRect(r, cardX, mixCardY, cardW, mixCardH, 5.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cardX, mixCardY, cardW, mixCardH, 5.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // Volume Slider in Sidebar
    drawText(r, "VOL", cardX + 10.0f, mixCardY + 11.0f, 9.0f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
    float sbVolX = cardX + 38.0f;
    float sbVolY = mixCardY + 14.0f;
    float sbVolW = cardW - 120.0f;
    drawRoundedRect(r, sbVolX, sbVolY, sbVolW, 5.0f, 2.5f, 0.10f, 0.10f, 0.12f, 1.0f);
    float normV = std::clamp(track.volume / 1.5f, 0.0f, 1.0f);
    drawRoundedRect(r, sbVolX, sbVolY, sbVolW * normV, 5.0f, 2.5f, track.r, track.g, track.b, 1.0f);
    drawCircle(r, sbVolX + sbVolW * normV, sbVolY + 2.5f, 5.5f, 0.9f, 0.9f, 0.95f, 1.0f);
    int volPct = static_cast<int>(std::round(track.volume * 100.0f));
    drawText(r, std::to_string(volPct) + "%", sbVolX + sbVolW + 8.0f, mixCardY + 11.0f, 8.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.9f);

    // Pan Knob in Sidebar
    float sbPanX = cardX + cardW - 24.0f;
    float sbPanY = mixCardY + 23.0f;
    drawCircle(r, sbPanX, sbPanY, 10.0f, 0.14f, 0.15f, 0.17f, 1.0f);
    drawCircle(r, sbPanX, sbPanY, 8.5f, 0.08f, 0.08f, 0.09f, 1.0f);
    float sbPanAng = -1.57f + track.pan * 2.2f;
    drawLine(r, sbPanX, sbPanY, sbPanX + std::cos(sbPanAng) * 7.0f, sbPanY + std::sin(sbPanAng) * 7.0f,
             track.r, track.g, track.b, 1.0f, 1.6f);
    std::string panStr = (std::abs(track.pan) < 0.04f) ? "C" : ((track.pan < 0.0f) ? "L" : "R");
    drawText(r, panStr, sbPanX - 4.0f, mixCardY + 36.0f, 8.0f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

    // 2. Instrument Card with 4 Knobs & Change Instrument Button
    float instY = mixCardY + mixCardH + 8.0f;
    float instH = 126.0f;
    drawRoundedRect(r, cardX, instY, cardW, instH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cardX, instY, cardW, instH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawCircle(r, cardX + 14.0f, instY + 16.0f, 3.5f, track.r, track.g, track.b, 1.0f);
    drawText(r, track.instrument, cardX + 24.0f, instY + 10.0f, 11.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    // 4 Rotary Knobs
    float kw = (cardW - 20.0f) / 4.0f;
    for (int k = 0; k < 4; ++k) {
        float kcx = cardX + 10.0f + static_cast<float>(k) * kw + kw * 0.5f;
        float kcy = instY + 50.0f;
        float val = (k == 0) ? track.knob1 : ((k == 1) ? track.knob2 : ((k == 2) ? track.knob3 : track.knob4));
        const std::string& kname = (k == 0) ? track.knob1Name : ((k == 1) ? track.knob2Name : ((k == 2) ? track.knob3Name : track.knob4Name));
        bool isDraggingThis = (dragMode_ == ClipDragMode::InspectorKnob && dragKnobIdx_ == k);

        drawCircle(r, kcx, kcy, 13.0f, 0.14f, 0.15f, 0.18f, 1.0f);
        drawCircle(r, kcx, kcy, 11.0f, 0.08f, 0.08f, 0.09f, 1.0f);
        float ang = -2.35f + val * 4.71f;
        Color pointerCol = isDraggingThis ? theme.highlight : theme.secondaryAccent;
        drawLine(r, kcx, kcy, kcx + std::cos(ang) * 9.5f, kcy + std::sin(ang) * 9.5f,
                 pointerCol.r, pointerCol.g, pointerCol.b, 1.0f, isDraggingThis ? 2.2f : 1.8f);

        drawText(r, kname, kcx - static_cast<float>(kname.length()) * 2.8f, kcy + 16.0f, 8.0f,
                 isDraggingThis ? theme.highlight.r : theme.textMuted.r,
                 isDraggingThis ? theme.highlight.g : theme.textMuted.g,
                 isDraggingThis ? theme.highlight.b : theme.textMuted.b, 0.9f);
        int pct = static_cast<int>(std::round(val * 100.0f));
        std::string pctStr = std::to_string(pct) + "%";
        drawText(r, pctStr, kcx - static_cast<float>(pctStr.length()) * 2.4f, kcy + 25.0f, 7.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.75f);
    }

    // Change Instrument Button
    float chgBtnY = instY + instH - 26.0f;
    drawRoundedRect(r, cardX + 10.0f, chgBtnY, cardW - 20.0f, 20.0f, 4.0f,
                    0.10f, 0.10f, 0.12f, 0.9f);
    drawRoundedRectOutline(r, cardX + 10.0f, chgBtnY, cardW - 20.0f, 20.0f, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
    drawText(r, "<-> CHANGE INSTRUMENT", cardX + (cardW * 0.5f) - 62.0f, chgBtnY + 4.5f, 9.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.9f);

    // 3. MIDI FX Section with '+ ADD MIDI FX'
    float midiY = instY + instH + 12.0f;
    drawText(r, "MIDI FX RACK", cardX + 4.0f, midiY + 4.0f, 11.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // '+ ADD MIDI FX' button on right
    float addMidiBtnW = 90.0f;
    float addMidiBtnX = cardX + cardW - addMidiBtnW;
    drawRoundedRect(r, addMidiBtnX, midiY, addMidiBtnW, 20.0f, 3.0f,
                    theme.primaryAccent.r * 0.25f, theme.primaryAccent.g * 0.25f, theme.primaryAccent.b * 0.25f, 0.9f);
    drawRoundedRectOutline(r, addMidiBtnX, midiY, addMidiBtnW, 20.0f, 3.0f,
                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.8f, 1.0f);
    drawText(r, "+ ADD MIDI FX", addMidiBtnX + 10.0f, midiY + 4.5f, 9.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // MIDI FX cards list
    float mCardY = midiY + 26.0f;
    for (size_t mi = 0; mi < track.midiFx.size(); ++mi) {
        const auto& mfx = track.midiFx[mi];
        drawRoundedRect(r, cardX, mCardY, cardW, 36.0f, 4.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
        drawRoundedRectOutline(r, cardX, mCardY, cardW, 36.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);

        drawCircle(r, cardX + 12.0f, mCardY + 18.0f, 3.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
        drawText(r, mfx.name, cardX + 22.0f, mCardY + 11.0f, 10.5f,
                 theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
        drawText(r, "[X]", cardX + cardW - 20.0f, mCardY + 11.0f, 9.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

        mCardY += 42.0f;
    }

    // 4. Audio FX Section with '+ ADD FX'
    float fxY = mCardY + 6.0f;
    drawText(r, "AUDIO FX RACK (" + std::to_string(track.audioFx.size()) + ")", cardX + 4.0f, fxY + 4.0f, 11.0f,
             theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);

    float addFxBtnW = 76.0f;
    float addFxBtnX = cardX + cardW - addFxBtnW;
    drawRoundedRect(r, addFxBtnX, fxY, addFxBtnW, 20.0f, 3.0f,
                    theme.secondaryAccent.r * 0.25f, theme.secondaryAccent.g * 0.25f, theme.secondaryAccent.b * 0.25f, 0.9f);
    drawRoundedRectOutline(r, addFxBtnX, fxY, addFxBtnW, 20.0f, 3.0f,
                           theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.8f, 1.0f);
    drawText(r, "+ ADD FX", addFxBtnX + 12.0f, fxY + 4.5f, 9.0f,
             theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);

    float fxCardY = fxY + 26.0f;
    for (size_t fi = 0; fi < track.audioFx.size(); ++fi) {
        const auto& afx = track.audioFx[fi];
        drawRoundedRect(r, cardX, fxCardY, cardW, 36.0f, 4.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
        drawRoundedRectOutline(r, cardX, fxCardY, cardW, 36.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);

        drawCircle(r, cardX + 12.0f, fxCardY + 18.0f, 3.0f, theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);
        drawText(r, afx.name, cardX + 22.0f, fxCardY + 11.0f, 10.5f,
                 theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
        drawText(r, "[X]", cardX + cardW - 20.0f, fxCardY + 11.0f, 9.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

        fxCardY += 42.0f;
    }

    // 5. Track Color Swatches (8 Quick Colors matching Eatsbeats)
    float colY = fxCardY + 8.0f;
    drawText(r, "TRACK COLOR", cardX + 4.0f, colY, 10.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    float swatchSpacing = cardW / 8.0f;
    for (int p = 0; p < 8; ++p) {
        float scx = cardX + static_cast<float>(p) * swatchSpacing + swatchSpacing * 0.5f;
        float scy = colY + 22.0f;
        float cr = kQuickPalette[p][0];
        float cg = kQuickPalette[p][1];
        float cb = kQuickPalette[p][2];

        bool isCurCol = (std::abs(track.r - cr) < 0.05f && std::abs(track.g - cg) < 0.05f && std::abs(track.b - cb) < 0.05f);

        drawCircle(r, scx, scy, 11.0f, cr, cg, cb, 1.0f);
        if (isCurCol) {
            drawCircle(r, scx, scy, 14.0f, cr, cg, cb, 0.4f);
            drawText(r, "V", scx - 3.5f, scy - 4.5f, 9.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    // 6. Harmonic Chord Follow Card (Eatsbeats parity)
    float flwCardY = colY + 44.0f;
    float flwCardH = 56.0f;
    drawRoundedRect(r, cardX, flwCardY, cardW, flwCardH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cardX, flwCardY, cardW, flwCardH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "HARMONIC CHORD FOLLOW", cardX + 10.0f, flwCardY + 8.0f, 9.5f,
             0.18f, 0.85f, 0.95f, 1.0f);

    // 5 Mode Chips: Off, Chord, Bass, Scale, ColorLead
    static const std::array<std::pair<theory::ChordFollowMode, const char*>, 5> kFollowChips = {{
        {theory::ChordFollowMode::Off, "Off"},
        {theory::ChordFollowMode::Chord, "Chord"},
        {theory::ChordFollowMode::Bass, "Bass"},
        {theory::ChordFollowMode::Scale, "Scale"},
        {theory::ChordFollowMode::ColorLead, "Lead"}
    }};

    float chipSpacing = (cardW - 16.0f) / 5.0f;
    for (size_t ci = 0; ci < kFollowChips.size(); ++ci) {
        float chpX = cardX + 8.0f + static_cast<float>(ci) * chipSpacing;
        float chpY = flwCardY + 26.0f;
        float chpW = chipSpacing - 4.0f;
        float chpH = 20.0f;
        bool isSel = (track.chordFollowMode == kFollowChips[ci].first);

        drawRoundedRect(r, chpX, chpY, chpW, chpH, 3.0f,
                        isSel ? 0.18f * 0.4f : 0.10f,
                        isSel ? 0.85f * 0.4f : 0.10f,
                        isSel ? 0.95f * 0.4f : 0.12f, 0.95f);
        drawRoundedRectOutline(r, chpX, chpY, chpW, chpH, 3.0f,
                               isSel ? 0.18f : theme.borderSubtle.r,
                               isSel ? 0.85f : theme.borderSubtle.g,
                               isSel ? 0.95f : theme.borderSubtle.b,
                               isSel ? 1.0f : 0.5f, 1.0f);
        drawText(r, kFollowChips[ci].second,
                 chpX + (chpW - static_cast<float>(std::strlen(kFollowChips[ci].second)) * 5.5f) * 0.5f,
                 chpY + 4.0f, 8.5f,
                 isSel ? 1.0f : theme.textMuted.r,
                 isSel ? 1.0f : theme.textMuted.g,
                 isSel ? 1.0f : theme.textMuted.b, 1.0f);
    }

    // "BAKE TO MIDI" button if follow mode is active
    if (track.chordFollowMode != theory::ChordFollowMode::Off) {
        float bakeW = 76.0f;
        float bakeH = 16.0f;
        float bakeX = cardX + cardW - bakeW - 8.0f;
        float bakeY = flwCardY + 6.0f;
        drawRoundedRect(r, bakeX, bakeY, bakeW, bakeH, 3.0f, 0.85f * 0.25f, 0.65f * 0.25f, 0.15f * 0.25f, 0.95f);
        drawRoundedRectOutline(r, bakeX, bakeY, bakeW, bakeH, 3.0f, 0.95f, 0.75f, 0.20f, 0.8f, 1.0f);
        drawText(r, "BAKE TO MIDI", bakeX + 5.0f, bakeY + 3.0f, 7.5f, 1.0f, 0.85f, 0.3f, 1.0f);
    }
}

void ArrangerView::renderClipProperties(const ViewContext& ctx, ArrangerTimelineTrack& track, ArrangerTimelineClip& clip) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;
    float startY = inspectorBounds_.y + 50.0f;
    float cardW = inspectorBounds_.w - 20.0f;
    float cardX = inspectorBounds_.x + 10.0f;

    // 1. Clip Title Card
    drawRoundedRect(r, cardX, startY, cardW, 40.0f, 5.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cardX, startY, cardW, 40.0f, 5.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
    drawText(r, clip.name, cardX + 14.0f, startY + 12.0f, 13.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    // 2. Clip Timing & Loop Parameters Card
    float infoY = startY + 48.0f;
    float infoH = 130.0f;
    drawRoundedRect(r, cardX, infoY, cardW, infoH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cardX, infoY, cardW, infoH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "Track: " + track.name, cardX + 14.0f, infoY + 12.0f, 10.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
    drawText(r, "Start Bar: " + std::to_string(clip.startBar), cardX + 14.0f, infoY + 30.0f, 10.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "Length: " + std::to_string(clip.lengthBars) + " Bars", cardX + 14.0f, infoY + 48.0f, 10.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    // Loop Mode Toggle Button
    float loopTglY = infoY + 70.0f;
    drawRoundedRect(r, cardX + 14.0f, loopTglY, cardW - 28.0f, 24.0f, 4.0f,
                    clip.isLooped ? theme.primaryAccent.r * 0.3f : 0.12f,
                    clip.isLooped ? theme.primaryAccent.g * 0.3f : 0.12f,
                    clip.isLooped ? theme.primaryAccent.b * 0.3f : 0.14f, 0.9f);
    drawRoundedRectOutline(r, cardX + 14.0f, loopTglY, cardW - 28.0f, 24.0f, 4.0f,
                           clip.isLooped ? theme.primaryAccent.r : theme.borderSubtle.r,
                           clip.isLooped ? theme.primaryAccent.g : theme.borderSubtle.g,
                           clip.isLooped ? theme.primaryAccent.b : theme.borderSubtle.b, 0.8f, 1.0f);
    drawText(r, clip.isLooped ? "LOOP MODE: ON (" + std::to_string(clip.loopLengthBars) + "b)" : "LOOP MODE: OFF",
             cardX + 24.0f, loopTglY + 6.0f, 9.5f,
             clip.isLooped ? theme.primaryAccent.r : theme.textMuted.r,
             clip.isLooped ? theme.primaryAccent.g : theme.textMuted.g,
             clip.isLooped ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);

    // 3. Edit in Piano Roll Action Button
    float actY = infoY + infoH + 16.0f;
    drawRoundedRect(r, cardX, actY, cardW, 34.0f, 5.0f,
                    theme.secondaryAccent.r * 0.3f, theme.secondaryAccent.g * 0.3f, theme.secondaryAccent.b * 0.3f, 0.95f);
    drawRoundedRectOutline(r, cardX, actY, cardW, 34.0f, 5.0f,
                           theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.9f, 1.2f);
    drawText(r, "[EDIT IN PIANO ROLL]", cardX + (cardW * 0.5f) - 64.0f, actY + 10.0f, 11.0f,
             theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);

    // 4. Duplicate Clip Button
    float dupY = actY + 42.0f;
    drawRoundedRect(r, cardX, dupY, cardW, 30.0f, 4.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
    drawText(r, "DUPLICATE CLIP (Ctrl+D)", cardX + (cardW * 0.5f) - 68.0f, dupY + 9.0f, 10.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.9f);

    // 5. Delete Clip Button
    float delY = dupY + 36.0f;
    drawRoundedRect(r, cardX, delY, cardW, 30.0f, 4.0f,
                    0.4f, 0.1f, 0.1f, 0.85f);
    drawText(r, "DELETE CLIP", cardX + (cardW * 0.5f) - 34.0f, delY + 9.0f, 10.0f,
             1.0f, 0.5f, 0.5f, 1.0f);
}

bool ArrangerView::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    // 0. Forward to Reusable Contextual Dialog if open
    if (pluginDialog_.isOpen()) {
        return pluginDialog_.handlePointer(ev);
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

    // 0b. Mouse Wheel / 2D Scroll
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
            if (dragMode_ == ClipDragMode::ChordMove || dragMode_ == ClipDragMode::ChordResize) {
                std::sort(chordTrack_.begin(), chordTrack_.end(), [](const theory::ChordEvent& a, const theory::ChordEvent& b) {
                    return a.startBar < b.startBar;
                });
            }
            dragMode_ = ClipDragMode::None;
            dragClipIdx_ = -1;
            dragKnobIdx_ = -1;
            dragChordIdx_ = -1;
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

    // 1. Properties Drawer Pull-Tab & Panel Click
    float pullTabX = inspectorBounds_.x;
    float pullTabW = 22.0f;
    float pullTabH = 96.0f;
    Rect2D pullTabRect(pullTabX - pullTabW, inspectorBounds_.y + 60.0f, pullTabW, pullTabH);

    if (ev.action == PointerAction::Down && pullTabRect.contains(ev.x, ev.y)) {
        toggleInspector();
        return true;
    }

    if (inspectorOpen_ && inspectorBounds_.contains(ev.x, ev.y)) {
        if (ev.action != PointerAction::Down) return true;

        float tabW = (inspectorBounds_.w - 24.0f) * 0.5f;
        float tabY = inspectorBounds_.y + 10.0f;
        Rect2D trackTabRect(inspectorBounds_.x + 10.0f, tabY, tabW, 26.0f);
        Rect2D clipTabRect(inspectorBounds_.x + 14.0f + tabW, tabY, tabW, 26.0f);

        if (trackTabRect.contains(ev.x, ev.y)) {
            inspectorTab_ = ArrangerInspectorTab::Track;
            return true;
        }
        if (clipTabRect.contains(ev.x, ev.y)) {
            inspectorTab_ = ArrangerInspectorTab::Clip;
            return true;
        }

        if (activeTrackIndex_ < tracks_.size()) {
            auto& track = tracks_[activeTrackIndex_];
            float cardW = inspectorBounds_.w - 20.0f;
            float cardX = inspectorBounds_.x + 10.0f;

            if (inspectorTab_ == ArrangerInspectorTab::Track) {
                float startY = inspectorBounds_.y + 50.0f;

                // 1. Quick Controls: Volume Slider & Pan Knob
                float mixCardY = startY + 44.0f;
                float mixCardH = 46.0f;
                float sbVolX = cardX + 38.0f;
                float sbVolY = mixCardY + 14.0f;
                float sbVolW = cardW - 120.0f;
                Rect2D sbVolBox(sbVolX - 6.0f, sbVolY - 8.0f, sbVolW + 12.0f, 22.0f);
                float sbPanX = cardX + cardW - 24.0f;
                float sbPanY = mixCardY + 23.0f;
                Rect2D sbPanBox(sbPanX - 14.0f, sbPanY - 14.0f, 28.0f, 28.0f);

                if (sbVolBox.contains(ev.x, ev.y)) {
                    if (ev.button == PointerButton::Right) {
                        if (ctx.onOpenValueEdit) {
                            ValueEditRequest req;
                            req.title = track.name + " VOLUME";
                            req.paramName = track.name + " Volume";
                            req.currentValue = track.volume;
                            req.minValue = 0.0f;
                            req.maxValue = 1.5f;
                            req.defaultValue = 1.0f;
                            req.hasDefault = true;
                            req.allowPercentage = true;
                            req.accentColor = Color(track.r, track.g, track.b, 1.0f);
                            req.onCommit = [this](float val) {
                                if (activeTrackIndex_ < tracks_.size()) {
                                    tracks_[activeTrackIndex_].volume = val;
                                    if (onVolumeChanged) onVolumeChanged(activeTrackIndex_, val);
                                }
                            };
                            ctx.onOpenValueEdit(req);
                        }
                        return true;
                    }
                    dragMode_ = ClipDragMode::InspectorVolume;
                    dragStartPointerX_ = ev.x;
                    dragStartPointerY_ = ev.y;
                    dragStartVal_ = track.volume;
                    float norm = std::clamp((ev.x - sbVolX) / sbVolW, 0.0f, 1.0f);
                    track.volume = norm * 1.5f;
                    if (onVolumeChanged) onVolumeChanged(activeTrackIndex_, track.volume);
                    return true;
                }

                if (sbPanBox.contains(ev.x, ev.y)) {
                    if (ev.button == PointerButton::Right) {
                        if (ctx.onOpenValueEdit) {
                            ValueEditRequest req;
                            req.title = track.name + " PAN";
                            req.paramName = track.name + " Pan";
                            req.currentValue = track.pan;
                            req.minValue = -1.0f;
                            req.maxValue = 1.0f;
                            req.defaultValue = 0.0f;
                            req.hasDefault = true;
                            req.allowPercentage = false;
                            req.accentColor = Color(track.r, track.g, track.b, 1.0f);
                            req.onCommit = [this](float val) {
                                if (activeTrackIndex_ < tracks_.size()) {
                                    tracks_[activeTrackIndex_].pan = val;
                                    if (onPanChanged) onPanChanged(activeTrackIndex_, val);
                                }
                            };
                            ctx.onOpenValueEdit(req);
                        }
                        return true;
                    }
                    dragMode_ = ClipDragMode::InspectorPan;
                    dragStartPointerX_ = ev.x;
                    dragStartPointerY_ = ev.y;
                    dragStartVal_ = track.pan;
                    return true;
                }

                // 2. Instrument Card: 4 Rotary Knobs
                float instY = mixCardY + mixCardH + 8.0f;
                float instH = 126.0f;
                float kw = (cardW - 20.0f) / 4.0f;
                for (int k = 0; k < 4; ++k) {
                    float kcx = cardX + 10.0f + static_cast<float>(k) * kw + kw * 0.5f;
                    float kcy = instY + 50.0f;
                    Rect2D knobBox(kcx - 18.0f, kcy - 18.0f, 36.0f, 48.0f);
                    if (knobBox.contains(ev.x, ev.y)) {
                        std::string kname = (k == 0) ? track.knob1Name : ((k == 1) ? track.knob2Name : ((k == 2) ? track.knob3Name : track.knob4Name));
                        float curVal = (k == 0) ? track.knob1 : ((k == 1) ? track.knob2 : ((k == 2) ? track.knob3 : track.knob4));
                        if (ev.button == PointerButton::Right) {
                            if (ctx.onOpenValueEdit) {
                                ValueEditRequest req;
                                req.title = track.name + " " + kname;
                                req.paramName = kname;
                                req.currentValue = curVal;
                                req.minValue = 0.0f;
                                req.maxValue = 1.0f;
                                req.defaultValue = 0.5f;
                                req.hasDefault = true;
                                req.allowPercentage = true;
                                req.accentColor = Color(track.r, track.g, track.b, 1.0f);
                                req.onCommit = [this, k](float val) {
                                    if (activeTrackIndex_ < tracks_.size()) {
                                        auto& trk = tracks_[activeTrackIndex_];
                                        std::string name;
                                        if (k == 0) { trk.knob1 = val; name = trk.knob1Name; }
                                        else if (k == 1) { trk.knob2 = val; name = trk.knob2Name; }
                                        else if (k == 2) { trk.knob3 = val; name = trk.knob3Name; }
                                        else if (k == 3) { trk.knob4 = val; name = trk.knob4Name; }
                                        if (onParamChanged) onParamChanged(activeTrackIndex_, name, val);
                                    }
                                };
                                ctx.onOpenValueEdit(req);
                            }
                            return true;
                        }
                        dragMode_ = ClipDragMode::InspectorKnob;
                        dragKnobIdx_ = k;
                        dragStartPointerX_ = ev.x;
                        dragStartPointerY_ = ev.y;
                        dragStartVal_ = curVal;
                        return true;
                    }
                }

                // Change Instrument button
                float chgBtnY = instY + instH - 26.0f;
                Rect2D chgBtn(cardX + 10.0f, chgBtnY, cardW - 20.0f, 22.0f);
                if (chgBtn.contains(ev.x, ev.y)) {
                    pluginDialog_.open(PluginDialogMode::AddInstrument, track.name, activeTrackIndex_);
                    return true;
                }

                // '+ ADD MIDI FX' button
                float midiY = instY + instH + 12.0f;
                float addMidiBtnW = 90.0f;
                Rect2D addMidiBtn(cardX + cardW - addMidiBtnW, midiY, addMidiBtnW, 20.0f);
                if (addMidiBtn.contains(ev.x, ev.y)) {
                    pluginDialog_.open(PluginDialogMode::AddMidiFx, track.name, activeTrackIndex_);
                    return true;
                }

                // Delete MIDI FX buttons
                float mCardY = midiY + 26.0f;
                for (size_t mi = 0; mi < track.midiFx.size(); ++mi) {
                    Rect2D delBtn(cardX + cardW - 24.0f, mCardY + 8.0f, 20.0f, 20.0f);
                    if (delBtn.contains(ev.x, ev.y)) {
                        track.midiFx.erase(track.midiFx.begin() + mi);
                        return true;
                    }
                    mCardY += 42.0f;
                }

                // '+ ADD FX' button
                float fxY = mCardY + 6.0f;
                float addFxBtnW = 76.0f;
                Rect2D addFxBtn(cardX + cardW - addFxBtnW, fxY, addFxBtnW, 20.0f);
                if (addFxBtn.contains(ev.x, ev.y)) {
                    pluginDialog_.open(PluginDialogMode::AddAudioFx, track.name, activeTrackIndex_);
                    return true;
                }

                // Delete Audio FX buttons
                float fxCardY = fxY + 26.0f;
                for (size_t fi = 0; fi < track.audioFx.size(); ++fi) {
                    Rect2D delBtn(cardX + cardW - 24.0f, fxCardY + 8.0f, 20.0f, 20.0f);
                    if (delBtn.contains(ev.x, ev.y)) {
                        track.audioFx.erase(track.audioFx.begin() + fi);
                        return true;
                    }
                    fxCardY += 42.0f;
                }

                // Track Color Swatch buttons
                float colY = fxCardY + 8.0f;
                float swatchSpacing = cardW / 8.0f;
                for (int p = 0; p < 8; ++p) {
                    float scx = cardX + static_cast<float>(p) * swatchSpacing + swatchSpacing * 0.5f;
                    float scy = colY + 22.0f;
                    float dist = std::hypot(ev.x - scx, ev.y - scy);
                    if (dist <= 14.0f) {
                        track.r = kQuickPalette[p][0];
                        track.g = kQuickPalette[p][1];
                        track.b = kQuickPalette[p][2];
                        for (auto& cl : track.clips) {
                            cl.r = track.r; cl.g = track.g; cl.b = track.b;
                        }
                        return true;
                    }
                }

                // Harmonic Chord Follow Chips & Bake to MIDI Button
                float flwCardY = colY + 44.0f;
                if (track.chordFollowMode != theory::ChordFollowMode::Off) {
                    float bakeW = 76.0f;
                    float bakeH = 16.0f;
                    float bakeX = cardX + cardW - bakeW - 8.0f;
                    float bakeY = flwCardY + 6.0f;
                    Rect2D bakeBtn(bakeX, bakeY, bakeW, bakeH);
                    if (bakeBtn.contains(ev.x, ev.y)) {
                        bakeChordsToTrack(activeTrackIndex_);
                        return true;
                    }
                }

                float chipSpacing = (cardW - 16.0f) / 5.0f;
                for (size_t ci = 0; ci < 5; ++ci) {
                    float chpX = cardX + 8.0f + static_cast<float>(ci) * chipSpacing;
                    float chpY = flwCardY + 26.0f;
                    float chpW = chipSpacing - 4.0f;
                    float chpH = 20.0f;
                    Rect2D chpRect(chpX, chpY, chpW, chpH);
                    if (chpRect.contains(ev.x, ev.y)) {
                        track.chordFollowMode = static_cast<theory::ChordFollowMode>(ci);
                        if (onTrackChordFollowChanged) {
                            onTrackChordFollowChanged(activeTrackIndex_, track.chordFollowMode);
                        }
                        return true;
                    }
                }
            } else {
                // Clip properties buttons
                if (selectedClipIndex_ >= 0 && selectedClipIndex_ < static_cast<int>(track.clips.size())) {
                    auto& clip = track.clips[selectedClipIndex_];
                    float infoY = inspectorBounds_.y + 50.0f + 48.0f;

                    // Loop mode toggle
                    float loopTglY = infoY + 70.0f;
                    Rect2D loopTglBtn(cardX + 14.0f, loopTglY, cardW - 28.0f, 24.0f);
                    if (loopTglBtn.contains(ev.x, ev.y)) {
                        clip.isLooped = !clip.isLooped;
                        return true;
                    }

                    // Edit in piano roll
                    float actY = infoY + 130.0f + 16.0f;
                    Rect2D editBtn(cardX, actY, cardW, 34.0f);
                    if (editBtn.contains(ev.x, ev.y)) {
                        if (ctx.onJumpToClipEdit) {
                            ctx.onJumpToClipEdit(activeTrackIndex_, selectedClipIndex_);
                        }
                        return true;
                    }

                    // Duplicate clip
                    float dupY = actY + 42.0f;
                    Rect2D dupBtn(cardX, dupY, cardW, 30.0f);
                    if (dupBtn.contains(ev.x, ev.y)) {
                        auto copy = clip;
                        copy.id = "c_" + std::to_string(track.clips.size() + 1);
                        copy.startBar += copy.lengthBars;
                        track.clips.push_back(copy);
                        selectedClipIndex_ = static_cast<int>(track.clips.size() - 1);
                        return true;
                    }

                    // Delete clip
                    float delY = dupY + 36.0f;
                    Rect2D delBtn(cardX, delY, cardW, 30.0f);
                    if (delBtn.contains(ev.x, ev.y)) {
                        track.clips.erase(track.clips.begin() + selectedClipIndex_);
                        selectedClipIndex_ = -1;
                        inspectorTab_ = ArrangerInspectorTab::Track;
                        return true;
                    }
                }
            }
        }
        return true;
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

    // 3b. Chord Track Lane Header Card Click (WHEEL / Key / Card)
    if (chordHeaderBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            circleOfFifthsDialog_.open(0, songKeyRoot_, isSongKeyMinor_);
            return true;
        }
    }

    // 3c. Chord Lane Interaction (Add, Move, Resize, Audition, Delete)
    if (chordLaneBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            float localX = ev.x - chordLaneBounds_.x + scrollX_;

            for (size_t i = 0; i < chordTrack_.size(); ++i) {
                const auto& chord = chordTrack_[i];
                float cx = static_cast<float>(chord.startBar) * barWidth_;
                float cw = chord.barLength * barWidth_;

                if (localX >= cx && localX <= cx + cw) {
                    selectedChordIndex_ = static_cast<int>(i);

                    // Right click: Remove Chord
                    if (ev.button == PointerButton::Right) {
                        removeChord(chord.id);
                        selectedChordIndex_ = -1;
                        return true;
                    }

                    // Check right edge resize handle (last 16px)
                    if (localX >= cx + cw - 16.0f) {
                        dragMode_ = ClipDragMode::ChordResize;
                        dragChordIdx_ = static_cast<int>(i);
                        dragChordOrigLength_ = chord.barLength;
                        dragStartPointerX_ = ev.x;
                        dragStartPointerY_ = ev.y;
                        return true;
                    }

                    // Audition chord
                    if (onAuditionChord) {
                        onAuditionChord(chord);
                    }

                    // Check double click -> Open Circle of Fifths modal for this chord
                    auto now = std::chrono::steady_clock::now();
                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClipClickTime_).count();
                    if (lastClickedClipIdx_ == static_cast<int>(i) && elapsedMs < 350) {
                        lastClipClickTime_ = std::chrono::steady_clock::time_point{};
                        lastClickedClipIdx_ = -1;
                        circleOfFifthsDialog_.openForChord(chord, songKeyRoot_, isSongKeyMinor_);
                        return true;
                    }
                    lastClipClickTime_ = now;
                    lastClickedClipIdx_ = static_cast<int>(i);

                    // Move chord drag
                    dragMode_ = ClipDragMode::ChordMove;
                    dragChordIdx_ = static_cast<int>(i);
                    dragChordOrigStartBar_ = chord.startBar;
                    dragStartPointerX_ = ev.x;
                    dragStartPointerY_ = ev.y;
                    return true;
                }
            }

            // Clicked empty chord lane space -> Open Circle of Fifths dialog at clicked bar!
            int clickedBar = static_cast<int>(localX / barWidth_);
            selectedChordIndex_ = -1;
            circleOfFifthsDialog_.open(static_cast<uint32_t>(std::max(0, clickedBar)), songKeyRoot_, isSongKeyMinor_);
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

                float rowY = tracksListBounds_.y + static_cast<float>(clickedTrack) * trackRowHeight_ - scrollY_;
                float btnY = rowY + 8.0f;

                // Click on track icon glyph opens IconSearchDialog
                float iconBoxX = tracksListBounds_.x + 10.0f;
                float iconBoxY = rowY + 7.5f;
                float iconBoxSize = 15.0f;
                Rect2D iconHitBox(iconBoxX - 3.0f, iconBoxY - 3.0f, iconBoxSize + 6.0f, iconBoxSize + 6.0f);
                if (iconHitBox.contains(ev.x, ev.y)) {
                    iconDialog_.open(tracks_[clickedTrack].name, clickedTrack, tracks_[clickedTrack].iconRef);
                    return true;
                }

                Rect2D muteRect(tracksListBounds_.x + tracksListBounds_.w - 42.0f, btnY, 16.0f, 13.0f);
                Rect2D soloRect(tracksListBounds_.x + tracksListBounds_.w - 22.0f, btnY, 16.0f, 13.0f);

                // Chord Follow Mode Button Chip Click
                float flwBtnW = 34.0f;
                float flwBtnH = 13.0f;
                float flwBtnX = tracksListBounds_.x + tracksListBounds_.w - 82.0f;
                Rect2D flwBtnRect(flwBtnX, btnY, flwBtnW, flwBtnH);
                if (flwBtnRect.contains(ev.x, ev.y)) {
                    int nextMode = (static_cast<int>(tracks_[clickedTrack].chordFollowMode) + 1) % 5;
                    tracks_[clickedTrack].chordFollowMode = static_cast<theory::ChordFollowMode>(nextMode);
                    if (onTrackChordFollowChanged) {
                        onTrackChordFollowChanged(clickedTrack, tracks_[clickedTrack].chordFollowMode);
                    }
                    return true;
                }

                if (muteRect.contains(ev.x, ev.y)) {
                    tracks_[clickedTrack].mute = !tracks_[clickedTrack].mute;
                    return true;
                } else if (soloRect.contains(ev.x, ev.y)) {
                    tracks_[clickedTrack].solo = !tracks_[clickedTrack].solo;
                    return true;
                }

                // Volume Slider
                float sliderW = tracksListBounds_.w - 75.0f;
                float sliderX = tracksListBounds_.x + 36.0f;
                float sliderY = rowY + 34.0f;
                Rect2D volBox(sliderX - 6.0f, sliderY - 8.0f, sliderW + 12.0f, 18.0f);

                // Pan Knob
                float knobX = tracksListBounds_.x + tracksListBounds_.w - 22.0f;
                float knobY = rowY + 36.0f;
                Rect2D panBox(knobX - 12.0f, knobY - 12.0f, 24.0f, 24.0f);

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
            }
            return true;
        }

        if (ev.action == PointerAction::Move) {
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
    if (circleOfFifthsDialog_.isOpen()) {
        return circleOfFifthsDialog_.handleKey(key, scancode, action, mods);
    }
    if (iconDialog_.isOpen()) {
        return iconDialog_.handleKey(key, scancode, action, mods);
    }

    if (action != 1 && action != 2) return false;

    // 'F' -> Toggle continuous playback follow mode
    if (key == 'F' || key == 'f') {
        toggleFollowPlayback();
        return true;
    }

    // Ctrl+D -> Duplicate Selected Clip
    if ((key == 'D' || key == 'd') && (mods & 2)) {
        if (activeTrackIndex_ < tracks_.size() && selectedClipIndex_ >= 0) {
            auto& track = tracks_[activeTrackIndex_];
            if (selectedClipIndex_ < static_cast<int>(track.clips.size())) {
                auto newClip = track.clips[selectedClipIndex_];
                newClip.id = "c_" + std::to_string(track.clips.size() + 1);
                newClip.startBar += newClip.lengthBars;
                track.clips.push_back(newClip);
                selectedClipIndex_ = static_cast<int>(track.clips.size() - 1);
                return true;
            }
        }
    }

    // Delete or Backspace -> Delete Selected Clip
    if (key == 261 || key == 259) {
        if (activeTrackIndex_ < tracks_.size() && selectedClipIndex_ >= 0) {
            auto& track = tracks_[activeTrackIndex_];
            if (selectedClipIndex_ < static_cast<int>(track.clips.size())) {
                track.clips.erase(track.clips.begin() + selectedClipIndex_);
                selectedClipIndex_ = -1;
                inspectorTab_ = ArrangerInspectorTab::Track;
                return true;
            }
        }
    }

    return false;
}

} // namespace eatsbits::ui
