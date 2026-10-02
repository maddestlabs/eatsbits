#pragma once

#include "eatsbits/terminal/terminal_grid.hpp"
#include <string_view>
#include <cstdint>
#include <cstddef>

namespace eatsbits::terminal {

/**
 * @brief High-throughput, zero-allocation VT100/Xterm ANSI escape sequence state machine.
 * Parses raw byte streams (CSI, SGR TrueColor & 256-color, cursor repositioning, erase modes,
 * DEC private modes, bracketed paste, OSC window title, and UTF-8 multibyte characters)
 * and directly drives a TerminalGrid.
 */
class AnsiParser {
public:
    explicit AnsiParser(TerminalGrid& grid);
    ~AnsiParser() = default;

    // Zero-allocation streaming parser
    void parse(std::string_view bytes);
    void parseByte(uint8_t byte);
    void reset() noexcept;

    [[nodiscard]] const TerminalGrid& getGrid() const noexcept { return grid_; }
    [[nodiscard]] TerminalGrid& getGrid() noexcept { return grid_; }

private:
    enum class State {
        Ground,
        Escape,
        Csi,
        Osc,
        Charset
    };

    TerminalGrid& grid_;
    State state_{State::Ground};

    // CSI parameter accumulation (fixed size on stack/object, zero heap allocation)
    static constexpr size_t kMaxParams = 16;
    int params_[kMaxParams]{0};
    size_t paramCount_{0};
    bool inSubParam_{false};
    bool isPrivate_{false}; // '?' prefix in CSI (DEC private mode)

    // UTF-8 stream decoder state machine
    uint32_t utf8Codepoint_{0};
    int utf8BytesRemaining_{0};

    // OSC buffer (e.g., window title)
    static constexpr size_t kMaxOscBuffer = 256;
    char oscBuffer_[kMaxOscBuffer]{};
    size_t oscLength_{0};

    void handleCsi(char finalChar);
    void handleSgr();
    void handleOsc();
    void handleControlChar(uint8_t c);
    void resetCsi() noexcept;

    static tui::Color colorFrom256(uint8_t index) noexcept;
};

} // namespace eatsbits::terminal
