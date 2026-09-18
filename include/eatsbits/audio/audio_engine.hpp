#ifndef EATS_AUDIO_ENGINE_HPP
#define EATS_AUDIO_ENGINE_HPP

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "../abi/eats_plugin_abi.h"
#include "ringbuffer.hpp"
#include "graph/mixer.hpp"
#include "dsp/poly_synth.hpp"
#include "dsp/tb303_core.hpp"

// Forward declaration of miniaudio struct
struct ma_device;

namespace eatsbits::audio {

enum class SynthEngineMode {
    PolySynth,
    Tb303Acid,
    EatscriptPlugin
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
    bool postNoteOn(uint8_t note, float velocity, bool isSlide = false, bool isAccent = false) noexcept;
    bool postNoteOff(uint8_t note) noexcept;
    bool postParameter(uint32_t paramId, float value) noexcept;
    bool postAllNotesOff() noexcept;

    // Feedback Polling (Called from UI rendering loop)
    bool pollMeterFeedback(MeterFeedback& feedback) noexcept;

    // Synth Mode & Plugin Dispatch
    void setEngineMode(SynthEngineMode mode) noexcept;
    void setCutoff(float cutoffHz) noexcept;
    void setResonance(float res) noexcept;
    void setMasterVolume(float volume) noexcept;

    // Attach native C-ABI plugin instance
    void attachPlugin(const EatsPluginDescriptor* desc, void* instance) noexcept;
    void detachPlugin() noexcept;

    // Headless offline processing test helper (renders N frames directly for testing)
    void renderOfflineBlock(float* outL, float* outR, uint32_t frameCount) noexcept;

    // Miniaudio callback hook
    void audioCallbackInternal(float* pOutput, uint32_t frameCount) noexcept;

private:
    void processEvents() noexcept;

    AudioEngineConfig config_;
    bool isRunning_{false};
    bool isInitialized_{false};

    // Miniaudio handle (heap-allocated to keep miniaudio.h out of engine public headers)
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Lock-Free Communication Queues
    SpscRingBuffer<AudioEvent, 4096> eventQueue_;
    SpscRingBuffer<MeterFeedback, 512> feedbackQueue_;

    // DSP Engines (pre-allocated)
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
};

} // namespace eatsbits::audio

#endif // EATS_AUDIO_ENGINE_HPP
