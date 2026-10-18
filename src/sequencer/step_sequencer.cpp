#include "eatsbits/sequencer/step_sequencer.hpp"
#include "eatsbits/theory/chord_model.hpp"
#include <stdexcept>

namespace eatsbits::sequencer {

StepSequencer::StepSequencer() {
    Pattern defaultPattern;
    defaultPattern.name = "Pattern 1";
    patterns_.push_back(std::move(defaultPattern));
    activeVoices_.fill(ActiveVoice{});
}

void StepSequencer::stop() noexcept {
    stop(nullptr);
}

void StepSequencer::stop(audio::AudioGraph* graph) noexcept {
    transport_.stop();
    for (auto& voice : activeVoices_) {
        if (voice.active) {
            if (graph) {
                dispatchNoteOff(*graph, voice);
            }
            voice.active = false;
        }
    }
    if (graph) {
        AudioEvent allOff{};
        allOff.type = AudioEventType::AllNotesOff;
        graph->broadcastEvent(allOff);
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

void StepSequencer::removeTrack(size_t trackIdx) {
    for (auto& pat : patterns_) {
        if (trackIdx < pat.tracks.size() && pat.tracks.size() > 1) {
            pat.tracks.erase(pat.tracks.begin() + trackIdx);
        }
    }
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

    // 2. Intra-block sub-clock arpeggiator processing (for sub-step rates: 1/32, 1/64, 1/128, 50Hz C64)
    if (activePatternIndex_ < patterns_.size()) {
        auto& activePattern = patterns_[activePatternIndex_];
        bool hasSolo = false;
        for (const auto& track : activePattern.tracks) {
            if (track.isSolo()) { hasSolo = true; break; }
        }

        for (auto& track : activePattern.tracks) {
            if (!track.isArpActive() || track.getArpPitches().empty() || track.isMuted() || track.isFrozen()) {
                continue;
            }
            if (hasSolo && !track.isSolo()) continue;

            double samplesPerTick = track.getSamplesPerArpTick();
            // Trigger sub-step ticks for fast rates (1/32, 1/64, 1/128, 50Hz)
            if (samplesPerTick > 0.0 && track.getArpRate() < 1.0f) {
                double counter = track.getArpSampleCounter() + numFrames;
                if (counter >= samplesPerTick) {
                    while (counter >= samplesPerTick) {
                        counter -= samplesPerTick;
                        track.incrementArpCycleIndex();
                        const auto& pitches = track.getArpPitches();
                        size_t pIdx = track.getArpCycleIndex() % pitches.size();
                        uint8_t nextNote = pitches[pIdx];

                        // Send NoteOff for previous arp note
                        if (track.getLastArpNote() != 0 && track.getTargetNodeId() != 0) {
                            AudioEvent offEv{};
                            offEv.type = AudioEventType::NoteOff;
                            offEv.note = track.getLastArpNote();
                            graph.sendNodeEvent(track.getTargetNodeId(), offEv);
                        }

                        // Send NoteOn for new arp note
                        AudioEvent onEv{};
                        onEv.type = AudioEventType::NoteOn;
                        onEv.note = nextNote;
                        onEv.velocity = track.getArpVelocity();
                        if (track.getTargetNodeId() != 0) {
                            graph.sendNodeEvent(track.getTargetNodeId(), onEv);
                        }
                        track.setLastArpNote(nextNote);

                        uint32_t gateFrames = static_cast<uint32_t>(std::clamp(track.getArpGate() * samplesPerTick, 32.0, samplesPerTick * 1.5));
                        scheduleNoteOff(track.getTargetNodeId(), nextNote, gateFrames);
                    }
                }
                track.setArpSampleCounter(counter);
            }
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

        // 1. Sample Active Chord from Project Chord Track (Eatsbeats Parity)
        float currentBar = static_cast<float>(tick.stepIndex) / 16.0f;
        const theory::ChordEvent* activeChord = getActiveChordAtBar(currentBar);
        if (activeChord != nullptr) {
            timeContext_.activeChordRoot = activeChord->rootPitchClass;
            timeContext_.activeChordQuality = theory::getChordQualityDisplayName(activeChord->quality);
            timeContext_.chordPitchClasses = activeChord->getPitchClasses();
            timeContext_.bassPitchClass = (activeChord->bassPitchClass >= 0) ? activeChord->bassPitchClass : activeChord->rootPitchClass;
        }
        timeContext_.currentStep = static_cast<double>(tick.stepIndex);

        for (auto& track : activePattern.tracks) {
            if (track.isFrozen()) continue; // Skip live note triggering when track is frozen
            if (track.isMuted()) continue;
            if (hasSolo && !track.isSolo()) continue;

            uint32_t stepIdx = tick.stepIndex % track.getNumSteps();
            const StepData& step = track.getStep(stepIdx);

            bool isArpContinuation = !step.active && track.isArpActive() && track.getArpStepsRemaining() > 0 && !track.getArpPitches().empty();

            if (!step.active && !isArpContinuation) {
                track.setArpActive(false);
                track.setArpStepsRemaining(0);
                continue;
            }

            int noteNum = 60;
            float vel = 0.8f;
            std::vector<uint8_t> exNotes;
            bool slide = false;
            bool accent = false;
            float gateDurationSteps = 0.75f;

            if (isArpContinuation) {
                track.decrementArpSteps();
                if (track.getArpStepsRemaining() == 0) {
                    track.setArpActive(false);
                }
                if (track.getArpRate() < 1.0f) {
                    // Sub-step engine handles intra-step triggers continuously
                    continue;
                }
                uint32_t stepInterval = static_cast<uint32_t>(std::max(1.0f, std::round(track.getArpRate())));
                if (stepInterval > 1 && (tick.stepIndex % stepInterval != 0)) {
                    continue;
                }
                track.incrementArpCycleIndex();
                const auto& pitches = track.getArpPitches();
                size_t pIdx = track.getArpCycleIndex() % pitches.size();
                noteNum = pitches[pIdx];
                vel = track.getArpVelocity();
                gateDurationSteps = std::clamp(track.getArpGate(), 0.1f, 3.5f);
            } else {
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

                noteNum = std::clamp(static_cast<int>(step.note) + track.getTranspose(), 0, 127);
                vel = std::clamp(step.velocity * track.getVolume(), 0.0f, 1.0f);
                exNotes = step.extraNotes;
                slide = step.slide;
                accent = step.accent;
                gateDurationSteps = step.gateLength;

                // 2. Harmonic Track Follow Mode (BASS, CHORD, SCALE, COLOR)
                if (track.getChordFollowMode() != theory::ChordFollowMode::Off && activeChord != nullptr) {
                    noteNum = theory::ChordTheory::remapPitchForChord(noteNum, *activeChord, track.getChordFollowMode());
                    for (auto& en : exNotes) {
                        int enTrans = std::clamp(static_cast<int>(en) + track.getTranspose(), 0, 127);
                        en = static_cast<uint8_t>(theory::ChordTheory::remapPitchForChord(enTrans, *activeChord, track.getChordFollowMode()));
                    }
                }

                // Live MIDI FX Pipeline Processing
                const auto& midiFx = track.getMidiFxRack();
                bool hasArp = false;
                int arpOctaves = 2;
                std::string arpPattern = "up";
                float arpGate = 0.75f;
                float arpRate = 1.0f;
                if (!midiFx.empty()) {
                    for (const auto& fx : midiFx) {
                        if (!fx.enabled) continue;
                        if (fx.type == eatscript::MidiFxType::ScaleSnap) {
                            int root = fx.params.count("rootKey") ? static_cast<int>(fx.params.at("rootKey")) : (fx.params.count("root") ? static_cast<int>(fx.params.at("root")) : timeContext_.songKeyRoot);
                            bool minor = fx.params.count("scaleMode") ? (fx.params.at("scaleMode") > 0.5f) : (fx.params.count("scale") ? (fx.params.at("scale") > 0.5f) : timeContext_.isSongKeyMinor);
                            noteNum = eatscript::MidiPipelineEngine::snapToScale(noteNum, root, minor);
                            for (auto& en : exNotes) {
                                int enTrans = std::clamp(static_cast<int>(en) + track.getTranspose(), 0, 127);
                                en = static_cast<uint8_t>(eatscript::MidiPipelineEngine::snapToScale(enTrans, root, minor));
                            }
                        } else if (fx.type == eatscript::MidiFxType::ChordStabs) {
                            if (exNotes.empty()) {
                                if (activeChord != nullptr) {
                                    for (int pc : timeContext_.chordPitchClasses) {
                                        if (pc != (noteNum % 12)) {
                                            int chordNote = ((noteNum / 12) * 12) + pc;
                                            if (chordNote < noteNum) chordNote += 12;
                                            exNotes.push_back(static_cast<uint8_t>(std::clamp(chordNote, 0, 127)));
                                        }
                                    }
                                } else {
                                    bool isMinor = timeContext_.isSongKeyMinor;
                                    int third = isMinor ? 3 : 4;
                                    int fifth = 7;
                                    exNotes.push_back(static_cast<uint8_t>(std::clamp(noteNum + third, 0, 127)));
                                    exNotes.push_back(static_cast<uint8_t>(std::clamp(noteNum + fifth, 0, 127)));
                                }
                            }
                        } else if (fx.type == eatscript::MidiFxType::Transpose) {
                            int semi = fx.params.count("semitones") ? static_cast<int>(fx.params.at("semitones")) : (fx.params.count("transpose") ? static_cast<int>(fx.params.at("transpose")) : 0);
                            noteNum = std::clamp(noteNum + semi, 0, 127);
                            for (auto& en : exNotes) {
                                en = static_cast<uint8_t>(std::clamp(static_cast<int>(en) + semi, 0, 127));
                            }
                        } else if (fx.type == eatscript::MidiFxType::Humanize) {
                            float velAmt = fx.params.count("humanizeVelocity") ? fx.params.at("humanizeVelocity") : (fx.params.count("velocity") ? fx.params.at("velocity") : 0.20f);
                            float velJitter = (nextRandomFloat() - 0.5f) * velAmt;
                            vel = std::clamp(vel + velJitter, 0.05f, 1.0f);
                        } else if (fx.type == eatscript::MidiFxType::Arpeggiator || fx.type == eatscript::MidiFxType::ChordArp) {
                            hasArp = true;
                            arpOctaves = fx.params.count("Octaves") ? std::max(1, static_cast<int>(fx.params.at("Octaves"))) : (fx.params.count("octaves") ? std::max(1, static_cast<int>(fx.params.at("octaves"))) : 2);
                            if (fx.params.count("Rate")) arpRate = fx.params.at("Rate");
                            else if (fx.params.count("rate")) arpRate = fx.params.at("rate");
                            if (fx.params.count("Pattern")) {
                                int pInt = static_cast<int>(fx.params.at("Pattern"));
                                static const char* kPats[] = {"up", "down", "updown", "downup", "converge", "diverge", "random", "chord", "asplayed"};
                                if (pInt >= 0 && pInt < 9) arpPattern = kPats[pInt];
                            } else if (fx.params.count("pattern")) {
                                int pInt = std::clamp(static_cast<int>(std::round(fx.params.at("pattern") * 8.0f)), 0, 8);
                                static const char* kPats[] = {"up", "down", "updown", "downup", "converge", "diverge", "random", "chord", "asplayed"};
                                arpPattern = kPats[pInt];
                            }
                            if (fx.params.count("Gate")) arpGate = fx.params.at("Gate");
                            else if (fx.params.count("gate")) arpGate = fx.params.at("gate");

                            if (fx.type == eatscript::MidiFxType::ChordArp && exNotes.empty()) {
                                if (activeChord != nullptr && !timeContext_.chordPitchClasses.empty()) {
                                    for (int pc : timeContext_.chordPitchClasses) {
                                        if (pc != (noteNum % 12)) {
                                            int chordNote = ((noteNum / 12) * 12) + pc;
                                            if (chordNote < noteNum) chordNote += 12;
                                            exNotes.push_back(static_cast<uint8_t>(std::clamp(chordNote, 0, 127)));
                                        }
                                    }
                                } else {
                                    bool isMinor = timeContext_.isSongKeyMinor;
                                    int third = isMinor ? 3 : 4;
                                    int fifth = 7;
                                    exNotes.push_back(static_cast<uint8_t>(std::clamp(noteNum + third, 0, 127)));
                                    exNotes.push_back(static_cast<uint8_t>(std::clamp(noteNum + fifth, 0, 127)));
                                }
                            }
                        } else if (fx.type == eatscript::MidiFxType::ChordFollow) {
                            if (activeChord != nullptr) {
                                noteNum = theory::ChordTheory::remapPitchForChord(noteNum, *activeChord, theory::ChordFollowMode::Chord);
                                for (auto& en : exNotes) {
                                    int enTrans = std::clamp(static_cast<int>(en) + track.getTranspose(), 0, 127);
                                    en = static_cast<uint8_t>(theory::ChordTheory::remapPitchForChord(enTrans, *activeChord, theory::ChordFollowMode::Chord));
                                }
                            } else {
                                theory::ChordEvent chEvent;
                                chEvent.rootPitchClass = timeContext_.activeChordRoot;
                                noteNum = theory::ChordTheory::remapPitchForChord(noteNum, chEvent, theory::ChordFollowMode::Chord);
                                for (auto& en : exNotes) {
                                    int enTrans = std::clamp(static_cast<int>(en) + track.getTranspose(), 0, 127);
                                    en = static_cast<uint8_t>(theory::ChordTheory::remapPitchForChord(enTrans, chEvent, theory::ChordFollowMode::Chord));
                                }
                            }
                        }
                    }
                }

                if (hasArp) {
                    std::vector<int> sortedPitches;
                    sortedPitches.push_back(noteNum);
                    for (uint8_t en : exNotes) {
                        sortedPitches.push_back(static_cast<int>(en));
                    }
                    std::sort(sortedPitches.begin(), sortedPitches.end());

                    std::vector<int> multiOctave;
                    for (int oct = 0; oct < arpOctaves; ++oct) {
                        for (int p : sortedPitches) {
                            multiOctave.push_back(std::clamp(p + oct * 12, 0, 127));
                        }
                    }

                    auto patterned = eatscript::MidiPipelineEngine::generatePatternSequence(multiOctave, arpPattern);
                    std::vector<uint8_t> allPitches;
                    allPitches.reserve(patterned.size());
                    for (int p : patterned) {
                        allPitches.push_back(static_cast<uint8_t>(std::clamp(p, 0, 127)));
                    }

                    exNotes.clear();
                    size_t nextCycle = 0;
                    if (track.isArpActive() && !track.getArpPitches().empty() && track.getArpPitches() == allPitches) {
                        nextCycle = (track.getArpCycleIndex() + 1) % allPitches.size();
                    }
                    track.setArpActive(true);
                    track.setArpPitches(allPitches);
                    track.setArpCycleIndex(nextCycle);
                    track.setArpVelocity(vel);
                    track.setArpRate(arpRate);
                    track.setArpGate(arpGate);
                    double samplesPerTick = std::max(32.0, static_cast<double>(arpRate) * tick.stepDurationSamples);
                    track.setSamplesPerArpTick(samplesPerTick);
                    track.setArpSampleCounter(0.0);
                    uint32_t sustainedSteps = (step.gateLength > 1.0f)
                                                  ? (static_cast<uint32_t>(step.gateLength) - 1)
                                                  : static_cast<uint32_t>(allPitches.size() > 1 ? (allPitches.size() - 1) : 3);
                    track.setArpStepsRemaining(sustainedSteps);

                    if (!allPitches.empty()) {
                        noteNum = allPitches[nextCycle];
                    }
                    track.setLastArpNote(static_cast<uint8_t>(noteNum));
                    gateDurationSteps = std::clamp(arpGate, 0.1f, 3.5f);
                } else {
                    track.setArpActive(false);
                    track.setArpStepsRemaining(0);
                }
            }

            // Note on dispatch
            AudioEvent noteEvent{};
            noteEvent.type = AudioEventType::NoteOn;
            noteEvent.note = static_cast<uint8_t>(noteNum);
            noteEvent.velocity = vel;
            // Channel byte bit0 = slide, bit1 = accent
            noteEvent.channel = (slide ? 0x01 : 0x00) | (accent ? 0x02 : 0x00);

            if (track.getTargetNodeId() != 0) {
                graph.sendNodeEvent(track.getTargetNodeId(), noteEvent);
            }

            // Schedule NoteOff
            if (track.getTargetNodeId() != 0 && (!slide || gateDurationSteps < 1.0f)) {
                double duration = std::clamp(gateDurationSteps, 0.05f, 128.0f) * tick.stepDurationSamples;
                uint32_t framesUntilOff = tick.frameOffset + static_cast<uint32_t>(duration);
                scheduleNoteOff(track.getTargetNodeId(), noteEvent.note, framesUntilOff);
                for (uint8_t exNote : exNotes) {
                    scheduleNoteOff(track.getTargetNodeId(), exNote, framesUntilOff);
                }
            }

            // Dispatch extra chord notes
            if (track.getTargetNodeId() != 0) {
                for (uint8_t exNote : exNotes) {
                    AudioEvent exEvent = noteEvent;
                    exEvent.note = exNote;
                    graph.sendNodeEvent(track.getTargetNodeId(), exEvent);
                }
            }
        }
    }
}

void StepSequencer::scheduleNoteOff(audio::NodeId targetNodeId, uint8_t note, uint32_t framesUntilOff) noexcept {
    // 1. If this exact note on this targetNodeId already has a pending NoteOff, update its duration
    for (size_t i = 0; i < MAX_ACTIVE_VOICES; ++i) {
        if (activeVoices_[i].active && activeVoices_[i].targetNodeId == targetNodeId && activeVoices_[i].note == note) {
            activeVoices_[i].framesRemaining = framesUntilOff;
            return;
        }
    }

    // 2. Find free slot
    for (size_t i = 0; i < MAX_ACTIVE_VOICES; ++i) {
        if (!activeVoices_[i].active) {
            activeVoices_[i].active = true;
            activeVoices_[i].targetNodeId = targetNodeId;
            activeVoices_[i].note = note;
            activeVoices_[i].framesRemaining = framesUntilOff;
            return;
        }
    }

    // 3. If full, steal slot with smallest remaining time
    size_t minIdx = 0;
    uint32_t minFrames = activeVoices_[0].framesRemaining;
    for (size_t i = 1; i < MAX_ACTIVE_VOICES; ++i) {
        if (activeVoices_[i].framesRemaining < minFrames) {
            minFrames = activeVoices_[i].framesRemaining;
            minIdx = i;
        }
    }
    activeVoices_[minIdx].active = true;
    activeVoices_[minIdx].targetNodeId = targetNodeId;
    activeVoices_[minIdx].note = note;
    activeVoices_[minIdx].framesRemaining = framesUntilOff;
}

void StepSequencer::dispatchNoteOff(audio::AudioGraph& graph, const ActiveVoice& voice) noexcept {
    AudioEvent offEvent{};
    offEvent.type = AudioEventType::NoteOff;
    offEvent.note = voice.note;
    offEvent.velocity = 0.0f;

    if (voice.targetNodeId != 0) {
        graph.sendNodeEvent(voice.targetNodeId, offEvent);
    }
}

void StepSequencer::mixFrozenTracks(float* outL, float* outR, uint32_t numFrames, uint64_t startSample) noexcept {
    if (activePatternIndex_ >= patterns_.size() || numFrames == 0 || (!outL && !outR)) {
        return;
    }
    const auto& activePattern = patterns_[activePatternIndex_];
    bool hasSolo = false;
    for (const auto& track : activePattern.tracks) {
        if (track.isSolo()) {
            hasSolo = true;
            break;
        }
    }

    for (const auto& track : activePattern.tracks) {
        if (!track.isFrozen()) continue;
        if (track.isMuted()) continue;
        if (hasSolo && !track.isSolo()) continue;

        const auto& fzL = track.getFrozenBufferL();
        if (fzL.empty()) continue;
        const auto& fzR = track.getFrozenBufferR().empty() ? fzL : track.getFrozenBufferR();
        const size_t len = fzL.size();
        if (len == 0) continue;

        const float vol = track.getVolume();
        const float pan = track.getPan();
        const float panL = std::clamp(1.0f - pan, 0.0f, 1.0f);
        const float panR = std::clamp(1.0f + pan, 0.0f, 1.0f);
        const float gainL = vol * panL;
        const float gainR = vol * panR;

        for (uint32_t i = 0; i < numFrames; ++i) {
            size_t idx = static_cast<size_t>((startSample + i) % len);
            if (outL) outL[i] += fzL[idx] * gainL;
            if (outR) outR[i] += fzR[idx] * gainR;
        }
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
            steps_[s].gateLength = std::clamp(steps_[s].gateLength + deltaDuration, 0.05f, 64.0f);
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

const theory::ChordEvent* StepSequencer::getActiveChordAtBar(float bar) const noexcept {
    for (const auto& chord : chordTrack_) {
        float start = static_cast<float>(chord.startBar);
        float end = start + chord.barLength;
        if (bar >= start && bar < end) {
            return &chord;
        }
    }
    return nullptr;
}

} // namespace eatsbits::sequencer

