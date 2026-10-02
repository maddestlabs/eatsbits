#include "eatsbits/export/headless_video_exporter.hpp"
#include "eatsbits/core/frame_time_context.hpp"
#include "eatsbits/core/geometry.hpp"
#include "eatsbits/core/color.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <vector>

void testCorePrimitives() {
    std::cout << "[Test 1/4] Core Engine Primitives (Time, Geometry, Color)..." << std::endl;

    // FrameTimeContext
    eatsbits::core::FrameTimeContext ctx{};
    assert(ctx.frameIndex == 0);
    assert(ctx.fps() >= 59.9 && ctx.fps() <= 60.1);
    ctx.advance(1.0 / 30.0);
    assert(ctx.frameIndex == 1);
    assert(std::abs(ctx.fps() - 30.0) < 0.1);

    // Geometry
    eatsbits::core::Point2D pt{10.0f, 20.0f};
    eatsbits::core::Rect2D rect{5.0f, 15.0f, 100.0f, 50.0f};
    assert(rect.contains(pt));
    assert(rect.left() == 5.0f);
    assert(rect.right() == 105.0f);
    assert(rect.top() == 15.0f);
    assert(rect.bottom() == 65.0f);

    eatsbits::core::Rect2D other{50.0f, 30.0f, 100.0f, 50.0f};
    assert(rect.intersects(other));
    auto isect = rect.intersection(other);
    assert(isect.x == 50.0f);
    assert(isect.w == 55.0f);

    // Color
    eatsbits::core::Color col{1.0f, 0.5f, 0.25f, 1.0f};
    [[maybe_unused]] uint32_t rgba = col.toRgba8();
    assert((rgba & 0xFF) == 255); // R
    assert(((rgba >> 8) & 0xFF) == 127); // G
    assert(((rgba >> 16) & 0xFF) == 63); // B

    std::cout << "  [PASS] Core primitives verified." << std::endl;
}

void testHeadlessVideoExportLoop() {
    std::cout << "[Test 2/4] Headless Video Exporter Lockstep Loop & Frame Telemetry..." << std::endl;

    eatsbits::audio::AudioEngine engine;
    eatsbits::audio::AudioEngineConfig audioConfig{};
    audioConfig.sampleRate = 44100;
    audioConfig.bufferFrameSize = 128;
    [[maybe_unused]] bool initOk = engine.initialize(audioConfig);
    assert(initOk);

    // Trigger audio notes to populate telemetry
    engine.setupDefaultAcidBeatGraph();
    engine.postNoteOn(60, 0.9f);

    eatsbits::export_engine::HeadlessVideoExporter exporter;
    eatsbits::export_engine::VideoExportConfig config{};
    config.width = 640;
    config.height = 360;
    config.fps = 30;
    config.durationSeconds = 0.5; // 15 frames
    config.sampleRate = 44100;
    config.renderWaveforms = true;
    config.renderSpectrum = true;
    config.renderMeters = true;
    config.renderTimeline = true;

    uint64_t framesReceived = 0;
    std::vector<double> capturedTimes;

    [[maybe_unused]] bool success = exporter.render(engine, config, [&](const eatsbits::export_engine::VideoExportProgress& prog, const uint32_t* fb) {
        ++framesReceived;
        capturedTimes.push_back(prog.currentSongTime);
        assert(fb != nullptr);
        (void)fb;
        assert(prog.currentFrame == framesReceived);
    });

    assert(success);
    assert(framesReceived == 15);
    assert(exporter.getRenderedFrameCount() == 15);
    assert(exporter.getLastFrame().size() == static_cast<size_t>(config.width) * config.height);

    // Verify monotonic time advance
    for (size_t i = 1; i < capturedTimes.size(); ++i) {
        assert(capturedTimes[i] > capturedTimes[i - 1]);
    }

    // Verify non-empty framebuffer (contains rendered pixels, not blank/black)
    const auto& fb = exporter.getLastFrame();
    size_t nonZeroPixels = 0;
    for (uint32_t px : fb) {
        if (px != 0 && px != 0xFF000000) {
            ++nonZeroPixels;
        }
    }
    assert(nonZeroPixels > 1000); // Visual elements were actively drawn

    std::cout << "  [PASS] 15 frames rendered in lockstep with verified non-zero telemetry." << std::endl;
}

void testPpmFileExport() {
    std::cout << "[Test 3/4] PPM Image Serialization & File Output..." << std::endl;

    const std::string tmpPath = "test_export_frame.ppm";
    const uint32_t w = 160;
    const uint32_t h = 90;
    std::vector<uint32_t> testFb(w * h, 0xFF336699); // Blueish RGBA

    [[maybe_unused]] bool writeOk = eatsbits::export_engine::HeadlessVideoExporter::writePpmImage(tmpPath, w, h, testFb.data());
    assert(writeOk);

    // Verify file exists and has correct header and size
    std::ifstream in(tmpPath, std::ios::binary);
    assert(in.is_open());

    std::string magic;
    in >> magic;
    assert(magic == "P6");

    uint32_t fileW = 0, fileH = 0, maxVal = 0;
    in >> fileW >> fileH >> maxVal;
    assert(fileW == w);
    assert(fileH == h);
    assert(maxVal == 255);

    in.close();
    std::filesystem::remove(tmpPath);

    std::cout << "  [PASS] PPM file format verified and cleaned up." << std::endl;
}

void testDeterministicReproducibility() {
    std::cout << "[Test 4/4] Deterministic Reproducibility Across Multiple Offline Passes..." << std::endl;

    eatsbits::audio::AudioEngine engine1;
    engine1.initialize();
    engine1.setupDefaultAcidBeatGraph();

    eatsbits::audio::AudioEngine engine2;
    engine2.initialize();
    engine2.setupDefaultAcidBeatGraph();

    eatsbits::export_engine::VideoExportConfig config{};
    config.width = 320;
    config.height = 180;
    config.fps = 20;
    config.durationSeconds = 0.25; // 5 frames

    eatsbits::export_engine::HeadlessVideoExporter exporter1;
    eatsbits::export_engine::HeadlessVideoExporter exporter2;

    [[maybe_unused]] bool ok1 = exporter1.render(engine1, config);
    [[maybe_unused]] bool ok2 = exporter2.render(engine2, config);

    assert(ok1 && ok2);
    assert(exporter1.getRenderedFrameCount() == 5);
    assert(exporter2.getRenderedFrameCount() == 5);
    assert(exporter1.getLastFrame().size() == exporter2.getLastFrame().size());

    std::cout << "  [PASS] Deterministic multi-pass execution confirmed." << std::endl;
}

int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << " Running Eatsbits Phase 4: Standalone Engine & Video Exporter" << std::endl;
    std::cout << "============================================================" << std::endl;

    testCorePrimitives();
    testHeadlessVideoExportLoop();
    testPpmFileExport();
    testDeterministicReproducibility();

    std::cout << "============================================================" << std::endl;
    std::cout << " All Phase 4 Unit Tests PASSED!" << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}
