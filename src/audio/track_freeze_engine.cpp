#include "eatsbits/audio/track_freeze_engine.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/audio/export/wav_exporter.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace eatsbits::audio::trackfreeze {

std::string TrackFreezeEngine::computeTrackHash(
    const sequencer::SequencerTrack& track,
    double bpm,
    uint32_t sampleRate,
    const AudioGraph* graph
) {
    // 64-bit FNV-1a hash algorithm
    constexpr uint64_t fnvOffsetBasis = 0xcbf29ce484222325ULL;
    constexpr uint64_t fnvPrime = 0x100000001b3ULL;
    uint64_t hash = fnvOffsetBasis;

    auto hashString = [&](const std::string& str) {
        for (char c : str) {
            hash = (hash ^ static_cast<uint64_t>(static_cast<uint8_t>(c))) * fnvPrime;
        }
        hash = (hash ^ static_cast<uint64_t>('|')) * fnvPrime;
    };

    auto hashPod = [&](const auto& val) {
        const auto* p = reinterpret_cast<const uint8_t*>(&val);
        for (size_t i = 0; i < sizeof(val); ++i) {
            hash = (hash ^ static_cast<uint64_t>(p[i])) * fnvPrime;
        }
        hash = (hash ^ static_cast<uint64_t>(';')) * fnvPrime;
    };

    // 1. Basic track identity and mixer parameters
    hashString(track.getName());
    hashString(track.getIconRef());
    hashPod(track.getTargetNodeId());
    hashPod(track.getNumSteps());
    hashPod(track.getVolume());
    hashPod(track.getPan());
    hashPod(track.getTranspose());
    hashPod(bpm);
    hashPod(sampleRate);
    hashString(track.getEatscriptCode());

    // 2. Note step data
    const uint32_t numSteps = track.getNumSteps();
    for (uint32_t s = 0; s < numSteps; ++s) {
        const auto& step = track.getStep(s);
        if (step.active) {
            hashPod(s);
            hashPod(step.note);
            hashPod(step.velocity);
            hashPod(step.gateLength);
            hashPod(step.slide);
            hashPod(step.accent);
            hashPod(step.probability);
            hashPod(step.paramLockId);
            hashPod(step.paramLockValue);
            for (uint8_t ex : step.extraNotes) {
                hashPod(ex);
            }
        }
    }

    // 3. Target Node attributes from AudioGraph if available
    if (graph && track.getTargetNodeId() != 0) {
        auto node = graph->getNode(track.getTargetNodeId());
        if (node) {
            hashString(node->getName());
            hashPod(node->isEnabled());
            hashPod(node->numInputPorts());
            hashPod(node->numOutputPorts());
        }
    }

    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

bool TrackFreezeEngine::isFreezeValid(
    const sequencer::SequencerTrack& track,
    double bpm,
    uint32_t sampleRate,
    const AudioGraph* graph
) {
    if (!track.isFrozen() || track.getFrozenBufferL().empty()) {
        return false;
    }
    const std::string currentHash = computeTrackHash(track, bpm, sampleRate, graph);
    return currentHash == track.getFrozenContentHash();
}

FreezeResult TrackFreezeEngine::renderTrackOffline(
    AudioGraph& graph,
    sequencer::StepSequencer& seq,
    size_t trackIndex,
    const FreezeOptions& options
) {
    FreezeResult result;
    if (trackIndex >= seq.getNumTracks()) {
        result.errorMessage = "Track index out of bounds";
        return result;
    }

    auto* targetTrack = seq.getTrack(trackIndex);
    if (!targetTrack) {
        result.errorMessage = "Target track is null";
        return result;
    }

    const double bpm = options.bpm > 0.0 ? options.bpm : seq.getBpm();
    const uint32_t sampleRate = options.sampleRate > 0 ? options.sampleRate : 48000;

    // Calculate duration in bars & seconds
    uint32_t bars = options.numBars;
    if (bars == 0) {
        const uint32_t steps = targetTrack->getNumSteps();
        bars = (steps > 0) ? ((steps + 15) / 16) : 1;
    }
    const double stepDurationSec = 60.0 / bpm / 4.0;
    const double barDurationSec = stepDurationSec * 16.0;
    const double totalDurationSec = bars * barDurationSec;
    const uint32_t totalFrames = static_cast<uint32_t>(std::ceil(totalDurationSec * sampleRate));

    if (totalFrames == 0) {
        result.errorMessage = "Zero frames requested for render";
        return result;
    }

    if (options.onProgress) {
        options.onProgress(0.05f, "Allocating audio buffers...");
    }

    std::vector<float> masterL(totalFrames, 0.0f);
    std::vector<float> masterR(totalFrames, 0.0f);

    // Isolate target track: mute all other tracks
    const size_t numTracks = seq.getNumTracks();
    std::vector<bool> origMute(numTracks);
    for (size_t i = 0; i < numTracks; ++i) {
        auto* trk = seq.getTrack(i);
        origMute[i] = trk->isMuted();
        trk->setMuted(i != trackIndex);
    }
    const bool wasFrozen = targetTrack->isFrozen();
    targetTrack->setFrozen(false); // Unfreeze during render so notes trigger

    // Make sure target node is enabled during render
    const audio::NodeId targetId = targetTrack->getTargetNodeId();
    std::shared_ptr<GraphNode> targetNode = nullptr;
    bool targetNodeWasEnabled = true;
    if (targetId != 0) {
        targetNode = graph.getNode(targetId);
        if (targetNode) {
            targetNodeWasEnabled = targetNode->isEnabled();
            targetNode->setEnabled(true);
        }
    }

    // Prepare graph & transport
    const double origBpm = seq.getBpm();
    const uint32_t origSR = seq.getTransport().getSampleRate();
    seq.setBpm(bpm);
    seq.getTransport().setSampleRate(sampleRate);
    graph.prepare(sampleRate, 512);

    seq.stop();
    seq.start();

    constexpr uint32_t CHUNK_SIZE = 512;
    alignas(64) float chunkL[CHUNK_SIZE];
    alignas(64) float chunkR[CHUNK_SIZE];

    const auto startTime = std::chrono::high_resolution_clock::now();
    uint32_t framesRendered = 0;
    float peakL = 0.0f;
    float peakR = 0.0f;

    while (framesRendered < totalFrames) {
        const uint32_t cur = std::min(CHUNK_SIZE, totalFrames - framesRendered);

        seq.processBlock(cur, graph);
        graph.process(chunkL, chunkR, cur);

        for (uint32_t i = 0; i < cur; ++i) {
            const float sL = chunkL[i];
            const float sR = chunkR[i];
            masterL[framesRendered + i] = sL;
            masterR[framesRendered + i] = sR;
            peakL = std::max(peakL, std::abs(sL));
            peakR = std::max(peakR, std::abs(sR));
        }
        framesRendered += cur;

        if (options.onProgress && ((framesRendered % (CHUNK_SIZE * 16) == 0) || framesRendered == totalFrames)) {
            const float p = 0.10f + (0.75f * static_cast<float>(framesRendered) / static_cast<float>(totalFrames));
            options.onProgress(p, "Baking track audio...");
        }
    }

    seq.stop();

    // Restore sequencer and graph states
    for (size_t i = 0; i < numTracks; ++i) {
        seq.getTrack(i)->setMuted(origMute[i]);
    }
    targetTrack->setFrozen(wasFrozen);
    if (targetNode) {
        targetNode->setEnabled(targetNodeWasEnabled);
    }
    seq.setBpm(origBpm);
    seq.getTransport().setSampleRate(origSR);

    // Peak limiting / Normalization if peak exceeds limit ceiling
    if (options.normalizePeaks) {
        if (options.onProgress) {
            options.onProgress(0.90f, "Normalizing peaks...");
        }
        const float maxPeak = std::max(peakL, peakR);
        if (maxPeak > options.peakLimit && maxPeak > 0.0001f) {
            const float attenuation = options.peakLimit / maxPeak;
            for (uint32_t i = 0; i < totalFrames; ++i) {
                masterL[i] *= attenuation;
                masterR[i] *= attenuation;
            }
            peakL *= attenuation;
            peakR *= attenuation;
        }
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    result.success = true;
    result.bufferL = std::move(masterL);
    result.bufferR = std::move(masterR);
    result.sampleRate = sampleRate;
    result.totalFrames = totalFrames;
    result.durationSeconds = totalDurationSec;
    result.renderTimeMs = elapsedMs;
    result.speedMultiplier = (totalDurationSec * 1000.0) / std::max(0.001, elapsedMs);
    result.peakL = peakL;
    result.peakR = peakR;
    result.contentHash = computeTrackHash(*targetTrack, bpm, sampleRate, &graph);

    if (options.onProgress) {
        options.onProgress(1.0f, "Track freeze bake complete");
    }

    return result;
}

bool TrackFreezeEngine::freezeTrack(
    AudioEngine& engine,
    size_t trackIndex,
    const FreezeOptions& options
) {
    if (trackIndex >= engine.getSequencer().getNumTracks()) {
        return false;
    }

    auto* track = engine.getSequencer().getTrack(trackIndex);
    if (!track) return false;

    FreezeOptions opts = options;
    if (opts.sampleRate == 0) opts.sampleRate = engine.getSampleRate();
    if (opts.bpm <= 0.0) opts.bpm = engine.getSequencer().getBpm();

    FreezeResult result = renderTrackOffline(engine.getGraph(), engine.getSequencer(), trackIndex, opts);
    if (!result.success) return false;

    // Store frozen buffers and hash
    track->setFrozenBuffers(std::move(result.bufferL), std::move(result.bufferR), result.sampleRate, result.contentHash);
    track->setFrozen(true);

    // Disable target synth node in graph to reclaim 100% CPU
    if (track->getTargetNodeId() != 0) {
        auto node = engine.getGraph().getNode(track->getTargetNodeId());
        if (node) {
            node->setEnabled(false);
        }
    }

    return true;
}

bool TrackFreezeEngine::unfreezeTrack(
    AudioEngine& engine,
    size_t trackIndex,
    bool clearBuffers
) {
    if (trackIndex >= engine.getSequencer().getNumTracks()) {
        return false;
    }

    auto* track = engine.getSequencer().getTrack(trackIndex);
    if (!track) return false;

    if (clearBuffers) {
        track->clearFrozenBuffers();
    } else {
        track->setFrozen(false);
    }

    // Re-enable target synth node in graph
    if (track->getTargetNodeId() != 0) {
        auto node = engine.getGraph().getNode(track->getTargetNodeId());
        if (node) {
            node->setEnabled(true);
        }
    }

    return true;
}

bool TrackFreezeEngine::toggleFreezeTrack(
    AudioEngine& engine,
    size_t trackIndex,
    const FreezeOptions& options
) {
    if (trackIndex >= engine.getSequencer().getNumTracks()) {
        return false;
    }
    auto* track = engine.getSequencer().getTrack(trackIndex);
    if (!track) return false;

    if (track->isFrozen()) {
        return unfreezeTrack(engine, trackIndex, false);
    } else {
        return freezeTrack(engine, trackIndex, options);
    }
}

bool TrackFreezeEngine::bounceTrackToWav(
    AudioGraph& graph,
    sequencer::StepSequencer& seq,
    size_t trackIndex,
    const std::string& wavFilePath,
    const FreezeOptions& options
) {
    FreezeResult result = renderTrackOffline(graph, seq, trackIndex, options);
    if (!result.success || result.bufferL.empty()) {
        return false;
    }

    const float* channelPtrs[2] = {
        result.bufferL.data(),
        result.bufferR.empty() ? result.bufferL.data() : result.bufferR.data()
    };

    return exporting::WavExporter::writeWavFile(
        wavFilePath,
        channelPtrs,
        2,
        static_cast<uint32_t>(result.totalFrames),
        result.sampleRate,
        exporting::WavFormat::Pcm24,
        true
    );
}

std::future<FreezeResult> TrackFreezeEngine::renderTrackAsync(
    std::shared_ptr<AudioGraph> graphCopy,
    std::shared_ptr<sequencer::StepSequencer> seqCopy,
    size_t trackIndex,
    FreezeOptions options
) {
    return std::async(std::launch::async, [graphCopy, seqCopy, trackIndex, options]() mutable {
        if (!graphCopy || !seqCopy) {
            FreezeResult r;
            r.errorMessage = "Null graph or sequencer copy";
            return r;
        }
        return renderTrackOffline(*graphCopy, *seqCopy, trackIndex, options);
    });
}

} // namespace eatsbits::audio::trackfreeze
