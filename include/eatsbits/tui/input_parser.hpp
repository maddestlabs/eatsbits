#pragma once

#include <cstdint>
#include <string>
#include <deque>
#include <vector>

namespace eatsbits::tui {

enum class KeyCode {
    None,
    Char,
    Enter,
    Escape,
    Backspace,
    Tab,
    Space,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    PageUp,
    PageDown,
    Delete,
    Insert,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12
};

struct KeyEvent {
    KeyCode code{KeyCode::None};
    char32_t codepoint{0};
    bool shift{false};
    bool ctrl{false};
    bool alt{false};

    bool isChar(char c) const {
        return code == KeyCode::Char && codepoint == static_cast<char32_t>(c);
    }
};

struct MouseEvent {
    int x{0};
    int y{0};
    int button{0}; // 0 = Left, 1 = Middle, 2 = Right, 64 = WheelUp, 65 = WheelDown
    bool isRelease{false};
    bool isDrag{false};
    bool shift{false};
    bool ctrl{false};
};

class InputParser {
public:
    InputParser();
    ~InputParser() = default;

    void feed(const char* data, size_t length);
    void feed(std::string_view data);

    bool pollKey(KeyEvent& out);
    bool pollMouse(MouseEvent& out);

    bool hasPendingKeys() const { return !keyQueue_.empty(); }
    bool hasPendingMouse() const { return !mouseQueue_.empty(); }
    void clear();

private:
    std::string buffer_;
    std::deque<KeyEvent> keyQueue_;
    std::deque<MouseEvent> mouseQueue_;

    void processBuffer();
    bool parseEscapeSequence(size_t& consumed);
    bool parseSgrMouse(size_t& consumed);
};

} // namespace eatsbits::tui
