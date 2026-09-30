#include "eatsbits/eatscript/midi_fx_pipeline.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include <cmath>
#include <random>
#include <sstream>
#include <set>

namespace eatsbits::eatscript {

static std::string toLower(std::string_view str) {
    std::string out;
    out.reserve(str.size());
    for (char c : str) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

MidiFxType MidiPipelineEngine::detectMidiFxType(const std::string& code, const std::string& name) {
    std::string lowerCode = toLower(code);
    std::string lowerName = toLower(name);

    if (lowerCode.find("eat.") != std::string::npos ||
        lowerCode.find("def ") != std::string::npos ||
        lowerCode.find("for ") != std::string::npos) {
        return MidiFxType::Eatscript;
    }
    if (lowerCode.find("chord_follow") != std::string::npos ||
        lowerCode.find("chord_follower") != std::string::npos ||
        lowerName.find("chord follow") != std::string::npos ||
        lowerName.find("harmonic") != std::string::npos) {
        return MidiFxType::ChordFollow;
    }
    if (lowerCode.find("chord_arp") != std::string::npos ||
        lowerName.find("chord arp") != std::string::npos) {
        return MidiFxType::ChordArp;
    }
    if (lowerCode.find("scale_snap") != std::string::npos ||
        lowerCode.find("scale") != std::string::npos ||
        lowerName.find("scale") != std::string::npos ||
        lowerName.find("quantize") != std::string::npos ||
        lowerName.find("pitch quantizer") != std::string::npos) {
        return MidiFxType::ScaleSnap;
    }
    if (lowerCode.find("arpeggiator") != std::string::npos ||
        lowerCode.find("arpeggiat") != std::string::npos ||
        lowerCode.find("arp") != std::string::npos ||
        lowerName.find("arpeggiator") != std::string::npos ||
        lowerName.find("arp") != std::string::npos ||
        lowerName.find("multi-octave") != std::string::npos) {
        return MidiFxType::Arpeggiator;
    }
    if (lowerCode.find("humanize") != std::string::npos ||
        lowerName.find("humanize") != std::string::npos ||
        lowerName.find("groove") != std::string::npos ||
        lowerName.find("drift") != std::string::npos ||
        lowerName.find("timing") != std::string::npos) {
        return MidiFxType::Humanize;
    }
    if (lowerCode.find("chord_stabs") != std::string::npos ||
        lowerCode.find("voicing") != std::string::npos ||
        lowerName.find("voicing") != std::string::npos ||
        lowerName.find("voicer") != std::string::npos ||
        lowerName.find("chord") != std::string::npos ||
        lowerName.find("inversion") != std::string::npos ||
        lowerName.find("stabs") != std::string::npos) {
        return MidiFxType::ChordStabs;
    }
    if (lowerCode.find("transpose") != std::string::npos ||
        lowerName.find("transpose") != std::string::npos ||
        lowerName.find("pitch transposer") != std::string::npos) {
        return MidiFxType::Transpose;
    }

    return MidiFxType::Passthrough;
}

int MidiPipelineEngine::snapToScale(int pitch, int rootKey, bool isMinor) noexcept {
    static const int majorIntervals[] = {0, 2, 4, 5, 7, 9, 11};
    static const int minorIntervals[] = {0, 2, 3, 5, 7, 8, 10};

    const int* intervals = isMinor ? minorIntervals : majorIntervals;
    const int numIntervals = 7;

    int rel = pitch - rootKey;
    int noteInOctave = (rel % 12 + 12) % 12;
    int octave = (rel - noteInOctave) / 12;

    int closestInterval = intervals[0];
    int minDiff = 100;

    for (int i = 0; i < numIntervals; ++i) {
        int diff = std::abs(noteInOctave - intervals[i]);
        if (diff < minDiff) {
            minDiff = diff;
            closestInterval = intervals[i];
        }
    }

    int snapped = (octave * 12) + rootKey + closestInterval;
    return std::clamp(snapped, 0, 127);
}

static std::vector<int> buildArpSequence(const std::vector<int>& sortedPitches, const std::string& pattern) {
    if (sortedPitches.empty()) return {60};
    if (sortedPitches.size() == 1) return sortedPitches;

    if (pattern == "down") {
        return std::vector<int>(sortedPitches.rbegin(), sortedPitches.rend());
    }
    if (pattern == "updown") {
        if (sortedPitches.size() <= 2) return sortedPitches;
        std::vector<int> seq = sortedPitches;
        for (int i = static_cast<int>(sortedPitches.size()) - 2; i > 0; --i) {
            seq.push_back(sortedPitches[i]);
        }
        return seq;
    }
    if (pattern == "downup") {
        if (sortedPitches.size() <= 2) {
            return std::vector<int>(sortedPitches.rbegin(), sortedPitches.rend());
        }
        std::vector<int> seq(sortedPitches.rbegin(), sortedPitches.rend());
        for (int i = static_cast<int>(sortedPitches.size()) - 2; i > 0; --i) {
            seq.push_back(seq[seq.size() - 1 - i]);
        }
        return seq;
    }
    if (pattern == "converge") {
        std::vector<int> seq;
        int left = 0;
        int right = static_cast<int>(sortedPitches.size()) - 1;
        while (left <= right) {
            seq.push_back(sortedPitches[left]);
            if (left != right) seq.push_back(sortedPitches[right]);
            left++;
            right--;
        }
        return seq;
    }
    if (pattern == "diverge") {
        std::vector<int> seq;
        int mid = static_cast<int>(sortedPitches.size()) / 2;
        int left = mid;
        int right = mid + 1;
        while (left >= 0 || right < static_cast<int>(sortedPitches.size())) {
            if (left >= 0) seq.push_back(sortedPitches[left]);
            if (right < static_cast<int>(sortedPitches.size())) seq.push_back(sortedPitches[right]);
            left--;
            right++;
        }
        return seq;
    }

    // Default "up" or "asplayed"
    return sortedPitches;
}

std::vector<MidiNote> MidiPipelineEngine::applyArpeggiator(
    const std::vector<MidiNote>& baseNotes,
    double stepRate,
    int octaves,
    const std::string& pattern,
    double gate,
    double swing,
    const TimeContext* timeContext,
    bool useChordTrackTones) {

    if (baseNotes.empty()) return baseNotes;

    const double rate = std::max(0.125, stepRate);
    const int octs = std::clamp(octaves, 1, 4);

    // Sort notes chronologically by startStep
    std::vector<MidiNote> sorted = baseNotes;
    std::sort(sorted.begin(), sorted.end(), [](const MidiNote& a, const MidiNote& b) {
        return a.startStep < b.startStep;
    });

    // 1. Group notes into time-clusters (chords played together within 0.1 steps)
    std::vector<std::vector<MidiNote>> clusters;
    std::vector<MidiNote> currentCluster;

    for (const auto& note : sorted) {
        if (currentCluster.empty()) {
            currentCluster.push_back(note);
        } else {
            float clusterStart = currentCluster.front().startStep;
            if (std::abs(note.startStep - clusterStart) <= 0.1f) {
                currentCluster.push_back(note);
            } else {
                clusters.push_back(std::move(currentCluster));
                currentCluster.clear();
                currentCluster.push_back(note);
            }
        }
    }
    if (!currentCluster.empty()) {
        clusters.push_back(std::move(currentCluster));
    }

    std::vector<MidiNote> arpedNotes;
    std::mt19937 randGen(12345);

    // 2. Process each note cluster into an arpeggiated sequence
    for (const auto& cluster : clusters) {
        float clusterStart = cluster.front().startStep;
        float clusterEnd = clusterStart + static_cast<float>(rate);
        for (const auto& n : cluster) {
            float end = n.startStep + n.durationSteps;
            if (end > clusterEnd) clusterEnd = end;
        }

        double totalDuration = std::max(rate, static_cast<double>(clusterEnd - clusterStart));
        int stepCount = std::max(1, static_cast<int>(std::round(totalDuration / rate)));

        // Extract pitches
        std::vector<int> rawPitches;
        if (useChordTrackTones && timeContext != nullptr && !timeContext->chordPitchClasses.empty()) {
            int baseOct = cluster.front().pitch / 12;
            for (int pc : timeContext->chordPitchClasses) {
                rawPitches.push_back(std::clamp(baseOct * 12 + pc, 0, 127));
            }
        }

        if (rawPitches.empty()) {
            if (pattern == "asplayed") {
                for (const auto& n : cluster) rawPitches.push_back(n.pitch);
            } else {
                std::set<int> uniquePitches;
                for (const auto& n : cluster) uniquePitches.insert(n.pitch);
                rawPitches.assign(uniquePitches.begin(), uniquePitches.end());
            }
        }

        // Expand pitches across octaves
        std::vector<int> octaveExpanded;
        for (int o = 0; o < octs; ++o) {
            for (int p : rawPitches) {
                octaveExpanded.push_back(std::clamp(p + (o * 12), 0, 127));
            }
        }

        std::vector<int> arpSequence = buildArpSequence(octaveExpanded, pattern);
        const auto& primaryNote = cluster.front();

        for (int s = 0; s < stepCount; ++s) {
            double nominalStart = clusterStart + (s * rate);
            double swingOffset = (s % 2 == 1) ? (std::clamp(swing, 0.0, 0.6) * (rate * 0.35)) : 0.0;
            double finalStart = nominalStart + swingOffset;
            double finalDuration = std::max(0.1, rate * std::clamp(gate, 0.1, 3.0));

            if (pattern == "chord") {
                for (size_t pIdx = 0; pIdx < octaveExpanded.size(); ++pIdx) {
                    MidiNote n;
                    n.id = primaryNote.id + "_arp_" + std::to_string(s) + "_" + std::to_string(pIdx);
                    n.pitch = static_cast<uint8_t>(octaveExpanded[pIdx]);
                    n.startStep = static_cast<float>(finalStart);
                    n.durationSteps = static_cast<float>(finalDuration);
                    n.velocity = primaryNote.velocity;
                    n.column = primaryNote.column;
                    arpedNotes.push_back(n);
                }
            } else {
                int pitch;
                if (pattern == "random") {
                    std::uniform_int_distribution<size_t> dist(0, octaveExpanded.size() - 1);
                    pitch = octaveExpanded[dist(randGen)];
                } else {
                    pitch = arpSequence[s % arpSequence.size()];
                }

                float vel = (s % 4 == 0)
                    ? std::clamp(primaryNote.velocity * 1.05f, 0.1f, 1.0f)
                    : std::clamp(primaryNote.velocity * 0.95f, 0.1f, 1.0f);

                MidiNote n;
                n.id = primaryNote.id + "_arp_" + std::to_string(s);
                n.pitch = static_cast<uint8_t>(pitch);
                n.startStep = static_cast<float>(finalStart);
                n.durationSteps = static_cast<float>(finalDuration);
                n.velocity = vel;
                n.column = primaryNote.column;
                arpedNotes.push_back(n);
            }
        }
    }

    return arpedNotes;
}

std::vector<MidiNote> MidiPipelineEngine::applyChordFollow(
    const std::vector<MidiNote>& notes,
    const TimeContext& timeContext,
    const std::string& mode) {

    if (mode == "off" || timeContext.chordPitchClasses.empty()) {
        return notes;
    }

    std::vector<MidiNote> result;
    result.reserve(notes.size());

    for (const auto& note : notes) {
        MidiNote n = note;
        int oct = note.pitch / 12;

        if (mode == "bass") {
            int bassPc = (timeContext.bassPitchClass >= 0) ? timeContext.bassPitchClass : timeContext.chordPitchClasses.front();
            n.pitch = static_cast<uint8_t>(std::clamp((oct - 1) * 12 + bassPc, 0, 127));
        } else {
            // Find closest chord tone
            int notePc = note.pitch % 12;
            int closestPc = timeContext.chordPitchClasses.front();
            int minDiff = 100;

            for (int pc : timeContext.chordPitchClasses) {
                int diff = std::abs(notePc - pc);
                if (diff < minDiff) {
                    minDiff = diff;
                    closestPc = pc;
                }
            }
            n.pitch = static_cast<uint8_t>(std::clamp(oct * 12 + closestPc, 0, 127));
        }
        result.push_back(n);
    }

    return result;
}

std::vector<MidiNote> MidiPipelineEngine::applyScaleSnap(
    const std::vector<MidiNote>& notes,
    int rootKey,
    bool isMinor) {

    std::vector<MidiNote> result;
    result.reserve(notes.size());

    for (const auto& note : notes) {
        MidiNote n = note;
        n.pitch = static_cast<uint8_t>(snapToScale(note.pitch, rootKey, isMinor));
        result.push_back(n);
    }
    return result;
}

std::vector<MidiNote> MidiPipelineEngine::applyHumanize(
    const std::vector<MidiNote>& notes,
    float timingAmount,
    float velocityAmount,
    uint32_t seed) {

    std::vector<MidiNote> result;
    result.reserve(notes.size());

    std::mt19937 randGen(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (const auto& note : notes) {
        MidiNote n = note;
        float timingOffset = dist(randGen) * timingAmount;
        float velOffset = dist(randGen) * velocityAmount;

        n.startStep = std::max(0.0f, note.startStep + timingOffset);
        n.velocity = std::clamp(note.velocity + velOffset, 0.1f, 1.0f);
        result.push_back(n);
    }
    return result;
}

std::vector<MidiNote> MidiPipelineEngine::applyTranspose(
    const std::vector<MidiNote>& notes,
    int semitones) {

    std::vector<MidiNote> result;
    result.reserve(notes.size());

    for (const auto& note : notes) {
        MidiNote n = note;
        n.pitch = static_cast<uint8_t>(std::clamp(static_cast<int>(note.pitch) + semitones, 0, 127));
        result.push_back(n);
    }
    return result;
}

std::vector<MidiNote> MidiPipelineEngine::generateChordVoicings(
    const std::vector<MidiNote>& triggerNotes,
    const TimeContext& timeContext) {

    const auto& pcs = timeContext.chordPitchClasses.empty()
        ? std::vector<int>{0, 4, 7}
        : timeContext.chordPitchClasses;

    std::vector<MidiNote> result;

    for (const auto& trig : triggerNotes) {
        int oct = std::clamp(trig.pitch / 12, 2, 6);

        // Add bass note
        MidiNote bass;
        bass.id = trig.id + "_bass";
        int bassPc = (timeContext.bassPitchClass >= 0) ? timeContext.bassPitchClass : pcs.front();
        bass.pitch = static_cast<uint8_t>(std::clamp((oct - 1) * 12 + bassPc, 0, 127));
        bass.startStep = trig.startStep;
        bass.durationSteps = trig.durationSteps;
        bass.velocity = std::clamp(trig.velocity * 0.95f, 0.1f, 1.0f);
        result.push_back(bass);

        // Add chord tones
        for (size_t i = 0; i < pcs.size(); ++i) {
            MidiNote tone;
            tone.id = trig.id + "_v" + std::to_string(i);
            tone.pitch = static_cast<uint8_t>(std::clamp(oct * 12 + pcs[i], 0, 127));
            tone.startStep = trig.startStep;
            tone.durationSteps = trig.durationSteps;
            tone.velocity = std::clamp(trig.velocity * (1.0f - static_cast<float>(i) * 0.05f), 0.1f, 1.0f);
            result.push_back(tone);
        }
    }
    return result;
}

static float getParam(const std::map<std::string, float>& params, const std::string& key, float defVal) {
    auto it = params.find(key);
    if (it != params.end()) return it->second;
    std::string lk = toLower(key);
    for (const auto& [k, v] : params) {
        if (toLower(k) == lk) return v;
    }
    return defVal;
}

std::vector<MidiNote> MidiPipelineEngine::evaluateMidiFx(
    const MidiFxInsert& fx,
    const std::vector<MidiNote>& notes,
    const TimeContext& timeContext) {

    if (!fx.enabled) return notes;

    MidiFxType type = fx.type;
    if (type == MidiFxType::Passthrough && (!fx.name.empty() || !fx.eatscriptCode.empty())) {
        type = detectMidiFxType(fx.eatscriptCode, fx.name);
    }

    switch (type) {
        case MidiFxType::Arpeggiator: {
            double rate = getParam(fx.params, "Rate", 1.0f);
            int octaves = static_cast<int>(getParam(fx.params, "Octaves", 2.0f));
            float patternVal = getParam(fx.params, "Pattern", 0.0f);
            std::string patternStr = "up";
            int pInt = static_cast<int>(patternVal);
            if (pInt == 1) patternStr = "down";
            else if (pInt == 2) patternStr = "updown";
            else if (pInt == 3) patternStr = "downup";
            else if (pInt == 4) patternStr = "converge";
            else if (pInt == 5) patternStr = "diverge";
            else if (pInt == 6) patternStr = "random";
            else if (pInt == 7) patternStr = "chord";
            else if (pInt == 8) patternStr = "asplayed";

            double gate = getParam(fx.params, "Gate", 0.85f);
            double swing = getParam(fx.params, "Swing", 0.0f);
            return applyArpeggiator(notes, rate, octaves, patternStr, gate, swing, &timeContext);
        }
        case MidiFxType::ChordArp: {
            double rate = getParam(fx.params, "Rate", 1.0f);
            int octaves = static_cast<int>(getParam(fx.params, "Octaves", 1.0f));
            double gate = getParam(fx.params, "Gate", 0.85f);
            double swing = getParam(fx.params, "Swing", 0.0f);
            return applyArpeggiator(notes, rate, octaves, "up", gate, swing, &timeContext, true);
        }
        case MidiFxType::ChordFollow: {
            float modeVal = getParam(fx.params, "Mode", 0.0f);
            std::string modeStr = "chord";
            int mInt = static_cast<int>(modeVal);
            if (mInt == 1) modeStr = "bass";
            else if (mInt == 2) modeStr = "scale";
            else if (mInt == 3) modeStr = "colorLead";
            return applyChordFollow(notes, timeContext, modeStr);
        }
        case MidiFxType::ScaleSnap: {
            int key = static_cast<int>(getParam(fx.params, "Key", static_cast<float>(timeContext.songKeyRoot)));
            bool isMinor = getParam(fx.params, "Minor", timeContext.isSongKeyMinor ? 1.0f : 0.0f) > 0.5f;
            return applyScaleSnap(notes, key, isMinor);
        }
        case MidiFxType::Humanize: {
            float timing = getParam(fx.params, "Timing", 0.04f);
            float vel = getParam(fx.params, "Velocity", 0.15f);
            return applyHumanize(notes, timing, vel);
        }
        case MidiFxType::ChordStabs: {
            return generateChordVoicings(notes, timeContext);
        }
        case MidiFxType::Transpose: {
            int semitones = static_cast<int>(getParam(fx.params, "Semitones", 0.0f));
            return applyTranspose(notes, semitones);
        }
        case MidiFxType::Eatscript:
        case MidiFxType::Passthrough:
        default:
            return notes;
    }
}

std::vector<MidiNote> MidiPipelineEngine::processPipeline(
    const std::vector<MidiFxInsert>& rack,
    const std::vector<MidiNote>& notes,
    const TimeContext& timeContext) {

    std::vector<MidiNote> currentNotes = notes;
    for (const auto& fx : rack) {
        if (!fx.enabled) continue;
        currentNotes = evaluateMidiFx(fx, currentNotes, timeContext);
    }
    return currentNotes;
}

std::vector<MidiNote> MidiPipelineEngine::trackToNotes(const sequencer::SequencerTrack& track) {
    std::vector<MidiNote> notes;
    uint32_t numSteps = track.getNumSteps();

    for (uint32_t s = 0; s < numSteps; ++s) {
        const auto& step = track.getStep(s);
        if (!step.active) continue;

        MidiNote n;
        n.id = "step_" + std::to_string(s);
        n.pitch = step.note;
        n.startStep = static_cast<float>(s);
        n.durationSteps = step.gateLength;
        n.velocity = step.velocity;
        n.isSlide = step.slide;
        n.isAccent = step.accent;
        notes.push_back(n);
    }
    return notes;
}

void MidiPipelineEngine::notesToTrack(const std::vector<MidiNote>& notes, sequencer::SequencerTrack& track) {
    track.clear();
    uint32_t maxSteps = track.getNumSteps();

    for (const auto& n : notes) {
        uint32_t s = static_cast<uint32_t>(std::round(n.startStep)) % maxSteps;
        sequencer::StepData step;
        step.active = true;
        step.note = n.pitch;
        step.velocity = n.velocity;
        step.gateLength = n.durationSteps;
        step.slide = n.isSlide;
        step.accent = n.isAccent;
        track.setStep(s, step);
    }
}

void MidiPipelineEngine::applyPipelineToTrack(
    const std::vector<MidiFxInsert>& rack,
    sequencer::SequencerTrack& track,
    const TimeContext& timeContext) {

    if (rack.empty()) return;

    std::vector<MidiNote> baseNotes = trackToNotes(track);
    std::vector<MidiNote> processed = processPipeline(rack, baseNotes, timeContext);
    notesToTrack(processed, track);
}

} // namespace eatsbits::eatscript
