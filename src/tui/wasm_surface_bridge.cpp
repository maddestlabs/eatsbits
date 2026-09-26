#include "eatsbits/tui/tui_app.hpp"
#include "eatsbits/tui/ansi_diff_renderer.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

namespace {
static std::unique_ptr<eatsbits::tui::TuiApp> g_app;
static eatsbits::tui::AnsiDiffRenderer g_diffRenderer;
static std::string g_ansiDiffBuffer;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
int eats_tui_init(int cols, int rows) {
    g_app = std::make_unique<eatsbits::tui::TuiApp>();
    g_app->getSurface().resize(cols, rows);
    // AudioEngine config for web
    eatsbits::audio::AudioEngineConfig cfg;
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 256;
    g_app->getAudioEngine().initialize(cfg);
    g_app->getAudioEngine().setupDefaultAcidBeatGraph();
    return 1;
}

EMSCRIPTEN_KEEPALIVE
void eats_tui_start_audio() {
    if (g_app) {
        g_app->getAudioEngine().start();
        g_app->getAudioEngine().getSequencer().start();
    }
}

EMSCRIPTEN_KEEPALIVE
void eats_tui_stop_audio() {
    if (g_app) {
        g_app->getAudioEngine().getSequencer().stop();
        g_app->getAudioEngine().stop();
    }
}

EMSCRIPTEN_KEEPALIVE
void eats_tui_tick() {
    if (!g_app) return;
    // In WebAssembly, frame tick is driven by requestAnimationFrame or JS timer
}

EMSCRIPTEN_KEEPALIVE
const void* eats_tui_get_cell_buffer_ptr() {
    if (!g_app) return nullptr;
    return g_app->getSurface().getFrontBuffer();
}

EMSCRIPTEN_KEEPALIVE
int eats_tui_get_width() {
    return g_app ? g_app->getSurface().getWidth() : 0;
}

EMSCRIPTEN_KEEPALIVE
int eats_tui_get_height() {
    return g_app ? g_app->getSurface().getHeight() : 0;
}

EMSCRIPTEN_KEEPALIVE
int eats_tui_get_cell_size() {
    return static_cast<int>(sizeof(eatsbits::tui::Cell));
}

EMSCRIPTEN_KEEPALIVE
int eats_tui_has_dirty() {
    return (g_app && g_app->getSurface().hasAnyDirty()) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
const char* eats_tui_render_ansi_diff(int* outLength) {
    if (!g_app) {
        if (outLength) *outLength = 0;
        return "";
    }
    g_ansiDiffBuffer.clear();
    g_diffRenderer.renderDiff(g_app->getSurface(), g_ansiDiffBuffer);
    if (outLength) *outLength = static_cast<int>(g_ansiDiffBuffer.size());
    return g_ansiDiffBuffer.c_str();
}

} // extern "C"
