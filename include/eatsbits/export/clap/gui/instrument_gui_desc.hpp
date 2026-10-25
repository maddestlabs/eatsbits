#ifndef EATS_EXPORT_CLAP_GUI_INSTRUMENT_GUI_DESC_HPP
#define EATS_EXPORT_CLAP_GUI_INSTRUMENT_GUI_DESC_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace eatsbits::export_clap::gui {

enum class WidgetType : uint8_t {
    Knob,
    Switch,
    Divider
};

enum class KnobStyle : uint8_t {
    Potentiometer, // Authentic acid potentiometer with dial hash ticks and sunken collet
    Selector,      // Stepped selector knob
    CreamFluted,   // Studio cream / fluted knob
    Standard       // Clean hardware knob
};

struct WidgetDesc {
    WidgetType type{WidgetType::Knob};
    uint32_t paramId{0};
    const char* paramName{""};
    const char* label{""};
    KnobStyle style{KnobStyle::Potentiometer};
    double minValue{0.0};
    double maxValue{1.0};
    double defaultValue{0.0};
    bool isStepped{false};
    float size{44.0f};
};

struct RowDesc {
    std::vector<WidgetDesc> widgets;
};

struct InstrumentGuiDesc {
    const char* title{"EATSBITS INSTRUMENT"};
    const char* subtitle{""};
    const char* background{"minimal_white"}; // "minimal_white", "dark", "aluminum"
    std::vector<RowDesc> rows;
};

} // namespace eatsbits::export_clap::gui

#endif // EATS_EXPORT_CLAP_GUI_INSTRUMENT_GUI_DESC_HPP
