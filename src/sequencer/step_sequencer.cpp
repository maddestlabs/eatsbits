#include "eatsbits/sequencer/step_sequencer.hpp"
#include <stdexcept>

namespace eatsbits::sequencer {

StepSequencer::StepSequencer() {
    Pattern defaultPattern;
    defaultPattern.name = "Pattern 1";
    patterns_.push_back(std::move(defaultPattern));
    activeVoices_.fill(ActiveVoice{});
}

void StepSequencer::stop() noexcept {
    transport_.stop();
    for (auto& voice : activeVoices_) {
        voice.active = false;
    }
}

void StepSequencer::setActivePatternIndex(uint32_t index) noexcept {
    if (index < patterns_.size()) {
        activePatternIndex_ = index;
    }
}

Pattern& StepSequencer::getPattern(size_t index) {
    if (index >= patterns_.size()) {
        throw std::out_of_range("Pattern index out of range");
    }
    return patterns_[index];
}

const Pattern& StepSequencer::getPattern(size_t index) const {
    if (index >= patterns_.size()) {
        throw std::out_of_range("Pattern index out of range");
    }
    return patterns_[index];
}

size_t StepSequencer::addPattern(std::string name) {
    Pattern p;
    p.name = std::move(name);
    patterns_.push_back(std::move(p));
    return patterns_.size() - 1;
}

void StepSequencer::removePattern(size_t index) {
    if (patterns_.size() > 1 && index < patterns_.size()) {
        patterns_.erase(patterns_.begin() + index);
        if (activePatternIndex_ >= patterns_.size()) {
            activePatternIndex_ = static_cast<uint32_t>(patterns_.size() - 1);
        }
    }
}

SequencerTrack* StepSequencer::getTrack(size_t trackIdx) noexcept {
    if (activePatternIndex_ < patterns_.size()) {
        auto& tracks = patterns_[activePatternIndex_].tracks;
        if (trackIdx < tracks.size()) {
            return &tracks[trackIdx];
        }
    }
    return nullptr;
}

const SequencerTrack* StepSequencer::getTrack(size_t trackIdx) const noexcept {
    if (activePatternIndex_ < patterns_.size()) {
        const auto& tracks = patterns_[activePatternIndex_].tracks;
        if (trackIdx < tracks.size()) {
            return &tracks[trackIdx];
        }
    }
    return nullptr;
}

size_t StepSequencer::addTrack(const std::string& name, audio::NodeId targetNodeId, uint32_t numSteps) {
    if (activePatternIndex_ >= patterns_.size()) {
        addPattern();
    }
    patterns_[activePatternIndex_].tracks.emplace_back(name, targetNodeId, numSteps);
    return patterns_[activePatternIndex_].tracks.size() - 1;
}

void StepSequencer::processBlock(uint32_t numFrames, audio::AudioGraph& graph) noexcept {
    if (!transport_.isPlaying()) {
        return;
    }

    // 1. Process active sounding voices for note-offs
    for (size_t i = 0; i < MAX_ACTIVE_VOICES; ++i) {
        if (!activeVoices_[i].active) continue;

        if (activeVoices_[i].framesRemaining <= numFrames) {
            dispatchNoteOff(graph, activeVoices_[i]);
            activeVoices_[i].active = false;
        } else {
            activeVoices_[i].framesRemaining -= numFrames;
        }
    }

    // 2. Advance transport clock and gather step triggers
    std::array<StepTick, 8> ticks{};
    size_t tickCount = transport_.advance(numFrames, ticks);
    if (tickCount == 0 || activePatternIndex_ >= patterns_.size()) {
        return;
    }

    auto& activePattern = patterns_[activePatternIndex_];
    if (activePattern.tracks.empty()) {
        return;
    }

    // Check solo status
    bool hasSolo = false;
    for (const auto& track : activePattern.tracks) {
        if (track.isSolo()) {
            hasSolo = true;
            break;
        }
    }

    // 3. Dispatch notes for each triggered step tick
    for (size_t t = 0; t < tickCount; ++t) {
        const auto& tick = ticks[t];

        for (auto& track : activePattern.tracks) {
            if (track.isMuted()) continue;
            if (hasSolo && !track.isSolo()) continue;

            uint32_t stepIdx = tick.stepIndex % track.getNumSteps();
            const StepData& step = track.getStep(stepIdx);
            if (!step.active) continue;

            // Probability gate check
            if (step.probability < 1.0f && nextRandomFloat() > step.probability) {
                continue;
            }

            // Parameter lock dispatch
            if (step.paramLockId > 0) {
                AudioEvent pEvent{};
                pEvent.type = AudioEventType::SetParameter;
                pEvent.paramId = step.paramLockId;
                pEvent.paramValue = step.paramLockValue;
                if (track.getTargetNodeId() != 0) {
                    graph.sendNodeEvent(track.getTargetNodeId(), pEvent);
                } else {
                    graph.broadcastEvent(pEvent);
                }
            }

            // Note on dispatch
            AudioEvent noteEvent{};
            noteEvent.type = AudioEventType::NoteOn;
            int noteNum = std::clamp(static_cast<int>(step.note) + track.getTranspose(), 0, 127);
            noteEvent.note = static_cast<uint8_t>(noteNum);
            noteEvent.velocity = std::clamp(step.velocity * track.getVolume(), 0.0f, 1.0f);
            // Channel byte bit0 = slide, bit1 = accent
            noteEvent.channel = (step.slide ? 0x01 : 0x00) | (step.accent ? 0x02 : 0x00);

            if (track.getTargetNodeId() != 0) {
                graph.sendNodeEvent(track.getTargetNodeId(), noteEvent);
            } else {
                graph.broadcastEvent(noteEvent);
            }

            // Schedule NoteOff
            if (!step.slide || step.gateLength < 1.0f) {
                double duration = std::clamp(step.gateLength, 0.05f, 1.0f) * tick.stepDurationSamples;
                uint32_t framesUntilOff = tick.frameOffset + static_cast<uint32_t>(duration);
                scheduleNoteOff(track.getTargetNodeId(), noteEvent.note, framesUntilOff);
            }
        }
    }
}

void StepSequencer::scheduleNoteOff(audio::NodeId targetNodeId, uint8_t note, uint32_t framesUntilOff) noexcept {
    // Find free slot
    for (size_t i = 0; i < MAX_ACTIVE_VOICES; ++i) {
        if (!activeVoices_[i].active) {
            activeVoices_[i].active = true;
            activeVoices_[i].targetNodeId = targetNodeId;
            activeVoices_[i].note = note;
            activeVoices_[i].framesRemaining = framesUntilOff;
            return;
        }
    }
    // If full, replace slot 0
    activeVoices_[0].active = true;
    activeVoices_[0].targetNodeId = targetNodeId;
    activeVoices_[0].note = note;
    activeVoices_[0].framesRemaining = framesUntilOff;
}

void StepSequencer::dispatchNoteOff(audio::AudioGraph& graph, const ActiveVoice& voice) noexcept {
    AudioEvent offEvent{};
    offEvent.type = AudioEventType::NoteOff;
    offEvent.note = voice.note;
    offEvent.velocity = 0.0f;

    if (voice.targetNodeId != 0) {
        graph.sendNodeEvent(voice.targetNodeId, offEvent);
    } else {
        graph.broadcastEvent(offEvent);
    }
}

void StepSequencer::panic(audio::AudioGraph& graph) noexcept {
    for (auto& voice : activeVoices_) {
        if (voice.active) {
            dispatchNoteOff(graph, voice);
            voice.active = false;
        }
    }
    AudioEvent allOff{};
    allOff.type = AudioEventType::AllNotesOff;
    graph.broadcastEvent(allOff);
}

// =============================================================================
// SequencerTrack Selection & Batch Editing
// =============================================================================

void SequencerTrack::transposeSelectedNotes(int semitones) noexcept {
    if (semitones == 0 || selectedSteps_.empty()) return;
    for (uint32_t s : selectedSteps_) {
        if (s < numSteps_ && steps_[s].active) {
            int newPitch = std::clamp(static_cast<int>(steps_[s].note) + semitones, 0, 127);
            steps_[s].note = static_cast<uint8_t>(newPitch);
        }
    }
}

void SequencerTrack::nudgeSelectedNotes(int deltaSteps) noexcept {
    if (deltaSteps == 0 || selectedSteps_.empty()) return;

    std::vector<std::pair<uint32_t, StepData>> moving;
    for (uint32_t s : selectedSteps_) {
        if (s < numSteps_ && steps_[s].active) {
            moving.push_back({s, steps_[s]});
        }
    }
    if (moving.empty()) return;

    for (const auto& item : moving) {
        int target = static_cast<int>(item.first) + deltaSteps;
        if (target < 0 || target >= static_cast<int>(numSteps_)) {
            return;
        }
    }

    for (const auto& item : moving) {
        steps_[item.first] = StepData{};
    }

    std::set<uint32_t> newSelected;
    for (const auto& item : moving) {
        uint32_t target = static_cast<uint32_t>(static_cast<int>(item.first) + deltaSteps);
        steps_[target] = item.second;
        newSelected.insert(target);
    }
    selectedSteps_ = std::move(newSelected);
}

void SequencerTrack::changeSelectedNotesDuration(float deltaDuration) noexcept {
    if (std::abs(deltaDuration) < 0.001f || selectedSteps_.empty()) return;
    for (uint32_t s : selectedSteps_) {
        if (s < numSteps_ && steps_[s].active) {
            steps_[s].gateLength = std::clamp(steps_[s].gateLength + deltaDuration, 0.05f, 4.0f);
        }
    }
}

void SequencerTrack::setSelectedNotesVelocity(float velocity) noexcept {
    if (selectedSteps_.empty()) return;
    float clampedVel = std::clamp(velocity, 0.05f, 1.0f);
    for (uint32_t s : selectedSteps_) {
        if (s < numSteps_ && steps_[s].active) {
            steps_[s].velocity = clampedVel;
        }
    }
}

void SequencerTrack::humanizeSelectedNotes(float amount) noexcept {
    if (selectedSteps_.empty()) return;
    for (uint32_t s : selectedSteps_) {
        if (s < numSteps_ && steps_[s].active) {
            float delta = ((static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f) * amount;
            steps_[s].velocity = std::clamp(steps_[s].velocity + delta, 0.10f, 1.0f);
        }
    }
}

void SequencerTrack::quantizeSelectedNotes(uint32_t snapSteps) noexcept {
    if (snapSteps <= 1 || selectedSteps_.empty()) return;
    std::vector<std::pair<uint32_t, StepData>> moving;
    for (uint32_t s : selectedSteps_) {
        if (s < numSteps_ && steps_[s].active) {
            moving.push_back({s, steps_[s]});
        }
    }
    for (const auto& item : moving) {
        steps_[item.first] = StepData{};
    }
    std::set<uint32_t> newSelected;
    for (const auto& item : moving) {
        uint32_t snapped = (item.first / snapSteps) * snapSteps;
        if (snapped >= numSteps_) snapped = numSteps_ - 1;
        steps_[snapped] = item.second;
        newSelected.insert(snapped);
    }
    selectedSteps_ = std::move(newSelected);
}

void SequencerTrack::deleteSelectedNotes() noexcept {
    for (uint32_t s : selectedSteps_) {
        if (s < numSteps_) {
            steps_[s] = StepData{};
        }
    }
    selectedSteps_.clear();
}

void SequencerTrack::setSelectedNotesSlide(bool slide) noexcept {
    for (uint32_t s : selectedSteps_) {
        if (s < numSteps_ && steps_[s].active) {
            steps_[s].slide = slide;
        }
    }
}

void SequencerTrack::setSelectedNotesAccent(bool accent) noexcept {
    for (uint32_t s : selectedSteps_) {
        if (s < numSteps_ && steps_[s].active) {
            steps_[s].accent = accent;
        }
    }
}

} // namespace eatsbits::sequencer

