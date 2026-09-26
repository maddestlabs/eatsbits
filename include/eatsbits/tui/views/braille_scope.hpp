#pragma once

#include "eatsbits/tui/cell_surface.hpp"
#include "eatsbits/tui/audio_telemetry.hpp"

namespace eatsbits::tui {

class BrailleScopeView {
public:
    BrailleScopeView() = default;
    ~BrailleScopeView() = default;

    void render(CellSurface& surface, const Rect& area, const TelemetrySnapshot& telemetry);

private:
    void renderOscilloscope(CellSurface& surface, const Rect& area, const std::vector<float>& samples);
    void renderSpectrum(CellSurface& surface, const Rect& area, const std::array<float, 16>& bands);
    void renderMasterMeters(CellSurface& surface, const Rect& area, float l, float r, float holdL, float holdR);
};

} // namespace eatsbits::tui
