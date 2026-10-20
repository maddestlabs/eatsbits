#include "eatsbits/ui/gui_window.hpp"
#include "eatsbits/project/project_file.hpp"
#include "eatsbits/project/eats_serializer.hpp"
#include "eatsbits/audio/export/wav_exporter.hpp"
#include "eatsbits/eatscript/lexer.hpp"
#include "eatsbits/eatscript/parser.hpp"
#include "eatsbits/eatscript/vm.hpp"
#include "eatsbits/eatscript/transpiler.hpp"
#include "eatsbits/eatscript/note_script.hpp"
#include "eatsbits/eatscript/dispatch_scanner.hpp"
#include "eatsbits/eatscript/macro_runtime.hpp"
#include "eatsbits/procgen/procedural_song_engine.hpp"
#include "eatsbits/audio/graph/nodes/eatscript_node.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/poly_synth_node.hpp"
#include "eatsbits/audio/graph/nodes/physical_instrument_node.hpp"
#include "eatsbits/audio/graph/nodes/clavinet_node.hpp"
#include "eatsbits/audio/graph/nodes/clap_audio_node.hpp"
#include "eatsbits/audio/graph/nodes/snes_node.hpp"
#include "eatsbits/audio/graph/nodes/sid_node.hpp"
#include "eatsbits/audio/graph/nodes/dx7_node.hpp"
#include "eatsbits/audio/graph/nodes/ym2612_node.hpp"
#include "eatsbits/audio/clap/clap_plugin_loader.hpp"
#include "eatsbits/project/eats_builtin_presets.hpp"
#include "eatsbits/ui/widgets/track_properties_panel.hpp"
#include "eatsbits/ui/svg_path.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/floating_window_manager.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include "eatsbits/ui/app_icon_data.hpp"
#include "eatsbits/project/json_parser.hpp"
#include "eatsbits/ui/widgets/color_picker_dialog.hpp"
#include <fstream>
#include <filesystem>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

namespace eatsbits::ui {

void GlfwClipboard::setString(const std::string& text) {
#if !defined(__EMSCRIPTEN__)
    if (win_) {
        glfwSetClipboardString(win_, text.c_str());
    }
#else
    (void)text;
#endif
}

std::string GlfwClipboard::getString() const {
#if !defined(__EMSCRIPTEN__)
    if (win_) {
        const char* s = glfwGetClipboardString(win_);
        return s ? std::string(s) : std::string{};
    }
#endif
    return {};
}

} // namespace eatsbits::ui

namespace {

std::string promptOpenEatsFile() {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Eatsbits Project (*.eats)\0*.eats\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return "";
}

std::string promptOpenAudioFile() {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Audio Files (*.wav;*.mp3;*.flac;*.ogg;*.aif)\0*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return "";
}

std::string promptSaveEatsFile(const std::string& defaultName) {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    char filename[MAX_PATH] = "";
    std::strncpy(filename, defaultName.c_str(), MAX_PATH - 1);
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Eatsbits Project (*.eats)\0*.eats\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "eats";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return "";
}

std::string promptSaveWavFile(const std::string& defaultName) {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    char filename[MAX_PATH] = "";
    std::strncpy(filename, defaultName.c_str(), MAX_PATH - 1);
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "WAV Audio File (*.wav)\0*.wav\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "wav";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return "";
}

} // anonymous namespace

#if __has_include(<GLFW/glfw3.h>)
#define EATS_HAS_GLFW 1
#include <GLFW/glfw3.h>
#else
#define EATS_HAS_GLFW 0
#endif

using GLuint = unsigned int;

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include "eatsbits/ui/monospace_font_8x16.hpp"
#include "eatsbits/text/font_manager.hpp"
#include "eatsbits/text/text_shaper.hpp"
#include "eatsbits/text/glyph_atlas.hpp"
#include "eatsbits/text/embedded_fonts.hpp"

namespace eatsbits::ui {

#if defined(__EMSCRIPTEN__)
static GuiWindow* g_activeGuiWindowForWeb = nullptr;

extern "C" EMSCRIPTEN_KEEPALIVE
void eats_on_web_char_input(unsigned int codepoint) {
    if (!g_activeGuiWindowForWeb) return;
    g_activeGuiWindowForWeb->onChar(codepoint);
}

extern "C" EMSCRIPTEN_KEEPALIVE
void eats_on_web_key_input(int key, int action, int mods) {
    if (!g_activeGuiWindowForWeb) return;
    if (action == 1 /* GLFW_PRESS */ || action == 2 /* GLFW_REPEAT */) {
        g_activeGuiWindowForWeb->onKeyDown(key, mods);
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE
void eats_on_file_dropped_web(const char* virtualPath, float clientX, float clientY) {
    if (!g_activeGuiWindowForWeb || !virtualPath) return;
    float logX = g_activeGuiWindowForWeb->windowToLogicalX(clientX);
    float logY = g_activeGuiWindowForWeb->windowToLogicalY(clientY);
    if (g_activeGuiWindowForWeb->is3dConsoleEnabled()) {
        g_activeGuiWindowForWeb->transform3dMouseCoords(logX, logY, logX, logY);
    }
    g_activeGuiWindowForWeb->remapCrtMouseCoords(logX, logY, logX, logY);
    g_activeGuiWindowForWeb->onFilesDropped({std::string(virtualPath)}, logX, logY);
}
#endif

static inline std::string toHex2(uint32_t val) {
    char buf[4];
    std::snprintf(buf, sizeof(buf), "%02X", val & 0xFF);
    return std::string(buf);
}

static inline std::string midiNoteToTrackerString(uint8_t note) {
    static const char* noteNames[12] = {
        "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
    };
    int semi = note % 12;
    int oct = (note / 12) - 1;
    if (oct < 0) oct = 0;
    if (oct > 9) oct = 9;
    return std::string(noteNames[semi]) + std::to_string(oct);
}

struct TokenSegment {
    std::string text;
    float r{1.0f}, g{1.0f}, b{1.0f};
};

static std::vector<TokenSegment> colorizeEatscriptLine(const std::string& line) {
    std::vector<TokenSegment> segments;
    size_t i = 0;
    while (i < line.size()) {
        if (line[i] == '#') {
            segments.push_back({line.substr(i), 0.38f, 0.49f, 0.55f});
            break;
        }
        if (line[i] == ' ' || line[i] == '\t') {
            size_t start = i;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
            segments.push_back({line.substr(start, i - start), 1.0f, 1.0f, 1.0f});
            continue;
        }
        if (line[i] == '\'' || line[i] == '"') {
            char quote = line[i];
            size_t start = i++;
            while (i < line.size() && line[i] != quote) i++;
            if (i < line.size()) i++;
            segments.push_back({line.substr(start, i - start), 1.0f, 0.72f, 0.30f});
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(line[i])) || (line[i] == '.' && i + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            size_t start = i;
            while (i < line.size() && (std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '.' || line[i] == 'e' || line[i] == 'E' || line[i] == 'f')) i++;
            segments.push_back({line.substr(start, i - start), 0.0f, 0.90f, 0.45f});
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(line[i])) || line[i] == '_') {
            size_t start = i;
            while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) i++;
            std::string ident = line.substr(start, i - start);

            if (ident == "def" || ident == "return" || ident == "if" || ident == "else" ||
                ident == "while" || ident == "for" || ident == "in" || ident == "import" ||
                ident == "pass" || ident == "True" || ident == "False") {
                segments.push_back({ident, 0.0f, 0.95f, 1.0f});
            } else if (ident == "sin" || ident == "cos" || ident == "tanh" || ident == "exp" ||
                       ident == "floor" || ident == "sqrt" || ident == "random" || ident == "pow" ||
                       ident == "abs" || ident == "biquad_lp" || ident == "biquad_hp" || ident == "biquad_bp" ||
                       ident == "init" || ident == "process") {
                segments.push_back({ident, 1.0f, 0.84f, 0.0f});
            } else if (ident == "time" || ident == "freq" || ident == "note" || ident == "in_l" ||
                       ident == "in_r" || ident == "out_l" || ident == "out_r" || ident == "sample_rate") {
                segments.push_back({ident, 0.85f, 0.55f, 1.0f});
            } else {
                segments.push_back({ident, 0.92f, 0.94f, 0.97f});
            }
            continue;
        }
        char c = line[i++];
        float opR = 0.55f, opG = 0.65f, opB = 0.72f;
        if (c == ':' || c == '(' || c == ')' || c == '[' || c == ']') {
            opR = 0.40f; opG = 0.85f; opB = 0.95f;
        } else if (c == '=' || c == '+' || c == '-' || c == '*' || c == '/') {
            opR = 1.0f; opG = 0.45f; opB = 0.65f;
        }
        segments.push_back({std::string(1, c), opR, opG, opB});
    }
    return segments;
}

struct FontRendererState {
    eatsbits::text::FontManager fontMgr;
    eatsbits::text::TextShaper textShaper;
    eatsbits::text::GlyphAtlas glyphAtlas{1024, 1024};
    eatsbits::text::ShapedRun shapedRun;
    eatsbits::text::FontId fontNormal{eatsbits::text::kInvalidFontId};
    eatsbits::text::FontId fontFallback{eatsbits::text::kInvalidFontId};
    eatsbits::text::FontId fontMono{eatsbits::text::kInvalidFontId};
    std::string loadedFontPath;
    std::string loadedMonoFontPath;

    FontRendererState() {
        fontMgr.initialize();
        shapedRun.glyphs.reserve(512);
    }
};

static FontRendererState* g_activeFontRenderer = nullptr;
static BatchRenderer2D* g_activeBatchRenderer = nullptr;

inline BatchRenderer2D* getEffectiveBatchRenderer() noexcept {
    if (g_activeBatchRenderer) return g_activeBatchRenderer;
    return BatchRenderer2D::getActiveBatchRenderer();
}

static void renderShapedGlyphs(BatchRenderer2D& renderer, FontRendererState& state,
                               eatsbits::text::FontId primaryFontId, float fontSize,
                               float startX, float baselineY, uint32_t color) {
    if (state.shapedRun.glyphs.empty()) return;

    float curX = startX;
    for (const auto& g : state.shapedRun.glyphs) {
        if (g.glyphIndex == 0) {
            curX += g.xAdvance;
            continue;
        }

        float glyphX = curX + g.xOffset;
        eatsbits::text::FontId fid = (g.fontId != eatsbits::text::kInvalidFontId) ? g.fontId : primaryFontId;
        const auto* ag = state.glyphAtlas.getOrLoadGlyph(state.fontMgr, fid, g.glyphIndex, fontSize, glyphX);
        if (!ag || ag->width <= 0 || ag->height <= 0) {
            curX += g.xAdvance;
            continue;
        }

        curX += g.xAdvance;
    }

    if (renderer.getFontAtlasVersion() != state.glyphAtlas.getVersion()) {
        int dx = 0, dy = 0, dw = 0, dh = 0;
        if (renderer.getFontAtlasVersion() != 0 && state.glyphAtlas.isDirty()) {
            state.glyphAtlas.getDirtyBounds(dx, dy, dw, dh);
        }
        if (dw > 0 && dh > 0) {
            renderer.updateFontAtlas(dx, dy, dw, dh, state.glyphAtlas.getBuffer(),
                                     state.glyphAtlas.getWidth(), state.glyphAtlas.getHeight());
        } else {
            renderer.updateFontAtlas(0, 0, state.glyphAtlas.getWidth(), state.glyphAtlas.getHeight(),
                                     state.glyphAtlas.getBuffer(),
                                     state.glyphAtlas.getWidth(), state.glyphAtlas.getHeight());
        }
        renderer.setFontAtlasVersion(state.glyphAtlas.getVersion());
    }

    constexpr int kMaxBatchGlyphs = 64;
    float verts[kMaxBatchGlyphs * 12];
    float uvs[kMaxBatchGlyphs * 12];
    unsigned int cols[kMaxBatchGlyphs * 6];
    int queued = 0;

    auto flushBatch = [&]() {
        if (queued > 0) {
            renderer.drawTexturedTriangles(verts, uvs, cols, queued * 6);
            queued = 0;
        }
    };

    curX = startX;
    for (const auto& g : state.shapedRun.glyphs) {
        if (g.glyphIndex == 0) {
            curX += g.xAdvance;
            continue;
        }

        float glyphX = curX + g.xOffset;
        eatsbits::text::FontId fid = (g.fontId != eatsbits::text::kInvalidFontId) ? g.fontId : primaryFontId;
        const auto* ag = state.glyphAtlas.getOrLoadGlyph(state.fontMgr, fid, g.glyphIndex, fontSize, glyphX);
        if (!ag || ag->width <= 0 || ag->height <= 0) {
            curX += g.xAdvance;
            continue;
        }

        float qx0 = glyphX + ag->bearingX;
        float qy0 = baselineY - ag->bearingY;
        float qx1 = qx0 + static_cast<float>(ag->width);
        float qy1 = qy0 + static_cast<float>(ag->height);

        float u0 = ag->u0;
        float v0 = ag->v0;
        float u1 = ag->u1;
        float v1 = ag->v1;

        int vi = queued * 12;
        int ci = queued * 6;

        // Triangle 1: (x0, y0), (x1, y0), (x0, y1)
        verts[vi + 0] = qx0; verts[vi + 1] = qy0;
        uvs[vi + 0]   = u0;  uvs[vi + 1]   = v0;

        verts[vi + 2] = qx1; verts[vi + 3] = qy0;
        uvs[vi + 2]   = u1;  uvs[vi + 3]   = v0;

        verts[vi + 4] = qx0; verts[vi + 5] = qy1;
        uvs[vi + 4]   = u0;  uvs[vi + 5]   = v1;

        // Triangle 2: (x1, y0), (x1, y1), (x0, y1)
        verts[vi + 6] = qx1; verts[vi + 7] = qy0;
        uvs[vi + 6]   = u1;  uvs[vi + 7]   = v0;

        verts[vi + 8] = qx1; verts[vi + 9] = qy1;
        uvs[vi + 8]   = u1;  uvs[vi + 9]   = v1;

        verts[vi + 10] = qx0; verts[vi + 11] = qy1;
        uvs[vi + 10]   = u0;  uvs[vi + 11]   = v1;

        for (int k = 0; k < 6; ++k) {
            cols[ci + k] = color;
        }

        ++queued;
        if (queued >= kMaxBatchGlyphs) {
            flushBatch();
        }

        curX += g.xAdvance;
    }

    flushBatch();
}

inline void parseHexColor(const std::string& hex, float& r, float& g, float& b) {
    if (hex.size() >= 7 && hex[0] == '#') {
        try {
            int red = std::stoi(hex.substr(1, 2), nullptr, 16);
            int grn = std::stoi(hex.substr(3, 2), nullptr, 16);
            int blu = std::stoi(hex.substr(5, 2), nullptr, 16);
            r = red / 255.0f;
            g = grn / 255.0f;
            b = blu / 255.0f;
            return;
        } catch (...) {}
    }
    r = 0.0f; g = 0.9f; b = 1.0f; // Default cyan
}

#if EATS_HAS_GLFW
void drawTriangle(float x0, float y0, float x1, float y1, float x2, float y2, float r, float g, float b, float a = 1.0f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawTriangle(x0, y0, x1, y1, x2, y2, r, g, b, a);
    }
}

void drawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRect(x, y, w, h, r, g, b, a);
    }
}

void drawRectGradient(float x, float y, float w, float h, float r0, float g0, float b0, float r1, float g1, float b1, float a = 1.0f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRectGradient(x, y, w, h, r0, g0, b0, r1, g1, b1, a);
    }
}

void drawRectOutline(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f, float lineWidth = 1.0f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRectOutline(x, y, w, h, r, g, b, a, lineWidth);
    }
}

void drawRoundedRect(float x, float y, float w, float h, float radius, float r, float g, float b, float a = 1.0f, int cornerSegments = 8) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRoundedRect(x, y, w, h, radius, r, g, b, a, cornerSegments);
    }
}

void drawRoundedRectGradient(float x, float y, float w, float h, float radius, float r0, float g0, float b0, float r1, float g1, float b1, float a = 1.0f, int cornerSegments = 8) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRoundedRectGradient(x, y, w, h, radius, r0, g0, b0, r1, g1, b1, a, cornerSegments);
    }
}

void drawRoundedRectOutline(float x, float y, float w, float h, float radius, float r, float g, float b, float a = 1.0f, float lineWidth = 1.0f, int cornerSegments = 8) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRoundedRectOutline(x, y, w, h, radius, r, g, b, a, lineWidth, cornerSegments);
    }
}

void drawCircle(float cx, float cy, float radius, float r, float g, float b, float a = 1.0f, int segments = 36) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCircle(cx, cy, radius, r, g, b, a, segments);
    }
}

void drawCircleRadialGradient(float cx, float cy, float radius,
                              float innerR, float innerG, float innerB, float innerA,
                              float outerR, float outerG, float outerB, float outerA,
                              float offX = 0.0f, float offY = 0.0f, int segments = 36) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCircleRadialGradient(cx, cy, radius, innerR, innerG, innerB, innerA, outerR, outerG, outerB, outerA, offX, offY, segments);
    }
}

void drawCircleRadial3StopGradient(float cx, float cy, float radius,
                                   float innerR, float innerG, float innerB, float innerA,
                                   float midR, float midG, float midB, float midA,
                                   float outerR, float outerG, float outerB, float outerA,
                                   float offX = 0.0f, float offY = 0.0f, float midStop = 0.50f,
                                   int segments = 36) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCircleRadial3StopGradient(cx, cy, radius, innerR, innerG, innerB, innerA, midR, midG, midB, midA, outerR, outerG, outerB, outerA, offX, offY, midStop, segments);
    }
}

void drawCircleLinearGradient(float cx, float cy, float radius,
                              float r0, float g0, float b0, float a0,
                              float r1, float g1, float b1, float a1,
                              float angleRad = 1.5707963f, int segments = 36) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCircleLinearGradient(cx, cy, radius, r0, g0, b0, a0, r1, g1, b1, a1, angleRad, segments);
    }
}

void drawCircleOutline(float cx, float cy, float radius, float r, float g, float b, float a = 1.0f, float lineWidth = 1.5f, int segments = 36) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCircleOutline(cx, cy, radius, r, g, b, a, lineWidth, segments);
    }
}

void drawArc(float cx, float cy, float radius, float startAngle, float endAngle, float r, float g, float b, float a = 1.0f, float lineWidth = 2.0f, int segments = 36) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawArc(cx, cy, radius, startAngle, endAngle, r, g, b, a, lineWidth, segments);
    }
}

void drawLine(float x0, float y0, float x1, float y1, float r, float g, float b, float a = 1.0f, float lineWidth = 1.5f) {
    BatchRenderer2D* ren = getEffectiveBatchRenderer();
    if (ren) {
        ren->drawLine(x0, y0, x1, y1, r, g, b, a, lineWidth);
    }
}

void drawCatenaryBezier(Point2D p0, Point2D cp0, Point2D cp1, Point2D p1, float r, float g, float b, float a, float lineWidth) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCatenaryBezier(p0, cp0, cp1, p1, r, g, b, a, lineWidth);
    }
}

void drawCircleDropShadow(float cx, float cy, float radius, float elevation, const LightSource2D& light, float opacity = 0.45f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCircleDropShadow(cx, cy, radius, elevation, light, opacity);
    }
}

void drawRectDropShadow(float x, float y, float w, float h, float cornerRadius, float elevation, const LightSource2D& light, float opacity = 0.40f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRectDropShadow(x, y, w, h, cornerRadius, elevation, light, opacity);
    }
}

void drawKnurledRing(float cx, float cy, float rInner, float rOuter, int numTeeth, float rotationAngle,
                     const LightSource2D& light, float baseR, float baseG, float baseB,
                     float highlightR = 1.0f, float highlightG = 1.0f, float highlightB = 1.0f,
                     float shadowR = 0.08f, float shadowG = 0.09f, float shadowB = 0.11f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawKnurledRing(cx, cy, rInner, rOuter, numTeeth, rotationAngle, light,
                                              baseR, baseG, baseB, highlightR, highlightG, highlightB,
                                              shadowR, shadowG, shadowB);
    }
}

void drawFlutedRing(float cx, float cy, float rInner, float rOuter, int numFlutes,
                    float fluteDepthRatio, float rotationAngle, const LightSource2D& light,
                    float baseR, float baseG, float baseB,
                    float highlightR = 1.0f, float highlightG = 1.0f, float highlightB = 1.0f,
                    float shadowR = 0.08f, float shadowG = 0.09f, float shadowB = 0.11f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawFlutedRing(cx, cy, rInner, rOuter, numFlutes, fluteDepthRatio, rotationAngle,
                                             light, baseR, baseG, baseB, highlightR, highlightG, highlightB,
                                             shadowR, shadowG, shadowB);
    }
}

struct VectorMeshStringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
    size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

struct CachedPathMesh {
    std::vector<SvgPoint> triangles;
    SvgRect bounds;
};
static std::unordered_map<std::string, CachedPathMesh, VectorMeshStringHash, std::equal_to<>> s_vectorMeshCache;

void drawSvgPath(std::string_view path, float x, float y, float w, float h, uint32_t color, float opacity = 1.0f) {
    if (path.empty() || w <= 0.0f || h <= 0.0f || !g_activeBatchRenderer) return;

    auto it = s_vectorMeshCache.find(path);
    if (it == s_vectorMeshCache.end()) {
        SvgPath p = SvgPath::parse(path);
        CachedPathMesh mesh;
        mesh.triangles = p.triangulate();
        mesh.bounds = p.getBounds();
        it = s_vectorMeshCache.emplace(std::string(path), std::move(mesh)).first;
    }

    const auto& mesh = it->second;
    const auto& b = mesh.bounds;
    if (b.isEmpty() || mesh.triangles.empty()) return;

    float ubW = std::max(0.001f, b.width());
    float ubH = std::max(0.001f, b.height());
    float scale = std::min(w / ubW, h / ubH);
    float offsetX = x + (w - ubW * scale) * 0.5f - b.minX * scale;
    float offsetY = y + (h - ubH * scale) * 0.5f - b.minY * scale;

    uint8_t r = static_cast<uint8_t>(color & 0xFF);
    uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFF);
    uint8_t b_col = static_cast<uint8_t>((color >> 16) & 0xFF);
    uint8_t a = static_cast<uint8_t>((color >> 24) & 0xFF);
    float alpha = (static_cast<float>(a) / 255.0f) * opacity;

    const auto& tris = mesh.triangles;
    const size_t numTris = tris.size() / 3;
    for (size_t t = 0; t < numTris; ++t) {
        const auto& p0 = tris[t * 3 + 0];
        const auto& p1 = tris[t * 3 + 1];
        const auto& p2 = tris[t * 3 + 2];
        g_activeBatchRenderer->drawTriangle(
            p0.x * scale + offsetX, p0.y * scale + offsetY,
            p1.x * scale + offsetX, p1.y * scale + offsetY,
            p2.x * scale + offsetX, p2.y * scale + offsetY,
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b_col) / 255.0f,
            alpha
        );
    }
}

void drawSvgLayers(std::span<const SvgLayerDef> layers, float x, float y, float w, float h, float opacity = 1.0f) {
    if (layers.empty() || w <= 0.0f || h <= 0.0f || !g_activeBatchRenderer) return;

    SvgRect unionBounds{};
    for (const auto& layer : layers) {
        auto it = s_vectorMeshCache.find(layer.path);
        if (it == s_vectorMeshCache.end()) {
            SvgPath p = SvgPath::parse(layer.path);
            CachedPathMesh mesh;
            mesh.triangles = p.triangulate();
            mesh.bounds = p.getBounds();
            it = s_vectorMeshCache.emplace(layer.path, std::move(mesh)).first;
        }
        const auto& b = it->second.bounds;
        if (!b.isEmpty()) {
            if (unionBounds.isEmpty()) {
                unionBounds = b;
            } else {
                unionBounds.minX = std::min(unionBounds.minX, b.minX);
                unionBounds.minY = std::min(unionBounds.minY, b.minY);
                unionBounds.maxX = std::max(unionBounds.maxX, b.maxX);
                unionBounds.maxY = std::max(unionBounds.maxY, b.maxY);
            }
        }
    }

    if (unionBounds.isEmpty()) return;

    float ubW = std::max(0.001f, unionBounds.width());
    float ubH = std::max(0.001f, unionBounds.height());
    float scale = std::min(w / ubW, h / ubH);
    float offsetX = x + (w - ubW * scale) * 0.5f - unionBounds.minX * scale;
    float offsetY = y + (h - ubH * scale) * 0.5f - unionBounds.minY * scale;

    for (const auto& layer : layers) {
        auto it = s_vectorMeshCache.find(layer.path);
        if (it == s_vectorMeshCache.end()) continue;
        const auto& tris = it->second.triangles;
        if (tris.empty()) continue;

        uint8_t r = static_cast<uint8_t>(layer.color & 0xFF);
        uint8_t g = static_cast<uint8_t>((layer.color >> 8) & 0xFF);
        uint8_t b = static_cast<uint8_t>((layer.color >> 16) & 0xFF);
        uint8_t a = static_cast<uint8_t>((layer.color >> 24) & 0xFF);
        float alpha = (static_cast<float>(a) / 255.0f) * layer.opacity * opacity;

        const size_t numTris = tris.size() / 3;
        for (size_t t = 0; t < numTris; ++t) {
            const auto& p0 = tris[t * 3 + 0];
            const auto& p1 = tris[t * 3 + 1];
            const auto& p2 = tris[t * 3 + 2];
            g_activeBatchRenderer->drawTriangle(
                p0.x * scale + offsetX, p0.y * scale + offsetY,
                p1.x * scale + offsetX, p1.y * scale + offsetY,
                p2.x * scale + offsetX, p2.y * scale + offsetY,
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                alpha
            );
        }
    }
}

namespace {

inline void drawChassisPlate(float x, float y, float w, float h, bool isTopPanel, const ThemeTokens& theme) {
    if (!g_activeBatchRenderer || w <= 0.0f || h <= 0.0f) return;

    Color gradTop = isTopPanel ? theme.panelHeaderGradientTop : theme.panelHeaderGradientTop.darken(0.04f);
    Color gradBot = isTopPanel ? theme.panelHeaderGradientBottom : theme.panelHeaderGradientBottom.darken(0.08f);

    g_activeBatchRenderer->drawRectGradient(x, y, w, h,
                                           gradTop.r, gradTop.g, gradTop.b,
                                           gradBot.r, gradBot.g, gradBot.b, 1.0f);

    // Subtle 1px specular bevel highlight along outer edge & crisp border separator
    if (isTopPanel) {
        drawLine(x, y + 1.0f, x + w, y + 1.0f,
                 gradTop.lighten(0.12f).r, gradTop.lighten(0.12f).g, gradTop.lighten(0.12f).b, 0.40f, 1.0f);
        drawLine(x, y + h - 1.0f, x + w, y + h - 1.0f,
                 theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.65f, 1.0f);
    } else {
        drawLine(x, y, x + w, y,
                 theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.65f, 1.0f);
        drawLine(x, y + 1.0f, x + w, y + 1.0f,
                 gradTop.lighten(0.10f).r, gradTop.lighten(0.10f).g, gradTop.lighten(0.10f).b, 0.30f, 1.0f);
    }
}

inline void drawButtonNoise(float /*x*/, float /*y*/, float /*w*/, float /*h*/, bool /*isTop*/ = true, float /*opacity*/ = 0.28f,
                            float /*cornerRadius*/ = 0.0f, bool /*roundTL*/ = true, bool /*roundTR*/ = true,
                            bool /*roundBL*/ = true, bool /*roundBR*/ = true, bool /*pressed*/ = false, float /*pressShiftY*/ = 3.0f) {
    // Pure vector DAW mode: zero per-pixel CPU loops and zero GPU texture re-uploads
}

inline void setBlendMode(BlendMode mode) {
    if (g_activeBatchRenderer) g_activeBatchRenderer->setBlendMode(mode);
}

struct ScopedBlendMode {
    BatchRenderer2D* r;
    BlendMode prev;
    ScopedBlendMode(BlendMode mode) : r(g_activeBatchRenderer), prev(BlendMode::Normal) {
        if (r) {
            prev = r->getBlendMode();
            r->setBlendMode(mode);
        }
    }
    ~ScopedBlendMode() {
        if (r) r->setBlendMode(prev);
    }
};

} // namespace

void drawEatsbitsLogo(float x, float y, float size, float opacity = 1.0f, bool inverted = false) {
    static const auto s_logoLayers = SvgLogo::getLayers();
    static const auto s_invertedLayers = SvgLogo::getInvertedLayers();
    drawSvgLayers(inverted ? s_invertedLayers : s_logoLayers, x, y, size, size, opacity);
}

inline uint32_t packSvgColor(const Color& c, float extraAlpha = 1.0f) {
    auto ur = static_cast<uint32_t>(std::clamp(c.r * 255.0f, 0.0f, 255.0f));
    auto ug = static_cast<uint32_t>(std::clamp(c.g * 255.0f, 0.0f, 255.0f));
    auto ub = static_cast<uint32_t>(std::clamp(c.b * 255.0f, 0.0f, 255.0f));
    auto ua = static_cast<uint32_t>(std::clamp(c.a * extraAlpha * 255.0f, 0.0f, 255.0f));
    return ur | (ug << 8) | (ub << 16) | (ua << 24);
}

void drawPlainEatsbitsLogo(float x, float y, float w, float h, const Color& color, float opacity = 1.0f,
                           bool inverted = false, const Color& darkColor = Color(0.08f, 0.07f, 0.06f)) {
    if (!inverted) {
        // Subtle drop shadow / stamped depth edge for realistic faceplate silkscreen
        uint32_t shadowCol = packSvgColor(Color(0.0f, 0.0f, 0.0f, 0.50f));
        drawSvgPath(SvgLogo::kPlainLogoPath, x + 0.65f, y + 0.85f, w, h, shadowCol, 0.40f * opacity);

        // Stamped silkscreen emblem with theme accent color
        uint32_t emblemCol = packSvgColor(color);
        drawSvgPath(SvgLogo::kPlainLogoPath, x, y, w, h, emblemCol, opacity);
    } else {
        // Inverted state on hover: dark creature body with illuminated accent outline, eye, and eating bits
        uint32_t accentCol = packSvgColor(color);
        uint32_t darkCol = packSvgColor(darkColor);
        auto invertedLayers = SvgLogo::getPlainInvertedLayers(accentCol, darkCol);
        drawSvgLayers(invertedLayers, x, y, w, h, opacity);
    }
}

// Clean vector stroke typography for labels and unicode music/box characters
void drawVectorGlyph(BatchRenderer2D& renderer, uint32_t c, float x, float y, float scale, float r, float g, float b, float a = 1.0f) {
    auto seg = [&](float x0, float y0, float x1, float y1) {
        renderer.drawLine(x0, y0, x1, y1, r, g, b, a, 1.5f * scale);
    };

    if (c < 128) {
        switch (std::toupper(static_cast<char>(c))) {
            case 'A':
                seg(x, y + 8 * scale, x + 2.5f * scale, y);
                seg(x + 2.5f * scale, y, x + 5 * scale, y + 8 * scale);
                seg(x + 1.2f * scale, y + 4.5f * scale, x + 3.8f * scale, y + 4.5f * scale);
                break;
            case 'B':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4 * scale, y + 2 * scale);
                seg(x + 4 * scale, y + 2 * scale, x, y + 4 * scale);
                seg(x, y + 4 * scale, x + 4 * scale, y + 6 * scale);
                seg(x + 4 * scale, y + 6 * scale, x, y + 8 * scale);
                break;
            case 'C':
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                break;
            case 'D':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4.5f * scale, y + 4 * scale);
                seg(x + 4.5f * scale, y + 4 * scale, x, y + 8 * scale);
                break;
            case 'E':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4.5f * scale, y);
                seg(x, y + 4 * scale, x + 3.5f * scale, y + 4 * scale);
                seg(x, y + 8 * scale, x + 4.5f * scale, y + 8 * scale);
                break;
            case 'F':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4.5f * scale, y);
                seg(x, y + 4 * scale, x + 3.5f * scale, y + 4 * scale);
                break;
            case 'G':
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x + 5 * scale, y + 4 * scale);
                seg(x + 5 * scale, y + 4 * scale, x + 2.5f * scale, y + 4 * scale);
                break;
            case 'H':
                seg(x, y, x, y + 8 * scale);
                seg(x + 5 * scale, y, x + 5 * scale, y + 8 * scale);
                seg(x, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                break;
            case 'I':
                seg(x + 1 * scale, y, x + 4 * scale, y);
                seg(x + 2.5f * scale, y, x + 2.5f * scale, y + 8 * scale);
                seg(x + 1 * scale, y + 8 * scale, x + 4 * scale, y + 8 * scale);
                break;
            case 'K':
                seg(x, y, x, y + 8 * scale);
                seg(x + 4.5f * scale, y, x, y + 4 * scale);
                seg(x, y + 4 * scale, x + 4.5f * scale, y + 8 * scale);
                break;
            case 'L':
                seg(x, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 4.5f * scale, y + 8 * scale);
                break;
            case 'M':
                seg(x, y + 8 * scale, x, y);
                seg(x, y, x + 2.5f * scale, y + 5 * scale);
                seg(x + 2.5f * scale, y + 5 * scale, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 5 * scale, y + 8 * scale);
                break;
            case 'N':
                seg(x, y + 8 * scale, x, y);
                seg(x, y, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x + 5 * scale, y);
                break;
            case 'O':
            case '0':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x, y + 8 * scale);
                seg(x, y + 8 * scale, x, y);
                break;
            case 'P':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4.5f * scale, y);
                seg(x + 4.5f * scale, y, x + 4.5f * scale, y + 4.5f * scale);
                seg(x + 4.5f * scale, y + 4.5f * scale, x, y + 4.5f * scale);
                break;
            case 'R':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4.5f * scale, y);
                seg(x + 4.5f * scale, y, x + 4.5f * scale, y + 4.5f * scale);
                seg(x + 4.5f * scale, y + 4.5f * scale, x, y + 4.5f * scale);
                seg(x + 1.5f * scale, y + 4.5f * scale, x + 4.5f * scale, y + 8 * scale);
                break;
            case 'S':
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 4 * scale);
                seg(x, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                seg(x + 5 * scale, y + 4 * scale, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x, y + 8 * scale);
                break;
            case 'T':
                seg(x, y, x + 5 * scale, y);
                seg(x + 2.5f * scale, y, x + 2.5f * scale, y + 8 * scale);
                break;
            case 'U':
                seg(x, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x + 5 * scale, y);
                break;
            case 'V':
                seg(x, y, x + 2.5f * scale, y + 8 * scale);
                seg(x + 2.5f * scale, y + 8 * scale, x + 5 * scale, y);
                break;
            case 'W':
                seg(x, y, x + 1 * scale, y + 8 * scale);
                seg(x + 1 * scale, y + 8 * scale, x + 2.5f * scale, y + 3 * scale);
                seg(x + 2.5f * scale, y + 3 * scale, x + 4 * scale, y + 8 * scale);
                seg(x + 4 * scale, y + 8 * scale, x + 5 * scale, y);
                break;
            case 'Y':
                seg(x, y, x + 2.5f * scale, y + 4 * scale);
                seg(x + 5 * scale, y, x + 2.5f * scale, y + 4 * scale);
                seg(x + 2.5f * scale, y + 4 * scale, x + 2.5f * scale, y + 8 * scale);
                break;
            case '1':
                seg(x + 1.5f * scale, y + 2 * scale, x + 3 * scale, y);
                seg(x + 3 * scale, y, x + 3 * scale, y + 8 * scale);
                seg(x + 1 * scale, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                break;
            case '2':
                seg(x, y + 2 * scale, x + 3 * scale, y);
                seg(x + 3 * scale, y, x + 5 * scale, y + 2 * scale);
                seg(x + 5 * scale, y + 2 * scale, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                break;
            case '3':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 2.5f * scale, y + 4 * scale);
                seg(x + 2.5f * scale, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                seg(x + 5 * scale, y + 4 * scale, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x, y + 8 * scale);
                break;
            case '4':
                seg(x, y, x, y + 4.5f * scale);
                seg(x, y + 4.5f * scale, x + 5 * scale, y + 4.5f * scale);
                seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8 * scale);
                break;
            case '5':
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 3.5f * scale);
                seg(x, y + 3.5f * scale, x + 4.5f * scale, y + 3.5f * scale);
                seg(x + 4.5f * scale, y + 3.5f * scale, x + 4.5f * scale, y + 8 * scale);
                seg(x + 4.5f * scale, y + 8 * scale, x, y + 8 * scale);
                break;
            case '6':
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x + 5 * scale, y + 4 * scale);
                seg(x + 5 * scale, y + 4 * scale, x, y + 4 * scale);
                break;
            case '7':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 1.5f * scale, y + 8 * scale);
                break;
            case '8':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x, y + 8 * scale);
                seg(x, y + 8 * scale, x, y);
                seg(x, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                break;
            case '9':
                seg(x + 5 * scale, y + 8 * scale, x + 5 * scale, y);
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 4 * scale);
                seg(x, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                break;
            case '%':
                seg(x + 0.8f * scale, y + 1.2f * scale, x + 2.0f * scale, y + 1.2f * scale);
                seg(x + 3.0f * scale, y + 6.8f * scale, x + 4.2f * scale, y + 6.8f * scale);
                seg(x + 4.5f * scale, y + 0.5f * scale, x + 0.5f * scale, y + 7.5f * scale);
                break;
            case '-':
                seg(x + 1 * scale, y + 4 * scale, x + 4 * scale, y + 4 * scale);
                break;
            case ':':
                seg(x + 2 * scale, y + 2.5f * scale, x + 3 * scale, y + 2.5f * scale);
                seg(x + 2 * scale, y + 5.5f * scale, x + 3 * scale, y + 5.5f * scale);
                break;
            case '.':
                seg(x + 2 * scale, y + 7.5f * scale, x + 3 * scale, y + 7.5f * scale);
                break;
            case 'Q':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x, y + 8 * scale);
                seg(x, y + 8 * scale, x, y);
                seg(x + 3 * scale, y + 5 * scale, x + 5 * scale, y + 8 * scale);
                break;
            case 'X':
                seg(x, y, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y, x, y + 8 * scale);
                break;
            case 'Z':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                break;
            case '>':
                seg(x + 1 * scale, y, x + 4.5f * scale, y + 4 * scale);
                seg(x + 4.5f * scale, y + 4 * scale, x + 1 * scale, y + 8 * scale);
                break;
            case '<':
                seg(x + 4 * scale, y, x + 0.5f * scale, y + 4 * scale);
                seg(x + 0.5f * scale, y + 4 * scale, x + 4 * scale, y + 8 * scale);
                break;
            case '|':
                seg(x + 2.5f * scale, y, x + 2.5f * scale, y + 8 * scale);
                break;
            case '[':
                seg(x + 4 * scale, y, x + 1.5f * scale, y);
                seg(x + 1.5f * scale, y, x + 1.5f * scale, y + 8 * scale);
                seg(x + 1.5f * scale, y + 8 * scale, x + 4 * scale, y + 8 * scale);
                break;
            case ']':
                seg(x + 1 * scale, y, x + 3.5f * scale, y);
                seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8 * scale);
                seg(x + 3.5f * scale, y + 8 * scale, x + 1 * scale, y + 8 * scale);
                break;
            case '(':
                seg(x + 4 * scale, y, x + 2 * scale, y + 4 * scale);
                seg(x + 2 * scale, y + 4 * scale, x + 4 * scale, y + 8 * scale);
                break;
            case ')':
                seg(x + 1 * scale, y, x + 3 * scale, y + 4 * scale);
                seg(x + 3 * scale, y + 4 * scale, x + 1 * scale, y + 8 * scale);
                break;
            case '#':
                seg(x + 1.5f * scale, y, x + 1.5f * scale, y + 8 * scale);
                seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8 * scale);
                seg(x, y + 2.5f * scale, x + 5 * scale, y + 2.5f * scale);
                seg(x, y + 5.5f * scale, x + 5 * scale, y + 5.5f * scale);
                break;
            case '=':
                seg(x + 0.5f * scale, y + 2.5f * scale, x + 4.5f * scale, y + 2.5f * scale);
                seg(x + 0.5f * scale, y + 5.5f * scale, x + 4.5f * scale, y + 5.5f * scale);
                break;
            case '+':
                seg(x + 2.5f * scale, y + 1 * scale, x + 2.5f * scale, y + 7 * scale);
                seg(x, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                break;
            case '/':
                seg(x + 4.5f * scale, y, x + 0.5f * scale, y + 8 * scale);
                break;
            default:
                break;
        }
        return;
    }

    // Unicode Musical Notes & Box-Drawing Characters
    switch (c) {
        case 0x2669: // ♩ Quarter note
            drawCircle(x + 2.0f * scale, y + 6.5f * scale, 1.8f * scale, r, g, b, a, 10);
            seg(x + 3.5f * scale, y + 6.5f * scale, x + 3.5f * scale, y + 1.0f * scale);
            break;
        case 0x266A: // ♪ Eighth note
            drawCircle(x + 2.0f * scale, y + 6.5f * scale, 1.8f * scale, r, g, b, a, 10);
            seg(x + 3.5f * scale, y + 6.5f * scale, x + 3.5f * scale, y + 1.0f * scale);
            seg(x + 3.5f * scale, y + 1.0f * scale, x + 5.5f * scale, y + 3.0f * scale);
            break;
        case 0x266B: // ♫ Beamed eighth note pair
            drawCircle(x + 1.5f * scale, y + 6.8f * scale, 1.5f * scale, r, g, b, a, 8);
            seg(x + 2.7f * scale, y + 6.8f * scale, x + 2.7f * scale, y + 1.5f * scale);
            drawCircle(x + 5.0f * scale, y + 5.8f * scale, 1.5f * scale, r, g, b, a, 8);
            seg(x + 6.2f * scale, y + 5.8f * scale, x + 6.2f * scale, y + 0.5f * scale);
            seg(x + 2.7f * scale, y + 1.5f * scale, x + 6.2f * scale, y + 0.5f * scale);
            seg(x + 2.7f * scale, y + 2.5f * scale, x + 6.2f * scale, y + 1.5f * scale);
            break;
        case 0x266C: // ♬ Beamed sixteenth note pair
            drawCircle(x + 1.5f * scale, y + 6.8f * scale, 1.5f * scale, r, g, b, a, 8);
            seg(x + 2.7f * scale, y + 6.8f * scale, x + 2.7f * scale, y + 1.5f * scale);
            drawCircle(x + 5.0f * scale, y + 5.8f * scale, 1.5f * scale, r, g, b, a, 8);
            seg(x + 6.2f * scale, y + 5.8f * scale, x + 6.2f * scale, y + 0.5f * scale);
            seg(x + 2.7f * scale, y + 1.2f * scale, x + 6.2f * scale, y + 0.2f * scale);
            seg(x + 2.7f * scale, y + 2.8f * scale, x + 6.2f * scale, y + 1.8f * scale);
            break;
        case 0x25B6: // ▶ Play
            drawTriangle(x + 1.0f * scale, y + 1.0f * scale,
                         x + 6.0f * scale, y + 4.0f * scale,
                         x + 1.0f * scale, y + 7.0f * scale, r, g, b, a);
            break;
        case 0x25A0: // ■ Stop
            drawRect(x + 1.0f * scale, y + 1.5f * scale, 5.0f * scale, 5.0f * scale, r, g, b, a);
            break;
        case 0x25CF: // ● Record
            drawCircle(x + 3.5f * scale, y + 4.0f * scale, 3.0f * scale, r, g, b, a, 12);
            break;
        case 0x2500: // ─ Box horizontal
            seg(x, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            break;
        case 0x2502: // │ Box vertical
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8.0f * scale);
            break;
        case 0x250C: // ┌ Box top-left
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 3.5f * scale, y + 8.0f * scale);
            break;
        case 0x2510: // ┐ Box top-right
            seg(x, y + 4.0f * scale, x + 3.5f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 3.5f * scale, y + 8.0f * scale);
            break;
        case 0x2514: // └ Box bottom-left
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            break;
        case 0x2518: // ┘ Box bottom-right
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 4.0f * scale);
            seg(x, y + 4.0f * scale, x + 3.5f * scale, y + 4.0f * scale);
            break;
        case 0x251C: // ├ Box tee-right
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8.0f * scale);
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            break;
        case 0x2524: // ┤ Box tee-left
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8.0f * scale);
            seg(x, y + 4.0f * scale, x + 3.5f * scale, y + 4.0f * scale);
            break;
        case 0x252C: // ┬ Box tee-down
            seg(x, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 3.5f * scale, y + 8.0f * scale);
            break;
        case 0x2534: // ┴ Box tee-up
            seg(x, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 4.0f * scale);
            break;
        case 0x253C: // ┼ Box cross
            seg(x, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8.0f * scale);
            break;
        case 0x21C6: // ⇆ Left arrow over right arrow (swap)
            // Top arrow (points left)
            seg(x + 6.2f * scale, y + 2.4f * scale, x + 1.0f * scale, y + 2.4f * scale);
            seg(x + 1.0f * scale, y + 2.4f * scale, x + 2.8f * scale, y + 1.0f * scale);
            seg(x + 1.0f * scale, y + 2.4f * scale, x + 2.8f * scale, y + 3.8f * scale);
            // Bottom arrow (points right)
            seg(x + 1.0f * scale, y + 5.6f * scale, x + 6.2f * scale, y + 5.6f * scale);
            seg(x + 6.2f * scale, y + 5.6f * scale, x + 4.4f * scale, y + 4.2f * scale);
            seg(x + 6.2f * scale, y + 5.6f * scale, x + 4.4f * scale, y + 7.0f * scale);
            break;
        case 0x21C4: // ⇄ Right arrow over left arrow (swap)
            // Top arrow (points right)
            seg(x + 1.0f * scale, y + 2.4f * scale, x + 6.2f * scale, y + 2.4f * scale);
            seg(x + 6.2f * scale, y + 2.4f * scale, x + 4.4f * scale, y + 1.0f * scale);
            seg(x + 6.2f * scale, y + 2.4f * scale, x + 4.4f * scale, y + 3.8f * scale);
            // Bottom arrow (points left)
            seg(x + 6.2f * scale, y + 5.6f * scale, x + 1.0f * scale, y + 5.6f * scale);
            seg(x + 1.0f * scale, y + 5.6f * scale, x + 2.8f * scale, y + 4.2f * scale);
            seg(x + 1.0f * scale, y + 5.6f * scale, x + 2.8f * scale, y + 7.0f * scale);
            break;
        case 0x2190: // ← Left arrow
            seg(x + 6.5f * scale, y + 4.0f * scale, x + 0.8f * scale, y + 4.0f * scale);
            seg(x + 0.8f * scale, y + 4.0f * scale, x + 3.0f * scale, y + 1.6f * scale);
            seg(x + 0.8f * scale, y + 4.0f * scale, x + 3.0f * scale, y + 6.4f * scale);
            break;
        case 0x2192: // → Right arrow
            seg(x + 0.8f * scale, y + 4.0f * scale, x + 6.5f * scale, y + 4.0f * scale);
            seg(x + 6.5f * scale, y + 4.0f * scale, x + 4.3f * scale, y + 1.6f * scale);
            seg(x + 6.5f * scale, y + 4.0f * scale, x + 4.3f * scale, y + 6.4f * scale);
            break;
        case 0x2194: // ↔ Left right arrow
            seg(x + 1.0f * scale, y + 4.0f * scale, x + 6.2f * scale, y + 4.0f * scale);
            seg(x + 1.0f * scale, y + 4.0f * scale, x + 2.8f * scale, y + 2.2f * scale);
            seg(x + 1.0f * scale, y + 4.0f * scale, x + 2.8f * scale, y + 5.8f * scale);
            seg(x + 6.2f * scale, y + 4.0f * scale, x + 4.4f * scale, y + 2.2f * scale);
            seg(x + 6.2f * scale, y + 4.0f * scale, x + 4.4f * scale, y + 5.8f * scale);
            break;
        default:
            break;
    }
}

void drawVectorGlyph(uint32_t c, float x, float y, float scale, float r, float g, float b, float a = 1.0f) {
    BatchRenderer2D* ren = getEffectiveBatchRenderer();
    if (ren) {
        drawVectorGlyph(*ren, c, x, y, scale, r, g, b, a);
    }
}

inline uint32_t decodeNextUtf8(const char*& p, const char* end) {
    if (p >= end) return 0;
    unsigned char c = static_cast<unsigned char>(*p++);
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && p < end) {
        uint32_t cp = (c & 0x1F) << 6;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F);
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && p + 1 < end) {
        uint32_t cp = (c & 0x0F) << 12;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F);
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && p + 2 < end) {
        uint32_t cp = (c & 0x07) << 18;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F) << 12;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F);
        return cp;
    }
    return c;
}

inline bool isSpecialSymbol(uint32_t cp) {
    return (cp >= 0x2500 && cp <= 0x257F) || // Box drawing
           (cp >= 0x2660 && cp <= 0x266F) || // Music notes
           (cp >= 0x25A0 && cp <= 0x25FF) || // Geometric shapes
           (cp >= 0x2190 && cp <= 0x21FF);   // Arrows and swap symbols (⇄, ⇆, etc.)
}

void drawVectorString(BatchRenderer2D& renderer, std::string_view str, float x, float y, float scale, float r, float g, float b, float a) {
    if (str.empty()) return;

    bool hasSpecial = false;
    for (unsigned char c : str) {
        if (c >= 0x80) {
            hasSpecial = true;
            break;
        }
    }

    if (g_activeFontRenderer && g_activeFontRenderer->fontNormal != eatsbits::text::kInvalidFontId) {
        float fontSize = std::max(10.0f, scale * 15.5f);
        uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint8_t ca = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        uint32_t col = static_cast<uint32_t>(cr) |
                      (static_cast<uint32_t>(cg) << 8) |
                      (static_cast<uint32_t>(cb) << 16) |
                      (static_cast<uint32_t>(ca) << 24);

        if (!hasSpecial) {
            if (g_activeFontRenderer->textShaper.shapeText(g_activeFontRenderer->fontMgr,
                                                           g_activeFontRenderer->fontNormal,
                                                           fontSize, str,
                                                           g_activeFontRenderer->shapedRun)) {
                FT_Face face = g_activeFontRenderer->fontMgr.getFtFace(g_activeFontRenderer->fontNormal);
                float ascender = fontSize * 0.82f;
                if (face && face->size) {
                    float ftAsc = static_cast<float>(face->size->metrics.ascender) / 64.0f;
                    if (ftAsc > 0.0f) ascender = ftAsc;
                }
                float baselineY = y + ascender;
                renderShapedGlyphs(renderer, *g_activeFontRenderer,
                                   g_activeFontRenderer->fontNormal, fontSize, x, baselineY, col);
                return;
            }
        } else {
            // Hybrid UTF-8 processor: shapes non-special chunks via HarfBuzz and routes special symbols to vector glyphs
            const char* ptr = str.data();
            const char* end = ptr + str.size();
            float curX = x;
            const float charW = 7.5f * scale;

            FT_Face face = g_activeFontRenderer->fontMgr.getFtFace(g_activeFontRenderer->fontNormal);
            float ascender = fontSize * 0.82f;
            if (face && face->size) {
                float ftAsc = static_cast<float>(face->size->metrics.ascender) / 64.0f;
                if (ftAsc > 0.0f) ascender = ftAsc;
            }
            float baselineY = y + ascender;

            while (ptr < end) {
                const char* prevPtr = ptr;
                uint32_t cp = decodeNextUtf8(ptr, end);
                if (cp == ' ') {
                    curX += charW;
                    continue;
                }
                if (isSpecialSymbol(cp)) {
                    drawVectorGlyph(renderer, cp, curX, y, scale, r, g, b, a);
                    curX += charW;
                } else {
                    std::string_view chunk(prevPtr, static_cast<size_t>(ptr - prevPtr));
                    if (g_activeFontRenderer->textShaper.shapeText(g_activeFontRenderer->fontMgr,
                                                                   g_activeFontRenderer->fontNormal,
                                                                   fontSize, chunk,
                                                                   g_activeFontRenderer->shapedRun)) {
                        renderShapedGlyphs(renderer, *g_activeFontRenderer,
                                           g_activeFontRenderer->fontNormal, fontSize, curX, baselineY, col);
                        curX += g_activeFontRenderer->shapedRun.totalAdvanceX;
                    } else {
                        drawVectorGlyph(renderer, cp, curX, y, scale, r, g, b, a);
                        curX += charW;
                    }
                }
            }
            return;
        }
    }

    // Pure vector fallback
    const char* ptr = str.data();
    const char* end = ptr + str.size();
    float curX = x;
    const float charW = 7.5f * scale;
    while (ptr < end) {
        uint32_t cp = decodeNextUtf8(ptr, end);
        if (cp == ' ') {
            curX += charW;
            continue;
        }
        drawVectorGlyph(renderer, cp, curX, y, scale, r, g, b, a);
        curX += charW;
    }
}

void drawVectorString(std::string_view str, float x, float y, float scale, float r, float g, float b, float a) {
    BatchRenderer2D* ren = getEffectiveBatchRenderer();
    if (ren) {
        drawVectorString(*ren, str, x, y, scale, r, g, b, a);
    }
}

void drawVectorStringCentered(BatchRenderer2D& renderer, std::string_view str, float cx, float cy, float scale, float r, float g, float b, float a) {
    if (str.empty()) return;

    if (g_activeFontRenderer && g_activeFontRenderer->fontNormal != eatsbits::text::kInvalidFontId) {
        float fontSize = std::max(10.0f, scale * 15.5f);
        uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint8_t ca = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        uint32_t col = static_cast<uint32_t>(cr) |
                      (static_cast<uint32_t>(cg) << 8) |
                      (static_cast<uint32_t>(cb) << 16) |
                      (static_cast<uint32_t>(ca) << 24);

        if (g_activeFontRenderer->textShaper.shapeText(g_activeFontRenderer->fontMgr,
                                                       g_activeFontRenderer->fontNormal,
                                                       fontSize, str,
                                                       g_activeFontRenderer->shapedRun)) {
            FT_Face face = g_activeFontRenderer->fontMgr.getFtFace(g_activeFontRenderer->fontNormal);
            float ascender = fontSize * 0.82f;
            if (face && face->size) {
                float ftAsc = static_cast<float>(face->size->metrics.ascender) / 64.0f;
                if (ftAsc > 0.0f) ascender = ftAsc;
            }

            // For single-glyph symbols/icons (e.g. knob center icons), use exact visual bounding box optical centering
            if (g_activeFontRenderer->shapedRun.glyphs.size() == 1) {
                const auto& glyphItem = g_activeFontRenderer->shapedRun.glyphs[0];
                eatsbits::text::FontId fid = (glyphItem.fontId != eatsbits::text::kInvalidFontId) ? glyphItem.fontId : g_activeFontRenderer->fontNormal;
                const auto* ag = g_activeFontRenderer->glyphAtlas.getOrLoadGlyph(g_activeFontRenderer->fontMgr, fid, glyphItem.glyphIndex, fontSize, cx);
                if (ag && ag->width > 0 && ag->height > 0) {
                    float glyphCenterX = (glyphItem.xOffset + ag->bearingX) + static_cast<float>(ag->width) * 0.5f;
                    float glyphCenterY = -ag->bearingY + static_cast<float>(ag->height) * 0.5f;
                    float startX = cx - glyphCenterX;
                    float baselineY = cy - glyphCenterY;
                    renderShapedGlyphs(renderer, *g_activeFontRenderer,
                                       g_activeFontRenderer->fontNormal, fontSize, startX, baselineY, col);
                    return;
                }
            }

            // Multi-glyph text: center using total advance and typographic ascender
            float totalW = g_activeFontRenderer->shapedRun.totalAdvanceX;
            float startX = cx - (totalW * 0.5f);
            float baselineY = cy + (ascender * 0.35f);
            renderShapedGlyphs(renderer, *g_activeFontRenderer,
                               g_activeFontRenderer->fontNormal, fontSize, startX, baselineY, col);
            return;
        }
    }

    // Pure vector fallback
    const float charW = 7.5f * scale;
    size_t numCodepoints = 0;
    const char* p = str.data();
    const char* e = p + str.size();
    while (p < e) {
        decodeNextUtf8(p, e);
        numCodepoints++;
    }
    float textW = static_cast<float>(numCodepoints) * charW;
    float startX = cx - (textW * 0.5f);
    float startY = cy - (4.0f * scale);
    drawVectorString(renderer, str, startX, startY, scale, r, g, b, a);
}

void drawVectorStringCentered(std::string_view str, float cx, float cy, float scale, float r, float g, float b, float a) {
    BatchRenderer2D* ren = getEffectiveBatchRenderer();
    if (ren) {
        drawVectorStringCentered(*ren, str, cx, cy, scale, r, g, b, a);
    }
}

float drawSyntaxString(std::string_view str, float x, float y, float scale, float r, float g, float b, float a = 1.0f) {
    if (str.empty()) return x;
    BatchRenderer2D* ren = getEffectiveBatchRenderer();
    if (!ren) return x;

    if (g_activeFontRenderer &&
        (g_activeFontRenderer->fontMono != eatsbits::text::kInvalidFontId || g_activeFontRenderer->fontNormal != eatsbits::text::kInvalidFontId)) {
        eatsbits::text::FontId fid = (g_activeFontRenderer->fontMono != eatsbits::text::kInvalidFontId)
                                     ? g_activeFontRenderer->fontMono
                                     : g_activeFontRenderer->fontNormal;
        float fontSize = std::max(10.0f, scale * 15.5f);
        uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint8_t ca = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        uint32_t col = static_cast<uint32_t>(cr) |
                      (static_cast<uint32_t>(cg) << 8) |
                      (static_cast<uint32_t>(cb) << 16) |
                      (static_cast<uint32_t>(ca) << 24);

        if (g_activeFontRenderer->textShaper.shapeText(g_activeFontRenderer->fontMgr,
                                                       fid, fontSize, str,
                                                       g_activeFontRenderer->shapedRun)) {
            FT_Face face = g_activeFontRenderer->fontMgr.getFtFace(fid);
            float ascender = fontSize * 0.82f;
            if (face && face->size) {
                float ftAsc = static_cast<float>(face->size->metrics.ascender) / 64.0f;
                if (ftAsc > 0.0f) ascender = ftAsc;
            }
            float baselineY = y + ascender;
            renderShapedGlyphs(*ren, *g_activeFontRenderer,
                               fid, fontSize, x, baselineY, col);
            return x + g_activeFontRenderer->shapedRun.totalAdvanceX;
        }
    }

    drawVectorString(*ren, str, x, y, scale, r, g, b, a);
    return x + static_cast<float>(str.size()) * (7.0f * scale);
}

void drawMonoString(BatchRenderer2D& renderer, std::string_view str, float x, float y, float scale, float r, float g, float b, float a) {
    if (str.empty()) return;

    if (g_activeFontRenderer &&
        (g_activeFontRenderer->fontMono != eatsbits::text::kInvalidFontId || g_activeFontRenderer->fontNormal != eatsbits::text::kInvalidFontId)) {
        eatsbits::text::FontId fid = (g_activeFontRenderer->fontMono != eatsbits::text::kInvalidFontId)
                                     ? g_activeFontRenderer->fontMono
                                     : g_activeFontRenderer->fontNormal;
        float fontSize = std::max(10.0f, scale * 15.5f);
        uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint8_t ca = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        uint32_t col = static_cast<uint32_t>(cr) |
                      (static_cast<uint32_t>(cg) << 8) |
                      (static_cast<uint32_t>(cb) << 16) |
                      (static_cast<uint32_t>(ca) << 24);

        if (g_activeFontRenderer->textShaper.shapeText(g_activeFontRenderer->fontMgr,
                                                       fid, fontSize, str,
                                                       g_activeFontRenderer->shapedRun)) {
            FT_Face face = g_activeFontRenderer->fontMgr.getFtFace(fid);
            float ascender = fontSize * 0.82f;
            if (face && face->size) {
                float ftAsc = static_cast<float>(face->size->metrics.ascender) / 64.0f;
                if (ftAsc > 0.0f) ascender = ftAsc;
            }
            float baselineY = y + ascender;
            renderShapedGlyphs(renderer, *g_activeFontRenderer,
                               fid, fontSize, x, baselineY, col);
            return;
        }
    }

    drawVectorString(renderer, str, x, y, scale, r, g, b, a);
}

void drawMonoString(std::string_view str, float x, float y, float scale, float r, float g, float b, float a) {
    BatchRenderer2D* ren = getEffectiveBatchRenderer();
    if (ren) {
        drawMonoString(*ren, str, x, y, scale, r, g, b, a);
    }
}
#endif

inline void drawRect(float x, float y, float w, float h, const Color& c) {
    drawRect(x, y, w, h, c.r, c.g, c.b, c.a);
}

inline void drawRectGradient(float x, float y, float w, float h, const Color& c0, const Color& c1) {
    drawRectGradient(x, y, w, h, c0.r, c0.g, c0.b, c1.r, c1.g, c1.b, c0.a);
}

inline void drawRectOutline(float x, float y, float w, float h, const Color& c, float lineWidth = 1.0f) {
    drawRectOutline(x, y, w, h, c.r, c.g, c.b, c.a, lineWidth);
}

inline void drawRoundedRect(float x, float y, float w, float h, float radius, const Color& c, int cornerSegments = 8) {
    drawRoundedRect(x, y, w, h, radius, c.r, c.g, c.b, c.a, cornerSegments);
}

inline void drawRoundedRectGradient(float x, float y, float w, float h, float radius, const Color& c0, const Color& c1, int cornerSegments = 8) {
    drawRoundedRectGradient(x, y, w, h, radius, c0.r, c0.g, c0.b, c1.r, c1.g, c1.b, c0.a, cornerSegments);
}

inline void drawRoundedRectOutline(float x, float y, float w, float h, float radius, const Color& c, float lineWidth = 1.0f, int cornerSegments = 8) {
    drawRoundedRectOutline(x, y, w, h, radius, c.r, c.g, c.b, c.a, lineWidth, cornerSegments);
}

inline void drawCircle(float cx, float cy, float radius, const Color& c, int segments = 36) {
    drawCircle(cx, cy, radius, c.r, c.g, c.b, c.a, segments);
}

inline void drawCircleOutline(float cx, float cy, float radius, const Color& c, float lineWidth = 1.5f, int segments = 36) {
    drawCircleOutline(cx, cy, radius, c.r, c.g, c.b, c.a, lineWidth, segments);
}

inline void drawLine(float x0, float y0, float x1, float y1, const Color& c, float lineWidth = 1.5f) {
    drawLine(x0, y0, x1, y1, c.r, c.g, c.b, c.a, lineWidth);
}

inline float drawSyntaxString(const std::string& str, float x, float y, float scale, const Color& c) {
    return drawSyntaxString(str, x, y, scale, c.r, c.g, c.b, c.a);
}

inline void drawRect(const Rect& rect, const Color& c) {
    drawRect(rect.x, rect.y, rect.w, rect.h, c);
}

inline void drawRect(const Rect& rect, float r, float g, float b, float a = 1.0f) {
    drawRect(rect.x, rect.y, rect.w, rect.h, r, g, b, a);
}

inline void drawRectGradient(const Rect& rect, const Color& c0, const Color& c1) {
    drawRectGradient(rect.x, rect.y, rect.w, rect.h, c0, c1);
}

inline void drawRectOutline(const Rect& rect, const Color& c, float lineWidth = 1.0f) {
    drawRectOutline(rect.x, rect.y, rect.w, rect.h, c, lineWidth);
}

inline void drawRectOutline(const Rect& rect, float r, float g, float b, float a = 1.0f, float lineWidth = 1.0f) {
    drawRectOutline(rect.x, rect.y, rect.w, rect.h, r, g, b, a, lineWidth);
}

// =========================================================================
// SKEUOMORPHIC HARDWARE KNOB ENGINE (TB-303 DIODE LADDER POTENTIOMETER)
// =========================================================================

inline void renderTb303Knob(float cx, float cy, float kRad, float normVal,
                            bool isActive, bool isHovered,
                            const std::string& label, const std::string& readout,
                            const LightSource2D& light,
                            bool isSelector = false) {
    constexpr float minAngle = -2.35619449f; // -135 deg
    constexpr float maxAngle = 2.35619449f;  // +135 deg
    float currentAngle = minAngle + normVal * (maxAngle - minAngle);

    Point lDir = light.getLightDirection(cx, cy);
    float lightAngle = light.getLightAngle(cx, cy);

    // 1. Countersunk / Sunken Chassis Well (Recessed Beveled Collet)
    float wellOuterR = kRad * 1.52f;
    float wellInnerR = kRad * 1.34f;

    // Outer countersink bevel recess
    drawCircle(cx, cy, wellOuterR, 0.10f, 0.11f, 0.13f, 1.0f, 32);

    // Inner rim lighting (3D depth illusion):
    // Top inner lip facing light casts a dark shadow into well;
    // Bottom inner lip facing away catches bright specular reflection
    drawArc(cx, cy, wellInnerR, lightAngle - 1.57f, lightAngle + 1.57f,
            0.02f, 0.03f, 0.04f, 0.95f, 2.2f, 24);
    drawArc(cx, cy, wellInnerR, lightAngle + 1.57f, lightAngle + 4.71f,
            0.72f, 0.76f, 0.84f, 0.88f, 1.6f, 24);

    // Sunken well floor base
    drawCircle(cx, cy, wellInnerR - 1.0f, 0.18f, 0.19f, 0.22f, 1.0f, 32);

    // 2. Screen-Printed Dial Tick Marks (11 hash marks radiating outwards)
    constexpr int kTickCount = 11;
    const float tickStep = (maxAngle - minAngle) / static_cast<float>(kTickCount - 1);
    for (int t = 0; t < kTickCount; ++t) {
        float a = minAngle + static_cast<float>(t) * tickStep;
        float sa = std::sin(a);
        float ca = std::cos(a);

        bool isCenterTick = (t == (kTickCount / 2)); // 12-o'clock index tick
        float rStart = wellOuterR + 1.5f;
        float rEnd = rStart + (isCenterTick ? 7.5f : 4.8f);
        float tickW = isCenterTick ? 2.2f : 1.3f;

        // Ticks are screen-printed dark enamel
        drawLine(cx + sa * rStart, cy - ca * rStart,
                 cx + sa * rEnd, cy - ca * rEnd,
                 0.08f, 0.09f, 0.11f, 0.95f, tickW);
    }

    // 3. Contextual Drop Shadow onto the Well Floor
    drawCircleDropShadow(cx, cy, kRad * 1.08f, 9.5f, light, 0.62f);

    // 4. Knurled Fluted Rotor Grip (Teeth with N·L Shading)
    int numTeeth = isSelector ? 24 : 36;
    float rInnerTeeth = kRad * 0.70f;
    float rOuterTeeth = kRad * 1.08f;

    // Aluminum/Bakelite metallic palette
    drawKnurledRing(cx, cy, rInnerTeeth, rOuterTeeth, numTeeth, currentAngle, light,
                    0.72f, 0.74f, 0.78f,  // Base metal
                    1.0f, 1.0f, 1.0f,     // Specular glint
                    0.13f, 0.14f, 0.17f); // Deep fluted groove shadow

    // 5. Center Rotor Cap (with authentic 3D Lathe & Specular Radial Gradient)
    float capR = kRad * 0.75f;
    float offX = lDir.x * capR * 0.35f;
    float offY = lDir.y * capR * 0.35f;
    drawCircleRadial3StopGradient(cx, cy, capR,
                                  0.98f, 0.99f, 1.0f, 1.0f,   // Specular peak highlight
                                  0.78f, 0.80f, 0.85f, 1.0f,   // Satin metallic base
                                  0.45f, 0.47f, 0.54f, 1.0f,   // Outer bevel shadow
                                  offX, offY, 0.50f, 32);
    // Beveled rim edge of the top face
    drawCircleOutline(cx, cy, capR, 0.35f, 0.38f, 0.45f, 1.0f, 1.2f, 32);
    drawCircleOutline(cx, cy, capR * 0.55f, 0.88f, 0.90f, 0.95f, 0.35f, 0.8f, 24);

    // 6. Inlaid Indicator Needle Cutout (Molded groove extending across cap and knurls)
    float sa = std::sin(currentAngle);
    float ca = std::cos(currentAngle);
    float iStart = capR * 0.20f;
    float iEnd = rOuterTeeth - 0.5f;

    // Inlaid relief groove shadow
    drawLine(cx + sa * iStart + 0.6f, cy - ca * iStart + 0.6f,
             cx + sa * iEnd + 0.6f, cy - ca * iEnd + 0.6f,
             0.14f, 0.15f, 0.18f, 0.85f, 2.6f);

    // Indicator inlay line
    if (isActive) {
        // High-visibility glowing acid neon feedback when turning
        drawLine(cx + sa * iStart, cy - ca * iStart,
                 cx + sa * iEnd, cy - ca * iEnd,
                 1.0f, 0.88f, 0.12f, 1.0f, 2.6f);
    } else if (isHovered) {
        drawLine(cx + sa * iStart, cy - ca * iStart,
                 cx + sa * iEnd, cy - ca * iEnd,
                 0.30f, 0.95f, 1.0f, 1.0f, 2.2f);
    } else {
        // High-contrast clean white inlay
        drawLine(cx + sa * iStart, cy - ca * iStart,
                 cx + sa * iEnd, cy - ca * iEnd,
                 0.96f, 0.98f, 1.0f, 1.0f, 2.0f);
    }

    // 7. Label and Active Parameter Readout
    if (!label.empty()) {
        float labelScale = std::clamp(kRad * 0.038f, 0.44f, 0.65f);
        float labelW = label.size() * 5.5f * labelScale;
        drawVectorString(label, cx - labelW * 0.5f, cy + wellOuterR + 5.0f,
                         labelScale, 0.70f, 0.75f, 0.85f, 1.0f);
    }

    if (isActive || isHovered) {
        std::string text = readout.empty() ? (std::to_string(static_cast<int>(std::round(normVal * 100.0f))) + "%") : readout;
        float valScale = std::clamp(kRad * 0.036f, 0.42f, 0.60f);
        float valW = text.size() * 5.5f * valScale;
        drawVectorString(text, cx - valW * 0.5f, cy - wellOuterR - 10.0f,
                         valScale, isActive ? 1.0f : 0.4f,
                         isActive ? 0.90f : 1.0f,
                         isActive ? 0.20f : 1.0f, 1.0f);
    }
}

inline void renderVintageClavinetKnob(float cx, float cy, float kRad, float normVal,
                                      bool isActive, bool isHovered,
                                      const std::string& label, const std::string& readout,
                                      const LightSource2D& light) {
    constexpr float minAngle = -2.35619449f; // -135 deg
    constexpr float maxAngle = 2.35619449f;  // +135 deg
    float currentAngle = minAngle + normVal * (maxAngle - minAngle);

    // 1. Screen-printed dial numbers 0..10 and graduation tick lines
    constexpr int kTickCount = 11;
    const float tickStep = (maxAngle - minAngle) / static_cast<float>(kTickCount - 1);
    float dialR = kRad * 1.30f;
    float numR = kRad * 1.62f;

    for (int t = 0; t < kTickCount; ++t) {
        float a = minAngle + static_cast<float>(t) * tickStep;
        float sa = std::sin(a);
        float ca = std::cos(a);

        // Tick mark
        float rStart = dialR;
        float rEnd = dialR + (t % 5 == 0 ? 5.0f : 3.5f);
        drawLine(cx + sa * rStart, cy - ca * rStart,
                 cx + sa * rEnd, cy - ca * rEnd,
                 Color(0.85f, 0.78f, 0.68f, 0.90f), 1.2f);

        // Digit string "0".."10"
        std::string numStr = std::to_string(t);
        float digitW = numStr.size() * 3.5f;
        float nx = cx + sa * numR - digitW * 0.5f;
        float ny = cy - ca * numR - 3.5f;
        drawVectorString(numStr, nx, ny, 0.42f, Color(0.88f, 0.82f, 0.72f, 0.95f));
    }

    // 2. Drop shadow for knob body
    drawCircleDropShadow(cx, cy, kRad * 1.05f, 8.0f, light, 0.55f);

    // 3. Knurled fluted skirt
    int numTeeth = 32;
    drawKnurledRing(cx, cy, kRad * 0.72f, kRad * 1.04f, numTeeth, currentAngle, light,
                    0.18f, 0.19f, 0.22f,   // Dark charcoal base
                    0.38f, 0.40f, 0.45f,   // Specular glint
                    0.07f, 0.07f, 0.09f);  // Fluted groove

    // 4. Center Cap
    float capR = kRad * 0.76f;
    Point lDir = light.getLightDirection(cx, cy);
    float offX = lDir.x * capR * 0.30f;
    float offY = lDir.y * capR * 0.30f;
    drawCircleRadial3StopGradient(cx, cy, capR,
                                  0.28f, 0.30f, 0.34f, 1.0f, // Top specular
                                  0.14f, 0.15f, 0.17f, 1.0f, // Body charcoal
                                  0.08f, 0.08f, 0.09f, 1.0f, // Rim shadow
                                  offX, offY, 0.50f, 32);
    drawCircleOutline(cx, cy, capR, 0.40f, 0.42f, 0.48f, 1.0f, 1.2f, 32);

    // 5. Crisp white pointer indicator
    float sa = std::sin(currentAngle);
    float ca = std::cos(currentAngle);
    float iStart = capR * 0.15f;
    float iEnd = capR * 0.95f;
    if (isActive) {
        drawLine(cx + sa * iStart, cy - ca * iStart,
                 cx + sa * iEnd, cy - ca * iEnd,
                 Color(1.0f, 0.65f, 0.10f), 2.4f);
    } else if (isHovered) {
        drawLine(cx + sa * iStart, cy - ca * iStart,
                 cx + sa * iEnd, cy - ca * iEnd,
                 Color(0.20f, 0.90f, 1.0f), 2.2f);
    } else {
        drawLine(cx + sa * iStart, cy - ca * iStart,
                 cx + sa * iEnd, cy - ca * iEnd,
                 Color(0.98f, 0.98f, 0.98f), 2.0f);
    }

    // 6. Label below knob in bold warm tan/cream
    if (!label.empty()) {
        float labelScale = 0.58f;
        float labelW = label.size() * 5.6f * labelScale;
        drawVectorString(label, cx - labelW * 0.5f, cy + kRad * 1.55f + 4.0f,
                         labelScale, Color(0.92f, 0.86f, 0.78f));
    }

    // 7. Value readout badge below label
    std::string text = readout;
    if (text.empty()) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", normVal * 10.0f);
        text = buf;
    }
    float badgeW = std::max(text.size() * 7.5f + 14.0f, 32.0f);
    float badgeH = 16.0f;
    float badgeX = cx - badgeW * 0.5f;
    float badgeY = cy + kRad * 1.55f + 20.0f;
    drawRoundedRect(badgeX, badgeY, badgeW, badgeH, 3.0f, Color(0.08f, 0.08f, 0.10f));
    drawRoundedRectOutline(badgeX, badgeY, badgeW, badgeH, 3.0f, Color(0.28f, 0.26f, 0.22f), 1.0f);
    float valScale = 0.50f;
    float valW = text.size() * 5.4f * valScale;
    drawVectorString(text, cx - valW * 0.5f, badgeY + 4.0f, valScale,
                     isActive ? Color(1.0f, 0.65f, 0.10f) : Color(0.85f, 0.82f, 0.76f));
}

inline void renderClavinetRockerSwitch(float cx, float cy, float normVal,
                                       bool isActive, bool isHovered,
                                       const std::string& label,
                                       const LightSource2D& light,
                                       float scale = 1.0f) {
    (void)light; (void)isActive; (void)isHovered;
    bool isOn = (normVal > 0.5f);

    // Label on top
    if (!label.empty()) {
        float labelScale = 0.55f * scale + 0.05f;
        float labelW = label.size() * 5.6f * labelScale;
        Color lblColor = isOn ? Color(1.0f, 0.62f, 0.10f) : Color(0.48f, 0.52f, 0.58f);
        drawVectorString(label, cx - labelW * 0.5f, cy - 28.0f * scale, labelScale, lblColor);
    }

    // Vertical pill slot
    float slotW = 20.0f * scale;
    float slotH = 42.0f * scale;
    float r = slotW * 0.5f;
    float slotX = cx - r;
    float slotY = cy - slotH * 0.5f + 4.0f * scale;

    // Recessed dark well
    drawRoundedRect(slotX, slotY, slotW, slotH, r, Color(0.06f, 0.05f, 0.04f));
    drawRoundedRectOutline(slotX, slotY, slotW, slotH, r, Color(0.35f, 0.24f, 0.14f), 1.2f);

    if (isOn) {
        // Orange rocker paddle illumination when switch is engaged
        drawRoundedRect(slotX + 1.5f, slotY + 2.0f, slotW - 3.0f, slotH - 4.0f, r * 0.8f, Color(0.92f, 0.44f, 0.05f));
    }

    // Circular ivory/bone rocker thumb button with tactile dimples
    float btnY = isOn ? (slotY + slotH - r - 2.0f) : (slotY + r + 2.0f);
    float btnR = r * 0.86f;

    // Button drop shadow
    drawCircle(cx, btnY + 1.5f, btnR, 0.02f, 0.02f, 0.02f, 0.5f, 24);

    // Ivory/bone spherical gradient
    drawCircleRadial3StopGradient(cx, btnY, btnR,
                                  0.96f, 0.94f, 0.88f, 1.0f, // Top specular
                                  0.86f, 0.83f, 0.76f, 1.0f, // Bone body
                                  0.65f, 0.62f, 0.54f, 1.0f, // Underside bevel
                                  0.0f, -btnR * 0.35f, 0.50f, 24);
    drawCircleOutline(cx, btnY, btnR, 0.45f, 0.40f, 0.32f, 1.0f, 1.0f, 24);

    // Two small vertical tactile dots (dimples)
    drawCircle(cx, btnY - 2.5f * scale, 1.0f * scale, 0.35f, 0.30f, 0.24f, 0.9f);
    drawCircle(cx, btnY + 2.5f * scale, 1.0f * scale, 0.35f, 0.30f, 0.24f, 0.9f);
}

inline void renderClavinetSlider(float x, float y, float width, float height,
                                 float normVal, bool isActive, bool isHovered,
                                 const std::string& label, const std::string& readout,
                                 const LightSource2D& light, float scale = 1.0f) {
    (void)height; (void)light; (void)isActive; (void)isHovered;
    float trkX = x + 24.0f * scale;
    float trkY = y + 26.0f * scale;
    float trkW = std::max(40.0f, width - 48.0f * scale);
    float trkH = 7.0f * scale;

    // Top-left label: "STRING SUSTAIN & YARN DAMPING"
    if (!label.empty()) {
        float labelScale = 0.56f * scale + 0.05f;
        drawVectorString(label, trkX, trkY - 16.0f * scale, labelScale, Color(0.90f, 0.85f, 0.78f));
    }

    // Top-right readout: "1.0"
    std::string valStr = readout;
    if (valStr.empty()) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", normVal * 10.0f);
        valStr = buf;
    }
    float valScale = 0.58f * scale + 0.05f;
    float valW = valStr.size() * 5.6f * valScale;
    drawVectorString(valStr, trkX + trkW - valW, trkY - 16.0f * scale, valScale, Color(0.96f, 0.62f, 0.10f));

    // Horizontal illuminated track groove
    drawRoundedRect(trkX, trkY - trkH * 0.5f, trkW, trkH, trkH * 0.5f, Color(0.10f, 0.07f, 0.04f));
    // Warm golden amber active rail
    float fillW = std::clamp(normVal, 0.0f, 1.0f) * trkW;
    if (fillW > 2.0f) {
        drawRoundedRect(trkX, trkY - trkH * 0.5f + 1.0f, fillW, trkH - 2.0f, (trkH - 2.0f) * 0.5f, Color(0.88f, 0.66f, 0.22f));
    }
    drawRoundedRectOutline(trkX, trkY - trkH * 0.5f, trkW, trkH, trkH * 0.5f, Color(0.48f, 0.34f, 0.16f), 1.2f);

    // Circular metallic slider thumb button
    float thumbX = trkX + normVal * trkW;
    float thumbR = 9.0f * scale;
    drawCircle(thumbX, trkY + 1.5f, thumbR, 0.04f, 0.03f, 0.02f, 0.55f, 24);

    drawCircleRadial3StopGradient(thumbX, trkY, thumbR,
                                  0.98f, 0.99f, 1.0f, 1.0f,   // Specular rim
                                  0.82f, 0.84f, 0.88f, 1.0f,   // Satin metallic
                                  0.45f, 0.48f, 0.54f, 1.0f,   // Rim shadow
                                  0.0f, -thumbR * 0.35f, 0.50f, 24);
    drawCircleOutline(thumbX, trkY, thumbR, 0.32f, 0.34f, 0.40f, 1.0f, 1.0f, 24);

    // Recessed center dimple
    drawCircle(thumbX, trkY, thumbR * 0.45f, 0.48f, 0.50f, 0.56f, 1.0f, 16);
    drawCircleOutline(thumbX, trkY, thumbR * 0.45f, 0.25f, 0.27f, 0.32f, 1.0f, 0.8f, 16);
    drawCircle(thumbX - 1.0f, trkY - 1.0f, 1.2f * scale, 0.98f, 0.98f, 1.0f, 0.85f);
}

// =========================================================================
// HIGH-PERFORMANCE PROCEDURAL VECTOR UI ICONS (0 KB Bundle Cost)
// =========================================================================

inline void drawIconPlay(float cx, float cy, float size, const Color& fill, const Color& highlight = Color(0.0f, 0.0f, 0.0f, 0.0f)) {
    float halfH = size * 0.55f;
    float halfW = size * 0.50f;
    float x0 = cx - halfW * 0.7f;
    float y0 = cy - halfH;
    float x1 = cx + halfW;
    float y1 = cy;
    float x2 = cx - halfW * 0.7f;
    float y2 = cy + halfH;

    if (highlight.a > 0.0f) {
        drawTriangle(x0 + 0.6f, y0 + 0.8f, x1 + 0.6f, y1 + 0.8f, x2 + 0.6f, y2 + 0.8f, highlight.r, highlight.g, highlight.b, highlight.a);
    }
    drawTriangle(x0, y0, x1, y1, x2, y2, fill.r, fill.g, fill.b, fill.a);
}

inline void drawIconScrewClose(float cx, float cy, float radius, bool hovered, const Color& highlightColor = Color(0.98f, 0.32f, 0.28f)) {
    // 1. Recessed outer countersink well shadow
    drawCircle(cx, cy, radius + 1.2f, Color(0.04f, 0.03f, 0.04f, 0.95f), 16);
    drawCircle(cx, cy, radius + 0.4f, Color(0.08f, 0.08f, 0.09f, 0.85f), 16);

    if (hovered) {
        // Outer aura and halo ring in active theme highlight color
        drawCircle(cx, cy, radius + 2.2f, Color(highlightColor.r, highlightColor.g, highlightColor.b, 0.22f), 16);
        drawCircleOutline(cx, cy, radius + 1.5f, highlightColor.withAlpha(0.70f), 1.2f);
    }

    // 2. Metallic screw head body with highlight tint on hover
    Color screwBody = hovered
        ? Color::lerp(Color(0.28f, 0.30f, 0.36f), highlightColor, 0.22f)
        : Color(0.18f, 0.20f, 0.23f);
    drawCircle(cx, cy, radius, screwBody, 16);

    // 3. Chamfer highlight on top-left edge
    drawCircle(cx - 0.5f, cy - 0.5f, radius * 0.85f, Color(1.0f, 1.0f, 1.0f, hovered ? 0.35f : 0.15f), 16);
    drawCircle(cx + 0.2f, cy + 0.2f, radius * 0.85f, screwBody, 16);

    // 4. Inset Phillips cross slot ("X")
    float arm = radius * 0.55f;
    Color slotColor = hovered ? highlightColor : Color(0.06f, 0.06f, 0.08f);
    float strokeW = std::max(1.3f, radius * (hovered ? 0.32f : 0.28f));

    drawLine(cx - arm, cy - arm, cx + arm, cy + arm, slotColor, strokeW);
    drawLine(cx - arm, cy + arm, cx + arm, cy - arm, slotColor, strokeW);

    if (hovered) {
        drawCircle(cx, cy, std::max(1.5f, radius * 0.25f), highlightColor, 8);
    }
}

inline void drawIconArranger(float x, float y, float w, float h, const Color& c) {
    float laneH = h * 0.26f;
    float gap = h * 0.10f;
    drawRect(x, y, w * 0.65f, laneH, c);
    drawRect(x + w * 0.30f, y + laneH + gap, w * 0.55f, laneH, c);
    drawRect(x + w * 0.10f, y + (laneH + gap) * 2.0f, w * 0.45f, laneH, c);
}

inline void drawIconEdit(float x, float y, float size, const Color& c) {
    float s = size;

    // 1. Stylus Silhouette (45-deg chisel stylus pointing down to baseline)
    float p0x = x + 0.93f * s, p0y = y + 0.27f * s; // top right
    float p1x = x + 0.74f * s, p1y = y + 0.08f * s; // top left (cap)
    float p2x = x + 0.30f * s, p2y = y + 0.48f * s; // shaft left
    float p3x = x + 0.30f * s, p3y = y + 0.70f * s; // chisel tip bottom-left
    float p4x = x + 0.52f * s, p4y = y + 0.70f * s; // chisel tip bottom-right

    // Convex decomposition of 5-gon stylus
    drawTriangle(p0x, p0y, p1x, p1y, p2x, p2y, c.r, c.g, c.b, c.a);
    drawTriangle(p0x, p0y, p2x, p2y, p4x, p4y, c.r, c.g, c.b, c.a);
    drawTriangle(p2x, p2y, p3x, p3y, p4x, p4y, c.r, c.g, c.b, c.a);

    // 2. Diamond cutout near cap
    if (s >= 8.0f) {
        float cx = x + 0.75f * s;
        float cy = y + 0.25f * s;
        float d = std::max(1.0f, 0.065f * s);
        Color bg(0.08f, 0.09f, 0.11f, c.a);
        drawTriangle(cx, cy - d, cx + d, cy, cx, cy + d, bg.r, bg.g, bg.b, bg.a);
        drawTriangle(cx, cy - d, cx - d, cy, cx, cy + d, bg.r, bg.g, bg.b, bg.a);
    }

    // 3. Baseline underline bar (solid filled left section + outlined right box)
    float barH = std::max(1.5f, 0.14f * s);
    float barY = y + 0.78f * s;
    float barX = x + 0.06f * s;
    float barW = 0.88f * s;

    // Solid filled left portion (~68%)
    float filledW = barW * 0.68f;
    drawRect(barX, barY, filledW, barH, c);

    // Outlined right box (~32%)
    float emptyX = barX + filledW;
    float emptyW = barW - filledW;
    float strokeW = std::max(1.0f, 0.06f * s);
    drawLine(emptyX, barY + strokeW * 0.5f, emptyX + emptyW, barY + strokeW * 0.5f, c, strokeW);
    drawLine(emptyX, barY + barH - strokeW * 0.5f, emptyX + emptyW, barY + barH - strokeW * 0.5f, c, strokeW);
    drawLine(emptyX + emptyW - strokeW * 0.5f, barY, emptyX + emptyW - strokeW * 0.5f, barY + barH, c, strokeW);
}

inline void drawIconTrack(float x, float y, float w, float h, const Color& c) {
    float bars[5] = {0.35f, 0.75f, 1.0f, 0.60f, 0.40f};
    float bw = w / 6.0f;
    for (int i = 0; i < 5; ++i) {
        float bh = h * bars[i];
        float by = y + (h - bh) * 0.5f;
        drawRect(x + i * (bw + 1.0f), by, bw, bh, c);
    }
}

inline void drawIconMixer(float x, float y, float w, float h, const Color& c) {
    float step = w / 3.0f;
    float faderH = h * 0.28f;
    float faderW = step * 0.85f;
    for (int i = 0; i < 3; ++i) {
        float fx = x + i * step + step * 0.5f;
        drawLine(fx, y, fx, y + h, Color(c.r, c.g, c.b, 0.45f), 1.2f);
        float knobY = y + (i == 0 ? h * 0.20f : (i == 1 ? h * 0.65f : h * 0.40f));
        drawRect(fx - faderW * 0.5f, knobY - faderH * 0.5f, faderW, faderH, c);
    }
}

inline void drawIconDesign(float x, float y, float w, float h, const Color& c) {
    float midY = y + h * 0.5f;
    drawLine(x + w * 0.35f, y + 1.0f, x + 1.0f, midY, c, 1.5f);
    drawLine(x + 1.0f, midY, x + w * 0.35f, y + h - 1.0f, c, 1.5f);
    drawLine(x + w * 0.65f, y + 1.0f, x + w - 1.0f, midY, c, 1.5f);
    drawLine(x + w - 1.0f, midY, x + w * 0.65f, y + h - 1.0f, c, 1.5f);
}

inline void drawIconPianoRoll(float x, float y, float w, float h, const Color& c) {
    drawRect(x, y, w, h, c);
    float kw = w / 3.0f;
    drawLine(x + kw, y, x + kw, y + h, Color(0.1f, 0.1f, 0.12f, 0.8f), 1.0f);
    drawLine(x + kw * 2.0f, y, x + kw * 2.0f, y + h, Color(0.1f, 0.1f, 0.12f, 0.8f), 1.0f);
    float bkW = kw * 0.55f;
    float bkH = h * 0.55f;
    drawRect(x + kw - bkW * 0.5f, y, bkW, bkH, Color(0.08f, 0.08f, 0.10f, 1.0f));
    drawRect(x + kw * 2.0f - bkW * 0.5f, y, bkW, bkH, Color(0.08f, 0.08f, 0.10f, 1.0f));
}

inline void drawIconTracker(float x, float y, float w, float h, const Color& c) {
    drawRectOutline(x, y, w, h, c, 1.0f);
    drawLine(x + w * 0.35f, y, x + w * 0.35f, y + h, Color(c.r, c.g, c.b, 0.5f), 1.0f);
    drawLine(x + w * 0.70f, y, x + w * 0.70f, y + h, Color(c.r, c.g, c.b, 0.5f), 1.0f);
    for (float ry = y + 3.0f; ry < y + h - 2.0f; ry += 3.5f) {
        drawLine(x + 2.0f, ry, x + w * 0.30f, ry, c, 1.0f);
        drawLine(x + w * 0.40f, ry, x + w * 0.65f, ry, c, 1.0f);
    }
}

inline void drawIconScore(float x, float y, float w, float h, const Color& c) {
    drawCircle(x + w * 0.35f, y + h - 3.5f, 2.5f, c);
    drawLine(x + w * 0.35f + 2.0f, y + h - 3.5f, x + w * 0.35f + 2.0f, y + 2.0f, c, 1.2f);
    drawLine(x + w * 0.35f + 2.0f, y + 2.0f, x + w * 0.35f + 6.0f, y + 5.0f, c, 1.2f);
}

GuiWindow::GuiWindow(uint32_t width, uint32_t height, const std::string& title)
    : width_(width), height_(height),
      windowWidth_(static_cast<int>(width)), windowHeight_(static_cast<int>(height)),
      fbWidth_(static_cast<int>(width)), fbHeight_(static_cast<int>(height)),
      title_(title),
      canvas_(static_cast<float>(width), static_cast<float>(height)),
      dawnBridge_(width, height) {
    setStatusMessage("Eatsbits Modular Workstation Active [Google Dawn]");
    loadPresets();
    initEatscriptIde();
    initNoteScriptEditor();
    initArrangerTracks();
    audio::ClapPluginLoader::instance().scanDirectoriesAsync();
    loadSettings();
}

GuiWindow::~GuiWindow() {
#if defined(__EMSCRIPTEN__)
    if (g_activeGuiWindowForWeb == this) {
        g_activeGuiWindowForWeb = nullptr;
    }
#endif
    close();
}

void GuiWindow::setStatusMessage(const std::string& msg) noexcept {
    lastStatusMessage_ = msg;
    statusToastText_ = msg;
    statusToastTimer_ = 3.5f;
}

void GuiWindow::setUiScale(float scale) noexcept {
    uiScale_ = std::clamp(scale, 0.50f, 2.50f);
    updateLogicalDimensions();
    setStatusMessage("UI Scale: " + std::to_string(static_cast<int>(std::round(uiScale_ * 100.0f))) + "%");
}

void GuiWindow::zoomIn() noexcept {
    setUiScale(uiScale_ + 0.10f);
}

void GuiWindow::zoomOut() noexcept {
    setUiScale(uiScale_ - 0.10f);
}

void GuiWindow::resetZoom() noexcept {
    setUiScale(1.0f);
}

void GuiWindow::setAntiAliasingMode(int mode) noexcept {
    antiAliasingMode_ = std::clamp(mode, 0, 2);
    if (batchRenderer_) {
        batchRenderer_->setAntiAliasingMode(antiAliasingMode_);
    }
    const char* names[3] = {"Off (1x Point Sampling)", "2x Fast RGSS", "4x High Quality RGSS"};
    setStatusMessage(std::string("Anti-Aliasing: ") + names[antiAliasingMode_]);
}

void GuiWindow::setHiDpiEnabled(bool enable) noexcept {
    if (hiDpiEnabled_ == enable) return;
    hiDpiEnabled_ = enable;
    updateLogicalDimensions();
    if (window_) {
        uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
        uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));
        dawnBridge_.resize(physW, physH);
        if (batchRenderer_) {
            batchRenderer_->resize(physW, physH);
#if defined(__EMSCRIPTEN__)
            if (dawnBridge_.isNativeActive()) {
                batchRenderer_->setCustomRenderTargetView(dawnBridge_.getDawTextureView());
            }
#endif
        }
    }
    setStatusMessage(hiDpiEnabled_ ? "HiDPI Canvas: Native 1:1 Enabled" : "HiDPI Canvas: Standard 1x (Performance Mode)");
}

void GuiWindow::toggleHiDpi() noexcept {
    setHiDpiEnabled(!hiDpiEnabled_);
}

float GuiWindow::windowToLogicalX(double winX) const noexcept {
    if (uiScale_ <= 0.0001f) return static_cast<float>(winX);
    return static_cast<float>(winX) / uiScale_;
}

float GuiWindow::windowToLogicalY(double winY) const noexcept {
    if (uiScale_ <= 0.0001f) return static_cast<float>(winY);
    return static_cast<float>(winY) / uiScale_;
}

void GuiWindow::remapCrtMouseCoords(float inX, float inY, float& outX, float& outY) const noexcept {
    if (!crtShaderEnabled_ || width_ <= 0 || height_ <= 0) {
        outX = inX;
        outY = inY;
        return;
    }

    const auto& cfg = dawnBridge_.getMaterialConfig();
    const float totalW = static_cast<float>(width_);
    const float totalH = static_cast<float>(height_);
    const float topBarH = static_cast<float>(cfg.topBarHeightPx);
    const float bottomBarH = static_cast<float>(cfg.bottomBarHeightPx);

    const float topCut = topBarH / totalH;
    const float bottomCut = 1.0f - (bottomBarH / totalH);

    // Apply rumble shake offset matching shader globalUV += u.rumbleOffset
    float u = (inX / totalW) + dawnBridge_.getRumbleOffsetX();
    float v = (inY / totalH) + dawnBridge_.getRumbleOffsetY();

    // Top transport bar and bottom navigation chin have zero curvature and 1:1 hit testing
    if (v < topCut || v > bottomCut) {
        outX = std::clamp(u, 0.0f, 1.0f) * totalW;
        outY = std::clamp(v, 0.0f, 1.0f) * totalH;
        return;
    }

    const float span = std::max(0.01f, bottomCut - topCut);
    const float suvX = u;
    const float suvY = (v - topCut) / span;

    // Chassis frame dimensions matching crt_screen.wgsl:
    // let screenRes = vec2<f32>(u.resolution.x, u.resolution.y * span);
    // let frameWidthX_px = 22.0;
    // let frameHeightY_px = 20.0;
    const float fboW = static_cast<float>(dawnBridge_.getFboWidth() > 0 ? dawnBridge_.getFboWidth() : width_);
    const float fboH = static_cast<float>(dawnBridge_.getFboHeight() > 0 ? dawnBridge_.getFboHeight() : height_);
    const float screenResX = fboW;
    const float screenResY = fboH * span;

    constexpr float frameWidthX_px = 22.0f;
    constexpr float frameHeightY_px = 20.0f;

    const float frameFracX = frameWidthX_px / std::max(1.0f, screenResX);
    const float frameFracY = frameHeightY_px / std::max(1.0f, screenResY);

    // Map screen coordinate inside the phosphor tube aperture:
    // tubeUV = (suv - frameFrac) / (1.0 - 2.0 * frameFrac)
    const float denomX = std::max(0.001f, 1.0f - 2.0f * frameFracX);
    const float denomY = std::max(0.001f, 1.0f - 2.0f * frameFracY);
    float tubeUVX = std::clamp((suvX - frameFracX) / denomX, 0.0f, 1.0f);
    float tubeUVY = std::clamp((suvY - frameFracY) / denomY, 0.0f, 1.0f);

    // Apply exact CRT bulb curvature polynomial matching WGSL:
    // curvedTube += dCenter * pow(dist, 2.6) * (u.curvature * 0.08)
    float curvedTubeX = tubeUVX;
    float curvedTubeY = tubeUVY;
    if (cfg.curvature > 0.001f) {
        const float dCenterX = curvedTubeX - 0.5f;
        const float dCenterY = curvedTubeY - 0.5f;
        const float dist = std::sqrt(dCenterX * dCenterX + dCenterY * dCenterY);
        const float factor = std::pow(dist, 2.6f) * (cfg.curvature * 0.08f);
        curvedTubeX += dCenterX * factor;
        curvedTubeY += dCenterY * factor;
    }

    // Horizontal sync scan wave modulation matching shader
    float hWave = 0.0f;
    const float hWaveStrength = dawnBridge_.getHWaveStrength();
    if (hWaveStrength > 0.00001f && cfg.hsyncDistortion > 0.001f) {
        hWave = std::sin(suvY * 10.0f + dawnBridge_.getLampTime() * 5.0f) * (hWaveStrength * cfg.hsyncDistortion);
    }

    const float texUVX = std::clamp(curvedTubeX + hWave, 0.0f, 1.0f);
    const float texUVY = topCut + std::clamp(curvedTubeY, 0.0f, 1.0f) * span;

    outX = texUVX * totalW;
    outY = texUVY * totalH;
}

void GuiWindow::onWindowResize(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return;
    windowWidth_ = width;
    windowHeight_ = height;
    updateLogicalDimensions();
    uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
    uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));
    dawnBridge_.resize(physW, physH);
    if (batchRenderer_) {
        batchRenderer_->resize(physW, physH);
#if defined(__EMSCRIPTEN__)
        if (dawnBridge_.isNativeActive()) {
            batchRenderer_->setCustomRenderTargetView(dawnBridge_.getDawTextureView());
        }
#endif
    }
}

void GuiWindow::onFramebufferResize(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return;
    fbWidth_ = width;
    fbHeight_ = height;
    updateLogicalDimensions();
    uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
    uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));
    dawnBridge_.resize(physW, physH);
    if (batchRenderer_) {
        batchRenderer_->resize(physW, physH);
#if defined(__EMSCRIPTEN__)
        if (dawnBridge_.isNativeActive()) {
            batchRenderer_->setCustomRenderTargetView(dawnBridge_.getDawTextureView());
        }
#endif
    }
    if (!isRendering_ && engine_) {
        renderFrame();
    }
}

void GuiWindow::updateLogicalDimensions() noexcept {
    if (uiScale_ <= 0.0001f) uiScale_ = 1.0f;

    float dpiScaleX = 1.0f;
#if EATS_HAS_GLFW
#if defined(__EMSCRIPTEN__)
    double dpr = EM_ASM_DOUBLE({ return window.devicePixelRatio || 1.0; });
    dpiScaleX = static_cast<float>(dpr);
    if (dpiScaleX <= 0.0f) dpiScaleX = 1.0f;
#else
    if (window_ && windowWidth_ > 0 && fbWidth_ > 0) {
        dpiScaleX = static_cast<float>(fbWidth_) / static_cast<float>(windowWidth_);
    }
#endif
#endif
    if (!hiDpiEnabled_) {
        dpiScale_ = 1.0f;
    } else {
        dpiScale_ = std::max(1.0f, dpiScaleX);
    }
    renderScale_ = dpiScale_ * uiScale_;

    float baseW = hiDpiEnabled_ ? static_cast<float>(fbWidth_) : static_cast<float>(windowWidth_);
    float baseH = hiDpiEnabled_ ? static_cast<float>(fbHeight_) : static_cast<float>(windowHeight_);
    width_ = static_cast<uint32_t>(std::max(1.0f, std::round(baseW / renderScale_)));
    height_ = static_cast<uint32_t>(std::max(1.0f, std::round(baseH / renderScale_)));
    canvas_.setCanvasSize(static_cast<float>(width_), static_cast<float>(height_));

    if (batchRenderer_) {
        batchRenderer_->setRenderScale(renderScale_, renderScale_);
    }
    dawnBridge_.setRenderScale(renderScale_);
    invalidateChromeCache();
}

void GuiWindow::toggleFullscreen() noexcept {
    setFullscreen(!isFullscreen_);
}

void GuiWindow::setFullscreen(bool enable) noexcept {
#if EATS_HAS_GLFW
    if (!window_) return;
    if (isFullscreen_ == enable) return;
    isFullscreen_ = enable;

#if defined(__EMSCRIPTEN__)
    if (enable) {
        EM_ASM({
            var el = document.getElementById('canvas-container') || document.documentElement;
            if (el.requestFullscreen) {
                el.requestFullscreen();
            } else if (el.webkitRequestFullscreen) {
                el.webkitRequestFullscreen();
            }
        });
    } else {
        EM_ASM({
            if (document.exitFullscreen) {
                document.exitFullscreen();
            } else if (document.webkitExitFullscreen) {
                document.webkitExitFullscreen();
            }
        });
    }
    setStatusMessage(isFullscreen_ ? "Display: FULLSCREEN (Esc to exit)" : "Display: WINDOWED");
    pollWebResize();
#elif defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(window_);
    if (hwnd) {
        if (enable) {
            // Save current window style & rect before going fullscreen
            LONG style = GetWindowLong(hwnd, GWL_STYLE);
            if (!hasSavedWindowState_) {
                savedWindowStyle_ = style;
                RECT rect{};
                if (GetWindowRect(hwnd, &rect)) {
                    savedWindowX_ = rect.left;
                    savedWindowY_ = rect.top;
                    savedWindowW_ = rect.right - rect.left;
                    savedWindowH_ = rect.bottom - rect.top;
                }
                hasSavedWindowState_ = true;
            }

            int posX = 0;
            int posY = 0;
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);

            HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            if (hMonitor) {
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(hMonitor, &mi)) {
                    posX = mi.rcMonitor.left;
                    posY = mi.rcMonitor.top;
                    screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
                    screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
                }
            }

            // Remove window borders & titlebar (borderless fullscreen)
            LONG newStyle = (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP | WS_VISIBLE;
            SetWindowLong(hwnd, GWL_STYLE, newStyle);

            // Position window to cover entire monitor
            SetWindowPos(
                hwnd,
                HWND_TOP,
                posX,
                posY,
                screenWidth,
                screenHeight,
                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
            );
        } else {
            // Restore regular windowed style
            LONG restoreStyle = (savedWindowStyle_ != 0) ? savedWindowStyle_ : (WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowLong(hwnd, GWL_STYLE, restoreStyle);

            SetWindowPos(
                hwnd,
                HWND_TOP,
                savedWindowX_,
                savedWindowY_,
                savedWindowW_,
                savedWindowH_,
                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
            );
            ShowWindow(hwnd, SW_RESTORE);
        }

        // Update GLFW cached sizes and redraw
        glfwGetWindowSize(window_, &windowWidth_, &windowHeight_);
        glfwGetFramebufferSize(window_, &fbWidth_, &fbHeight_);
        updateLogicalDimensions();
        uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
        uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));
        dawnBridge_.resize(physW, physH);
        if (batchRenderer_) {
            batchRenderer_->resize(physW, physH);
#if defined(__EMSCRIPTEN__)
            if (dawnBridge_.isNativeActive()) {
                batchRenderer_->setCustomRenderTargetView(dawnBridge_.getDawTextureView());
            }
#endif
        }
        renderFrame();
    }
#else
    if (enable) {
        glfwGetWindowPos(window_, &savedWindowX_, &savedWindowY_);
        glfwGetWindowSize(window_, &savedWindowW_, &savedWindowH_);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode) {
                glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            }
        }
    } else {
        glfwSetWindowMonitor(window_, nullptr, savedWindowX_, savedWindowY_, savedWindowW_, savedWindowH_, 0);
    }
#endif

    setStatusMessage(isFullscreen_ ? "Display: FULLSCREEN (F11 / Alt+Enter to exit)" : "Display: WINDOWED (F11 / Alt+Enter for fullscreen)");
#endif
}

bool GuiWindow::isFullscreenDeviceOpen() const noexcept {
    return fullscreenDeviceModal_.isOpen();
}

void GuiWindow::openFullscreenDevice(uint32_t trackIndex) {
    uint32_t tIdx = (trackIndex < arrangerTracks_.size()) ? trackIndex : selectedTrackIndex_;
    selectedTrackIndex_ = tIdx;
    syncTrackToPreset(tIdx);

    TrackPropertiesDrawerData data;
    data.trackIndex = tIdx;
    if (tIdx < arrangerTracks_.size()) {
        const auto& at = arrangerTracks_[tIdx];
        data.trackName = at.name;
        data.r = at.r;
        data.g = at.g;
        data.b = at.b;
        data.volume = at.volume;
        data.pan = at.pan;
        data.mute = at.mute;
        data.solo = at.solo;
        data.freeze = at.freeze;
        data.midiFxData = at.midiFx;
        data.audioFxData = at.audioFx;
        data.instrument = at.instrument;
        data.instrumentEngine = at.instrumentEngine;
        data.eatscriptCode = at.eatscriptCode;
        data.customGuiPanel = at.customGuiPanel;
        data.presetTitle = at.instrument.empty() ? at.name : at.instrument;
        data.presetSubtitle = at.name + " • " + (at.instrument.empty() ? "Track" : at.instrument);
        data.isDetached = FloatingWindowManager::instance().isDetached(tIdx);
    }
    if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
        const auto& trk = modularArrangerView_->getTracks()[tIdx];
        if (trk.customGuiPanel.has_value()) {
            data.customGuiPanel = trk.customGuiPanel;
        }
        if (!trk.eatscriptCode.empty()) {
            data.eatscriptCode = trk.eatscriptCode;
        }
        if (!trk.knobs.empty()) {
            data.knobs = trk.knobs;
        }
    }
    auto clapNode = engine_ ? engine_->getTrackClapNode(tIdx) : nullptr;
    if (clapNode) {
        data.isClapPlugin = true;
        data.hasNativeGui = clapNode->hasGui();
        data.isGuiOpen = clapNode->isGuiOpen();
        data.presetTitle = clapNode->getPluginName().empty() ? data.instrument : clapNode->getPluginName();
        data.presetSubtitle = data.trackName + " • CLAP Instrument";
        data.instrument = data.presetTitle;
        data.instrumentEngine = "clap:" + clapNode->getPluginId();

        data.knobs.clear();
        const auto& params = clapNode->getParameterList();
        size_t limit = std::min<size_t>(params.size(), 24);
        for (size_t i = 0; i < limit; ++i) {
            const auto& p = params[i];
            TrackPropertiesKnob k;
            k.name = p.name;
            k.label = p.name;
            double norm = 0.5;
            if (p.maxValue > p.minValue) {
                norm = (p.currentValue - p.minValue) / (p.maxValue - p.minValue);
            }
            k.value = static_cast<float>(std::clamp(norm, 0.0, 1.0));
            k.display = std::to_string(static_cast<int>(p.currentValue));
            data.knobs.push_back(k);
        }
    } else {
        if (data.customGuiPanel.has_value()) {
            if (!data.customGuiPanel->title.empty()) {
                data.presetTitle = data.customGuiPanel->title;
            }
            if (!data.customGuiPanel->subtitle.empty()) {
                data.presetSubtitle = data.customGuiPanel->subtitle;
            }
        } else {
            const auto* preset = getActivePreset();
            if (preset) {
                data.instrument = preset->metadata.name.empty() ? preset->guiRoot.title : preset->metadata.name;
                data.instrumentEngine = preset->metadata.engineId;
                data.presetTitle = preset->guiRoot.title;
                data.presetSubtitle = preset->guiRoot.subtitle;
                data.activePresetIdx = activePresetIndex_;
                data.totalPresets = presets_.size();

                data.knobs.clear();
                for (const auto& [name, param] : preset->params) {
                    TrackPropertiesKnob k;
                    k.name = name;
                    k.label = param.name.empty() ? name : param.name;
                    k.value = param.getNormalized();
                    k.display = param.getFormatted();
                    data.knobs.push_back(k);
                }
            }
        }
    }
    if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
        const auto& trk = modularArrangerView_->getTracks()[tIdx];
        data.audioFx = trk.audioFx;
        data.midiFx = trk.midiFx;
    } else if (modularMixerView_ && tIdx < modularMixerView_->getChannels().size()) {
        const auto& ch = modularMixerView_->getChannels()[tIdx];
        data.audioFx = ch.audioFx;
        data.midiFx = ch.midiFx;
    } else if (modularTrackInspectorView_ && tIdx < modularTrackInspectorView_->getTracks().size()) {
        const auto& insp = modularTrackInspectorView_->getTracks()[tIdx];
        data.audioFx = insp.audioFxList;
        data.midiFx = insp.midiFxList;
    }
    data.syncKnobsIfEmpty();

    DeviceTarget target;
    target.type = DeviceTargetType::Instrument;
    target.trackIndex = tIdx;
    target.fxIndex = -1;
    target.deviceName = data.instrument;
    lastFocusedDevice_ = target;

    fullscreenDeviceModal_.setIsMobile(width_ < 768);
    fullscreenDeviceModal_.open(target);
    fullscreenDeviceModal_.syncData(data);
    fullscreenDeviceModal_.setAudioScopeBuffer(scopeBuffer_, 128);
    fullscreenDeviceModal_.layout(static_cast<float>(width_), static_cast<float>(height_));

#if defined(_WIN32)
    if (clapNode && clapNode->hasGui()) {
        HWND mainHwnd = glfwGetWin32Window(window_);
        const auto& b = fullscreenDeviceModal_.getBodyBounds();
        bool ok = clapNode->openEmbeddedGui(mainHwnd,
            static_cast<int>(b.x), static_cast<int>(b.y),
            static_cast<int>(b.w), static_cast<int>(b.h));
        if (ok) {
            fullscreenDeviceModal_.setIsGuiEmbedded(true);
        } else {
            bool floatOk = clapNode->openGuiWindow();
            fullscreenDeviceModal_.setIsGuiEmbedded(false);
            if (!floatOk) {
                setStatusMessage("Custom GUI unavailable: loaded generic parameter controls for " + clapNode->getPluginName());
            }
        }
    }
#else
    if (clapNode && clapNode->hasGui()) {
        bool floatOk = clapNode->openGuiWindow();
        if (!floatOk) {
            setStatusMessage("Custom GUI unavailable: loaded generic parameter controls for " + clapNode->getPluginName());
        }
    }
#endif

    setStatusMessage("Device Full-Display: " + data.instrument + " (Esc to exit)");
}

void GuiWindow::openFullscreenFx(uint32_t trackIndex, int fxIndex) {
    uint32_t tIdx = (trackIndex < arrangerTracks_.size()) ? trackIndex : selectedTrackIndex_;
    selectedTrackIndex_ = tIdx;

    TrackPropertiesDrawerData data;
    data.trackIndex = tIdx;
    if (tIdx < arrangerTracks_.size()) {
        data.trackName = arrangerTracks_[tIdx].name;
        data.r = arrangerTracks_[tIdx].r;
        data.g = arrangerTracks_[tIdx].g;
        data.b = arrangerTracks_[tIdx].b;
        data.midiFxData = arrangerTracks_[tIdx].midiFx;
        data.audioFxData = arrangerTracks_[tIdx].audioFx;
    }
    data.activePresetIdx = activePresetIndex_;
    data.totalPresets = presets_.size();

    if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
        const auto& trk = modularArrangerView_->getTracks()[tIdx];
        data.audioFx = trk.audioFx;
        data.midiFx = trk.midiFx;
    } else if (modularMixerView_ && tIdx < modularMixerView_->getChannels().size()) {
        const auto& ch = modularMixerView_->getChannels()[tIdx];
        data.audioFx = ch.audioFx;
        data.midiFx = ch.midiFx;
    } else if (modularTrackInspectorView_ && tIdx < modularTrackInspectorView_->getTracks().size()) {
        const auto& insp = modularTrackInspectorView_->getTracks()[tIdx];
        data.audioFx = insp.audioFxList;
        data.midiFx = insp.midiFxList;
    }
    data.syncKnobsIfEmpty();

    std::string fxTitle = "Audio FX Insert Rack";
    if (fxIndex >= 0 && static_cast<size_t>(fxIndex) < data.audioFx.size()) {
        fxTitle = data.audioFx[fxIndex].name;
        data.knobs = data.audioFx[fxIndex].knobs;
        if (data.audioFx[fxIndex].customGuiPanel.has_value()) {
            data.customGuiPanel = data.audioFx[fxIndex].customGuiPanel;
        }
        if (!data.audioFx[fxIndex].eatscriptCode.empty()) {
            data.eatscriptCode = data.audioFx[fxIndex].eatscriptCode;
        }
        data.presetTitle = fxTitle;
        data.presetSubtitle = data.trackName + " • Audio FX";
    }

    std::shared_ptr<audio::ClapAudioNode> clapFxNode = nullptr;
    if (fxIndex >= 0 && engine_) {
        clapFxNode = engine_->getTrackAudioFxClapNode(tIdx, static_cast<size_t>(fxIndex));
        if (clapFxNode) {
            data.isClapPlugin = true;
            data.hasNativeGui = clapFxNode->hasGui();
            data.isGuiOpen = clapFxNode->isGuiOpen();
            data.presetTitle = clapFxNode->getPluginName().empty() ? fxTitle : clapFxNode->getPluginName();
            data.presetSubtitle = data.trackName + " • Audio FX";
            data.knobs.clear();
            const auto& params = clapFxNode->getParameterList();
            size_t limit = std::min<size_t>(params.size(), 24);
            for (size_t i = 0; i < limit; ++i) {
                const auto& p = params[i];
                TrackPropertiesKnob k;
                k.name = p.name;
                k.label = p.name;
                double norm = 0.5;
                if (p.maxValue > p.minValue) {
                    norm = (p.currentValue - p.minValue) / (p.maxValue - p.minValue);
                }
                k.value = static_cast<float>(std::clamp(norm, 0.0, 1.0));
                k.display = std::to_string(static_cast<int>(p.currentValue));
                data.knobs.push_back(k);
            }
        }
    }

    DeviceTarget target;
    target.type = DeviceTargetType::AudioFx;
    target.trackIndex = tIdx;
    target.fxIndex = fxIndex;
    target.deviceName = fxTitle;
    lastFocusedDevice_ = target;

    fullscreenDeviceModal_.setIsMobile(width_ < 768);
    fullscreenDeviceModal_.open(target);
    fullscreenDeviceModal_.syncData(data);
    fullscreenDeviceModal_.setAudioScopeBuffer(scopeBuffer_, 128);
    fullscreenDeviceModal_.layout(static_cast<float>(width_), static_cast<float>(height_));

#if defined(_WIN32)
    if (clapFxNode && clapFxNode->hasGui()) {
        HWND mainHwnd = glfwGetWin32Window(window_);
        const auto& b = fullscreenDeviceModal_.getBodyBounds();
        bool ok = clapFxNode->openEmbeddedGui(mainHwnd,
            static_cast<int>(b.x), static_cast<int>(b.y),
            static_cast<int>(b.w), static_cast<int>(b.h));
        if (ok) {
            fullscreenDeviceModal_.setIsGuiEmbedded(true);
        } else {
            bool floatOk = clapFxNode->openGuiWindow();
            fullscreenDeviceModal_.setIsGuiEmbedded(false);
            if (!floatOk) {
                setStatusMessage("Custom GUI unavailable: loaded generic parameter controls for " + clapFxNode->getPluginName());
            }
        }
    }
#else
    if (clapFxNode && clapFxNode->hasGui()) {
        bool floatOk = clapFxNode->openGuiWindow();
        if (!floatOk) {
            setStatusMessage("Custom GUI unavailable: loaded generic parameter controls for " + clapFxNode->getPluginName());
        }
    }
#endif

    setStatusMessage("Device Full-Display: " + fxTitle + " (Esc to exit)");
}

void GuiWindow::openFullscreenMidiFx(uint32_t trackIndex, int fxIndex) {
    uint32_t tIdx = (trackIndex < arrangerTracks_.size()) ? trackIndex : selectedTrackIndex_;
    selectedTrackIndex_ = tIdx;

    TrackPropertiesDrawerData data;
    data.trackIndex = tIdx;
    if (tIdx < arrangerTracks_.size()) {
        data.trackName = arrangerTracks_[tIdx].name;
        data.r = arrangerTracks_[tIdx].r;
        data.g = arrangerTracks_[tIdx].g;
        data.b = arrangerTracks_[tIdx].b;
        data.midiFxData = arrangerTracks_[tIdx].midiFx;
        data.audioFxData = arrangerTracks_[tIdx].audioFx;
    }
    data.activePresetIdx = activePresetIndex_;
    data.totalPresets = presets_.size();

    if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
        const auto& trk = modularArrangerView_->getTracks()[tIdx];
        data.audioFx = trk.audioFx;
        data.midiFx = trk.midiFx;
    } else if (modularMixerView_ && tIdx < modularMixerView_->getChannels().size()) {
        const auto& ch = modularMixerView_->getChannels()[tIdx];
        data.audioFx = ch.audioFx;
        data.midiFx = ch.midiFx;
    } else if (modularTrackInspectorView_ && tIdx < modularTrackInspectorView_->getTracks().size()) {
        const auto& insp = modularTrackInspectorView_->getTracks()[tIdx];
        data.audioFx = insp.audioFxList;
        data.midiFx = insp.midiFxList;
    }
    data.syncKnobsIfEmpty();

    std::string mfxTitle = "MIDI FX Processor";
    if (fxIndex >= 0 && static_cast<size_t>(fxIndex) < data.midiFx.size()) {
        mfxTitle = data.midiFx[fxIndex].name;
        data.knobs = data.midiFx[fxIndex].knobs;
        if (data.midiFx[fxIndex].customGuiPanel.has_value()) {
            data.customGuiPanel = data.midiFx[fxIndex].customGuiPanel;
        }
        if (!data.midiFx[fxIndex].eatscriptCode.empty()) {
            data.eatscriptCode = data.midiFx[fxIndex].eatscriptCode;
        }
        data.presetTitle = mfxTitle;
        data.presetSubtitle = data.trackName + " • MIDI FX";
    }

    DeviceTarget target;
    target.type = DeviceTargetType::MidiFx;
    target.trackIndex = tIdx;
    target.fxIndex = fxIndex;
    target.deviceName = mfxTitle;
    lastFocusedDevice_ = target;

    fullscreenDeviceModal_.setIsMobile(width_ < 768);
    fullscreenDeviceModal_.open(target);
    fullscreenDeviceModal_.syncData(data);
    fullscreenDeviceModal_.setAudioScopeBuffer(scopeBuffer_, 128);
    fullscreenDeviceModal_.layout(static_cast<float>(width_), static_cast<float>(height_));

    setStatusMessage("Device Full-Display: " + mfxTitle + " (Esc to exit)");
}

void GuiWindow::closeFullscreenDevice() noexcept {
    if (valueEditDialog_.isOpen()) {
        valueEditDialog_.close();
    }
    fullscreenDeviceModal_.close();
    setStatusMessage("Device Full-Display Closed");
}

void GuiWindow::toggleFullscreenDevice() noexcept {
    if (fullscreenDeviceModal_.isOpen()) {
        closeFullscreenDevice();
    } else {
        if (lastFocusedDevice_.type == DeviceTargetType::AudioFx && lastFocusedDevice_.fxIndex >= 0) {
            openFullscreenFx(lastFocusedDevice_.trackIndex, lastFocusedDevice_.fxIndex);
        } else if (lastFocusedDevice_.type == DeviceTargetType::MidiFx && lastFocusedDevice_.fxIndex >= 0) {
            openFullscreenMidiFx(lastFocusedDevice_.trackIndex, lastFocusedDevice_.fxIndex);
        } else {
            openFullscreenDevice(selectedTrackIndex_);
        }
    }
}

void GuiWindow::openDetachedInstrument(uint32_t trackIndex) {
#if !defined(__EMSCRIPTEN__)
    uint32_t tIdx = (trackIndex < arrangerTracks_.size()) ? trackIndex : selectedTrackIndex_;
    selectedTrackIndex_ = tIdx;

    auto clapNode = engine_ ? engine_->getTrackClapNode(tIdx) : nullptr;
    if (clapNode && clapNode->hasGui()) {
        if (fullscreenDeviceModal_.isOpen() && fullscreenDeviceModal_.getTarget().trackIndex == tIdx) {
            clapNode->destroyEmbeddedGui();
            fullscreenDeviceModal_.setIsGuiEmbedded(false);
            fullscreenDeviceModal_.close();
        } else if (clapNode->isEmbeddedGuiOpen()) {
            clapNode->destroyEmbeddedGui();
        }
        clapNode->onGuiClosed = [tIdx]() {
            FloatingWindowManager::instance().closeDetachedInstrument(tIdx);
        };
        bool ok = clapNode->openGuiWindow();
        if (ok) {
            FloatingWindowManager::instance().openDetachedInstrument(tIdx, clapNode->getPluginName(), GuiPanelDef{}, window_, true, clapNode->getGuiHwnd());
            setStatusMessage("Popped out plugin window: " + clapNode->getPluginName());
        } else {
            setStatusMessage("Could not open native window for " + clapNode->getPluginName());
        }
        return;
    }

    syncTrackToPreset(tIdx);
    std::string instName = (tIdx < arrangerTracks_.size() && !arrangerTracks_[tIdx].instrument.empty())
        ? arrangerTracks_[tIdx].instrument
        : "Instrument " + std::to_string(tIdx + 1);

    if (fullscreenDeviceModal_.isOpen() && fullscreenDeviceModal_.getTarget().trackIndex == tIdx) {
        fullscreenDeviceModal_.close();
    }

    TrackPropertiesDrawerData data;
    data.trackIndex = tIdx;
    if (tIdx < arrangerTracks_.size()) {
        const auto& at = arrangerTracks_[tIdx];
        data.trackName = at.name;
        data.r = at.r;
        data.g = at.g;
        data.b = at.b;
        data.volume = at.volume;
        data.pan = at.pan;
        data.mute = at.mute;
        data.solo = at.solo;
        data.freeze = at.freeze;
        data.midiFxData = at.midiFx;
        data.audioFxData = at.audioFx;
        data.instrument = at.instrument;
        data.instrumentEngine = at.instrumentEngine;
        data.presetTitle = at.instrument.empty() ? at.name : at.instrument;
        data.presetSubtitle = at.name + " • " + (at.instrument.empty() ? "Track" : at.instrument);
        data.eatscriptCode = at.eatscriptCode;
        data.customGuiPanel = at.customGuiPanel;
    }
    data.syncKnobsIfEmpty();

    DeviceTarget target;
    target.type = DeviceTargetType::Instrument;
    target.trackIndex = tIdx;
    target.fxIndex = -1;
    target.deviceName = instName;

    fullscreenDeviceModal_.open(target);
    fullscreenDeviceModal_.syncData(data);
    GuiPanelDef panel = fullscreenDeviceModal_.getGuiPanel();
    fullscreenDeviceModal_.close();

    FloatingWindowManager::instance().onParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
        if (idx != selectedTrackIndex_) {
            setSelectedTrackIndex(idx);
        }
        syncTrackToPreset(idx);
        dispatchTrackParam(idx, paramName, normVal);
        const auto* pDef = FloatingWindowManager::instance().getPanelDef(idx);
        if (pDef && idx < arrangerTracks_.size()) {
            arrangerTracks_[idx].customGuiPanel = *pDef;
        }
    };

    FloatingWindowManager::instance().onWindowClosed = [this](uint32_t closedIdx) {
        const auto* closedPanel = FloatingWindowManager::instance().getPanelDef(closedIdx);
        if (closedPanel && closedIdx < arrangerTracks_.size()) {
            arrangerTracks_[closedIdx].customGuiPanel = *closedPanel;
        }

        if (fullscreenDeviceModal_.isOpen() && fullscreenDeviceModal_.getTarget().trackIndex == closedIdx) {
            TrackPropertiesDrawerData d;
            d.trackIndex = closedIdx;
            if (closedIdx < arrangerTracks_.size()) {
                const auto& at = arrangerTracks_[closedIdx];
                d.trackName = at.name;
                d.r = at.r; d.g = at.g; d.b = at.b;
                d.volume = at.volume; d.pan = at.pan;
                d.mute = at.mute; d.solo = at.solo; d.freeze = at.freeze;
                d.midiFxData = at.midiFx; d.audioFxData = at.audioFx;
                d.instrument = at.instrument; d.instrumentEngine = at.instrumentEngine;
                d.presetTitle = at.instrument.empty() ? at.name : at.instrument;
                d.presetSubtitle = at.name + " • " + (at.instrument.empty() ? "Track" : at.instrument);
                d.eatscriptCode = at.eatscriptCode;
                d.customGuiPanel = at.customGuiPanel;
            }
            d.isDetached = false;
            fullscreenDeviceModal_.syncData(d);
        }
        if (modularTrackInspectorView_) {
            modularTrackInspectorView_->updateDrawerData();
        }
        if (modularArrangerView_ && closedIdx < modularArrangerView_->getTracks().size()) {
            modularArrangerView_->syncDrawerDataForTrack(closedIdx);
        }
        setStatusMessage("Re-attached instrument window to main workspace.");
    };

    bool spawned = FloatingWindowManager::instance().openDetachedInstrument(tIdx, instName, panel, window_, false);
    if (spawned) {
        setStatusMessage("Opened detached floating window: " + instName);
    }
#else
    (void)trackIndex;
    setStatusMessage("Detached windows are not supported in Web build.");
#endif
}

void GuiWindow::closeDetachedInstrument(uint32_t trackIndex) {
#if !defined(__EMSCRIPTEN__)
    FloatingWindowManager::instance().closeDetachedInstrument(trackIndex);
#else
    (void)trackIndex;
#endif
}

bool GuiWindow::isDetachedInstrumentOpen(uint32_t trackIndex) const noexcept {
#if !defined(__EMSCRIPTEN__)
    return FloatingWindowManager::instance().isDetached(trackIndex);
#else
    (void)trackIndex;
    return false;
#endif
}

void GuiWindow::initDefaultKnobValues() {
    const auto& modules = canvas_.getModules();
    for (const auto& m : modules) {
        for (size_t k = 0; k < m.knobNames.size(); ++k) {
            auto key = std::make_pair(m.id, static_cast<uint32_t>(k));
            if (knobValues_.find(key) != knobValues_.end()) {
                continue;
            }
            float defVal = 0.5f;
            if (m.type == "tb303") {
                if (k == 0) defVal = 0.45f;      // Cutoff
                else if (k == 1) defVal = 0.70f; // Res
                else if (k == 2) defVal = 0.65f; // EnvMod
                else if (k == 3) defVal = 0.50f; // Decay
                else if (k == 4) defVal = 0.75f; // Accent
                else if (k == 5) defVal = 0.0f;  // Wave
            } else if (m.type == "drum_kit") {
                if (k == 0) defVal = 0.85f;      // Master
                else if (k == 1) defVal = 0.50f; // Tune
                else if (k == 2) defVal = 0.60f; // Decay
                else if (k == 3) defVal = 0.20f; // Overdrive
            } else if (m.type == "delay") {
                if (k == 0) defVal = 0.35f;      // Time
                else if (k == 1) defVal = 0.45f; // Feedback
                else if (k == 2) defVal = 0.30f; // Mix
            } else if (m.type == "gain") {
                if (k == 0) defVal = 0.80f;      // Volume
                else if (k == 1) defVal = 0.50f; // Pan
            } else if (m.type == "sid") {
                if (k == 0) defVal = 0.0f;       // Pulse
                else if (k == 1) defVal = 0.50f; // 50% PW
                else if (k == 2) defVal = 0.65f; // Cutoff
                else if (k == 3) defVal = 0.60f; // Res
                else if (k == 4) defVal = 0.07f; // Attack
                else if (k == 5) defVal = 0.33f; // Release
            } else if (m.type == "dx7") {
                if (k == 0) defVal = 4.0f / 31.0f; // Algorithm 5
                else if (k == 1) defVal = 6.0f / 7.0f; // Feedback 6
                else if (k == 2) defVal = 0.50f;  // Brightness 1.0
                else if (k == 3) defVal = 0.425f; // TineBell 0.85
                else if (k == 4) defVal = 0.50f;  // BodyWarmth 1.0
                else if (k == 5) defVal = 0.425f; // MasterVolume 0.85
            } else if (m.type == "snes") {
                if (k == 0) defVal = 1.0f / 11.0f; // Square
                else if (k == 1) defVal = 0.05f;   // Attack
                else if (k == 2) defVal = 0.25f;   // Decay
                else if (k == 3) defVal = 0.40f;   // Sustain
                else if (k == 4) defVal = 0.20f;   // Release
                else if (k == 5) defVal = 0.40f;   // Echo Volume
                else if (k == 6) defVal = 0.85f;   // Master Volume
            } else if (m.type == "ym2612") {
                if (k == 0) defVal = 4.0f / 7.0f;  // Algorithm 4
                else if (k == 1) defVal = 5.0f / 7.0f; // Feedback 5
                else if (k == 2) defVal = 24.0f / 127.0f; // Op1TL
                else if (k == 3) defVal = 0.0f;    // Op2TL
                else if (k == 4) defVal = 18.0f / 127.0f; // Op3TL
                else if (k == 5) defVal = 0.80f;   // Master Volume
            } else if (m.type == "convolver") {
                if (k == 0) defVal = 1.0f / 12.0f; // Great Hall (index 1)
                else if (k == 1) defVal = 0.12f;   // PreDelay 12ms
                else if (k == 2) defVal = 0.375f;  // Decay 1.0
                else if (k == 3) defVal = 0.41f;   // HighCut 8500Hz
                else if (k == 4) defVal = 0.03f;   // LowCut 80Hz
                else if (k == 5) defVal = 0.35f;   // Mix 35%
            }
            knobValues_[key] = defVal;
        }
    }
}

void GuiWindow::updateMixerStrips() {
    // Keep mixerStrips_ strictly 1:1 in sync with arrangerTracks_
    if (!arrangerTracks_.empty()) {
        if (mixerStrips_.size() < arrangerTracks_.size()) {
            for (size_t i = mixerStrips_.size(); i < arrangerTracks_.size(); ++i) {
                MixerStrip strip;
                strip.name = arrangerTracks_[i].name;
                strip.nodeId = static_cast<audio::NodeId>(i + 1);
                strip.volume = arrangerTracks_[i].volume;
                strip.pan = arrangerTracks_[i].pan;
                strip.mute = arrangerTracks_[i].mute;
                strip.solo = arrangerTracks_[i].solo;
                strip.freeze = arrangerTracks_[i].freeze;
                strip.peakL = 0.0f;
                strip.peakR = 0.0f;
                mixerStrips_.push_back(strip);
            }
        } else if (mixerStrips_.size() > arrangerTracks_.size()) {
            mixerStrips_.resize(arrangerTracks_.size());
        }
        for (size_t i = 0; i < arrangerTracks_.size(); ++i) {
            mixerStrips_[i].name = arrangerTracks_[i].name;
            mixerStrips_[i].mute = arrangerTracks_[i].mute;
            mixerStrips_[i].solo = arrangerTracks_[i].solo;
            mixerStrips_[i].freeze = arrangerTracks_[i].freeze;
        }
    }
}

float GuiWindow::getKnobValue(audio::NodeId nodeId, uint32_t knobIndex) const noexcept {
    auto it = knobValues_.find({nodeId, knobIndex});
    if (it != knobValues_.end()) {
        return it->second;
    }
    return 0.5f;
}

void GuiWindow::setKnobValue(audio::NodeId nodeId, uint32_t knobIndex, float value) noexcept {
    knobValues_[{nodeId, knobIndex}] = std::clamp(value, 0.0f, 1.0f);
}

void GuiWindow::dispatchKnobParameter(audio::NodeId nodeId, uint32_t knobIndex, float normalizedVal) noexcept {
    if (!engine_) return;

    std::string modType = "generic";
    for (const auto& m : canvas_.getModules()) {
        if (m.id == nodeId) {
            modType = m.type;
            break;
        }
    }

    float audioVal = normalizedVal;
    if (modType == "tb303") {
        if (knobIndex == 0) { // Cutoff sweep: 150 Hz to 8000 Hz
            audioVal = 150.0f * std::pow(8000.0f / 150.0f, normalizedVal);
            engine_->setCutoff(audioVal);
        } else if (knobIndex == 1) { // Res: 0.0 to 0.98
            audioVal = normalizedVal * 0.98f;
            engine_->setResonance(audioVal);
        } else if (knobIndex == 2) { // EnvMod
            audioVal = normalizedVal;
        } else if (knobIndex == 3) { // Decay
            audioVal = normalizedVal;
        } else if (knobIndex == 4) { // Accent
            audioVal = normalizedVal;
        } else if (knobIndex == 5) { // Waveform (0=Saw, 1=Square)
            audioVal = (normalizedVal >= 0.5f) ? 1.0f : 0.0f;
        }
    } else if (modType == "delay") {
        if (knobIndex == 0) audioVal = 10.0f + normalizedVal * 990.0f; // 10ms - 1000ms
        else if (knobIndex == 1) audioVal = normalizedVal * 0.95f;     // Feedback
        else if (knobIndex == 2) audioVal = normalizedVal;             // Dry/Wet
    } else if (modType == "gain") {
        if (knobIndex == 0) {
            audioVal = normalizedVal * 1.5f;                           // Volume
            engine_->setMasterVolume(audioVal);
        } else if (knobIndex == 1) {
            audioVal = (normalizedVal - 0.5f) * 2.0f;                  // Pan
        }
    } else if (modType == "drum_kit") {
        if (knobIndex == 0) audioVal = normalizedVal * 1.5f;           // Master
        else audioVal = normalizedVal;
    } else if (modType == "biquad") {
        if (knobIndex == 0) audioVal = 60.0f * std::pow(18000.0f / 60.0f, normalizedVal);
        else if (knobIndex == 1) audioVal = 0.5f + normalizedVal * 9.5f;
    } else if (modType == "poly_synth") {
        if (knobIndex == 0) {
            audioVal = 150.0f * std::pow(12000.0f / 150.0f, normalizedVal);
            engine_->setCutoff(audioVal);
        } else if (knobIndex == 1) {
            audioVal = normalizedVal * 0.98f;
            engine_->setResonance(audioVal);
        } else {
            audioVal = normalizedVal;
        }
    } else if (modType == "sid") {
        uint32_t sidParamId = 0;
        if (knobIndex == 0) { // Waveform (0 to 5)
            audioVal = std::round(normalizedVal * 5.0f);
            sidParamId = 0;
        } else if (knobIndex == 1) { // PW (100 to 4000)
            audioVal = 100.0f + normalizedVal * 3900.0f;
            sidParamId = 1;
        } else if (knobIndex == 2) { // Cutoff (100 to 2047)
            audioVal = 100.0f + normalizedVal * 1947.0f;
            sidParamId = 8;
        } else if (knobIndex == 3) { // Res (0 to 15)
            audioVal = normalizedVal * 15.0f;
            sidParamId = 9;
        } else if (knobIndex == 4) { // Atk (0 to 15)
            audioVal = std::round(normalizedVal * 15.0f);
            sidParamId = 11;
        } else if (knobIndex == 5) { // Rel (0 to 15)
            audioVal = std::round(normalizedVal * 15.0f);
            sidParamId = 14;
        }
        engine_->postNodeParameter(nodeId, sidParamId, audioVal);
        return;
    } else if (modType == "dx7") {
        uint32_t dx7ParamId = 0;
        if (knobIndex == 0) { // Algorithm (1..32)
            audioVal = 1.0f + std::round(normalizedVal * 31.0f);
            dx7ParamId = 0;
        } else if (knobIndex == 1) { // Feedback (0..7)
            audioVal = std::round(normalizedVal * 7.0f);
            dx7ParamId = 1;
        } else if (knobIndex == 2) { // Brightness (0..2)
            audioVal = normalizedVal * 2.0f;
            dx7ParamId = 3;
        } else if (knobIndex == 3) { // TineBell (0..2)
            audioVal = normalizedVal * 2.0f;
            dx7ParamId = 4;
        } else if (knobIndex == 4) { // BodyWarmth (0..2)
            audioVal = normalizedVal * 2.0f;
            dx7ParamId = 5;
        } else if (knobIndex == 5) { // MasterVolume (0..2)
            audioVal = normalizedVal * 2.0f;
            dx7ParamId = 2;
        }
        engine_->postNodeParameter(nodeId, dx7ParamId, audioVal);
        return;
    } else if (modType == "snes") {
        uint32_t snesParamId = 0;
        if (knobIndex == 0) { // Waveform (0..11)
            audioVal = std::round(normalizedVal * 11.0f);
            snesParamId = 0;
        } else if (knobIndex == 1) { // Attack
            audioVal = 0.001f + normalizedVal * 0.999f;
            snesParamId = 2;
        } else if (knobIndex == 2) { // Decay
            audioVal = 0.01f + normalizedVal * 0.99f;
            snesParamId = 3;
        } else if (knobIndex == 3) { // Sustain
            audioVal = normalizedVal;
            snesParamId = 4;
        } else if (knobIndex == 4) { // Release
            audioVal = 0.01f + normalizedVal * 0.99f;
            snesParamId = 5;
        } else if (knobIndex == 5) { // Echo Volume
            audioVal = normalizedVal;
            snesParamId = 9;
        } else if (knobIndex == 6) { // Master Volume
            audioVal = normalizedVal * 1.5f;
            snesParamId = 14;
        }
        engine_->postNodeParameter(nodeId, snesParamId, audioVal);
        return;
    } else if (modType == "ym2612") {
        uint32_t ymParamId = 0;
        if (knobIndex == 0) { // Algorithm (0..7)
            audioVal = std::round(normalizedVal * 7.0f);
            ymParamId = 0;
        } else if (knobIndex == 1) { // Feedback (0..7)
            audioVal = std::round(normalizedVal * 7.0f);
            ymParamId = 1;
        } else if (knobIndex == 2) { // Op1TL (0..127)
            audioVal = normalizedVal * 127.0f;
            ymParamId = 4;
        } else if (knobIndex == 3) { // Op2TL (0..127)
            audioVal = normalizedVal * 127.0f;
            ymParamId = 6;
        } else if (knobIndex == 4) { // Op3TL (0..127)
            audioVal = normalizedVal * 127.0f;
            ymParamId = 8;
        } else if (knobIndex == 5) { // Master Volume (0..1.5)
            audioVal = normalizedVal * 1.5f;
            ymParamId = 2;
        }
        engine_->postNodeParameter(nodeId, ymParamId, audioVal);
        return;
    } else if (modType == "convolver") {
        uint32_t convParamId = 0;
        if (knobIndex == 0) { // Preset index (0..12)
            audioVal = std::round(normalizedVal * 12.0f);
            convParamId = 1;
        } else if (knobIndex == 1) { // Pre-Delay (0..100 ms)
            audioVal = normalizedVal * 100.0f;
            convParamId = 2;
        } else if (knobIndex == 2) { // Decay (0.1..2.5x)
            audioVal = 0.1f + normalizedVal * 2.4f;
            convParamId = 3;
        } else if (knobIndex == 3) { // HighCut (500..20000 Hz)
            audioVal = 500.0f + normalizedVal * 19500.0f;
            convParamId = 4;
        } else if (knobIndex == 4) { // LowCut (20..2000 Hz)
            audioVal = 20.0f + normalizedVal * 1980.0f;
            convParamId = 5;
        } else if (knobIndex == 5) { // Mix (0..1)
            audioVal = normalizedVal;
            convParamId = 0;
        }
        engine_->postNodeParameter(nodeId, convParamId, audioVal);
        return;
    }

    engine_->postNodeParameter(nodeId, knobIndex, audioVal);
}

namespace {
void hitTestNodeRecursive(const project::GuiLayoutNode& node, float x, float y, const project::PresetDefinition& preset, HitTestHardwareKnobResult& out, float faceplateX = 40.0f, float faceplateY = 98.0f, float scale = 1.0f) {
    if (out.hit) return;

    float adjNodeY = faceplateY + 54.0f + (node.boundsY - 152.0f) * scale;
    float adjNodeX = faceplateX + (node.boundsX - 40.0f);

    if (node.type == project::GuiNodeType::Knob) {
        float cx = adjNodeX + node.boundsW * 0.5f;
        float cy = adjNodeY + node.boundsH * 0.38f * (scale < 0.95f ? scale : 1.0f);
        float kRad = node.size * 0.42f;
        float drawCx = (node.labelAlignment == "inline_right") ? (cx - kRad * 0.65f) : cx;
        float radius = std::max(kRad + 4.0f, 18.0f);
        if (std::hypot(x - drawCx, y - cy) <= radius) {
            out.hit = true;
            out.paramName = node.paramName;
            out.position = {drawCx, cy};
            const auto* p = preset.findParam(node.paramName);
            out.currentNormVal = p ? p->getNormalized() : 0.5f;
            return;
        }
    } else if (node.type == project::GuiNodeType::Switch) {
        float cx = adjNodeX + node.boundsW * 0.5f;
        float cy = adjNodeY + node.boundsH * 0.52f * scale;
        if (std::abs(x - cx) <= 18.0f && std::abs(y - cy) <= 28.0f) {
            out.hit = true;
            out.paramName = node.paramName;
            out.position = {cx, cy};
            const auto* p = preset.findParam(node.paramName);
            out.currentNormVal = p ? p->getNormalized() : 0.0f;
            return;
        }
    } else if (node.type == project::GuiNodeType::Slider) {
        float trkX = adjNodeX + 24.0f;
        float trkY = adjNodeY + 20.0f;
        float trkW = node.boundsW - 52.0f;
        if (x >= trkX - 10.0f && x <= trkX + trkW + 10.0f && y >= trkY - 12.0f && y <= trkY + 26.0f) {
            out.hit = true;
            out.paramName = node.paramName;
            out.position = {trkX, trkY};
            const auto* p = preset.findParam(node.paramName);
            out.currentNormVal = p ? p->getNormalized() : 0.5f;
            return;
        }
    }

    for (const auto& child : node.children) {
        hitTestNodeRecursive(child, x, y, preset, out, faceplateX, faceplateY, scale);
        if (out.hit) return;
    }
}

void hitTestCompactNodeRecursive(const project::GuiLayoutNode& node, float localX, float localY, const project::PresetDefinition& preset, HitTestHardwareKnobResult& out) {
    if (out.hit) return;

    if (node.type == project::GuiNodeType::Knob) {
        float cx = node.boundsX + node.boundsW * 0.5f;
        float cy = node.boundsY + node.boundsH * 0.38f;
        float kRad = node.size * 0.42f;
        float drawCx = (node.labelAlignment == "inline_right") ? (cx - kRad * 0.65f) : cx;
        float radius = std::max(kRad + 4.0f, 18.0f);
        if (std::hypot(localX - drawCx, localY - cy) <= radius) {
            out.hit = true;
            out.paramName = node.paramName;
            out.position = {drawCx, cy};
            const auto* p = preset.findParam(node.paramName);
            out.currentNormVal = p ? p->getNormalized() : 0.5f;
            return;
        }
    } else if (node.type == project::GuiNodeType::Switch) {
        float cx = node.boundsX + node.boundsW * 0.5f;
        float cy = node.boundsY + node.boundsH * 0.50f;
        if (std::abs(localX - cx) <= 14.0f && std::abs(localY - cy) <= 22.0f) {
            out.hit = true;
            out.paramName = node.paramName;
            out.position = {cx, cy};
            const auto* p = preset.findParam(node.paramName);
            out.currentNormVal = p ? p->getNormalized() : 0.0f;
            return;
        }
    } else if (node.type == project::GuiNodeType::Slider) {
        float trkX = node.boundsX + 10.0f;
        float trkY = node.boundsY + node.boundsH * 0.5f;
        float trkW = node.boundsW - 20.0f;
        if (localX >= trkX - 8.0f && localX <= trkX + trkW + 8.0f && std::abs(localY - trkY) <= 12.0f) {
            out.hit = true;
            out.paramName = node.paramName;
            out.position = {trkX, trkY};
            const auto* p = preset.findParam(node.paramName);
            out.currentNormVal = p ? p->getNormalized() : 0.5f;
            return;
        }
    }

    for (const auto& child : node.children) {
        hitTestCompactNodeRecursive(child, localX, localY, preset, out);
        if (out.hit) return;
    }
}
} // namespace

void GuiWindow::loadPresets() {
    // 1. Initialize PresetManager catalog
    project::PresetManager::instance().initialize();

    // 2. Start with baseline builtin presets for test compatibility
    presets_ = project::PresetLoader::getBuiltinPresets();

    // 3. Enrich and append all presets from PresetManager catalog
    const auto& catalog = project::PresetManager::instance().getAllPresets();
    for (const auto& item : catalog) {
        bool replaced = false;
        for (auto& existing : presets_) {
            if (existing.metadata.id == item.id) {
                if (existing.rawScript.empty() && !item.rawScript.empty()) {
                    existing.rawScript = item.rawScript;
                }
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            project::PresetDefinition def{};
            try {
                if (project::PresetManager::instance().loadPresetDefinition(item, def)) {
                    presets_.push_back(std::move(def));
                }
            } catch (const std::exception& e) {
                std::cerr << "[GuiWindow] Warning: Skipping preset '" << item.id << "': " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[GuiWindow] Warning: Unknown error loading preset '" << item.id << "'" << std::endl;
            }
        }
    }

    if (!presets_.empty()) {
        for (auto& pr : presets_) {
            pr.compactGuiRoot = pr.guiRoot;
            project::PresetLoader::computeLayoutBounds(pr.compactGuiRoot, 0.0f, 0.0f, 520.0f, 210.0f);
        }
        if (activePresetIndex_ >= presets_.size()) activePresetIndex_ = 0;
        project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                                   40.0f, 98.0f,
                                                   static_cast<float>(width_) - 80.0f,
                                                   static_cast<float>(height_) - 154.0f);
    }
}

void GuiWindow::loadPresetFile(const std::string& filePath) {
    project::PresetDefinition loaded{};
    if (project::PresetLoader::loadFromFile(filePath, loaded)) {
        loaded.compactGuiRoot = loaded.guiRoot;
        project::PresetLoader::computeLayoutBounds(loaded.compactGuiRoot, 0.0f, 0.0f, 520.0f, 210.0f);
        presets_.push_back(loaded);
        activePresetIndex_ = presets_.size() - 1;
        project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                                   40.0f, 98.0f,
                                                   static_cast<float>(width_) - 80.0f,
                                                   static_cast<float>(height_) - 154.0f);
    }
}

void GuiWindow::nextPreset() {
    if (presets_.empty()) return;
    activePresetIndex_ = (activePresetIndex_ + 1) % presets_.size();
    presets_[activePresetIndex_].compactGuiRoot = presets_[activePresetIndex_].guiRoot;
    project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].compactGuiRoot, 0.0f, 0.0f, 520.0f, 210.0f);
    project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                               40.0f, 98.0f,
                                               static_cast<float>(width_) - 80.0f,
                                               static_cast<float>(height_) - 154.0f);
    if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
        auto* trk = engine_->getSequencer().getTrack(selectedTrackIndex_);
        if (trk && !presets_[activePresetIndex_].rawScript.empty()) {
            trk->setEatscriptCode(presets_[activePresetIndex_].rawScript);
        }
    }
    if (designSubView_ == DesignSubView::Eatscript && !presets_[activePresetIndex_].rawScript.empty()) {
        setScriptCode(presets_[activePresetIndex_].rawScript);
    }
}

void GuiWindow::prevPreset() {
    if (presets_.empty()) return;
    if (activePresetIndex_ == 0) {
        activePresetIndex_ = presets_.size() - 1;
    } else {
        activePresetIndex_--;
    }
    presets_[activePresetIndex_].compactGuiRoot = presets_[activePresetIndex_].guiRoot;
    project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].compactGuiRoot, 0.0f, 0.0f, 520.0f, 210.0f);
    project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                               40.0f, 98.0f,
                                               static_cast<float>(width_) - 80.0f,
                                               static_cast<float>(height_) - 154.0f);
    if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
        auto* trk = engine_->getSequencer().getTrack(selectedTrackIndex_);
        if (trk && !presets_[activePresetIndex_].rawScript.empty()) {
            trk->setEatscriptCode(presets_[activePresetIndex_].rawScript);
        }
    }
    if (designSubView_ == DesignSubView::Eatscript && !presets_[activePresetIndex_].rawScript.empty()) {
        setScriptCode(presets_[activePresetIndex_].rawScript);
    }
}

void GuiWindow::setSelectedTrackIndex(uint32_t idx) noexcept {
    selectedTrackIndex_ = idx;
    if (engine_) {
        engine_->setActiveTrack(idx);
    }
    if (virtualKeyboardDrawerWidget_) {
        virtualKeyboardDrawerWidget_->setActiveTrackIndex(idx);
        std::string eng;
        if (idx < arrangerTracks_.size()) {
            eng = arrangerTracks_[idx].instrumentEngine;
            if (eng.empty()) eng = arrangerTracks_[idx].instrument;
        } else if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size()) {
            eng = modularArrangerView_->getTracks()[idx].instrumentEngine;
            if (eng.empty()) eng = modularArrangerView_->getTracks()[idx].instrument;
        }
        std::string engLower = eng;
        for (char& c : engLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (engLower.find("drum") != std::string::npos || engLower.find("808") != std::string::npos ||
            engLower.find("909") != std::string::npos || engLower.find("mpc") != std::string::npos ||
            engLower.find("drumpad") != std::string::npos || engLower.find("drum_pad") != std::string::npos) {
            virtualKeyboardDrawerWidget_->setMode(KeyboardDrawerMode::DrumPads);
            if (engLower.find("808") != std::string::npos) {
                virtualKeyboardDrawerWidget_->getDrumPadGrid().setProfile(DrumKitProfile::Eats808);
            } else if (engLower.find("909") != std::string::npos) {
                virtualKeyboardDrawerWidget_->getDrumPadGrid().setProfile(DrumKitProfile::Eats909);
            } else {
                virtualKeyboardDrawerWidget_->getDrumPadGrid().setProfile(DrumKitProfile::StandardGm);
            }
        } else {
            virtualKeyboardDrawerWidget_->setMode(KeyboardDrawerMode::Piano);
        }
    }
    if (modularArrangerView_) {
        modularArrangerView_->setActiveTrack(idx);
    }
    if (modularTrackInspectorView_) {
        modularTrackInspectorView_->setActiveTrack(idx);
    }
    syncActiveClipToEditView(idx, -1);
    syncTrackToPreset(idx);
}

void GuiWindow::handleTrackAdded(uint32_t trackIdx) {
    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        const auto& trk = modularArrangerView_->getTracks()[trackIdx];
        if (trackIdx >= arrangerTracks_.size()) {
            ArrangerTrackData atd;
            atd.id = trk.id.empty() ? ("trk_" + std::to_string(trackIdx + 1)) : trk.id;
            atd.name = trk.name;
            atd.instrument = trk.instrument;
            atd.instrumentEngine = trk.instrumentEngine.empty() ? "synth" : trk.instrumentEngine;
            atd.eatscriptCode = trk.eatscriptCode;
            atd.r = trk.r; atd.g = trk.g; atd.b = trk.b;
            atd.clips.clear();
            arrangerTracks_.push_back(atd);
            updateMixerStrips();
            if (engine_) {
                std::string eng = atd.instrumentEngine.empty() ? atd.instrument : atd.instrumentEngine;
                if (trackIdx >= engine_->getSequencer().getNumTracks()) {
                    engine_->addTrack(trk.name, 0, 16, eng);
                } else {
                    engine_->assignTrackInstrument(trackIdx, eng);
                }
            }
        }
        setSelectedTrackIndex(trackIdx);
        recordProjectHistory("Add Track " + std::to_string(trackIdx + 1), "TRACK");
        setStatusMessage("Added Track " + std::to_string(trackIdx + 1) + ": " + trk.name);
    }
}

void GuiWindow::addTrackToProject(const std::string& name, const std::string& instrumentEngine, float r, float g, float b) {
    uint32_t newIdx = static_cast<uint32_t>(arrangerTracks_.size());
    std::string trkName = name.empty() ? ("Track " + std::to_string(newIdx + 1)) : name;
    std::string eng = instrumentEngine.empty() ? "synth" : instrumentEngine;

    ArrangerTrackData atd;
    static std::atomic<uint64_t> s_guiTrackSeq{300};
    atd.id = "trk_" + std::to_string(++s_guiTrackSeq);
    atd.name = trkName;
    atd.instrument = trkName;
    atd.instrumentEngine = eng;
    atd.r = r; atd.g = g; atd.b = b;
    atd.clips.clear(); // Clean track ready for audio or MIDI clips
    arrangerTracks_.push_back(atd);
    updateMixerStrips();

    if (engine_) {
        if (newIdx >= engine_->getSequencer().getNumTracks()) {
            engine_->addTrack(trkName, 0, 16, eng);
        } else {
            engine_->assignTrackInstrument(newIdx, eng);
        }
    }

    if (modularArrangerView_) {
        modularArrangerView_->addTrack(trkName, eng, r, g, b);
        if (newIdx < modularArrangerView_->getTracks().size()) {
            modularArrangerView_->getTracks()[newIdx].id = atd.id;
        }
    }
    if (modularTrackInspectorView_) {
        InspectorTrackChannel ch;
        ch.id = atd.id;
        ch.name = trkName;
        ch.instrument = trkName;
        ch.instrumentEngine = eng;
        ch.r = r; ch.g = g; ch.b = b;
        ch.volume = 0.8f;
        modularTrackInspectorView_->getTracks().push_back(ch);
        modularTrackInspectorView_->updateDrawerData();
    }
    if (modularMixerView_) {
        MixerChannelStrip ch;
        ch.name = trkName;
        ch.instrument = trkName;
        ch.r = r; ch.g = g; ch.b = b;
        ch.fader = 0.8f;
        modularMixerView_->getChannels().push_back(ch);
    }
    syncDesignViewToProject();

    setSelectedTrackIndex(newIdx);
    recordProjectHistory("Add Track " + std::to_string(newIdx + 1), "TRACK");
    setStatusMessage("Added Track " + std::to_string(newIdx + 1) + ": " + trkName);
}

void GuiWindow::changeTrackInstrument(uint32_t tIdx, const std::string& instId, const std::string& instName) {
    project::PresetDefinition def{};
    bool loadedDef = false;
    if (!instId.empty() && project::PresetManager::instance().loadPresetDefinition(instId, def)) {
        loadedDef = true;
    } else if (!instName.empty() && project::PresetManager::instance().loadPresetDefinition(instName, def)) {
        loadedDef = true;
    } else {
        auto opt = project::BuiltinPresetRegistry::createPresetById(instId);
        if (!opt) opt = project::BuiltinPresetRegistry::createPresetById(instName);
        if (opt) {
            def = *opt;
            loadedDef = true;
        }
    }

    std::optional<GuiPanelDef> customGui;
    std::vector<TrackPropertiesKnob> customKnobs;
    std::string scriptCode;
    if (loadedDef) {
        customGui = createGuiPanelFromPresetDefinition(def);
        customKnobs = extractKnobsFromPresetDefinition(def);
        scriptCode = def.rawScript;
    }

    if (tIdx < arrangerTracks_.size()) {
        arrangerTracks_[tIdx].instrument = instName;
        arrangerTracks_[tIdx].instrumentEngine = instId;
        if (loadedDef) {
            arrangerTracks_[tIdx].customGuiPanel = customGui;
            arrangerTracks_[tIdx].eatscriptCode = scriptCode;
        }
    }
    if (tIdx < mixerStrips_.size()) {
        mixerStrips_[tIdx].name = instName;
    }
    if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
        auto& trk = modularArrangerView_->getTracks()[tIdx];
        trk.instrument = instName;
        trk.instrumentEngine = instId;
        if (loadedDef) {
            trk.customGuiPanel = customGui;
            trk.knobs = customKnobs;
            trk.eatscriptCode = scriptCode;
        }
        modularArrangerView_->syncDrawerDataForTrack(tIdx);
    }
    if (modularTrackInspectorView_ && tIdx < modularTrackInspectorView_->getTracks().size()) {
        auto& trk = modularTrackInspectorView_->getTracks()[tIdx];
        trk.instrument = instName;
        trk.instrumentEngine = instId;
        if (loadedDef) {
            trk.customGuiPanel = customGui;
            trk.eatscriptCode = scriptCode;
            trk.knobs.clear();
            for (const auto& k : customKnobs) {
                trk.knobs.push_back({k.name, k.label, k.value, k.display});
            }
        } else {
            modularTrackInspectorView_->syncKnobsForTrack(trk);
        }
        modularTrackInspectorView_->updateDrawerData();
    }
    if (modularMixerView_ && tIdx < modularMixerView_->getChannels().size()) {
        auto& ch = modularMixerView_->getChannels()[tIdx];
        ch.instrument = instName;
    }
    if (engine_) {
        engine_->assignTrackInstrument(tIdx, instId.empty() ? instName : instId);
        if (!scriptCode.empty() && tIdx < engine_->getSequencer().getNumTracks()) {
            auto* seqTrk = engine_->getSequencer().getTrack(tIdx);
            if (seqTrk) {
                seqTrk->setEatscriptCode(scriptCode);
            }
        }
    }
    setSelectedTrackIndex(tIdx);
    syncTrackToPreset(tIdx);
    syncDesignViewToProject();
    recordProjectHistory("Change Track " + std::to_string(tIdx + 1) + " Instrument to " + instName, "INSTRUMENT");
    setStatusMessage("Changed Track " + std::to_string(tIdx + 1) + " Instrument: " + instName);
}

void GuiWindow::addAudioFxToTrackIndex(uint32_t trackIdx, const std::string& fxId, const std::string& fxNameHint, const std::string& engineTagHint) {
    if (trackIdx >= arrangerTracks_.size()) return;
    std::string fxName = fxNameHint.empty() ? fxId : fxNameHint;
    std::string engineTag = engineTagHint.empty() ? fxId : engineTagHint;
    const auto* item = project::PresetManager::instance().findPreset(fxId);
    if (item) {
        fxName = item->name;
        engineTag = item->engineTag.empty() ? item->id : item->engineTag;
    }
    TrackAudioFxItem fxItem(fxName, engineTag, 0.5f, 0.5f, true);
    fxItem.id = fxId;
    project::PresetDefinition def{};
    if (project::PresetManager::instance().loadPresetDefinition(fxId, def)) {
        fxItem.populateFromPresetDefinition(def);
    }
    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        modularArrangerView_->getTracks()[trackIdx].audioFx.push_back(fxItem);
        modularArrangerView_->syncDrawerDataForTrack(trackIdx);
    }
    if (modularTrackInspectorView_ && trackIdx < modularTrackInspectorView_->getTracks().size()) {
        modularTrackInspectorView_->getTracks()[trackIdx].audioFxList.push_back(fxItem);
        modularTrackInspectorView_->updateDrawerData();
    }
    syncTrackAudioFxToEngine(trackIdx);
    syncDesignViewToProject();
    recordProjectHistory("Add Audio FX " + fxName + " to Track " + std::to_string(trackIdx + 1), "AUDIO_FX");
    setStatusMessage("Added Audio FX: " + fxName + " to Track " + std::to_string(trackIdx + 1));
}

void GuiWindow::addMidiFxToTrackIndex(uint32_t trackIdx, const std::string& fxId, const std::string& fxNameHint, const std::string& engineTagHint) {
    if (trackIdx >= arrangerTracks_.size()) return;
    std::string fxName = fxNameHint.empty() ? fxId : fxNameHint;
    std::string engineTag = engineTagHint.empty() ? fxId : engineTagHint;
    const auto* item = project::PresetManager::instance().findPreset(fxId);
    if (item) {
        fxName = item->name;
        engineTag = (item->engineTag.empty() || item->engineTag == "MIDI Processor" || item->engineTag == "DSP Effect") ? item->name : item->engineTag;
    }
    TrackMidiFxItem mfxItem(fxName, engineTag, 0, 0, true);
    mfxItem.id = fxId;
    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        modularArrangerView_->getTracks()[trackIdx].midiFx.push_back(mfxItem);
        modularArrangerView_->syncDrawerDataForTrack(trackIdx);
    }
    if (modularTrackInspectorView_ && trackIdx < modularTrackInspectorView_->getTracks().size()) {
        modularTrackInspectorView_->getTracks()[trackIdx].midiFxList.push_back(mfxItem);
        modularTrackInspectorView_->updateDrawerData();
    }
    syncTrackMidiFxToEngine(trackIdx);
    syncArrangerToSequencer();
    syncDesignViewToProject();
    recordProjectHistory("Add MIDI FX " + fxName + " to Track " + std::to_string(trackIdx + 1), "MIDI_FX");
    setStatusMessage("Added MIDI FX: " + fxName + " to Track " + std::to_string(trackIdx + 1));
}

void GuiWindow::deleteTrack(uint32_t trackIdx) {
    if (isDeletingTrack_) return;
    struct DeletingTrackGuard {
        bool& flag;
        explicit DeletingTrackGuard(bool& f) : flag(f) { flag = true; }
        ~DeletingTrackGuard() { flag = false; }
    } guard(isDeletingTrack_);

    if (arrangerTracks_.size() <= 1 || trackIdx >= arrangerTracks_.size()) {
        setStatusMessage("Cannot delete the only remaining track");
        return;
    }

    std::string deletedName = arrangerTracks_[trackIdx].name;
    arrangerTracks_.erase(arrangerTracks_.begin() + trackIdx);
    updateMixerStrips();

    if (engine_) {
        engine_->removeTrack(trackIdx);
    }

    if (modularArrangerView_) {
        if (modularArrangerView_->getTracks().size() > arrangerTracks_.size() &&
            trackIdx < modularArrangerView_->getTracks().size()) {
            modularArrangerView_->deleteTrack(trackIdx, /*notify=*/false);
        } else {
            if (modularArrangerView_->getActiveTrackIndex() >= arrangerTracks_.size()) {
                modularArrangerView_->setActiveTrack(static_cast<uint32_t>(arrangerTracks_.size() - 1));
            }
            modularArrangerView_->setSelectedClip(-1);
        }
    }

    if (modularTrackInspectorView_ && trackIdx < modularTrackInspectorView_->getTracks().size()) {
        auto& inspTracks = modularTrackInspectorView_->getTracks();
        if (inspTracks.size() > 1) {
            inspTracks.erase(inspTracks.begin() + trackIdx);
            uint32_t newActive = std::min(selectedTrackIndex_, static_cast<uint32_t>(inspTracks.size() - 1));
            modularTrackInspectorView_->setActiveTrack(newActive);
            modularTrackInspectorView_->updateDrawerData();
        }
    }

    if (modularMixerView_ && trackIdx < modularMixerView_->getChannels().size()) {
        auto& chs = modularMixerView_->getChannels();
        if (chs.size() > 1) {
            chs.erase(chs.begin() + trackIdx);
            uint32_t newActive = std::min(selectedTrackIndex_, static_cast<uint32_t>(chs.size() - 1));
            modularMixerView_->setSelectedChannel(newActive);
        }
    }

    if (selectedTrackIndex_ >= arrangerTracks_.size()) {
        selectedTrackIndex_ = static_cast<uint32_t>(arrangerTracks_.size() - 1);
    }
    setSelectedTrackIndex(selectedTrackIndex_);
    syncArrangerToSequencer();
    syncDesignViewToProject();
    recordProjectHistory("Delete Track " + std::to_string(trackIdx + 1), "TRACK");
    setStatusMessage("Deleted Track: " + deletedName);
}

void GuiWindow::duplicateTrack(uint32_t trackIdx) {
    if (isDuplicatingTrack_) return;
    struct DuplicatingTrackGuard {
        bool& flag;
        explicit DuplicatingTrackGuard(bool& f) : flag(f) { flag = true; }
        ~DuplicatingTrackGuard() { flag = false; }
    } guard(isDuplicatingTrack_);

    if (trackIdx >= arrangerTracks_.size()) return;
    ArrangerTrackData dup = arrangerTracks_[trackIdx];
    dup.name += " (Copy)";
    uint32_t newTrackIdx = static_cast<uint32_t>(arrangerTracks_.size());
    arrangerTracks_.push_back(dup);
    updateMixerStrips();

    if (engine_) {
        std::string eng = dup.instrumentEngine.empty() ? dup.instrument : dup.instrumentEngine;
        if (newTrackIdx >= engine_->getSequencer().getNumTracks()) {
            engine_->addTrack(dup.name, 0, 16, eng);
        } else {
            engine_->assignTrackInstrument(newTrackIdx, eng);
        }
    }

    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        auto arrDup = modularArrangerView_->getTracks()[trackIdx];
        arrDup.name += " (Copy)";
        for (size_t i = 0; i < arrDup.clips.size(); ++i) {
            arrDup.clips[i].id = "clip_" + std::to_string(newTrackIdx + 1) + "_" + std::to_string(i + 1);
            arrDup.clips[i].trackIndex = newTrackIdx;
        }
        modularArrangerView_->getTracks().push_back(arrDup);
        modularArrangerView_->setActiveTrack(newTrackIdx);
        modularArrangerView_->setSelectedClip(-1);
    }
    syncDesignViewToProject();

    setSelectedTrackIndex(newTrackIdx);
    recordProjectHistory("Duplicate Track " + std::to_string(trackIdx + 1), "TRACK");
    setStatusMessage("Duplicated Track: " + dup.name);
}

void GuiWindow::openDesignForTrack(uint32_t trackIdx) {
    if (trackIdx < arrangerTracks_.size()) {
        setSelectedTrackIndex(trackIdx);
    }
    setActiveView(WorkspaceView::Design);
    setDesignSubView(DesignSubView::Eatscript);
    if (modularDesignView_) {
        modularDesignView_->selectTargetByTrackAndType(static_cast<int>(trackIdx), ScriptTargetType::TrackDsp);
    }
}

void GuiWindow::addClipToTrack(uint32_t trackIdx) {
    if (trackIdx >= arrangerTracks_.size()) return;
    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        modularArrangerView_->addClipToTrack(trackIdx);
        const auto& arrTrk = modularArrangerView_->getTracks()[trackIdx];
        arrangerTracks_[trackIdx].clips.clear();
        for (const auto& c : arrTrk.clips) {
            ArrangerClip ac;
            ac.name = c.name;
            ac.startBar = c.startBar;
            ac.barLength = c.lengthBars;
            ac.isLooped = c.isLooped;
            ac.loopLengthBars = c.loopLengthBars;
            ac.isAudio = c.isAudio;
            ac.transposeSemitones = c.transposeSemitones;
            ac.r = c.r; ac.g = c.g; ac.b = c.b;
            arrangerTracks_[trackIdx].clips.push_back(ac);
        }
    }
    syncArrangerToSequencer();
    recordProjectHistory("Add Clip to Track " + std::to_string(trackIdx + 1), "CLIP");
    setStatusMessage("Added new clip to Track " + std::to_string(trackIdx + 1));
}

void GuiWindow::duplicateClip(uint32_t trackIdx, int clipIdx) {
    if (trackIdx >= arrangerTracks_.size()) return;
    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        modularArrangerView_->duplicateClip(trackIdx, clipIdx);
        const auto& arrTrk = modularArrangerView_->getTracks()[trackIdx];
        arrangerTracks_[trackIdx].clips.clear();
        for (const auto& c : arrTrk.clips) {
            ArrangerClip ac;
            ac.name = c.name;
            ac.startBar = c.startBar;
            ac.barLength = c.lengthBars;
            ac.isLooped = c.isLooped;
            ac.loopLengthBars = c.loopLengthBars;
            ac.isAudio = c.isAudio;
            ac.transposeSemitones = c.transposeSemitones;
            ac.r = c.r; ac.g = c.g; ac.b = c.b;
            arrangerTracks_[trackIdx].clips.push_back(ac);
        }
    }
    syncArrangerToSequencer();
    recordProjectHistory("Duplicate Clip on Track " + std::to_string(trackIdx + 1), "CLIP");
    setStatusMessage("Duplicated clip on Track " + std::to_string(trackIdx + 1));
}

void GuiWindow::deleteClip(uint32_t trackIdx, int clipIdx) {
    if (trackIdx >= arrangerTracks_.size()) return;
    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        modularArrangerView_->deleteClip(trackIdx, clipIdx);
        const auto& arrTrk = modularArrangerView_->getTracks()[trackIdx];
        arrangerTracks_[trackIdx].clips.clear();
        for (const auto& c : arrTrk.clips) {
            ArrangerClip ac;
            ac.name = c.name;
            ac.startBar = c.startBar;
            ac.barLength = c.lengthBars;
            ac.isLooped = c.isLooped;
            ac.loopLengthBars = c.loopLengthBars;
            ac.isAudio = c.isAudio;
            ac.transposeSemitones = c.transposeSemitones;
            ac.r = c.r; ac.g = c.g; ac.b = c.b;
            arrangerTracks_[trackIdx].clips.push_back(ac);
        }
    }
    syncArrangerToSequencer();
    recordProjectHistory("Delete Clip from Track " + std::to_string(trackIdx + 1), "CLIP");
    setStatusMessage("Deleted clip from Track " + std::to_string(trackIdx + 1));
}

void GuiWindow::setTrackMuteState(uint32_t trackIdx, bool mute) {
    if (trackIdx < arrangerTracks_.size()) {
        arrangerTracks_[trackIdx].mute = mute;
    }
    if (trackIdx < mixerStrips_.size()) {
        mixerStrips_[trackIdx].mute = mute;
    }
    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        modularArrangerView_->getTracks()[trackIdx].mute = mute;
    }
    if (engine_) {
        engine_->setTrackMute(trackIdx, mute);
    }
    recordProjectHistory("Toggle Mute on Track " + std::to_string(trackIdx + 1), "TRACK");
}

void GuiWindow::setTrackSoloState(uint32_t trackIdx, bool solo) {
    if (trackIdx < arrangerTracks_.size()) {
        arrangerTracks_[trackIdx].solo = solo;
    }
    if (trackIdx < mixerStrips_.size()) {
        mixerStrips_[trackIdx].solo = solo;
    }
    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        modularArrangerView_->getTracks()[trackIdx].solo = solo;
    }
    if (engine_) {
        engine_->setTrackSolo(trackIdx, solo);
    }
    recordProjectHistory("Toggle Solo on Track " + std::to_string(trackIdx + 1), "TRACK");
}

void GuiWindow::setTrackFreezeState(uint32_t trackIdx, bool freeze) {
    if (trackIdx < arrangerTracks_.size()) {
        arrangerTracks_[trackIdx].freeze = freeze;
    }
    if (trackIdx < mixerStrips_.size()) {
        mixerStrips_[trackIdx].freeze = freeze;
    }
    if (engine_) {
        if (freeze) {
            engine_->freezeTrack(trackIdx);
        } else {
            engine_->unfreezeTrack(trackIdx);
        }
    }
    recordProjectHistory("Toggle Freeze on Track " + std::to_string(trackIdx + 1), "TRACK");
    setStatusMessage(freeze ?
        ("Track " + std::to_string(trackIdx + 1) + ": FROZEN (DSP synth & FX offloaded to PCM buffer)") :
        ("Track " + std::to_string(trackIdx + 1) + ": UNFROZEN (Live real-time synthesis restored)"));
}

void GuiWindow::syncActiveClipToEditView(uint32_t trackIdx, int clipIdx) {
    if (!modularEditView_ || !modularArrangerView_) return;

    auto& tracks = modularArrangerView_->getTracks();
    if (trackIdx >= tracks.size()) return;

    auto& trk = tracks[trackIdx];
    int cIdx = clipIdx;
    if (cIdx < 0 || static_cast<size_t>(cIdx) >= trk.clips.size()) {
        cIdx = modularArrangerView_->getSelectedClipIndex();
        if (cIdx < 0 || static_cast<size_t>(cIdx) >= trk.clips.size()) {
            cIdx = 0;
        }
    }

    if (!trk.clips.empty() && static_cast<size_t>(cIdx) < trk.clips.size()) {
        modularArrangerView_->setSelectedClip(cIdx);

        // Build MIDI FX rack for EditView
        std::vector<eatscript::MidiFxInsert> mfxRack;
        for (const auto& fx : trk.midiFx) {
            if (!fx.enabled) continue;
            eatscript::MidiFxInsert ins;
            ins.name = fx.name;
            ins.type = eatscript::MidiPipelineEngine::detectMidiFxType(fx.id, fx.name + " " + fx.type);
            ins.enabled = fx.enabled;
            ins.params["rootKey"] = static_cast<float>(fx.rootKey);
            ins.params["scaleMode"] = static_cast<float>(fx.scaleMode);
            for (const auto& k : fx.knobs) ins.params[k.name] = k.value;
            mfxRack.push_back(ins);
        }

        modularEditView_->loadFromArrangerClip(
            trk.clips[cIdx],
            trk.name,
            Color(trk.r, trk.g, trk.b),
            tracks,
            trk.chordFollowMode,
            mfxRack,
            modularArrangerView_->getChordTrack()
        );
    }
}

void GuiWindow::syncArrangerToSequencer() {
    if (!engine_ || !modularArrangerView_) return;

    auto& seq = engine_->getSequencer();
    const auto& tracks = modularArrangerView_->getTracks();
    uint32_t numTracks = static_cast<uint32_t>(tracks.size());

    // Determine arrangement extent in bars / steps
    uint32_t maxBar = 16; // At least 16 bars for standard arrangement
    for (const auto& trk : tracks) {
        for (const auto& clip : trk.clips) {
            uint32_t clipEndBar = clip.startBar + clip.lengthBars - 1;
            if (clipEndBar > maxBar) {
                maxBar = clipEndBar;
            }
        }
    }
    uint32_t totalArrangerSteps = std::min(static_cast<uint32_t>(sequencer::MAX_STEPS_PER_TRACK), maxBar * 16u);
    seq.getTransport().setMaxSteps(totalArrangerSteps);

    while (seq.getNumTracks() < numTracks) {
        uint32_t nextIdx = static_cast<uint32_t>(seq.getNumTracks());
        audio::NodeId srcId = engine_->getTrackSourceNodeId(nextIdx);
        seq.addTrack("Track " + std::to_string(nextIdx + 1), (srcId != audio::INVALID_NODE_ID) ? srcId : 0, totalArrangerSteps);
    }

    for (uint32_t t = 0; t < numTracks; ++t) {
        auto* seqTrack = seq.getTrack(t);
        if (!seqTrack) continue;

        seqTrack->setNumSteps(totalArrangerSteps);
        audio::NodeId srcId = engine_->getTrackSourceNodeId(t);
        if (srcId != audio::INVALID_NODE_ID && srcId != 0) {
            seqTrack->setTargetNodeId(srcId);
        }

        // Clear all steps to inactive
        for (uint32_t s = 0; s < totalArrangerSteps; ++s) {
            sequencer::StepData sd{};
            sd.active = false;
            seqTrack->setStep(s, sd);
        }

        const auto& arrTrack = tracks[t];
        std::map<uint32_t, std::vector<ArrangerClipNote>> stepBuckets;

        for (const auto& clip : arrTrack.clips) {
            if (clip.mute) continue;

            uint32_t clipStartStep = (clip.startBar > 0 ? (clip.startBar - 1) : 0) * 16;
            uint32_t clipLengthSteps = clip.lengthBars * 16;
            uint32_t clipEndStep = clipStartStep + clipLengthSteps;

            uint32_t loopLenBars = clip.isLooped ? (clip.loopLengthBars > 0 ? clip.loopLengthBars : clip.lengthBars) : clip.lengthBars;
            if (loopLenBars == 0) loopLenBars = 1;
            uint32_t loopLenSteps = loopLenBars * 16;

            for (uint32_t cycleStart = clipStartStep; cycleStart < clipEndStep; cycleStart += loopLenSteps) {
                uint32_t cycleEnd = std::min(cycleStart + loopLenSteps, clipEndStep);
                for (const auto& cn : clip.notes) {
                    uint32_t noteStep = cycleStart + static_cast<uint32_t>(std::round(cn.startBeat * 4.0f));
                    if (noteStep >= cycleStart && noteStep < cycleEnd && noteStep < totalArrangerSteps) {
                        stepBuckets[noteStep].push_back(cn);
                    }
                }
            }
        }

        // Apply grouped notes to sequencer track
        for (const auto& [s, noteList] : stepBuckets) {
            if (noteList.empty() || s >= totalArrangerSteps) continue;
            sequencer::StepData sd{};
            sd.active = true;
            sd.note = noteList[0].pitch;
            sd.velocity = std::clamp(noteList[0].velocity, 0.05f, 1.0f);
            float maxBeats = 0.0f;
            for (const auto& n : noteList) {
                maxBeats = std::max(maxBeats, n.lengthBeats);
            }
            sd.gateLength = std::clamp(maxBeats * 4.0f, 0.1f, 128.0f);
            for (size_t i = 1; i < noteList.size(); ++i) {
                sd.extraNotes.push_back(noteList[i].pitch);
            }
            seqTrack->setStep(s, sd);
        }

        seqTrack->setChordFollowMode(arrTrack.chordFollowMode);
        syncTrackMidiFxToEngine(t);
    }
    seq.setChordTrack(modularArrangerView_->getChordTrack());
}

void GuiWindow::syncArrangerFromSequencer() {
    if (!engine_ || !modularArrangerView_) return;

    auto& seq = engine_->getSequencer();
    if (seq.getNumPatterns() == 0) {
        initArrangerTracks();
        return;
    }

    const auto& pat = seq.getPattern(0);
    if (pat.tracks.empty()) {
        initArrangerTracks();
        return;
    }

    arrangerTracks_.clear();
    auto& arrangerTracks = modularArrangerView_->getTracks();
    arrangerTracks.clear();

    struct TrackStyleInfo {
        float r, g, b;
        const char* icon;
        const char* instrument;
        const char* engine;
    };
    static const TrackStyleInfo kStyles[] = {
        {1.0f,  0.55f, 0.0f,  "preset:inst_synth",   "Roland TB-303",        "tb303"},       // Amber
        {0.13f, 0.96f, 0.91f, "preset:drum_machine", "Analog 808",           "tr808"},       // Cyan
        {1.0f,  0.16f, 0.43f, "preset:drum_kick",    "Analog 909",           "tr909"},       // Pink/Crimson
        {0.62f, 0.31f, 0.87f, "preset:inst_piano",   "Yamaha DX7 6-Op FM",   "dx7"},         // Purple
        {0.88f, 0.66f, 0.43f, "preset:inst_piano",   "Waveguide Grand Piano","piano"},       // Gold
        {0.20f, 0.85f, 0.35f, "preset:inst_keys",    "Polyphonic Synth",     "poly_synth"},  // Emerald
        {0.25f, 0.65f, 1.0f,  "preset:audio_filter", "Bass Synthesizer",     "synth"},       // Sky Blue
        {1.0f,  0.85f, 0.20f, "preset:fx_reverb",    "EatScript Sound FX",   "eatscript"},   // Yellow
    };
    constexpr size_t numStyles = sizeof(kStyles) / sizeof(kStyles[0]);

    uint32_t maxSteps = 16;

    for (size_t t = 0; t < pat.tracks.size(); ++t) {
        const auto& seqTrk = pat.tracks[t];
        ArrangerTimelineTrack tArr;
        tArr.name = seqTrk.getName().empty() ? ("Track " + std::to_string(t + 1)) : seqTrk.getName();
        tArr.volume = seqTrk.getVolume();
        tArr.pan = seqTrk.getPan();
        tArr.mute = seqTrk.isMuted();
        tArr.solo = seqTrk.isSolo();
        tArr.freeze = seqTrk.isFrozen();
        tArr.chordFollowMode = seqTrk.getChordFollowMode();

        // Style matching based on name and target node
        std::string lower = tArr.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        size_t styleIdx = t % numStyles;
        if (lower.find("303") != std::string::npos || lower.find("acid") != std::string::npos || lower.find("bass") != std::string::npos) {
            styleIdx = 0;
        } else if (lower.find("808") != std::string::npos || lower.find("kick") != std::string::npos || lower.find("bd") != std::string::npos) {
            styleIdx = 1;
        } else if (lower.find("909") != std::string::npos || lower.find("drum") != std::string::npos || lower.find("hat") != std::string::npos || lower.find("snare") != std::string::npos || lower.find("clap") != std::string::npos) {
            styleIdx = 2;
        } else if (lower.find("dx7") != std::string::npos || lower.find("rhodes") != std::string::npos || lower.find("ep") != std::string::npos || lower.find("keys") != std::string::npos) {
            styleIdx = 3;
        } else if (lower.find("piano") != std::string::npos || lower.find("grand") != std::string::npos || lower.find("guitar") != std::string::npos || lower.find("string") != std::string::npos) {
            styleIdx = 4;
        }

        const auto& st = kStyles[styleIdx];
        tArr.r = st.r;
        tArr.g = st.g;
        tArr.b = st.b;
        tArr.iconRef = seqTrk.getIconRef().empty() ? st.icon : seqTrk.getIconRef();
        tArr.instrument = st.instrument;
        tArr.instrumentEngine = st.engine;

        // Create timeline clip from steps
        uint32_t trkSteps = std::max(16u, seqTrk.getNumSteps());
        if (trkSteps > maxSteps) maxSteps = trkSteps;
        uint32_t lengthBars = std::max(1u, (trkSteps + 15) / 16);

        ArrangerTimelineClip clip;
        clip.id = "clip_" + std::to_string(t) + "_0";
        clip.name = tArr.name + " Pattern";
        clip.trackIndex = static_cast<uint32_t>(t);
        clip.startBar = 1;
        clip.lengthBars = lengthBars;
        clip.r = tArr.r;
        clip.g = tArr.g;
        clip.b = tArr.b;
        clip.isSelected = (t == 0);
        clip.isAudio = false;
        clip.isLooped = false;
        clip.loopLengthBars = lengthBars;
        clip.volumeScale = 1.0f;
        clip.mute = tArr.mute;

        for (uint32_t s = 0; s < seqTrk.getNumSteps(); ++s) {
            const auto& step = seqTrk.getStep(s);
            if (!step.active) continue;

            ArrangerClipNote note;
            note.pitch = step.note;
            note.startBeat = static_cast<float>(s) * 0.25f; // 16 steps = 4 beats = 1 bar
            note.lengthBeats = std::clamp(step.gateLength * 0.25f, 0.05f, 64.0f);
            note.velocity = std::clamp(step.velocity, 0.05f, 1.0f);
            clip.notes.push_back(note);

            for (uint8_t ex : step.extraNotes) {
                ArrangerClipNote exNote = note;
                exNote.pitch = ex;
                clip.notes.push_back(exNote);
            }
        }

        modularArrangerView_->updateClipDetectedChords(clip);
        tArr.clips.push_back(clip);

        // Populate legacy ArrangerTrackData
        ArrangerTrackData legTrk;
        legTrk.name = tArr.name;
        legTrk.type = (tArr.instrumentEngine == "tr808" || tArr.instrumentEngine == "tr909") ? "SAMPLER" : "SYNTH";
        legTrk.r = tArr.r; legTrk.g = tArr.g; legTrk.b = tArr.b;
        legTrk.volume = tArr.volume;
        legTrk.pan = tArr.pan;
        legTrk.mute = tArr.mute;
        legTrk.solo = tArr.solo;
        legTrk.freeze = tArr.freeze;
        legTrk.chordFollowMode = tArr.chordFollowMode;

        ArrangerClip legClip;
        legClip.name = clip.name;
        legClip.startBar = clip.startBar;
        legClip.barLength = clip.lengthBars;
        legClip.r = clip.r; legClip.g = clip.g; legClip.b = clip.b;
        legClip.isLooped = clip.isLooped;
        legClip.loopLengthBars = clip.loopLengthBars;
        legTrk.clips.push_back(legClip);
        arrangerTracks_.push_back(legTrk);

        arrangerTracks.push_back(tArr);
    }

    seq.setChordTrack(modularArrangerView_->getChordTrack());

    selectedTrackIndex_ = 0;
    modularArrangerView_->setActiveTrack(0);
    modularArrangerView_->setSelectedClip(0);
    syncActiveClipToEditView(0, 0);

    seq.getTransport().setMaxSteps(std::max(16u, maxSteps));
    seq.getTransport().setPosition(0);

    mixerStrips_.clear();
    updateMixerStrips();
}

void GuiWindow::syncTrackAudioFxToEngine(uint32_t trackIdx) {
    if (!engine_ || !modularArrangerView_) return;

    // Bridge Inspector, Mixer, and Arranger Audio FX lists
    if (modularTrackInspectorView_ && trackIdx < modularTrackInspectorView_->getTracks().size() && trackIdx < modularArrangerView_->getTracks().size()) {
        modularTrackInspectorView_->getTracks()[trackIdx].audioFxList = modularArrangerView_->getTracks()[trackIdx].audioFx;
    }
    if (modularMixerView_ && trackIdx < modularMixerView_->getChannels().size() && trackIdx < modularArrangerView_->getTracks().size()) {
        modularMixerView_->getChannels()[trackIdx].audioFx = modularArrangerView_->getTracks()[trackIdx].audioFx;
    }

    const auto& tracks = modularArrangerView_->getTracks();
    if (trackIdx >= tracks.size()) return;

    const auto& trk = tracks[trackIdx];
    std::vector<eatsbits::audio::AudioEngine::TrackAudioFxItem> engineFxList;
    engineFxList.reserve(trk.audioFx.size());
    for (const auto& fx : trk.audioFx) {
        eatsbits::audio::AudioEngine::TrackAudioFxItem item;
        item.name = fx.name;
        item.type = fx.type;
        item.drive = fx.drive;
        item.mix = fx.mix;
        item.enabled = fx.enabled;
        item.id = fx.id;
        engineFxList.push_back(item);
    }
    engine_->rebuildTrackAudioFx(trackIdx, engineFxList);
}

void GuiWindow::syncTrackMidiFxToEngine(uint32_t trackIdx) {
    if (!engine_ || !modularArrangerView_) return;

    // Bridge Inspector, Mixer, and Arranger MIDI FX lists
    if (modularTrackInspectorView_ && trackIdx < modularTrackInspectorView_->getTracks().size() && trackIdx < modularArrangerView_->getTracks().size()) {
        modularTrackInspectorView_->getTracks()[trackIdx].midiFxList = modularArrangerView_->getTracks()[trackIdx].midiFx;
    }
    if (modularMixerView_ && trackIdx < modularMixerView_->getChannels().size() && trackIdx < modularArrangerView_->getTracks().size()) {
        modularMixerView_->getChannels()[trackIdx].midiFx = modularArrangerView_->getTracks()[trackIdx].midiFx;
    }

    const auto& tracks = modularArrangerView_->getTracks();
    if (trackIdx >= tracks.size()) return;

    auto* seqTrack = engine_->getSequencer().getTrack(trackIdx);
    if (!seqTrack) return;

    auto& trk = modularArrangerView_->getTracks()[trackIdx];
    if (trackIdx < arrangerTracks_.size()) {
        auto& legacyFx = arrangerTracks_[trackIdx].midiFx;
        legacyFx.scaleSnapEnabled = false;
        legacyFx.arpEnabled = false;
        legacyFx.humanizeEnabled = false;
    }

    std::vector<eatsbits::eatscript::MidiFxInsert> rack;
    rack.reserve(trk.midiFx.size());
    for (size_t i = 0; i < trk.midiFx.size(); ++i) {
        const auto& fx = trk.midiFx[i];
        eatsbits::eatscript::MidiFxInsert insert;
        insert.id = "mfx_" + std::to_string(i);
        insert.name = fx.name;
        insert.enabled = fx.enabled;
        insert.type = eatsbits::eatscript::MidiPipelineEngine::detectMidiFxType(fx.id, fx.name + " " + fx.type);
        if (trackIdx < arrangerTracks_.size()) {
            auto& legacyFx = arrangerTracks_[trackIdx].midiFx;
            if ((insert.type == eatsbits::eatscript::MidiFxType::Arpeggiator ||
                 insert.type == eatsbits::eatscript::MidiFxType::ChordArp) && fx.enabled) {
                legacyFx.arpEnabled = true;
            } else if (insert.type == eatsbits::eatscript::MidiFxType::ScaleSnap && fx.enabled) {
                legacyFx.scaleSnapEnabled = true;
            } else if (insert.type == eatsbits::eatscript::MidiFxType::Humanize && fx.enabled) {
                legacyFx.humanizeEnabled = true;
            }
        }
        insert.params["rootKey"] = static_cast<float>(fx.rootKey);
        insert.params["root"] = static_cast<float>(fx.rootKey);
        insert.params["scaleMode"] = static_cast<float>(fx.scaleMode);
        insert.params["scale"] = static_cast<float>(fx.scaleMode);

        // Serialize full parameter set from knobs
        for (const auto& knob : fx.knobs) {
            insert.params[knob.name] = knob.value;
            std::string kLower = knob.name;
            for (char& c : kLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            if (kLower == "octaves" || kLower == "oct") {
                int oct = std::clamp(static_cast<int>(std::round(knob.value * 3.0f)) + 1, 1, 4);
                insert.params["Octaves"] = static_cast<float>(oct);
                insert.params["octaves"] = static_cast<float>(oct);
            } else if (kLower == "semitones" || kLower == "transpose" || kLower == "trans") {
                int semi = static_cast<int>(std::round((knob.value - 0.5f) * 24.0f));
                insert.params["semitones"] = static_cast<float>(semi);
                insert.params["transpose"] = static_cast<float>(semi);
            } else if (kLower == "root" || kLower == "rootkey") {
                int rk = std::clamp(static_cast<int>(std::round(knob.value * 11.0f)), 0, 11);
                insert.params["rootKey"] = static_cast<float>(rk);
                insert.params["root"] = static_cast<float>(rk);
            } else if (kLower == "scale" || kLower == "scalemode") {
                int sm = std::clamp(static_cast<int>(std::round(knob.value * 6.0f)), 0, 6);
                insert.params["scaleMode"] = static_cast<float>(sm);
                insert.params["scale"] = static_cast<float>(sm);
            } else if (kLower == "voicing") {
                insert.params["voicing"] = knob.value;
            } else if (kLower == "inversion") {
                insert.params["inversion"] = knob.value;
            } else if (kLower == "pattern") {
                int pat = std::clamp(static_cast<int>(std::round(knob.value * 8.0f)), 0, 8);
                insert.params["Pattern"] = static_cast<float>(pat);
                insert.params["pattern"] = static_cast<float>(pat);
            } else if (kLower == "rate") {
                int rIdx = std::clamp(static_cast<int>(std::round(knob.value * 6.0f)), 0, 6);
                static const float kRateSteps[] = {4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.05f};
                static const char* kRateDisplays[] = {"1/4", "1/8", "1/16", "1/32", "1/64", "1/128", "50Hz (C64)"};
                float rateSteps = kRateSteps[rIdx];
                const_cast<TrackPropertiesKnob&>(knob).display = kRateDisplays[rIdx];
                insert.params["Rate"] = rateSteps;
                insert.params["rate"] = rateSteps;
            } else if (kLower == "gate") {
                float g = std::clamp(0.1f + knob.value * 1.9f, 0.1f, 2.0f);
                insert.params["Gate"] = g;
                insert.params["gate"] = g;
            } else if (kLower == "swing") {
                float sw = knob.value * 0.5f;
                insert.params["Swing"] = sw;
                insert.params["swing"] = sw;
            } else if (kLower == "velocity") {
                insert.params["velocity"] = knob.value;
                insert.params["humanizeVelocity"] = knob.value;
            } else if (kLower == "timing") {
                insert.params["timing"] = knob.value;
                insert.params["humanizeTiming"] = knob.value;
            }
        }
        rack.push_back(std::move(insert));
    }
    seqTrack->setMidiFxRack(std::move(rack));

    if (modularEditView_ && selectedTrackIndex_ == trackIdx) {
        modularEditView_->setMidiFxRack(seqTrack->getMidiFxRack());
    }
}

void GuiWindow::syncAllTracksFxToEngine() {
    if (!engine_ || !modularArrangerView_) return;
    const auto& tracks = modularArrangerView_->getTracks();
    for (uint32_t i = 0; i < tracks.size(); ++i) {
        syncTrackAudioFxToEngine(i);
        syncTrackMidiFxToEngine(i);
    }
}

void GuiWindow::bakeTrackChordsAndFx(uint32_t trackIdx) {
    if (!engine_ || !modularArrangerView_) return;
    auto& arrTracks = modularArrangerView_->getTracks();
    if (trackIdx >= arrTracks.size()) return;

    auto& arrTrk = arrTracks[trackIdx];
    auto* seqTrk = engine_->getSequencer().getTrack(trackIdx);

    // 1. If modularEditView_ is viewing this track, bake its notes
    if (modularEditView_ && selectedTrackIndex_ == trackIdx) {
        modularEditView_->setChordFollowMode(arrTrk.chordFollowMode);
        modularEditView_->setChordTrack(modularArrangerView_->getChordTrack());
        modularEditView_->bakeChordFollowAndMidiFx();

        int cIdx = modularArrangerView_->getSelectedClipIndex();
        if (cIdx >= 0 && static_cast<size_t>(cIdx) < arrTrk.clips.size()) {
            modularEditView_->writeBackToArrangerClip(arrTrk.clips[cIdx]);
            modularArrangerView_->updateClipDetectedChords(arrTrk.clips[cIdx]);
        }
    } else {
        // Bake across all clips on the track using ArrangerView & MidiPipelineEngine
        if (arrTrk.chordFollowMode != theory::ChordFollowMode::Off) {
            modularArrangerView_->bakeChordsToTrack(trackIdx);
        }
        bool hasMidiFx = false;
        for (const auto& fx : arrTrk.midiFx) {
            if (fx.enabled) { hasMidiFx = true; break; }
        }
        if (hasMidiFx) {
            eatscript::TimeContext timeCtx{};
            timeCtx.bpm = engine_->getSequencer().getTransport().getBpm();
            if (!modularArrangerView_->getChordTrack().empty()) {
                const auto& ch = modularArrangerView_->getChordTrack().front();
                timeCtx.activeChordRoot = ch.rootPitchClass;
                timeCtx.activeChordQuality = theory::getChordQualityDisplayName(ch.quality);
                timeCtx.chordPitchClasses = ch.getPitchClasses();
                timeCtx.bassPitchClass = ch.bassPitchClass >= 0 ? ch.bassPitchClass : ch.rootPitchClass;
            }
            std::vector<eatscript::MidiFxInsert> rack;
            for (const auto& fx : arrTrk.midiFx) {
                if (!fx.enabled) continue;
                eatscript::MidiFxInsert ins;
                ins.name = fx.name;
                ins.type = eatscript::MidiPipelineEngine::detectMidiFxType(fx.id, fx.name + " " + fx.type);
                ins.enabled = true;
                ins.params["rootKey"] = static_cast<float>(fx.rootKey);
                ins.params["scaleMode"] = static_cast<float>(fx.scaleMode);
                for (const auto& k : fx.knobs) ins.params[k.name] = k.value;
                rack.push_back(ins);
            }
            for (auto& clip : arrTrk.clips) {
                std::vector<eatscript::MidiNote> mNotes;
                for (size_t ci = 0; ci < clip.notes.size(); ++ci) {
                    eatscript::MidiNote mn;
                    mn.id = "c_" + std::to_string(ci);
                    mn.pitch = clip.notes[ci].pitch;
                    mn.startStep = clip.notes[ci].startBeat * 4.0f;
                    mn.durationSteps = clip.notes[ci].lengthBeats * 4.0f;
                    mn.velocity = clip.notes[ci].velocity;
                    mNotes.push_back(mn);
                }
                auto transformed = eatscript::MidiPipelineEngine::processPipeline(rack, mNotes, timeCtx);
                clip.notes.clear();
                for (const auto& tn : transformed) {
                    ArrangerClipNote an;
                    an.pitch = tn.pitch;
                    an.startBeat = static_cast<float>(tn.startStep) / 4.0f;
                    an.lengthBeats = static_cast<float>(tn.durationSteps) / 4.0f;
                    an.velocity = static_cast<float>(tn.velocity);
                    clip.notes.push_back(an);
                }
                modularArrangerView_->updateClipDetectedChords(clip);
            }
        }
    }

    // 2. Reset Chord Follow Mode to Off
    arrTrk.chordFollowMode = theory::ChordFollowMode::Off;
    if (trackIdx < arrangerTracks_.size()) {
        arrangerTracks_[trackIdx].chordFollowMode = theory::ChordFollowMode::Off;
    }
    if (modularTrackInspectorView_ && trackIdx < modularTrackInspectorView_->getTracks().size()) {
        modularTrackInspectorView_->getTracks()[trackIdx].chordFollowMode = theory::ChordFollowMode::Off;
    }
    if (seqTrk) {
        seqTrk->setChordFollowMode(theory::ChordFollowMode::Off);
    }

    // 3. Disable MIDI FX rack units
    for (auto& fx : arrTrk.midiFx) fx.enabled = false;
    if (modularTrackInspectorView_ && trackIdx < modularTrackInspectorView_->getTracks().size()) {
        for (auto& fx : modularTrackInspectorView_->getTracks()[trackIdx].midiFxList) fx.enabled = false;
    }
    if (modularMixerView_ && trackIdx < modularMixerView_->getChannels().size()) {
        for (auto& fx : modularMixerView_->getChannels()[trackIdx].midiFx) fx.enabled = false;
    }
    syncTrackMidiFxToEngine(trackIdx);

    // 4. Sync Arranger to Sequencer to update playback steps
    syncArrangerToSequencer();

    // 5. Update Track's Script content (the source of truth for note data)
    if (seqTrk) {
        std::string script = eatscript::NoteScriptEngine::serializeTrackNotes(*seqTrk);
        seqTrk->setEatscriptCode(script);
        if (selectedTrackIndex_ == trackIdx) {
            setScriptCode(script);
        }
    }

    // 6. Refresh EditView with baked notes if active
    if (modularEditView_ && selectedTrackIndex_ == trackIdx) {
        syncActiveClipToEditView(trackIdx, modularArrangerView_->getSelectedClipIndex());
    }

    recordProjectHistory("Bake Chords & MIDI FX on Track " + std::to_string(trackIdx + 1), "CHORD");
    setStatusMessage("Harmonic chords & MIDI FX baked into Track " + std::to_string(trackIdx + 1) + " Script");
}

void GuiWindow::auditionTrackNote(uint32_t trackIdx, uint8_t note, float vel) {
    if (!engine_) return;

    bool hasArp = false;
    bool isChordArp = false;
    int arpOctaves = 2;
    std::string arpPattern = "up";
    float arpRate = 1.0f; // 1.0 = 16th note (~110ms)
    float arpGate = 0.85f;

    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        for (const auto& fx : modularArrangerView_->getTracks()[trackIdx].midiFx) {
            if (!fx.enabled) continue;
            auto fxType = eatsbits::eatscript::MidiPipelineEngine::detectMidiFxType(fx.id, fx.name + " " + fx.type);
            if (fxType == eatsbits::eatscript::MidiFxType::Arpeggiator ||
                fxType == eatsbits::eatscript::MidiFxType::ChordArp) {
                hasArp = true;
                if (fxType == eatsbits::eatscript::MidiFxType::ChordArp) isChordArp = true;

                for (const auto& k : fx.knobs) {
                    std::string kLower = k.name;
                    for (char& c : kLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (kLower == "octaves" || kLower == "oct") {
                        arpOctaves = std::clamp(static_cast<int>(std::round(k.value * 3.0f)) + 1, 1, 4);
                    } else if (kLower == "rate") {
                        int rIdx = std::clamp(static_cast<int>(std::round(k.value * 6.0f)), 0, 6);
                        static const float kRateSteps[] = {4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.05f};
                        arpRate = kRateSteps[rIdx];
                    } else if (kLower == "pattern") {
                        int pInt = std::clamp(static_cast<int>(std::round(k.value * 8.0f)), 0, 8);
                        static const char* kPats[] = {"up", "down", "updown", "downup", "converge", "diverge", "random", "chord", "asplayed"};
                        arpPattern = kPats[pInt];
                    } else if (kLower == "gate") {
                        arpGate = std::clamp(0.1f + k.value * 1.9f, 0.1f, 1.0f);
                    }
                }
            }
        }
    }

    if (hasArp) {
        std::vector<int> basePitches;
        basePitches.push_back(note);
        if (isChordArp) {
            basePitches.push_back(std::clamp(note + 4, 0, 127));
            basePitches.push_back(std::clamp(note + 7, 0, 127));
        }
        std::sort(basePitches.begin(), basePitches.end());

        std::vector<int> multiOctave;
        for (int oct = 0; oct < arpOctaves; ++oct) {
            for (int p : basePitches) {
                multiOctave.push_back(std::clamp(p + oct * 12, 0, 127));
            }
        }

        auto patterned = eatsbits::eatscript::MidiPipelineEngine::generatePatternSequence(multiOctave, arpPattern);
        std::vector<uint8_t> arpNotes;
        for (int p : patterned) arpNotes.push_back(static_cast<uint8_t>(p));

        if (arpNotes.size() > 1) {
            int stepMs = (arpRate <= 0.05f) ? 20 : std::clamp(static_cast<int>(std::round(125.0f * arpRate)), 18, 600);
            int onMs = std::clamp(static_cast<int>(std::round(stepMs * arpGate)), 12, stepMs - 4);
            int offMs = std::max(4, stepMs - onMs);

            std::thread([eng = this->engine_, tIdx = static_cast<int>(trackIdx), sequence = std::move(arpNotes), vel, onMs, offMs]() {
                size_t totalNotes = std::min<size_t>(sequence.size() * 3, 24);
                for (size_t i = 0; i < totalNotes; ++i) {
                    uint8_t p = sequence[i % sequence.size()];
                    eng->postNoteOn(p, vel, false, false, tIdx, false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(onMs));
                    eng->postNoteOff(p, tIdx, false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(offMs));
                }
            }).detach();
            return;
        }
    }

    engine_->postNoteOn(note, vel, false, false, static_cast<int>(trackIdx), true);
    std::thread([eng = this->engine_, tIdx = static_cast<int>(trackIdx), n = note]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(280));
        eng->postNoteOff(n, tIdx, true);
    }).detach();
}

void GuiWindow::stopChordAudition() noexcept {
    if (!auditionChordNotes_.empty() && engine_) {
        uint32_t tIdx = modularArrangerView_ ? modularArrangerView_->getActiveTrackIndex() : selectedTrackIndex_;
        for (uint8_t note : auditionChordNotes_) {
            engine_->postNoteOff(note, static_cast<int>(tIdx), false);
        }
        auditionChordNotes_.clear();
    }
}

void GuiWindow::syncTrackToPreset(uint32_t trackIndex) {
    selectedTrackIndex_ = trackIndex;
    if (engine_) {
        engine_->setActiveTrack(trackIndex);
    }
    if (engine_ && engine_->getTrackClapNode(trackIndex)) {
        // Track uses a CLAP plugin; do not force-map to built-in presets (e.g. 808/303)
        return;
    }
    if (presets_.empty()) return;

    std::string targetKeyword;
    if (trackIndex < arrangerTracks_.size()) {
        const auto& at = arrangerTracks_[trackIndex];
        const std::string& eng = at.instrumentEngine;
        const std::string& inst = at.instrument;
        const std::string& name = at.name;
        if (eng == "tb303" || inst.find("303") != std::string::npos || name.find("303") != std::string::npos) targetKeyword = "303";
        else if (eng == "tr808" || inst.find("808") != std::string::npos || name.find("808") != std::string::npos) targetKeyword = "808";
        else if (eng == "tr909" || inst.find("909") != std::string::npos || name.find("909") != std::string::npos) targetKeyword = "909";
        else if (eng == "dx7" || inst.find("DX7") != std::string::npos || name.find("DX7") != std::string::npos) targetKeyword = "dx7";
        else if (eng == "piano" || inst.find("Piano") != std::string::npos || name.find("Grand") != std::string::npos) targetKeyword = "piano";
        else if (eng.find("astral") != std::string::npos || inst.find("astral") != std::string::npos || name.find("astral") != std::string::npos ||
                 eng.find("Astral") != std::string::npos || inst.find("Astral") != std::string::npos || name.find("Astral") != std::string::npos ||
                 eng.find("shimmer") != std::string::npos || inst.find("shimmer") != std::string::npos || name.find("Shimmer") != std::string::npos) targetKeyword = "astral";
        else if (!eng.empty() && eng != "synth") targetKeyword = eng;
        else if (!inst.empty() && inst != "synth") targetKeyword = inst;
    }
    if (targetKeyword.empty() && engine_ && trackIndex < engine_->getSequencer().getNumTracks()) {
        auto* trk = engine_->getSequencer().getTrack(trackIndex);
        if (trk) {
            const std::string& name = trk->getName();
            if (name.find("303") != std::string::npos || name.find("Acid") != std::string::npos) targetKeyword = "303";
            else if (name.find("808") != std::string::npos || name.find("Drum") != std::string::npos) targetKeyword = "808";
            else if (name.find("909") != std::string::npos) targetKeyword = "909";
            else if (name.find("Bass") != std::string::npos || name.find("Sub") != std::string::npos) targetKeyword = "bass";
            else if (name.find("Lead") != std::string::npos || name.find("Poly") != std::string::npos || name.find("DX7") != std::string::npos) targetKeyword = "dx7";
            else if (name.find("Master") != std::string::npos || name.find("Delay") != std::string::npos || name.find("Echo") != std::string::npos) targetKeyword = "delay";

            if (targetKeyword.empty()) {
                audio::NodeId nId = trk->getTargetNodeId();
                auto nodePtr = engine_->getGraph().getNode(nId);
                if (nodePtr) {
                    if (dynamic_cast<audio::Tb303Node*>(nodePtr.get())) targetKeyword = "303";
                    else if (dynamic_cast<audio::ClavinetNode*>(nodePtr.get())) targetKeyword = "clavinet";
                    else if (auto* dk = dynamic_cast<audio::DrumKitNode*>(nodePtr.get())) {
                        targetKeyword = (dk->getKitType() == audio::DrumKitType::TR909) ? "909" : "808";
                    }
                    else if (dynamic_cast<audio::DelayNode*>(nodePtr.get())) targetKeyword = "delay";
                    else if (dynamic_cast<audio::PolySynthNode*>(nodePtr.get())) targetKeyword = "dx7";
                    else if (dynamic_cast<audio::PhysicalInstrumentNode*>(nodePtr.get())) targetKeyword = "piano";
                }
            }
        }
    }
    if (targetKeyword.empty()) {
        if (trackIndex == 0) targetKeyword = "303";
        else if (trackIndex == 1) targetKeyword = "808";
        else if (trackIndex == 2) targetKeyword = "909";
        else if (trackIndex == 3) targetKeyword = "dx7";
        else if (trackIndex == 4) targetKeyword = "piano";
        else {
            if (trackIndex < arrangerTracks_.size()) {
                targetKeyword = arrangerTracks_[trackIndex].instrumentEngine;
                if (targetKeyword.empty()) targetKeyword = arrangerTracks_[trackIndex].instrument;
            }
            if (targetKeyword.empty()) targetKeyword = "synth";
        }
    }

    for (size_t i = 0; i < presets_.size(); ++i) {
        const auto& meta = presets_[i].metadata;
        if (meta.id.find(targetKeyword) != std::string::npos ||
            meta.engineId.find(targetKeyword) != std::string::npos ||
            meta.name.find(targetKeyword) != std::string::npos) {
            activePresetIndex_ = i;
            presets_[activePresetIndex_].compactGuiRoot = presets_[activePresetIndex_].guiRoot;
            project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].compactGuiRoot, 0.0f, 0.0f, 520.0f, 210.0f);
            project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                                       40.0f, 98.0f,
                                                       static_cast<float>(width_) - 80.0f,
                                                       static_cast<float>(height_) - 154.0f);
            break;
        }
    }

    if (engine_ && activePresetIndex_ < presets_.size()) {
        auto* trk = engine_->getSequencer().getTrack(trackIndex);
        if (trk && !presets_[activePresetIndex_].rawScript.empty()) {
            trk->setEatscriptCode(presets_[activePresetIndex_].rawScript);
        }
    }
    if (designSubView_ == DesignSubView::Eatscript && activePresetIndex_ < presets_.size() && !presets_[activePresetIndex_].rawScript.empty()) {
        setScriptCode(presets_[activePresetIndex_].rawScript);
    }
}

void GuiWindow::setMasterVolume(float vol) noexcept {
    masterVolume_ = std::clamp(vol, 0.0f, 1.5f);
    if (engine_) {
        engine_->setMasterVolume(masterVolume_);
    }
}

const project::PresetDefinition* GuiWindow::getActivePreset() const noexcept {
    if (presets_.empty() || activePresetIndex_ >= presets_.size()) return nullptr;
    return &presets_[activePresetIndex_];
}

project::PresetDefinition* GuiWindow::getActivePreset() noexcept {
    if (presets_.empty() || activePresetIndex_ >= presets_.size()) return nullptr;
    return &presets_[activePresetIndex_];
}

HitTestHardwareKnobResult GuiWindow::hitTestHardwareKnob(float x, float y, float faceplateX, float faceplateY, float scale) const noexcept {
    HitTestHardwareKnobResult res{};
    const auto* preset = getActivePreset();
    if (!preset) return res;
    hitTestNodeRecursive(preset->guiRoot, x, y, *preset, res, faceplateX, faceplateY, scale);
    return res;
}

void GuiWindow::dispatchHardwareParam(const std::string& paramName, float normVal) noexcept {
    dispatchTrackParam(selectedTrackIndex_, paramName, normVal);
}

void GuiWindow::dispatchTrackParam(uint32_t trackIndex, const std::string& paramName, float normVal) noexcept {
    if (!engine_) return;

    // 1. Check if track uses a CLAP plugin
    auto clapNode = engine_->getTrackClapNode(trackIndex);
    if (clapNode) {
        const auto& params = clapNode->getParameterList();
        for (const auto& p : params) {
            if (p.name == paramName) {
                double realVal = p.minValue + static_cast<double>(normVal) * (p.maxValue - p.minValue);
                clapNode->setParamValue(p.id, realVal);
                return;
            }
        }
        if (paramName.rfind("param", 0) == 0 && paramName.size() > 5) {
            try {
                size_t pIdx = std::stoul(paramName.substr(5)) - 1;
                if (pIdx < params.size()) {
                    const auto& p = params[pIdx];
                    double realVal = p.minValue + static_cast<double>(normVal) * (p.maxValue - p.minValue);
                    clapNode->setParamValue(p.id, realVal);
                    return;
                }
            } catch (...) {}
        }
        std::string lowerParam = paramName;
        for (char& c : lowerParam) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (const auto& p : params) {
            std::string pLow = p.name;
            for (char& c : pLow) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (pLow.find(lowerParam) != std::string::npos) {
                double realVal = p.minValue + static_cast<double>(normVal) * (p.maxValue - p.minValue);
                clapNode->setParamValue(p.id, realVal);
                return;
            }
        }
        return;
    }

    // 2. Locate active or track-associated preset and update parameter if matched
    project::PresetDefinition* preset = (trackIndex == selectedTrackIndex_) ? getActivePreset() : nullptr;
    project::PresetParam* p = nullptr;
    if (preset) {
        p = preset->findParam(paramName);
        if (!p) {
            std::string lowerParam = paramName;
            for (char& c : lowerParam) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lowerParam == "tuning") lowerParam = "tune";
            else if (lowerParam == "bright") lowerParam = "brightness";
            else if (lowerParam == "snappy" || lowerParam == "punch") lowerParam = "drive";

            for (auto& [k, v] : preset->params) {
                std::string lk = k;
                for (char& c : lk) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (lk == lowerParam) {
                    p = &v;
                    break;
                }
            }
        }
        if (p) {
            if (normVal > 1.0f) {
                p->currentVal = std::clamp(normVal, p->minVal, p->maxVal);
            } else {
                p->setNormalized(normVal);
            }
        }
    }

    // 3. Resolve the target NodeId on this track
    audio::NodeId srcId = engine_->getTrackSourceNodeId(trackIndex);
    auto nodePtr = (srcId != audio::INVALID_NODE_ID) ? engine_->getGraph().getNode(srcId) : nullptr;

    std::string normKey = paramName;
    for (char& c : normKey) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    auto postParam = [&](uint32_t paramId, float realValue, std::string_view moduleType = "") {
        if (srcId != audio::INVALID_NODE_ID) {
            engine_->postNodeParameter(srcId, paramId, realValue);
        }
        if (!moduleType.empty()) {
            for (const auto& m : canvas_.getModules()) {
                if (m.type == moduleType || m.name.find(std::string(moduleType)) != std::string::npos) {
                    engine_->postNodeParameter(m.id, paramId, realValue);
                }
            }
        }
    };

    // 4. Identify engine type: first from node dynamic_cast, then fallback to preset / arranger track
    bool isSnes = (std::dynamic_pointer_cast<audio::SnesNode>(nodePtr) != nullptr);
    bool isSid = (std::dynamic_pointer_cast<audio::SidNode>(nodePtr) != nullptr);
    bool isDx7 = (std::dynamic_pointer_cast<audio::Dx7Node>(nodePtr) != nullptr);
    bool isYm2612 = (std::dynamic_pointer_cast<audio::Ym2612Node>(nodePtr) != nullptr);
    bool isDrums = (std::dynamic_pointer_cast<audio::DrumKitNode>(nodePtr) != nullptr);
    bool isTb303 = (std::dynamic_pointer_cast<audio::Tb303Node>(nodePtr) != nullptr);
    bool isPoly = (std::dynamic_pointer_cast<audio::PolySynthNode>(nodePtr) != nullptr);
    bool isPhys = (std::dynamic_pointer_cast<audio::PhysicalInstrumentNode>(nodePtr) != nullptr);
    bool isClav = (std::dynamic_pointer_cast<audio::ClavinetNode>(nodePtr) != nullptr);

    if (!isSnes && !isSid && !isDx7 && !isYm2612 && !isDrums && !isTb303 && !isPoly && !isPhys && !isClav) {
        std::string engHint;
        if (preset) {
            engHint = preset->metadata.engineId + " " + preset->metadata.id;
        }
        if (trackIndex < arrangerTracks_.size()) {
            engHint += " " + arrangerTracks_[trackIndex].instrumentEngine + " " + arrangerTracks_[trackIndex].instrument;
        }
        for (char& c : engHint) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (engHint.find("clavinet") != std::string::npos || engHint.find("clav") != std::string::npos || engHint.find("d6") != std::string::npos) isClav = true;
        else if (engHint.find("snes") != std::string::npos || engHint.find("sfxr") != std::string::npos || engHint.find("spc") != std::string::npos) isSnes = true;
        else if (engHint.find("sid") != std::string::npos || engHint.find("c64") != std::string::npos) isSid = true;
        else if (engHint.find("dx7") != std::string::npos || engHint.find("fm") != std::string::npos || engHint.find("rhodes") != std::string::npos) isDx7 = true;
        else if (engHint.find("ym2612") != std::string::npos || engHint.find("genesis") != std::string::npos) isYm2612 = true;
        else if (engHint.find("808") != std::string::npos || engHint.find("909") != std::string::npos || engHint.find("drum") != std::string::npos) isDrums = true;
        else if (engHint.find("303") != std::string::npos || engHint.find("acid") != std::string::npos) isTb303 = true;
        else if (engHint.find("piano") != std::string::npos || engHint.find("grand") != std::string::npos || engHint.find("guitar") != std::string::npos || engHint.find("upright") != std::string::npos) isPhys = true;
        else isPoly = true;
    }

    if (isClav) {
        if (normKey == "pickupselect" || normKey == "pickup" || normKey == "pickupa/b" || normKey == "pickup_a_b" || normKey == "pickupm") {
            float val = p ? p->currentVal : normVal;
            postParam(0, val, "clavinet");
        } else if (normKey == "phaseinvert" || normKey == "phase" || normKey == "phase(a-b)") {
            float val = p ? p->currentVal : (normVal > 0.5f ? 1.0f : 0.0f);
            postParam(1, val, "clavinet");
        } else if (normKey == "brilliant" || normKey == "brilliance" || normKey == "brill") {
            float val = p ? p->currentVal : (normVal > 0.5f ? 1.0f : 0.0f);
            postParam(2, val, "clavinet");
        } else if (normKey == "treble" || normKey == "treb") {
            float val = p ? p->currentVal : (normVal > 0.5f ? 1.0f : 0.0f);
            postParam(3, val, "clavinet");
        } else if (normKey == "medium" || normKey == "med") {
            float val = p ? p->currentVal : (normVal > 0.5f ? 1.0f : 0.0f);
            postParam(4, val, "clavinet");
        } else if (normKey == "soft") {
            float val = p ? p->currentVal : (normVal > 0.5f ? 1.0f : 0.0f);
            postParam(5, val, "clavinet");
        } else if (normKey == "hammerhardness" || normKey == "hammer") {
            float val = p ? p->currentVal : (0.2f + normVal * 2.8f);
            postParam(6, val, "clavinet");
        } else if (normKey == "hammersnap" || normKey == "snap") {
            float val = p ? p->currentVal : (normVal * 2.0f);
            postParam(7, val, "clavinet");
        } else if (normKey == "sustain") {
            float val = p ? p->currentVal : (0.8f + normVal * 0.199f);
            postParam(8, val, "clavinet");
        } else if (normKey == "damperthump" || normKey == "damper") {
            float val = p ? p->currentVal : normVal;
            postParam(9, val, "clavinet");
        } else if (normKey == "drive" || normKey == "overdrive") {
            float val = p ? p->currentVal : (0.5f + normVal * 2.5f);
            postParam(10, val, "clavinet");
        }
    } else if (isTb303) {
        if (normKey == "cutoff") {
            float val = p ? p->currentVal : (50.0f + normVal * 9000.0f);
            if (trackIndex == 0) engine_->setCutoff(val);
            postParam(0, val, "tb303");
        } else if (normKey == "resonance") {
            float val = p ? ((p->currentVal - p->minVal) / (p->maxVal - p->minVal)) : normVal;
            val = std::clamp(val, 0.0f, 0.98f);
            if (trackIndex == 0) engine_->setResonance(val);
            postParam(1, val, "tb303");
        } else if (normKey == "envmod" || normKey == "env_mod") {
            postParam(2, normVal, "tb303");
        } else if (normKey == "decay") {
            postParam(3, normVal, "tb303");
        } else if (normKey == "accent") {
            postParam(4, normVal, "tb303");
        } else if (normKey == "waveform") {
            float wave = (normVal >= 0.5f) ? 1.0f : 0.0f;
            postParam(5, wave, "tb303");
        } else if (normKey == "drive" || normKey == "overdrive") {
            postParam(6, normVal, "tb303");
        } else if (normKey == "pitch" || normKey == "tuning") {
            float val = p ? p->currentVal : ((normVal - 0.5f) * 24.0f);
            postParam(7, val, "tb303");
        } else if (normKey == "octave") {
            float val = p ? p->currentVal : std::round((normVal - 0.5f) * 4.0f);
            postParam(8, val, "tb303");
        } else if (normKey == "subvolume" || normKey == "subvol" || normKey == "sub_vol") {
            postParam(9, normVal, "tb303");
        } else if (normKey == "subwaveform" || normKey == "subosc" || normKey == "sub_osc") {
            float wave = (normVal >= 0.5f) ? 1.0f : 0.0f;
            postParam(10, wave, "tb303");
        } else if (normKey == "glidecurve" || normKey == "glide_curve" || normKey == "glide") {
            float curve = p ? p->currentVal : (normVal * 2.0f);
            postParam(11, curve, "tb303");
        } else if (normKey == "volume" || normKey == "level") {
            if (trackIndex < arrangerTracks_.size()) {
                engine_->setTrackVolume(trackIndex, normVal);
            }
        }
    } else if (isSnes) {
        uint32_t paramId = 999;
        float realVal = normVal;
        if (normKey == "waveform" || normKey == "wavetable") {
            paramId = 0;
            if (normVal > 1.0f) {
                realVal = std::clamp(normVal, 0.0f, 11.0f);
            } else if (p) {
                realVal = p->currentVal;
            } else {
                realVal = std::round(normVal * 11.0f);
            }
        } else if (normKey == "envelope" || normKey == "envmode") {
            paramId = 1;
            if (normVal > 1.0f) {
                realVal = std::clamp(normVal, 0.0f, 5.0f);
            } else if (p) {
                realVal = p->currentVal;
            } else {
                realVal = std::round(normVal * 5.0f);
            }
        } else if (normKey == "attack") {
            paramId = 2;
            realVal = p ? p->currentVal : (0.001f + normVal * 0.5f);
        } else if (normKey == "decay") {
            paramId = 3;
            realVal = p ? p->currentVal : (0.01f + normVal * 2.0f);
        } else if (normKey == "sustain") {
            paramId = 4;
            realVal = p ? p->currentVal : normVal;
        } else if (normKey == "release") {
            paramId = 5;
            realVal = p ? p->currentVal : (0.01f + normVal * 2.0f);
        } else if (normKey == "echoenabled" || normKey == "echo_on") {
            paramId = 6;
            realVal = (normVal > 0.5f) ? 1.0f : 0.0f;
        } else if (normKey == "echodelay" || normKey == "echoms" || normKey == "echo_delay") {
            paramId = 7;
            realVal = p ? p->currentVal : (16.0f + normVal * 464.0f);
        } else if (normKey == "echofeedback" || normKey == "echofdbk" || normKey == "feedback") {
            paramId = 8;
            realVal = p ? p->currentVal : (normVal * 0.95f);
        } else if (normKey == "echovolume" || normKey == "echovol") {
            paramId = 9;
            realVal = p ? p->currentVal : normVal;
        } else if (normKey == "noisemix") {
            paramId = 12;
            realVal = p ? p->currentVal : normVal;
        } else if (normKey == "volume" || normKey == "mastervol") {
            paramId = 14;
            realVal = p ? p->currentVal : normVal;
        } else if (normKey == "firprofile") {
            paramId = 15;
            if (normVal > 1.0f) {
                realVal = std::clamp(normVal, 0.0f, 3.0f);
            } else if (p) {
                realVal = p->currentVal;
            } else {
                realVal = std::round(normVal * 3.0f);
            }
        } else if (normKey == "pitchsweep" || normKey == "sweep") {
            paramId = 16;
            realVal = p ? p->currentVal : (normVal * 4.0f - 2.0f);
        } else if (normKey == "vibratorate" || normKey == "vibrate") {
            paramId = 17;
            realVal = p ? p->currentVal : (normVal * 20.0f);
        } else if (normKey == "vibratodepth" || normKey == "vibdepth") {
            paramId = 18;
            realVal = p ? p->currentVal : normVal;
        } else if (normKey == "sfxtype") {
            paramId = 19;
            if (normVal > 1.0f) {
                realVal = std::clamp(normVal, 0.0f, 10.0f);
            } else if (p) {
                realVal = p->currentVal;
            } else {
                realVal = std::round(normVal * 10.0f);
            }
        } else if (normKey == "seed") {
            paramId = 20;
            if (normVal > 1.0f) {
                realVal = std::clamp(normVal, 1.0f, 9999.0f);
            } else if (p) {
                realVal = p->currentVal;
            } else {
                realVal = std::round(1.0f + normVal * 9998.0f);
            }
        } else if (normKey == "randomize" || normKey == "randomizetrigger") {
            paramId = 21;
            realVal = 1.0f;
        }

        if (paramId != 999) {
            postParam(paramId, realVal, "snes");
        }
    } else if (isSid) {
        uint32_t paramId = 999;
        float realVal = p ? p->currentVal : normVal;
        if (normKey == "waveform") paramId = 0;
        else if (normKey == "pulsewidth") paramId = 1;
        else if (normKey == "pwmrate") paramId = 2;
        else if (normKey == "pwmdepth") paramId = 3;
        else if (normKey == "arpmode") paramId = 4;
        else if (normKey == "glidespeed") paramId = 5;
        else if (normKey == "chipmodel") paramId = 6;
        else if (normKey == "filtermode") paramId = 7;
        else if (normKey == "cutoff") {
            paramId = 8;
            realVal = p ? p->currentVal : (30.0f + normVal * 12000.0f);
        } else if (normKey == "resonance") paramId = 9;
        else if (normKey == "overdrive") paramId = 10;
        else if (normKey == "attack") {
            paramId = 11;
            realVal = p ? p->currentVal : (normVal * 2.0f);
        } else if (normKey == "decay") {
            paramId = 12;
            realVal = p ? p->currentVal : (normVal * 2.0f);
        } else if (normKey == "sustain") paramId = 13;
        else if (normKey == "release") {
            paramId = 14;
            realVal = p ? p->currentVal : (normVal * 2.0f);
        }

        if (paramId != 999) {
            postParam(paramId, realVal, "sid");
        }
    } else if (isDx7) {
        uint32_t paramId = 999;
        float realVal = p ? p->currentVal : normVal;
        if (normKey == "algorithm") paramId = 0;
        else if (normKey == "feedback") paramId = 1;
        else if (normKey == "volume" || normKey == "mastervolume") paramId = 2;
        else if (normKey == "brightness") paramId = 3;
        else if (normKey == "tinebell") paramId = 4;
        else if (normKey == "bodywarmth") paramId = 5;
        else if (normKey == "oplevel1") paramId = 6;
        else if (normKey == "oplevel2") paramId = 7;
        else if (normKey == "oplevel3") paramId = 8;
        else if (normKey == "oplevel4") paramId = 9;
        else if (normKey == "oplevel5") paramId = 10;
        else if (normKey == "oplevel6") paramId = 11;
        else if (normKey == "opcoarse1") paramId = 12;
        else if (normKey == "opcoarse2") paramId = 13;
        else if (normKey == "opcoarse3") paramId = 14;
        else if (normKey == "opcoarse4") paramId = 15;
        else if (normKey == "opcoarse5") paramId = 16;
        else if (normKey == "opcoarse6") paramId = 17;

        if (paramId != 999) {
            postParam(paramId, realVal, "dx7");
        }
    } else if (isYm2612) {
        uint32_t paramId = 999;
        float realVal = p ? p->currentVal : normVal;
        if (normKey == "algorithm") paramId = 0;
        else if (normKey == "feedback") paramId = 1;
        else if (normKey == "volume") paramId = 2;
        else if (normKey == "op1tl") paramId = 4;
        else if (normKey == "op2tl") paramId = 6;
        else if (normKey == "op3tl") paramId = 8;
        else if (normKey == "op4tl") paramId = 10;
        else if (normKey == "attack") {
            paramId = 11;
            realVal = p ? p->currentVal : (normVal * 2.0f);
        } else if (normKey == "decay") {
            paramId = 12;
            realVal = p ? p->currentVal : (normVal * 2.0f);
        } else if (normKey == "sustain") paramId = 13;
        else if (normKey == "release") {
            paramId = 14;
            realVal = p ? p->currentVal : (normVal * 2.0f);
        } else if (normKey == "sfxr" || normKey == "sfx") paramId = 15;

        if (paramId != 999) {
            postParam(paramId, realVal, "ym2612");
        }
    } else if (isDrums) {
        if (normKey == "tune" || normKey == "pitch" || normKey == "tuning") {
            postParam(1, normVal, "drum_kit");
        } else if (normKey == "decay") {
            postParam(2, normVal, "drum_kit");
        } else if (normKey == "overdrive" || normKey == "drive" || normKey == "snappy" || normKey == "punch") {
            postParam(3, normVal, "drum_kit");
        } else if (normKey == "tone" || normKey == "volume" || normKey == "level" || normKey == "attack" || normKey == "mastergain") {
            postParam(0, normVal * 1.5f, "drum_kit");
        }
    } else if (isPhys) {
        if (normKey == "brightness" || normKey == "tone" || normKey == "cutoff") {
            postParam(0, normVal, "piano");
        } else if (normKey == "sustain" || normKey == "decay") {
            postParam(1, 0.5f + normVal * 0.7f, "piano");
        } else if (normKey == "hardness" || normKey == "hammerhardness" || normKey == "punch") {
            postParam(2, 0.1f + normVal * 2.9f, "piano");
        } else if (normKey == "strumspread" || normKey == "spread") {
            postParam(3, 1.0f + normVal * 39.0f, "piano");
        }
    } else if (isPoly) {
        if (normKey == "cutoff" || normKey == "filtercutoff") {
            float val = p ? p->currentVal : (100.0f + normVal * 15900.0f);
            postParam(0, val, "poly");
        } else if (normKey == "resonance" || normKey == "filterresonance" || normKey == "reso") {
            float val = p ? p->currentVal : (0.1f + normVal * 4.9f);
            postParam(1, val, "poly");
        } else if (normKey == "waveform") {
            postParam(2, std::round(normVal * 3.0f), "poly");
        } else if (normKey == "attack") {
            float val = p ? p->currentVal : (0.05f + normVal * 3.95f);
            postParam(3, val, "poly");
        } else if (normKey == "decay") {
            float val = p ? p->currentVal : (0.1f + normVal * 5.9f);
            postParam(4, val, "poly");
        } else if (normKey == "sustain") {
            float val = p ? p->currentVal : normVal;
            postParam(5, val, "poly");
        } else if (normKey == "release") {
            float val = p ? p->currentVal : (0.1f + normVal * 7.9f);
            postParam(6, val, "poly");
        } else if (normKey == "detune") {
            float val = p ? p->currentVal : (1.0f + normVal * 34.0f);
            postParam(7, val, "poly");
        } else if (normKey == "warmth" || normKey == "drive") {
            float val = p ? p->currentVal : normVal;
            postParam(8, val, "poly");
        }
    }
}

bool GuiWindow::initialize(audio::AudioEngine& engine) {
    engine_ = &engine;
    engine_->getSequencer().getTransport().setMaxSteps(18 * 16);
    if (engine_->getSequencer().getNumTracks() == 0) {
        engine_->getSequencer().addTrack("DXVII E-Piano", engine_->getTrackSourceNodeId(0), 128);
    }
    for (uint32_t t = 0; t < engine_->getSequencer().getNumTracks(); ++t) {
        audio::NodeId srcId = engine_->getTrackSourceNodeId(t);
        if (srcId != audio::INVALID_NODE_ID && srcId != 0) {
            engine_->getSequencer().getTrack(t)->setTargetNodeId(srcId);
        }
        syncTrackToPreset(t);
    }
    syncTrackToPreset(0);
    canvas_.setCanvasSize(static_cast<float>(width_), static_cast<float>(height_));
    canvas_.updateRackLayout(engine_->getGraph());
    initDefaultKnobValues();
    initHistory();
    updateMixerStrips();
    dawnBridge_.initialize(width_, height_);

    // Instantiate Modular Subsystem Architecture (Option B)
    modularArrangerView_ = std::make_unique<ArrangerView>();
    modularEditView_ = std::make_unique<EditView>();
    modularTrackInspectorView_ = std::make_unique<TrackInspectorView>();
    modularMixerView_ = std::make_unique<MixerView>();
    modularDesignView_ = std::make_unique<DesignView>();
    virtualKeyboardDrawerWidget_ = std::make_unique<VirtualKeyboardDrawer>();
    projectBrowserDrawerWidget_ = std::make_unique<ProjectBrowserDrawer>();
    transportHeaderWidget_ = std::make_unique<TransportHeader>();
    bottomNavBarWidget_ = std::make_unique<BottomNavBar>();
    syncDesignViewToProject();

    bottomNavBarWidget_->onTabSelected = [this](int tabIdx) {
        if (tabIdx >= 0 && tabIdx <= 4) {
            setActiveView(static_cast<WorkspaceView>(tabIdx));
        }
    };
    transportHeaderWidget_->onToggleProjectHub = [this]() { toggleProjectHub(); };
    transportHeaderWidget_->onToggleBrowser = [this]() { toggleBrowser(); };
    transportHeaderWidget_->onToggleLoop = [this]() { toggleLoop(); };
    transportHeaderWidget_->onToggleMetronome = [this]() { toggleMetronome(); };
    transportHeaderWidget_->onOpenValueEdit = [this](const auto& req) { openValueEditDialog(req); };
    transportHeaderWidget_->onTogglePlay = [this]() {
        if (engine_) {
            if (engine_->getSequencer().isPlaying()) engine_->stopPlayback();
            else {
                syncArrangerToSequencer();
                engine_->getSequencer().start();
            }
        }
    };
    transportHeaderWidget_->onStop = [this]() {
        if (engine_) {
            auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if ((nowMs - lastStopClickTimeMs_) < 400) {
                engine_->panic();
                setStatusMessage("[PANIC] Full Audio Stop: All buffers flushed & voices killed");
            } else {
                engine_->stopPlayback();
            }
            lastStopClickTimeMs_ = nowMs;
        }
    };
    projectBrowserDrawerWidget_->onClose = [this]() { setBrowserOpen(false); };
    projectBrowserDrawerWidget_->onSelectTrack = [this](uint32_t trackIndex) {
        if (trackIndex < arrangerTracks_.size()) {
            selectedTrackIndex_ = trackIndex;
            if (modularArrangerView_) modularArrangerView_->setActiveTrack(trackIndex);
            if (modularMixerView_) modularMixerView_->setSelectedChannel(trackIndex);
            if (engine_) engine_->setActiveTrack(trackIndex);
        }
    };
    projectBrowserDrawerWidget_->onSelectPreset = [this](const std::string& presetId) {
        for (size_t i = 0; i < presets_.size(); ++i) {
            if (presets_[i].metadata.id == presetId || presets_[i].metadata.name.find(presetId) != std::string::npos) {
                loadPresetToSelectedTrack(i);
                return;
            }
        }
        if (!presets_.empty()) loadPresetToSelectedTrack(0);
    };
    projectBrowserDrawerWidget_->onRunMacro = [this](const std::string& macroId) {
        if (macroId == "macro_song") {
            runMacro(4);
        } else if (macroId == "macro_909") {
            runMacro(1);
        } else if (macroId == "macro_humanize") {
            runMacro(2);
        } else {
            runMacro(0);
        }
    };
    projectBrowserDrawerWidget_->onRunScript = [this](const std::string& scriptId) {
        runMacro(0);
    };
    projectBrowserDrawerWidget_->onAddPresetTrack = [this](const std::string& presetId) {
        std::string instName = presetId;
        std::string instEngine = presetId;
        float r = 1.0f, g = 0.55f, b = 0.0f;
        const auto* item = project::PresetManager::instance().findPreset(presetId);
        if (item) {
            instName = item->name;
            instEngine = item->engineTag.empty() ? item->id : item->engineTag;
            r = item->colorR; g = item->colorG; b = item->colorB;
        } else {
            for (const auto& p : presets_) {
                if (p.metadata.id == presetId || p.metadata.name == presetId) {
                    instName = p.metadata.name;
                    instEngine = p.metadata.id;
                    break;
                }
            }
        }
        addTrackToProject(instName, instEngine, r, g, b);
        recordProjectHistory("Add Track from Script: " + instName, "TRACK");
        setStatusMessage("Added Track: " + instName);
    };
    projectBrowserDrawerWidget_->onReplaceActiveTrackInstrument = [this](const std::string& scriptId) {
        if (selectedTrackIndex_ >= arrangerTracks_.size()) return;
        std::string instName = scriptId;
        std::string instEngine = scriptId;
        const auto* item = project::PresetManager::instance().findPreset(scriptId);
        if (item) {
            instName = item->name;
            instEngine = item->engineTag.empty() ? item->id : item->engineTag;
        } else {
            for (const auto& p : presets_) {
                if (p.metadata.id == scriptId || p.metadata.name == scriptId) {
                    instName = p.metadata.name;
                    instEngine = p.metadata.id;
                    break;
                }
            }
        }
        changeTrackInstrument(selectedTrackIndex_, instEngine, instName);
    };
    projectBrowserDrawerWidget_->onAddAudioFx = [this](const std::string& fxId) {
        addAudioFxToTrackIndex(selectedTrackIndex_, fxId);
    };
    projectBrowserDrawerWidget_->onAddMidiFx = [this](const std::string& fxId) {
        addMidiFxToTrackIndex(selectedTrackIndex_, fxId);
    };
    projectBrowserDrawerWidget_->onDropScript = [this](const BrowserScriptItem& script, float dropX, float dropY) {
        std::string scriptName = script.name.empty() ? script.id : script.name;
        std::string engineTag = script.id;
        float r = 1.0f, g = 0.55f, b = 0.0f;
        const auto* item = project::PresetManager::instance().findPreset(script.id);
        if (item) {
            scriptName = item->name;
            engineTag = item->engineTag.empty() ? item->id : item->engineTag;
            r = item->colorR; g = item->colorG; b = item->colorB;
        }

        bool isInstrument = (script.category == "SYNTH" || script.category == "INSTRUMENT");
        bool isAudioFx = (script.category == "AUDIO_FX");
        bool isMidiFx = (script.category == "MIDI_FX" || script.category == "MIDI_SEQ");

        // 1. Check if dropped over Track Properties Sidebar / Inspector
        bool droppedOnProperties = false;
        if (modularArrangerView_ && modularArrangerView_->isInspectorOpen() &&
            modularArrangerView_->getPropertiesDrawer().getDrawerBounds().contains(dropX, dropY)) {
            droppedOnProperties = true;
        } else if (modularTrackInspectorView_ && activeView_ == WorkspaceView::Track &&
                   modularTrackInspectorView_->getBounds().contains(dropX, dropY)) {
            droppedOnProperties = true;
        }

        if (droppedOnProperties) {
            if (selectedTrackIndex_ < arrangerTracks_.size()) {
                if (isInstrument) {
                    changeTrackInstrument(selectedTrackIndex_, engineTag, scriptName);
                } else if (isAudioFx) {
                    addAudioFxToTrackIndex(selectedTrackIndex_, script.id, scriptName, engineTag);
                } else if (isMidiFx) {
                    addMidiFxToTrackIndex(selectedTrackIndex_, script.id, scriptName, engineTag);
                }
            }
            return;
        }

        // 2. Check if dropped over Arranger View
        if (modularArrangerView_ && activeView_ == WorkspaceView::Arranger) {
            int targetTrack = modularArrangerView_->getTrackIndexAt(dropX, dropY);
            if (targetTrack >= 0 && targetTrack < static_cast<int>(arrangerTracks_.size())) {
                if (isInstrument) {
                    changeTrackInstrument(static_cast<uint32_t>(targetTrack), engineTag, scriptName);
                } else if (isAudioFx) {
                    addAudioFxToTrackIndex(static_cast<uint32_t>(targetTrack), script.id, scriptName, engineTag);
                } else if (isMidiFx) {
                    addMidiFxToTrackIndex(static_cast<uint32_t>(targetTrack), script.id, scriptName, engineTag);
                }
                return;
            }

            if (modularArrangerView_->isBelowLastTrack(dropX, dropY)) {
                if (isInstrument) {
                    addTrackToProject(scriptName, engineTag, r, g, b);
                    recordProjectHistory("Add Track from Dropped Script: " + scriptName, "TRACK");
                    setStatusMessage("Added Track: " + scriptName);
                } else {
                    setStatusMessage("Cannot add effect to empty space. Drop on a track.");
                }
                return;
            }
        }
    };
    projectBrowserDrawerWidget_->onLoadProject = [this](const std::string& filePath) {
        loadProjectFromFile(filePath);
    };
    projectBrowserDrawerWidget_->onSaveProject = [this](const std::string& filePath) {
        saveProjectToFile(filePath);
        if (projectBrowserDrawerWidget_) projectBrowserDrawerWidget_->scanSavedProjects();
    };
    projectBrowserDrawerWidget_->onSaveProjectAs = [this]() {
        saveProjectAs();
        if (projectBrowserDrawerWidget_) projectBrowserDrawerWidget_->scanSavedProjects();
    };
    projectBrowserDrawerWidget_->onOpenProjectsFolder = [this]() {
#if defined(_WIN32)
        system("start Projects");
#endif
    };
    projectBrowserDrawerWidget_->onDeleteProject = [this](const std::string& filePath) {
        std::error_code ec;
        std::filesystem::remove(filePath, ec);
        if (projectBrowserDrawerWidget_) projectBrowserDrawerWidget_->scanSavedProjects();
    };
    projectBrowserDrawerWidget_->onUndo = [this]() { undoHistory(); };
    projectBrowserDrawerWidget_->onRedo = [this]() { redoHistory(); };
    projectBrowserDrawerWidget_->onJumpToHistory = [this](size_t historyIndex) { jumpToHistoryIndex(historyIndex); };
    projectBrowserDrawerWidget_->onCreateCheckpoint = [this](const std::string& name) {
        createHistoryMilestone(name + " " + std::to_string(diffHistory_.getTimelineCount()));
    };
    projectBrowserDrawerWidget_->onClearHistory = [this]() { clearHistory(); };
    projectBrowserDrawerWidget_->onLaunchAudioToMidi = [this]() {
        openAudioToMidiConverter();
    };

    // Configure Audio-to-MIDI Transcription Modal Dialog
    audioToMidiDialog_.onBrowseAudioFile = [this]() {
        return promptOpenAudioFile();
    };

    audioToMidiDialog_.onTranscriptionComplete = [this](const audio::TranscribedMidiTrack& track,
                                                        bool createNewTrack,
                                                        const std::string& trackName,
                                                        bool extractChords) {
        if (track.notes.empty()) {
            lastStatusMessage_ = "Audio to MIDI: No notes detected in audio file";
            return;
        }

        uint32_t targetTrackIdx = selectedTrackIndex_;
        if (createNewTrack && modularArrangerView_) {
            ArrangerTimelineTrack newTrack;
            newTrack.name = trackName.empty() ? "Transcribed MIDI" : trackName;
            newTrack.instrument = "Polyphonic Synth";
            newTrack.instrumentEngine = "poly_synth";
            newTrack.iconRef = "preset:inst_keys";
            newTrack.r = 0.0f; newTrack.g = 0.85f; newTrack.b = 1.0f; // Cyan
            modularArrangerView_->getTracks().push_back(newTrack);
            targetTrackIdx = static_cast<uint32_t>(modularArrangerView_->getTracks().size() - 1);
            setSelectedTrackIndex(targetTrackIdx);
        }

        if (modularArrangerView_ && targetTrackIdx < modularArrangerView_->getTracks().size()) {
            auto& t = modularArrangerView_->getTracks()[targetTrackIdx];
            ArrangerTimelineClip clip;
            clip.id = "transcribed_clip_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            clip.name = trackName.empty() ? "Transcribed Clip" : trackName;
            clip.trackIndex = targetTrackIdx;
            clip.startBar = 1;
            float maxStep = 16.0f;
            for (const auto& tn : track.notes) {
                ArrangerClipNote cn;
                cn.pitch = tn.pitch;
                cn.startBeat = tn.getStartBeat();
                cn.lengthBeats = std::max(0.25f, tn.getDurationBeats());
                cn.velocity = tn.velocity;
                clip.notes.push_back(cn);
                maxStep = std::max(maxStep, tn.startStep + tn.durationSteps);
            }
            clip.lengthBars = std::max(1u, static_cast<uint32_t>(std::ceil(maxStep / 16.0f)));
            clip.r = 0.0f; clip.g = 0.85f; clip.b = 1.0f;
            if (extractChords && !track.detectedChords.empty()) {
                clip.detectedChords = track.detectedChords;
            }
            t.clips.push_back(clip);
        }

        if (engine_ && targetTrackIdx < engine_->getSequencer().getNumTracks()) {
            auto* seqTrack = engine_->getSequencer().getTrack(targetTrackIdx);
            if (seqTrack) {
                for (const auto& tn : track.notes) {
                    uint32_t sIdx = static_cast<uint32_t>(std::round(tn.startStep));
                    if (sIdx < seqTrack->getNumSteps()) {
                        auto& step = seqTrack->getStep(sIdx);
                        step.active = true;
                        step.note = tn.pitch;
                        step.velocity = tn.velocity;
                        step.gateLength = std::clamp(tn.durationSteps, 0.1f, 64.0f);
                    }
                }
            }
        }

        lastStatusMessage_ = "Transcribed " + std::to_string(track.notes.size()) +
                             " MIDI notes into " + trackName +
                             (extractChords && !track.detectedChords.empty()
                                 ? " (" + std::to_string(track.detectedChords.size()) + " chords extracted)"
                                 : "");
    };

    audioToMidiDialog_.onAuditionStart = [this](const std::vector<float>& /*samples*/, uint32_t /*sampleRate*/) {
        if (engine_) engine_->postNoteOn(60, 0.85f, false, false, static_cast<int>(selectedTrackIndex_));
    };

    audioToMidiDialog_.onAuditionStop = [this]() {
        if (engine_) engine_->postNoteOff(60, static_cast<int>(selectedTrackIndex_));
    };

    // Configure Top-Level Reusable PresetSearchDialog
    presetSearchDialog_.onPluginSelected = [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t trackIndex) {
        if (mode == PluginDialogMode::SelectPreset || mode == PluginDialogMode::AddInstrument || mode == PluginDialogMode::AddDrums) {
            if (trackIndex >= arrangerTracks_.size() && (mode == PluginDialogMode::AddInstrument || mode == PluginDialogMode::AddDrums)) {
                addTrackToProject(entry.name, entry.id.empty() ? entry.engineTag : entry.id, entry.r, entry.g, entry.b);
                return;
            }
            uint32_t targetIdx = (trackIndex < arrangerTracks_.size()) ? trackIndex : selectedTrackIndex_;
            setSelectedTrackIndex(targetIdx);

            const auto* pItem = project::PresetManager::instance().findPreset(entry.id);
            if (pItem) {
                project::PresetDefinition def{};
                if (project::PresetManager::instance().loadPresetDefinition(*pItem, def)) {
                    size_t foundIdx = presets_.size();
                    for (size_t i = 0; i < presets_.size(); ++i) {
                        if (presets_[i].metadata.id == def.metadata.id) {
                            foundIdx = i;
                            break;
                        }
                    }
                    if (foundIdx >= presets_.size()) {
                        presets_.push_back(def);
                        foundIdx = presets_.size() - 1;
                    }
                    loadPresetToSelectedTrack(foundIdx);
                }
            }

            if (targetIdx < mixerStrips_.size()) {
                mixerStrips_[targetIdx].name = entry.name;
            }
            if (targetIdx < arrangerTracks_.size()) {
                arrangerTracks_[targetIdx].name = entry.name;
                arrangerTracks_[targetIdx].instrument = entry.name;
                arrangerTracks_[targetIdx].instrumentEngine = entry.id.empty() ? entry.engineTag : entry.id;
                arrangerTracks_[targetIdx].type = entry.engineTag.empty() ? entry.category : entry.engineTag;
                arrangerTracks_[targetIdx].r = entry.r;
                arrangerTracks_[targetIdx].g = entry.g;
                arrangerTracks_[targetIdx].b = entry.b;
            }
            if (modularArrangerView_ && targetIdx < modularArrangerView_->getTracks().size()) {
                auto& trk = modularArrangerView_->getTracks()[targetIdx];
                trk.name = entry.name;
                trk.instrument = entry.name;
                trk.instrumentEngine = entry.id.empty() ? entry.engineTag : entry.id;
                trk.r = entry.r;
                trk.g = entry.g;
                trk.b = entry.b;
                modularArrangerView_->syncDrawerDataForTrack(targetIdx);
            }
            if (modularTrackInspectorView_ && targetIdx < modularTrackInspectorView_->getTracks().size()) {
                auto& trk = modularTrackInspectorView_->getTracks()[targetIdx];
                trk.name = entry.name;
                trk.instrument = entry.name;
                trk.instrumentEngine = entry.id.empty() ? entry.engineTag : entry.id;
                trk.r = entry.r;
                trk.g = entry.g;
                trk.b = entry.b;
                modularTrackInspectorView_->syncKnobsForTrack(trk);
                modularTrackInspectorView_->updateDrawerData();
            }
            if (modularMixerView_ && targetIdx < modularMixerView_->getChannels().size()) {
                auto& ch = modularMixerView_->getChannels()[targetIdx];
                ch.name = entry.name;
                ch.instrument = entry.name;
                ch.r = entry.r; ch.g = entry.g; ch.b = entry.b;
            }
            if (engine_) {
                engine_->assignTrackInstrument(targetIdx, entry.id.empty() ? entry.name : entry.id);
            }
            syncTrackToPreset(targetIdx);
            recordProjectHistory("Change Instrument on Track " + std::to_string(targetIdx + 1) + " to " + entry.name, "INSTRUMENT");
            setStatusMessage("Loaded Preset: " + entry.name + " to Track " + std::to_string(targetIdx + 1));
        } else if (mode == PluginDialogMode::AddAudioFx) {
            if (modularArrangerView_ && trackIndex < modularArrangerView_->getTracks().size()) {
                TrackAudioFxItem fxItem(entry.name, entry.engineTag.empty() ? entry.id : entry.engineTag, 0.5f, 0.5f, true);
                fxItem.id = entry.id;
                project::PresetDefinition def{};
                if (project::PresetManager::instance().loadPresetDefinition(entry.id, def)) {
                    fxItem.populateFromPresetDefinition(def);
                }
                modularArrangerView_->getTracks()[trackIndex].audioFx.push_back(std::move(fxItem));
                syncTrackAudioFxToEngine(trackIndex);
                syncDesignViewToProject();
                recordProjectHistory("Add Audio FX " + entry.name + " to Track " + std::to_string(trackIndex + 1), "AUDIO_FX");
            }
            setStatusMessage("Added Audio FX: " + entry.name);
        } else if (mode == PluginDialogMode::AddMidiFx) {
            if (modularArrangerView_ && trackIndex < modularArrangerView_->getTracks().size()) {
                modularArrangerView_->getTracks()[trackIndex].midiFx.push_back({entry.name, entry.engineTag, 0, 0, true});
                syncTrackMidiFxToEngine(trackIndex);
                syncDesignViewToProject();
                recordProjectHistory("Add MIDI FX " + entry.name + " to Track " + std::to_string(trackIndex + 1), "MIDI_FX");
            }
            setStatusMessage("Added MIDI FX: " + entry.name);
        }
    };
    presetSearchDialog_.onClose = [this]() {
        presetSearchDialog_.close();
    };

    // Register Default Universal Quick Commands into CommandPaletteDialog
    commandPaletteDialog_.clearCommands();
    commandPaletteDialog_.registerCommand({
        "view.arranger", "Arranger View", "Full multi-track timeline, clips and automation",
        CommandCategory::View, "1", [this]() { setActiveView(WorkspaceView::Arranger); }
    });
    commandPaletteDialog_.registerCommand({
        "view.edit", "Edit View (Piano Roll / Tracker)", "Note editing, step sequencer and chords",
        CommandCategory::View, "2", [this]() { setActiveView(WorkspaceView::Edit); }
    });
    commandPaletteDialog_.registerCommand({
        "view.inspector", "Track Inspector", "Channel parameters, inserts and routing",
        CommandCategory::View, "3", [this]() { setActiveView(WorkspaceView::Track); }
    });
    commandPaletteDialog_.registerCommand({
        "view.mixer", "Modular Mixer", "High-density 8-channel console with VU ballistics",
        CommandCategory::View, "4", [this]() { setActiveView(WorkspaceView::Mixer); }
    });
    commandPaletteDialog_.registerCommand({
        "view.design", "Modular Rack & Design", "Synthesizer node graph and modular routing",
        CommandCategory::View, "5", [this]() { setActiveView(WorkspaceView::Design); }
    });
    commandPaletteDialog_.registerCommand({
        "view.drawer.keyboard", "Toggle Piano / Drum Pad Drawer", "Slide-up interactive audition drawer",
        CommandCategory::View, "K", [this]() { toggleVirtualKeyboardDrawer(); }
    });
    commandPaletteDialog_.registerCommand({
        "view.drawer.arranger_mixer", "Toggle Arranger Mixer Drawer", "Slide-up bottom mixing console in Arranger timeline",
        CommandCategory::View, "M", [this]() {
            if (activeView_ != WorkspaceView::Arranger) {
                setActiveView(WorkspaceView::Arranger);
            }
            if (modularArrangerView_) {
                modularArrangerView_->toggleMixerDrawer();
            }
        }
    });
    commandPaletteDialog_.registerCommand({
        "view.drawer.browser", "Toggle Project Browser Drawer", "Projects, presets, macros, and history timeline",
        CommandCategory::View, "B", [this]() { toggleBrowser(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.play_pause", "Play / Pause Transport", "Toggle global transport playback",
        CommandCategory::Action, "Space", [this]() {
            if (engine_) {
                if (engine_->getSequencer().isPlaying()) engine_->stopPlayback();
                else {
                    syncArrangerToSequencer();
                    engine_->getSequencer().start();
                }
            }
        }
    });
    commandPaletteDialog_.registerCommand({
        "action.stop_panic", "Panic / All Notes Off", "Halt transport and kill all sounding voices",
        CommandCategory::Action, "Esc", [this]() {
            if (engine_) engine_->panic();
        }
    });
    commandPaletteDialog_.registerCommand({
        "action.loop", "Toggle Loop Cycle", "Enable or disable global arrangement cycle loop",
        CommandCategory::Action, "L", [this]() { toggleLoop(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.metronome", "Toggle Metronome", "Audible click track on beats",
        CommandCategory::Action, "M", [this]() { toggleMetronome(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.undo", "Undo Last Action", "Revert last project or parameter state",
        CommandCategory::Action, "Ctrl+Z", [this]() { undoHistory(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.redo", "Redo Reverted Action", "Re-apply reverted state modification",
        CommandCategory::Action, "Ctrl+Y", [this]() { redoHistory(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.save", "Save Project", "Write project to current active file",
        CommandCategory::Action, "Ctrl+S", [this]() { saveProjectToFile(projectFilePath_); }
    });
    commandPaletteDialog_.registerCommand({
        "action.save_as", "Save Project As...", "Export project with new name or path",
        CommandCategory::Action, "Ctrl+Shift+S", [this]() { saveProjectAs(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.bounce_wav", "Bounce Master to WAV", "Render arrangement master audio to WAV file",
        CommandCategory::Action, "Ctrl+E", [this]() { bounceMasterToWav(projectName_ + ".wav"); }
    });
    commandPaletteDialog_.registerCommand({
        "action.audio_to_midi", "Audio to MIDI Converter", "Transcribe audio recording or sample into MIDI clip",
        CommandCategory::Action, "Ctrl+M", [this]() { openAudioToMidiConverter(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.toggle_fullscreen_device", "Toggle Fullscreen Instrument / FX GUI", "Dedicated full-display view for active instrument or FX rack",
        CommandCategory::Action, "Shift+F", [this]() { toggleFullscreenDevice(); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.browse_dialog", "Browse Presets (Preset Dialog)", "Open Preset Search and Library Dialog",
        CommandCategory::Preset, "Shift+P", [this]() { openPresetDialog(PluginDialogMode::SelectPreset, selectedTrackIndex_); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.add_instrument", "Add / Change Instrument", "Browse all synthesized & physical instruments",
        CommandCategory::Preset, "", [this]() { openPresetDialog(PluginDialogMode::AddInstrument, selectedTrackIndex_); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.add_audio_fx", "Add Audio Effect", "Browse audio DSP effects & spaces",
        CommandCategory::Preset, "", [this]() { openPresetDialog(PluginDialogMode::AddAudioFx, selectedTrackIndex_); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.add_midi_fx", "Add MIDI Effect", "Browse MIDI processors & arpeggiators",
        CommandCategory::Preset, "", [this]() { openPresetDialog(PluginDialogMode::AddMidiFx, selectedTrackIndex_); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.303_acid", "TB-303 Acid Bass", "Resonant acid squelch synth preset",
        CommandCategory::Preset, "", [this]() { loadPresetToSelectedTrack(0); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.808_kit", "TR-808 Rhythm Kit", "Classic analog drum machine kit",
        CommandCategory::Preset, "", [this]() { if (presets_.size() > 1) loadPresetToSelectedTrack(1); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.909_drive", "TR-909 Punch Kit", "Club kick and snappy snares",
        CommandCategory::Preset, "", [this]() { if (presets_.size() > 2) loadPresetToSelectedTrack(2); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.dx7_rhodes", "DX7 Electric Piano", "Lush FM synth tine keys and bells",
        CommandCategory::Preset, "", [this]() { if (presets_.size() > 3) loadPresetToSelectedTrack(3); }
    });
    commandPaletteDialog_.registerCommand({
        "theme.browse", "Theme: Browse All Themes & Palettes", "Open filterable theme browser with live swatches and search",
        CommandCategory::Theme, "Ctrl+Shift+T", [this]() { openThemeBrowserDialog(); }
    });
    const auto& themeCatalog = Theme::getPresetCatalog();
    for (size_t i = 0; i < themeCatalog.size(); ++i) {
        const auto& item = themeCatalog[i];
        commandPaletteDialog_.registerCommand({
            "theme." + std::to_string(i),
            "Theme: " + item.name,
            item.description + " (" + item.subtitle + ")",
            CommandCategory::Theme,
            "",
            [this, i]() {
                setActiveThemePreset(static_cast<int>(i));
                setStatusMessage("Theme: " + getTheme().name);
            }
        });
    }
    commandPaletteDialog_.registerCommand({
        "view.toggle_minimal_gui", "View: Toggle Minimalist Controls",
        "Switch between 3D skeuomorphic styling and clean high-performance vector knobs (Bitwig/Ableton style)",
        CommandCategory::View, "Alt+M",
        [this]() { toggleMinimalGui(); }
    });
    commandPaletteDialog_.registerCommand({
        "macro.procedural_acid", "Macro: Procedural Acid Bassline", "Generate algorithmic 16-step 303 pattern",
        CommandCategory::Macro, "", [this]() { runMacro(0); }
    });
    commandPaletteDialog_.registerCommand({
        "macro.techno_groove", "Macro: 909 Techno Groove", "Populate kick, hi-hat offbeat, and claps",
        CommandCategory::Macro, "", [this]() { runMacro(1); }
    });
    commandPaletteDialog_.registerCommand({
        "macro.humanize", "Macro: Humanize Velocities & Timing", "Apply subtle organic jitter to selected notes",
        CommandCategory::Macro, "", [this]() { runMacro(2); }
    });
    commandPaletteDialog_.registerCommand({
        "macro.arp_track", "Macro: Arpeggiate Active Track", "Transform active track notes into 16th-note arpeggio",
        CommandCategory::Macro, "", [this]() { runMacro(3); }
    });
    commandPaletteDialog_.registerCommand({
        "macro.procedural_song", "Macro: Procedural Song Architect", "Generate complete multi-track song arrangement",
        CommandCategory::Macro, "Ctrl+Shift+G", [this]() { runMacro(4); }
    });

    if (modularArrangerView_) {
        modularArrangerView_->onVolumeChanged = [this](uint32_t idx, float vol) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].volume = vol;
                if (idx < mixerStrips_.size()) mixerStrips_[idx].volume = vol;
            }
            if (engine_) {
                engine_->setTrackVolume(idx, vol);
            }
        };
        modularArrangerView_->onPanChanged = [this](uint32_t idx, float pan) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].pan = pan;
                if (idx < mixerStrips_.size()) mixerStrips_[idx].pan = pan;
            }
            if (engine_) {
                engine_->setTrackPan(idx, pan);
            }
        };
        modularArrangerView_->onMuteToggled = [this](uint32_t idx, bool mute) {
            setTrackMuteState(idx, mute);
        };
        modularArrangerView_->onSoloToggled = [this](uint32_t idx, bool solo) {
            setTrackSoloState(idx, solo);
        };
        modularArrangerView_->onParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
            if (idx != selectedTrackIndex_) {
                setSelectedTrackIndex(idx);
            }
            syncTrackToPreset(idx);
            dispatchTrackParam(idx, paramName, normVal);
        };
        modularArrangerView_->onAuditionNote = [this](uint32_t idx, uint8_t note, float vel) {
            auditionTrackNote(idx, note, vel);
        };
        modularArrangerView_->onClipsChanged = [this]() {
            syncArrangerToSequencer();
        };
        modularArrangerView_->onTrackChordFollowChanged = [this](uint32_t idx, theory::ChordFollowMode mode) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].chordFollowMode = mode;
            }
            if (modularTrackInspectorView_ && idx < modularTrackInspectorView_->getTracks().size()) {
                modularTrackInspectorView_->getTracks()[idx].chordFollowMode = mode;
            }
            if (modularMixerView_ && idx < modularMixerView_->getChannels().size()) {
                modularMixerView_->getChannels()[idx].chordFollowMode = mode;
            }
            if (engine_) {
                auto& seq = engine_->getSequencer();
                if (auto* trk = seq.getTrack(idx)) {
                    trk->setChordFollowMode(mode);
                }
            }
            if (modularEditView_ && selectedTrackIndex_ == idx) {
                modularEditView_->setChordFollowMode(mode);
                modularEditView_->setChordTrack(modularArrangerView_->getChordTrack());
            }
        };
        modularArrangerView_->onChordTrackChanged = [this]() {
            if (engine_ && modularArrangerView_) {
                engine_->getSequencer().setChordTrack(modularArrangerView_->getChordTrack());
                if (modularEditView_) {
                    modularEditView_->setChordTrack(modularArrangerView_->getChordTrack());
                }
            }
        };
        modularArrangerView_->onAuditionChord = [this](const theory::ChordEvent& chord) {
            stopChordAudition();
            if (engine_) {
                uint32_t tIdx = modularArrangerView_ ? modularArrangerView_->getActiveTrackIndex() : selectedTrackIndex_;
                auto notes = theory::ChordTheory::getAuditionMidiNotes(chord);
                for (int note : notes) {
                    if (note >= 0 && note <= 127) {
                        uint8_t uNote = static_cast<uint8_t>(note);
                        engine_->postNoteOn(uNote, 0.80f, false, false, static_cast<int>(tIdx), false);
                        auditionChordNotes_.push_back(uNote);
                    }
                }
            }
        };
        modularArrangerView_->onAuditionChordEnd = [this]() {
            stopChordAudition();
        };
        modularArrangerView_->onTrackSelected = [this](uint32_t idx) {
            setSelectedTrackIndex(idx);
        };
        modularArrangerView_->onTrackRename = [this](uint32_t idx, const std::string& newName) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].name = newName;
            }
            if (idx < mixerStrips_.size()) {
                mixerStrips_[idx].name = newName;
            }
            setStatusMessage("Track renamed: " + newName);
        };
        modularArrangerView_->onTrackIconChanged = [this](uint32_t idx, const std::string& iconRef) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].iconRef = iconRef;
            }
            if (idx < mixerStrips_.size()) {
                mixerStrips_[idx].iconRef = iconRef;
            }
            if (valueEditDialog_.isOpen()) {
                valueEditDialog_.setIconRef(iconRef);
            }
            setStatusMessage("Track icon updated");
        };
        modularArrangerView_->getIconSearchDialog().setClipboardProvider([this]() -> std::string {
#if EATS_HAS_GLFW
            if (window_) {
                const char* str = glfwGetClipboardString(window_);
                return str ? std::string(str) : std::string();
            }
#endif
            return {};
        });
        modularArrangerView_->onAudioFxChanged = [this](uint32_t idx) {
            syncTrackAudioFxToEngine(idx);
        };
        modularArrangerView_->onMidiFxChanged = [this](uint32_t idx) {
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
        };
        modularArrangerView_->onMidiFxParamChanged = [this](uint32_t idx, const std::string&, float) {
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
        };
        modularArrangerView_->onToggleAudioFx = [this](uint32_t idx, size_t fxIdx, bool en) {
            syncTrackAudioFxToEngine(idx);
        };
        modularArrangerView_->onToggleMidiFx = [this](uint32_t idx, size_t fxIdx, bool en) {
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
        };
        modularArrangerView_->onAudioFxParamChanged = [this](uint32_t idx, size_t fxIdx, const std::string& paramName, float normVal) {
            if (engine_) {
                engine_->setTrackAudioFxParam(idx, fxIdx, paramName, normVal);
            }
        };
        modularArrangerView_->onMasterVolumeChanged = [this](float vol) {
            setMasterVolume(vol);
        };
        modularArrangerView_->onMasterPanChanged = [this](float pan) {
            setMasterPan(pan);
        };
        modularArrangerView_->onMasterMuteToggled = [this](bool mute) {
            setMasterMuted(mute);
        };
        modularArrangerView_->onTrackAdded = [this](uint32_t idx) {
            handleTrackAdded(idx);
        };
        modularArrangerView_->onTrackDeleted = [this](uint32_t idx) {
            deleteTrack(idx);
        };
        modularArrangerView_->onTrackDuplicated = [this](uint32_t idx) {
            duplicateTrack(idx);
        };
        modularArrangerView_->onAddClip = [this](uint32_t idx) {
            addClipToTrack(idx);
        };
        modularArrangerView_->onDuplicateClip = [this](uint32_t tIdx, int cIdx) {
            duplicateClip(tIdx, cIdx);
        };
        modularArrangerView_->onDeleteClip = [this](uint32_t tIdx, int cIdx) {
            deleteClip(tIdx, cIdx);
        };
        modularArrangerView_->getPropertiesDrawer().onAddClip = [this](uint32_t idx) {
            addClipToTrack(idx);
        };
        modularArrangerView_->getPropertiesDrawer().onDeleteTrack = [this](uint32_t idx) {
            deleteTrack(idx);
        };
        modularArrangerView_->getPropertiesDrawer().onDuplicateTrack = [this](uint32_t idx) {
            duplicateTrack(idx);
        };
        modularArrangerView_->getPropertiesDrawer().onDuplicateClip = [this](uint32_t tIdx, int cIdx) {
            duplicateClip(tIdx, cIdx);
        };
        modularArrangerView_->getPropertiesDrawer().onDeleteClip = [this](uint32_t tIdx, int cIdx) {
            deleteClip(tIdx, cIdx);
        };
        modularArrangerView_->onOpenDesign = [this](uint32_t idx) {
            openDesignForTrack(idx);
        };
        modularArrangerView_->getPropertiesDrawer().onOpenDesign = [this](uint32_t idx) {
            openDesignForTrack(idx);
        };
        modularArrangerView_->onInstrumentChanged = [this](uint32_t tIdx, const std::string& instId, const std::string& instName) {
            changeTrackInstrument(tIdx, instId, instName);
        };
    }

    valueEditDialog_.onCopyToClipboard = [this](const std::string& text) {
#if EATS_HAS_GLFW
        if (window_) {
            glfwSetClipboardString(window_, text.c_str());
        }
#endif
    };
    valueEditDialog_.onPasteFromClipboard = [this]() -> std::string {
#if EATS_HAS_GLFW
        if (window_) {
            const char* str = glfwGetClipboardString(window_);
            return str ? std::string(str) : std::string();
        }
#endif
        return {};
    };

    if (modularTrackInspectorView_) {
        modularTrackInspectorView_->onTrackSelected = [this](uint32_t idx) {
            setSelectedTrackIndex(idx);
        };
        modularTrackInspectorView_->onVolumeChanged = [this](uint32_t idx, float vol) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].volume = vol;
                if (idx < mixerStrips_.size()) mixerStrips_[idx].volume = vol;
            }
            if (engine_) {
                engine_->setTrackVolume(idx, vol);
            }
        };
        modularTrackInspectorView_->onPanChanged = [this](uint32_t idx, float pan) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].pan = pan;
                if (idx < mixerStrips_.size()) mixerStrips_[idx].pan = pan;
            }
            if (engine_) {
                engine_->setTrackPan(idx, pan);
            }
        };
        modularTrackInspectorView_->onMuteToggled = [this](uint32_t idx, bool mute) {
            setTrackMuteState(idx, mute);
        };
        modularTrackInspectorView_->onSoloToggled = [this](uint32_t idx, bool solo) {
            setTrackSoloState(idx, solo);
        };
        modularTrackInspectorView_->onFreezeToggled = [this](uint32_t idx, bool freeze) {
            setTrackFreezeState(idx, freeze);
        };
        modularTrackInspectorView_->onColorChanged = [this](uint32_t idx, float r, float g, float b) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].r = r;
                arrangerTracks_[idx].g = g;
                arrangerTracks_[idx].b = b;
                for (auto& clp : arrangerTracks_[idx].clips) {
                    clp.r = r; clp.g = g; clp.b = b;
                }
                recordProjectHistory("Change Accent Color on Track " + std::to_string(idx + 1), "TRACK");
            }
        };
        modularTrackInspectorView_->onOpenCodeEditor = [this](uint32_t idx) {
            (void)idx;
            setActiveView(WorkspaceView::Design);
            setDesignSubView(DesignSubView::Eatscript);
        };
        modularTrackInspectorView_->onPrevPreset = [this]() {
            prevPreset();
        };
        modularTrackInspectorView_->onNextPreset = [this]() {
            nextPreset();
        };
        modularTrackInspectorView_->onChordFollowChanged = [this](uint32_t idx, ChordFollowMode mode) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].chordFollowMode = mode;
                recordProjectHistory("Set Chord Follow on Track " + std::to_string(idx + 1), "CHORD");
            }
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size()) {
                modularArrangerView_->getTracks()[idx].chordFollowMode = mode;
                modularArrangerView_->syncDrawerDataForTrack(idx);
            }
            if (modularMixerView_ && idx < modularMixerView_->getChannels().size()) {
                modularMixerView_->getChannels()[idx].chordFollowMode = mode;
            }
            if (engine_) {
                auto& seq = engine_->getSequencer();
                if (auto* trk = seq.getTrack(idx)) {
                    trk->setChordFollowMode(mode);
                }
            }
            if (modularEditView_ && selectedTrackIndex_ == idx) {
                modularEditView_->setChordFollowMode(mode);
                modularEditView_->setChordTrack(modularArrangerView_->getChordTrack());
            }
        };
        modularTrackInspectorView_->onBakeChords = [this](uint32_t idx) {
            bakeTrackChordsAndFx(idx);
        };
        modularTrackInspectorView_->onParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
            if (idx != selectedTrackIndex_) {
                setSelectedTrackIndex(idx);
            }
            syncTrackToPreset(idx);
            dispatchTrackParam(idx, paramName, normVal);
        };
        modularTrackInspectorView_->onAuditionNote = [this](uint32_t idx, uint8_t note, float vel) {
            auditionTrackNote(idx, note, vel);
        };
        modularTrackInspectorView_->onAudioFxParamChanged = [this](uint32_t idx, size_t fxIdx, const std::string& paramName, float normVal) {
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].audioFx = modularTrackInspectorView_->getTracks()[idx].audioFxList;
                }
            }
            if (engine_) {
                engine_->setTrackAudioFxParam(idx, fxIdx, paramName, normVal);
            }
            dispatchHardwareParam(paramName, normVal);
        };
        modularTrackInspectorView_->onMidiFxParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].midiFx = modularTrackInspectorView_->getTracks()[idx].midiFxList;
                }
            }
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
            dispatchHardwareParam(paramName, normVal);
        };
        modularTrackInspectorView_->onAddMidiFx = [this](uint32_t idx) {
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].midiFx = modularTrackInspectorView_->getTracks()[idx].midiFxList;
                }
            }
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
        };
        modularTrackInspectorView_->onAddAudioFx = [this](uint32_t idx) {
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].audioFx = modularTrackInspectorView_->getTracks()[idx].audioFxList;
                }
            }
            syncTrackAudioFxToEngine(idx);
        };
        modularTrackInspectorView_->onToggleAudioFx = [this](uint32_t idx, size_t fxIdx, bool en) {
            (void)fxIdx; (void)en;
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].audioFx = modularTrackInspectorView_->getTracks()[idx].audioFxList;
                }
            }
            syncTrackAudioFxToEngine(idx);
        };
        modularTrackInspectorView_->onToggleMidiFx = [this](uint32_t idx, size_t fxIdx, bool en) {
            (void)fxIdx; (void)en;
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].midiFx = modularTrackInspectorView_->getTracks()[idx].midiFxList;
                }
            }
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
        };
        modularTrackInspectorView_->onReorderAudioFx = [this](uint32_t idx, size_t fromIdx, size_t toIdx) {
            (void)fromIdx; (void)toIdx;
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].audioFx = modularTrackInspectorView_->getTracks()[idx].audioFxList;
                }
            }
            syncTrackAudioFxToEngine(idx);
        };
        modularTrackInspectorView_->onReorderMidiFx = [this](uint32_t idx, size_t fromIdx, size_t toIdx) {
            (void)fromIdx; (void)toIdx;
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].midiFx = modularTrackInspectorView_->getTracks()[idx].midiFxList;
                }
            }
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
        };
        modularTrackInspectorView_->onRemoveAudioFx = [this](uint32_t idx, size_t fxIdx) {
            (void)fxIdx;
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].audioFx = modularTrackInspectorView_->getTracks()[idx].audioFxList;
                }
            }
            syncTrackAudioFxToEngine(idx);
        };
        modularTrackInspectorView_->onRemoveMidiFx = [this](uint32_t idx, size_t fxIdx) {
            (void)fxIdx;
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].midiFx = modularTrackInspectorView_->getTracks()[idx].midiFxList;
                }
            }
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
        };
        modularTrackInspectorView_->onAudioFxChanged = [this](uint32_t idx) {
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].audioFx = modularTrackInspectorView_->getTracks()[idx].audioFxList;
                }
            }
            syncTrackAudioFxToEngine(idx);
        };
        modularTrackInspectorView_->onMidiFxChanged = [this](uint32_t idx) {
            if (modularArrangerView_ && modularTrackInspectorView_) {
                if (idx < modularTrackInspectorView_->getTracks().size() && idx < modularArrangerView_->getTracks().size()) {
                    modularArrangerView_->getTracks()[idx].midiFx = modularTrackInspectorView_->getTracks()[idx].midiFxList;
                }
            }
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
        };
        modularTrackInspectorView_->onScrollChanged = [this](float sY) {
            trackInspectorScrollY_ = sY;
        };
        modularTrackInspectorView_->onOpenFullscreenDevice = [this](uint32_t idx) {
            openFullscreenDevice(idx);
        };
        modularTrackInspectorView_->onOpenPluginGui = [this](uint32_t idx) {
            openDetachedInstrument(idx);
        };
        modularTrackInspectorView_->onPopulateClapData = [this](uint32_t tIdx, TrackPropertiesDrawerData& d) {
            if (!engine_) return;
            auto clapNode = engine_->getTrackClapNode(tIdx);
            if (clapNode) {
                d.isClapPlugin = true;
                d.hasNativeGui = clapNode->hasGui();
                d.isGuiOpen = clapNode->isGuiOpen();
                d.presetTitle = clapNode->getPluginName().empty() ? d.instrument : clapNode->getPluginName();
                d.presetSubtitle = d.trackName + " • CLAP Instrument";
                d.instrument = d.presetTitle;
                d.instrumentEngine = "clap:" + clapNode->getPluginId();

                d.knobs.clear();
                const auto& params = clapNode->getParameterList();
                size_t limit = std::min<size_t>(params.size(), 24);
                for (size_t i = 0; i < limit; ++i) {
                    const auto& p = params[i];
                    TrackPropertiesKnob k;
                    k.name = p.name;
                    k.label = p.name;
                    double norm = 0.5;
                    if (p.maxValue > p.minValue) {
                        norm = (p.currentValue - p.minValue) / (p.maxValue - p.minValue);
                    }
                    k.value = static_cast<float>(std::clamp(norm, 0.0, 1.0));
                    k.display = std::to_string(static_cast<int>(p.currentValue));
                    d.knobs.push_back(k);
                }
            }
        };
        modularTrackInspectorView_->onOpenFullscreenAudioFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenFx(idx, static_cast<int>(fxIdx));
        };
        modularTrackInspectorView_->onOpenFullscreenMidiFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenMidiFx(idx, static_cast<int>(fxIdx));
        };
        modularTrackInspectorView_->onOpenDetachedWindow = [this](uint32_t idx) {
            openDetachedInstrument(idx);
        };
        modularTrackInspectorView_->getPanel().onOpenFullscreenDevice = [this](uint32_t idx) {
            openFullscreenDevice(idx);
        };
        modularTrackInspectorView_->getPanel().onOpenDetachedWindow = [this](uint32_t idx) {
            openDetachedInstrument(idx);
        };
        modularTrackInspectorView_->getPanel().onFocusDetachedWindow = [this](uint32_t idx) {
            FloatingWindowManager::instance().focusDetachedInstrument(idx);
        };
        modularTrackInspectorView_->getPanel().onReattachDetachedWindow = [this](uint32_t idx) {
            closeDetachedInstrument(idx);
        };
        modularTrackInspectorView_->getPanel().onOpenPluginGui = [this](uint32_t idx) {
            openDetachedInstrument(idx);
        };
        modularTrackInspectorView_->getPanel().onOpenFullscreenAudioFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenFx(idx, static_cast<int>(fxIdx));
        };
        modularTrackInspectorView_->getPanel().onOpenFullscreenMidiFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenMidiFx(idx, static_cast<int>(fxIdx));
        };
        modularTrackInspectorView_->onAddClip = [this](uint32_t idx) {
            addClipToTrack(idx);
        };
        modularTrackInspectorView_->onDeleteTrack = [this](uint32_t idx) {
            deleteTrack(idx);
        };
        modularTrackInspectorView_->onDuplicateTrack = [this](uint32_t idx) {
            duplicateTrack(idx);
        };
        modularTrackInspectorView_->onDuplicateClip = [this](uint32_t tIdx, int cIdx) {
            duplicateClip(tIdx, cIdx);
        };
        modularTrackInspectorView_->onDeleteClip = [this](uint32_t tIdx, int cIdx) {
            deleteClip(tIdx, cIdx);
        };
        modularTrackInspectorView_->onOpenDesign = [this](uint32_t idx) {
            openDesignForTrack(idx);
        };
        modularTrackInspectorView_->getPanel().onOpenDesign = [this](uint32_t idx) {
            openDesignForTrack(idx);
        };
        modularTrackInspectorView_->onInstrumentChanged = [this](uint32_t idx, const std::string& instId, const std::string& instName) {
            changeTrackInstrument(idx, instId, instName);
        };
    }

    if (modularArrangerView_) {
        modularArrangerView_->onOpenFullscreenDevice = [this](uint32_t idx) {
            openFullscreenDevice(idx);
        };
        modularArrangerView_->onOpenDetachedWindow = [this](uint32_t idx) {
            openDetachedInstrument(idx);
        };
        modularArrangerView_->onOpenFullscreenAudioFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenFx(idx, static_cast<int>(fxIdx));
        };
        modularArrangerView_->onOpenFullscreenMidiFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenMidiFx(idx, static_cast<int>(fxIdx));
        };
        modularArrangerView_->getPropertiesDrawer().onOpenFullscreenDevice = [this](uint32_t idx) {
            openFullscreenDevice(idx);
        };
        modularArrangerView_->getPropertiesDrawer().onOpenDetachedWindow = [this](uint32_t idx) {
            openDetachedInstrument(idx);
        };
        modularArrangerView_->getPropertiesDrawer().onOpenFullscreenAudioFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenFx(idx, static_cast<int>(fxIdx));
        };
        modularArrangerView_->getPropertiesDrawer().onOpenFullscreenMidiFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenMidiFx(idx, static_cast<int>(fxIdx));
        };
        modularArrangerView_->getPropertiesDrawer().getPanel().onOpenFullscreenDevice = [this](uint32_t idx) {
            openFullscreenDevice(idx);
        };
        modularArrangerView_->getPropertiesDrawer().getPanel().onOpenDetachedWindow = [this](uint32_t idx) {
            openDetachedInstrument(idx);
        };
        modularArrangerView_->getPropertiesDrawer().getPanel().onFocusDetachedWindow = [this](uint32_t idx) {
            FloatingWindowManager::instance().focusDetachedInstrument(idx);
        };
        modularArrangerView_->getPropertiesDrawer().getPanel().onReattachDetachedWindow = [this](uint32_t idx) {
            closeDetachedInstrument(idx);
        };
        modularArrangerView_->getPropertiesDrawer().getPanel().onOpenPluginGui = [this](uint32_t idx) {
            openDetachedInstrument(idx);
        };
        modularArrangerView_->getPropertiesDrawer().getPanel().onOpenFullscreenAudioFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenFx(idx, static_cast<int>(fxIdx));
        };
        modularArrangerView_->getPropertiesDrawer().getPanel().onOpenFullscreenMidiFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenMidiFx(idx, static_cast<int>(fxIdx));
        };
    }

    // High-performance Dedicated Fullscreen Device Modal Callbacks
    fullscreenDeviceModal_.onClose = [this]() {
        if (valueEditDialog_.isOpen()) {
            valueEditDialog_.close();
        }
        if (engine_) {
            const auto& tgt = fullscreenDeviceModal_.getTarget();
            if (tgt.type == DeviceTargetType::Instrument) {
                auto clapNode = engine_->getTrackClapNode(tgt.trackIndex);
                if (clapNode && clapNode->isEmbeddedGuiOpen()) {
                    clapNode->hideEmbeddedGui();
                }
            } else if (tgt.type == DeviceTargetType::AudioFx && tgt.fxIndex >= 0) {
                auto clapFxNode = engine_->getTrackAudioFxClapNode(tgt.trackIndex, static_cast<size_t>(tgt.fxIndex));
                if (clapFxNode && clapFxNode->isEmbeddedGuiOpen()) {
                    clapFxNode->hideEmbeddedGui();
                }
            }
        }
        fullscreenDeviceModal_.setIsGuiEmbedded(false);
        setStatusMessage("Device Full-Display Closed");
    };
    fullscreenDeviceModal_.onOpenPluginGui = [this](uint32_t idx) {
        if (!engine_) return;
        auto clapNode = engine_->getTrackClapNode(idx);
        if (!clapNode || !clapNode->hasGui()) return;

        if (fullscreenDeviceModal_.isGuiEmbedded()) {
            // User clicked DETACH -> Destroy embedded child, open floating window
            clapNode->destroyEmbeddedGui();
            clapNode->onGuiClosed = [idx]() {
                FloatingWindowManager::instance().closeDetachedInstrument(idx);
            };
            bool floatOk = clapNode->openGuiWindow();
            fullscreenDeviceModal_.setIsGuiEmbedded(false);
            if (floatOk) {
                FloatingWindowManager::instance().openDetachedInstrument(idx, clapNode->getPluginName(), GuiPanelDef{}, window_, true, clapNode->getGuiHwnd());
                setStatusMessage("Popped out plugin window: " + clapNode->getPluginName());
            } else {
                setStatusMessage("Could not float plugin window; keeping generic parameter inspector for " + clapNode->getPluginName());
            }
        } else {
            // User clicked EMBED -> Close floating window, dock into fullscreen view
            clapNode->closeGuiWindow();
#if defined(_WIN32)
            HWND mainHwnd = glfwGetWin32Window(window_);
            const auto& b = fullscreenDeviceModal_.getBodyBounds();
            bool embedOk = clapNode->openEmbeddedGui(mainHwnd,
                static_cast<int>(b.x), static_cast<int>(b.y),
                static_cast<int>(b.w), static_cast<int>(b.h));
            if (embedOk) {
                fullscreenDeviceModal_.setIsGuiEmbedded(true);
                setStatusMessage("Docked plugin into view: " + clapNode->getPluginName());
            } else {
                fullscreenDeviceModal_.setIsGuiEmbedded(false);
                setStatusMessage("Could not dock plugin GUI; falling back to generic parameter controls for " + clapNode->getPluginName());
            }
#endif
        }
    };
    fullscreenDeviceModal_.onOpenDetachedWindow = [this](uint32_t idx) {
        openDetachedInstrument(idx);
    };
    fullscreenDeviceModal_.onPrevPreset = [this]() {
        prevPreset();
        const auto& tgt = fullscreenDeviceModal_.getTarget();
        if (tgt.type == DeviceTargetType::AudioFx && tgt.fxIndex >= 0) {
            openFullscreenFx(tgt.trackIndex, tgt.fxIndex);
        } else if (tgt.type == DeviceTargetType::MidiFx && tgt.fxIndex >= 0) {
            openFullscreenMidiFx(tgt.trackIndex, tgt.fxIndex);
        } else {
            openFullscreenDevice(selectedTrackIndex_);
        }
    };
    fullscreenDeviceModal_.onNextPreset = [this]() {
        nextPreset();
        const auto& tgt = fullscreenDeviceModal_.getTarget();
        if (tgt.type == DeviceTargetType::AudioFx && tgt.fxIndex >= 0) {
            openFullscreenFx(tgt.trackIndex, tgt.fxIndex);
        } else if (tgt.type == DeviceTargetType::MidiFx && tgt.fxIndex >= 0) {
            openFullscreenMidiFx(tgt.trackIndex, tgt.fxIndex);
        } else {
            openFullscreenDevice(selectedTrackIndex_);
        }
    };
    fullscreenDeviceModal_.onOpenCodeEditor = [this](uint32_t idx) {
        (void)idx;
        setActiveView(WorkspaceView::Design);
        setDesignSubView(DesignSubView::Eatscript);
        if (modularDesignView_) {
            const auto& tgt = fullscreenDeviceModal_.getTarget();
            if (tgt.type == DeviceTargetType::AudioFx) {
                modularDesignView_->selectTargetByTrackAndType(static_cast<int>(tgt.trackIndex), ScriptTargetType::AudioFx, tgt.fxIndex);
            } else if (tgt.type == DeviceTargetType::MidiFx) {
                modularDesignView_->selectTargetByTrackAndType(static_cast<int>(tgt.trackIndex), ScriptTargetType::MidiFx, tgt.fxIndex);
            } else if (tgt.type == DeviceTargetType::Instrument) {
                modularDesignView_->selectTargetByTrackAndType(static_cast<int>(tgt.trackIndex), ScriptTargetType::TrackDsp);
            }
        }
    };
    fullscreenDeviceModal_.onOpenPresetDialog = [this](uint32_t idx) {
        const std::string& pTitle = fullscreenDeviceModal_.getTrackData().presetTitle;
        for (size_t i = 0; i < presets_.size(); ++i) {
            if (presets_[i].metadata.name == pTitle || presets_[i].metadata.id == pTitle) {
                loadPresetToSelectedTrack(i);
                break;
            }
        }
        setStatusMessage("Applied Preset: " + pTitle);
    };
    fullscreenDeviceModal_.onParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
        if (idx != selectedTrackIndex_) {
            setSelectedTrackIndex(idx);
            syncTrackToPreset(idx);
        }
        dispatchTrackParam(idx, paramName, normVal);
    };
    fullscreenDeviceModal_.onAuditionNote = [this](uint32_t idx, uint8_t note, float vel) {
        auditionTrackNote(idx, note, vel);
    };
    fullscreenDeviceModal_.onAudioFxParamChanged = [this](uint32_t idx, size_t fxIdx, const std::string& paramName, float normVal) {
        if (engine_) {
            auto clapFxNode = engine_->getTrackAudioFxClapNode(idx, fxIdx);
            if (clapFxNode) {
                const auto& params = clapFxNode->getParameterList();
                for (const auto& p : params) {
                    if (p.name == paramName) {
                        double realVal = p.minValue + static_cast<double>(normVal) * (p.maxValue - p.minValue);
                        clapFxNode->setParamValue(p.id, realVal);
                        return;
                    }
                }
            }
            engine_->setTrackAudioFxParam(idx, fxIdx, paramName, normVal);
        }
        dispatchHardwareParam(paramName, normVal);
    };
    fullscreenDeviceModal_.onMidiFxParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
        if (engine_) {
            auto* seqTrack = engine_->getSequencer().getTrack(idx);
            int fxIdx = fullscreenDeviceModal_.getTarget().fxIndex;
            if (seqTrack && fxIdx >= 0) {
                auto& rack = seqTrack->getMidiFxRack();
                if (static_cast<size_t>(fxIdx) < rack.size()) {
                    rack[fxIdx].params[paramName] = normVal;
                    std::string kLower = paramName;
                    for (char& c : kLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (kLower == "octaves" || kLower == "oct") {
                        int oct = std::clamp(static_cast<int>(std::round(normVal * 3.0f)) + 1, 1, 4);
                        rack[fxIdx].params["Octaves"] = static_cast<float>(oct);
                        rack[fxIdx].params["octaves"] = static_cast<float>(oct);
                    } else if (kLower == "semitones" || kLower == "transpose" || kLower == "trans") {
                        int semi = static_cast<int>(std::round((normVal - 0.5f) * 24.0f));
                        rack[fxIdx].params["semitones"] = static_cast<float>(semi);
                        rack[fxIdx].params["transpose"] = static_cast<float>(semi);
                    } else if (kLower == "root" || kLower == "rootkey") {
                        int rk = std::clamp(static_cast<int>(std::round(normVal * 11.0f)), 0, 11);
                        rack[fxIdx].params["rootKey"] = static_cast<float>(rk);
                    } else if (kLower == "scale" || kLower == "scalemode") {
                        int sm = std::clamp(static_cast<int>(std::round(normVal * 6.0f)), 0, 6);
                        rack[fxIdx].params["scaleMode"] = static_cast<float>(sm);
                    } else if (kLower == "pattern") {
                        int pat = std::clamp(static_cast<int>(std::round(normVal * 8.0f)), 0, 8);
                        rack[fxIdx].params["Pattern"] = static_cast<float>(pat);
                        rack[fxIdx].params["pattern"] = static_cast<float>(pat);
                    } else if (kLower == "rate") {
                        int rIdx = std::clamp(static_cast<int>(std::round(normVal * 3.0f)), 0, 3);
                        float rateSteps = 1.0f;
                        if (rIdx == 0) rateSteps = 4.0f;
                        else if (rIdx == 1) rateSteps = 2.0f;
                        else if (rIdx == 2) rateSteps = 1.0f;
                        else if (rIdx == 3) rateSteps = 0.5f;
                        rack[fxIdx].params["Rate"] = rateSteps;
                        rack[fxIdx].params["rate"] = rateSteps;
                    } else if (kLower == "gate") {
                        float g = std::clamp(0.1f + normVal * 1.9f, 0.1f, 2.0f);
                        rack[fxIdx].params["Gate"] = g;
                        rack[fxIdx].params["gate"] = g;
                    } else if (kLower == "swing") {
                        rack[fxIdx].params["Swing"] = normVal * 0.5f;
                        rack[fxIdx].params["swing"] = normVal * 0.5f;
                    } else if (kLower == "velocity") {
                        rack[fxIdx].params["humanizeVelocity"] = normVal;
                    } else if (kLower == "timing") {
                        rack[fxIdx].params["humanizeTiming"] = normVal;
                    }
                }
            }
        }
        int targetFxIdx = fullscreenDeviceModal_.getTarget().fxIndex;
        if (targetFxIdx >= 0) {
            if (modularTrackInspectorView_ && idx < modularTrackInspectorView_->getTracks().size()) {
                auto& fxList = modularTrackInspectorView_->getTracks()[idx].midiFxList;
                if (static_cast<size_t>(targetFxIdx) < fxList.size()) {
                    for (auto& k : fxList[targetFxIdx].knobs) {
                        if (k.name == paramName) { k.value = normVal; break; }
                    }
                }
            }
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size()) {
                auto& fxList = modularArrangerView_->getTracks()[idx].midiFx;
                if (static_cast<size_t>(targetFxIdx) < fxList.size()) {
                    for (auto& k : fxList[targetFxIdx].knobs) {
                        if (k.name == paramName) { k.value = normVal; break; }
                    }
                }
            }
        }
        syncTrackMidiFxToEngine(idx);
        syncArrangerToSequencer();
        dispatchHardwareParam(paramName, normVal);
    };
    fullscreenDeviceModal_.onOpenValueEdit = [this](const ValueEditRequest& req) {
        openValueEditDialog(req);
    };

    if (modularMixerView_) {
        modularMixerView_->onVolumeChanged = [this](uint32_t idx, float vol) {
            if (idx < mixerStrips_.size()) mixerStrips_[idx].volume = vol;
            if (idx < arrangerTracks_.size()) arrangerTracks_[idx].volume = vol;
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size()) {
                modularArrangerView_->getTracks()[idx].volume = vol;
            }
            if (engine_) engine_->setTrackVolume(idx, vol);
        };
        modularMixerView_->onPanChanged = [this](uint32_t idx, float pan) {
            if (idx < mixerStrips_.size()) mixerStrips_[idx].pan = pan;
            if (idx < arrangerTracks_.size()) arrangerTracks_[idx].pan = pan;
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size()) {
                modularArrangerView_->getTracks()[idx].pan = pan;
            }
            if (engine_) engine_->setTrackPan(idx, pan);
        };
        modularMixerView_->onMasterVolumeChanged = [this](float vol) {
            masterVolume_ = vol;
            if (engine_) engine_->setMasterVolume(masterMute_ ? 0.0f : vol);
        };
        modularMixerView_->onMasterPanChanged = [this](float pan) {
            setMasterPan(pan);
        };
        modularMixerView_->onMasterMuteToggled = [this](bool mute) {
            masterMute_ = mute;
            if (engine_) engine_->setMasterVolume(mute ? 0.0f : masterVolume_);
        };
        modularMixerView_->onPropertiesDrawerStateChanged = [this](bool exp, float w) {
            mixerPropertiesExpanded_ = exp;
            mixerPropertiesWidth_ = w;
        };
        modularMixerView_->onParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
            if (idx != selectedTrackIndex_) {
                setSelectedTrackIndex(idx);
            }
            syncTrackToPreset(idx);
            dispatchTrackParam(idx, paramName, normVal);
        };
        modularMixerView_->onAuditionNote = [this](uint32_t idx, uint8_t note, float vel) {
            auditionTrackNote(idx, note, vel);
        };
        modularMixerView_->onMuteToggled = [this](uint32_t idx, bool mute) {
            setTrackMuteState(idx, mute);
        };
        modularMixerView_->onSoloToggled = [this](uint32_t idx, bool solo) {
            setTrackSoloState(idx, solo);
        };
        modularMixerView_->onFreezeToggled = [this](uint32_t idx, bool freeze) {
            setTrackFreezeState(idx, freeze);
        };
        modularMixerView_->onTrackSelected = [this](uint32_t idx) {
            setSelectedTrackIndex(idx);
        };
        modularMixerView_->onTrackRename = [this](uint32_t idx, const std::string& newName) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].name = newName;
            }
            if (idx < mixerStrips_.size()) {
                mixerStrips_[idx].name = newName;
            }
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size()) {
                modularArrangerView_->getTracks()[idx].name = newName;
            }
            setStatusMessage("Track renamed: " + newName);
        };
        modularMixerView_->onChooseTrackIcon = [this](uint32_t idx) {
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size()) {
                const auto& trk = modularArrangerView_->getTracks()[idx];
                modularArrangerView_->getIconSearchDialog().open(trk.name, idx, trk.iconRef);
            }
        };
        modularMixerView_->onAudioFxChanged = [this](uint32_t idx) {
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size() &&
                idx < modularMixerView_->getChannels().size()) {
                modularArrangerView_->getTracks()[idx].audioFx = modularMixerView_->getChannels()[idx].audioFx;
            }
            syncTrackAudioFxToEngine(idx);
        };
        modularMixerView_->onMidiFxChanged = [this](uint32_t idx) {
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size() &&
                idx < modularMixerView_->getChannels().size()) {
                modularArrangerView_->getTracks()[idx].midiFx = modularMixerView_->getChannels()[idx].midiFx;
            }
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
        };
        modularMixerView_->onToggleAudioFx = [this](uint32_t idx, size_t fxIdx, bool en) {
            (void)fxIdx;
            (void)en;
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size() &&
                idx < modularMixerView_->getChannels().size()) {
                modularArrangerView_->getTracks()[idx].audioFx = modularMixerView_->getChannels()[idx].audioFx;
            }
            syncTrackAudioFxToEngine(idx);
        };
        modularMixerView_->onToggleMidiFx = [this](uint32_t idx, size_t fxIdx, bool en) {
            (void)fxIdx;
            (void)en;
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size() &&
                idx < modularMixerView_->getChannels().size()) {
                modularArrangerView_->getTracks()[idx].midiFx = modularMixerView_->getChannels()[idx].midiFx;
            }
            syncTrackMidiFxToEngine(idx);
            syncArrangerToSequencer();
        };
        modularMixerView_->onOpenFullscreenDevice = [this](uint32_t idx) {
            openFullscreenDevice(idx);
        };
        modularMixerView_->onOpenFullscreenAudioFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenFx(idx, static_cast<int>(fxIdx));
        };
        modularMixerView_->onOpenFullscreenMidiFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenMidiFx(idx, static_cast<int>(fxIdx));
        };
        modularMixerView_->getPropertiesDrawer().onOpenFullscreenDevice = [this](uint32_t idx) {
            openFullscreenDevice(idx);
        };
        modularMixerView_->getPropertiesDrawer().onOpenFullscreenAudioFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenFx(idx, static_cast<int>(fxIdx));
        };
        modularMixerView_->getPropertiesDrawer().onOpenFullscreenMidiFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenMidiFx(idx, static_cast<int>(fxIdx));
        };
        modularMixerView_->getPropertiesDrawer().getPanel().onOpenFullscreenDevice = [this](uint32_t idx) {
            openFullscreenDevice(idx);
        };
        modularMixerView_->getPropertiesDrawer().getPanel().onOpenPluginGui = [this](uint32_t idx) {
            openDetachedInstrument(idx);
        };
        modularMixerView_->getPropertiesDrawer().getPanel().onOpenFullscreenAudioFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenFx(idx, static_cast<int>(fxIdx));
        };
        modularMixerView_->getPropertiesDrawer().getPanel().onOpenFullscreenMidiFx = [this](uint32_t idx, size_t fxIdx) {
            openFullscreenMidiFx(idx, static_cast<int>(fxIdx));
        };
        modularMixerView_->onAddClip = [this](uint32_t idx) {
            addClipToTrack(idx);
        };
        modularMixerView_->onDeleteTrack = [this](uint32_t idx) {
            deleteTrack(idx);
        };
        modularMixerView_->onDuplicateTrack = [this](uint32_t idx) {
            duplicateTrack(idx);
        };
        modularMixerView_->getPropertiesDrawer().onAddClip = [this](uint32_t idx) {
            addClipToTrack(idx);
        };
        modularMixerView_->getPropertiesDrawer().onDeleteTrack = [this](uint32_t idx) {
            deleteTrack(idx);
        };
        modularMixerView_->getPropertiesDrawer().onDuplicateTrack = [this](uint32_t idx) {
            duplicateTrack(idx);
        };
        modularMixerView_->getPropertiesDrawer().onDuplicateClip = [this](uint32_t tIdx, int cIdx) {
            duplicateClip(tIdx, cIdx);
        };
        modularMixerView_->getPropertiesDrawer().onDeleteClip = [this](uint32_t tIdx, int cIdx) {
            deleteClip(tIdx, cIdx);
        };
        modularMixerView_->onOpenDesign = [this](uint32_t idx) {
            openDesignForTrack(idx);
        };
        modularMixerView_->getPropertiesDrawer().onOpenDesign = [this](uint32_t idx) {
            openDesignForTrack(idx);
        };
        modularMixerView_->onInstrumentChanged = [this](uint32_t idx, const std::string& instId, const std::string& instName) {
            changeTrackInstrument(idx, instId, instName);
        };
        modularMixerView_->onTrackChordFollowChanged = [this](uint32_t idx, theory::ChordFollowMode mode) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].chordFollowMode = mode;
                recordProjectHistory("Set Chord Follow on Track " + std::to_string(idx + 1), "CHORD");
            }
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size()) {
                modularArrangerView_->getTracks()[idx].chordFollowMode = mode;
                modularArrangerView_->syncDrawerDataForTrack(idx);
            }
            if (modularTrackInspectorView_ && idx < modularTrackInspectorView_->getTracks().size()) {
                modularTrackInspectorView_->getTracks()[idx].chordFollowMode = mode;
            }
            if (engine_) {
                auto& seq = engine_->getSequencer();
                if (auto* trk = seq.getTrack(idx)) {
                    trk->setChordFollowMode(mode);
                }
            }
            if (modularEditView_ && selectedTrackIndex_ == idx) {
                modularEditView_->setChordFollowMode(mode);
                if (modularArrangerView_) {
                    modularEditView_->setChordTrack(modularArrangerView_->getChordTrack());
                }
            }
        };
        modularMixerView_->onBakeChords = [this](uint32_t idx) {
            bakeTrackChordsAndFx(idx);
        };
    }

    if (modularEditView_) {
        modularEditView_->onSubViewChanged = [this](EditSubViewMode mode) {
            switch (mode) {
                case EditSubViewMode::PianoRoll: editSubView_ = EditSubView::PianoRoll; break;
                case EditSubViewMode::Tracker:   editSubView_ = EditSubView::Tracker; break;
                case EditSubViewMode::Score:     editSubView_ = EditSubView::Score; break;
                case EditSubViewMode::Script:    editSubView_ = EditSubView::Script; break;
            }
        };
        modularEditView_->onNotesChanged = [this](uint32_t tIdx, int cIdx) {
            if (!modularArrangerView_) return;
            auto& tracks = modularArrangerView_->getTracks();
            if (tIdx < tracks.size()) {
                int clipToUpdate = cIdx;
                if (clipToUpdate < 0 || static_cast<size_t>(clipToUpdate) >= tracks[tIdx].clips.size()) {
                    clipToUpdate = modularArrangerView_->getSelectedClipIndex();
                    if (clipToUpdate < 0 || static_cast<size_t>(clipToUpdate) >= tracks[tIdx].clips.size()) {
                        clipToUpdate = 0;
                    }
                }
                if (!tracks[tIdx].clips.empty() && static_cast<size_t>(clipToUpdate) < tracks[tIdx].clips.size()) {
                    modularEditView_->writeBackToArrangerClip(tracks[tIdx].clips[clipToUpdate]);
                    modularArrangerView_->updateClipDetectedChords(tracks[tIdx].clips[clipToUpdate]);
                }
            }
            if (engine_) {
                syncArrangerToSequencer();
            }
        };
        modularEditView_->onBakeRequested = [this](uint32_t tIdx, int cIdx) {
            (void)cIdx;
            bakeTrackChordsAndFx(tIdx);
        };
    }

    if (engine_->getSequencer().getNumTracks() == 2) {
        if (!engine_->getSequencer().getChordTrack().empty()) {
            modularArrangerView_->setChordTrack(engine_->getSequencer().getChordTrack());
        }
        syncArrangerFromSequencer();
    } else {
        syncArrangerToSequencer();
    }
    if (modularArrangerView_) {
        engine_->getSequencer().setChordTrack(modularArrangerView_->getChordTrack());
        if (modularEditView_) {
            modularEditView_->setChordTrack(modularArrangerView_->getChordTrack());
        }
    }
    syncAllTracksFxToEngine();
    syncActiveClipToEditView(0, 0);

    if (modularDesignView_) {
        modularDesignView_->onParamChanged = [this](const std::string& targetId, const std::string& paramName, float val) {
            if (targetId.rfind("track_", 0) == 0) {
                size_t mfxPos = targetId.find("_mfx_");
                size_t afxPos = targetId.find("_afx_");
                if (mfxPos != std::string::npos) {
                    try {
                        uint32_t tIdx = static_cast<uint32_t>(std::stoul(targetId.substr(6, mfxPos - 6)));
                        size_t fxIdx = static_cast<size_t>(std::stoul(targetId.substr(mfxPos + 5)));
                        if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
                            auto& trk = modularArrangerView_->getTracks()[tIdx];
                            if (fxIdx < trk.midiFx.size()) {
                                for (auto& k : trk.midiFx[fxIdx].knobs) {
                                    if (k.name == paramName) {
                                        k.value = val;
                                        break;
                                    }
                                }
                                syncTrackMidiFxToEngine(tIdx);
                                syncArrangerToSequencer();
                            }
                        }
                    } catch (...) {}
                    dispatchHardwareParam(paramName, val);
                    return;
                } else if (afxPos != std::string::npos) {
                    try {
                        uint32_t tIdx = static_cast<uint32_t>(std::stoul(targetId.substr(6, afxPos - 6)));
                        size_t fxIdx = static_cast<size_t>(std::stoul(targetId.substr(afxPos + 5)));
                        if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
                            auto& trk = modularArrangerView_->getTracks()[tIdx];
                            if (fxIdx < trk.audioFx.size()) {
                                for (auto& k : trk.audioFx[fxIdx].knobs) {
                                    if (k.name == paramName) {
                                        k.value = val;
                                        break;
                                    }
                                }
                                syncTrackAudioFxToEngine(tIdx);
                            }
                        }
                        if (engine_) {
                            engine_->setTrackAudioFxParam(tIdx, fxIdx, paramName, val);
                        }
                    } catch (...) {}
                    dispatchHardwareParam(paramName, val);
                    return;
                }
            }
            dispatchHardwareParam(paramName, val);
        };
        modularDesignView_->onCompileScript = [this](const std::string& targetId, const std::string& code) {
            setScriptCode(code);
            compileActiveScript();

            if (modularDesignView_) {
                const auto& target = modularDesignView_->getActiveTarget();
                const auto& editedGui = modularDesignView_->getGuiPanel();

                if (target.trackIndex >= 0 && static_cast<size_t>(target.trackIndex) < arrangerTracks_.size()) {
                    auto& trk = arrangerTracks_[target.trackIndex];
                    if (target.type == ScriptTargetType::TrackDsp) {
                        trk.customGuiPanel = editedGui;
                        if (modularArrangerView_ && target.trackIndex < static_cast<int>(modularArrangerView_->getTracks().size())) {
                            modularArrangerView_->getTracks()[target.trackIndex].customGuiPanel = editedGui;
                        }
                        if (modularTrackInspectorView_ && target.trackIndex < static_cast<int>(modularTrackInspectorView_->getTracks().size())) {
                            modularTrackInspectorView_->getTracks()[target.trackIndex].customGuiPanel = editedGui;
                        }
                    } else if (target.type == ScriptTargetType::MidiFx && target.fxIndex >= 0) {
                        if (modularArrangerView_ && target.trackIndex < static_cast<int>(modularArrangerView_->getTracks().size())) {
                            auto& aTrk = modularArrangerView_->getTracks()[target.trackIndex];
                            if (static_cast<size_t>(target.fxIndex) < aTrk.midiFx.size()) {
                                aTrk.midiFx[target.fxIndex].customGuiPanel = editedGui;
                            }
                        }
                    } else if (target.type == ScriptTargetType::AudioFx && target.fxIndex >= 0) {
                        if (modularArrangerView_ && target.trackIndex < static_cast<int>(modularArrangerView_->getTracks().size())) {
                            auto& aTrk = modularArrangerView_->getTracks()[target.trackIndex];
                            if (static_cast<size_t>(target.fxIndex) < aTrk.audioFx.size()) {
                                aTrk.audioFx[target.fxIndex].customGuiPanel = editedGui;
                            }
                        }
                    }

                    if (modularArrangerView_) {
                        modularArrangerView_->syncDrawerDataForTrack(static_cast<uint32_t>(target.trackIndex));
                        if (modularTrackInspectorView_) {
                            modularTrackInspectorView_->syncFromDrawerData(modularArrangerView_->getDrawerData());
                        }
                        fullscreenDeviceModal_.syncData(modularArrangerView_->getDrawerData());
                    }
                }
            }
        };
        modularDesignView_->onCopyToClipboard = [this](const std::string& text) {
#if EATS_HAS_GLFW
            if (window_) {
                glfwSetClipboardString(window_, text.c_str());
            }
#endif
        };
    }

    valueEditDialog_.onCopyToClipboard = [this](const std::string& text) {
#if EATS_HAS_GLFW
        if (window_) {
            glfwSetClipboardString(window_, text.c_str());
        }
#endif
    };

#if EATS_HAS_GLFW
#if defined(_WIN32)
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetProcessDpiAwarenessContextFunc = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto setDpiFunc = reinterpret_cast<SetProcessDpiAwarenessContextFunc>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setDpiFunc) {
            setDpiFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        } else {
            using SetProcessDPIAwareFunc = BOOL(WINAPI*)();
            auto fallbackFunc = reinterpret_cast<SetProcessDPIAwareFunc>(
                GetProcAddress(user32, "SetProcessDPIAware"));
            if (fallbackFunc) fallbackFunc();
        }
    }
#endif
    if (!glfwInit()) {
        std::cerr << "[GuiWindow] Warning: Could not initialize GLFW (running in headless mode)." << std::endl;
        return true;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

#if defined(__EMSCRIPTEN__)
    double initCssW = EM_ASM_DOUBLE({
        var c = document.getElementById('canvas-container');
        return (c && c.clientWidth > 0) ? c.clientWidth : (window.innerWidth || 1280);
    });
    double initCssH = EM_ASM_DOUBLE({
        var c = document.getElementById('canvas-container');
        return (c && c.clientHeight > 0) ? c.clientHeight : (window.innerHeight || 800);
    });
    if (initCssW > 200.0) windowWidth_ = static_cast<int>(std::round(initCssW));
    if (initCssH > 200.0) windowHeight_ = static_cast<int>(std::round(initCssH));
#endif

    window_ = glfwCreateWindow(windowWidth_, windowHeight_, title_.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "[GuiWindow] Warning: GLFW window creation failed (running in headless mode)." << std::endl;
        return true;
    }
    clipboardBridge_.setWindow(window_);

    GLFWimage icons[2];
    icons[0].width = kAppIcon48Width;
    icons[0].height = kAppIcon48Height;
    icons[0].pixels = const_cast<unsigned char*>(kAppIcon48Pixels);
    icons[1].width = kAppIcon32Width;
    icons[1].height = kAppIcon32Height;
    icons[1].pixels = const_cast<unsigned char*>(kAppIcon32Pixels);
    glfwSetWindowIcon(window_, 2, icons);

    glfwShowWindow(window_);

    glfwGetWindowSize(window_, &windowWidth_, &windowHeight_);
    glfwGetFramebufferSize(window_, &fbWidth_, &fbHeight_);
#if defined(__EMSCRIPTEN__)
    g_activeGuiWindowForWeb = this;
    double initDpr = EM_ASM_DOUBLE({ return window.devicePixelRatio || 1.0; });
    fbWidth_ = static_cast<int>(std::round(static_cast<double>(windowWidth_) * (hiDpiEnabled_ ? initDpr : 1.0)));
    fbHeight_ = static_cast<int>(std::round(static_cast<double>(windowHeight_) * (hiDpiEnabled_ ? initDpr : 1.0)));
    EM_ASM({
        var canvas = document.getElementById('canvas');
        if (canvas) {
            canvas.width = $0;
            canvas.height = $1;
        }
        if (!canvas) canvas = document.body;
        if (canvas && !canvas._eatsDropInitialized) {
            canvas._eatsDropInitialized = true;
            canvas.addEventListener('dragover', function(e) {
                e.preventDefault();
                e.stopPropagation();
                if (e.dataTransfer) {
                    e.dataTransfer.dropEffect = 'copy';
                }
            });
            canvas.addEventListener('drop', function(e) {
                e.preventDefault();
                e.stopPropagation();
                var files = e.dataTransfer ? e.dataTransfer.files : null;
                if (!files || files.length === 0) return;
                var rect = canvas.getBoundingClientRect ? canvas.getBoundingClientRect() : {left: 0, top: 0};
                var clientX = e.clientX - rect.left;
                var clientY = e.clientY - rect.top;

                for (var i = 0; i < files.length; i++) {
                    (function(file) {
                        var reader = new FileReader();
                        reader.onload = function(evt) {
                            var arrayBuffer = evt.target.result;
                            var uint8 = new Uint8Array(arrayBuffer);
                            var safeName = file.name.replace(/[^a-zA-Z0-9._-]/g, '_');
                            var vpath = '/tmp/' + safeName;
                            try {
                                FS.writeFile(vpath, uint8);
                                Module.ccall('eats_on_file_dropped_web', null,
                                    ['string', 'number', 'number'],
                                    [vpath, clientX, clientY]
                                );
                            } catch (err) {
                                console.error('Eatsbits Web Drop Error:', err);
                            }
                        };
                        reader.readAsArrayBuffer(file);
                    })(files[i]);
                }
            });
        }
    }, fbWidth_, fbHeight_);
#endif
    updateLogicalDimensions();
    initFontRenderer();

    uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
    uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));

    batchRenderer_ = std::make_unique<BatchRenderer2D>();
    batchRenderer_->setRenderScale(renderScale_, renderScale_);
    RenderBackendType backend = RenderBackendType::WebGPU;
    batchRenderer_->initialize(window_, physW, physH, backend);
    batchRenderer_->setAntiAliasingMode(antiAliasingMode_);
    g_activeBatchRenderer = batchRenderer_.get();

    batchRenderer_->setMonospaceGlyphHook([this](float x, float y, float w, float h, char32_t codepoint, uint32_t fgColor) -> bool {
        if (!fontRenderer_) return false;
        eatsbits::text::FontId fid = (fontRenderer_->fontMono != eatsbits::text::kInvalidFontId) ? fontRenderer_->fontMono : fontRenderer_->fontNormal;
        if (fid == eatsbits::text::kInvalidFontId) return false;
        if (codepoint <= 32) return true;

        char utf8[8] = {0};
        if (codepoint < 0x80) {
            utf8[0] = static_cast<char>(codepoint);
        } else if (codepoint < 0x800) {
            utf8[0] = static_cast<char>(0xC0 | (codepoint >> 6));
            utf8[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            utf8[0] = static_cast<char>(0xE0 | (codepoint >> 12));
            utf8[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            utf8[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x110000) {
            utf8[0] = static_cast<char>(0xF0 | (codepoint >> 18));
            utf8[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            utf8[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            utf8[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            return false;
        }

        if (!g_activeBatchRenderer && batchRenderer_) {
            g_activeBatchRenderer = batchRenderer_.get();
        }

        float fontSize = std::min(h * 0.90f, w / 0.585f);
        if (fontSize < 9.0f) fontSize = 9.0f;

        if (fontRenderer_->textShaper.shapeText(fontRenderer_->fontMgr, fid, fontSize, utf8, fontRenderer_->shapedRun)) {
            FT_Face face = fontRenderer_->fontMgr.getFtFace(fid);
            float ascender = fontSize * 0.82f;
            if (face && face->size) {
                float ftAsc = static_cast<float>(face->size->metrics.ascender) / 64.0f;
                if (ftAsc > 0.0f) ascender = ftAsc;
            }
            float totalW = fontRenderer_->shapedRun.totalAdvanceX;
            float startX = x + (w - totalW) * 0.5f;
            float baselineY = y + (h - (ascender * 0.7f)) * 0.5f + ascender * 0.7f;
            renderShapedGlyphs(*g_activeBatchRenderer, *fontRenderer_, fid, fontSize, startX, baselineY, fgColor);
            return true;
        }
        return false;
    });

#if defined(__EMSCRIPTEN__)
    if (dawnBridge_.initializeWeb(batchRenderer_->getNativeDevice(), batchRenderer_->getNativeSurface(), physW, physH)) {
        if (dawnBridge_.isNativeActive()) {
            std::cout << "[GuiWindow] Connected Google Dawn / WebGPU pipeline on Web." << std::endl;
            batchRenderer_->setDirectPresent(false);
            batchRenderer_->setCustomRenderTargetView(dawnBridge_.getDawTextureView());
        }
    }
#else
    if (dawnBridge_.initializeNative(window_, physW, physH)) {
        if (dawnBridge_.isNativeActive()) {
            std::cout << "[GuiWindow] Connected Google Dawn / WebGPU pipeline." << std::endl;
            batchRenderer_->setDirectPresent(false);
        }
    }
#endif

    // Store pointer for GLFW callbacks
    glfwSetWindowUserPointer(window_, this);

    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* w, int width, int height) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) {
            app->markNeedsRedraw();
            app->onFramebufferResize(width, height);
        }
    });

    glfwSetWindowSizeCallback(window_, [](GLFWwindow* w, int width, int height) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) {
            app->markNeedsRedraw();
            app->onWindowResize(width, height);
        }
    });

    glfwSetWindowRefreshCallback(window_, [](GLFWwindow* w) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) {
            app->markNeedsRedraw();
            if (!app->isRendering_) {
                app->renderFrame();
            }
        }
    });

    glfwSetWindowIconifyCallback(window_, [](GLFWwindow* w, int iconified) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app && !iconified) {
            app->markNeedsRedraw();
        }
    });

    glfwSetCursorPosCallback(window_, [](GLFWwindow* w, double xpos, double ypos) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) {
            app->markNeedsRedraw();
            float logX = app->windowToLogicalX(xpos);
            float logY = app->windowToLogicalY(ypos);
            if (app->is3dConsoleEnabled()) {
                app->transform3dMouseCoords(logX, logY, logX, logY);
            }
            app->remapCrtMouseCoords(logX, logY, logX, logY);
            app->onMouseMove(logX, logY);
        }
    });

    glfwSetMouseButtonCallback(window_, [](GLFWwindow* w, int button, int action, int /*mods*/) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (!app) return;
        app->markNeedsRedraw();
        double x, y;
        glfwGetCursorPos(w, &x, &y);
        float logX = app->windowToLogicalX(x);
        float logY = app->windowToLogicalY(y);
        if (app->is3dConsoleEnabled()) {
            app->transform3dMouseCoords(logX, logY, logX, logY);
        }
        app->remapCrtMouseCoords(logX, logY, logX, logY);
        if (action == GLFW_PRESS) {
            app->onMouseDown(button, logX, logY);
        } else if (action == GLFW_RELEASE) {
            app->onMouseUp(button, logX, logY);
        }
    });

    glfwSetKeyCallback(window_, [](GLFWwindow* w, int key, int /*scancode*/, int action, int mods) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) {
            app->markNeedsRedraw();
            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                app->onKeyDown(key, mods);
            }
        }
    });

    glfwSetCharCallback(window_, [](GLFWwindow* w, unsigned int codepoint) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) {
#if defined(__EMSCRIPTEN__)
            // If soft keyboard proxy is active, typographic characters are streamed
            // exclusively via eats_on_web_char_input to avoid duplicate entry.
            int proxyActive = EM_ASM_INT({
                return (window.eatsbitsSoftKeyboardActive && document.activeElement === document.getElementById('mobile-text-proxy')) ? 1 : 0;
            });
            if (proxyActive) return;
#endif
            app->markNeedsRedraw();
            app->onChar(codepoint);
        }
    });

    glfwSetScrollCallback(window_, [](GLFWwindow* w, double xoffset, double yoffset) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) {
            app->markNeedsRedraw();
            app->onMouseScroll(xoffset, yoffset);
        }
    });

    glfwSetDropCallback(window_, [](GLFWwindow* w, int count, const char** paths) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (!app || count <= 0 || !paths) return;
        app->markNeedsRedraw();
        double x, y;
        glfwGetCursorPos(w, &x, &y);
        float logX = app->windowToLogicalX(x);
        float logY = app->windowToLogicalY(y);
        if (app->is3dConsoleEnabled()) {
            app->transform3dMouseCoords(logX, logY, logX, logY);
        }
        app->remapCrtMouseCoords(logX, logY, logX, logY);
        std::vector<std::string> fileList;
        fileList.reserve(count);
        for (int i = 0; i < count; ++i) {
            if (paths[i]) fileList.emplace_back(paths[i]);
        }
        app->onFilesDropped(fileList, logX, logY);
    });
#endif

    scalarDragPresenter_.setOnMouseCapture([this](bool capture, float rx, float ry) {
        if (window_) {
            if (capture) {
                glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                glfwSetCursorPos(window_, static_cast<double>(rx * uiScale_), static_cast<double>(ry * uiScale_));
            }
        }
    });

    return true;
}

ViewContext GuiWindow::createViewContext() noexcept {
    ViewContext ctx;
    ctx.renderer = batchRenderer_.get();
    ctx.theme = (activeFrameTheme_.backgroundDark.a > 0.0f) ? &activeFrameTheme_ : &getTheme();
    ctx.audioEngine = engine_;
    ctx.clipboard = &clipboardBridge_;
    ctx.focusManager = &focusManager_;
    ctx.screenWidth = static_cast<float>(width_);
    ctx.screenHeight = static_cast<float>(height_);
    ctx.logicalWidth = static_cast<float>(width_);
    ctx.logicalHeight = static_cast<float>(height_);
    ctx.uiScale = renderScale_;
#if defined(__EMSCRIPTEN__)
    ctx.isMobile = EM_ASM_INT({
        var isTouch = ('ontouchstart' in window) || (navigator.maxTouchPoints > 0) || (navigator.msMaxTouchPoints > 0);
        var isSmallScreen = window.innerWidth <= 800;
        var isMobAgent = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
        return (isTouch || isMobAgent || isSmallScreen) ? 1 : 0;
    }) != 0;
#else
    ctx.isMobile = (width_ < 768);
#endif
    ctx.dt = (frameDt_ > 0.0001f) ? frameDt_ : 0.0166f;
    ctx.time.deltaTime = static_cast<double>(ctx.dt);
    ctx.time.songTimeSeconds = telemetryPresenter_.getTransport().songTimeSeconds;
    ctx.time.frameIndex = frameIndex_++;
    ctx.time.isOfflineExport = false;
    ctx.mouseX = mouseX_;
    ctx.mouseY = mouseY_;
    ctx.browserOffset = (projectBrowserDrawerWidget_ && (browserOpen_ || projectBrowserDrawerWidget_->getAnimOffset() < ProjectBrowserDrawer::getDrawerWidth()))
        ? projectBrowserDrawerWidget_->getEffectiveWidth() : 0.0f;
    ctx.onNavigateTab = [this](WorkspaceView v) {
        if (v == WorkspaceView::Edit && modularEditView_) {
            modularEditView_->autoCenterOnNotesOrDefault();
        }
        setActiveView(v);
    };
    ctx.onJumpToClipEdit = [this](uint32_t tIdx, int cIdx) {
        setSelectedTrackIndex(tIdx);
        syncActiveClipToEditView(tIdx, cIdx);
        if (modularEditView_) {
            modularEditView_->autoCenterOnNotesOrDefault();
        }
        setActiveView(WorkspaceView::Edit);
    };
    ctx.onToggleBrowser = [this](bool open) { setBrowserOpen(open); };
    ctx.onShowNotification = [this](const std::string& msg) { setStatusMessage(msg); };
    ctx.onOpenValueEdit = [this](const ValueEditRequest& req) { openValueEditDialog(req); };
    return ctx;
}

void GuiWindow::pollWebResize() noexcept {
#if defined(__EMSCRIPTEN__)
    if (!window_) return;

    double cssW = EM_ASM_DOUBLE({
        var container = document.getElementById('canvas-container');
        return (container && container.clientWidth > 0) ? container.clientWidth : (window.innerWidth || 1280);
    });
    double cssH = EM_ASM_DOUBLE({
        var container = document.getElementById('canvas-container');
        return (container && container.clientHeight > 0) ? container.clientHeight : (window.innerHeight || 800);
    });
    double dpr = EM_ASM_DOUBLE({
        return window.devicePixelRatio || 1.0;
    });

    int targetW = static_cast<int>(std::round(cssW));
    int targetH = static_cast<int>(std::round(cssH));

    if (targetW < 320) targetW = 320;
    if (targetH < 240) targetH = 240;

    float expectedDpi = hiDpiEnabled_ ? static_cast<float>(dpr) : 1.0f;
    int targetFbW = static_cast<int>(std::round(static_cast<float>(targetW) * expectedDpi));
    int targetFbH = static_cast<int>(std::round(static_cast<float>(targetH) * expectedDpi));

    if (targetW != windowWidth_ || targetH != windowHeight_ || targetFbW != fbWidth_ || targetFbH != fbHeight_) {
        glfwSetWindowSize(window_, targetW, targetH);
        windowWidth_ = targetW;
        windowHeight_ = targetH;
        fbWidth_ = targetFbW;
        fbHeight_ = targetFbH;

        EM_ASM({
            var canvas = document.getElementById('canvas');
            if (canvas) {
                canvas.width = $0;
                canvas.height = $1;
            }
        }, fbWidth_, fbHeight_);

        onWindowResize(targetW, targetH);
        onFramebufferResize(fbWidth_, fbHeight_);
    }
#endif
}

bool GuiWindow::isAnyDrawerAnimating() const noexcept {
    if (std::abs(virtualKeyboardAnimProgress_ - (virtualKeyboardDrawerOpen_ ? 1.0f : 0.0f)) > 0.001f) {
        return true;
    }
    if (projectBrowserDrawerWidget_ && projectBrowserDrawerWidget_->isAnimating()) {
        return true;
    }
    if (modularArrangerView_ && modularArrangerView_->getPropertiesDrawer().isAnimating()) {
        return true;
    }
    if (modularMixerView_ && modularMixerView_->getPropertiesDrawer().isAnimating()) {
        return true;
    }
    return false;
}

bool GuiWindow::isIdle() const noexcept {
    if (!engine_) return true;
    if (crtShaderEnabled_) return false;
    if (audio::ClapAudioNode::isAnyGuiOpen()) return false;
    if (engine_->getSequencer().isPlaying()) return false;
    if (!telemetryPresenter_.isSettled()) return false;
    if (activeDragHandler_ != nullptr) return false;
    if (fullscreenDeviceModal_.isOpen()) return false;
    if (FloatingWindowManager::instance().getOpenCount() > 0) return false;
    if (valueEditDialog_.isOpen()) return false;
    if (commandPaletteDialog_.isOpen()) return false;
    if (audioToMidiDialog_.isOpen()) return false;
    if (presetSearchDialog_.isOpen()) return false;
    if (isAnyDrawerAnimating()) return false;
    return true;
}

void GuiWindow::runEventLoop() {
#if defined(__EMSCRIPTEN__)
    if (!window_) return;
    std::cout << "[GUI] Registering requestAnimationFrame main loop with responsive auto-resize..." << std::endl;
    pollWebResize();
    emscripten_set_main_loop_arg([](void* arg) {
        auto* self = static_cast<GuiWindow*>(arg);
        if (self && self->isOpen()) {
            audio::ClapAudioNode::pollAllMainThreadCallbacks();
            self->pollWebResize();
            glfwPollEvents();
            self->renderFrame();
        }
    }, this, 0, 0);
#elif EATS_HAS_GLFW
    if (!window_) return;

    using clock = std::chrono::steady_clock;
    constexpr auto targetFrameDuration = std::chrono::microseconds(16666); // ~60 FPS smooth frame pacing

    while (!glfwWindowShouldClose(window_)) {
        audio::ClapAudioNode::pollAllMainThreadCallbacks();

#if defined(_WIN32)
        MSG winMsg;
        while (PeekMessageA(&winMsg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&winMsg);
            DispatchMessageA(&winMsg);
        }
#endif

        // If window is minimized or has zero framebuffer size, avoid presenting to GPU swapchain
        if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED)) {
            glfwWaitEventsTimeout(0.05);
            continue;
        }

        int curFbW = 0, curFbH = 0;
        glfwGetFramebufferSize(window_, &curFbW, &curFbH);
        if (curFbW <= 0 || curFbH <= 0) {
            glfwWaitEventsTimeout(0.05);
            continue;
        }

        bool idle = isIdle();
        if (idle && !needsRedraw_) {
            // Zero-CPU idle sleep: suspend into OS event wait for up to 50ms (wakes immediately on user input/event)
            glfwWaitEventsTimeout(0.05);
        } else {
            glfwPollEvents();
        }

        if (!idle || needsRedraw_) {
            auto frameStart = clock::now();
            needsRedraw_ = false;
            renderFrame();

            auto frameEnd = clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart);
            if (elapsed < targetFrameDuration) {
                std::this_thread::sleep_for(targetFrameDuration - elapsed);
            }
        }
    }
#endif
}

void GuiWindow::close() noexcept {
#if EATS_HAS_GLFW
    dawnBridge_.shutdownNative();
    if (g_activeBatchRenderer == batchRenderer_.get()) {
        g_activeBatchRenderer = nullptr;
    }
    if (g_activeFontRenderer == fontRenderer_.get()) {
        g_activeFontRenderer = nullptr;
    }
    batchRenderer_.reset();
    fontRenderer_.reset();
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
#endif
}

bool GuiWindow::isOpen() const noexcept {
#if EATS_HAS_GLFW
    if (window_) {
        return !glfwWindowShouldClose(window_);
    }
#endif
    return false;
}

void GuiWindow::invalidateChromeCache() noexcept {
    if (batchRenderer_) {
        batchRenderer_->invalidateChromeCache();
    }
}

bool GuiWindow::isChromeCacheValid() const noexcept {
    return batchRenderer_ ? batchRenderer_->isChromeCacheValid() : false;
}

void GuiWindow::setChromeCachingEnabled(bool enable) noexcept {
    if (batchRenderer_) {
        batchRenderer_->setChromeCachingEnabled(enable);
    }
}

bool GuiWindow::isChromeCachingEnabled() const noexcept {
    return batchRenderer_ ? batchRenderer_->isChromeCachingEnabled() : false;
}

void GuiWindow::renderStaticChrome(const ThemeTokens& theme, float width, float height) {
    if (!batchRenderer_) return;
    const float r = width;
    const bool is3d = is3dConsoleEnabled();
    const float bPanelY = height - 48.0f;
    const float bPanelH = 48.0f;

    // 1. Overall window background gradient
    drawRectGradient(0.0f, 0.0f, width, height,
                     theme.backgroundDark, theme.backgroundDark.darken(0.04f));

    // 2. TOP TRANSPORT HEADER BAR (Height = 56px) - Heavy Machined Metal Faceplate
    drawChassisPlate(0.0f, 0.0f, r, 56.0f, true, theme);

    // Top specular rim highlight & uniform bottom machined bevel
    drawLine(0.0f, 0.0f, r, 0.0f, theme.borderSubtle.lighten(0.15f), 1.5f);
    drawLine(0.0f, 1.5f, r, 1.5f, theme.borderSubtle, 1.0f);
    drawLine(0.0f, 54.5f, r, 54.5f, theme.borderSubtle.darken(0.10f), 1.0f);
    drawLine(0.0f, 56.0f, r, 56.0f, theme.backgroundDark, 1.5f);

    // Industrial Hex Corner Fasteners
    auto drawRivet = [&](float rx, float ry) {
        drawCircle(rx, ry, 5.0f, theme.backgroundDark.darken(0.15f));
        drawCircle(rx, ry, 3.5f, theme.borderSubtle);
        drawLine(rx - 2.0f, ry, rx + 2.0f, ry, theme.backgroundDark, 1.2f);
        drawLine(rx, ry - 2.0f, rx, ry + 2.0f, theme.backgroundDark, 1.2f);
    };
    drawRivet(r - 8.0f, 8.0f);

    // Deep recessed perimeter trench housing clustered transport keys
    const float tX = 54.0f;
    const float tY = 10.0f;
    const float btnW = 38.0f;
    const float btnH = 36.0f;
    const float clusterW = btnW * 3.0f;
    drawRoundedRect(tX - 2.5f, tY - 2.5f, clusterW + 5.0f, btnH + 5.0f, 4.0f, theme.controlWell.darken(0.20f));
    drawRoundedRectOutline(tX - 2.5f, tY - 2.5f, clusterW + 5.0f, btnH + 5.0f, 4.0f, theme.borderSubtle.darken(0.20f), 1.0f);

    // 3. BOTTOM HARDWARE NAVIGATION CONTROL STRIP
    const float chinTopY = is3d ? (bPanelY - 14.0f) : bPanelY;
    const float chinTotalH = is3d ? (bPanelH + 14.0f) : bPanelH;
    drawChassisPlate(0.0f, chinTopY, width, chinTotalH, false, theme);
    drawLine(0.0f, chinTopY, width, chinTopY, theme.borderSubtle.lighten(0.15f), 1.5f);
    drawLine(0.0f, chinTopY + 1.5f, width, chinTopY + 1.5f, theme.backgroundDark, 1.0f);
}

void GuiWindow::drawTopTransportBar() {
#if EATS_HAS_GLFW
    const auto& theme = getTheme();
    const float r = static_cast<float>(width_);

    // =========================================================================
    // =========================================================================
    // TOP TRANSPORT HEADER BAR (Height = 56px) - Heavy Machined Metal Faceplate
    // =========================================================================
    const float tX = 54.0f;
    const float tY = 10.0f;
    const float btnW = 38.0f;
    const float btnH = 36.0f;
    const float clusterW = btnW * 3.0f; // 114px

    if (batchRenderer_ && batchRenderer_->isChromeCacheValid()) {
        batchRenderer_->drawCachedChromeRegion(0.0f, 0.0f, 0.0f, 0.0f, r, 56.0f);
    } else {
        // Procedural Theme-Tinted Grungy Metal Chassis Faceplate (Shader-Off & Base)
        drawChassisPlate(0.0f, 0.0f, r, 56.0f, true, theme);

        // Top specular rim highlight & uniform bottom machined bevel
        drawLine(0.0f, 0.0f, r, 0.0f, theme.borderSubtle.lighten(0.15f), 1.5f);
        drawLine(0.0f, 1.5f, r, 1.5f, theme.borderSubtle, 1.0f);
        drawLine(0.0f, 54.5f, r, 54.5f, theme.borderSubtle.darken(0.10f), 1.0f);
        drawLine(0.0f, 56.0f, r, 56.0f, theme.backgroundDark, 1.5f);

        // Industrial Hex Corner Fasteners
        auto drawRivet = [&](float rx, float ry) {
            drawCircle(rx, ry, 5.0f, theme.backgroundDark.darken(0.15f));
            drawCircle(rx, ry, 3.5f, theme.borderSubtle);
            drawLine(rx - 2.0f, ry, rx + 2.0f, ry, theme.backgroundDark, 1.2f);
            drawLine(rx, ry - 2.0f, rx, ry + 2.0f, theme.backgroundDark, 1.2f);
        };
        drawRivet(r - 8.0f, 8.0f);

        // Deep recessed perimeter trench housing the clustered transport keys
        drawRoundedRect(tX - 2.5f, tY - 2.5f, clusterW + 5.0f, btnH + 5.0f, 4.0f, theme.controlWell.darken(0.20f));
        drawRoundedRectOutline(tX - 2.5f, tY - 2.5f, clusterW + 5.0f, btnH + 5.0f, 4.0f, theme.borderSubtle.darken(0.20f), 1.0f);
    }

    const bool isPlaying = engine_ ? engine_->getSequencer().isPlaying() : false;

    // 1. EATSBITS BRAND EMBLEM LOGO (Unboxed plain silkscreen logo directly on metal faceplate)
    const float logoX = 14.0f;
    const float logoY = 16.0f;
    const float logoW = 27.0f;
    const float logoH = 24.0f;
    const bool logoHovered = (mouseX_ >= 8.0f && mouseX_ <= 50.0f &&
                              mouseY_ >= 8.0f && mouseY_ <= 48.0f);
    Color logoColor = logoHovered ? theme.primaryAccent.lighten(0.18f) : theme.primaryAccent;
    drawPlainEatsbitsLogo(logoX, logoY, logoW, logoH, logoColor, 1.0f, logoHovered, theme.backgroundDark);

    // Keycap base colors derived dynamically from theme primary accent
    Color baseCapTop = theme.isLight ? theme.primaryAccent : theme.primaryAccent.darken(0.16f);
    Color baseCapBot = theme.isLight ? baseCapTop.darken(0.15f) : baseCapTop.darken(0.24f);

    // Helper for rendering realistic tactile keycaps with smooth corner radii and bevels
    auto drawTransportKeyBase = [&](float bx, float by, float bw, float bh, bool pressed, int keyPos) {
        float po = pressed ? 2.0f : 0.0f;
        Color capTop = pressed ? baseCapTop.darken(0.14f) : baseCapTop;
        Color capBot = pressed ? baseCapBot.darken(0.14f) : baseCapBot;

        float cornerRadius = 3.5f;
        if (keyPos == 0) {
            // Left key: rounded left corners, square right
            drawRoundedRectGradient(bx, by + po, bw, bh - po, cornerRadius, capTop, capBot);
            drawRectGradient(bx + bw * 0.5f, by + po, bw * 0.5f, bh - po, capTop, capBot);
        } else if (keyPos == 2) {
            // Right key: square left corners, rounded right
            drawRoundedRectGradient(bx, by + po, bw, bh - po, cornerRadius, capTop, capBot);
            drawRectGradient(bx, by + po, bw * 0.5f, bh - po, capTop, capBot);
        } else {
            // Center key: square
            drawRectGradient(bx, by + po, bw, bh - po, capTop, capBot);
        }
    };

    auto drawTransportKeyOverlay = [&](float bx, float by, float bw, float bh, bool pressed, int keyPos) {
        float po = pressed ? 2.0f : 0.0f;
        Color capTop = pressed ? baseCapTop.darken(0.14f) : baseCapTop;
        Color capBot = pressed ? baseCapBot.darken(0.14f) : baseCapBot;

        float cornerRadius = 3.5f;
        float cr = (keyPos == 1) ? 0.0f : cornerRadius;
        bool rTL = (keyPos == 0);
        bool rBL = (keyPos == 0);
        bool rTR = (keyPos == 2);
        bool rBR = (keyPos == 2);

        // Tactile micro-noise overlay applied across keycap AND printed icon content!
        drawButtonNoise(bx, by + po, bw, bh - po, true, 0.28f, cr, rTL, rTR, rBL, rBR, pressed, 3.0f);

        // Multi-layer 3D Chamfer Bevels
        Color topBevel = capTop.lighten(0.22f);
        Color botShadow = capBot.darken(0.22f);
        drawLine(bx + 1.0f, by + po + 1.0f, bx + bw - 1.0f, by + po + 1.0f, topBevel, 1.5f);
        drawLine(bx + 1.0f, by + bh - 1.0f, bx + bw - 1.0f, by + bh - 1.0f, botShadow, 1.5f);

        // Vertical hairline shadow outline
        drawRectOutline(bx, by + po, bw, bh - po, capBot.darken(0.15f), 1.0f);
    };

    // Dark screen-printed icon color providing clean tactile contrast against keycaps
    Color darkIcon = theme.isLight ? theme.textPrimary : Color(0.14f, 0.12f, 0.10f);

    // PLAY KEY (Key 0)
    drawTransportKeyBase(tX, tY, btnW, btnH, isPlaying, 0);
    // Dark Charcoal / Graphite Screen-Printed Triangle Icon
    float plX = tX + 13.5f;
    float plY = tY + 11.0f + (isPlaying ? 2.0f : 0.0f);
    drawTriangle(plX, plY, plX + 13.0f, plY + 7.0f, plX, plY + 14.0f, darkIcon.r, darkIcon.g, darkIcon.b, 0.92f);
    drawTransportKeyOverlay(tX, tY, btnW, btnH, isPlaying, 0);

    // Tight Division Crevice between Play & Stop
    drawLine(tX + btnW, tY, tX + btnW, tY + btnH, theme.controlWell.darken(0.35f), 1.5f);

    // STOP KEY (Key 1)
    drawTransportKeyBase(tX + btnW, tY, btnW, btnH, !isPlaying, 1);
    // Dark Charcoal / Graphite Screen-Printed Square Icon
    float stX = tX + btnW + 12.5f;
    float stY = tY + 11.5f + (!isPlaying ? 2.0f : 0.0f);
    drawRect(stX, stY, 13.0f, 13.0f, darkIcon.r, darkIcon.g, darkIcon.b, 0.92f);
    drawTransportKeyOverlay(tX + btnW, tY, btnW, btnH, !isPlaying, 1);

    // Tight Division Crevice between Stop & Record
    drawLine(tX + btnW * 2.0f, tY, tX + btnW * 2.0f, tY + btnH, theme.controlWell.darken(0.35f), 1.5f);

    // RECORD KEY (Key 2)
    drawTransportKeyBase(tX + btnW * 2.0f, tY, btnW, btnH, false, 2);
    // Screen-Printed Deep Vermilion / Crimson Circle Dot (Themed)
    float recX = tX + btnW * 2.0f + 19.0f;
    float recY = tY + 18.0f;
    drawCircleOutline(recX, recY, 6.5f, theme.recordActive.darken(0.30f), 1.2f);
    drawCircle(recX, recY, 6.5f, theme.recordActive.withAlpha(0.92f));
    drawTransportKeyOverlay(tX + btnW * 2.0f, tY, btnW, btnH, false, 2);

    // =========================================================================
    // 3. MINIMAL RECESSED READOUT PODS (BPM & TIMECODE)
    // =========================================================================
    // Helper 1: Recessed dark trench and smoked substrate (behind the content)
    auto drawReadoutPodBase = [&](float podX, float podY, float podW, float podH) {
        drawRoundedRect(podX - 2.5f, podY - 2.5f, podW + 5.0f, podH + 5.0f, 6.0f, theme.controlWell.darken(0.20f));
        drawRoundedRectOutline(podX - 2.5f, podY - 2.5f, podW + 5.0f, podH + 5.0f, 6.0f, theme.borderSubtle.darken(0.20f), 1.0f);
        drawRoundedRect(podX, podY, podW, podH, 4.5f, theme.lcdBackground);
    };

    // Helper 2: Subtle lens reflection gradient rendered IN FRONT of the content
    // Matching CSS: linear-gradient(to bottom, 0% rgba(226,226,226), 50% rgba(219,219,219), 51% rgba(209,209,209), 100% rgba(254,254,254))
    // tinted with theme.lcdBackground
    auto drawReadoutPodGlassOverlay = [&](float podX, float podY, float podW, float podH) {
        const Color& B = theme.lcdBackground;
        float halfH = podH * 0.5f;

        // Top half specular reflection: 0% to 50%
        Color gradTop0 = Color(std::clamp(B.r * 1.20f + 0.16f, 0.0f, 1.0f),
                               std::clamp(B.g * 1.20f + 0.16f, 0.0f, 1.0f),
                               std::clamp(B.b * 1.20f + 0.16f, 0.0f, 1.0f), 0.18f);
        Color gradTop1 = Color(std::clamp(B.r * 1.05f + 0.06f, 0.0f, 1.0f),
                               std::clamp(B.g * 1.05f + 0.06f, 0.0f, 1.0f),
                               std::clamp(B.b * 1.05f + 0.06f, 0.0f, 1.0f), 0.07f);
        drawRoundedRectGradient(podX, podY, podW, halfH + 1.0f, 4.5f, gradTop0, gradTop1);
        drawRectGradient(podX, podY + 4.5f, podW, halfH - 3.5f, gradTop0, gradTop1);

        // Sharp specular split horizon crease at 50%/51%
        drawLine(podX + 2.0f, podY + halfH, podX + podW - 2.0f, podY + halfH, Color(0.0f, 0.0f, 0.0f, 0.30f), 1.0f);

        // Bottom half specular refraction: 51% to 100%
        Color gradBot0 = Color(B.r * 0.85f, B.g * 0.85f, B.b * 0.85f, 0.04f);
        Color gradBot1 = Color(std::clamp(B.r * 1.35f + 0.22f, 0.0f, 1.0f),
                               std::clamp(B.g * 1.35f + 0.22f, 0.0f, 1.0f),
                               std::clamp(B.b * 1.35f + 0.22f, 0.0f, 1.0f), 0.24f);
        drawRoundedRectGradient(podX, podY + halfH, podW, halfH, 4.5f, gradBot0, gradBot1);
        drawRectGradient(podX, podY + halfH, podW, halfH - 4.5f, gradBot0, gradBot1);

        // Smoked acrylic glass outer lens bezel rim
        drawRoundedRectOutline(podX, podY, podW, podH, 4.5f, theme.lcdBorder.withAlpha(0.65f), 1.0f);

        // Top subtle inner shadow line
        drawLine(podX + 2.0f, podY + 1.0f, podX + podW - 2.0f, podY + 1.0f, Color(0.0f, 0.0f, 0.0f, 0.55f), 1.5f);

        // Bottom specular flare reflection line
        drawLine(podX + 2.0f, podY + podH - 1.0f, podX + podW - 2.0f, podY + podH - 1.0f,
                 Color(1.0f, 1.0f, 1.0f, 0.20f), 1.0f);
    };

    // Shared readout palette (both readouts in theme.lcdText with subtle, delicate phosphor bloom)
    Color textColor = theme.lcdText;
    Color glowColor = theme.tempoGlow;

    // --- POD 1: BPM READOUT CAPSULE (Center-Left) ---
    const float bpmPodX = tX + clusterW + 14.0f; // ~182px
    const float bpmPodY = 10.0f;
    const float bpmPodW = 112.0f;
    const float bpmPodH = 36.0f;

    // 1. Base substrate
    drawReadoutPodBase(bpmPodX, bpmPodY, bpmPodW, bpmPodH);

    int bpmVal = engine_ ? static_cast<int>(std::round(engine_->getSequencer().getTransport().getBpm())) : 120;
    char bpmBuf[32];
    std::snprintf(bpmBuf, sizeof(bpmBuf), "%d BPM", bpmVal);
    std::string_view bpmStr(bpmBuf);

    float bpmTextX = bpmPodX + (bpmPodW - static_cast<float>(bpmStr.size()) * 11.5f) * 0.5f;
    float bpmTextY = bpmPodY + 8.5f;

    // 2. Soft, delicate ambient bloom behind digits
    float bpmCx = bpmPodX + bpmPodW * 0.5f;
    float bpmCy = bpmPodY + bpmPodH * 0.5f;
    drawRoundedRect(bpmCx - 30.0f, bpmCy - 9.0f, 60.0f, 18.0f, 5.0f,
                    Color(glowColor.r, glowColor.g, glowColor.b, 0.045f));
    drawRoundedRect(bpmCx - 18.0f, bpmCy - 6.0f, 36.0f, 12.0f, 4.0f,
                    Color(glowColor.r, glowColor.g, glowColor.b, 0.065f));

    // 3. Subtle character optical bloom halo (lower intensity, soft & natural)
    // Faint aura (1.2px)
    drawMonoString(bpmStr, bpmTextX - 1.2f, bpmTextY, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(bpmStr, bpmTextX + 1.2f, bpmTextY, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(bpmStr, bpmTextX, bpmTextY - 1.0f, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(bpmStr, bpmTextX, bpmTextY + 1.0f, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    // Proximity halo (0.6px)
    drawMonoString(bpmStr, bpmTextX - 0.6f, bpmTextY, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.16f);
    drawMonoString(bpmStr, bpmTextX + 0.6f, bpmTextY, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.16f);
    // Active illuminated core text
    drawMonoString(bpmStr, bpmTextX, bpmTextY, 1.15f, textColor.r, textColor.g, textColor.b, 0.98f);

    // 4. Glass reflection gradient rendered IN FRONT of content
    drawReadoutPodGlassOverlay(bpmPodX, bpmPodY, bpmPodW, bpmPodH);

    // --- POD 2: SECONDARY TIMECODE / BAR:BEAT:DIV READOUT (Right side) ---
    int curStep = engine_ ? static_cast<int>(engine_->getSequencer().getTransport().getCurrentStep()) : 0;
    int barNum = (curStep / 16) + 1;
    int beatNum = ((curStep % 16) / 4) + 1;
    int subBeat = (curStep % 4) + 1;

    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%d:%d", barNum, beatNum, subBeat);

    const float timePodW = 96.0f;
    const float timePodH = 36.0f;
    const float timePodX = r - 228.0f;
    const float timePodY = 10.0f;

    // 1. Base substrate
    drawReadoutPodBase(timePodX, timePodY, timePodW, timePodH);

    float timeTextX = timePodX + (timePodW - 6.0f * 10.5f) * 0.5f;
    float timeTextY = timePodY + 8.5f;

    // 2. Soft, delicate ambient bloom behind digits
    float timeCx = timePodX + timePodW * 0.5f;
    float timeCy = timePodY + timePodH * 0.5f;
    drawRoundedRect(timeCx - 26.0f, timeCy - 9.0f, 52.0f, 18.0f, 5.0f,
                    Color(glowColor.r, glowColor.g, glowColor.b, 0.045f));
    drawRoundedRect(timeCx - 15.0f, timeCy - 6.0f, 30.0f, 12.0f, 4.0f,
                    Color(glowColor.r, glowColor.g, glowColor.b, 0.065f));

    // 3. Unlit ghost segment mask
    Color ghostCol = textColor.withAlpha(0.12f);
    drawMonoString("88:8:8", timeTextX, timeTextY, 1.05f, ghostCol.r, ghostCol.g, ghostCol.b, ghostCol.a);

    // 4. Active illuminated digits with subtle, softened bloom
    drawMonoString(timeBuf, timeTextX - 1.2f, timeTextY, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(timeBuf, timeTextX + 1.2f, timeTextY, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(timeBuf, timeTextX, timeTextY - 1.0f, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(timeBuf, timeTextX, timeTextY + 1.0f, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(timeBuf, timeTextX - 0.6f, timeTextY, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.16f);
    drawMonoString(timeBuf, timeTextX + 0.6f, timeTextY, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.16f);
    drawMonoString(timeBuf, timeTextX, timeTextY, 1.05f, textColor.r, textColor.g, textColor.b, 0.98f);

    // 5. Glass reflection gradient rendered IN FRONT of content
    drawReadoutPodGlassOverlay(timePodX, timePodY, timePodW, timePodH);

    // =========================================================================
    // 5. RIGHT TACTILE HARDWARE BUTTONS (Lock, Search, Folder)
    // =========================================================================
    const float toolY = 11.0f;
    const float sqBtnSize = 34.0f;
    Color buttonEtch = theme.secondaryAccent;

    auto drawTactileSquareBtnBase = [&](float bx, float by, bool active) {
        float po = active ? 2.0f : 0.0f;
        // Recessed shadow well (Themed)
        drawRoundedRect(bx - 2.0f, by - 2.0f, sqBtnSize + 4.0f, sqBtnSize + 4.0f, 4.0f, theme.controlWell.darken(0.20f));
        drawRoundedRectOutline(bx - 2.0f, by - 2.0f, sqBtnSize + 4.0f, sqBtnSize + 4.0f, 4.0f, theme.borderSubtle.darken(0.20f), 1.0f);
        // Keycap gradient
        Color capTop = active ? theme.controlWell : theme.controlBackground;
        Color capBot = active ? theme.controlWell.darken(0.12f) : theme.controlWell;
        drawRoundedRectGradient(bx, by + po, sqBtnSize, sqBtnSize - po, 3.5f, capTop, capBot);
    };

    auto drawTactileSquareBtnOverlay = [&](float bx, float by, bool active) {
        float po = active ? 2.0f : 0.0f;
        Color capTop = active ? theme.controlWell : theme.controlBackground;

        // Tactile micro-noise overlay applied across keycap AND icon glyph!
        drawButtonNoise(bx, by + po, sqBtnSize, sqBtnSize - po, true, 0.28f, 3.5f, true, true, true, true, active, 3.0f);

        // Top specular bevel highlight
        drawLine(bx + 2.0f, by + po + 1.0f, bx + sqBtnSize - 2.0f, by + po + 1.0f, capTop.lighten(0.16f), 1.5f);
        drawRoundedRectOutline(bx, by + po, sqBtnSize, sqBtnSize - po, 3.5f,
                               active ? theme.primaryAccent : theme.borderSubtle.darken(0.15f), active ? 1.5f : 1.0f);
    };

    // BUTTON 1: LOCK (Toggle Workspace Edit Lock)
    const float lockX = r - 120.0f;
    drawTactileSquareBtnBase(lockX, toolY, projectLocked_);
    // Etched Lock Glyph (Themed with clear contrast)
    Color lockCol = projectLocked_ ? theme.primaryAccent : buttonEtch.lighten(0.10f);
    float lBodyX = lockX + 10.5f;
    float lBodyY = toolY + 16.0f + (projectLocked_ ? 2.0f : 0.0f);
    drawRoundedRect(lBodyX, lBodyY, 13.0f, 10.0f, 2.0f, lockCol);
    drawCircleOutline(lBodyX + 6.5f, lBodyY - 1.0f, 4.0f, lockCol, 1.5f);
    drawCircle(lBodyX + 6.5f, lBodyY + 4.5f, 1.5f, darkIcon);
    drawTactileSquareBtnOverlay(lockX, toolY, projectLocked_);

    // BUTTON 2: SEARCH / INSPECT (Toggle Fullscreen / Plugin Finder)
    const float searchX = r - 80.0f;
    drawTactileSquareBtnBase(searchX, toolY, isFullscreen_);
    // Etched Magnifying Glass Glyph (Themed with clear contrast)
    Color searchCol = isFullscreen_ ? theme.primaryAccent : buttonEtch.lighten(0.10f);
    float sLensX = searchX + 15.0f;
    float sLensY = toolY + 16.0f + (isFullscreen_ ? 2.0f : 0.0f);
    drawCircleOutline(sLensX, sLensY, 5.0f, searchCol, 1.6f);
    drawLine(sLensX + 3.5f, sLensY + 3.5f, sLensX + 7.5f, sLensY + 7.5f, searchCol, 2.0f);
    drawTactileSquareBtnOverlay(searchX, toolY, isFullscreen_);

    // BUTTON 3: FOLDER / PRESET LIBRARY (Toggle Sliding Drawer)
    const float folderX = r - 40.0f;
    drawTactileSquareBtnBase(folderX, toolY, browserOpen_);
    // Etched Folder Glyph (Themed with clear contrast)
    Color fldCol = browserOpen_ ? theme.primaryAccent : buttonEtch.lighten(0.10f);
    float fX = folderX + 8.5f;
    float fY = toolY + 12.0f + (browserOpen_ ? 2.0f : 0.0f);
    drawRect(fX, fY, 6.0f, 3.5f, fldCol);
    drawRoundedRect(fX, fY + 3.0f, 17.0f, 11.5f, 1.5f, fldCol);
    drawLine(fX, fY + 5.0f, fX + 17.0f, fY + 5.0f, darkIcon, 1.0f);
    drawTactileSquareBtnOverlay(folderX, toolY, browserOpen_);

    // 6. Floating tactile tooltip badge when hovering header transport elements
    std::string tip = getTransportTooltip(mouseX_, mouseY_);
    if (!tip.empty() && batchRenderer_) {
        float tipX = mouseX_;
        const float rW = static_cast<float>(width_);
        if (mouseX_ >= 8.0f && mouseX_ <= 50.0f) tipX = 29.0f;
        else if (mouseX_ >= 54.0f && mouseX_ < 92.0f) tipX = 73.0f;
        else if (mouseX_ >= 92.0f && mouseX_ < 130.0f) tipX = 111.0f;
        else if (mouseX_ >= 130.0f && mouseX_ < 168.0f) tipX = 149.0f;
        else if (mouseX_ >= 176.0f && mouseX_ <= 296.0f) tipX = 236.0f;
        else if (mouseX_ >= (rW - 228.0f) && mouseX_ < (rW - 132.0f)) tipX = rW - 180.0f;
        else if (mouseX_ >= (rW - 132.0f) && mouseX_ < (rW - 96.0f)) tipX = rW - 103.0f;
        else if (mouseX_ >= (rW - 96.0f) && mouseX_ < (rW - 66.0f)) tipX = rW - 63.0f;
        else if (mouseX_ >= (rW - 66.0f) && mouseX_ <= (rW - 4.0f)) tipX = rW - 23.0f;

        // Render tooltip badge directly ABOVE the elements, strictly within the top panel faceplate (y <= 54px)
        drawTooltipBadge(*batchRenderer_, tip, tipX, 10.0f, theme, false, rW, static_cast<float>(height_), 0.0f, 54.0f);
    }
#endif
}

void GuiWindow::renderFrame() {
    if (!engine_ || isRendering_) return;
#if EATS_HAS_GLFW
    if (window_) {
        if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED)) return;
        int curFbW = 0, curFbH = 0;
        glfwGetFramebufferSize(window_, &curFbW, &curFbH);
        if (curFbW <= 0 || curFbH <= 0) return;
    }
#endif
    isRendering_ = true;
    struct RenderGuard {
        bool& flag;
        ~RenderGuard() { flag = false; }
    } guard{isRendering_};

    static int s_frameLog = 0;
    if (s_frameLog++ < 3) {
        std::cout << "[GUI] Frame #" << s_frameLog << " rendering (size " << width_ << "x" << height_
                  << ", fb " << fbWidth_ << "x" << fbHeight_ << ")" << std::endl;
    }

    static auto s_lastFrameTime = std::chrono::steady_clock::now();
    const auto currentFrameTime = std::chrono::steady_clock::now();
    float frameDt = std::chrono::duration<float>(currentFrameTime - s_lastFrameTime).count();
    s_lastFrameTime = currentFrameTime;
    if (frameDt <= 0.0001f || frameDt > 0.1f) frameDt = 0.016f;
    frameDt_ = frameDt;

    // 1. Pull real-time audio scope data & meters from lock-free TelemetryPresenter bridge
    if (engine_) {
        telemetryPresenter_.update(*engine_, frameDt);
    }

    auto scope = telemetryPresenter_.getScopeSpan();
    if (!scope.empty()) {
        canvas_.setScopeData(scope.data(), std::min(scope.size(), size_t(128)));
    }

    const auto& masterMeter = telemetryPresenter_.getMasterMeter();
    canvas_.setVuMeter(masterMeter.peakL, masterMeter.peakR);
    masterPeakL_ = masterMeter.peakL;
    masterPeakR_ = masterMeter.peakR;

    const auto& trackMeters = telemetryPresenter_.getTrackMeters();
    for (size_t i = 0; i < mixerStrips_.size() && i < chPeakL_.size(); ++i) {
        if (i < trackMeters.size()) {
            chPeakL_[i] = trackMeters[i].peakL;
            chPeakR_[i] = trackMeters[i].peakR;
        } else {
            float est = (masterPeakL_ + masterPeakR_) * 0.5f * (mixerStrips_[i].mute ? 0.0f : mixerStrips_[i].volume);
            chPeakL_[i] = std::max(est, chPeakL_[i] * 0.91f);
            chPeakR_[i] = std::max(est, chPeakR_[i] * 0.91f);
        }
        mixerStrips_[i].peakL = chPeakL_[i];
        mixerStrips_[i].peakR = chPeakR_[i];
    }

#if EATS_HAS_GLFW
    if (window_) {
        g_activeFontRenderer = fontRenderer_.get();
        g_activeBatchRenderer = batchRenderer_.get();
        ScoreVectorGlyphs::setBatchRenderer(batchRenderer_.get());
        if (windowWidth_ == 0 || windowHeight_ == 0 || fbWidth_ == 0 || fbHeight_ == 0) {
            glfwGetFramebufferSize(window_, &fbWidth_, &fbHeight_);
            glfwGetWindowSize(window_, &windowWidth_, &windowHeight_);
        }
        updateLogicalDimensions();

        uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
        uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));

        if (batchRenderer_) {
            batchRenderer_->setRenderScale(renderScale_, renderScale_);
            batchRenderer_->resize(physW, physH);
            batchRenderer_->beginFrame(static_cast<float>(width_), static_cast<float>(height_));
        }

        lampTime_ += (1.0f / 60.0f);

        activeFrameTheme_ = getTheme();
        if (crtShaderEnabled_) {
            // Dynamic overhead swaying incandescent studio spotlight matching crt_screen.wgsl / dawn_bridge.cpp
            const float lightNormX = 0.5f + std::sin(lampTime_ * 1.5f) * 0.35f;
            activeFrameTheme_.globalLight = LightSource2D(
                lightNormX * static_cast<float>(width_),
                0.28f * static_cast<float>(height_) - 220.0f,
                480.0f,
                0.88f
            );
        }
        const auto& theme = activeFrameTheme_;
        const bool is3d = is3dConsoleEnabled();
        const float bPanelY = static_cast<float>(height_) - 48.0f;
        const float bPanelH = 48.0f;

        // Static Chrome Offscreen Texture (FBO) Caching
        if (batchRenderer_ && batchRenderer_->isChromeCachingEnabled()) {
            if (batchRenderer_->isChromeCacheValid()) {
                batchRenderer_->drawCachedChrome();
            } else {
                batchRenderer_->beginChromeRecording();
                renderStaticChrome(theme, static_cast<float>(width_), static_cast<float>(height_));
                batchRenderer_->endChromeRecording();
                batchRenderer_->drawCachedChrome();
            }
        }

        // =========================================================================
        // 2. ACTIVE VIEW DISPATCH OR HIGH-PERFORMANCE DEDICATED FULL-DISPLAY DEVICE
        // =========================================================================
        if (fullscreenDeviceModal_.isOpen()) {
            fullscreenDeviceModal_.setAudioScopeBuffer(std::span<const float>(scopeBuffer_, 128));
            fullscreenDeviceModal_.layout(static_cast<float>(width_), static_cast<float>(height_));
            fullscreenDeviceModal_.update(frameDt);
            if (fullscreenDeviceModal_.isGuiEmbedded() && engine_) {
                const auto& b = fullscreenDeviceModal_.getBodyBounds();
                const auto& tgt = fullscreenDeviceModal_.getTarget();
                if (tgt.type == DeviceTargetType::Instrument) {
                    auto clapNode = engine_->getTrackClapNode(tgt.trackIndex);
                    if (clapNode && clapNode->isEmbeddedGuiOpen()) {
                        clapNode->setEmbeddedGuiBounds(
                            static_cast<int>(b.x), static_cast<int>(b.y),
                            static_cast<int>(b.w), static_cast<int>(b.h));
                    }
                } else if (tgt.type == DeviceTargetType::AudioFx && tgt.fxIndex >= 0) {
                    auto clapFxNode = engine_->getTrackAudioFxClapNode(tgt.trackIndex, static_cast<size_t>(tgt.fxIndex));
                    if (clapFxNode && clapFxNode->isEmbeddedGuiOpen()) {
                        clapFxNode->setEmbeddedGuiBounds(
                            static_cast<int>(b.x), static_cast<int>(b.y),
                            static_cast<int>(b.w), static_cast<int>(b.h));
                    }
                }
            }
            if (batchRenderer_) {
                fullscreenDeviceModal_.render(*batchRenderer_, theme);
            }
        } else {
        if (activeView_ == WorkspaceView::Arranger) {
            if (modularArrangerView_) {
                ViewContext ctx = createViewContext();
                float topY = 56.0f;
                float bottomY = static_cast<float>(height_) - 48.0f;
                size_t numTrackPeaks = std::min(chPeakL_.size(), arrangerTracks_.size());
                modularArrangerView_->setMasterLevels(
                    masterVolume_, masterPan_, masterMute_,
                    masterPeakL_, masterPeakR_,
                    std::span<const float>(chPeakL_.data(), numTrackPeaks),
                    std::span<const float>(chPeakR_.data(), numTrackPeaks)
                );
                modularArrangerView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
                modularArrangerView_->render(ctx);
            }
        } else if (activeView_ == WorkspaceView::ModularRack || activeView_ == WorkspaceView::Design) {
            if (modularDesignView_) {
                ViewContext ctx = createViewContext();
                float topY = 56.0f;
                float bottomY = static_cast<float>(height_) - 48.0f;
                modularDesignView_->setAudioScopeBuffer(std::span<const float>(scopeBuffer_, 256));
                modularDesignView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
                modularDesignView_->render(ctx);
            }
        } else if (activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) {
            if (modularEditView_) {
                ViewContext ctx = createViewContext();
                float topY = 56.0f;
                float bottomY = static_cast<float>(height_) - 48.0f;
                if (modularEditView_->getActiveTrackIndex() != selectedTrackIndex_) {
                    syncActiveClipToEditView(selectedTrackIndex_, -1);
                }
                modularEditView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
                modularEditView_->render(ctx);
            }
        } else if (activeView_ == WorkspaceView::Mixer) {
            if (modularMixerView_) {
                ViewContext ctx = createViewContext();
                float topY = 56.0f;
                float bottomY = bPanelY - 6.0f;

                size_t numTrackPeaks = std::min(chPeakL_.size(), arrangerTracks_.size());
                std::span<const float> peaksL(chPeakL_.data(), numTrackPeaks);
                std::span<const float> peaksR(chPeakR_.data(), numTrackPeaks);

                modularMixerView_->syncFromWindow(
                    std::span<const ArrangerTrackData>(arrangerTracks_),
                    selectedTrackIndex_, masterVolume_, masterPan_, masterMute_,
                    masterPeakL_, masterPeakR_, peaksL, peaksR,
                    mixerPropertiesExpanded_, mixerPropertiesWidth_,
                    showMixerRouting_, showMixerPan_, showMixerButtons_,
                    showMixerMeters_, showMixerAutomation_, showMixerReadouts_,
                    browserOpen_, trackInspectorScrollY_
                );
                if (modularArrangerView_) {
                    const auto& arrTracks = modularArrangerView_->getTracks();
                    auto& mixChannels = modularMixerView_->getChannels();
                    for (size_t i = 0; i < mixChannels.size() && i < arrTracks.size(); ++i) {
                        if (mixChannels[i].instrument != arrTracks[i].instrument) {
                            mixChannels[i].instrument = arrTracks[i].instrument;
                        }
                        if (mixChannels[i].audioFx.size() != arrTracks[i].audioFx.size()) {
                            mixChannels[i].audioFx = arrTracks[i].audioFx;
                        }
                        if (mixChannels[i].midiFx.size() != arrTracks[i].midiFx.size()) {
                            mixChannels[i].midiFx = arrTracks[i].midiFx;
                        }
                    }
                }
                modularMixerView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
                modularMixerView_->render(ctx);
            }
        } else if (activeView_ == WorkspaceView::HardwarePanel || activeView_ == WorkspaceView::Track) {
            if (modularTrackInspectorView_) {
                ViewContext ctx = createViewContext();
                float topY = 56.0f;
                float bottomY = bPanelY - 6.0f;

                uint32_t activeIdx = modularArrangerView_ ? modularArrangerView_->getActiveTrackIndex() : selectedTrackIndex_;
                if (selectedTrackIndex_ != activeIdx) {
                    selectedTrackIndex_ = activeIdx;
                    if (modularArrangerView_) {
                        modularArrangerView_->syncDrawerDataForTrack(activeIdx);
                        modularTrackInspectorView_->syncFromDrawerData(modularArrangerView_->getDrawerData());
                    }
                }

                modularTrackInspectorView_->setAudioScopeBuffer(std::span<const float>(scopeBuffer_, 256));
                modularTrackInspectorView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
                modularTrackInspectorView_->render(ctx);
            }
        }

        // Virtual Piano Keyboard Drawer (Horizontal collapsible drawer docking above bottom nav)
        drawVirtualKeyboardDrawer();

        // =========================================================================
        // 3. BOTTOM HARDWARE NAVIGATION CONTROL STRIP
        // =========================================================================
        const float chinTopY = is3d ? (bPanelY - 14.0f) : bPanelY;
        const float chinTotalH = is3d ? (bPanelH + 14.0f) : bPanelH;

        if (batchRenderer_ && batchRenderer_->isChromeCacheValid()) {
            batchRenderer_->drawCachedChromeRegion(0.0f, chinTopY, 0.0f, chinTopY, static_cast<float>(width_), chinTotalH);
        } else {
            // Procedural Theme-Tinted Grungy Metal Chin Chassis (matching top panel)
            drawChassisPlate(0.0f, chinTopY, static_cast<float>(width_), chinTotalH, false, theme);

            // Top lip specular highlight line & crevice
            drawLine(0.0f, chinTopY, static_cast<float>(width_), chinTopY, theme.borderSubtle.lighten(0.15f), 1.5f);
            drawLine(0.0f, chinTopY + 1.5f, static_cast<float>(width_), chinTopY + 1.5f, theme.backgroundDark, 1.0f);
        }

        // 5 Primary Navigation Buttons: ARRANGER, EDIT, TRACK, MIXER, DESIGN
        const float totalNavW = static_cast<float>(width_) - 24.0f;
        const float navBtnW = totalNavW / 5.0f;
        struct BottomNavBtn {
            const char* label;
            WorkspaceView view;
            bool isSecondary;
        };
        const BottomNavBtn navButtons[5] = {
            {"ARRANGER", WorkspaceView::Arranger, false},
            {"EDIT",     WorkspaceView::Edit,     true},
            {"TRACK",    WorkspaceView::Track,    false},
            {"MIXER",    WorkspaceView::Mixer,    false},
            {"DESIGN",   WorkspaceView::Design,   true}
        };
        for (int i = 0; i < 5; ++i) {
            float bx = 12.0f + i * navBtnW;
            float bw = navBtnW - 6.0f;
            float bh = 32.0f;
            float by = bPanelY + (bPanelH - bh) * 0.5f;
            bool isAct = (activeView_ == navButtons[i].view);
            const Color& btnAccent = navButtons[i].isSecondary ? theme.secondaryAccent : theme.primaryAccent;

            // Tactile 3D Extruded Mechanical Keycaps with subtle rounded corners (r = 2.5px)
            float pressOff = isAct ? 2.0f : 0.0f;

            // 1. Deep Recessed Shadow Trench (Themed)
            drawRoundedRect(bx - 2.0f, by - 2.0f, bw + 4.0f, bh + 4.0f, 3.5f, theme.controlWell.darken(0.20f));
            drawRoundedRectOutline(bx - 2.0f, by - 2.0f, bw + 4.0f, bh + 4.0f, 3.5f, theme.borderSubtle.darken(0.20f), 1.0f);

            // 2. Extruded Keycap Body Plate (Themed)
            Color capTop = isAct ? theme.controlWell : theme.controlBackground;
            Color capBot = isAct ? theme.controlWell.darken(0.12f) : theme.controlWell;
            drawRoundedRectGradient(bx, by + pressOff, bw, bh - pressOff, 2.5f, capTop, capBot);

            // 3. Consistent Linear Notch / LED Tally Strip along top center edge of keycap
            if (isAct) {
                drawRoundedRect(bx + bw * 0.5f - 16.0f, by + pressOff + 1.5f, 32.0f, 2.5f, 1.0f, btnAccent);
                drawRoundedRect(bx + bw * 0.5f - 18.0f, by + pressOff + 1.0f, 36.0f, 3.5f, 1.5f, Color(btnAccent.r, btnAccent.g, btnAccent.b, 0.30f));
            } else {
                drawRoundedRect(bx + bw * 0.5f - 16.0f, by + 1.5f, 32.0f, 2.5f, 1.0f, theme.controlWell.darken(0.35f));
                drawRoundedRectOutline(bx + bw * 0.5f - 16.0f, by + 1.5f, 32.0f, 2.5f, 1.0f, theme.borderSubtle.darken(0.20f), 0.8f);
            }

            // 4. Engraved Backlit Icon and Text with theme accent radiance
            Color glowCol = isAct ? btnAccent : theme.textSecondary.lighten(0.12f);

            float iconW = 14.0f;
            float gap = 8.0f;
            float textLen = static_cast<float>(std::strlen(navButtons[i].label)) * 7.0f;
            float totalW = iconW + gap + textLen;
            float startX = bx + (bw - totalW) * 0.5f;
            float textY = by + pressOff + 9.0f;

            // Procedural Navigation Icon
            if (navButtons[i].view == WorkspaceView::Arranger) {
                drawIconArranger(startX, textY - 1.0f, 13.0f, 11.0f, glowCol);
            } else if (navButtons[i].view == WorkspaceView::Edit) {
                drawIconEdit(startX, textY - 1.0f, 11.0f, glowCol);
            } else if (navButtons[i].view == WorkspaceView::Track) {
                drawIconTrack(startX, textY - 1.0f, 13.0f, 11.0f, glowCol);
            } else if (navButtons[i].view == WorkspaceView::Mixer) {
                drawIconMixer(startX, textY - 1.0f, 13.0f, 11.0f, glowCol);
            } else if (navButtons[i].view == WorkspaceView::Design) {
                drawIconDesign(startX, textY - 1.0f, 13.0f, 11.0f, glowCol);
            }

            drawVectorString(navButtons[i].label, startX + iconW + gap, textY, 0.90f, glowCol);

            // 5. Tactile micro-noise overlay applied across keycap, icon, AND text label (shifts on active)
            drawButtonNoise(bx, by + pressOff, bw, bh - pressOff, false, 0.28f, 2.5f, true, true, true, true, isAct, 3.0f);

            // 6. Multi-layer 3D Chamfer Bevels and border outline
            drawLine(bx + 2.0f, by + pressOff + 1.0f, bx + bw - 2.0f, by + pressOff + 1.0f,
                     isAct ? capTop.lighten(0.12f) : capTop.lighten(0.18f), 1.6f);
            drawLine(bx + 1.0f, by + pressOff + 2.0f, bx + 1.0f, by + bh - 2.0f, capTop.lighten(0.08f), 1.0f);
            drawRoundedRectOutline(bx, by + pressOff, bw, bh - pressOff, 2.5f,
                                   isAct ? btnAccent : theme.borderSubtle.darken(0.15f),
                                   isAct ? 1.6f : 1.0f);
        }

        // =========================================================================
        // TOP TRANSPORT HEADER BAR (Rendered after workspace contents so scrolled
        // tracks and containers never draw over top panel chrome)
        // =========================================================================
        drawTopTransportBar();

        // =========================================================================
        // PRESET BROWSER / PATCH LIBRARIAN SLIDING DRAWER OVERLAY
        // =========================================================================
        if (projectBrowserDrawerWidget_ && (browserOpen_ || projectBrowserDrawerWidget_->getAnimOffset() < projectBrowserDrawerWidget_->getDrawerWidth())) {
            if (browserOpen_) {
                // Synchronize live tracks
                std::vector<BrowserTrackAssetItem> bTracks;
                if (engine_) {
                    for (size_t i = 0; i < engine_->getSequencer().getNumTracks(); ++i) {
                        const auto* t = engine_->getSequencer().getTrack(static_cast<uint32_t>(i));
                        if (!t) continue;
                        BrowserTrackAssetItem item;
                        item.index = static_cast<uint32_t>(i);
                        item.name = t->getName();
                        item.type = (i == 1 || i == 2) ? "DRUMS" : "SYNTH";
                        item.clipCount = (i < arrangerTracks_.size()) ? arrangerTracks_[i].clips.size() : 1;
                        item.isMuted = t->isMuted();
                        item.isSolo = t->isSolo();
                        if (i < arrangerTracks_.size()) {
                            item.color = Color(arrangerTracks_[i].r, arrangerTracks_[i].g, arrangerTracks_[i].b, 1.0f);
                        }
                        bTracks.push_back(item);
                    }
                }
                projectBrowserDrawerWidget_->setTracks(bTracks);

                // Synchronize live diff history
                std::vector<BrowserHistoryMilestoneItem> bHist;
                auto timeline = diffHistory_.getTimeline();
                size_t curIdx = diffHistory_.getCurrentTimelineIndex();
                for (size_t i = 0; i < timeline.size(); ++i) {
                    BrowserHistoryMilestoneItem m;
                    m.stepIndex = i;
                    m.description = timeline[i].description;
                    m.category = timeline[i].category;
                    m.isMilestone = timeline[i].isMilestone;
                    m.isCurrent = (i == curIdx);
                    bHist.push_back(m);
                }
                projectBrowserDrawerWidget_->setHistory(bHist, diffHistory_.canUndo(), diffHistory_.canRedo());
                projectBrowserDrawerWidget_->open();
            } else {
                projectBrowserDrawerWidget_->close();
            }

            projectBrowserDrawerWidget_->layout(static_cast<float>(width_), static_cast<float>(height_), 56.0f, 48.0f);
            projectBrowserDrawerWidget_->update(frameDt);
            if (batchRenderer_) {
                projectBrowserDrawerWidget_->render(*batchRenderer_, getTheme());
            }
        }

        } // End of !fullscreenDeviceModal_.isOpen()

        // =========================================================================
        // 5. PROJECT HUB & SYSTEM SETTINGS MODAL DIALOG
        // =========================================================================
        if (projectHubOpen_) {
            DialogFrameConfig cfg;
            cfg.width = 540.0f;
            cfg.height = 620.0f;
            cfg.title = "EATSBITS SETTINGS";
            cfg.showLogo = true;
            cfg.showCloseButton = true;
            cfg.showBottomClose = true;
            cfg.cornerRadius = 14.0f;

            DialogLayout dl = drawModalDialogFrame(cfg);
            const float hubX = dl.x;
            const float hubY = dl.y;
            const float hubW = dl.w;
            const float hubH = dl.h;

            // Accordion and Drawer Dimensions
            const float topContentY = hubY + 50.0f;
            const float footerY = hubY + hubH - 36.0f;
            const float headerH = 30.0f;
            const float headerGap = 6.0f;
            const float headerStep = headerH + headerGap; // 36.0f
            const int numSections = 6;
            // Maximum height available for any expanded drawer inside the dialog
            const float availableDrawerH = (footerY - topContentY) - (numSections * headerStep) - headerGap;

            float curDrawerH = 0.0f;
            if (projectHubSection_ >= 0) {
                if (projectHubSection_ == 0) curDrawerH = 190.0f;
                else if (projectHubSection_ == 1) curDrawerH = 186.0f;
                else if (projectHubSection_ == 2) curDrawerH = 270.0f;
                else if (projectHubSection_ == 3) curDrawerH = availableDrawerH;
                else if (projectHubSection_ == 4) curDrawerH = 176.0f;
                else if (projectHubSection_ == 5) curDrawerH = 100.0f;
            }

            const float drawerX = hubX + 16.0f;
            const float drawerW = hubW - 32.0f;
            const float drawerY = topContentY + (projectHubSection_ + 1) * headerStep;
            const float drawerH = curDrawerH;

            // Configure scrollable area constrained to the CRT Shader drawer
            if (projectHubSection_ == 3) {
                projectHubScrollArea_.setViewport(drawerX, drawerY, drawerW, drawerH);
                projectHubScrollArea_.setContentHeight(586.0f);
                projectHubScrollArea_.setScrollY(projectHubScrollY_);
                projectHubMaxScroll_ = projectHubScrollArea_.getMaxScroll();
                projectHubScrollY_ = projectHubScrollArea_.getScrollY();
            } else {
                projectHubScrollArea_.setViewport(0.0f, 0.0f, 0.0f, 0.0f);
                projectHubScrollArea_.setContentHeight(0.0f);
                projectHubScrollY_ = 0.0f;
            }

            const char* secTitles[6] = {
                "PROJECT HUB",
                "SESSION PERSISTENCE & AUTO-RESTORE",
                "DISPLAY & WORKSPACE",
                "CRT SHADER",
                "AUDIO & CLAP PLUGINS",
                "CREDITS & ACKNOWLEDGMENTS"
            };

            for (int s = 0; s < 6; ++s) {
                bool isExp = (projectHubSection_ == s);
                float curY = 0.0f;
                if (projectHubSection_ == -1 || s <= projectHubSection_) {
                    curY = topContentY + s * headerStep;
                } else {
                    curY = drawerY + drawerH + headerGap + (s - (projectHubSection_ + 1)) * headerStep;
                }

                // Section Header Bar [hubX + 12, curY, hubW - 24, 30]
                if (isExp) {
                    drawRoundedRectGradient(hubX + 12.0f, curY, hubW - 24.0f, 30.0f, 5.0f,
                                            theme.controlWell.darken(0.04f), theme.controlWell.darken(0.12f));
                    drawRoundedRectOutline(hubX + 12.0f, curY, hubW - 24.0f, 30.0f, 5.0f,
                                           theme.primaryAccent.darken(0.35f), 1.0f);
                    // Small closed down arrow ▼
                    float ax = hubX + hubW - 28.0f;
                    float ay = curY + 15.0f;
                    drawTriangle(ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f,
                                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
                    drawVectorString(secTitles[s], hubX + 44.0f, curY + 8.0f, 0.85f, theme.primaryAccent);
                } else {
                    drawRoundedRect(hubX + 12.0f, curY, hubW - 24.0f, 30.0f, 5.0f, theme.panelBackground.darken(0.04f));
                    drawRoundedRectOutline(hubX + 12.0f, curY, hubW - 24.0f, 30.0f, 5.0f, theme.borderSubtle.darken(0.20f), 1.0f);
                    // Small closed right arrow ▶
                    float ax = hubX + hubW - 28.0f;
                    float ay = curY + 15.0f;
                    drawTriangle(ax - 3.0f, ay - 4.5f, ax + 3.5f, ay, ax - 3.0f, ay + 4.5f,
                                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
                    drawVectorString(secTitles[s], hubX + 44.0f, curY + 8.0f, 0.85f, theme.textSecondary);
                }

                // Section Icons
                if (s == 0) {
                    drawRect(hubX + 22.0f, curY + 11.0f, 13.0f, 9.0f, isExp ? theme.primaryAccent : theme.textSecondary);
                    drawRect(hubX + 22.0f, curY + 9.0f, 6.0f, 3.0f, isExp ? theme.primaryAccent : theme.textSecondary);
                } else if (s == 1) {
                    drawCircleOutline(hubX + 28.0f, curY + 15.0f, 5.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawLine(hubX + 28.0f, curY + 12.0f, hubX + 28.0f, curY + 15.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.0f);
                    drawLine(hubX + 28.0f, curY + 15.0f, hubX + 31.0f, curY + 15.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.0f);
                } else if (s == 2) {
                    drawIconEdit(hubX + 22.0f, curY + 10.0f, 10.0f, isExp ? theme.primaryAccent : theme.textSecondary);
                } else if (s == 3) {
                    drawRoundedRectOutline(hubX + 22.0f, curY + 10.0f, 12.0f, 9.0f, 2.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawLine(hubX + 25.0f, curY + 19.0f, hubX + 31.0f, curY + 19.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                } else if (s == 4) {
                    drawLine(hubX + 22.0f, curY + 15.0f, hubX + 32.0f, curY + 15.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawLine(hubX + 25.0f, curY + 12.0f, hubX + 25.0f, curY + 18.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawLine(hubX + 28.0f, curY + 10.0f, hubX + 28.0f, curY + 20.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                } else if (s == 5) {
                    drawCircleOutline(hubX + 28.0f, curY + 15.0f, 5.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawLine(hubX + 28.0f, curY + 14.0f, hubX + 28.0f, curY + 17.5f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawCircle(hubX + 28.0f, curY + 12.0f, 1.0f, isExp ? theme.primaryAccent : theme.textSecondary);
                }

                if (isExp) {
                    // Drawer Container Well
                    drawRoundedRect(drawerX, drawerY, drawerW, drawerH, 6.0f, theme.backgroundDark);
                    drawRoundedRectOutline(drawerX, drawerY, drawerW, drawerH, 6.0f, theme.borderSubtle.darken(0.18f), 1.0f);

                    float contentY = drawerY;
                    if (s == 0) {
                        // SECTION 0: PROJECT HUB
                        // Composition Details Outer Card
                        drawRoundedRect(hubX + 20.0f, contentY + 2.0f, hubW - 40.0f, 96.0f, 6.0f, theme.backgroundDark);
                        drawRoundedRectOutline(hubX + 20.0f, contentY + 2.0f, hubW - 40.0f, 96.0f, 6.0f, theme.borderSubtle.darken(0.18f), 1.0f);
                        drawVectorString("COMPOSITION DETAILS", hubX + 32.0f, contentY + 10.0f, 0.58f, theme.textMuted);

                        // Title Box
                        drawRoundedRect(hubX + 28.0f, contentY + 22.0f, hubW - 56.0f, 30.0f, 4.0f, theme.controlWell);
                        drawRoundedRectOutline(hubX + 28.0f, contentY + 22.0f, hubW - 56.0f, 30.0f, 4.0f,
                                               isEditingTitle_ ? theme.primaryAccent : theme.borderSubtle.darken(0.10f),
                                               isEditingTitle_ ? 1.5f : 1.0f);
                        drawVectorString("Title / Song Name", hubX + 36.0f, contentY + 17.0f, 0.50f, theme.textMuted);
                        drawVectorString(projectName_ + (isEditingTitle_ ? "_" : ""), hubX + 36.0f, contentY + 29.0f, 0.90f, theme.textPrimary);

                        // Author Box
                        drawRoundedRect(hubX + 28.0f, contentY + 58.0f, hubW - 56.0f, 30.0f, 4.0f, theme.controlWell);
                        drawRoundedRectOutline(hubX + 28.0f, contentY + 58.0f, hubW - 56.0f, 30.0f, 4.0f,
                                               isEditingAuthor_ ? theme.primaryAccent : theme.borderSubtle.darken(0.10f),
                                               isEditingAuthor_ ? 1.5f : 1.0f);
                        drawVectorString("Author / Creator", hubX + 36.0f, contentY + 53.0f, 0.50f, theme.textMuted);
                        drawVectorString(authorName_ + (isEditingAuthor_ ? "_" : ""), hubX + 36.0f, contentY + 65.0f, 0.90f, theme.textPrimary);

                        // Action Buttons 2x3 Grid
                        const float btnW = (hubW - 48.0f - 16.0f) / 3.0f;
                        const float row1Y = contentY + 104.0f;
                        const float row2Y = contentY + 144.0f;

                        // [ SAVE (.eats) ]
                        drawRoundedRectGradient(hubX + 24.0f, row1Y, btnW, 34.0f, 4.0f,
                                                theme.primaryAccent.darken(0.65f), theme.primaryAccent.darken(0.80f));
                        drawRoundedRectOutline(hubX + 24.0f, row1Y, btnW, 34.0f, 4.0f, theme.primaryAccent, 1.2f);
                        drawVectorString("SAVE (.eats)", hubX + 44.0f, row1Y + 10.0f, 0.85f, theme.primaryAccent.lighten(0.15f));

                        // [ SAVE AS... ]
                        drawRoundedRectGradient(hubX + 32.0f + btnW, row1Y, btnW, 34.0f, 4.0f,
                                                theme.primaryAccent.darken(0.70f), theme.primaryAccent.darken(0.82f));
                        drawRoundedRectOutline(hubX + 32.0f + btnW, row1Y, btnW, 34.0f, 4.0f, theme.primaryAccent.darken(0.25f), 1.0f);
                        drawVectorString("SAVE AS...", hubX + 54.0f + btnW, row1Y + 10.0f, 0.85f, theme.primaryAccent.lighten(0.15f));

                        // [ LOAD (.eats) ]
                        drawRoundedRectGradient(hubX + 40.0f + 2.0f * btnW, row1Y, btnW, 34.0f, 4.0f,
                                                theme.secondaryAccent.darken(0.65f), theme.secondaryAccent.darken(0.80f));
                        drawRoundedRectOutline(hubX + 40.0f + 2.0f * btnW, row1Y, btnW, 34.0f, 4.0f, theme.secondaryAccent, 1.2f);
                        drawVectorString("LOAD (.eats)", hubX + 62.0f + 2.0f * btnW, row1Y + 10.0f, 0.85f, theme.secondaryAccent);

                        // [ NEW / RESET ]
                        drawRoundedRectGradient(hubX + 24.0f, row2Y, btnW, 34.0f, 4.0f,
                                                Color(0.24f, 0.08f, 0.12f), Color(0.14f, 0.05f, 0.07f));
                        drawRoundedRectOutline(hubX + 24.0f, row2Y, btnW, 34.0f, 4.0f, Color(0.85f, 0.30f, 0.40f), 1.2f);
                        drawVectorString("NEW / RESET", hubX + 44.0f, row2Y + 10.0f, 0.85f, Color(1.0f, 0.45f, 0.55f));

                        // [ BOUNCE WAV ]
                        drawRoundedRectGradient(hubX + 32.0f + btnW, row2Y, btnW, 34.0f, 4.0f,
                                                Color(0.06f, 0.20f, 0.10f), Color(0.03f, 0.11f, 0.05f));
                        drawRoundedRectOutline(hubX + 32.0f + btnW, row2Y, btnW, 34.0f, 4.0f, Color(0.0f, 0.85f, 0.40f), 1.2f);
                        drawVectorString("EXPORT WAV", hubX + 50.0f + btnW, row2Y + 10.0f, 0.85f, Color(0.0f, 0.95f, 0.45f));

                        // [ SCRIPT VIEW ]
                        drawRoundedRectGradient(hubX + 40.0f + 2.0f * btnW, row2Y, btnW, 34.0f, 4.0f,
                                                Color(0.20f, 0.08f, 0.22f), Color(0.11f, 0.04f, 0.12f));
                        drawRoundedRectOutline(hubX + 40.0f + 2.0f * btnW, row2Y, btnW, 34.0f, 4.0f, Color(0.80f, 0.35f, 0.90f), 1.2f);
                        drawVectorString("SCRIPT VIEW", hubX + 58.0f + 2.0f * btnW, row2Y + 10.0f, 0.85f, Color(0.95f, 0.45f, 1.0f));
                    } else if (s == 1) {
                        // SECTION 1: SESSION PERSISTENCE & AUTO-RESTORE
                        drawVectorString("Restore last project on startup", hubX + 28.0f, contentY + 14.0f, 0.85f, theme.textPrimary);
                        drawVectorString("Automatically resumes previous workspace", hubX + 28.0f, contentY + 28.0f, 0.65f, theme.textMuted);
                        // Switch [hubX + hubW - 70, contentY + 12, 44, 20]
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 12.0f, 44.0f, 20.0f, 10.0f,
                                       autoRestoreSession_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 12.0f, 44.0f, 20.0f, 10.0f,
                                              autoRestoreSession_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(autoRestoreSession_ ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 22.0f, 7.0f,
                                   autoRestoreSession_ ? theme.primaryAccent : theme.textMuted);

                        // Row 2: Auto-save project state
                        drawLine(hubX + 24.0f, contentY + 36.0f, hubX + hubW - 24.0f, contentY + 36.0f, theme.borderSubtle.darken(0.15f), 1.0f);
                        drawVectorString("Auto-save project state snapshot", hubX + 28.0f, contentY + 46.0f, 0.85f, theme.textPrimary);
                        drawVectorString("Silently writes .eats state to disk every 60s", hubX + 28.0f, contentY + 60.0f, 0.65f, theme.textMuted);
                        // Switch
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 44.0f, 44.0f, 20.0f, 10.0f,
                                       autoSaveEnabled_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 44.0f, 44.0f, 20.0f, 10.0f,
                                              autoSaveEnabled_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(autoSaveEnabled_ ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 54.0f, 7.0f,
                                   autoSaveEnabled_ ? theme.primaryAccent : theme.textMuted);

                        // Row 3: Reset to clean slate button
                        drawRoundedRect(hubX + 24.0f, contentY + 74.0f, hubW - 48.0f, 28.0f, 4.0f, theme.controlWell);
                        drawRoundedRectOutline(hubX + 24.0f, contentY + 74.0f, hubW - 48.0f, 28.0f, 4.0f, theme.borderSubtle, 1.0f);
                        drawVectorString("RESET PROJECT (CLEAN SLATE)", hubX + 130.0f, contentY + 81.0f, 0.78f, theme.textSecondary);

                        // Row 4: Settings Persistence & Desktop Native File Open
#if defined(__EMSCRIPTEN__)
                        drawRoundedRect(hubX + 24.0f, contentY + 108.0f, hubW - 48.0f, 28.0f, 4.0f, theme.controlWell);
                        drawRoundedRectOutline(hubX + 24.0f, contentY + 108.0f, hubW - 48.0f, 28.0f, 4.0f, Color(1.0f, 0.45f, 0.35f), 1.0f);
                        drawVectorString("REVERT TO DEFAULT SETTINGS", hubX + 120.0f, contentY + 115.0f, 0.78f, Color(1.0f, 0.55f, 0.35f));
#else
                        const float setBtnW = (hubW - 48.0f - 10.0f) * 0.5f;
                        drawRoundedRect(hubX + 24.0f, contentY + 108.0f, setBtnW, 28.0f, 4.0f, theme.controlWell);
                        drawRoundedRectOutline(hubX + 24.0f, contentY + 108.0f, setBtnW, 28.0f, 4.0f, Color(1.0f, 0.45f, 0.35f), 1.0f);
                        drawVectorString("REVERT TO DEFAULT", hubX + 44.0f, contentY + 115.0f, 0.75f, Color(1.0f, 0.55f, 0.35f));

                        drawRoundedRect(hubX + 34.0f + setBtnW, contentY + 108.0f, setBtnW, 28.0f, 4.0f, theme.controlWell);
                        drawRoundedRectOutline(hubX + 34.0f + setBtnW, contentY + 108.0f, setBtnW, 28.0f, 4.0f, theme.primaryAccent, 1.0f);
                        drawVectorString("OPEN SETTINGS (JSON)", hubX + 48.0f + setBtnW, contentY + 115.0f, 0.75f, theme.primaryAccent);
#endif
                        drawVectorString("Settings stored natively in JSON (auto-saved on change)", hubX + 28.0f, contentY + 144.0f, 0.60f, theme.textMuted);
                    } else if (s == 2) {
                        // SECTION 2: DISPLAY & WORKSPACE
                        // UI Magnification / Scale Slider with 25% Increments (50% to 200%)
                        drawVectorString("UI MAGNIFICATION / SCALE FACTOR (50% - 200%)", hubX + 28.0f, contentY + 10.0f, 0.65f, theme.textMuted);
                        const float scales[7] = {0.50f, 0.75f, 1.0f, 1.25f, 1.50f, 1.75f, 2.0f};
                        const char* scaleLabels[7] = {"50%", "75%", "100%", "125%", "150%", "175%", "200%"};

                        // Slider Track
                        float sliderX = hubX + 24.0f;
                        float sliderW = hubW - 130.0f;
                        float sliderY = contentY + 22.0f;
                        float sliderH = 6.0f;
                        float norm = std::clamp((uiScale_ - 0.50f) / 1.50f, 0.0f, 1.0f);

                        // Track Background
                        drawRoundedRect(sliderX, sliderY, sliderW, sliderH, 3.0f, theme.controlWell);
                        drawRoundedRectOutline(sliderX, sliderY, sliderW, sliderH, 3.0f, theme.borderSubtle.darken(0.15f), 1.0f);

                        // Active track fill
                        if (norm > 0.001f) {
                            drawRoundedRect(sliderX, sliderY, sliderW * norm, sliderH, 3.0f, theme.primaryAccent.darken(0.20f));
                        }

                        // Tick marks for 25% increments (0..6)
                        for (int i = 0; i < 7; ++i) {
                            float tx = sliderX + (static_cast<float>(i) / 6.0f) * sliderW;
                            bool isCur = (std::abs(uiScale_ - scales[i]) < 0.05f);
                            const Color& tickCol = isCur ? theme.primaryAccent : theme.borderSubtle;
                            drawLine(tx, sliderY - 2.5f, tx, sliderY + sliderH + 2.5f,
                                     tickCol.r, tickCol.g, tickCol.b, isCur ? 1.0f : 0.5f, isCur ? 1.5f : 1.0f);
                        }

                        // Slider Thumb Knob
                        float thumbX = sliderX + norm * sliderW;
                        float thumbY = sliderY + sliderH * 0.5f;
                        drawCircle(thumbX, thumbY + 1.0f, 7.5f, Color(0.04f, 0.05f, 0.07f, 0.50f));
                        drawCircle(thumbX, thumbY, 6.5f, theme.primaryAccent);
                        drawCircleOutline(thumbX, thumbY, 6.5f, Color(1.0f, 1.0f, 1.0f, 0.90f), 1.2f);

                        // Digital Readout Pod on the right
                        float podX = hubX + hubW - 96.0f;
                        float podY = contentY + 13.0f;
                        float podW = 72.0f;
                        float podH = 22.0f;
                        drawRoundedRect(podX, podY, podW, podH, 4.0f, theme.controlWell);
                        drawRoundedRectOutline(podX, podY, podW, podH, 4.0f, theme.primaryAccent, 1.0f);
                        int pct = static_cast<int>(std::round(uiScale_ * 100.0f));
                        std::string pctStr = std::to_string(pct) + "%";
                        drawVectorString(pctStr, podX + (pct >= 100 ? 18.0f : 24.0f), podY + 5.0f, 0.82f, theme.primaryAccent);

                        // Discrete 25% increment chips below the track
                        float chipY = contentY + 36.0f;
                        float chipH = 16.0f;
                        float chipGap = 5.0f;
                        float chipW = (hubW - 48.0f - (6.0f * chipGap)) / 7.0f;
                        for (int sc = 0; sc < 7; ++sc) {
                            float cx = hubX + 24.0f + sc * (chipW + chipGap);
                            bool isAct = (std::abs(uiScale_ - scales[sc]) < 0.05f);
                            drawRoundedRect(cx, chipY, chipW, chipH, 3.0f,
                                           isAct ? theme.primaryAccent.darken(0.40f) : theme.controlBackground);
                            drawRoundedRectOutline(cx, chipY, chipW, chipH, 3.0f,
                                                  isAct ? theme.primaryAccent : theme.borderSubtle, isAct ? 1.2f : 0.8f);
                            drawVectorString(scaleLabels[sc], cx + (sc == 6 ? 17.0f : (sc >= 2 ? 17.0f : 21.0f)), chipY + 3.0f, 0.65f,
                                             isAct ? Color(1.0f, 1.0f, 1.0f) : theme.textSecondary);
                        }

                        // Theme Engine Palette with Bank Switcher & Theme Browser Modal
                        drawLine(hubX + 24.0f, contentY + 54.0f, hubX + hubW - 24.0f, contentY + 54.0f, theme.borderSubtle.darken(0.15f), 1.0f);
                        drawVectorString("THEME:", hubX + 28.0f, contentY + 60.0f, 0.65f, theme.textMuted);

                        int curBank = getThemeBank();
                        // Bank 0: [ 1: CONSOLES (1-5) ]
                        float b0X = hubX + 76.0f;
                        float b0Y = contentY + 57.0f;
                        float b0W = 120.0f;
                        float b0H = 18.0f;
                        drawRoundedRect(b0X, b0Y, b0W, b0H, 3.0f,
                                       (curBank == 0) ? theme.primaryAccent.darken(0.35f) : theme.controlBackground);
                        drawRoundedRectOutline(b0X, b0Y, b0W, b0H, 3.0f,
                                              (curBank == 0) ? theme.primaryAccent : theme.borderSubtle, (curBank == 0) ? 1.2f : 0.8f);
                        drawVectorString("1: CONSOLES (1-5)", b0X + 10.0f, b0Y + 4.0f, 0.60f,
                                         (curBank == 0) ? Color(1.0f, 1.0f, 1.0f) : theme.textSecondary);

                        // Bank 1: [ 2: SYNTAX (6-10) ]
                        float b1X = hubX + 202.0f;
                        float b1Y = contentY + 57.0f;
                        float b1W = 126.0f;
                        float b1H = 18.0f;
                        drawRoundedRect(b1X, b1Y, b1W, b1H, 3.0f,
                                       (curBank == 1) ? theme.primaryAccent.darken(0.35f) : theme.controlBackground);
                        drawRoundedRectOutline(b1X, b1Y, b1W, b1H, 3.0f,
                                              (curBank == 1) ? theme.primaryAccent : theme.borderSubtle, (curBank == 1) ? 1.2f : 0.8f);
                        drawVectorString("2: SYNTAX (6-10)", b1X + 10.0f, b1Y + 4.0f, 0.60f,
                                         (curBank == 1) ? Color(1.0f, 1.0f, 1.0f) : theme.textSecondary);

                        // Modal Browse Button: [ + BROWSE ALL ]
                        float browseBtnW = 124.0f;
                        float browseBtnH = 18.0f;
                        float browseBtnX = hubX + hubW - browseBtnW - 24.0f;
                        float browseBtnY = contentY + 57.0f;
                        drawRoundedRect(browseBtnX, browseBtnY, browseBtnW, browseBtnH, 3.0f, theme.primaryAccent.darken(0.40f));
                        drawRoundedRectOutline(browseBtnX, browseBtnY, browseBtnW, browseBtnH, 3.0f, theme.primaryAccent, 1.2f);
                        drawVectorString("+ BROWSE ALL (10)", browseBtnX + 10.0f, browseBtnY + 4.0f, 0.60f, Color(1.0f, 1.0f, 1.0f));

                        // Chips Row (5 Chips per active bank)
                        const char* themeLabelsBank0[5] = {"ATE TRACK", "MIDNIGHT", "LT SNACK", "BREAKFAST", "DINNER"};
                        const char* themeLabelsBank1[5] = {"DRACULA", "NORDIC", "CATPPUCCIN", "DARK ROAST", "SEA & SALT"};
                        const char** currentLabels = (curBank == 1) ? themeLabelsBank1 : themeLabelsBank0;

                        for (int th = 0; th < 5; ++th) {
                            int presetIdx = (curBank == 1) ? (5 + th) : th;
                            float tx = hubX + 24.0f + th * 95.0f;
                            bool isAct = (activeThemePreset_ == presetIdx);
                            const auto& thToken = Theme::get(static_cast<Theme::Preset>(presetIdx));
                            drawRoundedRect(tx, contentY + 80.0f, 88.0f, 22.0f, 4.0f,
                                           isAct ? thToken.primaryAccent.darken(0.45f) : theme.controlBackground);
                            drawRoundedRectOutline(tx, contentY + 80.0f, 88.0f, 22.0f, 4.0f,
                                                  isAct ? thToken.primaryAccent : theme.borderSubtle, isAct ? 1.5f : 1.0f);
                            drawCircle(tx + 10.0f, contentY + 91.0f, 4.0f, thToken.primaryAccent);
                            drawVectorString(currentLabels[th], tx + 18.0f, contentY + 84.0f, 0.65f,
                                             isAct ? Color(1.0f, 1.0f, 1.0f) : theme.textSecondary);
                        }

                        // Active Theme Readout line
                        const auto& catalog = Theme::getPresetCatalog();
                        if (static_cast<size_t>(activeThemePreset_) < catalog.size()) {
                            std::string readout = "Active: " + catalog[activeThemePreset_].name + " (" + catalog[activeThemePreset_].subtitle + ")";
                            drawVectorString(readout, hubX + 28.0f, contentY + 105.0f, 0.58f, theme.textMuted);
                        }

                        // Anti-Aliasing Mode Chips (No AA 1x, 2x Fast, 4x RGSS)
                        drawLine(hubX + 24.0f, contentY + 98.0f, hubX + hubW - 24.0f, contentY + 98.0f, theme.borderSubtle.darken(0.15f), 1.0f);
                        drawVectorString("ANTI-ALIASING (SUBPIXEL EDGE QUALITY)", hubX + 28.0f, contentY + 105.0f, 0.65f, theme.textMuted);
                        const char* aaLabels[3] = {"NO AA (1X)", "2X FAST", "4X RGSS"};
                        const float aaWidths[3] = {100.0f, 100.0f, 110.0f};
                        const float aaOffsets[3] = {0.0f, 108.0f, 216.0f};
                        for (int aa = 0; aa < 3; ++aa) {
                            float ax = hubX + 24.0f + aaOffsets[aa];
                            bool isAct = (antiAliasingMode_ == aa);
                            drawRoundedRect(ax, contentY + 118.0f, aaWidths[aa], 24.0f, 4.0f,
                                           isAct ? theme.controlWell : theme.controlBackground);
                            drawRoundedRectOutline(ax, contentY + 118.0f, aaWidths[aa], 24.0f, 4.0f,
                                                  isAct ? theme.primaryAccent : theme.borderSubtle, isAct ? 1.5f : 1.0f);
                            drawVectorString(aaLabels[aa], ax + 14.0f, contentY + 123.0f, 0.78f,
                                             isAct ? theme.primaryAccent : theme.textSecondary);
                        }

                        // HiDPI Canvas Resolution Switch
                        drawLine(hubX + 24.0f, contentY + 150.0f, hubX + hubW - 24.0f, contentY + 150.0f, theme.borderSubtle.darken(0.15f), 1.0f);
                        drawVectorString("Native 1:1 HiDPI Resolution (Retina/4K)", hubX + 28.0f, contentY + 160.0f, 0.85f, theme.textPrimary);
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 156.0f, 44.0f, 20.0f, 10.0f,
                                       hiDpiEnabled_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 156.0f, 44.0f, 20.0f, 10.0f,
                                              hiDpiEnabled_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(hiDpiEnabled_ ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 166.0f, 7.0f,
                                   hiDpiEnabled_ ? theme.primaryAccent : theme.textMuted);

                        // CRT Curved Shaders
                        drawVectorString("Screen Shaders & CRT Glass Curvature", hubX + 28.0f, contentY + 186.0f, 0.85f, theme.textPrimary);
                        // CRT toggle pill
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 182.0f, 44.0f, 20.0f, 10.0f,
                                       crtShaderEnabled_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 182.0f, 44.0f, 20.0f, 10.0f,
                                              crtShaderEnabled_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(crtShaderEnabled_ ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 192.0f, 7.0f,
                                   crtShaderEnabled_ ? theme.primaryAccent : theme.textMuted);

                        // GUI Animations toggle
                        drawVectorString("Real-time Oscilloscopes & Ticker Animations", hubX + 28.0f, contentY + 212.0f, 0.85f, theme.textPrimary);
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 208.0f, 44.0f, 20.0f, 10.0f,
                                       guiAnimationsEnabled_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 208.0f, 44.0f, 20.0f, 10.0f,
                                              guiAnimationsEnabled_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(guiAnimationsEnabled_ ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 218.0f, 7.0f,
                                   guiAnimationsEnabled_ ? theme.primaryAccent : theme.textMuted);

                        // Minimalist Controls toggle (Performance Vector Knobs & Sliders)
                        bool isMin = isMinimalGuiEnabled();
                        drawVectorString("Minimalist Controls (Performance Vector Knobs)", hubX + 28.0f, contentY + 238.0f, 0.85f, theme.textPrimary);
                        drawVectorString("Bypasses 3D skeuomorphism for clean vector dials (Bitwig/Ableton)", hubX + 28.0f, contentY + 252.0f, 0.62f, theme.textMuted);
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 234.0f, 44.0f, 20.0f, 10.0f,
                                       isMin ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 234.0f, 44.0f, 20.0f, 10.0f,
                                              isMin ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(isMin ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 244.0f, 7.0f,
                                   isMin ? theme.primaryAccent : theme.textMuted);
                    } else if (s == 3) {
                        // SECTION 3: CRT SHADER (Scrollable within constrained drawer space)
                        const float contentBaseY = drawerY - projectHubScrollY_;

                        // Master Toggle Row
                        float rowMasterY = contentBaseY + 6.0f;
                        if (projectHubScrollArea_.isVisible(rowMasterY, 26.0f) && rowMasterY >= drawerY - 4.0f && rowMasterY + 26.0f <= drawerY + drawerH + 4.0f) {
                            drawVectorString("CRT Screen Shader & Curvature Engine", drawerX + 12.0f, rowMasterY + 6.0f, 0.85f, theme.textPrimary);

                            // CRT Toggle Switch
                            drawRoundedRect(drawerX + drawerW - 60.0f, rowMasterY + 1.0f, 44.0f, 20.0f, 10.0f,
                                           crtShaderEnabled_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                            drawRoundedRectOutline(drawerX + drawerW - 60.0f, rowMasterY + 1.0f, 44.0f, 20.0f, 10.0f,
                                                  crtShaderEnabled_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                            drawCircle(crtShaderEnabled_ ? (drawerX + drawerW - 28.0f) : (drawerX + drawerW - 48.0f), rowMasterY + 11.0f, 7.0f,
                                       crtShaderEnabled_ ? theme.primaryAccent : theme.textMuted);
                        }

                        // Presets Row
                        float prY = contentBaseY + 36.0f;
                        if (projectHubScrollArea_.isVisible(prY, 32.0f) && prY >= drawerY - 4.0f && prY + 32.0f <= drawerY + drawerH + 4.0f) {
                            drawLine(drawerX + 12.0f, prY, drawerX + drawerW - 12.0f, prY, theme.borderSubtle.darken(0.15f), 1.0f);
                            drawVectorString("PRESETS:", drawerX + 12.0f, prY + 9.0f, 0.65f, theme.primaryAccent);

                            const char* prLabels[4] = {"STUDIO REF", "MAX CLARITY", "WARM VINTAGE", "RESET"};
                            const float prWidths[4] = {92.0f, 98.0f, 102.0f, 68.0f};
                            float prOffsets[4] = {74.0f, 172.0f, 276.0f, 384.0f};
                            for (int p = 0; p < 4; ++p) {
                                float px = drawerX + prOffsets[p];
                                drawRoundedRect(px, prY + 5.0f, prWidths[p], 20.0f, 4.0f, theme.controlBackground);
                                drawRoundedRectOutline(px, prY + 5.0f, prWidths[p], 20.0f, 4.0f, theme.borderSubtle, 1.0f);
                                drawVectorString(prLabels[p], px + 8.0f, prY + 9.0f, 0.65f, theme.textSecondary);
                            }
                        }

                        // Divider
                        float divY = contentBaseY + 68.0f;
                        if (divY >= drawerY && divY <= drawerY + drawerH) {
                            drawLine(drawerX + 12.0f, divY, drawerX + drawerW - 12.0f, divY, theme.borderSubtle.darken(0.15f), 1.0f);
                        }

                        const auto& crtCfg = dawnBridge_.getMaterialConfig();
                        char bufSoftness[32];
                        snprintf(bufSoftness, sizeof(bufSoftness), (crtCfg.panelSoftness <= 0.01f) ? "0.00 px (SHARP)" : "%.2f px", crtCfg.panelSoftness);
                        char bufBlackLift[32];
                        snprintf(bufBlackLift, sizeof(bufBlackLift), (crtCfg.panelBlackLift <= 0.001f) ? "0.00 (PITCH BLACK)" : "+%.3f", crtCfg.panelBlackLift);

                        struct HubSlider {
                            std::string label;
                            float minVal;
                            float maxVal;
                            float curVal;
                            std::string valStr;
                            bool isGreen;
                        };

                        HubSlider hSliders[11] = {
                            {"SCANLINE INTENSITY (0 = NONE)", 0.0f, 1.0f, crtCfg.scanlineIntensity,
                             (crtCfg.scanlineIntensity <= 0.005f) ? "0.00 (OFF / CRISP)" : (std::to_string(static_cast<int>(std::round(crtCfg.scanlineIntensity * 100.0f))) + "%"),
                             (crtCfg.scanlineIntensity <= 0.005f)},
                            {"CRT BULB CURVATURE", 0.0f, 1.50f, crtCfg.curvature,
                             (crtCfg.curvature <= 0.01f) ? "0.00 (FLAT GLASS)" : (std::to_string(static_cast<int>(std::round(crtCfg.curvature * 100.0f))) + "%"), false},
                            {"CRT TUBE ROOM REFLECTION", 0.0f, 1.0f, crtCfg.crtReflectionLevel,
                             (crtCfg.crtReflectionLevel <= 0.005f) ? "0.00 (OFF / NONE)" : (std::to_string(static_cast<int>(std::round(crtCfg.crtReflectionLevel * 100.0f))) + "%"),
                             (crtCfg.crtReflectionLevel <= 0.005f)},
                            {"H-SYNC WAVE DISTORTION", 0.0f, 2.0f, crtCfg.hsyncDistortion,
                             (crtCfg.hsyncDistortion <= 0.005f) ? "0.00 (ZERO / STATIC)" : (std::to_string(static_cast<int>(std::round(crtCfg.hsyncDistortion * 100.0f))) + "%"),
                             (crtCfg.hsyncDistortion <= 0.005f)},
                            {"SPOTLIGHT INTENSITY", 0.0f, 2.0f, crtCfg.spotlightIntensity,
                             (crtCfg.spotlightIntensity <= 0.01f) ? "0.00 (FLAT LIGHT)" : (std::to_string(static_cast<int>(std::round(crtCfg.spotlightIntensity * 100.0f))) + "%"), false},
                            {"SPOTLIGHT BEAM SIZE", 0.5f, 2.5f, crtCfg.spotlightSize,
                             std::to_string(static_cast<int>(std::round(crtCfg.spotlightSize * 100.0f))) + "%", false},
                            {"VIGNETTE CORNER FALLOFF", 0.0f, 2.0f, crtCfg.vignetteStrength,
                             (crtCfg.vignetteStrength <= 0.01f) ? "0.00 (NONE)" : (std::to_string(static_cast<int>(std::round(crtCfg.vignetteStrength * 100.0f))) + "%"), false},
                            {"BEZEL FRAME REFLECTION", 0.0f, 1.0f, crtCfg.reflectionOpacity,
                             (crtCfg.reflectionOpacity <= 0.01f) ? "0.00 (MATTE)" : (std::to_string(static_cast<int>(std::round(crtCfg.reflectionOpacity * 100.0f))) + "%"), false},
                            {"PANEL SOFTNESS (SUBPIXEL BLUR)", 0.0f, 1.50f, crtCfg.panelSoftness, bufSoftness, false},
                            {"PANEL HARDWARE SATURATION", 0.0f, 1.0f, crtCfg.panelSaturation,
                             std::to_string(static_cast<int>(std::round(crtCfg.panelSaturation * 100.0f))) + "%", false},
                            {"PANEL BLACK FLOOR LIFT", 0.0f, 0.08f, crtCfg.panelBlackLift, bufBlackLift, false}
                        };

                        const float sTrackX = drawerX + 12.0f;
                        const float sTrackW = drawerW - 32.0f;
                        const float sTrackH = 5.0f;

                        for (int i = 0; i < 11; ++i) {
                            float rowY = contentBaseY + 76.0f + i * 46.0f;
                            if (!projectHubScrollArea_.isVisible(rowY, 46.0f) || rowY < drawerY - 4.0f || rowY + 36.0f > drawerY + drawerH + 4.0f) continue;
                            float trackY = rowY + 18.0f;

                            // Label
                            drawVectorString(hSliders[i].label, sTrackX, rowY + 2.0f, 0.65f, theme.textPrimary);

                            // Value readout
                            float valW = static_cast<float>(hSliders[i].valStr.size()) * 7.0f;
                            drawVectorString(hSliders[i].valStr, sTrackX + sTrackW - valW, rowY + 2.0f, 0.68f,
                                             hSliders[i].isGreen ? theme.playActive : theme.primaryAccent);

                            // Track Background
                            drawRoundedRect(sTrackX, trackY, sTrackW, sTrackH, 2.5f, theme.controlWell);
                            drawRoundedRectOutline(sTrackX, trackY, sTrackW, sTrackH, 2.5f, theme.borderSubtle.darken(0.20f), 1.0f);

                            // Fill bar
                            float t = std::clamp((hSliders[i].curVal - hSliders[i].minVal) / (hSliders[i].maxVal - hSliders[i].minVal), 0.0f, 1.0f);
                            float fillW = t * sTrackW;
                            if (fillW > 2.0f) {
                                drawRoundedRectGradient(sTrackX, trackY, fillW, sTrackH, 2.5f,
                                                        theme.primaryAccent.darken(0.20f), theme.primaryAccent);
                            }

                            // Thumb
                            float thumbX = sTrackX + fillW;
                            float thumbY = trackY + 2.5f;
                            drawCircle(thumbX, thumbY, 7.0f, theme.backgroundDark);
                            drawCircle(thumbX, thumbY, 5.5f, theme.primaryAccent);
                            drawCircleOutline(thumbX, thumbY, 5.5f, theme.textPrimary, 1.2f);
                        }

                        // Render scrollbar constrained inside the drawer
                        if (projectHubScrollArea_.canScroll() && batchRenderer_) {
                            projectHubScrollArea_.renderScrollbar(*batchRenderer_, theme);
                        }
                    } else if (s == 4) {
                        // SECTION 4: AUDIO ENGINE & CLAP PLUGINS
                        drawRoundedRect(hubX + 20.0f, contentY + 2.0f, hubW - 40.0f, 78.0f, 6.0f, theme.backgroundDark);
                        drawRoundedRectOutline(hubX + 20.0f, contentY + 2.0f, hubW - 40.0f, 78.0f, 6.0f, theme.borderSubtle.darken(0.18f), 1.0f);
#if defined(__EMSCRIPTEN__)
                        drawVectorString("• Hardware Driver: WebAssembly AudioWorklet / WebGPU Native", hubX + 32.0f, contentY + 12.0f, 0.70f, theme.textPrimary);
#elif defined(_WIN32)
                        drawVectorString("• Hardware Driver: WASAPI Exclusive Low-Latency (miniaudio)", hubX + 32.0f, contentY + 12.0f, 0.70f, theme.textPrimary);
#else
                        drawVectorString("• Hardware Driver: ALSA / CoreAudio Low-Latency (miniaudio)", hubX + 32.0f, contentY + 12.0f, 0.70f, theme.textPrimary);
#endif
                        drawVectorString("• Sample Rate: 48,000 Hz Native Stereo / Strict Zero-Allocation Audio Loop", hubX + 32.0f, contentY + 27.0f, 0.70f, theme.textSecondary);
                        drawVectorString("• Script Engine: Dual-Mode Eatscript VM & SIMD AOT Transpiler (Pure C++20)", hubX + 32.0f, contentY + 42.0f, 0.70f, theme.textSecondary);
                        drawVectorString("• Lock-Free Concurrency: SPSC Wait-Free Event & Meter Ringbuffers", hubX + 32.0f, contentY + 57.0f, 0.70f, theme.textSecondary);

                        // CLAP Plugin Scanner & Portable Configuration Card
                        float clapCardY = contentY + 86.0f;
                        drawRoundedRect(hubX + 20.0f, clapCardY, hubW - 40.0f, 82.0f, 6.0f, theme.backgroundDark);
                        drawRoundedRectOutline(hubX + 20.0f, clapCardY, hubW - 40.0f, 82.0f, 6.0f, theme.borderSubtle.darken(0.18f), 1.0f);

                        // Row 1: Include Application / Binary Directory Toggle (Reaper portable mode)
                        bool incBin = audio::ClapPluginLoader::instance().isIncludeBinaryPathEnabled();
                        drawVectorString("Scan App / Binary Directory (Portable / Reaper mode)", hubX + 32.0f, clapCardY + 14.0f, 0.80f, theme.textPrimary);
                        drawVectorString("Includes ./plugins and executable folder without install", hubX + 32.0f, clapCardY + 27.0f, 0.65f, theme.textMuted);

                        // Toggle Switch [hubX + hubW - 74, clapCardY + 12, 44, 20]
                        drawRoundedRect(hubX + hubW - 74.0f, clapCardY + 12.0f, 44.0f, 20.0f, 10.0f,
                                       incBin ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 74.0f, clapCardY + 12.0f, 44.0f, 20.0f, 10.0f,
                                              incBin ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(incBin ? (hubX + hubW - 42.0f) : (hubX + hubW - 62.0f), clapCardY + 22.0f, 7.0f,
                                   incBin ? theme.primaryAccent : theme.textMuted);

                        // Row 2: Status & [ ↻ RESCAN PLUGINS ] Button
                        float row2Y = clapCardY + 45.0f;
                        drawLine(hubX + 24.0f, row2Y, hubX + hubW - 24.0f, row2Y, theme.borderSubtle.darken(0.15f), 1.0f);

                        size_t numDiscovered = audio::ClapPluginLoader::instance().getDiscoveredPlugins().size();
                        bool isScanning = audio::ClapPluginLoader::instance().isScanning();
                        std::string statusStr = isScanning ? "Scanning plugin directories in background..."
                                                           : ("CLAP Plugins: " + std::to_string(numDiscovered) + " discovered");
                        drawVectorString(statusStr, hubX + 32.0f, row2Y + 17.0f, 0.75f,
                                         isScanning ? theme.primaryAccent : theme.textSecondary);

                        // Rescan Button
                        float btnW = 146.0f;
                        float btnH = 24.0f;
                        float btnX = hubX + hubW - btnW - 32.0f;
                        float btnY = row2Y + 6.0f;
                        drawRoundedRect(btnX, btnY, btnW, btnH, 4.0f, theme.controlBackground);
                        drawRoundedRectOutline(btnX, btnY, btnW, btnH, 4.0f, isScanning ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawVectorString(isScanning ? "SCANNING..." : "↻ RESCAN PLUGINS", btnX + 16.0f, btnY + 7.0f, 0.70f,
                                         isScanning ? theme.primaryAccent : theme.textPrimary);
                    } else if (s == 5) {
                        // SECTION 5: CREDITS & ACKNOWLEDGMENTS
                        drawRoundedRect(hubX + 20.0f, contentY + 4.0f, hubW - 40.0f, 92.0f, 6.0f, theme.backgroundDark);
                        drawRoundedRectOutline(hubX + 20.0f, contentY + 4.0f, hubW - 40.0f, 92.0f, 6.0f, theme.borderSubtle.darken(0.18f), 1.0f);
                        drawVectorString("• Stanford CCRMA / Bank-Bensa commuted waveguide piano, bass & guitar models.", hubX + 32.0f, contentY + 14.0f, 0.70f, theme.textPrimary);
                        drawVectorString("• Roland TB-303 diode ladder acid core & analog 60ms slide modeling.", hubX + 32.0f, contentY + 30.0f, 0.70f, theme.textPrimary);
                        drawVectorString("• Vintage Soundchips: Commodore 64 MOS 6581 SID, SPC700, Yamaha YM2612 & DX7.", hubX + 32.0f, contentY + 46.0f, 0.70f, theme.textPrimary);
                        drawVectorString("• Studio FX: Convolution Reverb procedural IR simulator & 5-band parametric EQ.", hubX + 32.0f, contentY + 62.0f, 0.70f, theme.textPrimary);
                        drawVectorString("• Built with pure C++20, miniaudio, WebGPU BatchRenderer2D, and Google Filament.", hubX + 32.0f, contentY + 78.0f, 0.70f, theme.textPrimary);
                    }
                }
            }

            // Clean caps above and below drawer to prevent any subpixel bleed
            if (projectHubSection_ >= 0) {
                drawRect(hubX + 12.0f, drawerY - headerGap, hubW - 24.0f, headerGap, theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 1.0f);
                drawRect(hubX + 12.0f, drawerY + drawerH, hubW - 24.0f, headerGap, theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 1.0f);
            }

            // Status message at bottom of modal
            if (!lastStatusMessage_.empty()) {
                drawVectorString(lastStatusMessage_, hubX + 24.0f, hubY + hubH - 18.0f, 0.75f, theme.playActive);
            }
        }

        // 6. REUSABLE VALUE EDIT MODAL DIALOG
        if (valueEditDialog_.isOpen()) {
            valueEditDialog_.layout(static_cast<float>(width_), static_cast<float>(height_));
            if (batchRenderer_) {
                valueEditDialog_.render(*batchRenderer_, theme);
            }
        }

        // 6a. STACKED ICON SEARCH MODAL DIALOG (stacked above value edit dialog)
        if (valueEditDialog_.isOpen() && modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
            modularArrangerView_->getIconSearchDialog().layout(static_cast<float>(width_), static_cast<float>(height_));
            if (batchRenderer_) {
                modularArrangerView_->getIconSearchDialog().render(*batchRenderer_, theme);
            }
        }

        // 6b. AUDIO TO MIDI TRANSCRIPTION MODAL DIALOG
        if (audioToMidiDialog_.isOpen()) {
            audioToMidiDialog_.layout(static_cast<float>(width_), static_cast<float>(height_));
            audioToMidiDialog_.update(frameDt);
            if (batchRenderer_) {
                audioToMidiDialog_.render(*batchRenderer_, theme);
            }
        }

        // 7. UNIVERSAL QUICK COMMAND PALETTE DIALOG (Ctrl+P / Ctrl+K / Search)
        if (commandPaletteDialog_.isOpen()) {
            commandPaletteDialog_.layout(static_cast<float>(width_), static_cast<float>(height_));
            commandPaletteDialog_.update(frameDt);
            if (batchRenderer_) {
                commandPaletteDialog_.render(*batchRenderer_, theme);
            }
        }

        // 7b. REUSABLE MODAL PLUGIN / FX / PRESET SEARCH DIALOG (Full-Screen Backdrop)
        PluginSearchDialog* activePluginDialog = getActivePluginSearchDialog();
        if (activePluginDialog && activePluginDialog->isOpen()) {
            activePluginDialog->layout(static_cast<float>(width_), static_cast<float>(height_));
            if (batchRenderer_) {
                activePluginDialog->render(*batchRenderer_, theme);
            }
        }

        // 7c. REUSABLE MODAL THEME BROWSER DIALOG
        if (themeBrowserDialog_.isOpen()) {
            themeBrowserDialog_.layout(static_cast<float>(width_), static_cast<float>(height_));
            if (batchRenderer_) {
                themeBrowserDialog_.render(*batchRenderer_, theme);
            }
        }

        // 8. CRT SHADER & CHASSIS TWEAKER HUD MODAL
        // 8. REUSABLE COLOR PICKER MODAL DIALOG
        ColorPickerDialog* activeColorPicker = getActiveColorPickerDialog();
        if (activeColorPicker && activeColorPicker->isOpen()) {
            activeColorPicker->layout(static_cast<float>(width_), static_cast<float>(height_));
            if (batchRenderer_) {
                activeColorPicker->render(*batchRenderer_, theme);
            }
        }

        // Persistent Floating Status Toast Overlay (F10 / F11 / Action Feedback)
        if (statusToastTimer_ > 0.0f && !statusToastText_.empty()) {
            statusToastTimer_ -= (1.0f / 60.0f);
            float alpha = std::clamp(statusToastTimer_ / 0.4f, 0.0f, 1.0f);
            float pillW = std::max(280.0f, static_cast<float>(statusToastText_.size()) * 8.5f + 48.0f);
            float pillH = 34.0f;
            float pillX = (static_cast<float>(width_) - pillW) * 0.5f;
            float pillY = 16.0f;

            drawRect(pillX - 2.0f, pillY - 2.0f, pillW + 4.0f, pillH + 4.0f, 0.0f, 0.0f, 0.0f, 0.50f * alpha);
            drawRectGradient(pillX, pillY, pillW, pillH, 0.14f, 0.16f, 0.20f, 0.08f, 0.09f, 0.12f, 0.96f * alpha);
            drawRectOutline(pillX, pillY, pillW, pillH, 1.0f, 0.65f, 0.10f, 0.90f * alpha, 1.5f);
            drawCircle(pillX + 16.0f, pillY + 17.0f, 4.0f, 1.0f, 0.65f, 0.10f, alpha);
            drawVectorString(statusToastText_, pillX + 28.0f, pillY + 11.0f, 0.88f, 0.95f, 0.96f, 1.0f, alpha);
        }

        if (batchRenderer_) {
            batchRenderer_->endFrame();
        }
        g_activeBatchRenderer = nullptr;
        ScoreVectorGlyphs::setBatchRenderer(nullptr);
    }
#endif

    // Submit frame to Google Dawn / WebGPU Bridge
    if (dawnBridge_.isNativeActive()) {
        const uint32_t* dawPixels = batchRenderer_ ? batchRenderer_->getFramebuffer() : nullptr;
        dawnBridge_.setCrtShaderEnabled(crtShaderEnabled_);
        uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
        uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));
        float subBass = engine_ ? engine_->getSubBassEnergy() : 0.0f;
        dawnBridge_.renderCrtScene(dawPixels, physW, physH, lampTime_, subBass, renderScale_);
    } else {
        dawnBridge_.beginFrame(static_cast<float>(width_), static_cast<float>(height_), 1.0f);
        dawnBridge_.endFrame();
    }

    if (FloatingWindowManager::instance().getOpenCount() > 0) {
        FloatingWindowManager::instance().renderAllWindows(getTheme(), scopeBuffer_, 128);
    }
}

DialogLayout GuiWindow::computeDialogLayout(float w, float h) const noexcept {
    DialogLayout dl{};
    dl.w = std::min(w, static_cast<float>(width_) - 24.0f);
    dl.h = std::min(h, static_cast<float>(height_) - 24.0f);
    dl.x = std::max(12.0f, (static_cast<float>(width_) - dl.w) * 0.5f);
    dl.y = std::max(12.0f, (static_cast<float>(height_) - dl.h) * 0.5f);
    dl.contentX = dl.x + 16.0f;
    dl.contentY = dl.y + 48.0f;
    dl.contentW = dl.w - 32.0f;
    dl.contentH = std::max(80.0f, dl.h - 86.0f);
    dl.closeBtnW = 24.0f;
    dl.closeBtnH = 24.0f;
    dl.closeBtnX = dl.x + dl.w - 32.0f;
    dl.closeBtnY = dl.y + 12.0f;
    dl.bottomCloseW = 60.0f;
    dl.bottomCloseH = 22.0f;
    dl.bottomCloseX = dl.x + dl.w - dl.bottomCloseW - 20.0f;
    dl.bottomCloseY = dl.y + dl.h - dl.bottomCloseH - 12.0f;
    return dl;
}

DialogLayout GuiWindow::drawModalDialogFrame(const DialogFrameConfig& config) {
    const auto& theme = getTheme();
    DialogLayout dl = computeDialogLayout(config.width, config.height);

    // Soft dimming backdrop (GPU blur disabled by default for maximum 60+ FPS performance)
    if (config.enableBlur && batchRenderer_) {
        batchRenderer_->applyBackdropBlur(4.0f, 0.48f);
    }

    // 1. Semi-transparent backdrop shadow
    drawRect(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 0.0f, 0.0f, 0.45f);

    // 2. Dialog Chassis with softly rounded corners
    drawRoundedRect(dl.x - 2.0f, dl.y - 2.0f, dl.w + 4.0f, dl.h + 4.0f, config.cornerRadius + 1.0f, Color(0.0f, 0.0f, 0.0f, 0.40f));
    drawRoundedRectGradient(dl.x, dl.y, dl.w, dl.h, config.cornerRadius,
                            theme.panelBackground, theme.panelBackground.darken(0.06f));
    drawRoundedRectOutline(dl.x, dl.y, dl.w, dl.h, config.cornerRadius,
                           theme.borderSubtle.darken(0.12f), 1.2f);

    // 3. Top Header: Eatsbits Logo + Title in theme.primaryAccent
    float textStartX = dl.x + 22.0f;
    if (config.showLogo) {
        drawEatsbitsLogo(dl.x + 22.0f, dl.y + 16.0f, 22.0f, 1.0f, false);
        textStartX += 32.0f;
    }
    drawVectorString(config.title, textStartX, dl.y + 20.0f, 1.0f, theme.primaryAccent);

    // 4. Close Button [Top-Right]
    if (config.showCloseButton) {
        float hClX = dl.closeBtnX + dl.closeBtnW * 0.5f;
        float hClY = dl.closeBtnY + dl.closeBtnH * 0.5f;
        bool hClHov = (mouseX_ >= dl.closeBtnX - 4.0f && mouseX_ <= dl.closeBtnX + dl.closeBtnW + 4.0f &&
                       mouseY_ >= dl.closeBtnY - 4.0f && mouseY_ <= dl.closeBtnY + dl.closeBtnH + 4.0f) ||
                      (std::hypot(mouseX_ - hClX, mouseY_ - hClY) <= 13.0f);
        drawIconScrewClose(hClX, hClY, 9.0f, hClHov, theme.primaryAccent);
    }

    // 5. Close Button [Bottom-Right]
    if (config.showBottomClose) {
        bool bClHov = (mouseX_ >= dl.bottomCloseX && mouseX_ <= dl.bottomCloseX + dl.bottomCloseW &&
                       mouseY_ >= dl.bottomCloseY && mouseY_ <= dl.bottomCloseY + dl.bottomCloseH);
        drawVectorString("CLOSE", dl.bottomCloseX + 6.0f, dl.bottomCloseY + 5.0f, 0.85f,
                         bClHov ? theme.primaryAccent.lighten(0.2f) : theme.primaryAccent);
    }

    return dl;
}

void GuiWindow::initFontRenderer() {
#if EATS_HAS_GLFW
    fontRenderer_ = std::make_unique<FontRendererState>();
    g_activeFontRenderer = fontRenderer_.get();

    // Prioritize Oswald for primary UI typography
    std::vector<std::string> searchPaths;
    if (!pendingFontPath_.empty()) {
        searchPaths.push_back(pendingFontPath_);
    }
    searchPaths.push_back("assets/fonts/Oswald-Medium.ttf");
    searchPaths.push_back("assets/fonts/Oswald-Regular.ttf");
    searchPaths.push_back("assets/fonts/Oswald-Bold.ttf");
    searchPaths.push_back("../assets/fonts/Oswald-Medium.ttf");
    searchPaths.push_back("../../assets/fonts/Oswald-Medium.ttf");
    searchPaths.push_back("c:/git/eatsbits/assets/fonts/Oswald-Medium.ttf");

    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            std::string fName = pendingFontName_.empty() ? "ui" : pendingFontName_;
            if (loadFont(path, fName)) {
                break;
            }
        }
    }

    // Fallback UI font (e.g. Oswald-Regular if Oswald-Medium is primary)
    if (fontRenderer_->fontNormal != eatsbits::text::kInvalidFontId) {
        const std::vector<std::string> fallbackPaths = {
            "assets/fonts/Oswald-Regular.ttf",
            "../assets/fonts/Oswald-Regular.ttf",
            "../../assets/fonts/Oswald-Regular.ttf",
            "c:/git/eatsbits/assets/fonts/Oswald-Regular.ttf"
        };
        for (const auto& fbPath : fallbackPaths) {
            if (std::filesystem::exists(fbPath) && fbPath != fontRenderer_->loadedFontPath) {
                eatsbits::text::FontId fbId = fontRenderer_->fontMgr.loadFontFromFile("fallback", fbPath);
                if (fbId != eatsbits::text::kInvalidFontId) {
                    fontRenderer_->fontMgr.setFallbackFont(fontRenderer_->fontNormal, fbId);
                    fontRenderer_->fontFallback = fbId;
                    std::cout << "[GuiWindow] Loaded fallback font from " << fbPath << std::endl;
                    break;
                }
            }
        }
    }

    // Load Share Tech Mono for code editor and digital metrics readout
    std::vector<std::string> monoSearchPaths;
    if (!pendingMonoFontPath_.empty()) {
        monoSearchPaths.push_back(pendingMonoFontPath_);
    }
    monoSearchPaths.push_back("assets/fonts/ShareTechMono-Regular.ttf");
    monoSearchPaths.push_back("../assets/fonts/ShareTechMono-Regular.ttf");
    monoSearchPaths.push_back("../../assets/fonts/ShareTechMono-Regular.ttf");
    monoSearchPaths.push_back("c:/git/eatsbits/assets/fonts/ShareTechMono-Regular.ttf");

    for (const auto& mPath : monoSearchPaths) {
        if (std::filesystem::exists(mPath)) {
            std::string mfName = pendingMonoFontName_.empty() ? "mono" : pendingMonoFontName_;
            if (loadMonoFont(mPath, mfName)) {
                break;
            }
        }
    }

    // If mono font was not found on disk, load the embedded Share Tech Mono!
    if (fontRenderer_->fontMono == eatsbits::text::kInvalidFontId) {
        fontRenderer_->fontMono = fontRenderer_->fontMgr.loadEmbeddedFallback("embedded_mono");
        std::cout << "[GuiWindow] Loaded embedded Share Tech Mono fallback font." << std::endl;
    }

    // If primary font was not found on disk, use embedded font as primary!
    if (fontRenderer_->fontNormal == eatsbits::text::kInvalidFontId) {
        fontRenderer_->fontNormal = fontRenderer_->fontMono;
    }

    // Load Nerd Font Symbols as fallback for iconography
    const std::vector<std::string> nerdSymbolPaths = {
        "assets/fonts/SymbolsNerdFont-Regular.ttf",
        "../assets/fonts/SymbolsNerdFont-Regular.ttf",
        "../../assets/fonts/SymbolsNerdFont-Regular.ttf",
        "c:/git/eatsbits/assets/fonts/SymbolsNerdFont-Regular.ttf"
    };
    for (const auto& nsPath : nerdSymbolPaths) {
        if (std::filesystem::exists(nsPath)) {
            eatsbits::text::FontId nsId = fontRenderer_->fontMgr.loadFontFromFile("nerd_symbols", nsPath);
            if (nsId != eatsbits::text::kInvalidFontId) {
                if (fontRenderer_->fontFallback != eatsbits::text::kInvalidFontId) {
                    fontRenderer_->fontMgr.setFallbackFont(fontRenderer_->fontFallback, nsId);
                } else if (fontRenderer_->fontNormal != eatsbits::text::kInvalidFontId) {
                    fontRenderer_->fontMgr.setFallbackFont(fontRenderer_->fontNormal, nsId);
                }
                if (fontRenderer_->fontMono != eatsbits::text::kInvalidFontId) {
                    fontRenderer_->fontMgr.setFallbackFont(fontRenderer_->fontMono, nsId);
                }
                std::cout << "[GuiWindow] Loaded Nerd Font Symbols fallback from " << nsPath << std::endl;
                break;
            }
        }
    }
#endif
}

bool GuiWindow::loadFont(const std::string& fontPath, const std::string& fontName) {
    activeFontName_ = fontName;
    activeFontPath_ = fontPath;

    if (!fontRenderer_) {
        pendingFontPath_ = fontPath;
        pendingFontName_ = fontName;
        return false;
    }

    if (!std::filesystem::exists(fontPath)) {
        std::cerr << "[GuiWindow] Error: Font file not found at " << fontPath << std::endl;
        return false;
    }

    eatsbits::text::FontId fid = fontRenderer_->fontMgr.loadFontFromFile(fontName, fontPath);
    if (fid == eatsbits::text::kInvalidFontId) {
        std::cerr << "[GuiWindow] Error: Failed to parse font file " << fontPath << std::endl;
        return false;
    }

    fontRenderer_->fontNormal = fid;
    fontRenderer_->loadedFontPath = fontPath;
    if (fontRenderer_->fontMono != eatsbits::text::kInvalidFontId) {
        fontRenderer_->fontMgr.setFallbackFont(fontRenderer_->fontMono, fontRenderer_->fontNormal);
    }
    std::cout << "[GuiWindow] Successfully loaded font '" << fontName << "' (" << fontPath << ")" << std::endl;
    return true;
}

bool GuiWindow::loadMonoFont(const std::string& fontPath, const std::string& fontName) {
    activeMonoFontName_ = fontName;
    activeMonoFontPath_ = fontPath;

    if (!fontRenderer_) {
        pendingMonoFontPath_ = fontPath;
        pendingMonoFontName_ = fontName;
        return false;
    }

    if (!std::filesystem::exists(fontPath)) {
        std::cerr << "[GuiWindow] Error: Mono font file not found at " << fontPath << std::endl;
        return false;
    }

    eatsbits::text::FontId fid = fontRenderer_->fontMgr.loadFontFromFile(fontName, fontPath);
    if (fid == eatsbits::text::kInvalidFontId) {
        std::cerr << "[GuiWindow] Error: Failed to parse mono font file " << fontPath << std::endl;
        return false;
    }

    fontRenderer_->fontMono = fid;
    fontRenderer_->loadedMonoFontPath = fontPath;
    if (fontRenderer_->fontNormal != eatsbits::text::kInvalidFontId) {
        fontRenderer_->fontMgr.setFallbackFont(fontRenderer_->fontMono, fontRenderer_->fontNormal);
    }
    std::cout << "[GuiWindow] Successfully loaded mono font '" << fontName << "' (" << fontPath << ")" << std::endl;
    return true;
}

bool GuiWindow::isFontLoaded() const noexcept {
    return fontRenderer_ && fontRenderer_->fontNormal != eatsbits::text::kInvalidFontId;
}

bool GuiWindow::isMonoFontLoaded() const noexcept {
    return fontRenderer_ && fontRenderer_->fontMono != eatsbits::text::kInvalidFontId;
}

std::string GuiWindow::getTransportTooltip(float x, float y) const noexcept {
    if (y < 0.0f || y > 56.0f) {
        return "";
    }

    const float r = static_cast<float>(width_);

    // 0. Top-Left Eatsbits Brand Logo (opens Project Hub / Settings dialog) [8 to 50]
    if (x >= 8.0f && x <= 50.0f && y >= 8.0f && y <= 48.0f) {
        return "EATSBITS Project Hub & Settings";
    }

    // 1. Tactile Clustered Transport Keys: Play [54..92], Stop [92..130], Record [130..168]
    if (x >= 54.0f && x < 92.0f && y >= 8.0f && y <= 48.0f) {
        const bool isPlaying = engine_ ? engine_->getSequencer().isPlaying() : false;
        return isPlaying ? "Pause Playback (Space)" : "Play / Start Playback (Space)";
    }
    if (x >= 92.0f && x < 130.0f && y >= 8.0f && y <= 48.0f) {
        return "Stop Playback (Return)";
    }
    if (x >= 130.0f && x < 168.0f && y >= 8.0f && y <= 48.0f) {
        return "Toggle Recording (R)";
    }

    // 2. Minimal Recessed BPM Display Capsule [176 to 296]
    if (x >= 176.0f && x <= 296.0f && y >= 8.0f && y <= 48.0f) {
        int bpmVal = engine_ ? static_cast<int>(std::round(engine_->getSequencer().getTransport().getBpm())) : 120;
        return "Project Tempo: " + std::to_string(bpmVal) + " BPM (Click to Edit)";
    }

    // Secondary Timecode / Bar:Beat:Div Readout [r - 228 to r - 132]
    if (x >= (r - 228.0f) && x < (r - 132.0f) && y >= 8.0f && y <= 48.0f) {
        return "Song Position (Bar : Beat : 16th)";
    }

    // 3. Right-Side Tactile Pushbuttons:
    // Lock Button [r - 134 to r - 96]
    if (x >= (r - 134.0f) && x < (r - 96.0f) && y >= 8.0f && y <= 48.0f) {
        return projectLocked_ ? "Workspace Locked (Ctrl+L to Unlock)" : "Lock Workspace Editing (Ctrl+L)";
    }

    // Search / Fullscreen Button [r - 96 to r - 66]
    if (x >= (r - 96.0f) && x < (r - 66.0f) && y >= 8.0f && y <= 48.0f) {
        return isFullscreen_ ? "Exit Fullscreen Device Mode (F)" : "Fullscreen Device Mode / Finder (F)";
    }

    // Folder / Preset Library Button [r - 66 to r - 4]
    if (x >= (r - 66.0f) && x <= (r - 4.0f) && y >= 8.0f && y <= 48.0f) {
        return browserOpen_ ? "Close Preset & Sample Browser (B)" : "Preset & Sample Browser (B)";
    }

    // Snap Selector Box [695, 11, 65, 34]
    if (x >= 695.0f && x <= 760.0f && y >= 11.0f && y <= 45.0f) {
        return "Timeline Grid Snap (1/16)";
    }

    // Scale Selector Box [r - 345, 11, 72, 34]
    if (x >= (r - 345.0f) && x <= (r - 273.0f) && y >= 11.0f && y <= 45.0f) {
        return "Musical Scale & Key Selector";
    }

    // Metronome Toggle [r - 265, 11, 75, 34]
    if (x >= (r - 265.0f) && x <= (r - 190.0f) && y >= 11.0f && y <= 45.0f) {
        return "Metronome Click (C)";
    }

    // Loop Toggle [r - 180, 11, 65, 34]
    if (x >= (r - 180.0f) && x <= (r - 115.0f) && y >= 11.0f && y <= 45.0f) {
        return "Loop Playback (L)";
    }

    return "";
}

HitTestTransportResult GuiWindow::hitTestTransport(float x, float y) const noexcept {
    HitTestTransportResult res{};
    if (y < 0.0f || y > 56.0f) {
        return res;
    }

    const float r = static_cast<float>(width_);

    // 0. Top-Left Eatsbits Brand Logo (opens Project Hub / Settings dialog) [10 to 46]
    if (x >= 10.0f && x <= 46.0f && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::ProjectHubToggle;
        return res;
    }

    // 1. Tactile Clustered Transport Keys: Play [54..92], Stop [92..130], Record [130..168]
    if (x >= 54.0f && x < 92.0f && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::PlayPause;
        return res;
    }
    if (x >= 92.0f && x < 130.0f && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::Stop;
        return res;
    }
    if (x >= 130.0f && x < 168.0f && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::Record;
        return res;
    }

    // 2. Minimal Recessed BPM Display Capsule [178 to 294]
    if (x >= 176.0f && x <= 296.0f && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::Bpm;
        return res;
    }

    // 3. Right-Side Tactile Pushbuttons:
    // Lock Button [r - 134 to r - 96]
    if (x >= (r - 134.0f) && x < (r - 96.0f) && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::LockToggle;
        return res;
    }

    // Search / Fullscreen Button [r - 96 to r - 66]
    if (x >= (r - 96.0f) && x < (r - 66.0f) && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::FullscreenToggle;
        return res;
    }

    // Folder / Preset Library Button [r - 66 to r - 4]
    if (x >= (r - 66.0f) && x <= (r - 4.0f) && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::BrowserToggle;
        return res;
    }

    // Snap Selector Box [695, 11, 65, 34]
    if (x >= 695.0f && x <= 760.0f && y >= 11.0f && y <= 45.0f) {
        res.hit = true;
        res.action = TransportAction::SnapToggle;
        return res;
    }

    // Scale Selector Box [r - 345, 11, 72, 34]
    if (x >= (r - 345.0f) && x <= (r - 273.0f) && y >= 11.0f && y <= 45.0f) {
        res.hit = true;
        res.action = TransportAction::ScaleToggle;
        return res;
    }

    // Metronome Toggle [r - 265, 11, 75, 34]
    if (x >= (r - 265.0f) && x <= (r - 190.0f) && y >= 11.0f && y <= 45.0f) {
        res.hit = true;
        res.action = TransportAction::MetronomeToggle;
        return res;
    }

    // Loop Toggle [r - 180, 11, 65, 34]
    if (x >= (r - 180.0f) && x <= (r - 115.0f) && y >= 11.0f && y <= 45.0f) {
        res.hit = true;
        res.action = TransportAction::LoopToggle;
        return res;
    }

    // Project Browser Button [r - 105, 11, 85, 34]
    if (x >= (r - 105.0f) && x <= (r - 20.0f) && y >= 11.0f && y <= 45.0f) {
        res.hit = true;
        res.action = TransportAction::BrowserToggle;
        return res;
    }

    // Legacy tab positions (for backwards compatibility if invoked directly):
    // Tab Rack [r - 430, 12, 68, 32]
    if (x >= (r - 430.0f) && x <= (r - 362.0f) && y >= 12.0f && y <= 44.0f) {
        res.hit = true;
        res.action = TransportAction::TabRack;
        return res;
    }
    // Tab Hardware Panel [r - 355, 12, 75, 32]
    if (x >= (r - 355.0f) && x <= (r - 280.0f) && y >= 12.0f && y <= 44.0f) {
        res.hit = true;
        res.action = TransportAction::TabHardware;
        return res;
    }
    // Tab Tracker [r - 274, 12, 80, 32]
    if (x >= (r - 274.0f) && x <= (r - 194.0f) && y >= 12.0f && y <= 44.0f) {
        res.hit = true;
        res.action = TransportAction::TabTracker;
        return res;
    }
    // Tab Mixer [r - 188, 12, 75, 32]
    if (x >= (r - 188.0f) && x <= (r - 113.0f) && y >= 12.0f && y <= 44.0f) {
        res.hit = true;
        res.action = TransportAction::TabMixer;
        return res;
    }
    // Tab Script [r - 106, 12, 75, 32]
    if (x >= (r - 106.0f) && x <= (r - 31.0f) && y >= 12.0f && y <= 44.0f) {
        res.hit = true;
        res.action = TransportAction::TabEatscript;
        return res;
    }

    // Preset switcher buttons (when activeView_ == HardwarePanel / Track):
    if ((activeView_ == WorkspaceView::HardwarePanel || activeView_ == WorkspaceView::Track) && y >= 85.0f && y <= 120.0f) {
        if (x >= 52.0f && x <= 134.0f) {
            res.hit = true;
            res.action = TransportAction::PresetPrev;
            return res;
        }
        if (x >= 135.0f && x <= 218.0f) {
            res.hit = true;
            res.action = TransportAction::PresetNext;
            return res;
        }
    }

    return res;
}

HitTestBottomNavResult GuiWindow::hitTestBottomNav(float x, float y) const noexcept {
    HitTestBottomNavResult res{};
    const float bPanelY = static_cast<float>(height_) - 48.0f;
    if (y < bPanelY || y > static_cast<float>(height_)) {
        return res;
    }

    const float totalNavW = static_cast<float>(width_) - 24.0f;
    const float navBtnW = totalNavW / 5.0f;

    if (x < 12.0f || x > (12.0f + totalNavW)) {
        return res;
    }

    int idx = static_cast<int>((x - 12.0f) / navBtnW);
    if (idx >= 0 && idx < 5) {
        res.hit = true;
        switch (idx) {
            case 0: res.action = BottomNavAction::Arranger; break;
            case 1: res.action = BottomNavAction::Edit; break;
            case 2: res.action = BottomNavAction::Track; break;
            case 3: res.action = BottomNavAction::Mixer; break;
            case 4: res.action = BottomNavAction::Design; break;
            default: break;
        }
    }
    return res;
}

HitTestMixerResult GuiWindow::hitTestMixer(float x, float y) const noexcept {
    HitTestMixerResult res{};
    if (activeView_ != WorkspaceView::Mixer) return res;

    // 0. Check Top Modular Section Toolbar (Y: 58..90)
    if (y >= 58.0f && y <= 90.0f) {
        if (x >= 390.0f && x <= 444.0f) {
            res.hit = true;
            res.isPresetFull = true;
            return res;
        }
        if (x >= 452.0f && x <= 560.0f) {
            res.hit = true;
            res.isPresetFaders = true;
            return res;
        }
        if (x >= 568.0f && x <= 700.0f) {
            res.hit = true;
            res.isPresetMeters = true;
            return res;
        }
        if (x >= 710.0f && x <= 786.0f) {
            res.hit = true;
            res.isToggleRouting = true;
            return res;
        }
        if (x >= 794.0f && x <= 842.0f) {
            res.hit = true;
            res.isTogglePan = true;
            return res;
        }
        if (x >= 850.0f && x <= 930.0f) {
            res.hit = true;
            res.isToggleButtons = true;
            return res;
        }
        if (x >= 938.0f && x <= 1012.0f) {
            res.hit = true;
            res.isToggleMeters = true;
            return res;
        }
    }

    const float bPanelY = static_cast<float>(height_) - 48.0f;
    const float startY = 96.0f;
    const float bottomY = bPanelY - 6.0f;
    const float topY = 56.0f;

    const float w = static_cast<float>(width_);
    const float propW = mixerPropertiesExpanded_ ? mixerPropertiesWidth_ : 0.0f;
    const float pullTabW = kMixerPullTabW;
    const float drawerTotalW = mixerPropertiesExpanded_ ? (pullTabW + propW) : pullTabW;
    const float browserOffset = browserOpen_ ? ProjectBrowserDrawer::getDrawerWidth() : 0.0f;
    const float rightBoundary = w - drawerTotalW - browserOffset;
    const float pullTabX = rightBoundary;
    const float propX = pullTabX + pullTabW;

    // 0.1 Check Right-Side Pull-Tab Strip Hit
    if (x >= pullTabX && x <= pullTabX + pullTabW && y >= topY && y <= bottomY) {
        res.hit = true;
        res.isPullTab = true;
        return res;
    }

    // 0.2 Check Expanded Track Properties Sidebar Body Hit
    if (mixerPropertiesExpanded_ && x > pullTabX + pullTabW && x <= propX + propW && y >= topY && y <= bottomY) {
        res.hit = true;
        // Close Button [propX + propW - 28, topY + 7, 22, 22]
        float clBtnX = propX + propW - 28.0f;
        float clBtnY = topY + 7.0f;
        if (x >= clBtnX && x <= clBtnX + 22.0f && y >= clBtnY && y <= clBtnY + 22.0f) {
            res.isPropertiesClose = true;
            return res;
        }

        float cardX = propX + 10.0f;
        float cardW = propW - 20.0f;
        float cardY = topY + 44.0f;
        float cardH = bottomY - cardY - 8.0f;
        auto inspHit = hitTestTrackPropertiesContainer(x, y, cardX, cardY, cardW, cardH, selectedTrackIndex_, trackInspectorScrollY_, true);
        if (inspHit.hit) {
            res.trackInspectorHit = inspHit;
            return res;
        }
        return res;
    }

    // 1. Check Master Strip [40, startY, 135, stripH] (Also accepts legacy Y: 110..640)
    if (x >= 40.0f && x <= 175.0f && y >= std::min(startY, 110.0f) && y <= std::max(bottomY, 640.0f)) {
        res.hit = true;
        res.isMaster = true;
        res.channelIndex = 999;

        // Master Backlit LCD Screen: [48, 116..150]
        if (x >= 48.0f && x <= 167.0f && y >= 116.0f && y <= 150.0f) {
            res.isLcdScreen = true;
            return res;
        }

        // Master Mute button: [54, 152, 50, 24]
        if (x >= 54.0f && x <= 104.0f && y >= 152.0f && y <= 176.0f) {
            res.isMute = true;
            return res;
        }
        // Master Pan pot: around (107, 215), radius ~ 22
        if (std::hypot(x - 107.0f, y - 215.0f) <= 22.0f) {
            res.isPan = true;
            return res;
        }
        // Master Fader slot: [65, 260, 50, (bottomY - 260)]
        float mSlotBottom = std::max(555.0f, bottomY - 36.0f);
        if (x >= 60.0f && x <= 115.0f && y >= 260.0f && y <= mSlotBottom) {
            res.isFader = true;
            float totalTravel = std::max(120.0f, mSlotBottom - 265.0f);
            float norm = 1.0f - std::clamp((y - 265.0f) / totalTravel, 0.0f, 1.0f);
            res.normVal = norm * 1.5f;
            return res;
        }
        return res;
    }

    // 2. Check Channel Strips [195 + i*(135+16), startY, 135, stripH]
    const float startX = 195.0f;
    const float stripW = 135.0f;
    const float gap = 16.0f;

    for (size_t i = 0; i < mixerStrips_.size() && i < 5; ++i) {
        float cx = startX + i * (stripW + gap);
        if (x >= cx && x <= cx + stripW && y >= std::min(startY, 110.0f) && y <= std::max(bottomY, 640.0f)) {
            res.hit = true;
            res.channelIndex = static_cast<uint32_t>(i);

            // Backlit LCD Screen: [cx + 8, 116..150]
            if (x >= cx + 8.0f && x <= cx + stripW - 8.0f && y >= 116.0f && y <= 150.0f) {
                res.isLcdScreen = true;
                return res;
            }

            // Mute button: [cx + 12, 152, 50, 24]
            if (x >= cx + 12.0f && x <= cx + 62.0f && y >= 152.0f && y <= 176.0f) {
                res.isMute = true;
                return res;
            }
            // Solo button: [cx + 68, 152, 50, 24]
            if (x >= cx + 68.0f && x <= cx + 118.0f && y >= 152.0f && y <= 176.0f) {
                res.isSolo = true;
                return res;
            }
            // Automation Mode button: [cx + 68, 184, 50, 18]
            if (x >= cx + 68.0f && x <= cx + 118.0f && y >= 184.0f && y <= 202.0f) {
                res.isAutomation = true;
                return res;
            }
            // Pan pot: center (cx + 65, 215)
            if (std::hypot(x - (cx + 65.0f), y - 215.0f) <= 22.0f) {
                res.isPan = true;
                return res;
            }
            // Phase Invert button: [cx + 80, 269, 26, 20]
            if (x >= cx + 80.0f && x <= cx + 106.0f && y >= 269.0f && y <= 289.0f) {
                res.isPhase = true;
                return res;
            }
            // FX In button: [cx + 80, 295, 26, 20]
            if (x >= cx + 80.0f && x <= cx + 106.0f && y >= 295.0f && y <= 315.0f) {
                res.isFxIn = true;
                return res;
            }
            // Freeze button: [cx + 80, 321, 26, 20]
            if (x >= cx + 80.0f && x <= cx + 106.0f && y >= 321.0f && y <= 341.0f) {
                res.isFreeze = true;
                return res;
            }
            // Edit button: [cx + 20, 602..626] or [cx + 20, bottomY - 34..bottomY]
            float editBtnY = (bottomY >= 635.0f) ? (bottomY - 26.0f) : 602.0f;
            if (x >= cx + 20.0f && x <= cx + 115.0f &&
                ((y >= 600.0f && y <= 628.0f) || (y >= editBtnY && y <= editBtnY + 26.0f))) {
                res.isEditButton = true;
                return res;
            }

            // Fader slot: [cx + 10, 260, 65, (editBtnY - 8)]
            float chSlotBottom = editBtnY - 8.0f;
            if (x >= cx + 10.0f && x <= cx + 75.0f && y >= 260.0f && y <= chSlotBottom) {
                res.isFader = true;
                float totalTravel = std::max(120.0f, chSlotBottom - 265.0f);
                float norm = 1.0f - std::clamp((y - 265.0f) / totalTravel, 0.0f, 1.0f);
                res.normVal = norm * 1.5f;
                return res;
            }

            return res;
        }
    }

    return res;
}

HitTestTrackTabResult GuiWindow::hitTestTrackTab(float, float) const noexcept {
    HitTestTrackTabResult res{};
    return res;
}

void GuiWindow::drawTrackPropertiesContainer(float x, float y, float width, float height, uint32_t trackIndex, float scrollY, bool isCompact) {
    uint32_t tIdx = (trackIndex < arrangerTracks_.size()) ? trackIndex : 0;
    auto& trk = (tIdx < arrangerTracks_.size()) ? arrangerTracks_[tIdx] : arrangerTracks_[0];
    if (selectedTrackIndex_ != tIdx) {
        syncTrackToPreset(tIdx);
    }
    (void)getTheme();

    static const float swatches[8][3] = {
        {0.0f, 0.90f, 1.0f},   // Neon Cyan
        {1.0f, 0.55f, 0.0f},   // Neon Amber
        {0.0f, 1.00f, 0.40f},  // Acid Green
        {1.0f, 0.00f, 0.48f},  // Hot Pink
        {0.74f, 0.00f, 1.0f},  // Electric Purple
        {1.0f, 0.20f, 0.20f},  // Crimson Red
        {1.0f, 0.85f, 0.0f},   // Gold Yellow
        {0.20f, 0.60f, 1.0f}   // Sky Blue
    };

    float card1H = isCompact ? 52.0f : 60.0f;
    float gap1 = isCompact ? 8.0f : 8.0f;
    float cardColorH = isCompact ? 50.0f : 0.0f;
    float gapColor = isCompact ? 8.0f : 0.0f;
    float card2H = isCompact ? 86.0f : 68.0f;
    float gap2 = isCompact ? 6.0f : 8.0f;
    const float designW = 520.0f;
    float card3H = isCompact ? (30.0f + 210.0f * (std::max(200.0f, width - 16.0f) / designW)) : 280.0f;
    float gap3 = isCompact ? 8.0f : 12.0f;
    float card4H = isCompact ? 84.0f : 80.0f;
    float gap4 = isCompact ? 6.0f : 10.0f;
    float card5H = isCompact ? 180.0f : 120.0f;
    float gap5 = isCompact ? 6.0f : 10.0f;
    float card6H = isCompact ? 280.0f : 135.0f;
    float gap6 = isCompact ? 16.0f : 20.0f;

    float totalContentH = card1H + gap1 + (isCompact ? (cardColorH + gapColor) : 0.0f) + card2H + gap2 + card3H + gap3 + card4H + gap4 + card5H + gap5 + card6H + gap6;
    float maxScroll = std::max(10.0f, totalContentH - height + 40.0f);

    bool needScrollbar = (totalContentH > height);
    float contentW = needScrollbar ? (width - 16.0f) : width;

    // Helper lambda for mini dials
    auto drawMiniDial = [&](float cx, float cy, float radius, float normVal, const char* label) {
        constexpr float minA = -2.35619449f;
        constexpr float maxA = 2.35619449f;
        float curA = minA + std::clamp(normVal, 0.0f, 1.0f) * (maxA - minA);
        drawCircle(cx, cy, radius + 3.0f, 0.06f, 0.07f, 0.09f);
        drawArc(cx, cy, radius + 1.5f, minA, maxA, 0.20f, 0.24f, 0.30f, 1.0f, 1.8f);
        if (normVal > 0.01f) {
            drawArc(cx, cy, radius + 1.5f, minA, curA, 0.0f, 0.85f, 1.0f, 1.0f, 2.0f);
        }
        drawCircle(cx, cy, radius, 0.14f, 0.16f, 0.22f);
        drawCircleOutline(cx, cy, radius, 0.28f, 0.32f, 0.42f, 1.0f, 1.0f);
        float indX = cx + std::sin(curA) * (radius - 2.0f);
        float indY = cy - std::cos(curA) * (radius - 2.0f);
        drawLine(cx, cy, indX, indY, Color(0.0f, 0.95f, 1.0f), 1.8f);
        if (label && label[0] != '\0') {
            drawVectorString(label, cx - 12.0f, cy + radius + 3.0f, 0.50f, Color(0.65f, 0.70f, 0.80f));
        }
    };

    float curY = y - scrollY;

    // SECTION 1: Track Header Card
    if (curY + card1H >= y && curY <= y + height) {
        drawRoundedRectGradient(x, curY, contentW, card1H, 6.0f, Color(0.13f, 0.15f, 0.20f), Color(0.08f, 0.09f, 0.12f));
        drawRoundedRectOutline(x, curY, contentW, card1H, 6.0f, Color(0.24f, 0.28f, 0.38f), 1.2f);
        drawRoundedRect(x + 10.0f, curY + 10.0f, 5.0f, card1H - 20.0f, 2.5f, Color(trk.r, trk.g, trk.b));

        // Clean Track Name (no numeric prefix)
        drawVectorString(trk.name, x + 24.0f, curY + 12.0f, isCompact ? 0.90f : 1.05f, Color(0.95f, 0.95f, 1.0f));
        drawVectorString("TRACK CHANNEL " + std::to_string(tIdx + 1) + " • EATSCRIPT DSP",
                         x + 24.0f, curY + (isCompact ? 32.0f : 36.0f), isCompact ? 0.56f : 0.62f, Color(0.55f, 0.60f, 0.70f));

        if (!isCompact) {
            // Fullscreen Color Swatches in Header
            float swStartX = x + 310.0f;
            float swY = curY + 18.0f;
            for (uint32_t s = 0; s < 8; ++s) {
                float sx = swStartX + s * 26.0f;
                drawCircle(sx + 10.0f, swY + 12.0f, 8.0f, swatches[s][0], swatches[s][1], swatches[s][2]);
                if (std::abs(trk.r - swatches[s][0]) < 0.05f && std::abs(trk.g - swatches[s][1]) < 0.05f) {
                    drawCircleOutline(sx + 10.0f, swY + 12.0f, 10.5f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f);
                }
            }

            // [ CODE ] button
            float btnCodeX = x + contentW - 245.0f;
            drawRoundedRectGradient(btnCodeX, curY + 15.0f, 62.0f, 30.0f, 4.0f, Color(0.14f, 0.18f, 0.26f), Color(0.08f, 0.10f, 0.16f));
            drawRoundedRectOutline(btnCodeX, curY + 15.0f, 62.0f, 30.0f, 4.0f, Color(0.0f, 0.85f, 1.0f), 1.2f);
            drawVectorString("[ CODE ]", btnCodeX + 8.0f, curY + 22.0f, 0.70f, Color(0.0f, 0.95f, 1.0f));

            // MUTE button
            float btnMuteX = x + contentW - 175.0f;
            if (trk.mute) {
                drawRoundedRect(btnMuteX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.85f, 0.25f, 0.15f));
                drawVectorString("MUTE", btnMuteX + 10.0f, curY + 22.0f, 0.72f, Color(1.0f, 1.0f, 1.0f));
            } else {
                drawRoundedRect(btnMuteX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.14f, 0.16f, 0.20f));
                drawRoundedRectOutline(btnMuteX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.28f, 0.32f, 0.40f), 1.0f);
                drawVectorString("MUTE", btnMuteX + 10.0f, curY + 22.0f, 0.72f, Color(0.60f, 0.65f, 0.72f));
            }

            // SOLO button
            float btnSoloX = x + contentW - 117.0f;
            if (trk.solo) {
                drawRoundedRect(btnSoloX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.95f, 0.75f, 0.10f));
                drawVectorString("SOLO", btnSoloX + 10.0f, curY + 22.0f, 0.72f, Color(0.05f, 0.08f, 0.12f));
            } else {
                drawRoundedRect(btnSoloX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.14f, 0.16f, 0.20f));
                drawRoundedRectOutline(btnSoloX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.28f, 0.32f, 0.40f), 1.0f);
                drawVectorString("SOLO", btnSoloX + 10.0f, curY + 22.0f, 0.72f, Color(0.60f, 0.65f, 0.72f));
            }

            // FREEZE button
            float btnFzX = x + contentW - 59.0f;
            if (trk.freeze) {
                drawRoundedRect(btnFzX, curY + 15.0f, 54.0f, 30.0f, 4.0f, Color(0.0f, 0.85f, 1.0f));
                drawVectorString("FREEZE", btnFzX + 6.0f, curY + 22.0f, 0.65f, Color(0.05f, 0.08f, 0.12f));
            } else {
                drawRoundedRect(btnFzX, curY + 15.0f, 54.0f, 30.0f, 4.0f, Color(0.14f, 0.16f, 0.20f));
                drawRoundedRectOutline(btnFzX, curY + 15.0f, 54.0f, 30.0f, 4.0f, Color(0.28f, 0.32f, 0.40f), 1.0f);
                drawVectorString("FREEZE", btnFzX + 6.0f, curY + 22.0f, 0.65f, Color(0.60f, 0.65f, 0.72f));
            }
        } else {
            // Compact MUTE and SOLO
            float btnMuteX = x + contentW - 110.0f;
            drawRoundedRect(btnMuteX, curY + 12.0f, 50.0f, 30.0f, 4.0f, trk.mute ? Color(0.85f, 0.25f, 0.15f) : Color(0.14f, 0.16f, 0.20f));
            drawVectorString("MUTE", btnMuteX + 8.0f, curY + 20.0f, 0.68f, trk.mute ? Color(1.0f, 1.0f, 1.0f) : Color(0.60f, 0.65f, 0.72f));

            float btnSoloX = x + contentW - 54.0f;
            drawRoundedRect(btnSoloX, curY + 12.0f, 50.0f, 30.0f, 4.0f, trk.solo ? Color(0.95f, 0.75f, 0.10f) : Color(0.14f, 0.16f, 0.20f));
            drawVectorString("SOLO", btnSoloX + 8.0f, curY + 20.0f, 0.68f, trk.solo ? Color(0.05f, 0.08f, 0.12f) : Color(0.60f, 0.65f, 0.72f));
        }
    }
    curY += card1H + gap1;

    // SECTION 1B: Track Accent Color (Compact Sidebar Mode)
    if (isCompact) {
        if (curY + cardColorH >= y && curY <= y + height) {
            drawRoundedRectGradient(x, curY, contentW, cardColorH, 6.0f, Color(0.12f, 0.14f, 0.18f), Color(0.08f, 0.09f, 0.12f));
            drawRoundedRectOutline(x, curY, contentW, cardColorH, 6.0f, Color(0.20f, 0.24f, 0.32f), 1.0f);
            drawVectorString("TRACK ACCENT COLOR", x + 10.0f, curY + 6.0f, 0.65f, Color(0.60f, 0.65f, 0.75f));

            float swatchStep = (contentW - 4.0f) / 8.0f;
            for (uint32_t s = 0; s < 8; ++s) {
                float sx = x + 12.0f + s * swatchStep;
                float sy = curY + 20.0f;
                float sw = swatchStep - 4.0f;
                float sh = 24.0f;
                drawRoundedRect(sx, sy, sw, sh, 4.0f, Color(swatches[s][0], swatches[s][1], swatches[s][2]));
                if (std::abs(trk.r - swatches[s][0]) < 0.05f && std::abs(trk.g - swatches[s][1]) < 0.05f) {
                    drawRoundedRectOutline(sx - 1.0f, sy - 1.0f, sw + 2.0f, sh + 2.0f, 4.0f, Color(1.0f, 1.0f, 1.0f), 2.0f);
                }
            }
        }
        curY += cardColorH + gapColor;
    }

    // SECTION 2: Channel Mixer Quick Controls
    if (curY + card2H >= y && curY <= y + height) {
        drawRoundedRectGradient(x, curY, contentW, card2H, 6.0f, Color(0.11f, 0.12f, 0.16f), Color(0.07f, 0.08f, 0.11f));
        drawRoundedRectOutline(x, curY, contentW, card2H, 6.0f, Color(0.22f, 0.25f, 0.34f), 1.0f);
        drawVectorString("CHANNEL MIXER QUICK CONTROLS", x + 16.0f, curY + 10.0f, 0.62f, Color(0.50f, 0.55f, 0.65f));

        float normVol = std::clamp(trk.volume / 1.5f, 0.0f, 1.0f);
        float normPan = std::clamp((trk.pan + 1.0f) * 0.5f, 0.0f, 1.0f);
        int volPct = static_cast<int>(std::round(trk.volume * 100.0f));
        std::string panStr = (std::abs(trk.pan) < 0.04f) ? "C" : ((trk.pan < 0.0f) ? "L" + std::to_string(static_cast<int>(std::round(-trk.pan * 100.0f))) : "R" + std::to_string(static_cast<int>(std::round(trk.pan * 100.0f))));

        if (!isCompact) {
            // Fullscreen Volume & Pan Side-by-Side
            drawVectorString("VOL", x + 24.0f, curY + 38.0f, 0.72f, Color(0.70f, 0.75f, 0.85f));
            drawRoundedRect(x + 80.0f, curY + 36.0f, 220.0f, 18.0f, 4.0f, Color(0.06f, 0.07f, 0.09f));
            drawRoundedRectOutline(x + 80.0f, curY + 36.0f, 220.0f, 18.0f, 4.0f, Color(0.20f, 0.22f, 0.28f), 1.0f);
            if (normVol > 0.01f) {
                drawRoundedRect(x + 82.0f, curY + 38.0f, (220.0f - 4.0f) * normVol, 14.0f, 3.0f, Color(0.0f, 0.85f, 1.0f));
            }
            float volThumbX = x + 80.0f + normVol * 220.0f;
            drawRoundedRectGradient(volThumbX - 6.0f, curY + 33.0f, 12.0f, 24.0f, 3.0f, Color(0.35f, 0.40f, 0.50f), Color(0.18f, 0.20f, 0.28f));
            drawRoundedRectOutline(volThumbX - 6.0f, curY + 33.0f, 12.0f, 24.0f, 3.0f, Color(0.60f, 0.65f, 0.80f), 1.0f);
            drawVectorString(std::to_string(volPct) + "%", x + 315.0f, curY + 38.0f, 0.72f, Color(0.0f, 0.95f, 1.0f));

            drawVectorString("PAN", x + 375.0f, curY + 38.0f, 0.72f, Color(0.70f, 0.75f, 0.85f));
            drawRoundedRect(x + 420.0f, curY + 36.0f, 180.0f, 18.0f, 4.0f, Color(0.06f, 0.07f, 0.09f));
            drawRoundedRectOutline(x + 420.0f, curY + 36.0f, 180.0f, 18.0f, 4.0f, Color(0.20f, 0.22f, 0.28f), 1.0f);
            drawLine(x + 510.0f, curY + 37.0f, x + 510.0f, curY + 53.0f, Color(0.35f, 0.40f, 0.50f), 1.5f);
            float panThumbX = x + 420.0f + normPan * 180.0f;
            drawRoundedRectGradient(panThumbX - 6.0f, curY + 33.0f, 12.0f, 24.0f, 3.0f, Color(0.35f, 0.40f, 0.50f), Color(0.18f, 0.20f, 0.28f));
            drawRoundedRectOutline(panThumbX - 6.0f, curY + 33.0f, 12.0f, 24.0f, 3.0f, Color(0.60f, 0.65f, 0.80f), 1.0f);
            drawVectorString(panStr, x + 615.0f, curY + 38.0f, 0.72f, Color(0.0f, 0.95f, 1.0f));
        } else {
            // Compact Vertical Stack
            float sliderW = contentW - 120.0f;
            drawVectorString("VOL", x + 16.0f, curY + 30.0f, 0.65f, Color(0.70f, 0.75f, 0.85f));
            drawRoundedRect(x + 55.0f, curY + 28.0f, sliderW, 16.0f, 3.0f, Color(0.06f, 0.07f, 0.09f));
            if (normVol > 0.01f) drawRoundedRect(x + 56.0f, curY + 29.0f, (sliderW - 2.0f) * normVol, 14.0f, 2.5f, Color(0.0f, 0.85f, 1.0f));
            drawVectorString(std::to_string(volPct) + "%", x + 65.0f + sliderW, curY + 30.0f, 0.65f, Color(0.0f, 0.95f, 1.0f));

            drawVectorString("PAN", x + 16.0f, curY + 58.0f, 0.65f, Color(0.70f, 0.75f, 0.85f));
            drawRoundedRect(x + 55.0f, curY + 56.0f, sliderW, 16.0f, 3.0f, Color(0.06f, 0.07f, 0.09f));
            float panX = x + 55.0f + normPan * sliderW;
            drawRoundedRect(panX - 4.0f, curY + 54.0f, 8.0f, 20.0f, 2.0f, Color(0.0f, 0.95f, 1.0f));
            drawVectorString(panStr, x + 65.0f + sliderW, curY + 58.0f, 0.65f, Color(0.0f, 0.95f, 1.0f));
        }
    }
    curY += card2H + gap2;

    // SECTION 3: Dynamic Instrument Faceplate
    const auto* preset = getActivePreset();
    if (preset && (curY + card3H >= y && curY <= y + height)) {
        const float pX = x;
        const float pY = curY;
        const float pW = contentW;
        const float pH = card3H;

        if (isCompact) {
            float scale = contentW / designW;

            const std::string& bg = preset->compactGuiRoot.background.empty() ? preset->guiRoot.background : preset->compactGuiRoot.background;
            if (bg == "minimal_white") {
                drawRectGradient(pX, pY, pW, pH, 0.92f, 0.94f, 0.96f, 0.82f, 0.84f, 0.88f);
                drawRectOutline(pX, pY, pW, pH, 0.45f, 0.48f, 0.55f, 1.0f, 1.5f);
            } else if (bg == "c64_breadbin") {
                drawRectGradient(pX, pY, pW, pH, 0.24f, 0.21f, 0.17f, 0.16f, 0.14f, 0.11f);
                drawRectOutline(pX, pY, pW, pH, 0.42f, 0.37f, 0.71f, 1.0f, 1.5f);
            } else if (bg == "dx7_faceplate") {
                drawRectGradient(pX, pY, pW, pH, 0.12f, 0.11f, 0.09f, 0.07f, 0.06f, 0.06f);
                drawRectOutline(pX, pY, pW, pH, 0.0f, 0.66f, 0.53f, 1.0f, 1.5f);
            } else if (bg == "snes_faceplate") {
                drawRectGradient(pX, pY, pW, pH, 0.76f, 0.76f, 0.80f, 0.62f, 0.62f, 0.66f);
                drawRectOutline(pX, pY, pW, pH, 0.48f, 0.42f, 0.72f, 1.0f, 1.5f);
            } else if (bg == "ym2612_faceplate") {
                drawRectGradient(pX, pY, pW, pH, 0.08f, 0.08f, 0.09f, 0.04f, 0.04f, 0.05f);
                drawRectOutline(pX, pY, pW, pH, 0.90f, 0.66f, 0.14f, 1.0f, 1.5f);
            } else if (bg == "convolver_faceplate") {
                drawRectGradient(pX, pY, pW, pH, 0.13f, 0.15f, 0.18f, 0.07f, 0.08f, 0.10f);
                drawRectOutline(pX, pY, pW, pH, 0.0f, 0.90f, 1.0f, 1.0f, 1.5f);
            } else if (bg == "walnut" || bg == "wood" || bg == "clavinet") {
                drawRectGradient(pX, pY, pW, pH, 0.18f, 0.11f, 0.07f, 0.10f, 0.06f, 0.04f);
                drawRectOutline(pX, pY, pW, pH, 0.65f, 0.42f, 0.18f, 1.0f, 1.5f);
            } else if (bg == "silver") {
                drawRectGradient(pX, pY, pW, pH, 0.26f, 0.28f, 0.35f, 0.16f, 0.18f, 0.23f);
                drawRectOutline(pX, pY, pW, pH, 0.45f, 0.50f, 0.62f, 1.0f, 1.5f);
            } else {
                drawRectGradient(pX, pY, pW, pH, 0.11f, 0.12f, 0.16f, 0.06f, 0.07f, 0.10f);
                drawRectOutline(pX, pY, pW, pH, 0.22f, 0.26f, 0.35f, 1.0f, 1.5f);
            }

            // Top Compact Banner Bar (Height = 28px)
            drawRect(pX, pY, pW, 28.0f, (bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.12f : 0.07f,
                                        (bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.07f : 0.08f,
                                        (bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.04f : 0.11f);
            drawLine(pX, pY + 28.0f, pX + pW, pY + 28.0f,
                     (bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.50f : 0.25f,
                     (bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.32f : 0.30f,
                     (bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.14f : 0.38f,
                     1.0f, 1.2f);

            // Preset Navigation: < and >
            drawRectGradient(pX + 6.0f, pY + 4.0f, 32.0f, 20.0f, 0.18f, 0.20f, 0.26f, 0.10f, 0.11f, 0.15f);
            drawRectOutline(pX + 6.0f, pY + 4.0f, 32.0f, 20.0f, 0.0f, 0.85f, 1.0f, 1.0f, 1.0f);
            drawVectorString("<", pX + 18.0f, pY + 9.0f, 0.70f, 0.0f, 1.0f, 1.0f);

            drawRectGradient(pX + 42.0f, pY + 4.0f, 32.0f, 20.0f, 0.18f, 0.20f, 0.26f, 0.10f, 0.11f, 0.15f);
            drawRectOutline(pX + 42.0f, pY + 4.0f, 32.0f, 20.0f, 0.0f, 0.85f, 1.0f, 1.0f, 1.0f);
            drawVectorString(">", pX + 54.0f, pY + 9.0f, 0.70f, 0.0f, 1.0f, 1.0f);

            // Title
            const std::string& pTitle = preset->compactGuiRoot.title.empty() ? preset->guiRoot.title : preset->compactGuiRoot.title;
            drawVectorString(pTitle, pX + 80.0f, pY + 9.0f, 0.72f, 0.0f, 0.95f, 1.0f);

            // [ FULL ] button on right
            float fullBtnW = 44.0f;
            float fullBtnX = pX + pW - fullBtnW - 6.0f;
            drawRectGradient(fullBtnX, pY + 4.0f, fullBtnW, 20.0f, 0.18f, 0.20f, 0.26f, 0.10f, 0.11f, 0.15f);
            drawRectOutline(fullBtnX, pY + 4.0f, fullBtnW, 20.0f, 0.0f, 0.85f, 1.0f, 1.0f, 1.0f);
            drawVectorString("FULL", fullBtnX + 8.0f, pY + 9.0f, 0.65f, 0.0f, 0.95f, 1.0f);

            // Recursive Compact Node rendering using designWidth 520.0f
            std::function<void(const project::GuiLayoutNode&)> renderCompactNode = [&](const project::GuiLayoutNode& node) {
                float adjNodeX = pX + node.boundsX * scale;
                float adjNodeY = pY + 28.0f + (node.boundsY - 28.0f) * scale;
                float adjNodeW = node.boundsW * scale;
                float adjNodeH = node.boundsH * scale;

                if (node.type == project::GuiNodeType::Row || node.type == project::GuiNodeType::Column || node.type == project::GuiNodeType::Panel) {
                    for (const auto& c : node.children) renderCompactNode(c);
                } else if (node.type == project::GuiNodeType::Group) {
                    if (bg == "walnut" || bg == "wood" || bg == "clavinet") {
                        drawRoundedRectGradient(adjNodeX, adjNodeY, adjNodeW, adjNodeH, 4.0f,
                                                Color(0.17f, 0.11f, 0.07f), Color(0.12f, 0.08f, 0.05f));
                        drawRoundedRectOutline(adjNodeX, adjNodeY, adjNodeW, adjNodeH, 4.0f,
                                               Color(0.50f, 0.32f, 0.14f), 1.2f);
                        if (!node.label.empty()) {
                            float lScale = 0.50f * scale + 0.15f;
                            float lW = node.label.size() * 5.6f * lScale;
                            float tX = adjNodeX + (adjNodeW - lW) * 0.5f;
                            drawVectorString(node.label, tX, adjNodeY + 3.0f, lScale, Color(0.96f, 0.58f, 0.10f));
                        }
                    } else {
                        drawRoundedRect(adjNodeX, adjNodeY, adjNodeW, adjNodeH, 3.0f, Color(0.08f, 0.09f, 0.12f));
                        drawRoundedRectOutline(adjNodeX, adjNodeY, adjNodeW, adjNodeH, 3.0f, Color(0.20f, 0.24f, 0.30f), 1.0f);
                        if (!node.label.empty()) {
                            drawVectorString(node.label, adjNodeX + 4.0f, adjNodeY + 3.0f, 0.50f * scale + 0.15f, Color(0.60f, 0.65f, 0.75f));
                        }
                    }
                    for (const auto& c : node.children) renderCompactNode(c);
                } else if (node.type == project::GuiNodeType::Knob) {
                    float cx = adjNodeX + adjNodeW * 0.5f;
                    float cy = adjNodeY + adjNodeH * 0.38f;
                    float kRad = node.size * 0.42f * scale;
                    float drawCx = (node.labelAlignment == "inline_right") ? (cx - kRad * 0.65f) : cx;
                    const auto* p = preset->findParam(node.paramName);
                    float normVal = p ? p->getNormalized() : 0.5f;
                    bool isActive = (dragMode_ == DragMode::HardwareKnob && activeHardwareParam_ == node.paramName);
                    bool isHovered = (std::hypot(mouseX_ - drawCx, mouseY_ - cy) <= kRad * 1.5f);
                    const auto& light = (activeFrameTheme_.backgroundDark.a > 0.0f) ? activeFrameTheme_.globalLight : getTheme().globalLight;

                    if (node.hardwareStyle == "vintage" || bg == "walnut" || bg == "wood" || bg == "clavinet") {
                        std::string readout = p ? p->getFormatted() : "";
                        renderVintageClavinetKnob(cx, cy, kRad, normVal, isActive, isHovered, node.label, readout, light);
                    } else if (node.hardwareStyle == "tb303_potentiometer" || node.hardwareStyle == "tb303_selector" ||
                        preset->metadata.id == "eats_303" || preset->metadata.engineId == "tb303") {
                        std::string readout = p ? p->getFormatted() : "";
                        renderTb303Knob(cx, cy, kRad, normVal, isActive, isHovered, node.label, readout, light, node.hardwareStyle == "tb303_selector");
                    } else {
                        GuiKnobStyle kStyle = GuiKnobStyle::Standard;
                        const std::string& s = node.hardwareStyle;
                        if (s == "davies" || s == "davies_fluted" || s == "chromeFluted" || s == "chrome") kStyle = GuiKnobStyle::DaviesFluted;
                        else if (s == "rogan" || s == "rogan_moog") kStyle = GuiKnobStyle::RoganMoog;
                        else if (s == "chicken" || s == "chicken_head") kStyle = GuiKnobStyle::ChickenHead;
                        else if (s == "pastel" || s == "pastel_dimple") kStyle = GuiKnobStyle::PastelDimple;
                        else if (s == "encoder" || s == "modern_encoder") kStyle = GuiKnobStyle::ModernEncoder;
                        else if (s == "cream_fluted") kStyle = GuiKnobStyle::CreamFluted;
                        else if (s == "bakelite" || s == "bakelite_skirt") kStyle = GuiKnobStyle::BakeliteSkirt;
                        else if (s == "anodized") kStyle = GuiKnobStyle::AnodizedKnurled;
                        else if (s == "twoToneStepped" || s == "two_tone_stepped") kStyle = GuiKnobStyle::TwoToneStepped;
                        else if (s == "studio_clean" || s == "clean") kStyle = GuiKnobStyle::StudioClean;
                        else if (s == "studio_ruby" || s == "ruby") kStyle = GuiKnobStyle::StudioRuby;
                        else if (s == "studio_magenta" || s == "magenta") kStyle = GuiKnobStyle::StudioMagenta;
                        else if (s == "studio_cyan" || s == "cyan" || s == "astral") kStyle = GuiKnobStyle::StudioCyan;
                        else if (s == "studio_dark" || s == "dark_track") kStyle = GuiKnobStyle::StudioDark;
                        else if (s == "studio_bipolar" || s == "bipolar") kStyle = GuiKnobStyle::StudioBipolar;
                        else if (s == "neumorphic_pink" || s == "lens_pink") kStyle = GuiKnobStyle::NeumorphicLensPink;
                        else if (s == "neumorphic_lens" || s == "lens" || s == "volcano" || s == "neumorphic") kStyle = GuiKnobStyle::NeumorphicLens;

                        if (g_activeBatchRenderer) {
                            GuiWidgetDef w;
                            w.knobStyle = kStyle;
                            w.label = node.label;
                            w.sublabel = node.sublabel;
                            w.centerGlyph = node.centerGlyph;
                            w.accentColor = (kStyle == GuiKnobStyle::StudioRuby) ? Color(0.84f, 0.14f, 0.20f) :
                                           ((kStyle == GuiKnobStyle::StudioMagenta) ? Color(0.72f, 0.12f, 0.60f) :
                                           ((kStyle == GuiKnobStyle::NeumorphicLensPink) ? Color(1.0f, 0.35f, 0.65f) :
                                             Color(0.0f, 0.95f, 1.0f)));
                            drawKnobWidget(*g_activeBatchRenderer, drawCx, cy, kRad, normVal, w, light, isActive, isHovered, (bg == "minimal_white"));
                            if (node.labelAlignment == "inline_right") {
                                float textX = drawCx + kRad + 6.0f;
                                std::string readout = p ? p->getFormatted() : "";
                                drawVectorString(readout, textX, cy - 6.0f, 0.52f, (bg == "minimal_white") ? Color(0.12f, 0.14f, 0.18f) : Color(0.95f, 0.96f, 0.98f));
                                drawVectorString(node.label, textX, cy + 4.0f, 0.42f, (bg == "minimal_white") ? Color(0.35f, 0.38f, 0.45f) : Color(0.70f, 0.75f, 0.85f));
                                if (!node.sublabel.empty()) {
                                    drawVectorString(node.sublabel, textX, cy + 12.0f, 0.35f, (bg == "minimal_white") ? Color(0.50f, 0.54f, 0.62f) : Color(0.55f, 0.60f, 0.70f));
                                }
                            } else {
                                drawVectorString(node.label, cx - node.label.size() * 2.5f, cy + kRad + 5.0f, 0.48f, (bg == "minimal_white") ? Color(0.20f, 0.22f, 0.26f) : Color(0.70f, 0.75f, 0.85f));
                            }
                        }
                    }
                } else if (node.type == project::GuiNodeType::Switch) {
                    float cx = adjNodeX + adjNodeW * 0.5f;
                    float cy = adjNodeY + adjNodeH * 0.50f;
                    const auto* p = preset->findParam(node.paramName);
                    float normVal = p ? p->getNormalized() : 0.0f;
                    bool isActive = (dragMode_ == DragMode::HardwareKnob && activeHardwareParam_ == node.paramName);
                    bool isHovered = (std::abs(mouseX_ - cx) <= 18.0f * scale && std::abs(mouseY_ - cy) <= 24.0f * scale);
                    const auto& light = (activeFrameTheme_.backgroundDark.a > 0.0f) ? activeFrameTheme_.globalLight : getTheme().globalLight;
                    renderClavinetRockerSwitch(cx, cy, normVal, isActive, isHovered, node.label, light, scale);
                } else if (node.type == project::GuiNodeType::Slider) {
                    const auto* p = preset->findParam(node.paramName);
                    float normVal = p ? p->getNormalized() : 0.5f;
                    bool isActive = (dragMode_ == DragMode::HardwareKnob && activeHardwareParam_ == node.paramName);
                    bool isHovered = (mouseX_ >= adjNodeX && mouseX_ <= adjNodeX + adjNodeW &&
                                      mouseY_ >= adjNodeY && mouseY_ <= adjNodeY + adjNodeH);
                    std::string readout = p ? p->getFormatted() : "";
                    const auto& light = (activeFrameTheme_.backgroundDark.a > 0.0f) ? activeFrameTheme_.globalLight : getTheme().globalLight;
                    renderClavinetSlider(adjNodeX, adjNodeY, adjNodeW, adjNodeH, normVal, isActive, isHovered, node.label, readout, light, scale);
                } else if (node.type == project::GuiNodeType::Nixie) {
                    float cx = adjNodeX + adjNodeW * 0.5f;
                    float cy = adjNodeY + adjNodeH * 0.4f;
                    drawRoundedRect(cx - 20.0f, cy - 8.0f, 40.0f, 16.0f, 2.0f, Color(0.05f, 0.02f, 0.0f));
                    drawRoundedRectOutline(cx - 20.0f, cy - 8.0f, 40.0f, 16.0f, 2.0f, Color(0.85f, 0.40f, 0.0f), 1.0f);
                    drawVectorString(node.label, cx - 12.0f, cy - 3.0f, 0.60f, Color(1.0f, 0.60f, 0.10f));
                } else if (node.type == project::GuiNodeType::Divider) {
                    drawLine(adjNodeX + adjNodeW * 0.5f, adjNodeY + 4.0f,
                             adjNodeX + adjNodeW * 0.5f, adjNodeY + adjNodeH - 4.0f,
                             Color(0.4f, 0.45f, 0.55f, 0.5f), 1.0f);
                }
            };

            for (const auto& child : preset->compactGuiRoot.children) {
                renderCompactNode(child);
            }
        } else {
            float scale = 0.70f;

        const auto& light = (activeFrameTheme_.backgroundDark.a > 0.0f) ? activeFrameTheme_.globalLight : getTheme().globalLight;
        drawRectDropShadow(pX, pY, pW, pH, 6.0f, 14.0f, light, 0.40f);

        const std::string& bg = preset->guiRoot.background;
        if (bg == "minimal_white") {
            drawRectGradient(pX, pY, pW, pH, 0.92f, 0.94f, 0.96f, 0.82f, 0.84f, 0.88f);
            drawRectOutline(pX, pY, pW, pH, 0.45f, 0.48f, 0.55f, 1.0f, 2.0f);
        } else if (bg == "c64_breadbin") {
            drawRectGradient(pX, pY, pW, pH, 0.24f, 0.21f, 0.17f, 0.16f, 0.14f, 0.11f);
            drawRectOutline(pX, pY, pW, pH, 0.42f, 0.37f, 0.71f, 1.0f, 2.5f);
        } else if (bg == "dx7_faceplate") {
            drawRectGradient(pX, pY, pW, pH, 0.12f, 0.11f, 0.09f, 0.07f, 0.06f, 0.06f);
            drawRectOutline(pX, pY, pW, pH, 0.0f, 0.66f, 0.53f, 1.0f, 2.5f);
        } else if (bg == "snes_faceplate") {
            drawRectGradient(pX, pY, pW, pH, 0.76f, 0.76f, 0.80f, 0.62f, 0.62f, 0.66f);
            drawRectOutline(pX, pY, pW, pH, 0.48f, 0.42f, 0.72f, 1.0f, 2.5f);
        } else if (bg == "ym2612_faceplate") {
            drawRectGradient(pX, pY, pW, pH, 0.08f, 0.08f, 0.09f, 0.04f, 0.04f, 0.05f);
            drawRectOutline(pX, pY, pW, pH, 0.90f, 0.66f, 0.14f, 1.0f, 2.5f);
        } else if (bg == "convolver_faceplate") {
            drawRectGradient(pX, pY, pW, pH, 0.13f, 0.15f, 0.18f, 0.07f, 0.08f, 0.10f);
            drawRectOutline(pX, pY, pW, pH, 0.0f, 0.90f, 1.0f, 1.0f, 2.5f);
        } else if (bg == "walnut" || bg == "wood" || bg == "clavinet") {
            drawRectGradient(pX, pY, pW, pH, 0.18f, 0.11f, 0.07f, 0.10f, 0.06f, 0.04f);
            drawRectOutline(pX, pY, pW, pH, 0.65f, 0.42f, 0.18f, 1.0f, 2.0f);
        } else if (bg == "silver") {
            drawRectGradient(pX, pY, pW, pH, 0.26f, 0.28f, 0.35f, 0.16f, 0.18f, 0.23f);
            drawRectOutline(pX, pY, pW, pH, 0.45f, 0.50f, 0.62f, 1.0f, 2.0f);
        } else {
            drawRectGradient(pX, pY, pW, pH, 0.11f, 0.12f, 0.16f, 0.06f, 0.07f, 0.10f);
            drawRectOutline(pX, pY, pW, pH, 0.22f, 0.26f, 0.35f, 1.0f, 2.0f);
        }

        // Corner hex mounting bolts
        drawCircle(pX + 12.0f, pY + 12.0f, 3.5f, 0.4f, 0.44f, 0.52f);
        drawCircle(pX + pW - 12.0f, pY + 12.0f, 3.5f, 0.4f, 0.44f, 0.52f);
        drawCircle(pX + 12.0f, pY + pH - 12.0f, 3.5f, 0.4f, 0.44f, 0.52f);
        drawCircle(pX + pW - 12.0f, pY + pH - 12.0f, 3.5f, 0.4f, 0.44f, 0.52f);

        // Top Banner Plate
        drawRect(pX, pY, pW, 52.0f, (bg == "minimal_white") ? 0.85f : ((bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.12f : 0.07f),
                                    (bg == "minimal_white") ? 0.87f : ((bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.07f : 0.08f),
                                    (bg == "minimal_white") ? 0.90f : ((bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.04f : 0.11f));
        drawLine(pX, pY + 52.0f, pX + pW, pY + 52.0f,
                 (bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.50f : 0.25f,
                 (bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.32f : 0.30f,
                 (bg == "walnut" || bg == "wood" || bg == "clavinet") ? 0.14f : 0.38f,
                 1.0f, 1.5f);

        // Preset Navigation: < PREV and NEXT >
        drawRectGradient(pX + 16.0f, pY + 12.0f, 75.0f, 28.0f, 0.18f, 0.20f, 0.26f, 0.10f, 0.11f, 0.15f);
        drawRectOutline(pX + 16.0f, pY + 12.0f, 75.0f, 28.0f, 0.0f, 0.85f, 1.0f, 1.0f, 1.2f);
        drawVectorString("< PREV", pX + 24.0f, pY + 20.0f, 0.85f, 0.0f, 1.0f, 1.0f);

        drawRectGradient(pX + 98.0f, pY + 12.0f, 75.0f, 28.0f, 0.18f, 0.20f, 0.26f, 0.10f, 0.11f, 0.15f);
        drawRectOutline(pX + 98.0f, pY + 12.0f, 75.0f, 28.0f, 0.0f, 0.85f, 1.0f, 1.0f, 1.2f);
        drawVectorString("NEXT >", pX + 106.0f, pY + 20.0f, 0.85f, 0.0f, 1.0f, 1.0f);

        std::string pCounter = std::to_string(activePresetIndex_ + 1) + "/" + std::to_string(presets_.size());
        drawVectorString(pCounter, pX + 184.0f, pY + 20.0f, 0.85f, (bg == "minimal_white") ? 0.2f : 0.6f,
                                                                     (bg == "minimal_white") ? 0.2f : 0.65f,
                                                                     (bg == "minimal_white") ? 0.2f : 0.75f);

        drawVectorString(preset->guiRoot.title, pX + 245.0f, pY + 12.0f, 1.15f, 0.0f, 0.95f, 1.0f);
        drawVectorString(preset->guiRoot.subtitle, pX + 245.0f, pY + 32.0f, 0.72f, 0.55f, 0.60f, 0.70f);

        // Recursive node rendering with layout scaling and offset
        std::function<void(const project::GuiLayoutNode&)> renderNode = [&](const project::GuiLayoutNode& node) {
            float adjNodeY = pY + 54.0f + (node.boundsY - 152.0f) * scale;
            float adjNodeX = pX + (node.boundsX - 40.0f);
            if (node.type == project::GuiNodeType::Row || node.type == project::GuiNodeType::Column || node.type == project::GuiNodeType::Panel) {
                for (const auto& c : node.children) renderNode(c);
            } else if (node.type == project::GuiNodeType::Group) {
                if (bg == "walnut" || bg == "wood" || bg == "clavinet") {
                    drawRoundedRectGradient(adjNodeX, adjNodeY, node.boundsW, node.boundsH * scale, 6.0f,
                                            Color(0.17f, 0.11f, 0.07f), Color(0.12f, 0.08f, 0.05f));
                    drawRoundedRectOutline(adjNodeX, adjNodeY, node.boundsW, node.boundsH * scale, 6.0f,
                                           Color(0.50f, 0.32f, 0.14f), 1.5f);
                    if (!node.label.empty()) {
                        float labelScale = 0.68f;
                        float labelW = node.label.size() * 5.8f * labelScale;
                        float titleX = adjNodeX + (node.boundsW - labelW) * 0.5f;
                        drawVectorString(node.label, titleX, adjNodeY + 8.0f, labelScale, Color(0.96f, 0.58f, 0.10f));
                    }
                } else {
                    drawRoundedRect(adjNodeX, adjNodeY, node.boundsW, node.boundsH * scale, 4.0f, Color(0.08f, 0.09f, 0.12f));
                    drawRoundedRectOutline(adjNodeX, adjNodeY, node.boundsW, node.boundsH * scale, 4.0f, Color(0.20f, 0.24f, 0.30f), 1.0f);
                    if (!node.label.empty()) {
                        drawVectorString(node.label, adjNodeX + 8.0f, adjNodeY + 4.0f, 0.60f, Color(0.60f, 0.65f, 0.75f));
                    }
                }
                for (const auto& c : node.children) renderNode(c);
            } else if (node.type == project::GuiNodeType::Knob) {
                float cx = adjNodeX + node.boundsW * 0.5f;
                float cy = adjNodeY + node.boundsH * 0.35f * (scale < 0.95f ? scale : 1.0f);
                float kRad = node.size * 0.42f;
                float drawCx = (node.labelAlignment == "inline_right") ? (cx - kRad * 0.65f) : cx;
                const auto* p = preset->findParam(node.paramName);
                float normVal = p ? p->getNormalized() : 0.5f;
                bool isActive = (dragMode_ == DragMode::HardwareKnob && activeHardwareParam_ == node.paramName);
                bool isHovered = (std::hypot(mouseX_ - drawCx, mouseY_ - cy) <= kRad * 1.5f);

                if (node.hardwareStyle == "vintage" || bg == "walnut" || bg == "wood" || bg == "clavinet") {
                    std::string readout = p ? p->getFormatted() : "";
                    renderVintageClavinetKnob(cx, cy, kRad, normVal, isActive, isHovered, node.label, readout, light);
                } else if (node.hardwareStyle == "tb303_potentiometer" || node.hardwareStyle == "tb303_selector" ||
                    preset->metadata.id == "eats_303" || preset->metadata.engineId == "tb303") {
                    std::string readout = p ? p->getFormatted() : "";
                    renderTb303Knob(cx, cy, kRad, normVal, isActive, isHovered, node.label, readout, light, node.hardwareStyle == "tb303_selector");
                } else {
                    GuiKnobStyle kStyle = GuiKnobStyle::Standard;
                    const std::string& s = node.hardwareStyle;
                    if (s == "davies" || s == "davies_fluted" || s == "chromeFluted" || s == "chrome") kStyle = GuiKnobStyle::DaviesFluted;
                    else if (s == "rogan" || s == "rogan_moog") kStyle = GuiKnobStyle::RoganMoog;
                    else if (s == "chicken" || s == "chicken_head") kStyle = GuiKnobStyle::ChickenHead;
                    else if (s == "pastel" || s == "pastel_dimple") kStyle = GuiKnobStyle::PastelDimple;
                    else if (s == "encoder" || s == "modern_encoder") kStyle = GuiKnobStyle::ModernEncoder;
                    else if (s == "cream_fluted") kStyle = GuiKnobStyle::CreamFluted;
                    else if (s == "bakelite" || s == "bakelite_skirt") kStyle = GuiKnobStyle::BakeliteSkirt;
                    else if (s == "anodized") kStyle = GuiKnobStyle::AnodizedKnurled;
                    else if (s == "twoToneStepped" || s == "two_tone_stepped") kStyle = GuiKnobStyle::TwoToneStepped;
                    else if (s == "studio_clean" || s == "clean") kStyle = GuiKnobStyle::StudioClean;
                    else if (s == "studio_ruby" || s == "ruby") kStyle = GuiKnobStyle::StudioRuby;
                    else if (s == "studio_magenta" || s == "magenta") kStyle = GuiKnobStyle::StudioMagenta;
                    else if (s == "studio_cyan" || s == "cyan" || s == "astral") kStyle = GuiKnobStyle::StudioCyan;
                    else if (s == "studio_dark" || s == "dark_track") kStyle = GuiKnobStyle::StudioDark;
                    else if (s == "studio_bipolar" || s == "bipolar") kStyle = GuiKnobStyle::StudioBipolar;
                    else if (s == "neumorphic_pink" || s == "lens_pink") kStyle = GuiKnobStyle::NeumorphicLensPink;
                    else if (s == "neumorphic_lens" || s == "lens" || s == "volcano" || s == "neumorphic") kStyle = GuiKnobStyle::NeumorphicLens;

                    if (g_activeBatchRenderer) {
                        GuiWidgetDef w;
                        w.knobStyle = kStyle;
                        w.label = node.label;
                        w.sublabel = node.sublabel;
                        w.centerGlyph = node.centerGlyph;
                        w.accentColor = (kStyle == GuiKnobStyle::StudioRuby) ? Color(0.84f, 0.14f, 0.20f) :
                                       ((kStyle == GuiKnobStyle::StudioMagenta) ? Color(0.72f, 0.12f, 0.60f) :
                                       ((kStyle == GuiKnobStyle::NeumorphicLensPink) ? Color(1.0f, 0.35f, 0.65f) :
                                         Color(0.0f, 0.95f, 1.0f)));
                        drawKnobWidget(*g_activeBatchRenderer, drawCx, cy, kRad, normVal, w, light, isActive, isHovered, (bg == "minimal_white"));
                        if (node.labelAlignment == "inline_right") {
                            float textX = drawCx + kRad + 8.0f;
                            std::string readout = p ? p->getFormatted() : "";
                            drawVectorString(readout, textX, cy - 8.0f, 0.65f, (bg == "minimal_white") ? Color(0.12f, 0.14f, 0.18f) : Color(0.95f, 0.96f, 0.98f));
                            drawVectorString(node.label, textX, cy + 4.0f, 0.50f, (bg == "minimal_white") ? Color(0.35f, 0.38f, 0.45f) : Color(0.70f, 0.75f, 0.85f));
                            if (!node.sublabel.empty()) {
                                drawVectorString(node.sublabel, textX, cy + 14.0f, 0.42f, (bg == "minimal_white") ? Color(0.50f, 0.54f, 0.62f) : Color(0.55f, 0.60f, 0.70f));
                            }
                        } else {
                            drawVectorString(node.label, cx - node.label.size() * 3.0f, cy + kRad + 8.0f, 0.55f, (bg == "minimal_white") ? Color(0.20f, 0.22f, 0.26f) : Color(0.70f, 0.75f, 0.85f));
                        }
                    }
                }
            } else if (node.type == project::GuiNodeType::Switch) {
                float cx = adjNodeX + node.boundsW * 0.5f;
                float cy = adjNodeY + node.boundsH * 0.52f * scale;
                const auto* p = preset->findParam(node.paramName);
                float normVal = p ? p->getNormalized() : 0.0f;
                bool isActive = (dragMode_ == DragMode::HardwareKnob && activeHardwareParam_ == node.paramName);
                bool isHovered = (std::abs(mouseX_ - cx) <= 20.0f && std::abs(mouseY_ - cy) <= 28.0f);
                renderClavinetRockerSwitch(cx, cy, normVal, isActive, isHovered, node.label, light, 1.0f);
            } else if (node.type == project::GuiNodeType::Slider) {
                const auto* p = preset->findParam(node.paramName);
                float normVal = p ? p->getNormalized() : 0.5f;
                bool isActive = (dragMode_ == DragMode::HardwareKnob && activeHardwareParam_ == node.paramName);
                bool isHovered = (mouseX_ >= adjNodeX && mouseX_ <= adjNodeX + node.boundsW &&
                                  mouseY_ >= adjNodeY && mouseY_ <= adjNodeY + node.boundsH * scale);
                std::string readout = p ? p->getFormatted() : "";
                renderClavinetSlider(adjNodeX, adjNodeY, node.boundsW, node.boundsH * scale, normVal, isActive, isHovered, node.label, readout, light, 1.0f);
            } else if (node.type == project::GuiNodeType::Nixie) {
                float cx = adjNodeX + node.boundsW * 0.5f;
                float cy = adjNodeY + node.boundsH * 0.4f;
                drawRoundedRect(cx - 28.0f, cy - 12.0f, 56.0f, 24.0f, 3.0f, Color(0.05f, 0.02f, 0.0f));
                drawRoundedRectOutline(cx - 28.0f, cy - 12.0f, 56.0f, 24.0f, 3.0f, Color(0.85f, 0.40f, 0.0f), 1.2f);
                drawVectorString(node.label, cx - 18.0f, cy - 4.0f, 0.80f, Color(1.0f, 0.60f, 0.10f));
            } else if (node.type == project::GuiNodeType::Divider) {
                drawLine(adjNodeX + node.boundsW * 0.5f, adjNodeY + 6.0f,
                         adjNodeX + node.boundsW * 0.5f, adjNodeY + node.boundsH * scale - 6.0f,
                         Color(0.4f, 0.45f, 0.55f, 0.5f), 1.0f);
            }
        };

        for (const auto& child : preset->guiRoot.children) {
            renderNode(child);
        }

        // Mini oscilloscope on bottom right of faceplate (fullscreen only)
        if (!isCompact && pW > 600.0f && bg != "walnut" && bg != "wood" && bg != "clavinet") {
            float scX = pX + pW - 195.0f;
            float scY = pY + pH - 90.0f;
            drawRect(scX, scY, 180.0f, 78.0f, 0.02f, 0.05f, 0.03f);
            drawRectOutline(scX, scY, 180.0f, 78.0f, 0.10f, 0.75f, 0.25f, 0.8f, 1.2f);
            drawVectorString("OSCILLOSCOPE", scX + 8.0f, scY + 6.0f, 0.60f, 0.15f, 0.85f, 0.30f);
            constexpr int PTS = 48;
            float prevX = scX + 6.0f;
            float prevY = scY + 45.0f - scopeBuffer_[0] * 28.0f;
            for (int i = 1; i < PTS; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(PTS - 1);
                float sx = scX + 6.0f + t * 168.0f;
                float wave = scopeBuffer_[(i * 4) % 256];
                float sy = scY + 45.0f - wave * 28.0f;
                drawLine(prevX, prevY, sx, sy, Color(0.15f, 1.0f, 0.35f, 0.95f), 1.6f);
                prevX = sx;
                prevY = sy;
            }
        }
        }
    }
    curY += card3H + gap3;

    // SECTION 4: HARMONIC CHORD TRACK FOLLOW
    if (curY + card4H >= y && curY <= y + height) {
        bool isFollowing = (trk.chordFollowMode != ChordFollowMode::Off);
        drawRoundedRectGradient(x, curY, contentW, card4H, 6.0f, Color(0.12f, 0.13f, 0.16f), Color(0.08f, 0.09f, 0.11f));
        drawRoundedRectOutline(x, curY, contentW, card4H, 6.0f,
                               isFollowing ? Color(0.95f, 0.75f, 0.15f) : Color(0.24f, 0.28f, 0.36f),
                               isFollowing ? 1.6f : 1.0f);

        drawVectorString("HARMONIC CHORD TRACK FOLLOW", x + 16.0f, curY + 14.0f, 0.72f, Color(0.95f, 0.75f, 0.15f));
        if (!isCompact) {
            drawVectorString("Conform notes on this track non-destructively to active chords",
                             x + 16.0f, curY + 28.0f, 0.58f, Color(0.60f, 0.65f, 0.72f));
            float bakeX = x + contentW - 130.0f;
            drawRoundedRect(bakeX, curY + 12.0f, 115.0f, 26.0f, 4.0f, Color(0.18f, 0.15f, 0.10f));
            drawRoundedRectOutline(bakeX, curY + 12.0f, 115.0f, 26.0f, 4.0f, Color(0.95f, 0.75f, 0.15f), 1.2f);
            drawVectorString("BAKE TO MIDI", bakeX + 16.0f, curY + 19.0f, 0.65f, Color(0.95f, 0.80f, 0.20f));
        }

        static const char* chipNames[5] = {"OFF", "CHORD", "BASS", "SCALE", "COLOR LEAD"};
        static const ChordFollowMode modes[5] = {
            ChordFollowMode::Off,
            ChordFollowMode::Chord,
            ChordFollowMode::Bass,
            ChordFollowMode::Scale,
            ChordFollowMode::ColorLead
        };
        float chipX = x + 16.0f;
        float chipY = curY + (isCompact ? 40.0f : 44.0f);
        for (int c = 0; c < 5; ++c) {
            float chipW = (c == 4) ? (isCompact ? 80.0f : 100.0f) : (isCompact ? 60.0f : 78.0f);
            bool isSel = (trk.chordFollowMode == modes[c]);
            if (isSel) {
                drawRoundedRect(chipX, chipY, chipW, 26.0f, 13.0f, Color(0.0f, 0.85f, 1.0f));
                drawVectorString(chipNames[c], chipX + 10.0f, chipY + 7.0f, isCompact ? 0.58f : 0.68f, Color(0.05f, 0.08f, 0.12f));
            } else {
                drawRoundedRect(chipX, chipY, chipW, 26.0f, 13.0f, Color(0.14f, 0.16f, 0.20f));
                drawRoundedRectOutline(chipX, chipY, chipW, 26.0f, 13.0f, Color(0.28f, 0.32f, 0.40f), 1.0f);
                drawVectorString(chipNames[c], chipX + 10.0f, chipY + 7.0f, isCompact ? 0.58f : 0.65f, Color(0.70f, 0.75f, 0.82f));
            }
            chipX += chipW + 8.0f;
        }
    }
    curY += card4H + gap4;

    // SECTION 5: SCRIPTABLE MIDI FX RACK
    if (curY + card5H >= y && curY <= y + height) {
        drawRoundedRectGradient(x, curY, contentW, card5H, 6.0f, Color(0.12f, 0.13f, 0.18f), Color(0.08f, 0.09f, 0.12f));
        drawRoundedRectOutline(x, curY, contentW, card5H, 6.0f, Color(0.25f, 0.28f, 0.38f), 1.0f);
        drawVectorString("MIDI FX INSERT RACK", x + 16.0f, curY + 14.0f, 0.72f, Color(0.92f, 0.35f, 0.85f));

        float modW = (contentW - 48.0f) / 3.0f;

        // Mod 1: ARPEGGIATOR
        float m1X = x + 16.0f;
        drawRoundedRect(m1X, curY + 28.0f, modW, 82.0f, 4.0f, Color(0.09f, 0.10f, 0.14f));
        drawRoundedRectOutline(m1X, curY + 28.0f, modW, 82.0f, 4.0f,
                               trk.midiFx.arpEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.30f), 1.2f);
        drawVectorString("ARPEGGIATOR", m1X + 8.0f, curY + 36.0f, 0.65f, Color(0.95f, 0.95f, 1.0f));
        drawRoundedRect(m1X + modW - 32.0f, curY + 34.0f, 24.0f, 14.0f, 7.0f,
                        trk.midiFx.arpEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.28f));
        static const char* arpPatterns[5] = {"UP", "DOWN", "UP/DN", "RAND", "CHORD"};
        std::string patStr = (trk.midiFx.arpPattern >= 0 && trk.midiFx.arpPattern < 5) ? arpPatterns[trk.midiFx.arpPattern] : "UP";
        drawRoundedRect(m1X + 8.0f, curY + 54.0f, 60.0f, 20.0f, 3.0f, Color(0.16f, 0.18f, 0.24f));
        drawVectorString(patStr, m1X + 14.0f, curY + 60.0f, 0.60f, Color(0.90f, 0.90f, 1.0f));
        drawVectorString("RATE: 1/16", m1X + 75.0f, curY + 60.0f, 0.60f, Color(0.65f, 0.70f, 0.80f));
        drawVectorString("OCT: 2", m1X + 145.0f, curY + 60.0f, 0.60f, Color(0.65f, 0.70f, 0.80f));
        drawVectorString("GATE: 85%", m1X + 75.0f, curY + 76.0f, 0.60f, Color(0.65f, 0.70f, 0.80f));
        drawVectorString("SWING: 0%", m1X + 145.0f, curY + 76.0f, 0.60f, Color(0.65f, 0.70f, 0.80f));

        // Mod 2: SCALE SNAP
        float m2X = x + 24.0f + modW;
        drawRoundedRect(m2X, curY + 28.0f, modW, 82.0f, 4.0f, Color(0.09f, 0.10f, 0.14f));
        drawRoundedRectOutline(m2X, curY + 28.0f, modW, 82.0f, 4.0f,
                               trk.midiFx.scaleSnapEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.30f), 1.2f);
        drawVectorString("SCALE SNAP", m2X + 8.0f, curY + 36.0f, 0.65f, Color(0.95f, 0.95f, 1.0f));
        drawRoundedRect(m2X + modW - 32.0f, curY + 34.0f, 24.0f, 14.0f, 7.0f,
                        trk.midiFx.scaleSnapEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.28f));
        drawRoundedRect(m2X + 8.0f, curY + 54.0f, 70.0f, 20.0f, 3.0f, Color(0.16f, 0.18f, 0.24f));
        drawVectorString("KEY: C", m2X + 14.0f, curY + 60.0f, 0.60f, Color(0.90f, 0.90f, 1.0f));
        drawRoundedRect(m2X + 85.0f, curY + 54.0f, 80.0f, 20.0f, 3.0f, Color(0.16f, 0.18f, 0.24f));
        drawVectorString(trk.midiFx.scaleMinor ? "NAT MINOR" : "MAJOR", m2X + 92.0f, curY + 60.0f, 0.60f, Color(0.90f, 0.90f, 1.0f));

        // Mod 3: HUMANIZE
        float m3X = x + 32.0f + 2 * modW;
        drawRoundedRect(m3X, curY + 28.0f, modW, 82.0f, 4.0f, Color(0.09f, 0.10f, 0.14f));
        drawRoundedRectOutline(m3X, curY + 28.0f, modW, 82.0f, 4.0f,
                               trk.midiFx.humanizeEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.30f), 1.2f);
        drawVectorString("HUMANIZE", m3X + 8.0f, curY + 36.0f, 0.65f, Color(0.95f, 0.95f, 1.0f));
        drawRoundedRect(m3X + modW - 32.0f, curY + 34.0f, 24.0f, 14.0f, 7.0f,
                        trk.midiFx.humanizeEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.28f));
        drawVectorString("TIMING: ±15ms", m3X + 12.0f, curY + 60.0f, 0.60f, Color(0.70f, 0.75f, 0.85f));
        drawVectorString("VELOCITY: ±20%", m3X + 105.0f, curY + 60.0f, 0.60f, Color(0.70f, 0.75f, 0.85f));
    }
    curY += card5H + gap5;

    // SECTION 6: AUDIO FX INSERT RACK
    if (curY + card6H >= y && curY <= y + height) {
        drawRoundedRectGradient(x, curY, contentW, card6H, 6.0f, Color(0.12f, 0.14f, 0.18f), Color(0.08f, 0.09f, 0.12f));
        drawRoundedRectOutline(x, curY, contentW, card6H, 6.0f, Color(0.25f, 0.28f, 0.38f), 1.0f);
        drawVectorString("AUDIO FX INSERT RACK", x + 16.0f, curY + 14.0f, 0.72f, Color(0.0f, 0.90f, 1.0f));

        float fxW = (contentW - 64.0f) / 5.0f;
        const char* fxTitles[5] = {"DELAY", "CHORUS", "5-BAND EQ", "COMPRESSOR", "CONVOLVER"};
        bool fxEnabled[5] = {
            trk.audioFx.delayEnabled,
            trk.audioFx.chorusEnabled,
            trk.audioFx.eqEnabled,
            trk.audioFx.compEnabled,
            trk.audioFx.convolverEnabled
        };

        for (int u = 0; u < 5; ++u) {
            float ux = x + 16.0f + u * (fxW + 8.0f);
            drawRoundedRect(ux, curY + 28.0f, fxW, 95.0f, 4.0f, Color(0.09f, 0.10f, 0.14f));
            drawRoundedRectOutline(ux, curY + 28.0f, fxW, 95.0f, 4.0f,
                                   fxEnabled[u] ? Color(0.0f, 0.85f, 1.0f) : Color(0.20f, 0.24f, 0.30f), 1.2f);
            drawVectorString(fxTitles[u], ux + 8.0f, curY + 36.0f, 0.62f, Color(0.95f, 0.95f, 1.0f));
            drawRoundedRect(ux + fxW - 28.0f, curY + 34.0f, 20.0f, 12.0f, 6.0f,
                            fxEnabled[u] ? Color(0.0f, 0.90f, 1.0f) : Color(0.20f, 0.24f, 0.28f));

            float kw = fxW / 3.0f;
            float ky = curY + 68.0f;
            if (u == 0) { // Delay: Time, Fdbk, Mix
                drawMiniDial(ux + kw * 0.5f, ky, 12.0f, trk.audioFx.delayTime, "TIME");
                drawMiniDial(ux + kw * 1.5f, ky, 12.0f, trk.audioFx.delayFeedback, "FDBK");
                drawMiniDial(ux + kw * 2.5f, ky, 12.0f, trk.audioFx.delayMix, "MIX");
            } else if (u == 1) { // Chorus: Rate, Depth, Mix
                drawMiniDial(ux + kw * 0.5f, ky, 12.0f, trk.audioFx.chorusRate, "RATE");
                drawMiniDial(ux + kw * 1.5f, ky, 12.0f, trk.audioFx.chorusDepth, "DPTH");
                drawMiniDial(ux + kw * 2.5f, ky, 12.0f, trk.audioFx.chorusMix, "MIX");
            } else if (u == 2) { // EQ: Low, Mid, High
                drawMiniDial(ux + kw * 0.5f, ky, 12.0f, trk.audioFx.eqLow, "LOW");
                drawMiniDial(ux + kw * 1.5f, ky, 12.0f, trk.audioFx.eqMid, "MID");
                drawMiniDial(ux + kw * 2.5f, ky, 12.0f, trk.audioFx.eqHigh, "HIGH");
            } else if (u == 3) { // Comp: Thresh, Ratio, Gain
                drawMiniDial(ux + kw * 0.5f, ky, 12.0f, trk.audioFx.compThreshold, "THRS");
                drawMiniDial(ux + kw * 1.5f, ky, 12.0f, trk.audioFx.compRatio, "RATIO");
                drawMiniDial(ux + kw * 2.5f, ky, 12.0f, trk.audioFx.compGain, "GAIN");
            } else if (u == 4) { // Convolver: Space, Mix
                drawMiniDial(ux + kw * 0.8f, ky, 12.0f, static_cast<float>(trk.audioFx.convolverPreset) / 12.0f, "SPACE");
                drawMiniDial(ux + kw * 2.2f, ky, 12.0f, trk.audioFx.convolverMix, "MIX");
            }
        }
    }

    // Scrollbar Rendering on the right edge
    if (needScrollbar) {
        float sbX = x + width - 8.0f;
        float sbY = y;
        float sbW = 6.0f;
        float sbH = height;
        drawRoundedRect(sbX, sbY, sbW, sbH, 3.0f, Color(0.06f, 0.07f, 0.09f));
        drawRoundedRectOutline(sbX, sbY, sbW, sbH, 3.0f, Color(0.18f, 0.20f, 0.26f), 1.0f);
        float thumbH = std::clamp(height * (height / totalContentH), 32.0f, height * 0.85f);
        float thumbNorm = std::clamp(scrollY / maxScroll, 0.0f, 1.0f);
        float thumbY = sbY + thumbNorm * (sbH - thumbH);
        bool isDraggingSb = (dragMode_ == DragMode::TrackInspectorScrollbar);
        drawRoundedRectGradient(sbX, thumbY, sbW, thumbH, 3.0f,
                                isDraggingSb ? Color(0.0f, 0.90f, 1.0f) : Color(0.35f, 0.40f, 0.50f),
                                Color(0.18f, 0.22f, 0.30f));
        drawRoundedRectOutline(sbX, thumbY, sbW, thumbH, 3.0f,
                               isDraggingSb ? Color(1.0f, 1.0f, 1.0f) : Color(0.0f, 0.85f, 1.0f, 0.7f), 1.0f);
    }
}

HitTestTrackInspectorResult GuiWindow::hitTestTrackPropertiesContainer(float mx, float my, float x, float y, float width, float height, uint32_t trackIndex, float scrollY, bool isCompact) const noexcept {
    HitTestTrackInspectorResult res{};
    uint32_t tIdx = (trackIndex < arrangerTracks_.size()) ? trackIndex : 0;
    const auto& trk = (tIdx < arrangerTracks_.size()) ? arrangerTracks_[tIdx] : arrangerTracks_[0];
    res.trackIndex = tIdx;

    float card1H = isCompact ? 52.0f : 60.0f;
    float gap1 = isCompact ? 8.0f : 8.0f;
    float cardColorH = isCompact ? 50.0f : 0.0f;
    float gapColor = isCompact ? 8.0f : 0.0f;
    float card2H = isCompact ? 86.0f : 68.0f;
    float gap2 = isCompact ? 6.0f : 8.0f;
    const float designW = 520.0f;
    float card3H = isCompact ? (30.0f + 210.0f * (std::max(200.0f, width - 16.0f) / designW)) : 260.0f;
    float gap3 = isCompact ? 8.0f : 12.0f;
    float card4H = isCompact ? 84.0f : 80.0f;
    float gap4 = isCompact ? 6.0f : 10.0f;
    float card5H = isCompact ? 180.0f : 120.0f;
    float gap5 = isCompact ? 6.0f : 10.0f;
    float card6H = isCompact ? 280.0f : 135.0f;
    float gap6 = isCompact ? 16.0f : 20.0f;

    float totalContentH = card1H + gap1 + (isCompact ? (cardColorH + gapColor) : 0.0f) + card2H + gap2 + card3H + gap3 + card4H + gap4 + card5H + gap5 + card6H + gap6;
    float maxScroll = std::max(10.0f, totalContentH - height + 40.0f);

    bool needScrollbar = (totalContentH > height);
    float contentW = needScrollbar ? (width - 16.0f) : width;

    // Check Scrollbar hit first
    if (needScrollbar) {
        float sbX = x + width - 8.0f;
        float sbY = y;
        float sbH = height;
        if (mx >= sbX - 6.0f && mx <= sbX + 10.0f && my >= sbY && my <= sbY + sbH) {
            float thumbH = std::clamp(height * (height / totalContentH), 32.0f, height * 0.85f);
            float thumbNorm = std::clamp(scrollY / maxScroll, 0.0f, 1.0f);
            float thumbY = sbY + thumbNorm * (sbH - thumbH);
            res.hit = true;
            if (my >= thumbY && my <= thumbY + thumbH) {
                res.area = TrackInspectorHitArea::ScrollbarThumb;
            } else {
                res.area = TrackInspectorHitArea::ScrollbarTrack;
            }
            return res;
        }
    }

    if (mx < x || mx > x + contentW || my < y || my > y + height) {
        return res;
    }

    float curY = y - scrollY;

    // SECTION 1: Track Header Card
    if (my >= curY && my <= curY + card1H) {
        if (!isCompact) {
            float swStartX = x + 310.0f;
            float swY = curY + 18.0f;
            for (uint32_t s = 0; s < 8; ++s) {
                float sx = swStartX + s * 26.0f;
                if (mx >= sx && mx <= sx + 20.0f && my >= swY && my <= swY + 24.0f) {
                    res.hit = true;
                    res.area = TrackInspectorHitArea::ColorSwatch;
                    res.colorSwatchIndex = s;
                    return res;
                }
            }

            float btnCodeX = x + contentW - 245.0f;
            if (mx >= btnCodeX && mx <= btnCodeX + 62.0f && my >= curY + 15.0f && my <= curY + 45.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::CodeButton;
                return res;
            }
            float btnMuteX = x + contentW - 175.0f;
            if (mx >= btnMuteX && mx <= btnMuteX + 52.0f && my >= curY + 15.0f && my <= curY + 45.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::MuteButton;
                return res;
            }
            float btnSoloX = x + contentW - 117.0f;
            if (mx >= btnSoloX && mx <= btnSoloX + 52.0f && my >= curY + 15.0f && my <= curY + 45.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::SoloButton;
                return res;
            }
            float btnFzX = x + contentW - 59.0f;
            if (mx >= btnFzX && mx <= btnFzX + 54.0f && my >= curY + 15.0f && my <= curY + 45.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::FreezeButton;
                return res;
            }
        } else {
            float btnMuteX = x + contentW - 110.0f;
            if (mx >= btnMuteX && mx <= btnMuteX + 50.0f && my >= curY + 12.0f && my <= curY + 42.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::MuteButton;
                return res;
            }
            float btnSoloX = x + contentW - 54.0f;
            if (mx >= btnSoloX && mx <= btnSoloX + 50.0f && my >= curY + 12.0f && my <= curY + 42.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::SoloButton;
                return res;
            }
        }
    }
    curY += card1H + gap1;

    // SECTION 1B: Track Accent Color (Compact Sidebar Mode)
    if (isCompact) {
        if (my >= curY && my <= curY + cardColorH) {
            float swatchStep = (contentW - 4.0f) / 8.0f;
            for (uint32_t s = 0; s < 8; ++s) {
                float sx = x + 12.0f + s * swatchStep;
                float sy = curY + 20.0f;
                float sw = swatchStep - 4.0f;
                float sh = 24.0f;
                if (mx >= sx && mx <= sx + sw && my >= sy && my <= sy + sh) {
                    res.hit = true;
                    res.area = TrackInspectorHitArea::ColorSwatch;
                    res.colorSwatchIndex = s;
                    return res;
                }
            }
        }
        curY += cardColorH + gapColor;
    }

    // SECTION 2: Channel Mixer
    if (my >= curY && my <= curY + card2H) {
        if (!isCompact) {
            if (mx >= x + 70.0f && mx <= x + 310.0f && my >= curY + 30.0f && my <= curY + 60.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::VolumeSlider;
                res.normVal = trk.volume;
                return res;
            }
            if (mx >= x + 410.0f && mx <= x + 610.0f && my >= curY + 30.0f && my <= curY + 60.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PanKnob;
                res.normVal = trk.pan;
                return res;
            }
        } else {
            float sliderW = contentW - 120.0f;
            if (mx >= x + 50.0f && mx <= x + 60.0f + sliderW && my >= curY + 22.0f && my <= curY + 48.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::VolumeSlider;
                res.normVal = trk.volume;
                return res;
            }
            if (mx >= x + 50.0f && mx <= x + 60.0f + sliderW && my >= curY + 50.0f && my <= curY + 76.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PanKnob;
                res.normVal = trk.pan;
                return res;
            }
        }
    }
    curY += card2H + gap2;

    // SECTION 3: Dynamic Instrument Faceplate
    if (my >= curY && my <= curY + card3H) {
        if (isCompact) {
            float pW = contentW;
            // < PREV: [x + 6, curY + 4, 32, 20]
            if (mx >= x + 6.0f && mx <= x + 38.0f && my >= curY + 4.0f && my <= curY + 24.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PresetPrev;
                return res;
            }
            // NEXT >: [x + 42, curY + 4, 32, 20]
            if (mx >= x + 42.0f && mx <= x + 74.0f && my >= curY + 4.0f && my <= curY + 24.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PresetNext;
                return res;
            }
            // FULL: [x + pW - 50, curY + 4, 44, 20]
            float fullBtnW = 44.0f;
            float fullBtnX = x + pW - fullBtnW - 6.0f;
            if (mx >= fullBtnX && mx <= fullBtnX + fullBtnW && my >= curY + 4.0f && my <= curY + 24.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::FullscreenDevice;
                return res;
            }
            if (my >= curY + 28.0f) {
                const auto* preset = getActivePreset();
                if (preset) {
                    float scale = contentW / designW;
                    float localX = (mx - x) / scale;
                    float localY = (my - (curY + 28.0f)) / scale + 28.0f;
                    HitTestHardwareKnobResult knobHit{};
                    hitTestCompactNodeRecursive(preset->compactGuiRoot, localX, localY, *preset, knobHit);
                    if (knobHit.hit) {
                        res.hit = true;
                        res.area = TrackInspectorHitArea::HardwareKnob;
                        res.paramName = knobHit.paramName;
                        res.normVal = knobHit.currentNormVal;
                        res.position = { x + knobHit.position.x * scale, (curY + 28.0f) + (knobHit.position.y - 28.0f) * scale };
                        return res;
                    }
                }
            }
        } else {
            // Preset Prev: [x + 16, curY + 12, 75, 28]
            if (mx >= x + 16.0f && mx <= x + 91.0f && my >= curY + 12.0f && my <= curY + 40.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PresetPrev;
                return res;
            }
            // Preset Next: [x + 98, curY + 12, 75, 28]
            if (mx >= x + 98.0f && mx <= x + 173.0f && my >= curY + 12.0f && my <= curY + 40.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PresetNext;
                return res;
            }

            float scale = 0.70f;
            auto knobHit = hitTestHardwareKnob(mx, my, x, curY, scale);
            if (knobHit.hit) {
                res.hit = true;
                res.area = TrackInspectorHitArea::HardwareKnob;
                res.paramName = knobHit.paramName;
                res.normVal = knobHit.currentNormVal;
                res.position = knobHit.position;
                return res;
            }
        }
    }
    curY += card3H + gap3;

    // SECTION 4: Harmonic Chord Follow
    if (my >= curY && my <= curY + card4H) {
        if (!isCompact) {
            float bakeX = x + contentW - 130.0f;
            if (mx >= bakeX && mx <= bakeX + 115.0f && my >= curY + 12.0f && my <= curY + 38.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::BakeChordsButton;
                return res;
            }
        }
        static const ChordFollowMode modes[5] = {
            ChordFollowMode::Off,
            ChordFollowMode::Chord,
            ChordFollowMode::Bass,
            ChordFollowMode::Scale,
            ChordFollowMode::ColorLead
        };
        float chipX = x + 16.0f;
        float chipY = curY + (isCompact ? 40.0f : 44.0f);
        for (int c = 0; c < 5; ++c) {
            float chipW = (c == 4) ? (isCompact ? 80.0f : 100.0f) : (isCompact ? 60.0f : 78.0f);
            if (mx >= chipX && mx <= chipX + chipW && my >= chipY && my <= chipY + 26.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::ChordFollowChip;
                res.chordMode = modes[c];
                return res;
            }
            chipX += chipW + 8.0f;
        }
    }
    curY += card4H + gap4;

    // SECTION 5: MIDI FX
    if (my >= curY && my <= curY + card5H) {
        float modW = (contentW - 48.0f) / 3.0f;
        float m1X = x + 16.0f;
        if (mx >= m1X + modW - 36.0f && mx <= m1X + modW - 8.0f && my >= curY + 28.0f && my <= curY + 50.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpToggle;
            return res;
        }
        if (mx >= m1X + 8.0f && mx <= m1X + 70.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpKnob;
            res.paramName = "arpPattern";
            return res;
        }
        if (mx >= m1X + 75.0f && mx <= m1X + 140.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpKnob;
            res.paramName = "arpRate";
            res.normVal = trk.midiFx.arpRate;
            return res;
        }
        if (mx >= m1X + 145.0f && mx <= m1X + 210.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpKnob;
            res.paramName = "arpOctaves";
            res.normVal = static_cast<float>(trk.midiFx.arpOctaves);
            return res;
        }
        if (mx >= m1X + 75.0f && mx <= m1X + 140.0f && my >= curY + 74.0f && my <= curY + 96.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpKnob;
            res.paramName = "arpGate";
            res.normVal = trk.midiFx.arpGate;
            return res;
        }
        if (mx >= m1X + 145.0f && mx <= m1X + 210.0f && my >= curY + 74.0f && my <= curY + 96.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpKnob;
            res.paramName = "arpSwing";
            res.normVal = trk.midiFx.arpSwing;
            return res;
        }

        float m2X = x + 24.0f + modW;
        if (mx >= m2X + modW - 36.0f && mx <= m2X + modW - 8.0f && my >= curY + 28.0f && my <= curY + 50.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxScaleToggle;
            return res;
        }
        if (mx >= m2X + 8.0f && mx <= m2X + 80.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxScaleKnob;
            res.paramName = "scaleRoot";
            return res;
        }
        if (mx >= m2X + 85.0f && mx <= m2X + 170.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxScaleKnob;
            res.paramName = "scaleMinor";
            return res;
        }

        float m3X = x + 32.0f + 2 * modW;
        if (mx >= m3X + modW - 36.0f && mx <= m3X + modW - 8.0f && my >= curY + 28.0f && my <= curY + 50.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxHumanizeToggle;
            return res;
        }
        if (mx >= m3X + 8.0f && mx <= m3X + 100.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxHumanizeKnob;
            res.paramName = "humanizeTiming";
            res.normVal = trk.midiFx.humanizeTiming;
            return res;
        }
        if (mx >= m3X + 105.0f && mx <= m3X + 210.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxHumanizeKnob;
            res.paramName = "humanizeVelocity";
            res.normVal = trk.midiFx.humanizeVelocity;
            return res;
        }
    }
    curY += card5H + gap5;

    // SECTION 6: AUDIO FX
    if (my >= curY && my <= curY + card6H) {
        float fxW = (contentW - 64.0f) / 5.0f;
        for (int u = 0; u < 5; ++u) {
            float ux = x + 16.0f + u * (fxW + 8.0f);
            if (mx >= ux + fxW - 32.0f && mx <= ux + fxW - 6.0f && my >= curY + 28.0f && my <= curY + 50.0f) {
                res.hit = true;
                if (u == 0) res.area = TrackInspectorHitArea::AudioFxDelayToggle;
                else if (u == 1) res.area = TrackInspectorHitArea::AudioFxChorusToggle;
                else if (u == 2) res.area = TrackInspectorHitArea::AudioFxEqToggle;
                else if (u == 3) res.area = TrackInspectorHitArea::AudioFxCompToggle;
                else if (u == 4) res.area = TrackInspectorHitArea::AudioFxConvolverToggle;
                return res;
            }
            float kw = fxW / 3.0f;
            float ky = curY + 68.0f;
            for (int k = 0; k < 3; ++k) {
                float kx = ux + (k + 0.5f) * kw;
                if (std::hypot(mx - kx, my - ky) <= 16.0f) {
                    res.hit = true;
                    if (u == 0) {
                        res.area = TrackInspectorHitArea::AudioFxDelayKnob;
                        res.paramName = (k == 0) ? "delayTime" : ((k == 1) ? "delayFeedback" : "delayMix");
                        res.normVal = (k == 0) ? trk.audioFx.delayTime : ((k == 1) ? trk.audioFx.delayFeedback : trk.audioFx.delayMix);
                    } else if (u == 1) {
                        res.area = TrackInspectorHitArea::AudioFxChorusKnob;
                        res.paramName = (k == 0) ? "chorusRate" : ((k == 1) ? "chorusDepth" : "chorusMix");
                        res.normVal = (k == 0) ? trk.audioFx.chorusRate : ((k == 1) ? trk.audioFx.chorusDepth : trk.audioFx.chorusMix);
                    } else if (u == 2) {
                        res.area = TrackInspectorHitArea::AudioFxEqKnob;
                        res.paramName = (k == 0) ? "eqLow" : ((k == 1) ? "eqMid" : "eqHigh");
                        res.normVal = (k == 0) ? trk.audioFx.eqLow : ((k == 1) ? trk.audioFx.eqMid : trk.audioFx.eqHigh);
                    } else if (u == 3) {
                        res.area = TrackInspectorHitArea::AudioFxCompKnob;
                        res.paramName = (k == 0) ? "compThreshold" : ((k == 1) ? "compRatio" : "compGain");
                        res.normVal = (k == 0) ? trk.audioFx.compThreshold : ((k == 1) ? trk.audioFx.compRatio : trk.audioFx.compGain);
                    } else if (u == 4) {
                        res.area = TrackInspectorHitArea::AudioFxConvolverKnob;
                        res.paramName = (k == 0) ? "convolverPreset" : "convolverMix";
                        res.normVal = (k == 0) ? static_cast<float>(trk.audioFx.convolverPreset) / 12.0f : trk.audioFx.convolverMix;
                    }
                    return res;
                }
            }
        }
    }

    return res;
}

HitTestTrackInspectorResult GuiWindow::hitTestTrackInspector(float x, float y) const noexcept {
    HitTestTrackInspectorResult res{};
    if (activeView_ != WorkspaceView::Track && activeView_ != WorkspaceView::HardwarePanel) return res;

    const float inspX = 40.0f;
    const float inspY = 66.0f;
    const float inspW = static_cast<float>(width_) - 80.0f;
    const float bPanelY = static_cast<float>(height_) - 48.0f;
    const float inspH = bPanelY - inspY - 8.0f;

    return hitTestTrackPropertiesContainer(x, y, inspX, inspY, inspW, inspH, selectedTrackIndex_, trackInspectorScrollY_, false);
}

HitTestBrowserResult GuiWindow::hitTestBrowser(float x, float y) const noexcept {
    HitTestBrowserResult res{};
    if (!browserOpen_) return res;

    const float drW = 440.0f;
    const float drX = static_cast<float>(width_) - drW - 16.0f;
    const float drY = 60.0f;
    const float drH = static_cast<float>(height_) - 116.0f;

    if (x < drX || x > drX + drW || y < drY || y > drY + drH) {
        return res;
    }

    res.hit = true;

    // 1. Close button [drX + drW - 32, drY + 8, 24, 24]
    if (x >= (drX + drW - 32.0f) && x <= (drX + drW - 8.0f) && y >= (drY + 8.0f) && y <= (drY + 32.0f)) {
        res.action = BrowserHitAction::Close;
        return res;
    }

    // 2. Tab Selection pills: [drY + 48..drY + 72]
    if (y >= (drY + 48.0f) && y <= (drY + 72.0f)) {
        const float tabW = 130.0f;
        const float tabGap = 8.0f;
        float tab1X = drX + 16.0f;
        float tab2X = drX + 16.0f + tabW + tabGap;
        float tab3X = drX + 16.0f + 2.0f * (tabW + tabGap);

        if (x >= tab1X && x <= tab1X + tabW) {
            res.action = BrowserHitAction::TabPresets;
            return res;
        }
        if (x >= tab2X && x <= tab2X + tabW) {
            res.action = BrowserHitAction::TabMacros;
            return res;
        }
        if (x >= tab3X && x <= tab3X + tabW) {
            res.action = BrowserHitAction::TabHistory;
            return res;
        }
    }

    // 3. Project Action buttons at bottom: [drY + drH - 46..drY + drH - 14]
    float actY = drY + drH - 46.0f;
    if (y >= actY && y <= actY + 32.0f) {
        // [ SAVE ] at [drX + 16, actY, 124, 32]
        if (x >= drX + 16.0f && x <= drX + 140.0f) {
            res.action = BrowserHitAction::SaveProject;
            return res;
        }
        // [ LOAD ] at [drX + 150, actY, 124, 32]
        if (x >= drX + 150.0f && x <= drX + 274.0f) {
            res.action = BrowserHitAction::LoadProject;
            return res;
        }
        // [ BOUNCE ] at [drX + 284, actY, 140, 32]
        if (x >= drX + 284.0f && x <= drX + 424.0f) {
            res.action = BrowserHitAction::BounceMaster;
            return res;
        }
    }

    // 4. Content rows based on active tab:
    if (browserTab_ == BrowserTab::Presets) {
        // Category selection pills [drY + 76..drY + 102]
        if (y >= (drY + 76.0f) && y <= (drY + 102.0f)) {
            const char* cats[6] = {"ALL", "BASS", "LEAD", "DRUMS", "KEYS", "FX"};
            const float catW = 58.0f;
            const float catGap = 8.0f;
            for (int c = 0; c < 6; ++c) {
                float cx = drX + 16.0f + c * (catW + catGap);
                if (x >= cx && x <= cx + catW) {
                    res.action = BrowserHitAction::CategorySelect;
                    res.category = cats[c];
                    return res;
                }
            }
        }

        // Preset list rows [drY + 110..actY - 8]
        float rowStartY = drY + 110.0f;
        float rowH = 34.0f;
        float rowGap = 4.0f;
        size_t visibleIdx = 0;
        for (size_t i = 0; i < presets_.size(); ++i) {
            const auto& p = presets_[i];
            if (browserCategory_ != "ALL") {
                std::string catUpper = p.metadata.category;
                std::transform(catUpper.begin(), catUpper.end(), catUpper.begin(), ::toupper);
                std::string idUpper = p.metadata.id;
                std::transform(idUpper.begin(), idUpper.end(), idUpper.begin(), ::toupper);
                std::string engUpper = p.metadata.engineId;
                std::transform(engUpper.begin(), engUpper.end(), engUpper.begin(), ::toupper);

                if (catUpper.find(browserCategory_) == std::string::npos &&
                    idUpper.find(browserCategory_) == std::string::npos &&
                    engUpper.find(browserCategory_) == std::string::npos) {
                    continue;
                }
            }

            float ry = rowStartY + visibleIdx * (rowH + rowGap);
            if (ry + rowH > actY - 8.0f) break;

            if (y >= ry && y <= ry + rowH && x >= drX + 16.0f && x <= drX + drW - 16.0f) {
                if (x >= (drX + drW - 74.0f) && x <= (drX + drW - 16.0f)) {
                    res.action = BrowserHitAction::PresetLoad;
                } else {
                    res.action = BrowserHitAction::PresetSelect;
                }
                res.presetIndex = i;
                return res;
            }
            visibleIdx++;
        }
    } else if (browserTab_ == BrowserTab::Macros) {
        // Macro list rows [drY + 84..actY - 8]
        const auto& macros = eatscript::MacroRuntime::getBuiltinMacros();
        float rowStartY = drY + 84.0f;
        float rowH = 48.0f;
        float rowGap = 6.0f;

        for (size_t i = 0; i < macros.size(); ++i) {
            float ry = rowStartY + i * (rowH + rowGap);
            if (ry + rowH > actY - 8.0f) break;

            if (y >= ry && y <= ry + rowH && x >= drX + 16.0f && x <= drX + drW - 16.0f) {
                if (x >= (drX + drW - 74.0f) && x <= (drX + drW - 16.0f)) {
                    res.action = BrowserHitAction::MacroRun;
                    res.macroIndex = i;
                    return res;
                }
            }
        }
    } else if (browserTab_ == BrowserTab::History) {
        float tbY = drY + 76.0f;
        if (y >= tbY && y <= tbY + 26.0f) {
            // [ UNDO ]
            if (x >= drX + 16.0f && x <= drX + 106.0f) {
                res.action = BrowserHitAction::HistoryUndo;
                return res;
            }
            // [ REDO ]
            if (x >= drX + 112.0f && x <= drX + 202.0f) {
                res.action = BrowserHitAction::HistoryRedo;
                return res;
            }
            // [ + CHECKPOINT ]
            if (x >= drX + 208.0f && x <= drX + 324.0f) {
                res.action = BrowserHitAction::HistoryMilestone;
                return res;
            }
            // [ CLEAR ]
            if (x >= drX + 330.0f && x <= drX + 424.0f) {
                res.action = BrowserHitAction::HistoryClear;
                return res;
            }
        }

        // Timeline item clicks
        float listY = drY + 124.0f;
        float itemH = 34.0f;
        float itemGap = 4.0f;
        float maxListH = (actY - 146.0f) - listY;

        auto timeline = diffHistory_.getTimeline();
        size_t curIdx = diffHistory_.getCurrentTimelineIndex();
        size_t maxItems = static_cast<size_t>(std::max(1, static_cast<int>(maxListH / (itemH + itemGap))));
        size_t startItem = 0;
        if (curIdx >= maxItems) {
            startItem = curIdx - maxItems + 1;
        }

        for (size_t i = startItem; i < timeline.size() && (i - startItem) < maxItems; ++i) {
            float iy = listY + (i - startItem) * (itemH + itemGap);
            if (y >= iy && y <= iy + itemH && x >= drX + 16.0f && x <= drX + drW - 16.0f) {
                res.action = BrowserHitAction::HistoryStepSelect;
                res.historyStepIndex = i;
                return res;
            }
        }
    }

    return res;
}

HitTestProjectHubResult GuiWindow::hitTestProjectHub(float x, float y) const noexcept {
    HitTestProjectHubResult res{};
    if (!projectHubOpen_) return res;

    DialogLayout dl = computeDialogLayout(540.0f, 620.0f);
    const float hubX = dl.x;
    const float hubY = dl.y;
    const float hubW = dl.w;
    const float hubH = dl.h;

    if (x < hubX || x > hubX + hubW || y < hubY || y > hubY + hubH) {
        return res;
    }

    res.hit = true;

    // 1. Close button (Top-Right screw button)
    if (x >= (dl.closeBtnX - 4.0f) && x <= (dl.closeBtnX + dl.closeBtnW + 4.0f) &&
        y >= (dl.closeBtnY - 4.0f) && y <= (dl.closeBtnY + dl.closeBtnH + 4.0f)) {
        res.action = ProjectHubAction::Close;
        return res;
    }

    // 2. Close button (Bottom-Right CLOSE button)
    if (x >= (dl.bottomCloseX - 6.0f) && x <= (dl.bottomCloseX + dl.bottomCloseW + 6.0f) &&
        y >= (dl.bottomCloseY - 4.0f) && y <= (dl.bottomCloseY + dl.bottomCloseH + 6.0f)) {
        res.action = ProjectHubAction::Close;
        return res;
    }

    const float topContentY = hubY + 50.0f;
    const float footerY = hubY + hubH - 36.0f;
    const float headerH = 30.0f;
    const float headerGap = 6.0f;
    const float headerStep = headerH + headerGap; // 36.0f
    const int numSections = 6;
    const float availableDrawerH = (footerY - topContentY) - (numSections * headerStep) - headerGap;

    float curDrawerH = 0.0f;
    if (projectHubSection_ >= 0) {
        if (projectHubSection_ == 0) curDrawerH = 190.0f;
        else if (projectHubSection_ == 1) curDrawerH = 186.0f;
        else if (projectHubSection_ == 2) curDrawerH = 244.0f;
        else if (projectHubSection_ == 3) curDrawerH = availableDrawerH;
        else if (projectHubSection_ == 4) curDrawerH = 176.0f;
        else if (projectHubSection_ == 5) curDrawerH = 100.0f;
    }

    const float drawerX = hubX + 16.0f;
    const float drawerW = hubW - 32.0f;
    const float drawerY = topContentY + (projectHubSection_ + 1) * headerStep;
    const float drawerH = curDrawerH;

    // 1. Check Section Headers hit FIRST (headers always receive clicks over underlying drawer bleed)
    for (int s = 0; s < 6; ++s) {
        float hY = 0.0f;
        if (projectHubSection_ == -1 || s <= projectHubSection_) {
            hY = topContentY + s * headerStep;
        } else {
            hY = drawerY + drawerH + headerGap + (s - (projectHubSection_ + 1)) * headerStep;
        }

        if (y >= hY && y <= hY + 30.0f && x >= hubX + 12.0f && x <= hubX + hubW - 12.0f) {
            res.action = ProjectHubAction::SectionHeader;
            res.sectionIndex = s;
            return res;
        }
    }

    // 2. If a section is open, check hit within drawer bounds
    if (projectHubSection_ >= 0 && x >= drawerX && x <= drawerX + drawerW && y >= drawerY && y <= drawerY + drawerH) {
        if (projectHubSection_ == 0) {
            float contentStartY = drawerY;
            if (y >= contentStartY + 6.0f && y <= contentStartY + 48.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::TitleClick;
                return res;
            }
            if (y >= contentStartY + 54.0f && y <= contentStartY + 96.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::AuthorClick;
                return res;
            }
            const float btnW = (hubW - 48.0f - 16.0f) / 3.0f;
            const float row1Y = contentStartY + 104.0f;
            const float row2Y = contentStartY + 144.0f;

            if (y >= row1Y && y <= row1Y + 34.0f) {
                if (x >= hubX + 24.0f && x <= hubX + 24.0f + btnW) { res.action = ProjectHubAction::SaveProject; return res; }
                if (x >= hubX + 32.0f + btnW && x <= hubX + 32.0f + 2.0f * btnW) { res.action = ProjectHubAction::SaveAsProject; return res; }
                if (x >= hubX + 40.0f + 2.0f * btnW && x <= hubX + 40.0f + 3.0f * btnW) { res.action = ProjectHubAction::LoadProject; return res; }
            } else if (y >= row2Y && y <= row2Y + 34.0f) {
                if (x >= hubX + 24.0f && x <= hubX + 24.0f + btnW) { res.action = ProjectHubAction::NewProject; return res; }
                if (x >= hubX + 32.0f + btnW && x <= hubX + 32.0f + 2.0f * btnW) { res.action = ProjectHubAction::BounceWav; return res; }
                if (x >= hubX + 40.0f + 2.0f * btnW && x <= hubX + 40.0f + 3.0f * btnW) { res.action = ProjectHubAction::OpenScriptView; return res; }
            }
        } else if (projectHubSection_ == 1) {
            float contentStartY = drawerY;
            if (y >= contentStartY + 6.0f && y <= contentStartY + 34.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ToggleRestoreSession;
                return res;
            }
            if (y >= contentStartY + 38.0f && y <= contentStartY + 66.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ToggleAutosave;
                return res;
            }
            if (y >= contentStartY + 74.0f && y <= contentStartY + 102.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ResetCleanSlate;
                return res;
            }
#if defined(__EMSCRIPTEN__)
            if (y >= contentStartY + 108.0f && y <= contentStartY + 136.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::RevertSettingsDefault;
                return res;
            }
#else
            const float setBtnW = (hubW - 48.0f - 10.0f) * 0.5f;
            if (y >= contentStartY + 108.0f && y <= contentStartY + 136.0f) {
                if (x >= hubX + 24.0f && x <= hubX + 24.0f + setBtnW) {
                    res.action = ProjectHubAction::RevertSettingsDefault;
                    return res;
                }
                if (x >= hubX + 34.0f + setBtnW && x <= hubX + 34.0f + 2.0f * setBtnW) {
                    res.action = ProjectHubAction::OpenSettingsFile;
                    return res;
                }
            }
#endif
        } else if (projectHubSection_ == 2) {
            float contentStartY = drawerY;
            // 1. Slider track hit [contentStartY + 10.0f .. contentStartY + 34.0f]
            float sliderX = hubX + 24.0f;
            float sliderW = hubW - 130.0f;
            if (y >= contentStartY + 10.0f && y <= contentStartY + 34.0f && x >= sliderX && x <= sliderX + sliderW + 8.0f) {
                float norm = std::clamp((x - sliderX) / sliderW, 0.0f, 1.0f);
                float rawScale = 0.50f + norm * 1.50f;
                float snappedScale = std::round(rawScale / 0.25f) * 0.25f;
                snappedScale = std::clamp(snappedScale, 0.50f, 2.00f);
                res.action = ProjectHubAction::SetUiScale;
                res.scaleValue = snappedScale;
                return res;
            }
            // 2. Discrete 7 chips hit [contentStartY + 35.0f .. contentStartY + 52.0f]
            if (y >= contentStartY + 35.0f && y <= contentStartY + 52.0f) {
                const float scales[7] = {0.50f, 0.75f, 1.0f, 1.25f, 1.50f, 1.75f, 2.0f};
                float chipGap = 5.0f;
                float chipW = (hubW - 48.0f - (6.0f * chipGap)) / 7.0f;
                for (int sc = 0; sc < 7; ++sc) {
                    float cx = hubX + 24.0f + sc * (chipW + chipGap);
                    if (x >= cx && x <= cx + chipW) {
                        res.action = ProjectHubAction::SetUiScale;
                        res.scaleValue = scales[sc];
                        return res;
                    }
                }
            }

            // 3. Theme Header: Bank Toggles & Browse Button [contentStartY + 54.0f .. contentStartY + 76.0f]
            if (y >= contentStartY + 54.0f && y <= contentStartY + 76.0f) {
                // Bank 0: [ 1: CONSOLES (1-5) ]
                if (x >= hubX + 76.0f && x <= hubX + 196.0f) {
                    res.action = ProjectHubAction::SelectThemeBank;
                    res.themeBank = 0;
                    return res;
                }
                // Bank 1: [ 2: SYNTAX (6-10) ]
                if (x >= hubX + 202.0f && x <= hubX + 328.0f) {
                    res.action = ProjectHubAction::SelectThemeBank;
                    res.themeBank = 1;
                    return res;
                }
                // Browse button: [ + BROWSE ALL (10) ]
                float browseBtnW = 124.0f;
                float browseBtnX = hubX + hubW - browseBtnW - 24.0f;
                if (x >= browseBtnX && x <= browseBtnX + browseBtnW) {
                    res.action = ProjectHubAction::OpenThemeBrowser;
                    return res;
                }
            }

            // 4. Theme Preset Chips [contentStartY + 78.0f .. contentStartY + 104.0f]
            if (y >= contentStartY + 78.0f && y <= contentStartY + 104.0f) {
                int curBank = getThemeBank();
                for (int th = 0; th < 5; ++th) {
                    float tx = hubX + 24.0f + th * 95.0f;
                    if (x >= tx && x <= tx + 88.0f) {
                        res.action = ProjectHubAction::SelectTheme;
                        res.themeIndex = (curBank == 1) ? (5 + th) : th;
                        return res;
                    }
                }
            }
            if (y >= contentStartY + 116.0f && y <= contentStartY + 144.0f) {
                if (x >= hubX + 24.0f && x <= hubX + 124.0f) { res.action = ProjectHubAction::SetAntiAliasing; res.aaMode = 0; return res; }
                if (x >= hubX + 132.0f && x <= hubX + 232.0f) { res.action = ProjectHubAction::SetAntiAliasing; res.aaMode = 1; return res; }
                if (x >= hubX + 240.0f && x <= hubX + 350.0f) { res.action = ProjectHubAction::SetAntiAliasing; res.aaMode = 2; return res; }
            }
            if (y >= contentStartY + 154.0f && y <= contentStartY + 178.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ToggleHiDpi;
                return res;
            }
            if (y >= contentStartY + 180.0f && y <= contentStartY + 204.0f) {
                if (x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) { res.action = ProjectHubAction::ToggleCrtShader; return res; }
            }
            if (y >= contentStartY + 206.0f && y <= contentStartY + 230.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ToggleAnimations;
                return res;
            }
            if (y >= contentStartY + 232.0f && y <= contentStartY + 258.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ToggleMinimalGui;
                return res;
            }
        } else if (projectHubSection_ == 3) {
            // Check scrollbar hit within drawer
            if (projectHubScrollArea_.canScroll() && projectHubScrollArea_.getScrollbarTrackBounds().contains(x, y)) {
                res.action = ProjectHubAction::Scrollbar;
                return res;
            }

            float contentStartY = drawerY - projectHubScrollY_;

            // CRT Switch
            if (y >= contentStartY + 6.0f && y <= contentStartY + 30.0f) {
                if (x >= drawerX + drawerW - 60.0f && x <= drawerX + drawerW - 16.0f) {
                    res.action = ProjectHubAction::ToggleCrtShader;
                    return res;
                }
            }

            // Presets
            if (y >= contentStartY + 38.0f && y <= contentStartY + 64.0f) {
                if (x >= drawerX + 74.0f && x <= drawerX + 166.0f) { res.action = ProjectHubAction::CrtPresetStudioRef; return res; }
                if (x >= drawerX + 172.0f && x <= drawerX + 270.0f) { res.action = ProjectHubAction::CrtPresetMaxClarity; return res; }
                if (x >= drawerX + 276.0f && x <= drawerX + 378.0f) { res.action = ProjectHubAction::CrtPresetWarmVintage; return res; }
                if (x >= drawerX + 384.0f && x <= drawerX + 452.0f) { res.action = ProjectHubAction::CrtPresetReset; return res; }
            }

            // Sliders 0..10
            const float sTrackX = drawerX + 12.0f;
            const float sTrackW = drawerW - 32.0f;
            for (int i = 0; i < 11; ++i) {
                float rowY = contentStartY + 76.0f + i * 46.0f;
                if (y >= rowY + 6.0f && y <= rowY + 36.0f && x >= sTrackX - 4.0f && x <= sTrackX + sTrackW + 4.0f) {
                    res.action = ProjectHubAction::CrtSlider;
                    res.crtSliderIndex = i;
                    return res;
                }
            }

            res.action = ProjectHubAction::ContentDrag;
            return res;
        } else if (projectHubSection_ == 4) {
            float contentStartY = drawerY;
            float clapCardY = contentStartY + 86.0f;
            // Toggle switch at [hubX + hubW - 74, clapCardY + 12, 44, 20]
            if (x >= hubX + hubW - 80.0f && x <= hubX + hubW - 20.0f &&
                y >= clapCardY + 8.0f && y <= clapCardY + 36.0f) {
                res.action = ProjectHubAction::ToggleIncludeBinaryClapPath;
                return res;
            }
            // Rescan button at [hubX + hubW - 178, clapCardY + 51, 146, 24]
            if (x >= hubX + hubW - 186.0f && x <= hubX + hubW - 24.0f &&
                y >= clapCardY + 47.0f && y <= clapCardY + 77.0f) {
                res.action = ProjectHubAction::RescanClapPlugins;
                return res;
            }
            return res;
        }
    }
    return res;
}

HitTestEatscriptResult GuiWindow::hitTestEatscript(float x, float y) const noexcept {
    HitTestEatscriptResult res{};
    if (activeView_ != WorkspaceView::Design) return res;

    // 1. Top Sub-Navigation Tabs: [Y = 60..95]
    if (y >= 60.0f && y <= 95.0f) {
        // [ MODULAR RACK ] at [30, 64, 125, 28]
        if (x >= 30.0f && x <= 155.0f) {
            res.hit = true;
            res.action = HitTestEatscriptResult::Action::SubNavModular;
            return res;
        }
        // [ EATSCRIPT IDE ] at [165, 64, 125, 28]
        if (x >= 165.0f && x <= 290.0f) {
            res.hit = true;
            res.action = HitTestEatscriptResult::Action::SubNavEatscript;
            return res;
        }
        // [ GUI DESIGNER ] at [300, 64, 125, 28]
        if (x >= 300.0f && x <= 425.0f) {
            res.hit = true;
            res.action = HitTestEatscriptResult::Action::SubNavGuiDesigner;
            return res;
        }

        // When in Eatscript sub-view, check action toolbar and template buttons
        if (designSubView_ == DesignSubView::Eatscript) {
            // [ COMPILE (F5) ] at [445, 64, 115, 28]
            if (x >= 445.0f && x <= 560.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::Compile;
                return res;
            }
            // [ HOT-RELOAD ] at [570, 64, 135, 28]
            if (x >= 570.0f && x <= 705.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::HotReload;
                return res;
            }
            // [ AOT C++ ] at [715, 64, 95, 28]
            if (x >= 715.0f && x <= 810.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::ToggleAotView;
                return res;
            }

            // Template buttons at Y = 64..92
            // Template 0 [ ACID 303 ] at [825, 64, 80, 28]
            if (x >= 825.0f && x <= 905.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::LoadTemplate;
                res.templateIndex = 0;
                return res;
            }
            // Template 1 [ DUAL SAW ] at [912, 64, 80, 28]
            if (x >= 912.0f && x <= 992.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::LoadTemplate;
                res.templateIndex = 1;
                return res;
            }
            // Template 2 [ DELAY ] at [999, 64, 75, 28]
            if (x >= 999.0f && x <= 1074.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::LoadTemplate;
                res.templateIndex = 2;
                return res;
            }
            // Template 3 [ FM BELL ] at [1081, 64, 78, 28]
            if (x >= 1081.0f && x <= 1159.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::LoadTemplate;
                res.templateIndex = 3;
                return res;
            }
            // Template 4 [ DISTORT ] at [1166, 64, 78, 28]
            if (x >= 1166.0f && x <= 1244.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::LoadTemplate;
                res.templateIndex = 4;
                return res;
            }
        }
    }

    if (designSubView_ != DesignSubView::Eatscript) return res;

    // 2. Code Editor Canvas area: [X = 30..820, Y = 134..700]
    if (x >= 30.0f && x <= 820.0f && y >= 134.0f && y <= 700.0f) {
        res.hit = true;
        res.action = HitTestEatscriptResult::Action::EditorClick;
        int line = static_cast<int>((y - 138.0f) / 22.0f);
        int col = static_cast<int>((x - 90.0f) / 8.5f);
        if (col < 0) col = 0;
        res.line = line;
        res.col = col;
        return res;
    }

    // 3. Right Panel Bind Track Button: [850, 260, 220, 32]
    if (x >= 850.0f && x <= 1070.0f && y >= 260.0f && y <= 292.0f) {
        res.hit = true;
        res.action = HitTestEatscriptResult::Action::BindTrack;
        return res;
    }

    return res;
}

void GuiWindow::setEditSubView(EditSubView subView) noexcept {
    editSubView_ = subView;
    if (modularEditView_) {
        switch (subView) {
            case EditSubView::PianoRoll: modularEditView_->setSubView(EditSubViewMode::PianoRoll); break;
            case EditSubView::Tracker:   modularEditView_->setSubView(EditSubViewMode::Tracker); break;
            case EditSubView::Score:     modularEditView_->setSubView(EditSubViewMode::Score); break;
            case EditSubView::Script:    modularEditView_->setSubView(EditSubViewMode::Script); break;
        }
    }
}

void GuiWindow::setActiveView(WorkspaceView view) noexcept {
    if (activeView_ != view) {
        invalidateChromeCache();
    }
    activeView_ = view;
    if (view == WorkspaceView::Design) {
        syncDesignViewToProject();
    } else if (view == WorkspaceView::Track && modularTrackInspectorView_) {
        if (modularArrangerView_) {
            selectedTrackIndex_ = modularArrangerView_->getActiveTrackIndex();
            modularArrangerView_->syncDrawerDataForTrack(selectedTrackIndex_);
            modularTrackInspectorView_->syncFromDrawerData(modularArrangerView_->getDrawerData());
        }
        float topY = 56.0f;
        float bottomY = static_cast<float>(height_) - 32.0f;
        ViewContext ctx = createViewContext();
        modularTrackInspectorView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
    }
}

void GuiWindow::syncDesignViewToProject() noexcept {
    if (modularDesignView_ && modularArrangerView_) {
        modularDesignView_->syncWithProject(modularArrangerView_->getTracks());
    }
}

void GuiWindow::setDesignSubView(DesignSubView subView) {
    designSubView_ = subView;
    if (modularDesignView_) {
        syncDesignViewToProject();
        if (subView == DesignSubView::ModularRack) {
            modularDesignView_->setSubMode(DesignSubMode::ModularRack);
        } else if (subView == DesignSubView::Eatscript) {
            modularDesignView_->setSubMode(DesignSubMode::Code);
        } else if (subView == DesignSubView::GuiDesigner) {
            modularDesignView_->setSubMode(DesignSubMode::GuiDesigner);
        }
    }
    if (subView == DesignSubView::Eatscript) {
        if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
            auto* trk = engine_->getSequencer().getTrack(selectedTrackIndex_);
            if (trk && !trk->getEatscriptCode().empty()) {
                setScriptCode(trk->getEatscriptCode());
            } else if (activePresetIndex_ < presets_.size() && !presets_[activePresetIndex_].rawScript.empty()) {
                setScriptCode(presets_[activePresetIndex_].rawScript);
            }
        } else if (activePresetIndex_ < presets_.size() && !presets_[activePresetIndex_].rawScript.empty()) {
            setScriptCode(presets_[activePresetIndex_].rawScript);
        }
    }
}

void GuiWindow::initEatscriptIde() {
    scriptTemplates_.clear();

    scriptTemplates_.push_back({"ACID 303",
        "# @engine: tb303\n"
        "# ACID 303 Voice\n"
        "def init():\n"
        "    return {\n"
        "        \"Cutoff\": 1500.0,\n"
        "        \"Resonance\": 0.75\n"
        "    }\n"
        "\n"
        "def process(time, freq, note, params):\n"
        "    # Roland TB-303 Acid Bassline Oscillator\n"
        "    val = math.sin(2.0 * math.pi * freq * time)\n"
        "    return math.tanh(val * 2.5) * 0.75\n"
    });

    scriptTemplates_.push_back({"DUAL SAW",
        "# DUAL SAW Voice with detune\n"
        "def init():\n"
        "    return {\n"
        "        \"Detune\": 1.008,\n"
        "        \"Gain\": 0.70\n"
        "    }\n"
        "\n"
        "def process(time, freq, note, params):\n"
        "    # Stereo Detuned Saw Voice\n"
        "    s1 = math.sin(2.0 * math.pi * freq * time)\n"
        "    s2 = math.sin(2.0 * math.pi * freq * 1.008 * time)\n"
        "    return (s1 + s2) * 0.4\n"
    });

    scriptTemplates_.push_back({"DELAY FX",
        "# DELAY FX with feedback\n"
        "def init():\n"
        "    return {\n"
        "        \"Feedback\": 0.45,\n"
        "        \"Mix\": 0.35\n"
        "    }\n"
        "\n"
        "def process(time, freq, note, params):\n"
        "    # Modulated Echo Effect\n"
        "    carrier = math.sin(2.0 * math.pi * freq * time)\n"
        "    echo = math.sin(2.0 * math.pi * freq * (time - 0.05))\n"
        "    return carrier * 0.6 + echo * 0.25\n"
    });

    scriptTemplates_.push_back({"FM BELL",
        "# FM BELL Voice with mod\n"
        "def init():\n"
        "    return {\n"
        "        \"Ratio\": 2.76,\n"
        "        \"Depth\": 3.50\n"
        "    }\n"
        "\n"
        "def process(time, freq, note, params):\n"
        "    # 2-Operator FM Synthesis\n"
        "    mod = math.sin(2.0 * math.pi * freq * 2.76 * time) * 3.5\n"
        "    car = math.sin(2.0 * math.pi * freq * time + mod)\n"
        "    return car * 0.7\n"
    });

    scriptTemplates_.push_back({"DISTORT",
        "# DISTORT Waveshaping Saturation with tanh\n"
        "def init():\n"
        "    return {\n"
        "        \"Drive\": 4.0,\n"
        "        \"Ceiling\": 0.8\n"
        "    }\n"
        "\n"
        "def process(time, freq, note, params):\n"
        "    # Non-linear Waveshaping Saturation\n"
        "    raw = math.sin(2.0 * math.pi * freq * time)\n"
        "    return math.tanh(raw * 4.0) * 0.8\n"
    });

    loadScriptTemplate(0);
}

std::string GuiWindow::getScriptCode() const {
    std::string code;
    for (size_t i = 0; i < scriptBuffer_.size(); ++i) {
        code += scriptBuffer_[i];
        if (i + 1 < scriptBuffer_.size()) {
            code += "\n";
        }
    }
    return code;
}

void GuiWindow::setScriptCode(const std::string& code) {
    scriptBuffer_.clear();
    std::stringstream ss(code);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        scriptBuffer_.push_back(line);
    }
    if (scriptBuffer_.empty()) {
        scriptBuffer_.push_back("");
    }
    scriptCursorLine_ = 0;
    scriptCursorCol_ = 0;
    compileActiveScript();
}

void GuiWindow::setScriptCursor(int line, int col) noexcept {
    if (scriptBuffer_.empty()) {
        scriptBuffer_.push_back("");
    }
    if (line < 0) line = 0;
    if (line >= static_cast<int>(scriptBuffer_.size())) {
        line = static_cast<int>(scriptBuffer_.size()) - 1;
    }
    scriptCursorLine_ = line;
    int lineLen = static_cast<int>(scriptBuffer_[line].size());
    if (col < 0) col = 0;
    if (col > lineLen) col = lineLen;
    scriptCursorCol_ = col;
}

void GuiWindow::insertScriptChar(char c) {
    if (scriptBuffer_.empty()) scriptBuffer_.push_back("");
    if (scriptCursorLine_ >= static_cast<int>(scriptBuffer_.size())) {
        scriptCursorLine_ = static_cast<int>(scriptBuffer_.size()) - 1;
    }
    auto& line = scriptBuffer_[scriptCursorLine_];
    if (scriptCursorCol_ > static_cast<int>(line.size())) {
        scriptCursorCol_ = static_cast<int>(line.size());
    }
    line.insert(line.begin() + scriptCursorCol_, c);
    scriptCursorCol_++;
}

void GuiWindow::insertScriptText(const std::string& text) {
    for (char c : text) {
        if (c == '\n') {
            insertScriptNewLine();
        } else if (c != '\r') {
            insertScriptChar(c);
        }
    }
}

void GuiWindow::deleteScriptCharBackwards() {
    if (scriptBuffer_.empty()) return;
    if (scriptCursorLine_ >= static_cast<int>(scriptBuffer_.size())) {
        scriptCursorLine_ = static_cast<int>(scriptBuffer_.size()) - 1;
    }
    auto& line = scriptBuffer_[scriptCursorLine_];
    if (scriptCursorCol_ > 0) {
        if (scriptCursorCol_ <= static_cast<int>(line.size())) {
            line.erase(line.begin() + scriptCursorCol_ - 1);
            scriptCursorCol_--;
        }
    } else if (scriptCursorLine_ > 0) {
        int prevLineIdx = scriptCursorLine_ - 1;
        int prevLineLen = static_cast<int>(scriptBuffer_[prevLineIdx].size());
        scriptBuffer_[prevLineIdx] += line;
        scriptBuffer_.erase(scriptBuffer_.begin() + scriptCursorLine_);
        scriptCursorLine_ = prevLineIdx;
        scriptCursorCol_ = prevLineLen;
    }
}

void GuiWindow::deleteScriptCharForwards() {
    if (scriptBuffer_.empty()) return;
    if (scriptCursorLine_ >= static_cast<int>(scriptBuffer_.size())) {
        scriptCursorLine_ = static_cast<int>(scriptBuffer_.size()) - 1;
    }
    auto& line = scriptBuffer_[scriptCursorLine_];
    if (scriptCursorCol_ < static_cast<int>(line.size())) {
        line.erase(line.begin() + scriptCursorCol_);
    } else if (scriptCursorLine_ + 1 < static_cast<int>(scriptBuffer_.size())) {
        line += scriptBuffer_[scriptCursorLine_ + 1];
        scriptBuffer_.erase(scriptBuffer_.begin() + scriptCursorLine_ + 1);
    }
}

void GuiWindow::insertScriptNewLine() {
    if (scriptBuffer_.empty()) scriptBuffer_.push_back("");
    if (scriptCursorLine_ >= static_cast<int>(scriptBuffer_.size())) {
        scriptCursorLine_ = static_cast<int>(scriptBuffer_.size()) - 1;
    }
    auto& curLine = scriptBuffer_[scriptCursorLine_];
    if (scriptCursorCol_ > static_cast<int>(curLine.size())) {
        scriptCursorCol_ = static_cast<int>(curLine.size());
    }

    std::string leftPart = curLine.substr(0, scriptCursorCol_);
    std::string remaining = curLine.substr(scriptCursorCol_);

    std::string indent;
    for (char ch : leftPart) {
        if (ch == ' ' || ch == '\t') indent += ch;
        else break;
    }
    std::string trimmed = leftPart;
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) trimmed.pop_back();
    if (!trimmed.empty() && trimmed.back() == ':') {
        indent += "    ";
    }

    curLine = leftPart;
    scriptBuffer_.insert(scriptBuffer_.begin() + scriptCursorLine_ + 1, indent + remaining);
    scriptCursorLine_++;
    scriptCursorCol_ = static_cast<int>(indent.size());
}

void GuiWindow::loadScriptTemplate(size_t index) {
    if (index < scriptTemplates_.size()) {
        setScriptCode(scriptTemplates_[index].second);
        setStatusMessage("LOADED TEMPLATE: " + scriptTemplates_[index].first);
    }
}
std::string GuiWindow::getScriptTemplateName(size_t index) const {
    if (index < scriptTemplates_.size()) {
        return scriptTemplates_[index].first;
    }
    return "";
}

bool GuiWindow::compileActiveScript() {
    std::cout << "[compileActiveScript] Starting..." << std::endl;
    std::string src = getScriptCode();
    scriptDisassembly_.clear();
    scriptError_.clear();
    scriptErrorLine_ = -1;

    auto nativeTarget = eatscript::NativeDispatchScanner::detectTarget(src);
    if (nativeTarget != eatscript::NativeDispatchTarget::None) {
        scriptCompiled_ = true;
        scriptDisassembly_.push_back(std::string("0000: NATIVE_SIMD_DISPATCH ") + eatscript::NativeDispatchScanner::targetToString(nativeTarget));
        auto extracted = eatscript::NativeDispatchScanner::extractParamsFromInit(src);
        for (const auto& [k, v] : extracted) {
            scriptDisassembly_.push_back(std::string("PARAM: ") + k + " = " + std::to_string(v));
        }
        eatscript::Transpiler transpiler;
        scriptTranspiledCode_ = transpiler.transpileSource(src, "eatscript_dsp", "Live Eatscript Voice");
        scriptStatusMsg_ = std::string("Native DSP Dispatched (") + eatscript::NativeDispatchScanner::targetToString(nativeTarget) + ")";
        return true;
    }

    try {
        std::cout << "[compileActiveScript] Parsing..." << std::endl;
        eatscript::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        eatscript::Parser parser(std::move(tokens));
        activeProgram_ = parser.parse();
        if (!activeProgram_) {
            scriptCompiled_ = false;
            scriptError_ = "Parser returned empty program";
            scriptErrorLine_ = 1;
            scriptStatusMsg_ = "Parse Error";
            return false;
        }

        std::cout << "[compileActiveScript] Compiling VM..." << std::endl;
        eatscript::VM vm;
        if (!vm.compileProgram(*activeProgram_)) {
            scriptCompiled_ = false;
            scriptError_ = "Failed to compile bytecode chunk";
            scriptErrorLine_ = 1;
            scriptStatusMsg_ = "Bytecode Error";
            return false;
        }

        std::cout << "[compileActiveScript] Disassembling bytecode..." << std::endl;
        const auto& chunk = vm.getProcessChunk();
        size_t ip = 0;
        int instNum = 0;
        while (ip < chunk.code.size()) {
            uint8_t opByte = chunk.code[ip++];
            eatscript::OpCode op = static_cast<eatscript::OpCode>(opByte);
            char lineBuf[96];
            std::string opName;
            std::string detail;

            switch (op) {
                case eatscript::OpCode::Constant: {
                    uint8_t cIdx = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    double val = (cIdx < chunk.constants.size()) ? chunk.constants[cIdx] : 0.0;
                    opName = "OP_CONSTANT";
                    detail = std::to_string(val);
                    break;
                }
                case eatscript::OpCode::GetLocal: {
                    uint8_t slot = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    opName = "OP_GETLOCAL";
                    detail = "slot " + std::to_string(slot);
                    break;
                }
                case eatscript::OpCode::SetLocal: {
                    uint8_t slot = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    opName = "OP_SETLOCAL";
                    detail = "slot " + std::to_string(slot);
                    break;
                }
                case eatscript::OpCode::GetParam: {
                    uint8_t p = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    opName = "OP_GETPARAM";
                    detail = "param " + std::to_string(p);
                    break;
                }
                case eatscript::OpCode::Add: opName = "OP_ADD"; break;
                case eatscript::OpCode::Subtract: opName = "OP_SUBTRACT"; break;
                case eatscript::OpCode::Multiply: opName = "OP_MULTIPLY"; break;
                case eatscript::OpCode::Divide: opName = "OP_DIVIDE"; break;
                case eatscript::OpCode::Modulo: opName = "OP_MODULO"; break;
                case eatscript::OpCode::Power: opName = "OP_POWER"; break;
                case eatscript::OpCode::Negate: opName = "OP_NEGATE"; break;
                case eatscript::OpCode::Equal: opName = "OP_EQUAL"; break;
                case eatscript::OpCode::NotEqual: opName = "OP_NOTEQUAL"; break;
                case eatscript::OpCode::Less: opName = "OP_LESS"; break;
                case eatscript::OpCode::LessEqual: opName = "OP_LESSEQUAL"; break;
                case eatscript::OpCode::Greater: opName = "OP_GREATER"; break;
                case eatscript::OpCode::GreaterEqual: opName = "OP_GREATEREQUAL"; break;
                case eatscript::OpCode::MathSin: opName = "OP_MATHSIN"; break;
                case eatscript::OpCode::MathCos: opName = "OP_MATHCOS"; break;
                case eatscript::OpCode::MathTanh: opName = "OP_MATHTANH"; break;
                case eatscript::OpCode::MathExp: opName = "OP_MATHEXP"; break;
                case eatscript::OpCode::MathFloor: opName = "OP_MATHFLOOR"; break;
                case eatscript::OpCode::MathSqrt: opName = "OP_MATHSQRT"; break;
                case eatscript::OpCode::MathRandom: opName = "OP_MATHRANDOM"; break;
                case eatscript::OpCode::Jump: {
                    uint8_t hi = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    uint8_t lo = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    opName = "OP_JUMP";
                    detail = "offset " + std::to_string((hi << 8) | lo);
                    break;
                }
                case eatscript::OpCode::JumpIfFalse: {
                    uint8_t hi = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    uint8_t lo = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    opName = "OP_JUMPIFFALSE";
                    detail = "offset " + std::to_string((hi << 8) | lo);
                    break;
                }
                case eatscript::OpCode::Return: opName = "OP_RETURN"; break;
                default: opName = "OP_UNKNOWN (" + std::to_string(opByte) + ")"; break;
            }

            std::snprintf(lineBuf, sizeof(lineBuf), "%04d: %-15s %s", instNum++, opName.c_str(), detail.c_str());
            scriptDisassembly_.push_back(lineBuf);
        }

        std::cout << "[compileActiveScript] Transpiling..." << std::endl;
        eatscript::Transpiler transpiler;
        scriptTranspiledCode_ = transpiler.transpile(*activeProgram_, "eatscript_dsp", "Live Eatscript Voice");
        std::cout << "[compileActiveScript] Transpiled successfully!" << std::endl;

        scriptCompiled_ = true;
        scriptStatusMsg_ = "Compiled Successfully (" + std::to_string(scriptDisassembly_.size()) + " instructions)";
        std::cout << "[compileActiveScript] Done!" << std::endl;
        return true;
    } catch (const std::exception& e) {
        scriptCompiled_ = false;
        scriptError_ = e.what();
        scriptErrorLine_ = 1;
        scriptStatusMsg_ = "Error: " + scriptError_;
        return false;
    } catch (...) {
        scriptCompiled_ = false;
        scriptError_ = "Unknown compilation error";
        scriptErrorLine_ = 1;
        scriptStatusMsg_ = "Compilation Error";
        return false;
    }
}

bool GuiWindow::hotReloadScriptToTrack(uint32_t trackIndex) {
    if (!compileActiveScript()) {
        return false;
    }
    if (!engine_) return false;

    std::string code = getScriptCode();
    auto& seq = engine_->getSequencer();
    auto* trk = (trackIndex < seq.getNumTracks()) ? seq.getTrack(trackIndex) : nullptr;
    if (trk) {
        trk->setEatscriptCode(code);
    }

    auto dispatchTarget = eatscript::NativeDispatchScanner::detectTarget(code);
    if (dispatchTarget != eatscript::NativeDispatchTarget::None) {
        auto extracted = eatscript::NativeDispatchScanner::extractParamsFromInit(code);
        if (activePresetIndex_ < presets_.size()) {
            auto& p = presets_[activePresetIndex_];
            for (const auto& [k, v] : extracted) {
                if (auto* param = p.findParam(k)) {
                    param->currentVal = v;
                }
            }
        }
        scriptTargetTrack_ = trackIndex;
        if (trk) {
            trk->setName("EATSCRIPT");
        }
        setStatusMessage(std::string("HOT-RELOADED NATIVE DSP (") + eatscript::NativeDispatchScanner::targetToString(dispatchTarget) + ") TO TRACK 0" + std::to_string(trackIndex + 1));
        return true;
    }

    auto& graph = engine_->getGraph();

    std::shared_ptr<audio::EatscriptNode> targetNode;
    const auto& nodes = graph.getNodes();
    for (const auto& [id, node] : nodes) {
        if (auto es = std::dynamic_pointer_cast<audio::EatscriptNode>(node)) {
            targetNode = es;
            break;
        }
    }

    if (!targetNode) {
        targetNode = std::make_shared<audio::EatscriptNode>("LiveEatscript");
        audio::NodeId esId = graph.addNode(targetNode);
        audio::NodeId outId = graph.getOutputNodeId();
        if (outId != 0 && outId != esId) {
            graph.connect(esId, 0, outId, 0);
        }
        graph.compile();
    }

    targetNode->setScript(code);

    if (trk) {
        trk->setName("EATSCRIPT");
    }

    scriptTargetTrack_ = trackIndex;
    setStatusMessage("HOT-RELOADED EATSCRIPT INTO TRACK 0" + std::to_string(trackIndex + 1));
    return true;
}

void GuiWindow::initNoteScriptEditor() {
    noteScriptBuffer_.clear();
    noteScriptBuffer_.push_back("# Clip Notes (Eatscript Declarative Event Notepad)");
    noteScriptBuffer_.push_back("# Syntax: { pitch = \"C3\", step = 0, dur = 0.75, vel = 0.80, slide = false, accent = false }");
    noteScriptBuffer_.push_back("");
    noteScriptBuffer_.push_back("{ pitch = \"C3\", step = 0, dur = 0.75, vel = 0.85 }");
    noteScriptBuffer_.push_back("{ pitch = \"D#3\", step = 3, dur = 0.75, vel = 0.90, slide = true, accent = true }");
    noteScriptBuffer_.push_back("{ pitch = \"G3\", step = 7, dur = 0.75, vel = 0.80 }");
    noteScriptBuffer_.push_back("{ pitch = \"A#3\", step = 10, dur = 0.75, vel = 0.95, slide = true }");
    noteScriptCursorLine_ = 3;
    noteScriptCursorCol_ = 0;
    noteScriptScrollY_ = 0.0f;
    noteScriptDirty_ = false;
    noteScriptStatusMsg_ = "Ready";
    noteScriptError_ = "";
    noteScriptErrorLine_ = -1;
    noteScriptLastTrack_ = 9999;
}

void GuiWindow::initArrangerTracks() {
    arrangerTracks_.clear();

    // Track 0: DXVII E-Piano (Single default track)
    ArrangerTrackData t0;
    t0.name = "DXVII E-Piano";
    t0.instrument = "DXVII E-Piano";
    t0.instrumentEngine = "dxvii_epiano";
    t0.type = "SYNTH";
    t0.r = 0.62f; t0.g = 0.31f; t0.b = 0.87f;
    t0.volume = 0.75f;
    t0.eqLow = 0.40f; t0.eqMid = 0.60f; t0.eqHigh = 0.70f;
    t0.chordFollowMode = theory::ChordFollowMode::Chord;

    ArrangerClip c1;
    c1.name = "FM Chords";
    c1.startBar = 1;
    c1.barLength = 8;
    c1.isLooped = false;
    c1.loopLengthBars = 8;
    c1.r = 0.62f; c1.g = 0.31f; c1.b = 0.87f;
    t0.clips.push_back(c1);

    arrangerTracks_.push_back(t0);
}

void GuiWindow::setArrangerPropertiesWidth(float width) noexcept {
    arrangerPropertiesWidth_ = std::clamp(width, kArrangerPropertiesMinW, kArrangerPropertiesMaxW);
}

void GuiWindow::setMixerPropertiesExpanded(bool expanded) noexcept {
    mixerPropertiesExpanded_ = expanded;
    if (modularMixerView_) {
        modularMixerView_->getPropertiesDrawer().setExpanded(expanded);
    }
}

void GuiWindow::toggleMixerProperties() noexcept {
    setMixerPropertiesExpanded(!mixerPropertiesExpanded_);
}

void GuiWindow::setMixerPropertiesWidth(float width) noexcept {
    mixerPropertiesWidth_ = std::clamp(width, kMixerPropertiesMinW, kMixerPropertiesMaxW);
    if (modularMixerView_) {
        modularMixerView_->getPropertiesDrawer().setWidth(mixerPropertiesWidth_);
    }
}

void GuiWindow::handleTrackInspectorInteraction(const HitTestTrackInspectorResult& inspHit, float x, float y) {
    uint32_t tIdx = (selectedTrackIndex_ < arrangerTracks_.size()) ? selectedTrackIndex_ : 0;
    auto& trk = arrangerTracks_[tIdx];
    switch (inspHit.area) {
        case TrackInspectorHitArea::ScrollbarThumb:
        case TrackInspectorHitArea::ScrollbarTrack:
            dragMode_ = DragMode::TrackInspectorScrollbar;
            dragStartY_ = y;
            dragStartScrollY_ = trackInspectorScrollY_;
            break;
        case TrackInspectorHitArea::PresetPrev:
            prevPreset();
            break;
        case TrackInspectorHitArea::PresetNext:
            nextPreset();
            break;
        case TrackInspectorHitArea::FullscreenDevice:
            openFullscreenDevice(inspHit.trackIndex);
            break;
        case TrackInspectorHitArea::TrackTab:
            syncTrackToPreset(inspHit.trackIndex);
            activeView_ = WorkspaceView::Track;
            break;
        case TrackInspectorHitArea::CodeButton:
            activeView_ = WorkspaceView::Design;
            designSubView_ = DesignSubView::Eatscript;
            break;
        case TrackInspectorHitArea::MuteButton:
            trk.mute = !trk.mute;
            if (tIdx < mixerStrips_.size()) mixerStrips_[tIdx].mute = trk.mute;
            if (engine_ && tIdx < engine_->getSequencer().getNumTracks()) {
                auto* tr = engine_->getSequencer().getTrack(tIdx);
                if (tr) tr->setMuted(trk.mute);
            }
            recordProjectHistory("Toggle Mute on Track " + std::to_string(tIdx + 1), "TRACK");
            break;
        case TrackInspectorHitArea::SoloButton:
            trk.solo = !trk.solo;
            if (tIdx < mixerStrips_.size()) mixerStrips_[tIdx].solo = trk.solo;
            if (engine_ && tIdx < engine_->getSequencer().getNumTracks()) {
                auto* tr = engine_->getSequencer().getTrack(tIdx);
                if (tr) tr->setSolo(trk.solo);
            }
            recordProjectHistory("Toggle Solo on Track " + std::to_string(tIdx + 1), "TRACK");
            break;
        case TrackInspectorHitArea::FreezeButton:
            setTrackFreezeState(tIdx, !trk.freeze);
            break;
        case TrackInspectorHitArea::VolumeSlider: {
            if (isCtrlPressed()) {
                if (tIdx < arrangerTracks_.size()) arrangerTracks_[tIdx].volume = 1.0f;
                if (tIdx < mixerStrips_.size()) mixerStrips_[tIdx].volume = 1.0f;
                if (engine_) engine_->setTrackVolume(tIdx, 1.0f);
                break;
            }
            dragMode_ = DragMode::TrackInspectorVolume;
            dragStartX_ = x;
            dragStartValGeneric_ = trk.volume;
            presenter::ScalarDragConfig cfg{};
            cfg.dragMode = DragMode::TrackInspectorVolume;
            cfg.direction = presenter::DragDirection::HorizontalRightIncreases;
            cfg.minValue = 0.0f;
            cfg.maxValue = 1.5f;
            cfg.defaultValue = 1.0f;
            cfg.sensitivity = 1.5f / 220.0f;
            cfg.relativeMode = true;
            cfg.captureMouse = true;
            scalarDragPresenter_.startDrag(x, y, trk.volume, cfg,
                [this, t = tIdx](float v) {
                    if (t < arrangerTracks_.size()) arrangerTracks_[t].volume = v;
                    if (t < mixerStrips_.size()) mixerStrips_[t].volume = v;
                    if (engine_) engine_->setTrackVolume(t, v);
                });
            activeDragHandler_ = &scalarDragPresenter_;
            break;
        }
        case TrackInspectorHitArea::PanKnob: {
            if (isCtrlPressed()) {
                if (tIdx < arrangerTracks_.size()) arrangerTracks_[tIdx].pan = 0.0f;
                if (tIdx < mixerStrips_.size()) mixerStrips_[tIdx].pan = 0.0f;
                if (engine_) engine_->setTrackPan(tIdx, 0.0f);
                break;
            }
            dragMode_ = DragMode::TrackInspectorPan;
            dragStartX_ = x;
            dragStartPan_ = trk.pan;
            presenter::ScalarDragConfig cfg{};
            cfg.dragMode = DragMode::TrackInspectorPan;
            cfg.direction = presenter::DragDirection::HorizontalRightIncreases;
            cfg.minValue = -1.0f;
            cfg.maxValue = 1.0f;
            cfg.defaultValue = 0.0f;
            cfg.sensitivity = 2.0f / 180.0f;
            cfg.relativeMode = true;
            cfg.captureMouse = true;
            scalarDragPresenter_.startDrag(x, y, trk.pan, cfg,
                [this, t = tIdx](float p) {
                    if (t < arrangerTracks_.size()) arrangerTracks_[t].pan = p;
                    if (t < mixerStrips_.size()) mixerStrips_[t].pan = p;
                    if (engine_) engine_->setTrackPan(t, p);
                });
            activeDragHandler_ = &scalarDragPresenter_;
            break;
        }
        case TrackInspectorHitArea::ChordFollowChip:
            trk.chordFollowMode = inspHit.chordMode;
            if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
                modularArrangerView_->getTracks()[tIdx].chordFollowMode = inspHit.chordMode;
                modularArrangerView_->syncDrawerDataForTrack(tIdx);
            }
            if (modularMixerView_ && tIdx < modularMixerView_->getChannels().size()) {
                modularMixerView_->getChannels()[tIdx].chordFollowMode = inspHit.chordMode;
            }
            if (modularTrackInspectorView_ && tIdx < modularTrackInspectorView_->getTracks().size()) {
                modularTrackInspectorView_->getTracks()[tIdx].chordFollowMode = inspHit.chordMode;
            }
            if (modularEditView_ && selectedTrackIndex_ == tIdx) {
                modularEditView_->setChordFollowMode(inspHit.chordMode);
                if (modularArrangerView_) {
                    modularEditView_->setChordTrack(modularArrangerView_->getChordTrack());
                }
            }
            if (engine_) {
                auto& seq = engine_->getSequencer();
                if (auto* trkPtr = seq.getTrack(tIdx)) {
                    trkPtr->setChordFollowMode(inspHit.chordMode);
                }
            }
            recordProjectHistory("Set Chord Follow on Track " + std::to_string(tIdx + 1), "CHORD");
            break;
        case TrackInspectorHitArea::BakeChordsButton:
            bakeTrackChordsAndFx(tIdx);
            break;
        case TrackInspectorHitArea::MidiFxArpToggle:
            trk.midiFx.arpEnabled = !trk.midiFx.arpEnabled;
            recordProjectHistory("Toggle Arp on Track " + std::to_string(tIdx + 1), "MIDI_FX");
            if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
                auto& fxList = modularArrangerView_->getTracks()[tIdx].midiFx;
                bool found = false;
                for (auto& item : fxList) {
                    std::string itemType = item.type;
                    for (char& c : itemType) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (itemType.find("arp") != std::string::npos) {
                        item.enabled = trk.midiFx.arpEnabled;
                        found = true;
                        break;
                    }
                }
                if (!found && trk.midiFx.arpEnabled) {
                    TrackMidiFxItem arp("Arpeggiator", "ARPEGGIATOR", true);
                    arp.knobs = {
                        {"rate", "RATE", trk.midiFx.arpRate, "1/16", ""},
                        {"pattern", "PATTERN", static_cast<float>(trk.midiFx.arpPattern) / 4.0f, "UP", ""},
                        {"octaves", "OCTAVES", static_cast<float>(trk.midiFx.arpOctaves - 1) / 3.0f, std::to_string(trk.midiFx.arpOctaves), "oct"},
                        {"gate", "GATE", trk.midiFx.arpGate, "75%", ""},
                        {"swing", "SWING", trk.midiFx.arpSwing, "0%", "%"}
                    };
                    fxList.push_back(arp);
                }
            }
            if (modularTrackInspectorView_ && tIdx < modularTrackInspectorView_->getTracks().size()) {
                if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
                    modularTrackInspectorView_->getTracks()[tIdx].midiFxList = modularArrangerView_->getTracks()[tIdx].midiFx;
                }
            }
            syncTrackMidiFxToEngine(tIdx);
            break;
        case TrackInspectorHitArea::MidiFxScaleToggle:
            trk.midiFx.scaleSnapEnabled = !trk.midiFx.scaleSnapEnabled;
            recordProjectHistory("Toggle Scale Snap on Track " + std::to_string(tIdx + 1), "MIDI_FX");
            if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
                auto& fxList = modularArrangerView_->getTracks()[tIdx].midiFx;
                bool found = false;
                for (auto& item : fxList) {
                    std::string itemType = item.type;
                    for (char& c : itemType) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (itemType.find("scale") != std::string::npos) {
                        item.enabled = trk.midiFx.scaleSnapEnabled;
                        found = true;
                        break;
                    }
                }
                if (!found && trk.midiFx.scaleSnapEnabled) {
                    TrackMidiFxItem snap("Scale Snap", "SCALE_SNAP", trk.midiFx.scaleRoot, trk.midiFx.scaleMinor ? 1 : 0, true);
                    fxList.push_back(snap);
                }
            }
            if (modularTrackInspectorView_ && tIdx < modularTrackInspectorView_->getTracks().size()) {
                if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
                    modularTrackInspectorView_->getTracks()[tIdx].midiFxList = modularArrangerView_->getTracks()[tIdx].midiFx;
                }
            }
            syncTrackMidiFxToEngine(tIdx);
            break;
        case TrackInspectorHitArea::MidiFxHumanizeToggle:
            trk.midiFx.humanizeEnabled = !trk.midiFx.humanizeEnabled;
            recordProjectHistory("Toggle Humanize on Track " + std::to_string(tIdx + 1), "MIDI_FX");
            if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
                auto& fxList = modularArrangerView_->getTracks()[tIdx].midiFx;
                bool found = false;
                for (auto& item : fxList) {
                    std::string itemType = item.type;
                    for (char& c : itemType) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (itemType.find("humanize") != std::string::npos) {
                        item.enabled = trk.midiFx.humanizeEnabled;
                        found = true;
                        break;
                    }
                }
                if (!found && trk.midiFx.humanizeEnabled) {
                    TrackMidiFxItem hum("Humanize", "HUMANIZE", true);
                    hum.knobs = {
                        {"timing", "TIMING", trk.midiFx.humanizeTiming, "15 ms", "ms"},
                        {"velocity", "VELOCITY", trk.midiFx.humanizeVelocity, "20%", "%"},
                        {"swing", "SWING", 0.50f, "50%", "%"}
                    };
                    fxList.push_back(hum);
                }
            }
            if (modularTrackInspectorView_ && tIdx < modularTrackInspectorView_->getTracks().size()) {
                if (modularArrangerView_ && tIdx < modularArrangerView_->getTracks().size()) {
                    modularTrackInspectorView_->getTracks()[tIdx].midiFxList = modularArrangerView_->getTracks()[tIdx].midiFx;
                }
            }
            syncTrackMidiFxToEngine(tIdx);
            break;
        case TrackInspectorHitArea::AudioFxDelayToggle:
            trk.audioFx.delayEnabled = !trk.audioFx.delayEnabled;
            recordProjectHistory("Toggle Delay on Track " + std::to_string(tIdx + 1), "AUDIO_FX");
            break;
        case TrackInspectorHitArea::AudioFxChorusToggle:
            trk.audioFx.chorusEnabled = !trk.audioFx.chorusEnabled;
            recordProjectHistory("Toggle Chorus on Track " + std::to_string(tIdx + 1), "AUDIO_FX");
            break;
        case TrackInspectorHitArea::AudioFxEqToggle:
            trk.audioFx.eqEnabled = !trk.audioFx.eqEnabled;
            recordProjectHistory("Toggle EQ on Track " + std::to_string(tIdx + 1), "AUDIO_FX");
            break;
        case TrackInspectorHitArea::AudioFxCompToggle:
            trk.audioFx.compEnabled = !trk.audioFx.compEnabled;
            recordProjectHistory("Toggle Compressor on Track " + std::to_string(tIdx + 1), "AUDIO_FX");
            break;
        case TrackInspectorHitArea::AudioFxConvolverToggle:
            trk.audioFx.convolverEnabled = !trk.audioFx.convolverEnabled;
            recordProjectHistory("Toggle Convolver on Track " + std::to_string(tIdx + 1), "AUDIO_FX");
            break;
        case TrackInspectorHitArea::MidiFxArpKnob:
        case TrackInspectorHitArea::MidiFxScaleKnob:
        case TrackInspectorHitArea::MidiFxHumanizeKnob:
        case TrackInspectorHitArea::AudioFxDelayKnob:
        case TrackInspectorHitArea::AudioFxChorusKnob:
        case TrackInspectorHitArea::AudioFxEqKnob:
        case TrackInspectorHitArea::AudioFxCompKnob:
        case TrackInspectorHitArea::AudioFxConvolverKnob: {
            dragMode_ = DragMode::TrackInspectorFxKnob;
            activeTrackInspectorFxParam_ = inspHit.paramName;
            dragStartY_ = y;
            dragStartTrackInspectorVal_ = inspHit.normVal;
            presenter::ScalarDragConfig cfg{};
            cfg.dragMode = DragMode::TrackInspectorFxKnob;
            cfg.direction = presenter::DragDirection::VerticalUpIncreases;
            cfg.minValue = 0.0f;
            cfg.maxValue = 1.0f;
            cfg.sensitivity = 1.0f / 140.0f;
            scalarDragPresenter_.startDrag(x, y, inspHit.normVal, cfg,
                [this, t = tIdx, param = inspHit.paramName](float newVal) {
                    if (t >= arrangerTracks_.size()) return;
                    auto& tr = arrangerTracks_[t];
                    if (param == "delayTime") tr.audioFx.delayTime = newVal;
                    else if (param == "delayFeedback") tr.audioFx.delayFeedback = newVal;
                    else if (param == "delayMix") tr.audioFx.delayMix = newVal;
                    else if (param == "chorusRate") tr.audioFx.chorusRate = newVal;
                    else if (param == "chorusDepth") tr.audioFx.chorusDepth = newVal;
                    else if (param == "chorusMix") tr.audioFx.chorusMix = newVal;
                    else if (param == "eqLow") tr.audioFx.eqLow = newVal;
                    else if (param == "eqMid") tr.audioFx.eqMid = newVal;
                    else if (param == "eqHigh") tr.audioFx.eqHigh = newVal;
                    else if (param == "compThreshold") tr.audioFx.compThreshold = newVal;
                    else if (param == "compRatio") tr.audioFx.compRatio = newVal;
                    else if (param == "compGain") tr.audioFx.compGain = newVal;
                    else if (param == "convolverPreset") tr.audioFx.convolverPreset = static_cast<int>(std::round(newVal * 12.0f));
                    else if (param == "convolverMix") tr.audioFx.convolverMix = newVal;
                    else if (param == "arpPattern") tr.midiFx.arpPattern = static_cast<int>(std::round(newVal * 4.0f));
                    else if (param == "arpRate") tr.midiFx.arpRate = newVal;
                    else if (param == "arpOctaves") tr.midiFx.arpOctaves = static_cast<int>(std::round(newVal));
                    else if (param == "arpGate") tr.midiFx.arpGate = newVal;
                    else if (param == "arpSwing") tr.midiFx.arpSwing = newVal;
                    else if (param == "scaleRoot") tr.midiFx.scaleRoot = static_cast<int>(std::round(newVal * 11.0f));
                    else if (param == "scaleMinor") tr.midiFx.scaleMinor = (newVal >= 0.5f);
                    else if (param == "humanizeTiming") tr.midiFx.humanizeTiming = newVal;
                    else if (param == "humanizeVelocity") tr.midiFx.humanizeVelocity = newVal;

                    if (param.rfind("arp", 0) == 0 || param.rfind("scale", 0) == 0 || param.rfind("humanize", 0) == 0) {
                        if (modularArrangerView_ && t < modularArrangerView_->getTracks().size()) {
                            auto& mFx = modularArrangerView_->getTracks()[t].midiFx;
                            for (auto& item : mFx) {
                                std::string itemType = item.type;
                                for (char& c : itemType) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                                if (itemType.find("arp") != std::string::npos) {
                                    for (auto& k : item.knobs) {
                                        if (k.name == "rate" && param == "arpRate") k.value = newVal;
                                        else if (k.name == "pattern" && param == "arpPattern") k.value = newVal;
                                        else if (k.name == "octaves" && param == "arpOctaves") k.value = (newVal - 1.0f) / 3.0f;
                                        else if (k.name == "gate" && param == "arpGate") k.value = newVal;
                                        else if (k.name == "swing" && param == "arpSwing") k.value = newVal;
                                    }
                                } else if (itemType.find("scale") != std::string::npos) {
                                    if (param == "scaleRoot") item.rootKey = tr.midiFx.scaleRoot;
                                    if (param == "scaleMinor") item.scaleMode = tr.midiFx.scaleMinor ? 1 : 0;
                                } else if (itemType.find("humanize") != std::string::npos) {
                                    for (auto& k : item.knobs) {
                                        if (k.name == "timing" && param == "humanizeTiming") k.value = newVal;
                                        else if (k.name == "velocity" && param == "humanizeVelocity") k.value = newVal;
                                    }
                                }
                            }
                            if (modularTrackInspectorView_ && t < modularTrackInspectorView_->getTracks().size()) {
                                modularTrackInspectorView_->getTracks()[t].midiFxList = mFx;
                            }
                        }
                        syncTrackMidiFxToEngine(t);
                    }
                });
            activeDragHandler_ = &scalarDragPresenter_;
            break;
        }
        case TrackInspectorHitArea::HardwareKnob: {
            const auto* preset = getActivePreset();
            bool isSwitch = false;
            bool isSlider = false;
            if (preset) {
                std::function<void(const project::GuiLayoutNode&)> findType = [&](const project::GuiLayoutNode& n) {
                    if (n.paramName == inspHit.paramName) {
                        if (n.type == project::GuiNodeType::Switch) isSwitch = true;
                        else if (n.type == project::GuiNodeType::Slider) isSlider = true;
                    }
                    for (const auto& c : n.children) findType(c);
                };
                findType(preset->guiRoot);
                if (!isSwitch && !isSlider) findType(preset->compactGuiRoot);
            }
            if (isSwitch) {
                float curV = inspHit.normVal;
                float newV = (curV >= 0.5f) ? 0.0f : 1.0f;
                dispatchHardwareParam(inspHit.paramName, newV);
                break;
            }
            dragMode_ = DragMode::HardwareKnob;
            activeHardwareParam_ = inspHit.paramName;
            dragStartY_ = y;
            dragStartX_ = x;
            dragStartHardwareVal_ = inspHit.normVal;
            presenter::ScalarDragConfig cfg{};
            cfg.dragMode = DragMode::HardwareKnob;
            if (isSlider) {
                cfg.direction = presenter::DragDirection::HorizontalRightIncreases;
                cfg.sensitivity = 1.0f / 220.0f;
            } else {
                cfg.direction = presenter::DragDirection::VerticalUpIncreases;
                cfg.sensitivity = 1.0f / 160.0f;
            }
            cfg.minValue = 0.0f;
            cfg.maxValue = 1.0f;
            scalarDragPresenter_.startDrag(x, y, inspHit.normVal, cfg,
                [this, p = inspHit.paramName](float v) {
                    dispatchHardwareParam(p, v);
                });
            activeDragHandler_ = &scalarDragPresenter_;
            break;
        }
        case TrackInspectorHitArea::ColorSwatch: {
            static const float swatches[8][3] = {
                {0.0f, 0.90f, 1.0f},   // Neon Cyan
                {1.0f, 0.55f, 0.0f},   // Neon Amber
                {0.0f, 1.00f, 0.40f},  // Acid Green
                {1.0f, 0.00f, 0.48f},  // Hot Pink
                {0.74f, 0.00f, 1.0f},  // Electric Purple
                {1.0f, 0.20f, 0.20f},  // Crimson Red
                {1.0f, 0.85f, 0.0f},   // Gold Yellow
                {0.20f, 0.60f, 1.0f}   // Sky Blue
            };
            if (inspHit.colorSwatchIndex < 8) {
                trk.r = swatches[inspHit.colorSwatchIndex][0];
                trk.g = swatches[inspHit.colorSwatchIndex][1];
                trk.b = swatches[inspHit.colorSwatchIndex][2];
                for (auto& clp : trk.clips) {
                    clp.r = trk.r;
                    clp.g = trk.g;
                    clp.b = trk.b;
                }
                recordProjectHistory("Change Accent Color on Track " + std::to_string(tIdx + 1), "TRACK");
            }
            break;
        }
        default:
            break;
    }
}

void GuiWindow::selectArrangerClip(int trackIdx, int clipIdx) noexcept {
    selectedArrangerClipTrack_ = trackIdx;
    selectedArrangerClipIndex_ = clipIdx;
    if (trackIdx >= 0 && static_cast<size_t>(trackIdx) < arrangerTracks_.size()) {
        selectedTrackIndex_ = static_cast<uint32_t>(trackIdx);
        arrangerInspectorTab_ = ArrangerInspectorTab::Clip;
    }
}

std::string GuiWindow::getNoteScriptText() const {
    std::ostringstream ss;
    for (size_t i = 0; i < noteScriptBuffer_.size(); ++i) {
        ss << noteScriptBuffer_[i];
        if (i + 1 < noteScriptBuffer_.size()) ss << "\n";
    }
    return ss.str();
}

void GuiWindow::setNoteScriptText(const std::string& text) {
    noteScriptBuffer_.clear();
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        noteScriptBuffer_.push_back(line);
    }
    if (noteScriptBuffer_.empty()) {
        noteScriptBuffer_.push_back("");
    }
    noteScriptCursorLine_ = 0;
    noteScriptCursorCol_ = 0;
    noteScriptDirty_ = true;
}

void GuiWindow::setNoteScriptCursor(int line, int col) noexcept {
    if (noteScriptBuffer_.empty()) {
        noteScriptBuffer_.push_back("");
    }
    if (line < 0) line = 0;
    if (line >= static_cast<int>(noteScriptBuffer_.size())) {
        line = static_cast<int>(noteScriptBuffer_.size()) - 1;
    }
    noteScriptCursorLine_ = line;
    int lineLen = static_cast<int>(noteScriptBuffer_[line].size());
    if (col < 0) col = 0;
    if (col > lineLen) col = lineLen;
    noteScriptCursorCol_ = col;
}

void GuiWindow::insertNoteScriptChar(char c) {
    if (noteScriptBuffer_.empty()) noteScriptBuffer_.push_back("");
    if (noteScriptCursorLine_ >= static_cast<int>(noteScriptBuffer_.size())) {
        noteScriptCursorLine_ = static_cast<int>(noteScriptBuffer_.size()) - 1;
    }
    auto& line = noteScriptBuffer_[noteScriptCursorLine_];
    if (noteScriptCursorCol_ > static_cast<int>(line.size())) {
        noteScriptCursorCol_ = static_cast<int>(line.size());
    }
    line.insert(line.begin() + noteScriptCursorCol_, c);
    noteScriptCursorCol_++;
    noteScriptDirty_ = true;
}

void GuiWindow::insertNoteScriptText(const std::string& text) {
    for (char c : text) {
        if (c == '\n') {
            insertNoteScriptNewLine();
        } else if (c != '\r') {
            insertNoteScriptChar(c);
        }
    }
}

void GuiWindow::deleteNoteScriptCharBackwards() {
    if (noteScriptBuffer_.empty()) return;
    if (noteScriptCursorLine_ >= static_cast<int>(noteScriptBuffer_.size())) {
        noteScriptCursorLine_ = static_cast<int>(noteScriptBuffer_.size()) - 1;
    }
    auto& line = noteScriptBuffer_[noteScriptCursorLine_];
    if (noteScriptCursorCol_ > 0) {
        if (noteScriptCursorCol_ <= static_cast<int>(line.size())) {
            line.erase(line.begin() + noteScriptCursorCol_ - 1);
            noteScriptCursorCol_--;
            noteScriptDirty_ = true;
        }
    } else if (noteScriptCursorLine_ > 0) {
        int prevLineIdx = noteScriptCursorLine_ - 1;
        int prevLineLen = static_cast<int>(noteScriptBuffer_[prevLineIdx].size());
        noteScriptBuffer_[prevLineIdx] += line;
        noteScriptBuffer_.erase(noteScriptBuffer_.begin() + noteScriptCursorLine_);
        noteScriptCursorLine_ = prevLineIdx;
        noteScriptCursorCol_ = prevLineLen;
        noteScriptDirty_ = true;
    }
}

void GuiWindow::deleteNoteScriptCharForwards() {
    if (noteScriptBuffer_.empty()) return;
    if (noteScriptCursorLine_ >= static_cast<int>(noteScriptBuffer_.size())) {
        noteScriptCursorLine_ = static_cast<int>(noteScriptBuffer_.size()) - 1;
    }
    auto& line = noteScriptBuffer_[noteScriptCursorLine_];
    if (noteScriptCursorCol_ < static_cast<int>(line.size())) {
        line.erase(line.begin() + noteScriptCursorCol_);
        noteScriptDirty_ = true;
    } else if (noteScriptCursorLine_ + 1 < static_cast<int>(noteScriptBuffer_.size())) {
        line += noteScriptBuffer_[noteScriptCursorLine_ + 1];
        noteScriptBuffer_.erase(noteScriptBuffer_.begin() + noteScriptCursorLine_ + 1);
        noteScriptDirty_ = true;
    }
}

void GuiWindow::insertNoteScriptNewLine() {
    if (noteScriptBuffer_.empty()) noteScriptBuffer_.push_back("");
    if (noteScriptCursorLine_ >= static_cast<int>(noteScriptBuffer_.size())) {
        noteScriptCursorLine_ = static_cast<int>(noteScriptBuffer_.size()) - 1;
    }
    auto& curLine = noteScriptBuffer_[noteScriptCursorLine_];
    if (noteScriptCursorCol_ > static_cast<int>(curLine.size())) {
        noteScriptCursorCol_ = static_cast<int>(curLine.size());
    }

    std::string leftPart = curLine.substr(0, noteScriptCursorCol_);
    std::string remaining = curLine.substr(noteScriptCursorCol_);

    curLine = leftPart;
    noteScriptBuffer_.insert(noteScriptBuffer_.begin() + noteScriptCursorLine_ + 1, remaining);
    noteScriptCursorLine_++;
    noteScriptCursorCol_ = 0;
    noteScriptDirty_ = true;
}

bool GuiWindow::syncNoteScriptToTrack() {
    if (!engine_) return false;
    auto& seq = engine_->getSequencer();
    if (selectedTrackIndex_ >= seq.getNumTracks()) return false;
    auto* tr = seq.getTrack(selectedTrackIndex_);
    if (!tr) return false;

    std::string err;
    int errLine = -1;
    bool ok = eatscript::NoteScriptEngine::parseTrackNotesFromLines(noteScriptBuffer_, *tr, &err, &errLine);
    if (ok) {
        noteScriptDirty_ = false;
        noteScriptError_.clear();
        noteScriptErrorLine_ = -1;
        noteScriptStatusMsg_ = "Applied to " + tr->getName();
        setStatusMessage("APPLIED SCRIPT TO " + tr->getName());
        return true;
    } else {
        noteScriptError_ = err;
        noteScriptErrorLine_ = errLine;
        noteScriptStatusMsg_ = "Error (Line " + std::to_string(errLine) + "): " + err;
        setStatusMessage("SCRIPT SYNTAX ERROR: " + err);
        return false;
    }
}

void GuiWindow::syncTrackToNoteScript() {
    if (!engine_) return;
    auto& seq = engine_->getSequencer();
    if (selectedTrackIndex_ >= seq.getNumTracks()) return;
    auto* tr = seq.getTrack(selectedTrackIndex_);
    if (!tr) return;

    noteScriptBuffer_ = eatscript::NoteScriptEngine::serializeTrackNotesToLines(*tr);
    noteScriptDirty_ = false;
    noteScriptError_.clear();
    noteScriptErrorLine_ = -1;
    noteScriptStatusMsg_ = "Synced from " + tr->getName();
    noteScriptLastTrack_ = selectedTrackIndex_;
    if (noteScriptCursorLine_ >= static_cast<int>(noteScriptBuffer_.size())) {
        noteScriptCursorLine_ = std::max(0, static_cast<int>(noteScriptBuffer_.size()) - 1);
    }
}

void GuiWindow::insertNoteScriptTemplate() {
    noteScriptBuffer_.push_back("{ pitch = \"C3\", step = 0, dur = 0.75, vel = 0.85 }");
    noteScriptBuffer_.push_back("{ pitch = \"D#3\", step = 4, dur = 0.75, vel = 0.80, slide = true }");
    noteScriptBuffer_.push_back("{ pitch = \"F3\", step = 8, dur = 0.75, vel = 0.90 }");
    noteScriptBuffer_.push_back("{ pitch = \"G3\", step = 12, dur = 0.75, vel = 0.95, accent = true }");
    noteScriptDirty_ = true;
    syncNoteScriptToTrack();
}

HitTestNoteScriptResult GuiWindow::hitTestNoteScript(float x, float y) const noexcept {
    HitTestNoteScriptResult res{};
    if (activeView_ != WorkspaceView::Edit && activeView_ != WorkspaceView::Tracker) return res;
    if (editSubView_ != EditSubView::Script) return res;

    // Toolbar buttons: [Y = 98..126]
    if (y >= 98.0f && y <= 126.0f) {
        // [ APPLY TO TRACK ] at [40..230]
        if (x >= 40.0f && x <= 230.0f) {
            res.hit = true;
            res.action = HitTestNoteScriptResult::Action::ApplySync;
            return res;
        }
        // [ REVERT FROM TRACK ] at [240..390]
        if (x >= 240.0f && x <= 390.0f) {
            res.hit = true;
            res.action = HitTestNoteScriptResult::Action::RevertFromTrack;
            return res;
        }
        // [ + 4-BAR ARP ] at [400..520]
        if (x >= 400.0f && x <= 520.0f) {
            res.hit = true;
            res.action = HitTestNoteScriptResult::Action::InsertTemplate;
            return res;
        }
        // [ CLEAR ] at [530..610]
        if (x >= 530.0f && x <= 610.0f) {
            res.hit = true;
            res.action = HitTestNoteScriptResult::Action::Clear;
            return res;
        }
    }

    // Code Editor Canvas: [X = 30..width-30, Y = 134..680]
    float rightBound = static_cast<float>(width_) - 30.0f;
    if (x >= 30.0f && x <= rightBound && y >= 134.0f && y <= 680.0f) {
        res.hit = true;
        res.action = HitTestNoteScriptResult::Action::EditorClick;
        int line = static_cast<int>((y - 138.0f) / 20.0f);
        int col = static_cast<int>((x - 85.0f) / 8.5f);
        if (col < 0) col = 0;
        res.line = line;
        res.col = col;
        return res;
    }

    return res;
}

void GuiWindow::loadPresetToSelectedTrack(size_t presetIndex) {
    if (presetIndex >= presets_.size()) return;
    activePresetIndex_ = presetIndex;
    selectedBrowserPresetIndex_ = presetIndex;
    const auto& p = presets_[presetIndex];

    project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                               40.0f, 98.0f,
                                               static_cast<float>(width_) - 80.0f,
                                               static_cast<float>(height_) - 154.0f);

    if (selectedTrackIndex_ < mixerStrips_.size()) {
        mixerStrips_[selectedTrackIndex_].name = p.metadata.name;
    }
    if (selectedTrackIndex_ < arrangerTracks_.size()) {
        arrangerTracks_[selectedTrackIndex_].name = p.metadata.name;
        arrangerTracks_[selectedTrackIndex_].instrument = p.metadata.name;
        arrangerTracks_[selectedTrackIndex_].instrumentEngine = p.metadata.id;
    }
    if (modularArrangerView_ && selectedTrackIndex_ < modularArrangerView_->getTracks().size()) {
        auto& trk = modularArrangerView_->getTracks()[selectedTrackIndex_];
        trk.name = p.metadata.name;
        trk.instrument = p.metadata.name;
        trk.instrumentEngine = p.metadata.id;
    }
    if (engine_) {
        if (selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
            auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
            if (tr) tr->setName(p.metadata.name);
        }
        engine_->assignTrackInstrument(selectedTrackIndex_, p.metadata.id);
    }

    for (const auto& [paramName, param] : p.params) {
        dispatchTrackParam(selectedTrackIndex_, paramName, param.getNormalized());
    }

    setSelectedTrackIndex(selectedTrackIndex_);
    if (fullscreenDeviceModal_.isOpen() && fullscreenDeviceModal_.getTarget().trackIndex == selectedTrackIndex_) {
        openFullscreenDevice(selectedTrackIndex_);
    }
    lastStatusMessage_ = "LOADED: " + p.metadata.name;
    recordProjectHistory("Loaded Preset: " + p.metadata.name, "PRESET");
}

void GuiWindow::runMacro(size_t macroIndex) {
    if (!engine_) return;
    const auto& macros = eatscript::MacroRuntime::getBuiltinMacros();
    if (macroIndex >= macros.size()) return;

    eatscript::MacroRuntime runtime;
    auto res = runtime.execute(macros[macroIndex].scriptCode, *engine_, engine_->getSequencer());
    if (res.success) {
        lastStatusMessage_ = "MACRO: " + macros[macroIndex].name + " EXECUTED";
        recordProjectHistory("Ran Macro: " + macros[macroIndex].name, "MACRO");

        if (macroIndex == 4) {
            // Procedural Song: sync arranger timeline tracks & clips
            procgen::SongGenerationParams p;
            p.style = "Lo-Fi Hip Hop";
            p.bars = 16;
            p.seed = 42;
            auto songRes = procgen::ProceduralSongEngine::generateSong(p);
            if (songRes.success && !songRes.generatedTracks.empty()) {
                arrangerTracks_.clear();
                for (const auto& gt : songRes.generatedTracks) {
                    ArrangerTrackData atd;
                    atd.name = gt.name;
                    atd.type = gt.instrumentEngine;
                    atd.volume = gt.volume;
                    atd.pan = gt.pan;
                    atd.r = gt.r;
                    atd.g = gt.g;
                    atd.b = gt.b;
                    for (const auto& c : gt.clips) {
                        ArrangerClip ac;
                        ac.name = c.name;
                        ac.startBar = c.startBar;
                        ac.barLength = c.lengthBars;
                        ac.r = c.r;
                        ac.g = c.g;
                        ac.b = c.b;
                        atd.clips.push_back(ac);
                    }
                    arrangerTracks_.push_back(std::move(atd));
                }
                projectName_ = "Lo-Fi Hip Hop (Procedural)";
                if (engine_) engine_->getSequencer().setBpm(songRes.bpm);
            }
        }

        // If on note script view, synchronize script editor with new track data
        if (selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
            auto* trk = engine_->getSequencer().getTrack(selectedTrackIndex_);
            if (trk) {
                noteScriptBuffer_ = eatscript::NoteScriptEngine::serializeTrackNotesToLines(*trk);
            }
        }
    } else {
        lastStatusMessage_ = "MACRO ERROR: " + res.message;
    }
}

bool GuiWindow::saveProjectToFile(const std::string& filePath) {
    if (!engine_) return false;
    if (modularArrangerView_) {
        engine_->getSequencer().setChordTrack(modularArrangerView_->getChordTrack());
        const auto& arrTracks = modularArrangerView_->getTracks();
        for (size_t t = 0; t < arrTracks.size() && t < engine_->getSequencer().getNumTracks(); ++t) {
            if (auto* trk = engine_->getSequencer().getTrack(t)) {
                trk->setChordFollowMode(arrTracks[t].chordFollowMode);
            }
        }
    }
    double bpm = engine_->getSequencer().getBpm();
    double swing = engine_->getSequencer().getSwing();
    bool ok = project::ProjectFile::saveToFile(filePath, engine_->getGraph(), engine_->getSequencer(), projectName_, bpm, swing);
    if (ok) {
        projectFilePath_ = filePath;
        lastStatusMessage_ = "PROJECT SAVED TO " + filePath;
    } else {
        lastStatusMessage_ = "ERROR: FAILED TO SAVE PROJECT";
    }
    return ok;
}

bool GuiWindow::saveProjectAs() {
    std::string suggested = projectName_.empty() ? "my_song.eats" : (projectName_ + ".eats");
    std::string path = promptSaveEatsFile(suggested);
    if (path.empty()) {
        path = suggested;
    }
    return saveProjectToFile(path);
}

bool GuiWindow::loadProjectFromFile(const std::string& filePath) {
    if (!engine_) return false;
    std::string title;
    double bpm = 135.0, swing = 0.50;
    bool ok = project::ProjectFile::loadFromFile(filePath, engine_->getGraph(), engine_->getSequencer(), title, bpm, swing);
    if (ok) {
        if (!title.empty()) {
            projectName_ = title;
        }
        projectFilePath_ = filePath;
        engine_->setEngineMode(audio::SynthEngineMode::ModularGraph);
        engine_->getGraph().compile();
        canvas_.updateRackLayout(engine_->getGraph());
        updateMixerStrips();
        syncArrangerFromSequencer();
        const auto& loadedChords = engine_->getSequencer().getChordTrack();
        if (!loadedChords.empty()) {
            if (modularArrangerView_) modularArrangerView_->setChordTrack(loadedChords);
            if (modularEditView_) modularEditView_->setChordTrack(loadedChords);
        }
        initDefaultKnobValues();
        initHistory();
        lastStatusMessage_ = "LOADED PROJECT: " + (title.empty() ? filePath : title);
    } else {
        lastStatusMessage_ = "ERROR: FAILED TO LOAD " + filePath;
    }
    return ok;
}

bool GuiWindow::loadProjectPrompt() {
    std::string path = promptOpenEatsFile();
    if (path.empty()) {
        path = "project.eats";
    }
    return loadProjectFromFile(path);
}

void GuiWindow::resetToDefaultProject() {
    if (!engine_) return;
    engine_->setupDefaultEpianoGraph();
    canvas_.updateRackLayout(engine_->getGraph());
    updateMixerStrips();
    initDefaultKnobValues();
    initArrangerTracks();
    projectName_ = "Untitled Song";
    authorName_ = "Anonymous Producer";
    projectFilePath_ = "project.eats";
    initHistory();
    lastStatusMessage_ = "RESET WORKSPACE TO DEFAULT TEMPLATE";
}

bool GuiWindow::bounceMasterToWav(const std::string& filePath) {
    if (!engine_) return false;
    auto stats = engine_->bounceProject(filePath, 8.0, audio::exporting::WavFormat::Pcm24);
    if (stats.totalFrames > 0) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << stats.speedMultiplier << "x FAST-BOUNCE: " << filePath;
        lastStatusMessage_ = ss.str();
        return true;
    }
    lastStatusMessage_ = "ERROR: BOUNCE FAILED";
    return false;
}

void GuiWindow::initHistory() {
    if (!engine_) return;
    double bpm = engine_->getSequencer().getBpm();
    double swing = engine_->getSequencer().getSwing();
    std::string currentEats = project::EatsProjectSerializer::serialize(
        engine_->getGraph(), engine_->getSequencer(), projectName_, bpm, swing);
    diffHistory_.init(currentEats, "Project Initialized: " + projectName_);
    selectedHistoryIndex_ = 0;
}

void GuiWindow::recordProjectHistory(const std::string& description, const std::string& category, bool isMilestone, const std::string& milestoneName) {
    if (!engine_) return;
    double bpm = engine_->getSequencer().getBpm();
    double swing = engine_->getSequencer().getSwing();
    std::string newEats = project::EatsProjectSerializer::serialize(
        engine_->getGraph(), engine_->getSequencer(), projectName_, bpm, swing);
    bool recorded = diffHistory_.record(newEats, description, category, isMilestone, milestoneName);
    if (recorded) {
        selectedHistoryIndex_ = diffHistory_.getCurrentTimelineIndex();
        historyDirty_ = true;
        setStatusMessage(description);
    }
}

bool GuiWindow::undoHistory() {
    std::string restored;
    if (diffHistory_.undo(restored)) {
        if (engine_) {
            std::string title;
            double bpm = 135.0, swing = 0.50;
            if (project::EatsProjectSerializer::deserialize(restored, engine_->getGraph(), engine_->getSequencer(), title, bpm, swing)) {
                if (!title.empty()) projectName_ = title;
                engine_->getGraph().compile();
                canvas_.updateRackLayout(engine_->getGraph());
                updateMixerStrips();
                initDefaultKnobValues();
                syncArrangerFromSequencer();
            }
        }
        selectedHistoryIndex_ = diffHistory_.getCurrentTimelineIndex();
        const auto* cur = diffHistory_.getCurrentEntry();
        std::string desc = cur ? cur->description : "Undo Action";
        setStatusMessage("UNDO: " + desc);
        return true;
    }
    setStatusMessage("Undo: At oldest project state");
    return false;
}

bool GuiWindow::redoHistory() {
    std::string restored;
    if (diffHistory_.redo(restored)) {
        if (engine_) {
            std::string title;
            double bpm = 135.0, swing = 0.50;
            if (project::EatsProjectSerializer::deserialize(restored, engine_->getGraph(), engine_->getSequencer(), title, bpm, swing)) {
                if (!title.empty()) projectName_ = title;
                engine_->getGraph().compile();
                canvas_.updateRackLayout(engine_->getGraph());
                updateMixerStrips();
                initDefaultKnobValues();
                syncArrangerFromSequencer();
            }
        }
        selectedHistoryIndex_ = diffHistory_.getCurrentTimelineIndex();
        const auto* cur = diffHistory_.getCurrentEntry();
        std::string desc = cur ? cur->description : "Redo Action";
        setStatusMessage("REDO: " + desc);
        return true;
    }
    setStatusMessage("Redo: At newest project state");
    return false;
}

bool GuiWindow::jumpToHistoryIndex(size_t index) {
    std::string restored;
    if (diffHistory_.jumpToTimelineIndex(index, restored)) {
        if (engine_) {
            std::string title;
            double bpm = 135.0, swing = 0.50;
            if (project::EatsProjectSerializer::deserialize(restored, engine_->getGraph(), engine_->getSequencer(), title, bpm, swing)) {
                if (!title.empty()) projectName_ = title;
                engine_->getGraph().compile();
                canvas_.updateRackLayout(engine_->getGraph());
                updateMixerStrips();
                initDefaultKnobValues();
                syncArrangerFromSequencer();
            }
        }
        selectedHistoryIndex_ = index;
        const auto* cur = diffHistory_.getCurrentEntry();
        std::string desc = cur ? cur->description : "Timeline State";
        setStatusMessage("TIME-TRAVEL: #" + std::to_string(index) + " - " + desc);
        return true;
    }
    return false;
}

void GuiWindow::createHistoryMilestone(const std::string& name) {
    diffHistory_.createMilestone(name);
    setStatusMessage("MILESTONE SAVED: " + name);
}

void GuiWindow::clearHistory() {
    diffHistory_.clear();
    selectedHistoryIndex_ = 0;
    setStatusMessage("HISTORY CLEARED (Origin Preserved)");
}

HitTestJackResult GuiWindow::hitTestJack(float x, float y) const noexcept {
    HitTestJackResult res{};
    const auto& modules = canvas_.getModules();

    for (const auto& m : modules) {
        // Test Input Jacks
        for (uint32_t i = 0; i < m.inputJacks.size(); ++i) {
            const auto& pt = m.inputJacks[i];
            const float dist = std::hypot(x - pt.x, y - pt.y);
            if (dist <= 14.0f) {
                res.hit = true;
                res.nodeId = m.id;
                res.portIndex = i;
                res.isOutput = false;
                res.position = pt;
                return res;
            }
        }

        // Test Output Jacks
        for (uint32_t i = 0; i < m.outputJacks.size(); ++i) {
            const auto& pt = m.outputJacks[i];
            const float dist = std::hypot(x - pt.x, y - pt.y);
            if (dist <= 14.0f) {
                res.hit = true;
                res.nodeId = m.id;
                res.portIndex = i;
                res.isOutput = true;
                res.position = pt;
                return res;
            }
        }
    }

    return res;
}

HitTestKnobResult GuiWindow::hitTestKnob(float x, float y) const noexcept {
    HitTestKnobResult res{};
    const auto& modules = canvas_.getModules();

    for (const auto& m : modules) {
        for (size_t k = 0; k < m.knobNames.size(); ++k) {
            const float kx = m.x + 36.0f + ((k % 2) * 90.0f);
            const float ky = m.y + 56.0f + ((k / 2) * 52.0f);
            const float dist = std::hypot(x - kx, y - ky);
            if (dist <= 18.0f) {
                res.hit = true;
                res.nodeId = m.id;
                res.knobIndex = static_cast<uint32_t>(k);
                res.knobName = m.knobNames[k];
                res.position = Point2D{kx, ky};
                return res;
            }
        }
    }

    return res;
}

HitTestStepResult GuiWindow::hitTestStep(float x, float y) const noexcept {
    HitTestStepResult res{};

    if (activeView_ == WorkspaceView::Tracker && engine_) {
        const float startY = 120.0f;
        const float trackH = 88.0f;
        const float gap = 20.0f;
        const float stepW = (static_cast<float>(width_) - 180.0f) / 16.0f;
        const size_t numTracks = engine_->getSequencer().getNumTracks();

        for (size_t t = 0; t < numTracks && t < 4; ++t) {
            float ty = startY + t * (trackH + gap);
            for (uint32_t s = 0; s < 16; ++s) {
                float sx = 140.0f + s * stepW;
                float sy = ty;
                if (x >= sx && x <= (sx + stepW - 4.0f) && y >= sy && y <= (sy + trackH)) {
                    res.hit = true;
                    res.trackIndex = static_cast<uint32_t>(t);
                    res.stepIndex = s;
                    return res;
                }
            }
        }
        return res;
    }

    const float scopeX = 30.0f;
    const float scopeW = 280.0f;
    const float vuX = scopeX + scopeW + 20.0f;
    const float vuW = 50.0f;
    const float seqX = vuX + vuW + 20.0f;
    const float seqY = static_cast<float>(height_) - 180.0f;
    const float seqW = static_cast<float>(width_) - seqX - 30.0f;

    const float stepW = (seqW - 24.0f) / 16.0f;

    for (uint32_t s = 0; s < 16; ++s) {
        const float sx = seqX + 12.0f + (s * stepW);
        const float sy = seqY + 38.0f;
        if (x >= sx && x <= (sx + stepW - 4.0f) && y >= sy && y <= (sy + 60.0f)) {
            res.hit = true;
            res.trackIndex = 0;
            res.stepIndex = s;
            return res;
        }
    }

    return res;
}

HitTestEditSubNavResult GuiWindow::hitTestEditSubNav(float x, float y) const noexcept {
    HitTestEditSubNavResult res{};
    if (modularEditView_ && modularEditView_->getBtnPianoRoll().w > 0.0f) {
        if (modularEditView_->getBtnPianoRoll().contains(x, y)) {
            res.hit = true;
            res.subView = EditSubView::PianoRoll;
            return res;
        }
        if (modularEditView_->getBtnTracker().contains(x, y)) {
            res.hit = true;
            res.subView = EditSubView::Tracker;
            return res;
        }
        if (modularEditView_->getBtnScore().contains(x, y)) {
            res.hit = true;
            res.subView = EditSubView::Score;
            return res;
        }
        if (modularEditView_->getBtnScript().contains(x, y)) {
            res.hit = true;
            res.subView = EditSubView::Script;
            return res;
        }
        return res;
    }

    if (y < 60.0f || y > 95.0f) return res;

    // Subnav buttons fallback for uninitialized / legacy tests:
    // [ PIANO ROLL ] : 830..928
    // [ TRACKER ]    : 940..1038
    // [ SCORE ]      : 1050..1148
    // [ SCRIPT ]     : 1160..1258
    if (x >= 830.0f && x <= 928.0f) {
        res.hit = true;
        res.subView = EditSubView::PianoRoll;
    } else if (x >= 940.0f && x <= 1038.0f) {
        res.hit = true;
        res.subView = EditSubView::Tracker;
    } else if (x >= 1050.0f && x <= 1148.0f) {
        res.hit = true;
        res.subView = EditSubView::Score;
    } else if (x >= 1160.0f && x <= 1258.0f) {
        res.hit = true;
        res.subView = EditSubView::Script;
    }
    return res;
}

HitTestPianoKeyResult GuiWindow::hitTestPianoKey(float x, float y) const noexcept {
    HitTestPianoKeyResult res{};
    const float kbX = 20.0f;
    const float kbW = pianoRollKeyboard_.getKeyWidth();
    const float topY = 100.0f;
    const float bottomY = 650.0f;
    const float kbH = bottomY - topY;

    auto hit = pianoRollKeyboard_.hitTest(x, y, kbX, topY, kbW, kbH, pianoRollScrollY_);
    if (hit.hit) {
        res.hit = true;
        res.pitch = static_cast<uint8_t>(hit.pitch);
        res.isBlackKey = hit.isBlack;
        res.velocity = hit.velocity;
    }
    return res;
}

HitTestPianoRollGridResult GuiWindow::hitTestPianoRollGrid(float x, float y) const noexcept {
    HitTestPianoRollGridResult res{};
    const float gridX = 110.0f;
    const bool showSidebar = engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_) &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_)->hasSelectedNotes();
    const float scrollbarW = 12.0f;
    const float gridW = static_cast<float>(width_) - gridX - (showSidebar ? 280.0f : 20.0f) - scrollbarW;
    const float topY = 100.0f;
    const float bottomY = 650.0f;

    if (x < gridX || x > (gridX + gridW) || y < topY || y > bottomY) {
        return res;
    }

    uint32_t step = static_cast<uint32_t>((x - gridX + pianoRollScrollX_) / pianoRollStepW_);
    int pitch = pianoRollKeyboard_.verticalYToPitch(y - topY, pianoRollScrollY_);

    res.hit = true;
    res.step = step;
    res.pitch = static_cast<uint8_t>(pitch);

    if (engine_) {
        auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
        if (tr && step < tr->getNumSteps()) {
            const auto& st = tr->getStep(step);
            if (st.active && st.note == pitch) {
                res.hasExistingNote = true;
            }
        }
    }
    return res;
}

HitTestPianoRollScrollbarResult GuiWindow::hitTestPianoRollScrollbar(float mx, float my) const noexcept {
    HitTestPianoRollScrollbarResult res{};
    const float gridX = 110.0f;
    const bool showSidebar = engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_) &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_)->hasSelectedNotes();
    const float scrollbarW = 12.0f;
    const float gridW = static_cast<float>(width_) - gridX - (showSidebar ? 280.0f : 20.0f) - scrollbarW;
    const float topY = 100.0f;
    const float bottomY = 650.0f;
    const float sbH = bottomY - topY;

    // 1. Vertical Scrollbar (on right of grid)
    const float vsbX = gridX + gridW + 2.0f;
    const float vsbY = topY;
    if (mx >= vsbX && mx <= vsbX + scrollbarW && my >= vsbY && my <= vsbY + sbH) {
        res.hit = true;
        res.isVertical = true;
        float totalH = pianoRollKeyboard_.getTotalVerticalContentHeight();
        float thumbH = std::clamp((sbH / std::max(totalH, sbH)) * sbH, 24.0f, sbH);
        float maxScrollY = std::max(0.0f, totalH - sbH);
        float thumbY = vsbY + (maxScrollY > 0.0f ? (pianoRollScrollY_ / maxScrollY) * (sbH - thumbH) : 0.0f);
        if (my >= thumbY && my <= thumbY + thumbH) {
            res.isThumb = true;
        }
        res.normOffset = std::clamp((my - vsbY) / sbH, 0.0f, 1.0f);
        return res;
    }

    // 2. Horizontal Scrollbar (at bottom of grid)
    const float hsbX = gridX;
    const float hsbY = bottomY + 2.0f;
    const float hsbH = 10.0f;
    if (mx >= hsbX && mx <= hsbX + gridW && my >= hsbY && my <= hsbY + hsbH) {
        res.hit = true;
        res.isVertical = false;
        const uint32_t totalSteps = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() && engine_->getSequencer().getTrack(selectedTrackIndex_))
                                        ? std::max(64u, engine_->getSequencer().getTrack(selectedTrackIndex_)->getNumSteps())
                                        : 64u;
        float totalW = static_cast<float>(totalSteps) * pianoRollStepW_;
        float thumbW = std::clamp((gridW / std::max(totalW, gridW)) * gridW, 24.0f, gridW);
        float maxScrollX = std::max(0.0f, totalW - gridW);
        float thumbX = hsbX + (maxScrollX > 0.0f ? (pianoRollScrollX_ / maxScrollX) * (gridW - thumbW) : 0.0f);
        if (mx >= thumbX && mx <= thumbX + thumbW) {
            res.isThumb = true;
        }
        res.normOffset = std::clamp((mx - hsbX) / gridW, 0.0f, 1.0f);
        return res;
    }

    return res;
}

void GuiWindow::setPianoRollScrollX(float scrollX) noexcept {
    const float gridX = 110.0f;
    const bool showSidebar = engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_) &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_)->hasSelectedNotes();
    const float scrollbarW = 12.0f;
    const float gridW = static_cast<float>(width_) - gridX - (showSidebar ? 280.0f : 20.0f) - scrollbarW;
    const uint32_t totalSteps = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() && engine_->getSequencer().getTrack(selectedTrackIndex_))
                                    ? std::max(64u, engine_->getSequencer().getTrack(selectedTrackIndex_)->getNumSteps())
                                    : 64u;
    float totalW = static_cast<float>(totalSteps) * pianoRollStepW_;
    float maxScrollX = std::max(0.0f, totalW - gridW);
    pianoRollScrollX_ = std::clamp(scrollX, 0.0f, maxScrollX);
}

void GuiWindow::setPianoRollScrollY(float scrollY) noexcept {
    const float topY = 100.0f;
    const float bottomY = 650.0f;
    const float sbH = bottomY - topY;
    float totalH = pianoRollKeyboard_.getTotalVerticalContentHeight();
    float maxScrollY = std::max(0.0f, totalH - sbH);
    pianoRollScrollY_ = std::clamp(scrollY, 0.0f, maxScrollY);
}

void GuiWindow::setPianoRollStepWidth(float stepW) noexcept {
    pianoRollStepW_ = std::clamp(stepW, 16.0f, 160.0f);
}

void GuiWindow::setPianoRollRowHeight(float rowH) noexcept {
    pianoRollRowH_ = std::clamp(rowH, 12.0f, 48.0f);
    pianoRollKeyboard_.setKeyHeight(pianoRollRowH_);
}

void GuiWindow::centerPianoRollOnNotes() noexcept {
    if (!engine_ || selectedTrackIndex_ >= engine_->getSequencer().getNumTracks()) return;
    auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
    if (!tr) return;

    int minNote = 127;
    int maxNote = 0;
    int noteCount = 0;
    for (uint32_t s = 0; s < tr->getNumSteps(); ++s) {
        const auto& st = tr->getStep(s);
        if (st.active) {
            minNote = std::min(minNote, static_cast<int>(st.note));
            maxNote = std::max(maxNote, static_cast<int>(st.note));
            noteCount++;
        }
    }

    int avgNote = (noteCount > 0) ? ((minNote + maxNote) / 2) : 60;
    const float viewportH = 550.0f;
    float targetY = static_cast<float>(pianoRollKeyboard_.getMaxPitch() - avgNote) * pianoRollRowH_ - viewportH * 0.5f;
    setPianoRollScrollY(targetY);
}

void GuiWindow::selectPianoRollNotesByPitch(int pitch, bool additive) {
    if (!engine_ || selectedTrackIndex_ >= engine_->getSequencer().getNumTracks()) return;
    auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
    if (!tr) return;

    std::vector<uint32_t> matching;
    for (uint32_t s = 0; s < tr->getNumSteps(); ++s) {
        const auto& st = tr->getStep(s);
        if (st.active && st.note == pitch) {
            matching.push_back(s);
        }
    }

    if (!matching.empty()) {
        tr->selectSteps(matching, !additive);
        if (engine_) {
            engine_->postNoteOn(static_cast<uint8_t>(pitch), 0.85f, false, false, static_cast<int>(selectedTrackIndex_));
        }
        previewingPitch_ = pitch;
        previewingVelocity_ = 0.85f;
    }
}

bool GuiWindow::isShiftPressed() const noexcept {
    if (mockShiftPressed_) return true;
    if (!window_) return false;
    return (glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
}

bool GuiWindow::isCtrlPressed() const noexcept {
    if (!window_) return false;
    return (glfwGetKey(window_, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
            glfwGetKey(window_, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
}

bool GuiWindow::isAltPressed() const noexcept {
    if (!window_) return false;
    return (glfwGetKey(window_, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
            glfwGetKey(window_, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS);
}

void GuiWindow::setVirtualKeyboardBaseOctave(int octave) noexcept {
    virtualKeyboardBaseOctave_ = std::clamp(octave, 1, 6);
    virtualPianoDrawerKeyboard_.setBaseOctave(virtualKeyboardBaseOctave_);
    if (virtualKeyboardDrawerWidget_) {
        virtualKeyboardDrawerWidget_->setBaseOctave(virtualKeyboardBaseOctave_);
    }
}

HitTestVirtualKeyboardDrawerResult GuiWindow::hitTestVirtualKeyboardDrawer(float mx, float my) const noexcept {
    HitTestVirtualKeyboardDrawerResult res{};
    const float bPanelY = static_cast<float>(height_) - 48.0f;
    const float tabW = 140.0f;
    const float tabH = 20.0f;
    const float tabX = (static_cast<float>(width_) - tabW) * 0.5f;

    float animProg = guiAnimationsEnabled_ ? virtualKeyboardAnimProgress_ : (virtualKeyboardDrawerOpen_ ? 1.0f : 0.0f);
    float currentH = virtualKeyboardDrawerHeight_ * animProg;
    float drawerY = bPanelY - currentH;
    float tabY = drawerY - tabH;

    // Pull-tab hit test (active in both open and closed animated states)
    if (mx >= tabX && mx <= tabX + tabW && my >= tabY && my <= (currentH > 0.0f ? drawerY + 4.0f : bPanelY)) {
        res.hit = true;
        res.isPullTab = true;
        return res;
    }

    if (animProg > 0.2f && currentH > 30.0f) {
        // Inside Drawer Body
        if (my >= drawerY && my <= bPanelY && mx >= 0.0f && mx <= static_cast<float>(width_)) {
            res.hit = true;

            // Octave controls in toolbar [y: drawerY+3..drawerY+25]
            if (my >= drawerY + 3.0f && my <= drawerY + 25.0f) {
                // [< OCT] button
                if (mx >= 240.0f && mx <= 300.0f) {
                    res.isOctaveDown = true;
                    return res;
                }
                // [OCT >] button
                if (mx >= 420.0f && mx <= 480.0f) {
                    res.isOctaveUp = true;
                    return res;
                }
            }

            // Keyboard area
            float kx = 16.0f;
            float ky = drawerY + 28.0f;
            float kw = static_cast<float>(width_) - 32.0f;
            float kh = currentH - 34.0f;

            if (kh > 20.0f && my >= ky && my <= ky + kh && mx >= kx && mx <= kx + kw) {
                auto keyHit = virtualPianoDrawerKeyboard_.hitTest(mx, my, kx, ky, kw, kh, 0.0f);
                if (keyHit.hit) {
                    res.isKey = true;
                    res.pitch = keyHit.pitch;
                    res.velocity = keyHit.velocity;
                    return res;
                }
            }
        }
    }

    return res;
}

void GuiWindow::drawVirtualKeyboardDrawer() {
    const float bPanelY = static_cast<float>(height_) - 48.0f;
    const float tabW = 140.0f;
    const float tabH = 20.0f;
    const float tabX = (static_cast<float>(width_) - tabW) * 0.5f;
    const auto& theme = getTheme();

    // Smooth ease-in animation step with exponential damping
    float target = virtualKeyboardDrawerOpen_ ? 1.0f : 0.0f;
    constexpr float kKeyboardDamping = 18.0f;
    float factor = 1.0f - std::exp(-kKeyboardDamping * frameDt_);
    virtualKeyboardAnimProgress_ += (target - virtualKeyboardAnimProgress_) * factor;
    if (std::abs(virtualKeyboardAnimProgress_ - target) < 0.001f) {
        virtualKeyboardAnimProgress_ = target;
    }

    float currentH = virtualKeyboardDrawerHeight_ * virtualKeyboardAnimProgress_;
    float drawerY = bPanelY - currentH;
    float tabY = drawerY - tabH;

    if (virtualKeyboardAnimProgress_ <= 0.001f) {
        // Closed: draw pull-tab docked above bottom bar
        float closedTabY = bPanelY - tabH;
        drawRoundedRectGradient(tabX, closedTabY, tabW, tabH + 4.0f, 4.0f,
                                theme.controlBackground, theme.controlWell);
        drawRoundedRectOutline(tabX, closedTabY, tabW, tabH + 4.0f, 4.0f,
                               theme.borderSubtle, 1.0f);
        if (batchRenderer_) {
            drawPianoIcon(*batchRenderer_, tabX + 16.0f, closedTabY + 4.5f, 16.0f, 11.0f, theme.primaryAccent);
            drawCenteredText(*batchRenderer_, "VIRTUAL PIANO", tabX + 36.0f, closedTabY, tabW - 44.0f, tabH, 10.5f, theme.primaryAccent);
        } else {
            drawVectorString("VIRTUAL PIANO", tabX + 38.0f, closedTabY + 5.0f, 0.72f, theme.primaryAccent);
        }
        return;
    }

    // Pull Tab above drawer sliding smoothly
    drawRoundedRectGradient(tabX, tabY, tabW, tabH + 4.0f, 4.0f,
                            theme.controlBackground, theme.controlWell);
    drawRoundedRectOutline(tabX, tabY, tabW, tabH + 4.0f, 4.0f,
                           theme.borderSubtle, 1.0f);
    if (batchRenderer_) {
        drawPianoIcon(*batchRenderer_, tabX + 16.0f, tabY + 4.5f, 16.0f, 11.0f, theme.secondaryAccent);
        drawCenteredText(*batchRenderer_, "VIRTUAL PIANO", tabX + 36.0f, tabY, tabW - 44.0f, tabH, 10.5f, theme.secondaryAccent);
    } else {
        drawVectorString("VIRTUAL PIANO", tabX + 38.0f, tabY + 5.0f, 0.72f, theme.secondaryAccent);
    }

    // Chassis background with drop shadow and sleek bevel following theme
    drawRect(0.0f, drawerY - 4.0f, static_cast<float>(width_), 4.0f, 0.0f, 0.0f, 0.0f, 0.45f * virtualKeyboardAnimProgress_);
    drawRectGradient(0.0f, drawerY, static_cast<float>(width_), currentH,
                     theme.panelBackground.lighten(0.04f).r, theme.panelBackground.lighten(0.04f).g, theme.panelBackground.lighten(0.04f).b,
                     theme.panelBackground.darken(0.12f).r, theme.panelBackground.darken(0.12f).g, theme.panelBackground.darken(0.12f).b);
    drawLine(0.0f, drawerY, static_cast<float>(width_), drawerY,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f * virtualKeyboardAnimProgress_, 1.5f);

    if (virtualKeyboardAnimProgress_ <= 0.15f || currentH <= 40.0f) {
        return;
    }

    // Top Header / Toolbar inside Drawer
    drawVectorString("VIRTUAL PIANO KEYBOARD", 20.0f, drawerY + 6.0f, 0.85f, theme.primaryAccent);
    drawVectorString("| TOUCH / DRAG GLISSANDO & VELOCITY", 210.0f, drawerY + 6.0f, 0.70f, theme.textMuted);

    // Octave controls
    // [< OCT] button
    drawRoundedRect(240.0f, drawerY + 3.0f, 60.0f, 22.0f, 3.0f,
                     theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 1.0f);
    drawRoundedRectOutline(240.0f, drawerY + 3.0f, 60.0f, 22.0f, 3.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 1.0f, 1.0f);
    drawVectorStringCentered("< OCT", 270.0f, drawerY + 14.0f, 0.70f, theme.textPrimary);

    // Current Octave Label
    std::string octText = "OCTAVE: C" + std::to_string(virtualKeyboardBaseOctave_);
    drawVectorString(octText, 320.0f, drawerY + 6.5f, 0.75f, theme.secondaryAccent);

    // [OCT >] button
    drawRoundedRect(420.0f, drawerY + 3.0f, 60.0f, 22.0f, 3.0f,
                     theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 1.0f);
    drawRoundedRectOutline(420.0f, drawerY + 3.0f, 60.0f, 22.0f, 3.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 1.0f, 1.0f);
    drawVectorStringCentered("OCT >", 450.0f, drawerY + 14.0f, 0.70f, theme.textPrimary);

    // Keyboard geometry
    const float kx = 16.0f;
    const float ky = drawerY + 28.0f;
    const float kw = static_cast<float>(width_) - 32.0f;
    const float kh = currentH - 34.0f;
    if (kh <= 10.0f) return;

    int startPitch = (virtualKeyboardBaseOctave_ + 1) * 12;
    int octavesCount = virtualPianoDrawerKeyboard_.getOctavesCount();
    int totalNotes = octavesCount * 12;
    int numWhiteKeys = octavesCount * 7;
    float whiteKeyW = kw / static_cast<float>(numWhiteKeys);
    float blackKeyW = whiteKeyW * 0.65f;
    float blackKeyH = kh * 0.60f;

    // Pass 1: Render White Keys
    int currWhite = 0;
    for (int p = startPitch; p < startPitch + totalNotes; ++p) {
        if (!PianoKeyboard::isBlackKey(p)) {
            float wx = kx + static_cast<float>(currWhite) * whiteKeyW;
            bool isPressed = (previewingPitch_ == p || virtualKeyboardActivePitch_ == p);

            if (isPressed) {
                drawRectGradient(wx, ky, whiteKeyW - 1.0f, kh,
                                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b,
                                 theme.primaryAccent.darken(0.35f).r, theme.primaryAccent.darken(0.35f).g, theme.primaryAccent.darken(0.35f).b);
                // Dynamic Velocity Fill Bar (extending from bottom up, matching top-is-higher velocity)
                float fillH = std::clamp(kh * previewingVelocity_, 4.0f, kh);
                drawRect(wx, ky + kh - fillH, whiteKeyW - 1.0f, fillH,
                         theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.90f);
            } else {
                drawRectGradient(wx, ky, whiteKeyW - 1.0f, kh, 0.92f, 0.94f, 0.97f, 0.76f, 0.78f, 0.82f);
            }
            drawRectOutline(wx, ky, whiteKeyW - 1.0f, kh, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.85f, 1.0f);

            // Note label on white key bottom lip
            if (p % 12 == 0) {
                std::string cLabel = "C" + std::to_string(p / 12 - 1);
                Color lblCol = isPressed ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textMuted;
                drawVectorString(cLabel, wx + 4.0f, ky + kh - 18.0f, 0.75f, lblCol);
            }
            currWhite++;
        }
    }

    // Pass 2: Render Black Keys
    currWhite = 0;
    for (int p = startPitch; p < startPitch + totalNotes; ++p) {
        if (PianoKeyboard::isBlackKey(p)) {
            float cx = kx + static_cast<float>(currWhite) * whiteKeyW;
            float bx = cx - blackKeyW * 0.5f;
            bool isPressed = (previewingPitch_ == p || virtualKeyboardActivePitch_ == p);

            if (isPressed) {
                drawRectGradient(bx, ky, blackKeyW, blackKeyH,
                                 theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b,
                                 theme.secondaryAccent.darken(0.40f).r, theme.secondaryAccent.darken(0.40f).g, theme.secondaryAccent.darken(0.40f).b);
                // Dynamic Velocity Fill Bar (extending from bottom up, matching top-is-higher velocity)
                float fillH = std::clamp(blackKeyH * previewingVelocity_, 4.0f, blackKeyH);
                drawRect(bx, ky + blackKeyH - fillH, blackKeyW, fillH,
                         theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.95f);
            } else {
                drawRectGradient(bx, ky, blackKeyW, blackKeyH, 0.22f, 0.24f, 0.30f, 0.08f, 0.09f, 0.12f);
            }
            drawRectOutline(bx, ky, blackKeyW, blackKeyH, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.90f, 1.0f);
        } else {
            currWhite++;
        }
    }
}

Rect GuiWindow::getMarqueeRect() const noexcept {
    float mx = std::min(marqueeStartX_, marqueeCurX_);
    float my = std::min(marqueeStartY_, marqueeCurY_);
    float mw = std::abs(marqueeCurX_ - marqueeStartX_);
    float mh = std::abs(marqueeCurY_ - marqueeStartY_);
    return {mx, my, mw, mh};
}

void GuiWindow::updatePianoRollMarqueeSelection(float startX, float startY, float curX, float curY, bool isShift) {
    if (!engine_ || selectedTrackIndex_ >= engine_->getSequencer().getNumTracks()) return;
    auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
    if (!tr) return;

    const int minPitch = pianoRollKeyboard_.getMinPitch();
    const int maxPitch = pianoRollKeyboard_.getMaxPitch();
    const float topY = 100.0f;
    const float rowH = pianoRollRowH_;
    const float gridX = 110.0f;
    const float stepW = pianoRollStepW_;

    float rLeft = std::min(startX, curX);
    float rRight = std::max(startX, curX);
    float rTop = std::min(startY, curY);
    float rBottom = std::max(startY, curY);

    std::vector<uint32_t> matchedSteps;
    for (uint32_t s = 0; s < tr->getNumSteps(); ++s) {
        const auto& st = tr->getStep(s);
        if (st.active && st.note >= minPitch && st.note <= maxPitch) {
            float nx = gridX + static_cast<float>(s) * stepW - pianoRollScrollX_ + 2.0f;
            float ny = topY + static_cast<float>(maxPitch - st.note) * rowH - pianoRollScrollY_ + 2.0f;
            float nw = std::max(14.0f, (stepW * st.gateLength) - 4.0f);
            float nh = rowH - 4.0f;

            if (nx < rRight && (nx + nw) > rLeft && ny < rBottom && (ny + nh) > rTop) {
                matchedSteps.push_back(s);
            }
        }
    }

    if (isShift) {
        for (uint32_t s : matchedSteps) {
            tr->selectStep(s, false);
        }
    } else {
        tr->selectSteps(matchedSteps, true);
    }
}

HitTestSelectionSidebarResult GuiWindow::hitTestSelectionSidebar(float mx, float my, float x, float y, float width, float height, const sequencer::SequencerTrack& track) const noexcept {
    HitTestSelectionSidebarResult res{};
    (void)track;
    if (mx < x || mx > x + width || my < y || my > y + height) return res;

    res.hit = true;

    // Header buttons
    float delX = x + width - 68.0f;
    float delY = y + 5.0f;
    if (mx >= delX && mx <= delX + 32.0f && my >= delY && my <= delY + 22.0f) {
        res.action = SelectionSidebarAction::Delete;
        return res;
    }

    float closeX = x + width - 30.0f;
    float closeY = y + 5.0f;
    if (mx >= closeX && mx <= closeX + 22.0f && my >= closeY && my <= closeY + 22.0f) {
        res.action = SelectionSidebarAction::Close;
        return res;
    }

    // Transpose buttons (y + 100)
    float curY = y + 100.0f;
    if (my >= curY && my <= curY + 26.0f) {
        if (mx >= x + 10.0f && mx <= x + 50.0f) {
            res.action = SelectionSidebarAction::TransposeOctDown;
            res.paramValue = -12;
            return res;
        }
        if (mx >= x + 54.0f && mx <= x + 90.0f) {
            res.action = SelectionSidebarAction::TransposeSemiDown;
            res.paramValue = -1;
            return res;
        }
        if (mx >= x + width - 90.0f && mx <= x + width - 54.0f) {
            res.action = SelectionSidebarAction::TransposeSemiUp;
            res.paramValue = 1;
            return res;
        }
        if (mx >= x + width - 50.0f && mx <= x + width - 10.0f) {
            res.action = SelectionSidebarAction::TransposeOctUp;
            res.paramValue = 12;
            return res;
        }
    }

    // Step nudge buttons (y + 144)
    curY = y + 144.0f;
    if (my >= curY && my <= curY + 30.0f) {
        if (mx >= x + width - 128.0f && mx <= x + width - 72.0f) {
            res.action = SelectionSidebarAction::NudgeLeft;
            res.paramValue = -1;
            return res;
        }
        if (mx >= x + width - 68.0f && mx <= x + width - 12.0f) {
            res.action = SelectionSidebarAction::NudgeRight;
            res.paramValue = 1;
            return res;
        }
    }

    // Duration buttons (y + 174..220)
    curY = y + 174.0f;
    if (my >= curY - 4.0f && my <= curY + 18.0f) {
        if (mx >= x + width - 128.0f && mx <= x + width - 72.0f) {
            res.action = SelectionSidebarAction::ShortenDuration;
            res.floatValue = -0.25f;
            return res;
        }
        if (mx >= x + width - 68.0f && mx <= x + width - 12.0f) {
            res.action = SelectionSidebarAction::LengthenDuration;
            res.floatValue = 0.25f;
            return res;
        }
    }

    // Quick duration chips (y + 196)
    float chipY = curY + 22.0f;
    if (my >= chipY && my <= chipY + 22.0f) {
        if (mx >= x + 10.0f && mx <= x + 64.0f) {
            res.action = SelectionSidebarAction::DurPreset25;
            res.floatValue = 0.25f;
            return res;
        }
        if (mx >= x + 68.0f && mx <= x + 122.0f) {
            res.action = SelectionSidebarAction::DurPreset50;
            res.floatValue = 0.50f;
            return res;
        }
        if (mx >= x + 126.0f && mx <= x + 180.0f) {
            res.action = SelectionSidebarAction::DurPreset75;
            res.floatValue = 0.75f;
            return res;
        }
        if (mx >= x + 184.0f && mx <= x + 238.0f) {
            res.action = SelectionSidebarAction::DurPreset100;
            res.floatValue = 1.00f;
            return res;
        }
    }

    // Velocity buttons (y + 244)
    curY = y + 244.0f;
    if (my >= curY && my <= curY + 22.0f) {
        if (mx >= x + 10.0f && mx <= x + 52.0f) {
            res.action = SelectionSidebarAction::Vel25;
            res.floatValue = 0.25f;
            return res;
        }
        if (mx >= x + 56.0f && mx <= x + 98.0f) {
            res.action = SelectionSidebarAction::Vel50;
            res.floatValue = 0.50f;
            return res;
        }
        if (mx >= x + 102.0f && mx <= x + 144.0f) {
            res.action = SelectionSidebarAction::Vel75;
            res.floatValue = 0.75f;
            return res;
        }
        if (mx >= x + 148.0f && mx <= x + 190.0f) {
            res.action = SelectionSidebarAction::Vel100;
            res.floatValue = 1.00f;
            return res;
        }
        if (mx >= x + 194.0f && mx <= x + 254.0f) {
            res.action = SelectionSidebarAction::Humanize;
            return res;
        }
    }

    // Expression (y + 294)
    curY = y + 294.0f;
    if (my >= curY && my <= curY + 26.0f) {
        if (mx >= x + 10.0f && mx <= x + 125.0f) {
            res.action = SelectionSidebarAction::ToggleSlide;
            return res;
        }
        if (mx >= x + 135.0f && mx <= x + 250.0f) {
            res.action = SelectionSidebarAction::ToggleAccent;
            return res;
        }
    }

    // Selection utilities (y + 346..400)
    curY = y + 346.0f;
    if (my >= curY && my <= curY + 24.0f) {
        if (mx >= x + 10.0f && mx <= x + 125.0f) {
            res.action = SelectionSidebarAction::SelectAll;
            return res;
        }
        if (mx >= x + 135.0f && mx <= x + 250.0f) {
            res.action = SelectionSidebarAction::Invert;
            return res;
        }
    }

    curY += 28.0f;
    if (my >= curY && my <= curY + 24.0f) {
        if (mx >= x + 10.0f && mx <= x + 125.0f) {
            res.action = SelectionSidebarAction::Quantize;
            return res;
        }
        if (mx >= x + 135.0f && mx <= x + 250.0f) {
            res.action = SelectionSidebarAction::Clear;
            return res;
        }
    }

    return res;
}

void GuiWindow::drawNoteSelectionSidebar(float x, float y, float width, float height, sequencer::SequencerTrack& track) {
    const auto& selected = track.getSelectedSteps();
    if (selected.empty()) return;

    const bool isSingle = (selected.size() == 1);
    uint32_t firstStep = *selected.begin();
    const auto& firstNote = track.getStep(firstStep);

    // 1. Chassis Background & Drop Shadow
    drawRect(x - 3.0f, y, 3.0f, height, 0.0f, 0.0f, 0.0f, 0.45f);
    drawRectGradient(x, y, width, height, 0.09f, 0.10f, 0.13f, 0.06f, 0.07f, 0.09f);
    drawRectOutline(x, y, width, height, 0.0f, 0.80f, 0.95f, 0.85f, 1.5f);

    // 2. Header Bar (Y: y .. y + 32)
    drawRectGradient(x, y, width, 32.0f, 0.14f, 0.16f, 0.22f, 0.08f, 0.09f, 0.13f);
    drawLine(x, y + 32.0f, x + width, y + 32.0f, 0.24f, 0.28f, 0.38f, 1.0f, 1.0f);

    // Track color indicator dot
    drawCircle(x + 14.0f, y + 16.0f, 4.0f, 0.0f, 0.95f, 1.0f);

    // Header Title
    std::string title = isSingle ? "NOTE INSPECTOR" : "MULTI-NOTE INSPECTOR";
    drawVectorString(title, x + 24.0f, y + 10.0f, 0.78f, 0.90f, 0.95f, 1.0f);

    // Delete Button [x + width - 68, y + 5, 32, 22]
    float delX = x + width - 68.0f;
    float delY = y + 5.0f;
    drawRectGradient(delX, delY, 32.0f, 22.0f, 0.35f, 0.08f, 0.10f, 0.18f, 0.04f, 0.05f);
    drawRectOutline(delX, delY, 32.0f, 22.0f, 0.85f, 0.20f, 0.20f, 1.0f, 1.0f);
    drawVectorString("DEL", delX + 6.0f, delY + 5.0f, 0.70f, 1.0f, 0.40f, 0.40f);

    // Close Button [x + width - 30, y + 5, 22, 22]
    float closeX = x + width - 30.0f;
    float closeY = y + 5.0f;
    bool closeHov = (mouseX_ >= closeX - 2.0f && mouseX_ <= closeX + 24.0f && mouseY_ >= closeY - 2.0f && mouseY_ <= closeY + 24.0f) ||
                    (std::hypot(mouseX_ - (closeX + 11.0f), mouseY_ - (closeY + 11.0f)) <= 12.0f);
    drawIconScrewClose(closeX + 11.0f, closeY + 11.0f, 8.5f, closeHov, getTheme().primaryAccent);

    // 3. Summary Box (Y: y + 38 .. y + 78)
    float sumY = y + 38.0f;
    drawRectGradient(x + 8.0f, sumY, width - 16.0f, 40.0f, 0.06f, 0.18f, 0.26f, 0.03f, 0.09f, 0.14f);
    drawRectOutline(x + 8.0f, sumY, width - 16.0f, 40.0f, 0.0f, 0.85f, 1.0f, 0.7f, 1.0f);

    static const char* kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (isSingle) {
        std::string nName = std::string(kNoteNames[firstNote.note % 12]) + std::to_string(firstNote.note / 12 - 1);
        drawVectorString(nName, x + 16.0f, sumY + 8.0f, 1.15f, 0.0f, 1.0f, 0.95f);

        std::string stepInfo = "STEP " + std::to_string(firstStep) + " (" + std::to_string(static_cast<int>(firstNote.gateLength * 100)) + "% dur)";
        drawVectorString(stepInfo, x + 16.0f, sumY + 24.0f, 0.70f, 0.70f, 0.78f, 0.90f);

        std::string velInfo = "VEL: " + std::to_string(static_cast<int>(firstNote.velocity * 100)) + "%";
        drawVectorString(velInfo, x + width - 85.0f, sumY + 12.0f, 0.80f, 1.0f, 0.85f, 0.20f);
    } else {
        std::string countStr = std::to_string(selected.size()) + " NOTES SELECTED";
        drawVectorString(countStr, x + 16.0f, sumY + 12.0f, 0.90f, 0.0f, 1.0f, 0.95f);

        // [BATCH MODE] Badge
        float badgeX = x + width - 105.0f;
        drawRectGradient(badgeX, sumY + 9.0f, 90.0f, 22.0f, 0.30f, 0.22f, 0.05f, 0.15f, 0.11f, 0.02f);
        drawRectOutline(badgeX, sumY + 9.0f, 90.0f, 22.0f, 1.0f, 0.80f, 0.10f, 1.0f, 1.0f);
        drawVectorString("BATCH MODE", badgeX + 8.0f, sumY + 14.0f, 0.70f, 1.0f, 0.85f, 0.20f);
    }

    // 4. Section: PITCH TRANSPOSE (Y: y + 84)
    float curY = y + 84.0f;
    drawVectorString("PITCH TRANSPOSE", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    curY += 16.0f;

    auto drawBtn = [&](float bx, float by, float bw, float bh, const std::string& label, float tr, float tg, float tb) {
        drawRectGradient(bx, by, bw, bh, 0.12f, 0.14f, 0.18f, 0.07f, 0.08f, 0.11f);
        drawRectOutline(bx, by, bw, bh, 0.22f, 0.26f, 0.35f, 1.0f, 1.0f);
        drawVectorString(label, bx + (bw - label.size() * 7.5f) * 0.5f, by + (bh - 10.0f) * 0.5f, 0.75f, tr, tg, tb);
    };

    drawBtn(x + 10.0f, curY, 40.0f, 26.0f, "-12", 0.0f, 0.95f, 1.0f);
    drawBtn(x + 54.0f, curY, 36.0f, 26.0f, "-1", 0.0f, 0.85f, 1.0f);

    // Center badge
    float badgeW = width - 188.0f;
    float badgePosX = x + 94.0f;
    drawRect(badgePosX, curY, badgeW, 26.0f, 0.05f, 0.07f, 0.10f);
    drawRectOutline(badgePosX, curY, badgeW, 26.0f, 0.0f, 0.85f, 1.0f, 0.6f, 1.0f);
    std::string pitchBadge = isSingle ? (std::string(kNoteNames[firstNote.note % 12]) + std::to_string(firstNote.note / 12 - 1)) : "+-PITCH";
    drawVectorString(pitchBadge, badgePosX + (badgeW - pitchBadge.size() * 7.5f) * 0.5f, curY + 7.0f, 0.78f, 0.0f, 1.0f, 0.95f);

    drawBtn(x + width - 90.0f, curY, 36.0f, 26.0f, "+1", 0.0f, 0.85f, 1.0f);
    drawBtn(x + width - 50.0f, curY, 40.0f, 26.0f, "+12", 0.0f, 0.95f, 1.0f);
    curY += 34.0f;

    // 5. Section: POSITION (STEP NUDGE)
    drawVectorString("POSITION (STEP NUDGE)", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    std::string nudgeLabel = isSingle ? ("Step " + std::to_string(firstStep)) : "Shift in Time";
    drawVectorString(nudgeLabel, x + 10.0f, curY + 16.0f, 0.75f, 1.0f, 0.85f, 0.20f);
    drawBtn(x + width - 128.0f, curY + 10.0f, 56.0f, 24.0f, "-STEP", 1.0f, 0.85f, 0.20f);
    drawBtn(x + width - 68.0f, curY + 10.0f, 56.0f, 24.0f, "+STEP", 1.0f, 0.85f, 0.20f);
    curY += 40.0f;

    // 6. Section: LENGTH / DURATION
    drawVectorString("LENGTH / DURATION", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    drawBtn(x + width - 128.0f, curY - 4.0f, 56.0f, 22.0f, "-LEN", 0.0f, 0.90f, 1.0f);
    drawBtn(x + width - 68.0f, curY - 4.0f, 56.0f, 22.0f, "+LEN", 0.0f, 0.90f, 1.0f);
    curY += 22.0f;

    // Quick duration chips: [0.25] [0.50] [0.75] [1.00]
    drawBtn(x + 10.0f, curY, 54.0f, 22.0f, "0.25", 0.75f, 0.80f, 0.90f);
    drawBtn(x + 68.0f, curY, 54.0f, 22.0f, "0.50", 0.75f, 0.80f, 0.90f);
    drawBtn(x + 126.0f, curY, 54.0f, 22.0f, "0.75", 0.75f, 0.80f, 0.90f);
    drawBtn(x + 184.0f, curY, 54.0f, 22.0f, "1.00", 0.75f, 0.80f, 0.90f);
    curY += 32.0f;

    // 7. Section: VELOCITY
    float avgVel = 0.0f;
    for (uint32_t s : selected) avgVel += track.getStep(s).velocity;
    avgVel /= static_cast<float>(selected.size());
    int avgVelPct = static_cast<int>(avgVel * 100.0f);
    drawVectorString("VELOCITY (" + std::to_string(avgVelPct) + "% AVG)", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    curY += 16.0f;

    drawBtn(x + 10.0f, curY, 42.0f, 22.0f, "25%", 0.0f, 0.85f, 1.0f);
    drawBtn(x + 56.0f, curY, 42.0f, 22.0f, "50%", 0.0f, 0.85f, 1.0f);
    drawBtn(x + 102.0f, curY, 42.0f, 22.0f, "75%", 0.0f, 0.85f, 1.0f);
    drawBtn(x + 148.0f, curY, 42.0f, 22.0f, "100%", 0.0f, 0.85f, 1.0f);
    drawBtn(x + 194.0f, curY, 60.0f, 22.0f, "HUMAN", 1.0f, 0.85f, 0.20f);
    curY += 34.0f;

    // 8. Section: EXPRESSION / TB-303
    drawVectorString("EXPRESSION / TB-303", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    curY += 16.0f;

    bool isSlide = firstNote.slide;
    bool isAccent = firstNote.accent;

    if (isSlide) {
        drawRectGradient(x + 10.0f, curY, 115.0f, 26.0f, 0.0f, 0.35f, 0.45f, 0.0f, 0.18f, 0.25f);
        drawRectOutline(x + 10.0f, curY, 115.0f, 26.0f, 0.0f, 0.95f, 1.0f, 1.0f, 1.4f);
        drawVectorString("SLIDE: ON", x + 24.0f, curY + 7.0f, 0.75f, 0.0f, 1.0f, 1.0f);
    } else {
        drawBtn(x + 10.0f, curY, 115.0f, 26.0f, "SLIDE: OFF", 0.50f, 0.55f, 0.65f);
    }

    if (isAccent) {
        drawRectGradient(x + 135.0f, curY, 115.0f, 26.0f, 0.35f, 0.25f, 0.05f, 0.18f, 0.12f, 0.02f);
        drawRectOutline(x + 135.0f, curY, 115.0f, 26.0f, 1.0f, 0.85f, 0.10f, 1.0f, 1.4f);
        drawVectorString("ACCENT: ON", x + 144.0f, curY + 7.0f, 0.75f, 1.0f, 0.88f, 0.20f);
    } else {
        drawBtn(x + 135.0f, curY, 115.0f, 26.0f, "ACCENT: OFF", 0.50f, 0.55f, 0.65f);
    }
    curY += 36.0f;

    // 9. Section: SELECTION UTILITIES
    drawVectorString("SELECTION UTILITIES", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    curY += 16.0f;

    drawBtn(x + 10.0f, curY, 115.0f, 24.0f, "SELECT ALL", 0.75f, 0.85f, 0.95f);
    drawBtn(x + 135.0f, curY, 115.0f, 24.0f, "INVERT", 0.75f, 0.85f, 0.95f);
    curY += 28.0f;

    drawBtn(x + 10.0f, curY, 115.0f, 24.0f, "QUANTIZE", 0.0f, 0.90f, 1.0f);
    drawBtn(x + 135.0f, curY, 115.0f, 24.0f, "CLEAR", 0.80f, 0.45f, 0.45f);
}

HitTestTrackerResult GuiWindow::hitTestTracker(float x, float y) const noexcept {
    HitTestTrackerResult res{};
    if (activeView_ != WorkspaceView::Edit && activeView_ != WorkspaceView::Tracker) return res;
    if (editSubView_ != EditSubView::Tracker) return res;

    const size_t numTracks = engine_ ? engine_->getSequencer().getNumTracks() : 5;
    const float startX = 95.0f;
    const float endX = static_cast<float>(width_) - 25.0f;
    const float colW = (numTracks > 0) ? std::max(180.0f, (endX - startX) / static_cast<float>(numTracks)) : 220.0f;

    // 1. Column Headers (Y = 135..165)
    if (y >= 135.0f && y <= 165.0f) {
        if (x >= startX && x <= (startX + colW * numTracks)) {
            uint32_t t = static_cast<uint32_t>((x - startX) / colW);
            if (t < numTracks) {
                res.hit = true;
                res.isHeader = true;
                res.trackIndex = t;
                float hx = startX + t * colW;
                if (x >= (hx + colW - 54.0f) && x <= (hx + colW - 32.0f)) {
                    res.isMute = true;
                } else if (x >= (hx + colW - 28.0f) && x <= (hx + colW - 6.0f)) {
                    res.isSolo = true;
                }
                return res;
            }
        }
    }

    // 2. Tracker Rows (Y = 170..bPanelY-6)
    const float topY = 170.0f;
    const float bottomY = (static_cast<float>(height_) - 48.0f) - 6.0f;
    const float rowH = 24.0f;
    const int visibleRows = static_cast<int>((bottomY - topY) / rowH); // 23 rows

    if (y >= topY && y <= bottomY && x >= 30.0f) {
        int startRow = 0;
        if (trackerFollowPlayback_ && engine_ && engine_->getSequencer().isPlaying()) {
            int playRow = static_cast<int>(engine_->getSequencer().getTransport().getCurrentStep() % 64);
            startRow = std::clamp(playRow - 11, 0, 64 - visibleRows);
        } else {
            startRow = std::clamp(static_cast<int>(trackerSelectedRow_) - 11, 0, 64 - visibleRows);
        }

        int rowOffset = static_cast<int>((y - topY) / rowH);
        uint32_t r = static_cast<uint32_t>(startRow + rowOffset);
        if (r < 64) {
            res.hit = true;
            res.row = r;
            if (x >= startX && x <= (startX + colW * numTracks)) {
                uint32_t t = static_cast<uint32_t>((x - startX) / colW);
                if (t < numTracks) {
                    res.trackIndex = t;
                }
            } else {
                res.trackIndex = trackerSelectedTrack_;
            }
            return res;
        }
    }

    return res;
}

int GuiWindow::qwertyKeyToMidiPitch(int key, int baseOctave) noexcept {
    int baseMidi = baseOctave * 12 + 12; // Base Octave 4: 60 (Middle C)

    switch (key) {
        // Lower Octave (Z S X D C V G B H N J M -> C to B)
        case 90: return baseMidi + 0;  // Z -> C
        case 83: return baseMidi + 1;  // S -> C#
        case 88: return baseMidi + 2;  // X -> D
        case 68: return baseMidi + 3;  // D -> D#
        case 67: return baseMidi + 4;  // C -> E
        case 86: return baseMidi + 5;  // V -> F
        case 71: return baseMidi + 6;  // G -> F#
        case 66: return baseMidi + 7;  // B -> G
        case 72: return baseMidi + 8;  // H -> G#
        case 78: return baseMidi + 9;  // N -> A
        case 74: return baseMidi + 10; // J -> A#
        case 77: return baseMidi + 11; // M -> B

        // Upper Octave (baseOctave + 1): Q 2 W 3 E R 5 T 6 Y 7 U I -> C5 to C6
        case 81: return baseMidi + 12; // Q -> C5
        case 50: return baseMidi + 13; // 2 -> C#5
        case 87: return baseMidi + 14; // W -> D5
        case 51: return baseMidi + 15; // 3 -> D#5
        case 69: return baseMidi + 16; // E -> E5
        case 82: return baseMidi + 17; // R -> F5
        case 53: return baseMidi + 18; // 5 -> F#5
        case 84: return baseMidi + 19; // T -> G5
        case 54: return baseMidi + 20; // 6 -> G#5
        case 89: return baseMidi + 21; // Y -> A5
        case 55: return baseMidi + 22; // 7 -> A#5
        case 85: return baseMidi + 23; // U -> B5
        case 73: return baseMidi + 24; // I -> C6

        default: return -1;
    }
}

static float scoreDurationToSteps(ScoreNoteType dur) {
    switch (dur) {
        case ScoreNoteType::Whole: return 16.0f;
        case ScoreNoteType::Half: return 8.0f;
        case ScoreNoteType::Quarter: return 4.0f;
        case ScoreNoteType::Eighth: return 2.0f;
        case ScoreNoteType::Sixteenth: return 1.0f;
    }
    return 4.0f;
}

HitTestScoreResult GuiWindow::hitTestScore(float x, float y) const noexcept {
    HitTestScoreResult res{};
    if (activeView_ != WorkspaceView::Edit && activeView_ != WorkspaceView::Tracker) return res;
    if (editSubView_ != EditSubView::Score) return res;

    // 1. Toolbar button hit testing: Y in [100..132]
    if (y >= 100.0f && y <= 132.0f) {
        // Clefs:
        // [ GRAND ] : [80..144]
        if (x >= 80.0f && x <= 144.0f) {
            res.hit = true;
            res.action = ScoreHitAction::ClefGrandStaff;
            return res;
        }
        // [ TREBLE ] : [148..212]
        if (x >= 148.0f && x <= 212.0f) {
            res.hit = true;
            res.action = ScoreHitAction::ClefTreble;
            return res;
        }
        // [ BASS ] : [216..270]
        if (x >= 216.0f && x <= 270.0f) {
            res.hit = true;
            res.action = ScoreHitAction::ClefBass;
            return res;
        }

        // Note durations:
        // [ 1 ] : [330..364]
        if (x >= 330.0f && x <= 364.0f) {
            res.hit = true;
            res.action = ScoreHitAction::DurationWhole;
            return res;
        }
        // [ 1/2 ] : [368..408]
        if (x >= 368.0f && x <= 408.0f) {
            res.hit = true;
            res.action = ScoreHitAction::DurationHalf;
            return res;
        }
        // [ 1/4 ] : [412..452]
        if (x >= 412.0f && x <= 452.0f) {
            res.hit = true;
            res.action = ScoreHitAction::DurationQuarter;
            return res;
        }
        // [ 1/8 ] : [456..496]
        if (x >= 456.0f && x <= 496.0f) {
            res.hit = true;
            res.action = ScoreHitAction::DurationEighth;
            return res;
        }
        // [ 1/16 ] : [500..546]
        if (x >= 500.0f && x <= 546.0f) {
            res.hit = true;
            res.action = ScoreHitAction::DurationSixteenth;
            return res;
        }

        // Accidentals:
        // [ NAT ] : [600..642]
        if (x >= 600.0f && x <= 642.0f) {
            res.hit = true;
            res.action = ScoreHitAction::AccidentalNone;
            return res;
        }
        // [ # ] : [646..678]
        if (x >= 646.0f && x <= 678.0f) {
            res.hit = true;
            res.action = ScoreHitAction::AccidentalSharp;
            return res;
        }
        // [ b ] : [682..714]
        if (x >= 682.0f && x <= 714.0f) {
            res.hit = true;
            res.action = ScoreHitAction::AccidentalFlat;
            return res;
        }
    }

    // 2. Grand Staff interactive note placement:
    // X in [140 .. width - 60], Y in [160 .. 380]
    const float staffLeftX = 140.0f;
    const float staffRightX = static_cast<float>(width_) - 60.0f;
    if (x >= staffLeftX && x <= staffRightX && y >= 160.0f && y <= 380.0f) {
        float stepW = (staffRightX - staffLeftX) / 16.0f;
        uint32_t step = std::clamp(static_cast<uint32_t>((x - staffLeftX) / stepW), 0u, 15u);

        const float sp = scoreStaffSpace_; // 14.0f
        const float trebleLine1Y = 256.0f;
        const float bassLine1Y = 340.0f;

        bool isTreble = (scoreClef_ == ScoreClef::Treble) ? true :
                       ((scoreClef_ == ScoreClef::Bass) ? false : (y <= 270.0f));

        float l1Y = isTreble ? trebleLine1Y : bassLine1Y;
        int midi = ScoreLayoutEngine::yToMidiPitch(y, l1Y, sp, isTreble, scoreAccidental_);

        res.hit = true;
        res.action = ScoreHitAction::StaffClick;
        res.step = step;
        res.pitch = static_cast<uint8_t>(midi);
        res.isTrebleStaff = isTreble;

        if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
            auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
            if (tr) {
                auto st = tr->getStep(step);
                if (st.active && st.note == res.pitch) {
                    res.hasExistingNote = true;
                }
            }
        }
        return res;
    }

    return res;
}

HitTestArrangerResult GuiWindow::hitTestArranger(float x, float y) const noexcept {
    HitTestArrangerResult res{};
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float topY = 56.0f;
    const float rulerH = 22.0f;
    const float trackHeaderW = 210.0f;
    const float barW = 60.0f;
    const float trackH = 76.0f;
    const float bottomY = h - 48.0f;
    const float miniY = bottomY - 32.0f;
    const float miniH = 22.0f;

    const float propW = arrangerPropertiesExpanded_ ? arrangerPropertiesWidth_ : 0.0f;
    const float pullTabW = kArrangerPullTabW;
    const float drawerTotalW = arrangerPropertiesExpanded_ ? (pullTabW + propW) : pullTabW;
    const float browserOffset = browserOpen_ ? ProjectBrowserDrawer::getDrawerWidth() : 0.0f;
    const float rightBoundary = w - drawerTotalW - browserOffset;
    const float pullTabX = rightBoundary;
    const float propX = pullTabX + pullTabW;

    // 0. Pull-Tab Strip Hit (X in [pullTabX, pullTabX + pullTabW])
    if (x >= pullTabX && x <= pullTabX + pullTabW && y >= topY && y <= bottomY) {
        res.hit = true;
        res.area = ArrangerHitArea::SidebarPullTab;
        return res;
    }

    // 0.1 Expanded Properties Sidebar Body Hit
    if (arrangerPropertiesExpanded_ && x > pullTabX + pullTabW && x <= propX + propW && y >= topY && y <= bottomY) {
        res.hit = true;
        // Close Button [propX + propW - 28, topY + 7, 22, 22]
        float clBtnX = propX + propW - 28.0f;
        float clBtnY = topY + 7.0f;
        if (x >= clBtnX && x <= clBtnX + 22.0f && y >= clBtnY && y <= clBtnY + 22.0f) {
            res.area = ArrangerHitArea::SidebarCloseButton;
            return res;
        }

        // Segmented Tab Switcher [Y = topY + 44..topY + 72]
        float tabY = topY + 44.0f;
        float halfW = (propW - 26.0f) * 0.5f;
        if (y >= tabY && y <= tabY + 28.0f) {
            if (x >= propX + 10.0f && x <= propX + 10.0f + halfW) {
                res.area = ArrangerHitArea::SidebarTabTrack;
                return res;
            } else if (x >= propX + 14.0f + halfW && x <= propX + 14.0f + 2.0f * halfW) {
                res.area = ArrangerHitArea::SidebarTabClip;
                return res;
            }
        }

        float cardX = propX + 10.0f;
        float cardW = propW - 20.0f;
        float cardY = topY + 80.0f;

        if (arrangerInspectorTab_ == ArrangerInspectorTab::Track) {
            float cardH = bottomY - cardY - 8.0f;
            auto inspHit = hitTestTrackPropertiesContainer(x, y, cardX, cardY, cardW, cardH, selectedTrackIndex_, trackInspectorScrollY_, true);
            if (inspHit.hit) {
                res.hit = true;
                res.trackInspectorHit = inspHit;
                if (inspHit.area == TrackInspectorHitArea::VolumeSlider) {
                    res.area = ArrangerHitArea::TrackVolume;
                    res.trackIndex = selectedTrackIndex_;
                    res.normVal = inspHit.normVal;
                } else if (inspHit.area == TrackInspectorHitArea::PanKnob) {
                    res.area = ArrangerHitArea::TrackPan;
                    res.trackIndex = selectedTrackIndex_;
                    res.normVal = inspHit.normVal;
                } else if (inspHit.area == TrackInspectorHitArea::MuteButton) {
                    res.area = ArrangerHitArea::TrackMute;
                    res.trackIndex = selectedTrackIndex_;
                } else if (inspHit.area == TrackInspectorHitArea::SoloButton) {
                    res.area = ArrangerHitArea::TrackSolo;
                    res.trackIndex = selectedTrackIndex_;
                } else if (inspHit.area == TrackInspectorHitArea::ColorSwatch) {
                    res.area = ArrangerHitArea::SidebarColorSwatch;
                    res.trackIndex = selectedTrackIndex_;
                    res.colorSwatchIndex = inspHit.colorSwatchIndex;
                } else {
                    res.area = ArrangerHitArea::SidebarTrackInspector;
                }
                return res;
            }
        } else {
            // Clip Tab
            if (selectedArrangerClipTrack_ >= 0 && selectedArrangerClipIndex_ >= 0) {
                // Loop toggle button
                float loopCardY = cardY + 60.0f;
                float loopBtnX = cardX + cardW - 74.0f;
                float loopBtnY = loopCardY + 26.0f;
                if (x >= loopBtnX && x <= loopBtnX + 64.0f && y >= loopBtnY && y <= loopBtnY + 26.0f) {
                    res.area = ArrangerHitArea::SidebarClipLoopToggle;
                    return res;
                }

                // Pitch & Transpose Card
                float transCardY = cardY + 142.0f;
                if (y >= transCardY + 28.0f && y <= transCardY + 56.0f) {
                    float transBtnW = (cardW - 32.0f) / 5.0f;
                    for (int tb = 0; tb < 5; ++tb) {
                        float tbx = cardX + 10.0f + tb * (transBtnW + 3.0f);
                        if (x >= tbx && x <= tbx + transBtnW) {
                            res.area = ArrangerHitArea::SidebarClipTranspose;
                            if (tb == 0) res.transposeDelta = -12;
                            else if (tb == 1) res.transposeDelta = -1;
                            else if (tb == 2) res.transposeDelta = 1;
                            else if (tb == 3) res.transposeDelta = 12;
                            else res.transposeDelta = 0;
                            return res;
                        }
                    }
                }

                // Clip Actions Card
                float clipActY = cardY + 220.0f;
                if (y >= clipActY + 24.0f && y <= clipActY + 42.0f && x >= cardX + 10.0f && x <= cardX + cardW - 10.0f) {
                    res.area = ArrangerHitArea::SidebarAction;
                    res.actionName = "QuantizeClip";
                    return res;
                }
                if (y >= clipActY + 46.0f && y <= clipActY + 64.0f && x >= cardX + 10.0f && x <= cardX + cardW - 10.0f) {
                    res.area = ArrangerHitArea::SidebarAction;
                    res.actionName = "DuplicateClip";
                    return res;
                }
                if (y >= clipActY + 68.0f && y <= clipActY + 86.0f && x >= cardX + 10.0f && x <= cardX + cardW - 10.0f) {
                    res.area = ArrangerHitArea::SidebarAction;
                    res.actionName = "DeleteClip";
                    return res;
                }
            }
        }
        return res;
    }

    // 1. Time Ruler scrubbing: Y in [topY, topY + rulerH]
    if (y >= topY && y <= topY + rulerH) {
        if (x >= trackHeaderW && x <= rightBoundary) {
            res.hit = true;
            res.area = ArrangerHitArea::Ruler;
            float barProgress = (x - trackHeaderW) / barW;
            res.bar = static_cast<uint32_t>(barProgress) + 1;
            res.step = static_cast<uint32_t>(std::clamp(barProgress * 16.0f, 0.0f, 18.0f * 16.0f));
            res.normVal = std::clamp((x - trackHeaderW) / (rightBoundary - trackHeaderW), 0.0f, 1.0f);
            return res;
        }
    }

    // 2. Track Lanes and Headers
    for (uint32_t t = 0; t < 5; ++t) {
        float curY = topY + rulerH + t * trackH;
        if (curY + trackH > bottomY - 30.0f) break;

        if (y >= curY && y < curY + trackH) {
            res.trackIndex = t;

            if (x < trackHeaderW) {
                // Row 1: Mute, Solo, Freeze buttons
                // Mute button: X in [128, 153], Y in [curY + 6, curY + 30]
                if (x >= 128.0f && x <= 153.0f && y >= curY + 6.0f && y <= curY + 30.0f) {
                    res.hit = true;
                    res.area = ArrangerHitArea::TrackMute;
                    return res;
                }
                // Solo button: X in [154, 179], Y in [curY + 6, curY + 30]
                if (x >= 154.0f && x <= 179.0f && y >= curY + 6.0f && y <= curY + 30.0f) {
                    res.hit = true;
                    res.area = ArrangerHitArea::TrackSolo;
                    return res;
                }
                // Freeze button: X in [180, 206], Y in [curY + 6, curY + 30]
                if (x >= 180.0f && x <= 206.0f && y >= curY + 6.0f && y <= curY + 30.0f) {
                    res.hit = true;
                    res.area = ArrangerHitArea::TrackFreeze;
                    return res;
                }

                // Row 2: Volume slider & meter, Track Pan knob
                // Volume slider: X in [36, 156], Y in [curY + 38, curY + 64]
                if (x >= 36.0f && x <= 156.0f && y >= curY + 38.0f && y <= curY + 64.0f) {
                    res.hit = true;
                    res.area = ArrangerHitArea::TrackVolume;
                    res.normVal = std::clamp((x - 38.0f) / 114.0f, 0.0f, 1.0f);
                    return res;
                }
                // Track pan knob: X in [168, 202], Y in [curY + 38, curY + 68]
                if (x >= 168.0f && x <= 202.0f && y >= curY + 38.0f && y <= curY + 68.0f) {
                    res.hit = true;
                    res.area = ArrangerHitArea::TrackPan;
                    return res;
                }
                // Entire track header card
                res.hit = true;
                res.area = ArrangerHitArea::TrackHeader;
                return res;
            } else if (x >= trackHeaderW && x <= rightBoundary) {
                // Timeline lane
                float barProgress = (x - trackHeaderW) / barW;
                res.bar = static_cast<uint32_t>(barProgress) + 1;
                res.step = static_cast<uint32_t>(std::clamp(barProgress * 16.0f, 0.0f, 18.0f * 16.0f));

                // Check clip bounds for tracks
                if (t < arrangerTracks_.size() && !arrangerTracks_[t].clips.empty()) {
                    for (size_t c = 0; c < arrangerTracks_[t].clips.size(); ++c) {
                        const auto& clp = arrangerTracks_[t].clips[c];
                        float cX = trackHeaderW + (clp.startBar - 1) * barW;
                        float cW = clp.barLength * barW;
                        if (x >= cX && x <= cX + cW) {
                            res.hit = true;
                            res.area = ArrangerHitArea::Clip;
                            res.clipIndex = static_cast<int>(c);
                            return res;
                        }
                    }
                } else {
                    bool inClip = false;
                    if (t == 0) {
                        if (x >= trackHeaderW && x <= trackHeaderW + 8.0f * barW) inClip = true;
                    } else if (t == 1 || t == 2) {
                        if (x >= trackHeaderW && x <= trackHeaderW + 8.0f * barW) inClip = true;
                    } else if (t == 3) {
                        if (x >= trackHeaderW + 4.0f * barW && x <= trackHeaderW + 12.0f * barW) inClip = true;
                    }

                    if (inClip) {
                        res.hit = true;
                        res.area = ArrangerHitArea::Clip;
                        res.clipIndex = 0;
                        return res;
                    }
                }
            }
        }
    }

    // 3. Minimap Overview Scrollbar
    if (y >= miniY && y <= miniY + miniH) {
        float miniTrackX = trackHeaderW;
        float miniTrackW = rightBoundary - trackHeaderW - 20.0f;
        if (x >= miniTrackX && x <= miniTrackX + miniTrackW) {
            res.hit = true;
            res.area = ArrangerHitArea::OverviewScrollbar;
            res.normVal = std::clamp((x - miniTrackX) / miniTrackW, 0.0f, 1.0f);
            return res;
        }
    }

    return res;
}

void GuiWindow::onMouseMove(float x, float y) {
    ColorPickerDialog* activeColorPicker = getActiveColorPickerDialog();
    if (activeColorPicker && activeColorPicker->isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        activeColorPicker->handlePointer(pev);
        mouseX_ = x;
        mouseY_ = y;
        return;
    }

    if (projectHubOpen_) {
        if (dragMode_ == DragMode::ProjectHubScroll) {
            if (projectHubScrollArea_.canScroll()) {
                const float deltaY = y - dragStartY_;
                if (projectHubDraggingThumb_) {
                    PointerEvent pev;
                    pev.type = PointerType::Mouse;
                    pev.action = PointerAction::Move;
                    pev.x = x;
                    pev.y = y;
                    projectHubScrollArea_.handlePointer(pev);
                    projectHubScrollY_ = projectHubScrollArea_.getScrollY();
                } else {
                    projectHubScrollArea_.setScrollY(dragStartScrollY_ - deltaY);
                    projectHubScrollY_ = projectHubScrollArea_.getScrollY();
                }
            }
        } else if (dragMode_ == DragMode::UiScaleSlider) {
            DialogLayout dl = computeDialogLayout(540.0f, 620.0f);
            float sliderX = dl.x + 24.0f;
            float sliderW = dl.w - 130.0f;
            float norm = std::clamp((x - sliderX) / sliderW, 0.0f, 1.0f);
            float rawScale = 0.50f + norm * 1.50f;
            float snappedScale = std::round(rawScale / 0.25f) * 0.25f;
            snappedScale = std::clamp(snappedScale, 0.50f, 2.00f);
            if (std::abs(uiScale_ - snappedScale) > 0.001f) {
                setUiScale(snappedScale);
            }
        } else if (dragMode_ == DragMode::ProjectHubCrtSlider && activeCrtSliderIndex_ >= 0) {
            DialogLayout dl = computeDialogLayout(540.0f, 620.0f);
            const float drawerX = dl.x + 16.0f;
            const float drawerW = dl.w - 32.0f;
            const float sTrackX = drawerX + 12.0f;
            const float sTrackW = drawerW - 32.0f;
            float t = std::clamp((x - sTrackX) / sTrackW, 0.0f, 1.0f);
            auto cfg = dawnBridge_.getMaterialConfig();
            switch (activeCrtSliderIndex_) {
                case 0:
                    cfg.scanlineIntensity = t * 1.0f;
                    setStatusMessage("Scanlines: " + std::to_string(static_cast<int>(std::round(cfg.scanlineIntensity * 100.0f))) + "%" + (cfg.scanlineIntensity <= 0.005f ? " (OFF)" : ""));
                    break;
                case 1:
                    cfg.curvature = t * 1.50f;
                    setStatusMessage("CRT Curvature: " + std::to_string(static_cast<int>(std::round(cfg.curvature * 100.0f))) + "%");
                    break;
                case 2:
                    cfg.crtReflectionLevel = t * 1.0f;
                    setStatusMessage("CRT Tube Reflection: " + std::to_string(static_cast<int>(std::round(cfg.crtReflectionLevel * 100.0f))) + "%" + (cfg.crtReflectionLevel <= 0.005f ? " (OFF)" : ""));
                    break;
                case 3:
                    cfg.hsyncDistortion = t * 2.0f;
                    setStatusMessage("H-Sync Distortion: " + std::to_string(static_cast<int>(std::round(cfg.hsyncDistortion * 100.0f))) + "%" + (cfg.hsyncDistortion <= 0.005f ? " (ZERO / STATIC)" : ""));
                    break;
                case 4:
                    cfg.spotlightIntensity = t * 2.0f;
                    setStatusMessage("Spotlight Intensity: " + std::to_string(static_cast<int>(std::round(cfg.spotlightIntensity * 100.0f))) + "%");
                    break;
                case 5:
                    cfg.spotlightSize = 0.5f + t * 2.0f;
                    setStatusMessage("Spotlight Beam Size: " + std::to_string(static_cast<int>(std::round(cfg.spotlightSize * 100.0f))) + "%");
                    break;
                case 6:
                    cfg.vignetteStrength = t * 2.0f;
                    setStatusMessage("Vignette Falloff: " + std::to_string(static_cast<int>(std::round(cfg.vignetteStrength * 100.0f))) + "%");
                    break;
                case 7:
                    cfg.reflectionOpacity = t * 1.0f;
                    setStatusMessage("Frame Reflection: " + std::to_string(static_cast<int>(std::round(cfg.reflectionOpacity * 100.0f))) + "%");
                    break;
                case 8: {
                    cfg.panelSoftness = t * 1.50f;
                    char buf[32];
                    snprintf(buf, sizeof(buf), "Panel Softness: %.2f px", cfg.panelSoftness);
                    setStatusMessage(buf);
                    break;
                }
                case 9:
                    cfg.panelSaturation = t * 1.0f;
                    setStatusMessage("Panel Saturation: " + std::to_string(static_cast<int>(std::round(cfg.panelSaturation * 100.0f))) + "%");
                    break;
                case 10: {
                    cfg.panelBlackLift = t * 0.08f;
                    char buf[32];
                    snprintf(buf, sizeof(buf), "Panel Black Floor Lift: +%.3f", cfg.panelBlackLift);
                    setStatusMessage(buf);
                    break;
                }
                default: break;
            }
            dawnBridge_.setMaterialConfig(cfg);
            saveSettings();
        }
        mouseX_ = x;
        mouseY_ = y;
        return;
    }

    if (fullscreenDeviceModal_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        mouseX_ = x;
        mouseY_ = y;
        fullscreenDeviceModal_.handlePointer(pev);
        markNeedsRedraw();
        return;
    }

    if (valueEditDialog_.isOpen()) {
        if (modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Move;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            pev.dx = x - mouseX_;
            pev.dy = y - mouseY_;
            if (modularArrangerView_->getIconSearchDialog().handlePointer(pev)) {
                mouseX_ = x;
                mouseY_ = y;
                return;
            }
        }

        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (valueEditDialog_.handlePointer(pev)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }

    if (themeBrowserDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        mouseX_ = x;
        mouseY_ = y;
        themeBrowserDialog_.handlePointer(pev);
        markNeedsRedraw();
        return;
    }

    // Intercept if Plugin / Preset Search Modal Dialog is open
    PluginSearchDialog* activePluginDialog = getActivePluginSearchDialog();
    if (activePluginDialog && activePluginDialog->isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (activePluginDialog->handlePointer(pev)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
        mouseX_ = x;
        mouseY_ = y;
        return; // Absorb events behind modal
    }

    if (commandPaletteDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (commandPaletteDialog_.handlePointer(pev)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }

    if (audioToMidiDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (audioToMidiDialog_.handlePointer(pev)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }

    if (modularArrangerView_ && modularArrangerView_->getCircleOfFifthsDialog().isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        modularArrangerView_->getCircleOfFifthsDialog().handlePointer(pev);
        mouseX_ = x;
        mouseY_ = y;
        return;
    }

    if (isAnyModalActive()) {
        mouseX_ = x;
        mouseY_ = y;
        return;
    }

    if (browserOpen_ && projectBrowserDrawerWidget_) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        bool handled = projectBrowserDrawerWidget_->handlePointer(pev);
        if (handled || projectBrowserDrawerWidget_->getDrawerBounds().contains(x, y)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }

    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (modularArrangerView_->handlePointer(pev, ctx)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }
    if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (modularEditView_->handlePointer(pev, ctx)) {
            if (dragMode_ == DragMode::PianoRollMiddlePan) {
                float dx = x - panStartMouseX_;
                float dy = y - panStartMouseY_;
                setPianoRollScrollX(panStartScrollX_ - dx);
                setPianoRollScrollY(panStartScrollY_ - dy);
            }
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }
    if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (modularTrackInspectorView_->handlePointer(pev, ctx)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }
    if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        modularMixerView_->handlePointer(pev, ctx);
        mixerPropertiesExpanded_ = modularMixerView_->isPropertiesExpanded();
        mixerPropertiesWidth_ = modularMixerView_->getPropertiesWidth();
    }
    if ((activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) && modularDesignView_ && dragMode_ == DragMode::None) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (modularDesignView_->handlePointer(pev, ctx)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }

    float mouseDx = x - mouseX_;
    float mouseDy = y - mouseY_;
    mouseX_ = x;
    mouseY_ = y;

    if (activeDragHandler_ && activeDragHandler_->isDragging()) {
        PointerEvent pev{};
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.dx = mouseDx;
        pev.dy = mouseDy;
        pev.mods.shift = isShiftPressed();
        pev.mods.ctrl = isCtrlPressed();
        pev.mods.alt = isAltPressed();
        activeDragHandler_->onPointerMove(pev);
        return;
    }

    if (dragMode_ == DragMode::PianoRollMiddlePan) {
        float dx = x - panStartMouseX_;
        float dy = y - panStartMouseY_;
        setPianoRollScrollX(panStartScrollX_ - dx);
        setPianoRollScrollY(panStartScrollY_ - dy);
        return;
    } else if (dragMode_ == DragMode::PianoRollScrollbarV) {
        float dy = y - panStartMouseY_;
        const float sbH = 550.0f;
        float totalH = pianoRollKeyboard_.getTotalVerticalContentHeight();
        float thumbH = std::clamp((sbH / std::max(totalH, sbH)) * sbH, 24.0f, sbH);
        float trackH = std::max(1.0f, sbH - thumbH);
        float maxScrollY = std::max(0.0f, totalH - sbH);
        setPianoRollScrollY(panStartScrollY_ + (dy / trackH) * maxScrollY);
        return;
    } else if (dragMode_ == DragMode::PianoRollScrollbarH) {
        float dx = x - panStartMouseX_;
        const float gridX = 110.0f;
        const bool showSidebar = engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() &&
                                 engine_->getSequencer().getTrack(selectedTrackIndex_) &&
                                 engine_->getSequencer().getTrack(selectedTrackIndex_)->hasSelectedNotes();
        const float scrollbarW = 12.0f;
        const float gridW = static_cast<float>(width_) - gridX - (showSidebar ? 280.0f : 20.0f) - scrollbarW;
        const uint32_t totalSteps = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() && engine_->getSequencer().getTrack(selectedTrackIndex_))
                                        ? std::max(64u, engine_->getSequencer().getTrack(selectedTrackIndex_)->getNumSteps())
                                        : 64u;
        float totalW = static_cast<float>(totalSteps) * pianoRollStepW_;
        float thumbW = std::clamp((gridW / std::max(totalW, gridW)) * gridW, 24.0f, gridW);
        float trackW = std::max(1.0f, gridW - thumbW);
        float maxScrollX = std::max(0.0f, totalW - gridW);
        setPianoRollScrollX(panStartScrollX_ + (dx / trackW) * maxScrollX);
        return;
    } else if (dragMode_ == DragMode::VirtualKeyboardGlissando) {
        if (virtualKeyboardDrawerOpen_) {
            auto dHit = hitTestVirtualKeyboardDrawer(x, y);
            if (dHit.hit && dHit.isKey && dHit.pitch != virtualKeyboardActivePitch_) {
                if (engine_) {
                    if (virtualKeyboardActivePitch_ != -1) {
                        engine_->postNoteOff(static_cast<uint8_t>(virtualKeyboardActivePitch_), static_cast<int>(selectedTrackIndex_));
                    }
                    if (previewingPitch_ != -1 && previewingPitch_ != virtualKeyboardActivePitch_) {
                        engine_->postNoteOff(static_cast<uint8_t>(previewingPitch_), static_cast<int>(selectedTrackIndex_));
                    }
                    engine_->postNoteOn(static_cast<uint8_t>(dHit.pitch), dHit.velocity, false, false, static_cast<int>(selectedTrackIndex_));
                }
                virtualKeyboardActivePitch_ = dHit.pitch;
                previewingPitch_ = dHit.pitch;
                previewingVelocity_ = dHit.velocity;
                return;
            }
        }
        if (activeView_ == WorkspaceView::Edit && editSubView_ == EditSubView::PianoRoll) {
            auto kHit = hitTestPianoKey(x, y);
            if (kHit.hit && kHit.pitch != previewingPitch_) {
                if (engine_) {
                    if (previewingPitch_ != -1) {
                        engine_->postNoteOff(static_cast<uint8_t>(previewingPitch_), static_cast<int>(selectedTrackIndex_));
                    }
                    if (virtualKeyboardActivePitch_ != -1 && virtualKeyboardActivePitch_ != previewingPitch_) {
                        engine_->postNoteOff(static_cast<uint8_t>(virtualKeyboardActivePitch_), static_cast<int>(selectedTrackIndex_));
                    }
                    engine_->postNoteOn(static_cast<uint8_t>(kHit.pitch), kHit.velocity, false, false, static_cast<int>(selectedTrackIndex_));
                }
                previewingPitch_ = kHit.pitch;
                virtualKeyboardActivePitch_ = kHit.pitch;
                previewingVelocity_ = kHit.velocity;
                return;
            }
        }
        return;
    } else if (dragMode_ == DragMode::PianoRollMarquee) {
        marqueeCurX_ = x;
        marqueeCurY_ = y;
        float dist = std::hypot(marqueeCurX_ - marqueeStartX_, marqueeCurY_ - marqueeStartY_);
        if (dist > 4.0f) {
            isMarqueeSelecting_ = true;
            updatePianoRollMarqueeSelection(marqueeStartX_, marqueeStartY_, x, y, isShiftPressed());
        }
    } else if (dragMode_ == DragMode::PianoRollNoteMove && engine_) {
        auto* activeTrk = (selectedTrackIndex_ < engine_->getSequencer().getNumTracks())
                              ? engine_->getSequencer().getTrack(selectedTrackIndex_)
                              : nullptr;
        if (activeTrk) {
            const int minPitch = pianoRollKeyboard_.getMinPitch();
            const int maxPitch = pianoRollKeyboard_.getMaxPitch();
            const float rowH = pianoRollRowH_;
            int deltaPitches = -static_cast<int>((y - dragStartY_) / rowH);

            if (deltaPitches != 0) {
                bool canMove = true;
                for (const auto& kv : batchMoveStartPitches_) {
                    int newP = static_cast<int>(kv.second) + deltaPitches;
                    if (newP < minPitch || newP > maxPitch) {
                        canMove = false;
                        break;
                    }
                }
                if (canMove) {
                    for (const auto& kv : batchMoveStartPitches_) {
                        if (kv.first < activeTrk->getNumSteps()) {
                            activeTrk->getStep(kv.first).note = static_cast<uint8_t>(kv.second + deltaPitches);
                        }
                    }
                    if (activeMoveStep_ >= 0 && static_cast<uint32_t>(activeMoveStep_) < activeTrk->getNumSteps()) {
                        previewingPitch_ = activeTrk->getStep(activeMoveStep_).note;
                    }
                }
            }
        }
    } else if (dragMode_ == DragMode::PianoRollNoteResize && engine_) {
        auto* activeTrk = (selectedTrackIndex_ < engine_->getSequencer().getNumTracks())
                              ? engine_->getSequencer().getTrack(selectedTrackIndex_)
                              : nullptr;
        if (activeTrk) {
            const float stepW = pianoRollStepW_;
            float deltaDur = (x - dragStartX_) / stepW;

            for (const auto& kv : batchResizeStartDurations_) {
                if (kv.first < activeTrk->getNumSteps()) {
                    activeTrk->getStep(kv.first).gateLength = std::clamp(kv.second + deltaDur, 0.10f, 64.0f);
                }
            }
        }
    } else if (dragMode_ == DragMode::HardwareKnob) {
        const float deltaY = dragStartY_ - y;
        constexpr float sensitivity = 1.0f / 160.0f;
        float newVal = std::clamp(dragStartHardwareVal_ + deltaY * sensitivity, 0.0f, 1.0f);
        dispatchHardwareParam(activeHardwareParam_, newVal);
    } else if (dragMode_ == DragMode::Knob && engine_) {
        const float deltaY = dragStartY_ - y; // Dragging up increases parameter
        constexpr float sensitivity = 1.0f / 160.0f; // 160px for full 0..1 range
        float newVal = std::clamp(dragStartValue_ + deltaY * sensitivity, 0.0f, 1.0f);
        setKnobValue(activeKnobNode_, activeKnobIndex_, newVal);
        dispatchKnobParameter(activeKnobNode_, activeKnobIndex_, newVal);
    } else if (dragMode_ == DragMode::BpmScrubber && engine_) {
        const float deltaY = dragStartY_ - y;
        float newBpm = std::clamp(dragStartValGeneric_ + deltaY * 0.5f, 40.0f, 300.0f);
        engine_->getSequencer().setBpm(newBpm);
    } else if (dragMode_ == DragMode::SwingScrubber && engine_) {
        const float deltaY = dragStartY_ - y;
        float newSwing = std::clamp(dragStartValGeneric_ + deltaY * 0.005f, 0.0f, 0.75f);
        engine_->getSequencer().setSwing(newSwing);
    } else if (dragMode_ == DragMode::MasterFader) {
        const float deltaY = dragStartY_ - y;
        float newVol = std::clamp(dragStartValGeneric_ + deltaY * (1.5f / 280.0f), 0.0f, 1.5f);
        setMasterVolume(newVol);
    } else if (dragMode_ == DragMode::MasterPan) {
        const float deltaY = dragStartY_ - y;
        float newPan = std::clamp(dragStartValGeneric_ + deltaY * (2.0f / 100.0f), -1.0f, 1.0f);
        setMasterPan(newPan);
    } else if (dragMode_ == DragMode::MixerFader) {
        if (activeMixerChannel_ < mixerStrips_.size()) {
            const float deltaY = dragStartY_ - y;
            float newVol = std::clamp(dragStartValGeneric_ + deltaY * (1.5f / 280.0f), 0.0f, 1.5f);
            mixerStrips_[activeMixerChannel_].volume = newVol;
            if (activeMixerChannel_ < arrangerTracks_.size()) {
                arrangerTracks_[activeMixerChannel_].volume = newVol;
            }
            if (engine_) {
                if (activeMixerChannel_ < 4) {
                    engine_->setTrackVolume(activeMixerChannel_, newVol);
                } else {
                    setMasterVolume(newVol);
                }
            }
        }
    } else if (dragMode_ == DragMode::MixerPan) {
        if (activeMixerChannel_ < mixerStrips_.size()) {
            const float deltaY = dragStartY_ - y;
            float newPan = std::clamp(dragStartValGeneric_ + deltaY * (2.0f / 100.0f), -1.0f, 1.0f);
            mixerStrips_[activeMixerChannel_].pan = newPan;
            if (activeMixerChannel_ < arrangerTracks_.size()) {
                arrangerTracks_[activeMixerChannel_].pan = newPan;
            }
            if (engine_) {
                if (activeMixerChannel_ < 4) {
                    engine_->setTrackPan(activeMixerChannel_, newPan);
                } else {
                    setMasterPan(newPan);
                }
            }
        }
    } else if (dragMode_ == DragMode::ArrangerRulerScrub && engine_) {
        const float trackHeaderW = 210.0f;
        const float barW = 60.0f;
        if (x >= trackHeaderW) {
            float barProgress = (x - trackHeaderW) / barW;
            uint32_t targetStep = static_cast<uint32_t>(std::clamp(barProgress * 16.0f, 0.0f, 18.0f * 16.0f));
            engine_->getSequencer().getTransport().setPosition(targetStep);
        }
    } else if (dragMode_ == DragMode::ArrangerTrackVolume) {
        float normVal = std::clamp((x - 38.0f) / 114.0f, 0.0f, 1.0f);
        float newVol = normVal * 1.5f;
        if (activeArrangerTrack_ < mixerStrips_.size()) {
            mixerStrips_[activeArrangerTrack_].volume = newVol;
        }
        if (activeArrangerTrack_ < arrangerTracks_.size()) {
            arrangerTracks_[activeArrangerTrack_].volume = newVol;
        }
        if (engine_) {
            engine_->setTrackVolume(activeArrangerTrack_, newVol);
        }
    } else if (dragMode_ == DragMode::ArrangerTrackPan) {
        float delta = (x - dragStartX_) * 0.015f - (y - dragStartY_) * 0.015f;
        float newPan = std::clamp(dragStartPan_ + delta, -1.0f, 1.0f);
        if (activeArrangerTrack_ < arrangerTracks_.size()) {
            arrangerTracks_[activeArrangerTrack_].pan = newPan;
        }
        if (activeArrangerTrack_ < mixerStrips_.size()) {
            mixerStrips_[activeArrangerTrack_].pan = newPan;
        }
        if (engine_) {
            engine_->setTrackPan(activeArrangerTrack_, newPan);
        }
    } else if (dragMode_ == DragMode::ArrangerOverviewScroll && engine_) {
        const float trackHeaderW = 210.0f;
        const float miniTrackW = static_cast<float>(width_) - trackHeaderW - 20.0f;
        if (miniTrackW > 0.0f) {
            float norm = std::clamp((x - trackHeaderW) / miniTrackW, 0.0f, 1.0f);
            uint32_t targetStep = static_cast<uint32_t>(norm * 18.0f * 16.0f);
            engine_->getSequencer().getTransport().setPosition(targetStep);
        }
    } else if (dragMode_ == DragMode::ArrangerPropertiesResize) {
        if (!arrangerPropertiesExpanded_) {
            if (x - dragStartX_ < -4.0f) {
                arrangerPropertiesExpanded_ = true;
                dragStartX_ = x;
                dragStartValGeneric_ = arrangerPropertiesWidth_;
            }
        } else {
            float newWidth = dragStartValGeneric_ + (dragStartX_ - x);
            if (newWidth < kArrangerPropertiesMinW - 25.0f) {
                arrangerPropertiesExpanded_ = false;
                arrangerPropertiesWidth_ = 300.0f;
                dragMode_ = DragMode::None;
            } else {
                arrangerPropertiesWidth_ = std::clamp(newWidth, kArrangerPropertiesMinW, kArrangerPropertiesMaxW);
            }
        }
    } else if (dragMode_ == DragMode::MixerPropertiesResize) {
        if (!mixerPropertiesExpanded_) {
            if (x - dragStartX_ < -4.0f) {
                mixerPropertiesExpanded_ = true;
                dragStartX_ = x;
                dragStartValGeneric_ = mixerPropertiesWidth_;
            }
        } else {
            float newWidth = dragStartValGeneric_ + (dragStartX_ - x);
            if (newWidth < kMixerPropertiesMinW - 25.0f) {
                mixerPropertiesExpanded_ = false;
                mixerPropertiesWidth_ = 360.0f;
                dragMode_ = DragMode::None;
            } else {
                mixerPropertiesWidth_ = std::clamp(newWidth, kMixerPropertiesMinW, kMixerPropertiesMaxW);
            }
        }
    } else if (dragMode_ == DragMode::TrackInspectorVolume) {
        const float inspX = 40.0f;
        float norm = std::clamp((x - (inspX + 80.0f)) / 220.0f, 0.0f, 1.0f);
        float newVol = norm * 1.5f;
        uint32_t tIdx = (selectedTrackIndex_ < arrangerTracks_.size()) ? selectedTrackIndex_ : 0;
        arrangerTracks_[tIdx].volume = newVol;
        if (tIdx < mixerStrips_.size()) mixerStrips_[tIdx].volume = newVol;
        if (engine_) {
            engine_->setTrackVolume(tIdx, newVol);
        }
    } else if (dragMode_ == DragMode::TrackInspectorPan) {
        const float inspX = 40.0f;
        float norm = std::clamp((x - (inspX + 420.0f)) / 180.0f, 0.0f, 1.0f);
        float newPan = norm * 2.0f - 1.0f;
        uint32_t tIdx = (selectedTrackIndex_ < arrangerTracks_.size()) ? selectedTrackIndex_ : 0;
        arrangerTracks_[tIdx].pan = newPan;
        if (tIdx < mixerStrips_.size()) mixerStrips_[tIdx].pan = newPan;
        if (engine_) {
            engine_->setTrackPan(tIdx, newPan);
        }
    } else if (dragMode_ == DragMode::TrackInspectorFxKnob) {
        const float deltaY = dragStartY_ - y;
        constexpr float sensitivity = 1.0f / 140.0f;
        float newVal = std::clamp(dragStartTrackInspectorVal_ + deltaY * sensitivity, 0.0f, 1.0f);
        uint32_t tIdx = (selectedTrackIndex_ < arrangerTracks_.size()) ? selectedTrackIndex_ : 0;
        auto& trk = arrangerTracks_[tIdx];
        if (activeTrackInspectorFxParam_ == "delayTime") trk.audioFx.delayTime = newVal;
        else if (activeTrackInspectorFxParam_ == "delayFeedback") trk.audioFx.delayFeedback = newVal;
        else if (activeTrackInspectorFxParam_ == "delayMix") trk.audioFx.delayMix = newVal;
        else if (activeTrackInspectorFxParam_ == "chorusRate") trk.audioFx.chorusRate = newVal;
        else if (activeTrackInspectorFxParam_ == "chorusDepth") trk.audioFx.chorusDepth = newVal;
        else if (activeTrackInspectorFxParam_ == "chorusMix") trk.audioFx.chorusMix = newVal;
        else if (activeTrackInspectorFxParam_ == "eqLow") trk.audioFx.eqLow = newVal;
        else if (activeTrackInspectorFxParam_ == "eqMid") trk.audioFx.eqMid = newVal;
        else if (activeTrackInspectorFxParam_ == "eqHigh") trk.audioFx.eqHigh = newVal;
        else if (activeTrackInspectorFxParam_ == "compThreshold") trk.audioFx.compThreshold = newVal;
        else if (activeTrackInspectorFxParam_ == "compRatio") trk.audioFx.compRatio = newVal;
        else if (activeTrackInspectorFxParam_ == "compGain") trk.audioFx.compGain = newVal;
        else if (activeTrackInspectorFxParam_ == "convolverPreset") trk.audioFx.convolverPreset = static_cast<int>(std::round(newVal * 12.0f));
        else if (activeTrackInspectorFxParam_ == "convolverMix") trk.audioFx.convolverMix = newVal;
        else if (activeTrackInspectorFxParam_ == "arpPattern") trk.midiFx.arpPattern = static_cast<int>(std::round(newVal * 4.0f));
        else if (activeTrackInspectorFxParam_ == "arpRate") trk.midiFx.arpRate = newVal;
        else if (activeTrackInspectorFxParam_ == "arpOctaves") trk.midiFx.arpOctaves = static_cast<int>(std::round(newVal));
        else if (activeTrackInspectorFxParam_ == "arpGate") trk.midiFx.arpGate = newVal;
        else if (activeTrackInspectorFxParam_ == "arpSwing") trk.midiFx.arpSwing = newVal;
        else if (activeTrackInspectorFxParam_ == "scaleMinor") trk.midiFx.scaleMinor = (newVal >= 0.5f);
        else if (activeTrackInspectorFxParam_ == "humanizeTiming") trk.midiFx.humanizeTiming = newVal;
        else if (activeTrackInspectorFxParam_ == "humanizeVelocity") trk.midiFx.humanizeVelocity = newVal;
    } else if (dragMode_ == DragMode::TrackInspectorScrollbar) {
        constexpr float maxScroll = 450.0f;
        const float deltaY = y - dragStartY_;
        const float scrollRange = static_cast<float>(height_) - 120.0f;
        if (scrollRange > 0.0f) {
            float scrollDelta = (deltaY / scrollRange) * (maxScroll + scrollRange);
            trackInspectorScrollY_ = std::clamp(dragStartScrollY_ + scrollDelta, 0.0f, maxScroll);
        }
    } else if (dragMode_ == DragMode::ProjectHubScroll) {
        if (projectHubScrollArea_.canScroll()) {
            const float deltaY = y - dragStartY_;
            if (projectHubDraggingThumb_) {
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Move;
                pev.x = x;
                pev.y = y;
                projectHubScrollArea_.handlePointer(pev);
                projectHubScrollY_ = projectHubScrollArea_.getScrollY();
            } else {
                projectHubScrollArea_.setScrollY(dragStartScrollY_ - deltaY);
                projectHubScrollY_ = projectHubScrollArea_.getScrollY();
            }
        }

    } else if (dragMode_ == DragMode::UiScaleSlider) {
        DialogFrameConfig cfg;
        cfg.width = 540.0f;
        cfg.height = 580.0f;
        float hubX = (static_cast<float>(width_) - cfg.width) * 0.5f;
        float sliderX = hubX + 24.0f;
        float sliderW = cfg.width - 130.0f;
        float norm = std::clamp((x - sliderX) / sliderW, 0.0f, 1.0f);
        float rawScale = 0.50f + norm * 1.50f;
        float snappedScale = std::round(rawScale / 0.25f) * 0.25f;
        snappedScale = std::clamp(snappedScale, 0.50f, 2.00f);
        if (std::abs(uiScale_ - snappedScale) > 0.001f) {
            setUiScale(snappedScale);
        }
    }
}

void GuiWindow::onMouseDown(int button, float x, float y) {
    mouseX_ = x;
    mouseY_ = y;

    ColorPickerDialog* activeColorPicker = getActiveColorPickerDialog();
    if (activeColorPicker && activeColorPicker->isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        activeColorPicker->handlePointer(pev);
        return;
    }

    // 0. Intercept if Command Palette or Value Edit Modal Dialog is open
    if (commandPaletteDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (commandPaletteDialog_.handlePointer(pev)) return;
    }

    if (valueEditDialog_.isOpen()) {
        if (modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularArrangerView_->getIconSearchDialog().handlePointer(pev)) return;
        }

        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (valueEditDialog_.handlePointer(pev)) return;
    }

    if (audioToMidiDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (audioToMidiDialog_.handlePointer(pev)) return;
    }

    // Intercept if Dedicated Full-Display Device GUI is open
    if (fullscreenDeviceModal_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        fullscreenDeviceModal_.handlePointer(pev);
        markNeedsRedraw();
        return;
    }

    if (themeBrowserDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        themeBrowserDialog_.handlePointer(pev);
        markNeedsRedraw();
        return;
    }

    // Modal Preset / Plugin Search Dialog intercepts clicks
    PluginSearchDialog* activePluginDialog = getActivePluginSearchDialog();
    if (activePluginDialog && activePluginDialog->isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        activePluginDialog->handlePointer(pev);
        return; // Absorb events behind modal
    }

    if (modularArrangerView_ && modularArrangerView_->getCircleOfFifthsDialog().isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularArrangerView_->getCircleOfFifthsDialog().handlePointer(pev);
        return;
    }

    // Check Project Hub & Settings Modal Hit (if open)
    if (projectHubOpen_) {
        if (button == 0) { // Left click
            auto hubHit = hitTestProjectHub(x, y);
            if (hubHit.hit) {
                switch (hubHit.action) {
                    case ProjectHubAction::Close:
                        projectHubOpen_ = false;
                        isEditingTitle_ = false;
                        isEditingAuthor_ = false;
                        break;
                    case ProjectHubAction::SectionHeader:
                        projectHubSection_ = (projectHubSection_ == hubHit.sectionIndex) ? -1 : hubHit.sectionIndex;
                        projectHubScrollY_ = 0.0f;
                        projectHubScrollArea_.setScrollY(0.0f);
                        isEditingTitle_ = false;
                        isEditingAuthor_ = false;
                        break;
                    case ProjectHubAction::TitleClick:
                        isEditingTitle_ = true;
                        isEditingAuthor_ = false;
                        break;
                    case ProjectHubAction::AuthorClick:
                        isEditingAuthor_ = true;
                        isEditingTitle_ = false;
                        break;
                    case ProjectHubAction::SaveProject:
                        saveProjectToFile(projectFilePath_);
                        break;
                    case ProjectHubAction::SaveAsProject:
                        saveProjectAs();
                        break;
                    case ProjectHubAction::LoadProject:
                        loadProjectPrompt();
                        break;
                    case ProjectHubAction::NewProject:
                        resetToDefaultProject();
                        break;
                    case ProjectHubAction::BounceWav:
                        bounceMasterToWav(projectName_ + ".wav");
                        break;
                    case ProjectHubAction::OpenScriptView:
                        setActiveView(WorkspaceView::Edit);
                        setEditSubView(EditSubView::Script);
                        projectHubOpen_ = false;
                        break;
                    case ProjectHubAction::ToggleRestoreSession:
                        autoRestoreSession_ = !autoRestoreSession_; saveSettings();
                        break;
                    case ProjectHubAction::ToggleAutosave:
                        autoSaveEnabled_ = !autoSaveEnabled_; saveSettings();
                        break;
                    case ProjectHubAction::ResetCleanSlate:
                        resetToDefaultProject();
                        break;
                    case ProjectHubAction::RevertSettingsDefault:
                        revertSettingsToDefault();
                        break;
                    case ProjectHubAction::OpenSettingsFile:
                        openLocalSettingsFile();
                        break;
                    case ProjectHubAction::SetUiScale:
                        setUiScale(hubHit.scaleValue); saveSettings();
                        dragMode_ = DragMode::UiScaleSlider;
                        break;
                    case ProjectHubAction::SelectTheme:
                        setActiveThemePreset(hubHit.themeIndex); saveSettings();
                        setStatusMessage("Theme: " + getTheme().name);
                        break;
                    case ProjectHubAction::SelectThemeBank:
                        setThemeBank(hubHit.themeBank); saveSettings();
                        break;
                    case ProjectHubAction::OpenThemeBrowser:
                        openThemeBrowserDialog();
                        break;
                    case ProjectHubAction::ToggleCrtShader:
                        crtShaderEnabled_ = !crtShaderEnabled_; saveSettings();
                        break;

                    case ProjectHubAction::ToggleAnimations:
                        guiAnimationsEnabled_ = !guiAnimationsEnabled_; saveSettings();
                        break;
                    case ProjectHubAction::ToggleMinimalGui:
                        toggleMinimalGui();
                        break;
                    case ProjectHubAction::SetAntiAliasing:
                        setAntiAliasingMode(hubHit.aaMode); saveSettings();
                        break;
                    case ProjectHubAction::ToggleHiDpi:
                        toggleHiDpi(); saveSettings();
                        break;

                    case ProjectHubAction::ToggleIncludeBinaryClapPath: {
                        bool cur = audio::ClapPluginLoader::instance().isIncludeBinaryPathEnabled();
                        audio::ClapPluginLoader::instance().setIncludeBinaryPath(!cur);
                        setStatusMessage(std::string("Portable CLAP Binary Path: ") + (!cur ? "ENABLED (Reaper Mode)" : "DISABLED (System Paths Only)"));
                        break;
                    }
                    case ProjectHubAction::RescanClapPlugins: {
                        if (!audio::ClapPluginLoader::instance().isScanning()) {
                            setStatusMessage("Scanning CLAP plugin directories in background...");
                            audio::ClapPluginLoader::instance().scanDirectoriesAsync({}, [this](const auto& res) {
                                setStatusMessage("CLAP Scan Complete: " + std::to_string(res.size()) + " plugins discovered.");
                            });
                        }
                        break;
                    }
                    case ProjectHubAction::CrtPresetStudioRef: {
                        auto cfg = dawnBridge_.getMaterialConfig();
                        cfg.scanlineIntensity = 0.38f;
                        cfg.curvature = 0.85f;
                        cfg.crtReflectionLevel = 1.0f;
                        cfg.hsyncDistortion = 1.0f;
                        cfg.spotlightIntensity = 1.0f;
                        cfg.spotlightSize = 1.0f;
                        cfg.vignetteStrength = 1.0f;
                        cfg.reflectionOpacity = 0.45f;
                        cfg.panelSoftness = 0.50f;
                        cfg.panelSaturation = 0.70f;
                        cfg.panelBlackLift = 0.025f;
                        crtShaderEnabled_ = true;
                        dawnBridge_.setMaterialConfig(cfg);
                        dawnBridge_.setCrtShaderEnabled(true);
                        setStatusMessage("CRT Preset: STUDIO REF (Calibrated Hardware)");
                        break;
                    }
                    case ProjectHubAction::CrtPresetMaxClarity: {
                        auto cfg = dawnBridge_.getMaterialConfig();
                        cfg.scanlineIntensity = 0.0f;
                        cfg.curvature = 0.0f;
                        cfg.crtReflectionLevel = 0.0f;
                        cfg.hsyncDistortion = 0.0f;
                        cfg.spotlightIntensity = 0.0f;
                        cfg.spotlightSize = 1.0f;
                        cfg.vignetteStrength = 0.0f;
                        cfg.reflectionOpacity = 0.0f;
                        cfg.panelSoftness = 0.0f;
                        cfg.panelSaturation = 1.0f;
                        cfg.panelBlackLift = 0.0f;
                        dawnBridge_.setMaterialConfig(cfg);
                        setStatusMessage("CRT Preset: MAX CLARITY (Flat Screen, 0 Scanlines, 0 Glare)");
                        break;
                    }
                    case ProjectHubAction::CrtPresetWarmVintage: {
                        auto cfg = dawnBridge_.getMaterialConfig();
                        cfg.scanlineIntensity = 0.55f;
                        cfg.curvature = 1.15f;
                        cfg.crtReflectionLevel = 1.25f;
                        cfg.hsyncDistortion = 1.35f;
                        cfg.spotlightIntensity = 1.25f;
                        cfg.spotlightSize = 1.20f;
                        cfg.vignetteStrength = 1.35f;
                        cfg.reflectionOpacity = 0.65f;
                        cfg.panelSoftness = 1.0f;
                        cfg.panelSaturation = 0.65f;
                        cfg.panelBlackLift = 0.035f;
                        crtShaderEnabled_ = true;
                        dawnBridge_.setMaterialConfig(cfg);
                        dawnBridge_.setCrtShaderEnabled(true);
                        setStatusMessage("CRT Preset: WARM VINTAGE (Deep Bulb & Scanlines)");
                        break;
                    }
                    case ProjectHubAction::CrtPresetReset: {
                        CrtMaterialConfig cfg{};
                        dawnBridge_.setMaterialConfig(cfg);
                        setStatusMessage("CRT Preset: RESET (Factory Defaults)");
                        break;
                    }
                    case ProjectHubAction::CrtSlider: {
                        DialogLayout dl = computeDialogLayout(540.0f, 620.0f);
                        const float drawerX = dl.x + 16.0f;
                        const float drawerW = dl.w - 32.0f;
                        const float sTrackX = drawerX + 12.0f;
                        const float sTrackW = drawerW - 32.0f;
                        float t = std::clamp((x - sTrackX) / sTrackW, 0.0f, 1.0f);
                        auto cfg = dawnBridge_.getMaterialConfig();
                        switch (hubHit.crtSliderIndex) {
                            case 0:
                                cfg.scanlineIntensity = t * 1.0f;
                                setStatusMessage("Scanlines: " + std::to_string(static_cast<int>(std::round(cfg.scanlineIntensity * 100.0f))) + "%" + (cfg.scanlineIntensity <= 0.005f ? " (OFF)" : ""));
                                break;
                            case 1:
                                cfg.curvature = t * 1.50f;
                                setStatusMessage("CRT Curvature: " + std::to_string(static_cast<int>(std::round(cfg.curvature * 100.0f))) + "%");
                                break;
                            case 2:
                                cfg.crtReflectionLevel = t * 1.0f;
                                setStatusMessage("CRT Tube Reflection: " + std::to_string(static_cast<int>(std::round(cfg.crtReflectionLevel * 100.0f))) + "%" + (cfg.crtReflectionLevel <= 0.005f ? " (OFF)" : ""));
                                break;
                            case 3:
                                cfg.hsyncDistortion = t * 2.0f;
                                setStatusMessage("H-Sync Distortion: " + std::to_string(static_cast<int>(std::round(cfg.hsyncDistortion * 100.0f))) + "%" + (cfg.hsyncDistortion <= 0.005f ? " (ZERO / STATIC)" : ""));
                                break;
                            case 4:
                                cfg.spotlightIntensity = t * 2.0f;
                                setStatusMessage("Spotlight Intensity: " + std::to_string(static_cast<int>(std::round(cfg.spotlightIntensity * 100.0f))) + "%");
                                break;
                            case 5:
                                cfg.spotlightSize = 0.5f + t * 2.0f;
                                setStatusMessage("Spotlight Beam Size: " + std::to_string(static_cast<int>(std::round(cfg.spotlightSize * 100.0f))) + "%");
                                break;
                            case 6:
                                cfg.vignetteStrength = t * 2.0f;
                                setStatusMessage("Vignette Falloff: " + std::to_string(static_cast<int>(std::round(cfg.vignetteStrength * 100.0f))) + "%");
                                break;
                            case 7:
                                cfg.reflectionOpacity = t * 1.0f;
                                setStatusMessage("Frame Reflection: " + std::to_string(static_cast<int>(std::round(cfg.reflectionOpacity * 100.0f))) + "%");
                                break;
                            case 8: {
                                cfg.panelSoftness = t * 1.50f;
                                char buf[32];
                                snprintf(buf, sizeof(buf), "Panel Softness: %.2f px", cfg.panelSoftness);
                                setStatusMessage(buf);
                                break;
                            }
                            case 9:
                                cfg.panelSaturation = t * 1.0f;
                                setStatusMessage("Panel Saturation: " + std::to_string(static_cast<int>(std::round(cfg.panelSaturation * 100.0f))) + "%");
                                break;
                            case 10: {
                                cfg.panelBlackLift = t * 0.08f;
                                char buf[32];
                                snprintf(buf, sizeof(buf), "Panel Black Floor Lift: +%.3f", cfg.panelBlackLift);
                                setStatusMessage(buf);
                                break;
                            }
                            default: break;
                        }
                        dawnBridge_.setMaterialConfig(cfg);
                        saveSettings();
                        dragMode_ = DragMode::ProjectHubCrtSlider;
                        activeCrtSliderIndex_ = hubHit.crtSliderIndex;
                        break;
                    }
                    case ProjectHubAction::Scrollbar: {
                        PointerEvent pev;
                        pev.type = PointerType::Mouse;
                        pev.action = PointerAction::Down;
                        pev.button = PointerButton::Left;
                        pev.x = x;
                        pev.y = y;
                        if (projectHubScrollArea_.handlePointer(pev)) {
                            projectHubScrollY_ = projectHubScrollArea_.getScrollY();
                        }
                        dragMode_ = DragMode::ProjectHubScroll;
                        dragStartY_ = y;
                        dragStartScrollY_ = projectHubScrollY_;
                        projectHubDraggingThumb_ = true;
                        break;
                    }
                    case ProjectHubAction::ContentDrag:
                        dragMode_ = DragMode::ProjectHubScroll;
                        dragStartY_ = y;
                        dragStartScrollY_ = projectHubScrollY_;
                        projectHubDraggingThumb_ = false;
                        break;
                    default:
                        break;
                }
                return;
            } else {
                // Clicked outside modal -> close modal and absorb click so background is untouched
                projectHubOpen_ = false;
                isEditingTitle_ = false;
                isEditingAuthor_ = false;
                return;
            }
        } else {
            // Right or middle click outside/inside open settings modal -> absorb so background is untouched
            return;
        }
    }

    if (isAnyModalActive()) {
        return;
    }

    if (button == 2) { // Middle click: 2D panning in Arranger or Piano Roll
        if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Middle;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularArrangerView_->handlePointer(pev, ctx)) return;
        } else if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Middle;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularEditView_->handlePointer(pev, ctx)) {
                if (editSubView_ == EditSubView::PianoRoll) {
                    dragMode_ = DragMode::PianoRollMiddlePan;
                    panStartMouseX_ = x;
                    panStartMouseY_ = y;
                    panStartScrollX_ = pianoRollScrollX_;
                    panStartScrollY_ = pianoRollScrollY_;
                }
                return;
            }
        }
        if (activeView_ == WorkspaceView::Edit && editSubView_ == EditSubView::PianoRoll) {
            dragMode_ = DragMode::PianoRollMiddlePan;
            panStartMouseX_ = x;
            panStartMouseY_ = y;
            panStartScrollX_ = pianoRollScrollX_;
            panStartScrollY_ = pianoRollScrollY_;
            return;
        }
    }

    if (button == 0) { // Left click

        // 0. Check Preset Browser Drawer Hit (if open)
        if (browserOpen_) {
            auto bHit = hitTestBrowser(x, y);
            if (bHit.hit) {
                if (bHit.action == BrowserHitAction::Close) {
                    setBrowserOpen(false);
                    return;
                } else if (bHit.action == BrowserHitAction::CategorySelect) {
                    browserCategory_ = bHit.category;
                    return;
                } else if (bHit.action == BrowserHitAction::PresetSelect) {
                    if (bHit.presetIndex < presets_.size()) {
                        selectedBrowserPresetIndex_ = bHit.presetIndex;
                    }
                    return;
                } else if (bHit.action == BrowserHitAction::PresetLoad) {
                    if (bHit.presetIndex < presets_.size()) {
                        selectedBrowserPresetIndex_ = bHit.presetIndex;
                        loadPresetToSelectedTrack(bHit.presetIndex);
                    }
                    return;
                } else if (bHit.action == BrowserHitAction::TabPresets) {
                    browserTab_ = BrowserTab::Presets;
                    return;
                } else if (bHit.action == BrowserHitAction::TabMacros) {
                    browserTab_ = BrowserTab::Macros;
                    return;
                } else if (bHit.action == BrowserHitAction::TabHistory) {
                    browserTab_ = BrowserTab::History;
                    return;
                } else if (bHit.action == BrowserHitAction::MacroRun) {
                    runMacro(bHit.macroIndex);
                    return;
                } else if (bHit.action == BrowserHitAction::SaveProject) {
                    saveProjectToFile(projectFilePath_.empty() ? "project.eats" : projectFilePath_);
                    return;
                } else if (bHit.action == BrowserHitAction::LoadProject) {
                    loadProjectFromFile(projectFilePath_.empty() ? "project.eats" : projectFilePath_);
                    return;
                } else if (bHit.action == BrowserHitAction::BounceMaster) {
                    bounceMasterToWav("bounce.wav");
                    return;
                } else if (bHit.action == BrowserHitAction::HistoryUndo) {
                    undoHistory();
                    return;
                } else if (bHit.action == BrowserHitAction::HistoryRedo) {
                    redoHistory();
                    return;
                } else if (bHit.action == BrowserHitAction::HistoryMilestone) {
                    createHistoryMilestone("Checkpoint " + std::to_string(diffHistory_.getTimelineCount()));
                    return;
                } else if (bHit.action == BrowserHitAction::HistoryClear) {
                    clearHistory();
                    return;
                } else if (bHit.action == BrowserHitAction::HistoryStepSelect) {
                    jumpToHistoryIndex(bHit.historyStepIndex);
                    return;
                }
            }

            if (projectBrowserDrawerWidget_) {
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Down;
                pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
                pev.x = x;
                pev.y = y;
                bool handled = projectBrowserDrawerWidget_->handlePointer(pev);
                if (handled || projectBrowserDrawerWidget_->getDrawerBounds().contains(x, y)) {
                    return;
                }
            }
        }

        // 1. Check Top Transport Header Hit
        auto transportHit = hitTestTransport(x, y);
        if (transportHit.hit) {
            switch (transportHit.action) {
                case TransportAction::ProjectHubToggle:
                    projectHubOpen_ = !projectHubOpen_;
                    if (projectHubOpen_) browserOpen_ = false;
                    break;
                case TransportAction::PlayPause:
                    if (engine_) {
                        if (engine_->getSequencer().isPlaying()) {
                            engine_->stopPlayback();
                        } else {
                            engine_->getSequencer().start();
                        }
                    }
                    break;
                case TransportAction::Stop:
                    if (engine_) {
                        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
                        if ((nowMs - lastStopClickTimeMs_) < 400) {
                            engine_->panic();
                            setStatusMessage("[PANIC] Full Audio Stop: All buffers flushed & voices killed");
                        } else {
                            engine_->stopPlayback();
                        }
                        lastStopClickTimeMs_ = nowMs;
                    }
                    break;
                case TransportAction::Record:
                    break;
                case TransportAction::Bpm:
                    if (engine_) {
                        if (isCtrlPressed()) {
                            engine_->getSequencer().setBpm(120.0f);
                            break;
                        }
                        dragStartY_ = y;
                        dragStartValGeneric_ = static_cast<float>(engine_->getSequencer().getBpm());
                        presenter::ScalarDragConfig cfg{};
                        cfg.dragMode = DragMode::BpmScrubber;
                        cfg.direction = presenter::DragDirection::VerticalUpIncreases;
                        cfg.minValue = 40.0f;
                        cfg.maxValue = 300.0f;
                        cfg.defaultValue = 120.0f;
                        cfg.sensitivity = 0.5f;
                        cfg.relativeMode = true;
                        cfg.captureMouse = true;
                        scalarDragPresenter_.startDrag(x, y, dragStartValGeneric_, cfg,
                            [this](float bpm) {
                                if (engine_) engine_->getSequencer().setBpm(bpm);
                            });
                        activeDragHandler_ = &scalarDragPresenter_;
                        dragMode_ = DragMode::BpmScrubber;
                    }
                    break;
                case TransportAction::Swing:
                    if (engine_) {
                        if (isCtrlPressed()) {
                            engine_->getSequencer().setSwing(0.0f);
                            break;
                        }
                        dragStartY_ = y;
                        dragStartValGeneric_ = static_cast<float>(engine_->getSequencer().getSwing());
                        presenter::ScalarDragConfig cfg{};
                        cfg.dragMode = DragMode::SwingScrubber;
                        cfg.direction = presenter::DragDirection::VerticalUpIncreases;
                        cfg.minValue = 0.0f;
                        cfg.maxValue = 0.75f;
                        cfg.defaultValue = 0.0f;
                        cfg.sensitivity = 0.005f;
                        cfg.relativeMode = true;
                        cfg.captureMouse = true;
                        scalarDragPresenter_.startDrag(x, y, dragStartValGeneric_, cfg,
                            [this](float sw) {
                                if (engine_) engine_->getSequencer().setSwing(sw);
                            });
                        activeDragHandler_ = &scalarDragPresenter_;
                        dragMode_ = DragMode::SwingScrubber;
                    }
                    break;
                case TransportAction::MasterVol: {
                    if (isCtrlPressed()) {
                        setMasterVolume(1.0f);
                        break;
                    }
                    dragStartY_ = y;
                    dragStartValGeneric_ = masterVolume_;
                    presenter::ScalarDragConfig cfg{};
                    cfg.dragMode = DragMode::MasterVolume;
                    cfg.direction = presenter::DragDirection::VerticalUpIncreases;
                    cfg.minValue = 0.0f;
                    cfg.maxValue = 1.5f;
                    cfg.defaultValue = 1.0f;
                    cfg.sensitivity = 1.5f / 280.0f;
                    cfg.relativeMode = true;
                    cfg.captureMouse = true;
                    scalarDragPresenter_.startDrag(x, y, masterVolume_, cfg,
                        [this](float v) { setMasterVolume(v); });
                    activeDragHandler_ = &scalarDragPresenter_;
                    dragMode_ = DragMode::MasterVolume;
                    break;
                }
                case TransportAction::LoopToggle:
                    toggleLoop();
                    break;
                case TransportAction::MetronomeToggle:
                    toggleMetronome();
                    break;
                case TransportAction::BrowserToggle:
                    toggleBrowser();
                    if (browserOpen_) projectHubOpen_ = false;
                    break;
                case TransportAction::FullscreenToggle:
                    toggleFullscreen();
                    break;
                case TransportAction::SearchToggle:
                    toggleCommandPalette();
                    break;
                case TransportAction::LockToggle:
                    projectLocked_ = !projectLocked_;
                    setStatusMessage(projectLocked_ ? "[LOCK] Workspace Edit Protection Engaged" : "[LOCK] Workspace Edit Protection Released");
                    break;
                case TransportAction::SnapToggle:
                    break;
                case TransportAction::ScaleToggle:
                    if (uiScale_ < 0.80f) setUiScale(0.85f);
                    else if (uiScale_ < 0.95f) setUiScale(1.0f);
                    else if (uiScale_ < 1.15f) setUiScale(1.25f);
                    else if (uiScale_ < 1.40f) setUiScale(1.50f);
                    else setUiScale(0.75f);
                    break;
                case TransportAction::TabRack:
                    setActiveView(WorkspaceView::ModularRack);
                    break;
                case TransportAction::TabHardware:
                    syncTrackToPreset(selectedTrackIndex_);
                    setActiveView(WorkspaceView::HardwarePanel);
                    break;
                case TransportAction::PresetPrev:
                    prevPreset();
                    break;
                case TransportAction::PresetNext:
                    nextPreset();
                    break;
                case TransportAction::Toggle3dConsole:
                case TransportAction::ToggleCameraFocus:
                    break;
                default:
                    break;
            }
            return;
        }

        // 2. Check Bottom Navigation Control Strip Hit
        auto navHit = hitTestBottomNav(x, y);
        if (navHit.hit) {
            switch (navHit.action) {
                case BottomNavAction::Arranger:
                    setActiveView(WorkspaceView::Arranger);
                    break;
                case BottomNavAction::Edit:
                    setActiveView(WorkspaceView::Edit);
                    break;
                case BottomNavAction::Track:
                    if (modularArrangerView_) {
                        selectedTrackIndex_ = modularArrangerView_->getActiveTrackIndex();
                        modularArrangerView_->syncDrawerDataForTrack(selectedTrackIndex_);
                        if (modularTrackInspectorView_) {
                            modularTrackInspectorView_->syncFromDrawerData(modularArrangerView_->getDrawerData());
                        }
                    }
                    setActiveView(WorkspaceView::Track);
                    break;
                case BottomNavAction::Mixer:
                    setActiveView(WorkspaceView::Mixer);
                    break;
                case BottomNavAction::Design:
                    setActiveView(WorkspaceView::Design);
                    break;
                default:
                    break;
            }
            return;
        }

        // 2b. Check Virtual Keyboard Drawer Hit (Global across views)
        auto drawerHit = hitTestVirtualKeyboardDrawer(x, y);
        if (drawerHit.hit) {
            if (drawerHit.isPullTab) {
                toggleVirtualKeyboardDrawer();
                return;
            }
            if (drawerHit.isOctaveDown) {
                setVirtualKeyboardBaseOctave(virtualKeyboardBaseOctave_ - 1);
                return;
            }
            if (drawerHit.isOctaveUp) {
                setVirtualKeyboardBaseOctave(virtualKeyboardBaseOctave_ + 1);
                return;
            }
            if (drawerHit.isKey) {
                virtualKeyboardActivePitch_ = drawerHit.pitch;
                previewingPitch_ = drawerHit.pitch;
                previewingVelocity_ = drawerHit.velocity;
                if (engine_) {
                    engine_->postNoteOn(static_cast<uint8_t>(drawerHit.pitch), drawerHit.velocity, false, false, static_cast<int>(selectedTrackIndex_));
                }
                dragMode_ = DragMode::VirtualKeyboardGlissando;
                return;
            }
        }

        // 3. Sub-bar Switchers (y: 56 .. 95)
        if (activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) {
            auto sn = hitTestEditSubNav(x, y);
            if (sn.hit) {
                setEditSubView(sn.subView);
                return;
            }
        }

        // 4. Dispatch clicks according to Active View
        if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
            bool skipModularEdit = false;
            if (editSubView_ == EditSubView::Tracker) {
                float folX = static_cast<float>(width_) - 250.0f;
                bool isFol = (x >= folX && x <= (folX + 105.0f) && y >= 103.0f && y <= 127.0f);
                if (isFol || hitTestTracker(x, y).hit) {
                    skipModularEdit = true;
                }
            }
            if (!skipModularEdit) {
                ViewContext ctx = createViewContext();
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Down;
                pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
                pev.x = x;
                pev.y = y;
                pev.rawX = x;
                pev.rawY = y;
                modularEditView_->setActiveTrackIndex(selectedTrackIndex_);
                if (modularEditView_->handlePointer(pev, ctx)) {
                    return;
                }
            }
        }
        if (activeView_ == WorkspaceView::Arranger) {
            if (modularArrangerView_) {
                ViewContext ctx = createViewContext();
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Down;
                pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
                pev.x = x;
                pev.y = y;
                pev.rawX = x;
                pev.rawY = y;
                if (modularArrangerView_->handlePointer(pev, ctx)) {
                    setSelectedTrackIndex(modularArrangerView_->getActiveTrackIndex());
                    return;
                }
            }
            auto arrHit = hitTestArranger(x, y);
            if (arrHit.hit) {
                if (arrHit.area == ArrangerHitArea::Ruler) {
                    uint32_t curStep = engine_ ? engine_->getSequencer().getTransport().getCurrentStep() : 0;
                    timelineScrubPresenter_.startRulerScrub(x, 210.0f, 60.0f, curStep, 18 * 16,
                        [this](uint32_t step) {
                            if (engine_) engine_->getSequencer().getTransport().setPosition(step);
                        });
                    activeDragHandler_ = &timelineScrubPresenter_;
                    dragMode_ = DragMode::ArrangerRulerScrub;
                    if (engine_) {
                        engine_->getSequencer().getTransport().setPosition(arrHit.step);
                    }
                } else if (arrHit.area == ArrangerHitArea::TrackMute) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    bool currentMute = (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) ? arrangerTracks_[arrHit.trackIndex].mute : false;
                    setTrackMuteState(arrHit.trackIndex, !currentMute);
                } else if (arrHit.area == ArrangerHitArea::TrackSolo) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    bool currentSolo = (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) ? arrangerTracks_[arrHit.trackIndex].solo : false;
                    setTrackSoloState(arrHit.trackIndex, !currentSolo);
                } else if (arrHit.area == ArrangerHitArea::TrackFreeze) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    bool currentFz = (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) ? arrangerTracks_[arrHit.trackIndex].freeze : false;
                    setTrackFreezeState(arrHit.trackIndex, !currentFz);
                } else if (arrHit.area == ArrangerHitArea::TrackVolume) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    activeArrangerTrack_ = arrHit.trackIndex;
                    if (isCtrlPressed()) {
                        if (static_cast<size_t>(arrHit.trackIndex) < mixerStrips_.size()) mixerStrips_[arrHit.trackIndex].volume = 1.0f;
                        if (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) arrangerTracks_[arrHit.trackIndex].volume = 1.0f;
                        if (engine_) engine_->setTrackVolume(arrHit.trackIndex, 1.0f);
                        return;
                    }
                    dragStartX_ = x;
                    if (static_cast<size_t>(arrHit.trackIndex) < mixerStrips_.size()) {
                        mixerStrips_[arrHit.trackIndex].volume = arrHit.normVal;
                    }
                    if (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) {
                        arrangerTracks_[arrHit.trackIndex].volume = arrHit.normVal;
                    }
                    presenter::ScalarDragConfig cfg{};
                    cfg.dragMode = DragMode::ArrangerTrackVolume;
                    cfg.direction = presenter::DragDirection::HorizontalRightIncreases;
                    cfg.minValue = 0.0f;
                    cfg.maxValue = 1.5f;
                    cfg.defaultValue = 1.0f;
                    cfg.sensitivity = 1.5f / 114.0f;
                    cfg.relativeMode = true;
                    cfg.captureMouse = true;
                    scalarDragPresenter_.startDrag(x, y, arrHit.normVal, cfg,
                        [this, trk = activeArrangerTrack_](float v) {
                            if (trk < mixerStrips_.size()) mixerStrips_[trk].volume = v;
                            if (trk < arrangerTracks_.size()) arrangerTracks_[trk].volume = v;
                            if (engine_) engine_->setTrackVolume(trk, v);
                        });
                    activeDragHandler_ = &scalarDragPresenter_;
                    dragMode_ = DragMode::ArrangerTrackVolume;
                } else if (arrHit.area == ArrangerHitArea::TrackPan) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    activeArrangerTrack_ = arrHit.trackIndex;
                    if (isCtrlPressed()) {
                        if (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) arrangerTracks_[arrHit.trackIndex].pan = 0.0f;
                        if (static_cast<size_t>(arrHit.trackIndex) < mixerStrips_.size()) mixerStrips_[arrHit.trackIndex].pan = 0.0f;
                        if (engine_) engine_->setTrackPan(arrHit.trackIndex, 0.0f);
                        return;
                    }
                    dragStartX_ = x;
                    dragStartY_ = y;
                    dragStartPan_ = (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) ? arrangerTracks_[arrHit.trackIndex].pan : 0.0f;
                    presenter::ScalarDragConfig cfg{};
                    cfg.dragMode = DragMode::ArrangerTrackPan;
                    cfg.direction = presenter::DragDirection::BidirectionalPan;
                    cfg.minValue = -1.0f;
                    cfg.maxValue = 1.0f;
                    cfg.defaultValue = 0.0f;
                    cfg.sensitivity = 0.015f;
                    cfg.relativeMode = true;
                    cfg.captureMouse = true;
                    scalarDragPresenter_.startDrag(x, y, dragStartPan_, cfg,
                        [this, trk = activeArrangerTrack_](float p) {
                            if (trk < arrangerTracks_.size()) arrangerTracks_[trk].pan = p;
                            if (trk < mixerStrips_.size()) mixerStrips_[trk].pan = p;
                            if (engine_) engine_->setTrackPan(trk, p);
                        });
                    activeDragHandler_ = &scalarDragPresenter_;
                    dragMode_ = DragMode::ArrangerTrackPan;
                } else if (arrHit.area == ArrangerHitArea::TrackEditButton) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    setActiveView(WorkspaceView::Edit);
                } else if (arrHit.area == ArrangerHitArea::TrackHeader) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                    if (arrHit.trackIndex == lastClickedTrack_ && (nowMs - static_cast<int64_t>(lastTrackClickTime_)) < 400) {
                        setActiveView(WorkspaceView::Edit);
                    }
                    lastClickedTrack_ = arrHit.trackIndex;
                    lastTrackClickTime_ = static_cast<double>(nowMs);
                } else if (arrHit.area == ArrangerHitArea::Clip) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    selectArrangerClip(arrHit.trackIndex, arrHit.clipIndex);
                    arrangerPropertiesExpanded_ = true;
                    arrangerInspectorTab_ = ArrangerInspectorTab::Clip;
                    if (engine_) {
                        engine_->getSequencer().getTransport().setPosition(arrHit.step);
                    }
                    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                    if (lastClickedClipTrack_ == static_cast<int>(arrHit.trackIndex) &&
                        lastClickedClipIdx_ == arrHit.clipIndex &&
                        (nowMs - static_cast<int64_t>(lastClipClickTime_)) < 400) {
                        setActiveView(WorkspaceView::Edit);
                    }
                    lastClickedClipTrack_ = static_cast<int>(arrHit.trackIndex);
                    lastClickedClipIdx_ = arrHit.clipIndex;
                    lastClipClickTime_ = static_cast<double>(nowMs);
                } else if (arrHit.area == ArrangerHitArea::SidebarPullTab) {
                    dragStartX_ = x;
                    dragStartValGeneric_ = arrangerPropertiesWidth_;
                    presenter::SplitterConfig scfg{};
                    scfg.dragMode = DragMode::ArrangerPropertiesResize;
                    scfg.minWidth = kArrangerPropertiesMinW;
                    scfg.maxWidth = kArrangerPropertiesMaxW;
                    scfg.defaultWidth = 300.0f;
                    scfg.collapseThresholdMargin = 25.0f;
                    splitterDragPresenter_.startDrag(x, arrangerPropertiesWidth_, arrangerPropertiesExpanded_, scfg,
                        [this](float w, bool exp) {
                            arrangerPropertiesWidth_ = w;
                            arrangerPropertiesExpanded_ = exp;
                            if (!exp) dragMode_ = DragMode::None;
                        });
                    activeDragHandler_ = &splitterDragPresenter_;
                    dragMode_ = DragMode::ArrangerPropertiesResize;
                } else if (arrHit.area == ArrangerHitArea::SidebarCloseButton) {
                    arrangerPropertiesExpanded_ = false;
                } else if (arrHit.area == ArrangerHitArea::SidebarTabTrack) {
                    arrangerInspectorTab_ = ArrangerInspectorTab::Track;
                } else if (arrHit.area == ArrangerHitArea::SidebarTabClip) {
                    arrangerInspectorTab_ = ArrangerInspectorTab::Clip;
                } else if (arrHit.area == ArrangerHitArea::SidebarColorSwatch) {
                    static const float swatches[8][3] = {
                        {0.0f, 0.90f, 1.0f},   // Neon Cyan
                        {1.0f, 0.55f, 0.0f},   // Neon Amber
                        {0.0f, 1.00f, 0.40f},  // Acid Green
                        {1.0f, 0.00f, 0.48f},  // Hot Pink
                        {0.74f, 0.00f, 1.0f},  // Electric Purple
                        {1.0f, 0.20f, 0.20f},  // Crimson Red
                        {1.0f, 0.85f, 0.0f},   // Gold Yellow
                        {0.20f, 0.60f, 1.0f}   // Sky Blue
                    };
                    if (arrHit.colorSwatchIndex < 8 && selectedTrackIndex_ < arrangerTracks_.size()) {
                        auto& trk = arrangerTracks_[selectedTrackIndex_];
                        trk.r = swatches[arrHit.colorSwatchIndex][0];
                        trk.g = swatches[arrHit.colorSwatchIndex][1];
                        trk.b = swatches[arrHit.colorSwatchIndex][2];
                        for (auto& clp : trk.clips) {
                            clp.r = trk.r;
                            clp.g = trk.g;
                            clp.b = trk.b;
                        }
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarClipLoopToggle) {
                    if (selectedArrangerClipTrack_ >= 0 && selectedArrangerClipIndex_ >= 0 &&
                        static_cast<size_t>(selectedArrangerClipTrack_) < arrangerTracks_.size()) {
                        auto& trk = arrangerTracks_[selectedArrangerClipTrack_];
                        if (static_cast<size_t>(selectedArrangerClipIndex_) < trk.clips.size()) {
                            trk.clips[selectedArrangerClipIndex_].isLooped = !trk.clips[selectedArrangerClipIndex_].isLooped;
                        }
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarClipTranspose) {
                    if (selectedArrangerClipTrack_ >= 0 && selectedArrangerClipIndex_ >= 0 &&
                        static_cast<size_t>(selectedArrangerClipTrack_) < arrangerTracks_.size()) {
                        auto& trk = arrangerTracks_[selectedArrangerClipTrack_];
                        if (static_cast<size_t>(selectedArrangerClipIndex_) < trk.clips.size()) {
                            if (arrHit.transposeDelta == 0) {
                                trk.clips[selectedArrangerClipIndex_].transposeSemitones = 0;
                            } else {
                                trk.clips[selectedArrangerClipIndex_].transposeSemitones += arrHit.transposeDelta;
                                trk.clips[selectedArrangerClipIndex_].transposeSemitones = std::clamp(trk.clips[selectedArrangerClipIndex_].transposeSemitones, -48, 48);
                            }
                        }
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarEqLow) {
                    if (selectedTrackIndex_ < arrangerTracks_.size()) {
                        arrangerTracks_[selectedTrackIndex_].eqLow = arrHit.normVal;
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarEqMid) {
                    if (selectedTrackIndex_ < arrangerTracks_.size()) {
                        arrangerTracks_[selectedTrackIndex_].eqMid = arrHit.normVal;
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarEqHigh) {
                    if (selectedTrackIndex_ < arrangerTracks_.size()) {
                        arrangerTracks_[selectedTrackIndex_].eqHigh = arrHit.normVal;
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarAction) {
                    if (arrHit.actionName == "EditInPianoRoll") {
                        setActiveView(WorkspaceView::Edit);
                    } else if (arrHit.actionName == "DuplicateTrack") {
                        if (selectedTrackIndex_ < arrangerTracks_.size()) {
                            ArrangerTrackData dup = arrangerTracks_[selectedTrackIndex_];
                            dup.name += " (Copy)";
                            arrangerTracks_.push_back(dup);
                        }
                    } else if (arrHit.actionName == "DuplicateClip") {
                        if (selectedArrangerClipTrack_ >= 0 && selectedArrangerClipIndex_ >= 0 &&
                            static_cast<size_t>(selectedArrangerClipTrack_) < arrangerTracks_.size()) {
                            auto& trk = arrangerTracks_[selectedArrangerClipTrack_];
                            if (static_cast<size_t>(selectedArrangerClipIndex_) < trk.clips.size()) {
                                ArrangerClip dup = trk.clips[selectedArrangerClipIndex_];
                                dup.startBar += dup.barLength;
                                trk.clips.push_back(dup);
                                selectedArrangerClipIndex_ = static_cast<int>(trk.clips.size() - 1);
                            }
                        }
                    } else if (arrHit.actionName == "DeleteClip") {
                        if (selectedArrangerClipTrack_ >= 0 && selectedArrangerClipIndex_ >= 0 &&
                            static_cast<size_t>(selectedArrangerClipTrack_) < arrangerTracks_.size()) {
                            auto& trk = arrangerTracks_[selectedArrangerClipTrack_];
                            if (static_cast<size_t>(selectedArrangerClipIndex_) < trk.clips.size()) {
                                trk.clips.erase(trk.clips.begin() + selectedArrangerClipIndex_);
                                selectedArrangerClipIndex_ = -1;
                                selectedArrangerClipTrack_ = -1;
                            }
                        }
                    }
                } else if (arrHit.area == ArrangerHitArea::OverviewScrollbar) {
                    const float trackHeaderW = 210.0f;
                    const float miniTrackW = static_cast<float>(width_) - trackHeaderW - 20.0f;
                    uint32_t curStep = engine_ ? engine_->getSequencer().getTransport().getCurrentStep() : 0;
                    timelineScrubPresenter_.startOverviewScrub(x, trackHeaderW, miniTrackW, curStep, 18 * 16,
                        [this](uint32_t step) {
                            if (engine_) engine_->getSequencer().getTransport().setPosition(step);
                        });
                    activeDragHandler_ = &timelineScrubPresenter_;
                    dragMode_ = DragMode::ArrangerOverviewScroll;
                } else if (arrHit.area == ArrangerHitArea::SidebarTrackInspector || arrHit.trackInspectorHit.hit) {
                    handleTrackInspectorInteraction(arrHit.trackInspectorHit, x, y);
                }
            }
            return;
        }

        if (activeView_ == WorkspaceView::HardwarePanel || activeView_ == WorkspaceView::Track) {
            auto inspHit = hitTestTrackInspector(x, y);
            if (inspHit.hit) {
                handleTrackInspectorInteraction(inspHit, x, y);
                return;
            }
            if (modularTrackInspectorView_) {
                ViewContext ctx = createViewContext();
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Down;
                pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
                pev.x = x;
                pev.y = y;
                pev.rawX = x;
                pev.rawY = y;
                if (modularTrackInspectorView_->handlePointer(pev, ctx)) return;
            }
            auto tabHit = hitTestTrackTab(x, y);
            if (tabHit.hit) {
                setSelectedTrackIndex(tabHit.trackIndex);
                if (modularTrackInspectorView_) {
                    modularTrackInspectorView_->setActiveTrack(tabHit.trackIndex);
                }
                return;
            }
            return;
        }

        if (activeView_ == WorkspaceView::Mixer) {
            auto mixerHit = hitTestMixer(x, y);
            if (mixerHit.hit) {
                // Toolbar Presets & Toggles
                if (mixerHit.isPresetFull) {
                    setMixerSectionPreset(MixerSectionPreset::Full);
                    setStatusMessage("[MIXER] Preset: FULL STUDIO CONSOLE");
                    return;
                } else if (mixerHit.isPresetFaders) {
                    setMixerSectionPreset(MixerSectionPreset::FadersOnly);
                    setStatusMessage("[MIXER] Preset: COMPACT FADERS");
                    return;
                } else if (mixerHit.isPresetMeters) {
                    setMixerSectionPreset(MixerSectionPreset::FadersAndMeters);
                    setStatusMessage("[MIXER] Preset: FADERS & METERS");
                    return;
                } else if (mixerHit.isToggleRouting) {
                    showMixerRouting_ = !showMixerRouting_;
                    showMixerAutomation_ = showMixerRouting_;
                    setStatusMessage(std::string("[MIXER] Routing Section: ") + (showMixerRouting_ ? "ENABLED" : "HIDDEN"));
                    return;
                } else if (mixerHit.isTogglePan) {
                    showMixerPan_ = !showMixerPan_;
                    setStatusMessage(std::string("[MIXER] Panorama Section: ") + (showMixerPan_ ? "ENABLED" : "HIDDEN"));
                    return;
                } else if (mixerHit.isToggleButtons) {
                    showMixerButtons_ = !showMixerButtons_;
                    setStatusMessage(std::string("[MIXER] Console Buttons: ") + (showMixerButtons_ ? "ENABLED" : "HIDDEN"));
                    return;
                } else if (mixerHit.isToggleMeters) {
                    showMixerMeters_ = !showMixerMeters_;
                    setStatusMessage(std::string("[MIXER] LED Meters: ") + (showMixerMeters_ ? "ENABLED" : "HIDDEN"));
                    return;
                } else if (mixerHit.isAutomation && mixerHit.channelIndex < mixerStrips_.size()) {
                    mixerStrips_[mixerHit.channelIndex].automationMode = (mixerStrips_[mixerHit.channelIndex].automationMode + 1) % 4;
                    const char* aModes[] = {"TRIM", "READ", "TOUCH", "LATCH"};
                    setStatusMessage("Track " + std::to_string(mixerHit.channelIndex + 1) + " Automation: " + aModes[mixerStrips_[mixerHit.channelIndex].automationMode]);
                    return;
                } else if (mixerHit.isPhase && mixerHit.channelIndex < mixerStrips_.size()) {
                    mixerStrips_[mixerHit.channelIndex].phaseInvert = !mixerStrips_[mixerHit.channelIndex].phaseInvert;
                    setStatusMessage("Track " + std::to_string(mixerHit.channelIndex + 1) + ": Phase " + (mixerStrips_[mixerHit.channelIndex].phaseInvert ? "INVERTED (180°)" : "NORMAL"));
                    return;
                } else if (mixerHit.isFreeze && mixerHit.channelIndex < mixerStrips_.size()) {
                    bool currentFz = mixerStrips_[mixerHit.channelIndex].freeze;
                    setTrackFreezeState(mixerHit.channelIndex, !currentFz);
                    return;
                } else if (mixerHit.isFxIn && mixerHit.channelIndex < mixerStrips_.size()) {
                    mixerStrips_[mixerHit.channelIndex].fxIn = !mixerStrips_[mixerHit.channelIndex].fxIn;
                    setStatusMessage("Track " + std::to_string(mixerHit.channelIndex + 1) + ": Inserts " + (mixerStrips_[mixerHit.channelIndex].fxIn ? "ACTIVE" : "BYPASSED"));
                    return;
                }
            }

            // Track Properties Sidebar Boundary Check & Hit Routing
            const float w = static_cast<float>(width_);
            const float propW = mixerPropertiesExpanded_ ? mixerPropertiesWidth_ : 0.0f;
            const float pullTabW = kMixerPullTabW;
            const float drawerTotalW = mixerPropertiesExpanded_ ? (pullTabW + propW) : pullTabW;
            const float browserOffset = browserOpen_ ? ProjectBrowserDrawer::getDrawerWidth() : 0.0f;
            const float rightBoundary = w - drawerTotalW - browserOffset;
            const float pullTabX = rightBoundary;
            const float propX = pullTabX + pullTabW;
            const float topY = 56.0f;
            const float bottomY = static_cast<float>(height_) - 48.0f - 6.0f;

            if (mixerPropertiesExpanded_ && x >= pullTabX && x <= propX + propW && y >= topY && y <= bottomY) {
                if (mixerHit.isPullTab) {
                    dragStartX_ = x;
                    dragStartValGeneric_ = mixerPropertiesWidth_;
                    presenter::SplitterConfig scfg{};
                    scfg.dragMode = DragMode::MixerPropertiesResize;
                    scfg.minWidth = kMixerPropertiesMinW;
                    scfg.maxWidth = kMixerPropertiesMaxW;
                    scfg.defaultWidth = 360.0f;
                    scfg.collapseThresholdMargin = 25.0f;
                    splitterDragPresenter_.startDrag(x, mixerPropertiesWidth_, mixerPropertiesExpanded_, scfg,
                        [this](float widthVal, bool exp) {
                            setMixerPropertiesWidth(widthVal);
                            setMixerPropertiesExpanded(exp);
                            if (!exp) dragMode_ = DragMode::None;
                        });
                    activeDragHandler_ = &splitterDragPresenter_;
                    dragMode_ = DragMode::MixerPropertiesResize;
                    return;
                }
                if (mixerHit.isPropertiesClose) {
                    setMixerPropertiesExpanded(false);
                    return;
                }
                if (mixerHit.trackInspectorHit.hit) {
                    handleTrackInspectorInteraction(mixerHit.trackInspectorHit, x, y);
                    return;
                }
                // Absorb click inside sidebar so it never clicks underlying mixer channels
                return;
            } else if (!mixerPropertiesExpanded_ && x >= pullTabX && x <= pullTabX + pullTabW && y >= topY && y <= bottomY) {
                if (mixerHit.isPullTab) {
                    dragStartX_ = x;
                    dragStartValGeneric_ = mixerPropertiesWidth_;
                    presenter::SplitterConfig scfg{};
                    scfg.dragMode = DragMode::MixerPropertiesResize;
                    scfg.minWidth = kMixerPropertiesMinW;
                    scfg.maxWidth = kMixerPropertiesMaxW;
                    scfg.defaultWidth = 360.0f;
                    scfg.collapseThresholdMargin = 25.0f;
                    splitterDragPresenter_.startDrag(x, mixerPropertiesWidth_, mixerPropertiesExpanded_, scfg,
                        [this](float widthVal, bool exp) {
                            setMixerPropertiesWidth(widthVal);
                            setMixerPropertiesExpanded(exp);
                            if (!exp) dragMode_ = DragMode::None;
                        });
                    activeDragHandler_ = &splitterDragPresenter_;
                    dragMode_ = DragMode::MixerPropertiesResize;
                    return;
                }
            }

            if (modularMixerView_) {
                ViewContext ctx = createViewContext();
                modularMixerView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Down;
                pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
                pev.x = x;
                pev.y = y;
                pev.rawX = x;
                pev.rawY = y;
                if (modularMixerView_->handlePointer(pev, ctx)) {
                    mixerPropertiesExpanded_ = modularMixerView_->isPropertiesExpanded();
                    mixerPropertiesWidth_ = modularMixerView_->getPropertiesWidth();
                    if (modularMixerView_->getActivePanIndex() == -1) {
                        dragMode_ = DragMode::MasterPan;
                        dragStartY_ = y;
                        dragStartValGeneric_ = masterPan_;
                    } else if (modularMixerView_->getActiveFaderIndex() == -1) {
                        dragMode_ = DragMode::MasterFader;
                        dragStartY_ = y;
                        dragStartValGeneric_ = masterVolume_;
                    } else if (modularMixerView_->getActivePanIndex() >= 0) {
                        dragMode_ = DragMode::MixerPan;
                        activeMixerChannel_ = static_cast<size_t>(modularMixerView_->getActivePanIndex());
                        dragStartY_ = y;
                        if (activeMixerChannel_ < mixerStrips_.size()) {
                            dragStartValGeneric_ = mixerStrips_[activeMixerChannel_].pan;
                        }
                    } else if (modularMixerView_->getActiveFaderIndex() >= 0) {
                        dragMode_ = DragMode::MixerFader;
                        activeMixerChannel_ = static_cast<size_t>(modularMixerView_->getActiveFaderIndex());
                        dragStartY_ = y;
                        if (activeMixerChannel_ < mixerStrips_.size()) {
                            dragStartValGeneric_ = mixerStrips_[activeMixerChannel_].volume;
                        }
                    }
                    return;
                }
                return;
            }
        }

        if (activeView_ == WorkspaceView::Tracker || activeView_ == WorkspaceView::Edit) {
            auto* activeTrk = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks())
                                  ? engine_->getSequencer().getTrack(selectedTrackIndex_)
                                  : nullptr;

            // 0. Check Note Selection Sidebar Hit first
            if (activeTrk && activeTrk->hasSelectedNotes()) {
                const float bPanelY = static_cast<float>(height_) - 48.0f;
                const float sbW = 265.0f;
                const float sbX = static_cast<float>(width_) - sbW - 10.0f;
                const float sbY = 98.0f;
                const float sbH = bPanelY - sbY - 6.0f;
                auto sbHit = hitTestSelectionSidebar(x, y, sbX, sbY, sbW, sbH, *activeTrk);
                if (sbHit.hit) {
                    switch (sbHit.action) {
                        case SelectionSidebarAction::Close:
                        case SelectionSidebarAction::Clear:
                            activeTrk->clearSelection();
                            break;
                        case SelectionSidebarAction::Delete:
                            activeTrk->deleteSelectedNotes();
                            recordProjectHistory("Delete Selected Notes on Track " + std::to_string(selectedTrackIndex_ + 1), "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::TransposeOctDown:
                            activeTrk->transposeSelectedNotes(-12);
                            recordProjectHistory("Transpose -1 Octave", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::TransposeSemiDown:
                            activeTrk->transposeSelectedNotes(-1);
                            recordProjectHistory("Transpose -1 Semitone", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::TransposeSemiUp:
                            activeTrk->transposeSelectedNotes(1);
                            recordProjectHistory("Transpose +1 Semitone", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::TransposeOctUp:
                            activeTrk->transposeSelectedNotes(12);
                            recordProjectHistory("Transpose +1 Octave", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::NudgeLeft:
                            activeTrk->nudgeSelectedNotes(-1);
                            recordProjectHistory("Nudge Notes Left", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::NudgeRight:
                            activeTrk->nudgeSelectedNotes(1);
                            recordProjectHistory("Nudge Notes Right", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::ShortenDuration:
                            activeTrk->changeSelectedNotesDuration(-0.25f);
                            recordProjectHistory("Shorten Duration", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::LengthenDuration:
                            activeTrk->changeSelectedNotesDuration(0.25f);
                            recordProjectHistory("Lengthen Duration", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::DurPreset25:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                activeTrk->getStep(s).gateLength = 0.25f;
                            }
                            recordProjectHistory("Set Duration 0.25", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::DurPreset50:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                activeTrk->getStep(s).gateLength = 0.50f;
                            }
                            recordProjectHistory("Set Duration 0.50", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::DurPreset75:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                activeTrk->getStep(s).gateLength = 0.75f;
                            }
                            recordProjectHistory("Set Duration 0.75", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::DurPreset100:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                activeTrk->getStep(s).gateLength = 1.00f;
                            }
                            recordProjectHistory("Set Duration 1.00", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::Vel25:
                            activeTrk->setSelectedNotesVelocity(0.25f);
                            recordProjectHistory("Set Velocity 25%", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::Vel50:
                            activeTrk->setSelectedNotesVelocity(0.50f);
                            recordProjectHistory("Set Velocity 50%", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::Vel75:
                            activeTrk->setSelectedNotesVelocity(0.75f);
                            recordProjectHistory("Set Velocity 75%", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::Vel100:
                            activeTrk->setSelectedNotesVelocity(1.00f);
                            recordProjectHistory("Set Velocity 100%", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::Humanize:
                            activeTrk->humanizeSelectedNotes(0.15f);
                            recordProjectHistory("Humanize Velocity", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::ToggleSlide:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                auto& st = activeTrk->getStep(s);
                                st.slide = !st.slide;
                            }
                            recordProjectHistory("Toggle Slide", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::ToggleAccent:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                auto& st = activeTrk->getStep(s);
                                st.accent = !st.accent;
                            }
                            recordProjectHistory("Toggle Accent", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::SelectAll:
                            activeTrk->selectAllNotes();
                            break;
                        case SelectionSidebarAction::Invert:
                            activeTrk->invertNoteSelection();
                            break;
                        case SelectionSidebarAction::Quantize:
                            activeTrk->quantizeSelectedNotes(1);
                            recordProjectHistory("Quantize Notes", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        default:
                            break;
                    }
                    return;
                }
            }

            if (editSubView_ == EditSubView::PianoRoll) {
                // 0. Piano Roll Scrollbar Hit
                auto sbHit = hitTestPianoRollScrollbar(x, y);
                if (sbHit.hit) {
                    if (sbHit.isVertical) {
                        if (sbHit.isThumb) {
                            dragMode_ = DragMode::PianoRollScrollbarV;
                            panStartMouseY_ = y;
                            panStartScrollY_ = pianoRollScrollY_;
                        } else {
                            float totalH = pianoRollKeyboard_.getTotalVerticalContentHeight();
                            float maxScrollY = std::max(0.0f, totalH - 550.0f);
                            setPianoRollScrollY(sbHit.normOffset * maxScrollY);
                        }
                        return;
                    } else {
                        if (sbHit.isThumb) {
                            dragMode_ = DragMode::PianoRollScrollbarH;
                            panStartMouseX_ = x;
                            panStartScrollX_ = pianoRollScrollX_;
                        } else {
                            const uint32_t totalSteps = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() && engine_->getSequencer().getTrack(selectedTrackIndex_))
                                                            ? std::max(64u, engine_->getSequencer().getTrack(selectedTrackIndex_)->getNumSteps())
                                                            : 64u;
                            float totalW = static_cast<float>(totalSteps) * pianoRollStepW_;
                            float maxScrollX = std::max(0.0f, totalW - 1000.0f);
                            setPianoRollScrollX(sbHit.normOffset * maxScrollX);
                        }
                        return;
                    }
                }

                // 1. Piano Key Hit (Auditory & Visual Note Preview with Velocity & Glissando Drag)
                auto keyHit = hitTestPianoKey(x, y);
                if (keyHit.hit) {
                    previewingPitch_ = keyHit.pitch;
                    previewingVelocity_ = keyHit.velocity;
                    virtualKeyboardActivePitch_ = keyHit.pitch;
                    dragMode_ = DragMode::VirtualKeyboardGlissando;
                    if (engine_) {
                        engine_->postNoteOn(keyHit.pitch, keyHit.velocity, false, false, static_cast<int>(selectedTrackIndex_));
                    }
                    return;
                }

                // 2. Check Hit on Existing Note Blocks or their Resize Handles
                if (activeTrk) {
                    const int minPitch = pianoRollKeyboard_.getMinPitch();
                    const int maxPitch = pianoRollKeyboard_.getMaxPitch();
                    const float topY = 100.0f;
                    const float rowH = pianoRollRowH_;
                    const float gridX = 110.0f;
                    const float stepW = pianoRollStepW_;

                    for (uint32_t s = 0; s < activeTrk->getNumSteps(); ++s) {
                        const auto& st = activeTrk->getStep(s);
                        if (st.active && st.note >= minPitch && st.note <= maxPitch) {
                            float nx = gridX + static_cast<float>(s) * stepW - pianoRollScrollX_ + 2.0f;
                            float ny = topY + static_cast<float>(maxPitch - st.note) * rowH - pianoRollScrollY_ + 2.0f;
                            float nw = std::max(14.0f, (stepW * st.gateLength) - 4.0f);
                            float nh = rowH - 4.0f;

                            // Check resize handle on selected note
                            if (activeTrk->isStepSelected(s) && nw >= 24.0f) {
                                float handleW = 12.0f;
                                float handleX = nx + nw - handleW;
                                if (x >= handleX && x <= nx + nw && y >= ny && y <= ny + nh) {
                                    dragMode_ = DragMode::PianoRollNoteResize;
                                    activeResizeStep_ = static_cast<int>(s);
                                    dragStartX_ = x;
                                    batchResizeStartDurations_.clear();
                                    for (uint32_t selS : activeTrk->getSelectedSteps()) {
                                        batchResizeStartDurations_[selS] = activeTrk->getStep(selS).gateLength;
                                    }
                                    return;
                                }
                            }

                            // Check note block body
                            if (x >= nx && x <= (nx + nw) && y >= ny && y <= (ny + nh)) {
                                bool isDouble = (lastClickedStep_ == static_cast<int>(s));
                                lastClickedStep_ = static_cast<int>(s);

                                if (isDouble) {
                                    // Double click -> delete note (from Eatsbeats)
                                    if (activeTrk->isStepSelected(s) && activeTrk->getSelectedNoteCount() > 1) {
                                        activeTrk->deleteSelectedNotes();
                                    } else {
                                        activeTrk->getStep(s) = sequencer::StepData{};
                                        activeTrk->clearSelection();
                                    }
                                    lastClickedStep_ = -1;
                                    recordProjectHistory("Delete Note", "NOTE");
                                    syncTrackToNoteScript();
                                    return;
                                }

                                // Single click -> select note (supporting Shift multi-selection toggle from Eatsbeats)
                                if (isShiftPressed()) {
                                    activeTrk->toggleStepSelection(s);
                                } else if (!activeTrk->isStepSelected(s)) {
                                    activeTrk->selectStep(s, true);
                                }
                                if (engine_) {
                                    engine_->postNoteOn(st.note, st.velocity, false, false, static_cast<int>(selectedTrackIndex_));
                                    previewingPitch_ = st.note;
                                    previewingVelocity_ = st.velocity;
                                }

                                dragMode_ = DragMode::PianoRollNoteMove;
                                activeMoveStep_ = static_cast<int>(s);
                                dragStartX_ = x;
                                dragStartY_ = y;
                                batchMoveStartPitches_.clear();
                                batchMoveStartSteps_.clear();
                                for (uint32_t selS : activeTrk->getSelectedSteps()) {
                                    batchMoveStartPitches_[selS] = activeTrk->getStep(selS).note;
                                    batchMoveStartSteps_[selS] = selS;
                                }
                                return;
                            }
                        }
                    }
                }

                // 3. Piano Roll Grid Hit (Empty space -> Marquee Drag or Double-Click Insert)
                auto gridHit = hitTestPianoRollGrid(x, y);
                if (gridHit.hit && activeTrk) {
                    bool isDouble = (lastClickedStep_ == -2);
                    lastClickedStep_ = -2;

                    if (isDouble) {
                        // Double click on empty grid -> place note and select it (from Eatsbeats)
                        auto st = activeTrk->getStep(gridHit.step);
                        st.active = true;
                        st.note = gridHit.pitch;
                        st.velocity = 0.85f;
                        st.gateLength = 0.75f;
                        activeTrk->setStep(gridHit.step, st);
                        activeTrk->selectStep(gridHit.step, true);
                        if (engine_) {
                            engine_->postNoteOn(st.note, st.velocity, false, false, static_cast<int>(selectedTrackIndex_));
                            previewingPitch_ = st.note;
                            previewingVelocity_ = st.velocity;
                        }
                        syncTrackToNoteScript();
                        recordProjectHistory("Add Note on Track " + std::to_string(selectedTrackIndex_ + 1), "NOTE");
                        lastClickedStep_ = -1;
                        return;
                    }

                    // Single click on empty grid -> marquee selection drag (clears selection if released without drag)
                    marqueeStartX_ = x;
                    marqueeStartY_ = y;
                    marqueeCurX_ = x;
                    marqueeCurY_ = y;
                    isMarqueeSelecting_ = true;
                    marqueeSelectPresenter_.startSelection(x, y, isShiftPressed(),
                        [this](const presenter::MarqueeRect& r, bool add) {
                            isMarqueeSelecting_ = true;
                            marqueeStartX_ = r.minX;
                            marqueeStartY_ = r.minY;
                            marqueeCurX_ = r.maxX;
                            marqueeCurY_ = r.maxY;
                            updatePianoRollMarqueeSelection(r.minX, r.minY, r.maxX, r.maxY, add);
                        },
                        [this](const presenter::MarqueeRect& /*r*/, bool /*add*/, bool isDrag) {
                            if (!isDrag) {
                                if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
                                    auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
                                    if (tr) tr->clearSelection();
                                }
                            }
                            isMarqueeSelecting_ = false;
                        });
                    activeDragHandler_ = &marqueeSelectPresenter_;
                    dragMode_ = DragMode::PianoRollMarquee;
                    return;
                }
            } else if (editSubView_ == EditSubView::Tracker) {
                // Tracker Grid Hit (Header Mute/Solo or Cell Selection/Audition)
                // Also check top Follow button at [width - 250, 103, 105, 24]
                float folX = static_cast<float>(width_) - 250.0f;
                if (x >= folX && x <= (folX + 105.0f) && y >= 103.0f && y <= 127.0f) {
                    trackerFollowPlayback_ = !trackerFollowPlayback_;
                    return;
                }

                auto trkHit = hitTestTracker(x, y);
                if (trkHit.hit && engine_) {
                    auto& seq = engine_->getSequencer();
                    if (trkHit.trackIndex < seq.getNumTracks()) {
                        auto* tr = seq.getTrack(trkHit.trackIndex);
                        if (tr) {
                            if (trkHit.isHeader) {
                                if (trkHit.isMute) {
                                    tr->setMuted(!tr->isMuted());
                                    if (trkHit.trackIndex < mixerStrips_.size()) {
                                        mixerStrips_[trkHit.trackIndex].mute = tr->isMuted();
                                    }
                                } else if (trkHit.isSolo) {
                                    tr->setSolo(!tr->isSolo());
                                    if (trkHit.trackIndex < mixerStrips_.size()) {
                                        mixerStrips_[trkHit.trackIndex].solo = tr->isSolo();
                                    }
                                } else {
                                    trackerSelectedTrack_ = trkHit.trackIndex;
                                    setSelectedTrackIndex(trkHit.trackIndex);
                                }
                            } else {
                                trackerSelectedRow_ = trkHit.row;
                                trackerSelectedTrack_ = trkHit.trackIndex;
                                setSelectedTrackIndex(trkHit.trackIndex);
                                auto st = tr->getStep(trkHit.row);
                                if (st.active) {
                                    tr->selectStep(trkHit.row, true);
                                    engine_->postNoteOn(st.note, 0.85f, false, false, static_cast<int>(trkHit.trackIndex));
                                    previewingPitch_ = st.note;
                                } else {
                                    tr->clearSelection();
                                }
                            }
                        }
                    }
                    return;
                }
            } else if (editSubView_ == EditSubView::Score) {
                auto scoreHit = hitTestScore(x, y);
                if (scoreHit.hit) {
                    switch (scoreHit.action) {
                        case ScoreHitAction::ClefGrandStaff:
                            setScoreClef(ScoreClef::GrandStaff);
                            break;
                        case ScoreHitAction::ClefTreble:
                            setScoreClef(ScoreClef::Treble);
                            break;
                        case ScoreHitAction::ClefBass:
                            setScoreClef(ScoreClef::Bass);
                            break;
                        case ScoreHitAction::DurationWhole:
                            setScoreDuration(ScoreNoteType::Whole);
                            break;
                        case ScoreHitAction::DurationHalf:
                            setScoreDuration(ScoreNoteType::Half);
                            break;
                        case ScoreHitAction::DurationQuarter:
                            setScoreDuration(ScoreNoteType::Quarter);
                            break;
                        case ScoreHitAction::DurationEighth:
                            setScoreDuration(ScoreNoteType::Eighth);
                            break;
                        case ScoreHitAction::DurationSixteenth:
                            setScoreDuration(ScoreNoteType::Sixteenth);
                            break;
                        case ScoreHitAction::AccidentalNone:
                            setScoreAccidental(ScoreAccidental::None);
                            break;
                        case ScoreHitAction::AccidentalSharp:
                            setScoreAccidental(ScoreAccidental::Sharp);
                            break;
                        case ScoreHitAction::AccidentalFlat:
                            setScoreAccidental(ScoreAccidental::Flat);
                            break;
                        case ScoreHitAction::StaffClick:
                            if (engine_) {
                                auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
                                if (tr) {
                                    auto st = tr->getStep(scoreHit.step);
                                    if (st.active && st.note == scoreHit.pitch) {
                                        st.active = false;
                                        tr->setStep(scoreHit.step, st);
                                        tr->clearSelection();
                                        syncTrackToNoteScript();
                                    } else {
                                        st.active = true;
                                        st.note = scoreHit.pitch;
                                        st.velocity = 0.85f;
                                        float durSteps = scoreDurationToSteps(scoreDuration_);
                                        st.gateLength = std::clamp(durSteps, 0.25f, 64.0f);
                                        engine_->postNoteOn(st.note, st.velocity, false, false, static_cast<int>(selectedTrackIndex_));
                                        previewingPitch_ = st.note;
                                        tr->setStep(scoreHit.step, st);
                                        tr->selectStep(scoreHit.step, true);
                                        syncTrackToNoteScript();
                                    }
                                }
                            }
                            break;
                        default:
                            break;
                    }
                    return;
                }
            } else if (editSubView_ == EditSubView::Script) {
                auto nsHit = hitTestNoteScript(x, y);
                if (nsHit.hit) {
                    if (nsHit.action == HitTestNoteScriptResult::Action::ApplySync) {
                        syncNoteScriptToTrack();
                        return;
                    } else if (nsHit.action == HitTestNoteScriptResult::Action::RevertFromTrack) {
                        syncTrackToNoteScript();
                        return;
                    } else if (nsHit.action == HitTestNoteScriptResult::Action::InsertTemplate) {
                        insertNoteScriptTemplate();
                        return;
                    } else if (nsHit.action == HitTestNoteScriptResult::Action::Clear) {
                        if (engine_) {
                            auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
                            if (tr) tr->clear();
                        }
                        syncTrackToNoteScript();
                        return;
                    } else if (nsHit.action == HitTestNoteScriptResult::Action::EditorClick) {
                        setNoteScriptCursor(nsHit.line, nsHit.col);
                        return;
                    }
                }
                return;
            } else {
                auto stepHit = hitTestStep(x, y);
                if (stepHit.hit && engine_) {
                    auto* tr = engine_->getSequencer().getTrack(stepHit.trackIndex);
                    if (tr) {
                        auto st = tr->getStep(stepHit.stepIndex);
                        st.active = !st.active;
                        st.velocity = 0.85f;
                        tr->setStep(stepHit.stepIndex, st);
                        syncTrackToNoteScript();
                        recordProjectHistory("Toggle Step " + std::to_string(stepHit.stepIndex + 1) + " on Track " + std::to_string(stepHit.trackIndex + 1), "STEP");
                    }
                }
                return;
            }
        }

        // Design interactions (Modular Rack, Eatscript, Split, or GUI Designer)
        if (activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) {
            // Direct active audio-graph rack interactions (knob tweaks & cable patching)
            auto jack = hitTestJack(x, y);
            if (jack.hit && jack.isOutput) {
                dragCableSrcNode_ = jack.nodeId;
                dragCableSrcPort_ = jack.portIndex;
                dragCableSrcPos_ = jack.position;
                cablePatchPresenter_.startPatch(jack.nodeId, jack.portIndex, jack.position.x, jack.position.y);
                activeDragHandler_ = &cablePatchPresenter_;
                dragMode_ = DragMode::PatchCable;
                return;
            }

            auto knob = hitTestKnob(x, y);
            if (knob.hit) {
                if (isCtrlPressed()) {
                    setKnobValue(knob.nodeId, knob.knobIndex, 0.5f);
                    dispatchKnobParameter(knob.nodeId, knob.knobIndex, 0.5f);
                    return;
                }
                activeKnobNode_ = knob.nodeId;
                activeKnobIndex_ = knob.knobIndex;
                dragStartY_ = y;
                dragStartValue_ = getKnobValue(knob.nodeId, knob.knobIndex);
                presenter::ScalarDragConfig cfg{};
                cfg.dragMode = DragMode::Knob;
                cfg.direction = presenter::DragDirection::VerticalUpIncreases;
                cfg.minValue = 0.0f;
                cfg.maxValue = 1.0f;
                cfg.defaultValue = 0.5f;
                cfg.sensitivity = 1.0f / 160.0f;
                cfg.relativeMode = true;
                cfg.captureMouse = true;
                scalarDragPresenter_.startDrag(x, y, dragStartValue_, cfg,
                    [this, n = knob.nodeId, idx = knob.knobIndex](float v) {
                        setKnobValue(n, idx, v);
                        dispatchKnobParameter(n, idx, v);
                    });
                activeDragHandler_ = &scalarDragPresenter_;
                dragMode_ = DragMode::Knob;
                return;
            }

            if (modularDesignView_) {
                ViewContext ctx = createViewContext();
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Down;
                pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
                pev.x = x;
                pev.y = y;
                pev.rawX = x;
                pev.rawY = y;
                if (modularDesignView_->handlePointer(pev, ctx)) {
                    switch (modularDesignView_->getSubMode()) {
                        case DesignSubMode::ModularRack:
                            designSubView_ = DesignSubView::ModularRack;
                            break;
                        case DesignSubMode::Code:
                            designSubView_ = DesignSubView::Eatscript;
                            break;
                        case DesignSubMode::GuiDesigner:
                            designSubView_ = DesignSubView::GuiDesigner;
                            break;
                        case DesignSubMode::Split:
                            break;
                    }
                    return;
                }
            }

            auto esHit = hitTestEatscript(x, y);
            if (esHit.hit) {
                if (esHit.action == HitTestEatscriptResult::Action::SubNavModular) {
                    setDesignSubView(DesignSubView::ModularRack);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::SubNavEatscript) {
                    setDesignSubView(DesignSubView::Eatscript);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::SubNavGuiDesigner) {
                    setDesignSubView(DesignSubView::GuiDesigner);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::Compile) {
                    compileActiveScript();
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::HotReload) {
                    hotReloadScriptToTrack(selectedTrackIndex_);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::ToggleAotView) {
                    scriptShowAotView_ = !scriptShowAotView_;
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::LoadTemplate) {
                    loadScriptTemplate(esHit.templateIndex);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::EditorClick) {
                    setScriptCursor(esHit.line, esHit.col);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::BindTrack) {
                    hotReloadScriptToTrack(selectedTrackIndex_);
                    return;
                }
            }

            if (designSubView_ == DesignSubView::Eatscript) {
                return;
            }

            // Check Step sequencer hit
            auto step = hitTestStep(x, y);
            if (step.hit && engine_) {
                auto* tr = engine_->getSequencer().getTrack(step.trackIndex);
                if (tr) {
                    auto st = tr->getStep(step.stepIndex);
                    st.active = !st.active;
                    st.velocity = 0.85f;
                    tr->setStep(step.stepIndex, st);
                }
                return;
            }
        }
    } else if (button == 1) { // Right click: manual value edit dialog, select notes by pitch on Piano Roll, or disconnect jack in Modular Rack
        // Check right-click on tempo / BPM area to open manual value edit dialog
        bool tempoHit = false;
        if (transportHeaderWidget_ && transportHeaderWidget_->getBpmBounds().contains(x, y)) {
            tempoHit = true;
        } else if (x >= 170.0f && x <= 300.0f && y >= 6.0f && y <= 48.0f) {
            tempoHit = true;
        }
        if (tempoHit) {
            float bVal = engine_ ? static_cast<float>(engine_->getSequencer().getTransport().getBpm()) : 120.0f;
            ValueEditRequest req;
            req.title = "PROJECT TEMPO";
            req.paramName = "Tempo";
            req.currentValue = bVal;
            req.minValue = 20.0f;
            req.maxValue = 300.0f;
            req.defaultValue = 120.0f;
            req.hasDefault = true;
            req.allowPercentage = false;
            req.unit = "BPM";
            const auto& theme = getTheme();
            req.accentColor = theme.tempoGlow;
            req.onCommit = [this](float val) {
                if (engine_) {
                    engine_->getSequencer().getTransport().setBpm(val);
                }
            };
            openValueEditDialog(req);
            return;
        }

        // Forward right click to active view for contextual edit dialogs (Mixer volume sliders, Arranger track headers, etc.)
        if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Right;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularMixerView_->handlePointer(pev, ctx)) return;
        }
        if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Right;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularArrangerView_->handlePointer(pev, ctx)) return;
        }
        if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Right;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularTrackInspectorView_->handlePointer(pev, ctx)) return;
        }
        if (activeView_ == WorkspaceView::ModularRack || activeView_ == WorkspaceView::Design) {
            auto jack = hitTestJack(x, y);
            if (jack.hit && engine_) {
                const auto conns = engine_->getGraph().getConnections();
                for (const auto& c : conns) {
                    if ((jack.isOutput && c.srcNode == jack.nodeId && c.srcPort == jack.portIndex) ||
                        (!jack.isOutput && c.dstNode == jack.nodeId && c.dstPort == jack.portIndex)) {
                        engine_->getGraph().disconnect(c.srcNode, c.srcPort, c.dstNode, c.dstPort);
                    }
                }
                engine_->getGraph().compile();
                canvas_.updateRackLayout(engine_->getGraph());
                initDefaultKnobValues();
                return;
            }
        }

        if ((activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) && modularDesignView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Right;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularDesignView_->handlePointer(pev, ctx)) return;
        }

        if (activeView_ == WorkspaceView::Edit && editSubView_ == EditSubView::PianoRoll) {
            auto keyHit = hitTestPianoKey(x, y);
            if (keyHit.hit) {
                // Right-click on vertical piano key: select all notes on active track matching this pitch (from Eatsbeats)
                selectPianoRollNotesByPitch(keyHit.pitch, isShiftPressed());
                return;
            }
        }
    } else if (button == 2) { // Middle click
        if ((activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) && modularDesignView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Middle;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularDesignView_->handlePointer(pev, ctx)) return;
        }
        if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Middle;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularArrangerView_->handlePointer(pev, ctx)) return;
        }
    }
}

void GuiWindow::onMouseUp(int button, float x, float y) {
    ColorPickerDialog* activeColorPicker = getActiveColorPickerDialog();
    if (activeColorPicker && activeColorPicker->isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        activeColorPicker->handlePointer(pev);
        return;
    }

    if (projectHubOpen_) {
        projectHubScrollArea_.stopDragging();
        activeCrtSliderIndex_ = -1;
        dragMode_ = DragMode::None;
        return;
    }

    if (fullscreenDeviceModal_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        fullscreenDeviceModal_.handlePointer(pev);
        markNeedsRedraw();
        return;
    }

    if (valueEditDialog_.isOpen()) {
        if (modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Up;
            pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularArrangerView_->getIconSearchDialog().handlePointer(pev)) return;
        }

        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (valueEditDialog_.handlePointer(pev)) return;
    }

    if (commandPaletteDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (commandPaletteDialog_.handlePointer(pev)) return;
    }

    if (audioToMidiDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (audioToMidiDialog_.handlePointer(pev)) return;
    }

    if (themeBrowserDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        themeBrowserDialog_.handlePointer(pev);
        markNeedsRedraw();
        return;
    }

    PluginSearchDialog* activePluginDialog = getActivePluginSearchDialog();
    if (activePluginDialog && activePluginDialog->isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (activePluginDialog->handlePointer(pev)) return;
    }

    if (modularArrangerView_ && modularArrangerView_->getCircleOfFifthsDialog().isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularArrangerView_->getCircleOfFifthsDialog().handlePointer(pev);
        return;
    }

    if (isAnyModalActive()) {
        return;
    }

    if (browserOpen_ && projectBrowserDrawerWidget_) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        bool handled = projectBrowserDrawerWidget_->handlePointer(pev);
        if (handled || projectBrowserDrawerWidget_->getDrawerBounds().contains(x, y)) {
            return;
        }
    }

    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularArrangerView_->handlePointer(pev, ctx);
    }
    if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularEditView_->handlePointer(pev, ctx);
    }
    if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularMixerView_->handlePointer(pev, ctx);
        mixerPropertiesExpanded_ = modularMixerView_->isPropertiesExpanded();
        mixerPropertiesWidth_ = modularMixerView_->getPropertiesWidth();
    }
    if ((activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) && modularDesignView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (modularDesignView_->handlePointer(pev, ctx) && dragMode_ == DragMode::None) {
            return;
        }
    }
    if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularTrackInspectorView_->handlePointer(pev, ctx);
    }

    if (button == 0 || button == 2) {
        stopChordAudition();
        if (previewingPitch_ != -1) {
            if (engine_) {
                engine_->postNoteOff(static_cast<uint8_t>(previewingPitch_), static_cast<int>(selectedTrackIndex_));
            }
            previewingPitch_ = -1;
        }

        if (virtualKeyboardActivePitch_ != -1) {
            if (engine_) {
                engine_->postNoteOff(static_cast<uint8_t>(virtualKeyboardActivePitch_), static_cast<int>(selectedTrackIndex_));
            }
            virtualKeyboardActivePitch_ = -1;
        }

        if (dragMode_ == DragMode::PianoRollMarquee) {
            if (std::abs(marqueeCurX_ - marqueeStartX_) < 4.0f && std::abs(marqueeCurY_ - marqueeStartY_) < 4.0f) {
                if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
                    auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
                    if (tr) tr->clearSelection();
                }
            }
            isMarqueeSelecting_ = false;
        } else if (dragMode_ == DragMode::PianoRollNoteMove || dragMode_ == DragMode::PianoRollNoteResize) {
            syncTrackToNoteScript();
            recordProjectHistory("Modify Notes on Track " + std::to_string(selectedTrackIndex_ + 1), "NOTE");
        }

        if (dragMode_ == DragMode::PatchCable && engine_) {
            // Drop cable: Check if mouse is over an input jack
            auto destJack = hitTestJack(x, y);
            if (destJack.hit && !destJack.isOutput) {
                engine_->getGraph().connect(dragCableSrcNode_, dragCableSrcPort_, destJack.nodeId, destJack.portIndex);
                engine_->getGraph().compile();
                canvas_.updateRackLayout(engine_->getGraph());
                initDefaultKnobValues();
            }
        }
        if (dragMode_ == DragMode::ArrangerPropertiesResize) {
            if (std::abs(x - dragStartX_) < 4.0f) {
                arrangerPropertiesExpanded_ = !arrangerPropertiesExpanded_;
            }
        }
        if (dragMode_ == DragMode::MixerPropertiesResize) {
            if (std::abs(x - dragStartX_) < 4.0f) {
                mixerPropertiesExpanded_ = !mixerPropertiesExpanded_;
            }
        }
    }

    if (activeDragHandler_ && activeDragHandler_->isDragging()) {
        PointerEvent pev{};
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.x = x;
        pev.y = y;
        pev.button = (button == GLFW_MOUSE_BUTTON_LEFT) ? PointerButton::Left :
                     ((button == GLFW_MOUSE_BUTTON_RIGHT) ? PointerButton::Right : PointerButton::Middle);
        pev.mods.shift = isShiftPressed();
        pev.mods.ctrl = isCtrlPressed();
        pev.mods.alt = isAltPressed();

        if (activeDragHandler_ == &cablePatchPresenter_ && engine_) {
            auto destJack = hitTestJack(x, y);
            if (destJack.hit && !destJack.isOutput) {
                cablePatchPresenter_.commitConnection(destJack.nodeId, destJack.portIndex, destJack.isOutput, engine_->getGraph());
                canvas_.updateRackLayout(engine_->getGraph());
                initDefaultKnobValues();
            } else {
                cablePatchPresenter_.cancelDrag();
            }
        } else {
            activeDragHandler_->onPointerUp(pev);
        }
        activeDragHandler_ = nullptr;
    }

    if (browserOpen_ && projectBrowserDrawerWidget_) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        projectBrowserDrawerWidget_->handlePointer(pev);
    }

    isMarqueeSelecting_ = false;
    dragMode_ = DragMode::None;
}

void GuiWindow::onMouseScroll(double xoffset, double yoffset) {
    PointerEvent pev;
    pev.type = PointerType::Mouse;
    pev.action = PointerAction::Scroll;
    pev.x = mouseX_;
    pev.y = mouseY_;
    pev.scrollX = static_cast<float>(xoffset);
    pev.scrollY = static_cast<float>(yoffset);
    pev.mods.shift = isShiftPressed();
    pev.mods.ctrl = isCtrlPressed();
    pev.mods.alt = isAltPressed();

    // 1. Modals & Dialogs (ensure context-based scroll and absorb scroll so it never leaks to DAW)
    if (fullscreenDeviceModal_.isOpen()) {
        fullscreenDeviceModal_.handlePointer(pev);
        markNeedsRedraw();
        return;
    }
    if (commandPaletteDialog_.isOpen()) {
        commandPaletteDialog_.handlePointer(pev);
        return;
    }
    if (valueEditDialog_.isOpen()) {
        if (modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
            modularArrangerView_->getIconSearchDialog().handleScroll(static_cast<float>(yoffset));
        }
        return;
    }
    if (audioToMidiDialog_.isOpen()) {
        return;
    }
    PluginSearchDialog* activePluginDialog = getActivePluginSearchDialog();
    if (activePluginDialog && activePluginDialog->isOpen()) {
        activePluginDialog->handlePointer(pev);
        return;
    }
    if (themeBrowserDialog_.isOpen()) {
        themeBrowserDialog_.handlePointer(pev);
        markNeedsRedraw();
        return;
    }

    ColorPickerDialog* activeColorPicker = getActiveColorPickerDialog();
    if (activeColorPicker && activeColorPicker->isOpen()) {
        activeColorPicker->handlePointer(pev);
        return;
    }

    // 2. Eatsbits Settings / Project Hub (context-based scroll via reusable ScrollableArea)
    if (projectHubOpen_) {
        DialogLayout dl = computeDialogLayout(540.0f, 620.0f);
        if (mouseX_ >= dl.x && mouseX_ <= dl.x + dl.w && mouseY_ >= dl.y && mouseY_ <= dl.y + dl.h) {
            if (projectHubSection_ == 3 && projectHubScrollArea_.canScroll()) {
                projectHubScrollArea_.scrollBy(-static_cast<float>(yoffset) * 32.0f);
                projectHubScrollY_ = projectHubScrollArea_.getScrollY();
            }
        }
        return; // Absorb all scroll events while Settings is open so it never scrolls the DAW underneath
    }

    if (modularArrangerView_ && modularArrangerView_->getCircleOfFifthsDialog().isOpen()) {
        modularArrangerView_->getCircleOfFifthsDialog().handlePointer(pev);
        return;
    }

    if (isAnyModalActive()) {
        return;
    }

    // 3. Sidebars & Drawers
    if (browserOpen_ && projectBrowserDrawerWidget_) {
        bool handled = projectBrowserDrawerWidget_->handlePointer(pev);
        if (handled || projectBrowserDrawerWidget_->getDrawerBounds().contains(mouseX_, mouseY_)) {
            return;
        }
    }
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        ViewContext ctx = createViewContext();
        if (modularArrangerView_->handlePointer(pev, ctx)) return;
    }
    if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
        ViewContext ctx = createViewContext();
        if (editSubView_ == EditSubView::Script || modularEditView_->getSubView() == EditSubViewMode::Script) {
            if (modularEditView_->handlePointer(pev, ctx)) return;
        } else {
            modularEditView_->handlePointer(pev, ctx);
        }
    }
    if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        ViewContext ctx = createViewContext();
        if (modularMixerView_->handlePointer(pev, ctx)) return;
    }
    if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        ViewContext ctx = createViewContext();
        if (modularTrackInspectorView_->handlePointer(pev, ctx)) return;
    }
    if ((activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) && modularDesignView_) {
        ViewContext ctx = createViewContext();
        if (modularDesignView_->handlePointer(pev, ctx)) return;
    }
    (void)xoffset;
    if (activeView_ == WorkspaceView::Track ||
        (activeView_ == WorkspaceView::Arranger && arrangerPropertiesExpanded_ && arrangerInspectorTab_ == ArrangerInspectorTab::Track) ||
        (activeView_ == WorkspaceView::Mixer && mixerPropertiesExpanded_)) {
        constexpr float maxScroll = 450.0f;
        trackInspectorScrollY_ = std::clamp(trackInspectorScrollY_ - static_cast<float>(yoffset) * 35.0f, 0.0f, maxScroll);
    } else if (activeView_ == WorkspaceView::Edit) {
        if (editSubView_ == EditSubView::Tracker) {
            trackerScrollY_ = std::max(0.0f, trackerScrollY_ - static_cast<float>(yoffset) * 20.0f);
        } else if (editSubView_ == EditSubView::Score) {
            scoreScrollX_ = std::max(0.0f, scoreScrollX_ - static_cast<float>(yoffset) * 25.0f);
        } else if (editSubView_ == EditSubView::PianoRoll) {
            bool isCtrl = false;
            bool isShift = false;
#ifndef __EMSCRIPTEN__
            if (window_) {
                isCtrl = (glfwGetKey(window_, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
                isShift = (glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
            }
#endif
            if (isCtrl && isShift) {
                setPianoRollRowHeight(pianoRollRowH_ + static_cast<float>(yoffset) * 2.0f);
            } else if (isCtrl) {
                setPianoRollStepWidth(pianoRollStepW_ + static_cast<float>(yoffset) * 4.0f);
            } else if (isShift) {
                setPianoRollScrollX(pianoRollScrollX_ - static_cast<float>(yoffset) * 40.0f);
            } else {
                setPianoRollScrollY(pianoRollScrollY_ - static_cast<float>(yoffset) * 30.0f);
            }
        }
    }
}

void GuiWindow::onFilesDropped(const std::vector<std::string>& filePaths, float x, float y) {
    if (filePaths.empty()) return;

    if (onExternalFilesDropped) {
        onExternalFilesDropped(filePaths, x, y);
    }

    // 1. Audio-to-MIDI Modal Dialog routing
    if (audioToMidiDialog_.isOpen()) {
        for (const auto& path : filePaths) {
            std::filesystem::path fp(path);
            std::string ext = fp.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".aif" || ext == ".aiff") {
                if (audioToMidiDialog_.loadAudioFile(path)) {
                    setStatusMessage("Loaded audio into Audio-to-MIDI Converter: " + fp.filename().string());
                } else {
                    setStatusMessage("Audio-to-MIDI: " + audioToMidiDialog_.getStatusMessage());
                }
                return;
            }
        }
    }

    // 2. Delegate to active workspace view (e.g. ArrangerView timeline)
    ViewContext ctx = createViewContext();
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (modularArrangerView_->handleFileDrop(filePaths, x, y, ctx)) {
            return;
        }
    }

    // 3. Global Fallback Dispatch
    for (const auto& path : filePaths) {
        std::filesystem::path fp(path);
        std::string ext = fp.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext == ".eats" || ext == ".json") {
            if (loadProjectFromFile(path)) {
                setStatusMessage("Loaded project: " + fp.filename().string());
                return;
            }
        } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".aif" || ext == ".aiff" || ext == ".sf2") {
            if (modularArrangerView_) {
                if (modularArrangerView_->handleFileDrop({path}, x, y, ctx)) {
                    setStatusMessage("Imported sample into Arranger: " + fp.filename().string());
                    return;
                }
            }
        } else if (ext == ".mid" || ext == ".midi") {
            if (modularArrangerView_) {
                if (modularArrangerView_->handleFileDrop({path}, x, y, ctx)) {
                    setStatusMessage("Imported MIDI into Arranger: " + fp.filename().string());
                    return;
                }
            }
        } else if (ext == ".eatscript") {
            std::ifstream file(path);
            if (file.is_open()) {
                std::stringstream ss;
                ss << file.rdbuf();
                setActiveView(WorkspaceView::Design);
                setStatusMessage("Loaded Eatscript: " + fp.filename().string());
                return;
            }
        }
    }
}

void GuiWindow::onKeyDown(int key, int mods) {
    bool isCtrl = (mods & 2) != 0;
    bool isShift = (mods & 1) != 0;
    bool isAlt = (mods & 4) != 0;

    // F11 or Alt+Enter: Native Desktop Window Fullscreen ALWAYS works globally across the DAW
    if (key == 300 || (isAlt && (key == 257 || key == 335))) {
        toggleFullscreen();
        return;
    }

    // Intercept keyboard input if an element has active keyboard focus
    if (focusManager_.isAnyFocused()) {
        if (focusManager_.dispatchKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            markNeedsRedraw();
            return;
        }
    }

    PluginSearchDialog* activePluginDialog = getActivePluginSearchDialog();
    ColorPickerDialog* activeColorPicker = getActiveColorPickerDialog();
    bool modalOrTerminalActive = projectHubOpen_ ||
                                 (activeColorPicker && activeColorPicker->isOpen()) ||
                                 valueEditDialog_.isOpen() ||
                                 commandPaletteDialog_.isOpen() ||
                                 audioToMidiDialog_.isOpen() ||
                                 themeBrowserDialog_.isOpen() ||
                                 (activePluginDialog && activePluginDialog->isOpen());

    // Spacebar: Global DAW playback toggle
    if (key == 32 && !isCtrl && !isAlt && !modalOrTerminalActive) {
        if (engine_) {
            if (engine_->getSequencer().isPlaying()) {
                engine_->stopPlayback();
            } else {
                syncArrangerToSequencer();
                engine_->getSequencer().start();
            }
        }
        return;
    }

    if (activeColorPicker && activeColorPicker->isOpen()) {
        if (key == 256) { // Escape closes color picker
            activeColorPicker->close();
            markNeedsRedraw();
            return;
        }
        if (activeColorPicker->handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            markNeedsRedraw();
            return;
        }
        return;
    }

    if (projectHubOpen_) {
        if (key == 256) { // Escape closes project hub
            projectHubOpen_ = false;
            isEditingTitle_ = false;
            isEditingAuthor_ = false;
            markNeedsRedraw();
            return;
        }
        if (isEditingTitle_) {
            if (key == 259 && !projectName_.empty()) {
                projectName_.pop_back();
                markNeedsRedraw();
                return;
            }
            if (key == 257 || key == 335) {
                isEditingTitle_ = false;
                markNeedsRedraw();
                return;
            }
        }
        if (isEditingAuthor_) {
            if (key == 259 && !authorName_.empty()) {
                authorName_.pop_back();
                markNeedsRedraw();
                return;
            }
            if (key == 257 || key == 335) {
                isEditingAuthor_ = false;
                markNeedsRedraw();
                return;
            }
        }
        return; // Absorb all keyboard input while Settings dialog is active
    }

    // Intercept keyboard input if Command Palette is open
    if (commandPaletteDialog_.isOpen()) {
        if (commandPaletteDialog_.handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            return;
        }
    }

    // Intercept keyboard input if Stacked Icon Search Modal Dialog is open
    if (valueEditDialog_.isOpen() && modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
        if (modularArrangerView_->getIconSearchDialog().handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            return;
        }
    }

    // Intercept keyboard input if Reusable Value Edit Modal Dialog is open
    if (valueEditDialog_.isOpen()) {
        if (valueEditDialog_.handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            return;
        }
    }

    // Intercept keyboard input if Audio-to-MIDI Modal Dialog is open
    if (audioToMidiDialog_.isOpen()) {
        if (audioToMidiDialog_.handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            return;
        }
    }

    // Intercept keyboard input if Plugin / Preset Search Modal Dialog is open
    if (activePluginDialog && activePluginDialog->isOpen()) {
        // If printable key and not Ctrl/Alt, let onChar handle text insertion
        if (key >= 32 && key <= 126 && !isCtrl && !isAlt) {
            return; // Absorbed, onChar will receive the character
        }
        if (activePluginDialog->handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            markNeedsRedraw();
            return;
        }
        return; // Absorb all keys while modal is open
    }

    // Intercept keyboard input if Theme Browser Modal Dialog is open
    if (themeBrowserDialog_.isOpen()) {
        if (key >= 32 && key <= 126 && !isCtrl && !isAlt) {
            return; // Absorbed, onChar will receive the character
        }
        if (themeBrowserDialog_.handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            markNeedsRedraw();
            return;
        }
        return; // Absorb all keys while modal is open
    }

    // Intercept keyboard input if Dedicated Full-Display Device GUI is open
    if (fullscreenDeviceModal_.isOpen()) {
        if (fullscreenDeviceModal_.handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            markNeedsRedraw();
            return;
        }
        if (key == 256) { // GLFW_KEY_ESCAPE
            closeFullscreenDevice();
            return;
        }
        if (isShift && !isCtrl && !isAlt && (key == 70 || key == 102)) { // Shift+F closes
            closeFullscreenDevice();
            return;
        }
        // Absorb all other non-global keys while modal is open
        return;
    }

    // Ctrl+M: Open Audio to MIDI Converter Modal Dialog
    if (isCtrl && (key == 77 || key == 109)) { // 'M'
        openAudioToMidiConverter();
        return;
    }

    // 'M' in Arranger View: Toggle Docked Bottom Mixer Drawer
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (!isCtrl && !isAlt && (key == 77 || key == 109)) {
            modularArrangerView_->toggleMixerDrawer();
            markNeedsRedraw();
            return;
        }
    }

    // Ctrl+P or Ctrl+K: Toggle Universal Quick Command Palette
    if (isCtrl && (key == 80 || key == 112 || key == 75 || key == 107)) { // 'P' or 'K'
        toggleCommandPalette();
        return;
    }

    // Shift+P or F3: Toggle Preset Librarian & Script Search Dialog
    if (((mods & 1) != 0 && (key == 80 || key == 112)) || key == 292) { // Shift+P or F3
        if (!isPresetDialogOpen()) {
            openPresetDialog(PluginDialogMode::SelectPreset, selectedTrackIndex_);
        } else {
            closePresetDialog();
        }
        return;
    }

    if (!engine_) return;

    // Piano Roll & Editor Note Selection Keyboard Shortcuts
    auto* activeTrk = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks())
                          ? engine_->getSequencer().getTrack(selectedTrackIndex_)
                          : nullptr;
    if (activeView_ == WorkspaceView::Edit && editSubView_ == EditSubView::PianoRoll && activeTrk && activeTrk->hasSelectedNotes()) {
        if (key == 261 || key == 259) { // GLFW_KEY_DELETE or GLFW_KEY_BACKSPACE
            activeTrk->deleteSelectedNotes();
            recordProjectHistory("Delete Selected Notes on Track " + std::to_string(selectedTrackIndex_ + 1), "NOTE");
            syncTrackToNoteScript();
            return;
        }
        if (key == 256) { // GLFW_KEY_ESCAPE
            activeTrk->clearSelection();
            return;
        }
        if (isCtrl && (key == 65 || key == 97)) { // Ctrl+A
            activeTrk->selectAllNotes();
            return;
        }
        if (key == 265) { // GLFW_KEY_UP
            activeTrk->transposeSelectedNotes(isShift ? 12 : 1);
            recordProjectHistory("Transpose Up", "NOTE");
            syncTrackToNoteScript();
            return;
        }
        if (key == 264) { // GLFW_KEY_DOWN
            activeTrk->transposeSelectedNotes(isShift ? -12 : -1);
            recordProjectHistory("Transpose Down", "NOTE");
            syncTrackToNoteScript();
            return;
        }
        if (key == 263) { // GLFW_KEY_LEFT
            activeTrk->nudgeSelectedNotes(-1);
            recordProjectHistory("Nudge Left", "NOTE");
            syncTrackToNoteScript();
            return;
        }
        if (key == 262) { // GLFW_KEY_RIGHT
            activeTrk->nudgeSelectedNotes(1);
            recordProjectHistory("Nudge Right", "NOTE");
            syncTrackToNoteScript();
            return;
        }
    } else if (activeTrk && isCtrl && (key == 65 || key == 97) &&
               (activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker)) {
        activeTrk->selectAllNotes();
        return;
    }

    // Alt+F or F1: Toggle Project Hub & Settings Menu
    if ((isAlt && (key == 70 || key == 102)) || key == 290) { // 'F' or F1
        toggleProjectHub();
        return;
    }

    // Shift+F: Toggle Dedicated Full-Display Device/FX GUI
    if (isShift && !isCtrl && !isAlt && (key == 70 || key == 102)) { // 'F'
        toggleFullscreenDevice();
        return;
    }

    // F11 or Alt+Enter: Toggle Fullscreen (matching Eatsbeats)
    if (key == 300 || (isAlt && (key == 257 || key == 335))) {
        toggleFullscreen();
        return;
    }

    // F9: Toggle Apocalypse CRT Shader
    if (key == 298) { // F9
        toggleCrtShader();
        setStatusMessage(crtShaderEnabled_ ? "CRT Shader: ENABLED (Swaying Spotlight + Scanlines + Rumble)" : "CRT Shader: BYPASSED (Raw UI)");
        return;
    }

    // Ctrl+S / Ctrl+Shift+S: Save / Save As
    if (isCtrl && (key == 83 || key == 115)) {
        if (isShift) {
            saveProjectAs();
        } else {
            saveProjectToFile(projectFilePath_);
        }
        return;
    }

    // Ctrl+O: Open / Load Project
    if (isCtrl && (key == 79 || key == 111)) {
        loadProjectPrompt();
        return;
    }

    // Ctrl+N: New Project
    if (isCtrl && (key == 78 || key == 110)) {
        resetToDefaultProject();
        return;
    }

    // Ctrl+E: Export Master WAV
    if (isCtrl && (key == 69 || key == 101)) {
        bounceMasterToWav(projectName_ + ".wav");
        return;
    }

    // Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y: Pure Diff Undo / Redo
    if (isCtrl && (key == 90 || key == 122)) { // 'Z'
        if (isShift) {
            redoHistory();
        } else {
            undoHistory();
        }
        return;
    }
    if (isCtrl && (key == 89 || key == 121)) { // 'Y'
        redoHistory();
        return;
    }

    // Escape (256): Close modal/drawer if open
    if (key == 256) {
        if (projectHubOpen_) {
            projectHubOpen_ = false;
            isEditingTitle_ = false;
            isEditingAuthor_ = false;
            return;
        }
        if (browserOpen_) {
            browserOpen_ = false;
            return;
        }
    }

    // If editing Title or Author in Project Hub:
    if (projectHubOpen_ && (isEditingTitle_ || isEditingAuthor_)) {
        std::string& targetStr = isEditingTitle_ ? projectName_ : authorName_;
        if (key == 257 || key == 256) { // Enter or Esc
            isEditingTitle_ = false;
            isEditingAuthor_ = false;
            return;
        }
        if (key == 259) { // Backspace
            if (!targetStr.empty()) targetStr.pop_back();
            return;
        }
        if (key == 32) { // Space
            targetStr += ' ';
            return;
        }
        if (key >= 32 && key <= 126) {
            char c = static_cast<char>(key);
            if (!isShift && c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
            targetStr += c;
            return;
        }
        return;
    }

    // Global Zoom Hotkeys (Ctrl + '=', Ctrl + '-', Ctrl + '0')
    if (isCtrl) {
        if (key == 61 || key == 334) { // '=' / '+'
            zoomIn();
            return;
        }
        if (key == 45 || key == 333) { // '-'
            zoomOut();
            return;
        }
        if (key == 48 || key == 320) { // '0'
            resetZoom();
            return;
        }
    }

    // Arranger View Hotkeys & Plugin Search Dialog Text Input
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        ViewContext ctx = createViewContext();
        if (modularArrangerView_->handleKey(key, 0, 1, mods, ctx)) return;
    }
    // Edit View Hotkeys (Piano Roll, Tracker, Score, Script)
    if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
        if (editSubView_ != EditSubView::Tracker) {
            ViewContext ctx = createViewContext();
            if (modularEditView_->handleKey(key, 0, 1, mods, ctx)) return;
        }
    }
    // Track Inspector View Hotkeys & Plugin Search Dialog Input
    if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        ViewContext ctx = createViewContext();
        if (modularTrackInspectorView_->handleKey(key, 0, 1, mods, ctx)) return;
    }

    // Note Script Editor Keyboard Navigation & Text Editing (EDIT > SCRIPT)
    if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) &&
        editSubView_ == EditSubView::Script && !browserOpen_) {
        // Apply/Sync to track: Ctrl+Enter (257) or F5 (294)
        if ((key == 257 && isCtrl) || key == 294) {
            syncNoteScriptToTrack();
            return;
        }

        // F1..F5: View navigation shortcuts
        if (key == 290) { setActiveView(WorkspaceView::Arranger); return; }
        if (key == 291) { setActiveView(WorkspaceView::Edit); return; }
        if (key == 292) { setActiveView(WorkspaceView::Track); return; }
        if (key == 293) { setActiveView(WorkspaceView::Mixer); return; }
        if (key == 294) { setActiveView(WorkspaceView::Design); return; }

        // Escape (256): Panic stop
        if (key == 256) {
            engine_->panic();
            return;
        }

        // Navigation
        if (key == 265) { setNoteScriptCursor(noteScriptCursorLine_ - 1, noteScriptCursorCol_); return; }
        if (key == 264) { setNoteScriptCursor(noteScriptCursorLine_ + 1, noteScriptCursorCol_); return; }
        if (key == 263) { setNoteScriptCursor(noteScriptCursorLine_, noteScriptCursorCol_ - 1); return; }
        if (key == 262) { setNoteScriptCursor(noteScriptCursorLine_, noteScriptCursorCol_ + 1); return; }
        if (key == 268) { setNoteScriptCursor(noteScriptCursorLine_, 0); return; }
        if (key == 269) {
            int len = (noteScriptCursorLine_ < static_cast<int>(noteScriptBuffer_.size())) ? static_cast<int>(noteScriptBuffer_[noteScriptCursorLine_].size()) : 0;
            setNoteScriptCursor(noteScriptCursorLine_, len);
            return;
        }
        if (key == 266) { setNoteScriptCursor(noteScriptCursorLine_ - 5, noteScriptCursorCol_); return; }
        if (key == 267) { setNoteScriptCursor(noteScriptCursorLine_ + 5, noteScriptCursorCol_); return; }

        // Editing
        if (key == 259) { deleteNoteScriptCharBackwards(); return; }
        if (key == 261) { deleteNoteScriptCharForwards(); return; }
        if (key == 257) { insertNoteScriptNewLine(); return; }
        if (key == 258) { insertNoteScriptText("    "); return; }
        if (key == 32)  { insertNoteScriptChar(' '); return; }

        // Character typing with shift awareness
        if (isShift) {
            if (key >= 65 && key <= 90) { insertNoteScriptChar(static_cast<char>(key)); return; }
            if (key == '3') { insertNoteScriptChar('#'); return; }
            if (key == '9') { insertNoteScriptChar('('); return; }
            if (key == '0') { insertNoteScriptChar(')'); return; }
            if (key == 39)  { insertNoteScriptChar('"'); return; }
            if (key == 91)  { insertNoteScriptChar('{'); return; }
            if (key == 93)  { insertNoteScriptChar('}'); return; }
            if (key == 59)  { insertNoteScriptChar(':'); return; }
            if (key == 61)  { insertNoteScriptChar('+'); return; }
            if (key == 45)  { insertNoteScriptChar('_'); return; }
        } else {
            if (key >= 65 && key <= 90) { insertNoteScriptChar(static_cast<char>(std::tolower(key))); return; }
            if (key == 39) { insertNoteScriptChar('\''); return; }
            if (key == 91) { insertNoteScriptChar('['); return; }
            if (key == 93) { insertNoteScriptChar(']'); return; }
            if (key == 59) { insertNoteScriptChar(';'); return; }
        }

        // Numbers 0..9
        if (key >= 48 && key <= 57) {
            insertNoteScriptChar(static_cast<char>(key));
            return;
        }

        // Direct punctuation
        if (key == 44) { insertNoteScriptChar(','); return; }
        if (key == 46) { insertNoteScriptChar('.'); return; }
        if (key == 45) { insertNoteScriptChar('-'); return; }
        if (key == 61) { insertNoteScriptChar('='); return; }
        if (key == 47) { insertNoteScriptChar('/'); return; }

        return;
    }

    // Eatscript IDE Keyboard Navigation & Text Editing
    if ((activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) &&
        designSubView_ == DesignSubView::Eatscript && !browserOpen_) {
        // F5 (294): Compile Script
        if (key == 294) {
            compileActiveScript();
            return;
        }
        // F1..F4: View navigation shortcuts
        if (key == 290 || key == 49) { // F1 or '1'
            setActiveView(WorkspaceView::Arranger);
            return;
        } else if (key == 291 || key == 50) { // F2 or '2'
            setActiveView(WorkspaceView::Edit);
            return;
        } else if (key == 292 || key == 51) { // F3 or '3'
            setActiveView(WorkspaceView::Track);
            return;
        } else if (key == 293 || key == 52) { // F4 or '4'
            setActiveView(WorkspaceView::Mixer);
            return;
        }
        // Escape (256): Panic stop
        if (key == 256) {
            engine_->panic();
            return;
        }
        // Up Arrow (265)
        if (key == 265) {
            setScriptCursor(scriptCursorLine_ - 1, scriptCursorCol_);
            return;
        }
        // Down Arrow (264)
        if (key == 264) {
            setScriptCursor(scriptCursorLine_ + 1, scriptCursorCol_);
            return;
        }
        // Left Arrow (263)
        if (key == 263) {
            setScriptCursor(scriptCursorLine_, scriptCursorCol_ - 1);
            return;
        }
        // Right Arrow (262)
        if (key == 262) {
            setScriptCursor(scriptCursorLine_, scriptCursorCol_ + 1);
            return;
        }
        // Home (268)
        if (key == 268) {
            setScriptCursor(scriptCursorLine_, 0);
            return;
        }
        // End (269)
        if (key == 269) {
            int len = (scriptCursorLine_ < static_cast<int>(scriptBuffer_.size())) ? static_cast<int>(scriptBuffer_[scriptCursorLine_].size()) : 0;
            setScriptCursor(scriptCursorLine_, len);
            return;
        }
        // Page Up (266)
        if (key == 266) {
            setScriptCursor(scriptCursorLine_ - 5, scriptCursorCol_);
            return;
        }
        // Page Down (267)
        if (key == 267) {
            setScriptCursor(scriptCursorLine_ + 5, scriptCursorCol_);
            return;
        }
        // Backspace (259)
        if (key == 259) {
            deleteScriptCharBackwards();
            return;
        }
        // Delete (261)
        if (key == 261) {
            deleteScriptCharForwards();
            return;
        }
        // Enter / Return (257)
        if (key == 257) {
            insertScriptNewLine();
            return;
        }
        // Tab (258): Cycle active views when editor is not focused
        if (key == 258) {
            int next = (static_cast<int>(activeView_) + 1) % 5;
            setActiveView(static_cast<WorkspaceView>(next));
            return;
        }
        // Space (32)
        if (key == 32) {
            insertScriptChar(' ');
            return;
        }
        // Characters: Letters (A-Z -> a-z)
        if (key >= 65 && key <= 90) {
            insertScriptChar(static_cast<char>(std::tolower(key)));
            return;
        }
        // Numbers (0-9)
        if (key >= 48 && key <= 57) {
            insertScriptChar(static_cast<char>(key));
            return;
        }
        // Punctuation & Operators
        if (key == 44) { insertScriptChar(','); return; }
        if (key == 46) { insertScriptChar('.'); return; }
        if (key == 45) { insertScriptChar('-'); return; }
        if (key == 61) { insertScriptChar('='); return; }
        if (key == 47) { insertScriptChar('/'); return; }
        if (key == 59) { insertScriptChar(':'); return; }
        if (key == 39) { insertScriptChar('\''); return; }
        if (key == 91) { insertScriptChar('['); return; }
        if (key == 93) { insertScriptChar(']'); return; }
        if (key == 92) { insertScriptChar('\\'); return; }
        return;
    }

    // Spacebar (32): Toggle Sequencer Play / Stop
    if (key == 32) {
        if (engine_->getSequencer().isPlaying()) {
            engine_->stopPlayback();
        } else {
            syncArrangerToSequencer();
            engine_->getSequencer().start();
        }
    }
    // 'B' (66): Toggle Preset Browser / Patch Librarian Drawer
    else if (key == 66) {
        toggleBrowser();
    }
    // 'P' (80): Toggle Preset Dialog
    else if (key == 80) {
        if (!isPresetDialogOpen()) {
            openPresetDialog(PluginDialogMode::SelectPreset, selectedTrackIndex_);
        } else {
            closePresetDialog();
        }
    }
    // Escape (256): Close Preset Dialog or Browser if open, otherwise Panic Stop
    else if (key == 256) {
        if (presetSearchDialog_.isOpen()) {
            presetSearchDialog_.close();
        } else if (browserOpen_) {
            browserOpen_ = false;
        } else {
            engine_->panic();
        }
    }
    // Tracker View Navigation & QWERTY Musical Note Entry
    else if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) &&
             editSubView_ == EditSubView::Tracker && !browserOpen_) {
        auto& seq = engine_->getSequencer();
        const size_t numTracks = seq.getNumTracks();

        // Up Arrow (265): Move cursor up 1 row
        if (key == 265) {
            trackerSelectedRow_ = (trackerSelectedRow_ == 0) ? 63 : trackerSelectedRow_ - 1;
        }
        // Down Arrow (264): Move cursor down 1 row
        else if (key == 264) {
            trackerSelectedRow_ = (trackerSelectedRow_ + 1) % 64;
        }
        // Left Arrow (263): Move cursor left 1 track
        else if (key == 263) {
            if (trackerSelectedTrack_ > 0) {
                trackerSelectedTrack_--;
                selectedTrackIndex_ = trackerSelectedTrack_;
            }
        }
        // Right Arrow (262): Move cursor right 1 track
        else if (key == 262) {
            if (numTracks > 0 && trackerSelectedTrack_ + 1 < numTracks) {
                trackerSelectedTrack_++;
                selectedTrackIndex_ = trackerSelectedTrack_;
            }
        }
        // Page Up (266): Jump 16 rows up (1 bar)
        else if (key == 266) {
            trackerSelectedRow_ = (trackerSelectedRow_ < 16) ? 0 : trackerSelectedRow_ - 16;
        }
        // Page Down (267): Jump 16 rows down (1 bar)
        else if (key == 267) {
            trackerSelectedRow_ = std::min(63u, trackerSelectedRow_ + 16);
        }
        // Home (268): Jump to row 0
        else if (key == 268) {
            trackerSelectedRow_ = 0;
        }
        // End (269): Jump to row 63
        else if (key == 269) {
            trackerSelectedRow_ = 63;
        }
        // Delete (261) or Backspace (259): Clear step note
        else if (key == 261 || key == 259) {
            if (numTracks > 0 && trackerSelectedTrack_ < numTracks) {
                auto* tr = seq.getTrack(trackerSelectedTrack_);
                if (tr) {
                    auto st = tr->getStep(trackerSelectedRow_);
                    st.active = false;
                    tr->setStep(trackerSelectedRow_, st);
                }
            }
            trackerSelectedRow_ = (trackerSelectedRow_ + 1) % 64;
        }
        // QWERTY Musical Note Entry
        else {
            int pitch = qwertyKeyToMidiPitch(key, 4);
            if (pitch >= 0) {
                if (numTracks > 0 && trackerSelectedTrack_ < numTracks) {
                    auto* tr = seq.getTrack(trackerSelectedTrack_);
                    if (tr) {
                        auto st = tr->getStep(trackerSelectedRow_);
                        st.active = true;
                        st.note = static_cast<uint8_t>(pitch);
                        st.velocity = 0.85f;
                        st.gateLength = 0.75f;
                        tr->setStep(trackerSelectedRow_, st);
                        engine_->postNoteOn(static_cast<uint8_t>(pitch), 0.85f, false, false, static_cast<int>(trackerSelectedTrack_));
                        previewingPitch_ = pitch;
                    }
                }
                trackerSelectedRow_ = (trackerSelectedRow_ + 1) % 64;
            }
        }
    }
    // 'R' (82): Panic Stop (outside tracker mode)
    else if (key == 82) {
        engine_->panic();
    }
    // Tab / Function Keys for rapid view switching
    else if (key == 290 || key == 49) { // F1 or '1'
        setActiveView(WorkspaceView::Arranger);
    } else if (key == 291 || key == 50) { // F2 or '2'
        setActiveView(WorkspaceView::Edit);
    } else if (key == 292 || key == 51) { // F3 or '3'
        setActiveView(WorkspaceView::Track);
    } else if (key == 293 || key == 52) { // F4 or '4'
        setActiveView(WorkspaceView::Mixer);
    } else if (key == 294 || key == 53) { // F5 or '5'
        setActiveView(WorkspaceView::Design);
    } else if (key == 258) { // Tab key cycles views
        int next = (static_cast<int>(activeView_) + 1) % 5;
        setActiveView(static_cast<WorkspaceView>(next));
    }
}

void GuiWindow::openCommandPalette() noexcept {
    commandPaletteDialog_.open();
}

void GuiWindow::closeCommandPalette() noexcept {
    commandPaletteDialog_.close();
}

void GuiWindow::toggleCommandPalette() noexcept {
    commandPaletteDialog_.toggle();
}

bool GuiWindow::isCommandPaletteOpen() const noexcept {
    return commandPaletteDialog_.isOpen();
}

void GuiWindow::onChar(unsigned int codepoint) {
    ColorPickerDialog* activeColorPicker = getActiveColorPickerDialog();
    if (activeColorPicker && activeColorPicker->isOpen()) {
        return;
    }
    if (projectHubOpen_) {
        if (isEditingTitle_ && codepoint >= 32) {
            projectName_ += static_cast<char>(codepoint);
            markNeedsRedraw();
            return;
        }
        if (isEditingAuthor_ && codepoint >= 32) {
            authorName_ += static_cast<char>(codepoint);
            markNeedsRedraw();
            return;
        }
        return;
    }
    if (valueEditDialog_.isOpen()) {
        return;
    }
    if (focusManager_.isAnyFocused()) {
        if (focusManager_.dispatchChar(static_cast<char32_t>(codepoint))) {
            markNeedsRedraw();
            return;
        }
    }
    PluginSearchDialog* activePluginDialog = getActivePluginSearchDialog();
    if (activePluginDialog && activePluginDialog->isOpen()) {
        if (activePluginDialog->handleChar(static_cast<char32_t>(codepoint))) {
            markNeedsRedraw();
            return;
        }
        return; // Absorb characters behind modal
    }
    if (themeBrowserDialog_.isOpen()) {
        if (themeBrowserDialog_.handleChar(static_cast<char32_t>(codepoint))) {
            markNeedsRedraw();
            return;
        }
        return;
    }
    if (commandPaletteDialog_.isOpen()) {
        commandPaletteDialog_.handleChar(codepoint);
        return;
    }
    if (audioToMidiDialog_.isOpen()) {
        audioToMidiDialog_.handleChar(codepoint);
        return;
    }
    if (modularArrangerView_ && modularArrangerView_->getCircleOfFifthsDialog().isOpen()) {
        return;
    }

    if (isAnyModalActive()) {
        return;
    }

    // Active View text input fallback
    ViewContext ctx = createViewContext();
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (modularArrangerView_->handleChar(static_cast<char32_t>(codepoint), ctx)) {
            markNeedsRedraw();
            return;
        }
    } else if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        if (modularMixerView_->handleChar(static_cast<char32_t>(codepoint), ctx)) {
            markNeedsRedraw();
            return;
        }
    } else if (activeView_ == WorkspaceView::Edit && modularEditView_) {
        if (modularEditView_->handleChar(static_cast<char32_t>(codepoint), ctx)) {
            markNeedsRedraw();
            return;
        }
    } else if (activeView_ == WorkspaceView::Design && modularDesignView_) {
        if (modularDesignView_->handleChar(static_cast<char32_t>(codepoint), ctx)) {
            markNeedsRedraw();
            return;
        }
    } else if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        if (modularTrackInspectorView_->handleChar(static_cast<char32_t>(codepoint), ctx)) {
            markNeedsRedraw();
            return;
        }
    }
}

void GuiWindow::openAudioToMidiConverter(const std::optional<audio::DecodedAudioBuffer>& initialBuffer,
                                        const std::string& name) {
    audioToMidiDialog_.open(initialBuffer, name);
}

void GuiWindow::closeAudioToMidiConverter() noexcept {
    audioToMidiDialog_.close();
}

bool GuiWindow::isAudioToMidiDialogOpen() const noexcept {
    return audioToMidiDialog_.isOpen();
}

void GuiWindow::openPresetDialog(PluginDialogMode mode, uint32_t trackIndex) {
    presetSearchDialog_.layout(static_cast<float>(width_), static_cast<float>(height_));
    std::string trkName = "";
    if (trackIndex < mixerStrips_.size()) {
        trkName = mixerStrips_[trackIndex].name;
    } else if (trackIndex < arrangerTracks_.size()) {
        trkName = arrangerTracks_[trackIndex].name;
    }
    presetSearchDialog_.open(mode, trkName, trackIndex);
}

void GuiWindow::closePresetDialog() noexcept {
    presetSearchDialog_.close();
}

bool GuiWindow::isPresetDialogOpen() const noexcept {
    return presetSearchDialog_.isOpen();
}

PluginSearchDialog* GuiWindow::getActivePluginSearchDialog() noexcept {
    if (presetSearchDialog_.isOpen()) {
        return &presetSearchDialog_;
    }
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (modularArrangerView_->getPluginSearchDialog().isOpen()) {
            return &modularArrangerView_->getPluginSearchDialog();
        }
        if (modularArrangerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            return &modularArrangerView_->getPropertiesDrawer().getPluginSearchDialog();
        }
    } else if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        if (modularMixerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            return &modularMixerView_->getPropertiesDrawer().getPluginSearchDialog();
        }
    } else if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        if (modularTrackInspectorView_->getPluginSearchDialog().isOpen()) {
            return &modularTrackInspectorView_->getPluginSearchDialog();
        }
    } else if (activeView_ == WorkspaceView::Design && modularDesignView_) {
        if (modularDesignView_->getModuleSearchDialog().isOpen()) {
            return &modularDesignView_->getModuleSearchDialog();
        }
    }
    return nullptr;
}

ColorPickerDialog* GuiWindow::getActiveColorPickerDialog() noexcept {
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (modularArrangerView_->getPropertiesDrawer().isColorPickerOpen()) {
            return &modularArrangerView_->getPropertiesDrawer().getColorPickerDialog();
        }
    } else if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        if (modularMixerView_->getPropertiesDrawer().isColorPickerOpen()) {
            return &modularMixerView_->getPropertiesDrawer().getColorPickerDialog();
        }
    } else if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        if (modularTrackInspectorView_->getPanel().isColorPickerOpen()) {
            return &modularTrackInspectorView_->getPanel().getColorPickerDialog();
        }
    }
    return nullptr;
}

bool GuiWindow::isAnyModalActive() const noexcept {
    if (projectHubOpen_) return true;
    if (commandPaletteDialog_.isOpen()) return true;
    if (valueEditDialog_.isOpen()) return true;
    if (modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) return true;
    if (modularArrangerView_ && modularArrangerView_->getCircleOfFifthsDialog().isOpen()) return true;
    if (audioToMidiDialog_.isOpen()) return true;
    if (fullscreenDeviceModal_.isOpen()) return true;
    if (themeBrowserDialog_.isOpen()) return true;
    const PluginSearchDialog* activePlugin = const_cast<GuiWindow*>(this)->getActivePluginSearchDialog();
    if (activePlugin && activePlugin->isOpen()) return true;
    const ColorPickerDialog* activeColor = const_cast<GuiWindow*>(this)->getActiveColorPickerDialog();
    if (activeColor && activeColor->isOpen()) return true;
    return false;
}

std::string GuiWindow::getSettingsFilePath() const {
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    if (appData && *appData) {
        std::filesystem::path p(appData);
        p /= "Eatsbits";
        p /= "settings.json";
        return p.string();
    }
#elif !defined(__EMSCRIPTEN__)
    const char* home = std::getenv("HOME");
    if (home && *home) {
        std::filesystem::path p(home);
        p /= ".config";
        p /= "eatsbits";
        p /= "settings.json";
        return p.string();
    }
#endif
    return "settings.json";
}

bool GuiWindow::saveSettings() {
    using namespace eatsbits::json;
    Object root;
    root["uiScale"] = Value(uiScale_);
    root["activeThemePreset"] = Value(activeThemePreset_);
    root["themeBank"] = Value(getThemeBank());
    root["antiAliasingMode"] = Value(antiAliasingMode_);
    root["hiDpiEnabled"] = Value(hiDpiEnabled_);
    root["crtShaderEnabled"] = Value(crtShaderEnabled_);
    root["guiAnimationsEnabled"] = Value(guiAnimationsEnabled_);
    root["autoRestoreSession"] = Value(autoRestoreSession_);
    root["autoSaveEnabled"] = Value(autoSaveEnabled_);
    root["includeBinaryClapPath"] = Value(audio::ClapPluginLoader::instance().isIncludeBinaryPathEnabled());
    root["minimalGuiEnabled"] = Value(isMinimalGuiEnabled());

    const auto& crt = dawnBridge_.getMaterialConfig();
    Object crtObj;
    crtObj["scanlineIntensity"] = Value(crt.scanlineIntensity);
    crtObj["curvature"] = Value(crt.curvature);
    crtObj["crtReflectionLevel"] = Value(crt.crtReflectionLevel);
    crtObj["hsyncDistortion"] = Value(crt.hsyncDistortion);
    crtObj["spotlightIntensity"] = Value(crt.spotlightIntensity);
    crtObj["spotlightSize"] = Value(crt.spotlightSize);
    crtObj["vignetteStrength"] = Value(crt.vignetteStrength);
    crtObj["reflectionOpacity"] = Value(crt.reflectionOpacity);
    crtObj["panelSoftness"] = Value(crt.panelSoftness);
    crtObj["panelSaturation"] = Value(crt.panelSaturation);
    crtObj["panelBlackLift"] = Value(crt.panelBlackLift);
    root["crt"] = Value(std::move(crtObj));

    std::string jsonStr = stringify(Value(std::move(root)), 2);

#if defined(__EMSCRIPTEN__)
    MAIN_THREAD_EM_ASM({
        try {
            localStorage.setItem("eatsbits_settings", UTF8ToString($0));
        } catch (e) {
            console.error("Failed to save settings to localStorage:", e);
        }
    }, jsonStr.c_str());
    return true;
#else
    std::string filePath = getSettingsFilePath();
    try {
        std::filesystem::path p(filePath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream ofs(filePath);
        if (ofs.is_open()) {
            ofs << jsonStr;
            return true;
        }
    } catch (...) {}
    return false;
#endif
}

bool GuiWindow::loadSettings() {
    std::string jsonStr;
#if defined(__EMSCRIPTEN__)
    char* jsVal = reinterpret_cast<char*>(MAIN_THREAD_EM_ASM_PTR({
        try {
            var s = localStorage.getItem("eatsbits_settings");
            if (!s) return 0;
            var len = lengthBytesUTF8(s) + 1;
            var buf = _malloc(len);
            stringToUTF8(s, buf, len);
            return buf;
        } catch (e) {
            return 0;
        }
    }));
    if (jsVal) {
        jsonStr = jsVal;
        free(jsVal);
    }
#else
    std::string filePath = getSettingsFilePath();
    try {
        std::ifstream ifs(filePath);
        if (ifs.is_open()) {
            std::stringstream ss;
            ss << ifs.rdbuf();
            jsonStr = ss.str();
        }
    } catch (...) {}
#endif

    if (jsonStr.empty()) return false;

    try {
        auto root = eatsbits::json::Parser(jsonStr).parse();
        if (!root.isObject()) return false;

        if (root.contains("uiScale")) {
            setUiScale(root["uiScale"].asFloat(uiScale_));
        }
        if (root.contains("activeThemePreset")) {
            activeThemePreset_ = root["activeThemePreset"].asInt(activeThemePreset_);
        }
        if (root.contains("themeBank")) {
            setThemeBank(root["themeBank"].asInt(getThemeBank()));
        }
        if (root.contains("antiAliasingMode")) {
            antiAliasingMode_ = root["antiAliasingMode"].asInt(antiAliasingMode_);
        }
        if (root.contains("hiDpiEnabled")) {
            hiDpiEnabled_ = root["hiDpiEnabled"].asBool(hiDpiEnabled_);
        }
        if (root.contains("crtShaderEnabled")) {
            crtShaderEnabled_ = root["crtShaderEnabled"].asBool(crtShaderEnabled_);
            dawnBridge_.setCrtShaderEnabled(crtShaderEnabled_);
        }
        if (root.contains("guiAnimationsEnabled")) {
            guiAnimationsEnabled_ = root["guiAnimationsEnabled"].asBool(guiAnimationsEnabled_);
        }
        if (root.contains("autoRestoreSession")) {
            autoRestoreSession_ = root["autoRestoreSession"].asBool(autoRestoreSession_);
        }
        if (root.contains("autoSaveEnabled")) {
            autoSaveEnabled_ = root["autoSaveEnabled"].asBool(autoSaveEnabled_);
        }
        if (root.contains("includeBinaryClapPath")) {
            audio::ClapPluginLoader::instance().setIncludeBinaryPath(root["includeBinaryClapPath"].asBool());
        }
        if (root.contains("minimalGuiEnabled")) {
            setMinimalGuiEnabled(root["minimalGuiEnabled"].asBool(false));
        }
        if (root.contains("crt") && root["crt"].isObject()) {
            const auto& crtObj = root["crt"];
            auto cfg = dawnBridge_.getMaterialConfig();
            if (crtObj.contains("scanlineIntensity")) cfg.scanlineIntensity = crtObj["scanlineIntensity"].asFloat(cfg.scanlineIntensity);
            if (crtObj.contains("curvature")) cfg.curvature = crtObj["curvature"].asFloat(cfg.curvature);
            if (crtObj.contains("crtReflectionLevel")) cfg.crtReflectionLevel = crtObj["crtReflectionLevel"].asFloat(cfg.crtReflectionLevel);
            if (crtObj.contains("hsyncDistortion")) cfg.hsyncDistortion = crtObj["hsyncDistortion"].asFloat(cfg.hsyncDistortion);
            if (crtObj.contains("spotlightIntensity")) cfg.spotlightIntensity = crtObj["spotlightIntensity"].asFloat(cfg.spotlightIntensity);
            if (crtObj.contains("spotlightSize")) cfg.spotlightSize = crtObj["spotlightSize"].asFloat(cfg.spotlightSize);
            if (crtObj.contains("vignetteStrength")) cfg.vignetteStrength = crtObj["vignetteStrength"].asFloat(cfg.vignetteStrength);
            if (crtObj.contains("reflectionOpacity")) cfg.reflectionOpacity = crtObj["reflectionOpacity"].asFloat(cfg.reflectionOpacity);
            if (crtObj.contains("panelSoftness")) cfg.panelSoftness = crtObj["panelSoftness"].asFloat(cfg.panelSoftness);
            if (crtObj.contains("panelSaturation")) cfg.panelSaturation = crtObj["panelSaturation"].asFloat(cfg.panelSaturation);
            if (crtObj.contains("panelBlackLift")) cfg.panelBlackLift = crtObj["panelBlackLift"].asFloat(cfg.panelBlackLift);
            dawnBridge_.setMaterialConfig(cfg);
        }
        return true;
    } catch (...) {}
    return false;
}

bool GuiWindow::isMinimalGuiEnabled() const noexcept {
    return Theme::getControlMode() == ControlAestheticMode::ForceMinimal;
}

void GuiWindow::setMinimalGuiEnabled(bool enable) noexcept {
    Theme::setControlMode(enable ? ControlAestheticMode::ForceMinimal : ControlAestheticMode::AutoThemeDefault);
    invalidateChromeCache();
    markNeedsRedraw();
}

void GuiWindow::toggleMinimalGui() noexcept {
    setMinimalGuiEnabled(!isMinimalGuiEnabled());
    saveSettings();
    setStatusMessage(isMinimalGuiEnabled() ? "Minimalist Controls: ENABLED (High Performance Vector Knobs)"
                                          : "Minimalist Controls: DISABLED (Skeuomorphic 3D Knobs)");
}

void GuiWindow::revertSettingsToDefault() {
    setUiScale(1.0f);
    setActiveThemePreset(0);
    setThemeBank(0);
    setAntiAliasingMode(2);
    hiDpiEnabled_ = true;
    crtShaderEnabled_ = false;
    dawnBridge_.setCrtShaderEnabled(false);
    dawnBridge_.setMaterialConfig(CrtMaterialConfig{});
    guiAnimationsEnabled_ = true;
    setMinimalGuiEnabled(false);
    autoRestoreSession_ = true;
    autoSaveEnabled_ = true;
    audio::ClapPluginLoader::instance().setIncludeBinaryPath(false);
    saveSettings();
    setStatusMessage("Settings reverted to factory defaults");
    markNeedsRedraw();
}

void GuiWindow::openLocalSettingsFile() {
    saveSettings();
#if defined(_WIN32)
    std::string path = getSettingsFilePath();
    HINSTANCE hInst = ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(hInst) > 32) {
        setStatusMessage("Opened settings file in system editor: " + path);
    } else {
        setStatusMessage("Settings file location: " + path);
    }
#elif defined(__APPLE__)
    std::string path = getSettingsFilePath();
    system(("open "" + path + """).c_str());
    setStatusMessage("Opened settings file: " + path);
#elif !defined(__EMSCRIPTEN__)
    std::string path = getSettingsFilePath();
    system(("xdg-open "" + path + """).c_str());
    setStatusMessage("Opened settings file: " + path);
#else
    setStatusMessage("Settings stored in browser localStorage");
#endif
}

void GuiWindow::openThemeBrowserDialog() {
    themeBrowserDialog_.open();
    themeBrowserDialog_.onThemeApplied = [this](Theme::Preset preset) {
        activeThemePreset_ = static_cast<int>(preset);
        setStatusMessage("Theme applied: " + getTheme().name);
        markNeedsRedraw();
    };
    themeBrowserDialog_.onClose = [this]() {
        markNeedsRedraw();
    };
    markNeedsRedraw();
}

} // namespace eatsbits::ui

