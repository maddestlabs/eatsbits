#include "eatsbits/tui/terminal_device.hpp"
#include <iostream>
#include <cstdio>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/ioctl.h>
    #include <fcntl.h>
    #include <signal.h>
#endif

namespace eatsbits::tui {

TerminalDevice::TerminalDevice() = default;

TerminalDevice::~TerminalDevice() {
    shutdown();
}

bool TerminalDevice::init() {
#if defined(_WIN32)
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE) {
        return false;
    }
    hIn_ = hIn;
    hOut_ = hOut;

    origInCp_ = GetConsoleCP();
    origOutCp_ = GetConsoleOutputCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    DWORD inMode = 0;
    DWORD outMode = 0;
    GetConsoleMode(hIn, &inMode);
    GetConsoleMode(hOut, &outMode);
    origInMode_ = inMode;
    origOutMode_ = outMode;

    // Enable Virtual Terminal Processing (VT100/ANSI) for output
    DWORD newOutMode = outMode | ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, newOutMode);

    // Set raw mode for input (disable line buffering and local echo)
    DWORD newInMode = inMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    newInMode |= ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_WINDOW_INPUT;
    SetConsoleMode(hIn, newInMode);

    rawModeActive_ = true;
    return true;
#else
    termios* orig = new termios();
    if (tcgetattr(STDIN_FILENO, orig) == -1) {
        delete orig;
        return false;
    }
    origTermios_ = orig;

    termios raw = *orig;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        return false;
    }

    rawModeActive_ = true;
    return true;
#endif
}

void TerminalDevice::shutdown() {
    if (!rawModeActive_) return;

    if (mouseTrackingActive_) {
        disableMouseTracking();
    }
    if (alternateScreenActive_) {
        leaveAlternateScreen();
    }
    showCursor(true);
    flush();

#if defined(_WIN32)
    if (hOut_) {
        SetConsoleMode(static_cast<HANDLE>(hOut_), origOutMode_);
        SetConsoleOutputCP(origOutCp_);
    }
    if (hIn_) {
        SetConsoleMode(static_cast<HANDLE>(hIn_), origInMode_);
        SetConsoleCP(origInCp_);
    }
#else
    if (origTermios_) {
        termios* orig = static_cast<termios*>(origTermios_);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
        delete orig;
        origTermios_ = nullptr;
    }
#endif

    rawModeActive_ = false;
}

bool TerminalDevice::getWindowSize(int& cols, int& rows) {
#if defined(_WIN32)
    if (!hOut_) return false;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(static_cast<HANDLE>(hOut_), &csbi)) {
        cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return true;
    }
#else
    winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        cols = ws.ws_col;
        rows = ws.ws_row;
        return true;
    }
#endif
    cols = 80;
    rows = 24;
    return false;
}

size_t TerminalDevice::readInput(char* buffer, size_t maxBytes) {
    if (!buffer || maxBytes == 0) return 0;
#if defined(_WIN32)
    HANDLE hIn = static_cast<HANDLE>(hIn_);
    if (!hIn) return 0;

    DWORD eventsAvailable = 0;
    if (!GetNumberOfConsoleInputEvents(hIn, &eventsAvailable) || eventsAvailable == 0) {
        return 0;
    }

    DWORD bytesRead = 0;
    // ReadFile in non-blocking / available mode
    if (ReadFile(hIn, buffer, static_cast<DWORD>(maxBytes), &bytesRead, nullptr)) {
        return static_cast<size_t>(bytesRead);
    }
    return 0;
#else
    ssize_t n = read(STDIN_FILENO, buffer, maxBytes);
    if (n > 0) return static_cast<size_t>(n);
    return 0;
#endif
}

void TerminalDevice::writeRaw(std::string_view data) {
    if (data.empty()) return;
#if defined(_WIN32)
    if (hOut_) {
        DWORD written = 0;
        WriteFile(static_cast<HANDLE>(hOut_), data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
        return;
    }
#else
    ssize_t ret = write(STDOUT_FILENO, data.data(), data.size());
    (void)ret;
    return;
#endif
    std::cout << data;
}

void TerminalDevice::flush() {
#if !defined(_WIN32)
    fsync(STDOUT_FILENO);
#endif
    std::fflush(stdout);
}

void TerminalDevice::enterAlternateScreen() {
    writeRaw("\x1b[?1049h\x1b[H");
    alternateScreenActive_ = true;
}

void TerminalDevice::leaveAlternateScreen() {
    writeRaw("\x1b[?1049l");
    alternateScreenActive_ = false;
}

void TerminalDevice::enableMouseTracking() {
    // SGR 1006 mouse reporting mode with button event reporting
    writeRaw("\x1b[?1000h\x1b[?1006h");
    mouseTrackingActive_ = true;
}

void TerminalDevice::disableMouseTracking() {
    writeRaw("\x1b[?1006l\x1b[?1000l");
    mouseTrackingActive_ = false;
}

void TerminalDevice::showCursor(bool show) {
    if (show) {
        writeRaw("\x1b[?25h");
    } else {
        writeRaw("\x1b[?25l");
    }
}

} // namespace eatsbits::tui
