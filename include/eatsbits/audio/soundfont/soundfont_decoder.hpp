#ifndef EATS_SOUNDFONT_DECODER_HPP
#define EATS_SOUNDFONT_DECODER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <algorithm>

namespace eatsbits::audio {

/// SoundFont sample header describing a PCM slice in the sample pool.
struct Sf2SampleHeader {
    std::string name;
    uint32_t startSample{0};
    uint32_t endSample{0};
    uint32_t startLoop{0};
    uint32_t endLoop{0};
    uint32_t sampleRate{44100};
    uint8_t originalPitch{60};
    int8_t pitchCorrection{0};
    uint16_t sampleType{1}; // 1 = monoSample, 2 = rightSample, 4 = leftSample
};

/// SoundFont key-and-velocity split zone with synthesis generators.
struct Sf2Zone {
    uint8_t minKey{0};
    uint8_t maxKey{127};
    uint8_t minVel{0};
    uint8_t maxVel{127};
    int32_t sampleHeaderIdx{-1};
    std::optional<uint8_t> rootKeyOverride;
    int16_t coarseTune{0};      // Semitones
    int16_t fineTune{0};        // Cents
    float pan{0.0f};            // -1.0 (Left) to +1.0 (Right)
    uint16_t sampleModes{0};    // 0 = no loop, 1 = loop, 3 = loop while key pressed
    int32_t startLoopOffset{0};
    int32_t endLoopOffset{0};

    // TinySoundFont / SF2 Volume Envelope (seconds and linear gain)
    float volEnvDelay{0.0f};    // Seconds
    float volEnvAttack{0.001f}; // Seconds
    float volEnvHold{0.0f};     // Seconds
    float volEnvDecay{0.0f};    // Seconds
    float volEnvSustain{1.0f};  // Linear gain (0.0 to 1.0)
    float volEnvRelease{0.1f};  // Seconds
};

/// SoundFont preset instrument definition (e.g. Bank 0, Program 0: Grand Piano).
struct Sf2Preset {
    std::string name;
    uint16_t presetNum{0};
    uint16_t bankNum{0};
    std::vector<Sf2Zone> zones;
};

/// Complete decoded SoundFont data bank in memory.
class SoundFontData {
public:
    std::string fontName{"SoundFont Bank"};
    std::vector<float> pcmData; // 32-bit normalized float [-1.0f, +1.0f]
    std::vector<Sf2SampleHeader> sampleHeaders;
    std::vector<Sf2Preset> presets;

    [[nodiscard]] const Sf2Preset* findPreset(int presetNum, int bankNum = -1) const noexcept;
    [[nodiscard]] const Sf2Zone* findZone(const Sf2Preset& preset, uint8_t midiNote, uint8_t velocity = 64) const noexcept;
    [[nodiscard]] size_t getNumPresets() const noexcept { return presets.size(); }
    [[nodiscard]] size_t getNumSamples() const noexcept { return sampleHeaders.size(); }
};

/// General MIDI Program names reference table.
class GeneralMidiNames {
public:
    [[nodiscard]] static const char* getInstrumentName(int program) noexcept;
    [[nodiscard]] static std::string getPresetDisplayName(int bankNum, int presetNum, const std::string& sf2Name);
};

/**
 * SoundFont 2 (SF2) RIFF Parser and Decoder.
 * Parses `.sf2` binary data streams containing Hydrag chunks (`PHDR`, `PBAG`, `PMOD`,
 * `PGEN`, `INST`, `IBAG`, `IMOD`, `IGEN`, `SHDR`) and 16-bit PCM sound pools.
 */
class SoundFontDecoder {
public:
    /// Decodes a SoundFont 2 from raw bytes in memory.
    [[nodiscard]] static std::shared_ptr<SoundFontData> decode(const uint8_t* data, size_t length);

    /// Loads and decodes a SoundFont 2 file from disk.
    [[nodiscard]] static std::shared_ptr<SoundFontData> decodeFile(const std::string& filePath);

    /// Converts SF2 timecents (-32768 to +12000) to seconds.
    [[nodiscard]] static float timecentsToSeconds(int16_t timecents) noexcept;

    /// Converts SF2 centibels attenuation (0 to 1000) to linear gain (1.0 to 0.0).
    [[nodiscard]] static float centibelsToGain(int16_t cb) noexcept;
};

} // namespace eatsbits::audio

#endif // EATS_SOUNDFONT_DECODER_HPP
