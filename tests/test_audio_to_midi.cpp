#include <iostream>
#include <cassert>
#include <cmath>
#include <numbers>
#include <string>

#include "eatsbits/audio/audio_to_midi_engine.hpp"
#include "eatsbits/ui/widgets/audio_to_midi_dialog.hpp"
#include "eatsbits/ui/gui_window.hpp"

using namespace eatsbits;
using namespace eatsbits::audio;
using namespace eatsbits::ui;

void testMonophonicA4SineWave() {
    std::cout << "[Test 1/11] Transcribing monophonic A4 (440 Hz) sine wave into MIDI Note 69..." << std::endl;

    auto audio = DecodedAudioBuffer::createSyntheticSine(440.0, 1.0, 44100, 0.8f);
    assert(audio.sampleRate == 44100);
    assert(audio.channels == 1);
    assert(!audio.empty());

    AudioToMidiOptions opts;
    opts.mode = TranscriptionEngineMode::HybridDsp;
    opts.onsetThreshold = 0.30f;
    opts.frameThreshold = 0.25f;
    opts.minMidiPitch = 60;
    opts.maxMidiPitch = 80;
    opts.minNoteDurationMs = 80.0f;

    auto result = AudioToMidiEngine::transcribeAudioBuffer(audio, opts);
    assert(!result.notes.empty());

    // Verify primary note is 69 (A4)
    bool hasA4 = false;
    for (const auto& n : result.notes) {
        if (n.pitch == 69) {
            hasA4 = true;
            assert(n.velocity >= 0.2f);
            assert(n.durationSteps > 0.0f);
        }
    }
    assert(hasA4);
    std::cout << "  -> PASSED: Detected A4 (MIDI 69) with confidence." << std::endl;
}

void testMonophonicC4Tone() {
    std::cout << "[Test 2/11] Transcribing monophonic C4 (261.63 Hz) tone into MIDI Note 60..." << std::endl;

    auto audio = DecodedAudioBuffer::createSyntheticSine(261.63, 0.8, 44100, 0.8f);
    AudioToMidiOptions opts;
    opts.mode = TranscriptionEngineMode::HybridDsp;
    opts.onsetThreshold = 0.30f;
    opts.frameThreshold = 0.25f;
    opts.minMidiPitch = 50;
    opts.maxMidiPitch = 70;
    opts.minNoteDurationMs = 80.0f;

    auto result = AudioToMidiEngine::transcribeAudioBuffer(audio, opts);
    assert(!result.notes.empty());

    bool hasC4 = false;
    for (const auto& n : result.notes) {
        if (n.pitch == 60) hasC4 = true;
    }
    assert(hasC4);
    std::cout << "  -> PASSED: Detected C4 (MIDI 60)." << std::endl;
}

void testPolyphonicChord() {
    std::cout << "[Test 3/11] Transcribing polyphonic dyad (C4 60 + G4 67)..." << std::endl;

    std::vector<double> chordFreqs = {261.63, 392.00}; // C4 + G4 (Fifth)
    auto audio = DecodedAudioBuffer::createSyntheticChord(chordFreqs, 1.0, 44100, 0.8f);

    AudioToMidiOptions opts;
    opts.mode = TranscriptionEngineMode::HybridDsp;
    opts.onsetThreshold = 0.25f;
    opts.frameThreshold = 0.20f;
    opts.minMidiPitch = 55;
    opts.maxMidiPitch = 75;
    opts.minNoteDurationMs = 80.0f;

    auto result = AudioToMidiEngine::transcribeAudioBuffer(audio, opts);
    assert(!result.notes.empty());

    bool hasC4 = false;
    bool hasG4 = false;
    for (const auto& n : result.notes) {
        if (n.pitch == 60) hasC4 = true;
        if (n.pitch == 67) hasG4 = true;
    }
    assert(hasC4);
    assert(hasG4);
    std::cout << "  -> PASSED: Both polyphonic chord pitches (60 & 67) detected simultaneously." << std::endl;
}

void testYinMonophonicPitchTracker() {
    std::cout << "[Test 4/11] Testing high-precision YIN autocorrelation pitch tracker..." << std::endl;

    // Single frame test for A4
    constexpr size_t winSize = 1024;
    std::vector<float> frame(winSize);
    double phaseIncr = 2.0 * std::numbers::pi * 440.0 / 44100.0;
    for (size_t i = 0; i < winSize; ++i) {
        frame[i] = 0.8f * static_cast<float>(std::sin(phaseIncr * static_cast<double>(i)));
    }

    float detectedFreq = AudioToMidiEngine::detectPitchYin(frame.data(), winSize, 44100);
    assert(std::abs(detectedFreq - 440.0f) < 2.5f);

    // Full audio buffer test using YIN mode
    auto audio = DecodedAudioBuffer::createSyntheticSine(440.0, 0.8, 44100, 0.85f);
    AudioToMidiOptions opts;
    opts.mode = TranscriptionEngineMode::YinMonophonic;
    opts.onsetThreshold = 0.25f;
    opts.frameThreshold = 0.25f;
    opts.minMidiPitch = 55;
    opts.maxMidiPitch = 85;

    auto result = AudioToMidiEngine::transcribeAudioBuffer(audio, opts);
    assert(!result.notes.empty());
    assert(result.notes.front().pitch == 69);
    std::cout << "  -> PASSED: YIN algorithm detected 440.0 Hz with sub-sample parabolic interpolation." << std::endl;
}

void testSilenceHandling() {
    std::cout << "[Test 5/11] Verifying silence handling without phantom notes..." << std::endl;

    DecodedAudioBuffer silentBuffer;
    silentBuffer.samples.assign(44100, 0.0f);
    silentBuffer.sampleRate = 44100;
    silentBuffer.channels = 1;

    AudioToMidiOptions opts;
    auto result = AudioToMidiEngine::transcribeAudioBuffer(silentBuffer, opts);
    assert(result.notes.empty());
    std::cout << "  -> PASSED: Zero phantom notes produced on 1 second of total silence." << std::endl;
}

void testPercussiveOnsetDetection() {
    std::cout << "[Test 6/11] Testing percussive transient and sub-band onset detector..." << std::endl;

    // Synthesize a low frequency burst (Kick drum simulation)
    DecodedAudioBuffer kickAudio;
    kickAudio.sampleRate = 44100;
    kickAudio.channels = 1;
    kickAudio.samples.assign(44100, 0.0f);

    // Strike at sample 5000 (~0.11s)
    for (size_t i = 0; i < 2000; ++i) {
        float env = 1.0f - static_cast<float>(i) / 2000.0f;
        kickAudio.samples[5000 + i] = env * std::sin(2.0f * 3.14159f * 65.0f * (static_cast<float>(i) / 44100.0f));
    }

    auto onsets = AudioToMidiEngine::detectOnsetsAndTransients(kickAudio.samples, 44100, 0.2f);
    assert(!onsets.empty());
    assert(onsets.front().timeSec >= 0.09f && onsets.front().timeSec <= 0.15f);

    AudioToMidiOptions drumOpts;
    drumOpts.mode = TranscriptionEngineMode::PercussiveTransient;
    drumOpts.onsetThreshold = 0.2f;

    auto result = AudioToMidiEngine::transcribeAudioBuffer(kickAudio, drumOpts);
    assert(!result.notes.empty());
    assert(result.notes.front().pitch == 36 || result.notes.front().pitch == 38);
    std::cout << "  -> PASSED: Percussive transients accurately localized and mapped to drum notes." << std::endl;
}

void testCancellationToken() {
    std::cout << "[Test 7/11] Testing cancellation token immediate abort..." << std::endl;

    auto audio = DecodedAudioBuffer::createSyntheticSine(440.0, 2.0, 44100, 0.8f);
    CancellationToken token;
    token.cancel(); // Cancel before start

    auto result = AudioToMidiEngine::transcribeAudioBuffer(audio, AudioToMidiOptions{}, &token);
    assert(result.notes.empty());
    assert(result.name == "Cancelled");
    std::cout << "  -> PASSED: Transcription aborted instantly when cancelled." << std::endl;
}

void testWaveformOverview() {
    std::cout << "[Test 8/11] Generating WaveformOverview peaks for GUI display..." << std::endl;

    auto audio = DecodedAudioBuffer::createSyntheticSine(440.0, 0.5, 44100, 0.9f);
    auto wf = WaveformOverview::generate(audio.samples, 128);
    assert(wf.minPeaks.size() == 128);
    assert(wf.maxPeaks.size() == 128);
    assert(wf.maxPeaks[10] > 0.5f);
    assert(wf.minPeaks[10] < -0.5f);
    std::cout << "  -> PASSED: WaveformOverview generated 128 peak pairs." << std::endl;
}

void testChordExtractionIntegration() {
    std::cout << "[Test 9/11] Harmonic Chord Track integration from transcribed notes..." << std::endl;

    // C Major Triad (C4 261.63, E4 329.63, G4 392.00)
    std::vector<double> cMajFreqs = {261.63, 329.63, 392.00};
    auto audio = DecodedAudioBuffer::createSyntheticChord(cMajFreqs, 1.2, 44100, 0.9f);

    AudioToMidiOptions opts;
    opts.mode = TranscriptionEngineMode::HybridDsp;
    opts.minMidiPitch = 55;
    opts.maxMidiPitch = 75;
    opts.extractChords = true;

    auto result = AudioToMidiEngine::transcribeAudioBuffer(audio, opts);
    assert(!result.notes.empty());
    assert(!result.detectedChords.empty());

    const auto& chord = result.detectedChords.front();
    assert(chord.rootPitchClass == 0); // Root C (0)
    assert(chord.quality == theory::ChordQuality::Major);
    std::cout << "  -> PASSED: Extracted " << chord.getDisplayName() << " chord directly for Chord Track!" << std::endl;
}

void testAudioToMidiDialog() {
    std::cout << "[Test 10/11] AudioToMidiDialog layout, sliders, and pointer interactions..." << std::endl;

    AudioToMidiDialog dialog;
    assert(!dialog.isOpen());

    auto audio = DecodedAudioBuffer::createSyntheticSine(440.0, 1.0, 44100, 0.8f);
    dialog.open(audio, "vocals_lead.wav");
    assert(dialog.isOpen());
    assert(dialog.getAudioFileName() == "vocals_lead.wav");
    assert(dialog.getCustomTrackName() == "vocals_lead (MIDI)");

    dialog.layout(1280.0f, 800.0f);

    // Test slider dragging: Onset Sensitivity
    PointerEvent pDown;
    pDown.action = PointerAction::Down;
    pDown.x = 200.0f;
    pDown.y = 208.0f; // Inside onset slider
    assert(dialog.handlePointer(pDown));

    PointerEvent pMove;
    pMove.action = PointerAction::Move;
    pMove.x = 400.0f;
    pMove.y = 208.0f;
    dialog.handlePointer(pMove);
    assert(dialog.getOptions().onsetThreshold > 0.05f);

    // Test ESC closes dialog
    assert(dialog.handleKey(256 /* GLFW_KEY_ESCAPE */, 0, 1, 0));
    assert(!dialog.isOpen());
    std::cout << "  -> PASSED: AudioToMidiDialog modal lifecycle and slider hit-testing verified." << std::endl;
}

void testGuiWindowAudioToMidiIntegration() {
    std::cout << "[Test 11/11] GuiWindow subsystem integration and command palette binding..." << std::endl;

    GuiWindow win(1280, 800, "Eatsbits AudioToMidi Test");
    assert(!win.isAudioToMidiDialogOpen());

    // Open via API
    auto audio = DecodedAudioBuffer::createSyntheticSine(440.0, 0.5, 44100, 0.8f);
    win.openAudioToMidiConverter(audio, "solo_guitar.wav");
    assert(win.isAudioToMidiDialogOpen());
    assert(win.getAudioToMidiDialog().getAudioFileName() == "solo_guitar.wav");

    // Close via API
    win.closeAudioToMidiConverter();
    assert(!win.isAudioToMidiDialogOpen());

    // Verify command palette command registered and executable
    auto& cmdPal = win.getCommandPaletteDialog();
    cmdPal.open();
    cmdPal.setQuery("Audio to MIDI");
    assert(cmdPal.getFilteredCount() >= 1);
    cmdPal.executeSelected();
    assert(win.isAudioToMidiDialogOpen());
    win.closeAudioToMidiConverter();
    std::cout << "  -> PASSED: GuiWindow integration and action.audio_to_midi command registered and launched." << std::endl;
}

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << "   EATSBITS AUDIO-TO-MIDI TRANSCRIPTION ENGINE TESTS  " << std::endl;
    std::cout << "======================================================" << std::endl;

    testMonophonicA4SineWave();
    testMonophonicC4Tone();
    testPolyphonicChord();
    testYinMonophonicPitchTracker();
    testSilenceHandling();
    testPercussiveOnsetDetection();
    testCancellationToken();
    testWaveformOverview();
    testChordExtractionIntegration();
    testAudioToMidiDialog();
    testGuiWindowAudioToMidiIntegration();

    std::cout << "======================================================" << std::endl;
    std::cout << "   100% AUDIO-TO-MIDI SUBSYSTEM 1 TESTS PASSED!       " << std::endl;
    std::cout << "======================================================" << std::endl;

    return 0;
}
