#ifndef EATS_STEP_SEQUENCER_HPP
#define EATS_STEP_SEQUENCER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <set>
#include "transport.hpp"
#include "../audio/graph/audio_graph.hpp"
#include "../audio/graph/graph_node.hpp"
#include "eatsbits/lyrics/lyric_track.hpp"

namespace eatsbits::sequencer {

static constexpr size_t MAX_STEPS_PER_TRACK = 512;
static constexpr size_t MAX_ACTIVE_VOICES = 64;

struct StepData {
    bool active{false};
    uint8_t note{60};           // MIDI note number (0 - 127, 60 = Middle C)
    float velocity{0.8f};       // Velocity (0.0 - 1.0)
    float gateLength{0.75f};    // Gate duration as fraction of step (0.1 - 1.0)
    bool slide{false};          // Portamento slide (TB-303)
    bool accent{false};         // Dynamic accent (TB-303 / 808)
    float probability{1.0f};    // Trigger probability (0.0 - 1.0)
    uint32_t paramLockId{0};    // 0 = none, or Node Parameter ID
    float paramLockValue{0.0f}; // Value for parameter lock
    std::vector<uint8_t> extraNotes{}; // Additional polyphonic pitches for chords
    std::string lyric{};        // Synchronized lyric syllable / word
};

class SequencerTrack {
public:
    explicit SequencerTrack(std::string name = "Track", audio::NodeId targetNodeId = 0, uint32_t numSteps = 16)
        : name_(std::move(name)), targetNodeId_(targetNodeId), numSteps_(std::clamp(numSteps, 1u, static_cast<uint32_t>(MAX_STEPS_PER_TRACK))) {
        steps_.fill(StepData{});
    }

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const std::string& getIconRef() const noexcept { return iconRef_; }
    void setIconRef(std::string iconRef) { iconRef_ = std::move(iconRef); }

    [[nodiscard]] audio::NodeId getTargetNodeId() const noexcept { return targetNodeId_; }
    void setTargetNodeId(audio::NodeId id) noexcept { targetNodeId_ = id; }

    [[nodiscard]] uint32_t getNumSteps() const noexcept { return numSteps_; }
    void setNumSteps(uint32_t steps) noexcept {
        numSteps_ = std::clamp(steps, 1u, static_cast<uint32_t>(MAX_STEPS_PER_TRACK));
    }

    [[nodiscard]] bool isMuted() const noexcept { return muted_; }
    void setMuted(bool muted) noexcept { muted_ = muted; }

    [[nodiscard]] bool isSolo() const noexcept { return solo_; }
    void setSolo(bool solo) noexcept { solo_ = solo; }

    [[nodiscard]] float getVolume() const noexcept { return volume_; }
    void setVolume(float volume) noexcept { volume_ = std::max(0.0f, volume); }

    [[nodiscard]] float getPan() const noexcept { return pan_; }
    void setPan(float pan) noexcept { pan_ = std::clamp(pan, -1.0f, 1.0f); }

    [[nodiscard]] int8_t getTranspose() const noexcept { return transpose_; }
    void setTranspose(int8_t semi) noexcept { transpose_ = semi; }

    [[nodiscard]] const StepData& getStep(uint32_t stepIdx) const noexcept {
        return steps_[stepIdx % MAX_STEPS_PER_TRACK];
    }
    [[nodiscard]] StepData& getStep(uint32_t stepIdx) noexcept {
        return steps_[stepIdx % MAX_STEPS_PER_TRACK];
    }

    void setStep(uint32_t stepIdx, const StepData& data) noexcept {
        steps_[stepIdx % MAX_STEPS_PER_TRACK] = data;
    }

    [[nodiscard]] const std::string& getEatscriptCode() const noexcept { return eatscriptCode_; }
    void setEatscriptCode(std::string code) { eatscriptCode_ = std::move(code); }

    // --- Track Freeze ---
    [[nodiscard]] bool isFrozen() const noexcept { return isFrozen_; }
    void setFrozen(bool frozen) noexcept { isFrozen_ = frozen; }
    [[nodiscard]] const std::vector<float>& getFrozenBufferL() const noexcept { return frozenBufferL_; }
    [[nodiscard]] const std::vector<float>& getFrozenBufferR() const noexcept { return frozenBufferR_; }
    [[nodiscard]] const std::string& getFrozenContentHash() const noexcept { return frozenContentHash_; }
    void setFrozenContentHash(std::string hash) { frozenContentHash_ = std::move(hash); }
    [[nodiscard]] uint32_t getFrozenSampleRate() const noexcept { return frozenSampleRate_; }

    void setFrozenBuffers(std::vector<float> left, std::vector<float> right, uint32_t sampleRate = 48000, std::string contentHash = "") {
        frozenBufferL_ = std::move(left);
        frozenBufferR_ = std::move(right);
        frozenSampleRate_ = sampleRate;
        frozenContentHash_ = std::move(contentHash);
        isFrozen_ = !frozenBufferL_.empty();
    }

    void clearFrozenBuffers() noexcept {
        frozenBufferL_.clear();
        frozenBufferR_.clear();
        frozenContentHash_.clear();
        isFrozen_ = false;
    }

    void clear() noexcept {
        steps_.fill(StepData{});
        selectedSteps_.clear();
    }

    // --- Note Selection & Batch Editing ---
    [[nodiscard]] bool isStepSelected(uint32_t stepIdx) const noexcept {
        return selectedSteps_.find(stepIdx) != selectedSteps_.end();
    }
    [[nodiscard]] bool hasSelectedNotes() const noexcept {
        return !selectedSteps_.empty();
    }
    [[nodiscard]] const std::set<uint32_t>& getSelectedSteps() const noexcept {
        return selectedSteps_;
    }
    [[nodiscard]] size_t getSelectedNoteCount() const noexcept {
        return selectedSteps_.size();
    }

    void selectStep(uint32_t stepIdx, bool clearPrevious = true) {
        if (clearPrevious) selectedSteps_.clear();
        if (stepIdx < numSteps_) {
            selectedSteps_.insert(stepIdx);
        }
    }

    void toggleStepSelection(uint32_t stepIdx) {
        if (stepIdx < numSteps_) {
            auto it = selectedSteps_.find(stepIdx);
            if (it != selectedSteps_.end()) {
                selectedSteps_.erase(it);
            } else {
                selectedSteps_.insert(stepIdx);
            }
        }
    }

    void selectSteps(const std::vector<uint32_t>& steps, bool clearPrevious = true) {
        if (clearPrevious) selectedSteps_.clear();
        for (uint32_t s : steps) {
            if (s < numSteps_) selectedSteps_.insert(s);
        }
    }

    void clearSelection() noexcept {
        selectedSteps_.clear();
    }

    void selectAllNotes() noexcept {
        selectedSteps_.clear();
        for (uint32_t s = 0; s < numSteps_; ++s) {
            if (steps_[s].active) {
                selectedSteps_.insert(s);
            }
        }
    }

    void invertNoteSelection() noexcept {
        std::set<uint32_t> inverted;
        for (uint32_t s = 0; s < numSteps_; ++s) {
            if (steps_[s].active && selectedSteps_.find(s) == selectedSteps_.end()) {
                inverted.insert(s);
            }
        }
        selectedSteps_ = std::move(inverted);
    }

    void transposeSelectedNotes(int semitones) noexcept;
    void nudgeSelectedNotes(int deltaSteps) noexcept;
    void changeSelectedNotesDuration(float deltaDuration) noexcept;
    void setSelectedNotesVelocity(float velocity) noexcept;
    void humanizeSelectedNotes(float amount = 0.15f) noexcept;
    void quantizeSelectedNotes(uint32_t snapSteps) noexcept;
    void deleteSelectedNotes() noexcept;
    void setSelectedNotesSlide(bool slide) noexcept;
    void setSelectedNotesAccent(bool accent) noexcept;

    // --- Synchronized Lyrics & Speech Vocalizer ---
    [[nodiscard]] const std::vector<lyrics::LyricCue>& getLyrics() const noexcept { return lyrics_; }
    [[nodiscard]] std::vector<lyrics::LyricCue>& getLyrics() noexcept { return lyrics_; }
    void setLyrics(std::vector<lyrics::LyricCue> lyrics) { lyrics_ = std::move(lyrics); }
    [[nodiscard]] bool hasLyrics() const noexcept { return !lyrics_.empty(); }
    void addLyricCue(lyrics::LyricCue cue) { lyrics_.push_back(std::move(cue)); }
    void clearLyrics() noexcept { lyrics_.clear(); }

private:
    std::string name_;
    std::string iconRef_{"preset:inst_synth"};
    audio::NodeId targetNodeId_{0};
    uint32_t numSteps_{16};
    bool muted_{false};
    bool solo_{false};
    float volume_{0.85f};
    float pan_{0.0f};
    int8_t transpose_{0};
    std::array<StepData, MAX_STEPS_PER_TRACK> steps_{};
    std::string eatscriptCode_{""};
    std::set<uint32_t> selectedSteps_{};

    // Synchronized Lyric Cues
    std::vector<lyrics::LyricCue> lyrics_{};

    // Track Freeze state & baked audio buffers
    bool isFrozen_{false};
    std::string frozenContentHash_{""};
    std::vector<float> frozenBufferL_{};
    std::vector<float> frozenBufferR_{};
    uint32_t frozenSampleRate_{48000};
};

struct Pattern {
    std::string name{"Pattern 1"};
    std::vector<SequencerTrack> tracks;
};

/**
 * Multi-Track, Multi-Pattern Step Sequencer.
 * Sample-accurately dispatches NoteOn, NoteOff, and ParameterLocks to target GraphNodes.
 * Zero-allocation in processBlock() real-time audio thread.
 */
class StepSequencer {
public:
    StepSequencer();

    // Transport controls
    Transport& getTransport() noexcept { return transport_; }
    const Transport& getTransport() const noexcept { return transport_; }

    void start() noexcept { transport_.start(); }
    void stop() noexcept;
    void pause() noexcept { transport_.pause(); }
    [[nodiscard]] bool isPlaying() const noexcept { return transport_.isPlaying(); }

    void setBpm(double bpm) noexcept { transport_.setBpm(bpm); }
    [[nodiscard]] double getBpm() const noexcept { return transport_.getBpm(); }

    void setSwing(double swing) noexcept { transport_.setSwing(swing); }
    [[nodiscard]] double getSwing() const noexcept { return transport_.getSwing(); }

    // Pattern management
    [[nodiscard]] size_t getNumPatterns() const noexcept { return patterns_.size(); }
    [[nodiscard]] uint32_t getActivePatternIndex() const noexcept { return activePatternIndex_; }
    void setActivePatternIndex(uint32_t index) noexcept;

    Pattern& getPattern(size_t index);
    const Pattern& getPattern(size_t index) const;
    size_t addPattern(std::string name = "Pattern");
    void removePattern(size_t index);
    void clearPatterns() noexcept { patterns_.clear(); activePatternIndex_ = 0; }

    // Track helpers on active pattern
    [[nodiscard]] size_t getNumTracks() const noexcept {
        if (activePatternIndex_ < patterns_.size()) {
            return patterns_[activePatternIndex_].tracks.size();
        }
        return 0;
    }
    SequencerTrack* getTrack(size_t trackIdx) noexcept;
    const SequencerTrack* getTrack(size_t trackIdx) const noexcept;
    size_t addTrack(const std::string& name, audio::NodeId targetNodeId, uint32_t numSteps = 16);

    // Real-Time Audio Callback Processing (strictly zero-allocation)
    void processBlock(uint32_t numFrames, audio::AudioGraph& graph) noexcept;

    // Mix pre-rendered frozen audio buffers directly to master audio streams (zero-allocation)
    void mixFrozenTracks(float* outL, float* outR, uint32_t numFrames, uint64_t startSample) noexcept;

    // Reset all notes
    void panic(audio::AudioGraph& graph) noexcept;

private:
    struct ActiveVoice {
        bool active{false};
        audio::NodeId targetNodeId{0};
        uint8_t note{0};
        uint32_t framesRemaining{0};
    };

    void dispatchNoteOff(audio::AudioGraph& graph, const ActiveVoice& voice) noexcept;
    void scheduleNoteOff(audio::NodeId targetNodeId, uint8_t note, uint32_t framesUntilOff) noexcept;

    Transport transport_;
    std::vector<Pattern> patterns_;
    uint32_t activePatternIndex_{0};

    // Pre-allocated active note-off tracking table
    std::array<ActiveVoice, MAX_ACTIVE_VOICES> activeVoices_{};

    // Fast Xorshift PRNG for probability gates (real-time safe)
    uint32_t rngState_{987654321};
    float nextRandomFloat() noexcept {
        rngState_ ^= (rngState_ << 13);
        rngState_ ^= (rngState_ >> 17);
        rngState_ ^= (rngState_ << 5);
        return static_cast<float>(rngState_) * (1.0f / 4294967296.0f);
    }
};

} // namespace eatsbits::sequencer

#endif // EATS_STEP_SEQUENCER_HPP
