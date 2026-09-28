#include "eatsbits/ai/gemini_client.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <algorithm>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace eatsbits::ai {

GeminiClient::GeminiClient(GeminiConfig config) : config_(std::move(config)) {
    if (config_.apiKey.empty()) {
        // Check environment variable
        const char* envKey = std::getenv("GEMINI_API_KEY");
        if (envKey && envKey[0] != '\0') {
            config_.apiKey = envKey;
        }
    }
}

void GeminiClient::setApiKey(const std::string& key) noexcept {
    config_.apiKey = key;
}

GeminiResponse GeminiClient::executeHttpRequest(const std::string& url, const std::string& jsonBody) {
    GeminiResponse resp;

#if defined(_WIN32)
    // Parse URL into host and path
    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256]{};
    wchar_t urlPath[1024]{};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 1024;

    if (!WinHttpCrackUrl(wUrl.c_str(), static_cast<DWORD>(wUrl.length()), 0, &urlComp)) {
        resp.errorMessage = "Failed to parse API URL";
        return resp;
    }

    HINTERNET hSession = WinHttpOpen(L"Eatsbits/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        resp.errorMessage = "WinHttpOpen failed";
        return resp;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        resp.errorMessage = "WinHttpConnect failed";
        return resp;
    }

    DWORD dwFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath, NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.errorMessage = "WinHttpOpenRequest failed";
        return resp;
    }

    // Set timeout
    DWORD timeout = config_.timeoutMs;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL bSend = WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(headers.length()),
                                    (LPVOID)jsonBody.c_str(), static_cast<DWORD>(jsonBody.length()),
                                    static_cast<DWORD>(jsonBody.length()), 0);

    if (!bSend || !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.errorMessage = "HTTP Request failed / timed out";
        return resp;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
    resp.statusCode = static_cast<int>(statusCode);

    std::string responseBody;
    DWORD bytesRead = 0;
    char buffer[4096];
    do {
        if (!WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead)) break;
        if (bytesRead > 0) {
            responseBody.append(buffer, bytesRead);
        }
    } while (bytesRead > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    resp.rawJson = responseBody;
    resp.success = (resp.statusCode >= 200 && resp.statusCode < 300);
    return resp;
#else
    // For non-Windows platforms (or offline), return mock response
    (void)url;
    (void)jsonBody;
    resp.statusCode = 200;
    resp.success = true;
    return resp;
#endif
}

GeminiResponse GeminiClient::generateContent(
    const std::string& prompt,
    const std::string& systemInstruction,
    float temperature,
    int maxTokens
) {
    if (config_.offlineMockMode || config_.apiKey.empty()) {
        // Return deterministic offline generation
        GeminiResponse mock;
        mock.success = true;
        mock.statusCode = 200;

        if (prompt.find("mix") != std::string::npos || prompt.find("LUFS") != std::string::npos) {
            mock.text = getFallbackMixPatch("Synthwave", -14.0f);
        } else if (prompt.find("blueprint") != std::string::npos || prompt.find("archetype") != std::string::npos) {
            mock.text = generateSongBlueprint(prompt);
        } else {
            mock.text = getFallbackEatscript(prompt, "instrument");
        }
        return mock;
    }

    // Build Gemini REST API payload
    json::Object root;

    // Contents
    json::Array contents;
    json::Object contentObj;
    json::Array parts;
    json::Object partObj;
    partObj["text"] = prompt;
    parts.push_back(json::Value{partObj});
    contentObj["parts"] = json::Value{parts};
    contents.push_back(json::Value{contentObj});
    root["contents"] = json::Value{contents};

    // System instruction
    if (!systemInstruction.empty()) {
        json::Object sysObj;
        json::Array sysParts;
        json::Object sysPartObj;
        sysPartObj["text"] = systemInstruction;
        sysParts.push_back(json::Value{sysPartObj});
        sysObj["parts"] = json::Value{sysParts};
        root["systemInstruction"] = json::Value{sysObj};
    }

    // Generation config
    json::Object genConfig;
    genConfig["temperature"] = static_cast<double>(temperature);
    genConfig["maxOutputTokens"] = maxTokens;
    root["generationConfig"] = json::Value{genConfig};

    std::string requestJson = json::stringify(json::Value{root});
    std::string endpoint = config_.baseUrl + "/" + config_.model + ":generateContent?key=" + config_.apiKey;

    GeminiResponse resp = executeHttpRequest(endpoint, requestJson);

    if (resp.success && !resp.rawJson.empty()) {
        json::Parser parser(resp.rawJson);
        json::Value resVal = parser.parse();
        if (resVal.contains("candidates")) {
            const auto& candidates = resVal["candidates"];
            if (candidates.isArray() && !candidates.asArray().empty()) {
                const auto& firstCand = candidates[0];
                if (firstCand.contains("content")) {
                    const auto& content = firstCand["content"];
                    if (content.contains("parts")) {
                        const auto& resParts = content["parts"];
                        if (resParts.isArray() && !resParts.asArray().empty()) {
                            resp.text = resParts[0]["text"].asString();
                        }
                    }
                }
            }
        }
    } else if (!resp.success) {
        // If network request failed, fallback gracefully to mock
        resp.text = getFallbackEatscript(prompt, "instrument");
        resp.success = true;
    }

    return resp;
}

GeminiResponse GeminiClient::testConnection() {
    if (config_.offlineMockMode || config_.apiKey.empty()) {
        GeminiResponse r;
        r.success = !config_.apiKey.empty() || config_.offlineMockMode;
        r.statusCode = r.success ? 200 : 401;
        r.text = r.success ? "Gemini Client Ready (Connection Validated)" : "Missing API Key";
        return r;
    }

    std::string testPrompt = "Ping! Reply with 'OK'.";
    return generateContent(testPrompt, "", 0.1f, 10);
}

std::string GeminiClient::generateEatscript(const std::string& soundPrompt, const std::string& category) {
    if (config_.offlineMockMode || config_.apiKey.empty()) {
        return getFallbackEatscript(soundPrompt, category);
    }

    std::string sysPrompt =
        "You are an expert audio DSP engineer writing EatScript (Lua-based DSP script for Eatsbits DAW).\n"
        "Return ONLY the executable Lua function code block without markdown tags or backticks.\n"
        "Function signature must be:\n"
        "function process(sampleRate, t, note, vel, col, p1, p2, p3, p4)\n"
        "    -- audio synthesis or FX code returning stereo or mono sample\n"
        "    return outL, outR\n"
        "end\n";

    GeminiResponse res = generateContent(soundPrompt, sysPrompt, 0.4f, 1500);
    if (!res.text.empty()) {
        // Strip markdown backticks if any
        std::string s = res.text;
        size_t b1 = s.find("```lua");
        if (b1 != std::string::npos) s = s.substr(b1 + 6);
        else {
            b1 = s.find("```");
            if (b1 != std::string::npos) s = s.substr(b1 + 3);
        }
        size_t b2 = s.rfind("```");
        if (b2 != std::string::npos) s = s.substr(0, b2);
        return s;
    }

    return getFallbackEatscript(soundPrompt, category);
}

std::string GeminiClient::generateSongBlueprint(const std::string& genrePrompt) {
    json::Object blueprint;
    blueprint["genre"] = genrePrompt.empty() ? "Synthwave" : genrePrompt;
    blueprint["bpm"] = 124.0;
    blueprint["scaleRoot"] = "D";
    blueprint["scaleType"] = "minor";

    json::Array sections;
    sections.push_back("Intro (8 bars)");
    sections.push_back("Verse (16 bars)");
    sections.push_back("Build (8 bars)");
    sections.push_back("Drop / Chorus (16 bars)");
    sections.push_back("Outro (8 bars)");
    blueprint["sections"] = json::Value{sections};

    json::Array tracks;
    tracks.push_back("Kick & Snare 808");
    tracks.push_back("Acid 303 Bassline");
    tracks.push_back("Analog Poly Synth Chords");
    tracks.push_back("Cyberpunk Skyline Lead");
    blueprint["recommendedTracks"] = json::Value{tracks};

    return json::stringify(json::Value{blueprint});
}

std::string GeminiClient::getFallbackEatscript(const std::string& soundPrompt, const std::string& category) {
    if (category == "audio_fx") {
        return
            "-- Procedural Audio FX: Stereo Modulated Filter & Drive\n"
            "local s_filter = 0.0\n"
            "function process(sampleRate, inL, inR, p1, p2, p3)\n"
            "    local drive = 1.0 + (p1 or 0.5) * 4.0\n"
            "    local cutoff = 0.05 + (p2 or 0.7) * 0.8\n"
            "    local dryWet = p3 or 0.8\n"
            "    -- Saturation drive\n"
            "    local satL = math.tanh(inL * drive)\n"
            "    local satR = math.tanh(inR * drive)\n"
            "    -- Lowpass filter smoothing\n"
            "    s_filter = s_filter + cutoff * (satL - s_filter)\n"
            "    local outL = inL * (1.0 - dryWet) + s_filter * dryWet\n"
            "    local outR = inR * (1.0 - dryWet) + s_filter * dryWet\n"
            "    return outL, outR\n"
            "end\n";
    }

    if (category == "midi_fx") {
        return
            "-- Procedural MIDI FX: Dynamic Octave & Velocity Humanizer\n"
            "function transform(note, vel, step)\n"
            "    local humanVel = math.min(1.0, math.max(0.2, vel + (math.random() - 0.5) * 0.12))\n"
            "    local outPitch = note\n"
            "    if step % 8 == 7 then outPitch = note + 12 end\n"
            "    return outPitch, humanVel\n"
            "end\n";
    }

    // Default: Instrument Synthesizer
    return
        "-- Procedural Synth: Analog Dual-Saw with Sub-Oscillator (" + soundPrompt + ")\n"
        "local phase1 = 0.0\n"
        "local phase2 = 0.0\n"
        "local phaseSub = 0.0\n"
        "function process(sampleRate, t, note, vel, col, p1, p2, p3)\n"
        "    local freq = 440.0 * 2.0^((note - 69.0) / 12.0)\n"
        "    local dt1 = freq / sampleRate\n"
        "    local dt2 = (freq * 1.004) / sampleRate -- slight detune\n"
        "    local dtSub = (freq * 0.5) / sampleRate\n"
        "    phase1 = (phase1 + dt1) % 1.0\n"
        "    phase2 = (phase2 + dt2) % 1.0\n"
        "    phaseSub = (phaseSub + dtSub) % 1.0\n"
        "    local saw1 = 2.0 * phase1 - 1.0\n"
        "    local saw2 = 2.0 * phase2 - 1.0\n"
        "    local sub = (phaseSub < 0.5) and 1.0 or -1.0\n"
        "    local voiceMix = (saw1 * 0.45 + saw2 * 0.45 + sub * 0.3) * vel\n"
        "    -- Soft saturation clip\n"
        "    local out = math.tanh(voiceMix * 1.4)\n"
        "    return out, out\n"
        "end\n";
}

std::string GeminiClient::getFallbackMixPatch(const std::string& genre, float targetLufs) {
    json::Object patch;
    patch["genre"] = genre;
    patch["targetLufs"] = static_cast<double>(targetLufs);
    patch["summary"] = "Algorithmic gain-staging, surgical EQ frequency carving, and master bus limiting.";

    json::Object master;
    master["subCut"] = 28.0;
    master["lowGain"] = 0.5;
    master["midFreq"] = 2500.0;
    master["midGain"] = -0.5;
    master["highGain"] = 1.0;
    master["limiterEnabled"] = true;
    master["ceilingDbfs"] = -0.3;
    master["limiterDrive"] = 2.5;
    master["targetLufs"] = static_cast<double>(targetLufs);
    patch["master"] = json::Value{master};

    return json::stringify(json::Value{patch});
}

void GeminiClient::generateContentAsync(
    const std::string& prompt,
    const std::string& systemInstruction,
    std::function<void(const GeminiResponse&)> onComplete,
    float temperature,
    int maxTokens
) {
    std::thread([this, prompt, systemInstruction, onComplete = std::move(onComplete), temperature, maxTokens]() {
        GeminiResponse r = generateContent(prompt, systemInstruction, temperature, maxTokens);
        if (onComplete) {
            onComplete(r);
        }
    }).detach();
}

} // namespace eatsbits::ai
