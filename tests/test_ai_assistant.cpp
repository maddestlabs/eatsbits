#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include "eatsbits/ai/gemini_client.hpp"
#include "eatsbits/ai/ai_mixing_engine.hpp"
#include "eatsbits/ui/widgets/ai_assistant_dialog.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"

using namespace eatsbits;
using namespace eatsbits::ai;
using namespace eatsbits::ui;
using namespace eatsbits::sequencer;

void testGeminiClientCore() {
    std::cout << "[Test 1] Testing Gemini API Client Bridge & Offline Mock Mode..." << std::endl;

    GeminiConfig cfg;
    cfg.apiKey = "AIzaSyFakeKeyForTesting1234567890";
    cfg.model = "gemini-2.5-flash";
    cfg.offlineMockMode = true; // Use deterministic offline engine

    GeminiClient client(cfg);
    assert(client.hasApiKey());
    assert(client.getModel() == "gemini-2.5-flash");

    // 1. Connection test
    GeminiResponse connResp = client.testConnection();
    assert(connResp.success);
    assert(connResp.statusCode == 200);
    (void)connResp;

    // 2. EatScript generation
    std::string script = client.generateEatscript("Fat analog 303 acid lead", "instrument");
    assert(!script.empty());
    assert(script.find("function process") != std::string::npos);
    assert(script.find("sampleRate") != std::string::npos);

    // Audio FX script
    std::string fxScript = client.generateEatscript("Stereo warm saturation", "audio_fx");
    assert(!fxScript.empty());
    assert(fxScript.find("math.tanh") != std::string::npos);

    // 3. Song blueprint generation
    std::string blueprint = client.generateSongBlueprint("Cyberpunk Electro");
    assert(!blueprint.empty());
    assert(blueprint.find("Cyberpunk") != std::string::npos);
    assert(blueprint.find("sections") != std::string::npos);

    std::cout << "  -> Gemini Client tests passed successfully!" << std::endl;
}

void testMixTelemetryExtraction() {
    std::cout << "[Test 2] Testing Mix Telemetry Extraction from StepSequencer..." << std::endl;

    StepSequencer seq;
    assert(seq.getNumTracks() == 0);

    size_t kickIdx = seq.addTrack("808 Kick Drum", 1, 16);
    size_t bassIdx = seq.addTrack("Acid 303 Bass", 2, 16);
    size_t leadIdx = seq.addTrack("Skyline Lead", 3, 16);
    size_t padIdx  = seq.addTrack("Warm Poly Pad", 4, 16);

    auto* kickTrk = seq.getTrack(kickIdx);
    auto* bassTrk = seq.getTrack(bassIdx);
    auto* leadTrk = seq.getTrack(leadIdx);
    auto* padTrk  = seq.getTrack(padIdx);
    (void)padTrk;

    assert(kickTrk && bassTrk && leadTrk && padTrk);

    // Add some steps
    StepData s0; s0.active = true; s0.note = 36; kickTrk->setStep(0, s0); kickTrk->setStep(4, s0);
    StepData sB; sB.active = true; sB.note = 48; bassTrk->setStep(0, sB); bassTrk->setStep(2, sB);
    StepData sL; sL.active = true; sL.note = 72; leadTrk->setStep(0, sL);

    MixTelemetry telem = AiMixingEngine::extractTelemetry(seq, "Synthwave", -14.0f);
    assert(telem.tracks.size() == 4);
    assert(telem.genre == "Synthwave");
    assert(std::abs(telem.targetLufs - (-14.0f)) < 0.01f);

    // Verify instrument classification
    assert(telem.tracks[0].isDrumTrack);
    assert(telem.tracks[0].activeNotes == 2);

    assert(telem.tracks[1].isBassTrack);
    assert(telem.tracks[1].activeNotes == 2);

    assert(telem.tracks[2].isLeadTrack);
    assert(telem.tracks[2].activeNotes == 1);

    assert(telem.tracks[3].isPadTrack);
    assert(telem.tracks[3].activeNotes == 0);

    std::cout << "  -> Mix telemetry extraction verified!" << std::endl;
}

void testOfflineMixPatchComputation() {
    std::cout << "[Test 3] Testing Algorithmic Offline Gain-Staging & Spectral Unmasking..." << std::endl;

    StepSequencer seq;
    seq.addTrack("Kick Drum", 1, 16);
    seq.addTrack("Sub Bass", 2, 16);
    seq.addTrack("Main Lead", 3, 16);
    seq.addTrack("Backing Strings Pad", 4, 16);

    MixTelemetry telem = AiMixingEngine::extractTelemetry(seq, "Synthwave", -14.0f);
    json::Value patch = AiMixingEngine::computeOfflineMixPatch(telem);

    assert(patch.isObject());
    assert(patch.contains("tracks"));
    assert(patch.contains("master"));

    const auto& trks = patch["tracks"];
    (void)trks;
    // Track 0 (Kick): Volume should be loud (~0.88), pan 0.0
    assert(trks["0"]["volume"].asDouble() > 0.85);
    assert(std::abs(trks["0"]["pan"].asDouble()) < 0.01);

    // Track 1 (Bass): Volume ~0.82, pan 0.0 (strictly mono), low gain boost
    assert(trks["1"]["volume"].asDouble() > 0.80);
    assert(std::abs(trks["1"]["pan"].asDouble()) < 0.01);
    assert(trks["1"]["eq"]["lowGain"].asDouble() > 0.0);

    // Track 2 (Lead): HPF set to clean bass headroom (~160 Hz)
    assert(trks["2"]["eq"]["hpf"].asDouble() >= 120.0);

    // Track 3 (Pad): HPF set and stereo spread applied
    assert(trks["3"]["eq"]["hpf"].asDouble() >= 150.0);
    assert(std::abs(trks["3"]["pan"].asDouble()) > 0.10);

    // Master Bus: subCut at 28 Hz, limiter ceiling -0.3 dBFS
    const auto& master = patch["master"];
    (void)master;
    assert(std::abs(master["subCut"].asDouble() - 28.0) < 0.1);
    assert(std::abs(master["ceilingDbfs"].asDouble() - (-0.3)) < 0.05);

    std::cout << "  -> Algorithmic mix patch calculation verified!" << std::endl;
}

void testApplyMixPatchToSequencer() {
    std::cout << "[Test 4] Testing Direct Mix Patch Application to StepSequencer..." << std::endl;

    StepSequencer seq;
    size_t t0 = seq.addTrack("Kick", 1, 16);
    size_t t1 = seq.addTrack("Pad", 2, 16);

    auto* trk0 = seq.getTrack(t0);
    auto* trk1 = seq.getTrack(t1);

    // Initial volumes
    trk0->setVolume(0.5f);
    trk1->setVolume(0.5f);

    json::Object patchRoot;
    json::Object tracksObj;

    json::Object trk0Data;
    trk0Data["volume"] = 0.90;
    trk0Data["pan"] = 0.0;
    tracksObj["0"] = json::Value{trk0Data};

    json::Object trk1Data;
    trk1Data["volume"] = 0.65;
    trk1Data["pan"] = -0.30;
    tracksObj["1"] = json::Value{trk1Data};

    patchRoot["tracks"] = json::Value{tracksObj};

    int modified = AiMixingEngine::applyMixPatch(seq, json::Value{patchRoot});
    (void)modified;
    assert(modified == 2);

    assert(std::abs(trk0->getVolume() - 0.90f) < 1e-4f);
    assert(std::abs(trk0->getPan() - 0.0f) < 1e-4f);

    assert(std::abs(trk1->getVolume() - 0.65f) < 1e-4f);
    assert(std::abs(trk1->getPan() - (-0.30f)) < 1e-4f);

    std::cout << "  -> Track parameters applied successfully!" << std::endl;
}

void testRunAutoMixMasterEndToEnd() {
    std::cout << "[Test 5] Testing End-to-End runAutoMixMaster Workflow..." << std::endl;

    StepSequencer seq;
    seq.addTrack("Techno Kick", 1, 16);
    seq.addTrack("Acid 303", 2, 16);
    seq.addTrack("Vocal Hook", 3, 16);

    GeminiConfig cfg;
    cfg.offlineMockMode = true;
    GeminiClient client(cfg);

    AiMixResult res = AiMixingEngine::runAutoMixMaster(seq, client, "Acid Techno", -9.0f, "Heavy sub punch");
    assert(res.success);
    assert(res.tracksAdjusted == 3);
    assert(!res.summary.empty());
    assert(res.trackAdjustments.size() == 3);
    assert(std::abs(res.masterAdjustment.targetLufs - (-9.0f)) < 0.1f);

    std::cout << "  -> End-to-end auto-mixing pass verified!" << std::endl;
}

void testAiAssistantDialog() {
    std::cout << "[Test 6] Testing AiAssistantDialog UI state & navigation..." << std::endl;

    AiAssistantDialog dialog;
    assert(!dialog.isOpen());

    dialog.open(AiDialogTab::AutoMix);
    assert(dialog.isOpen());
    assert(dialog.getActiveTab() == AiDialogTab::AutoMix);

    dialog.layout(1280.0f, 720.0f);
    assert(dialog.getBounds().w > 400.0f);
    assert(dialog.getBounds().h > 300.0f);

    // Test tab switching
    dialog.setActiveTab(AiDialogTab::SoundDesign);
    assert(dialog.getActiveTab() == AiDialogTab::SoundDesign);

    dialog.setActiveTab(AiDialogTab::Settings);
    assert(dialog.getActiveTab() == AiDialogTab::Settings);

    // Test client model
    dialog.getClient().setModel("gemini-2.5-flash");
    assert(dialog.getClient().getModel() == "gemini-2.5-flash");

    dialog.close();
    assert(!dialog.isOpen());

    std::cout << "  -> AiAssistantDialog verified!" << std::endl;
}

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << " Running Eatsbits Subsystem 7: AI Assistant Tests" << std::endl;
    std::cout << "======================================================" << std::endl;

    testGeminiClientCore();
    testMixTelemetryExtraction();
    testOfflineMixPatchComputation();
    testApplyMixPatchToSequencer();
    testRunAutoMixMasterEndToEnd();
    testAiAssistantDialog();

    std::cout << "======================================================" << std::endl;
    std::cout << " ALL 6 AI ASSISTANT & AUTO-MIX TESTS PASSED (100%)!  " << std::endl;
    std::cout << "======================================================" << std::endl;
    return 0;
}
