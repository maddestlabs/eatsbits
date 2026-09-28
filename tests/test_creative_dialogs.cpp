#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <array>
#include <string>
#include "eatsbits/ui/widgets/waveshaper_dialog.hpp"
#include "eatsbits/sequencer/note_splitter_engine.hpp"
#include "eatsbits/ui/widgets/note_splitter_dialog.hpp"
#include "eatsbits/ui/widgets/color_picker_dialog.hpp"
#include "eatsbits/ui/widgets/space_visualizer_widget.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"

using namespace eatsbits;
using namespace eatsbits::ui;
using namespace eatsbits::sequencer;

static eatscript::MidiNote makeMidiNote(uint8_t pitch, float vel = 0.85f, float start = 0.0f, float dur = 1.0f) {
    eatscript::MidiNote n;
    n.id = "n_" + std::to_string(pitch);
    n.pitch = pitch;
    n.velocity = vel;
    n.startStep = start;
    n.durationSteps = dur;
    return n;
}

void testWaveshaperTransferCurves() {
    std::cout << "[Test 1] Testing Waveshaper transfer curve functions & harmonics..." << std::endl;

    WaveshaperParams params;
    params.shape = WaveshaperShape::SoftSaturation;
    params.preGain = 1.0f;
    params.postGain = 1.0f;
    params.tension = 0.0f;

    // 1. Zero crossing
    float zeroVal = WaveshaperDialog::evaluateTransfer(params, 0.0f);
    assert(std::abs(zeroVal) < 1e-5f);
    (void)zeroVal;

    // 2. Soft Saturation symmetry: f(-x) == -f(x)
    float posVal = WaveshaperDialog::evaluateTransfer(params, 0.5f);
    float negVal = WaveshaperDialog::evaluateTransfer(params, -0.5f);
    assert(std::abs(posVal + negVal) < 1e-4f);
    assert(posVal > 0.3f && posVal < 0.6f);
    (void)posVal;
    (void)negVal;

    // 3. Tube Asymmetric is asymmetric: f(-x) != -f(x)
    params.shape = WaveshaperShape::TubeAsymmetric;
    float tubePos = WaveshaperDialog::evaluateTransfer(params, 0.6f);
    float tubeNeg = WaveshaperDialog::evaluateTransfer(params, -0.6f);
    assert(std::abs(tubePos + tubeNeg) > 0.05f);
    (void)tubePos;
    (void)tubeNeg;

    // 4. Sine Wavefold
    params.shape = WaveshaperShape::SineWavefold;
    params.preGain = 2.0f;
    float fold1 = WaveshaperDialog::evaluateTransfer(params, 0.5f);
    float fold2 = WaveshaperDialog::evaluateTransfer(params, 1.0f);
    assert(std::isfinite(fold1) && std::isfinite(fold2));
    (void)fold1;
    (void)fold2;

    // 5. Angry multi-fold
    params.shape = WaveshaperShape::Angry1;
    float angryVal = WaveshaperDialog::evaluateTransfer(params, 0.8f);
    assert(std::abs(angryVal) <= 1.05f);
    (void)angryVal;

    // 6. WaveshaperDialog state & harmonic spectrum
    WaveshaperDialog dialog;
    assert(!dialog.isOpen());
    dialog.open(params);
    assert(dialog.isOpen());

    const auto& harmonics = dialog.getHarmonics();
    // Fundamental (harmonic 0) should have high energy
    assert(harmonics[0] > 0.1f);
    for (size_t i = 0; i < 8; ++i) {
        assert(harmonics[i] >= 0.0f && harmonics[i] <= 1.5f);
    }
    (void)harmonics;

    dialog.close();
    assert(!dialog.isOpen());
    std::cout << "  -> Waveshaper transfer curve tests passed!" << std::endl;
}

void testNoteSplitterThreeWayVoice() {
    std::cout << "[Test 2] Testing NoteSplitter 3-Way Voice Skyline / Chords / Bass..." << std::endl;

    std::vector<eatscript::MidiNote> notes;
    // Bass note: C2 (pitch 36)
    notes.push_back(makeMidiNote(36, 0.9f, 0.0f, 1.0f));

    // Chord cluster at step 0: C3 (48), E3 (52), G3 (55)
    notes.push_back(makeMidiNote(48, 0.8f, 0.0f, 1.0f));
    notes.push_back(makeMidiNote(52, 0.8f, 0.0f, 1.0f));
    notes.push_back(makeMidiNote(55, 0.8f, 0.0f, 1.0f));

    // Lead skyline note at step 0: C5 (72)
    notes.push_back(makeMidiNote(72, 0.95f, 0.0f, 1.0f));

    auto stems = NoteSplitterEngine::split3WayVoice(notes, 48, 64);
    assert(stems.size() == 3);

    // Stem 0: Bass
    assert(stems[0].name.find("Bass") != std::string::npos);
    assert(stems[0].noteCount == 1);
    assert(stems[0].notes[0].pitch == 36);

    // Stem 1: Harmony / Chords
    assert(stems[1].name.find("Harmony") != std::string::npos || stems[1].name.find("Chords") != std::string::npos);
    assert(stems[1].noteCount == 3); // 48, 52, 55

    // Stem 2: Lead / Skyline
    assert(stems[2].name.find("Lead") != std::string::npos || stems[2].name.find("Skyline") != std::string::npos);
    assert(stems[2].noteCount == 1);
    assert(stems[2].notes[0].pitch == 72);

    std::cout << "  -> 3-Way Voice Skyline separation passed!" << std::endl;
}

void testNoteSplitterBassTrebleAndSATB() {
    std::cout << "[Test 3] Testing 2-Way Piano Split & 4-Voice SATB Distribution..." << std::endl;

    std::vector<eatscript::MidiNote> notes;
    notes.push_back(makeMidiNote(36, 0.8f, 0.0f, 1.0f)); // C2
    notes.push_back(makeMidiNote(48, 0.8f, 0.0f, 1.0f)); // C3
    notes.push_back(makeMidiNote(60, 0.8f, 0.0f, 1.0f)); // C4
    notes.push_back(makeMidiNote(67, 0.8f, 0.0f, 1.0f)); // G4
    notes.push_back(makeMidiNote(72, 0.8f, 0.0f, 1.0f)); // C5

    // 2-Way Piano split at pivot 60 (Middle C)
    auto pianoStems = NoteSplitterEngine::splitBassTreble(notes, 60);
    assert(pianoStems.size() == 2);
    // Left hand (< 60): 36, 48
    assert(pianoStems[0].noteCount == 2);
    assert(pianoStems[0].maxPitch < 60);
    // Right hand (>= 60): 60, 67, 72
    assert(pianoStems[1].noteCount == 3);
    assert(pianoStems[1].minPitch >= 60);

    // 4-Voice SATB split on a 4-note chord: C3(48), G3(55), E4(64), C5(72)
    std::vector<eatscript::MidiNote> satbChord = {
        makeMidiNote(48, 0.8f, 0.0f, 1.0f),
        makeMidiNote(55, 0.8f, 0.0f, 1.0f),
        makeMidiNote(64, 0.8f, 0.0f, 1.0f),
        makeMidiNote(72, 0.8f, 0.0f, 1.0f)
    };
    auto satbStems = NoteSplitterEngine::split4VoicePolyphony(satbChord);
    assert(satbStems.size() == 4);
    // Soprano (highest): 72
    assert(satbStems[0].notes[0].pitch == 72);
    // Alto: 64
    assert(satbStems[1].notes[0].pitch == 64);
    // Tenor: 55
    assert(satbStems[2].notes[0].pitch == 55);
    // Bass (lowest): 48
    assert(satbStems[3].notes[0].pitch == 48);

    std::cout << "  -> Piano Clef & SATB polyphonic distribution passed!" << std::endl;
}

void testNoteSplitterDrumDemux() {
    std::cout << "[Test 4] Testing GM Drum Demuxer..." << std::endl;

    std::vector<eatscript::MidiNote> drumTrack = {
        makeMidiNote(36, 0.9f, 0.0f, 0.25f), // Kick 1
        makeMidiNote(35, 0.9f, 2.0f, 0.25f), // Acoustic Bass Drum
        makeMidiNote(38, 0.8f, 1.0f, 0.25f), // Snare 1
        makeMidiNote(39, 0.7f, 3.0f, 0.25f), // Hand Clap
        makeMidiNote(42, 0.6f, 0.5f, 0.25f), // Closed Hi-Hat
        makeMidiNote(46, 0.6f, 1.5f, 0.25f), // Open Hi-Hat
        makeMidiNote(49, 0.8f, 0.0f, 0.5f),  // Crash Cymbal 1
        makeMidiNote(45, 0.7f, 2.5f, 0.25f), // Low Tom
        makeMidiNote(50, 0.7f, 3.5f, 0.25f)  // High Tom
    };

    auto drumStems = NoteSplitterEngine::splitDrumPercussion(drumTrack);
    assert(drumStems.size() == 4);

    // Stem 0: Kick (notes 35, 36)
    assert(drumStems[0].name.find("Kick") != std::string::npos);
    assert(drumStems[0].noteCount == 2);

    // Stem 1: Snare & Claps (notes 38, 39)
    assert(drumStems[1].name.find("Snare") != std::string::npos);
    assert(drumStems[1].noteCount == 2);

    // Stem 2: Hi-Hats & Cymbals (notes 42, 46, 49)
    assert(drumStems[2].name.find("Hat") != std::string::npos);
    assert(drumStems[2].noteCount == 3);

    // Stem 3: Percussion & Toms (notes 45, 50)
    assert(drumStems[3].name.find("Perc") != std::string::npos || drumStems[3].name.find("Tom") != std::string::npos);
    assert(drumStems[3].noteCount == 2);

    std::cout << "  -> Drum Demuxer stem separation passed!" << std::endl;
}

void testNoteSplitterSequencerIntegration() {
    std::cout << "[Test 5] Testing StepSequencer track creation via NoteSplitterEngine..." << std::endl;

    StepSequencer seq;
    assert(seq.getNumTracks() == 0);

    // Add source track with mixed piano notes
    size_t srcIdx = seq.addTrack("Piano Master", 1, 16);
    auto* trk = seq.getTrack(srcIdx);
    assert(trk != nullptr);

    // Add step notes
    StepData s0; s0.active = true; s0.note = 36; s0.velocity = 0.9f; trk->setStep(0, s0);
    StepData s4; s4.active = true; s4.note = 72; s4.velocity = 0.8f; trk->setStep(4, s4);

    SplitParams params;
    params.pivotPitch = 60;

    auto newTrackIndices = NoteSplitterEngine::applySplitToSequencer(
        seq, srcIdx, SplitMode::BassTrebleClefs, params, false
    );

    assert(newTrackIndices.size() == 2);
    // Source track preserved + 2 split tracks = 3 tracks total
    assert(seq.getNumTracks() == 3);

    auto* leftHandTrk = seq.getTrack(newTrackIndices[0]);
    assert(leftHandTrk != nullptr);
    assert(leftHandTrk->getStep(0).active && leftHandTrk->getStep(0).note == 36);
    (void)leftHandTrk;

    auto* rightHandTrk = seq.getTrack(newTrackIndices[1]);
    assert(rightHandTrk != nullptr);
    assert(rightHandTrk->getStep(4).active && rightHandTrk->getStep(4).note == 72);
    (void)rightHandTrk;

    std::cout << "  -> StepSequencer integration passed!" << std::endl;
}

void testColorPickerDialog() {
    std::cout << "[Test 6] Testing ColorPickerDialog HSL/RGB conversions & hex strings..." << std::endl;

    // 1. Red (H=0, S=1, L=0.5)
    Color red = ColorPickerDialog::hslToRgb(0.0f, 1.0f, 0.5f);
    assert(std::abs(red.r - 1.0f) < 1e-3f);
    assert(std::abs(red.g - 0.0f) < 1e-3f);
    assert(std::abs(red.b - 0.0f) < 1e-3f);

    // 2. Green (H=120, S=1, L=0.5)
    Color green = ColorPickerDialog::hslToRgb(120.0f, 1.0f, 0.5f);
    assert(std::abs(green.r - 0.0f) < 1e-3f);
    assert(std::abs(green.g - 1.0f) < 1e-3f);
    assert(std::abs(green.b - 0.0f) < 1e-3f);

    // 3. Blue (H=240, S=1, L=0.5)
    Color blue = ColorPickerDialog::hslToRgb(240.0f, 1.0f, 0.5f);
    assert(std::abs(blue.r - 0.0f) < 1e-3f);
    assert(std::abs(blue.g - 0.0f) < 1e-3f);
    assert(std::abs(blue.b - 1.0f) < 1e-3f);

    // 4. Roundtrip RGB -> HSL -> RGB
    Color originalCol{0.3f, 0.7f, 0.9f, 1.0f};
    float h = 0.0f, s = 0.0f, l = 0.0f;
    ColorPickerDialog::rgbToHsl(originalCol, h, s, l);
    Color reconstructed = ColorPickerDialog::hslToRgb(h, s, l);
    assert(std::abs(originalCol.r - reconstructed.r) < 0.02f);
    assert(std::abs(originalCol.g - reconstructed.g) < 0.02f);
    assert(std::abs(originalCol.b - reconstructed.b) < 0.02f);

    // 5. Hex formatting
    std::string hex = ColorPickerDialog::colorToHex(Color{1.0f, 0.0f, 0.0f, 1.0f});
    assert(hex == "#FF0000");

    // 6. Dialog UI state
    ColorPickerDialog picker;
    assert(!picker.isOpen());
    picker.open(originalCol, "TRACK 2 COLOR", 2);
    assert(picker.isOpen());
    assert(picker.getTargetTrackIndex() == 2);
    picker.setSelectedColor(red);
    assert(std::abs(picker.getSelectedColor().r - 1.0f) < 0.01f);
    picker.close();
    assert(!picker.isOpen());

    std::cout << "  -> ColorPickerDialog tests passed!" << std::endl;
}

void testSpaceVisualizerWidget() {
    std::cout << "[Test 7] Testing SpaceVisualizerWidget Lissajous Goniometer & Room..." << std::endl;

    SpaceVisualizerWidget widget(220.0f);
    assert(widget.getMode() == SpaceVisualizerMode::StereoGoniometer);

    // Test Room coordinates
    widget.setMode(SpaceVisualizerMode::AcousticRoom2_5D);
    assert(widget.getMode() == SpaceVisualizerMode::AcousticRoom2_5D);

    RoomCoordinates coords;
    coords.roomWidth = 10.0f;
    coords.roomLength = 15.0f;
    coords.sourceX = 3.0f;
    widget.setRoomCoords(coords);
    assert(widget.getRoomCoords().roomWidth == 10.0f);

    // Test Goniometer real-time audio feeding: In-phase mono signal
    widget.setMode(SpaceVisualizerMode::StereoGoniometer);

    constexpr size_t N = 128;
    std::array<float, N> monoLeft{};
    std::array<float, N> monoRight{};
    for (size_t i = 0; i < N; ++i) {
        float s = std::sin(2.0f * 3.14159265f * static_cast<float>(i) / 32.0f);
        monoLeft[i] = s;
        monoRight[i] = s;
    }

    widget.feedAudio(monoLeft.data(), monoRight.data(), N);
    const auto& monoMetrics = widget.getStereoMetrics();
    // Phase correlation for identical signals should be close to +1.0
    assert(monoMetrics.phaseCorrelation > 0.85f);
    // Left/Right balance should be centered (0.0)
    assert(std::abs(monoMetrics.balanceLR) < 0.1f);
    (void)monoMetrics;

    // Test Out-of-phase signal (L = -R)
    for (size_t i = 0; i < N; ++i) {
        monoRight[i] = -monoLeft[i];
    }
    widget.feedAudio(monoLeft.data(), monoRight.data(), N);
    const auto& antiMetrics = widget.getStereoMetrics();
    // Phase correlation for opposite signals should be close to -1.0
    assert(antiMetrics.phaseCorrelation < -0.85f);
    (void)antiMetrics;

    // Test Hard Left signal (L > 0, R = 0)
    for (size_t i = 0; i < N; ++i) {
        monoRight[i] = 0.0f;
    }
    widget.feedAudio(monoLeft.data(), monoRight.data(), N);
    const auto& leftMetrics = widget.getStereoMetrics();
    assert(leftMetrics.balanceLR < -0.85f);
    (void)leftMetrics;

    std::cout << "  -> SpaceVisualizer Lissajous Goniometer & Room passed!" << std::endl;
}

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << " Running Eatsbits Subsystem 6: Creative Dialogs Tests" << std::endl;
    std::cout << "======================================================" << std::endl;

    testWaveshaperTransferCurves();
    testNoteSplitterThreeWayVoice();
    testNoteSplitterBassTrebleAndSATB();
    testNoteSplitterDrumDemux();
    testNoteSplitterSequencerIntegration();
    testColorPickerDialog();
    testSpaceVisualizerWidget();

    std::cout << "======================================================" << std::endl;
    std::cout << " ALL 7 CREATIVE DIALOGS & SCOPES TESTS PASSED (100%)! " << std::endl;
    std::cout << "======================================================" << std::endl;
    return 0;
}
