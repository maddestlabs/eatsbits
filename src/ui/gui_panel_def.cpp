#include "eatsbits/ui/gui_panel_def.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace eatsbits::ui {

void drawGuiFaceplate(BatchRenderer2D& r,
                      GuiPanelDef& panel,
                      const Rect2D& rect,
                      const ThemeTokens& theme,
                      const float* scopeBuffer,
                      size_t scopeBufferCount,
                      int draggingRow,
                      int draggingWidget) {
    float fpX = rect.x;
    float fpY = rect.y;
    float fpW = rect.w;
    float fpH = rect.h;
    panel.bounds = rect;

    // 1. Chassis background style
    float cr = 0.14f, cg = 0.16f, cb = 0.20f;
    switch (panel.chassisStyle) {
        case GuiChassisStyle::DarkChassis: cr = 0.13f; cg = 0.14f; cb = 0.18f; break;
        case GuiChassisStyle::PcbGreen:    cr = 0.06f; cg = 0.24f; cb = 0.12f; break;
        case GuiChassisStyle::MinimalWhite:cr = 0.88f; cg = 0.90f; cb = 0.94f; break;
        case GuiChassisStyle::Silver:      cr = 0.76f; cg = 0.77f; cb = 0.80f; break;
        case GuiChassisStyle::Snes:        cr = 0.78f; cg = 0.77f; cb = 0.75f; break;
        case GuiChassisStyle::Grunge:      cr = 0.18f; cg = 0.14f; cb = 0.12f; break;
        case GuiChassisStyle::Walnut:      cr = 0.28f; cg = 0.17f; cb = 0.10f; break;
        case GuiChassisStyle::Rosewood:    cr = 0.22f; cg = 0.08f; cb = 0.06f; break;
        case GuiChassisStyle::BrushedSteel:cr = 0.25f; cg = 0.27f; cb = 0.32f; break;
        case GuiChassisStyle::Carbon:      cr = 0.08f; cg = 0.09f; cb = 0.11f; break;
    }

    drawRoundedRect(r, fpX, fpY, fpW, fpH, panel.cornerRadius, cr, cg, cb, 1.0f);
    drawRoundedRectOutline(r, fpX, fpY, fpW, fpH, panel.cornerRadius,
                           panel.accentColor.r, panel.accentColor.g, panel.accentColor.b, 0.75f, 1.8f);

    // 2. Vintage Wood Cheeks on sides
    if (panel.woodCheeks) {
        drawRoundedRect(r, fpX - 12.0f, fpY, 12.0f, fpH, 4.0f, 0.28f, 0.14f, 0.08f, 1.0f);
        drawLine(r, fpX - 2.0f, fpY + 4.0f, fpX - 2.0f, fpY + fpH - 4.0f, 0.38f, 0.20f, 0.12f, 0.85f, 1.0f);
        drawRoundedRect(r, fpX + fpW, fpY, 12.0f, fpH, 4.0f, 0.28f, 0.14f, 0.08f, 1.0f);
        drawLine(r, fpX + fpW + 2.0f, fpY + 4.0f, fpX + fpW + 2.0f, fpY + fpH - 4.0f, 0.38f, 0.20f, 0.12f, 0.85f, 1.0f);
    }

    // 3. Corner mounting screws
    auto drawCornerScrew = [&](float sx, float sy) {
        drawCircle(r, sx, sy, 4.5f, 0.35f, 0.38f, 0.45f, 1.0f);
        drawCircle(r, sx, sy, 3.2f, 0.20f, 0.22f, 0.26f, 1.0f);
        drawLine(r, sx - 2.5f, sy, sx + 2.5f, sy, 0.45f, 0.48f, 0.55f, 1.0f, 1.2f);
    };
    drawCornerScrew(fpX + 10.0f, fpY + 10.0f);
    drawCornerScrew(fpX + fpW - 10.0f, fpY + 10.0f);
    drawCornerScrew(fpX + 10.0f, fpY + fpH - 10.0f);
    drawCornerScrew(fpX + fpW - 10.0f, fpY + fpH - 10.0f);

    // 4. Machined Header banner
    float headerH = 44.0f;
    drawRoundedRect(r, fpX + 4.0f, fpY + 4.0f, fpW - 8.0f, headerH, 6.0f, 0.08f, 0.09f, 0.12f, 0.85f);
    drawCircle(r, fpX + 20.0f, fpY + 22.0f, 8.0f, panel.accentColor.r, panel.accentColor.g, panel.accentColor.b, 0.25f);
    drawCircle(r, fpX + 20.0f, fpY + 22.0f, 5.0f, panel.accentColor.r, panel.accentColor.g, panel.accentColor.b, 1.0f);
    drawText(r, panel.title, fpX + 34.0f, fpY + 14.0f, 13.0f, 0.95f, 0.95f, 0.95f, 1.0f);
    drawText(r, panel.subtitle, fpX + 34.0f, fpY + 29.0f, 9.5f, panel.accentColor.r, panel.accentColor.g, panel.accentColor.b, 0.9f);

    // 5. Render Rows and Widgets
    float rowStartY = fpY + headerH + 16.0f;
    float rowH = (fpH - headerH - 32.0f) / std::max(1, static_cast<int>(panel.rows.size()));

    for (size_t rIdx = 0; rIdx < panel.rows.size(); ++rIdx) {
        auto& row = panel.rows[rIdx];
        float ry = rowStartY + (rIdx * rowH);
        row.bounds = Rect2D{fpX + 12.0f, ry, fpW - 24.0f, rowH - 8.0f};

        size_t wCount = row.widgets.size();
        if (wCount == 0) continue;

        float colW = row.bounds.w / static_cast<float>(wCount);

        for (size_t wIdx = 0; wIdx < wCount; ++wIdx) {
            auto& w = row.widgets[wIdx];
            float wx = row.bounds.x + (wIdx * colW);
            float wy = ry + 4.0f;
            w.bounds = Rect2D{wx + 6.0f, wy, colW - 12.0f, rowH - 16.0f};

            float cx = w.bounds.x + (w.bounds.w * 0.5f);
            float cy = w.bounds.y + (w.bounds.h * 0.42f);
            bool isDragging = (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(wIdx));

            // Render widget based on type
            if (w.type == GuiWidgetType::Knob) {
                float rad = std::clamp(w.size * 0.38f, 22.0f, 44.0f);

                // Knob style variations with authentic 3D radial gradients
                float offX = -0.25f * rad;
                float offY = -0.30f * rad;

                if (w.knobStyle == GuiKnobStyle::CreamFluted) {
                    // Cream vintage fluted knob with warm specular gradient
                    drawCircle(r, cx, cy, rad + 3.0f, 0.25f, 0.22f, 0.18f, 0.6f);
                    drawCircleRadial3StopGradient(r, cx, cy, rad,
                                                  Color(0.97f, 0.95f, 0.88f, 1.0f),
                                                  Color(0.86f, 0.82f, 0.72f, 1.0f),
                                                  Color(0.62f, 0.58f, 0.48f, 1.0f),
                                                  offX, offY, 0.50f, 32);
                    drawCircleOutline(r, cx, cy, rad, 0.45f, 0.42f, 0.35f, 0.8f, 1.0f);
                } else if (w.knobStyle == GuiKnobStyle::BakeliteSkirt) {
                    // Classic dark bakelite with satin luster
                    drawCircle(r, cx, cy, rad + 5.0f, 0.06f, 0.06f, 0.08f, 1.0f);
                    drawCircleOutline(r, cx, cy, rad + 5.0f, 0.18f, 0.18f, 0.22f, 0.6f, 1.0f);
                    drawCircleRadial3StopGradient(r, cx, cy, rad,
                                                  Color(0.38f, 0.38f, 0.44f, 1.0f),
                                                  Color(0.20f, 0.20f, 0.24f, 1.0f),
                                                  Color(0.08f, 0.08f, 0.10f, 1.0f),
                                                  offX, offY, 0.50f, 32);
                    drawCircleOutline(r, cx, cy, rad, 0.05f, 0.05f, 0.07f, 0.9f, 1.0f);
                } else if (w.knobStyle == GuiKnobStyle::AnodizedKnurled) {
                    // Anodized aluminum knurled finish
                    drawCircle(r, cx, cy, rad + 3.0f, 0.14f, 0.15f, 0.18f, 1.0f);
                    drawCircleOutline(r, cx, cy, rad + 3.0f, 0.42f, 0.45f, 0.52f, 0.7f, 1.0f);
                    drawCircleRadial3StopGradient(r, cx, cy, rad,
                                                  Color(0.55f, 0.58f, 0.66f, 1.0f),
                                                  Color(0.26f, 0.28f, 0.34f, 1.0f),
                                                  Color(0.12f, 0.13f, 0.16f, 1.0f),
                                                  offX, offY, 0.50f, 32);
                    drawCircleOutline(r, cx, cy, rad, 0.45f, 0.48f, 0.56f, 0.5f, 1.0f);
                } else if (w.knobStyle == GuiKnobStyle::TwoToneStepped) {
                    // Two-tone stepped collar with contrasting inner brushed face
                    drawCircle(r, cx, cy, rad + 4.0f, 0.06f, 0.07f, 0.09f, 1.0f);
                    drawCircleOutline(r, cx, cy, rad + 4.0f, 0.25f, 0.27f, 0.32f, 0.8f, 1.0f);
                    drawCircleRadial3StopGradient(r, cx, cy, rad - 2.0f,
                                                  Color(0.96f, 0.97f, 0.99f, 1.0f),
                                                  Color(0.80f, 0.82f, 0.88f, 1.0f),
                                                  Color(0.52f, 0.54f, 0.62f, 1.0f),
                                                  offX, offY, 0.45f, 32);
                    drawCircleOutline(r, cx, cy, rad - 2.0f, 0.35f, 0.38f, 0.44f, 0.7f, 1.0f);
                } else if (w.knobStyle == GuiKnobStyle::Tb303Halo) {
                    // Roland TB-303 silver satin dial with glowing halo
                    drawCircle(r, cx, cy, rad + 6.0f, w.accentColor.r, w.accentColor.g, w.accentColor.b, 0.28f);
                    drawCircleOutline(r, cx, cy, rad + 6.0f, w.accentColor.r, w.accentColor.g, w.accentColor.b, 0.65f, 1.5f);
                    drawCircleRadial3StopGradient(r, cx, cy, rad,
                                                  Color(0.48f, 0.50f, 0.56f, 1.0f),
                                                  Color(0.24f, 0.26f, 0.31f, 1.0f),
                                                  Color(0.10f, 0.11f, 0.14f, 1.0f),
                                                  offX, offY, 0.50f, 32);
                    drawCircleOutline(r, cx, cy, rad, 0.12f, 0.13f, 0.16f, 0.8f, 1.0f);
                } else {
                    // Standard Studio pot with beveled ring
                    drawCircle(r, cx, cy, rad + 2.0f, 0.08f, 0.09f, 0.12f, 1.0f);
                    drawCircleOutline(r, cx, cy, rad + 2.0f, 0.22f, 0.24f, 0.30f, 0.6f, 1.0f);
                    drawCircleRadial3StopGradient(r, cx, cy, rad,
                                                  Color(0.45f, 0.48f, 0.55f, 1.0f),
                                                  Color(0.22f, 0.24f, 0.30f, 1.0f),
                                                  Color(0.10f, 0.11f, 0.14f, 1.0f),
                                                  offX, offY, 0.50f, 32);
                    drawCircleOutline(r, cx, cy, rad, 0.08f, 0.09f, 0.11f, 0.7f, 1.0f);
                }

                // Arc indicator around knob
                constexpr float minA = -2.35619449f;
                constexpr float maxA = 2.35619449f;
                float norm = std::clamp((w.currentVal - w.minVal) / std::max(0.001f, w.maxVal - w.minVal), 0.0f, 1.0f);
                float curA = minA + (norm * (maxA - minA));

                r.drawArc(cx, cy, rad + 3.0f, minA, maxA, 0.22f, 0.24f, 0.30f, 0.6f, 2.0f);
                if (norm > 0.01f) {
                    Color needleCol = isDragging ? theme.highlight : w.accentColor;
                    r.drawArc(cx, cy, rad + 3.0f, minA, curA, needleCol.r, needleCol.g, needleCol.b, 0.95f, 2.5f);
                }

                // Needle pointer
                float nx = cx + std::sin(curA) * (rad - 3.0f);
                float ny = cy - std::cos(curA) * (rad - 3.0f);
                Color needleCol = isDragging ? theme.highlight : w.accentColor;
                drawLine(r, cx, cy, nx, ny, needleCol.r, needleCol.g, needleCol.b, 1.0f, 2.5f);
                drawCircle(r, cx, cy, 3.5f, 0.20f, 0.22f, 0.25f, 1.0f);

                // Label and readout badge
                drawCenteredText(r, w.label, w.bounds.x, cy + rad + 6.0f, w.bounds.w, 14.0f, 9.0f,
                                 isDragging ? theme.highlight.r : 0.9f,
                                 isDragging ? theme.highlight.g : 0.92f,
                                 isDragging ? theme.highlight.b : 0.96f, 1.0f);
                std::ostringstream ss;
                if (!w.unit.empty()) {
                    ss << std::fixed << std::setprecision(1) << w.currentVal << " " << w.unit;
                } else {
                    int pct = static_cast<int>(std::round(norm * 100.0f));
                    ss << pct << "%";
                }
                float badgeW = std::clamp(static_cast<float>(ss.str().size()) * 7.5f + 16.0f, 48.0f, 74.0f);
                float badgeH = 16.0f;
                float badgeX = cx - (badgeW * 0.5f);
                float badgeY = cy + rad + 22.0f;
                drawRoundedRect(r, badgeX, badgeY, badgeW, badgeH, 3.0f, 0.07f, 0.08f, 0.10f, 0.85f);
                drawRoundedRectOutline(r, badgeX, badgeY, badgeW, badgeH, 3.0f, 0.22f, 0.25f, 0.32f, 0.6f, 1.0f);
                drawCenteredText(r, ss.str(), badgeX, badgeY + 1.0f, badgeW, badgeH, 8.0f,
                                 isDragging ? theme.highlight.r : w.accentColor.r,
                                 isDragging ? theme.highlight.g : w.accentColor.g,
                                 isDragging ? theme.highlight.b : w.accentColor.b, 1.0f);
            } else if (w.type == GuiWidgetType::Slider) {
                float trkW = w.bounds.w - 24.0f;
                float trkH = 6.0f;
                float trkX = cx - (trkW * 0.5f);
                float trkY = cy;

                drawRoundedRect(r, trkX, trkY, trkW, trkH, 3.0f, 0.06f, 0.07f, 0.09f, 1.0f);
                float norm = std::clamp((w.currentVal - w.minVal) / std::max(0.001f, w.maxVal - w.minVal), 0.0f, 1.0f);
                drawRoundedRect(r, trkX, trkY, trkW * norm, trkH, 3.0f, w.accentColor.r * 0.8f, w.accentColor.g * 0.8f, w.accentColor.b * 0.8f, 1.0f);

                float thumbX = trkX + (trkW * norm);
                drawRoundedRect(r, thumbX - 8.0f, trkY - 8.0f, 16.0f, 22.0f, 3.0f, 0.85f, 0.88f, 0.95f, 1.0f);
                drawLine(r, thumbX, trkY - 6.0f, thumbX, trkY + 12.0f, 0.2f, 0.2f, 0.2f, 1.0f, 1.5f);

                drawCenteredText(r, w.label, w.bounds.x, trkY + 18.0f, w.bounds.w, 14.0f, 8.5f, 0.9f, 0.92f, 0.96f, 1.0f);
            } else if (w.type == GuiWidgetType::ToggleSwitch) {
                drawRoundedRect(r, cx - 14.0f, cy - 14.0f, 28.0f, 28.0f, 4.0f, 0.08f, 0.09f, 0.12f, 1.0f);
                bool isOn = (w.currentVal > 0.5f);
                drawCircle(r, cx, isOn ? cy - 6.0f : cy + 6.0f, 7.0f, 0.85f, 0.88f, 0.92f, 1.0f);
                drawCenteredText(r, w.label, w.bounds.x, cy + 18.0f, w.bounds.w, 14.0f, 8.5f, 0.9f, 0.92f, 0.96f, 1.0f);
            } else if (w.type == GuiWidgetType::NixieDisplay) {
                drawRoundedRect(r, cx - 36.0f, cy - 16.0f, 72.0f, 32.0f, 4.0f, 0.04f, 0.02f, 0.01f, 1.0f);
                drawRoundedRectOutline(r, cx - 36.0f, cy - 16.0f, 72.0f, 32.0f, 4.0f, 1.0f, 0.45f, 0.0f, 0.8f, 1.2f);
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(1) << w.currentVal;
                drawCenteredText(r, ss.str(), cx - 36.0f, cy - 10.0f, 72.0f, 20.0f, 12.0f, 1.0f, 0.55f, 0.1f, 1.0f);
                drawCenteredText(r, w.label, w.bounds.x, cy + 20.0f, w.bounds.w, 14.0f, 8.5f, 0.9f, 0.92f, 0.96f, 1.0f);
            } else if (w.type == GuiWidgetType::VuMeter) {
                drawRoundedRect(r, cx - 30.0f, cy - 14.0f, 60.0f, 28.0f, 3.0f, 0.06f, 0.07f, 0.09f, 1.0f);
                for (int seg = 0; seg < 8; ++seg) {
                    float sx = cx - 25.0f + (seg * 6.5f);
                    Color segCol = (seg < 5) ? Color{0.0f, 0.95f, 0.45f} : ((seg < 7) ? Color{1.0f, 0.85f, 0.0f} : Color{1.0f, 0.2f, 0.2f});
                    drawRect(r, sx, cy - 8.0f, 5.0f, 16.0f, segCol.r, segCol.g, segCol.b, (seg <= 5) ? 1.0f : 0.2f);
                }
                drawCenteredText(r, w.label, w.bounds.x, cy + 18.0f, w.bounds.w, 14.0f, 8.5f, 0.9f, 0.92f, 0.96f, 1.0f);
            } else if (w.type == GuiWidgetType::ScopeScreen) {
                float sw = w.bounds.w - 12.0f;
                float sh = w.bounds.h - 16.0f;
                float sx = cx - sw * 0.5f;
                float sy = cy - sh * 0.5f;
                drawRoundedRect(r, sx, sy, sw, sh, 4.0f, 0.03f, 0.06f, 0.04f, 1.0f);
                drawRoundedRectOutline(r, sx, sy, sw, sh, 4.0f, 0.15f, 0.85f, 0.35f, 0.8f, 1.2f);
                float midY = sy + sh * 0.5f;
                drawLine(r, sx + 4.0f, midY, sx + sw - 4.0f, midY, 0.10f, 0.45f, 0.20f, 0.45f, 1.0f);
                if (scopeBuffer && scopeBufferCount > 0) {
                    constexpr int kPts = 32;
                    float prevX = sx + 4.0f;
                    float prevY = midY - scopeBuffer[0] * (sh * 0.4f);
                    for (int i = 1; i < kPts; ++i) {
                        size_t sIdx = (static_cast<size_t>(i) * scopeBufferCount) / static_cast<size_t>(kPts);
                        float curX = sx + 4.0f + (static_cast<float>(i) / static_cast<float>(kPts - 1)) * (sw - 8.0f);
                        float curYPoint = midY - scopeBuffer[sIdx] * (sh * 0.4f);
                        drawLine(r, prevX, prevY, curX, curYPoint, 0.20f, 1.0f, 0.45f, 0.95f, 1.6f);
                        prevX = curX;
                        prevY = curYPoint;
                    }
                }
                drawCenteredText(r, w.label, w.bounds.x, sy + sh + 2.0f, w.bounds.w, 12.0f, 8.0f, 0.9f, 0.92f, 0.96f, 1.0f);
            } else {
                drawRoundedRect(r, cx - 32.0f, cy - 14.0f, 64.0f, 28.0f, 4.0f, 0.14f, 0.16f, 0.22f, 1.0f);
                drawCenteredText(r, w.label, cx - 32.0f, cy - 7.0f, 64.0f, 14.0f, 9.0f, 0.9f, 0.92f, 0.96f, 1.0f);
            }
        }
    }
}

} // namespace eatsbits::ui
