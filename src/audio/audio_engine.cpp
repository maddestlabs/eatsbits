#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "eatsbits/audio/audio_engine.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <iostream>
#include <cstring>
#include <cmath>

namespace eatsbits::audio {

struct AudioEngine::Impl {
    ma_device device;
    bool deviceInitialized{false};
};

static void miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* /*pInput*/, ma_uint32 frameCount) {
    auto* engine = static_cast<AudioEngine*>(pDevice->pUserData);
    if (engine && pOutput) {
        engine->audioCallbackInternal(static_cast<float*>(pOutput), frameCount);
    }
}

AudioEngine::AudioEngine() : impl_(std::make_unique<Impl>()) {
    audioBuffer_.inputs = channelInputs_;
    audioBuffer_.outputs = channelOutputs_;
    audioBuffer_.numChannels = 2;
    audioBuffer_.numSamples = 0;
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize(const AudioEngineConfig& config) {
    if (isInitialized_) return true;

    config_ = config;

    polySynth_.setSampleRate(static_cast<float>(config_.sampleRate));
    tb303_.setSampleRate(static_cast<float>(config_.sampleRate));
    masterMixer_.prepare(static_cast<double>(config_.sampleRate), config_.bufferFrameSize);

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = config_.numChannels;
    deviceConfig.sampleRate        = config_.sampleRate;
    deviceConfig.periodSizeInFrames = config_.bufferFrameSize;
    deviceConfig.dataCallback      = miniaudio_data_callback;
    deviceConfig.pUserData         = this;

    if (ma_device_init(nullptr, &deviceConfig, &impl_->device) != MA_SUCCESS) {
        std::cerr << "[Eatsbits] Failed to initialize miniaudio playback device." << std::endl;
        return false;
    }

    impl_->deviceInitialized = true;
    isInitialized_ = true;
    config_.sampleRate = impl_->device.sampleRate;
    config_.bufferFrameSize = impl_->device.playback.internalPeriodSizeInFrames;

    std::cout << "[Eatsbits] Audio device initialized: " << impl_->device.playback.name
              << " | " << config_.sampleRate << " Hz | Buffer: " << config_.bufferFrameSize << " frames" << std::endl;
    return true;
}

bool AudioEngine::start() {
    if (!isInitialized_ || !impl_->deviceInitialized) return false;
    if (isRunning_) return true;

    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        std::cerr << "[Eatsbits] Failed to start audio device." << std::endl;
        return false;
    }

    isRunning_ = true;
    return true;
}

bool AudioEngine::stop() {
    if (!isRunning_) return true;
    if (impl_->deviceInitialized) {
        ma_device_stop(&impl_->device);
    }
    isRunning_ = false;
    return true;
}

void AudioEngine::shutdown() {
    stop();
    if (impl_ && impl_->deviceInitialized) {
        ma_device_uninit(&impl_->device);
        impl_->deviceInitialized = false;
    }
    isInitialized_ = false;
}

bool AudioEngine::isRunning() const noexcept {
    return isRunning_;
}

uint32_t AudioEngine::getSampleRate() const noexcept {
    return config_.sampleRate;
}

uint32_t AudioEngine::getBufferFrameSize() const noexcept {
    return config_.bufferFrameSize;
}

bool AudioEngine::postNoteOn(uint8_t note, float velocity, bool isSlide, bool isAccent) noexcept {
    AudioEvent evt;
    evt.type = AudioEventType::NoteOn;
    evt.note = note;
    evt.velocity = velocity;
    evt.paramId = isSlide ? 1 : 0;
    evt.paramValue = isAccent ? 1.0f : 0.0f;
    return eventQueue_.push(evt);
}

bool AudioEngine::postNoteOff(uint8_t note) noexcept {
    AudioEvent evt;
    evt.type = AudioEventType::NoteOff;
    evt.note = note;
    evt.velocity = 0.0f;
    return eventQueue_.push(evt);
}

bool AudioEngine::postParameter(uint32_t paramId, float value) noexcept {
    AudioEvent evt;
    evt.type = AudioEventType::SetParameter;
    evt.paramId = paramId;
    evt.paramValue = value;
    return eventQueue_.push(evt);
}

bool AudioEngine::postAllNotesOff() noexcept {
    AudioEvent evt;
    evt.type = AudioEventType::AllNotesOff;
    return eventQueue_.push(evt);
}

bool AudioEngine::pollMeterFeedback(MeterFeedback& feedback) noexcept {
    return feedbackQueue_.pop(feedback);
}

void AudioEngine::setEngineMode(SynthEngineMode mode) noexcept {
    engineMode_ = mode;
}

void AudioEngine::setCutoff(float cutoffHz) noexcept {
    polySynth_.setCutoff(cutoffHz);
    tb303_.setCutoff(cutoffHz);
}

void AudioEngine::setResonance(float res) noexcept {
    polySynth_.setResonance(res);
    tb303_.setResonance(res);
}

void AudioEngine::setMasterVolume(float volume) noexcept {
    masterMixer_.setVolume(volume);
}

void AudioEngine::attachPlugin(const EatsPluginDescriptor* desc, void* instance) noexcept {
    attachedPluginDesc_ = desc;
    attachedPluginInstance_ = instance;
    engineMode_ = SynthEngineMode::EatscriptPlugin;
}

void AudioEngine::detachPlugin() noexcept {
    attachedPluginDesc_ = nullptr;
    attachedPluginInstance_ = nullptr;
    engineMode_ = SynthEngineMode::PolySynth;
}

void AudioEngine::processEvents() noexcept {
    AudioEvent evt;
    while (eventQueue_.pop(evt)) {
        switch (evt.type) {
            case AudioEventType::NoteOn:
                if (attachedPluginDesc_ && attachedPluginInstance_) {
                    attachedPluginDesc_->note_on(attachedPluginInstance_, evt.note, evt.velocity);
                }
                if (engineMode_ == SynthEngineMode::Tb303Acid) {
                    const bool isSlide = (evt.paramId != 0);
                    const bool isAccent = (evt.paramValue > 0.5f);
                    tb303_.noteOn(evt.note, evt.velocity, isSlide, isAccent);
                } else {
                    polySynth_.noteOn(evt.note, evt.velocity);
                }
                break;

            case AudioEventType::NoteOff:
                if (attachedPluginDesc_ && attachedPluginInstance_) {
                    attachedPluginDesc_->note_off(attachedPluginInstance_, evt.note);
                }
                if (engineMode_ == SynthEngineMode::Tb303Acid) {
                    tb303_.noteOff();
                } else {
                    polySynth_.noteOff(evt.note);
                }
                break;

            case AudioEventType::AllNotesOff:
                polySynth_.allNotesOff();
                tb303_.reset();
                break;

            case AudioEventType::SetParameter:
                if (attachedPluginDesc_ && attachedPluginInstance_) {
                    attachedPluginDesc_->set_param(attachedPluginInstance_, evt.paramId, evt.paramValue);
                }
                // Standard parameter mapping
                if (evt.paramId == 0) { // Cutoff
                    polySynth_.setCutoff(evt.paramValue);
                    tb303_.setCutoff(evt.paramValue);
                } else if (evt.paramId == 1) { // Resonance
                    polySynth_.setResonance(evt.paramValue);
                    tb303_.setResonance(evt.paramValue);
                } else if (evt.paramId == 2) { // Volume
                    masterMixer_.setVolume(evt.paramValue);
                }
                break;

            default:
                break;
        }
    }
}

void AudioEngine::renderOfflineBlock(float* outL, float* outR, uint32_t frameCount) noexcept {
    if (frameCount > MAX_BLOCK_SIZE) frameCount = MAX_BLOCK_SIZE;

    processEvents();

    if (engineMode_ == SynthEngineMode::Tb303Acid) {
        for (uint32_t i = 0; i < frameCount; ++i) {
            const float s = tb303_.processSample();
            outL[i] = s;
            outR[i] = s;
        }
    } else if (engineMode_ == SynthEngineMode::EatscriptPlugin && attachedPluginDesc_ && attachedPluginInstance_) {
        // Zero-fill outputs before plugin process
        std::memset(outL, 0, frameCount * sizeof(float));
        std::memset(outR, 0, frameCount * sizeof(float));
        float* pluginOutputs[2] = {outL, outR};
        const float* pluginInputs[2] = {outL, outR};
        EatsAudioBuffer pluginBuf{pluginInputs, pluginOutputs, 2, frameCount};
        attachedPluginDesc_->process(attachedPluginInstance_, &pluginBuf);
    } else {
        polySynth_.processBlock(outL, outR, frameCount);
    }

    float* mixerOutputs[2] = {outL, outR};
    const float* mixerInputs[2] = {outL, outR};
    EatsAudioBuffer buf{mixerInputs, mixerOutputs, 2, frameCount};
    masterMixer_.process(buf);

    // Push meter feedback
    feedbackQueue_.push(masterMixer_.getMeterFeedback());
}

void AudioEngine::audioCallbackInternal(float* pOutput, uint32_t frameCount) noexcept {
    const uint32_t framesToRender = std::min(frameCount, static_cast<uint32_t>(MAX_BLOCK_SIZE));

    renderOfflineBlock(scratchL_, scratchR_, framesToRender);

    // Interleave planar scratch buffers into interleaved stereo output
    for (uint32_t i = 0; i < framesToRender; ++i) {
        pOutput[2 * i + 0] = scratchL_[i];
        pOutput[2 * i + 1] = scratchR_[i];
    }
}

} // namespace eatsbits::audio
