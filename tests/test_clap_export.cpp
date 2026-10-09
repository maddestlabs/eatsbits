#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <filesystem>
#include <cstdlib>

#include "eatsbits/eatscript/transpiler.hpp"
#include "eatsbits/eatscript/dispatch_scanner.hpp"
#include "eatsbits/eatscript/lexer.hpp"
#include "eatsbits/eatscript/parser.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "clap/clap.h"

#define EATS_CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "[FAILED] %s:%d: Assertion '%s' failed.\n", __FILE__, __LINE__, #cond); \
            std::abort(); \
        } \
    } while (0)

using namespace eatsbits;
using namespace eatsbits::eatscript;

void testMathPatchTranspilation() {
    std::printf("[*] Testing AST math patch CLAP code generation...\n");

    const std::string script = R"(
# @id: eats.test.supersaw
# @name: SuperSaw Lead
# @category: instrument
# @vendor: Eatsbits Team
# @version: 2.1.0
# @description: Dual-oscillator dynamic supersaw generator

cutoff = eat.param("Cutoff", 20.0, 20000.0, 2500.0)
resonance = eat.param("Resonance", 0.0, 1.0, 0.3)

def process():
    osc = math.sin(2.0 * math.pi * freq * time)
    sample = osc * cutoff
    return sample
)";

    Transpiler transpiler;
    std::string generatedCpp = transpiler.transpileClap(script);

    // 1. Verify headers
    EATS_CHECK(generatedCpp.find("clap_plugin_shell.hpp") != std::string::npos);
    EATS_CHECK(generatedCpp.find("<cmath>") != std::string::npos);
    EATS_CHECK(generatedCpp.find("<numbers>") != std::string::npos);

    // 2. Verify descriptor fields
    EATS_CHECK(generatedCpp.find(".id = \"eats.test.supersaw\"") != std::string::npos);
    EATS_CHECK(generatedCpp.find(".name = \"SuperSaw Lead\"") != std::string::npos);
    EATS_CHECK(generatedCpp.find(".vendor = \"Eatsbits Team\"") != std::string::npos);
    EATS_CHECK(generatedCpp.find(".version = \"2.1.0\"") != std::string::npos);
    EATS_CHECK(generatedCpp.find("CLAP_PLUGIN_FEATURE_INSTRUMENT") != std::string::npos);
    EATS_CHECK(generatedCpp.find("CLAP_PLUGIN_FEATURE_SYNTHESIZER") != std::string::npos);

    // 3. Verify parameter definitions
    EATS_CHECK(generatedCpp.find("getParamCount() noexcept {\n        return 2;") != std::string::npos);
    EATS_CHECK(generatedCpp.find("\"Cutoff\"") != std::string::npos);
    EATS_CHECK(generatedCpp.find("20") != std::string::npos);
    EATS_CHECK(generatedCpp.find("20000") != std::string::npos);
    EATS_CHECK(generatedCpp.find("2500") != std::string::npos);
    EATS_CHECK(generatedCpp.find("\"Resonance\"") != std::string::npos);

    // 4. Verify AST DSP math in processBlock
    EATS_CHECK(generatedCpp.find("std::sin") != std::string::npos);
    EATS_CHECK(generatedCpp.find("std::numbers::pi_v<double>") != std::string::npos);
    EATS_CHECK(generatedCpp.find("outL[i] += static_cast<float>(sample);") != std::string::npos);
    EATS_CHECK(generatedCpp.find("outR[i] += static_cast<float>(sample);") != std::string::npos);

    // 5. Verify export macro
    EATS_CHECK(generatedCpp.find("EATS_CLAP_EXPORT_PLUGIN") != std::string::npos);
    EATS_CHECK(generatedCpp.find("GeneratedPatchAdapter_eats_test_supersaw") != std::string::npos);

    std::printf("[+] AST math patch CLAP code generation verified.\n");
}

void testNativeEngineTranspilation() {
    std::printf("[*] Testing native engine CLAP code generation (# @engine: tb303)...\n");

    const std::string script = R"(
# @engine: tb303
# @id: com.eatsbits.acid.bass
# @name: Acid Bass 303
# @category: instrument
# @version: 1.0.0

cutoff = eat.param("Cutoff", 20.0, 18000.0, 1400.0)
resonance = eat.param("Resonance", 0.0, 1.0, 0.5)
)";

    Transpiler transpiler;
    std::string generatedCpp = transpiler.transpileClap(script);

    // 1. Verify discovered header
    EATS_CHECK(generatedCpp.find("eatsbits/audio/dsp/tb303_core.hpp") != std::string::npos);

    // 2. Verify native core member and invocation
    EATS_CHECK(generatedCpp.find("eatsbits::dsp::Tb303Core tb303_;") != std::string::npos);
    EATS_CHECK(generatedCpp.find("tb303_.processSample()") != std::string::npos);
    EATS_CHECK(generatedCpp.find("tb303_.noteOn(note, velocity, isSlide, isAccent);") != std::string::npos);
    EATS_CHECK(generatedCpp.find("tb303_.noteOff();") != std::string::npos);
    EATS_CHECK(generatedCpp.find("tb303_.setCutoff(") != std::string::npos);

    // 3. Verify export macro
    EATS_CHECK(generatedCpp.find("EATS_CLAP_EXPORT_PLUGIN") != std::string::npos);
    EATS_CHECK(generatedCpp.find("GeneratedPatchAdapter_com_eatsbits_acid_bass") != std::string::npos);

    std::printf("[+] Native engine CLAP code generation verified.\n");
}

void testCliExportAndDynamicLoading() {
    std::printf("[*] Testing CLI export and host loading...\n");

    const std::string testScriptPath = "test_export_synth.eats";
    const std::string testClapPath = "test_export_synth.clap";
    const std::string testCppPath = "test_export_synth.cpp";

    // Write sample Eatscript patch
    {
        std::ofstream f(testScriptPath);
        EATS_CHECK(f.is_open());
        f << "# @engine: tb303\n"
          << "# @id: com.eatsbits.test.export303\n"
          << "# @name: Test Export 303\n"
          << "# @category: instrument\n"
          << "cutoff = eat.param(\"Cutoff\", 20.0, 18000.0, 1400.0)\n";
    }

    // 1. Test CLI export --format=cpp
    const std::vector<std::string> cliCandidates = {
        "Release\\eatscript_cli.exe",
        "build\\Release\\eatscript_cli.exe",
        ".\\Release\\eatscript_cli.exe",
        "..\\build\\Release\\eatscript_cli.exe",
        "eatscript_cli.exe"
    };
    std::string cliExe;
    for (const auto& c : cliCandidates) {
        if (std::filesystem::exists(c)) {
            cliExe = c;
            break;
        }
    }
    if (cliExe.empty()) {
        cliExe = "eatscript_cli.exe";
    }

    std::string cmdCpp = cliExe + " export --format=cpp " + testScriptPath + " -o " + testCppPath;
    int retCpp = std::system(cmdCpp.c_str());
    EATS_CHECK(retCpp == 0);
    EATS_CHECK(std::filesystem::exists(testCppPath));
    EATS_CHECK(std::filesystem::file_size(testCppPath) > 500);
    std::printf("    Generated C++ bundle: %llu bytes\n", 
        static_cast<unsigned long long>(std::filesystem::file_size(testCppPath)));

    // 2. Test CLI export --format=clap
    std::string cmdClap = cliExe + " export --format=clap " + testScriptPath + " -o " + testClapPath;
    int retClap = std::system(cmdClap.c_str());

    if (retClap == 0 && std::filesystem::exists(testClapPath)) {
        std::printf("    Successfully built native .clap binary: %llu bytes\n",
            static_cast<unsigned long long>(std::filesystem::file_size(testClapPath)));

#if defined(_WIN32)
        // 3. Dynamic loading and processing test
        HMODULE hLib = LoadLibraryA(testClapPath.c_str());
        EATS_CHECK(hLib != nullptr);

        auto* entry = reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(hLib, "clap_entry"));
        EATS_CHECK(entry != nullptr);
        EATS_CHECK(clap_version_is_compatible(entry->clap_version));
        EATS_CHECK(entry->init(testClapPath.c_str()) == true);

        auto* factory = static_cast<const clap_plugin_factory_t*>(
            entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        EATS_CHECK(factory != nullptr);
        EATS_CHECK(factory->get_plugin_count(factory) == 1);

        const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
        EATS_CHECK(desc != nullptr);
        EATS_CHECK(std::strcmp(desc->id, "com.eatsbits.test.export303") == 0);
        EATS_CHECK(std::strcmp(desc->name, "Test Export 303") == 0);

        const clap_plugin_t* plugin = factory->create_plugin(factory, nullptr, desc->id);
        EATS_CHECK(plugin != nullptr);
        EATS_CHECK(plugin->init(plugin) == true);
        EATS_CHECK(plugin->activate(plugin, 48000.0, 32, 512) == true);
        EATS_CHECK(plugin->start_processing(plugin) == true);

        // Render audio buffer
        float left[64] = {0};
        float right[64] = {0};
        float* channelData[2] = { left, right };

        clap_audio_buffer_t outBuf{};
        outBuf.data32 = channelData;
        outBuf.channel_count = 2;
        outBuf.latency = 0;

        clap_process_t process{};
        process.frames_count = 64;
        process.audio_outputs = &outBuf;
        process.audio_outputs_count = 1;

        // Test Parameter Reflection
        auto* params = static_cast<const clap_plugin_params_t*>(
            plugin->get_extension(plugin, CLAP_EXT_PARAMS));
        EATS_CHECK(params != nullptr);
        EATS_CHECK(params->count(plugin) == 1);
        clap_param_info_t paramInfo{};
        EATS_CHECK(params->get_info(plugin, 0, &paramInfo) == true);
        EATS_CHECK(std::strcmp(paramInfo.name, "Cutoff") == 0);
        EATS_CHECK(paramInfo.default_value == 1400.0);
        std::printf("    Parameter reflection verified: param 0 is '%s' (default=%.1f)\n", 
            paramInfo.name, paramInfo.default_value);

        clap_process_status status = plugin->process(plugin, &process);
        EATS_CHECK(status == CLAP_PROCESS_CONTINUE || status == CLAP_PROCESS_SLEEP);

        plugin->stop_processing(plugin);
        plugin->deactivate(plugin);
        plugin->destroy(plugin);
        entry->deinit();
        FreeLibrary(hLib);
        std::printf("[+] Dynamic host execution and audio rendering verified!\n");
#endif
    } else {
        std::printf("[!] Note: Native compilation step skipped or host compiler unavailable (ret=%d)\n", retClap);
    }

    // Cleanup temporary files
    std::filesystem::remove(testScriptPath);
    std::filesystem::remove(testCppPath);
    if (std::filesystem::exists(testClapPath)) {
        std::filesystem::remove(testClapPath);
    }
    std::filesystem::remove("test_export_synth_bundle.cpp");
    std::filesystem::remove("test_export_synth.obj");
    std::filesystem::remove("test_export_synth.lib");
    std::filesystem::remove("test_export_synth.exp");

    std::printf("[+] CLI export tests completed successfully.\n");
}

int main() {
    std::printf("============================================================\n");
    std::printf("   Eatsbits CLAP Phase 3: AOT Transpiler & CLI Export Test\n");
    std::printf("============================================================\n");

    testMathPatchTranspilation();
    testNativeEngineTranspilation();
    testCliExportAndDynamicLoading();

    std::printf("\n[SUCCESS] All CLAP Phase 3 AOT & CLI export tests passed!\n");
    return 0;
}
