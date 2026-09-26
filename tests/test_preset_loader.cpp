#include <iostream>
#include <cassert>
#include <cmath>
#include "eatsbits/project/preset_loader.hpp"
#include "eatsbits/ui/gui_window.hpp"
#include "eatsbits/audio/audio_engine.hpp"

using namespace eatsbits;
using namespace eatsbits::project;

void testBuiltinPresets() {
    std::cout << "[Test 1] Built-in Presets Verification..." << std::endl;
    auto presets = PresetLoader::getBuiltinPresets();
    assert(presets.size() == 13);

    // 1. Verify TB-303 Preset
    auto& p303 = presets[0];
    assert(p303.metadata.id == "eats_303");
    assert(p303.metadata.name == "EATS-303 ACID BASSLINE");
    assert(p303.params.count("Cutoff") == 1);
    assert(p303.params.count("Resonance") == 1);
    assert(p303.params.count("Waveform") == 1);
    assert(p303.findParam("Cutoff")->defaultVal == 1400.0f);
    assert(p303.guiRoot.background == "minimal_white");

    // 2. Verify C64 SID Preset
    auto& pSid = presets[1];
    assert(pSid.metadata.id == "c64_sid_synth");
    assert(pSid.metadata.name == "COMMODORE 64 SID SYNTH");
    assert(pSid.metadata.engineId == "sid");
    assert(pSid.params.count("Waveform") == 1);
    assert(pSid.params.count("PulseWidth") == 1);
    assert(pSid.params.count("Cutoff") == 1);
    assert(pSid.params.count("Resonance") == 1);
    assert(pSid.params.count("Attack") == 1);
    assert(pSid.guiRoot.background == "c64_breadbin");

    // 3. Verify Yamaha DX7 Preset
    auto& pDx7 = presets[2];
    assert(pDx7.metadata.id == "dx7_epiano");
    assert(pDx7.metadata.name == "YAMAHA DX7 E.PIANO 1");
    assert(pDx7.metadata.engineId == "dx7");
    assert(pDx7.params.count("Algorithm") == 1);
    assert(pDx7.params.count("Feedback") == 1);
    assert(pDx7.params.count("Brightness") == 1);
    assert(pDx7.params.count("TineBell") == 1);
    assert(pDx7.guiRoot.background == "dx7_faceplate");

    // 4. Verify Super Nintendo S-DSP Preset
    auto& pSnes = presets[3];
    assert(pSnes.metadata.id == "snes_dsp");
    assert(pSnes.metadata.name == "SUPER NINTENDO S-DSP");
    assert(pSnes.metadata.engineId == "snes");
    assert(pSnes.params.count("Waveform") == 1);
    assert(pSnes.params.count("EchoDelay") == 1);
    assert(pSnes.params.count("EchoFeedback") == 1);
    assert(pSnes.guiRoot.background == "snes_faceplate");

    // 5. Verify Sega Genesis YM2612 Preset
    auto& pYm = presets[4];
    assert(pYm.metadata.id == "genesis_ym2612");
    assert(pYm.metadata.name == "SEGA GENESIS YM2612");
    assert(pYm.metadata.engineId == "ym2612");
    assert(pYm.params.count("Algorithm") == 1);
    assert(pYm.params.count("Feedback") == 1);
    assert(pYm.params.count("Op1TL") == 1);
    assert(pYm.guiRoot.background == "ym2612_faceplate");

    // 6. Verify Acoustic Convolution Reverb Preset
    auto& pConv = presets[5];
    assert(pConv.metadata.id == "convolver_space");
    assert(pConv.metadata.name == "ACOUSTIC CONVOLUTION REVERB");
    assert(pConv.metadata.engineId == "convolver");
    assert(pConv.params.count("Preset") == 1);
    assert(pConv.params.count("PreDelay") == 1);
    assert(pConv.params.count("Mix") == 1);
    assert(pConv.guiRoot.background == "convolver_faceplate");

    // 7. Verify 808 Kick Preset
    auto& p808 = presets[6];
    assert(p808.metadata.id == "analog_808_kick");
    assert(p808.params.count("Tune") == 1);
    assert(p808.params.count("Decay") == 1);
    assert(p808.guiRoot.children.size() >= 2); // Nixie row + Knob row

    // 8. Verify 909 Snare Preset
    auto& p909 = presets[7];
    assert(p909.metadata.id == "analog_909_snare");
    assert(p909.params.count("Snappy") == 1);

    // 9. Verify Stereo Delay Preset
    auto& pDelay = presets[8];
    assert(pDelay.metadata.id == "stereo_delay");
    assert(pDelay.params.count("TimeMs") == 1);
    assert(pDelay.params.count("Feedback") == 1);

    // 10. Verify Concert Grand Piano Preset
    auto& pPiano = presets[9];
    assert(pPiano.metadata.id == "concert_grand_piano");
    assert(pPiano.metadata.engineId == "piano_physical");
    assert(pPiano.params.count("HammerHardness") == 1);
    assert(pPiano.params.count("Brightness") == 1);
    assert(pPiano.params.count("Sustain") == 1);

    // 11. Verify Upright Double Bass Preset
    auto& pBass = presets[10];
    assert(pBass.metadata.id == "upright_bass");
    assert(pBass.metadata.engineId == "upright_bass");
    assert(pBass.params.count("FingerMass") == 1);
    assert(pBass.params.count("SlapClick") == 1);

    // 12. Verify Spanish Classical Guitar Preset
    auto& pSpanish = presets[11];
    assert(pSpanish.metadata.id == "spanish_guitar");
    assert(pSpanish.metadata.engineId == "spanish_guitar");
    assert(pSpanish.params.count("FleshNail") == 1);

    // 13. Verify Steel Acoustic Guitar Preset
    auto& pSteel = presets[12];
    assert(pSteel.metadata.id == "acoustic_steel_guitar");
    assert(pSteel.metadata.engineId == "acoustic_steel_guitar");
    assert(pSteel.params.count("PickBite") == 1);

    std::cout << "  -> PASSED: All 13 canonical built-in presets verified." << std::endl;
}

void testEatscriptParsing() {
    std::cout << "[Test 2] Eatscript Script Parser & DSL Extraction..." << std::endl;
    const std::string script = R"(
# @id: test_lead
# @name: Test Cyber Lead
# @category: instrument
# @description: High-resonance test synthesizer.
# @engine: poly_synth

def init():
    return {
        "Cutoff": eat.param("Cutoff", 100.0, 10000.0, 2500.0, step=0.0, unit="Hz"),
        "Resonance": eat.param("Resonance", 0.5, 10.0, 4.2),
        "Wave": eat.param("Wave", 0.0, 3.0, 1.0, step=1.0, allow_variance=False),
    }

def gui():
    return {
        "panel": {
            "title": "TEST CYBER LEAD",
            "subtitle": "Polyphonic Virtual Analog",
            "background": "silver",
            "accent": "#00FFE0",
            "knobStyle": "chromeFluted",
            "layout": [
                {
                    "type": "row",
                    "align": "space_around",
                    "children": [
                        {
                            "type": "knob",
                            "param": "Cutoff",
                            "label": "CUTOFF",
                            "size": 52,
                            "style": "chromeFluted",
                            "unit": "Hz"
                        },
                        {
                            "type": "knob",
                            "param": "Resonance",
                            "label": "RESO",
                            "size": 52
                        }
                    ]
                }
            ]
        }
    }
)";

    PresetDefinition def = PresetLoader::parseFromEatscript(script);
    assert(def.metadata.id == "test_lead");
    assert(def.metadata.name == "Test Cyber Lead");
    assert(def.metadata.engineId == "poly_synth");
    assert(def.params.size() == 3);

    auto* pCut = def.findParam("Cutoff");
    assert(pCut != nullptr);
    assert(pCut->minVal == 100.0f);
    assert(pCut->maxVal == 10000.0f);
    assert(pCut->defaultVal == 2500.0f);
    assert(pCut->unit == "Hz");

    auto* pWave = def.findParam("Wave");
    assert(pWave != nullptr);
    assert(pWave->step == 1.0f);
    assert(!pWave->allowVariance);

    // Verify GUI Tree
    assert(def.guiRoot.title == "TEST CYBER LEAD");
    assert(def.guiRoot.background == "silver");
    assert(def.guiRoot.accent == "#00FFE0");
    assert(def.guiRoot.children.size() == 1);
    assert(def.guiRoot.children[0].type == GuiNodeType::Row);
    assert(def.guiRoot.children[0].children.size() == 2);
    assert(def.guiRoot.children[0].children[0].paramName == "Cutoff");
    assert(def.guiRoot.children[0].children[1].paramName == "Resonance");

    std::cout << "  -> PASSED: Script metadata, parameters, and declarative GUI parsed accurately." << std::endl;
}

void testLayoutComputation() {
    std::cout << "[Test 3] Declarative Flex Layout Bounds Computation..." << std::endl;
    auto p = PresetLoader::createEats303Preset();
    PresetLoader::computeLayoutBounds(p.guiRoot, 40.0f, 75.0f, 1200.0f, 650.0f);

    assert(p.guiRoot.boundsX == 40.0f);
    assert(p.guiRoot.boundsY == 75.0f);
    assert(p.guiRoot.boundsW == 1200.0f);
    assert(p.guiRoot.boundsH == 650.0f);

    assert(!p.guiRoot.children.empty());
    auto& row = p.guiRoot.children[0];
    assert(row.boundsY > 75.0f); // Space for top header banner
    assert(row.children.size() == 8); // 8 knobs

    // Verify each child knob has positive width and non-overlapping X bounds
    for (size_t i = 0; i < row.children.size(); ++i) {
        assert(row.children[i].boundsW > 0.0f);
        if (i > 0) {
            assert(row.children[i].boundsX > row.children[i - 1].boundsX);
        }
    }

    std::cout << "  -> PASSED: Flex layout hierarchy bounds calculated successfully." << std::endl;
}

void testParamNormalizedMapping() {
    std::cout << "[Test 4] Parameter Normalized Conversion & Step Snapping..." << std::endl;
    PresetParam p{"Cutoff", 200.0f, 4500.0f, 1400.0f, 1400.0f, 0.0f, "Hz", true};
    float norm = p.getNormalized();
    assert(std::abs(norm - (1400.0f - 200.0f) / (4500.0f - 200.0f)) < 1e-4f);

    p.setNormalized(0.0f);
    assert(p.currentVal == 200.0f);
    p.setNormalized(1.0f);
    assert(p.currentVal == 4500.0f);
    p.setNormalized(0.5f);
    assert(p.currentVal == 2350.0f);

    // Stepped parameter (e.g. Octave -2 to 0 in step 1.0)
    PresetParam pStep{"Octave", -2.0f, 0.0f, 0.0f, 0.0f, 1.0f, "", false};
    pStep.setNormalized(0.25f);
    assert(pStep.currentVal == -2.0f || pStep.currentVal == -1.0f);
    pStep.setNormalized(0.6f);
    assert(pStep.currentVal == -1.0f);
    pStep.setNormalized(0.95f);
    assert(pStep.currentVal == 0.0f);

    std::cout << "  -> PASSED: Continuous and stepped parameter conversions verified." << std::endl;
}

void testGuiWindowIntegration() {
    std::cout << "[Test 5] GuiWindow Hardware Panel & Parameter Dispatch..." << std::endl;
    audio::AudioEngine engine;
    assert(engine.initialize(48000, 128));

    ui::GuiWindow gui(1280, 800, "Eatsbits Test");
    assert(gui.initialize(engine));

    // 1. Check loaded presets
    const auto* activePreset = gui.getActivePreset();
    assert(activePreset != nullptr);
    assert(activePreset->metadata.id == "eats_303");

    // 2. Switch to HardwarePanel view
    gui.setActiveView(ui::WorkspaceView::HardwarePanel);
    assert(gui.getActiveView() == ui::WorkspaceView::HardwarePanel);

    // 3. Test Next & Prev preset cycling
    gui.nextPreset();
    assert(gui.getActivePreset()->metadata.id == "c64_sid_synth");
    gui.nextPreset();
    assert(gui.getActivePreset()->metadata.id == "dx7_epiano");
    gui.nextPreset();
    assert(gui.getActivePreset()->metadata.id == "snes_dsp");
    gui.nextPreset();
    assert(gui.getActivePreset()->metadata.id == "genesis_ym2612");
    gui.nextPreset();
    assert(gui.getActivePreset()->metadata.id == "convolver_space");
    gui.nextPreset();
    assert(gui.getActivePreset()->metadata.id == "analog_808_kick");
    gui.nextPreset();
    assert(gui.getActivePreset()->metadata.id == "analog_909_snare");
    gui.prevPreset();
    assert(gui.getActivePreset()->metadata.id == "analog_808_kick");
    gui.prevPreset();
    assert(gui.getActivePreset()->metadata.id == "convolver_space");
    gui.prevPreset();
    assert(gui.getActivePreset()->metadata.id == "genesis_ym2612");
    gui.prevPreset();
    assert(gui.getActivePreset()->metadata.id == "snes_dsp");
    gui.prevPreset();
    assert(gui.getActivePreset()->metadata.id == "dx7_epiano");
    gui.prevPreset();
    assert(gui.getActivePreset()->metadata.id == "c64_sid_synth");
    gui.prevPreset();
    assert(gui.getActivePreset()->metadata.id == "eats_303");

    // 4. Hit-test a knob on the TB-303 panel
    // Find Cutoff knob position
    auto& cutKnob = gui.getActivePreset()->guiRoot.children[0].children[2];
    float hitX = cutKnob.boundsX + cutKnob.boundsW * 0.5f;
    float hitY = cutKnob.boundsY + cutKnob.boundsH * 0.5f - 8.0f;

    auto hit = gui.hitTestHardwareKnob(hitX, hitY);
    assert(hit.hit);
    assert(hit.paramName == "Cutoff");

    // 5. Test Mouse Dragging Sweeping Cutoff Parameter
    gui.onMouseDown(0, hitX, hitY);
    assert(gui.getDragMode() == ui::DragMode::HardwareKnob);
    // Drag upwards by 80px -> +0.5 normalized increase
    gui.onMouseMove(hitX, hitY - 80.0f);
    gui.onMouseUp(0, hitX, hitY - 80.0f);
    assert(gui.getDragMode() == ui::DragMode::None);

    auto* pCut = gui.getActivePreset()->findParam("Cutoff");
    assert(pCut != nullptr);
    assert(pCut->currentVal > 1400.0f); // Cutoff swept upwards!

    // 6. Test Track Selection & Preset Sync
    gui.setSelectedTrackIndex(1); // Track 1: TR-808 Drums
    assert(gui.getActivePreset()->metadata.id == "analog_808_kick");
    gui.setSelectedTrackIndex(3); // Track 3: Poly Lead (DX7)
    assert(gui.getActivePreset()->metadata.id == "dx7_epiano");
    gui.setSelectedTrackIndex(0); // Track 0: TB-303 Acid
    assert(gui.getActivePreset()->metadata.id == "eats_303");

    std::cout << "  -> PASSED: GuiWindow hardware panel, hit testing, knob sweeping, and track sync fully integrated." << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "  Eatsbits Preset Loader & Declarative GUI Tests  " << std::endl;
    std::cout << "==================================================" << std::endl;

    testBuiltinPresets();
    testEatscriptParsing();
    testLayoutComputation();
    testParamNormalizedMapping();
    testGuiWindowIntegration();

    std::cout << "==================================================" << std::endl;
    std::cout << "  100% PRESET LOADER & DECLARATIVE GUI TESTS PASS " << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
