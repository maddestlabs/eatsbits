#include "eatsbits/abi/eats_plugin_abi.h"
#include "eatsbits/audio/dsp/tb303_core.hpp"
#include <new>

struct Tb303PluginInstance {
    eatsbits::dsp::Tb303Core core;
};

extern "C" {

static void* tb303_create(double sampleRate) {
    auto* inst = new (std::nothrow) Tb303PluginInstance();
    if (inst) {
        inst->core.setSampleRate(static_cast<float>(sampleRate));
    }
    return inst;
}

static void tb303_destroy(void* instance) {
    delete static_cast<Tb303PluginInstance*>(instance);
}

static void tb303_process(void* instance, const EatsAudioBuffer* buffer) {
    if (!instance || !buffer || !buffer->outputs || buffer->numChannels < 2) return;
    auto* inst = static_cast<Tb303PluginInstance*>(instance);

    float* outL = buffer->outputs[0];
    float* outR = buffer->outputs[1];

    for (uint32_t i = 0; i < buffer->numSamples; ++i) {
        const float s = inst->core.processSample();
        outL[i] += s;
        outR[i] += s;
    }
}

static void tb303_set_param(void* instance, uint32_t paramId, float value) {
    if (!instance) return;
    auto* inst = static_cast<Tb303PluginInstance*>(instance);

    switch (paramId) {
        case 0: inst->core.setCutoff(value); break;
        case 1: inst->core.setResonance(value); break;
        case 2: inst->core.setEnvMod(value); break;
        case 3: inst->core.setDecay(value); break;
        case 4: inst->core.setAccent(value); break;
        case 5: inst->core.setOverdrive(value); break;
        case 6: inst->core.setWaveform(value); break;
        default: break;
    }
}

static void tb303_note_on(void* instance, uint8_t pitch, float velocity) {
    if (!instance) return;
    auto* inst = static_cast<Tb303PluginInstance*>(instance);
    inst->core.noteOn(pitch, velocity, false, velocity > 0.75f);
}

static void tb303_note_off(void* instance, uint8_t /*pitch*/) {
    if (!instance) return;
    auto* inst = static_cast<Tb303PluginInstance*>(instance);
    inst->core.noteOff();
}

const EatsPluginDescriptor g_plugin_tb303 = {
    "org.eatsbits.tb303",
    "Roland TB-303 Acid Bassline",
    7,
    tb303_create,
    tb303_destroy,
    tb303_process,
    tb303_set_param,
    tb303_note_on,
    tb303_note_off
};

} // extern "C"
