#pragma once

#include <string>
#include <string_view>
#include <cstddef>

namespace eatsbits::tui {

class TerminalDevice {
public:
    TerminalDevice();
    ~TerminalDevice();

    bool init();
    void shutdown();

    bool getWindowSize(int& cols, int& rows);
    size_t readInput(char* buffer, size_t maxBytes);
    bool waitForInput(int timeoutMs);
    void writeRaw(std::string_view data);
    void flush();

    void enterAlternateScreen();
    void leaveAlternateScreen();
    void enableMouseTracking();
    void disableMouseTracking();
    void showCursor(bool show);

    bool isRawMode() const { return rawModeActive_; }

private:
    bool rawModeActive_{false};
    bool alternateScreenActive_{false};
    bool mouseTrackingActive_{false};

#if defined(_WIN32)
    void* hIn_{nullptr};
    void* hOut_{nullptr};
    unsigned long origInMode_{0};
    unsigned long origOutMode_{0};
    unsigned int origInCp_{0};
    unsigned int origOutCp_{0};
#else
    void* origTermios_{nullptr};
#endif
};

} // namespace eatsbits::tui
