#include "eatsbits/ai/ai_mixing_engine.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace eatsbits::ai {

MixTelemetry AiMixingEngine::extractTelemetry(
    const sequencer::StepSequencer& seq,
    const std::string& genre,
    float targetLufs
) {
    MixTelemetry telem;
    telem.genre = genre;
    telem.bpm = seq.getBpm();
    telem.targetLufs = targetLufs;

    size_t numTracks = seq.getNumTracks();
    for (size_t i = 0; i < numTracks; ++i) {
        const auto* trk = seq.getTrack(i);
        if (!trk) continue;

        TrackChannelTelemetry tTelem;
        tTelem.trackId = std::to_string(i);
        tTelem.name = trk->getName();
        tTelem.volume = trk->getVolume();
        tTelem.pan = trk->getPan();

        // Count active notes
        size_t activeCount = 0;
        for (uint32_t s = 0; s < trk->getNumSteps(); ++s) {
            if (trk->getStep(s).active) {
                activeCount++;
            }
        }
        tTelem.activeNotes = activeCount;

        // Classify track instrument role from name
        std::string lowerName = tTelem.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lowerName.find("drum") != std::string::npos || lowerName.find("kick") != std::string::npos ||
            lowerName.find("snare") != std::string::npos || lowerName.find("hat") != std::string::npos ||
            lowerName.find("808") != std::string::npos || lowerName.find("909") != std::string::npos ||
            lowerName.find("perc") != std::string::npos) {
            tTelem.isDrumTrack = true;
        } else if (lowerName.find("bass") != std::string::npos || lowerName.find("303") != std::string::npos ||
                   lowerName.find("acid") != std::string::npos || lowerName.find("sub") != std::string::npos) {
            tTelem.isBassTrack = true;
        } else if (lowerName.find("lead") != std::string::npos || lowerName.find("solo") != std::string::npos ||
                   lowerName.find("skyline") != std::string::npos || lowerName.find("melody") != std::string::npos) {
            tTelem.isLeadTrack = true;
        } else if (lowerName.find("pad") != std::string::npos || lowerName.find("chord") != std::string::npos ||
                   lowerName.find("string") != std::string::npos || lowerName.find("brass") != std::string::npos ||
                   lowerName.find("harmony") != std::string::npos) {
            tTelem.isPadTrack = true;
        }

        telem.tracks.push_back(tTelem);
    }

    return telem;
}

json::Value AiMixingEngine::computeOfflineMixPatch(const MixTelemetry& telemetry) {
    json::Object root;
    root["genre"] = telemetry.genre;
    root["targetLufs"] = static_cast<double>(telemetry.targetLufs);
    root["summary"] = "Algorithmic surgical gain-staging, high-pass rumble clearing, and master bus limiting.";

    json::Object tracksObj;

    for (size_t i = 0; i < telemetry.tracks.size(); ++i) {
        const auto& t = telemetry.tracks[i];
        json::Object trkData;

        float targetVol = 0.75f;
        float targetPan = 0.0f;

        json::Object eqObj;
        eqObj["enabled"] = true;

        if (t.isDrumTrack) {
            targetVol = 0.88f;
            targetPan = 0.0f;
            eqObj["hpf"] = 30.0;
            eqObj["lowGain"] = 1.0;
            eqObj["midFreq"] = 400.0;
            eqObj["midGain"] = -1.5; // carve boxiness
            eqObj["highGain"] = 1.5; // snare & hat snap
        } else if (t.isBassTrack) {
            targetVol = 0.82f;
            targetPan = 0.0f; // Bass must stay centered
            eqObj["hpf"] = 35.0; // eliminate subsonic rumble
            eqObj["lowGain"] = 1.5;
            eqObj["midFreq"] = 300.0;
            eqObj["midGain"] = -2.0; // avoid clashing with lower-mid instruments
            eqObj["highGain"] = 0.0;
        } else if (t.isLeadTrack) {
            targetVol = 0.78f;
            targetPan = 0.0f;
            eqObj["hpf"] = 160.0; // clean headroom for kick & bass
            eqObj["lowGain"] = -0.5;
            eqObj["midFreq"] = 3200.0;
            eqObj["midGain"] = 1.5; // vocal/lead presence
            eqObj["highGain"] = 1.0;
        } else if (t.isPadTrack) {
            targetVol = 0.68f;
            targetPan = (i % 2 == 0) ? -0.25f : 0.25f; // spread stereo backing
            eqObj["hpf"] = 220.0; // clean lower octaves
            eqObj["lowGain"] = -1.0;
            eqObj["midFreq"] = 1200.0;
            eqObj["midGain"] = -1.0;
            eqObj["highGain"] = 1.2; // air / sheen
        } else {
            // General track
            targetVol = 0.75f;
            targetPan = 0.0f;
            eqObj["hpf"] = 120.0;
            eqObj["lowGain"] = 0.0;
            eqObj["midFreq"] = 1000.0;
            eqObj["midGain"] = 0.0;
            eqObj["highGain"] = 0.0;
        }

        trkData["volume"] = static_cast<double>(targetVol);
        trkData["pan"] = static_cast<double>(targetPan);
        trkData["eq"] = json::Value{eqObj};

        tracksObj[t.trackId] = json::Value{trkData};
    }

    root["tracks"] = json::Value{tracksObj};

    // Master Bus Processing
    json::Object masterObj;
    masterObj["subCut"] = 28.0;
    masterObj["lowGain"] = 0.5;
    masterObj["midFreq"] = 2500.0;
    masterObj["midGain"] = -0.5;
    masterObj["highGain"] = 1.0;
    masterObj["limiterEnabled"] = true;
    masterObj["ceilingDbfs"] = -0.3;
    masterObj["limiterDrive"] = 2.5;
    masterObj["targetLufs"] = static_cast<double>(telemetry.targetLufs);
    root["master"] = json::Value{masterObj};

    return json::Value{root};
}

int AiMixingEngine::applyMixPatch(sequencer::StepSequencer& seq, const json::Value& patchRoot) {
    if (!patchRoot.contains("tracks")) return 0;

    int modifiedCount = 0;
    const auto& tracksVal = patchRoot["tracks"];
    if (!tracksVal.isObject()) return 0;

    const auto& tracksMap = tracksVal.asObject();
    for (const auto& [trackIdStr, dataVal] : tracksMap) {
        if (!dataVal.isObject()) continue;

        size_t trackIdx = 0;
        try {
            trackIdx = static_cast<size_t>(std::stoul(trackIdStr));
        } catch (...) {
            continue;
        }

        auto* trk = seq.getTrack(trackIdx);
        if (!trk) continue;

        if (dataVal.contains("volume")) {
            trk->setVolume(static_cast<float>(dataVal["volume"].asDouble(0.85)));
        }
        if (dataVal.contains("pan")) {
            trk->setPan(static_cast<float>(dataVal["pan"].asDouble(0.0)));
        }

        modifiedCount++;
    }

    return modifiedCount;
}

AiMixResult AiMixingEngine::runAutoMixMaster(
    sequencer::StepSequencer& seq,
    GeminiClient& client,
    const std::string& genre,
    float targetLufs,
    const std::string& customInstructions
) {
    AiMixResult result;

    MixTelemetry telem = extractTelemetry(seq, genre, targetLufs);

    json::Value patchVal;

    if (!client.isOfflineMock() && client.hasApiKey()) {
        std::ostringstream prompt;
        prompt << "Analyze and generate an optimal mix patch for this project:\n"
               << "Genre: " << genre << "\n"
               << "Target LUFS: " << targetLufs << " dB\n";
        if (!customInstructions.empty()) {
            prompt << "Instructions: " << customInstructions << "\n";
        }
        prompt << "Project tracks:\n";
        for (const auto& t : telem.tracks) {
            prompt << "  Track " << t.trackId << " (" << t.name << "): vol=" << t.volume
                   << ", pan=" << t.pan << ", notes=" << t.activeNotes << "\n";
        }
        prompt << "Return ONLY valid JSON matching schema:\n"
               << "{\n"
               << "  \"summary\": \"...\",\n"
               << "  \"tracks\": {\n"
               << "    \"0\": { \"volume\": 0.85, \"pan\": 0.0, \"eq\": { \"hpf\": 30.0 } }\n"
               << "  },\n"
               << "  \"master\": { \"subCut\": 28.0, \"ceilingDbfs\": -0.3, \"targetLufs\": " << targetLufs << " }\n"
               << "}";

        GeminiResponse gResp = client.generateContent(prompt.str(), "You are a professional mastering engineer.", 0.2f, 2048);
        if (gResp.success && !gResp.text.empty()) {
            json::Parser parser(gResp.text);
            patchVal = parser.parse();
        }
    }

    if (!patchVal.isObject() || !patchVal.contains("tracks")) {
        // Fallback to algorithmic spectral unmasking
        patchVal = computeOfflineMixPatch(telem);
    }

    result.rawPatchJson = json::stringify(patchVal);
    result.summary = patchVal["summary"].asString();
    if (result.summary.empty()) {
        result.summary = "Applied algorithmic gain-staging & spectral unmasking.";
    }

    result.tracksAdjusted = applyMixPatch(seq, patchVal);
    result.success = (result.tracksAdjusted > 0 || telem.tracks.empty());

    // Populate structured adjustments
    if (patchVal.contains("tracks") && patchVal["tracks"].isObject()) {
        for (const auto& [tid, tval] : patchVal["tracks"].asObject()) {
            TrackMixAdjustment adj;
            adj.trackId = tid;
            adj.volume = static_cast<float>(tval["volume"].asDouble(0.85));
            adj.pan = static_cast<float>(tval["pan"].asDouble(0.0));
            if (tval.contains("eq") && tval["eq"].isObject()) {
                const auto& eqVal = tval["eq"];
                adj.eq.enabled = eqVal["enabled"].asBool(true);
                adj.eq.hpfHz = static_cast<float>(eqVal["hpf"].asDouble(30.0));
                adj.eq.lowGainDb = static_cast<float>(eqVal["lowGain"].asDouble(0.0));
                adj.eq.midFreqHz = static_cast<float>(eqVal["midFreq"].asDouble(1000.0));
                adj.eq.midGainDb = static_cast<float>(eqVal["midGain"].asDouble(0.0));
                adj.eq.highGainDb = static_cast<float>(eqVal["highGain"].asDouble(0.0));
            }
            result.trackAdjustments.push_back(adj);
        }
    }

    if (patchVal.contains("master") && patchVal["master"].isObject()) {
        const auto& m = patchVal["master"];
        result.masterAdjustment.subCutHz = static_cast<float>(m["subCut"].asDouble(28.0));
        result.masterAdjustment.lowGainDb = static_cast<float>(m["lowGain"].asDouble(0.0));
        result.masterAdjustment.midFreqHz = static_cast<float>(m["midFreq"].asDouble(2500.0));
        result.masterAdjustment.midGainDb = static_cast<float>(m["midGain"].asDouble(-0.5));
        result.masterAdjustment.highGainDb = static_cast<float>(m["highGain"].asDouble(1.0));
        result.masterAdjustment.limiterEnabled = m["limiterEnabled"].asBool(true);
        result.masterAdjustment.ceilingDbfs = static_cast<float>(m["ceilingDbfs"].asDouble(-0.3));
        result.masterAdjustment.limiterDriveDb = static_cast<float>(m["limiterDrive"].asDouble(2.5));
        result.masterAdjustment.targetLufs = static_cast<float>(m["targetLufs"].asDouble(targetLufs));
    }

    return result;
}

} // namespace eatsbits::ai
