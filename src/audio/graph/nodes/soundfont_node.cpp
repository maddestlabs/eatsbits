#include "eatsbits/audio/graph/nodes/soundfont_node.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace eatsbits::audio {

SoundFontNode::SoundFontNode(std::string name)
    : GraphNode(std::move(name)) {
    addOutputPort(2); // Stereo output
    reset();
}

void SoundFontNode::prepare(double sampleRate, uint32_t maxBlockSize) {
    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;
    reset();
}

void SoundFontNode::reset() noexcept {
    for (auto& v : voices_) {
        v.active = false;
    }
}

bool SoundFontNode::loadSoundFont(const uint8_t* data, size_t length) {
    auto sf = SoundFontDecoder::decode(data, length);
    if (!sf) return false;
    setSoundFontData(sf);
    return true;
}

bool SoundFontNode::loadSoundFontFile(const std::string& filePath) {
    auto sf = SoundFontDecoder::decodeFile(filePath);
    if (!sf) return false;
    setSoundFontData(sf);
    return true;
}

void SoundFontNode::setSoundFontData(std::shared_ptr<SoundFontData> sfData) {
    sfData_ = std::move(sfData);
    allNotesOff();
    if (sfData_ && !sfData_->presets.empty()) {
        setPreset(sfData_->presets.front().presetNum, sfData_->presets.front().bankNum);
    } else {
        activePreset_ = nullptr;
        currentPresetName_ = "None";
    }
}

void SoundFontNode::setPreset(uint16_t presetNum, uint16_t bankNum) {
    currentPresetNum_ = presetNum;
    currentBankNum_ = bankNum;
    if (sfData_) {
        activePreset_ = sfData_->findPreset(presetNum, bankNum);
        if (activePreset_) {
            currentPresetName_ = activePreset_->name;
        } else {
            currentPresetName_ = "Program " + std::to_string(presetNum);
        }
    }
}

const std::string& SoundFontNode::getCurrentPresetName() const noexcept {
    return currentPresetName_;
}

size_t SoundFontNode::getActiveVoiceCount() const noexcept {
    size_t count = 0;
    for (const auto& v : voices_) {
        if (v.active) ++count;
    }
    return count;
}

void SoundFontNode::noteOn(uint8_t note, float velocity) noexcept {
    if (!sfData_ || !activePreset_ || velocity <= 0.0f) return;

    uint8_t velInt = static_cast<uint8_t>(std::clamp(velocity * 127.0f, 1.0f, 127.0f));

    // SoundFont 2 spec: find all matching key/velocity zones in active preset
    for (const auto& zone : activePreset_->zones) {
        if (note >= zone.minKey && note <= zone.maxKey &&
            velInt >= zone.minVel && velInt <= zone.maxVel) {
            if (zone.sampleHeaderIdx >= 0 &&
                static_cast<size_t>(zone.sampleHeaderIdx) < sfData_->sampleHeaders.size()) {
                spawnVoice(note, velocity, &zone, &sfData_->sampleHeaders[zone.sampleHeaderIdx]);
            }
        }
    }
}

void SoundFontNode::spawnVoice(
    uint8_t note,
    float velocity,
    const Sf2Zone* zone,
    const Sf2SampleHeader* header
) noexcept {
    if (!zone || !header || sfData_->pcmData.empty()) return;

    // 1. Find free voice or steal oldest
    size_t targetIdx = kMaxVoices;
    uint32_t oldestAge = 0;
    size_t oldestIdx = 0;

    for (size_t i = 0; i < kMaxVoices; ++i) {
        if (!voices_[i].active) {
            targetIdx = i;
            break;
        }
        uint32_t age = voiceAgeCounter_ - voices_[i].age;
        if (age >= oldestAge) {
            oldestAge = age;
            oldestIdx = i;
        }
    }

    if (targetIdx == kMaxVoices) {
        targetIdx = oldestIdx; // Voice stealing
    }

    auto& v = voices_[targetIdx];
    v.active = true;
    v.note = note;
    v.velocity = velocity;
    v.zone = zone;
    v.header = header;
    v.age = ++voiceAgeCounter_;
    v.timeSec = 0.0;
    v.isRelease = false;
    v.releaseStartTime = 0.0;
    v.releaseStartGain = 1.0f;

    uint8_t rootKey = zone->rootKeyOverride.value_or(
        header->originalPitch > 0 ? header->originalPitch : 60
    );

    float totalCents = (static_cast<float>(note) - static_cast<float>(rootKey)) * 100.0f +
                       (static_cast<float>(zone->coarseTune) * 100.0f) +
                       static_cast<float>(zone->fineTune) +
                       static_cast<float>(header->pitchCorrection) +
                       (pitchBend_ * 100.0f);

    double baseRatio = std::pow(2.0, static_cast<double>(totalCents) / 1200.0);
    double srRatio = static_cast<double>(header->sampleRate) / (sampleRate_ > 1000.0 ? sampleRate_ : 44100.0);
    v.playbackRate = baseRatio * srRatio;

    // Equal-power stereo panning
    float panNorm = std::clamp(zone->pan, -1.0f, 1.0f);
    float angle = (panNorm + 1.0f) * 0.25f * 3.14159265f;
    v.panL = std::cos(angle);
    v.panR = std::sin(angle);

    v.samplePosition = static_cast<double>(header->startSample);
}

void SoundFontNode::noteOff(uint8_t note) noexcept {
    for (auto& v : voices_) {
        if (v.active && v.note == note && !v.isRelease) {
            v.isRelease = true;
            v.releaseStartTime = v.timeSec;
            v.releaseStartGain = computeEnvelopeGain(v);
        }
    }
}

void SoundFontNode::allNotesOff() noexcept {
    for (auto& v : voices_) {
        if (v.active && !v.isRelease) {
            v.isRelease = true;
            v.releaseStartTime = v.timeSec;
            v.releaseStartGain = computeEnvelopeGain(v);
        }
    }
}

float SoundFontNode::computeEnvelopeGain(const Voice& v) const noexcept {
    if (!v.zone) return 0.0f;
    const auto& z = *v.zone;

    if (v.isRelease) {
        double elapsed = v.timeSec - v.releaseStartTime;
        float relDur = std::max(0.001f, z.volEnvRelease);
        float progress = static_cast<float>(elapsed / relDur);
        if (progress >= 1.0f) return 0.0f;
        return v.releaseStartGain * (1.0f - progress);
    }

    double t = v.timeSec;
    double delayT = z.volEnvDelay;
    double attackT = delayT + z.volEnvAttack;
    double holdT = attackT + z.volEnvHold;
    double decayT = holdT + z.volEnvDecay;

    if (t < delayT) {
        return 0.0f;
    } else if (t < attackT) {
        return (attackT > delayT) ? static_cast<float>((t - delayT) / (attackT - delayT)) : 1.0f;
    } else if (t < holdT) {
        return 1.0f;
    } else if (t < decayT) {
        float decayProgress = static_cast<float>((t - holdT) / (decayT - holdT));
        return 1.0f - (decayProgress * (1.0f - z.volEnvSustain));
    }
    return z.volEnvSustain;
}

void SoundFontNode::handleEvent(const AudioEvent& event) noexcept {
    switch (event.type) {
        case AudioEventType::NoteOn:
            noteOn(event.note, event.velocity);
            break;
        case AudioEventType::NoteOff:
            noteOff(event.note);
            break;
        case AudioEventType::AllNotesOff:
            allNotesOff();
            break;
        case AudioEventType::SetParameter:
            setParameter(event.paramId, event.paramValue);
            break;
        default:
            break;
    }
}

void SoundFontNode::setParameter(uint32_t paramId, float value) noexcept {
    switch (paramId) {
        case 0:
            setPreset(static_cast<uint16_t>(value), currentBankNum_);
            break;
        case 1:
            setPreset(currentPresetNum_, static_cast<uint16_t>(value));
            break;
        case 2:
            setMasterGain(value);
            break;
        case 3:
            setPitchBend(value);
            break;
        default:
            break;
    }
}

float SoundFontNode::getParameter(uint32_t paramId) const noexcept {
    switch (paramId) {
        case 0: return static_cast<float>(currentPresetNum_);
        case 1: return static_cast<float>(currentBankNum_);
        case 2: return getMasterGain();
        case 3: return getPitchBend();
        default: return 0.0f;
    }
}

void SoundFontNode::processBlock(uint32_t numFrames) noexcept {
    float* outL = getOutputBuffer(0, 0);
    float* outR = getOutputBuffer(0, 1);

    if (!outL || !outR) return;

    std::fill_n(outL, numFrames, 0.0f);
    std::fill_n(outR, numFrames, 0.0f);

    if (!enabled_ || !sfData_ || sfData_->pcmData.empty()) return;

    const float* pcm = sfData_->pcmData.data();
    const size_t pcmLen = sfData_->pcmData.size();
    const double dt = 1.0 / (sampleRate_ > 1000.0 ? sampleRate_ : 44100.0);

    for (size_t vIdx = 0; vIdx < kMaxVoices; ++vIdx) {
        auto& v = voices_[vIdx];
        if (!v.active || !v.header || !v.zone) continue;

        const auto& sh = *v.header;
        const auto& zone = *v.zone;

        uint32_t startSample = sh.startSample;
        uint32_t endSample = std::min(sh.endSample, static_cast<uint32_t>(pcmLen));
        if (startSample >= endSample) {
            v.active = false;
            continue;
        }

        bool isLooping = (zone.sampleModes == 1 || zone.sampleModes == 3) &&
                         sh.endLoop > sh.startLoop &&
                         sh.startLoop >= startSample &&
                         sh.endLoop <= endSample;

        uint32_t startLoop = std::clamp(static_cast<uint32_t>(static_cast<int32_t>(sh.startLoop) + zone.startLoopOffset), startSample, endSample);
        uint32_t endLoop = std::clamp(static_cast<uint32_t>(static_cast<int32_t>(sh.endLoop) + zone.endLoopOffset), startLoop, endSample);
        double loopLen = static_cast<double>(endLoop - startLoop);

        float voiceVelScale = v.velocity * masterGain_;

        for (uint32_t f = 0; f < numFrames; ++f) {
            if (isLooping && loopLen > 0.0 && v.samplePosition >= static_cast<double>(endLoop)) {
                v.samplePosition = static_cast<double>(startLoop) +
                                   std::fmod(v.samplePosition - static_cast<double>(startLoop), loopLen);
            }

            if (!isLooping && v.samplePosition >= static_cast<double>(endSample)) {
                v.active = false;
                break;
            }

            size_t idx0 = static_cast<size_t>(v.samplePosition);
            if (idx0 >= pcmLen) {
                v.active = false;
                break;
            }
            size_t idx1 = std::min(idx0 + 1, pcmLen - 1);
            float frac = static_cast<float>(v.samplePosition - static_cast<double>(idx0));

            // Linear interpolation
            float rawSample = (1.0f - frac) * pcm[idx0] + frac * pcm[idx1];

            // Anti-click fade near raw sample boundary for one-shots
            if (!isLooping && v.samplePosition >= static_cast<double>(endSample) - (128.0 * v.playbackRate)) {
                double dist = std::max(0.0, static_cast<double>(endSample) - v.samplePosition);
                float fade = static_cast<float>(dist / (128.0 * v.playbackRate));
                rawSample *= std::clamp(fade, 0.0f, 1.0f);
            }

            // Envelope calculation
            float envGain = computeEnvelopeGain(v);
            if (v.isRelease && envGain <= 1e-4f) {
                v.active = false;
                break;
            }

            float finalSample = rawSample * envGain * voiceVelScale;
            outL[f] += finalSample * v.panL;
            outR[f] += finalSample * v.panR;

            v.samplePosition += v.playbackRate;
            v.timeSec += dt;
        }
    }
}

} // namespace eatsbits::audio
