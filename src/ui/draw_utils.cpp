#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>

namespace eatsbits::ui {

void drawText(BatchRenderer2D& /*r*/, const std::string& str, float x, float y, float size,
              float red, float green, float blue, float alpha) {
    float scale = std::max(0.6f, size / 13.0f);
    drawVectorString(str, x, y, scale, red, green, blue, alpha);
}

void drawCenteredText(BatchRenderer2D& /*r*/, const std::string& str, const Rect2D& bounds,
                      float size, const Color& col, float alpha) {
    float scale = std::max(0.6f, size / 13.0f);
    float cx = bounds.x + bounds.w * 0.5f;
    float cy = bounds.y + bounds.h * 0.5f;
    drawVectorStringCentered(str, cx, cy, scale, col.r, col.g, col.b, alpha * col.a);
}

void drawRotatedText(BatchRenderer2D& r, const std::string& str, float cx, float cy,
                     float angleDegrees, float size, const Color& col, float alpha) {
    float scale = std::max(0.6f, size / 13.0f);
    r.setRotation(angleDegrees, cx, cy);
    drawVectorStringCentered(str, cx, cy, scale, col.r, col.g, col.b, alpha * col.a);
    r.resetRotation();
}

void drawButton(BatchRenderer2D& r, const Rect2D& bounds, const std::string& label,
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

void drawMonoText(BatchRenderer2D& /*r*/, const std::string& str, float x, float y, float size,
                  float red, float green, float blue, float alpha) {
    float scale = std::max(0.6f, size / 13.0f);
    drawMonoString(str, x, y, scale, red, green, blue, alpha);
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

} // namespace eatsbits::ui
