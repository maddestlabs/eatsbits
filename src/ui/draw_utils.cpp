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

} // namespace eatsbits::ui
