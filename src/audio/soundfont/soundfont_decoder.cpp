#include "eatsbits/audio/soundfont/soundfont_decoder.hpp"

#include <fstream>
#include <cmath>
#include <cstring>
#include <map>
#include <array>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace eatsbits::audio {

namespace {

inline uint16_t readU16LE(const uint8_t* p) noexcept {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline int16_t readS16LE(const uint8_t* p) noexcept {
    return static_cast<int16_t>(readU16LE(p));
}

inline uint32_t readU32LE(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline std::string readFourCC(const uint8_t* p) {
    return std::string(reinterpret_cast<const char*>(p), 4);
}

inline std::string readFixedString(const uint8_t* p, size_t maxLen) {
    size_t len = 0;
    while (len < maxLen && p[len] != 0) {
        ++len;
    }
    // Trim trailing whitespace
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t' || p[len - 1] == '\r' || p[len - 1] == '\n')) {
        --len;
    }
    return std::string(reinterpret_cast<const char*>(p), len);
}

struct Sf2GenMap {
    std::array<int16_t, 65> values{};
    std::array<bool, 65> present{};

    void clear() noexcept {
        present.fill(false);
    }

    void set(uint16_t id, int16_t val) noexcept {
        if (id < 65) {
            values[id] = val;
            present[id] = true;
        }
    }

    [[nodiscard]] bool count(uint16_t id) const noexcept {
        return (id < 65) && present[id];
    }

    [[nodiscard]] int16_t operator[](uint16_t id) const noexcept {
        return (id < 65) ? values[id] : 0;
    }

    void overlay(const Sf2GenMap& other) noexcept {
        for (size_t i = 0; i < 65; ++i) {
            if (other.present[i]) {
                values[i] = other.values[i];
                present[i] = true;
            }
        }
    }
};

const char* const kGeneralMidiNames[128] = {
    "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
    "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
    "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
    "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
    "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
    "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
    "Acoustic Guitar (Nylon)", "Acoustic Guitar (Steel)", "Electric Guitar (Jazz)", "Electric Guitar (Clean)",
    "Electric Guitar (Muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
    "Acoustic Bass", "Electric Bass (Finger)", "Electric Bass (Pick)", "Fretless Bass",
    "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
    "Violin", "Viola", "Cello", "Contrabass",
    "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
    "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
    "Choir Aahs", "Voice Oohs", "Synth Choir", "Orchestra Hit",
    "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
    "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
    "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
    "Oboe", "English Horn", "Bassoon", "Clarinet",
    "Piccolo", "Flute", "Recorder", "Pan Flute",
    "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
    "Lead 1 (Square)", "Lead 2 (Sawtooth)", "Lead 3 (Calliope)", "Lead 4 (Chiff)",
    "Lead 5 (Charang)", "Lead 6 (Voice)", "Lead 7 (Fifths)", "Lead 8 (Bass + Lead)",
    "Pad 1 (New Age)", "Pad 2 (Warm)", "Pad 3 (Polysynth)", "Pad 4 (Choir)",
    "Pad 5 (Bowed)", "Pad 6 (Metallic)", "Pad 7 (Halo)", "Pad 8 (Sweep)",
    "FX 1 (Rain)", "FX 2 (Soundtrack)", "FX 3 (Crystal)", "FX 4 (Atmosphere)",
    "FX 5 (Brightness)", "FX 6 (Goblins)", "FX 7 (Echoes)", "FX 8 (Sci-Fi)",
    "Sitar", "Banjo", "Shamisen", "Koto",
    "Kalimba", "Bagpipe", "Fiddle", "Shanai",
    "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
    "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
    "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
    "Telephone Ring", "Helicopter", "Applause", "Gunshot"
};

} // anonymous namespace

const char* GeneralMidiNames::getInstrumentName(int program) noexcept {
    if (program >= 0 && program < 128) {
        return kGeneralMidiNames[program];
    }
    return "Unknown Instrument";
}

std::string GeneralMidiNames::getPresetDisplayName(int bankNum, int presetNum, const std::string& sf2Name) {
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(3) << presetNum << ": ";

    bool hasCustomName = !sf2Name.empty() && sf2Name != "Untitled";

    if (bankNum == 128 || bankNum == 127) {
        ss << "[Drums] " << (hasCustomName ? sf2Name : "Standard Drum Kit");
        return ss.str();
    }

    if (bankNum > 0) {
        ss << "[Bank " << bankNum << "] ";
    }

    if (hasCustomName) {
        ss << sf2Name;
    } else {
        ss << getInstrumentName(presetNum);
    }
    return ss.str();
}

const Sf2Preset* SoundFontData::findPreset(int presetNum, int bankNum) const noexcept {
    if (bankNum >= 0) {
        auto it = std::lower_bound(presets.begin(), presets.end(), std::make_pair(bankNum, presetNum),
            [](const Sf2Preset& p, const std::pair<int, int>& target) {
                if (p.bankNum != target.first) return p.bankNum < target.first;
                return p.presetNum < target.second;
            });
        if (it != presets.end() && it->bankNum == bankNum && it->presetNum == presetNum) {
            return &(*it);
        }
    }
    for (const auto& p : presets) {
        if (p.presetNum == presetNum) {
            return &p;
        }
    }
    return presets.empty() ? nullptr : &presets.front();
}

const Sf2Zone* SoundFontData::findZone(const Sf2Preset& preset, uint8_t midiNote, uint8_t velocity) const noexcept {
    for (const auto& z : preset.zones) {
        if (midiNote >= z.minKey && midiNote <= z.maxKey &&
            velocity >= z.minVel && velocity <= z.maxVel) {
            return &z;
        }
    }
    return preset.zones.empty() ? nullptr : &preset.zones.front();
}

float SoundFontDecoder::timecentsToSeconds(int16_t timecents) noexcept {
    if (timecents == -32768 || timecents <= -12000) return 0.0f;
    return std::pow(2.0f, static_cast<float>(timecents) / 1200.0f);
}

float SoundFontDecoder::centibelsToGain(int16_t cb) noexcept {
    if (cb <= 0) return 1.0f;
    if (cb >= 1000) return 0.0f;
    return std::pow(10.0f, -static_cast<float>(cb) / 200.0f);
}

std::shared_ptr<SoundFontData> SoundFontDecoder::decodeFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return nullptr;

    auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize < 12) return nullptr;

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        return nullptr;
    }

    return decode(buffer.data(), buffer.size());
}

std::shared_ptr<SoundFontData> SoundFontDecoder::decode(const uint8_t* data, size_t length) {
    if (!data || length < 12) return nullptr;

    // Validate RIFF header
    if (readFourCC(data) != "RIFF") return nullptr;
    if (readFourCC(data + 8) != "sfbk") return nullptr;

    auto result = std::make_shared<SoundFontData>();

    size_t offset = 12;
    size_t sdtaOffset = 0;
    size_t sdtaLength = 0;
    size_t pdtaOffset = 0;
    size_t pdtaLength = 0;

    // First scan top-level LIST chunks
    while (offset + 8 <= length) {
        std::string chunkId = readFourCC(data + offset);
        uint32_t chunkSize = readU32LE(data + offset + 4);

        if (chunkId == "LIST" && offset + 12 <= length) {
            std::string listType = readFourCC(data + offset + 8);
            if (listType == "sdta") {
                sdtaOffset = offset + 12;
                sdtaLength = chunkSize >= 4 ? chunkSize - 4 : 0;
            } else if (listType == "pdta") {
                pdtaOffset = offset + 12;
                pdtaLength = chunkSize >= 4 ? chunkSize - 4 : 0;
            }
        }

        offset += 8 + chunkSize;
        if (chunkSize % 2 != 0) ++offset; // RIFF chunk 2-byte alignment
    }

    // 1. Decode PCM Audio samples from sdta/smpl
    if (sdtaOffset > 0 && sdtaOffset + sdtaLength <= length) {
        size_t sPos = sdtaOffset;
        while (sPos + 8 <= sdtaOffset + sdtaLength) {
            std::string subId = readFourCC(data + sPos);
            uint32_t subSize = readU32LE(data + sPos + 4);

            if (subId == "smpl") {
                size_t pcmStart = sPos + 8;
                size_t numSamples = subSize / 2;
                if (pcmStart + numSamples * 2 <= length) {
                    result->pcmData.resize(numSamples);
                    const uint8_t* pcmPtr = data + pcmStart;
                    for (size_t i = 0; i < numSamples; ++i) {
                        int16_t sample16 = readS16LE(pcmPtr + i * 2);
                        result->pcmData[i] = static_cast<float>(sample16) / 32768.0f;
                    }
                }
                break;
            }

            sPos += 8 + subSize;
            if (subSize % 2 != 0) ++sPos;
        }
    }

    // 2. Decode Preset & Generator Data from pdta
    if (pdtaOffset > 0 && pdtaOffset + pdtaLength <= length) {
        size_t pPos = pdtaOffset;
        size_t pEnd = pdtaOffset + pdtaLength;

        struct RawPreset {
            std::string name;
            uint16_t presetNum{0};
            uint16_t bankNum{0};
            uint16_t bagIdx{0};
        };

        struct RawInst {
            std::string name;
            uint16_t bagIdx{0};
        };

        struct RawGen {
            uint16_t genId{0};
            int16_t val{0};
        };

        std::vector<RawPreset> rawPresets;
        std::vector<uint16_t> rawPresetBags;
        std::vector<RawGen> rawPresetGens;

        std::vector<RawInst> rawInsts;
        std::vector<uint16_t> rawInstBags;
        std::vector<RawGen> rawInstGens;

        while (pPos + 8 <= pEnd) {
            std::string subId = readFourCC(data + pPos);
            uint32_t subSize = readU32LE(data + pPos + 4);
            size_t dataStart = pPos + 8;

            if (dataStart + subSize > length) break;

            if (subId == "shdr") {
                size_t count = subSize / 46;
                result->sampleHeaders.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    const uint8_t* b = data + dataStart + i * 46;
                    Sf2SampleHeader sh;
                    sh.name = readFixedString(b, 20);
                    sh.startSample = readU32LE(b + 20);
                    sh.endSample = readU32LE(b + 24);
                    sh.startLoop = readU32LE(b + 28);
                    sh.endLoop = readU32LE(b + 32);
                    sh.sampleRate = readU32LE(b + 36);
                    sh.originalPitch = b[40];
                    sh.pitchCorrection = static_cast<int8_t>(b[41]);
                    sh.sampleType = readU16LE(b + 44);
                    result->sampleHeaders.push_back(sh);
                }
            } else if (subId == "phdr") {
                size_t count = subSize / 38;
                rawPresets.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    const uint8_t* b = data + dataStart + i * 38;
                    RawPreset rp;
                    rp.name = readFixedString(b, 20);
                    rp.presetNum = readU16LE(b + 20);
                    rp.bankNum = readU16LE(b + 22);
                    rp.bagIdx = readU16LE(b + 24);
                    rawPresets.push_back(rp);
                }
            } else if (subId == "pbag") {
                size_t count = subSize / 4;
                rawPresetBags.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    const uint8_t* b = data + dataStart + i * 4;
                    rawPresetBags.push_back(readU16LE(b));
                }
            } else if (subId == "pgen") {
                size_t count = subSize / 4;
                rawPresetGens.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    const uint8_t* b = data + dataStart + i * 4;
                    rawPresetGens.push_back({ readU16LE(b), readS16LE(b + 2) });
                }
            } else if (subId == "inst") {
                size_t count = subSize / 22;
                rawInsts.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    const uint8_t* b = data + dataStart + i * 22;
                    RawInst ri;
                    ri.name = readFixedString(b, 20);
                    ri.bagIdx = readU16LE(b + 20);
                    rawInsts.push_back(ri);
                }
            } else if (subId == "ibag") {
                size_t count = subSize / 4;
                rawInstBags.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    const uint8_t* b = data + dataStart + i * 4;
                    rawInstBags.push_back(readU16LE(b));
                }
            } else if (subId == "igen") {
                size_t count = subSize / 4;
                rawInstGens.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    const uint8_t* b = data + dataStart + i * 4;
                    rawInstGens.push_back({ readU16LE(b), readS16LE(b + 2) });
                }
            }

            pPos += 8 + subSize;
            if (subSize % 2 != 0) ++pPos;
        }

        // 3. Assemble Presets & Zones from SoundFont Generator hierarchy (SoundFont 2.04)
        if (!rawPresets.empty()) {
            for (size_t i = 0; i + 1 < rawPresets.size(); ++i) {
                const auto& rp = rawPresets[i];
                size_t pBagStart = rp.bagIdx;
                size_t pBagEnd = rawPresets[i + 1].bagIdx;

                Sf2Preset preset;
                preset.name = rp.name;
                preset.presetNum = rp.presetNum;
                preset.bankNum = rp.bankNum;

                Sf2GenMap globalPresetGens;

                for (size_t pb = pBagStart; pb < pBagEnd && pb + 1 < rawPresetBags.size(); ++pb) {
                    size_t pGenStart = rawPresetBags[pb];
                    size_t pGenEnd = rawPresetBags[pb + 1];

                    Sf2GenMap pgenMap;
                    for (size_t g = pGenStart; g < pGenEnd && g < rawPresetGens.size(); ++g) {
                        pgenMap.set(rawPresetGens[g].genId, rawPresetGens[g].val);
                    }

                    // If no instrument operator (genId 41), this is a global preset generator
                    if (!pgenMap.count(41)) {
                        globalPresetGens.overlay(pgenMap);
                        continue;
                    }

                    // Overlay global preset gens with local preset zone gens
                    Sf2GenMap effectivePresetGens = globalPresetGens;
                    effectivePresetGens.overlay(pgenMap);

                    uint8_t pMinKey = 0, pMaxKey = 127;
                    if (effectivePresetGens.count(43)) {
                        uint16_t val = static_cast<uint16_t>(effectivePresetGens[43]);
                        pMinKey = static_cast<uint8_t>(val & 0xFF);
                        pMaxKey = static_cast<uint8_t>((val >> 8) & 0xFF);
                    }

                    uint8_t pMinVel = 0, pMaxVel = 127;
                    if (effectivePresetGens.count(44)) {
                        uint16_t val = static_cast<uint16_t>(effectivePresetGens[44]);
                        pMinVel = static_cast<uint8_t>(val & 0xFF);
                        pMaxVel = static_cast<uint8_t>((val >> 8) & 0xFF);
                    }

                    size_t instIdx = static_cast<size_t>(pgenMap[41]);
                    if (instIdx + 1 < rawInsts.size()) {
                        const auto& inst = rawInsts[instIdx];
                        size_t iBagStart = inst.bagIdx;
                        size_t iBagEnd = rawInsts[instIdx + 1].bagIdx;

                        Sf2GenMap globalInstGens;

                        for (size_t ib = iBagStart; ib < iBagEnd && ib + 1 < rawInstBags.size(); ++ib) {
                            size_t iGenStart = rawInstBags[ib];
                            size_t iGenEnd = rawInstBags[ib + 1];

                            Sf2GenMap igenMap;
                            for (size_t g = iGenStart; g < iGenEnd && g < rawInstGens.size(); ++g) {
                                igenMap.set(rawInstGens[g].genId, rawInstGens[g].val);
                            }

                            // If no sampleID operator (genId 53), treat as global instrument generator
                            if (!igenMap.count(53)) {
                                globalInstGens.overlay(igenMap);
                                continue;
                            }

                            int32_t sampleHeaderIdx = static_cast<int32_t>(igenMap[53]);
                            if (sampleHeaderIdx < 0 || static_cast<size_t>(sampleHeaderIdx) >= result->sampleHeaders.size()) {
                                continue;
                            }

                            Sf2GenMap effectiveInstGens = globalInstGens;
                            effectiveInstGens.overlay(igenMap);

                            uint8_t iMinKey = 0, iMaxKey = 127;
                            if (effectiveInstGens.count(43)) {
                                uint16_t val = static_cast<uint16_t>(effectiveInstGens[43]);
                                iMinKey = static_cast<uint8_t>(val & 0xFF);
                                iMaxKey = static_cast<uint8_t>((val >> 8) & 0xFF);
                            }

                            uint8_t iMinVel = 0, iMaxVel = 127;
                            if (effectiveInstGens.count(44)) {
                                uint16_t val = static_cast<uint16_t>(effectiveInstGens[44]);
                                iMinVel = static_cast<uint8_t>(val & 0xFF);
                                iMaxVel = static_cast<uint8_t>((val >> 8) & 0xFF);
                            }

                            uint8_t minKey = std::max(pMinKey, iMinKey);
                            uint8_t maxKey = std::min(pMaxKey, iMaxKey);
                            if (minKey > maxKey) continue;

                            uint8_t minVel = std::max(pMinVel, iMinVel);
                            uint8_t maxVel = std::min(pMaxVel, iMaxVel);
                            if (minVel > maxVel) continue;

                            int16_t coarseTune = 0;
                            if (effectiveInstGens.count(51)) coarseTune += effectiveInstGens[51];
                            if (effectivePresetGens.count(51)) coarseTune += effectivePresetGens[51];

                            int16_t fineTune = 0;
                            if (effectiveInstGens.count(52)) fineTune += effectiveInstGens[52];
                            if (effectivePresetGens.count(52)) fineTune += effectivePresetGens[52];

                            std::optional<uint8_t> rootKeyOverride;
                            if (effectiveInstGens.count(58) && effectiveInstGens[58] > 0) {
                                rootKeyOverride = static_cast<uint8_t>(effectiveInstGens[58]);
                            } else if (effectivePresetGens.count(58) && effectivePresetGens[58] > 0) {
                                rootKeyOverride = static_cast<uint8_t>(effectivePresetGens[58]);
                            }

                            float pan = 0.0f;
                            if (effectiveInstGens.count(17)) pan += static_cast<float>(effectiveInstGens[17]) / 500.0f;
                            if (effectivePresetGens.count(17)) pan += static_cast<float>(effectivePresetGens[17]) / 500.0f;
                            pan = std::clamp(pan, -1.0f, 1.0f);

                            uint16_t sampleModes = effectiveInstGens.count(54) ? static_cast<uint16_t>(effectiveInstGens[54]) : 0;

                            int32_t startLoopOffset = 0;
                            if (effectiveInstGens.count(2)) startLoopOffset += effectiveInstGens[2];
                            if (effectiveInstGens.count(45)) startLoopOffset += effectiveInstGens[45] * 32768;

                            int32_t endLoopOffset = 0;
                            if (effectiveInstGens.count(3)) endLoopOffset += effectiveInstGens[3];
                            if (effectiveInstGens.count(50)) endLoopOffset += effectiveInstGens[50] * 32768;

                            // Volume Envelope generators (33..38)
                            int16_t delayVal = (effectiveInstGens.count(33) ? effectiveInstGens[33] : -32768) +
                                               (effectivePresetGens.count(33) ? effectivePresetGens[33] : 0);
                            int16_t attackVal = (effectiveInstGens.count(34) ? effectiveInstGens[34] : -32768) +
                                                (effectivePresetGens.count(34) ? effectivePresetGens[34] : 0);
                            int16_t holdVal = (effectiveInstGens.count(35) ? effectiveInstGens[35] : -32768) +
                                              (effectivePresetGens.count(35) ? effectivePresetGens[35] : 0);
                            int16_t decayVal = (effectiveInstGens.count(36) ? effectiveInstGens[36] : -32768) +
                                               (effectivePresetGens.count(36) ? effectivePresetGens[36] : 0);
                            int16_t sustainVal = (effectiveInstGens.count(37) ? effectiveInstGens[37] : 0) +
                                                 (effectivePresetGens.count(37) ? effectivePresetGens[37] : 0);
                            int16_t releaseVal = (effectiveInstGens.count(38) ? effectiveInstGens[38] : -32768) +
                                                 (effectivePresetGens.count(38) ? effectivePresetGens[38] : 0);

                            Sf2Zone zone;
                            zone.minKey = minKey;
                            zone.maxKey = maxKey;
                            zone.minVel = minVel;
                            zone.maxVel = maxVel;
                            zone.sampleHeaderIdx = sampleHeaderIdx;
                            zone.rootKeyOverride = rootKeyOverride;
                            zone.coarseTune = coarseTune;
                            zone.fineTune = fineTune;
                            zone.pan = pan;
                            zone.sampleModes = sampleModes;
                            zone.startLoopOffset = startLoopOffset;
                            zone.endLoopOffset = endLoopOffset;
                            zone.volEnvDelay = timecentsToSeconds(delayVal);
                            zone.volEnvAttack = std::max(0.001f, timecentsToSeconds(attackVal));
                            zone.volEnvHold = timecentsToSeconds(holdVal);
                            zone.volEnvDecay = std::max(0.001f, timecentsToSeconds(decayVal));
                            zone.volEnvSustain = centibelsToGain(sustainVal);
                            zone.volEnvRelease = std::max(0.01f, timecentsToSeconds(releaseVal));

                            preset.zones.push_back(zone);
                        }
                    }
                }

                // Fallback zone assignment if preset had no explicit generator links
                if (preset.zones.empty()) {
                    for (size_t shIdx = 0; shIdx < result->sampleHeaders.size(); ++shIdx) {
                        const auto& sh = result->sampleHeaders[shIdx];
                        if (sh.endSample > sh.startSample && sh.startSample < result->pcmData.size()) {
                            Sf2Zone zone;
                            zone.minKey = 0;
                            zone.maxKey = 127;
                            zone.minVel = 0;
                            zone.maxVel = 127;
                            zone.sampleHeaderIdx = static_cast<int32_t>(shIdx);
                            zone.rootKeyOverride = sh.originalPitch > 0 ? sh.originalPitch : 60;
                            preset.zones.push_back(zone);
                        }
                    }
                }

                result->presets.push_back(preset);
            }
        }
    }

    // Sort presets by bank then preset number
    std::sort(result->presets.begin(), result->presets.end(), [](const Sf2Preset& a, const Sf2Preset& b) {
        if (a.bankNum != b.bankNum) return a.bankNum < b.bankNum;
        return a.presetNum < b.presetNum;
    });

    if (!result->presets.empty()) {
        result->fontName = result->presets.front().name;
    }

    return result;
}

} // namespace eatsbits::audio
