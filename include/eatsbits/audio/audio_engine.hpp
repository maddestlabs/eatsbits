#ifndef EATS_AUDIO_ENGINE_HPP
#define EATS_AUDIO_ENGINE_HPP

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include "../abi/eats_plugin_abi.h"
#include "ringbuffer.hpp"
#include "graph/mixer.hpp"
#include "graph/audio_graph.hpp"
#include "dsp/poly_synth.hpp"
#include "dsp/tb303_core.hpp"
#include "../sequencer/step_sequencer.hpp"
#include "export/wav_exporter.hpp"
#include "track_freeze_engine.hpp"

// Forward declaration of miniaudio struct
struct ma_device;

namespace eatsbits::audio {

enum class SynthEngineMode {
    PolySynth,
    Tb303Acid,
    EatscriptPlugin,
    ModularGraph
};

struct AudioEngineConfig {
    uint32_t sampleRate{48000};
    uint32_t bufferFrameSize{128};
    uint32_t numChannels{2};
    std::string deviceName{""};
};

/**
 * High-Performance Low-Latency Real-Time Audio Engine.
 * Implements strict zero-allocation real-time audio loop over miniaudio.
 */
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // Lifecycle
    bool initialize(const AudioEngineConfig& config = {});
    bool start();
    bool stop();
    void shutdown();

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] uint32_t getSampleRate() const noexcept;
    [[nodiscard]] uint32_t getBufferFrameSize() const noexcept;

    // Real-time Safe Event Posting (Called from UI / Worker threads)
    void setActiveTrack(uint32_t trackIndex) noexcept { activeTrackIndex_.store(trackIndex, std::memory_order_relaxed); }
    [[nodiscard]] uint32_t getActiveTrack() const noexcept { return activeTrackIndex_.load(std::memory_order_relaxed); }

    bool postNoteOn(uint8_t note, float velocity, bool isSlide = false, bool isAccent = false, int trackIndex = -1) noexcept;
    bool postNoteOff(uint8_t note, int trackIndex = -1) noexcept;
    bool postTrackNoteOn(uint32_t trackIndex, uint8_t note, float velocity, bool isSlide = false, bool isAccent = false) noexcept;
    bool postTrackNoteOff(uint32_t trackIndex, uint8_t note) noexcept;
    bool postParameter(uint32_t paramId, float value) noexcept;
    bool postNodeParameter(NodeId nodeId, uint32_t paramId, float value) noexcept;
    bool postAllNotesOff() noexcept;
    void panic() noexcept;

    // Feedback Polling (Called from UI rendering loop)
    bool pollMeterFeedback(MeterFeedback& feedback) noexcept;
    bool getTrackMeterFeedback(uint32_t trackIndex, MeterFeedback& feedback) const noexcept;
    void flushMeterFeedback() noexcept;
    size_t getScopeSamples(float* dest, size_t maxCount) noexcept;
    [[nodiscard]] float getSubBassEnergy() const noexcept {
        return subBassEnergy_.load(std::memory_order_relaxed);
    }

    // Synth Mode & Plugin Dispatch
    void setEngineMode(SynthEngineMode mode) noexcept;
    void setCutoff(float cutoffHz) noexcept;
    void setResonance(float res) noexcept;
    void setMasterVolume(float volume) noexcept;
    void setTrackVolume(uint32_t trackIndex, float volume) noexcept;
    void setTrackPan(uint32_t trackIndex, float pan) noexcept;
    void setTrackMute(uint32_t trackIndex, bool mute) noexcept;
    void setTrackSolo(uint32_t trackIndex, bool solo) noexcept;
    void updateMuteSoloRouting() noexcept;

    // Attach native C-ABI plugin instance
    void attachPlugin(const EatsPluginDescriptor* desc, void* instance) noexcept;
    void detachPlugin() noexcept;

    // Modular Audio Graph
    AudioGraph& getGraph() noexcept { return graph_; }
    const AudioGraph& getGraph() const noexcept { return graph_; }
    void setupDefaultAcidGraph();
    void setupDefaultPolyGraph();
    void setupDefaultAcidBeatGraph();

    // Track Audio FX Inserts
    struct TrackAudioFxItem {
        std::string name{"Effect"};
        std::string type{"DELAY"}; // "DELAY", "CHORUS", "COMPRESSOR", "DISTORTION", "EQ", "CONVOLVER", "LIMITER"
        float drive{0.5f};
        float mix{0.8f};
        bool enabled{true};
    };

    bool rebuildTrackAudioFx(uint32_t trackIndex, const std::vector<TrackAudioFxItem>& fxList);
    bool reorderTrackAudioFx(uint32_t trackIndex, size_t fromIdx, size_t toIdx);
    bool setTrackAudioFxParam(uint32_t trackIndex, size_t fxIndex, const std::string& paramName, float value);
    [[nodiscard]] std::vector<TrackAudioFxItem> getTrackAudioFx(uint32_t trackIndex) const;
    [[nodiscard]] NodeId getTrackSourceNodeId(uint32_t trackIndex) const;

    // Step Sequencer
    sequencer::StepSequencer& getSequencer() noexcept { return sequencer_; }
    const sequencer::StepSequencer& getSequencer() const noexcept { return sequencer_; }

    // Project I/O
    bool saveProject(const std::string& path, const std::string& title = "Project");
    bool loadProject(const std::string& path);

    // Audio Bouncing & Export
    exporting::BounceStats bounceProject(const std::string& wavPath,
                                         double durationSeconds = 8.0,
                                         exporting::WavFormat format = exporting::WavFormat::Pcm24,
                                         bool enableDither = true);
    std::vector<exporting::BounceStats> bounceStems(const std::string& outputDir,
                                                   double durationSeconds = 8.0,
                                                   exporting::WavFormat format = exporting::WavFormat::Pcm24,
                                                   bool enableDither = true);

    // Track Freeze & Dynamic Synth/FX Bypass
    bool freezeTrack(uint32_t trackIndex, const trackfreeze::FreezeOptions& options = {});
    bool unfreezeTrack(uint32_t trackIndex, bool clearBuffers = false);
    bool toggleFreezeTrack(uint32_t trackIndex, const trackfreeze::FreezeOptions& options = {});
    [[nodiscard]] bool isTrackFrozen(uint32_t trackIndex) const noexcept;

    // Track Topology Management
    uint32_t addTrack(const std::string& name, NodeId targetNodeId = 0, uint32_t numSteps = 16);
    void removeTrack(uint32_t trackIndex);
    void invalidateTrackStripCache() noexcept;

    // Headless offline processing test helper (renders N frames directly for testing)
    void renderOfflineBlock(float* outL, float* outR, uint32_t frameCount) noexcept;

    // Miniaudio callback hook
    void audioCallbackInternal(float* pOutput, uint32_t frameCount) noexcept;

private:
    void processEvents() noexcept;

    [[nodiscard]] NodeId getTrackGainNodeId(uint32_t trackIndex) const;

    AudioEngineConfig config_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isInitialized_{false};

    // Miniaudio handle (heap-allocated to keep miniaudio.h out of engine public headers)
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Lock-Free Communication Queues
    SpscRingBuffer<AudioEvent, 4096> eventQueue_;
    SpscRingBuffer<MeterFeedback, 512> feedbackQueue_;
    SpscRingBuffer<float, 2048> scopeRingBuffer_;

    // Modular Audio Graph Engine
    AudioGraph graph_;
    mutable std::unordered_map<uint32_t, NodeId> trackGainNodeCache_;
    mutable std::unordered_map<uint32_t, NodeId> trackSourceNodeCache_;
    mutable std::unordered_map<uint32_t, std::vector<NodeId>> trackFxNodeIds_;
    mutable std::unordered_map<uint32_t, std::vector<TrackAudioFxItem>> trackFxConfigs_;

    // Sample-Accurate Step Sequencer
    sequencer::StepSequencer sequencer_;

    // Legacy/Standalone DSP Engines (pre-allocated)
    dsp::PolySynth<16> polySynth_;
    dsp::Tb303Core tb303_;
    MixerStrip masterMixer_;
    SynthEngineMode engineMode_{SynthEngineMode::PolySynth};

    // Attached C-ABI plugin (if any)
    const EatsPluginDescriptor* attachedPluginDesc_{nullptr};
    void* attachedPluginInstance_{nullptr};

    // Scratch buffers for real-time planar audio processing (Zero allocations in callback)
    static constexpr size_t MAX_BLOCK_SIZE = 2048;
    alignas(64) float scratchL_[MAX_BLOCK_SIZE];
    alignas(64) float scratchR_[MAX_BLOCK_SIZE];
    float* channelOutputs_[2]{scratchL_, scratchR_};
    const float* channelInputs_[2]{scratchL_, scratchR_};
    EatsAudioBuffer audioBuffer_{channelInputs_, channelOutputs_, 2, 0};

    // Real-time sub-bass energy detector (<90Hz) for diegetic CRT chassis rumble
    std::atomic<float> subBassEnergy_{0.0f};
    std::atomic<uint32_t> activeTrackIndex_{0};
    float subLp1_{0.0f};
    float subLp2_{0.0f};
    float subEnv_{0.0f};
};

} // namespace eatsbits::audio

#endif // EATS_AUDIO_ENGINE_HPP
