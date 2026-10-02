#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

#include <GLFW/glfw3.h>

#include "eatsbits/terminal/terminal_grid.hpp"
#include "eatsbits/terminal/ansi_parser.hpp"
#include "eatsbits/eatscript/repl.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"

namespace {

struct TermAppState {
    eatsbits::terminal::TerminalGrid grid{110, 34};
    eatsbits::terminal::AnsiParser parser{grid};
    eatsbits::eatscript::ReplEngine repl{};
    eatsbits::ui::BatchRenderer2D renderer{};

    std::string currentLine;
    double cursorTimer{0.0};
    bool cursorBlink{true};
    int windowWidth{960};
    int windowHeight{600};
};

TermAppState* g_appState = nullptr;

void printPrompt(TermAppState& app) {
    std::string prompt = app.repl.getCurrentPrompt();
    app.parser.parse("\x1b[1;38;2;140;110;250m" + prompt + "\x1b[0m");
}

void commitLine(TermAppState& app) {
    app.parser.parse("\r\n");
    std::string line = app.currentLine;
    app.currentLine.clear();

    auto res = app.repl.feedLine(line);
    if (res.status == eatsbits::eatscript::ReplResult::Status::Complete) {
        if (!res.output.empty()) {
            app.parser.parse(res.output);
            if (res.output.back() != '\n') {
                app.parser.parse("\r\n");
            }
        }
    } else if (res.status == eatsbits::eatscript::ReplResult::Status::Error) {
        app.parser.parse("\x1b[38;2;255;90;90m" + res.error + "\x1b[0m\r\n");
    }

    printPrompt(app);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    (void)mods;
    if (!g_appState) return;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    auto& app = *g_appState;

    if (key == GLFW_KEY_ENTER) {
        commitLine(app);
    } else if (key == GLFW_KEY_BACKSPACE) {
        if (!app.currentLine.empty()) {
            app.currentLine.pop_back();
            app.parser.parse("\b \b");
        }
    } else if (key == GLFW_KEY_UP) {
        std::string prev = app.repl.historyPrev();
        if (!prev.empty()) {
            while (!app.currentLine.empty()) {
                app.currentLine.pop_back();
                app.parser.parse("\b \b");
            }
            app.currentLine = prev;
            app.parser.parse(app.currentLine);
        }
    } else if (key == GLFW_KEY_DOWN) {
        std::string next = app.repl.historyNext();
        while (!app.currentLine.empty()) {
            app.currentLine.pop_back();
            app.parser.parse("\b \b");
        }
        app.currentLine = next;
        if (!app.currentLine.empty()) {
            app.parser.parse(app.currentLine);
        }
    } else if (key == GLFW_KEY_TAB) {
        auto matches = app.repl.complete(app.currentLine);
        if (matches.size() == 1) {
            while (!app.currentLine.empty()) {
                app.currentLine.pop_back();
                app.parser.parse("\b \b");
            }
            app.currentLine = matches[0];
            app.parser.parse(app.currentLine);
        } else if (matches.size() > 1) {
            app.parser.parse("\r\n\x1b[38;2;120;140;180m");
            for (const auto& m : matches) {
                app.parser.parse(m + "  ");
            }
            app.parser.parse("\x1b[0m\r\n");
            printPrompt(app);
            app.parser.parse(app.currentLine);
        }
    }
}

void charCallback(GLFWwindow* window, unsigned int codepoint) {
    (void)window;
    if (!g_appState) return;
    if (codepoint < 32 || codepoint == 127) return;

    auto& app = *g_appState;
    std::string utf8;
    if (codepoint < 0x80) {
        utf8.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        utf8.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        utf8.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        utf8.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }

    app.currentLine += utf8;
    app.parser.parse(utf8);
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    (void)window;
    if (!g_appState || width <= 0 || height <= 0) return;

    auto& app = *g_appState;
    app.windowWidth = width;
    app.windowHeight = height;
    app.renderer.resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    int cols = std::clamp((width - 24) / 8, 20, 240);
    int rows = std::clamp((height - 24) / 16, 5, 100);
    if (cols != app.grid.getCols() || rows != app.grid.getRows()) {
        app.grid.resize(cols, rows);
    }
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(960, 600, "Eatsbits WebGPU Terminal", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }

    TermAppState app;
    g_appState = &app;

    app.renderer.initialize(window, 960, 600, eatsbits::ui::RenderBackendType::Filament);
    app.renderer.setDirectPresent(true);
    app.renderer.initDefaultMonospaceAtlas();

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    // Initial Welcome Banner
    app.parser.parse("\x1b[38;2;160;100;255m╔═════════════════════════════════════════════════════════════════╗\x1b[0m\r\n");
    app.parser.parse("\x1b[38;2;160;100;255m║\x1b[0m  \x1b[1;38;2;255;255;255mEatsbits Standalone WebGPU GPU Terminal & Eatscript Shell\x1b[0m      \x1b[38;2;160;100;255m║\x1b[0m\r\n");
    app.parser.parse("\x1b[38;2;160;100;255m║\x1b[0m  Type expressions, \x1b[38;2;80;220;255mmath.pi\x1b[0m, \x1b[38;2;80;220;255msys.platform\x1b[0m, or functions.             \x1b[38;2;160;100;255m║\x1b[0m\r\n");
    app.parser.parse("\x1b[38;2;160;100;255m╚═════════════════════════════════════════════════════════════════╝\x1b[0m\r\n\r\n");
    printPrompt(app);

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;

        app.cursorTimer += dt;
        app.cursorBlink = (std::fmod(app.cursorTimer, 0.8) < 0.4);

        float w = static_cast<float>(app.windowWidth);
        float h = static_cast<float>(app.windowHeight);

        app.renderer.beginFrame(w, h);

        // Dark background
        app.renderer.drawRect(0.0f, 0.0f, w, h, 0.07f, 0.08f, 0.10f, 1.0f);

        // Render Monospace Terminal Grid
        app.renderer.drawTerminalGrid(12.0f, 12.0f, 8.0f, 16.0f,
                                      app.grid.getFrontBuffer(),
                                      app.grid.getCols(), app.grid.getRows());

        // Render Blinking Cursor
        if (app.cursorBlink && app.grid.isCursorVisible()) {
            float cx = 12.0f + static_cast<float>(app.grid.getCursorX()) * 8.0f;
            float cy = 12.0f + static_cast<float>(app.grid.getCursorY()) * 16.0f;
            if (cx + 8.0f <= w && cy + 16.0f <= h) {
                app.renderer.drawRect(cx, cy, 8.0f, 16.0f, 0.85f, 0.85f, 0.95f, 0.75f);
            }
        }

        app.renderer.endFrame();
    }

    g_appState = nullptr;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
