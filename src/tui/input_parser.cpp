#include "eatsbits/tui/input_parser.hpp"
#include <cctype>

namespace eatsbits::tui {

InputParser::InputParser() = default;

void InputParser::feed(const char* data, size_t length) {
    if (!data || length == 0) return;
    buffer_.append(data, length);
    processBuffer();
}

void InputParser::feed(std::string_view data) {
    feed(data.data(), data.size());
}

bool InputParser::pollKey(KeyEvent& out) {
    if (keyQueue_.empty()) return false;
    out = keyQueue_.front();
    keyQueue_.pop_front();
    return true;
}

bool InputParser::pollMouse(MouseEvent& out) {
    if (mouseQueue_.empty()) return false;
    out = mouseQueue_.front();
    mouseQueue_.pop_front();
    return true;
}

void InputParser::clear() {
    buffer_.clear();
    keyQueue_.clear();
    mouseQueue_.clear();
}

bool InputParser::parseSgrMouse(size_t& consumed) {
    // Format: \x1b[<b;x;yM or \x1b[<b;x;ym
    if (buffer_.size() < 6) return false;
    if (buffer_[0] != '\x1b' || buffer_[1] != '[' || buffer_[2] != '<') return false;

    size_t endPos = buffer_.find_first_of("Mm", 3);
    if (endPos == std::string::npos) return false;

    std::string payload = buffer_.substr(3, endPos - 3);
    char termChar = buffer_[endPos];
    consumed = endPos + 1;

    // Parse button, x, y
    int b = 0, x = 0, y = 0;
    if (sscanf(payload.c_str(), "%d;%d;%d", &b, &x, &y) == 3) {
        MouseEvent me;
        me.x = x - 1; // 1-indexed to 0-indexed
        me.y = y - 1;
        me.button = b & 0x03;
        if (b & 64) {
            me.button = (b & 1) ? 65 : 64; // WheelDown or WheelUp
        }
        me.isDrag = (b & 32) != 0;
        me.shift = (b & 4) != 0;
        me.ctrl = (b & 16) != 0;
        me.isRelease = (termChar == 'm');

        mouseQueue_.push_back(me);
        return true;
    }

    return false;
}

bool InputParser::parseEscapeSequence(size_t& consumed) {
    if (buffer_.empty() || buffer_[0] != '\x1b') return false;

    if (buffer_.size() == 1) {
        // Just ESC
        KeyEvent ev;
        ev.code = KeyCode::Escape;
        keyQueue_.push_back(ev);
        consumed = 1;
        return true;
    }

    // Check SGR mouse first
    if (buffer_.size() >= 3 && buffer_[1] == '[' && buffer_[2] == '<') {
        if (parseSgrMouse(consumed)) {
            return true;
        }
    }

    if (buffer_[1] == '[') {
        if (buffer_.size() < 3) return false; // Need more bytes

        char c = buffer_[2];
        if (c == 'A') { keyQueue_.push_back({KeyCode::Up}); consumed = 3; return true; }
        if (c == 'B') { keyQueue_.push_back({KeyCode::Down}); consumed = 3; return true; }
        if (c == 'C') { keyQueue_.push_back({KeyCode::Right}); consumed = 3; return true; }
        if (c == 'D') { keyQueue_.push_back({KeyCode::Left}); consumed = 3; return true; }
        if (c == 'H') { keyQueue_.push_back({KeyCode::Home}); consumed = 3; return true; }
        if (c == 'F') { keyQueue_.push_back({KeyCode::End}); consumed = 3; return true; }
        if (c == 'Z') { keyQueue_.push_back({KeyCode::Tab, 0, true}); consumed = 3; return true; } // Shift-Tab

        // Sequence with tilde: \x1b[1~ through \x1b[6~
        if (buffer_.size() >= 4 && buffer_[3] == '~') {
            consumed = 4;
            if (c == '1' || c == '7') { keyQueue_.push_back({KeyCode::Home}); return true; }
            if (c == '2') { keyQueue_.push_back({KeyCode::Insert}); return true; }
            if (c == '3') { keyQueue_.push_back({KeyCode::Delete}); return true; }
            if (c == '4' || c == '8') { keyQueue_.push_back({KeyCode::End}); return true; }
            if (c == '5') { keyQueue_.push_back({KeyCode::PageUp}); return true; }
            if (c == '6') { keyQueue_.push_back({KeyCode::PageDown}); return true; }
        }

        // F1-F4 often \x1b[11~ to \x1b[14~
        if (buffer_.size() >= 5 && buffer_[4] == '~') {
            consumed = 5;
            int fNum = (c - '0') * 10 + (buffer_[3] - '0');
            if (fNum >= 11 && fNum <= 24) {
                keyQueue_.push_back({static_cast<KeyCode>(static_cast<int>(KeyCode::F1) + (fNum - 11))});
                return true;
            }
        }
    } else if (buffer_[1] == 'O') {
        // SS3 sequences: \x1bOP to \x1bOS (F1 - F4)
        if (buffer_.size() >= 3) {
            char c = buffer_[2];
            consumed = 3;
            if (c == 'P') { keyQueue_.push_back({KeyCode::F1}); return true; }
            if (c == 'Q') { keyQueue_.push_back({KeyCode::F2}); return true; }
            if (c == 'R') { keyQueue_.push_back({KeyCode::F3}); return true; }
            if (c == 'S') { keyQueue_.push_back({KeyCode::F4}); return true; }
        }
    }

    // Alt + key sequence: \x1b + char
    if (buffer_.size() >= 2) {
        KeyEvent ev;
        ev.code = KeyCode::Char;
        ev.codepoint = static_cast<char32_t>(static_cast<uint8_t>(buffer_[1]));
        ev.alt = true;
        keyQueue_.push_back(ev);
        consumed = 2;
        return true;
    }

    return false;
}

void InputParser::processBuffer() {
    size_t i = 0;
    while (i < buffer_.size()) {
        uint8_t b = static_cast<uint8_t>(buffer_[i]);

        if (b == '\x1b') {
            size_t consumed = 0;
            // Create a sub-slice from current position
            std::string sub = buffer_.substr(i);
            std::string origBuf = buffer_;
            buffer_ = sub;
            bool ok = parseEscapeSequence(consumed);
            buffer_ = origBuf;

            if (ok && consumed > 0) {
                i += consumed;
                continue;
            } else if (buffer_.size() - i > 8) {
                // Unknown long sequence, drop ESC
                keyQueue_.push_back({KeyCode::Escape});
                i++;
                continue;
            } else {
                // Incomplete sequence, keep remaining bytes
                break;
            }
        } else if (b == '\r' || b == '\n') {
            keyQueue_.push_back({KeyCode::Enter});
            i++;
        } else if (b == '\t') {
            keyQueue_.push_back({KeyCode::Tab});
            i++;
        } else if (b == ' ') {
            keyQueue_.push_back({KeyCode::Space, ' '});
            i++;
        } else if (b == 0x7F || b == 0x08) {
            keyQueue_.push_back({KeyCode::Backspace});
            i++;
        } else if (b < 32) {
            // Ctrl+Key (Ctrl+A = 1, Ctrl+B = 2, ...)
            KeyEvent ev;
            ev.code = KeyCode::Char;
            ev.codepoint = static_cast<char32_t>('a' + (b - 1));
            ev.ctrl = true;
            keyQueue_.push_back(ev);
            i++;
        } else {
            // UTF-8 lead byte check
            char32_t cp = 0;
            size_t len = 1;
            if (b < 0x80) {
                cp = b;
            } else if ((b & 0xE0) == 0xC0) {
                len = 2;
                cp = b & 0x1F;
            } else if ((b & 0xF0) == 0xE0) {
                len = 3;
                cp = b & 0x0F;
            } else if ((b & 0xF8) == 0xF0) {
                len = 4;
                cp = b & 0x07;
            }

            if (i + len > buffer_.size()) {
                // Incomplete UTF-8 sequence, wait for next feed
                break;
            }

            for (size_t k = 1; k < len; ++k) {
                cp = (cp << 6) | (static_cast<uint8_t>(buffer_[i + k]) & 0x3F);
            }

            keyQueue_.push_back({KeyCode::Char, cp});
            i += len;
        }
    }

    if (i > 0) {
        buffer_.erase(0, i);
    }
}

} // namespace eatsbits::tui
