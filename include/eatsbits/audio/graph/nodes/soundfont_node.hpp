#ifndef EATS_SOUNDFONT_NODE_HPP
#define EATS_SOUNDFONT_NODE_HPP

#include <vector>
#include <string>
#include <memory>
#include <array>
#include "../graph_node.hpp"
#include "../../soundfont/soundfont_decoder.hpp"

namespace eatsbits::audio {

/**
 * Modular Multitimbral SoundFont 2 Sample Player Node for AudioGraph.
 * Provides multi-zone key/velocity splitting, sample loop interpolation,
 * 32-voice polyphony, and per-zone ADSR volume envelopes with ZERO heap allocation
 * during the real-time audio thread execution.
 */
class SoundFontNode : public GraphNode {
public:
    static constexpr size_t kMaxVoices = 32;

    explicit SoundFontNode(std::string name = "SoundFontPlayer");
    ~SoundFontNode() override = default;

    void prepare(double sampleRate, uint32_t maxBlockSize) override;
    void reset() noexcept override;

    // MIDI Voice Control
    void noteOn(uint8_t note, float velocity = 0.8f) noexcept;
    void noteOff(uint8_t note) noexcept;
    void allNotesOff() noexcept;

    // SoundFont Bank Loading & Preset Selection
    bool loadSoundFont(const uint8_t* data, size_t length);
    bool loadSoundFontFile(const std::string& filePath);
    void setSoundFontData(std::shared_ptr<SoundFontData> sfData);
    [[nodiscard]] std::shared_ptr<SoundFontData> getSoundFontData() const noexcept { return sfData_; }

    void setPreset(uint16_t presetNum, uint16_t bankNum = 0);
    [[nodiscard]] uint16_t getPreset() const noexcept { return currentPresetNum_; }
    [[nodiscard]] uint16_t getBank() const noexcept { return currentBankNum_; }
    [[nodiscard]] const std::string& getCurrentPresetName() const noexcept;

    void setMasterGain(float gain) noexcept { masterGain_ = std::max(0.0f, gain); }
    [[nodiscard]] float getMasterGain() const noexcept { return masterGain_; }

    void setPitchBend(float semitones) noexcept { pitchBend_ = std::clamp(semitones, -12.0f, 12.0f); }
    [[nodiscard]] float getPitchBend() const noexcept { return pitchBend_; }

    [[nodiscard]] size_t getActiveVoiceCount() const noexcept;

    // Event & Parameter Handling
    void handleEvent(const AudioEvent& event) noexcept override;
    void setParameter(uint32_t paramId, float value) noexcept override;
    [[nodiscard]] float getParameter(uint32_t paramId) const noexcept;

    // Real-Time Audio Block Processing (ZERO allocations)
    void processBlock(uint32_t numFrames) noexcept override;

private:
    struct Voice {
        bool active{false};
        uint8_t note{60};
        float velocity{0.8f};
        const Sf2Zone* zone{nullptr};
        const Sf2SampleHeader* header{nullptr};

        double samplePosition{0.0};
        double playbackRate{1.0};
        double timeSec{0.0};

        bool isRelease{false};
        double releaseStartTime{0.0};
        float releaseStartGain{1.0f};

        float panL{0.707f};
        float panR{0.707f};
        uint32_t age{0};
    };

    void spawnVoice(uint8_t note, float velocity, const Sf2Zone* zone, const Sf2SampleHeader* header) noexcept;
    float computeEnvelopeGain(const Voice& v) const noexcept;

    std::shared_ptr<SoundFontData> sfData_;
    const Sf2Preset* activePreset_{nullptr};
    std::string currentPresetName_{"None"};

    uint16_t currentPresetNum_{0};
    uint16_t currentBankNum_{0};
    float masterGain_{1.0f};
    float pitchBend_{0.0f};

    std::array<Voice, kMaxVoices> voices_{};
    uint32_t voiceAgeCounter_{0};
};

} // namespace eatsbits::audio

#endif // EATS_SOUNDFONT_NODE_HPP
