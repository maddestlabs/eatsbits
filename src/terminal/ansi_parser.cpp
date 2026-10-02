#include "eatsbits/terminal/ansi_parser.hpp"
#include <algorithm>
#include <cstring>

namespace eatsbits::terminal {

AnsiParser::AnsiParser(TerminalGrid& grid)
    : grid_(grid) {
    reset();
}

void AnsiParser::reset() noexcept {
    state_ = State::Ground;
    resetCsi();
    utf8Codepoint_ = 0;
    utf8BytesRemaining_ = 0;
    oscLength_ = 0;
    oscBuffer_[0] = '\0';
}

void AnsiParser::resetCsi() noexcept {
    std::fill_n(params_, kMaxParams, 0);
    paramCount_ = 0;
    inSubParam_ = false;
    isPrivate_ = false;
}

tui::Color AnsiParser::colorFrom256(uint8_t index) noexcept {
    // 0..15: Standard and bright terminal colors
    static const uint32_t kPalette16[16] = {
        0x000000, 0xCD0000, 0x00CD00, 0xCDCD00,
        0x0000EE, 0xCD00CD, 0x00CDCD, 0xE5E5E5,
        0x7F7F7F, 0xFF0000, 0x00FF00, 0xFFFF00,
        0x5C5CFF, 0xFF00FF, 0x00FFFF, 0xFFFFFF
    };
    if (index < 16) {
        return tui::Color::FromHex(kPalette16[index]);
    }

    // 16..231: 6x6x6 color cube
    if (index < 232) {
        uint8_t idx = index - 16;
        uint8_t r = idx / 36;
        uint8_t g = (idx / 6) % 6;
        uint8_t b = idx % 6;
        uint8_t cr = r ? static_cast<uint8_t>(r * 40 + 55) : 0;
        uint8_t cg = g ? static_cast<uint8_t>(g * 40 + 55) : 0;
        uint8_t cb = b ? static_cast<uint8_t>(b * 40 + 55) : 0;
        return tui::Color::FromRgb(cr, cg, cb);
    }

    // 232..255: Grayscale ramp (24 steps from 8 to 238)
    uint8_t gray = static_cast<uint8_t>((index - 232) * 10 + 8);
    return tui::Color::FromRgb(gray, gray, gray);
}

void AnsiParser::parse(std::string_view bytes) {
    for (char ch : bytes) {
        parseByte(static_cast<uint8_t>(ch));
    }
}

void AnsiParser::parseByte(uint8_t byte) {
    switch (state_) {
        case State::Ground: {
            if (byte == 0x1B) {
                state_ = State::Escape;
            } else if (byte < 0x20 || byte == 0x7F) {
                handleControlChar(byte);
            } else {
                // UTF-8 Decoder State Machine
                if ((byte & 0x80) == 0) {
                    // Single-byte ASCII [0x00..0x7F]
                    grid_.writeCodepoint(static_cast<char32_t>(byte));
                } else if ((byte & 0xE0) == 0xC0) {
                    // 2-byte UTF-8 sequence lead byte
                    utf8Codepoint_ = byte & 0x1F;
                    utf8BytesRemaining_ = 1;
                } else if ((byte & 0xF0) == 0xE0) {
                    // 3-byte UTF-8 sequence lead byte
                    utf8Codepoint_ = byte & 0x0F;
                    utf8BytesRemaining_ = 2;
                } else if ((byte & 0xF8) == 0xF0) {
                    // 4-byte UTF-8 sequence lead byte
                    utf8Codepoint_ = byte & 0x07;
                    utf8BytesRemaining_ = 3;
                } else if ((byte & 0xC0) == 0x80) {
                    // UTF-8 continuation byte
                    if (utf8BytesRemaining_ > 0) {
                        utf8Codepoint_ = (utf8Codepoint_ << 6) | (byte & 0x3F);
                        utf8BytesRemaining_--;
                        if (utf8BytesRemaining_ == 0) {
                            grid_.writeCodepoint(static_cast<char32_t>(utf8Codepoint_));
                        }
                    }
                } else {
                    // Invalid byte; reset UTF-8 state
                    utf8BytesRemaining_ = 0;
                }
            }
            break;
        }

        case State::Escape: {
            if (byte == '[') {
                state_ = State::Csi;
                resetCsi();
            } else if (byte == ']') {
                state_ = State::Osc;
                oscLength_ = 0;
                oscBuffer_[0] = '\0';
            } else if (byte == '(' || byte == ')') {
                state_ = State::Charset;
            } else if (byte == '7') {
                grid_.saveCursor();
                state_ = State::Ground;
            } else if (byte == '8') {
                grid_.restoreCursor();
                state_ = State::Ground;
            } else if (byte == 'c') {
                grid_.resetAttributes();
                grid_.eraseInDisplay(2);
                grid_.setCursorPos(0, 0);
                state_ = State::Ground;
            } else {
                state_ = State::Ground;
            }
            break;
        }

        case State::Charset: {
            // Ignore charset designation byte (e.g., B for US-ASCII, 0 for DEC line drawing)
            state_ = State::Ground;
            break;
        }

        case State::Csi: {
            if (byte >= '0' && byte <= '9') {
                if (paramCount_ < kMaxParams) {
                    params_[paramCount_] = params_[paramCount_] * 10 + (byte - '0');
                    inSubParam_ = true;
                }
            } else if (byte == ';' || byte == ':') {
                if (paramCount_ + 1 < kMaxParams) {
                    paramCount_++;
                    params_[paramCount_] = 0;
                }
                inSubParam_ = false;
            } else if (byte == '?') {
                isPrivate_ = true;
            } else if ((byte >= '@' && byte <= '~')) {
                // Command terminator
                if (inSubParam_ && paramCount_ < kMaxParams) {
                    paramCount_++;
                }
                handleCsi(static_cast<char>(byte));
                state_ = State::Ground;
            } else if (byte == 0x1B) {
                // Interrupted by new Escape
                state_ = State::Escape;
            }
            break;
        }

        case State::Osc: {
            if (byte == 0x07) {
                // BEL ends OSC
                handleOsc();
                state_ = State::Ground;
            } else if (byte == '\\' && oscLength_ > 0 && oscBuffer_[oscLength_ - 1] == 0x1B) {
                // ST (ESC \) ends OSC
                oscBuffer_[--oscLength_] = '\0';
                handleOsc();
                state_ = State::Ground;
            } else {
                if (oscLength_ + 1 < kMaxOscBuffer) {
                    oscBuffer_[oscLength_++] = static_cast<char>(byte);
                    oscBuffer_[oscLength_] = '\0';
                }
            }
            break;
        }
    }
}

void AnsiParser::handleControlChar(uint8_t c) {
    switch (c) {
        case '\r':
            grid_.carriageReturn();
            break;
        case '\n':
            grid_.newline();
            break;
        case '\b':
            grid_.backspace();
            break;
        case '\t':
            grid_.tab();
            break;
        default:
            break;
    }
}

void AnsiParser::handleCsi(char finalChar) {
    switch (finalChar) {
        case 'm': // Select Graphic Rendition (SGR)
            handleSgr();
            break;

        case 'A': // CUU - Cursor Up
            grid_.moveCursor(0, -std::max(1, params_[0]));
            break;

        case 'B': // CUD - Cursor Down
            grid_.moveCursor(0, std::max(1, params_[0]));
            break;

        case 'C': // CUF - Cursor Forward
            grid_.moveCursor(std::max(1, params_[0]), 0);
            break;

        case 'D': // CUB - Cursor Back
            grid_.moveCursor(-std::max(1, params_[0]), 0);
            break;

        case 'E': // CNL - Cursor Next Line
            grid_.setCursorPos(0, grid_.getCursorY() + std::max(1, params_[0]));
            break;

        case 'F': // CPL - Cursor Previous Line
            grid_.setCursorPos(0, grid_.getCursorY() - std::max(1, params_[0]));
            break;

        case 'G': // CHA - Cursor Horizontal Absolute
            grid_.setCursorPos(std::max(1, params_[0]) - 1, grid_.getCursorY());
            break;

        case 'H': // CUP - Cursor Position (1-indexed: row; col)
        case 'f': {
            int row = (paramCount_ >= 1 && params_[0] > 0) ? (params_[0] - 1) : 0;
            int col = (paramCount_ >= 2 && params_[1] > 0) ? (params_[1] - 1) : 0;
            grid_.setCursorPos(col, row);
            break;
        }

        case 'J': // ED - Erase in Display
            grid_.eraseInDisplay(paramCount_ >= 1 ? params_[0] : 0);
            break;

        case 'K': // EL - Erase in Line
            grid_.eraseInLine(paramCount_ >= 1 ? params_[0] : 0);
            break;

        case 'S': // SU - Scroll Up
            grid_.scrollUp(std::max(1, params_[0]));
            break;

        case 'T': // SD - Scroll Down
            grid_.scrollDown(std::max(1, params_[0]));
            break;

        case 's': // SCP - Save Cursor Position
            grid_.saveCursor();
            break;

        case 'u': // RCP - Restore Cursor Position
            grid_.restoreCursor();
            break;

        case 'h': // Set Mode / DEC Private Mode Set
            if (isPrivate_) {
                int p = (paramCount_ >= 1) ? params_[0] : 0;
                if (p == 25) {
                    grid_.setCursorVisible(true);
                } else if (p == 1049) {
                    grid_.setAlternateScreen(true);
                } else if (p == 2004) {
                    grid_.setBracketedPaste(true);
                }
            }
            break;

        case 'l': // Reset Mode / DEC Private Mode Reset
            if (isPrivate_) {
                int p = (paramCount_ >= 1) ? params_[0] : 0;
                if (p == 25) {
                    grid_.setCursorVisible(false);
                } else if (p == 1049) {
                    grid_.setAlternateScreen(false);
                } else if (p == 2004) {
                    grid_.setBracketedPaste(false);
                }
            }
            break;

        default:
            break;
    }
}

void AnsiParser::handleSgr() {
    if (paramCount_ == 0) {
        grid_.resetAttributes();
        return;
    }

    static const tui::Color standardColors[8] = {
        tui::Color::FromHex(0x000000), // 30: Black
        tui::Color::FromHex(0xCD0000), // 31: Red
        tui::Color::FromHex(0x00CD00), // 32: Green
        tui::Color::FromHex(0xCDCD00), // 33: Yellow
        tui::Color::FromHex(0x0000EE), // 34: Blue
        tui::Color::FromHex(0xCD00CD), // 35: Magenta
        tui::Color::FromHex(0x00CDCD), // 36: Cyan
        tui::Color::FromHex(0xE5E5E5)  // 37: White
    };

    static const tui::Color brightColors[8] = {
        tui::Color::FromHex(0x7F7F7F), // 90: Bright Black (Dark Gray)
        tui::Color::FromHex(0xFF0000), // 91: Bright Red
        tui::Color::FromHex(0x00FF00), // 92: Bright Green
        tui::Color::FromHex(0xFFFF00), // 93: Bright Yellow
        tui::Color::FromHex(0x5C5CFF), // 94: Bright Blue
        tui::Color::FromHex(0xFF00FF), // 95: Bright Magenta
        tui::Color::FromHex(0x00FFFF), // 96: Bright Cyan
        tui::Color::FromHex(0xFFFFFF)  // 97: Bright White
    };

    for (size_t i = 0; i < paramCount_; ++i) {
        int p = params_[i];
        if (p == 0) {
            grid_.resetAttributes();
        } else if (p == 1) {
            grid_.addAttributes(static_cast<uint8_t>(tui::TextAttr::Bold));
        } else if (p == 2) {
            grid_.addAttributes(static_cast<uint8_t>(tui::TextAttr::Dim));
        } else if (p == 4) {
            grid_.addAttributes(static_cast<uint8_t>(tui::TextAttr::Underline));
        } else if (p == 5 || p == 6) {
            grid_.addAttributes(static_cast<uint8_t>(tui::TextAttr::Blink));
        } else if (p == 7) {
            grid_.addAttributes(static_cast<uint8_t>(tui::TextAttr::Reverse));
        } else if (p == 22) {
            grid_.removeAttributes(static_cast<uint8_t>(tui::TextAttr::Bold) |
                                  static_cast<uint8_t>(tui::TextAttr::Dim));
        } else if (p == 24) {
            grid_.removeAttributes(static_cast<uint8_t>(tui::TextAttr::Underline));
        } else if (p == 25) {
            grid_.removeAttributes(static_cast<uint8_t>(tui::TextAttr::Blink));
        } else if (p == 27) {
            grid_.removeAttributes(static_cast<uint8_t>(tui::TextAttr::Reverse));
        } else if (p >= 30 && p <= 37) {
            grid_.setFgColor(standardColors[p - 30]);
        } else if (p == 38) {
            // Extended Foreground Color
            if (i + 4 < paramCount_ && params_[i + 1] == 2) {
                // 38;2;r;g;b (24-bit TrueColor)
                uint8_t r = static_cast<uint8_t>(params_[i + 2]);
                uint8_t g = static_cast<uint8_t>(params_[i + 3]);
                uint8_t b = static_cast<uint8_t>(params_[i + 4]);
                grid_.setFgColor(tui::Color::FromRgb(r, g, b));
                i += 4;
            } else if (i + 2 < paramCount_ && params_[i + 1] == 5) {
                // 38;5;n (256-color palette)
                grid_.setFgColor(colorFrom256(static_cast<uint8_t>(params_[i + 2])));
                i += 2;
            }
        } else if (p == 39) {
            grid_.setFgColor(tui::Color::DefaultFg());
        } else if (p >= 40 && p <= 47) {
            grid_.setBgColor(standardColors[p - 40]);
        } else if (p == 48) {
            // Extended Background Color
            if (i + 4 < paramCount_ && params_[i + 1] == 2) {
                // 48;2;r;g;b (24-bit TrueColor)
                uint8_t r = static_cast<uint8_t>(params_[i + 2]);
                uint8_t g = static_cast<uint8_t>(params_[i + 3]);
                uint8_t b = static_cast<uint8_t>(params_[i + 4]);
                grid_.setBgColor(tui::Color::FromRgb(r, g, b));
                i += 4;
            } else if (i + 2 < paramCount_ && params_[i + 1] == 5) {
                // 48;5;n (256-color palette)
                grid_.setBgColor(colorFrom256(static_cast<uint8_t>(params_[i + 2])));
                i += 2;
            }
        } else if (p == 49) {
            grid_.setBgColor(tui::Color::DefaultBg());
        } else if (p >= 90 && p <= 97) {
            grid_.setFgColor(brightColors[p - 90]);
        } else if (p >= 100 && p <= 107) {
            grid_.setBgColor(brightColors[p - 100]);
        }
    }
}

void AnsiParser::handleOsc() {
    std::string_view osc(oscBuffer_, oscLength_);
    // Check for "0;" or "2;" to set window/terminal title
    if (osc.rfind("0;", 0) == 0 || osc.rfind("2;", 0) == 0) {
        grid_.setTitle(std::string(osc.substr(2)));
    }
}

} // namespace eatsbits::terminal
