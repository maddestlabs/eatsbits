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
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/poly_synth_node.hpp"
#include "eatsbits/audio/graph/nodes/biquad_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/chorus_node.hpp"
#include "eatsbits/audio/graph/nodes/compressor_node.hpp"
#include "eatsbits/audio/graph/nodes/parametric_eq_node.hpp"
#include "eatsbits/audio/graph/nodes/convolver_node.hpp"
#include "eatsbits/audio/graph/nodes/limiter_node.hpp"
#include "eatsbits/audio/graph/nodes/waveshaper_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/project/project_file.hpp"

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
    sequencer_.getTransport().setSampleRate(config_.sampleRate);

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

    // Re-align DSP voice engines, mixer, and transport with the negotiated hardware sample rate
    polySynth_.setSampleRate(static_cast<float>(config_.sampleRate));
    tb303_.setSampleRate(static_cast<float>(config_.sampleRate));
    masterMixer_.prepare(static_cast<double>(config_.sampleRate), config_.bufferFrameSize);
    sequencer_.getTransport().setSampleRate(config_.sampleRate);

    std::cout << "[Eatsbits] Audio device initialized: " << impl_->device.playback.name
              << " | " << config_.sampleRate << " Hz | Buffer: " << config_.bufferFrameSize << " frames" << std::endl;

    graph_.prepare(config_.sampleRate, config_.bufferFrameSize);
    return true;
}

void AudioEngine::setupDefaultAcidGraph() {
    graph_.clear();
    invalidateTrackStripCache();
    auto tb = std::make_shared<Tb303Node>("Tb303");
    auto delay = std::make_shared<DelayNode>("AcidEcho");
    auto gain = std::make_shared<GainNode>("MasterGain");

    delay->setDelayTimeMs(250.0f);
    delay->setFeedback(0.30f);
    delay->setDryWet(0.20f);

    NodeId tbId = graph_.addNode(tb);
    NodeId delayId = graph_.addNode(delay);
    NodeId gainId = graph_.addNode(gain);

    graph_.connect(tbId, 0, delayId, 0);
    graph_.connect(delayId, 0, gainId, 0);
    graph_.setOutputNode(gainId, 0);

    engineMode_ = SynthEngineMode::ModularGraph;
}

void AudioEngine::setupDefaultPolyGraph() {
    graph_.clear();
    invalidateTrackStripCache();
    auto poly = std::make_shared<PolySynthNode>("PolySynth");
    auto biquad = std::make_shared<BiquadNode>("Filter");
    auto delay = std::make_shared<DelayNode>("Delay");
    auto gain = std::make_shared<GainNode>("MasterGain");

    NodeId polyId = graph_.addNode(poly);
    NodeId biquadId = graph_.addNode(biquad);
    NodeId delayId = graph_.addNode(delay);
    NodeId gainId = graph_.addNode(gain);

    graph_.connect(polyId, 0, biquadId, 0);
    graph_.connect(biquadId, 0, delayId, 0);
    graph_.connect(delayId, 0, gainId, 0);
    graph_.setOutputNode(gainId, 0);

    engineMode_ = SynthEngineMode::ModularGraph;
}

void AudioEngine::setupDefaultAcidBeatGraph() {
    graph_.clear();
    invalidateTrackStripCache();

    // Track 1: TB-303 Acid
    auto tb = std::make_shared<Tb303Node>("Tb303");
    auto delay = std::make_shared<DelayNode>("AcidEcho");
    auto tbStrip = std::make_shared<GainNode>("Track1_Gain");

    delay->setDelayTimeMs(125.0f); // 16th note echo
    delay->setFeedback(0.35f);
    delay->setDryWet(0.30f);
    tbStrip->setVolume(0.80f);

    // Track 2: TR-808 Drums
    auto drums808 = std::make_shared<DrumKitNode>("Drums808");
    auto drum808Strip = std::make_shared<GainNode>("Track2_Gain");
    drum808Strip->setVolume(0.85f);

    // Track 3: TR-909 Drums
    auto drums909 = std::make_shared<DrumKitNode>("Drums909");
    auto drum909Strip = std::make_shared<GainNode>("Track3_Gain");
    drum909Strip->setVolume(0.80f);

    // Track 4: DX7 Rhodes (PolySynth configured with warm EP/Rhodes parameters)
    auto dx7 = std::make_shared<PolySynthNode>("Dx7Rhodes");
    dx7->setWaveform(::eatsbits::dsp::Waveform::Triangle);
    dx7->setFilterCutoff(3400.0f);
    dx7->setFilterResonance(1.1f);
    dx7->setAdsr(0.015f, 1.8f, 0.55f, 0.8f);
    auto dx7Strip = std::make_shared<GainNode>("Track4_Gain");
    dx7Strip->setVolume(0.75f);

    // Track 5: Concert Grand (PolySynth with rich harmonic piano timbre)
    auto piano = std::make_shared<PolySynthNode>("ConcertGrand");
    piano->setWaveform(::eatsbits::dsp::Waveform::Saw);
    piano->setFilterCutoff(4200.0f);
    piano->setFilterResonance(1.0f);
    piano->setAdsr(0.008f, 2.4f, 0.35f, 1.0f);
    auto pianoStrip = std::make_shared<GainNode>("Track5_Gain");
    pianoStrip->setVolume(0.75f);

    // Master Output
    auto masterGain = std::make_shared<GainNode>("MasterOut");
    masterGain->setVolume(0.85f); // Bus headroom to prevent clipping when summing

    NodeId tbId = graph_.addNode(tb);
    NodeId delayId = graph_.addNode(delay);
    NodeId tbStripId = graph_.addNode(tbStrip);

    NodeId drums808Id = graph_.addNode(drums808);
    NodeId drum808StripId = graph_.addNode(drum808Strip);

    NodeId drums909Id = graph_.addNode(drums909);
    NodeId drum909StripId = graph_.addNode(drum909Strip);

    NodeId dx7Id = graph_.addNode(dx7);
    NodeId dx7StripId = graph_.addNode(dx7Strip);

    NodeId pianoId = graph_.addNode(piano);
    NodeId pianoStripId = graph_.addNode(pianoStrip);

    NodeId masterId = graph_.addNode(masterGain);

    // Route Track 1: Tb303 -> AcidEcho -> Track1_Gain -> MasterOut
    graph_.connect(tbId, 0, delayId, 0);
    graph_.connect(delayId, 0, tbStripId, 0);
    graph_.connect(tbStripId, 0, masterId, 0);

    // Route Track 2: Drums808 -> Track2_Gain -> MasterOut
    graph_.connect(drums808Id, 0, drum808StripId, 0);
    graph_.connect(drum808StripId, 0, masterId, 0);

    // Route Track 3: Drums909 -> Track3_Gain -> MasterOut
    graph_.connect(drums909Id, 0, drum909StripId, 0);
    graph_.connect(drum909StripId, 0, masterId, 0);

    // Route Track 4: Dx7Rhodes -> Track4_Gain -> MasterOut
    graph_.connect(dx7Id, 0, dx7StripId, 0);
    graph_.connect(dx7StripId, 0, masterId, 0);

    // Route Track 5: ConcertGrand -> Track5_Gain -> MasterOut
    graph_.connect(pianoId, 0, pianoStripId, 0);
    graph_.connect(pianoStripId, 0, masterId, 0);

    // Register Source Nodes & Default Insert FX Caches
    trackSourceNodeCache_[0] = tbId;
    trackFxNodeIds_[0] = {delayId};
    trackFxConfigs_[0] = {{"AcidEcho", "DELAY", 0.35f, 0.30f, true}};

    trackSourceNodeCache_[1] = drums808Id;
    trackFxNodeIds_[1] = {};
    trackFxConfigs_[1] = {};

    trackSourceNodeCache_[2] = drums909Id;
    trackFxNodeIds_[2] = {};
    trackFxConfigs_[2] = {};

    trackSourceNodeCache_[3] = dx7Id;
    trackFxNodeIds_[3] = {};
    trackFxConfigs_[3] = {};

    trackSourceNodeCache_[4] = pianoId;
    trackFxNodeIds_[4] = {};
    trackFxConfigs_[4] = {};

    graph_.setOutputNode(masterId, 0);
    engineMode_ = SynthEngineMode::ModularGraph;
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

bool AudioEngine::postNoteOn(uint8_t note, float velocity, bool isSlide, bool isAccent, int trackIndex) noexcept {
    AudioEvent evt{};
    evt.type = AudioEventType::NoteOn;
    evt.note = note;
    evt.velocity = velocity;
    evt.paramId = isSlide ? 1 : 0;
    evt.paramValue = isAccent ? 1.0f : 0.0f;

    uint32_t tIdx = (trackIndex >= 0) ? static_cast<uint32_t>(trackIndex) : activeTrackIndex_.load(std::memory_order_relaxed);
    NodeId targetNodeId = 0;
    if (tIdx < sequencer_.getNumTracks()) {
        const auto* trk = sequencer_.getTrack(tIdx);
        if (trk) {
            targetNodeId = trk->getTargetNodeId();
        }
    }
    if (targetNodeId == 0 && tIdx < 5) {
        targetNodeId = static_cast<NodeId>(tIdx + 1);
    }
    evt.channel = static_cast<uint8_t>(targetNodeId);
    return eventQueue_.push(evt);
}

bool AudioEngine::postNoteOff(uint8_t note, int trackIndex) noexcept {
    AudioEvent evt{};
    evt.type = AudioEventType::NoteOff;
    evt.note = note;
    evt.velocity = 0.0f;

    uint32_t tIdx = (trackIndex >= 0) ? static_cast<uint32_t>(trackIndex) : activeTrackIndex_.load(std::memory_order_relaxed);
    NodeId targetNodeId = 0;
    if (tIdx < sequencer_.getNumTracks()) {
        const auto* trk = sequencer_.getTrack(tIdx);
        if (trk) {
            targetNodeId = trk->getTargetNodeId();
        }
    }
    if (targetNodeId == 0 && tIdx < 5) {
        targetNodeId = static_cast<NodeId>(tIdx + 1);
    }
    evt.channel = static_cast<uint8_t>(targetNodeId);
    return eventQueue_.push(evt);
}

bool AudioEngine::postTrackNoteOn(uint32_t trackIndex, uint8_t note, float velocity, bool isSlide, bool isAccent) noexcept {
    return postNoteOn(note, velocity, isSlide, isAccent, static_cast<int>(trackIndex));
}

bool AudioEngine::postTrackNoteOff(uint32_t trackIndex, uint8_t note) noexcept {
    return postNoteOff(note, static_cast<int>(trackIndex));
}

bool AudioEngine::postParameter(uint32_t paramId, float value) noexcept {
    AudioEvent evt{};
    evt.type = AudioEventType::SetParameter;
    evt.channel = 0;
    evt.paramId = paramId;
    evt.paramValue = value;
    return eventQueue_.push(evt);
}

bool AudioEngine::postNodeParameter(NodeId nodeId, uint32_t paramId, float value) noexcept {
    AudioEvent evt{};
    evt.type = AudioEventType::SetParameter;
    evt.channel = static_cast<uint8_t>(nodeId);
    evt.paramId = paramId;
    evt.paramValue = value;
    return eventQueue_.push(evt);
}

bool AudioEngine::postAllNotesOff() noexcept {
    AudioEvent evt{};
    evt.type = AudioEventType::AllNotesOff;
    return eventQueue_.push(evt);
}

void AudioEngine::panic() noexcept {
    // 1. Halt sequencer playhead and active voice registry
    sequencer_.stop();

    // 2. Kill all active notes on standalone synths
    polySynth_.allNotesOff();
    tb303_.reset();

    // 3. Reset modular audio graph (clears delay lines, IIR filter states, drum playheads)
    graph_.reset();

    // 4. Drain pending audio event queue
    AudioEvent evt;
    while (eventQueue_.pop(evt)) {}

    // 5. Zero out intermediate scratch rendering buffers
    std::fill_n(scratchL_, MAX_BLOCK_SIZE, 0.0f);
    std::fill_n(scratchR_, MAX_BLOCK_SIZE, 0.0f);

    // 6. Drain oscilloscope ring buffer and feedback queue to eliminate visual/audio artifacts
    float dummySample = 0.0f;
    while (scopeRingBuffer_.pop(dummySample)) {}
    MeterFeedback dummyFeedback{};
    while (feedbackQueue_.pop(dummyFeedback)) {}
}

bool AudioEngine::pollMeterFeedback(MeterFeedback& feedback) noexcept {
    MeterFeedback fb{};
    bool hadAny = false;
    float maxPeakL = 0.0f;
    float maxPeakR = 0.0f;
    float sumRmsL = 0.0f;
    float sumRmsR = 0.0f;
    int count = 0;

    while (feedbackQueue_.pop(fb)) {
        hadAny = true;
        if (fb.peakLeft > maxPeakL) maxPeakL = fb.peakLeft;
        if (fb.peakRight > maxPeakR) maxPeakR = fb.peakRight;
        sumRmsL += fb.rmsLeft;
        sumRmsR += fb.rmsRight;
        count++;
    }

    if (hadAny && count > 0) {
        feedback.peakLeft = maxPeakL;
        feedback.peakRight = maxPeakR;
        feedback.rmsLeft = sumRmsL / static_cast<float>(count);
        feedback.rmsRight = sumRmsR / static_cast<float>(count);
        return true;
    }
    return false;
}

void AudioEngine::flushMeterFeedback() noexcept {
    MeterFeedback dummyFeedback{};
    while (feedbackQueue_.pop(dummyFeedback)) {}
}

void AudioEngine::invalidateTrackStripCache() noexcept {
    trackGainNodeCache_.clear();
    trackSourceNodeCache_.clear();
    trackFxNodeIds_.clear();
    trackFxConfigs_.clear();
}

NodeId AudioEngine::getTrackSourceNodeId(uint32_t trackIndex) const {
    auto it = trackSourceNodeCache_.find(trackIndex);
    if (it != trackSourceNodeCache_.end() && it->second != INVALID_NODE_ID) {
        if (graph_.getNode(it->second)) {
            return it->second;
        }
    }

    if (trackIndex < sequencer_.getNumTracks()) {
        const auto* trk = sequencer_.getTrack(trackIndex);
        if (trk && trk->getTargetNodeId() != 0 && graph_.getNode(trk->getTargetNodeId())) {
            trackSourceNodeCache_[trackIndex] = trk->getTargetNodeId();
            return trk->getTargetNodeId();
        }
    }

    static const char* kDefaultSources[] = {
        "Tb303", "Drums808", "Drums909", "Dx7Rhodes", "ConcertGrand"
    };
    if (trackIndex < 5) {
        NodeId foundId = graph_.findNodeByName(kDefaultSources[trackIndex]);
        if (foundId != INVALID_NODE_ID) {
            trackSourceNodeCache_[trackIndex] = foundId;
            return foundId;
        }
    }

    return INVALID_NODE_ID;
}

bool AudioEngine::rebuildTrackAudioFx(uint32_t trackIndex, const std::vector<TrackAudioFxItem>& fxList) {
    NodeId srcId = getTrackSourceNodeId(trackIndex);
    NodeId gainId = getTrackGainNodeId(trackIndex);
    if (srcId == INVALID_NODE_ID || gainId == INVALID_NODE_ID) {
        return false;
    }

    // 1. Remove previous dynamic FX nodes for this track
    auto fxIt = trackFxNodeIds_.find(trackIndex);
    if (fxIt != trackFxNodeIds_.end()) {
        for (NodeId oldId : fxIt->second) {
            graph_.removeNode(oldId);
        }
        fxIt->second.clear();
    }

    // 2. Disconnect existing direct connection between srcId and gainId
    graph_.disconnect(srcId, 0, gainId, 0);

    // 3. Build new serialized chain between srcId and gainId
    NodeId prevNode = srcId;
    std::vector<NodeId> newFxNodeIds;

    for (const auto& fx : fxList) {
        if (!fx.enabled) continue; // true bypass: bypasses node connection

        std::string searchKey = fx.type + " " + fx.name;
        std::string tUpper;
        tUpper.reserve(searchKey.size());
        for (char c : searchKey) tUpper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));

        std::shared_ptr<GraphNode> node;
        if (tUpper.find("DELAY") != std::string::npos || tUpper.find("ECHO") != std::string::npos ||
            tUpper.find("PING-PONG") != std::string::npos || tUpper == "ACIDECHO") {
            auto d = std::make_shared<DelayNode>(fx.name.empty() ? "StereoDelay" : fx.name);
            d->setDelayTimeMs(std::max(10.0f, fx.drive * 800.0f));
            d->setFeedback(std::clamp(fx.drive * 0.75f, 0.0f, 0.90f));
            d->setDryWet(std::clamp(fx.mix, 0.0f, 1.0f));
            node = d;
        } else if (tUpper.find("CONVOLVER") != std::string::npos || tUpper.find("REVERB") != std::string::npos ||
                   tUpper.find("ZERO-LATENCY IR") != std::string::npos || tUpper.find("ROOM") != std::string::npos ||
                   tUpper.find("HALL") != std::string::npos || tUpper.find("SPACE") != std::string::npos) {
            auto conv = std::make_shared<ConvolverNode>(fx.name.empty() ? "ConvolverReverb" : fx.name);
            conv->setMix(std::clamp(fx.mix, 0.0f, 1.0f));
            conv->setDecay(0.4f + fx.drive * 3.6f);
            conv->setRoomSize(0.5f + fx.drive * 0.9f);
            node = conv;
        } else if (tUpper.find("CHORUS") != std::string::npos || tUpper.find("FLANGER") != std::string::npos ||
                   tUpper.find("BUCKET") != std::string::npos || tUpper.find("BBD") != std::string::npos) {
            auto ch = std::make_shared<ChorusNode>(fx.name.empty() ? "ChorusFlanger" : fx.name);
            ch->setRateHz(std::max(0.1f, fx.drive * 4.0f));
            ch->setDepthMs(std::max(0.5f, fx.drive * 6.0f));
            ch->setMix(std::clamp(fx.mix, 0.0f, 1.0f));
            node = ch;
        } else if (tUpper.find("COMP") != std::string::npos || tUpper.find("DYNAMICS") != std::string::npos) {
            auto comp = std::make_shared<CompressorNode>(fx.name.empty() ? "Compressor" : fx.name);
            comp->setThreshold(-30.0f + (1.0f - fx.drive) * 24.0f);
            comp->setRatio(1.5f + fx.drive * 6.0f);
            comp->setMix(std::clamp(fx.mix, 0.0f, 1.0f));
            node = comp;
        } else if (tUpper.find("LIMIT") != std::string::npos) {
            auto lim = std::make_shared<LimiterNode>(fx.name.empty() ? "BrickwallLimiter" : fx.name);
            lim->setCeilingDbfs(-0.1f);
            node = lim;
        } else if (tUpper.find("EQ") != std::string::npos || tUpper.find("PARAMETRIC") != std::string::npos) {
            auto eq = std::make_shared<ParametricEqNode>(fx.name.empty() ? "ParametricEQ" : fx.name);
            node = eq;
        } else if (tUpper.find("CRUSH") != std::string::npos || tUpper.find("BIT") != std::string::npos) {
            auto ws = std::make_shared<WaveShaperNode>(fx.name.empty() ? "Bitcrusher" : fx.name);
            ws->setDrive(1.5f + fx.drive * 6.0f);
            ws->setMix(std::clamp(fx.mix, 0.0f, 1.0f));
            node = ws;
        } else { // DISTORTION / TUBE_DISTORTION / SHAPER / fallback
            auto ws = std::make_shared<WaveShaperNode>(fx.name.empty() ? "TubeDistortion" : fx.name);
            ws->setDrive(1.0f + fx.drive * 6.0f);
            ws->setMix(std::clamp(fx.mix, 0.0f, 1.0f));
            node = ws;
        }

        NodeId fxId = graph_.addNode(node);
        newFxNodeIds.push_back(fxId);
        graph_.connect(prevNode, 0, fxId, 0);
        prevNode = fxId;
    }

    graph_.connect(prevNode, 0, gainId, 0);
    trackFxNodeIds_[trackIndex] = std::move(newFxNodeIds);
    trackFxConfigs_[trackIndex] = fxList;
    graph_.compile();
    return true;
}

bool AudioEngine::reorderTrackAudioFx(uint32_t trackIndex, size_t fromIdx, size_t toIdx) {
    auto it = trackFxConfigs_.find(trackIndex);
    if (it == trackFxConfigs_.end()) return false;
    auto list = it->second;
    if (fromIdx >= list.size() || toIdx >= list.size() || fromIdx == toIdx) return false;

    auto item = list[fromIdx];
    list.erase(list.begin() + fromIdx);
    list.insert(list.begin() + toIdx, item);

    return rebuildTrackAudioFx(trackIndex, list);
}

bool AudioEngine::setTrackAudioFxParam(uint32_t trackIndex, size_t fxIndex, const std::string& paramName, float value) {
    auto it = trackFxNodeIds_.find(trackIndex);
    if (it == trackFxNodeIds_.end() || fxIndex >= it->second.size()) return false;
    NodeId fxId = it->second[fxIndex];
    auto node = graph_.getNode(fxId);
    if (!node) return false;

    if (auto delay = std::dynamic_pointer_cast<DelayNode>(node)) {
        if (paramName == "time" || paramName == "param1" || paramName == "rate") delay->setDelayTimeMs(std::max(10.0f, value * 800.0f));
        else if (paramName == "feedback" || paramName == "param2") delay->setFeedback(std::clamp(value * 0.75f, 0.0f, 0.90f));
        else if (paramName == "mix" || paramName == "param4" || paramName == "wet") delay->setDryWet(std::clamp(value, 0.0f, 1.0f));
        else if (paramName == "drive") delay->setDelayTimeMs(std::max(10.0f, value * 800.0f));
    } else if (auto conv = std::dynamic_pointer_cast<ConvolverNode>(node)) {
        if (paramName == "mix" || paramName == "param4" || paramName == "wet") conv->setMix(std::clamp(value, 0.0f, 1.0f));
        else if (paramName == "decay" || paramName == "time" || paramName == "param1" || paramName == "drive") conv->setDecay(0.3f + value * 4.0f);
        else if (paramName == "predelay" || paramName == "param2") conv->setPreDelay(value * 200.0f);
        else if (paramName == "roomsize" || paramName == "size" || paramName == "param3") conv->setRoomSize(0.5f + value * 1.0f);
        else if (paramName == "damping" || paramName == "param5") conv->setDamping(value);
    } else if (auto chorus = std::dynamic_pointer_cast<ChorusNode>(node)) {
        if (paramName == "rate" || paramName == "param1" || paramName == "time") chorus->setRateHz(std::max(0.1f, value * 4.0f));
        else if (paramName == "depth" || paramName == "param2" || paramName == "feedback") chorus->setDepthMs(std::max(0.5f, value * 6.0f));
        else if (paramName == "mix" || paramName == "param4" || paramName == "wet") chorus->setMix(std::clamp(value, 0.0f, 1.0f));
        else if (paramName == "drive") chorus->setRateHz(std::max(0.1f, value * 4.0f));
    } else if (auto comp = std::dynamic_pointer_cast<CompressorNode>(node)) {
        if (paramName == "threshold" || paramName == "param1") comp->setThreshold(-30.0f + (1.0f - value) * 24.0f);
        else if (paramName == "ratio" || paramName == "param2" || paramName == "drive") comp->setRatio(1.5f + value * 6.0f);
        else if (paramName == "mix" || paramName == "param4" || paramName == "wet") comp->setMix(std::clamp(value, 0.0f, 1.0f));
    } else if (auto ws = std::dynamic_pointer_cast<WaveShaperNode>(node)) {
        if (paramName == "drive" || paramName == "param1" || paramName == "param5" || paramName == "time") ws->setDrive(1.0f + value * 8.0f);
        else if (paramName == "mix" || paramName == "param4" || paramName == "wet") ws->setMix(std::clamp(value, 0.0f, 1.0f));
    }

    auto cfgIt = trackFxConfigs_.find(trackIndex);
    if (cfgIt != trackFxConfigs_.end() && fxIndex < cfgIt->second.size()) {
        if (paramName == "drive" || paramName == "param1" || paramName == "time" || paramName == "rate" || paramName == "decay") {
            cfgIt->second[fxIndex].drive = value;
        } else if (paramName == "mix" || paramName == "param4" || paramName == "wet") {
            cfgIt->second[fxIndex].mix = value;
        }
    }

    return true;
}

std::vector<AudioEngine::TrackAudioFxItem> AudioEngine::getTrackAudioFx(uint32_t trackIndex) const {
    auto it = trackFxConfigs_.find(trackIndex);
    if (it != trackFxConfigs_.end()) {
        return it->second;
    }
    return {};
}

NodeId AudioEngine::getTrackGainNodeId(uint32_t trackIndex) const {
    auto it = trackGainNodeCache_.find(trackIndex);
    if (it != trackGainNodeCache_.end() && it->second != INVALID_NODE_ID) {
        if (graph_.getNode(it->second)) {
            return it->second;
        }
    }

    std::string stripName = "Track" + std::to_string(trackIndex + 1) + "_Gain";
    NodeId foundId = graph_.findNodeByName(stripName);
    trackGainNodeCache_[trackIndex] = foundId;
    return foundId;
}

bool AudioEngine::getTrackMeterFeedback(uint32_t trackIndex, MeterFeedback& feedback) const noexcept {
    NodeId stripId = getTrackGainNodeId(trackIndex);
    if (stripId != INVALID_NODE_ID) {
        auto node = graph_.getNode(stripId);
        if (auto* gn = dynamic_cast<GainNode*>(node.get())) {
            float pL = 0.0f, pR = 0.0f;
            gn->getPeakLevels(pL, pR);
            feedback.peakLeft = pL;
            feedback.peakRight = pR;
            feedback.rmsLeft = pL * 0.707f;
            feedback.rmsRight = pR * 0.707f;
            return true;
        }
    }

    if (trackIndex < sequencer_.getNumTracks()) {
        const auto* trk = sequencer_.getTrack(trackIndex);
        if (trk) {
            auto node = graph_.getNode(trk->getTargetNodeId());
            if (auto* gn = dynamic_cast<GainNode*>(node.get())) {
                float pL = 0.0f, pR = 0.0f;
                gn->getPeakLevels(pL, pR);
                feedback.peakLeft = pL;
                feedback.peakRight = pR;
                feedback.rmsLeft = pL * 0.707f;
                feedback.rmsRight = pR * 0.707f;
                return true;
            }
        }
    }

    // Default graph fallback for Track 0 or master
    if (trackIndex == 0) {
        NodeId outId = graph_.getOutputNodeId();
        auto outNode = graph_.getNode(outId);
        if (auto* gn = dynamic_cast<GainNode*>(outNode.get())) {
            float pL = 0.0f, pR = 0.0f;
            gn->getPeakLevels(pL, pR);
            feedback.peakLeft = pL;
            feedback.peakRight = pR;
            feedback.rmsLeft = pL * 0.707f;
            feedback.rmsRight = pR * 0.707f;
            return true;
        }
    }

    return false;
}

size_t AudioEngine::getScopeSamples(float* dest, size_t maxCount) noexcept {
    if (!dest || maxCount == 0) return 0;
    size_t count = 0;
    while (count < maxCount && scopeRingBuffer_.pop(dest[count])) {
        count++;
    }
    return count;
}

void AudioEngine::setEngineMode(SynthEngineMode mode) noexcept {
    engineMode_ = mode;
}

void AudioEngine::setCutoff(float cutoffHz) noexcept {
    polySynth_.setCutoff(cutoffHz);
    tb303_.setCutoff(cutoffHz);
    for (const auto& [id, node] : graph_.getNodeSnapshot()) {
        if (auto* tb = dynamic_cast<Tb303Node*>(node.get())) {
            tb->setCutoff(cutoffHz);
        }
    }
}

void AudioEngine::setResonance(float res) noexcept {
    polySynth_.setResonance(res);
    tb303_.setResonance(res);
    for (const auto& [id, node] : graph_.getNodeSnapshot()) {
        if (auto* tb = dynamic_cast<Tb303Node*>(node.get())) {
            tb->setResonance(res);
        }
    }
}

void AudioEngine::setMasterVolume(float volume) noexcept {
    masterMixer_.setVolume(volume);
}

void AudioEngine::setTrackVolume(uint32_t trackIndex, float volume) noexcept {
    if (trackIndex < sequencer_.getNumTracks()) {
        auto* tr = sequencer_.getTrack(trackIndex);
        if (tr) tr->setVolume(volume);
    }
    NodeId stripId = getTrackGainNodeId(trackIndex);
    if (stripId != INVALID_NODE_ID) {
        postNodeParameter(stripId, 0, volume);
        return;
    }
    if (trackIndex < sequencer_.getNumTracks()) {
        auto* tr = sequencer_.getTrack(trackIndex);
        if (tr && tr->getTargetNodeId() != 0) {
            auto nodePtr = graph_.getNode(tr->getTargetNodeId());
            if (nodePtr && dynamic_cast<DrumKitNode*>(nodePtr.get())) {
                postNodeParameter(tr->getTargetNodeId(), 0, volume);
            }
        }
    }
}

void AudioEngine::setTrackPan(uint32_t trackIndex, float pan) noexcept {
    if (trackIndex < sequencer_.getNumTracks()) {
        auto* tr = sequencer_.getTrack(trackIndex);
        if (tr) tr->setPan(pan);
    }
    NodeId stripId = getTrackGainNodeId(trackIndex);
    if (stripId != INVALID_NODE_ID) {
        postNodeParameter(stripId, 1, pan);
    }
}

void AudioEngine::updateMuteSoloRouting() noexcept {
    bool hasSolo = false;
    uint32_t numTracks = sequencer_.getNumTracks();
    for (uint32_t i = 0; i < numTracks; ++i) {
        auto* tr = sequencer_.getTrack(i);
        if (tr && tr->isSolo()) {
            hasSolo = true;
            break;
        }
    }

    for (uint32_t i = 0; i < numTracks; ++i) {
        auto* tr = sequencer_.getTrack(i);
        if (!tr) continue;
        bool isSilenced = tr->isMuted() || (hasSolo && !tr->isSolo());

        NodeId stripId = getTrackGainNodeId(i);
        if (stripId != INVALID_NODE_ID) {
            postNodeParameter(stripId, 2, isSilenced ? 1.0f : 0.0f);
        }
        if (isSilenced && tr->getTargetNodeId() != 0) {
            AudioEvent offEvent{};
            offEvent.type = AudioEventType::AllNotesOff;
            graph_.sendNodeEvent(tr->getTargetNodeId(), offEvent);
        }
    }
}

void AudioEngine::setTrackMute(uint32_t trackIndex, bool mute) noexcept {
    if (trackIndex < sequencer_.getNumTracks()) {
        auto* tr = sequencer_.getTrack(trackIndex);
        if (tr) tr->setMuted(mute);
    }
    updateMuteSoloRouting();
}

void AudioEngine::setTrackSolo(uint32_t trackIndex, bool solo) noexcept {
    if (trackIndex < sequencer_.getNumTracks()) {
        auto* tr = sequencer_.getTrack(trackIndex);
        if (tr) tr->setSolo(solo);
    }
    updateMuteSoloRouting();
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
        if (engineMode_ == SynthEngineMode::ModularGraph) {
            if (evt.channel != 0) {
                graph_.sendNodeEvent(evt.channel, evt);
            } else if (evt.type != AudioEventType::SetParameter) {
                graph_.broadcastEvent(evt);
            }
            if (evt.type == AudioEventType::SetParameter && evt.paramId == 2 && evt.channel == 0) {
                masterMixer_.setVolume(evt.paramValue);
            }
            continue;
        }

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

    if (engineMode_ == SynthEngineMode::ModularGraph) {
        uint64_t startSample = sequencer_.getTransport().getTotalSamplesElapsed();
        if (sequencer_.isPlaying()) {
            sequencer_.processBlock(frameCount, graph_);
        }
        graph_.process(outL, outR, frameCount);

        if (sequencer_.isPlaying()) {
            sequencer_.mixFrozenTracks(outL, outR, frameCount, startSample);
        }
    } else if (engineMode_ == SynthEngineMode::Tb303Acid) {
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

    // Push oscilloscope samples (mono sum)
    for (uint32_t i = 0; i < frameCount; ++i) {
        scopeRingBuffer_.push(0.5f * (outL[i] + outR[i]));
    }
}

void AudioEngine::audioCallbackInternal(float* pOutput, uint32_t frameCount) noexcept {
    if (!pOutput || frameCount == 0) return;

    // Track real-time sub-bass energy (<90Hz) with 2-pole lowpass & peak follower
    const float dt = 1.0f / (config_.sampleRate > 0 ? static_cast<float>(config_.sampleRate) : 48000.0f);
    const float cutoffHz = 85.0f;
    const float alpha = std::clamp(2.0f * 3.14159265f * cutoffHz * dt, 0.001f, 0.5f);
    const float decay = std::exp(-dt / 0.075f); // 75ms smooth release

    float maxBlockSub = subEnv_;
    uint32_t framesRendered = 0;

    // Process arbitrary frameCount in chunks up to MAX_BLOCK_SIZE to prevent buffer truncation/silence
    while (framesRendered < frameCount) {
        const uint32_t chunkFrames = std::min(frameCount - framesRendered, static_cast<uint32_t>(MAX_BLOCK_SIZE));

        renderOfflineBlock(scratchL_, scratchR_, chunkFrames);

        for (uint32_t i = 0; i < chunkFrames; ++i) {
            float mono = 0.5f * (scratchL_[i] + scratchR_[i]);
            subLp1_ += alpha * (mono - subLp1_);
            subLp2_ += alpha * (subLp1_ - subLp2_);
            float mag = std::abs(subLp2_);
            if (mag > maxBlockSub) {
                maxBlockSub = mag;
            } else {
                maxBlockSub *= decay;
            }
        }

        // Interleave planar scratch buffers into target stereo output offset
        float* chunkOutput = pOutput + (2 * framesRendered);
        for (uint32_t i = 0; i < chunkFrames; ++i) {
            chunkOutput[2 * i + 0] = scratchL_[i];
            chunkOutput[2 * i + 1] = scratchR_[i];
        }

        framesRendered += chunkFrames;
    }

    subEnv_ = maxBlockSub;
    subBassEnergy_.store(subEnv_, std::memory_order_relaxed);
}

bool AudioEngine::saveProject(const std::string& path, const std::string& title) {
    return project::ProjectFile::saveToFile(path, graph_, sequencer_, title, sequencer_.getBpm(), sequencer_.getSwing());
}

bool AudioEngine::loadProject(const std::string& path) {
    std::string title;
    double bpm = 135.0;
    double swing = 0.50;
    bool success = project::ProjectFile::loadFromFile(path, graph_, sequencer_, title, bpm, swing);
    if (success) {
        engineMode_ = SynthEngineMode::ModularGraph;
    }
    return success;
}

exporting::BounceStats AudioEngine::bounceProject(const std::string& wavPath,
                                                 double durationSeconds,
                                                 exporting::WavFormat format,
                                                 bool enableDither) {
    exporting::BounceConfig cfg{};
    cfg.sampleRate = config_.sampleRate > 0 ? config_.sampleRate : 48000;
    cfg.numChannels = 2;
    cfg.durationSeconds = durationSeconds;
    cfg.format = format;
    cfg.enableDither = enableDither;
    return exporting::WavExporter::bounceMaster(graph_, sequencer_, cfg, wavPath);
}

std::vector<exporting::BounceStats> AudioEngine::bounceStems(const std::string& outputDir,
                                                           double durationSeconds,
                                                           exporting::WavFormat format,
                                                           bool enableDither) {
    exporting::BounceConfig cfg{};
    cfg.sampleRate = config_.sampleRate > 0 ? config_.sampleRate : 48000;
    cfg.numChannels = 2;
    cfg.durationSeconds = durationSeconds;
    cfg.format = format;
    cfg.enableDither = enableDither;
    return exporting::WavExporter::bounceStems(graph_, sequencer_, cfg, outputDir);
}

bool AudioEngine::freezeTrack(uint32_t trackIndex, const trackfreeze::FreezeOptions& options) {
    return trackfreeze::TrackFreezeEngine::freezeTrack(*this, trackIndex, options);
}

bool AudioEngine::unfreezeTrack(uint32_t trackIndex, bool clearBuffers) {
    return trackfreeze::TrackFreezeEngine::unfreezeTrack(*this, trackIndex, clearBuffers);
}

bool AudioEngine::toggleFreezeTrack(uint32_t trackIndex, const trackfreeze::FreezeOptions& options) {
    return trackfreeze::TrackFreezeEngine::toggleFreezeTrack(*this, trackIndex, options);
}

bool AudioEngine::isTrackFrozen(uint32_t trackIndex) const noexcept {
    if (trackIndex >= sequencer_.getNumTracks()) return false;
    const auto* track = sequencer_.getTrack(trackIndex);
    return track ? track->isFrozen() : false;
}

} // namespace eatsbits::audio
