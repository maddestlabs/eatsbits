#pragma once

#include "eatsbits/presenter/telemetry_presenter.hpp"
#include "eatsbits/audio/audio_engine.hpp"

namespace eatsbits::tui {

// Unify TelemetrySnapshot with headless Presenter core
using TelemetrySnapshot = eatsbits::presenter::TelemetrySnapshot;

/**
 * @brief TUI Audio Telemetry Bridge adapter.
 * Wraps headless TelemetryPresenter to drive terminal VU meters, Braille scopes,
 * and 16-band ASCII spectrum visualizers with zero terminal dependencies.
 */
class AudioTelemetryBridge {
public:
    explicit AudioTelemetryBridge(size_t scopeCapacity = 256)
        : presenter_(scopeCapacity) {}
    ~AudioTelemetryBridge() = default;

    void update(audio::AudioEngine& engine, float dt = 0.016666f) {
        presenter_.update(engine, dt);
    }

    [[nodiscard]] const TelemetrySnapshot& getSnapshot() const noexcept {
        return presenter_.getSnapshot();
    }

    [[nodiscard]] bool isSettled() const noexcept {
        return presenter_.isSettled();
    }

    [[nodiscard]] bool isDirty() const noexcept {
        return presenter_.isDirty();
    }

    void clearDirty() noexcept {
        presenter_.clearDirty();
    }

    [[nodiscard]] eatsbits::presenter::TelemetryPresenter& getPresenter() noexcept {
        return presenter_;
    }

    [[nodiscard]] const eatsbits::presenter::TelemetryPresenter& getPresenter() const noexcept {
        return presenter_;
    }

private:
    eatsbits::presenter::TelemetryPresenter presenter_;
};

} // namespace eatsbits::tui
