#include "eatsbits/ui/gui_panel_def.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace eatsbits::ui {

KnobShapeDef getKnobShapeDef(GuiKnobStyle style, const Color& accent) {
    KnobShapeDef def;
    switch (style) {
        case GuiKnobStyle::DaviesFluted:
            def.shape = KnobBodyShape::FlutedScalloped;
            def.flutes = 18;
            def.fluteDepth = 0.16f;
            def.hasSkirt = true;
            def.skirtRatio = 1.28f;
            def.skirtTicks = 11;
            def.capStyle = KnobCapStyle::Flat;
            def.capRatio = 0.70f;
            def.bodyColor = Color(0.12f, 0.12f, 0.14f, 1.0f);
            def.capColor = Color(0.18f, 0.18f, 0.20f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = Color(0.96f, 0.98f, 1.0f, 1.0f);
            def.showHaloArc = true;
            break;

        case GuiKnobStyle::RoganMoog:
            def.shape = KnobBodyShape::FlutedScalloped;
            def.flutes = 12;
            def.fluteDepth = 0.20f;
            def.hasSkirt = true;
            def.skirtRatio = 1.34f;
            def.skirtTicks = 24;
            def.capStyle = KnobCapStyle::LatheMetallic;
            def.capRatio = 0.68f;
            def.bodyColor = Color(0.14f, 0.14f, 0.16f, 1.0f);
            def.capColor = Color(0.85f, 0.88f, 0.94f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = Color(0.95f, 0.95f, 0.95f, 1.0f);
            def.showHaloArc = true;
            break;

        case GuiKnobStyle::ChickenHead:
            def.shape = KnobBodyShape::ChickenHead;
            def.flutes = 0;
            def.hasSkirt = false;
            def.capStyle = KnobCapStyle::LatheMetallic;
            def.capRatio = 0.44f;
            def.bodyColor = Color(0.14f, 0.14f, 0.16f, 1.0f);
            def.capColor = Color(0.35f, 0.38f, 0.44f, 1.0f);
            def.indicator = KnobIndicatorType::PointerNose;
            def.indicatorColor = Color(0.95f, 0.95f, 0.95f, 1.0f);
            def.showHaloArc = false;
            break;

        case GuiKnobStyle::PastelDimple:
            def.shape = KnobBodyShape::Round;
            def.flutes = 0;
            def.hasSkirt = false;
            def.capStyle = KnobCapStyle::IndentedDimple;
            def.capRatio = 0.64f;
            def.bodyColor = Color(accent.r * 0.88f, accent.g * 0.88f, accent.b * 0.88f, 1.0f);
            def.capColor = Color(accent.r * 0.65f, accent.g * 0.65f, accent.b * 0.65f, 1.0f);
            def.indicator = KnobIndicatorType::DotPip;
            def.indicatorColor = Color(0.98f, 0.99f, 1.0f, 1.0f);
            def.showHaloArc = false;
            break;

        case GuiKnobStyle::ModernEncoder:
            def.shape = KnobBodyShape::RibbedKnurled;
            def.flutes = 32;
            def.fluteDepth = 0.08f;
            def.hasSkirt = false;
            def.capStyle = KnobCapStyle::LatheMetallic;
            def.capRatio = 0.80f;
            def.bodyColor = Color(0.18f, 0.20f, 0.24f, 1.0f);
            def.capColor = Color(0.72f, 0.76f, 0.84f, 1.0f);
            def.indicator = KnobIndicatorType::DotPip;
            def.indicatorColor = accent;
            def.showHaloArc = true;
            break;

        case GuiKnobStyle::CreamFluted:
            def.shape = KnobBodyShape::FlutedScalloped;
            def.flutes = 14;
            def.fluteDepth = 0.14f;
            def.hasSkirt = false;
            def.capStyle = KnobCapStyle::Domed;
            def.capRatio = 0.72f;
            def.bodyColor = Color(0.92f, 0.90f, 0.82f, 1.0f);
            def.capColor = Color(0.96f, 0.94f, 0.88f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = Color(0.18f, 0.15f, 0.12f, 1.0f);
            def.showHaloArc = false;
            break;

        case GuiKnobStyle::MiniPotCream:
            def.shape = KnobBodyShape::FlutedScalloped;
            def.flutes = 10;
            def.fluteDepth = 0.12f;
            def.hasSkirt = false;
            def.capStyle = KnobCapStyle::Domed;
            def.capRatio = 0.75f;
            def.bodyColor = Color(0.88f, 0.86f, 0.78f, 1.0f);
            def.capColor = Color(0.95f, 0.93f, 0.86f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = Color(0.18f, 0.15f, 0.12f, 1.0f);
            def.showHaloArc = false;
            break;

        case GuiKnobStyle::BakeliteSkirt:
            def.shape = KnobBodyShape::SkirtedVintage;
            def.flutes = 16;
            def.fluteDepth = 0.16f;
            def.hasSkirt = true;
            def.skirtRatio = 1.30f;
            def.skirtTicks = 11;
            def.capStyle = KnobCapStyle::Flat;
            def.capRatio = 0.70f;
            def.bodyColor = Color(0.20f, 0.10f, 0.06f, 1.0f);
            def.capColor = Color(0.26f, 0.14f, 0.08f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = Color(0.95f, 0.95f, 0.95f, 1.0f);
            def.showHaloArc = false;
            break;

        case GuiKnobStyle::AnodizedKnurled:
            def.shape = KnobBodyShape::RibbedKnurled;
            def.flutes = 28;
            def.fluteDepth = 0.10f;
            def.hasSkirt = false;
            def.capStyle = KnobCapStyle::LatheMetallic;
            def.capRatio = 0.75f;
            def.bodyColor = Color(accent.r * 0.75f, accent.g * 0.75f, accent.b * 0.75f, 1.0f);
            def.capColor = Color(accent.r * 1.15f, accent.g * 1.15f, accent.b * 1.15f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
            def.showHaloArc = true;
            break;

        case GuiKnobStyle::TwoToneStepped:
            def.shape = KnobBodyShape::Round;
            def.hasSkirt = true;
            def.skirtRatio = 1.22f;
            def.skirtTicks = 11;
            def.capStyle = KnobCapStyle::LatheMetallic;
            def.capRatio = 0.65f;
            def.bodyColor = Color(0.20f, 0.22f, 0.26f, 1.0f);
            def.capColor = Color(0.85f, 0.88f, 0.94f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = accent;
            def.showHaloArc = true;
            break;

        case GuiKnobStyle::Tb303Halo:
        case GuiKnobStyle::Tb303Potentiometer:
        case GuiKnobStyle::Tb303SelectorSilver:
            def.shape = KnobBodyShape::FlutedScalloped;
            def.flutes = 24;
            def.fluteDepth = 0.12f;
            def.hasSkirt = true;
            def.skirtRatio = 1.18f;
            def.skirtTicks = 11;
            def.capStyle = KnobCapStyle::LatheMetallic;
            def.capRatio = 0.72f;
            def.bodyColor = Color(0.70f, 0.72f, 0.76f, 1.0f);
            def.capColor = Color(0.88f, 0.90f, 0.95f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = Color(0.96f, 0.98f, 1.0f, 1.0f);
            def.showHaloArc = true;
            break;

        case GuiKnobStyle::Tb303SelectorBlack:
            def.shape = KnobBodyShape::FlutedScalloped;
            def.flutes = 20;
            def.fluteDepth = 0.14f;
            def.hasSkirt = true;
            def.skirtRatio = 1.18f;
            def.skirtTicks = 11;
            def.capStyle = KnobCapStyle::Flat;
            def.capRatio = 0.70f;
            def.bodyColor = Color(0.12f, 0.13f, 0.15f, 1.0f);
            def.capColor = Color(0.18f, 0.19f, 0.22f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = Color(0.95f, 0.95f, 0.95f, 1.0f);
            def.showHaloArc = false;
            break;

        default: // Standard or CustomShape fallback
            def.shape = KnobBodyShape::Round;
            def.flutes = 0;
            def.hasSkirt = false;
            def.capStyle = KnobCapStyle::LatheMetallic;
            def.capRatio = 0.78f;
            def.bodyColor = Color(0.20f, 0.22f, 0.26f, 1.0f);
            def.capColor = Color(0.35f, 0.38f, 0.45f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = accent;
            def.showHaloArc = true;
            break;
    }
    return def;
}

void drawKnobWidget(BatchRenderer2D& r,
                    float cx, float cy, float radius, float normVal,
                    const GuiWidgetDef& w,
                    const LightSource2D& light,
                    bool isActive, bool isHovered, bool isLightChassis) {
    KnobShapeDef shape = w.customShape.value_or(getKnobShapeDef(w.knobStyle, w.accentColor));

    constexpr float minAngle = -2.35619449f; // -135 degrees
    constexpr float maxAngle = 2.35619449f;  // +135 degrees
    float currentAngle = minAngle + std::clamp(normVal, 0.0f, 1.0f) * (maxAngle - minAngle);

    Point lDir = light.getLightDirection(cx, cy);

    // Stepped selector text numbers (0-10) for TB-303 selector knobs
    if (w.knobStyle == GuiKnobStyle::Tb303SelectorSilver || w.knobStyle == GuiKnobStyle::Tb303SelectorBlack) {
        float numDist = radius + std::clamp(radius * 0.34f, 9.0f, 22.0f);
        float numFontSize = std::clamp(radius * 0.24f, 6.5f, 14.0f);
        float numBoxW = numFontSize * 1.8f;
        float numBoxH = numFontSize * 1.4f;
        static constexpr const char* kStepStrings[11] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
        for (int step = 0; step <= 10; ++step) {
            float sAng = minAngle + (static_cast<float>(step) / 10.0f) * (maxAngle - minAngle);
            float nx = cx + std::sin(sAng) * numDist;
            float ny = cy - std::cos(sAng) * numDist;
            drawCenteredText(r, kStepStrings[step], nx - numBoxW * 0.5f, ny - numBoxH * 0.5f, numBoxW, numBoxH, numFontSize,
                             isLightChassis ? 0.25f : 0.80f,
                             isLightChassis ? 0.28f : 0.82f,
                             isLightChassis ? 0.32f : 0.86f, 0.95f);
        }
    }

    // 1. Halo Arc (Progress ring)
    float haloR = (shape.hasSkirt ? radius * shape.skirtRatio : radius) + 4.5f;
    if (shape.showHaloArc) {
        r.drawArc(cx, cy, haloR, minAngle, maxAngle, 0.18f, 0.20f, 0.25f, 0.7f, 1.8f);
        if (normVal > 0.005f) {
            Color arcCol = isActive ? Color(1.0f, 0.88f, 0.12f) : (isHovered ? Color(0.30f, 0.95f, 1.0f) : w.accentColor);
            r.drawArc(cx, cy, haloR, minAngle, currentAngle, arcCol.r, arcCol.g, arcCol.b, 1.0f, isActive ? 2.6f : 2.0f);
        }
    }

    // 2. Drop Shadow onto Chassis
    float shadowR = shape.hasSkirt ? (radius * shape.skirtRatio) : radius;
    r.drawCircleDropShadow(cx, cy, shadowR * 1.06f, 7.5f, light, 0.48f);

    // 3. Flared Base Skirt (if enabled)
    if (shape.hasSkirt && shape.shape != KnobBodyShape::ChickenHead) {
        float sR = radius * shape.skirtRatio;
        r.drawCircleRadial3StopGradient(cx, cy, sR,
                                        shape.bodyColor.r * 1.25f, shape.bodyColor.g * 1.25f, shape.bodyColor.b * 1.25f, 1.0f,
                                        shape.bodyColor.r * 0.85f, shape.bodyColor.g * 0.85f, shape.bodyColor.b * 0.85f, 1.0f,
                                        shape.bodyColor.r * 0.45f, shape.bodyColor.g * 0.45f, shape.bodyColor.b * 0.45f, 1.0f,
                                        lDir.x * sR * 0.25f, lDir.y * sR * 0.25f, 0.50f, 32);
        r.drawCircleOutline(cx, cy, sR, isLightChassis ? 0.35f : 0.08f, isLightChassis ? 0.35f : 0.09f, isLightChassis ? 0.40f : 0.12f, 0.9f, 1.0f, 32);

        if (shape.skirtTicks > 0) {
            float tickStep = (maxAngle - minAngle) / static_cast<float>(shape.skirtTicks - 1);
            for (int t = 0; t < shape.skirtTicks; ++t) {
                float a = minAngle + static_cast<float>(t) * tickStep;
                float sa = std::sin(a), ca = std::cos(a);
                bool isCenter = (t == shape.skirtTicks / 2);
                float r0 = radius + 1.5f;
                float r1 = r0 + (isCenter ? 4.5f : 3.0f);
                r.drawLine(cx + sa * r0, cy - ca * r0, cx + sa * r1, cy - ca * r1,
                           isLightChassis ? 0.2f : 0.85f,
                           isLightChassis ? 0.2f : 0.88f,
                           isLightChassis ? 0.25f : 0.92f, 0.85f, isCenter ? 1.8f : 1.1f);
            }
        }
    }

    // 4. Rotor Body
    if (shape.shape == KnobBodyShape::ChickenHead) {
        Color baseCol = shape.bodyColor;
        Color hlCol = baseCol.lighten(0.35f);
        Color shCol = baseCol.darken(0.35f);
        Color ptrCol = isActive ? Color(1.0f, 0.90f, 0.15f) : (isHovered ? Color(0.30f, 0.95f, 1.0f) : shape.indicatorColor);
        r.drawChickenHeadContour(cx, cy, radius, currentAngle, light,
                                 baseCol.r, baseCol.g, baseCol.b,
                                 hlCol.r, hlCol.g, hlCol.b,
                                 shCol.r, shCol.g, shCol.b,
                                 ptrCol.r, ptrCol.g, ptrCol.b);
    } else if (shape.shape == KnobBodyShape::FlutedScalloped || shape.shape == KnobBodyShape::RibbedKnurled) {
        float rInner = radius * shape.capRatio;
        Color baseCol = shape.bodyColor;
        Color hlCol = baseCol.lighten(0.40f);
        Color shCol = baseCol.darken(0.45f);
        r.drawFlutedRing(cx, cy, rInner, radius, shape.flutes, shape.fluteDepth, currentAngle, light,
                         baseCol.r, baseCol.g, baseCol.b,
                         hlCol.r, hlCol.g, hlCol.b,
                         shCol.r, shCol.g, shCol.b);
        r.drawCircleOutline(cx, cy, radius, shCol.r, shCol.g, shCol.b, 0.7f, 1.0f, 32);
    } else {
        Color baseCol = shape.bodyColor;
        Color hlCol = baseCol.lighten(0.30f);
        Color shCol = baseCol.darken(0.35f);
        r.drawCircleRadial3StopGradient(cx, cy, radius,
                                        hlCol.r, hlCol.g, hlCol.b, 1.0f,
                                        baseCol.r, baseCol.g, baseCol.b, 1.0f,
                                        shCol.r, shCol.g, shCol.b, 1.0f,
                                        lDir.x * radius * 0.25f, lDir.y * radius * 0.25f, 0.50f, 32);
        r.drawCircleOutline(cx, cy, radius, shCol.r, shCol.g, shCol.b, 0.8f, 1.0f, 32);
    }

    // 5. Center Rotor Cap (if not chicken head)
    float capR = radius * shape.capRatio;
    if (shape.shape != KnobBodyShape::ChickenHead && capR > 4.0f) {
        Color capCol = shape.capColor;
        Color capHl = capCol.lighten(0.28f);
        Color capSh = capCol.darken(0.32f);

        if (shape.capStyle == KnobCapStyle::LatheMetallic) {
            r.drawCircleRadial3StopGradient(cx, cy, capR,
                                            capHl.r, capHl.g, capHl.b, 1.0f,
                                            capCol.r, capCol.g, capCol.b, 1.0f,
                                            capSh.r, capSh.g, capSh.b, 1.0f,
                                            lDir.x * capR * 0.35f, lDir.y * capR * 0.35f, 0.50f, 28);
            r.drawCircleOutline(cx, cy, capR, capSh.r, capSh.g, capSh.b, 0.85f, 1.1f, 28);
            r.drawCircleOutline(cx, cy, capR * 0.60f, capHl.r, capHl.g, capHl.b, 0.35f, 0.8f, 20);
        } else if (shape.capStyle == KnobCapStyle::IndentedDimple) {
            r.drawCircleRadial3StopGradient(cx, cy, capR,
                                            capSh.r, capSh.g, capSh.b, 1.0f,
                                            capCol.r, capCol.g, capCol.b, 1.0f,
                                            capHl.r, capHl.g, capHl.b, 1.0f,
                                            -lDir.x * capR * 0.30f, -lDir.y * capR * 0.30f, 0.45f, 28);
            r.drawCircleOutline(cx, cy, capR, capSh.r, capSh.g, capSh.b, 0.9f, 1.2f, 28);
        } else if (shape.capStyle == KnobCapStyle::Domed) {
            r.drawCircleRadial3StopGradient(cx, cy, capR,
                                            capHl.r, capHl.g, capHl.b, 1.0f,
                                            capCol.r, capCol.g, capCol.b, 1.0f,
                                            capSh.r, capSh.g, capSh.b, 1.0f,
                                            lDir.x * capR * 0.20f, lDir.y * capR * 0.20f, 0.55f, 28);
            r.drawCircleOutline(cx, cy, capR, capSh.r, capSh.g, capSh.b, 0.7f, 1.0f, 28);
        } else {
            r.drawCircle(cx, cy, capR, capCol.r, capCol.g, capCol.b, 1.0f, 28);
            r.drawCircleOutline(cx, cy, capR, capSh.r, capSh.g, capSh.b, 0.8f, 1.0f, 28);
        }
    }

    // 6. Indicator (Needle line, Pip dot, or Skirt Notch)
    if (shape.shape != KnobBodyShape::ChickenHead) {
        float sinA = std::sin(currentAngle);
        float cosA = std::cos(currentAngle);

        Color indCol = isActive ? Color(1.0f, 0.88f, 0.12f) : (isHovered ? Color(0.30f, 0.95f, 1.0f) : shape.indicatorColor);

        if (shape.indicator == KnobIndicatorType::LineGroove) {
            float startR = capR * 0.22f;
            float endR = radius * shape.indicatorLength;
            r.drawLine(cx + sinA * startR + 0.6f, cy - cosA * startR + 0.6f,
                       cx + sinA * endR + 0.6f, cy - cosA * endR + 0.6f,
                       0.08f, 0.09f, 0.11f, 0.85f, shape.indicatorWidth + 0.6f);
            r.drawLine(cx + sinA * startR, cy - cosA * startR,
                       cx + sinA * endR, cy - cosA * endR,
                       indCol.r, indCol.g, indCol.b, 1.0f, shape.indicatorWidth);
        } else if (shape.indicator == KnobIndicatorType::DotPip) {
            float pipDist = capR * 0.72f;
            float pipR = std::clamp(radius * 0.08f, 1.8f, 3.2f);
            float px = cx + sinA * pipDist;
            float py = cy - cosA * pipDist;
            r.drawCircle(px + 0.5f, py + 0.5f, pipR, 0.08f, 0.09f, 0.11f, 0.8f, 12);
            r.drawCircle(px, py, pipR, indCol.r, indCol.g, indCol.b, 1.0f, 12);
        } else if (shape.indicator == KnobIndicatorType::PointerNotch) {
            float n0 = radius * 0.85f;
            float n1 = radius * 1.15f;
            r.drawLine(cx + sinA * n0, cy - cosA * n0, cx + sinA * n1, cy - cosA * n1,
                       indCol.r, indCol.g, indCol.b, 1.0f, shape.indicatorWidth + 1.0f);
        }
    }

    // 7. Active Drag Ring Outline
    if (isActive) {
        float dragR = (shape.hasSkirt ? radius * shape.skirtRatio : radius) + 5.0f;
        r.drawCircleOutline(cx, cy, dragR, 0.0f, 0.95f, 1.0f, 0.9f, 2.0f, 32);
    }
}

void drawGuiFaceplate(BatchRenderer2D& r,
                      GuiPanelDef& panel,
                      const Rect2D& rect,
                      const ThemeTokens& theme,
                      const float* scopeBuffer,
                      size_t scopeBufferCount,
                      int draggingRow,
                      int draggingWidget,
                      const std::string& focusedWidgetId) {
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
        case GuiChassisStyle::Snes:            cr = 0.865f; cg = 0.850f; cb = 0.810f; break;
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

    bool isLightChassis = (panel.chassisStyle == GuiChassisStyle::MinimalWhite || panel.chassisStyle == GuiChassisStyle::Snes);
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
            float maxRadW = (w.bounds.w * 0.5f) - 6.0f;
            float maxRadH = (w.bounds.h * 0.40f) - 8.0f;
            float rad = std::clamp(std::min(maxRadW, maxRadH) * 0.78f, 12.0f, 60.0f);

            bool isCurrentDragging = isDragging;
            bool isCurrentHovered = (isCurrentDragging || (!focusedWidgetId.empty() && focusedWidgetId == w.id));

            drawKnobWidget(r, cx, cy, rad, norm, w, theme.globalLight, isCurrentDragging, isCurrentHovered, isLightChassis);

            float labelY = cy + rad + std::clamp(rad * 0.24f, 6.0f, 18.0f);
            float labelH = std::clamp(rad * 0.35f, 12.0f, 20.0f);
            float labelFontSize = std::clamp(rad * 0.27f, 8.5f, 16.0f);
            drawCenteredText(r, w.label, w.bounds.x, labelY, w.bounds.w, labelH, labelFontSize,
                             isLightChassis ? 0.14f : 0.90f,
                             isLightChassis ? 0.16f : 0.92f,
                             isLightChassis ? 0.18f : 0.96f, 1.0f);
            return;
        }

        if (w.type == GuiWidgetType::Slider) {
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

        } else if (w.type == GuiWidgetType::ListBox) {
            // Retro Hardware ListBox with Dark Glass, glowing Red LED header, item selection bar, and steppers
            int totalOpts = static_cast<int>(w.options.size());
            int currentIdx = std::clamp(static_cast<int>(std::round(w.currentVal)), 0, std::max(0, totalOpts - 1));

            float boxX = w.bounds.x + 4.0f;
            float boxY = w.bounds.y + 16.0f;
            float boxW = w.bounds.w - 8.0f;
            float boxH = w.bounds.h - 20.0f;

            // 1. Header: Red LED dot + Label + Digital Red Counter
            float ledX = boxX + 4.0f;
            float ledY = w.bounds.y + 7.0f;
            drawCircle(r, ledX, ledY, 4.0f, 0.95f, 0.15f, 0.15f, 0.35f);
            drawCircle(r, ledX, ledY, 2.2f, 1.0f, 0.20f, 0.20f, 1.0f);

            drawText(r, w.label, ledX + 8.0f, w.bounds.y + 2.0f, 9.5f, 0.42f, 0.48f, 0.55f, 1.0f);

            if (totalOpts > 0) {
                char counterBuf[16];
                std::snprintf(counterBuf, sizeof(counterBuf), "%02d/%02d", currentIdx + 1, totalOpts);
                drawText(r, counterBuf, boxX + boxW - 36.0f, w.bounds.y + 2.0f, 9.5f, 0.95f, 0.15f, 0.15f, 1.0f);
            }

            // 2. Beveled Dark Smoky Glass Container
            drawRoundedRect(r, boxX, boxY, boxW, boxH, 4.0f, 0.08f, 0.08f, 0.10f, 0.98f);
            drawRoundedRectOutline(r, boxX, boxY, boxW, boxH, 4.0f, 0.30f, 0.14f, 0.16f, 0.85f, 1.0f);

            // Active Focus indicator ring if this listbox is focused
            bool isFocused = (!focusedWidgetId.empty() && (w.id == focusedWidgetId || w.param == focusedWidgetId));
            if (isFocused) {
                drawRoundedRectOutline(r, boxX - 1.0f, boxY - 1.0f, boxW + 2.0f, boxH + 2.0f, 5.0f, 0.95f, 0.25f, 0.25f, 0.95f, 1.5f);
            }

            // Glass top specular glare
            drawRoundedRect(r, boxX + 1.0f, boxY + 1.0f, boxW - 2.0f, boxH * 0.40f, 3.0f, 1.0f, 1.0f, 1.0f, 0.04f);

            // 3. Visible Items
            if (totalOpts > 0) {
                int visibleRows = 4;
                float rowH = (boxH - 4.0f) / static_cast<float>(visibleRows);
                
                // Track scroll offset to ensure selected item is in view
                if (static_cast<float>(currentIdx) < w.scrollOffset) {
                    w.scrollOffset = static_cast<float>(currentIdx);
                } else if (static_cast<float>(currentIdx) >= w.scrollOffset + static_cast<float>(visibleRows)) {
                    w.scrollOffset = static_cast<float>(currentIdx - visibleRows + 1);
                }
                w.scrollOffset = std::clamp(w.scrollOffset, 0.0f, static_cast<float>(std::max(0, totalOpts - visibleRows)));
                int scrollStart = static_cast<int>(std::round(w.scrollOffset));

                float listW = boxW - 14.0f;
                for (int row = 0; row < visibleRows; ++row) {
                    int itemIdx = scrollStart + row;
                    if (itemIdx >= totalOpts) break;
                    float iy = boxY + 2.0f + static_cast<float>(row) * rowH;
                    bool isSelected = (itemIdx == currentIdx);

                    if (isSelected) {
                        // Glowing red selection bar with subtle gradient
                        drawRoundedRect(r, boxX + 2.0f, iy, listW, rowH - 1.0f, 2.0f, 0.38f, 0.08f, 0.08f, 0.85f);
                        drawRoundedRectOutline(r, boxX + 2.0f, iy, listW, rowH - 1.0f, 2.0f, 0.80f, 0.15f, 0.15f, 0.9f, 1.0f);
                        // Red chevron
                        drawText(r, ">", boxX + 6.0f, iy + (rowH - 9.0f) * 0.5f, 8.5f, 1.0f, 0.25f, 0.25f, 1.0f);
                        // Selected text in bright white
                        drawText(r, w.options[itemIdx], boxX + 16.0f, iy + (rowH - 9.0f) * 0.5f, 9.5f, 1.0f, 1.0f, 1.0f, 1.0f);
                    } else {
                        // Muted slate text
                        drawText(r, w.options[itemIdx], boxX + 16.0f, iy + (rowH - 9.0f) * 0.5f, 9.0f, 0.50f, 0.54f, 0.60f, 0.90f);
                    }
                }

                // 4. Steppers & Scroll Strip on the right
                float stripX = boxX + boxW - 13.0f;
                drawLine(r, stripX, boxY + 3.0f, stripX, boxY + boxH - 3.0f, 0.35f, 0.12f, 0.14f, 0.70f, 1.0f);
                // Stepper chevrons
                drawText(r, "^", stripX + 3.0f, boxY + 3.0f, 8.0f, 0.95f, 0.20f, 0.20f, 1.0f);
                drawText(r, "v", stripX + 3.0f, boxY + boxH - 11.0f, 8.0f, 0.95f, 0.20f, 0.20f, 1.0f);

                // Scroll thumb
                float trackH = boxH - 24.0f;
                float thumbH = std::max(6.0f, trackH * (static_cast<float>(visibleRows) / static_cast<float>(totalOpts)));
                float thumbY = boxY + 12.0f + (static_cast<float>(scrollStart) / static_cast<float>(std::max(1, totalOpts - visibleRows))) * (trackH - thumbH);
                drawRoundedRect(r, stripX + 2.0f, thumbY, 3.0f, thumbH, 1.0f, 0.90f, 0.18f, 0.18f, 0.95f);
            }

        } else if (w.type == GuiWidgetType::NixieDisplay) {
            float nixW = std::clamp(w.bounds.w * 0.82f, 60.0f, 130.0f);
            float nixH = std::clamp(w.bounds.h * 0.52f, 20.0f, 44.0f);
            float tubeX = cx - nixW * 0.5f;
            float tubeY = cy - nixH * 0.5f + 4.0f;

            // Label above tube
            drawCenteredText(r, w.label, w.bounds.x, tubeY - 13.0f, w.bounds.w, 11.0f, 8.5f,
                             isLightChassis ? 0.38f : 0.70f,
                             isLightChassis ? 0.42f : 0.75f,
                             isLightChassis ? 0.48f : 0.85f, 1.0f);

            // Dark glossy beveled tube housing
            drawRoundedRect(r, tubeX, tubeY, nixW, nixH, 4.0f, 0.05f, 0.04f, 0.05f, 1.0f);
            drawRoundedRectOutline(r, tubeX, tubeY, nixW, nixH, 4.0f, 0.26f, 0.24f, 0.28f, 0.9f, 1.2f);

            // Glass reflection highlight on top half
            drawRoundedRect(r, tubeX + 1.0f, tubeY + 1.0f, nixW - 2.0f, nixH * 0.42f, 3.0f, 1.0f, 1.0f, 1.0f, 0.12f);

            // Glowing neon numerals (orange-red / warm nixie)
            std::ostringstream ss;
            ss << static_cast<int>(std::round(w.currentVal));
            std::string valStr = ss.str();

            float nixFontSize = std::clamp(nixH * 0.55f, 13.0f, 24.0f);
            // Outer glow blur
            drawCenteredText(r, valStr, tubeX, tubeY + (nixH - nixFontSize) * 0.45f, nixW, nixH * 0.6f, nixFontSize + 2.0f, 1.0f, 0.25f, 0.10f, 0.35f);
            // Inner bright neon numeral
            drawCenteredText(r, valStr, tubeX, tubeY + (nixH - nixFontSize) * 0.45f, nixW, nixH * 0.6f, nixFontSize, 1.0f, 0.45f, 0.20f, 1.0f);

        } else if (w.type == GuiWidgetType::ActionButton) {
            // Tactile skeuomorphic push-button with metallic bevel, red indicator dot, and bold red label
            float btnW = std::clamp(w.bounds.w * 0.84f, 68.0f, 130.0f);
            float btnH = std::clamp(w.bounds.h * 0.54f, 20.0f, 36.0f);
            float bx = cx - btnW * 0.5f;
            float by = cy - btnH * 0.5f;

            // Drop shadow
            drawRoundedRect(r, bx, by + 1.5f, btnW, btnH, 4.0f, 0.10f, 0.10f, 0.12f, 0.35f);

            // Metallic / Light warm gray gradient button body
            drawRoundedRect(r, bx, by, btnW, btnH, 4.0f, 0.78f, 0.77f, 0.75f, 1.0f);
            drawRoundedRect(r, bx + 1.0f, by + 1.0f, btnW - 2.0f, btnH * 0.45f, 3.0f, 0.92f, 0.90f, 0.88f, 0.6f);
            drawRoundedRectOutline(r, bx, by, btnW, btnH, 4.0f, 0.44f, 0.42f, 0.40f, 0.95f, 1.2f);

            // Glowing red dot + bold red label
            float dotX = bx + 12.0f;
            float dotY = by + btnH * 0.5f;
            drawCircle(r, dotX, dotY, 4.0f, 0.90f, 0.15f, 0.15f, 0.35f);
            drawCircle(r, dotX, dotY, 2.5f, 1.0f, 0.20f, 0.20f, 1.0f);

            drawCenteredText(r, w.label, bx + 6.0f, by + (btnH - 10.0f) * 0.5f, btnW - 6.0f, 10.0f, 9.5f,
                             0.82f, 0.14f, 0.14f, 1.0f);

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

            // Check if this row ends with stacked NixieDisplay + ActionButton
            bool hasStackedRng = (wCount >= 3 &&
                                  row.widgets[wCount - 2].type == GuiWidgetType::NixieDisplay &&
                                  row.widgets[wCount - 1].type == GuiWidgetType::ActionButton);

            if (hasStackedRng) {
                size_t leadCount = wCount - 2;
                float rngColW = std::clamp(remainingW * (leadCount == 2 ? 0.24f : 0.32f), 70.0f, 140.0f);
                float leadRemW = remainingW - rngColW;
                float leadColW = leadRemW / static_cast<float>(std::max(size_t{1}, leadCount));

                float curX = row.bounds.x;
                for (size_t wIdx = 0; wIdx < leadCount; ++wIdx) {
                    auto& w = row.widgets[wIdx];
                    float itemW = (w.type == GuiWidgetType::Divider) ? divWidth : leadColW;
                    w.bounds = Rect2D{curX, ry + 2.0f, itemW, rowH - 4.0f};
                    curX += itemW;

                    bool isDragging = (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(wIdx));
                    drawSingleWidget(w, isDragging);
                }

                // Stacked column: Nixie on top, ActionButton on bottom
                auto& wNixie = row.widgets[leadCount];
                auto& wBtn = row.widgets[leadCount + 1];
                float halfH = (rowH - 6.0f) * 0.5f;

                wNixie.bounds = Rect2D{curX, ry + 2.0f, rngColW, halfH};
                bool isDragNix = (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(leadCount));
                drawSingleWidget(wNixie, isDragNix);

                wBtn.bounds = Rect2D{curX, ry + 2.0f + halfH + 2.0f, rngColW, halfH};
                bool isDragBtn = (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(leadCount + 1));
                drawSingleWidget(wBtn, isDragBtn);
            } else {
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
}

} // namespace eatsbits::ui
