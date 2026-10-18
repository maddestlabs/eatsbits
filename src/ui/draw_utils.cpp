#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::ui {

void drawText(BatchRenderer2D& r, std::string_view str, float x, float y, float size,
              float red, float green, float blue, float alpha) {
    float scale = std::max(0.6f, size / 13.0f);
    drawVectorString(r, str, x, y, scale, red, green, blue, alpha);
}

void drawCenteredText(BatchRenderer2D& r, std::string_view str, const Rect2D& bounds,
                      float size, const Color& col, float alpha) {
    float scale = std::max(0.6f, size / 13.0f);
    float cx = bounds.x + bounds.w * 0.5f;
    float cy = bounds.y + bounds.h * 0.5f;
    drawVectorStringCentered(r, str, cx, cy, scale, col.r, col.g, col.b, alpha * col.a);
}

void drawRotatedText(BatchRenderer2D& r, std::string_view str, float cx, float cy,
                     float angleDegrees, float size, const Color& col, float alpha) {
    float scale = std::max(0.6f, size / 13.0f);
    r.setRotation(angleDegrees, cx, cy);
    drawVectorStringCentered(r, str, cx, cy, scale, col.r, col.g, col.b, alpha * col.a);
    r.resetRotation();
}

void drawButton(BatchRenderer2D& r, const Rect2D& bounds, std::string_view label,
                const Color& bgCol, const Color& borderCol, const Color& textCol,
                float size, float cornerRadius, float borderWidth) {
    if (cornerRadius > 0.5f) {
        drawRoundedRect(r, bounds.x, bounds.y, bounds.w, bounds.h, cornerRadius, bgCol);
        if (borderWidth > 0.0f && borderCol.a > 0.001f) {
            drawRoundedRectOutline(r, bounds.x, bounds.y, bounds.w, bounds.h, cornerRadius, borderCol, 1.0f, borderWidth);
        }
    } else {
        drawRect(r, bounds.x, bounds.y, bounds.w, bounds.h, bgCol);
        if (borderWidth > 0.0f && borderCol.a > 0.001f) {
            drawRect(r, bounds.x, bounds.y, bounds.w, borderWidth, borderCol);
            drawRect(r, bounds.x, bounds.y + bounds.h - borderWidth, bounds.w, borderWidth, borderCol);
            drawRect(r, bounds.x, bounds.y, borderWidth, bounds.h, borderCol);
            drawRect(r, bounds.x + bounds.w - borderWidth, bounds.y, borderWidth, bounds.h, borderCol);
        }
    }
    if (!label.empty()) {
        drawCenteredText(r, label, bounds, size, textCol);
    }
}

void drawPianoIcon(BatchRenderer2D& r, float x, float y, float w, float h, const Color& accentCol) {
    // Piano chassis / outline
    drawRoundedRect(r, x, y, w, h, 2.0f, 0.90f, 0.92f, 0.95f, 1.0f);
    drawRoundedRectOutline(r, x, y, w, h, 2.0f, accentCol.r, accentCol.g, accentCol.b, 0.9f, 1.0f);

    // 4 White keys dividers
    float keyW = w / 4.0f;
    for (int k = 1; k < 4; ++k) {
        float kx = x + static_cast<float>(k) * keyW;
        drawLine(r, kx, y + 2.0f, kx, y + h - 1.0f, 0.5f, 0.55f, 0.60f, 0.7f, 1.0f);
    }

    // 3 Black keys: at boundaries kx1, kx2, kx3
    float bkW = std::max(1.5f, keyW * 0.45f);
    float bkH = h * 0.55f;
    float bkOffsets[3] = { keyW, 2.0f * keyW, 3.0f * keyW };
    for (float off : bkOffsets) {
        float bx = x + off - (bkW * 0.5f);
        drawRect(r, bx, y + 1.0f, bkW, bkH, 0.12f, 0.14f, 0.18f, 1.0f);
    }
}

void drawPropertiesIcon(BatchRenderer2D& r, float x, float y, float w, float h, const Color& accentCol) {
    // 3 horizontal slider tracks with knobs representing track / channel properties
    float lineY0 = y + h * 0.22f;
    float lineY1 = y + h * 0.50f;
    float lineY2 = y + h * 0.78f;

    // Background tracks
    drawLine(r, x, lineY0, x + w, lineY0, 0.45f, 0.50f, 0.60f, 0.6f, 1.0f);
    drawLine(r, x, lineY1, x + w, lineY1, 0.45f, 0.50f, 0.60f, 0.6f, 1.0f);
    drawLine(r, x, lineY2, x + w, lineY2, 0.45f, 0.50f, 0.60f, 0.6f, 1.0f);

    // Knobs at positions: 30%, 75%, 45%
    float knobW = std::max(3.0f, w * 0.26f);
    float knobH = std::max(4.0f, h * 0.28f);

    auto drawKnob = [&](float kx, float ky) {
        drawRoundedRect(r, kx - (knobW * 0.5f), ky - (knobH * 0.5f), knobW, knobH, 1.5f,
                        accentCol.r, accentCol.g, accentCol.b, 1.0f);
    };

    drawKnob(x + w * 0.30f, lineY0);
    drawKnob(x + w * 0.75f, lineY1);
    drawKnob(x + w * 0.45f, lineY2);
}

void drawMonoText(BatchRenderer2D& r, std::string_view str, float x, float y, float size,
                  float red, float green, float blue, float alpha) {
    float scale = std::max(0.6f, size / 13.0f);
    drawMonoString(r, str, x, y, scale, red, green, blue, alpha);
}

void drawMonoTextClipped(BatchRenderer2D& r, std::string_view str, float x, float y, float size,
                         float clipMinX, float clipMaxX,
                         float red, float green, float blue, float alpha) {
    if (clipMaxX <= clipMinX || str.empty()) return;
    float charW = getMonoCharAdvance(size);
    float scale = std::max(0.6f, size / 13.0f);

    for (size_t i = 0; i < str.length(); ++i) {
        float charX = x + static_cast<float>(i) * charW;
        if (charX + charW > clipMinX && charX < clipMaxX) {
            std::string_view singleChar = str.substr(i, 1);
            drawMonoString(r, singleChar, charX, y, scale, red, green, blue, alpha);
        }
    }
}

float getMonoCharAdvance(float size) {
    float scale = std::max(0.6f, size / 13.0f);
    float fontSize = std::max(11.0f, scale * 15.5f);
    return fontSize * 0.588f;
}

void drawScrewCloseButton(BatchRenderer2D& r, float cx, float cy, float radius, bool hovered, const Color& highlightColor) {
    // 1. Recessed outer countersink well shadow
    drawCircle(r, cx, cy, radius + 1.2f, Color(0.04f, 0.03f, 0.04f, 0.95f));
    drawCircle(r, cx, cy, radius + 0.4f, Color(0.08f, 0.08f, 0.09f, 0.85f));

    if (hovered) {
        // Outer aura and halo ring in active theme highlight color
        drawCircle(r, cx, cy, radius + 2.2f, Color(highlightColor.r, highlightColor.g, highlightColor.b, 0.22f));
        drawCircleOutline(r, cx, cy, radius + 1.5f, highlightColor.withAlpha(0.70f), 1.0f);
    }

    // 2. Metallic screw head body with highlight tint on hover
    Color screwBody = hovered
        ? Color::lerp(Color(0.28f, 0.30f, 0.36f), highlightColor, 0.22f)
        : Color(0.18f, 0.20f, 0.23f);
    drawCircle(r, cx, cy, radius, screwBody);

    // 3. Chamfer highlight on top-left edge
    drawCircle(r, cx - 0.5f, cy - 0.5f, radius * 0.85f, Color(1.0f, 1.0f, 1.0f, hovered ? 0.35f : 0.15f));
    drawCircle(r, cx + 0.2f, cy + 0.2f, radius * 0.85f, screwBody);

    // 4. Inset Phillips cross slot ("X")
    float arm = radius * 0.55f;
    Color slotColor = hovered ? highlightColor : Color(0.06f, 0.06f, 0.08f);
    float strokeW = std::max(1.3f, radius * (hovered ? 0.32f : 0.28f));

    drawLine(r, cx - arm, cy - arm, cx + arm, cy + arm, slotColor, 1.0f, strokeW);
    drawLine(r, cx - arm, cy + arm, cx + arm, cy - arm, slotColor, 1.0f, strokeW);

    if (hovered) {
        drawCircle(r, cx, cy, std::max(1.5f, radius * 0.25f), highlightColor);
    }
}

void drawIconEdit(BatchRenderer2D& r, float x, float y, float size, const Color& c) {
    float s = size;

    // 1. Stylus Silhouette (45-deg chisel stylus pointing down to baseline)
    float p0x = x + 0.93f * s, p0y = y + 0.27f * s; // top right
    float p1x = x + 0.74f * s, p1y = y + 0.08f * s; // top left (cap)
    float p2x = x + 0.30f * s, p2y = y + 0.48f * s; // shaft left
    float p3x = x + 0.30f * s, p3y = y + 0.70f * s; // chisel tip bottom-left
    float p4x = x + 0.52f * s, p4y = y + 0.70f * s; // chisel tip bottom-right

    // Convex decomposition of 5-gon stylus
    drawTriangle(r, p0x, p0y, p1x, p1y, p2x, p2y, c);
    drawTriangle(r, p0x, p0y, p2x, p2y, p4x, p4y, c);
    drawTriangle(r, p2x, p2y, p3x, p3y, p4x, p4y, c);

    // 2. Diamond cutout near cap
    if (s >= 8.0f) {
        float cx = x + 0.75f * s;
        float cy = y + 0.25f * s;
        float d = std::max(1.0f, 0.065f * s);
        Color bg(0.08f, 0.09f, 0.11f, c.a);
        drawTriangle(r, cx, cy - d, cx + d, cy, cx, cy + d, bg);
        drawTriangle(r, cx, cy - d, cx - d, cy, cx, cy + d, bg);
    }

    // 3. Baseline underline bar
    float barH = std::max(1.5f, 0.14f * s);
    float barY = y + 0.78f * s;
    float barX = x + 0.06f * s;
    float barW = 0.88f * s;

    float filledW = barW * 0.68f;
    drawRect(r, barX, barY, filledW, barH, c.r, c.g, c.b, c.a);

    float emptyX = barX + filledW;
    float emptyW = barW - filledW;
    float strokeW = std::max(1.0f, 0.06f * s);
    drawLine(r, emptyX, barY + strokeW * 0.5f, emptyX + emptyW, barY + strokeW * 0.5f, c, 1.0f, strokeW);
    drawLine(r, emptyX, barY + barH - strokeW * 0.5f, emptyX + emptyW, barY + barH - strokeW * 0.5f, c, 1.0f, strokeW);
    drawLine(r, emptyX + emptyW - strokeW * 0.5f, barY, emptyX + emptyW - strokeW * 0.5f, barY + barH, c, 1.0f, strokeW);
}

void drawDesignChipIcon(BatchRenderer2D& r, float x, float y, float size, const Color& c) {
    float chipW = size * 0.74f;
    float chipH = size * 0.86f;
    float chipX = x + size * 0.06f;
    float chipY = y + (size - chipH) * 0.5f;

    // 1. Pins on right edge (3 pins projecting outward)
    float pinW = size * 0.16f;
    float pinH = size * 0.13f;
    float pinX = chipX + chipW - 0.5f;
    float pinY0 = chipY + chipH * 0.18f;
    float pinY1 = chipY + chipH * 0.44f;
    float pinY2 = chipY + chipH * 0.70f;

    drawRoundedRect(r, pinX, pinY0, pinW, pinH, 1.5f, c);
    drawRoundedRect(r, pinX, pinY1, pinW, pinH, 1.5f, c);
    drawRoundedRect(r, pinX, pinY2, pinW, pinH, 1.5f, c);

    // 2. Outer chip body
    float cornerR = 2.8f;
    float borderW = std::max(1.2f, size * 0.11f);
    drawRoundedRectOutline(r, chipX, chipY, chipW, chipH, cornerR, c, 1.0f, borderW);

    // 3. Four internal silicon die pads (2x2 grid separated by cross channel)
    float innerMarginX = borderW + 1.2f;
    float innerMarginY = borderW + 1.2f;
    float innerW = chipW - (innerMarginX * 2.0f);
    float innerH = chipH - (innerMarginY * 2.0f);
    if (innerW > 3.0f && innerH > 3.0f) {
        float gutter = std::max(1.0f, size * 0.09f);
        float padW = (innerW - gutter) * 0.5f;
        float padH = (innerH - gutter) * 0.5f;

        float padX0 = chipX + innerMarginX;
        float padX1 = padX0 + padW + gutter;
        float padY0 = chipY + innerMarginY;
        float padY1 = padY0 + padH + gutter;

        drawRoundedRect(r, padX0, padY0, padW, padH, 1.0f, c);
        drawRoundedRect(r, padX1, padY0, padW, padH, 1.0f, c);
        drawRoundedRect(r, padX0, padY1, padW, padH, 1.0f, c);
        drawRoundedRect(r, padX1, padY1, padW, padH, 1.0f, c);
    }
}

void drawSlidersTuneIcon(BatchRenderer2D& r, float x, float y, float size, const Color& c) {
    float strokeW = std::max(1.0f, size * 0.10f);
    float trackH = size * 0.80f;
    float trackY0 = y + (size - trackH) * 0.5f;
    float trackY1 = trackY0 + trackH;

    float col0 = x + size * 0.22f;
    float col1 = x + size * 0.50f;
    float col2 = x + size * 0.78f;

    // 3 vertical slider tracks
    drawLine(r, col0, trackY0, col0, trackY1, c, 0.65f, strokeW);
    drawLine(r, col1, trackY0, col1, trackY1, c, 0.65f, strokeW);
    drawLine(r, col2, trackY0, col2, trackY1, c, 0.65f, strokeW);

    // 3 slider thumb bars
    float thumbW = size * 0.32f;
    float thumbH = std::max(2.0f, size * 0.18f);

    float t0Y = trackY0 + trackH * 0.65f - thumbH * 0.5f;
    float t1Y = trackY0 + trackH * 0.28f - thumbH * 0.5f;
    float t2Y = trackY0 + trackH * 0.50f - thumbH * 0.5f;

    drawRoundedRect(r, col0 - thumbW * 0.5f, t0Y, thumbW, thumbH, 1.0f, c);
    drawRoundedRect(r, col1 - thumbW * 0.5f, t1Y, thumbW, thumbH, 1.0f, c);
    drawRoundedRect(r, col2 - thumbW * 0.5f, t2Y, thumbW, thumbH, 1.0f, c);
}

void drawFullscreenIcon(BatchRenderer2D& r, float x, float y, float size, const Color& c, float thickness) {
    float arm = size * 0.32f;
    float t = std::max(1.0f, thickness);
    // Top-left corner
    drawLine(r, x, y + t * 0.5f, x + arm, y + t * 0.5f, c, 1.0f, t);
    drawLine(r, x + t * 0.5f, y, x + t * 0.5f, y + arm, c, 1.0f, t);
    // Top-right corner
    drawLine(r, x + size - arm, y + t * 0.5f, x + size, y + t * 0.5f, c, 1.0f, t);
    drawLine(r, x + size - t * 0.5f, y, x + size - t * 0.5f, y + arm, c, 1.0f, t);
    // Bottom-left corner
    drawLine(r, x, y + size - t * 0.5f, x + arm, y + size - t * 0.5f, c, 1.0f, t);
    drawLine(r, x + t * 0.5f, y + size - arm, x + t * 0.5f, y + size, c, 1.0f, t);
    // Bottom-right corner
    drawLine(r, x + size - arm, y + size - t * 0.5f, x + size, y + size - t * 0.5f, c, 1.0f, t);
    drawLine(r, x + size - t * 0.5f, y + size - arm, x + size - t * 0.5f, y + size, c, 1.0f, t);
}

void drawChevronUp(BatchRenderer2D& r, float cx, float cy, float size, const Color& c, float thickness) {
    float hw = size * 0.5f;
    float hh = size * 0.35f;
    float t = std::max(1.0f, thickness);
    drawLine(r, cx - hw, cy + hh, cx, cy - hh, c, 1.0f, t);
    drawLine(r, cx, cy - hh, cx + hw, cy + hh, c, 1.0f, t);
}

void drawChevronDown(BatchRenderer2D& r, float cx, float cy, float size, const Color& c, float thickness) {
    float hw = size * 0.5f;
    float hh = size * 0.35f;
    float t = std::max(1.0f, thickness);
    drawLine(r, cx - hw, cy - hh, cx, cy + hh, c, 1.0f, t);
    drawLine(r, cx, cy + hh, cx + hw, cy - hh, c, 1.0f, t);
}

void drawTrashIcon(BatchRenderer2D& r, float cx, float cy, float size, const Color& c, float thickness) {
    float t = std::max(1.0f, thickness);
    float w = size * 0.72f;
    float h = size * 0.88f;
    float left = cx - w * 0.5f;
    float top = cy - h * 0.5f;

    // Top handle
    float handleW = w * 0.36f;
    float handleH = h * 0.16f;
    drawLine(r, cx - handleW * 0.5f, top, cx + handleW * 0.5f, top, c, 1.0f, t);
    drawLine(r, cx - handleW * 0.5f, top, cx - handleW * 0.5f, top + handleH, c, 1.0f, t);
    drawLine(r, cx + handleW * 0.5f, top, cx + handleW * 0.5f, top + handleH, c, 1.0f, t);

    // Lid rim
    float rimY = top + handleH;
    drawLine(r, left - 1.5f, rimY, left + w + 1.5f, rimY, c, 1.0f, t);

    // Can body (slightly tapered at bottom)
    float inset = w * 0.10f;
    float canTopY = rimY + 1.0f;
    float canBotY = top + h;
    drawLine(r, left, canTopY, left + inset, canBotY, c, 1.0f, t);
    drawLine(r, left + w, canTopY, left + w - inset, canBotY, c, 1.0f, t);
    drawLine(r, left + inset, canBotY, left + w - inset, canBotY, c, 1.0f, t);

    // Internal vertical ribs
    float midX1 = left + w * 0.38f;
    float midX2 = left + w * 0.62f;
    drawLine(r, midX1, canTopY + 2.0f, midX1, canBotY - 2.0f, c, 0.75f, t * 0.85f);
    drawLine(r, midX2, canTopY + 2.0f, midX2, canBotY - 2.0f, c, 0.75f, t * 0.85f);
}

void drawTooltipBadge(BatchRenderer2D& r, std::string_view text, float targetX, float targetY,
                      const ThemeTokens& theme, bool placeBelow, float screenW,
                      float screenH, float minY, float maxY) {
    if (text.empty()) return;

    float tw = static_cast<float>(text.length()) * 6.5f + 18.0f;
    float th = 20.0f;
    float tx = targetX - tw * 0.5f;

    const float effW = (screenW > 0.0f) ? screenW : r.getCurrentWidth();
    const float effH = (screenH > 0.0f) ? screenH : r.getCurrentHeight();

    // Boundary constraints for X
    if (effW > 0.0f) {
        tx = std::clamp(tx, 8.0f, std::max(8.0f, effW - tw - 8.0f));
    }

    float ty = 0.0f;

    // Optical Domain Separation:
    // 1. Top Transport Bar domain: targetY < 50.0f or explicitly bounded (maxY <= 56.0f)
    // Tooltips remain strictly within the top panel (above the elements, y in [3.0, 53.0])
    // and NEVER enter the DAW / CRT region (y >= 56.0f).
    if (targetY < 50.0f || (maxY > 0.0f && maxY <= 56.0f)) {
        float topMin = (minY >= 0.0f) ? minY : 0.0f;
        float topMax = (maxY > 0.0f) ? maxY : 54.0f;
        float desiredY = targetY - th - 3.0f;
        ty = std::clamp(desiredY, topMin + 3.0f, std::max(topMin + 3.0f, topMax - th));
    }
    // 2. Central DAW Workspace domain: targetY >= 50.0f
    // Tooltips remain strictly within the DAW workspace (y in [58.0, effH - 51.0])
    // and NEVER cross into the top transport faceplate (y < 56.0f) or bottom chin (y > effH - 48.0f).
    else {
        float dawMinY = (minY >= 0.0f) ? minY : 58.0f;
        float dawMaxY = (maxY > 0.0f) ? maxY : ((effH > 0.0f) ? (effH - 51.0f) : 9999.0f);

        if (!placeBelow) {
            ty = targetY - th - 8.0f;
            // If placing above would breach the top transport bar (56px) or CRT aperture bezel,
            // automatically flip below the target
            if (ty < dawMinY) {
                ty = targetY + 22.0f;
            }
        } else {
            ty = targetY + 14.0f;
            // If placing below would breach the bottom chin, flip above the target
            if (ty + th > dawMaxY) {
                ty = targetY - th - 8.0f;
            }
        }

        ty = std::clamp(ty, dawMinY, std::max(dawMinY, dawMaxY - th));
    }

    // Drop shadow
    drawRoundedRect(r, tx + 1.0f, ty + 2.0f, tw, th, 4.0f, 0.0f, 0.0f, 0.0f, 0.45f);

    if (theme.isLight) {
        // High-contrast tactile dark badge in light mode
        drawRoundedRect(r, tx, ty, tw, th, 4.0f, 0.12f, 0.13f, 0.16f, 0.96f);
        drawRoundedRectOutline(r, tx, ty, tw, th, 4.0f, 0.30f, 0.32f, 0.38f, 0.8f, 1.0f);
        drawText(r, text, tx + 9.0f, ty + 4.0f, 9.5f, 0.95f, 0.96f, 0.98f, 1.0f);
    } else {
        // Creamy studio label badge in dark mode (Eatsbeats analog hardware tape aesthetic)
        drawRoundedRect(r, tx, ty, tw, th, 4.0f, 0.94f, 0.95f, 0.96f, 0.98f);
        drawRoundedRectOutline(r, tx, ty, tw, th, 4.0f, 0.60f, 0.64f, 0.70f, 0.85f, 1.0f);
        drawLine(r, tx + 2.0f, ty + 1.0f, tx + tw - 2.0f, ty + 1.0f,
                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.6f, 1.0f);
        drawText(r, text, tx + 9.0f, ty + 4.0f, 9.5f, 0.10f, 0.12f, 0.15f, 1.0f);
    }
}

void drawUnifiedKnob(BatchRenderer2D& r, float cx, float cy, float radius, float normVal,
                     const KnobStyleCore& style,
                     std::string_view label, std::string_view valueText,
                     const ThemeTokens* theme,
                     bool isActive, bool isHovered) {
    constexpr float kMinAngle = -2.35619449f; // -135 degrees
    constexpr float kMaxAngle =  2.35619449f; // +135 degrees
    float clampedNorm = std::clamp(normVal, 0.0f, 1.0f);
    float currentAngle = kMinAngle + clampedNorm * (kMaxAngle - kMinAngle);

    bool isLight = theme ? theme->isLight : false;
    bool isMinimal = (theme && theme->controlMode == ControlAestheticMode::ForceMinimal) ||
                     (style.surface == RotorSurface::FlatVector);

    Color defaultAccent = theme ? theme->primaryAccent : Color(0.18f, 0.85f, 0.95f, 1.0f);
    Color arcCol = style.arcColor.value_or(defaultAccent);
    if (isActive) {
        arcCol = Color(1.0f, 0.88f, 0.12f, 1.0f);
    } else if (isHovered) {
        arcCol = arcCol.lighten(0.18f);
    }

    // 1. Background Halo / Track Arc
    float arcR = radius + style.arcRadiusOffset;
    if (style.arcType != KnobArcType::None && style.arcThickness > 0.0f) {
        Color trackCol = style.trackColor.value_or(
            isLight ? Color(0.78f, 0.80f, 0.84f, 0.60f) : Color(0.15f, 0.16f, 0.19f, 0.65f)
        );
        r.drawArc(cx, cy, arcR, kMinAngle, kMaxAngle,
                  trackCol.r, trackCol.g, trackCol.b, trackCol.a, style.arcThickness);

        // Active parameter value arc
        if (style.arcType == KnobArcType::BipolarCenterZero) {
            float centerAngle = kMinAngle + style.centerDetentVal * (kMaxAngle - kMinAngle);
            float a0 = std::min(centerAngle, currentAngle);
            float a1 = std::max(centerAngle, currentAngle);
            if (std::abs(a1 - a0) > 0.015f) {
                r.drawArc(cx, cy, arcR, a0, a1, arcCol.r, arcCol.g, arcCol.b, arcCol.a,
                          isActive ? (style.arcThickness + 0.6f) : style.arcThickness);
            }
        } else if (style.arcType == KnobArcType::Unipolar) {
            if (clampedNorm > 0.005f) {
                r.drawArc(cx, cy, arcR, kMinAngle, currentAngle, arcCol.r, arcCol.g, arcCol.b, arcCol.a,
                          isActive ? (style.arcThickness + 0.6f) : style.arcThickness);
            }
        }
    }

    // 2. Center Detent Indicator Tick
    if (style.hasCenterDetent) {
        Color detentCol = isLight ? Color(0.35f, 0.38f, 0.44f, 0.85f) : Color(0.65f, 0.68f, 0.74f, 0.85f);
        float tickY0 = cy - radius - 1.0f;
        float tickY1 = cy - arcR - 2.0f;
        r.drawLine(cx, tickY0, cx, tickY1, detentCol.r, detentCol.g, detentCol.b, detentCol.a, 1.2f);
    }

    // 3. Drop Shadow onto chassis (bypassed in minimal mode)
    if (!isMinimal && (!theme || theme->showControlDropShadows)) {
        r.drawCircle(cx + 0.8f, cy + 1.8f, radius * 1.03f, 0.0f, 0.0f, 0.0f, 0.30f, 28);
    }

    // 4. Rotor Body (Filmstrip or Procedural Mesh)
    if (style.filmstripTexture > 0 && style.filmstripFrames > 0) {
        int frameIndex = std::clamp(static_cast<int>(clampedNorm * (style.filmstripFrames - 1) + 0.5f),
                                    0, style.filmstripFrames - 1);
        r.drawFilmstripFrame(style.filmstripTexture,
                             cx - radius, cy - radius, radius * 2.0f, radius * 2.0f,
                             frameIndex, style.filmstripFrames, style.filmstripHorizontal);
    } else if (isMinimal || style.surface == RotorSurface::FlatVector) {
        // Flat vector aesthetic (gui-minimal / Bitwig / Ableton)
        Color bodyCol = style.bodyColor.value_or(
            isLight ? Color(0.88f, 0.89f, 0.92f, 1.0f) : Color(0.12f, 0.13f, 0.15f, 1.0f)
        );
        r.drawCircle(cx, cy, radius, bodyCol.r, bodyCol.g, bodyCol.b, bodyCol.a, 32);
        Color rimCol = isLight ? Color(0.70f, 0.72f, 0.76f, 0.85f) : Color(0.24f, 0.26f, 0.30f, 0.85f);
        r.drawCircleOutline(cx, cy, radius, rimCol.r, rimCol.g, rimCol.b, rimCol.a, 1.0f, 32);
    } else if (style.surface == RotorSurface::BrushedAluminum) {
        // Brushed aluminum finish (gui-variations / SMAOLAB)
        r.drawCircleRadial3StopGradient(cx, cy, radius,
                                        0.96f, 0.97f, 0.99f, 1.0f,
                                        0.84f, 0.85f, 0.88f, 1.0f,
                                        0.68f, 0.70f, 0.74f, 1.0f,
                                        -radius * 0.25f, -radius * 0.25f, 0.50f, 32);
        r.drawCircleOutline(cx, cy, radius, 0.35f, 0.37f, 0.42f, 0.80f, 1.0f, 32);
        // Inner lathe concentric chamfer
        r.drawCircleOutline(cx, cy, radius * 0.65f, 1.0f, 1.0f, 1.0f, 0.25f, 0.8f, 24);
    } else {
        // Modern Studio Subtle Gradient
        Color baseCol = style.bodyColor.value_or(
            isLight ? Color(0.85f, 0.87f, 0.90f, 1.0f) : Color(0.16f, 0.17f, 0.20f, 1.0f)
        );
        Color hlCol = baseCol.lighten(isLight ? 0.15f : 0.28f);
        Color shCol = baseCol.darken(isLight ? 0.22f : 0.35f);
        r.drawCircleRadial3StopGradient(cx, cy, radius,
                                        hlCol.r, hlCol.g, hlCol.b, 1.0f,
                                        baseCol.r, baseCol.g, baseCol.b, 1.0f,
                                        shCol.r, shCol.g, shCol.b, 1.0f,
                                        -radius * 0.22f, -radius * 0.22f, 0.50f, 32);
        r.drawCircleOutline(cx, cy, radius, shCol.r, shCol.g, shCol.b, 0.75f, 1.0f, 32);
    }

    // 5. Pointer Needle Line (Bypassed if filmstrip renders knob body unless pointer is explicit)
    if (style.filmstripTexture == 0 || style.pointerColor.has_value()) {
        float sinA = std::sin(currentAngle);
        float cosA = std::cos(currentAngle);
        float pStart = radius * 0.12f;
        float pEnd   = radius * std::clamp(style.pointerLengthRatio, 0.30f, 0.95f);

        Color ptrCol = style.pointerColor.value_or(
            isActive ? Color(1.0f, 0.92f, 0.18f, 1.0f) :
            (isHovered ? Color(0.40f, 0.95f, 1.0f, 1.0f) :
            (isLight ? Color(0.12f, 0.13f, 0.16f, 0.95f) : Color(0.96f, 0.97f, 0.99f, 0.95f)))
        );

        r.drawLine(cx + sinA * pStart, cy - cosA * pStart,
                   cx + sinA * pEnd,   cy - cosA * pEnd,
                   ptrCol.r, ptrCol.g, ptrCol.b, ptrCol.a, style.pointerWidth);
    }

    // 6. Typography & Value Display (KnobLayoutMode)
    Color textCol = style.labelColor.value_or(theme ? theme->textPrimary : Color(0.90f, 0.92f, 0.95f, 0.90f));
    Color valCol  = style.valueColor.value_or(theme ? theme->textMuted : Color(0.70f, 0.74f, 0.80f, 0.85f));

    if (style.layout == KnobLayoutMode::StackedVertical) {
        // Standard vertical stack (gui-minimal: title top, value bottom)
        if (!label.empty()) {
            float labelY = cy - arcR - 11.5f;
            drawCenteredText(r, label, cx - 40.0f, labelY, 80.0f, 10.0f, 8.5f, textCol);
        }
        if (!valueText.empty()) {
            float valY = cy + arcR + 3.0f;
            drawCenteredText(r, valueText, cx - 40.0f, valY, 80.0f, 10.0f, 9.0f, valCol);
        }
    } else if (style.layout == KnobLayoutMode::InlineRight) {
        // SMAOLAB style (gui-variations: value and label stacked to the right)
        float textX = cx + arcR + 6.0f;
        if (!valueText.empty()) {
            drawText(r, valueText, textX, cy - 8.0f, 10.5f, textCol);
        }
        if (!label.empty()) {
            drawText(r, label, textX, cy + 3.5f, 8.5f, valCol);
        }
    } else if (style.layout == KnobLayoutMode::InlineLeft) {
        // Mirrored inline: value and label stacked to the left of the knob
        float textW = 48.0f;
        float textX = cx - arcR - textW - 6.0f;
        if (!valueText.empty()) {
            drawText(r, valueText, textX, cy - 8.0f, 10.5f, textCol);
        }
        if (!label.empty()) {
            drawText(r, label, textX, cy + 3.5f, 8.5f, valCol);
        }
    } else if (style.layout == KnobLayoutMode::BottomStacked) {
        // gui-astral style: label below knob, value pill badge directly below label
        if (!label.empty()) {
            float labelY = cy + arcR + 3.0f;
            drawCenteredText(r, label, cx - 40.0f, labelY, 80.0f, 10.0f, 9.0f, textCol);
        }
        if (!valueText.empty()) {
            float valY = cy + arcR + 15.0f;
            float pillW = 54.0f;
            float pillH = 14.0f;
            float pillX = cx - pillW * 0.5f;
            r.drawRoundedRect(pillX, valY, pillW, pillH, 3.0f, 0.08f, 0.09f, 0.12f, 0.90f);
            r.drawRoundedRectOutline(pillX, valY, pillW, pillH, 3.0f, 0.25f, 0.28f, 0.35f, 0.80f, 1.0f);
            drawCenteredText(r, valueText, pillX, valY + 1.0f, pillW, pillH, 8.5f, valCol);
        }
    }
}

void drawUnifiedSlider(BatchRenderer2D& r, const Rect2D& bounds, float normVal,
                      const SliderStyleCore& style,
                      const Color& accent,
                      const ThemeTokens* theme,
                      bool isActive, bool isHovered) {
    float clampedNorm = std::clamp(normVal, 0.0f, 1.0f);
    bool isLight = theme ? theme->isLight : false;

    Color trackCol = style.trackColor.value_or(
        isLight ? Color(0.80f, 0.82f, 0.86f, 0.95f) : Color(0.10f, 0.11f, 0.13f, 0.95f)
    );
    Color fillCol = style.fillColor.value_or(accent);
    if (isActive) {
        fillCol = Color(1.0f, 0.88f, 0.12f, 1.0f);
    } else if (isHovered) {
        fillCol = fillCol.lighten(0.18f);
    }

    if (style.orientation == SliderOrientation::Horizontal) {
        float effH = std::min(bounds.h, style.trackHeight);
        float trackY = bounds.y + (bounds.h - effH) * 0.5f;

        // 1. Recessed Track Well
        drawRoundedRect(r, bounds.x, trackY, bounds.w, effH, style.cornerRadius, trackCol);
        drawRoundedRectOutline(r, bounds.x, trackY, bounds.w, effH, style.cornerRadius,
                               isLight ? 0.65f : 0.05f, isLight ? 0.65f : 0.05f, isLight ? 0.70f : 0.06f,
                               0.60f, 1.0f);

        // 2. Value Fill Bar
        if (style.bipolar) {
            float midX = bounds.x + bounds.w * 0.5f;
            float targetX = bounds.x + bounds.w * clampedNorm;
            float fx = std::min(midX, targetX);
            float fw = std::abs(targetX - midX);
            if (fw > 0.5f) {
                drawRoundedRect(r, fx, trackY, fw, effH, style.cornerRadius, fillCol);
            }
        } else {
            float fw = bounds.w * clampedNorm;
            if (fw > 0.5f) {
                drawRoundedRect(r, bounds.x, trackY, fw, effH, style.cornerRadius, fillCol);
            }
        }

        // 3. Thumb Cap / Pill / Filmstrip
        if (style.filmstripTexture > 0 && style.filmstripFrames > 0) {
            int frameIndex = std::clamp(static_cast<int>(clampedNorm * (style.filmstripFrames - 1) + 0.5f),
                                        0, style.filmstripFrames - 1);
            r.drawFilmstripFrame(style.filmstripTexture, bounds.x, bounds.y, bounds.w, bounds.h,
                                 frameIndex, style.filmstripFrames, style.filmstripHorizontal);
        } else if (style.thumbStyle == SliderThumbStyle::PillThumb) {
            float thumbX = bounds.x + bounds.w * clampedNorm;
            float thumbY = bounds.y + bounds.h * 0.5f;
            float tRadius = style.thumbRadius > 0.0f ? style.thumbRadius : 5.0f;

            // Subtle drop shadow
            drawCircle(r, thumbX + 0.5f, thumbY + 1.2f, tRadius * 1.05f, 0.0f, 0.0f, 0.0f, 0.35f);

            // Pill body
            Color thumbBody = style.thumbColor.value_or(
                isActive ? Color(1.0f, 0.92f, 0.20f, 1.0f) :
                (isHovered ? Color(1.0f, 1.0f, 1.0f, 1.0f) :
                (isLight ? Color(0.92f, 0.94f, 0.96f, 1.0f) : Color(0.85f, 0.87f, 0.90f, 1.0f)))
            );
            drawCircle(r, thumbX, thumbY, tRadius, thumbBody.r, thumbBody.g, thumbBody.b, thumbBody.a);
            drawCircleOutline(r, thumbX, thumbY, tRadius,
                              isLight ? 0.40f : 0.15f, isLight ? 0.40f : 0.15f, isLight ? 0.45f : 0.18f,
                              0.85f, 1.0f);
        }
    } else {
        // Vertical Slider Orientation
        float effW = std::min(bounds.w, style.trackHeight);
        float trackX = bounds.x + (bounds.w - effW) * 0.5f;

        drawRoundedRect(r, trackX, bounds.y, effW, bounds.h, style.cornerRadius, trackCol);

        float fillH = bounds.h * clampedNorm;
        float fillY = bounds.y + bounds.h - fillH;
        if (fillH > 0.5f) {
            drawRoundedRect(r, trackX, fillY, effW, fillH, style.cornerRadius, fillCol);
        }

        if (style.filmstripTexture > 0 && style.filmstripFrames > 0) {
            int frameIndex = std::clamp(static_cast<int>(clampedNorm * (style.filmstripFrames - 1) + 0.5f),
                                        0, style.filmstripFrames - 1);
            r.drawFilmstripFrame(style.filmstripTexture, bounds.x, bounds.y, bounds.w, bounds.h,
                                 frameIndex, style.filmstripFrames, style.filmstripHorizontal);
        } else if (style.thumbStyle == SliderThumbStyle::PillThumb) {
            float thumbX = bounds.x + bounds.w * 0.5f;
            float thumbY = bounds.y + bounds.h * (1.0f - clampedNorm);
            float tRadius = style.thumbRadius > 0.0f ? style.thumbRadius : 5.0f;

            drawCircle(r, thumbX + 0.5f, thumbY + 1.2f, tRadius * 1.05f, 0.0f, 0.0f, 0.0f, 0.35f);
            Color thumbBody = style.thumbColor.value_or(Color(0.85f, 0.87f, 0.90f, 1.0f));
            drawCircle(r, thumbX, thumbY, tRadius, thumbBody.r, thumbBody.g, thumbBody.b, thumbBody.a);
            drawCircleOutline(r, thumbX, thumbY, tRadius, 0.20f, 0.22f, 0.26f, 0.85f, 1.0f);
        }
    }
}

} // namespace eatsbits::ui

