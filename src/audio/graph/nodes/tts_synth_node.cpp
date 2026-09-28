#include "eatsbits/audio/graph/nodes/tts_synth_node.hpp"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace eatsbits::audio {

TtsSynthNode::TtsSynthNode(std::string name)
    : GraphNode(std::move(name)) {
    addInputPort(2);  // Stereo input (optional carrier or external audio)
    addOutputPort(2); // Stereo vocal output
    updateFormantTargets();
}

void TtsSynthNode::prepare(double sampleRate, uint32_t maxBlockSize) {
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    maxBlockSize_ = maxBlockSize;
    reset();
}

void TtsSynthNode::reset() noexcept {
    phase_ = 0.0f;
    vibratoPhase_ = 0.0f;
    ampEnv_ = 0.0f;
    noteOnCount_ = 0;
    queueReadIdx_ = 0;
    queueWriteIdx_ = 0;
    currentPhonemeFramesLeft_ = 0;

    for (size_t i = 0; i < 3; ++i) {
        formantsL_[i].reset();
        formantsR_[i].reset();
        currentF_[i] = targetF_[i];
    }
}

void TtsSynthNode::updateFormantTargets() noexcept {
    switch (currentVowel_) {
        case VowelPhoneme::A: // /a/ father
            targetF_[0] = 730.0f;  targetF_[1] = 1090.0f; targetF_[2] = 2440.0f;
            break;
        case VowelPhoneme::E: // /e/ bed
            targetF_[0] = 530.0f;  targetF_[1] = 1840.0f; targetF_[2] = 2480.0f;
            break;
        case VowelPhoneme::I: // /i/ see
            targetF_[0] = 270.0f;  targetF_[1] = 2290.0f; targetF_[2] = 3010.0f;
            break;
        case VowelPhoneme::O: // /o/ boat
            targetF_[0] = 570.0f;  targetF_[1] = 840.0f;  targetF_[2] = 2410.0f;
            break;
        case VowelPhoneme::U: // /u/ boot
            targetF_[0] = 300.0f;  targetF_[1] = 870.0f;  targetF_[2] = 2240.0f;
            break;
    }
}

void TtsSynthNode::setVowel(VowelPhoneme vowel) noexcept {
    currentVowel_ = vowel;
    updateFormantTargets();
}

void TtsSynthNode::setPitchHz(float hz) noexcept {
    targetPitchHz_ = std::clamp(hz, 40.0f, 2000.0f);
}

void TtsSynthNode::setGate(bool active) noexcept {
    gateActive_ = active;
}

void TtsSynthNode::setVocalMode(VocalMode mode) noexcept {
    vocalMode_ = mode;
}

void TtsSynthNode::setParameter(uint32_t paramId, float value) noexcept {
    switch (paramId) {
        case PARAM_PITCH:
            pitchMultiplier_ = std::clamp(value, 0.25f, 4.0f);
            break;
        case PARAM_TONE:
            tone_ = std::clamp(value, 0.1f, 3.0f);
            break;
        case PARAM_VOLUME:
            volume_ = std::clamp(value, 0.0f, 2.0f);
            break;
        case PARAM_SPACE:
            space_ = std::clamp(value, 0.0f, 1.0f);
            break;
        case PARAM_AIR:
            air_ = std::clamp(value, 0.0f, 1.0f);
            break;
        case PARAM_VOICE_MODE: {
            auto m = static_cast<uint32_t>(std::clamp(value, 0.0f, 2.0f));
            vocalMode_ = static_cast<VocalMode>(m);
            break;
        }
        case PARAM_SPEED:
            speedMultiplier_ = std::clamp(value, 0.25f, 3.0f);
            break;
        case PARAM_VOWEL: {
            auto v = static_cast<uint32_t>(std::clamp(value, 0.0f, 4.0f));
            setVowel(static_cast<VowelPhoneme>(v));
            break;
        }
        case PARAM_GATE:
            gateActive_ = (value > 0.5f);
            break;
        default:
            break;
    }
}

float TtsSynthNode::getParameter(uint32_t paramId) const noexcept {
    switch (paramId) {
        case PARAM_PITCH:      return pitchMultiplier_;
        case PARAM_TONE:       return tone_;
        case PARAM_VOLUME:     return volume_;
        case PARAM_SPACE:      return space_;
        case PARAM_AIR:        return air_;
        case PARAM_VOICE_MODE: return static_cast<float>(vocalMode_);
        case PARAM_SPEED:      return speedMultiplier_;
        case PARAM_VOWEL:      return static_cast<float>(currentVowel_);
        case PARAM_GATE:       return gateActive_ ? 1.0f : 0.0f;
        default:               return 0.0f;
    }
}

void TtsSynthNode::handleEvent(const AudioEvent& event) noexcept {
    GraphNode::handleEvent(event);

    if (event.type == AudioEventType::NoteOn) {
        if (event.velocity > 0.0f) {
            noteOnCount_++;
            float midiNote = static_cast<float>(event.note);
            targetPitchHz_ = 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
            gateActive_ = true;
        } else {
            if (noteOnCount_ > 0) noteOnCount_--;
            if (noteOnCount_ == 0) gateActive_ = false;
        }
    } else if (event.type == AudioEventType::NoteOff) {
        if (noteOnCount_ > 0) noteOnCount_--;
        if (noteOnCount_ == 0) gateActive_ = false;
    }
}

void TtsSynthNode::speakText(const std::string& text) {
    if (text.empty()) return;

    // Convert string syllables/characters into queued phonemes
    float curPitch = targetPitchHz_;
    for (char c : text) {
        char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        QueuedPhoneme p;
        p.pitchHz = curPitch;
        p.durationFrames = static_cast<uint32_t>((sampleRate_ * 0.14) / speedMultiplier_);

        if (lower == 'a') {
            p.vowel = VowelPhoneme::A;
            p.isConsonantNoise = false;
        } else if (lower == 'e') {
            p.vowel = VowelPhoneme::E;
            p.isConsonantNoise = false;
        } else if (lower == 'i' || lower == 'y') {
            p.vowel = VowelPhoneme::I;
            p.isConsonantNoise = false;
        } else if (lower == 'o') {
            p.vowel = VowelPhoneme::O;
            p.isConsonantNoise = false;
        } else if (lower == 'u') {
            p.vowel = VowelPhoneme::U;
            p.isConsonantNoise = false;
        } else if (lower == 's' || lower == 'f' || lower == 't' || lower == 'k' || lower == 'c') {
            p.vowel = currentVowel_;
            p.isConsonantNoise = true;
            p.durationFrames = static_cast<uint32_t>((sampleRate_ * 0.06) / speedMultiplier_);
        } else if (lower == ' ' || lower == '-') {
            continue;
        } else {
            // Default consonant
            p.vowel = currentVowel_;
            p.isConsonantNoise = false;
        }

        // Push to ring queue
        size_t nextWrite = (queueWriteIdx_ + 1) % MAX_PHONEME_QUEUE;
        if (nextWrite != queueReadIdx_) {
            phonemeQueue_[queueWriteIdx_] = p;
            queueWriteIdx_ = nextWrite;
        }
    }
}

void TtsSynthNode::triggerCue(const lyrics::LyricCue& cue) {
    pitchMultiplier_ = cue.pitch;
    speedMultiplier_ = cue.rate;
    speakText(cue.text);
}

void TtsSynthNode::processBlock(uint32_t numFrames) noexcept {
    float* outL = getOutputBuffer(0, 0);
    float* outR = getOutputBuffer(0, 1);
    if (!outL || !outR) return;

    const float* inL = getInputBuffer(0, 0);
    const float* inR = getInputBuffer(0, 1);

    const float invSr = 1.0f / static_cast<float>(sampleRate_);
    const float slewCoeff = 0.005f; // Smooth formant and pitch slewing

    for (uint32_t frame = 0; frame < numFrames; ++frame) {
        // 1. Process queued speech phonemes
        if (currentPhonemeFramesLeft_ > 0) {
            currentPhonemeFramesLeft_--;
        } else if (queueReadIdx_ != queueWriteIdx_) {
            const auto& nextP = phonemeQueue_[queueReadIdx_];
            queueReadIdx_ = (queueReadIdx_ + 1) % MAX_PHONEME_QUEUE;
            setVowel(nextP.vowel);
            targetPitchHz_ = nextP.pitchHz;
            currentPhonemeFramesLeft_ = nextP.durationFrames;
            gateActive_ = true;
        } else if (noteOnCount_ == 0) {
            // Queue is empty and no MIDI note held
            gateActive_ = false;
        }

        // 2. Amplitude Envelope tracking
        float targetAmp = gateActive_ ? 1.0f : 0.0f;
        if (ampEnv_ < targetAmp) {
            ampEnv_ += (targetAmp - ampEnv_) * 0.01f;
        } else {
            ampEnv_ += (targetAmp - ampEnv_) * 0.002f;
        }

        // 3. Smooth pitch slewing
        float effPitch = targetPitchHz_ * pitchMultiplier_;
        currentPitchHz_ += (effPitch - currentPitchHz_) * slewCoeff;

        // 4. Formant frequency slewing
        for (size_t i = 0; i < 3; ++i) {
            currentF_[i] += (targetF_[i] - currentF_[i]) * slewCoeff;
        }

        // 5. Vibrato generation (Natural mode only)
        float vibratoDepthHz = 0.0f;
        if (vocalMode_ == VocalMode::Natural) {
            vibratoPhase_ += 5.5f * 2.0f * static_cast<float>(M_PI) * invSr;
            if (vibratoPhase_ > 2.0f * static_cast<float>(M_PI)) {
                vibratoPhase_ -= 2.0f * static_cast<float>(M_PI);
            }
            vibratoDepthHz = std::sin(vibratoPhase_) * (currentPitchHz_ * 0.015f);
        }

        float instPitchHz = currentPitchHz_ + vibratoDepthHz;

        // 6. Excitation signal generation
        float excitation = 0.0f;

        if (vocalMode_ == VocalMode::Whisper) {
            // Pure unvoiced breath noise
            noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
            float white = static_cast<float>(static_cast<int32_t>(noiseSeed_)) / 2147483648.0f;
            excitation = white * 0.6f;
        } else if (vocalMode_ == VocalMode::Robot) {
            // Quantized pulse train / square carrier
            phase_ += instPitchHz * invSr;
            if (phase_ >= 1.0f) phase_ -= 1.0f;
            excitation = (phase_ < 0.25f) ? 0.7f : -0.7f;
        } else {
            // Natural glottal pulse (bandlimited pulse approximation)
            phase_ += instPitchHz * invSr;
            if (phase_ >= 1.0f) phase_ -= 1.0f;

            // Open phase of glottal cycle (first 40% of cycle)
            if (phase_ < 0.4f) {
                float p = phase_ / 0.4f;
                excitation = std::sin(p * static_cast<float>(M_PI));
            } else {
                excitation = 0.0f;
            }
        }

        // External carrier injection (if input buffer connected)
        if (inL && inR) {
            float extIn = 0.5f * (inL[frame] + inR[frame]);
            excitation = excitation * 0.5f + extIn * 0.5f;
        }

        // Add breath turbulence / air
        noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
        float airNoise = (static_cast<float>(static_cast<int32_t>(noiseSeed_)) / 2147483648.0f) * air_;
        float excL = excitation + airNoise;
        float excR = excitation + airNoise;

        // 7. Formant filtering (Left & Right channels with space stereo spread)
        float spaceDetune = space_ * 0.02f; // subtle spatial resonance difference
        formantsL_[0].f = currentF_[0] * (1.0f - spaceDetune);
        formantsL_[1].f = currentF_[1] * (1.0f - spaceDetune);
        formantsL_[2].f = currentF_[2] * (1.0f - spaceDetune);

        formantsR_[0].f = currentF_[0] * (1.0f + spaceDetune);
        formantsR_[1].f = currentF_[1] * (1.0f + spaceDetune);
        formantsR_[2].f = currentF_[2] * (1.0f + spaceDetune);

        // Tone parameter adjusts high formant weighting (F2 & F3)
        formantsL_[0].gain = 1.0f;
        formantsL_[1].gain = 0.8f * tone_;
        formantsL_[2].gain = 0.5f * tone_;

        formantsR_[0].gain = 1.0f;
        formantsR_[1].gain = 0.8f * tone_;
        formantsR_[2].gain = 0.5f * tone_;

        float sigL = formantsL_[0].process(excL, static_cast<float>(sampleRate_)) +
                     formantsL_[1].process(excL, static_cast<float>(sampleRate_)) +
                     formantsL_[2].process(excL, static_cast<float>(sampleRate_));

        float sigR = formantsR_[0].process(excR, static_cast<float>(sampleRate_)) +
                     formantsR_[1].process(excR, static_cast<float>(sampleRate_)) +
                     formantsR_[2].process(excR, static_cast<float>(sampleRate_));

        // 8. Output gain & envelope
        outL[frame] = sigL * ampEnv_ * volume_ * 1.5f;
        outR[frame] = sigR * ampEnv_ * volume_ * 1.5f;
    }
}

} // namespace eatsbits::audio
