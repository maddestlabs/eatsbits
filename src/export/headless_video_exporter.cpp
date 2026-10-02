#include "eatsbits/export/headless_video_exporter.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace eatsbits::export_engine {

HeadlessVideoExporter::HeadlessVideoExporter() = default;
HeadlessVideoExporter::~HeadlessVideoExporter() = default;

bool HeadlessVideoExporter::writePpmImage(const std::string& path, uint32_t width, uint32_t height, const uint32_t* rgbaBuffer) {
    if (!rgbaBuffer || width == 0 || height == 0 || path.empty()) {
        return false;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    // Standard P6 Binary PPM Header
    out << "P6\n" << width << " " << height << "\n255\n";

    std::vector<uint8_t> rowRgb(width * 3);
    for (uint32_t y = 0; y < height; ++y) {
        const uint32_t* srcRow = &rgbaBuffer[y * width];
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t pixel = srcRow[x];
            // Format is 0xAABBGGRR (RGBA in memory)
            uint8_t r = static_cast<uint8_t>(pixel & 0xFF);
            uint8_t g = static_cast<uint8_t>((pixel >> 8) & 0xFF);
            uint8_t b = static_cast<uint8_t>((pixel >> 16) & 0xFF);
            rowRgb[x * 3 + 0] = r;
            rowRgb[x * 3 + 1] = g;
            rowRgb[x * 3 + 2] = b;
        }
        out.write(reinterpret_cast<const char*>(rowRgb.data()), rowRgb.size());
    }

    return out.good();
}

bool HeadlessVideoExporter::render(audio::AudioEngine& engine,
                                   const VideoExportConfig& config,
                                   FrameRenderCallback frameCallback,
                                   std::function<bool()> cancelCheck) {
    if (config.width == 0 || config.height == 0 || config.fps == 0 || config.durationSeconds <= 0.0) {
        return false;
    }

    const uint64_t totalFrames = static_cast<uint64_t>(std::ceil(config.durationSeconds * config.fps));
    const uint32_t sampleRate = (config.sampleRate > 0) ? config.sampleRate : 44100;
    const uint32_t samplesPerFrame = sampleRate / config.fps;

    ui::BatchRenderer2D renderer;
    if (!renderer.initialize(nullptr, config.width, config.height, ui::RenderBackendType::Filament)) {
        return false;
    }
    renderer.initDefaultMonospaceAtlas();

    presenter::TelemetryPresenter telemetry(512, 16);
    std::vector<float> scratchL(samplesPerFrame, 0.0f);
    std::vector<float> scratchR(samplesPerFrame, 0.0f);

    lastFrameBuffer_.resize(static_cast<size_t>(config.width) * config.height, 0);
    renderedFrameCount_ = 0;

    for (uint64_t frame = 0; frame < totalFrames; ++frame) {
        if (cancelCheck && cancelCheck()) {
            return false;
        }

        const double songTime = static_cast<double>(frame) / static_cast<double>(config.fps);
        FrameTimeContext timeContext{songTime, 1.0 / static_cast<double>(config.fps), frame, true};

        // 1. Step AudioEngine offline by samplesPerFrame
        engine.renderOfflineBlock(scratchL.data(), scratchR.data(), samplesPerFrame);

        // 2. Extract exact telemetry buffers & step TelemetryPresenter
        telemetry.update(engine, timeContext);

        // 3. Render visual scene to offscreen BatchRenderer2D
        renderVisualScene(renderer, telemetry, timeContext, config);

        // 4. Retrieve rendered framebuffer
        const uint32_t* fb = renderer.getFramebuffer();
        if (fb) {
            std::memcpy(lastFrameBuffer_.data(), fb, lastFrameBuffer_.size() * sizeof(uint32_t));
        }

        ++renderedFrameCount_;

        if (frameCallback && fb) {
            VideoExportProgress prog{};
            prog.currentFrame = frame + 1;
            prog.totalFrames = totalFrames;
            prog.currentSongTime = songTime;
            prog.percentComplete = (totalFrames > 0) ? (static_cast<double>(frame + 1) / totalFrames * 100.0) : 100.0;
            prog.peakL = telemetry.getSnapshot().masterMeter.peakL;
            prog.peakR = telemetry.getSnapshot().masterMeter.peakR;
            frameCallback(prog, fb);
        }
    }

    // Save final frame if requested
    if (!config.outputPath.empty() && !lastFrameBuffer_.empty()) {
        writePpmImage(config.outputPath, config.width, config.height, lastFrameBuffer_.data());
    }

    return true;
}

void HeadlessVideoExporter::renderVisualScene(ui::BatchRenderer2D& renderer,
                                             const presenter::TelemetryPresenter& telemetry,
                                             const FrameTimeContext& time,
                                             const VideoExportConfig& config) {
    const float w = static_cast<float>(config.width);
    const float h = static_cast<float>(config.height);

    renderer.beginFrame(w, h);

    // 1. Background gradient
    renderer.drawRectGradient(0.0f, 0.0f, w, h,
                              0.08f, 0.09f, 0.13f,
                              0.04f, 0.05f, 0.07f, 1.0f);

    // 2. Top Header Bar
    renderer.drawRect(0.0f, 0.0f, w, 40.0f, 0.12f, 0.14f, 0.19f, 0.95f);
    renderer.drawLine(0.0f, 40.0f, w, 40.0f, 0.22f, 0.26f, 0.35f, 1.0f, 1.5f);

    // Header Monospace Text
    std::ostringstream ss;
    int mins = static_cast<int>(time.songTimeSeconds) / 60;
    float secs = static_cast<float>(std::fmod(time.songTimeSeconds, 60.0));
    ss << "EATSBITS OFFLINE RENDERER | TIME: "
       << std::setfill('0') << std::setw(2) << mins << ":"
       << std::setfill('0') << std::setw(5) << std::fixed << std::setprecision(2) << secs
       << " | FRAME: " << time.frameIndex
       << " | FPS: " << config.fps
       << " | 44.1kHz STEREO";
    renderer.drawMonospaceText(16.0f, 12.0f, 8.0f, 16.0f, ss.str(), 0xFFE0E0E0);

    const auto& snap = telemetry.getSnapshot();

    // 3. Oscilloscope Waveform View
    if (config.renderWaveforms) {
        const float oscX = 20.0f;
        const float oscY = 56.0f;
        const float oscW = w - 40.0f;
        const float oscH = 180.0f;

        // Panel background & border
        renderer.drawRoundedRect(oscX, oscY, oscW, oscH, 6.0f, 0.07f, 0.08f, 0.11f, 0.95f);
        renderer.drawRoundedRectOutline(oscX, oscY, oscW, oscH, 6.0f, 0.20f, 0.24f, 0.32f, 1.0f, 1.0f);

        // Center zero line
        const float midY = oscY + oscH * 0.5f;
        renderer.drawLine(oscX + 10.0f, midY, oscX + oscW - 10.0f, midY, 0.16f, 0.20f, 0.28f, 0.8f, 1.0f);

        // Oscilloscope waveform trace
        if (!snap.scopeSamples.empty()) {
            const size_t numSamples = snap.scopeSamples.size();
            const float stepX = (oscW - 20.0f) / static_cast<float>(numSamples - 1);
            float prevX = oscX + 10.0f;
            float prevY = midY - snap.scopeSamples[0] * (oscH * 0.45f);

            for (size_t i = 1; i < numSamples; ++i) {
                float curX = oscX + 10.0f + static_cast<float>(i) * stepX;
                float curY = midY - snap.scopeSamples[i] * (oscH * 0.45f);
                renderer.drawLine(prevX, prevY, curX, curY, 0.20f, 0.85f, 0.95f, 0.9f, 1.5f);
                prevX = curX;
                prevY = curY;
            }
        }
    }

    // 4. Spectrum Analyzer & Master Meters
    const float lowerY = 252.0f;
    const float lowerH = h - lowerY - 24.0f;

    if (config.renderSpectrum) {
        const float specX = 20.0f;
        const float specW = w - 180.0f;

        renderer.drawRoundedRect(specX, lowerY, specW, lowerH, 6.0f, 0.07f, 0.08f, 0.11f, 0.95f);
        renderer.drawRoundedRectOutline(specX, lowerY, specW, lowerH, 6.0f, 0.20f, 0.24f, 0.32f, 1.0f, 1.0f);

        // 16 Frequency Spectrum Bars
        const size_t numBands = snap.spectrumBands.size();
        const float barGap = 6.0f;
        const float barTotalW = (specW - 30.0f - static_cast<float>(numBands - 1) * barGap) / static_cast<float>(numBands);

        for (size_t b = 0; b < numBands; ++b) {
            float energy = std::clamp(snap.spectrumBands[b], 0.0f, 1.0f);
            float barH = energy * (lowerH - 40.0f);
            float bx = specX + 15.0f + static_cast<float>(b) * (barTotalW + barGap);
            float by = lowerY + lowerH - 15.0f - barH;

            if (barH > 1.0f) {
                renderer.drawRectGradient(bx, by, barTotalW, barH,
                                          0.95f, 0.30f, 0.25f,
                                          0.25f, 0.75f, 0.95f, 0.95f);
            }
        }
    }

    if (config.renderMeters) {
        const float meterX = w - 140.0f;
        const float meterW = 120.0f;

        renderer.drawRoundedRect(meterX, lowerY, meterW, lowerH, 6.0f, 0.07f, 0.08f, 0.11f, 0.95f);
        renderer.drawRoundedRectOutline(meterX, lowerY, meterW, lowerH, 6.0f, 0.20f, 0.24f, 0.32f, 1.0f, 1.0f);

        // Left & Right meter channels
        const float chW = 36.0f;
        const float chH = lowerH - 40.0f;
        const float leftX = meterX + 16.0f;
        const float rightX = meterX + 68.0f;
        const float bottomY = lowerY + lowerH - 20.0f;

        auto drawMeterChannel = [&](float x, float peak, float rms) {
            // Background slot
            renderer.drawRect(x, bottomY - chH, chW, chH, 0.12f, 0.14f, 0.18f, 1.0f);

            // RMS fill
            float rmsH = std::clamp(rms, 0.0f, 1.0f) * chH;
            if (rmsH > 1.0f) {
                renderer.drawRectGradient(x, bottomY - rmsH, chW, rmsH,
                                          0.95f, 0.85f, 0.20f,
                                          0.20f, 0.85f, 0.40f, 0.95f);
            }

            // Peak indicator line
            float peakY = bottomY - std::clamp(peak, 0.0f, 1.0f) * chH;
            renderer.drawLine(x, peakY, x + chW, peakY, 1.0f, 0.25f, 0.25f, 1.0f, 2.0f);
        };

        drawMeterChannel(leftX, snap.masterMeter.peakL, snap.masterMeter.rmsL);
        drawMeterChannel(rightX, snap.masterMeter.peakR, snap.masterMeter.rmsR);
    }

    // 5. Bottom Timeline Bar
    if (config.renderTimeline) {
        float totalSecs = static_cast<float>(config.durationSeconds);
        float progress = (totalSecs > 0.001f) ? std::clamp(static_cast<float>(time.songTimeSeconds) / totalSecs, 0.0f, 1.0f) : 1.0f;
        renderer.drawRect(0.0f, h - 8.0f, w, 8.0f, 0.12f, 0.14f, 0.18f, 1.0f);
        renderer.drawRect(0.0f, h - 8.0f, w * progress, 8.0f, 0.65f, 0.35f, 0.95f, 1.0f);
    }

    renderer.endFrame();
}

} // namespace eatsbits::export_engine
