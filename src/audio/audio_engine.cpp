#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "eatsbits/audio/audio_engine.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <iostream>
#include <fstream>
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
#include "eatsbits/audio/graph/nodes/eatscript_node.hpp"
#include "eatsbits/theory/chord_model.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/dx7_node.hpp"
#include "eatsbits/audio/graph/nodes/physical_instrument_node.hpp"
#include "eatsbits/audio/graph/nodes/sid_node.hpp"
#include "eatsbits/audio/graph/nodes/snes_node.hpp"
#include "eatsbits/audio/graph/nodes/ym2612_node.hpp"
#include "eatsbits/audio/clap/clap_plugin_loader.hpp"
#include "eatsbits/audio/graph/nodes/clap_audio_node.hpp"
#include "eatsbits/audio/graph/nodes/reverb_node.hpp"
#include "eatsbits/audio/graph/nodes/moog_filter_node.hpp"
#include "eatsbits/audio/graph/nodes/daisy/daisy_nodes.hpp"
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
#if defined(MA_USE_AUDIO_WORKLETS)
    std::cout << "[Eatsbits] AudioWorklet dedicated Wasm worker thread active." << std::endl;
#elif defined(__EMSCRIPTEN__)
    std::cout << "[Eatsbits] WebAudio real-time DSP pipeline active." << std::endl;
#endif

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

    // Track 4: DX7 Rhodes (6-Operator FM Synthesizer)
    auto dx7 = std::make_shared<Dx7Node>("Dx7Rhodes");
    auto dx7Strip = std::make_shared<GainNode>("Track4_Gain");
    dx7Strip->setVolume(0.75f);

    // Track 5: Concert Grand (Physical Modeling Waveguide Piano)
    auto piano = std::make_shared<PhysicalInstrumentNode>(PhysicalModelType::ConcertGrandPiano, "ConcertGrand");
    auto pianoStrip = std::make_shared<GainNode>("Track5_Gain");
    pianoStrip->setVolume(0.70f);

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

void AudioEngine::setupDefaultRhodesGrandGraph() {
    graph_.clear();
    invalidateTrackStripCache();

    // Track 1 (Index 0): DX7 Rhodes (6-Operator FM Synthesizer)
    auto dx7 = std::make_shared<Dx7Node>("Dx7Rhodes");
    auto dx7Strip = std::make_shared<GainNode>("Track1_Gain");
    dx7Strip->setVolume(0.85f);

    // Track 2 (Index 1): Concert Grand (Physical Modeling Waveguide Piano)
    auto piano = std::make_shared<PhysicalInstrumentNode>(PhysicalModelType::ConcertGrandPiano, "ConcertGrand");
    auto pianoStrip = std::make_shared<GainNode>("Track2_Gain");
    pianoStrip->setVolume(0.85f);

    // Master Output
    auto masterGain = std::make_shared<GainNode>("MasterOut");
    masterGain->setVolume(0.85f);

    NodeId dx7Id = graph_.addNode(dx7);
    NodeId dx7StripId = graph_.addNode(dx7Strip);

    NodeId pianoId = graph_.addNode(piano);
    NodeId pianoStripId = graph_.addNode(pianoStrip);

    NodeId masterId = graph_.addNode(masterGain);

    // Route Track 1 (Dx7): Dx7Rhodes -> Track1_Gain -> MasterOut
    graph_.connect(dx7Id, 0, dx7StripId, 0);
    graph_.connect(dx7StripId, 0, masterId, 0);

    // Route Track 2 (Piano): ConcertGrand -> Track2_Gain -> MasterOut
    graph_.connect(pianoId, 0, pianoStripId, 0);
    graph_.connect(pianoStripId, 0, masterId, 0);

    // Register Source Nodes & Strip Caches
    trackSourceNodeCache_[0] = dx7Id;
    trackFxNodeIds_[0] = {};
    trackFxConfigs_[0] = {};

    trackSourceNodeCache_[1] = pianoId;
    trackFxNodeIds_[1] = {};
    trackFxConfigs_[1] = {};

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

bool AudioEngine::postNoteOn(uint8_t note, float velocity, bool isSlide, bool isAccent, int trackIndex, bool applyMidiFx) noexcept {
    uint32_t tIdx = (trackIndex >= 0) ? static_cast<uint32_t>(trackIndex) : activeTrackIndex_.load(std::memory_order_relaxed);
    if (applyMidiFx && tIdx < sequencer_.getNumTracks()) {
        const auto* trk = sequencer_.getTrack(tIdx);
        if (trk) {
            int noteNum = note;
            if (trk->getChordFollowMode() != theory::ChordFollowMode::Off) {
                float currentBar = static_cast<float>(sequencer_.getTransport().getCurrentStep()) / 16.0f;
                const auto* activeChord = sequencer_.getActiveChordAtBar(currentBar);
                if (!activeChord && !sequencer_.getChordTrack().empty()) {
                    activeChord = &sequencer_.getChordTrack().front();
                }
                if (activeChord) {
                    noteNum = theory::ChordTheory::remapPitchForChord(noteNum, *activeChord, trk->getChordFollowMode());
                }
            }
            for (const auto& fx : trk->getMidiFxRack()) {
                if (!fx.enabled) continue;
                if (fx.type == eatscript::MidiFxType::ScaleSnap) {
                    int root = fx.params.count("rootKey") ? static_cast<int>(fx.params.at("rootKey")) : 0;
                    bool minor = fx.params.count("scaleMode") ? (fx.params.at("scaleMode") > 0.5f) : false;
                    noteNum = eatscript::MidiPipelineEngine::snapToScale(noteNum, root, minor);
                } else if (fx.type == eatscript::MidiFxType::Transpose) {
                    int semi = fx.params.count("semitones") ? static_cast<int>(fx.params.at("semitones")) : (fx.params.count("transpose") ? static_cast<int>(fx.params.at("transpose")) : 0);
                    noteNum = std::clamp(noteNum + semi, 0, 127);
                }
            }
            note = static_cast<uint8_t>(std::clamp(noteNum, 0, 127));
        }
    }

    AudioEvent evt{};
    evt.type = AudioEventType::NoteOn;
    evt.note = note;
    evt.velocity = velocity;
    evt.paramId = isSlide ? 1 : 0;
    evt.paramValue = isAccent ? 1.0f : 0.0f;

    NodeId targetNodeId = 0;
    if (tIdx < sequencer_.getNumTracks()) {
        const auto* trk = sequencer_.getTrack(tIdx);
        if (trk) {
            targetNodeId = trk->getTargetNodeId();
        }
    }
    if (targetNodeId == 0 || targetNodeId == INVALID_NODE_ID) {
        targetNodeId = getTrackSourceNodeId(tIdx);
    }
    if (targetNodeId == 0 || targetNodeId == INVALID_NODE_ID) {
        // Track has no assigned instrument node: do NOT broadcast to every node in the graph!
        return false;
    }
    evt.targetNodeId = targetNodeId;
    evt.channel = 0;
    return eventQueue_.push(evt);
}

bool AudioEngine::postNoteOff(uint8_t note, int trackIndex, bool applyMidiFx) noexcept {
    uint32_t tIdx = (trackIndex >= 0) ? static_cast<uint32_t>(trackIndex) : activeTrackIndex_.load(std::memory_order_relaxed);
    if (applyMidiFx && tIdx < sequencer_.getNumTracks()) {
        const auto* trk = sequencer_.getTrack(tIdx);
        if (trk) {
            int noteNum = note;
            if (trk->getChordFollowMode() != theory::ChordFollowMode::Off) {
                float currentBar = static_cast<float>(sequencer_.getTransport().getCurrentStep()) / 16.0f;
                const auto* activeChord = sequencer_.getActiveChordAtBar(currentBar);
                if (!activeChord && !sequencer_.getChordTrack().empty()) {
                    activeChord = &sequencer_.getChordTrack().front();
                }
                if (activeChord) {
                    noteNum = theory::ChordTheory::remapPitchForChord(noteNum, *activeChord, trk->getChordFollowMode());
                }
            }
            for (const auto& fx : trk->getMidiFxRack()) {
                if (!fx.enabled) continue;
                if (fx.type == eatscript::MidiFxType::ScaleSnap) {
                    int root = fx.params.count("rootKey") ? static_cast<int>(fx.params.at("rootKey")) : 0;
                    bool minor = fx.params.count("scaleMode") ? (fx.params.at("scaleMode") > 0.5f) : false;
                    noteNum = eatscript::MidiPipelineEngine::snapToScale(noteNum, root, minor);
                } else if (fx.type == eatscript::MidiFxType::Transpose) {
                    int semi = fx.params.count("semitones") ? static_cast<int>(fx.params.at("semitones")) : (fx.params.count("transpose") ? static_cast<int>(fx.params.at("transpose")) : 0);
                    noteNum = std::clamp(noteNum + semi, 0, 127);
                }
            }
            note = static_cast<uint8_t>(std::clamp(noteNum, 0, 127));
        }
    }

    AudioEvent evt{};
    evt.type = AudioEventType::NoteOff;
    evt.note = note;
    evt.velocity = 0.0f;

    NodeId targetNodeId = 0;
    if (tIdx < sequencer_.getNumTracks()) {
        const auto* trk = sequencer_.getTrack(tIdx);
        if (trk) {
            targetNodeId = trk->getTargetNodeId();
        }
    }
    if (targetNodeId == 0 || targetNodeId == INVALID_NODE_ID) {
        targetNodeId = getTrackSourceNodeId(tIdx);
    }
    if (targetNodeId == 0 || targetNodeId == INVALID_NODE_ID) {
        return false;
    }
    evt.targetNodeId = targetNodeId;
    evt.channel = 0;
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
    evt.targetNodeId = nodeId;
    evt.channel = static_cast<uint8_t>(nodeId > 255 ? 0 : nodeId);
    evt.paramId = paramId;
    evt.paramValue = value;
    return eventQueue_.push(evt);
}

bool AudioEngine::postAllNotesOff() noexcept {
    AudioEvent evt{};
    evt.type = AudioEventType::AllNotesOff;
    return eventQueue_.push(evt);
}

void AudioEngine::stopPlayback() noexcept {
    // 1. Halt sequencer and flush all active voices with NoteOff to target nodes in graph
    sequencer_.stop(&graph_);

    // 2. Kill all active notes on standalone synths
    polySynth_.allNotesOff();
    tb303_.noteOff();

    // 3. Post AllNotesOff to lock-free audio event queue
    postAllNotesOff();
}

void AudioEngine::panic() noexcept {
    // 1. Halt sequencer playhead and flush active voice registry
    sequencer_.stop(&graph_);

    // 2. Kill all active notes on standalone synths
    polySynth_.allNotesOff();
    tb303_.reset();

    // 3. Reset modular audio graph (clears delay lines, IIR filter states, triggers CLAP panic choke & reset)
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

NodeId AudioEngine::assignTrackInstrument(uint32_t trackIndex, const std::string& instrumentIdOrName) {
    if (trackIndex >= sequencer_.getNumTracks()) {
        sequencer_.addTrack("Track " + std::to_string(trackIndex + 1), 0, 16);
    }
    auto* trk = sequencer_.getTrack(trackIndex);

    // 1. Ensure track gain strip exists and connects to MasterOut
    NodeId gainId = getTrackGainNodeId(trackIndex);
    if (gainId == INVALID_NODE_ID) {
        auto gainNode = std::make_shared<GainNode>("Track" + std::to_string(trackIndex + 1) + "_Gain");
        gainNode->setVolume(0.80f);
        gainId = graph_.addNode(gainNode);
        trackGainNodeCache_[trackIndex] = gainId;

        NodeId masterId = graph_.findNodeByName("MasterOut");
        if (masterId != INVALID_NODE_ID) {
            graph_.connect(gainId, 0, masterId, 0);
        } else if (graph_.getOutputNodeId() != INVALID_NODE_ID) {
            graph_.connect(gainId, 0, graph_.getOutputNodeId(), 0);
        }
    }

    // 2. Disconnect and remove any old source node for this track
    NodeId oldSrcId = getTrackSourceNodeId(trackIndex);
    if (oldSrcId != INVALID_NODE_ID) {
        auto fxIt = trackFxNodeIds_.find(trackIndex);
        if (fxIt != trackFxNodeIds_.end() && !fxIt->second.empty()) {
            graph_.disconnect(oldSrcId, 0, fxIt->second.front(), 0);
        } else {
            graph_.disconnect(oldSrcId, 0, gainId, 0);
        }
        graph_.removeNode(oldSrcId);
        trackSourceNodeCache_.erase(trackIndex);
    }

    // 3. Instantiate appropriate instrument node based on ID or name
    std::string key = instrumentIdOrName;
    for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::shared_ptr<GraphNode> newInstrument;
    std::string nodeName = "Trk" + std::to_string(trackIndex + 1) + "_" + (instrumentIdOrName.empty() ? "Synth" : instrumentIdOrName);

    if (instrumentIdOrName.rfind("clap:", 0) == 0 || key.find(".clap") != std::string::npos) {
        std::string clapId = (instrumentIdOrName.rfind("clap:", 0) == 0)
            ? instrumentIdOrName.substr(5)
            : instrumentIdOrName;
        const auto* desc = ClapPluginLoader::instance().findPluginById(clapId);
        if (!desc) desc = ClapPluginLoader::instance().findPluginByName(clapId);

        if (desc) {
            newInstrument = ClapAudioNode::create(desc->libraryPath, desc->id, nodeName);
        } else if (key.find(".clap") != std::string::npos) {
            newInstrument = ClapAudioNode::create(instrumentIdOrName, clapId, nodeName);
        }
    }

    if (!newInstrument) {
        if (key.find("909") != std::string::npos) {
            newInstrument = std::make_shared<DrumKitNode>(nodeName, DrumKitType::TR909);
        } else if (key.find("808") != std::string::npos) {
            newInstrument = std::make_shared<DrumKitNode>(nodeName, DrumKitType::TR808);
        } else if (key.find("drum") != std::string::npos || key.find("percussion") != std::string::npos ||
                   key.find("mpc") != std::string::npos || key.find("drumpad") != std::string::npos ||
                   key.find("drum_pad") != std::string::npos) {
            newInstrument = std::make_shared<DrumKitNode>(nodeName, DrumKitType::Combined);
        } else if (key.find("pad") != std::string::npos || key.find("ambient") != std::string::npos ||
                   key.find("string") != std::string::npos || key.find("warm") != std::string::npos) {
            auto poly = std::make_shared<PolySynthNode>(nodeName);
            poly->setWaveform(::eatsbits::dsp::Waveform::Saw);
            poly->setFilterCutoff(2200.0f);
            poly->setFilterResonance(0.6f);
            poly->setAdsr(0.20f, 0.8f, 0.7f, 0.8f);
            newInstrument = poly;
        } else if (key.find("303") != std::string::npos || key.find("acid") != std::string::npos) {
            newInstrument = std::make_shared<Tb303Node>(nodeName);
        } else if (key.find("dx7") != std::string::npos || key.find("rhodes") != std::string::npos ||
                   key.find("epiano") != std::string::npos || key.find("fm") != std::string::npos) {
            newInstrument = std::make_shared<Dx7Node>(nodeName);
        } else if (key.find("piano") != std::string::npos || key.find("grand") != std::string::npos ||
                   key.find("harpsichord") != std::string::npos || key.find("clavinet") != std::string::npos) {
            newInstrument = std::make_shared<PhysicalInstrumentNode>(PhysicalModelType::ConcertGrandPiano, nodeName);
        } else if (key.find("upright_bass") != std::string::npos || key.find("double_bass") != std::string::npos ||
                   (key.find("bass") != std::string::npos && key.find("synth") == std::string::npos && key.find("sub") == std::string::npos)) {
            newInstrument = std::make_shared<PhysicalInstrumentNode>(PhysicalModelType::UprightBass, nodeName);
        } else if (key.find("guitar") != std::string::npos || key.find("lute") != std::string::npos ||
                   key.find("banjo") != std::string::npos || key.find("ukulele") != std::string::npos ||
                   key.find("mandolin") != std::string::npos) {
            newInstrument = std::make_shared<PhysicalInstrumentNode>(PhysicalModelType::SpanishGuitar, nodeName);
        } else if (key.find("sid") != std::string::npos || key.find("c64") != std::string::npos ||
                   key.find("chiptune") != std::string::npos) {
            newInstrument = std::make_shared<SidNode>(nodeName);
        } else if (key.find("snes") != std::string::npos || key.find("spc") != std::string::npos) {
            newInstrument = std::make_shared<SnesNode>(nodeName);
        } else if (key.find("ym2612") != std::string::npos || key.find("genesis") != std::string::npos) {
            newInstrument = std::make_shared<Ym2612Node>(nodeName);
        } else {
            auto poly = std::make_shared<PolySynthNode>(nodeName);
            poly->setWaveform(::eatsbits::dsp::Waveform::Saw);
            poly->setFilterCutoff(2800.0f);
            poly->setFilterResonance(0.8f);
            poly->setAdsr(0.015f, 0.6f, 0.5f, 0.4f);
            newInstrument = poly;
        }
    }

    newInstrument->prepare(static_cast<double>(config_.sampleRate), config_.bufferFrameSize);
    NodeId newSrcId = graph_.addNode(newInstrument);
    trackSourceNodeCache_[trackIndex] = newSrcId;
    if (trk) {
        trk->setTargetNodeId(newSrcId);
    }

    // 4. Connect to FX chain or gain strip
    auto fxIt = trackFxNodeIds_.find(trackIndex);
    if (fxIt != trackFxNodeIds_.end() && !fxIt->second.empty()) {
        graph_.connect(newSrcId, 0, fxIt->second.front(), 0);
    } else {
        graph_.connect(newSrcId, 0, gainId, 0);
    }

    graph_.compile();
    return newSrcId;
}

uint32_t AudioEngine::addTrack(const std::string& name, NodeId targetNodeId, uint32_t numSteps, const std::string& instrumentIdOrName) {
    size_t idx = sequencer_.addTrack(name, targetNodeId, numSteps);
    uint32_t trackIndex = static_cast<uint32_t>(idx);
    if (targetNodeId == 0) {
        std::string inst = instrumentIdOrName.empty() ? name : instrumentIdOrName;
        assignTrackInstrument(trackIndex, inst);
    } else {
        trackSourceNodeCache_[trackIndex] = targetNodeId;
    }
    updateMuteSoloRouting();
    return trackIndex;
}

void AudioEngine::removeTrack(uint32_t trackIndex) {
    sequencer_.removeTrack(trackIndex);
    invalidateTrackStripCache();
    updateMuteSoloRouting();
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

std::shared_ptr<ClapAudioNode> AudioEngine::getTrackClapNode(uint32_t trackIndex) const {
    NodeId srcId = getTrackSourceNodeId(trackIndex);
    if (srcId == INVALID_NODE_ID) return nullptr;
    auto node = graph_.getNode(srcId);
    return std::dynamic_pointer_cast<ClapAudioNode>(node);
}

std::shared_ptr<ClapAudioNode> AudioEngine::getTrackAudioFxClapNode(uint32_t trackIndex, size_t fxIndex) const {
    auto fxIt = trackFxNodeIds_.find(trackIndex);
    if (fxIt == trackFxNodeIds_.end() || fxIndex >= fxIt->second.size()) return nullptr;
    NodeId fxId = fxIt->second[fxIndex];
    auto node = graph_.getNode(fxId);
    return std::dynamic_pointer_cast<ClapAudioNode>(node);
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

    // 2. Safely disconnect any intermediate nodes between srcId and gainId
    // If track 0, remove default "AcidEcho" node if it exists in the graph
    if (trackIndex == 0) {
        NodeId echoId = graph_.findNodeByName("AcidEcho");
        if (echoId != INVALID_NODE_ID) {
            graph_.removeNode(echoId);
        }
    }

    // Safely disconnect any incoming connection to gainId port 0 (preventing parallel summing/distortion)
    std::vector<GraphConnection> toDisconnect;
    for (const auto& conn : graph_.getConnections()) {
        if (conn.dstNode == gainId && conn.dstPort == 0) {
            toDisconnect.push_back(conn);
        }
    }
    for (const auto& conn : toDisconnect) {
        graph_.disconnect(conn.srcNode, conn.srcPort, gainId, 0);
    }

    // Safely disconnect any outgoing connection from srcId port 0
    toDisconnect.clear();
    for (const auto& conn : graph_.getConnections()) {
        if (conn.srcNode == srcId && conn.srcPort == 0) {
            toDisconnect.push_back(conn);
        }
    }
    for (const auto& conn : toDisconnect) {
        graph_.disconnect(srcId, 0, conn.dstNode, conn.dstPort);
    }

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
        if (fx.type == "CLAP" || fx.id.rfind("clap:", 0) == 0 || tUpper.find(".CLAP") != std::string::npos) {
            std::string clapId = (fx.id.rfind("clap:", 0) == 0) ? fx.id.substr(5) : fx.id;
            const auto* desc = ClapPluginLoader::instance().findPluginById(clapId);
            if (!desc) desc = ClapPluginLoader::instance().findPluginByName(clapId);

            if (desc) {
                node = ClapAudioNode::create(desc->libraryPath, desc->id, fx.name.empty() ? desc->name : fx.name);
            } else if (fx.id.find(".clap") != std::string::npos) {
                node = ClapAudioNode::create(fx.id, clapId, fx.name.empty() ? "ClapFX" : fx.name);
            }
        }

        if (!node) {
            if (tUpper.find("CONVOLVER") != std::string::npos || tUpper.find("ZERO-LATENCY IR") != std::string::npos ||
                tUpper.find("ROOM") != std::string::npos || tUpper.find("HALL") != std::string::npos ||
                tUpper.find("SPACE") != std::string::npos) {
                auto conv = std::make_shared<ConvolverNode>(fx.name.empty() ? "ConvolverReverb" : fx.name);
                conv->setMix(std::clamp(fx.mix, 0.0f, 1.0f));
                conv->setDecay(0.4f + fx.drive * 3.6f);
                conv->setRoomSize(0.5f + fx.drive * 0.9f);
                node = conv;
            } else if (tUpper.find("REVERB") != std::string::npos || tUpper.find("REVERBSC") != std::string::npos ||
                       tUpper.find("REVERB_SC") != std::string::npos) {
                auto dr = std::make_shared<ReverbNode>(fx.name.empty() ? "Reverb" : fx.name);
                dr->setFeedback(std::clamp(fx.drive > 1.0f ? fx.drive / 100.0f : (fx.drive > 0.0f ? fx.drive : 0.85f), 0.1f, 0.99f));
                dr->setDryWet(std::clamp(fx.mix, 0.0f, 1.0f));
                node = dr;
            } else if (tUpper.find("DELAY") != std::string::npos || tUpper.find("ECHO") != std::string::npos ||
                       tUpper.find("PING-PONG") != std::string::npos || tUpper == "ACIDECHO") {
                auto dd = std::make_shared<DelayNode>(fx.name.empty() ? "Delay" : fx.name);
                dd->setDelayTime(std::max(0.01f, (fx.drive > 2.0f) ? fx.drive / 1000.0f : fx.drive * 0.8f));
                dd->setFeedback(std::clamp(fx.drive * 0.6f, 0.0f, 0.95f));
                dd->setDryWet(std::clamp(fx.mix, 0.0f, 1.0f));
                node = dd;
            } else if (tUpper.find("MOOG") != std::string::npos || tUpper.find("LADDER") != std::string::npos ||
                       tUpper.find("MOOG_FILTER") != std::string::npos || tUpper.find("MOOG_LADDER") != std::string::npos ||
                       tUpper.find("DAISY_FILTER") != std::string::npos || tUpper.find("DAISY FILTER") != std::string::npos) {
                auto df = std::make_shared<MoogFilterNode>(fx.name.empty() ? "MoogFilter" : fx.name);
                if (tUpper.find("HIGH") != std::string::npos || tUpper.find("HP") != std::string::npos) {
                    df->setMode(MoogFilterNode::FilterMode::SvfHighpass);
                } else if (tUpper.find("BAND") != std::string::npos || tUpper.find("BP") != std::string::npos) {
                    df->setMode(MoogFilterNode::FilterMode::SvfBandpass);
                } else if (tUpper.find("NOTCH") != std::string::npos) {
                    df->setMode(MoogFilterNode::FilterMode::SvfNotch);
                } else if (tUpper.find("SVF") != std::string::npos) {
                    df->setMode(MoogFilterNode::FilterMode::SvfLowpass);
                } else {
                    df->setMode(MoogFilterNode::FilterMode::MoogLadder);
                }
                float cutoff = (fx.drive > 20.0f) ? fx.drive : (20.0f * std::pow(1000.0f, std::clamp(fx.drive, 0.0f, 1.0f)));
                df->setCutoff(cutoff);
                df->setResonance(std::clamp(fx.mix, 0.0f, 0.98f));
                node = df;
            } else if (tUpper.find("CHORUS") != std::string::npos || tUpper.find("FLANGER") != std::string::npos ||
                       tUpper.find("MODULATION") != std::string::npos || tUpper.find("BUCKET") != std::string::npos ||
                       tUpper.find("BBD") != std::string::npos || tUpper.find("DAISY_MOD") != std::string::npos ||
                       tUpper.find("DAISY MODULATION") != std::string::npos || tUpper.find("DAISY_CHORUS") != std::string::npos ||
                       tUpper.find("DAISY_FLANGER") != std::string::npos) {
                auto dm = std::make_shared<ChorusNode>(fx.name.empty() ? "Chorus" : fx.name);
                if (tUpper.find("FLANGER") != std::string::npos) {
                    dm->setType(ChorusNode::ModType::Flanger);
                } else if (tUpper.find("OVERDRIVE") != std::string::npos) {
                    dm->setType(ChorusNode::ModType::Overdrive);
                } else {
                    dm->setType(ChorusNode::ModType::Chorus);
                }
                dm->setRate(std::max(0.1f, fx.drive * 4.0f));
                dm->setDepth(std::clamp(fx.drive, 0.1f, 1.0f));
                dm->setDrive(std::clamp(fx.drive, 0.0f, 1.0f));
                dm->setDryWet(std::clamp(fx.mix, 0.0f, 1.0f));
                node = dm;
            } else if (tUpper.find("COMP") != std::string::npos || tUpper.find("DYNAMICS") != std::string::npos) {
                auto dc = std::make_shared<CompressorNode>(fx.name.empty() ? "Compressor" : fx.name);
                dc->setThreshold(-30.0f + (1.0f - fx.drive) * 24.0f);
                dc->setRatio(1.5f + fx.drive * 6.0f);
                dc->setDryWet(std::clamp(fx.mix, 0.0f, 1.0f));
                node = dc;
            } else if (tUpper.find("LIMIT") != std::string::npos) {
                auto lim = std::make_shared<LimiterNode>(fx.name.empty() ? "BrickwallLimiter" : fx.name);
                lim->setCeilingDbfs(-0.1f);
                node = lim;
            } else if (tUpper.find("EQ") != std::string::npos || tUpper.find("PARAMETRIC") != std::string::npos) {
                auto eq = std::make_shared<ParametricEqNode>(fx.name.empty() ? "ParametricEQ" : fx.name);
                node = eq;
            } else if (tUpper.find("FILTER") != std::string::npos || tUpper.find("SVF") != std::string::npos ||
                       tUpper.find("VCF") != std::string::npos || tUpper.find("LOWPASS") != std::string::npos ||
                       tUpper.find("HIGHPASS") != std::string::npos || tUpper.find("BANDPASS") != std::string::npos) {
                auto bq = std::make_shared<BiquadNode>(fx.name.empty() ? "BiquadFilter" : fx.name);
                dsp::BiquadType filterType = dsp::BiquadType::LowPass;
                if (tUpper.find("HIGH") != std::string::npos || tUpper.find("HP") != std::string::npos) {
                    filterType = dsp::BiquadType::HighPass;
                } else if (tUpper.find("BAND") != std::string::npos || tUpper.find("BP") != std::string::npos) {
                    filterType = dsp::BiquadType::BandPass;
                } else if (tUpper.find("NOTCH") != std::string::npos) {
                    filterType = dsp::BiquadType::Notch;
                }
                bq->setFilterType(filterType);
                float cutoff = (fx.drive > 20.0f) ? fx.drive : (20.0f * std::pow(1000.0f, std::clamp(fx.drive, 0.0f, 1.0f)));
                bq->setCutoff(cutoff);
                bq->setResonance(std::max(0.1f, fx.mix * 10.0f + 0.5f));
                node = bq;
            } else if (tUpper.find(".EATS") != std::string::npos || fx.id.find(".eats") != std::string::npos ||
                       tUpper.find("SCRIPT") != std::string::npos || tUpper.find("EATSCRIPT") != std::string::npos) {
                auto esNode = std::make_shared<EatscriptNode>(fx.name.empty() ? "EatscriptFX" : fx.name);
                std::string scriptPath = !fx.id.empty() ? fx.id : fx.name;
                std::ifstream ifs(scriptPath);
                if (ifs.is_open()) {
                    std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                    esNode->setScript(code);
                } else {
                    esNode->setScript("process(inL, inR) { return (inL, inR); }");
                }
                node = esNode;
            } else if (tUpper.find("CRUSH") != std::string::npos || tUpper.find("BIT") != std::string::npos) {
                auto ws = std::make_shared<WaveShaperNode>(fx.name.empty() ? "Bitcrusher" : fx.name);
                ws->setDrive(1.5f + fx.drive * 4.0f);
                ws->setMix(std::clamp(fx.mix, 0.0f, 1.0f));
                node = ws;
            } else if (tUpper.find("DISTORT") != std::string::npos || tUpper.find("OVERDRIVE") != std::string::npos ||
                       tUpper.find("SATURAT") != std::string::npos || tUpper.find("DRIVE") != std::string::npos ||
                       tUpper.find("FUZZ") != std::string::npos || tUpper.find("TUBE") != std::string::npos ||
                       tUpper.find("SHAPER") != std::string::npos) {
                auto ws = std::make_shared<WaveShaperNode>(fx.name.empty() ? "TubeDistortion" : fx.name);
                ws->setDrive(std::clamp(fx.drive * 2.0f, 0.5f, 4.0f));
                ws->setMix(std::clamp(fx.mix, 0.0f, 1.0f));
                node = ws;
            }
        }

        if (!node) continue;

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

    std::string p = paramName;
    for (char& c : p) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (auto delay = std::dynamic_pointer_cast<DelayNode>(node)) {
        if (p == "time" || p == "delay" || p == "rate" || p == "param1" || p == "drive") {
            delay->setDelayTime(value > 2.0f ? value / 1000.0f : std::clamp(value, 0.005f, 1.95f));
        } else if (p == "feedback" || p == "fb" || p == "param2") {
            delay->setFeedback(std::clamp(value, 0.0f, 0.98f));
        } else if (p == "damping" || p == "damp" || p == "tone" || p == "freq" || p == "param3") {
            delay->setDampingFreq(std::clamp(value > 1.0f ? value : (200.0f + value * 18000.0f), 200.0f, 20000.0f));
        } else if (p == "mix" || p == "wet" || p == "drywet" || p == "param4") {
            delay->setDryWet(std::clamp(value, 0.0f, 1.0f));
        }
    } else if (auto conv = std::dynamic_pointer_cast<ConvolverNode>(node)) {
        if (p == "mix" || p == "param4" || p == "wet") conv->setMix(std::clamp(value, 0.0f, 1.0f));
        else if (p == "decay" || p == "time" || p == "param1" || p == "drive") conv->setDecay(0.3f + value * 4.0f);
        else if (p == "predelay" || p == "param2") conv->setPreDelay(value * 200.0f);
        else if (p == "roomsize" || p == "size" || p == "param3") conv->setRoomSize(0.5f + value * 1.0f);
        else if (p == "damping" || p == "param5") conv->setDamping(value);
    } else if (auto chorus = std::dynamic_pointer_cast<ChorusNode>(node)) {
        if (p == "rate" || p == "speed" || p == "freq" || p == "param1" || p == "time" || p == "drive") {
            chorus->setRate(std::clamp(value > 1.0f ? value : (0.1f + value * 10.0f), 0.05f, 20.0f));
        } else if (p == "depth" || p == "param2") {
            chorus->setDepth(std::clamp(value, 0.0f, 1.0f));
        } else if (p == "feedback" || p == "fb" || p == "param3") {
            chorus->setFeedback(std::clamp(value, 0.0f, 0.95f));
        } else if (p == "drive") {
            chorus->setDrive(std::clamp(value, 0.0f, 1.0f));
        } else if (p == "mix" || p == "wet" || p == "drywet" || p == "param4") {
            chorus->setDryWet(std::clamp(value, 0.0f, 1.0f));
        } else if (p == "type" || p == "mode" || p == "param5") {
            chorus->setType(static_cast<ChorusNode::ModType>(static_cast<int>(value)));
        }
    } else if (auto comp = std::dynamic_pointer_cast<CompressorNode>(node)) {
        if (p == "threshold" || p == "thresh" || p == "param1") {
            comp->setThreshold(value <= 0.0f && value >= -80.0f ? value : (-60.0f + value * 60.0f));
        } else if (p == "ratio" || p == "param2" || p == "drive") {
            comp->setRatio(value >= 1.0f ? value : (1.0f + value * 19.0f));
        } else if (p == "attack" || p == "att" || p == "param3") {
            comp->setAttack(value > 0.0001f && value <= 1.0f ? value : (0.001f + value * 0.2f));
        } else if (p == "release" || p == "rel" || p == "param4") {
            comp->setRelease(value > 0.001f && value <= 3.0f ? value : (0.01f + value * 1.5f));
        } else if (p == "gain" || p == "makeup" || p == "param5") {
            comp->setMakeupGain((value - 0.5f) * 24.0f);
        } else if (p == "mix" || p == "wet" || p == "drywet" || p == "param6") {
            comp->setDryWet(std::clamp(value, 0.0f, 1.0f));
        }
    } else if (auto ws = std::dynamic_pointer_cast<WaveShaperNode>(node)) {
        if (p == "drive" || p == "param1" || p == "param5" || p == "pre") ws->setDrive(value > 1.0f ? value : (1.0f + value * 8.0f));
        else if (p == "mix" || p == "param4" || p == "wet") ws->setMix(std::clamp(value, 0.0f, 1.0f));
        else if (p == "tone" || p == "param2" || p == "filter") ws->setTone(std::clamp(value, 0.0f, 1.0f));
        else if (p == "bias" || p == "param3" || p == "tension") ws->setBias(std::clamp(value, 0.0f, 1.0f));
        else if (p == "shape" || p == "mode") ws->setShape(static_cast<int>(value));
    } else if (auto eq = std::dynamic_pointer_cast<ParametricEqNode>(node)) {
        if (p == "low" || p == "lowgain" || p == "gain1" || p == "param1") eq->setBandGain(0, (std::abs(value) > 1.0f) ? value : ((value - 0.5f) * 36.0f));
        else if (p == "mid" || p == "mid1gain" || p == "gain2" || p == "param2") eq->setBandGain(1, (std::abs(value) > 1.0f) ? value : ((value - 0.5f) * 36.0f));
        else if (p == "high" || p == "highgain" || p == "gain3" || p == "param3") eq->setBandGain(3, (std::abs(value) > 1.0f) ? value : ((value - 0.5f) * 36.0f));
        else if (p == "q" || p == "mid1q" || p == "reso" || p == "param4") eq->setBandQ(1, std::max(0.1f, value * 9.9f + 0.1f));
        else if (p == "gain" || p == "mastergain" || p == "level" || p == "param5" || p == "mix") eq->setMasterGain((std::abs(value) > 1.0f) ? value : ((value - 0.5f) * 24.0f));
    } else if (auto lim = std::dynamic_pointer_cast<LimiterNode>(node)) {
        if (p == "ceiling" || p == "ceil" || p == "threshold" || p == "param1") lim->setCeilingDb((value <= 0.0f && value >= -24.0f) ? value : (-12.0f + value * 12.0f));
        else if (p == "release" || p == "rel" || p == "param2") lim->setRelease((value > 1.0f) ? value : (10.0f + value * 990.0f));
        else if (p == "gain" || p == "lookahead" || p == "param3") lim->setLookaheadMs((value > 1.0f) ? value : (1.0f + value * 9.0f));
    } else if (auto bq = std::dynamic_pointer_cast<BiquadNode>(node)) {
        if (p == "cutoff" || p == "freq" || p == "param1" || p == "drive") {
            float cutoff = (value > 20.0f) ? value : (20.0f * std::pow(1000.0f, std::clamp(value, 0.0f, 1.0f)));
            bq->setCutoff(cutoff);
        } else if (p == "resonance" || p == "q" || p == "param2" || p == "reso") {
            bq->setResonance(value > 10.0f ? value : std::max(0.1f, value * 10.0f + 0.5f));
        } else if (p == "type" || p == "mode" || p == "param3") {
            bq->setFilterType(static_cast<dsp::BiquadType>(static_cast<int>(value)));
        } else if (p == "gain" || p == "param4") {
            bq->setGainDb((std::abs(value) > 1.0f) ? value : ((value - 0.5f) * 24.0f));
        }
    } else if (auto es = std::dynamic_pointer_cast<EatscriptNode>(node)) {
        if (p.rfind("param", 0) == 0 && p.size() > 5) {
            try {
                uint32_t pid = static_cast<uint32_t>(std::stoul(p.substr(5))) - 1;
                es->setParameter(pid, value);
            } catch (...) {}
        } else if (p == "drive" || p == "gain") {
            es->setParameter(0, value);
        } else if (p == "mix" || p == "wet") {
            es->setParameter(1, value);
        }
    } else if (auto reverb = std::dynamic_pointer_cast<ReverbNode>(node)) {
        if (p == "decay" || p == "feedback" || p == "time" || p == "fb" || p == "param1" || p == "drive") {
            reverb->setFeedback(std::clamp(value > 1.0f ? value / 100.0f : value, 0.0f, 0.999f));
        } else if (p == "damping" || p == "damp" || p == "freq" || p == "lpfreq" || p == "param2") {
            reverb->setDampingFreq(std::clamp(value > 1.0f ? value : (100.0f + value * 15000.0f), 100.0f, 20000.0f));
        } else if (p == "mix" || p == "wet" || p == "drywet" || p == "param3" || p == "param4") {
            reverb->setDryWet(std::clamp(value, 0.0f, 1.0f));
        }
    } else if (auto moog = std::dynamic_pointer_cast<MoogFilterNode>(node)) {
        if (p == "cutoff" || p == "freq" || p == "param1" || p == "drive") {
            float cutoff = (value > 20.0f) ? value : (20.0f * std::pow(1000.0f, std::clamp(value, 0.0f, 1.0f)));
            moog->setCutoff(cutoff);
        } else if (p == "resonance" || p == "reso" || p == "res" || p == "q" || p == "param2") {
            moog->setResonance(std::clamp(value, 0.0f, 0.99f));
        } else if (p == "drive" || p == "param3") {
            moog->setDrive(std::clamp(value, 0.0f, 1.0f));
        } else if (p == "mode" || p == "type" || p == "param4") {
            moog->setMode(static_cast<MoogFilterNode::FilterMode>(static_cast<int>(value)));
        }
    }

    auto cfgIt = trackFxConfigs_.find(trackIndex);
    if (cfgIt != trackFxConfigs_.end() && fxIndex < cfgIt->second.size()) {
        if (p == "drive" || p == "param1" || p == "time" || p == "rate" || p == "decay" || p == "pre" || p == "cutoff" || p == "freq") {
            cfgIt->second[fxIndex].drive = value;
        } else if (p == "mix" || p == "param4" || p == "wet" || p == "resonance" || p == "reso" || p == "q") {
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
            if (evt.targetNodeId != 0 && evt.targetNodeId != INVALID_NODE_ID) {
                graph_.sendNodeEvent(evt.targetNodeId, evt);
            } else if (evt.channel != 0) {
                graph_.sendNodeEvent(evt.channel, evt);
            } else if (evt.type != AudioEventType::SetParameter &&
                       evt.type != AudioEventType::NoteOn &&
                       evt.type != AudioEventType::NoteOff) {
                graph_.broadcastEvent(evt);
            }
            if (evt.type == AudioEventType::SetParameter && evt.paramId == 2 && evt.channel == 0 && evt.targetNodeId == 0) {
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
