#ifndef EATS_AUDIO_TO_MIDI_DIALOG_HPP
#define EATS_AUDIO_TO_MIDI_DIALOG_HPP

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "text_field_state.hpp"
#include "eatsbits/audio/audio_to_midi_engine.hpp"

namespace eatsbits::ui {

/**
 * Interactive Audio-to-MIDI Transcription Modal Dialog.
 * Direct C++20 port with full Eatsbeats visual and functional parity.
 *
 * Capabilities:
 * - Load/Audition audio files (WAV, MP3, FLAC) with peak waveform visualization.
 * - Multi-mode transcription: Hybrid DSP (Polyphonic Harmonic), YIN (Monophonic Lead), Percussive (Drums).
 * - Interactive parameter sliders: Onset sensitivity, frame threshold, min duration, velocity sensitivity.
 * - Quantization grid selector and target track assignment.
 * - Seamless automatic chord extraction for direct synchronization with the Chord Track.
 */
class AudioToMidiDialog {
public:
    AudioToMidiDialog();
    ~AudioToMidiDialog() = default;

    void open(const std::optional<audio::DecodedAudioBuffer>& initialBuffer = std::nullopt,
              const std::string& initialName = "");
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);
    bool handleChar(unsigned int codepoint);
    void update(float dt);

    // Audio Buffer Management
    void setAudioBuffer(const audio::DecodedAudioBuffer& buffer, const std::string& fileName);
    bool loadAudioFile(const std::string& filePath);
    [[nodiscard]] const audio::DecodedAudioBuffer& getAudioBuffer() const noexcept { return audioBuffer_; }
    [[nodiscard]] const std::string& getAudioFileName() const noexcept { return audioFileName_; }
    [[nodiscard]] const Rect2D& getBounds() const noexcept { return dialogBounds_; }

    // Options accessors
    [[nodiscard]] const audio::AudioToMidiOptions& getOptions() const noexcept { return options_; }
    void setOptions(const audio::AudioToMidiOptions& opts) noexcept { options_ = opts; }

    [[nodiscard]] bool isCreateNewTrack() const noexcept { return createNewTrack_; }
    void setCreateNewTrack(bool val) noexcept { createNewTrack_ = val; }

    [[nodiscard]] const std::string& getCustomTrackName() const noexcept { return trackNameField_.getText(); }
    void setCustomTrackName(const std::string& name) { trackNameField_.setText(name); }

    [[nodiscard]] bool isExtractChords() const noexcept { return extractChords_; }
    void setExtractChords(bool val) noexcept { extractChords_ = val; }

    [[nodiscard]] bool isAuditioning() const noexcept { return isAuditioning_; }
    void stopAudition() noexcept;

    // Public Callbacks
    std::function<void(const audio::TranscribedMidiTrack& track,
                       bool createNewTrack,
                       const std::string& trackName,
                       bool extractChords)> onTranscriptionComplete;
    std::function<void(const std::vector<float>& samples, uint32_t sampleRate)> onAuditionStart;
    std::function<void()> onAuditionStop;
    std::function<std::string()> onBrowseAudioFile;
    std::function<void()> onClose;

private:
    void runTranscription();
    void renderHeader(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderWaveformSection(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderEngineModeTabs(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderSlidersSection(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderDestinationSection(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderProgressAndActions(BatchRenderer2D& r, const ThemeTokens& theme);

    bool isOpen_{false};
    float screenW_{1280.0f};
    float screenH_{800.0f};
    Rect2D dialogBounds_{0.0f, 0.0f, 740.0f, 570.0f};

    // Sub-rectangles
    Rect2D closeBtnBounds_{};
    Rect2D browseBtnBounds_{};
    Rect2D auditionBtnBounds_{};
    Rect2D waveformBounds_{};
    Rect2D modeHybridBounds_{};
    Rect2D modeYinBounds_{};
    Rect2D modePercussiveBounds_{};
    Rect2D createTrackToggleBounds_{};
    Rect2D trackNameInputBounds_{};
    Rect2D extractChordsToggleBounds_{};
    Rect2D cancelBtnBounds_{};
    Rect2D transcribeBtnBounds_{};

    // Sliders
    Rect2D onsetSliderBounds_{};
    Rect2D frameSliderBounds_{};
    Rect2D durationSliderBounds_{};
    Rect2D velocitySliderBounds_{};
    int activeDragSlider_{-1}; // 0 = onset, 1 = frame, 2 = duration, 3 = velocity

    // Audio & State
    audio::DecodedAudioBuffer audioBuffer_;
    std::string audioFileName_{"No audio file selected"};
    audio::WaveformOverview waveform_;
    audio::AudioToMidiOptions options_{};
    bool createNewTrack_{true};
    bool extractChords_{true};
    TextFieldState trackNameField_;

    // Audition playback
    bool isAuditioning_{false};
    float auditionPlayheadSec_{0.0f};

    // Processing & Progress
    bool isProcessing_{false};
    float progress_{0.0f};
    std::string statusMessage_{"Ready to transcribe"};
    audio::CancellationToken cancellationToken_;
    float lastMouseX_{0.0f};
    float lastMouseY_{0.0f};
};

} // namespace eatsbits::ui

#endif // EATS_AUDIO_TO_MIDI_DIALOG_HPP
