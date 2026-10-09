#include "eatsbits/export/clap/tb303_clap_adapter.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#define EATS_CHECK(cond)                                                                       \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::fprintf(stderr, "[FATAL] Check failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (false)

// Simple in-memory stream for testing CLAP state save/load
struct MemoryStreamCtx {
    std::vector<uint8_t> buffer;
    size_t readPos{0};
};

static int64_t CLAP_ABI memStreamWrite(const clap_ostream* stream, const void* buffer, uint64_t size) {
    auto* ctx = static_cast<MemoryStreamCtx*>(stream->ctx);
    const auto* bytes = static_cast<const uint8_t*>(buffer);
    ctx->buffer.insert(ctx->buffer.end(), bytes, bytes + size);
    return static_cast<int64_t>(size);
}

static int64_t CLAP_ABI memStreamRead(const clap_istream* stream, void* buffer, uint64_t size) {
    auto* ctx = static_cast<MemoryStreamCtx*>(stream->ctx);
    if (ctx->readPos >= ctx->buffer.size()) return 0;
    const size_t avail = ctx->buffer.size() - ctx->readPos;
    const size_t toRead = std::min(avail, static_cast<size_t>(size));
    std::memcpy(buffer, ctx->buffer.data() + ctx->readPos, toRead);
    ctx->readPos += toRead;
    return static_cast<int64_t>(toRead);
}

// Simple event list for testing
struct TestInputEventList {
    std::vector<const clap_event_header_t*> events;

    static uint32_t CLAP_ABI size(const clap_input_events_t* list) {
        return static_cast<uint32_t>(static_cast<const TestInputEventList*>(list->ctx)->events.size());
    }

    static const clap_event_header_t* CLAP_ABI get(const clap_input_events_t* list, uint32_t index) {
        return static_cast<const TestInputEventList*>(list->ctx)->events[index];
    }

    clap_input_events_t clapList() {
        return clap_input_events_t{
            .ctx = this,
            .size = size,
            .get = get
        };
    }
};

void testInProcessShell() {
    std::printf("[*] Testing in-process ClapPluginShell<Tb303ClapAdapter>...\n");

    const auto* desc = eatsbits::export_clap::Tb303ClapAdapter::getDescriptor();
    EATS_CHECK(desc != nullptr);
    EATS_CHECK(std::strcmp(desc->id, "com.maddestlabs.eatsbits.tb303") == 0);
    EATS_CHECK(std::strcmp(desc->name, "Eatsbits TB-303") == 0);
    std::printf("    Plugin ID: %s, Name: %s\n", desc->id, desc->name);

    auto* shell = new eatsbits::export_clap::ClapPluginShell<eatsbits::export_clap::Tb303ClapAdapter>(nullptr);
    const clap_plugin_t* plugin = shell->clapPlugin();
    EATS_CHECK(plugin != nullptr);

    // 1. Init
    EATS_CHECK(plugin->init(plugin) == true);

    // 2. Audio Ports Extension
    auto* audioPorts = static_cast<const clap_plugin_audio_ports_t*>(
        plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
    EATS_CHECK(audioPorts != nullptr);
    EATS_CHECK(audioPorts->count(plugin, true) == 0);  // 0 inputs (instrument)
    EATS_CHECK(audioPorts->count(plugin, false) == 1); // 1 stereo output
    clap_audio_port_info_t audioPortInfo{};
    EATS_CHECK(audioPorts->get(plugin, 0, false, &audioPortInfo) == true);
    EATS_CHECK(audioPortInfo.channel_count == 2);
    EATS_CHECK((audioPortInfo.flags & CLAP_AUDIO_PORT_IS_MAIN) != 0);
    EATS_CHECK(std::strcmp(audioPortInfo.port_type, CLAP_PORT_STEREO) == 0);
    std::printf("    Audio Ports verified (1 stereo main output)\n");

    // 3. Note Ports Extension
    auto* notePorts = static_cast<const clap_plugin_note_ports_t*>(
        plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
    EATS_CHECK(notePorts != nullptr);
    EATS_CHECK(notePorts->count(plugin, true) == 1);  // 1 note input
    EATS_CHECK(notePorts->count(plugin, false) == 0); // 0 note outputs
    clap_note_port_info_t notePortInfo{};
    EATS_CHECK(notePorts->get(plugin, 0, true, &notePortInfo) == true);
    EATS_CHECK((notePortInfo.supported_dialects & CLAP_NOTE_DIALECT_CLAP) != 0);
    EATS_CHECK((notePortInfo.supported_dialects & CLAP_NOTE_DIALECT_MIDI) != 0);
    std::printf("    Note Ports verified (1 input supporting CLAP & MIDI dialects)\n");

    // 4. Params Extension
    auto* params = static_cast<const clap_plugin_params_t*>(
        plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    EATS_CHECK(params != nullptr);
    const uint32_t paramCount = params->count(plugin);
    EATS_CHECK(paramCount == 8);

    clap_param_info_t paramInfo{};
    EATS_CHECK(params->get_info(plugin, 0, &paramInfo) == true);
    EATS_CHECK(std::strcmp(paramInfo.name, "Cutoff") == 0);
    EATS_CHECK(paramInfo.min_value == 20.0);
    EATS_CHECK(paramInfo.max_value == 18000.0);

    double cutoffVal = 0.0;
    EATS_CHECK(params->get_value(plugin, 0, &cutoffVal) == true);
    EATS_CHECK(cutoffVal == 1400.0);

    char valText[64]{};
    EATS_CHECK(params->value_to_text(plugin, 0, 1400.0, valText, sizeof(valText)) == true);
    EATS_CHECK(std::strstr(valText, "1400.0 Hz") != nullptr);

    EATS_CHECK(params->value_to_text(plugin, 6, 0.0, valText, sizeof(valText)) == true);
    EATS_CHECK(std::strcmp(valText, "Saw") == 0);
    EATS_CHECK(params->value_to_text(plugin, 6, 1.0, valText, sizeof(valText)) == true);
    EATS_CHECK(std::strcmp(valText, "Square") == 0);

    double parsedVal = 0.0;
    EATS_CHECK(params->text_to_value(plugin, 6, "Square", &parsedVal) == true);
    EATS_CHECK(parsedVal == 1.0);
    EATS_CHECK(params->text_to_value(plugin, 6, "Saw", &parsedVal) == true);
    EATS_CHECK(parsedVal == 0.0);
    std::printf("    Params verified (8 parameters with bidirectional formatting)\n");

    // 5. Activation & Start
    EATS_CHECK(plugin->activate(plugin, 48000.0, 32, 512) == true);
    EATS_CHECK(plugin->start_processing(plugin) == true);

    // 6. Real-time Audio Processing with Sample-Accurate Note Event
    constexpr uint32_t kFrames = 64;
    std::vector<float> leftOut(kFrames, 0.0f);
    std::vector<float> rightOut(kFrames, 0.0f);
    float* channelPointers[2] = { leftOut.data(), rightOut.data() };

    clap_audio_buffer_t audioOutputBuf{
        .data32 = channelPointers,
        .data64 = nullptr,
        .channel_count = 2,
        .latency = 0,
        .constant_mask = 0
    };

    clap_event_note_t noteOnEvent{
        .header = {
            .size = sizeof(clap_event_note_t),
            .time = 16, // Occurs at sample offset 16
            .space_id = CLAP_CORE_EVENT_SPACE_ID,
            .type = CLAP_EVENT_NOTE_ON,
            .flags = 0
        },
        .note_id = -1,
        .port_index = 0,
        .channel = 0,
        .key = 36, // C2
        .velocity = 0.85
    };

    TestInputEventList inList;
    inList.events.push_back(&noteOnEvent.header);
    clap_input_events_t inEvStruct = inList.clapList();

    clap_process_t processCtx{
        .steady_time = 0,
        .frames_count = kFrames,
        .transport = nullptr,
        .audio_inputs = nullptr,
        .audio_outputs = &audioOutputBuf,
        .audio_inputs_count = 0,
        .audio_outputs_count = 1,
        .in_events = &inEvStruct,
        .out_events = nullptr
    };

    const clap_process_status status = plugin->process(plugin, &processCtx);
    EATS_CHECK(status == CLAP_PROCESS_CONTINUE);

    // Verify silence before sample 16
    for (uint32_t i = 0; i < 16; ++i) {
        EATS_CHECK(leftOut[i] == 0.0f);
        EATS_CHECK(rightOut[i] == 0.0f);
    }

    // Verify audio generation after sample 16
    float sumAudio = 0.0f;
    for (uint32_t i = 16; i < kFrames; ++i) {
        sumAudio += std::abs(leftOut[i]);
        EATS_CHECK(leftOut[i] == rightOut[i]); // Perfect stereo center
    }
    EATS_CHECK(sumAudio > 0.001f);
    std::printf("    Sample-accurate block processing verified (frames 0..15 silent, frames 16..63 active: sum=%.4f)\n", sumAudio);

    // 7. Parameter Automation Sweep during process()
    clap_event_param_value_t paramEvent{
        .header = {
            .size = sizeof(clap_event_param_value_t),
            .time = 0,
            .space_id = CLAP_CORE_EVENT_SPACE_ID,
            .type = CLAP_EVENT_PARAM_VALUE,
            .flags = 0
        },
        .param_id = 0, // Cutoff
        .cookie = nullptr,
        .note_id = -1,
        .port_index = 0,
        .channel = 0,
        .key = -1,
        .value = 2800.0
    };

    TestInputEventList paramInList;
    paramInList.events.push_back(&paramEvent.header);
    clap_input_events_t paramInEv = paramInList.clapList();
    processCtx.in_events = &paramInEv;

    plugin->process(plugin, &processCtx);
    double updatedCutoff = 0.0;
    params->get_value(plugin, 0, &updatedCutoff);
    EATS_CHECK(std::abs(updatedCutoff - 2800.0) < 0.001);
    std::printf("    In-process parameter automation verified (Cutoff set to %.1f Hz)\n", updatedCutoff);

    // 8. Parameter Flush
    clap_event_param_value_t flushEvent{
        .header = {
            .size = sizeof(clap_event_param_value_t),
            .time = 0,
            .space_id = CLAP_CORE_EVENT_SPACE_ID,
            .type = CLAP_EVENT_PARAM_VALUE,
            .flags = 0
        },
        .param_id = 1, // Resonance
        .cookie = nullptr,
        .note_id = -1,
        .port_index = 0,
        .channel = 0,
        .key = -1,
        .value = 0.88
    };
    TestInputEventList flushList;
    flushList.events.push_back(&flushEvent.header);
    clap_input_events_t flushEv = flushList.clapList();
    params->flush(plugin, &flushEv, nullptr);

    double updatedRes = 0.0;
    params->get_value(plugin, 1, &updatedRes);
    EATS_CHECK(std::abs(updatedRes - 0.88) < 0.001);
    std::printf("    Parameter flush verified (Resonance flushed to %.2f)\n", updatedRes);

    // 9. State Save & Load
    auto* state = static_cast<const clap_plugin_state_t*>(
        plugin->get_extension(plugin, CLAP_EXT_STATE));
    EATS_CHECK(state != nullptr);

    // Set specific parameter values
    shell->dsp().setParamValue(0, 3456.0); // Cutoff
    shell->dsp().setParamValue(1, 0.95);   // Resonance
    shell->dsp().setParamValue(6, 1.0);    // Waveform (Square)

    MemoryStreamCtx memCtx;
    clap_ostream outStream{
        .ctx = &memCtx,
        .write = memStreamWrite
    };
    EATS_CHECK(state->save(plugin, &outStream) == true);
    EATS_CHECK(!memCtx.buffer.empty());
    std::printf("    State saved (%zu bytes written)\n", memCtx.buffer.size());

    // Reset parameters to defaults/zeros
    shell->dsp().setParamValue(0, 500.0);
    shell->dsp().setParamValue(1, 0.1);
    shell->dsp().setParamValue(6, 0.0);

    clap_istream inStream{
        .ctx = &memCtx,
        .read = memStreamRead
    };
    EATS_CHECK(state->load(plugin, &inStream) == true);
    EATS_CHECK(std::abs(shell->dsp().getParamValue(0) - 3456.0) < 0.001);
    EATS_CHECK(std::abs(shell->dsp().getParamValue(1) - 0.95) < 0.001);
    EATS_CHECK(std::abs(shell->dsp().getParamValue(6) - 1.0) < 0.001);
    std::printf("    State restored successfully (Cutoff=%.1f, Res=%.2f, Wave=%.0f)\n",
        shell->dsp().getParamValue(0),
        shell->dsp().getParamValue(1),
        shell->dsp().getParamValue(6));

    // 10. Teardown
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    std::printf("[+] In-process ClapPluginShell test passed completely.\n\n");
}

#if defined(_WIN32)
void testDynamicClapLoading(const char* clapPath) {
    std::printf("[*] Testing dynamic DSO loading from '%s'...\n", clapPath);
    HMODULE hLib = LoadLibraryA(clapPath);
    if (!hLib) {
        std::printf("[!] Warning: Could not load '%s' (GLE=%lu). Skipping DSO dynamic test.\n",
            clapPath, GetLastError());
        return;
    }

    auto* entry = reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(hLib, "clap_entry"));
    EATS_CHECK(entry != nullptr);
    EATS_CHECK(clap_version_is_compatible(entry->clap_version));
    std::printf("    clap_entry found (CLAP %u.%u.%u)\n",
        entry->clap_version.major, entry->clap_version.minor, entry->clap_version.revision);

    EATS_CHECK(entry->init(clapPath) == true);

    auto* factory = static_cast<const clap_plugin_factory_t*>(
        entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    EATS_CHECK(factory != nullptr);

    const uint32_t count = factory->get_plugin_count(factory);
    EATS_CHECK(count == 1);

    const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
    EATS_CHECK(desc != nullptr);
    EATS_CHECK(std::strcmp(desc->id, "com.maddestlabs.eatsbits.tb303") == 0);
    std::printf("    Factory descriptor: %s (%s)\n", desc->name, desc->id);

    const clap_plugin_t* plugin = factory->create_plugin(factory, nullptr, desc->id);
    EATS_CHECK(plugin != nullptr);
    EATS_CHECK(plugin->init(plugin) == true);
    EATS_CHECK(plugin->activate(plugin, 48000.0, 32, 512) == true);
    EATS_CHECK(plugin->start_processing(plugin) == true);
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);

    entry->deinit();
    FreeLibrary(hLib);
    std::printf("[+] Dynamic DSO CLAP host loading validation passed completely.\n\n");
}
#endif

int main(int argc, char* argv[]) {
    std::printf("============================================================\n");
    std::printf("   Eatsbits CLAP Architecture & TB-303 Reference Test\n");
    std::printf("============================================================\n");

    testInProcessShell();

#if defined(_WIN32)
    const char* clapPath = "eatsbits_tb303.clap";
    if (argc > 1) {
        clapPath = argv[1];
    }
    testDynamicClapLoading(clapPath);
#endif

    std::printf("[SUCCESS] All CLAP Phase 1 foundation tests passed.\n");
    return 0;
}
