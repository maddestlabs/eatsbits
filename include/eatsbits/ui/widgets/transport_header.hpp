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

namespace eatsbits::ui::clay {
    class ClayContext;
}

namespace eatsbits::ui {

/**
 * TransportHeader: Persistent top transport strip.
 * Features brand logo, Play/Stop/Record, Bar:Beat:Tick Nixie LCD readout,
 * BPM & Swing scrubbers, Snap quantize, Loop, Metronome, Master Volume, and Browser button.
 */
class TransportHeader {
public:
    TransportHeader();
    ~TransportHeader();
    TransportHeader(const TransportHeader&) = delete;
    TransportHeader& operator=(const TransportHeader&) = delete;
    TransportHeader(TransportHeader&&) noexcept;
    TransportHeader& operator=(TransportHeader&&) noexcept;

    void declareLayout(clay::ClayContext& ctx, float screenWidth, float height = 48.0f);
    void layout(float screenWidth, float height = 48.0f, clay::ClayContext* ctx = nullptr);
    void render(BatchRenderer2D& r, const ThemeTokens& theme, audio::AudioEngine* engine, bool isBrowserOpen);
    bool handlePointer(const PointerEvent& ev, audio::AudioEngine* engine);

    [[nodiscard]] Rect2D getBounds() const noexcept { return bounds_; }
    [[nodiscard]] Rect2D getBpmBounds() const noexcept { return bpmBounds_; }
    [[nodiscard]] std::string getTooltip(float x, float y) const noexcept;
    void setHoverPosition(float x, float y) noexcept { hoverX_ = x; hoverY_ = y; }
    void setLoopActive(bool active) noexcept { loopActive_ = active; }
    [[nodiscard]] bool isLoopActive() const noexcept { return loopActive_; }

    std::function<void()> onToggleProjectHub;
    std::function<void()> onToggleBrowser;
    std::function<void()> onTogglePlay;
    std::function<void()> onStop;
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
    float hoverX_{-1.0f};
    float hoverY_{-1.0f};
};

} // namespace eatsbits::ui
