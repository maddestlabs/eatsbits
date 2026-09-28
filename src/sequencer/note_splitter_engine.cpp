#include "eatsbits/sequencer/note_splitter_engine.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::sequencer {

bool NoteSplitterEngine::isSkyline(const eatscript::MidiNote& target, const std::vector<eatscript::MidiNote>& allNotes) noexcept {
    const float targetStart = target.startStep;
    const float targetEnd = target.startStep + target.durationSteps;

    for (const auto& other : allNotes) {
        if (&target == &other) continue;
        const float otherStart = other.startStep;
        const float otherEnd = other.startStep + other.durationSteps;

        // Check temporal overlap
        if (otherStart < targetEnd && otherEnd > targetStart) {
            if (other.pitch > target.pitch) {
                return false;
            }
        }
    }
    return true;
}

bool NoteSplitterEngine::isBassNote(const eatscript::MidiNote& target, const std::vector<eatscript::MidiNote>& allNotes) noexcept {
    const float targetStart = target.startStep;
    const float targetEnd = target.startStep + target.durationSteps;

    for (const auto& other : allNotes) {
        if (&target == &other) continue;
        const float otherStart = other.startStep;
        const float otherEnd = other.startStep + other.durationSteps;

        if (otherStart < targetEnd && otherEnd > targetStart) {
            if (other.pitch < target.pitch) {
                return false;
            }
        }
    }
    return true;
}

static void updatePitchRange(SplitTrackResult& res) {
    res.noteCount = res.notes.size();
    if (res.notes.empty()) {
        res.minPitch = 0;
        res.maxPitch = 0;
        return;
    }
    uint8_t minP = 127;
    uint8_t maxP = 0;
    for (const auto& n : res.notes) {
        if (n.pitch < minP) minP = n.pitch;
        if (n.pitch > maxP) maxP = n.pitch;
    }
    res.minPitch = minP;
    res.maxPitch = maxP;
}

std::vector<SplitTrackResult> NoteSplitterEngine::split3WayVoice(
    const std::vector<eatscript::MidiNote>& notes,
    uint8_t bassSplitPitch,
    uint8_t leadThresholdPitch
) {
    SplitTrackResult bass;
    bass.name = "Bassline";
    bass.color = ui::Color(0.0f, 1.0f, 0.40f);
    bass.iconRef = "preset:inst_bass";

    SplitTrackResult chords;
    chords.name = "Harmony & Chords";
    chords.color = ui::Color(0.13f, 0.96f, 0.91f);
    chords.iconRef = "preset:inst_keys";

    SplitTrackResult lead;
    lead.name = "Lead Melody";
    lead.color = ui::Color(1.0f, 0.0f, 0.48f);
    lead.iconRef = "preset:inst_lead";

    for (const auto& n : notes) {
        if (n.pitch < bassSplitPitch) {
            bass.notes.push_back(n);
        } else if (n.pitch >= leadThresholdPitch && isSkyline(n, notes)) {
            lead.notes.push_back(n);
        } else {
            chords.notes.push_back(n);
        }
    }

    std::vector<SplitTrackResult> results;
    if (!bass.notes.empty()) {
        updatePitchRange(bass);
        results.push_back(std::move(bass));
    }
    if (!chords.notes.empty()) {
        updatePitchRange(chords);
        results.push_back(std::move(chords));
    }
    if (!lead.notes.empty()) {
        updatePitchRange(lead);
        results.push_back(std::move(lead));
    }

    return results;
}

std::vector<SplitTrackResult> NoteSplitterEngine::splitBassTreble(
    const std::vector<eatscript::MidiNote>& notes,
    uint8_t splitPitch
) {
    SplitTrackResult low;
    low.name = "Bass Clef (Left Hand)";
    low.color = ui::Color(0.20f, 0.60f, 1.0f);
    low.iconRef = "preset:inst_bass";

    SplitTrackResult high;
    high.name = "Treble Clef (Right Hand)";
    high.color = ui::Color(1.0f, 0.84f, 0.0f);
    high.iconRef = "preset:inst_keys";

    for (const auto& n : notes) {
        if (n.pitch < splitPitch) {
            low.notes.push_back(n);
        } else {
            high.notes.push_back(n);
        }
    }

    std::vector<SplitTrackResult> results;
    if (!low.notes.empty()) {
        updatePitchRange(low);
        results.push_back(std::move(low));
    }
    if (!high.notes.empty()) {
        updatePitchRange(high);
        results.push_back(std::move(high));
    }
    return results;
}

std::vector<SplitTrackResult> NoteSplitterEngine::split4VoicePolyphony(
    const std::vector<eatscript::MidiNote>& notes
) {
    if (notes.empty()) return {};

    // Sort notes chronologically
    auto sorted = notes;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.startStep < b.startStep;
    });

    SplitTrackResult soprano;
    soprano.name = "Voice 1 (Soprano / Top)";
    soprano.color = ui::Color(1.0f, 0.0f, 0.48f);
    soprano.iconRef = "preset:inst_lead";

    SplitTrackResult alto;
    alto.name = "Voice 2 (Alto / High-Mid)";
    alto.color = ui::Color(1.0f, 0.55f, 0.0f);
    alto.iconRef = "preset:inst_keys";

    SplitTrackResult tenor;
    tenor.name = "Voice 3 (Tenor / Low-Mid)";
    tenor.color = ui::Color(0.13f, 0.96f, 0.91f);
    tenor.iconRef = "preset:inst_synth";

    SplitTrackResult bass;
    bass.name = "Voice 4 (Bass / Root)";
    bass.color = ui::Color(0.0f, 1.0f, 0.40f);
    bass.iconRef = "preset:inst_bass";

    std::vector<eatscript::MidiNote> cluster;
    float currentStep = -1.0f;

    auto processCluster = [&](std::vector<eatscript::MidiNote>& c) {
        if (c.empty()) return;
        std::sort(c.begin(), c.end(), [](const auto& a, const auto& b) {
            return a.pitch < b.pitch;
        });

        if (c.size() == 1) {
            if (c[0].pitch < 55) {
                bass.notes.push_back(c[0]);
            } else {
                soprano.notes.push_back(c[0]);
            }
        } else if (c.size() == 2) {
            bass.notes.push_back(c[0]);
            soprano.notes.push_back(c[1]);
        } else if (c.size() == 3) {
            bass.notes.push_back(c[0]);
            alto.notes.push_back(c[1]);
            soprano.notes.push_back(c[2]);
        } else {
            bass.notes.push_back(c[0]);
            tenor.notes.push_back(c[1]);
            alto.notes.push_back(c[2]);
            soprano.notes.push_back(c[3]);
            // If cluster has 5 or more notes, assign extras to tenor/alto
            for (size_t i = 4; i < c.size(); ++i) {
                alto.notes.push_back(c[i]);
            }
        }
    };

    for (const auto& n : sorted) {
        if (currentStep < 0.0f || std::abs(n.startStep - currentStep) > 0.25f) {
            processCluster(cluster);
            cluster.clear();
            cluster.push_back(n);
            currentStep = n.startStep;
        } else {
            cluster.push_back(n);
        }
    }
    processCluster(cluster);

    std::vector<SplitTrackResult> results;
    if (!soprano.notes.empty()) { updatePitchRange(soprano); results.push_back(std::move(soprano)); }
    if (!alto.notes.empty()) { updatePitchRange(alto); results.push_back(std::move(alto)); }
    if (!tenor.notes.empty()) { updatePitchRange(tenor); results.push_back(std::move(tenor)); }
    if (!bass.notes.empty()) { updatePitchRange(bass); results.push_back(std::move(bass)); }
    return results;
}

std::vector<SplitTrackResult> NoteSplitterEngine::splitDrumPercussion(
    const std::vector<eatscript::MidiNote>& notes
) {
    SplitTrackResult kick;
    kick.name = "Drums (Kick)";
    kick.color = ui::Color(1.0f, 0.20f, 0.20f);
    kick.iconRef = "preset:drum_kick";

    SplitTrackResult snare;
    snare.name = "Drums (Snare & Clap)";
    snare.color = ui::Color(1.0f, 0.55f, 0.0f);
    snare.iconRef = "preset:drum_snare";

    SplitTrackResult hats;
    hats.name = "Drums (Hi-Hats & Cymbals)";
    hats.color = ui::Color(1.0f, 0.90f, 0.0f);
    hats.iconRef = "preset:drum_hat";

    SplitTrackResult perc;
    perc.name = "Drums (Toms & Perc)";
    perc.color = ui::Color(0.74f, 0.0f, 1.0f);
    perc.iconRef = "preset:drum_perc";

    for (const auto& n : notes) {
        const uint8_t p = n.pitch;
        if (p == 35 || p == 36) {
            kick.notes.push_back(n);
        } else if (p == 38 || p == 40 || p == 37 || p == 39) {
            snare.notes.push_back(n);
        } else if (p == 42 || p == 44 || p == 46 || p == 49 || p == 51 || p == 52 || p == 55 || p == 57) {
            hats.notes.push_back(n);
        } else {
            perc.notes.push_back(n);
        }
    }

    std::vector<SplitTrackResult> results;
    if (!kick.notes.empty()) { updatePitchRange(kick); results.push_back(std::move(kick)); }
    if (!snare.notes.empty()) { updatePitchRange(snare); results.push_back(std::move(snare)); }
    if (!hats.notes.empty()) { updatePitchRange(hats); results.push_back(std::move(hats)); }
    if (!perc.notes.empty()) { updatePitchRange(perc); results.push_back(std::move(perc)); }
    return results;
}

std::vector<SplitTrackResult> NoteSplitterEngine::splitNotes(
    const std::vector<eatscript::MidiNote>& notes,
    SplitMode mode,
    const SplitParams& params
) {
    switch (mode) {
        case SplitMode::ThreeWayVoice:
            return split3WayVoice(notes, params.bassSplitPitch, params.leadThresholdPitch);
        case SplitMode::BassTrebleClefs:
            return splitBassTreble(notes, params.pivotPitch);
        case SplitMode::FourVoiceSatb:
            return split4VoicePolyphony(notes);
        case SplitMode::DrumDemux:
            return splitDrumPercussion(notes);
    }
    return split3WayVoice(notes, params.bassSplitPitch, params.leadThresholdPitch);
}

std::vector<SplitTrackResult> NoteSplitterEngine::splitTrack(
    const SequencerTrack& track,
    SplitMode mode,
    const SplitParams& params
) {
    auto notes = eatscript::MidiPipelineEngine::trackToNotes(track);
    return splitNotes(notes, mode, params);
}

std::vector<size_t> NoteSplitterEngine::applySplitToSequencer(
    StepSequencer& seq,
    size_t sourceTrackIdx,
    SplitMode mode,
    const SplitParams& params,
    bool removeSourceTrack
) {
    std::vector<size_t> createdTrackIndices;
    if (sourceTrackIdx >= seq.getNumTracks()) return createdTrackIndices;

    auto* sourceTrack = seq.getTrack(sourceTrackIdx);
    if (!sourceTrack) return createdTrackIndices;

    const audio::NodeId targetNodeId = sourceTrack->getTargetNodeId();
    const uint32_t numSteps = sourceTrack->getNumSteps();

    auto splitResults = splitTrack(*sourceTrack, mode, params);
    if (splitResults.empty()) return createdTrackIndices;

    for (const auto& res : splitResults) {
        size_t newIdx = seq.addTrack(res.name, targetNodeId, numSteps);
        auto* newTrk = seq.getTrack(newIdx);
        if (newTrk) {
            newTrk->setIconRef(res.iconRef);
            eatscript::MidiPipelineEngine::notesToTrack(res.notes, *newTrk);
            createdTrackIndices.push_back(newIdx);
        }
    }

    if (removeSourceTrack) {
        sourceTrack->setMuted(true);
    }

    return createdTrackIndices;
}

} // namespace eatsbits::sequencer
