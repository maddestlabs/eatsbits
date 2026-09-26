#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "value_edit_dialog.hpp"
#include <functional>
#include <string>

namespace eatsbits::audio {
    class AudioEngine;
}

namespace eatsbits::ui {

/**
 * TransportHeader: Persistent top transport strip.
 * Features brand logo, Play/Stop/Record, Bar:Beat:Tick Nixie LCD readout,
 * BPM & Swing scrubbers, Snap quantize, Loop, Metronome, Master Volume, and Browser button.
 */
class TransportHeader {
public:
    TransportHeader() = default;
    ~TransportHeader() = default;

    void layout(float screenWidth, float height = 48.0f);
    void render(BatchRenderer2D& r, const ThemeTokens& theme, audio::AudioEngine* engine, bool isBrowserOpen);
    bool handlePointer(const PointerEvent& ev, audio::AudioEngine* engine);

    [[nodiscard]] Rect2D getBounds() const noexcept { return bounds_; }
    [[nodiscard]] Rect2D getBpmBounds() const noexcept { return bpmBounds_; }

    std::function<void()> onToggleProjectHub;
    std::function<void()> onToggleBrowser;
    std::function<void()> onTogglePlay;
    std::function<void()> onToggleLoop;
    std::function<void()> onToggleMetronome;
    std::function<void(const ValueEditRequest&)> onOpenValueEdit;

private:
    Rect2D bounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D logoBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D playBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D stopBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D timeDisplayBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D bpmBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D snapBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D loopBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D metroBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D browserBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    bool loopActive_{true};
    bool metroActive_{false};
    float bpm_{128.0f};
};

} // namespace eatsbits::ui
