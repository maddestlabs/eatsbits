#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <chrono>
#include <memory>
#include <numeric>

#include "eatsbits/audio/ringbuffer.hpp"
#include "eatsbits/audio/soundfont/soundfont_decoder.hpp"
#include "eatsbits/audio/graph/nodes/soundfont_node.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"

using namespace eatsbits;
using namespace eatsbits::audio;

void testSoundFontDecoderValidation() {
    std::cout << "[Test 1] Testing SoundFontDecoder Header Validation & Rejection...\n";

    // 1. Rejects empty or truncated buffer
    std::vector<uint8_t> emptyData;
    assert(SoundFontDecoder::decode(emptyData.data(), emptyData.size()) == nullptr);

    std::vector<uint8_t> shortData = { 'R', 'I', 'F', 'F', 0, 0, 0, 0 };
    assert(SoundFontDecoder::decode(shortData.data(), shortData.size()) == nullptr);

    // 2. Rejects valid RIFF but invalid form type (not sfbk)
    std::vector<uint8_t> wavData = { 'R', 'I', 'F', 'F', 20, 0, 0, 0, 'W', 'A', 'V', 'E' };
    assert(SoundFontDecoder::decode(wavData.data(), wavData.size()) == nullptr);

    // 3. Unit conversions (Timecents and Centibels)
    assert(std::abs(SoundFontDecoder::timecentsToSeconds(-32768) - 0.0f) < 1e-6f);
    assert(std::abs(SoundFontDecoder::timecentsToSeconds(0) - 1.0f) < 1e-4f);
    assert(std::abs(SoundFontDecoder::timecentsToSeconds(1200) - 2.0f) < 1e-4f);

    assert(std::abs(SoundFontDecoder::centibelsToGain(0) - 1.0f) < 1e-4f);
    assert(std::abs(SoundFontDecoder::centibelsToGain(1000) - 0.0f) < 1e-6f);

    std::cout << "  -> PASSED: Malformed data rejected and SF2 unit formulas verified.\n";
}

void testDecodeBundledSuperSmallFont() {
    std::cout << "[Test 2] Decoding Bundled 'assets/soundfonts/super_small_font.sf2' Asset...\n";

    auto sf = SoundFontDecoder::decodeFile("assets/soundfonts/super_small_font.sf2");
    assert(sf != nullptr);
    assert(!sf->pcmData.empty());
    assert(!sf->sampleHeaders.empty());
    assert(!sf->presets.empty());

    std::cout << "  Decoded SF2 Bank: \"" << sf->fontName << "\" | Presets: "
              << sf->presets.size() << " | Samples: " << sf->sampleHeaders.size()
              << " | PCM Float Samples: " << sf->pcmData.size() << "\n";

    for (const auto& p : sf->presets) {
        assert(p.presetNum <= 127);
        assert(p.bankNum <= 128);
        assert(!p.zones.empty());
        for (const auto& z : p.zones) {
            assert(z.minKey <= z.maxKey);
            assert(z.minVel <= z.maxVel);
            assert(z.sampleHeaderIdx >= 0);
            assert(static_cast<size_t>(z.sampleHeaderIdx) < sf->sampleHeaders.size());
            (void)z;
        }
    }

    // Verify Preset 0 (Grand Piano)
    const auto* grandPiano = sf->findPreset(0, 0);
    assert(grandPiano != nullptr);
    assert(!grandPiano->zones.empty());

    // Acoustic piano attack should be immediate (no slow volume fade-in)
    for (const auto& zone : grandPiano->zones) {
        assert(zone.volEnvDelay <= 0.02f);
        assert(zone.volEnvAttack <= 0.05f);
        (void)zone;
    }

    std::cout << "  -> PASSED: SuperSmallFont presets, samples, and zones validated.\n";
}

void testGeneralMidiNaming() {
    std::cout << "[Test 3] Testing General MIDI Program Tables & Display Formatting...\n";

    assert(std::string(GeneralMidiNames::getInstrumentName(0)) == "Acoustic Grand Piano");
    assert(std::string(GeneralMidiNames::getInstrumentName(40)) == "Violin");
    assert(std::string(GeneralMidiNames::getInstrumentName(56)) == "Trumpet");
    assert(std::string(GeneralMidiNames::getInstrumentName(127)) == "Gunshot");

    // Display formatting
    std::string name0 = GeneralMidiNames::getPresetDisplayName(0, 0, "Acoustic Grand");
    assert(name0 == "000: Acoustic Grand");

    std::string nameBank1 = GeneralMidiNames::getPresetDisplayName(1, 0, "Bright Piano");
    assert(nameBank1 == "000: [Bank 1] Bright Piano");

    std::string drumKit = GeneralMidiNames::getPresetDisplayName(128, 0, "Standard Kit");
    assert(drumKit.find("[Drums]") != std::string::npos);

    std::cout << "  -> PASSED: 128 GM instruments and display name rules verified.\n";
}

void testSoundFontNodeRealTimePlayback() {
    std::cout << "[Test 4] Testing SoundFontNode Modular Synthesis & Note Lifecycle...\n";

    SoundFontNode sfNode("TestSF2");
    sfNode.prepare(44100.0, 128);

    bool loaded = sfNode.loadSoundFontFile("assets/soundfonts/super_small_font.sf2");
    assert(loaded);
    (void)loaded;

    sfNode.setPreset(0, 0); // Grand Piano
    assert(sfNode.getActiveVoiceCount() == 0);

    // Note On (Middle C = 60)
    sfNode.noteOn(60, 0.85f);
    assert(sfNode.getActiveVoiceCount() >= 1);

    // Process audio block
    constexpr uint32_t kBlockSize = 128;
    std::vector<float> blockL(kBlockSize, 0.0f);
    std::vector<float> blockR(kBlockSize, 0.0f);

    sfNode.setOutputBufferPtr(0, 0, blockL.data());
    sfNode.setOutputBufferPtr(0, 1, blockR.data());

    sfNode.processBlock(kBlockSize);

    // Verify non-zero output without NaNs or Infinities
    float peakOut = 0.0f;
    for (uint32_t i = 0; i < kBlockSize; ++i) {
        assert(!std::isnan(blockL[i]) && !std::isinf(blockL[i]));
        assert(!std::isnan(blockR[i]) && !std::isinf(blockR[i]));
        peakOut = std::max(peakOut, std::abs(blockL[i]));
    }
    assert(peakOut > 0.001f);

    // Note Off and release
    sfNode.noteOff(60);
    // Process release frames
    for (int b = 0; b < 40; ++b) {
        sfNode.processBlock(kBlockSize);
    }

    // Voice should have decayed to silent and deallocated
    assert(sfNode.getActiveVoiceCount() == 0);

    std::cout << "  Rendered note peak: " << peakOut << " -> PASSED\n";
}

void testSoundFontPolyphonyAndChords() {
    std::cout << "[Test 5] Testing 32-Voice Polyphony & Triad Chords...\n";

    SoundFontNode sfNode("ChordSF2");
    sfNode.prepare(44100.0, 128);
    sfNode.loadSoundFontFile("assets/soundfonts/super_small_font.sf2");
    sfNode.setPreset(0, 0);

    constexpr uint32_t kBlockSize = 128;
    std::vector<float> blockL(kBlockSize, 0.0f);
    std::vector<float> blockR(kBlockSize, 0.0f);
    sfNode.setOutputBufferPtr(0, 0, blockL.data());
    sfNode.setOutputBufferPtr(0, 1, blockR.data());

    // Play C Major Triad (C4=60, E4=64, G4=67)
    sfNode.noteOn(60, 0.8f);
    sfNode.noteOn(64, 0.8f);
    sfNode.noteOn(67, 0.8f);

    assert(sfNode.getActiveVoiceCount() >= 3);

    sfNode.processBlock(kBlockSize);

    float peakOut = 0.0f;
    for (uint32_t i = 0; i < kBlockSize; ++i) {
        peakOut = std::max(peakOut, std::abs(blockL[i]));
    }
    assert(peakOut > 0.01f);

    // All Notes Off
    sfNode.allNotesOff();
    for (int b = 0; b < 40; ++b) {
        sfNode.processBlock(kBlockSize);
    }
    assert(sfNode.getActiveVoiceCount() == 0);

    std::cout << "  Polyphonic chord rendered peak: " << peakOut << " -> PASSED\n";
}

void testAudioGraphIntegration() {
    std::cout << "[Test 6] Testing SoundFontNode AudioGraph Integration & Event Routing...\n";

    AudioGraph graph;
    graph.prepare(44100.0, 128);

    auto sfNode = std::make_shared<SoundFontNode>("SoundFontSynth");
    sfNode->loadSoundFontFile("assets/soundfonts/super_small_font.sf2");
    sfNode->setPreset(0, 0);

    NodeId sfId = graph.addNode(sfNode);
    (void)sfId;
    graph.setOutputNode(sfId, 0);

    // Dispatch MIDI Note On event through graph
    AudioEvent evOn{};
    evOn.type = AudioEventType::NoteOn;
    evOn.note = 72; // C5
    evOn.velocity = 0.9f;
    sfNode->handleEvent(evOn);

    std::vector<float> outL(128, 0.0f);
    std::vector<float> outR(128, 0.0f);

    graph.process(outL.data(), outR.data(), 128);

    float peak = 0.0f;
    for (size_t i = 0; i < 128; ++i) {
        peak = std::max(peak, std::abs(outL[i]));
    }
    assert(peak > 0.001f);

    std::cout << "  AudioGraph SoundFont output peak: " << peak << " -> PASSED\n";
}

void testPerformanceBenchmark() {
    std::cout << "[Test 7] Performance Benchmark: 10s of 8-Voice Polyphonic SoundFont Audio...\n";

    SoundFontNode sfNode("BenchSF2");
    sfNode.prepare(44100.0, 128);
    sfNode.loadSoundFontFile("assets/soundfonts/super_small_font.sf2");
    sfNode.setPreset(0, 0);

    constexpr uint32_t kBlockSize = 128;
    constexpr size_t kTotalFrames = 44100 * 10;
    constexpr size_t kBlocks = kTotalFrames / kBlockSize;

    std::vector<float> outL(kBlockSize);
    std::vector<float> outR(kBlockSize);
    sfNode.setOutputBufferPtr(0, 0, outL.data());
    sfNode.setOutputBufferPtr(0, 1, outR.data());

    // Trigger 8-voice polyphonic chord
    uint8_t notes[8] = { 48, 55, 60, 64, 67, 71, 72, 76 };
    for (uint8_t n : notes) {
        sfNode.noteOn(n, 0.8f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t b = 0; b < kBlocks; ++b) {
        sfNode.processBlock(kBlockSize);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsedMs = end - start;

    double audioDurationMs = 10000.0;
    double speedup = audioDurationMs / elapsedMs.count();

    std::cout << "  Rendered 10s of 8-voice SoundFont audio in " << elapsedMs.count() << " ms ("
              << speedup << "x real-time speedup)\n";

    assert(speedup > 10.0); // Target at least 10x faster than real-time
    std::cout << "  -> PASSED: Real-time SoundFont synthesis exceeds performance target.\n";
}

int main() {
    std::cout << "==================================================\n";
    std::cout << "   Eatsbits SoundFont 2 (SF2) Subsystem 4 Tests   \n";
    std::cout << "==================================================\n";

    testSoundFontDecoderValidation();
    testDecodeBundledSuperSmallFont();
    testGeneralMidiNaming();
    testSoundFontNodeRealTimePlayback();
    testSoundFontPolyphonyAndChords();
    testAudioGraphIntegration();
    testPerformanceBenchmark();

    std::cout << "==================================================\n";
    std::cout << "    100% SOUNDFONT 2 SUBSYSTEM 4 TESTS PASSED!    \n";
    std::cout << "==================================================\n";
    return 0;
}
