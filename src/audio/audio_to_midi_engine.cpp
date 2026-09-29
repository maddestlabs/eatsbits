#include "eatsbits/audio/audio_to_midi_engine.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <iostream>
#include <numbers>

// Miniaudio declaration for decoder functions
#include "miniaudio.h"

namespace eatsbits::audio {
namespace {
inline uint16_t readU16LE(const uint8_t* p) noexcept {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
inline int16_t readI16LE(const uint8_t* p) noexcept {
    return static_cast<int16_t>(readU16LE(p));
}
inline uint32_t readU32LE(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) |
          (static_cast<uint32_t>(p[1]) << 8) |
          (static_cast<uint32_t>(p[2]) << 16) |
          (static_cast<uint32_t>(p[3]) << 24);
}
} // namespace

// ============================================================================
// DecodedAudioBuffer Implementation
// ============================================================================

DecodedAudioBuffer DecodedAudioBuffer::decodeFromFile(const std::string& filePath) {
    DecodedAudioBuffer buffer;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder;

    ma_result result = ma_decoder_init_file(filePath.c_str(), &config, &decoder);
    if (result != MA_SUCCESS) {
        // Fallback: check if standard WAV file manually
        std::ifstream file(filePath, std::ios::binary);
        if (file.is_open()) {
            file.seekg(0, std::ios::end);
            size_t sz = static_cast<size_t>(file.tellg());
            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> data(sz);
            file.read(reinterpret_cast<char*>(data.data()), sz);
            return decodeFromMemory(data.data(), data.size());
        }
        return buffer;
    }

    buffer.sampleRate = decoder.outputSampleRate;
    buffer.channels = decoder.outputChannels;

    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);

    if (totalFrames > 0 && totalFrames < 100000000) { // Safety cap ~35 minutes
        buffer.samples.resize(static_cast<size_t>(totalFrames * buffer.channels));
        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(&decoder, buffer.samples.data(), totalFrames, &framesRead);
        buffer.samples.resize(static_cast<size_t>(framesRead * buffer.channels));
    } else {
        // Stream read in chunks if length is unknown
        constexpr size_t kChunkFrames = 4096;
        std::vector<float> chunk(kChunkFrames * buffer.channels);
        ma_uint64 framesRead = 0;
        while ((result = ma_decoder_read_pcm_frames(&decoder, chunk.data(), kChunkFrames, &framesRead)) == MA_SUCCESS && framesRead > 0) {
            buffer.samples.insert(buffer.samples.end(), chunk.begin(), chunk.begin() + static_cast<size_t>(framesRead * buffer.channels));
        }
    }

    ma_decoder_uninit(&decoder);
    return buffer;
}

DecodedAudioBuffer DecodedAudioBuffer::decodeFromMemory(const uint8_t* data, size_t size) {
    DecodedAudioBuffer buffer;
    if (!data || size < 44) return buffer;

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder;

    ma_result result = ma_decoder_init_memory(data, size, &config, &decoder);
    if (result == MA_SUCCESS) {
        buffer.sampleRate = decoder.outputSampleRate;
        buffer.channels = decoder.outputChannels;

        ma_uint64 totalFrames = 0;
        ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
        if (totalFrames > 0 && totalFrames < 100000000) {
            buffer.samples.resize(static_cast<size_t>(totalFrames * buffer.channels));
            ma_uint64 framesRead = 0;
            ma_decoder_read_pcm_frames(&decoder, buffer.samples.data(), totalFrames, &framesRead);
            buffer.samples.resize(static_cast<size_t>(framesRead * buffer.channels));
        }
        ma_decoder_uninit(&decoder);
        return buffer;
    }

    // Manual fallback parser for RIFF/WAV PCM
    if (std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WAVE", 4) == 0) {
        size_t offset = 12;
        uint16_t audioFormat = 1;
        uint16_t numChannels = 1;
        uint32_t sampleRate = 44100;
        uint16_t bitsPerSample = 16;
        const uint8_t* pcmData = nullptr;
        size_t pcmDataSize = 0;

        while (offset + 8 <= size) {
            char chunkId[5] = {0};
            std::memcpy(chunkId, data + offset, 4);
            uint32_t chunkSize = readU32LE(data + offset + 4);
            offset += 8;

            if (std::strcmp(chunkId, "fmt ") == 0 && offset + 16 <= size) {
                audioFormat = readU16LE(data + offset);
                numChannels = readU16LE(data + offset + 2);
                sampleRate = readU32LE(data + offset + 4);
                bitsPerSample = readU16LE(data + offset + 14);
            } else if (std::strcmp(chunkId, "data") == 0) {
                pcmData = data + offset;
                pcmDataSize = std::min(static_cast<size_t>(chunkSize), size - offset);
                break;
            }
            offset += chunkSize;
        }

        if (pcmData && pcmDataSize > 0 && numChannels > 0 && sampleRate > 0) {
            buffer.sampleRate = sampleRate;
            buffer.channels = numChannels;
            if (audioFormat == 1 && bitsPerSample == 16) {
                size_t numSamples = pcmDataSize / sizeof(int16_t);
                buffer.samples.resize(numSamples);
                for (size_t i = 0; i < numSamples; ++i) {
                    buffer.samples[i] = static_cast<float>(readI16LE(pcmData + i * 2)) / 32768.0f;
                }
            } else if (audioFormat == 3 && bitsPerSample == 32) {
                size_t numSamples = pcmDataSize / sizeof(float);
                buffer.samples.resize(numSamples);
                std::memcpy(buffer.samples.data(), pcmData, numSamples * sizeof(float));
            }
        }
    }

    return buffer;
}

DecodedAudioBuffer DecodedAudioBuffer::createSyntheticSine(double freqHz, double durationSec,
                                                         uint32_t sampleRate, float amplitude) {
    DecodedAudioBuffer buffer;
    buffer.sampleRate = sampleRate;
    buffer.channels = 1;
    size_t numSamples = static_cast<size_t>(static_cast<double>(sampleRate) * durationSec);
    buffer.samples.resize(numSamples);

    double phaseIncr = 2.0 * std::numbers::pi * freqHz / static_cast<double>(sampleRate);
    for (size_t i = 0; i < numSamples; ++i) {
        buffer.samples[i] = amplitude * static_cast<float>(std::sin(phaseIncr * static_cast<double>(i)));
    }
    return buffer;
}

DecodedAudioBuffer DecodedAudioBuffer::createSyntheticChord(const std::vector<double>& frequencies,
                                                          double durationSec,
                                                          uint32_t sampleRate, float amplitude) {
    DecodedAudioBuffer buffer;
    buffer.sampleRate = sampleRate;
    buffer.channels = 1;
    size_t numSamples = static_cast<size_t>(static_cast<double>(sampleRate) * durationSec);
    buffer.samples.resize(numSamples, 0.0f);

    if (frequencies.empty()) return buffer;

    float perToneAmp = amplitude / static_cast<float>(frequencies.size());
    for (double f : frequencies) {
        double phaseIncr = 2.0 * std::numbers::pi * f / static_cast<double>(sampleRate);
        for (size_t i = 0; i < numSamples; ++i) {
            buffer.samples[i] += perToneAmp * static_cast<float>(std::sin(phaseIncr * static_cast<double>(i)));
        }
    }
    return buffer;
}

std::vector<float> DecodedAudioBuffer::resampleAndDownmix(uint32_t targetSampleRate) const {
    return AudioToMidiEngine::resampleAndDownmix(samples, sampleRate, channels, targetSampleRate);
}

// ============================================================================
// WaveformOverview Implementation
// ============================================================================

WaveformOverview WaveformOverview::generate(const std::vector<float>& samples, size_t numPoints) {
    WaveformOverview overview;
    if (samples.empty() || numPoints == 0) return overview;

    overview.minPeaks.assign(numPoints, 0.0f);
    overview.maxPeaks.assign(numPoints, 0.0f);

    double samplesPerPoint = static_cast<double>(samples.size()) / static_cast<double>(numPoints);

    for (size_t i = 0; i < numPoints; ++i) {
        size_t start = static_cast<size_t>(static_cast<double>(i) * samplesPerPoint);
        size_t end = std::min(samples.size(), static_cast<size_t>(static_cast<double>(i + 1) * samplesPerPoint));
        if (start >= samples.size()) break;

        float minVal = 0.0f;
        float maxVal = 0.0f;
        for (size_t s = start; s < end; ++s) {
            float val = samples[s];
            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
        }
        overview.minPeaks[i] = minVal;
        overview.maxPeaks[i] = maxVal;
    }
    return overview;
}

// ============================================================================
// AudioToMidiEngine Implementation
// ============================================================================

std::vector<float> AudioToMidiEngine::resampleAndDownmix(
    const std::vector<float>& samples,
    uint32_t srcSampleRate,
    uint32_t channels,
    uint32_t dstSampleRate) {

    if (samples.empty() || srcSampleRate == 0 || channels == 0 || dstSampleRate == 0) {
        return {};
    }

    size_t numSrcFrames = samples.size() / channels;
    std::vector<float> monoSrc(numSrcFrames);

    if (channels == 1) {
        std::copy(samples.begin(), samples.begin() + numSrcFrames, monoSrc.begin());
    } else {
        float invCh = 1.0f / static_cast<float>(channels);
        for (size_t i = 0; i < numSrcFrames; ++i) {
            float sum = 0.0f;
            size_t base = i * channels;
            for (size_t c = 0; c < channels; ++c) {
                sum += samples[base + c];
            }
            monoSrc[i] = sum * invCh;
        }
    }

    if (srcSampleRate == dstSampleRate) {
        return monoSrc;
    }

    // Linear interpolation resampling
    double ratio = static_cast<double>(dstSampleRate) / static_cast<double>(srcSampleRate);
    size_t numDstFrames = static_cast<size_t>(static_cast<double>(numSrcFrames) * ratio);
    std::vector<float> dst(numDstFrames);

    for (size_t i = 0; i < numDstFrames; ++i) {
        double srcPos = static_cast<double>(i) / ratio;
        size_t idx0 = static_cast<size_t>(std::floor(srcPos));
        float frac = static_cast<float>(srcPos - static_cast<double>(idx0));
        size_t idx1 = std::min(idx0 + 1, numSrcFrames - 1);

        dst[i] = monoSrc[idx0] * (1.0f - frac) + monoSrc[idx1] * frac;
    }

    return dst;
}

float AudioToMidiEngine::correlateTone(
    const std::vector<float>& samples,
    size_t offset,
    size_t length,
    double frequency,
    uint32_t sampleRate,
    const std::vector<float>& window) {

    if (offset + length > samples.size() || sampleRate == 0 || length == 0) return 0.0f;

    double omega = 2.0 * std::numbers::pi * frequency / static_cast<double>(sampleRate);
    double real = 0.0;
    double imag = 0.0;

    for (size_t i = 0; i < length; ++i) {
        double s = static_cast<double>(samples[offset + i] * window[i]);
        double angle = omega * static_cast<double>(i);
        real += s * std::cos(angle);
        imag += s * std::sin(angle);
    }

    double magnitude = std::sqrt(real * real + imag * imag) / (static_cast<double>(length) * 0.5);
    return static_cast<float>(magnitude);
}

bool AudioToMidiEngine::isSpectralPeak(const std::vector<float>& frame, int pitch) {
    if (pitch < 0 || pitch >= static_cast<int>(frame.size())) return false;
    float val = frame[static_cast<size_t>(pitch)];
    if (pitch > 0 && frame[static_cast<size_t>(pitch - 1)] > val) return false;
    if (pitch + 1 < static_cast<int>(frame.size()) && frame[static_cast<size_t>(pitch + 1)] > val) return false;
    return true;
}

float AudioToMidiEngine::detectPitchYin(
    const float* frame,
    size_t windowSize,
    uint32_t sampleRate,
    float minFreq,
    float maxFreq,
    float threshold) {

    if (!frame || windowSize < 128 || sampleRate == 0) return 0.0f;

    size_t halfWindow = windowSize / 2;
    size_t minPeriod = static_cast<size_t>(std::max(2.0f, static_cast<float>(sampleRate) / maxFreq));
    size_t maxPeriod = static_cast<size_t>(std::min(static_cast<float>(halfWindow - 1), static_cast<float>(sampleRate) / minFreq));
    if (minPeriod >= maxPeriod) return 0.0f;

    std::vector<float> d(halfWindow, 0.0f);

    // Step 1: Difference function
    for (size_t tau = 1; tau < halfWindow; ++tau) {
        float sum = 0.0f;
        for (size_t j = 0; j < halfWindow; ++j) {
            float delta = frame[j] - frame[j + tau];
            sum += delta * delta;
        }
        d[tau] = sum;
    }

    // Step 2: Cumulative mean normalized difference
    std::vector<float> dPrime(halfWindow, 1.0f);
    float runningSum = 0.0f;
    for (size_t tau = 1; tau < halfWindow; ++tau) {
        runningSum += d[tau];
        if (runningSum > 1e-9f) {
            dPrime[tau] = d[tau] / (runningSum / static_cast<float>(tau));
        } else {
            dPrime[tau] = 1.0f;
        }
    }

    // Step 3: Absolute thresholding
    size_t bestTau = 0;
    for (size_t tau = minPeriod; tau < maxPeriod; ++tau) {
        if (dPrime[tau] < threshold) {
            // Find local minimum
            while (tau + 1 < maxPeriod && dPrime[tau + 1] < dPrime[tau]) {
                tau++;
            }
            bestTau = tau;
            break;
        }
    }

    // Fallback: Global minimum if none below threshold
    if (bestTau == 0) {
        float minVal = 100.0f;
        for (size_t tau = minPeriod; tau < maxPeriod; ++tau) {
            if (dPrime[tau] < minVal) {
                minVal = dPrime[tau];
                bestTau = tau;
            }
        }
        if (minVal > 0.45f) return 0.0f; // Insufficient pitch confidence
    }

    // Step 4: Parabolic interpolation
    if (bestTau > 0 && bestTau + 1 < halfWindow) {
        float s0 = dPrime[bestTau - 1];
        float s1 = dPrime[bestTau];
        float s2 = dPrime[bestTau + 1];
        float denom = 2.0f * (s0 - 2.0f * s1 + s2);
        float delta = (std::abs(denom) > 1e-7f) ? (s0 - s2) / denom : 0.0f;
        float trueTau = static_cast<float>(bestTau) + delta;
        if (trueTau > 0.0f) {
            return static_cast<float>(sampleRate) / trueTau;
        }
    }

    return (bestTau > 0) ? (static_cast<float>(sampleRate) / static_cast<float>(bestTau)) : 0.0f;
}

std::vector<TransientOnset> AudioToMidiEngine::detectOnsetsAndTransients(
    const std::vector<float>& monoSamples,
    uint32_t sampleRate,
    float threshold) {

    std::vector<TransientOnset> onsets;
    if (monoSamples.empty() || sampleRate == 0) return onsets;

    constexpr size_t hopSize = 256;
    constexpr size_t frameSize = 512;
    size_t numFrames = monoSamples.size() / hopSize;
    if (numFrames < 4) return onsets;

    float prevLow = 0.0f;
    float prevMid = 0.0f;
    float prevHigh = 0.0f;

    for (size_t f = 0; f < numFrames; ++f) {
        size_t start = f * hopSize;
        if (start + frameSize > monoSamples.size()) break;

        // Simple 3-band energy partitioning
        // Low: approx fundamental energy of kick (sub 250 Hz)
        // Mid: snare / body (250 - 2500 Hz)
        // High: hi-hat / cymbal sizzle (> 2500 Hz)
        float eLow = 0.0f, eMid = 0.0f, eHigh = 0.0f;
        float prevSample = 0.0f;

        for (size_t i = 0; i < frameSize; ++i) {
            float s = monoSamples[start + i];
            float diff = s - prevSample; // High pass pre-emphasis
            prevSample = s;

            float absS = std::abs(s);
            float absDiff = std::abs(diff);

            eLow += absS * absS;
            eMid += (absS * 0.5f + absDiff * 0.5f);
            eHigh += absDiff * absDiff;
        }

        eLow = std::sqrt(eLow / static_cast<float>(frameSize));
        eMid = eMid / static_cast<float>(frameSize);
        eHigh = std::sqrt(eHigh / static_cast<float>(frameSize));

        float fluxLow = std::max(0.0f, eLow - prevLow);
        float fluxMid = std::max(0.0f, eMid - prevMid);
        float fluxHigh = std::max(0.0f, eHigh - prevHigh);

        prevLow = eLow * 0.85f;
        prevMid = eMid * 0.85f;
        prevHigh = eHigh * 0.85f;

        float totalFlux = fluxLow + fluxMid + fluxHigh;
        if (totalFlux > threshold * 0.15f) {
            TransientOnset onset;
            onset.timeSec = static_cast<float>(start) / static_cast<float>(sampleRate);
            onset.strength = std::min(1.0f, totalFlux / (threshold * 0.5f));

            if (fluxLow > fluxMid && fluxLow > fluxHigh) {
                onset.suggestedMidiNote = 36; // Kick (C1)
                onset.subBand = 0;
            } else if (fluxHigh > fluxMid && fluxHigh > fluxLow) {
                onset.suggestedMidiNote = 42; // Closed Hat (F#1)
                onset.subBand = 2;
            } else {
                onset.suggestedMidiNote = 38; // Snare (D1)
                onset.subBand = 1;
            }
            onsets.push_back(onset);
        }
    }

    return onsets;
}

// ============================================================================
// Transcription Execution Pipelines
// ============================================================================

TranscribedMidiTrack AudioToMidiEngine::transcribeAudioBuffer(
    const DecodedAudioBuffer& audio,
    const AudioToMidiOptions& options,
    CancellationToken* cancellationToken,
    std::function<void(float progress, const std::string& status)> onProgress) {

    if (cancellationToken && cancellationToken->isCancelled()) {
        TranscribedMidiTrack cancelledTrack;
        cancelledTrack.name = "Cancelled";
        return cancelledTrack;
    }

    if (audio.empty()) {
        return TranscribedMidiTrack{};
    }

    if (onProgress) onProgress(0.05f, "Resampling and conditioning audio signal...");

    // Resample and downmix to 22050 Hz mono for optimal time-frequency DSP
    std::vector<float> monoSamples = resampleAndDownmix(
        audio.samples, audio.sampleRate, audio.channels, kTargetSampleRate);

    if (monoSamples.empty()) {
        return TranscribedMidiTrack{};
    }

    TranscribedMidiTrack result;

    switch (options.mode) {
        case TranscriptionEngineMode::YinMonophonic:
            result = transcribeYin(monoSamples, kTargetSampleRate, options, cancellationToken, onProgress);
            break;
        case TranscriptionEngineMode::PercussiveTransient:
            result = transcribePercussive(monoSamples, kTargetSampleRate, options, cancellationToken, onProgress);
            break;
        case TranscriptionEngineMode::HybridDsp:
        default:
            result = transcribeHybridDsp(monoSamples, options, cancellationToken, onProgress);
            break;
    }

    if (cancellationToken && cancellationToken->isCancelled()) {
        TranscribedMidiTrack cancelledTrack;
        cancelledTrack.name = "Cancelled";
        return cancelledTrack;
    }

    // Grid Quantization
    if (options.quantizeSteps > 0 && !result.notes.empty()) {
        float qFactor = static_cast<float>(options.quantizeSteps);
        for (auto& n : result.notes) {
            n.startStep = std::round(n.startStep / qFactor) * qFactor;
            n.durationSteps = std::max(qFactor, std::round(n.durationSteps / qFactor) * qFactor);
        }
    }

    // Sort notes chronologically, then by pitch
    std::sort(result.notes.begin(), result.notes.end(), [](const TranscribedNote& a, const TranscribedNote& b) {
        if (std::abs(a.startStep - b.startStep) > 0.001f) {
            return a.startStep < b.startStep;
        }
        return a.pitch < b.pitch;
    });

    // Chord Extraction for Chord Track integration
    if (options.extractChords && !result.notes.empty()) {
        if (onProgress) onProgress(0.96f, "Analyzing harmonic chord progression...");

        std::vector<theory::TheoryNote> theoryNotes;
        theoryNotes.reserve(result.notes.size());
        for (const auto& n : result.notes) {
            theoryNotes.push_back({n.pitch, n.startStep, n.durationSteps, n.velocity});
        }

        float maxStep = 0.0f;
        for (const auto& n : result.notes) {
            maxStep = std::max(maxStep, n.startStep + n.durationSteps);
        }
        uint32_t totalBars = static_cast<uint32_t>(std::ceil(maxStep / 16.0f));
        result.detectedChords = theory::ChordTheory::extractChordsFromNotes(theoryNotes, 0, totalBars, 16);
    }

    if (onProgress) onProgress(1.0f, "Transcription complete!");
    return result;
}

// ----------------------------------------------------------------------------
// Hybrid DSP Polyphonic Transcription Engine (Eatsbeats Parity)
// ----------------------------------------------------------------------------

namespace {
struct ActiveNoteTracker {
    int pitch{60};
    double startTimeSec{0.0};
    double durationSec{0.0};
    float peakVelocity{0.8f};
};

void emitNote(
    std::vector<TranscribedNote>& noteList,
    const ActiveNoteTracker& tracker,
    double secPerStep,
    const AudioToMidiOptions& options) {

    double durMs = tracker.durationSec * 1000.0;
    if (durMs < static_cast<double>(options.minNoteDurationMs)) return;

    float startStep = static_cast<float>(std::max(0.0, tracker.startTimeSec / secPerStep));
    float durationSteps = static_cast<float>(std::max(0.25, tracker.durationSec / secPerStep));
    float scaledVel = std::clamp(tracker.peakVelocity * options.velocitySensitivity, 0.2f, 1.0f);

    TranscribedNote note;
    note.id = "transcribed_" + std::to_string(tracker.pitch) + "_" + std::to_string(noteList.size());
    note.pitch = static_cast<uint8_t>(tracker.pitch);
    note.startStep = std::round(startStep * 4.0f) / 4.0f; // 1/64 step precision
    note.durationSteps = std::round(durationSteps * 4.0f) / 4.0f;
    note.velocity = scaledVel;

    noteList.push_back(note);
}
} // namespace

TranscribedMidiTrack AudioToMidiEngine::transcribeHybridDsp(
    const std::vector<float>& monoSamples,
    const AudioToMidiOptions& options,
    CancellationToken* token,
    std::function<void(float, const std::string&)> onProgress) {

    TranscribedMidiTrack track;
    track.name = "Transcribed MIDI Track";

    size_t numFrames = monoSamples.size() / kHopSize;
    if (numFrames < 2) return track;

    if (onProgress) onProgress(0.2f, "Calculating pitch frequency tone banks...");

    std::array<double, 128> pitchFrequencies{};
    for (int m = 0; m < 128; ++m) {
        pitchFrequencies[static_cast<size_t>(m)] = 440.0 * std::pow(2.0, (static_cast<double>(m) - 69.0) / 12.0);
    }

    // Hann window
    std::vector<float> window(kFftSize);
    for (size_t i = 0; i < kFftSize; ++i) {
        window[i] = static_cast<float>(0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(kFftSize - 1))));
    }

    // Activations matrix: [numFrames][128]
    std::vector<std::vector<float>> activations(numFrames, std::vector<float>(128, 0.0f));
    std::vector<float> onsets(numFrames, 0.0f);

    float prevEnergy = 0.0f;
    double nyquist = static_cast<double>(kTargetSampleRate) * 0.5;

    for (size_t f = 0; f < numFrames; ++f) {
        if (token && token->isCancelled()) return track;

        size_t startIdx = f * kHopSize;
        if (startIdx + kFftSize > monoSamples.size()) break;

        float frameEnergy = 0.0f;
        for (size_t i = 0; i < kFftSize; ++i) {
            float s = monoSamples[startIdx + i];
            frameEnergy += s * s;
        }
        frameEnergy = std::sqrt(frameEnergy / static_cast<float>(kFftSize));

        // Onset energy flux
        float flux = std::max(0.0f, frameEnergy - prevEnergy);
        onsets[f] = flux;
        prevEnergy = frameEnergy * 0.85f;

        // Multi-pitch harmonic correlation
        for (int pitch = options.minMidiPitch; pitch <= options.maxMidiPitch; ++pitch) {
            double f0 = pitchFrequencies[static_cast<size_t>(pitch)];
            if (f0 * 4.0 > nyquist) continue;

            float fundEnergy = correlateTone(monoSamples, startIdx, kFftSize, f0, kTargetSampleRate, window);
            float harm2 = correlateTone(monoSamples, startIdx, kFftSize, f0 * 2.0, kTargetSampleRate, window) * 0.5f;

            float totalScore = fundEnergy + harm2;
            activations[f][static_cast<size_t>(pitch)] = std::clamp(totalScore, 0.0f, 1.0f);
        }

        if (f % 60 == 0 && onProgress) {
            float p = 0.2f + 0.5f * (static_cast<float>(f) / static_cast<float>(numFrames));
            onProgress(p, "Analyzing polyphony & harmonic contours (" + std::to_string(static_cast<int>(p * 100.0f)) + "%)...");
        }
    }

    if (token && token->isCancelled()) return track;

    if (onProgress) onProgress(0.75f, "Tracking note onsets, sustained frames, and durations...");

    // Normalize activations
    float maxActivation = 0.001f;
    for (size_t f = 0; f < numFrames; ++f) {
        for (int p = options.minMidiPitch; p <= options.maxMidiPitch; ++p) {
            if (activations[f][static_cast<size_t>(p)] > maxActivation) {
                maxActivation = activations[f][static_cast<size_t>(p)];
            }
        }
    }

    float invMax = 1.0f / maxActivation;
    for (size_t f = 0; f < numFrames; ++f) {
        for (int p = options.minMidiPitch; p <= options.maxMidiPitch; ++p) {
            activations[f][static_cast<size_t>(p)] *= invMax;
        }
    }

    // Active note tracker map
    std::unordered_map<int, ActiveNoteTracker> activeNotes;
    double frameDurationSec = static_cast<double>(kHopSize) / static_cast<double>(kTargetSampleRate);
    double secPerStep = (60.0 / options.targetBpm) / 4.0; // 16th note in seconds

    for (size_t f = 0; f < numFrames; ++f) {
        double currentTimeSec = static_cast<double>(f) * frameDurationSec;
        bool isOnsetFrame = onsets[f] > (options.onsetThreshold * 0.1f);

        for (int p = options.minMidiPitch; p <= options.maxMidiPitch; ++p) {
            float act = activations[f][static_cast<size_t>(p)];
            bool isActive = act >= options.frameThreshold;
            bool isPeak = isSpectralPeak(activations[f], p);

            if (isActive && isPeak) {
                auto it = activeNotes.find(p);
                if (it == activeNotes.end()) {
                    // Start new note
                    ActiveNoteTracker tracker;
                    tracker.pitch = p;
                    tracker.startTimeSec = currentTimeSec;
                    tracker.durationSec = frameDurationSec;
                    tracker.peakVelocity = act;
                    activeNotes[p] = tracker;
                } else {
                    // Note continuation
                    it->second.durationSec = currentTimeSec - it->second.startTimeSec;
                    if (act > it->second.peakVelocity) {
                        it->second.peakVelocity = act;
                    }

                    // Check for re-articulation on strong onset
                    if (isOnsetFrame && (currentTimeSec - it->second.startTimeSec) * 1000.0 > static_cast<double>(options.minNoteDurationMs)) {
                        emitNote(track.notes, it->second, secPerStep, options);
                        ActiveNoteTracker newTracker;
                        newTracker.pitch = p;
                        newTracker.startTimeSec = currentTimeSec;
                        newTracker.durationSec = frameDurationSec;
                        newTracker.peakVelocity = act;
                        activeNotes[p] = newTracker;
                    }
                }
            } else {
                // Note ended
                auto it = activeNotes.find(p);
                if (it != activeNotes.end()) {
                    it->second.durationSec = currentTimeSec - it->second.startTimeSec;
                    emitNote(track.notes, it->second, secPerStep, options);
                    activeNotes.erase(it);
                }
            }
        }
    }

    // Flush any remaining active notes
    double totalDuration = static_cast<double>(numFrames) * frameDurationSec;
    for (auto& pair : activeNotes) {
        pair.second.durationSec = totalDuration - pair.second.startTimeSec;
        emitNote(track.notes, pair.second, secPerStep, options);
    }

    return track;
}

// ----------------------------------------------------------------------------
// YIN Monophonic Pitch Tracker
// ----------------------------------------------------------------------------

TranscribedMidiTrack AudioToMidiEngine::transcribeYin(
    const std::vector<float>& monoSamples,
    uint32_t sampleRate,
    const AudioToMidiOptions& options,
    CancellationToken* token,
    std::function<void(float, const std::string&)> onProgress) {

    TranscribedMidiTrack track;
    track.name = "Transcribed Lead (YIN)";

    constexpr size_t yinWindow = 1024;
    constexpr size_t hopSize = 256;
    size_t numFrames = monoSamples.size() / hopSize;
    if (numFrames < 4) return track;

    double frameDurationSec = static_cast<double>(hopSize) / static_cast<double>(sampleRate);
    double secPerStep = (60.0 / options.targetBpm) / 4.0;

    int currentPitch = -1;
    double currentStartTime = 0.0;
    double currentDuration = 0.0;
    float peakVel = 0.0f;

    for (size_t f = 0; f < numFrames; ++f) {
        if (token && token->isCancelled()) return track;

        size_t startIdx = f * hopSize;
        if (startIdx + yinWindow > monoSamples.size()) break;

        float freq = detectPitchYin(
            monoSamples.data() + startIdx,
            yinWindow,
            sampleRate,
            40.0f,
            2000.0f,
            options.frameThreshold * 0.4f);

        double currentTime = static_cast<double>(f) * frameDurationSec;

        // Calculate frame RMS
        float rms = 0.0f;
        for (size_t i = 0; i < yinWindow; ++i) {
            float s = monoSamples[startIdx + i];
            rms += s * s;
        }
        rms = std::sqrt(rms / static_cast<float>(yinWindow));

        if (freq > 20.0f && rms > (options.onsetThreshold * 0.03f)) {
            // Convert Hz to MIDI pitch
            double midiNoteDbl = 69.0 + 12.0 * std::log2(static_cast<double>(freq) / 440.0);
            int midiPitch = static_cast<int>(std::round(midiNoteDbl));

            if (midiPitch >= options.minMidiPitch && midiPitch <= options.maxMidiPitch) {
                if (currentPitch == -1) {
                    currentPitch = midiPitch;
                    currentStartTime = currentTime;
                    currentDuration = frameDurationSec;
                    peakVel = std::min(1.0f, rms * 3.0f);
                } else if (std::abs(midiPitch - currentPitch) <= 1) {
                    // Sustain
                    currentDuration = currentTime - currentStartTime;
                    peakVel = std::max(peakVel, std::min(1.0f, rms * 3.0f));
                } else {
                    // Pitch jump -> Emit previous, start new note
                    ActiveNoteTracker tr;
                    tr.pitch = currentPitch;
                    tr.startTimeSec = currentStartTime;
                    tr.durationSec = currentDuration;
                    tr.peakVelocity = peakVel;
                    emitNote(track.notes, tr, secPerStep, options);

                    currentPitch = midiPitch;
                    currentStartTime = currentTime;
                    currentDuration = frameDurationSec;
                    peakVel = std::min(1.0f, rms * 3.0f);
                }
            }
        } else {
            // Silence / unpitched
            if (currentPitch != -1) {
                ActiveNoteTracker tr;
                tr.pitch = currentPitch;
                tr.startTimeSec = currentStartTime;
                tr.durationSec = currentDuration;
                tr.peakVelocity = peakVel;
                emitNote(track.notes, tr, secPerStep, options);
                currentPitch = -1;
            }
        }

        if (f % 80 == 0 && onProgress) {
            float p = 0.2f + 0.6f * (static_cast<float>(f) / static_cast<float>(numFrames));
            onProgress(p, "YIN tracking melodic contour (" + std::to_string(static_cast<int>(p * 100.0f)) + "%)...");
        }
    }

    // Flush last note
    if (currentPitch != -1) {
        ActiveNoteTracker tr;
        tr.pitch = currentPitch;
        tr.startTimeSec = currentStartTime;
        tr.durationSec = currentDuration;
        tr.peakVelocity = peakVel;
        emitNote(track.notes, tr, secPerStep, options);
    }

    return track;
}

// ----------------------------------------------------------------------------
// Percussive Transient Rhythm Transcription Engine
// ----------------------------------------------------------------------------

TranscribedMidiTrack AudioToMidiEngine::transcribePercussive(
    const std::vector<float>& monoSamples,
    uint32_t sampleRate,
    const AudioToMidiOptions& options,
    CancellationToken* token,
    std::function<void(float, const std::string&)> onProgress) {

    TranscribedMidiTrack track;
    track.name = "Transcribed Drum Kit";

    if (onProgress) onProgress(0.3f, "Extracting percussive transient energy flux...");

    auto onsets = detectOnsetsAndTransients(monoSamples, sampleRate, options.onsetThreshold);

    if (token && token->isCancelled()) return track;

    double secPerStep = (60.0 / options.targetBpm) / 4.0;

    for (size_t i = 0; i < onsets.size(); ++i) {
        const auto& onset = onsets[i];
        float startStep = static_cast<float>(onset.timeSec / secPerStep);

        TranscribedNote note;
        note.id = "drum_" + std::to_string(onset.suggestedMidiNote) + "_" + std::to_string(i);
        note.pitch = onset.suggestedMidiNote;
        note.startStep = std::round(startStep * 4.0f) / 4.0f;
        note.durationSteps = 1.0f; // 16th note trigger
        note.velocity = std::clamp(onset.strength * options.velocitySensitivity, 0.3f, 1.0f);

        track.notes.push_back(note);
    }

    return track;
}

} // namespace eatsbits::audio
