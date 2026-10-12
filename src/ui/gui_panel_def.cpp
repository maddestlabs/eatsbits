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
        case GuiChassisStyle::DarkChassis:     cr = 0.13f; cg = 0.14f; cb = 0.18f; break;
        case GuiChassisStyle::PcbGreen:        cr = 0.06f; cg = 0.24f; cb = 0.12f; break;
        case GuiChassisStyle::MinimalWhite:    cr = 0.92f; cg = 0.93f; cb = 0.95f; break;
        case GuiChassisStyle::Silver:          cr = 0.76f; cg = 0.77f; cb = 0.80f; break;
        case GuiChassisStyle::Snes:            cr = 0.78f; cg = 0.77f; cb = 0.75f; break;
        case GuiChassisStyle::Grunge:          cr = 0.18f; cg = 0.14f; cb = 0.12f; break;
        case GuiChassisStyle::Walnut:          cr = 0.28f; cg = 0.17f; cb = 0.10f; break;
        case GuiChassisStyle::Rosewood:        cr = 0.22f; cg = 0.08f; cb = 0.06f; break;
        case GuiChassisStyle::BrushedSteel:    cr = 0.25f; cg = 0.27f; cb = 0.32f; break;
        case GuiChassisStyle::BrushedAluminum: cr = 0.75f; cg = 0.77f; cb = 0.82f; break;
        case GuiChassisStyle::MattePowderCoat: cr = 0.15f; cg = 0.16f; cb = 0.19f; break;
        case GuiChassisStyle::Bakelite:        cr = 0.32f; cg = 0.11f; cb = 0.06f; break;
        case GuiChassisStyle::CrinklePaint:    cr = 0.16f; cg = 0.16f; cb = 0.18f; break;
        case GuiChassisStyle::Carbon:          cr = 0.08f; cg = 0.09f; cb = 0.11f; break;
    }

    if (panel.chassisTint.has_value()) {
        cr = panel.chassisTint->r;
        cg = panel.chassisTint->g;
        cb = panel.chassisTint->b;
    }

    bool isLightChassis = (panel.chassisStyle == GuiChassisStyle::MinimalWhite);
    Color baseCol(cr, cg, cb);
    Color gradTop = isLightChassis ? baseCol : baseCol.lighten(0.06f);
    Color gradBot = isLightChassis ? baseCol.darken(0.04f) : baseCol.darken(0.08f);

    if (panel.cornerRadius > 0.0f) {
        r.drawRoundedRectGradient(fpX, fpY, fpW, fpH, panel.cornerRadius,
                                  gradTop.r, gradTop.g, gradTop.b,
                                  gradBot.r, gradBot.g, gradBot.b, 1.0f);
        Color rimCol = isLightChassis ? Color(0.68f, 0.71f, 0.76f) : baseCol.lighten(0.18f);
        drawRoundedRectOutline(r, fpX, fpY, fpW, fpH, panel.cornerRadius,
                               rimCol.r, rimCol.g, rimCol.b, 0.65f, 1.2f);
        drawRoundedRectOutline(r, fpX + 1.0f, fpY + 1.0f, fpW - 2.0f, fpH - 2.0f,
                               std::max(0.0f, panel.cornerRadius - 1.0f),
                               0.0f, 0.0f, 0.0f, 0.35f, 1.0f);
    } else {
        r.drawRectGradient(fpX, fpY, fpW, fpH,
                           gradTop.r, gradTop.g, gradTop.b,
                           gradBot.r, gradBot.g, gradBot.b, 1.0f);
        Color rimCol = isLightChassis ? Color(0.68f, 0.71f, 0.76f) : baseCol.lighten(0.18f);
        r.drawRectOutline(fpX, fpY, fpW, fpH,
                          rimCol.r, rimCol.g, rimCol.b, 0.65f, 1.2f);
    }

    // 2. Vintage Wood Cheeks on sides (skipped for clean minimal white or when disabled)
    if (panel.woodCheeks && !isLightChassis) {
        drawRoundedRect(r, fpX - 12.0f, fpY, 12.0f, fpH, 4.0f, 0.28f, 0.14f, 0.08f, 1.0f);
        drawLine(r, fpX - 2.0f, fpY + 4.0f, fpX - 2.0f, fpY + fpH - 4.0f, 0.38f, 0.20f, 0.12f, 0.85f, 1.0f);
        drawRoundedRect(r, fpX + fpW, fpY, 12.0f, fpH, 4.0f, 0.28f, 0.14f, 0.08f, 1.0f);
        drawLine(r, fpX + fpW + 2.0f, fpY + 4.0f, fpX + fpW + 2.0f, fpY + fpH - 4.0f, 0.38f, 0.20f, 0.12f, 0.85f, 1.0f);
    }

    // 3. Corner mounting screws (skipped for minimal white)
    if (!isLightChassis) {
        auto drawCornerScrew = [&](float sx, float sy) {
            drawCircle(r, sx, sy, 4.5f, 0.35f, 0.38f, 0.45f, 1.0f);
            drawCircle(r, sx, sy, 3.2f, 0.20f, 0.22f, 0.26f, 1.0f);
            drawLine(r, sx - 2.5f, sy, sx + 2.5f, sy, 0.45f, 0.48f, 0.55f, 1.0f, 1.2f);
        };
        drawCornerScrew(fpX + 10.0f, fpY + 10.0f);
        drawCornerScrew(fpX + fpW - 10.0f, fpY + 10.0f);
        drawCornerScrew(fpX + 10.0f, fpY + fpH - 10.0f);
        drawCornerScrew(fpX + fpW - 10.0f, fpY + fpH - 10.0f);
    }

    // 4. Machined Header banner (only if not hidden and not minimal white)
    float headerH = 0.0f;
    if (!panel.hideHeader && !isLightChassis) {
        headerH = 44.0f;
        drawRoundedRect(r, fpX + 4.0f, fpY + 4.0f, fpW - 8.0f, headerH, 6.0f, 0.08f, 0.09f, 0.12f, 0.85f);
        drawCircle(r, fpX + 20.0f, fpY + 22.0f, 8.0f, panel.accentColor.r, panel.accentColor.g, panel.accentColor.b, 0.25f);
        drawCircle(r, fpX + 20.0f, fpY + 22.0f, 5.0f, panel.accentColor.r, panel.accentColor.g, panel.accentColor.b, 1.0f);
        drawText(r, panel.title, fpX + 34.0f, fpY + 14.0f, 13.0f, 0.95f, 0.95f, 0.95f, 1.0f);
        drawText(r, panel.subtitle, fpX + 34.0f, fpY + 29.0f, 9.5f, panel.accentColor.r, panel.accentColor.g, panel.accentColor.b, 0.9f);
    }

    // 5. Render Rows and Widgets
    float topPadding = (headerH > 0.0f) ? (headerH + 12.0f) : 10.0f;
    float bottomPadding = 8.0f;
    float rowStartY = fpY + topPadding;
    float rowH = (fpH - topPadding - bottomPadding) / std::max(1, static_cast<int>(panel.rows.size()));

    auto drawSingleWidget = [&](GuiWidgetDef& w, bool isDragging) {
        float cx = w.bounds.x + (w.bounds.w * 0.5f);
        float cy = w.bounds.y + (w.bounds.h * 0.44f);

        constexpr float minA = -2.35619449f;
        constexpr float maxA = 2.35619449f;
        float norm = std::clamp((w.currentVal - w.minVal) / std::max(0.001f, w.maxVal - w.minVal), 0.0f, 1.0f);
        float curA = minA + (norm * (maxA - minA));

        if (w.type == GuiWidgetType::Divider) {
            // Vertical divider line between module sections
            float divX = w.bounds.x + (w.bounds.w * 0.5f);
            float linePad = std::clamp(w.bounds.h * 0.10f, 6.0f, 24.0f);
            float lineY1 = w.bounds.y + linePad;
            float lineY2 = w.bounds.y + w.bounds.h - linePad;
            float lineThick = std::clamp(w.bounds.w * 0.08f, 1.0f, 2.2f);
            drawLine(r, divX, lineY1, divX, lineY2,
                     isLightChassis ? 0.74f : 0.22f,
                     isLightChassis ? 0.77f : 0.24f,
                     isLightChassis ? 0.82f : 0.28f,
                     0.85f, lineThick);
            return;
        }

        if (w.type == GuiWidgetType::Knob) {
            if (w.knobStyle == GuiKnobStyle::Tb303SelectorSilver || w.knobStyle == GuiKnobStyle::Tb303SelectorBlack) {
                float maxRadW = (w.bounds.w * 0.5f) - 10.0f;
                float maxRadH = (w.bounds.h * 0.38f) - 10.0f;
                float rad = std::clamp(std::min(maxRadW, maxRadH) * 0.72f, 13.0f, 64.0f);
                float offX = -0.25f * rad;
                float offY = -0.30f * rad;
                bool isBlack = (w.knobStyle == GuiKnobStyle::Tb303SelectorBlack);

                float numDist = rad + std::clamp(rad * 0.34f, 9.0f, 22.0f);
                float numFontSize = std::clamp(rad * 0.24f, 6.5f, 14.0f);
                float numBoxW = numFontSize * 1.8f;
                float numBoxH = numFontSize * 1.4f;

                static constexpr const char* kStepStrings[11] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
                // 0-10 stepped selector dial ticks and numbers around perimeter
                for (int step = 0; step <= 10; ++step) {
                    float sAng = minA + (static_cast<float>(step) / 10.0f) * (maxA - minA);
                    float nx = cx + std::sin(sAng) * numDist;
                    float ny = cy - std::cos(sAng) * numDist;
                    drawCenteredText(r, kStepStrings[step], nx - numBoxW * 0.5f, ny - numBoxH * 0.5f, numBoxW, numBoxH, numFontSize,
                                     isLightChassis ? 0.25f : 0.80f,
                                     isLightChassis ? 0.28f : 0.82f,
                                     isLightChassis ? 0.32f : 0.86f, 0.95f);
                }

                float skirtExtra = std::clamp(rad * 0.10f, 2.0f, 5.0f);
                float ptrWidth = std::clamp(rad * 0.08f, 2.0f, 4.5f);
                float rivetR = std::clamp(rad * 0.10f, 2.2f, 5.5f);

                // Stepped knob cap
                if (isBlack) {
                    drawCircle(r, cx, cy, rad + skirtExtra, 0.08f, 0.08f, 0.10f, 1.0f);
                    drawCircleRadial3StopGradient(r, cx, cy, rad,
                                                  Color(0.26f, 0.27f, 0.30f, 1.0f),
                                                  Color(0.14f, 0.14f, 0.16f, 1.0f),
                                                  Color(0.06f, 0.06f, 0.08f, 1.0f),
                                                  offX, offY, 0.50f, 32);
                    drawCircleOutline(r, cx, cy, rad, 0.04f, 0.04f, 0.06f, 0.9f, 1.0f);
                    // White pointer line
                    float px = cx + std::sin(curA) * (rad - 1.5f);
                    float py = cy - std::cos(curA) * (rad - 1.5f);
                    drawLine(r, cx, cy, px, py, 0.92f, 0.94f, 0.96f, 1.0f, ptrWidth);
                    drawCircle(r, cx, cy, rivetR, 0.30f, 0.32f, 0.35f, 1.0f);
                } else {
                    drawCircle(r, cx, cy, rad + skirtExtra, 0.55f, 0.57f, 0.62f, 1.0f);
                    drawCircleRadial3StopGradient(r, cx, cy, rad,
                                                  Color(0.96f, 0.97f, 0.99f, 1.0f),
                                                  Color(0.80f, 0.82f, 0.86f, 1.0f),
                                                  Color(0.55f, 0.58f, 0.64f, 1.0f),
                                                  offX, offY, 0.50f, 32);
                    drawCircleOutline(r, cx, cy, rad, 0.40f, 0.42f, 0.48f, 0.85f, 1.0f);
                    // Dark pointer line
                    float px = cx + std::sin(curA) * (rad - 1.5f);
                    float py = cy - std::cos(curA) * (rad - 1.5f);
                    drawLine(r, cx, cy, px, py, 0.12f, 0.14f, 0.18f, 1.0f, ptrWidth);
                    drawCircle(r, cx, cy, rivetR, 0.20f, 0.22f, 0.26f, 1.0f);
                }

                // Label below
                float labelY = cy + rad + std::clamp(rad * 0.34f, 9.0f, 24.0f);
                float labelH = std::clamp(rad * 0.38f, 12.0f, 22.0f);
                float labelFontSize = std::clamp(rad * 0.28f, 8.5f, 17.0f);
                drawCenteredText(r, w.label, w.bounds.x, labelY, w.bounds.w, labelH, labelFontSize,
                                 isLightChassis ? 0.14f : 0.90f,
                                 isLightChassis ? 0.16f : 0.92f,
                                 isLightChassis ? 0.18f : 0.96f, 1.0f);

            } else if (w.knobStyle == GuiKnobStyle::Tb303Potentiometer) {
                float maxRadW = (w.bounds.w * 0.5f) - 6.0f;
                float maxRadH = (w.bounds.h * 0.40f) - 10.0f;
                float rad = std::clamp(std::min(maxRadW, maxRadH) * 0.75f, 14.0f, 68.0f);
                float offX = -0.25f * rad;
                float offY = -0.30f * rad;

                float tickIn = rad + std::clamp(rad * 0.12f, 3.0f, 8.0f);
                float tickOut = rad + std::clamp(rad * 0.28f, 6.5f, 18.0f);
                float tickWidth = std::clamp(rad * 0.05f, 1.2f, 2.8f);

                // Authentic 3D Roland TB-303 Silver Potentiometer with radial tick marks
                for (int t = 0; t <= 6; ++t) {
                    float tAng = minA + (static_cast<float>(t) / 6.0f) * (maxA - minA);
                    float tX1 = cx + std::sin(tAng) * tickIn;
                    float tY1 = cy - std::cos(tAng) * tickIn;
                    float tX2 = cx + std::sin(tAng) * tickOut;
                    float tY2 = cy - std::cos(tAng) * tickOut;
                    drawLine(r, tX1, tY1, tX2, tY2,
                             isLightChassis ? 0.18f : 0.70f,
                             isLightChassis ? 0.20f : 0.72f,
                             isLightChassis ? 0.24f : 0.76f, 0.9f, tickWidth);
                }

                // Drop shadow
                drawCircle(r, cx, cy + 2.0f, rad + 1.5f, 0.08f, 0.09f, 0.11f, 0.55f);

                // Silver knob body with 3-stop radial gradient
                drawCircleRadial3StopGradient(r, cx, cy, rad,
                                              Color(0.96f, 0.97f, 0.99f, 1.0f),
                                              Color(0.82f, 0.84f, 0.88f, 1.0f),
                                              Color(0.60f, 0.63f, 0.68f, 1.0f),
                                              offX, offY, 0.50f, 32);
                drawCircleOutline(r, cx, cy, rad, 0.45f, 0.47f, 0.52f, 0.85f, 1.0f);

                // Dark indicator line
                float ptrLen = rad * 0.82f;
                float px = cx + std::sin(curA) * ptrLen;
                float py = cy - std::cos(curA) * ptrLen;
                float ptrW = std::clamp(rad * 0.07f, 1.8f, 3.5f);
                drawLine(r, cx, cy, px, py, 0.10f, 0.12f, 0.15f, 1.0f, ptrW);

                // Center aluminum cap
                float capR = std::clamp(rad * 0.28f, 3.5f, 14.0f);
                drawCircle(r, cx, cy, capR, 0.72f, 0.74f, 0.78f, 1.0f);
                drawCircleOutline(r, cx, cy, capR, 0.40f, 0.42f, 0.46f, 0.7f, 0.8f);

                // Label below knob
                float labelY = cy + rad + std::clamp(rad * 0.22f, 6.0f, 18.0f);
                float labelH = std::clamp(rad * 0.35f, 12.0f, 20.0f);
                float labelFontSize = std::clamp(rad * 0.27f, 8.5f, 16.0f);
                drawCenteredText(r, w.label, w.bounds.x, labelY, w.bounds.w, labelH, labelFontSize,
                                 isLightChassis ? 0.14f : 0.90f,
                                 isLightChassis ? 0.16f : 0.92f,
                                 isLightChassis ? 0.18f : 0.96f, 1.0f);

            } else if (w.knobStyle == GuiKnobStyle::MiniPotCream) {
                float maxRadW = (w.bounds.w * 0.5f) - 4.0f;
                float maxRadH = (w.bounds.h * 0.42f) - 6.0f;
                float rad = std::clamp(std::min(maxRadW, maxRadH) * 0.75f, 10.0f, 44.0f);
                float offX = -0.25f * rad;
                float offY = -0.30f * rad;

                // Cream fluted mini-knob body
                drawCircleRadial3StopGradient(r, cx, cy, rad,
                                              Color(0.96f, 0.94f, 0.88f, 1.0f),
                                              Color(0.85f, 0.82f, 0.74f, 1.0f),
                                              Color(0.55f, 0.52f, 0.45f, 1.0f),
                                              offX, offY, 0.50f, 24);
                drawCircleOutline(r, cx, cy, rad, 0.40f, 0.38f, 0.32f, 0.8f, 1.0f);

                float ptrLen = rad * 0.82f;
                float px = cx + std::sin(curA) * ptrLen;
                float py = cy - std::cos(curA) * ptrLen;
                float ptrW = std::clamp(rad * 0.09f, 1.6f, 3.2f);
                drawLine(r, cx, cy, px, py, 0.16f, 0.14f, 0.12f, 1.0f, ptrW);

                float labelY = cy + rad + std::clamp(rad * 0.22f, 5.0f, 14.0f);
                float labelH = std::clamp(rad * 0.36f, 10.0f, 18.0f);
                float labelFontSize = std::clamp(rad * 0.30f, 8.0f, 15.0f);
                drawCenteredText(r, w.label, w.bounds.x, labelY, w.bounds.w, labelH, labelFontSize,
                                 isLightChassis ? 0.14f : 0.90f,
                                 isLightChassis ? 0.16f : 0.92f,
                                 isLightChassis ? 0.18f : 0.96f, 1.0f);

            } else {
                // CreamFluted, BakeliteSkirt, AnodizedKnurled, Tb303Halo, Standard
                float maxRadW = (w.bounds.w * 0.5f) - 6.0f;
                float maxRadH = (w.bounds.h * 0.42f) - 8.0f;
                float rad = std::clamp(std::min(maxRadW, maxRadH) * 0.78f, 12.0f, 60.0f);
                float offX = -0.25f * rad;
                float offY = -0.30f * rad;

                Color c1, c2, c3;
                if (w.knobStyle == GuiKnobStyle::BakeliteSkirt) {
                    c1 = Color(0.24f, 0.12f, 0.08f, 1.0f);
                    c2 = Color(0.14f, 0.07f, 0.04f, 1.0f);
                    c3 = Color(0.06f, 0.03f, 0.02f, 1.0f);
                } else if (w.knobStyle == GuiKnobStyle::AnodizedKnurled) {
                    c1 = Color(w.accentColor.r * 1.15f, w.accentColor.g * 1.15f, w.accentColor.b * 1.15f, 1.0f);
                    c2 = Color(w.accentColor.r * 0.70f, w.accentColor.g * 0.70f, w.accentColor.b * 0.70f, 1.0f);
                    c3 = Color(0.08f, 0.09f, 0.11f, 1.0f);
                } else if (w.knobStyle == GuiKnobStyle::Tb303Halo) {
                    float haloR = rad + std::clamp(rad * 0.22f, 4.0f, 10.0f);
                    drawCircleOutline(r, cx, cy, haloR, panel.accentColor.r, panel.accentColor.g, panel.accentColor.b, 0.45f, 2.0f);
                    c1 = Color(0.85f, 0.88f, 0.94f, 1.0f);
                    c2 = Color(0.65f, 0.68f, 0.75f, 1.0f);
                    c3 = Color(0.25f, 0.27f, 0.32f, 1.0f);
                } else if (w.knobStyle == GuiKnobStyle::CreamFluted) {
                    c1 = Color(0.96f, 0.94f, 0.88f, 1.0f);
                    c2 = Color(0.85f, 0.82f, 0.74f, 1.0f);
                    c3 = Color(0.55f, 0.52f, 0.45f, 1.0f);
                } else {
                    c1 = Color(0.35f, 0.38f, 0.45f, 1.0f);
                    c2 = Color(0.20f, 0.22f, 0.26f, 1.0f);
                    c3 = Color(0.10f, 0.11f, 0.13f, 1.0f);
                }

                drawCircleRadial3StopGradient(r, cx, cy, rad, c1, c2, c3, offX, offY, 0.50f, 28);
                drawCircleOutline(r, cx, cy, rad, isLightChassis ? 0.4f : 0.12f, isLightChassis ? 0.4f : 0.14f, isLightChassis ? 0.45f : 0.18f, 0.8f, 1.0f);

                float ptrLen = rad * 0.85f;
                float px = cx + std::sin(curA) * ptrLen;
                float py = cy - std::cos(curA) * ptrLen;
                Color ptrCol = (w.knobStyle == GuiKnobStyle::BakeliteSkirt) ? Color(0.95f, 0.95f, 0.95f, 1.0f) :
                               (w.knobStyle == GuiKnobStyle::CreamFluted ? Color(0.15f, 0.13f, 0.10f, 1.0f) : w.accentColor);
                float ptrW = std::clamp(rad * 0.08f, 1.8f, 3.5f);
                drawLine(r, cx, cy, px, py, ptrCol.r, ptrCol.g, ptrCol.b, 1.0f, ptrW);

                float labelY = cy + rad + std::clamp(rad * 0.22f, 6.0f, 18.0f);
                float labelH = std::clamp(rad * 0.35f, 12.0f, 20.0f);
                float labelFontSize = std::clamp(rad * 0.27f, 8.5f, 16.0f);
                drawCenteredText(r, w.label, w.bounds.x, labelY, w.bounds.w, labelH, labelFontSize,
                                 isLightChassis ? 0.14f : 0.90f,
                                 isLightChassis ? 0.16f : 0.92f,
                                 isLightChassis ? 0.18f : 0.96f, 1.0f);
            }

            if (isDragging) {
                drawCircleOutline(r, cx, cy, (w.bounds.h * 0.35f) + 4.0f, 0.0f, 0.95f, 1.0f, 0.9f, 2.0f);
            }

        } else if (w.type == GuiWidgetType::Slider) {
            float trkW = w.bounds.w * 0.75f;
            float trkH = std::clamp(w.bounds.h * 0.10f, 6.0f, 16.0f);
            float trkX = cx - (trkW * 0.5f);
            float trkY = cy;

            drawRoundedRect(r, trkX, trkY, trkW, trkH, 3.0f, 0.06f, 0.07f, 0.09f, 1.0f);
            drawRoundedRect(r, trkX, trkY, trkW * norm, trkH, 3.0f, w.accentColor.r * 0.8f, w.accentColor.g * 0.8f, w.accentColor.b * 0.8f, 1.0f);

            float thumbW = std::clamp(w.bounds.w * 0.08f, 14.0f, 26.0f);
            float thumbH = std::clamp(w.bounds.h * 0.25f, 20.0f, 44.0f);
            float thumbX = trkX + (trkW * norm);
            drawRoundedRect(r, thumbX - thumbW * 0.5f, trkY - thumbH * 0.35f, thumbW, thumbH, 3.0f, 0.85f, 0.88f, 0.95f, 1.0f);
            drawLine(r, thumbX, trkY - thumbH * 0.25f, thumbX, trkY + thumbH * 0.5f, 0.2f, 0.2f, 0.2f, 1.0f, 1.5f);

            float labelFontSize = std::clamp(w.bounds.h * 0.12f, 8.5f, 16.0f);
            drawCenteredText(r, w.label, w.bounds.x, trkY + thumbH * 0.6f, w.bounds.w, 14.0f, labelFontSize,
                             isLightChassis ? 0.14f : 0.9f,
                             isLightChassis ? 0.16f : 0.92f,
                             isLightChassis ? 0.18f : 0.96f, 1.0f);

        } else if (w.type == GuiWidgetType::ToggleSwitch) {
            // Authentic metal bat toggle switch with golden collar on dark pill bezel
            bool isOn = (w.currentVal > 0.5f);
            float maxW = (w.bounds.w * 0.5f) - 4.0f;
            float maxH = (w.bounds.h * 0.44f);
            float pillW = std::clamp(std::min(maxW, maxH * 0.55f), 18.0f, 50.0f);
            float pillH = pillW * 1.70f;
            float pillX = cx - (pillW * 0.5f);
            float pillY = cy - (pillH * 0.5f) + 2.0f;
            float pillCorner = pillW * 0.45f;

            // Dark recessed pill plate
            drawRoundedRect(r, pillX, pillY, pillW, pillH, pillCorner, 0.12f, 0.11f, 0.10f, 1.0f);
            drawRoundedRectOutline(r, pillX, pillY, pillW, pillH, pillCorner, 0.32f, 0.28f, 0.22f, 0.9f, 1.0f);

            // Golden/brass collar ring
            float collarR = pillW * 0.28f;
            float collarInR = pillW * 0.18f;
            drawCircle(r, cx, cy + 2.0f, collarR, 0.72f, 0.55f, 0.20f, 1.0f);
            drawCircle(r, cx, cy + 2.0f, collarInR, 0.15f, 0.13f, 0.10f, 1.0f);

            // Silver metal bat lever
            float tipOffset = pillH * 0.32f;
            float tipY = isOn ? (cy + 2.0f - tipOffset) : (cy + 2.0f + tipOffset);
            float leverWidth = std::clamp(pillW * 0.16f, 3.0f, 7.5f);
            drawLine(r, cx, cy + 2.0f, cx, tipY, 0.78f, 0.80f, 0.85f, 1.0f, leverWidth);
            // Gold/brass ball tip
            float ballR = std::clamp(pillW * 0.20f, 3.8f, 10.0f);
            drawCircle(r, cx, tipY, ballR, 0.88f, 0.70f, 0.25f, 1.0f);
            drawCircleOutline(r, cx, tipY, ballR, 0.40f, 0.30f, 0.10f, 0.8f, 1.0f);

            // Label ABOVE switch (matching Eatsbeats original layout)
            float labelY = pillY - std::clamp(pillH * 0.32f, 14.0f, 28.0f);
            float labelH = std::clamp(pillH * 0.28f, 12.0f, 22.0f);
            float labelFontSize = std::clamp(pillW * 0.38f, 7.5f, 16.0f);
            drawCenteredText(r, w.label, w.bounds.x, labelY, w.bounds.w, labelH, labelFontSize,
                             isLightChassis ? 0.20f : 0.85f,
                             isLightChassis ? 0.22f : 0.88f,
                             isLightChassis ? 0.26f : 0.92f, 1.0f);

        } else if (w.type == GuiWidgetType::NixieDisplay) {
            float nixW = std::clamp(w.bounds.w * 0.65f, 60.0f, 160.0f);
            float nixH = std::clamp(w.bounds.h * 0.35f, 28.0f, 70.0f);
            drawRoundedRect(r, cx - nixW * 0.5f, cy - nixH * 0.5f, nixW, nixH, 4.0f, 0.04f, 0.02f, 0.01f, 1.0f);
            drawRoundedRectOutline(r, cx - nixW * 0.5f, cy - nixH * 0.5f, nixW, nixH, 4.0f, 1.0f, 0.45f, 0.0f, 0.8f, 1.2f);
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1) << w.currentVal;
            float nixFontSize = std::clamp(nixH * 0.40f, 11.0f, 26.0f);
            drawCenteredText(r, ss.str(), cx - nixW * 0.5f, cy - nixH * 0.35f, nixW, nixH * 0.7f, nixFontSize, 1.0f, 0.55f, 0.1f, 1.0f);
            float labelFontSize = std::clamp(nixH * 0.28f, 8.5f, 16.0f);
            drawCenteredText(r, w.label, w.bounds.x, cy + nixH * 0.55f, w.bounds.w, 14.0f, labelFontSize,
                             isLightChassis ? 0.14f : 0.9f,
                             isLightChassis ? 0.16f : 0.92f,
                             isLightChassis ? 0.18f : 0.96f, 1.0f);

        } else if (w.type == GuiWidgetType::VuMeter) {
            float vuW = std::clamp(w.bounds.w * 0.60f, 55.0f, 150.0f);
            float vuH = std::clamp(w.bounds.h * 0.32f, 24.0f, 60.0f);
            drawRoundedRect(r, cx - vuW * 0.5f, cy - vuH * 0.5f, vuW, vuH, 3.0f, 0.06f, 0.07f, 0.09f, 1.0f);
            float segW = (vuW - 12.0f) / 8.0f;
            float segH = vuH * 0.55f;
            for (int seg = 0; seg < 8; ++seg) {
                float sx = cx - vuW * 0.5f + 6.0f + (seg * segW);
                Color segCol = (seg < 5) ? Color{0.0f, 0.95f, 0.45f} : ((seg < 7) ? Color{1.0f, 0.85f, 0.0f} : Color{1.0f, 0.2f, 0.2f});
                drawRect(r, sx, cy - segH * 0.5f, segW - 1.5f, segH, segCol.r, segCol.g, segCol.b, (seg <= 5) ? 1.0f : 0.2f);
            }
            float labelFontSize = std::clamp(vuH * 0.30f, 8.5f, 16.0f);
            drawCenteredText(r, w.label, w.bounds.x, cy + vuH * 0.55f, w.bounds.w, 14.0f, labelFontSize,
                             isLightChassis ? 0.14f : 0.9f,
                             isLightChassis ? 0.16f : 0.92f,
                             isLightChassis ? 0.18f : 0.96f, 1.0f);

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
                    float ptX = sx + 4.0f + (static_cast<float>(i) / static_cast<float>(kPts - 1)) * (sw - 8.0f);
                    float curYPoint = midY - scopeBuffer[sIdx] * (sh * 0.4f);
                    drawLine(r, prevX, prevY, ptX, curYPoint, 0.20f, 1.0f, 0.45f, 0.95f, 1.6f);
                    prevX = ptX;
                    prevY = curYPoint;
                }
            }
            drawCenteredText(r, w.label, w.bounds.x, sy + sh + 2.0f, w.bounds.w, 12.0f, 8.0f,
                             isLightChassis ? 0.14f : 0.9f,
                             isLightChassis ? 0.16f : 0.92f,
                             isLightChassis ? 0.18f : 0.96f, 1.0f);

        } else {
            drawRoundedRect(r, cx - 32.0f, cy - 14.0f, 64.0f, 28.0f, 4.0f, 0.14f, 0.16f, 0.22f, 1.0f);
            drawCenteredText(r, w.label, cx - 32.0f, cy - 7.0f, 64.0f, 14.0f, 9.0f,
                             isLightChassis ? 0.14f : 0.9f,
                             isLightChassis ? 0.16f : 0.92f,
                             isLightChassis ? 0.18f : 0.96f, 1.0f);
        }
    };

    bool isCompactSidebar = (fpW < 380.0f);
    auto macroWidgets = panel.getMacroWidgets();

    if (panel.layoutMode == GuiLayoutMode::AbsoluteCanvas && !panel.widgets.empty()) {
        if (isCompactSidebar) {
            // Render Compact Macro Strip for sidebar context
            std::vector<GuiWidgetDef*> activeMacros;
            if (!macroWidgets.empty()) {
                activeMacros = macroWidgets;
            } else {
                for (auto& w : panel.widgets) {
                    if (w.type != GuiWidgetType::Divider) activeMacros.push_back(&w);
                    if (activeMacros.size() >= 6) break;
                }
            }
            size_t count = activeMacros.size();
            if (count > 0) {
                int cols = (count > 3 && fpW >= 200.0f) ? 2 : 1;
                int rows = static_cast<int>((count + cols - 1) / cols);
                float cellW = (fpW - 16.0f - static_cast<float>(cols - 1) * 8.0f) / static_cast<float>(cols);
                float cellH = (fpH - topPadding - bottomPadding - static_cast<float>(rows - 1) * 6.0f) / static_cast<float>(rows);
                for (size_t i = 0; i < count; ++i) {
                    int c = static_cast<int>(i % cols);
                    int rw = static_cast<int>(i / cols);
                    auto* w = activeMacros[i];
                    w->bounds = Rect2D{
                        fpX + 8.0f + static_cast<float>(c) * (cellW + 8.0f),
                        rowStartY + static_cast<float>(rw) * (cellH + 6.0f),
                        cellW,
                        cellH
                    };
                    bool isDragging = (draggingRow == 0 && draggingWidget == static_cast<int>(i));
                    drawSingleWidget(*w, isDragging);
                }
            }
        } else {
            // Render Full Absolute Canvas with uniform vector scaling
            float availW = fpW - (panel.woodCheeks ? 24.0f : 0.0f);
            float availH = fpH - topPadding - bottomPadding;
            float dW = (panel.designWidth > 10.0f) ? panel.designWidth : 800.0f;
            float dH = (panel.designHeight > 10.0f) ? panel.designHeight : 480.0f;
            float scale = std::min(availW / dW, availH / dH);
            if (scale <= 0.001f) scale = 1.0f;

            float renderW = dW * scale;
            float renderH = dH * scale;
            float startX = fpX + (fpW - renderW) * 0.5f;
            float startY = fpY + topPadding + (availH - renderH) * 0.5f;

            for (size_t wIdx = 0; wIdx < panel.widgets.size(); ++wIdx) {
                auto& w = panel.widgets[wIdx];
                float origW = (w.designBounds.w > 0.0f) ? w.designBounds.w : w.designSize;
                float origH = (w.designBounds.h > 0.0f) ? w.designBounds.h : w.designSize;
                w.bounds = Rect2D{
                    startX + (w.designBounds.x * scale),
                    startY + (w.designBounds.y * scale),
                    origW * scale,
                    origH * scale
                };
                w.size = w.designSize * scale;
                bool isDragging = (draggingRow == 0 && draggingWidget == static_cast<int>(wIdx));
                drawSingleWidget(w, isDragging);
            }
        }
    } else {
        // Multi-row AutoRowFlow layout
        for (size_t rIdx = 0; rIdx < panel.rows.size(); ++rIdx) {
            auto& row = panel.rows[rIdx];
            float ry = rowStartY + (static_cast<float>(rIdx) * rowH);
            row.bounds = Rect2D{fpX + 8.0f, ry, fpW - 16.0f, rowH};

            size_t wCount = row.widgets.size();
            if (wCount == 0) continue;

            size_t dividerCount = 0;
            for (const auto& wid : row.widgets) {
                if (wid.type == GuiWidgetType::Divider) dividerCount++;
            }
            float divWidth = std::clamp(row.bounds.w * 0.015f, 12.0f, 28.0f);
            float remainingW = row.bounds.w - (static_cast<float>(dividerCount) * divWidth);
            size_t nonDivCount = (wCount > dividerCount) ? (wCount - dividerCount) : 1;
            float normalColW = remainingW / static_cast<float>(nonDivCount);

            float curX = row.bounds.x;
            for (size_t wIdx = 0; wIdx < wCount; ++wIdx) {
                auto& w = row.widgets[wIdx];
                float itemW = (w.type == GuiWidgetType::Divider) ? divWidth : normalColW;
                w.bounds = Rect2D{curX, ry + 2.0f, itemW, rowH - 4.0f};
                curX += itemW;

                bool isDragging = (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(wIdx));
                drawSingleWidget(w, isDragging);
            }
        }
    }
}

} // namespace eatsbits::ui
