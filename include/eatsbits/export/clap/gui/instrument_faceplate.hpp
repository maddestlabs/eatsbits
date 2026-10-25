#ifndef EATS_EXPORT_CLAP_GUI_INSTRUMENT_FACEPLATE_HPP
#define EATS_EXPORT_CLAP_GUI_INSTRUMENT_FACEPLATE_HPP

#include "instrument_gui_desc.hpp"
#include "software_canvas.hpp"
#include <clap/clap.h>
#include <array>
#include <string>
#include <vector>

namespace eatsbits::export_clap::gui {

class IParamProvider {
public:
    virtual ~IParamProvider() = default;
    [[nodiscard]] virtual double getParamValue(uint32_t id) const noexcept = 0;
    virtual void setParamValue(uint32_t id, double val) noexcept = 0;
    virtual bool valueToText(uint32_t id, double val, char* buf, uint32_t cap) const noexcept = 0;
};

/**
 * Universal instrument faceplate renderer.
 * Automatically renders any declarative Eatsbits instrument GUI (def gui():)
 * with authentic hardware knobs, selectors, switches, and dividers.
 */
class InstrumentFaceplate {
public:
    InstrumentFaceplate(IParamProvider* paramProvider,
                        const clap_host* host,
                        InstrumentGuiDesc desc) noexcept;

    void setHost(const clap_host* host) noexcept { host_ = host; }

    void render(SoftwareCanvas& canvas) noexcept;

    bool onMouseDown(int x, int y, bool isShift) noexcept;
    bool onMouseMove(int x, int y, bool isLeftDown, bool isShift) noexcept;
    bool onMouseUp(int x, int y) noexcept;
    bool onMouseLeave() noexcept;

    /// 60 Hz timer update. Returns true if visual state changed and window should repaint.
    bool onTimer() noexcept;

    [[nodiscard]] bool isDirty() const noexcept { return isDirty_; }
    void markDirty() noexcept { isDirty_ = true; }

    [[nodiscard]] const InstrumentGuiDesc& desc() const noexcept { return desc_; }

private:
    struct PlacedWidget {
        WidgetType type{WidgetType::Knob};
        uint32_t paramId{0};
        const char* label{""};
        KnobStyle style{KnobStyle::Potentiometer};
        double minValue{0.0};
        double maxValue{1.0};
        double defaultValue{0.0};
        bool isStepped{false};

        float cx{0.0f};
        float cy{0.0f};
        float radius{0.0f};
        float w{0.0f};
        float h{0.0f};
    };

    void renderPotentiometerKnob(SoftwareCanvas& canvas, const PlacedWidget& pw,
                                float normVal, bool isActive, bool isHovered,
                                const char* readout) noexcept;

    void renderSelectorKnob(SoftwareCanvas& canvas, const PlacedWidget& pw,
                           float normVal, bool isActive, bool isHovered,
                           const char* readout) noexcept;

    void renderCreamFlutedKnob(SoftwareCanvas& canvas, const PlacedWidget& pw,
                               float normVal, bool isActive, bool isHovered,
                               const char* readout) noexcept;

    void renderSwitch(SoftwareCanvas& canvas, const PlacedWidget& pw,
                      bool isOn, bool isHovered) noexcept;

    void syncParamsFromHost() noexcept;
    void sendParamChange(uint32_t paramId, double value) noexcept;

    IParamProvider* paramProvider_{nullptr};
    const clap_host* host_{nullptr};
    InstrumentGuiDesc desc_;

    std::vector<double> cachedParams_;
    std::vector<PlacedWidget> placedWidgets_;

    int hoveredIndex_{-1};
    int activeIndex_{-1};
    int dragStartY_{0};
    double dragStartVal_{0.0};

    bool isDirty_{true};
    uint32_t timerTick_{0};
};

} // namespace eatsbits::export_clap::gui

#endif // EATS_EXPORT_CLAP_GUI_INSTRUMENT_FACEPLATE_HPP
