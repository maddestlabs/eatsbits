#ifndef EATS_TERMINAL_CONSOLE_DRAWER_HPP
#define EATS_TERMINAL_CONSOLE_DRAWER_HPP

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <string_view>
#include "eatsbits/terminal/terminal_grid.hpp"
#include "eatsbits/terminal/ansi_parser.hpp"
#include "eatsbits/eatscript/repl.hpp"
#include "eatsbits/abi/host_registry.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include "eatsbits/ui/theme.hpp"
#include "eatsbits/input/pointer_event.hpp"
#include "eatsbits/core/geometry.hpp"
#include "eatsbits/presenter/frame_time_context.hpp"

namespace eatsbits::audio {
    class AudioEngine;
}

namespace eatsbits::ui {

/**
 * @brief Interactive In-DAW WebGPU Terminal & Live REPL Console Drawer.
 * Embedded directly in the GUI desktop and web DAW, docked above the bottom nav bar.
 * Powered by hardware-accelerated monospace font rendering, dual-buffered TerminalGrid,
 * zero-allocation ANSI escape stream parser, and the Pythonic Eatscript REPL with live
 * Host ABI audio engine reflection.
 */
class TerminalConsoleDrawer {
public:
    TerminalConsoleDrawer();
    ~TerminalConsoleDrawer();

    void layout(float screenWidth, float bottomNavTopY, float uiScale = 1.0f);
    void update(const FrameTimeContext& time) noexcept;
    void render(BatchRenderer2D& r, const ThemeTokens& theme, float uiScale = 1.0f);

    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);
    bool handleChar(char32_t codepoint);

    void bindAudioEngine(audio::AudioEngine* engine);

    [[nodiscard]] bool isExpanded() const noexcept { return isExpanded_; }
    void setExpanded(bool exp) noexcept;
    void toggleExpanded() noexcept { setExpanded(!isExpanded_); }

    [[nodiscard]] float getDrawerHeight() const noexcept {
        return isExpanded_ ? (drawerHeight_ + kPullTabHeight) : kPullTabHeight;
    }

    [[nodiscard]] core::Rect2D getDrawerBounds() const noexcept { return drawerBounds_; }
    [[nodiscard]] core::Rect2D getPullTabBounds() const noexcept { return pullTabBounds_; }

    void executeCommand(const std::string& command);
    void clear();

    [[nodiscard]] const terminal::TerminalGrid& getGrid() const noexcept { return grid_; }
    [[nodiscard]] terminal::TerminalGrid& getGrid() noexcept { return grid_; }
    [[nodiscard]] const eatscript::ReplEngine& getRepl() const noexcept { return repl_; }
    [[nodiscard]] eatscript::ReplEngine& getRepl() noexcept { return repl_; }

private:
    void printWelcomeBanner();
    void printPrompt();
    void commitCurrentLine();
    void setupHostBindings();

    static constexpr float kPullTabHeight{28.0f};
    static constexpr float kDefaultDrawerHeight{260.0f};
    static constexpr float kCellWidth{8.0f};
    static constexpr float kCellHeight{16.0f};

    bool isExpanded_{false};
    float drawerHeight_{kDefaultDrawerHeight};
    core::Rect2D drawerBounds_{};
    core::Rect2D pullTabBounds_{};
    core::Rect2D terminalAreaBounds_{};

    terminal::TerminalGrid grid_{100, 30};
    terminal::AnsiParser parser_{grid_};
    abi::HostRegistry hostRegistry_{};
    eatscript::ReplEngine repl_{&hostRegistry_};
    audio::AudioEngine* audioEngine_{nullptr};

    std::string currentLine_{};
    size_t cursorCol_{0};
    double cursorBlinkTimer_{0.0};
    bool cursorBlinkState_{true};
};

} // namespace eatsbits::ui

#endif // EATS_TERMINAL_CONSOLE_DRAWER_HPP
