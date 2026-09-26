#include "eatsbits/tui/tui_app.hpp"
#include <iostream>

int main() {
    try {
        eatsbits::tui::TuiApp app;
        if (!app.init()) {
            std::cerr << "Failed to initialize Eatsbits TUI DAW." << std::endl;
            return 1;
        }

        app.run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Unhandled fatal error in Eatsbits TUI: " << ex.what() << std::endl;
        return 1;
    }
}
