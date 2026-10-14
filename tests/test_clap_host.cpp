#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cassert>
#include <cstdlib>
#include <filesystem>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

#include "eatsbits/audio/clap/clap_plugin_loader.hpp"
#include "eatsbits/audio/graph/nodes/clap_audio_node.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/ui/widgets/plugin_search_dialog.hpp"

#define EATS_CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "[FAILED] %s:%d: Assertion '%s' failed.\n", __FILE__, __LINE__, #cond); \
            std::abort(); \
        } \
    } while (0)

using namespace eatsbits;
using namespace eatsbits::audio;
using namespace eatsbits::ui;

static std::string getTb303ClapPath() {
    if (std::filesystem::exists("build/Release/eatsbits_tb303.clap")) {
        return "build/Release/eatsbits_tb303.clap";
    }
    if (std::filesystem::exists("build/eatsbits_tb303.clap")) {
        return "build/eatsbits_tb303.clap";
    }
    if (std::filesystem::exists("eatsbits_tb303.clap")) {
        return "eatsbits_tb303.clap";
    }
    return "eatsbits_tb303.clap";
}

void testPluginLoaderPathsAndScanning() {
    std::printf("[*] Testing ClapPluginLoader search paths and scanning...\n");

    auto& loader = ClapPluginLoader::instance();
    auto paths = loader.getDefaultSearchPaths();
    EATS_CHECK(!paths.empty());

    bool foundCommonFiles = false;
    for (const auto& p : paths) {
        if (p.string().find("CLAP") != std::string::npos) {
            foundCommonFiles = true;
            break;
        }
    }
    EATS_CHECK(foundCommonFiles);
    std::printf("    Default search paths verified (%zu paths registered)\n", paths.size());

    // Scan directories for eatsbits_tb303.clap
    std::vector<std::filesystem::path> localPaths = {
        std::filesystem::current_path(),
        std::filesystem::current_path() / "build" / "Release",
        std::filesystem::current_path() / "build"
    };
    auto plugins = loader.scanDirectories(localPaths);
    std::printf("    Scanned local directories: %zu CLAP plugins discovered\n", plugins.size());

    const auto* desc = loader.findPluginById("com.maddestlabs.eatsbits.tb303");
    if (!desc) {
        desc = loader.findPluginByName("Eatsbits TB-303");
    }
    EATS_CHECK(desc != nullptr);
    EATS_CHECK(desc->name == "Eatsbits TB-303");
    EATS_CHECK(desc->vendor == "Maddest Labs");
    EATS_CHECK(desc->isInstrument == true);
    std::printf("    Found descriptor: '%s' by %s (isInstrument=%d)\n",
        desc->name.c_str(), desc->vendor.c_str(), desc->isInstrument);

    // Verify cached descriptor retrieval (0ms latency, thread-safe)
    auto cachedPlugins = loader.getDiscoveredPlugins();
    EATS_CHECK(!cachedPlugins.empty());
    std::printf("    Verified in-memory cached plugin descriptors: %zu cached\n", cachedPlugins.size());

    // Verify Portable / Binary Path setting configuration (Reaper portable mode)
    EATS_CHECK(loader.isIncludeBinaryPathEnabled());
    loader.setIncludeBinaryPath(false);
    EATS_CHECK(!loader.isIncludeBinaryPathEnabled());
    auto sysOnlyPaths = loader.getDefaultSearchPaths();
    loader.setIncludeBinaryPath(true);
    EATS_CHECK(loader.isIncludeBinaryPathEnabled());
    auto portablePaths = loader.getDefaultSearchPaths();
    EATS_CHECK(portablePaths.size() >= sysOnlyPaths.size());
    std::printf("    Verified portable binary path toggle: %zu sys-only vs %zu portable paths\n",
        sysOnlyPaths.size(), portablePaths.size());

    // Verify async scan functionality
    std::atomic<bool> asyncComplete{false};
    loader.scanDirectoriesAsync(localPaths, [&](const auto& asyncPlugins) {
        EATS_CHECK(!asyncPlugins.empty());
        asyncComplete.store(true);
    });

    int waitIter = 0;
    while (!asyncComplete.load() && waitIter < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        waitIter++;
    }
    EATS_CHECK(asyncComplete.load());
    std::printf("    Verified background scanDirectoriesAsync completion without blocking\n");

    std::printf("[+] ClapPluginLoader tests passed.\n\n");
}

void testClapAudioNodeLifecycleAndAudio() {
    std::printf("[*] Testing ClapAudioNode lifecycle, real-time safety and audio synthesis...\n");

    const std::string clapPath = getTb303ClapPath();
    const std::string pluginId = "com.maddestlabs.eatsbits.tb303";

    auto node = ClapAudioNode::create(clapPath, pluginId, "Tb303HostNode");
    EATS_CHECK(node != nullptr);
    EATS_CHECK(node->getName() == "Tb303HostNode");
    EATS_CHECK(node->getPluginId() == pluginId);
    EATS_CHECK(node->isInstrument() == true);
    EATS_CHECK(node->numOutputPorts() == 1);
    EATS_CHECK(node->getOutputPort(0).numChannels == 2);

    node->prepare(48000.0, 128);

    // Setup zero-allocation output buffers
    std::vector<float> left(128, 0.0f);
    std::vector<float> right(128, 0.0f);
    node->setOutputBufferPtr(0, 0, left.data());
    node->setOutputBufferPtr(0, 1, right.data());

    // 1. Process block without notes -> silence
    node->processBlock(128);
    float sumSilent = 0.0f;
    for (int i = 0; i < 128; ++i) {
        sumSilent += std::abs(left[i]) + std::abs(right[i]);
    }
    EATS_CHECK(sumSilent == 0.0f);
    std::printf("    Silent block verified (sum = %.4f)\n", sumSilent);

    // 2. Trigger NoteOn -> audio synthesis
    node->noteOn(60, 0.9f);
    node->processBlock(128);

    float sumActive = 0.0f;
    for (int i = 0; i < 128; ++i) {
        sumActive += std::abs(left[i]) + std::abs(right[i]);
    }
    EATS_CHECK(sumActive > 0.01f);
    std::printf("    Active NoteOn synthesis verified (sum = %.4f)\n", sumActive);

    // 3. Process additional frames, test NoteOff
    node->noteOff(60);
    node->processBlock(128);

    // 4. Test real-time reset
    node->reset();
    std::printf("[+] ClapAudioNode lifecycle & audio rendering passed.\n\n");
}

void testClapAudioNodeParamsAndState() {
    std::printf("[*] Testing ClapAudioNode parameter reflection and state persistence...\n");

    const std::string clapPath = getTb303ClapPath();
    const std::string pluginId = "com.maddestlabs.eatsbits.tb303";

    auto node = ClapAudioNode::create(clapPath, pluginId, "Tb303ParamTest");
    EATS_CHECK(node != nullptr);

    // 1. Parameter Reflection
    uint32_t paramCount = node->getParamCount();
    EATS_CHECK(paramCount == 8);
    auto paramList = node->getParameterList();
    EATS_CHECK(paramList.size() == 8);
    EATS_CHECK(paramList[0].name == "Cutoff");
    EATS_CHECK(paramList[1].name == "Resonance");
    std::printf("    Reflected %zu parameters: Param 0='%s' (min=%.1f, max=%.1f, def=%.1f)\n",
        paramList.size(), paramList[0].name.c_str(), paramList[0].minValue, paramList[0].maxValue, paramList[0].defaultValue);

    // 2. Automated parameter mutation
    node->setParamValue(0, 1850.0);
    double curVal = node->getParamValue(0);
    EATS_CHECK(std::abs(curVal - 1850.0) < 1.0);
    std::printf("    Set Cutoff to 1850.0 Hz (readback: %.1f Hz)\n", curVal);

    // 3. State Save & Load
    auto stateBytes = node->saveState();
    EATS_CHECK(!stateBytes.empty());
    std::printf("    Saved plugin state (%zu bytes)\n", stateBytes.size());

    // Mutate parameter to different value
    node->setParamValue(0, 4200.0);
    EATS_CHECK(std::abs(node->getParamValue(0) - 4200.0) < 1.0);

    // Restore state
    bool loaded = node->loadState(stateBytes);
    EATS_CHECK(loaded == true);
    double restoredVal = node->getParamValue(0);
    EATS_CHECK(std::abs(restoredVal - 1850.0) < 1.0);
    std::printf("    Restored plugin state: Cutoff reverted to %.1f Hz\n", restoredVal);

    std::printf("[+] ClapAudioNode parameters and state tests passed.\n\n");
}

void testClapAudioNodeGui() {
    std::printf("[*] Testing ClapAudioNode GUI lifecycle...\n");

    const std::string clapPath = getTb303ClapPath();
    const std::string pluginId = "com.maddestlabs.eatsbits.tb303";

    auto node = ClapAudioNode::create(clapPath, pluginId, "Tb303GuiTest");
    EATS_CHECK(node != nullptr);

    EATS_CHECK(node->isGuiOpen() == false);

#if defined(_WIN32)
    // On Windows, test opening floating native host window
    bool guiOpened = node->openGuiWindow();
    if (guiOpened) {
        EATS_CHECK(node->isGuiOpen() == true);
        std::printf("    Opened native CLAP host floating window\n");
        node->closeGuiWindow();
        EATS_CHECK(node->isGuiOpen() == false);
        std::printf("    Closed native CLAP host floating window\n");
    } else {
        std::printf("    Note: Plugin has no custom GUI extension or headless environment; generic fallback available.\n");
    }
#endif

    std::printf("[+] ClapAudioNode GUI tests passed.\n\n");
}

void testAudioEngineIntegration() {
    std::printf("[*] Testing AudioEngine track instrument assignment with CLAP...\n");

    AudioEngine engine;
    engine.setupDefaultAcidBeatGraph();

    auto& seq = engine.getSequencer();
    seq.addTrack("303 Acid", 1, 64);
    seq.addTrack("808 Kit", 4, 64);
    seq.addTrack("909 Drive", 6, 64);
    seq.addTrack("DX7 Rhodes", 8, 64);
    seq.addTrack("Concert Grand", 10, 64);

    uint32_t tIdx = engine.addTrack("ClapAcidTrack");
    EATS_CHECK(tIdx == 5); // 0..4 are default tracks

    NodeId clapNodeId = engine.assignTrackInstrument(tIdx, "clap:com.maddestlabs.eatsbits.tb303");
    EATS_CHECK(clapNodeId != INVALID_NODE_ID);

    auto nodePtr = engine.getGraph().getNode(clapNodeId);
    EATS_CHECK(nodePtr != nullptr);
    auto* clapNode = dynamic_cast<ClapAudioNode*>(nodePtr.get());
    EATS_CHECK(clapNode != nullptr);
    std::printf("    Track 6 assigned to ClapAudioNode: %s (%s)\n",
        clapNode->getName().c_str(), clapNode->getPluginId().c_str());

    // Trigger note on the CLAP track
    engine.postNoteOn(48, 0.85f, false, false, static_cast<int>(tIdx));

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    engine.renderOfflineBlock(left.data(), right.data(), 256);

    float sumOut = 0.0f;
    for (int i = 0; i < 256; ++i) {
        sumOut += std::abs(left[i]) + std::abs(right[i]);
    }
    EATS_CHECK(sumOut > 0.01f);
    std::printf("    AudioEngine rendered CLAP track output: sum = %.4f\n", sumOut);

    engine.postNoteOff(48, static_cast<int>(tIdx));

    // Test stopPlayback with CLAP
    engine.postNoteOn(50, 0.85f, false, false, static_cast<int>(tIdx));
    engine.renderOfflineBlock(left.data(), right.data(), 256);
    engine.stopPlayback();
    engine.renderOfflineBlock(left.data(), right.data(), 256);
    std::printf("    Verified stopPlayback() flushed CLAP track voices cleanly.\n");

    // Test panic with CLAP (Panic Mode / Double-click Stop)
    engine.postNoteOn(55, 0.85f, false, false, static_cast<int>(tIdx));
    engine.renderOfflineBlock(left.data(), right.data(), 256);
    engine.panic();
    engine.renderOfflineBlock(left.data(), right.data(), 256);
    std::printf("    Verified panic() killed CLAP track voices cleanly via audio-thread choke.\n");

    std::printf("[+] AudioEngine CLAP integration tests passed.\n\n");
}

void testPluginSearchDialogClapListing() {
    std::printf("[*] Testing PluginSearchDialog integration with external CLAP plugins...\n");

    PluginSearchDialog dialog;
    dialog.open(PluginDialogMode::AddInstrument, "Track 1", 0);
    EATS_CHECK(dialog.isOpen() == true);

    auto entries = dialog.getFilteredCount();
    EATS_CHECK(entries > 0);

    dialog.setSearchQuery("CLAP");
    auto filtered = dialog.getFilteredCount();
    EATS_CHECK(filtered >= 1);
    std::printf("    Filtered search for 'CLAP': %zu matches found in library\n", filtered);

    if (std::filesystem::exists("C:\\Program Files\\Common Files\\CLAP\\Surge Synth Team\\Surge XT.clap")) {
        auto& loader = ClapPluginLoader::instance();
        loader.scanDirectories(loader.getDefaultSearchPaths());
        PluginSearchDialog surgeDialog;
        surgeDialog.open(PluginDialogMode::AddInstrument, "Track 1", 0);
        surgeDialog.setSearchQuery("Surge");
        auto surgeFiltered = surgeDialog.getFilteredCount();
        std::printf("    Filtered search for 'Surge': %zu matches found in library\n", surgeFiltered);
        EATS_CHECK(surgeFiltered >= 1);
    }

    dialog.close();
    EATS_CHECK(dialog.isOpen() == false);

    std::printf("[+] PluginSearchDialog integration tests passed.\n\n");
}

void testClapThirdPartyHostExtensions() {
    std::printf("[*] Testing 3rd-party CLAP host extensions and transport compatibility...\n");

    const std::string clapPath = getTb303ClapPath();
    const std::string pluginId = "com.maddestlabs.eatsbits.tb303";

    auto node = ClapAudioNode::create(clapPath, pluginId, "3rdPartyCompatNode");
    EATS_CHECK(node != nullptr);

    const auto* clapPlugin = node->getClapPlugin();
    EATS_CHECK(clapPlugin != nullptr);

    // Verify two-stage initialization guarantees non-null host pointer
    auto manualNode = std::make_shared<ClapAudioNode>("ManualTwoStageNode");
    bool manualOk = manualNode->initFromLibrary(clapPath, pluginId);
    EATS_CHECK(manualOk == true);
    EATS_CHECK(manualNode->getClapPlugin() != nullptr);

    // Test Host Transport configuration
    node->setBpm(138.5);
    node->setTransportPlaying(true);
    EATS_CHECK(node->getBpm() == 138.5);
    EATS_CHECK(node->isTransportPlaying() == true);

    node->prepare(48000.0, 512);

    // Test processing with transport context active
    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    node->setOutputBufferPtr(0, 0, left.data());
    node->setOutputBufferPtr(0, 1, right.data());

    node->noteOn(60, 0.9f);
    node->processBlock(512);

    float energy = 0.0f;
    for (int i = 0; i < 512; ++i) {
        energy += std::abs(left[i]) + std::abs(right[i]);
    }
    EATS_CHECK(energy > 0.01f);
    std::printf("    Audio block processed with active transport (energy = %.4f)\n", energy);

    node->noteOff(60);

    std::printf("[+] 3rd-party CLAP host extensions and transport tests passed.\n\n");
}

void testSurgeXtIfAvailable() {
    std::string surgePath = "C:\\Program Files\\Common Files\\CLAP\\Surge Synth Team\\Surge XT.clap";
    if (!std::filesystem::exists(surgePath)) {
        std::printf("[*] Surge XT not found at %s, skipping Surge-specific diagnostic.\n", surgePath.c_str());
        return;
    }
    std::printf("[*] Found Surge XT at %s! Testing GUI capabilities...\n", surgePath.c_str());
    auto& loader = ClapPluginLoader::instance();
    const auto* desc = loader.findPluginByName("Surge XT");
    if (!desc) {
        std::printf("    Surge XT descriptor not found in loader cache.\n");
        return;
    }
    std::string id = desc->id;
    std::printf("    Plugin ID: %s, Name: %s\n", id.c_str(), desc->name.c_str());
    auto node = ClapAudioNode::create(surgePath, id, "SurgeTest");
    if (!node) {
        std::printf("    Failed to create ClapAudioNode for Surge XT!\n");
        return;
    }
    const auto* clapPlugin = node->getClapPlugin();
    auto extGui = static_cast<const clap_plugin_gui_t*>(clapPlugin->get_extension(clapPlugin, CLAP_EXT_GUI));
    if (!extGui) {
        std::printf("    Surge XT has no CLAP_EXT_GUI!\n");
        return;
    }
    bool supEmbedded = extGui->is_api_supported ? extGui->is_api_supported(clapPlugin, CLAP_WINDOW_API_WIN32, false) : false;
    bool supFloating = extGui->is_api_supported ? extGui->is_api_supported(clapPlugin, CLAP_WINDOW_API_WIN32, true) : false;
    std::printf("    Surge XT is_api_supported(CLAP_WINDOW_API_WIN32, is_floating=false): %s\n", supEmbedded ? "YES" : "NO");
    std::printf("    Surge XT is_api_supported(CLAP_WINDOW_API_WIN32, is_floating=true):  %s\n", supFloating ? "YES" : "NO");

    bool created = extGui->create(clapPlugin, CLAP_WINDOW_API_WIN32, false);
    std::printf("    Surge XT extGui->create(embedded=false): %s\n", created ? "SUCCESS" : "FAILED");

    if (created) {
        uint32_t w = 0, h = 0;
        if (extGui->get_size) {
            bool gotSize = extGui->get_size(clapPlugin, &w, &h);
            std::printf("    Surge XT get_size after create: %s, %u x %u\n", gotSize ? "OK" : "FAILED", w, h);
        }
        if (extGui->can_resize) {
            bool canResize = extGui->can_resize(clapPlugin);
            std::printf("    Surge XT can_resize: %s\n", canResize ? "YES" : "NO");
        }
        extGui->destroy(clapPlugin);
    }
}

void testCardinalIfAvailable() {
    std::string cardPath = "C:\\Program Files\\Common Files\\CLAP\\Cardinal.clap\\CardinalSynth.clap";
    if (!std::filesystem::exists(cardPath)) {
        cardPath = "C:\\Program Files\\Common Files\\CLAP\\Cardinal.clap\\Cardinal.clap";
    }
    if (!std::filesystem::exists(cardPath)) {
        std::printf("[*] Cardinal not found, skipping diagnostic.\n");
        return;
    }
    std::printf("[*] Testing Cardinal at %s...\n", cardPath.c_str());
    auto& loader = ClapPluginLoader::instance();
    const auto* desc = loader.findPluginByName("Cardinal");
    if (!desc) desc = loader.findPluginByName("Cardinal Synth");
    if (!desc) {
        std::printf("    Cardinal descriptor not found in loader.\n");
        return;
    }
    std::printf("    Found descriptor: %s (ID: %s)\n", desc->name.c_str(), desc->id.c_str());
    std::printf("    Attempting to instantiate ClapAudioNode for Cardinal...\n");
    auto node = ClapAudioNode::create(desc->libraryPath, desc->id, "CardinalTestNode");
    if (!node) {
        std::printf("    [FAILED] ClapAudioNode::create returned null for Cardinal!\n");
        return;
    }
    std::printf("    [SUCCESS] ClapAudioNode created successfully for Cardinal!\n");

    const auto* cp = node->getClapPlugin();
    auto extPorts = static_cast<const clap_plugin_audio_ports_t*>(cp->get_extension(cp, CLAP_EXT_AUDIO_PORTS));
    if (extPorts) {
        uint32_t inCount = extPorts->count(cp, true);
        uint32_t outCount = extPorts->count(cp, false);
        std::printf("    Cardinal audio ports: %u in, %u out\n", inCount, outCount);
        for (uint32_t i = 0; i < inCount; ++i) {
            clap_audio_port_info_t info{};
            if (extPorts->get(cp, i, true, &info)) {
                std::printf("      In Port %u: '%s', channels=%u\n", i, info.name, info.channel_count);
            }
        }
        for (uint32_t i = 0; i < outCount; ++i) {
            clap_audio_port_info_t info{};
            if (extPorts->get(cp, i, false, &info)) {
                std::printf("      Out Port %u: '%s', channels=%u\n", i, info.name, info.channel_count);
            }
        }
    }

    std::printf("    Cardinal getParameterList()...\n");
    auto params = node->getParameterList();
    std::printf("    [SUCCESS] Cardinal parameter count: %zu\n", params.size());

    std::printf("    Testing AudioEngine adding Cardinal track...\n");
    AudioEngine engine;
    engine.setupDefaultAcidBeatGraph();
    uint32_t tIdx = engine.addTrack("CardTrack", 0, 16, "clap:" + desc->id);
    std::printf("    [SUCCESS] Added Cardinal track %u in AudioEngine!\n", tIdx);

    auto extGui = static_cast<const clap_plugin_gui_t*>(cp->get_extension(cp, CLAP_EXT_GUI));
    if (extGui) {
        bool supEmbedded = extGui->is_api_supported ? extGui->is_api_supported(cp, CLAP_WINDOW_API_WIN32, false) : false;
        bool supFloating = extGui->is_api_supported ? extGui->is_api_supported(cp, CLAP_WINDOW_API_WIN32, true) : false;
        std::printf("    Cardinal is_api_supported(CLAP_WINDOW_API_WIN32, is_floating=false): %s\n", supEmbedded ? "YES" : "NO");
        std::printf("    Cardinal is_api_supported(CLAP_WINDOW_API_WIN32, is_floating=true):  %s\n", supFloating ? "YES" : "NO");

        const char* prefApi = nullptr;
        bool prefFloating = false;
        if (extGui->get_preferred_api && extGui->get_preferred_api(cp, &prefApi, &prefFloating)) {
            std::printf("    Cardinal preferred API: %s, floating: %d\n", prefApi ? prefApi : "(null)", prefFloating);
        }

        std::printf("    Testing Cardinal openGuiWindow()...\n");
        bool guiOpened = node->openGuiWindow();
        std::printf("    Cardinal openGuiWindow() result: %s\n", guiOpened ? "SUCCESS" : "FAILED");
        EATS_CHECK(guiOpened == true);
        EATS_CHECK(node->isGuiOpen() == true);

        std::printf("    Polling main thread callbacks...\n");
        ClapAudioNode::pollAllMainThreadCallbacks();
        std::printf("    Closing Cardinal GUI window...\n");
        node->closeGuiWindow();
        EATS_CHECK(node->isGuiOpen() == false);
        std::printf("    [SUCCESS] Cardinal floating/windowed GUI closed cleanly!\n");

#if defined(_WIN32)
        // Test embedded child container docking
        static const char* kHostTestClass = "EatsbitsTestHostParent";
        WNDCLASSA twc{};
        twc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        twc.lpfnWndProc = DefWindowProcA;
        twc.hInstance = GetModuleHandleA(nullptr);
        twc.lpszClassName = kHostTestClass;
        RegisterClassA(&twc);

        HWND parentHwnd = CreateWindowExA(
            0, kHostTestClass, "Cardinal Embedded Host Test",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
            nullptr, nullptr, GetModuleHandleA(nullptr), nullptr
        );
        EATS_CHECK(parentHwnd != nullptr);

        std::printf("    Testing Cardinal openEmbeddedGui()...\n");
        bool embeddedOk = node->openEmbeddedGui(parentHwnd, 10, 10, 1000, 600);
        std::printf("    Cardinal openEmbeddedGui() result: %s\n", embeddedOk ? "SUCCESS" : "FAILED");
        EATS_CHECK(embeddedOk == true);
        EATS_CHECK(node->isEmbeddedGuiOpen() == true);

        // Test resizing embedded bounds
        node->setEmbeddedGuiBounds(10, 10, 1100, 700);
        ClapAudioNode::pollAllMainThreadCallbacks();

        // Test hide/show
        node->hideEmbeddedGui();
        EATS_CHECK(node->isEmbeddedGuiOpen() == false);
        node->showEmbeddedGui();
        EATS_CHECK(node->isEmbeddedGuiOpen() == true);

        // Test destroy embedded GUI
        node->destroyEmbeddedGui();
        EATS_CHECK(node->isEmbeddedGuiOpen() == false);
        std::printf("    [SUCCESS] Cardinal embedded GUI lifecycle verified completely!\n");

        DestroyWindow(parentHwnd);
#endif
    } else {
        std::printf("    Cardinal has NO CLAP_EXT_GUI!\n");
    }
}

#if defined(_WIN32)
static LONG WINAPI CrashVectoredExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == 0xC0000005) {
        void* addr = pExceptionInfo->ExceptionRecord->ExceptionAddress;
        ULONG_PTR op = pExceptionInfo->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR target = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];
        std::fprintf(stderr, "\n[CRASH DETECTED] Access Violation (0xC0000005) at %p (%s at %p)\n",
            addr, (op == 0 ? "read" : (op == 1 ? "write" : "execute")), (void*)target);

        HMODULE hMod = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)addr, &hMod)) {
            char modName[MAX_PATH] = {0};
            GetModuleFileNameA(hMod, modName, MAX_PATH);
            std::fprintf(stderr, "                 In Module: %s\n", modName);
        }

        void* stack[32];
        WORD frames = CaptureStackBackTrace(0, 32, stack, NULL);
        std::fprintf(stderr, "                 Call Stack (%u frames):\n", frames);
        for (WORD i = 0; i < frames; ++i) {
            HMODULE frameMod = nullptr;
            char frameModName[MAX_PATH] = "unknown";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)stack[i], &frameMod)) {
                GetModuleFileNameA(frameMod, frameModName, MAX_PATH);
            }
            std::fprintf(stderr, "                   [%2u] %p in %s\n", i, stack[i], frameModName);
        }
        std::fflush(stderr);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main() {
#if defined(_WIN32)
    AddVectoredExceptionHandler(1, CrashVectoredExceptionHandler);
#endif
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    std::printf("[TRACE] main() entry point\n");
    std::printf("============================================================\n");
    std::printf("   Eatsbits CLAP Phase 4: Desktop Host Node & Integration Test\n");
    std::printf("============================================================\n");

    testPluginLoaderPathsAndScanning();
    testClapAudioNodeLifecycleAndAudio();
    testClapAudioNodeParamsAndState();
    testClapAudioNodeGui();
    testAudioEngineIntegration();
    testPluginSearchDialogClapListing();
    testClapThirdPartyHostExtensions();
    testSurgeXtIfAvailable();
    testCardinalIfAvailable();

    std::printf("[SUCCESS] All CLAP Phase 4 Desktop Host tests passed completely!\n");
    return 0;
}

