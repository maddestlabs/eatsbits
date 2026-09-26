#include "eatsbits/tui/views/braille_scope.hpp"
#include <algorithm>

namespace eatsbits::tui {

void BrailleScopeView::render(CellSurface& surface, const Rect& area, const TelemetrySnapshot& telemetry) {
    if (area.width < 20 || area.height < 6) return;

    // Draw frame
    surface.drawBox(area, Color::Gray(), Color::Black(), false, "LIVE OSCILLOSCOPE & SPECTRUM");

    // Layout: Left portion is Braille Oscilloscope, Right portion is Spectrum + Meters
    int meterWidth = 14;
    int scopeWidth = area.width - 2 - meterWidth;
    if (scopeWidth < 10) {
        scopeWidth = area.width - 2;
        meterWidth = 0;
    }

    Rect scopeArea{area.x + 1, area.y + 1, scopeWidth, area.height - 2};
    renderOscilloscope(surface, scopeArea, telemetry.scopeSamples);

    if (meterWidth > 0) {
        Rect meterArea{area.x + 1 + scopeWidth, area.y + 1, meterWidth, area.height - 2};
        // Split meter area: top is Spectrum, bottom is Master Meters
        int specHeight = std::max(2, meterArea.height - 4);
        Rect specArea{meterArea.x, meterArea.y, meterArea.width, specHeight};
        Rect vuArea{meterArea.x, meterArea.y + specHeight, meterArea.width, meterArea.height - specHeight};

        renderSpectrum(surface, specArea, telemetry.spectrumBands);
        renderMasterMeters(surface, vuArea, telemetry.peakL, telemetry.peakR, telemetry.peakHoldL, telemetry.peakHoldR);
    }
}

void BrailleScopeView::renderOscilloscope(CellSurface& surface, const Rect& area, const std::vector<float>& samples) {
    surface.clearBrailleArea(area, Color::Black());

    const int subW = area.width * 2;
    const int subH = area.height * 4;
    const int midY = subH / 2;
    const int startSubX = area.x * 2;
    const int startSubY = area.y * 4;

    // Draw center zero-axis with dim dots
    for (int x = 0; x < subW; x += 4) {
        surface.setBrailleDot(startSubX + x, startSubY + midY, true, Color::DarkGray(), Color::Black());
    }

    if (samples.size() < 2) return;

    // Plot waveform using continuous Braille lines
    int prevX = 0;
    float firstSample = std::clamp(samples[0], -1.0f, 1.0f);
    int prevY = std::clamp(midY - static_cast<int>(firstSample * (midY - 1)), 0, subH - 1);

    for (int x = 1; x < subW; ++x) {
        size_t sampleIdx = (static_cast<size_t>(x) * (samples.size() - 1)) / static_cast<size_t>(subW - 1);
        float s = std::clamp(samples[sampleIdx], -1.0f, 1.0f);
        int curY = std::clamp(midY - static_cast<int>(s * (midY - 1)), 0, subH - 1);

        surface.drawBrailleLine(startSubX + prevX, startSubY + prevY,
                                startSubX + x, startSubY + curY,
                                Color::CyberCyan(), Color::Black());
        prevX = x;
        prevY = curY;
    }
}

void BrailleScopeView::renderSpectrum(CellSurface& surface, const Rect& area, const std::array<float, 16>& bands) {
    if (area.width < 4 || area.height < 2) return;

    // Block elements for bars
    static const char32_t blocks[9] = {
        ' ', 0x2581, 0x2582, 0x2583, 0x2584, 0x2585, 0x2586, 0x2587, 0x2588
    };

    surface.drawText(area.x, area.y, "FFT SPECTRUM", Color::LightGray(), Color::Black());

    int numBars = std::min(static_cast<int>(area.width), 16);
    int barY = area.y + 1;
    int maxBarH = area.height - 1;

    for (int b = 0; b < numBars; ++b) {
        float energy = bands[static_cast<size_t>(b)];
        float totalRows = energy * static_cast<float>(maxBarH);

        for (int h = 0; h < maxBarH; ++h) {
            int row = barY + maxBarH - 1 - h;
            float rowFrac = totalRows - static_cast<float>(h);
            Color barColor = (h > maxBarH * 0.7f) ? Color::MeterRed() :
                             (h > maxBarH * 0.4f) ? Color::MeterYellow() : Color::NeonGreen();

            if (rowFrac >= 1.0f) {
                surface.setCell(area.x + b, row, Cell{blocks[8], barColor, Color::Black(), 0});
            } else if (rowFrac > 0.1f) {
                int p = std::clamp(static_cast<int>(rowFrac * 8.0f), 1, 8);
                surface.setCell(area.x + b, row, Cell{blocks[p], barColor, Color::Black(), 0});
            } else {
                surface.setCell(area.x + b, row, Cell{' ', Color::DarkGray(), Color::Black(), 0});
            }
        }
    }
}

void BrailleScopeView::renderMasterMeters(CellSurface& surface, const Rect& area, float l, float r, float holdL, float holdR) {
    if (area.width < 6 || area.height < 2) return;

    int meterW = area.width - 3;
    if (meterW < 2) return;

    // L meter
    surface.drawText(area.x, area.y, "L", Color::White(), Color::Black(), static_cast<uint8_t>(TextAttr::Bold));
    surface.drawHorizontalMeter(area.x + 2, area.y, meterW, l, holdL);

    // R meter
    if (area.height > 1) {
        surface.drawText(area.x, area.y + 1, "R", Color::White(), Color::Black(), static_cast<uint8_t>(TextAttr::Bold));
        surface.drawHorizontalMeter(area.x + 2, area.y + 1, meterW, r, holdR);
    }
}

} // namespace eatsbits::tui
