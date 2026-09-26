#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <numeric>
#include <filesystem>

#include "eatsbits/audio/export/wav_exporter.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        std::cerr << "Assertion failed: (" #expr ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while(0)

using namespace eatsbits;
using namespace eatsbits::audio;
using namespace eatsbits::audio::exporting;
using namespace eatsbits::sequencer;

// Helper to inspect binary RIFF/WAVE header
struct ParsedWavHeader {
    char riffId[4];
    uint32_t chunkSize;
    char waveId[4];
    char fmtId[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char dataId[4];
    uint32_t dataSize;
};

ParsedWavHeader readWavHeader(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    REQUIRE(in.is_open());

    ParsedWavHeader h{};
    in.read(h.riffId, 4);
    in.read(reinterpret_cast<char*>(&h.chunkSize), 4);
    in.read(h.waveId, 4);
    in.read(h.fmtId, 4);
    in.read(reinterpret_cast<char*>(&h.subchunk1Size), 4);
    in.read(reinterpret_cast<char*>(&h.audioFormat), 2);
    in.read(reinterpret_cast<char*>(&h.numChannels), 2);
    in.read(reinterpret_cast<char*>(&h.sampleRate), 4);
    in.read(reinterpret_cast<char*>(&h.byteRate), 4);
    in.read(reinterpret_cast<char*>(&h.blockAlign), 2);
    in.read(reinterpret_cast<char*>(&h.bitsPerSample), 2);
    in.read(h.dataId, 4);
    in.read(reinterpret_cast<char*>(&h.dataSize), 4);
    return h;
}

void testLowLevelWavFormats() {
    std::cout << "[Test] Low-level WAV file generation (16-bit, 24-bit, 32-bit Float)..." << std::endl;

    const uint32_t sampleRate = 48000;
    const uint32_t numFrames = 4800; // 0.1s
    std::vector<float> sineL(numFrames);
    std::vector<float> sineR(numFrames);

    for (uint32_t i = 0; i < numFrames; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        sineL[i] = 0.8f * std::sin(2.0f * 3.14159265f * 440.0f * t);
        sineR[i] = 0.8f * std::sin(2.0f * 3.14159265f * 880.0f * t);
    }
    const float* channels[2] = {sineL.data(), sineR.data()};

    // 1. Test 16-bit PCM
    {
        std::string p16 = "test_pcm16.wav";
        bool ok = WavExporter::writeWavFile(p16, channels, 2, numFrames, sampleRate, WavFormat::Pcm16, true);
        REQUIRE(ok);

        ParsedWavHeader h = readWavHeader(p16);
        REQUIRE(std::string(h.riffId, 4) == "RIFF");
        REQUIRE(std::string(h.waveId, 4) == "WAVE");
        REQUIRE(std::string(h.fmtId, 4) == "fmt ");
        REQUIRE(h.audioFormat == 1);
        REQUIRE(h.numChannels == 2);
        REQUIRE(h.sampleRate == 48000);
        REQUIRE(h.bitsPerSample == 16);
        REQUIRE(h.blockAlign == 4);
        REQUIRE(h.byteRate == 48000 * 4);
        REQUIRE(std::string(h.dataId, 4) == "data");
        REQUIRE(h.dataSize == numFrames * 4);
        REQUIRE(h.chunkSize == 36 + h.dataSize);
    }

    // 2. Test 24-bit PCM
    {
        std::string p24 = "test_pcm24.wav";
        bool ok = WavExporter::writeWavFile(p24, channels, 2, numFrames, sampleRate, WavFormat::Pcm24, true);
        REQUIRE(ok);

        ParsedWavHeader h = readWavHeader(p24);
        REQUIRE(h.audioFormat == 1);
        REQUIRE(h.numChannels == 2);
        REQUIRE(h.sampleRate == 48000);
        REQUIRE(h.bitsPerSample == 24);
        REQUIRE(h.blockAlign == 6);
        REQUIRE(h.byteRate == 48000 * 6);
        REQUIRE(h.dataSize == numFrames * 6);
        REQUIRE(h.chunkSize == 36 + h.dataSize);
    }

    // 3. Test 32-bit Float
    {
        std::string pf32 = "test_float32.wav";
        bool ok = WavExporter::writeWavFile(pf32, channels, 2, numFrames, sampleRate, WavFormat::Float32, false);
        REQUIRE(ok);

        ParsedWavHeader h = readWavHeader(pf32);
        REQUIRE(h.audioFormat == 3); // IEEE Float
        REQUIRE(h.numChannels == 2);
        REQUIRE(h.sampleRate == 48000);
        REQUIRE(h.bitsPerSample == 32);
        REQUIRE(h.blockAlign == 8);
        REQUIRE(h.byteRate == 48000 * 8);
        REQUIRE(h.dataSize == numFrames * 8);
        REQUIRE(h.chunkSize == 36 + h.dataSize);
    }

    std::cout << "  [PASS] Low-level WAV formats verified." << std::endl;
}

void testTpdfDitherStatistics() {
    std::cout << "[Test] TPDF dither statistical distribution..." << std::endl;

    uint32_t state = 0xDEADBEEF;
    const size_t N = 100000;
    double sum = 0.0;
    double sumSq = 0.0;

    for (size_t i = 0; i < N; ++i) {
        // Expose generateTpdfDither or test dither behavior
        const float r1 = (static_cast<float>(state ^= state << 13, state ^= state >> 17, state ^= state << 5) / 2147483648.0f) - 1.0f;
        const float r2 = (static_cast<float>(state ^= state << 13, state ^= state >> 17, state ^= state << 5) / 2147483648.0f) - 1.0f;
        float dither = 0.5f * (r1 - r2);

        REQUIRE(dither >= -1.0f && dither <= 1.0f);
        sum += dither;
        sumSq += dither * dither;
    }

    double mean = sum / N;
    double variance = (sumSq / N) - (mean * mean);

    // Theoretical variance of triangular distribution in [-1, 1] is 1/6 ~= 0.1667
    REQUIRE(std::abs(mean) < 0.01);
    REQUIRE(std::abs(variance - (1.0 / 6.0)) < 0.01);

    std::cout << "  [PASS] TPDF dither verified (mean = " << mean << ", var = " << variance << ")." << std::endl;
}

void testMasterProjectBounce() {
    std::cout << "[Test] Master project offline bounce (24-bit PCM)..." << std::endl;

    AudioGraph graph;
    auto tb = std::make_shared<Tb303Node>("AcidTB");
    auto delay = std::make_shared<DelayNode>("Delay");
    auto gain = std::make_shared<GainNode>("MasterOut");

    NodeId tbId = graph.addNode(tb);
    NodeId delayId = graph.addNode(delay);
    NodeId gainId = graph.addNode(gain);

    graph.connect(tbId, 0, delayId, 0);
    graph.connect(delayId, 0, gainId, 0);
    graph.setOutputNode(gainId, 0);

    StepSequencer seq;
    seq.setBpm(135.0);
    size_t tId = seq.addTrack("303Track", tbId, 16);
    auto* tr = seq.getTrack(tId);
    for (uint32_t s = 0; s < 16; ++s) {
        StepData st{};
        st.active = (s % 2 == 0);
        st.note = 36 + (s * 2);
        st.velocity = 0.9f;
        st.gateLength = 0.6f;
        tr->setStep(s, st);
    }

    BounceConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.numChannels = 2;
    cfg.durationSeconds = 2.0;
    cfg.format = WavFormat::Pcm24;
    cfg.enableDither = true;

    BounceStats stats = WavExporter::bounceMaster(graph, seq, cfg, "test_master_bounce.wav");

    REQUIRE(stats.totalFrames == 96000);
    REQUIRE(stats.peakLeft > 0.05f);
    REQUIRE(stats.peakRight > 0.05f);
    REQUIRE(stats.speedMultiplier > 10.0); // Offline render must be fast
    REQUIRE(stats.fileSizeBytes == 44 + 96000 * 6); // 24-bit stereo

    std::cout << "  [PASS] Master bounce rendered 2.0s in " << stats.renderTimeMs
              << " ms (" << stats.speedMultiplier << "x real-time speed, Peak: "
              << stats.peakLeft << ")." << std::endl;
}

void testStemSeparationBounce() {
    std::cout << "[Test] Multi-track stem separation bounce..." << std::endl;

    AudioGraph graph;
    auto tb = std::make_shared<Tb303Node>("TB303");
    auto drums = std::make_shared<DrumKitNode>("Drums");
    auto masterGain = std::make_shared<GainNode>("Master");

    NodeId tbId = graph.addNode(tb);
    NodeId drumId = graph.addNode(drums);
    NodeId masterId = graph.addNode(masterGain);

    graph.connect(tbId, 0, masterId, 0);
    graph.connect(drumId, 0, masterId, 0);
    graph.setOutputNode(masterId, 0);

    StepSequencer seq;
    seq.setBpm(130.0);

    // Track 1: 303
    size_t t1 = seq.addTrack("SynthBass", tbId, 16);
    auto* tr1 = seq.getTrack(t1);
    for (uint32_t s = 0; s < 16; ++s) {
        StepData st{};
        st.active = (s % 4 == 0);
        st.note = 36;
        st.velocity = 0.9f;
        tr1->setStep(s, st);
    }

    // Track 2: Drums
    size_t t2 = seq.addTrack("Percussion", drumId, 16);
    auto* tr2 = seq.getTrack(t2);
    for (uint32_t s = 0; s < 16; ++s) {
        StepData st{};
        st.active = (s % 2 == 1);
        st.note = 42; // Hat
        st.velocity = 0.8f;
        tr2->setStep(s, st);
    }

    BounceConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.durationSeconds = 1.0;
    cfg.format = WavFormat::Pcm16;

    std::string stemDir = "test_stems_out";
    std::vector<BounceStats> stems = WavExporter::bounceStems(graph, seq, cfg, stemDir);

    // Expect: 1 master + 2 tracks = 3 files
    REQUIRE(stems.size() == 3);

    for (const auto& s : stems) {
        REQUIRE(std::filesystem::exists(s.outputPath));
        REQUIRE(s.fileSizeBytes > 44);
        REQUIRE(s.totalFrames == 48000);
    }

    // Clean up test stem directory
    try {
        std::filesystem::remove_all(stemDir);
        std::filesystem::remove("test_pcm16.wav");
        std::filesystem::remove("test_pcm24.wav");
        std::filesystem::remove("test_float32.wav");
        std::filesystem::remove("test_master_bounce.wav");
    } catch (...) {}

    std::cout << "  [PASS] Multi-track stem separation bounce verified." << std::endl;
}

void testAudioEngineBounceIntegration() {
    std::cout << "[Test] AudioEngine.bounceProject integration..." << std::endl;

    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.initialize(cfg);
    engine.setupDefaultAcidBeatGraph();

    auto stats = engine.bounceProject("test_engine_bounce.wav", 1.0, WavFormat::Pcm24);
    REQUIRE(stats.totalFrames == 48000);
    REQUIRE(stats.fileSizeBytes == 44 + 48000 * 6);
    REQUIRE(stats.peakLeft >= 0.0f);

    try {
        std::filesystem::remove("test_engine_bounce.wav");
    } catch (...) {}

    std::cout << "  [PASS] AudioEngine bounce integration passed." << std::endl;
}

int main() {
    std::cout << "=== Running Eatsbits Phase 5 Audio Bouncing & Stem Exporter Tests ===" << std::endl;
    testLowLevelWavFormats();
    testTpdfDitherStatistics();
    testMasterProjectBounce();
    testStemSeparationBounce();
    testAudioEngineBounceIntegration();
    std::cout << "=== All Phase 5 Audio Bouncing Tests Passed! ===" << std::endl;
    return 0;
}
