#pragma once

#include "view_base.hpp"
#include "../widgets/plugin_search_dialog.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <algorithm>

#include "eatsbits/theory/chord_model.hpp"

namespace eatsbits::ui {

using ChordFollowMode = eatsbits::theory::ChordFollowMode;

struct TrackMidiFxData {
    bool arpEnabled{true};
    int arpPattern{0};    // 0: Up, 1: Down, 2: UpDown, 3: Random, 4: Chord
    float arpRate{1.0f};  // 1.0 = 16th, 0.5 = 8th, 2.0 = 32nd
    int arpOctaves{2};
    float arpGate{0.85f};
    float arpSwing{0.0f};

    bool scaleSnapEnabled{true};
    int scaleRoot{0};     // 0 = C
    bool scaleMinor{true};

    bool humanizeEnabled{true};
    float humanizeTiming{0.15f};    // ±15ms
    float humanizeVelocity{0.20f};  // ±20%
};

struct TrackAudioFxData {
    bool delayEnabled{true};
    float delayTime{0.375f};
    float delayFeedback{0.45f};
    float delayMix{0.30f};

    bool chorusEnabled{false};
    float chorusRate{0.5f};
    float chorusDepth{0.4f};
    float chorusMix{0.35f};

    bool eqEnabled{true};
    float eqLow{0.6f};
    float eqMid{0.5f};
    float eqHigh{0.5f};

    bool compEnabled{true};
    float compThreshold{0.7f};
    float compRatio{0.4f};
    float compGain{0.5f};

    bool convolverEnabled{true};
    int convolverPreset{0}; // 0: Stone Cathedral, 1: Vintage Plate, 2: Warm Hall, etc.
    float convolverMix{0.35f};
};

struct InspectorKnobDef {
    std::string name;
    std::string label;
    float value{0.5f};
    std::string display;
};

struct InspectorTrackChannel {
    std::string name{"303 Acid Bass"};
    std::string type{"SYNTH"};
    float r{1.0f}, g{0.55f}, b{0.0f};
    float volume{0.85f};
    float pan{0.0f};
    bool mute{false};
    bool solo{false};
    bool freeze{false};
    std::string instrument{"Roland TB-303"};
    std::string instrumentEngine{"tb303"}; // "tb303", "tr808", "tr909", "dx7", "snes", "c64", "convolver", "piano"
    ChordFollowMode chordFollowMode{ChordFollowMode::Off};
    std::vector<InspectorKnobDef> knobs;
    TrackMidiFxData midiFx;
    TrackAudioFxData audioFx;
};

/**
 * TrackInspectorView: Channel hardware faceplate, mixer settings & audio/MIDI FX rack.
 * Full Eatsbeats parity:
 * 1. Top Track Navigation Ribbon with quick channel switching
 * 2. Track Identity Header Card (accent pill, title, subtitle, 8 color swatches, [CODE], MUTE, SOLO, FREEZE)
 * 3. Channel Mixer Settings (continuous horizontal volume & pan sliders with exact readouts)
 * 4. Dynamic Instrument Hardware Faceplate (presets, [CHANGE INSTRUMENT], authentic skeuomorphic chassis,
 *    knurled rotary dials with pointer needles, and live real-time CRT audio oscilloscope)
 * 5. Harmonic Chord Track Follow Settings (interactive mode chips, bake to MIDI action, dynamic descriptions)
 * 6. MIDI FX Pipeline Rack (Arpeggiator, Scale Snap, Humanize with interactive toggles and dials)
 * 7. Audio FX Insert Rack (Delay, Chorus, 5-Band EQ, Compressor, Convolver with interactive dials and FLAT reset)
 * 8. Reusable PluginSearchDialog modal overlay for instant instrument and FX swaps
 */
class TrackInspectorView : public ViewBase {
public:
    TrackInspectorView();
    ~TrackInspectorView() override = default;

    void layout(const Rect2D& bounds, const ViewContext& ctx) override;
    void render(const ViewContext& ctx) override;
    bool handlePointer(const PointerEvent& ev, const ViewContext& ctx) override;
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) override;

    // Track Navigation & Accessors
    void setActiveTrack(uint32_t idx) noexcept;
    [[nodiscard]] uint32_t getActiveTrack() const noexcept { return selectedTrackIndex_; }
    [[nodiscard]] size_t getTrackCount() const noexcept { return tracks_.size(); }
    [[nodiscard]] const InspectorTrackChannel& getTrack(size_t idx) const;
    [[nodiscard]] InspectorTrackChannel& getTrack(size_t idx);
    [[nodiscard]] const std::vector<InspectorTrackChannel>& getTracks() const noexcept { return tracks_; }
    [[nodiscard]] std::vector<InspectorTrackChannel>& getTracks() noexcept { return tracks_; }

    // Live Oscilloscope Audio Buffer
    void setAudioScopeBuffer(const float* buffer, size_t count);

    // Preset & Preset Counter
    void setPresetInfo(size_t activePresetIdx, size_t totalPresets, const std::string& presetTitle, const std::string& presetSubtitle);

    // Reusable Plugin Search Dialog
    [[nodiscard]] PluginSearchDialog& getPluginSearchDialog() noexcept { return pluginDialog_; }
    [[nodiscard]] const PluginSearchDialog& getPluginSearchDialog() const noexcept { return pluginDialog_; }

    // Synchronization from Window
    void syncFromWindow(
        const std::vector<std::string>& trackNames,
        const std::vector<float>& trackVols,
        const std::vector<float>& trackPans,
        const std::vector<bool>& trackMutes,
        const std::vector<bool>& trackSolos,
        const std::vector<bool>& trackFreezes,
        const std::vector<Color>& trackColors,
        uint32_t selectedTrackIdx,
        const std::string& activePresetTitle,
        const std::string& activePresetSubtitle,
        size_t activePresetIdx,
        size_t totalPresets,
        float scrollY
    );

    // Callbacks for Window & Audio Engine synchronization
    std::function<void(uint32_t trackIdx)> onTrackSelected;
    std::function<void(uint32_t trackIdx, float vol)> onVolumeChanged;
    std::function<void(uint32_t trackIdx, float pan)> onPanChanged;
    std::function<void(uint32_t trackIdx, bool mute)> onMuteToggled;
    std::function<void(uint32_t trackIdx, bool solo)> onSoloToggled;
    std::function<void(uint32_t trackIdx, bool freeze)> onFreezeToggled;
    std::function<void(uint32_t trackIdx, float r, float g, float b)> onColorChanged;
    std::function<void(uint32_t trackIdx)> onOpenCodeEditor;
    std::function<void(uint32_t trackIdx)> onChangeInstrument;
    std::function<void()> onPrevPreset;
    std::function<void()> onNextPreset;
    std::function<void(uint32_t trackIdx)> onAddMidiFx;
    std::function<void(uint32_t trackIdx)> onAddAudioFx;
    std::function<void(uint32_t trackIdx, ChordFollowMode mode)> onChordFollowChanged;
    std::function<void(uint32_t trackIdx)> onBakeChords;
    std::function<void(uint32_t trackIdx, const std::string& paramName, float normVal)> onParamChanged;
    std::function<void(uint32_t trackIdx, const std::string& paramName, float normVal)> onMidiFxParamChanged;
    std::function<void(uint32_t trackIdx, const std::string& paramName, float normVal)> onAudioFxParamChanged;
    std::function<void(float scrollY)> onScrollChanged;

    [[nodiscard]] float getScrollY() const noexcept { return scrollY_; }
    void setScrollY(float y) noexcept { scrollY_ = std::max(0.0f, y); }

private:
    void initDefaultTracks();
    void syncKnobsForTrack(InspectorTrackChannel& trk);

    // Sub-section layout and rendering
    void renderTrackSelectorRibbon(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderHeaderCard(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderMixerControlsCard(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderDynamicFaceplateCard(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderChordFollowCard(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderMidiFxCard(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderAudioFxCard(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderScrollbar(BatchRenderer2D& r, const ThemeTokens& theme);

    // Internal Drag Modes
    enum class DragMode {
        None,
        VolumeSlider,
        PanSlider,
        HardwareKnob,
        MidiFxKnob,
        AudioFxKnob,
        Scrollbar
    } dragMode_{DragMode::None};

    uint32_t selectedTrackIndex_{0};
    std::vector<InspectorTrackChannel> tracks_;

    float scrollY_{0.0f};
    float totalContentHeight_{850.0f};
    bool needScrollbar_{false};

    // Sub-section bounding rects
    Rect2D trackRibbonBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D headerCardBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D mixerCardBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D faceplateBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D chordFollowBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D midiFxBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D audioFxBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D scrollbarBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Oscilloscope buffer
    float scopeBuffer_[256]{0.0f};

    // Preset info
    size_t activePresetIdx_{0};
    size_t totalPresets_{156};
    std::string presetTitle_{"TB-303 Acid Bassline"};
    std::string presetSubtitle_{"Diode Ladder Synthesizer"};

    // Active drag interaction state
    float dragStartY_{0.0f};
    float dragStartX_{0.0f};
    float dragStartVal_{0.0f};
    float dragStartScrollY_{0.0f};
    int activeKnobIndex_{-1};
    std::string activeParamName_{""};

    // Embedded PluginSearchDialog
    PluginSearchDialog pluginDialog_;
};

} // namespace eatsbits::ui
