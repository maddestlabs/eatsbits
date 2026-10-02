#ifndef EATS_HEADLESS_VIDEO_EXPORTER_HPP
#define EATS_HEADLESS_VIDEO_EXPORTER_HPP

#include "eatsbits/presenter/frame_time_context.hpp"
#include "eatsbits/presenter/telemetry_presenter.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace eatsbits::export_engine {

/**
 * @brief Configuration parameters for non-realtime headless video and visualizer export.
 */
struct VideoExportConfig {
    uint32_t width{1280};
    uint32_t height{720};
    uint32_t fps{60};
    double durationSeconds{5.0};
    uint32_t sampleRate{44100};
    bool renderWaveforms{true};
    bool renderSpectrum{true};
    bool renderMeters{true};
    bool renderTimeline{true};
    std::string outputPath{}; // Optional output path (.ppm for frame dump or raw binary stream)
};

/**
 * @brief Progress status reported during deterministic offline export.
 */
struct VideoExportProgress {
    uint64_t currentFrame{0};
    uint64_t totalFrames{0};
    double currentSongTime{0.0};
    double percentComplete{0.0};
    float peakL{0.0f};
    float peakR{0.0f};
};

using FrameRenderCallback = std::function<void(const VideoExportProgress&, const uint32_t* rgbaBuffer)>;

/**
 * @brief Headless Deterministic Audio-Visual Video Exporter.
 * Implements strict lockstep offline stepping (Rule 1: Virtual FrameTimeContext,
 * Rule 2: Decoupled Viewport Dimensions, Rule 3: Lockstep Audio-Visual Stepping).
 */
class HeadlessVideoExporter {
public:
    HeadlessVideoExporter();
    ~HeadlessVideoExporter();

    /**
     * @brief Executes the deterministic offline export loop.
     * @param engine Reference to AudioEngine to step in lockstep.
     * @param config Export resolution, framerate, duration, and visualizer flags.
     * @param frameCallback Invoked for every rendered frame with the framebuffer pointer.
     * @param cancelCheck Optional callback returning true to cancel export prematurely.
     * @return true if export finished successfully, false if cancelled or failed.
     */
    bool render(audio::AudioEngine& engine,
                const VideoExportConfig& config,
                FrameRenderCallback frameCallback = nullptr,
                std::function<bool()> cancelCheck = nullptr);

    /**
     * @brief Helper to write an RGBA framebuffer as a standard P6 PPM image file.
     */
    static bool writePpmImage(const std::string& path, uint32_t width, uint32_t height, const uint32_t* rgbaBuffer);

    [[nodiscard]] const std::vector<uint32_t>& getLastFrame() const noexcept { return lastFrameBuffer_; }
    [[nodiscard]] uint64_t getRenderedFrameCount() const noexcept { return renderedFrameCount_; }

private:
    void renderVisualScene(ui::BatchRenderer2D& renderer,
                           const presenter::TelemetryPresenter& telemetry,
                           const FrameTimeContext& time,
                           const VideoExportConfig& config);

    std::vector<uint32_t> lastFrameBuffer_;
    uint64_t renderedFrameCount_{0};
};

} // namespace eatsbits::export_engine

#endif // EATS_HEADLESS_VIDEO_EXPORTER_HPP
