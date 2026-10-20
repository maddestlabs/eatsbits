#include "eatsbits/ui/gui_panel_def.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/clay/clay_context.hpp"
#include "clay.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <array>

namespace eatsbits::ui {

std::string GuiWidgetDef::getFormattedValue() const {
    if (!valueDisplay.empty()) return valueDisplay;
    std::ostringstream oss;
    if (std::abs(currentVal) >= 1000.0f) {
        oss << std::fixed << std::setprecision(1) << currentVal;
    } else if (std::abs(currentVal) >= 10.0f) {
        oss << std::fixed << std::setprecision(1) << currentVal;
    } else {
        oss << std::fixed << std::setprecision(2) << currentVal;
    }
    if (!unit.empty()) {
        oss << " " << unit;
    }
    return oss.str();
}

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

        case GuiKnobStyle::Tb303Potentiometer:
            def.shape = KnobBodyShape::RibbedKnurled;
            def.flutes = 48; // authentic 48-tooth fine serration
            def.fluteDepth = 0.08f;
            def.hasSkirt = true;
            def.skirtRatio = 1.18f;
            def.skirtTicks = 0; // Chassis graduation marks replace skirt ticks
            def.capStyle = KnobCapStyle::DeclinedScoop;
            def.capRatio = 0.74f;
            def.bodyColor = Color(0.72f, 0.74f, 0.78f, 1.0f);
            def.capColor = Color(0.86f, 0.88f, 0.92f, 1.0f);
            def.indicator = KnobIndicatorType::None; // DeclinedScoop renders its own integrated etched notch line
            def.showHaloArc = false; // Authentic silver chassis ticks without LED halo
            def.sunkenSlotWidth = 1.8f;
            def.hasCenterDetent = true;
            def.hasBlockCenterDetent = true;
            def.dialTicks = 10;
            def.dialLabels = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
            break;

        case GuiKnobStyle::Tb303Halo:
        case GuiKnobStyle::Tb303SelectorSilver:
            def.shape = KnobBodyShape::FlutedScalloped;
            def.flutes = 24;
            def.fluteDepth = 0.12f;
            def.hasSkirt = true;
            def.skirtRatio = 1.18f;
            def.skirtTicks = (style == GuiKnobStyle::Tb303Halo) ? 11 : 0;
            def.capStyle = KnobCapStyle::LatheMetallic;
            def.capRatio = 0.72f;
            def.bodyColor = Color(0.70f, 0.72f, 0.76f, 1.0f);
            def.capColor = Color(0.88f, 0.90f, 0.95f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = (style == GuiKnobStyle::Tb303Halo) ? Color(0.96f, 0.98f, 1.0f, 1.0f) : Color(0.14f, 0.14f, 0.16f, 1.0f);
            def.showHaloArc = (style == GuiKnobStyle::Tb303Halo);
            def.sunkenSlotWidth = 1.8f;
            if (style == GuiKnobStyle::Tb303SelectorSilver) {
                def.hasCenterDetent = true;
                def.hasBlockCenterDetent = true;
                def.dialTicks = 10;
                def.dialLabels = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
            }
            break;

        case GuiKnobStyle::Tb303SelectorBlack:
            def.shape = KnobBodyShape::FlutedScalloped;
            def.flutes = 20;
            def.fluteDepth = 0.14f;
            def.hasSkirt = true;
            def.skirtRatio = 1.18f;
            def.skirtTicks = 0;
            def.capStyle = KnobCapStyle::Flat;
            def.capRatio = 0.70f;
            def.bodyColor = Color(0.12f, 0.13f, 0.15f, 1.0f);
            def.capColor = Color(0.18f, 0.19f, 0.22f, 1.0f);
            def.indicator = KnobIndicatorType::LineGroove;
            def.indicatorColor = Color(0.95f, 0.95f, 0.95f, 1.0f);
            def.showHaloArc = false;
            def.sunkenSlotWidth = 1.8f;
            def.hasCenterDetent = true;
            def.hasBlockCenterDetent = true;
            def.dialTicks = 10;
            def.dialLabels = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
            break;

        case GuiKnobStyle::StudioClean:
        case GuiKnobStyle::StudioRuby:
        case GuiKnobStyle::StudioMagenta:
        case GuiKnobStyle::StudioCyan:
        case GuiKnobStyle::StudioDark:
        case GuiKnobStyle::StudioBipolar:
            def.shape = KnobBodyShape::Round;
            def.flutes = 0;
            def.hasSkirt = false;
            def.capStyle = KnobCapStyle::Flat;
            def.capRatio = 0.82f;
            def.bodyColor = Color(0.88f, 0.90f, 0.93f, 1.0f);
            def.capColor = Color(0.95f, 0.96f, 0.98f, 1.0f);
            def.indicator = KnobIndicatorType::PointerNotch;
            def.indicatorLength = 1.0f;
            def.indicatorWidth = 2.2f;
            def.showHaloArc = true;
            def.trackColor = Color(0.12f, 0.13f, 0.16f, 0.95f);
            def.sunkenSlotWidth = 0.0f;
            def.isLightRotor = true;

            if (style == GuiKnobStyle::StudioRuby) {
                def.indicatorColor = Color(0.84f, 0.14f, 0.20f, 1.0f); // Ruby Red (like TUBE 12AX7)
            } else if (style == GuiKnobStyle::StudioMagenta) {
                def.indicatorColor = Color(0.72f, 0.12f, 0.60f, 1.0f); // Magenta / Purple (like BRILLIANCE)
            } else if (style == GuiKnobStyle::StudioCyan) {
                def.indicatorColor = Color(0.0f, 0.90f, 1.0f, 1.0f);  // Astral Shimmer Cyan
            } else if (style == GuiKnobStyle::StudioDark) {
                def.indicatorColor = Color(0.12f, 0.13f, 0.16f, 1.0f); // Dark Charcoal
            } else if (style == GuiKnobStyle::StudioBipolar) {
                def.indicatorColor = accent;
                def.hasCenterDetent = true;
                def.dialTicks = 2;
            } else {
                def.indicatorColor = Color(0.14f, 0.15f, 0.18f, 1.0f); // Crisp dark groove
            }
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

static const KnobShapeDef& getStockKnobShapeDef(GuiKnobStyle style) {
    static const auto stockShapes = []() {
        std::array<KnobShapeDef, 32> arr;
        constexpr GuiKnobStyle allStyles[] = {
            GuiKnobStyle::CreamFluted,
            GuiKnobStyle::BakeliteSkirt,
            GuiKnobStyle::AnodizedKnurled,
            GuiKnobStyle::TwoToneStepped,
            GuiKnobStyle::Tb303Halo,
            GuiKnobStyle::Standard,
            GuiKnobStyle::Tb303Potentiometer,
            GuiKnobStyle::Tb303SelectorSilver,
            GuiKnobStyle::Tb303SelectorBlack,
            GuiKnobStyle::MiniPotCream,
            GuiKnobStyle::DaviesFluted,
            GuiKnobStyle::RoganMoog,
            GuiKnobStyle::ChickenHead,
            GuiKnobStyle::PastelDimple,
            GuiKnobStyle::ModernEncoder,
            GuiKnobStyle::CustomShape,
            GuiKnobStyle::StudioClean,
            GuiKnobStyle::StudioRuby,
            GuiKnobStyle::StudioMagenta,
            GuiKnobStyle::StudioCyan,
            GuiKnobStyle::StudioDark,
            GuiKnobStyle::StudioBipolar
        };
        for (auto s : allStyles) {
            size_t idx = static_cast<size_t>(s);
            if (idx < arr.size()) {
                arr[idx] = getKnobShapeDef(s);
            }
        }
        return arr;
    }();
    size_t idx = static_cast<size_t>(style);
    if (idx < stockShapes.size()) {
        return stockShapes[idx];
    }
    return stockShapes[static_cast<size_t>(GuiKnobStyle::Standard)];
}

void drawKnobWidget(BatchRenderer2D& r,
                    float cx, float cy, float radius, float normVal,
                    const GuiWidgetDef& w,
                    const LightSource2D& light,
                    bool isActive, bool isHovered, bool isLightChassis,
                    const ThemeTokens* theme) {
    const KnobShapeDef& shape = w.customShape.has_value() ? *w.customShape : getStockKnobShapeDef(w.knobStyle);
    float sunkenSlotWidth = (w.sunkenSlotWidth > 0.0f) ? w.sunkenSlotWidth : shape.sunkenSlotWidth;

    Color bodyColor = shape.bodyColor;
    Color capColor = shape.capColor;
    Color indicatorColor = shape.indicatorColor;
    if (w.knobStyle == GuiKnobStyle::AnodizedKnurled) {
        bodyColor = Color(w.accentColor.r * 0.75f, w.accentColor.g * 0.75f, w.accentColor.b * 0.75f, 1.0f);
        capColor = Color(w.accentColor.r * 1.15f, w.accentColor.g * 1.15f, w.accentColor.b * 1.15f, 1.0f);
    } else if (w.knobStyle == GuiKnobStyle::TwoToneStepped ||
               w.knobStyle == GuiKnobStyle::StudioBipolar ||
               w.knobStyle == GuiKnobStyle::Standard) {
        indicatorColor = w.accentColor;
    }

    if (theme && theme->controlMode == ControlAestheticMode::ForceMinimal) {
        KnobStyleCore minStyle = KnobStyleCore::minimal(w.accentColor, shape.hasCenterDetent || shape.hasBlockCenterDetent);
        drawUnifiedKnob(r, cx, cy, radius, normVal, minStyle, {}, {}, theme, isActive, isHovered);
        return;
    }

    constexpr float minAngle = -2.35619449f; // -135 degrees
    constexpr float maxAngle = 2.35619449f;  // +135 degrees
    float currentAngle = minAngle + std::clamp(normVal, 0.0f, 1.0f) * (maxAngle - minAngle);

    Point lDir = light.getLightDirection(cx, cy);
    float baseRadius = shape.hasSkirt ? (radius * shape.skirtRatio) : radius;

    // 0. Recessed Faceplate Sunken Slot / Mounting Gutter (if enabled, <=0 is unrendered)
    if (sunkenSlotWidth > 0.0f) {
        float trenchR = baseRadius + sunkenSlotWidth;
        r.drawCircle(cx, cy, trenchR, shape.sunkenSlotColor.r, shape.sunkenSlotColor.g, shape.sunkenSlotColor.b, 1.0f, 36);
        r.drawCircleOutline(cx, cy, trenchR, 0.03f, 0.04f, 0.05f, 0.95f, 1.2f, 36);
        r.drawArc(cx, cy, trenchR, 0.785f, 2.356f, 1.0f, 1.0f, 1.0f, 0.20f, 1.0f, 24);
    }

    // Configurable graduation dial ticks and perimeter string labels
    const auto& labels = !w.dialLabels.empty() ? w.dialLabels : (!shape.dialLabels.empty() ? shape.dialLabels : w.options);
    size_t labelCount = labels.size();
    int divs = (shape.dialTicks > 0) ? shape.dialTicks : (labelCount > 1 ? static_cast<int>(labelCount - 1) : 0);

    float trackR = baseRadius + (shape.showHaloArc ? 4.5f : 2.0f);
    float tickR0 = trackR + 1.2f;
    float tickLen = 3.6f;
    float majorTickLen = 5.6f;
    Color tickCol = shape.dialTickColor.value_or(isLightChassis ? Color(0.20f, 0.22f, 0.26f, 0.90f) : Color(0.68f, 0.72f, 0.78f, 0.85f));

    if (divs > 0) {
        for (int t = 0; t <= divs; ++t) {
            float normT = static_cast<float>(t) / static_cast<float>(divs);
            float a = minAngle + normT * (maxAngle - minAngle);
            float sa = std::sin(a), ca = std::cos(a);
            bool isCenter = (divs % 2 == 0) && (t == divs / 2) && (shape.hasCenterDetent || shape.hasBlockCenterDetent);
            bool isMajor = (t == 0 || t == divs || isCenter);

            if (isCenter && shape.hasBlockCenterDetent) {
                // TB-303 authentic bold calibration block at 12 o'clock
                r.drawLine(cx + sa * tickR0, cy - ca * tickR0,
                           cx + sa * (tickR0 + majorTickLen + 1.5f), cy - ca * (tickR0 + majorTickLen + 1.5f),
                           tickCol.r, tickCol.g, tickCol.b, tickCol.a, 3.2f);
            } else {
                float tLen = isMajor ? majorTickLen : tickLen;
                float tWidth = isMajor ? 1.4f : 1.0f;
                r.drawLine(cx + sa * tickR0, cy - ca * tickR0,
                           cx + sa * (tickR0 + tLen), cy - ca * (tickR0 + tLen),
                           tickCol.r, tickCol.g, tickCol.b, tickCol.a, tWidth);
            }
        }
    }

    if (labelCount > 0) {
        float textRingRadius = tickR0 + majorTickLen + 2.5f;
        float labelDist = std::max(textRingRadius, baseRadius * shape.dialLabelDistanceRatio);
        float fontSize = std::clamp(shape.dialLabelFontSize * (radius / 28.0f), 6.5f, 13.0f);
        float boxW = fontSize * 3.5f;
        float boxH = fontSize * 1.5f;
        Color textCol = shape.dialLabelColor.value_or(isLightChassis ? Color(0.16f, 0.18f, 0.22f, 0.95f) : Color(0.85f, 0.88f, 0.92f, 0.95f));

        for (size_t i = 0; i < labelCount; ++i) {
            float t = (labelCount > 1) ? (static_cast<float>(i) / static_cast<float>(labelCount - 1)) : 0.5f;
            float a = minAngle + t * (maxAngle - minAngle);
            float lx = cx + std::sin(a) * labelDist;
            float ly = cy - std::cos(a) * labelDist;
            drawCenteredText(r, labels[i], lx - boxW * 0.5f, ly - boxH * 0.5f, boxW, boxH, fontSize,
                             textCol.r, textCol.g, textCol.b, textCol.a);
        }
    }

    // 1. Halo Arc (Progress ring)
    float haloR = (shape.hasSkirt ? radius * shape.skirtRatio : radius) + (shape.isLightRotor ? 3.0f : 4.5f);
    if (shape.showHaloArc) {
        Color baseTrackCol = isLightChassis ? Color(0.78f, 0.81f, 0.86f, 0.45f) : shape.trackColor.value_or(Color(0.18f, 0.20f, 0.25f, 0.7f));
        float arcWidth = shape.isLightRotor ? 3.4f : 2.2f;

        if (isLightChassis && shape.isLightRotor) {
            r.drawArc(cx, cy, haloR + arcWidth * 0.5f + 0.4f, minAngle, maxAngle, 0.65f, 0.68f, 0.74f, 0.30f, 0.8f);
            r.drawArc(cx, cy, haloR - arcWidth * 0.5f - 0.4f, minAngle, maxAngle, 0.55f, 0.58f, 0.64f, 0.25f, 0.8f);
        }

        r.drawArc(cx, cy, haloR, minAngle, maxAngle, baseTrackCol.r, baseTrackCol.g, baseTrackCol.b, baseTrackCol.a, arcWidth);

        bool isBipolar = (w.knobStyle == GuiKnobStyle::StudioBipolar) || (shape.hasCenterDetent && w.minVal < 0.0f);
        if (isBipolar) {
            float centerAngle = (minAngle + maxAngle) * 0.5f;
            float startA = std::min(centerAngle, currentAngle);
            float endA = std::max(centerAngle, currentAngle);
            if (std::abs(currentAngle - centerAngle) > 0.01f) {
                Color arcCol = isActive ? Color(1.0f, 0.88f, 0.12f) : (isHovered ? indicatorColor.lighten(0.18f) : indicatorColor);
                r.drawArc(cx, cy, haloR, startA, endA, arcCol.r, arcCol.g, arcCol.b, 1.0f, isActive ? 3.8f : arcWidth);
            }
        } else {
            if (normVal > 0.005f) {
                Color arcCol = isActive ? Color(1.0f, 0.88f, 0.12f) : (isHovered ? indicatorColor.lighten(0.18f) : indicatorColor);
                r.drawArc(cx, cy, haloR, minAngle, currentAngle, arcCol.r, arcCol.g, arcCol.b, 1.0f, isActive ? 3.8f : arcWidth);
            }
        }
    }

    // 2. Drop Shadow onto Chassis
    float shadowR = shape.hasSkirt ? (radius * shape.skirtRatio) : radius;
    r.drawCircleDropShadow(cx, cy, shadowR * (shape.isLightRotor ? 1.02f : 1.06f), shape.isLightRotor ? 9.0f : 7.5f, light, shape.isLightRotor ? 0.42f : 0.48f);

    // 3. Flared Base Skirt (if enabled)
    if (shape.hasSkirt && shape.shape != KnobBodyShape::ChickenHead) {
        float sR = radius * shape.skirtRatio;
        r.drawCircleRadial3StopGradient(cx, cy, sR,
                                        bodyColor.r * 1.25f, bodyColor.g * 1.25f, bodyColor.b * 1.25f, 1.0f,
                                        bodyColor.r * 0.85f, bodyColor.g * 0.85f, bodyColor.b * 0.85f, 1.0f,
                                        bodyColor.r * 0.45f, bodyColor.g * 0.45f, bodyColor.b * 0.45f, 1.0f,
                                        lDir.x * sR * 0.25f, lDir.y * sR * 0.25f, 0.50f, 32);
        r.drawCircleOutline(cx, cy, sR, isLightChassis ? 0.35f : 0.08f, isLightChassis ? 0.35f : 0.09f, isLightChassis ? 0.40f : 0.12f, 0.9f, 1.0f, 32);

        if (shape.skirtTicks > 0) {
            float tickStep = (maxAngle - minAngle) / static_cast<float>(shape.skirtTicks - 1);
            for (int t = 0; t < shape.skirtTicks; ++t) {
                float a = minAngle + static_cast<float>(t) * tickStep;
                float sa = std::sin(a), ca = std::cos(a);
                bool isCenter = (t == shape.skirtTicks / 2);
                float r0 = radius + 1.5f;
                float r1 = r0 + (isCenter ? 5.5f : 3.0f);
                float strokeW = isCenter ? 2.6f : 1.1f;
                r.drawLine(cx + sa * r0, cy - ca * r0, cx + sa * r1, cy - ca * r1,
                           isLightChassis ? 0.15f : 0.85f,
                           isLightChassis ? 0.15f : 0.88f,
                           isLightChassis ? 0.18f : 0.92f, 0.92f, strokeW);
            }
        }
    }

    // 4. Rotor Body
    if (shape.shape == KnobBodyShape::ChickenHead) {
        Color baseCol = bodyColor;
        Color hlCol = baseCol.lighten(0.35f);
        Color shCol = baseCol.darken(0.35f);
        Color ptrCol = isActive ? Color(1.0f, 0.90f, 0.15f) : (isHovered ? Color(0.30f, 0.95f, 1.0f) : indicatorColor);
        r.drawChickenHeadContour(cx, cy, radius, currentAngle, light,
                                 baseCol.r, baseCol.g, baseCol.b,
                                 hlCol.r, hlCol.g, hlCol.b,
                                 shCol.r, shCol.g, shCol.b,
                                 ptrCol.r, ptrCol.g, ptrCol.b);
    } else if (shape.shape == KnobBodyShape::FlutedScalloped || shape.shape == KnobBodyShape::RibbedKnurled) {
        float rInner = radius * shape.capRatio;
        Color baseCol = bodyColor;
        Color hlCol = baseCol.lighten(0.40f);
        Color shCol = baseCol.darken(0.45f);
        r.drawFlutedRing(cx, cy, rInner, radius, shape.flutes, shape.fluteDepth, currentAngle, light,
                         baseCol.r, baseCol.g, baseCol.b,
                         hlCol.r, hlCol.g, hlCol.b,
                         shCol.r, shCol.g, shCol.b);
        r.drawCircleOutline(cx, cy, radius, shCol.r, shCol.g, shCol.b, 0.7f, 1.0f, 32);
    } else {
        Color baseCol = bodyColor;
        Color hlCol = shape.isLightRotor ? baseCol.lighten(0.12f) : baseCol.lighten(0.30f);
        Color shCol = shape.isLightRotor ? baseCol.darken(0.20f) : baseCol.darken(0.35f);
        r.drawCircleRadial3StopGradient(cx, cy, radius,
                                        hlCol.r, hlCol.g, hlCol.b, 1.0f,
                                        baseCol.r, baseCol.g, baseCol.b, 1.0f,
                                        shCol.r, shCol.g, shCol.b, 1.0f,
                                        lDir.x * radius * 0.25f, lDir.y * radius * 0.25f, 0.50f, 32);
        r.drawCircleOutline(cx, cy, radius, shCol.r * 0.8f, shCol.g * 0.8f, shCol.b * 0.8f, shape.isLightRotor ? 0.90f : 0.8f, 1.0f, 32);
        if (shape.isLightRotor) {
            float lightAngle = std::atan2(lDir.y, lDir.x);
            float shadowAngle = lightAngle + 3.14159265f;
            r.drawArc(cx, cy, radius - 0.5f, lightAngle - 1.25f, lightAngle + 1.25f, 1.0f, 1.0f, 1.0f, 0.70f, 1.2f, 24);
            r.drawArc(cx, cy, radius, shadowAngle - 1.15f, shadowAngle + 1.15f, shCol.r * 0.6f, shCol.g * 0.6f, shCol.b * 0.6f, 0.85f, 1.3f, 24);
        }
    }

    // 5. Center Rotor Cap (if not chicken head)
    float capR = radius * shape.capRatio;
    if (shape.shape != KnobBodyShape::ChickenHead && capR > 4.0f) {
        Color capCol = capColor;
        Color capHl = capCol.lighten(0.28f);
        Color capSh = capCol.darken(0.32f);

        if (shape.capStyle == KnobCapStyle::DeclinedScoop) {
            // Authentic Roland TB-303 Declined Wedge Scoop Cap
            // 1. Base circular plateau disc with top-left directional highlight gradient
            r.drawCircleRadial3StopGradient(cx, cy, capR,
                                            capHl.r, capHl.g, capHl.b, 1.0f,
                                            capCol.r, capCol.g, capCol.b, 1.0f,
                                            capSh.r, capSh.g, capSh.b, 1.0f,
                                            lDir.x * capR * 0.35f, lDir.y * capR * 0.35f, 0.50f, 28);
            r.drawCircleOutline(cx, cy, capR, capSh.r * 0.7f, capSh.g * 0.7f, capSh.b * 0.7f, 0.85f, 1.1f, 28);

            // 2. The Declined Wedge Scoop (oriented along currentAngle towards indicator)
            // A chord ridge step cut into the cap disc perpendicular to currentAngle at chordDist = -capR * 0.12f
            float sinA = std::sin(currentAngle);
            float cosA = std::cos(currentAngle);
            float dirX = sinA;
            float dirY = -cosA;
            float perpX = -cosA;
            float perpY = -sinA;

            float chordDistRatio = -0.12f;
            float chordDist = capR * chordDistRatio;
            float chordCx = cx + dirX * chordDist;
            float chordCy = cy + dirY * chordDist;
            float chordHalfW = capR * std::sqrt(std::max(0.0f, 1.0f - chordDistRatio * chordDistRatio));

            // Contextual 2.5D lighting:
            // The ramp slopes downwards from the chord towards the outer rim along (dirX, dirY).
            // When (dirX, dirY) points towards the light source, the ramp is tilted into incoming light.
            // When pointing away, the ramp falls into deep shadow (as seen on authentic TB-303 hardware).
            float dotRamp = dirX * lDir.x + dirY * lDir.y;

            Color scoopCrest, scoopMid, scoopRim;
            if (dotRamp < 0.0f) {
                // Tilted away from light (shadowed ramp, e.g. Volume knob at 4:30):
                // Pronounced ambient occlusion shadow at the chord cliff step deepening into shadowed satin metal
                float shAmt = std::clamp(-dotRamp, 0.0f, 1.0f);
                scoopCrest = capCol.darken(0.38f + shAmt * 0.32f); // Deep shadow near cliff
                scoopMid   = capCol.darken(0.24f + shAmt * 0.22f);
                scoopRim   = capCol.darken(0.16f + shAmt * 0.14f);
            } else {
                // Tilted towards light (illuminated ramp):
                // Bright satin aluminum highlight and specular reflection
                float hlAmt = std::clamp(dotRamp, 0.0f, 1.0f);
                scoopCrest = capCol.lighten(0.22f + hlAmt * 0.24f);
                scoopMid   = capCol.lighten(0.12f + hlAmt * 0.18f);
                scoopRim   = capCol.darken(0.10f * (1.0f - hlAmt));
            }

            // Continuous, smooth Gouraud gradient mesh filling the exact circular segment
            r.drawDeclinedScoopGradient(cx, cy, capR, currentAngle, chordDistRatio,
                                        scoopCrest.r, scoopCrest.g, scoopCrest.b, 1.0f,
                                        scoopMid.r, scoopMid.g, scoopMid.b, 1.0f,
                                        scoopRim.r, scoopRim.g, scoopRim.b, 1.0f,
                                        0.45f, 24);

            // Chord Crest Transition Line (Cliff Face):
            float leftChordX = chordCx + perpX * chordHalfW * 0.96f;
            float leftChordY = chordCy + perpY * chordHalfW * 0.96f;
            float rightChordX = chordCx - perpX * chordHalfW * 0.96f;
            float rightChordY = chordCy - perpY * chordHalfW * 0.96f;

            if (dotRamp < -0.05f) {
                // Light is behind the plateau: cliff face is in shadow and casts a dark drop shadow onto the scoop floor
                float shadowIntensity = std::clamp(-dotRamp, 0.0f, 1.0f);
                // Plateau edge silhouette line
                r.drawLine(leftChordX, leftChordY, rightChordX, rightChordY,
                           capSh.r * 0.45f, capSh.g * 0.45f, capSh.b * 0.45f, 0.90f, 1.2f);
                // Soft cast shadow line forward onto ramp floor
                r.drawLine(leftChordX + dirX * 1.3f, leftChordY + dirY * 1.3f,
                           rightChordX + dirX * 1.3f, rightChordY + dirY * 1.3f,
                           0.05f, 0.05f, 0.08f, 0.75f * shadowIntensity, 1.8f);
            } else {
                // Light directly strikes cliff edge: crisp specular gleam
                float hlIntensity = std::clamp(dotRamp, 0.0f, 1.0f);
                r.drawLine(leftChordX, leftChordY, rightChordX, rightChordY,
                           1.0f, 1.0f, 1.0f, 0.65f + 0.35f * hlIntensity, 1.5f);
                // Secondary plateau reflection
                r.drawLine(leftChordX - dirX * 0.8f, leftChordY - dirY * 0.8f,
                           rightChordX - dirX * 0.8f, rightChordY - dirY * 0.8f,
                           capHl.r, capHl.g, capHl.b, 0.40f * hlIntensity, 1.0f);
            }

            // 3. Etched dark pointer notch line centered down the scoop slope
            float notchStart = chordDist + (capR * 0.18f);
            float notchEnd = capR * 0.96f;

            // Transverse lighting on the notch groove walls
            float dotPerp = perpX * lDir.x + perpY * lDir.y;
            float notchHlX = (dotPerp >= 0.0f ? perpX : -perpX) * 0.75f;
            float notchHlY = (dotPerp >= 0.0f ? perpY : -perpY) * 0.75f;
            float notchShX = -notchHlX;
            float notchShY = -notchHlY;

            // Physical bevel: Shadow side of etched groove
            r.drawLine(cx + dirX * notchStart + notchShX, cy + dirY * notchStart + notchShY,
                       cx + dirX * notchEnd + notchShX, cy + dirY * notchEnd + notchShY,
                       0.05f, 0.05f, 0.07f, 0.65f, 1.8f);

            // Physical bevel: Highlight side of etched groove
            r.drawLine(cx + dirX * notchStart + notchHlX, cy + dirY * notchStart + notchHlY,
                       cx + dirX * notchEnd + notchHlX, cy + dirY * notchEnd + notchHlY,
                       1.0f, 1.0f, 1.0f, 0.45f, 1.8f);

            // Core etched dark notch line
            Color notchCol = isActive ? Color(1.0f, 0.88f, 0.12f) : (isHovered ? Color(0.30f, 0.95f, 1.0f) : Color(0.08f, 0.09f, 0.12f));
            r.drawLine(cx + dirX * notchStart, cy + dirY * notchStart,
                       cx + dirX * notchEnd, cy + dirY * notchEnd,
                       notchCol.r, notchCol.g, notchCol.b, 1.0f, 2.0f);

            // Outer rim chamfer
            r.drawCircleOutline(cx, cy, capR, 1.0f, 1.0f, 1.0f, 0.35f, 1.0f, 32);
        } else if (shape.capStyle == KnobCapStyle::LatheMetallic) {
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
            // Enhanced 3D Flat / Cylindrical Plateau Cap (Studio / High-End Hardware aesthetic)
            Color plateauHl = shape.isLightRotor ? capCol.lighten(0.08f) : capCol.lighten(0.25f);
            Color plateauMid = capCol;
            Color plateauSh = shape.isLightRotor ? capCol.darken(0.18f) : capCol.darken(0.30f);

            // 1. Directional radial gradient reacting smoothly to light source
            r.drawCircleRadial3StopGradient(cx, cy, capR,
                                            plateauHl.r, plateauHl.g, plateauHl.b, 1.0f,
                                            plateauMid.r, plateauMid.g, plateauMid.b, 1.0f,
                                            plateauSh.r, plateauSh.g, plateauSh.b, 1.0f,
                                            lDir.x * capR * 0.32f, lDir.y * capR * 0.32f, 0.48f, 32);

            // 2. Beveled plateau rim outline / groove step
            r.drawCircleOutline(cx, cy, capR, plateauSh.r * 0.85f, plateauSh.g * 0.85f, plateauSh.b * 0.85f, 0.75f, 1.0f, 32);

            // 3. Directional specular gleam on illuminated rim of plateau
            float lightAngle = std::atan2(lDir.y, lDir.x);
            r.drawArc(cx, cy, capR - 0.5f, lightAngle - 1.25f, lightAngle + 1.25f, 1.0f, 1.0f, 1.0f, 0.65f, 1.2f, 24);

            // 4. Subtle inner reflection ring for light rotors (satin aluminum sheen)
            if (shape.isLightRotor) {
                r.drawCircleOutline(cx, cy, capR * 0.94f, 1.0f, 1.0f, 1.0f, 0.28f, 0.8f, 28);
            }
        }
    }

    // 6. Indicator (Needle line, Pip dot, or Skirt Notch)
    if (shape.shape != KnobBodyShape::ChickenHead && shape.capStyle != KnobCapStyle::DeclinedScoop) {
        float sinA = std::sin(currentAngle);
        float cosA = std::cos(currentAngle);

        Color indCol = isActive ? Color(1.0f, 0.88f, 0.12f) : (isHovered ? Color(0.30f, 0.95f, 1.0f) : indicatorColor);

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
            // Precision perimeter indicator notch bridging cap chamfer to outer rotor wall
            float n0 = std::max(capR * 0.84f, radius * 0.68f);
            float n1 = radius * 1.02f;

            float dirX = sinA;
            float dirY = -cosA;
            float perpX = -cosA;
            float perpY = -sinA;

            // Transverse lighting calculation across the notch groove walls
            float dotPerp = perpX * lDir.x + perpY * lDir.y;
            float bevelOffset = 0.70f;
            float notchHlX = (dotPerp >= 0.0f ? perpX : -perpX) * bevelOffset;
            float notchHlY = (dotPerp >= 0.0f ? perpY : -perpY) * bevelOffset;
            float notchShX = -notchHlX;
            float notchShY = -notchHlY;

            // Physical bevel: Shadow flank of notch
            r.drawLine(cx + dirX * n0 + notchShX, cy + dirY * n0 + notchShY,
                       cx + dirX * n1 + notchShX, cy + dirY * n1 + notchShY,
                       0.08f, 0.09f, 0.12f, 0.65f, shape.indicatorWidth);

            // Physical bevel: Specular highlight flank of notch
            r.drawLine(cx + dirX * n0 + notchHlX, cy + dirY * n0 + notchHlY,
                       cx + dirX * n1 + notchHlX, cy + dirY * n1 + notchHlY,
                       1.0f, 1.0f, 1.0f, 0.55f, shape.indicatorWidth);

            // Core precision pointer notch
            // For light rotors, default to crisp metallic white/silver (as in reference hardware),
            // lighting up brightly with accent color during hover or active drag
            Color notchCol;
            if (isActive) {
                notchCol = Color(1.0f, 0.88f, 0.12f, 1.0f);
            } else if (isHovered) {
                notchCol = shape.isLightRotor ? indCol.lighten(0.15f) : Color(0.30f, 0.95f, 1.0f, 1.0f);
            } else if (shape.isLightRotor) {
                notchCol = Color(0.98f, 0.99f, 1.0f, 1.0f);
            } else {
                notchCol = indCol;
            }

            r.drawLine(cx + dirX * n0, cy + dirY * n0,
                       cx + dirX * n1, cy + dirY * n1,
                       notchCol.r, notchCol.g, notchCol.b, 1.0f, shape.indicatorWidth);
        }
    }

    // 7. Active Drag Ring Outline
    if (isActive) {
        float dragR = (shape.hasSkirt ? radius * shape.skirtRatio : radius) + 5.0f;
        r.drawCircleOutline(cx, cy, dragR, 0.0f, 0.95f, 1.0f, 0.9f, 2.0f, 32);
    }
}

void GuiPanelDef::layoutControls(const Rect2D& rect) {
    bounds = rect;
    float fpX = rect.x;
    float fpY = rect.y;
    float fpW = rect.w;
    float fpH = rect.h;

    bool isLightChassis = (chassisStyle == GuiChassisStyle::MinimalWhite || chassisStyle == GuiChassisStyle::Snes);
    float headerH = (!hideHeader && !isLightChassis) ? 44.0f : 0.0f;
    float topPadding = (headerH > 0.0f) ? (headerH + 12.0f) : 10.0f;
    float bottomPadding = 8.0f;
    float rowStartY = fpY + topPadding;

    bool isCompactSidebar = (fpW < 380.0f) && isClapPlugin;

    if (layoutMode == GuiLayoutMode::AbsoluteCanvas && !widgets.empty()) {
        if (isCompactSidebar) {
            auto macroWidgets = getMacroWidgets();
            std::vector<GuiWidgetDef*> activeMacros;
            if (!macroWidgets.empty()) {
                activeMacros = macroWidgets;
            } else {
                for (auto& w : widgets) {
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
                }
            }
        } else {
            float availW = fpW - (woodCheeks ? 24.0f : 0.0f);
            float availH = fpH - topPadding - bottomPadding;
            float dW = (designWidth > 10.0f) ? designWidth : 800.0f;
            float dH = (designHeight > 10.0f) ? designHeight : 480.0f;
            float scale = std::min(availW / dW, availH / dH);
            if (scale <= 0.001f) scale = 1.0f;

            float renderW = dW * scale;
            float renderH = dH * scale;
            float startX = fpX + (fpW - renderW) * 0.5f;
            float startY = fpY + topPadding + (availH - renderH) * 0.5f;

            for (size_t wIdx = 0; wIdx < widgets.size(); ++wIdx) {
                auto& w = widgets[wIdx];
                float origW = (w.designBounds.w > 0.0f) ? w.designBounds.w : w.designSize;
                float origH = (w.designBounds.h > 0.0f) ? w.designBounds.h : w.designSize;
                w.bounds = Rect2D{
                    startX + (w.designBounds.x * scale),
                    startY + (w.designBounds.y * scale),
                    origW * scale,
                    origH * scale
                };
                w.size = w.designSize * scale;
            }
        }
        return;
    }

    if (rows.empty()) return;

    // Multi-row AutoRowFlow declarative layout via Clay
    thread_local static clay::ClayContext s_panelClayCtx;
    if (!s_panelClayCtx.isInitialized()) {
        s_panelClayCtx.initialize(2048.0f, 2048.0f, 1024 * 1024);
    }
    s_panelClayCtx.setDimensions(fpW, fpH);
    s_panelClayCtx.beginLayout();

    float availH = fpH - topPadding - bottomPadding;
    float rowH = availH / std::max(1, static_cast<int>(rows.size()));

    CLAY(CLAY_ID("GuiFaceplateRoot"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(fpW), CLAY_SIZING_FIXED(fpH) },
            .padding = { 8, 8, static_cast<uint16_t>(topPadding), static_cast<uint16_t>(bottomPadding) },
            .childGap = 0,
            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        }
    }) {
        for (size_t rIdx = 0; rIdx < rows.size(); ++rIdx) {
            auto& row = rows[rIdx];
            size_t wCount = row.widgets.size();
            if (wCount == 0) continue;

            size_t dividerCount = 0;
            for (const auto& wid : row.widgets) {
                if (wid.type == GuiWidgetType::Divider) dividerCount++;
            }
            float rowContentW = fpW - 16.0f;
            float divWidth = std::clamp(rowContentW * 0.015f, 12.0f, 28.0f);
            float remainingW = rowContentW - (static_cast<float>(dividerCount) * divWidth);

            bool hasStackedRng = (wCount >= 3 &&
                                  row.widgets[wCount - 2].type == GuiWidgetType::NixieDisplay &&
                                  row.widgets[wCount - 1].type == GuiWidgetType::ActionButton);
            size_t leadCount = hasStackedRng ? (wCount - 2) : wCount;
            float rngColW = hasStackedRng ? std::clamp(remainingW * (leadCount == 2 ? 0.24f : 0.32f), 70.0f, 140.0f) : 0.0f;

            CLAY(CLAY_IDI("GuiFaceplateRow", static_cast<uint32_t>(rIdx)), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(rowH) },
                    .padding = { 0, 0, 2, 2 },
                    .childGap = 0,
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                }
            }) {
                for (size_t wIdx = 0; wIdx < leadCount; ++wIdx) {
                    const auto& w = row.widgets[wIdx];
                    uint32_t widgetId = static_cast<uint32_t>(rIdx * 1000 + wIdx);
                    if (w.type == GuiWidgetType::Divider) {
                        CLAY(CLAY_IDI("GuiWidget", widgetId), {
                            .layout = {
                                .sizing = { CLAY_SIZING_FIXED(divWidth), CLAY_SIZING_GROW(0) },
                            }
                        }) {}
                    } else {
                        CLAY(CLAY_IDI("GuiWidget", widgetId), {
                            .layout = {
                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                            }
                        }) {}
                    }
                }

                if (hasStackedRng) {
                    uint32_t colId = static_cast<uint32_t>(rIdx * 1000 + 999);
                    CLAY(CLAY_IDI("GuiStackedCol", colId), {
                        .layout = {
                            .sizing = { CLAY_SIZING_FIXED(rngColW), CLAY_SIZING_GROW(0) },
                            .childGap = 2,
                            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        }
                    }) {
                        uint32_t nixieId = static_cast<uint32_t>(rIdx * 1000 + leadCount);
                        CLAY(CLAY_IDI("GuiWidget", nixieId), {
                            .layout = {
                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                            }
                        }) {}
                        uint32_t btnId = static_cast<uint32_t>(rIdx * 1000 + leadCount + 1);
                        CLAY(CLAY_IDI("GuiWidget", btnId), {
                            .layout = {
                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                            }
                        }) {}
                    }
                }
            }
        }
    }

    s_panelClayCtx.endLayout(0.016f);

    for (size_t rIdx = 0; rIdx < rows.size(); ++rIdx) {
        auto rowData = s_panelClayCtx.getElementData(CLAY_IDI("GuiFaceplateRow", static_cast<uint32_t>(rIdx)));
        if (rowData.found) {
            rows[rIdx].bounds = Rect2D(fpX + rowData.boundingBox.x, fpY + rowData.boundingBox.y,
                                       rowData.boundingBox.width, rowData.boundingBox.height);
        } else {
            float ry = fpY + topPadding + (static_cast<float>(rIdx) * rowH);
            rows[rIdx].bounds = Rect2D(fpX + 8.0f, ry, fpW - 16.0f, rowH);
        }

        auto& row = rows[rIdx];
        size_t wCount = row.widgets.size();
        if (wCount == 0) continue;

        bool hasStackedRng = (wCount >= 3 &&
                              row.widgets[wCount - 2].type == GuiWidgetType::NixieDisplay &&
                              row.widgets[wCount - 1].type == GuiWidgetType::ActionButton);
        size_t leadCount = hasStackedRng ? (wCount - 2) : wCount;

        for (size_t wIdx = 0; wIdx < leadCount; ++wIdx) {
            uint32_t widgetId = static_cast<uint32_t>(rIdx * 1000 + wIdx);
            auto wData = s_panelClayCtx.getElementData(CLAY_IDI("GuiWidget", widgetId));
            if (wData.found) {
                row.widgets[wIdx].bounds = Rect2D(fpX + wData.boundingBox.x, fpY + wData.boundingBox.y,
                                                  wData.boundingBox.width, wData.boundingBox.height);
            }
        }

        if (hasStackedRng) {
            uint32_t nixieId = static_cast<uint32_t>(rIdx * 1000 + leadCount);
            auto nData = s_panelClayCtx.getElementData(CLAY_IDI("GuiWidget", nixieId));
            if (nData.found) {
                row.widgets[leadCount].bounds = Rect2D(fpX + nData.boundingBox.x, fpY + nData.boundingBox.y,
                                                       nData.boundingBox.width, nData.boundingBox.height);
            }

            uint32_t btnId = static_cast<uint32_t>(rIdx * 1000 + leadCount + 1);
            auto bData = s_panelClayCtx.getElementData(CLAY_IDI("GuiWidget", btnId));
            if (bData.found) {
                row.widgets[leadCount + 1].bounds = Rect2D(fpX + bData.boundingBox.x, fpY + bData.boundingBox.y,
                                                           bData.boundingBox.width, bData.boundingBox.height);
            }
        }
    }
}

static void drawSwitchSymbol(BatchRenderer2D& r, SwitchSymbol sym, float x, float y, float w, float h, float rCol, float gCol, float bCol, float aCol) {
    if (sym == SwitchSymbol::SquareWave) {
        // Crisp vector square pulse: [ _ | ¯ | _ | ¯ | _ ]
        float topY = y + 1.0f;
        float botY = y + h - 1.0f;
        float p0 = x;
        float p1 = x + w * 0.24f;
        float p2 = x + w * 0.50f;
        float p3 = x + w * 0.76f;
        float p4 = x + w;
        r.drawLine(p0, botY, p1, botY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p1, botY, p1, topY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p1, topY, p2, topY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p2, topY, p2, botY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p2, botY, p3, botY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p3, botY, p3, topY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p3, topY, p4, topY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p4, topY, p4, botY, rCol, gCol, bCol, aCol, 1.4f);
    } else if (sym == SwitchSymbol::SawWave) {
        // Crisp vector sawtooth ramp: [ /| /| ]
        float topY = y + 1.0f;
        float botY = y + h - 1.0f;
        float p0 = x;
        float p1 = x + w * 0.48f;
        float p2 = x + w * 0.98f;
        // First ramp
        r.drawLine(p0, botY, p1, topY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p1, topY, p1, botY, rCol, gCol, bCol, aCol, 1.4f);
        // Second ramp
        r.drawLine(p1, botY, p2, topY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p2, topY, p2, botY, rCol, gCol, bCol, aCol, 1.4f);
    } else if (sym == SwitchSymbol::SineWave) {
        float midY = y + h * 0.5f;
        float amp = (h - 2.0f) * 0.45f;
        constexpr int steps = 16;
        for (int i = 0; i < steps; ++i) {
            float t0 = static_cast<float>(i) / steps;
            float t1 = static_cast<float>(i + 1) / steps;
            float x0 = x + t0 * w;
            float y0 = midY - std::sin(t0 * 6.2831853f) * amp;
            float x1 = x + t1 * w;
            float y1 = midY - std::sin(t1 * 6.2831853f) * amp;
            r.drawLine(x0, y0, x1, y1, rCol, gCol, bCol, aCol, 1.4f);
        }
    } else if (sym == SwitchSymbol::TriangleWave) {
        float midY = y + h * 0.5f;
        float topY = y + 1.0f;
        float botY = y + h - 1.0f;
        float p0 = x;
        float p1 = x + w * 0.25f;
        float p2 = x + w * 0.75f;
        float p3 = x + w;
        r.drawLine(p0, midY, p1, topY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p1, topY, p2, botY, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(p2, botY, p3, midY, rCol, gCol, bCol, aCol, 1.4f);
    } else if (sym == SwitchSymbol::Power) {
        float cX = x + w * 0.5f;
        float cY = y + h * 0.5f;
        float rad = std::min(w, h) * 0.40f;
        r.drawArc(cX, cY, rad, -2.4f, 2.4f, rCol, gCol, bCol, aCol, 1.4f);
        r.drawLine(cX, cY - rad * 1.1f, cX, cY, rCol, gCol, bCol, aCol, 1.4f);
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
    panel.layoutControls(rect);

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
            bool isCurrentDragging = isDragging;
            bool isCurrentHovered = (isCurrentDragging || (!focusedWidgetId.empty() && focusedWidgetId == w.id));

            if (w.labelAlignment == KnobLayoutMode::InlineRight) {
                // SMAOLAB style (gui-variations): knob on left, value, label, and sublabel stacked on the right
                float knobAreaW = w.bounds.w * 0.48f;
                float rad = std::clamp(std::min((knobAreaW * 0.5f) - 6.0f, (w.bounds.h * 0.5f) - 8.0f) * 0.88f, 14.0f, 52.0f);
                float kcx = w.bounds.x + knobAreaW * 0.46f;
                float kcy = w.bounds.y + (w.bounds.h * 0.48f);

                drawKnobWidget(r, kcx, kcy, rad, norm, w, theme.globalLight, isCurrentDragging, isCurrentHovered, isLightChassis, &theme);

                float textX = kcx + rad + 10.0f;
                std::string valStr = w.getFormattedValue();

                // 1. Prominent Value readout
                drawText(r, valStr, textX, kcy - 12.0f, 12.5f,
                         isLightChassis ? 0.12f : 0.96f,
                         isLightChassis ? 0.14f : 0.98f,
                         isLightChassis ? 0.18f : 1.0f, 1.0f);

                // 2. Primary Label
                drawText(r, w.label, textX, kcy + 2.0f, 9.5f,
                         isLightChassis ? 0.35f : 0.72f,
                         isLightChassis ? 0.38f : 0.76f,
                         isLightChassis ? 0.44f : 0.82f, 0.95f);

                // 3. Optional Sublabel (e.g. "ANDALL", "12AX7", "FILTER", "7-UNISON")
                if (!w.sublabel.empty()) {
                    drawText(r, w.sublabel, textX, kcy + 13.5f, 8.0f,
                             isLightChassis ? 0.52f : 0.58f,
                             isLightChassis ? 0.56f : 0.62f,
                             isLightChassis ? 0.62f : 0.68f, 0.85f);
                }
                return;
            }

            if (w.labelAlignment == KnobLayoutMode::InlineLeft) {
                // Mirrored inline: value & label stacked on left, knob on right
                float knobAreaW = w.bounds.w * 0.48f;
                float rad = std::clamp(std::min((knobAreaW * 0.5f) - 6.0f, (w.bounds.h * 0.5f) - 8.0f) * 0.88f, 14.0f, 52.0f);
                float kcx = w.bounds.x + w.bounds.w - (knobAreaW * 0.46f);
                float kcy = w.bounds.y + (w.bounds.h * 0.48f);

                drawKnobWidget(r, kcx, kcy, rad, norm, w, theme.globalLight, isCurrentDragging, isCurrentHovered, isLightChassis, &theme);

                float textX = w.bounds.x + 8.0f;
                std::string valStr = w.getFormattedValue();

                drawText(r, valStr, textX, kcy - 12.0f, 12.5f,
                         isLightChassis ? 0.12f : 0.96f,
                         isLightChassis ? 0.14f : 0.98f,
                         isLightChassis ? 0.18f : 1.0f, 1.0f);

                drawText(r, w.label, textX, kcy + 2.0f, 9.5f,
                         isLightChassis ? 0.35f : 0.72f,
                         isLightChassis ? 0.38f : 0.76f,
                         isLightChassis ? 0.44f : 0.82f, 0.95f);

                if (!w.sublabel.empty()) {
                    drawText(r, w.sublabel, textX, kcy + 13.5f, 8.0f,
                             isLightChassis ? 0.52f : 0.58f,
                             isLightChassis ? 0.56f : 0.62f,
                             isLightChassis ? 0.62f : 0.68f, 0.85f);
                }
                return;
            }

            if (w.labelAlignment == KnobLayoutMode::BottomStacked || w.showValueBox) {
                // gui-astral style: knob on top, label below, and rounded pill badge box for value below
                float maxRadW = (w.bounds.w * 0.5f) - 6.0f;
                float maxRadH = (w.bounds.h * 0.33f) - 6.0f;
                float rad = std::clamp(std::min(maxRadW, maxRadH) * 0.88f, 14.0f, 50.0f);
                float kcx = w.bounds.x + (w.bounds.w * 0.5f);
                float kcy = w.bounds.y + rad + 8.0f;

                drawKnobWidget(r, kcx, kcy, rad, norm, w, theme.globalLight, isCurrentDragging, isCurrentHovered, isLightChassis, &theme);

                float labelY = kcy + rad + 7.0f;
                float labelH = 12.0f;
                drawCenteredText(r, w.label, w.bounds.x, labelY, w.bounds.w, labelH, 10.0f,
                                 isLightChassis ? 0.16f : 0.92f,
                                 isLightChassis ? 0.18f : 0.94f,
                                 isLightChassis ? 0.22f : 0.98f, 1.0f);

                std::string valStr = w.getFormattedValue();
                float valY = labelY + labelH + 3.0f;
                float pillW = std::clamp(static_cast<float>(valStr.size()) * 7.2f + 18.0f, 50.0f, w.bounds.w - 10.0f);
                float pillH = 16.0f;
                float pillX = kcx - pillW * 0.5f;

                r.drawRoundedRect(pillX, valY, pillW, pillH, 3.5f,
                                  isLightChassis ? 0.92f : 0.08f,
                                  isLightChassis ? 0.94f : 0.09f,
                                  isLightChassis ? 0.96f : 0.12f, 0.95f);
                r.drawRoundedRectOutline(pillX, valY, pillW, pillH, 3.5f,
                                         isLightChassis ? 0.72f : 0.24f,
                                         isLightChassis ? 0.75f : 0.28f,
                                         isLightChassis ? 0.80f : 0.36f, 0.85f, 1.0f);
                drawCenteredText(r, valStr, pillX, valY + 1.0f, pillW, pillH, 9.0f,
                                 isLightChassis ? 0.14f : 0.90f,
                                 isLightChassis ? 0.16f : 0.94f,
                                 isLightChassis ? 0.20f : 0.98f, 1.0f);
                return;
            }

            // StackedVertical (default):
            float maxRadW = (w.bounds.w * 0.5f) - 6.0f;
            float maxRadH = (w.bounds.h * 0.40f) - 8.0f;
            float rad = std::clamp(std::min(maxRadW, maxRadH) * 0.78f, 12.0f, 60.0f);

            bool isCurrentDraggingDefault = isDragging;
            bool isCurrentHoveredDefault = (isCurrentDraggingDefault || (!focusedWidgetId.empty() && focusedWidgetId == w.id));

            drawKnobWidget(r, cx, cy, rad, norm, w, theme.globalLight, isCurrentDraggingDefault, isCurrentHoveredDefault, isLightChassis, &theme);

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
            bool isOn = (w.currentVal > 0.5f);

            if (w.switchStyle == SwitchStyle::RectangularSlide) {
                // Authentic TB-303 Rectangular Slide Switch with corrugated thumb & symbols
                bool isVert = (w.switchOrientation == SwitchOrientation::Vertical);
                float maxW = (w.bounds.w * 0.55f);
                float maxH = (w.bounds.h * 0.50f);

                float bezelW = isVert ? std::clamp(std::min(maxW, maxH * 0.60f), 20.0f, 34.0f)
                                      : std::clamp(std::max(maxW, maxH * 1.50f), 38.0f, 66.0f);
                float bezelH = isVert ? bezelW * 1.85f
                                      : std::clamp(maxH, 20.0f, 34.0f);

                // If vertical switch has adjacent waveform symbols on the right, shift switch center left
                bool hasRightSymbols = isVert && (w.switchSymbolTop != SwitchSymbol::None || w.switchSymbolBottom != SwitchSymbol::None);
                float switchCx = hasRightSymbols ? (cx - std::clamp(w.bounds.w * 0.16f, 8.0f, 15.0f)) : cx;
                float switchCy = cy + 2.0f;

                float bezelX = switchCx - bezelW * 0.5f;
                float bezelY = switchCy - bezelH * 0.5f;

                // 1. Outer Beveled Bezel Frame
                r.drawRectDropShadow(bezelX, bezelY, bezelW, bezelH, 3.0f, 5.0f, theme.globalLight, 0.40f);
                drawRoundedRect(r, bezelX, bezelY, bezelW, bezelH, 3.0f, 0.74f, 0.76f, 0.80f, 1.0f);
                drawRoundedRectOutline(r, bezelX, bezelY, bezelW, bezelH, 3.0f, 0.95f, 0.96f, 0.98f, 0.85f, 1.0f);
                drawLine(r, bezelX + 1.0f, bezelY + bezelH, bezelX + bezelW - 1.0f, bezelY + bezelH, 0.35f, 0.36f, 0.40f, 0.9f, 1.5f);

                // 2. Recessed Dark Slot/Channel
                float slotPad = 4.0f;
                float slotX = bezelX + slotPad;
                float slotY = bezelY + slotPad;
                float slotW = bezelW - slotPad * 2.0f;
                float slotH = bezelH - slotPad * 2.0f;
                drawRoundedRect(r, slotX, slotY, slotW, slotH, 2.0f, 0.10f, 0.11f, 0.13f, 1.0f);
                drawRoundedRectOutline(r, slotX, slotY, slotW, slotH, 2.0f, 0.04f, 0.05f, 0.06f, 0.95f, 1.0f);

                // 3. Corrugated Ribbed Slider Thumb Handle
                float thumbW = isVert ? (slotW - 2.0f) : (slotW * 0.46f);
                float thumbH = isVert ? (slotH * 0.46f) : (slotH - 2.0f);

                float thumbX = 0.0f;
                float thumbY = 0.0f;

                if (isVert) {
                    thumbX = slotX + 1.0f;
                    // TB-303 convention: isOn (Square) = Top position, !isOn (Saw) = Bottom position
                    float topY = slotY + 1.0f;
                    float botY = slotY + slotH - thumbH - 1.0f;
                    thumbY = isOn ? topY : botY;
                } else {
                    thumbY = slotY + 1.0f;
                    float leftX = slotX + 1.0f;
                    float rightX = slotX + slotW - thumbW - 1.0f;
                    thumbX = isOn ? rightX : leftX;
                }

                // Thumb drop shadow inside the channel
                drawRoundedRect(r, thumbX + 0.5f, thumbY + 1.0f, thumbW, thumbH, 2.0f, 0.05f, 0.05f, 0.06f, 0.6f);

                // Thumb body: silver metallic block with 3D directional gradient
                r.drawRoundedRectGradient(thumbX, thumbY, thumbW, thumbH, 2.0f,
                                          0.84f, 0.86f, 0.90f,
                                          0.58f, 0.60f, 0.65f, 1.0f);
                drawRoundedRectOutline(r, thumbX, thumbY, thumbW, thumbH, 2.0f, 0.94f, 0.96f, 0.98f, 0.90f, 1.0f);

                // Corrugated Grip Ribs (4 tactile horizontal ridges)
                if (isVert) {
                    float ribStep = thumbH / 5.0f;
                    for (int rib = 1; rib <= 4; ++rib) {
                        float ry = thumbY + static_cast<float>(rib) * ribStep;
                        drawLine(r, thumbX + 2.0f, ry, thumbX + thumbW - 2.0f, ry, 0.22f, 0.24f, 0.28f, 0.9f, 1.2f);
                        drawLine(r, thumbX + 2.0f, ry - 0.9f, thumbX + thumbW - 2.0f, ry - 0.9f, 0.92f, 0.94f, 0.98f, 0.7f, 0.9f);
                    }
                } else {
                    float ribStep = thumbW / 5.0f;
                    for (int rib = 1; rib <= 4; ++rib) {
                        float rx = thumbX + static_cast<float>(rib) * ribStep;
                        drawLine(r, rx, thumbY + 2.0f, rx, thumbY + thumbH - 2.0f, 0.22f, 0.24f, 0.28f, 0.9f, 1.2f);
                        drawLine(r, rx - 0.9f, thumbY + 2.0f, rx - 0.9f, thumbY + thumbH - 2.0f, 0.92f, 0.94f, 0.98f, 0.7f, 0.9f);
                    }
                }

                // 4. Waveform / Switch Symbols
                float symCol = isLightChassis ? 0.35f : 0.78f;
                if (isVert) {
                    float symX = bezelX + bezelW + 5.0f;
                    float symW = std::clamp(w.bounds.w * 0.30f, 14.0f, 22.0f);
                    float symH = std::clamp(slotH * 0.36f, 10.0f, 16.0f);

                    if (w.switchSymbolTop != SwitchSymbol::None) {
                        float topSymY = slotY + 2.0f;
                        float curAlpha = isOn ? 1.0f : 0.45f;
                        drawSwitchSymbol(r, w.switchSymbolTop, symX, topSymY, symW, symH,
                                         symCol, symCol, symCol + 0.05f, curAlpha);
                    }
                    if (w.switchSymbolBottom != SwitchSymbol::None) {
                        float botSymY = slotY + slotH - symH - 2.0f;
                        float curAlpha = !isOn ? 1.0f : 0.45f;
                        drawSwitchSymbol(r, w.switchSymbolBottom, symX, botSymY, symW, symH,
                                         symCol, symCol, symCol + 0.05f, curAlpha);
                    }
                } else {
                    float symY = bezelY + bezelH + 3.0f;
                    float symW = std::clamp(bezelW * 0.38f, 14.0f, 20.0f);
                    float symH = 10.0f;
                    if (w.switchSymbolTop != SwitchSymbol::None) {
                        drawSwitchSymbol(r, w.switchSymbolTop, bezelX, symY, symW, symH,
                                         symCol, symCol, symCol + 0.05f, !isOn ? 1.0f : 0.45f);
                    }
                    if (w.switchSymbolBottom != SwitchSymbol::None) {
                        drawSwitchSymbol(r, w.switchSymbolBottom, bezelX + bezelW - symW, symY, symW, symH,
                                         symCol, symCol, symCol + 0.05f, isOn ? 1.0f : 0.45f);
                    }
                }

                // 5. Label ABOVE switch
                float labelY = bezelY - std::clamp(bezelH * 0.34f, 14.0f, 26.0f);
                float labelH = std::clamp(bezelH * 0.28f, 12.0f, 20.0f);
                float labelFontSize = std::clamp(bezelW * 0.38f, 7.5f, 13.0f);
                drawCenteredText(r, w.label, w.bounds.x, labelY, w.bounds.w, labelH, labelFontSize,
                                 isLightChassis ? 0.20f : 0.85f,
                                 isLightChassis ? 0.22f : 0.88f,
                                 isLightChassis ? 0.26f : 0.92f, 1.0f);

            } else {
                // Authentic metal bat toggle switch with golden collar on dark pill bezel
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
            }

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

    bool isCompactSidebar = (fpW < 380.0f) && panel.isClapPlugin;
    if (panel.layoutMode == GuiLayoutMode::AbsoluteCanvas && !panel.widgets.empty()) {
        if (isCompactSidebar) {
            // Render Compact Macro Strip for sidebar context
            auto macroWidgets = panel.getMacroWidgets();
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
            for (size_t i = 0; i < activeMacros.size(); ++i) {
                bool isDragging = (draggingRow == 0 && draggingWidget == static_cast<int>(i));
                drawSingleWidget(*activeMacros[i], isDragging);
            }
        } else {
            for (size_t wIdx = 0; wIdx < panel.widgets.size(); ++wIdx) {
                auto& w = panel.widgets[wIdx];
                bool isDragging = (draggingRow == 0 && draggingWidget == static_cast<int>(wIdx));
                drawSingleWidget(w, isDragging);
            }
        }
    } else {
        // Multi-row AutoRowFlow layout rendered from Clay layout
        for (size_t rIdx = 0; rIdx < panel.rows.size(); ++rIdx) {
            auto& row = panel.rows[rIdx];
            for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                auto& w = row.widgets[wIdx];
                bool isDragging = (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(wIdx));
                drawSingleWidget(w, isDragging);
            }
        }
    }

    // Subtle horizontal section dividers & studio badge for minimal white / studio chassis
    if (isLightChassis && panel.rows.size() > 1) {
        for (size_t rIdx = 1; rIdx < panel.rows.size(); ++rIdx) {
            float divY = rowStartY + static_cast<float>(rIdx) * rowH;
            float padX = 20.0f;
            drawLine(r, fpX + padX, divY, fpX + fpW - padX, divY,
                     0.75f, 0.78f, 0.84f, 0.65f, 1.0f);
        }
    }

    if (isLightChassis) {
        std::string_view badgeText = (panel.title.find("ASTRAL") != std::string::npos ||
                                      panel.title.find("Astral") != std::string::npos ||
                                      panel.subtitle.find("Ambient") != std::string::npos)
                                     ? "ASTRAL // VER 2.0"
                                     : "SMAOLAB // VER 2.0";
        drawText(r, badgeText, fpX + 16.0f, fpY + fpH - 14.0f, 8.5f, 0.85f, 0.20f, 0.25f, 0.85f);
    }
}

} // namespace eatsbits::ui
