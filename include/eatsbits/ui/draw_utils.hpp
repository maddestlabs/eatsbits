#pragma once

#include <string>
#include "geometry.hpp"
#include "batch_renderer_2d.hpp"
#include "theme.hpp"

namespace eatsbits::ui {

void drawVectorString(const std::string& str, float x, float y, float scale, float r, float g, float b, float a = 1.0f);
void drawVectorStringCentered(const std::string& str, float cx, float cy, float scale, float r, float g, float b, float a = 1.0f);
void drawMonoString(const std::string& str, float x, float y, float scale, float r, float g, float b, float a = 1.0f);

inline void drawVectorString(const std::string& str, float x, float y, float scale, const Color& c) {
    drawVectorString(str, x, y, scale, c.r, c.g, c.b, c.a);
}

inline void drawVectorStringCentered(const std::string& str, float cx, float cy, float scale, const Color& c) {
    drawVectorStringCentered(str, cx, cy, scale, c.r, c.g, c.b, c.a);
}

inline void drawMonoString(const std::string& str, float x, float y, float scale, const Color& c) {
    drawMonoString(str, x, y, scale, c.r, c.g, c.b, c.a);
}

void drawText(BatchRenderer2D& r, const std::string& str, float x, float y, float size,
              float red, float green, float blue, float alpha = 1.0f);

inline void drawText(BatchRenderer2D& r, const std::string& str, float x, float y, float size,
                     const Color& col, float alpha = 1.0f) {
    drawText(r, str, x, y, size, col.r, col.g, col.b, alpha * col.a);
}

void drawCenteredText(BatchRenderer2D& r, const std::string& str, const Rect2D& bounds,
                      float size, const Color& col, float alpha = 1.0f);

inline void drawCenteredText(BatchRenderer2D& r, const std::string& str, float x, float y, float w, float h,
                             float size, const Color& col, float alpha = 1.0f) {
    drawCenteredText(r, str, Rect2D{x, y, w, h}, size, col, alpha);
}

inline void drawCenteredText(BatchRenderer2D& r, const std::string& str, float x, float y, float w, float h,
                             float size, float red, float green, float blue, float alpha = 1.0f) {
    drawCenteredText(r, str, Rect2D{x, y, w, h}, size, Color{red, green, blue, alpha}, 1.0f);
}

void drawRotatedText(BatchRenderer2D& r, const std::string& str, float cx, float cy,
                     float angleDegrees, float size, const Color& col, float alpha = 1.0f);

void drawButton(BatchRenderer2D& r, const Rect2D& bounds, const std::string& label,
                const Color& bgCol, const Color& borderCol, const Color& textCol,
                float size = 10.5f, float cornerRadius = 4.0f, float borderWidth = 1.0f);

inline void drawButton(BatchRenderer2D& r, float x, float y, float w, float h, const std::string& label,
                       const Color& bgCol, const Color& borderCol, const Color& textCol,
                       float size = 10.5f, float cornerRadius = 4.0f, float borderWidth = 1.0f) {
    drawButton(r, Rect2D{x, y, w, h}, label, bgCol, borderCol, textCol, size, cornerRadius, borderWidth);
}

void drawPianoIcon(BatchRenderer2D& r, float x, float y, float w, float h, const Color& accentCol);

void drawPropertiesIcon(BatchRenderer2D& r, float x, float y, float w, float h, const Color& accentCol);

void drawScrewCloseButton(BatchRenderer2D& r, float cx, float cy, float radius, bool hovered, const Color& highlightColor);

void drawIconEdit(BatchRenderer2D& r, float x, float y, float size, const Color& c);

void drawDesignChipIcon(BatchRenderer2D& r, float x, float y, float size, const Color& c);

void drawSlidersTuneIcon(BatchRenderer2D& r, float x, float y, float size, const Color& c);

void drawFullscreenIcon(BatchRenderer2D& r, float x, float y, float size, const Color& c, float thickness = 1.5f);

void drawChevronUp(BatchRenderer2D& r, float cx, float cy, float size, const Color& c, float thickness = 1.6f);

void drawChevronDown(BatchRenderer2D& r, float cx, float cy, float size, const Color& c, float thickness = 1.6f);

void drawTrashIcon(BatchRenderer2D& r, float cx, float cy, float size, const Color& c, float thickness = 1.4f);

void drawMonoText(BatchRenderer2D& r, const std::string& str, float x, float y, float size,
                  float red, float green, float blue, float alpha = 1.0f);

inline void drawMonoText(BatchRenderer2D& r, const std::string& str, float x, float y, float size,
                         const Color& col, float alpha = 1.0f) {
    drawMonoText(r, str, x, y, size, col.r, col.g, col.b, alpha * col.a);
}

void drawMonoTextClipped(BatchRenderer2D& r, const std::string& str, float x, float y, float size,
                         float clipMinX, float clipMaxX,
                         float red, float green, float blue, float alpha = 1.0f);

inline void drawMonoTextClipped(BatchRenderer2D& r, const std::string& str, float x, float y, float size,
                                float clipMinX, float clipMaxX,
                                const Color& col, float alpha = 1.0f) {
    drawMonoTextClipped(r, str, x, y, size, clipMinX, clipMaxX, col.r, col.g, col.b, alpha * col.a);
}

inline void drawRect(BatchRenderer2D& r, float x, float y, float w, float h,
                     float red, float green, float blue, float alpha = 1.0f) {
    r.drawRect(x, y, w, h, red, green, blue, alpha);
}

inline void drawRect(BatchRenderer2D& r, float x, float y, float w, float h,
                     const Color& col, float alpha = 1.0f) {
    r.drawRect(x, y, w, h, col.r, col.g, col.b, alpha * col.a);
}

inline void drawRectGradient(BatchRenderer2D& r, float x, float y, float w, float h,
                             float r0, float g0, float b0, float r1, float g1, float b1, float a = 1.0f) {
    r.drawRectGradient(x, y, w, h, r0, g0, b0, r1, g1, b1, a);
}

inline void drawRectGradient(BatchRenderer2D& r, float x, float y, float w, float h,
                             const Color& c0, const Color& c1) {
    r.drawRectGradient(x, y, w, h, c0.r, c0.g, c0.b, c1.r, c1.g, c1.b, c0.a);
}

inline void drawRoundedRect(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                            float red, float green, float blue, float alpha = 1.0f) {
    r.drawRoundedRect(x, y, w, h, radius, red, green, blue, alpha);
}

inline void drawRoundedRect(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                            const Color& col, float alpha = 1.0f) {
    r.drawRoundedRect(x, y, w, h, radius, col.r, col.g, col.b, alpha * col.a);
}

inline void drawRoundedRectGradient(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                                    float r0, float g0, float b0, float r1, float g1, float b1, float a = 1.0f) {
    r.drawRoundedRectGradient(x, y, w, h, radius, r0, g0, b0, r1, g1, b1, a);
}

inline void drawRoundedRectGradient(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                                    const Color& c0, const Color& c1) {
    r.drawRoundedRectGradient(x, y, w, h, radius, c0.r, c0.g, c0.b, c1.r, c1.g, c1.b, c0.a);
}

inline void drawRoundedRectOutline(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                                   float red, float green, float blue, float alpha = 1.0f, float lineWidth = 1.0f) {
    r.drawRoundedRectOutline(x, y, w, h, radius, red, green, blue, alpha, lineWidth);
}

inline void drawRoundedRectOutline(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                                   const Color& col, float alpha = 1.0f, float lineWidth = 1.0f) {
    r.drawRoundedRectOutline(x, y, w, h, radius, col.r, col.g, col.b, alpha * col.a, lineWidth);
}

inline void drawLine(BatchRenderer2D& r, float x0, float y0, float x1, float y1,
                     float red, float green, float blue, float alpha = 1.0f, float lineWidth = 1.5f) {
    r.drawLine(x0, y0, x1, y1, red, green, blue, alpha, lineWidth);
}

inline void drawLine(BatchRenderer2D& r, float x0, float y0, float x1, float y1,
                     const Color& col, float alpha = 1.0f, float lineWidth = 1.5f) {
    r.drawLine(x0, y0, x1, y1, col.r, col.g, col.b, alpha * col.a, lineWidth);
}

inline void drawCircle(BatchRenderer2D& r, float cx, float cy, float radius,
                       float red, float green, float blue, float alpha = 1.0f) {
    r.drawCircle(cx, cy, radius, red, green, blue, alpha);
}

inline void drawCircle(BatchRenderer2D& r, float cx, float cy, float radius,
                       const Color& col, float alpha = 1.0f) {
    r.drawCircle(cx, cy, radius, col.r, col.g, col.b, alpha * col.a);
}

inline void drawCircleRadialGradient(BatchRenderer2D& r, float cx, float cy, float radius,
                                     float innerR, float innerG, float innerB, float innerA,
                                     float outerR, float outerG, float outerB, float outerA,
                                     float offX = 0.0f, float offY = 0.0f, int segments = 36) {
    r.drawCircleRadialGradient(cx, cy, radius, innerR, innerG, innerB, innerA, outerR, outerG, outerB, outerA, offX, offY, segments);
}

inline void drawCircleRadialGradient(BatchRenderer2D& r, float cx, float cy, float radius,
                                     const Color& innerCol, const Color& outerCol,
                                     float offX = 0.0f, float offY = 0.0f, int segments = 36) {
    r.drawCircleRadialGradient(cx, cy, radius, innerCol.r, innerCol.g, innerCol.b, innerCol.a,
                               outerCol.r, outerCol.g, outerCol.b, outerCol.a, offX, offY, segments);
}

inline void drawCircleRadial3StopGradient(BatchRenderer2D& r, float cx, float cy, float radius,
                                          const Color& innerCol, const Color& midCol, const Color& outerCol,
                                          float offX = 0.0f, float offY = 0.0f, float midStop = 0.50f, int segments = 36) {
    r.drawCircleRadial3StopGradient(cx, cy, radius,
                                    innerCol.r, innerCol.g, innerCol.b, innerCol.a,
                                    midCol.r, midCol.g, midCol.b, midCol.a,
                                    outerCol.r, outerCol.g, outerCol.b, outerCol.a,
                                    offX, offY, midStop, segments);
}

inline void drawCircleLinearGradient(BatchRenderer2D& r, float cx, float cy, float radius,
                                     const Color& c0, const Color& c1,
                                     float angleRad = 1.5707963f, int segments = 36) {
    r.drawCircleLinearGradient(cx, cy, radius, c0.r, c0.g, c0.b, c0.a, c1.r, c1.g, c1.b, c1.a, angleRad, segments);
}

inline void drawCircleOutline(BatchRenderer2D& r, float cx, float cy, float radius,
                              float red, float green, float blue, float alpha = 1.0f, float lineWidth = 1.5f) {
    r.drawCircleOutline(cx, cy, radius, red, green, blue, alpha, lineWidth);
}

inline void drawCircleOutline(BatchRenderer2D& r, float cx, float cy, float radius,
                              const Color& col, float alpha = 1.0f, float lineWidth = 1.5f) {
    r.drawCircleOutline(cx, cy, radius, col.r, col.g, col.b, alpha * col.a, lineWidth);
}

inline void drawTriangle(BatchRenderer2D& r, float x0, float y0, float x1, float y1, float x2, float y2,
                         float red, float green, float blue, float alpha = 1.0f) {
    r.drawTriangle(x0, y0, x1, y1, x2, y2, red, green, blue, alpha);
}

inline void drawTriangle(BatchRenderer2D& r, float x0, float y0, float x1, float y1, float x2, float y2,
                         const Color& col, float alpha = 1.0f) {
    r.drawTriangle(x0, y0, x1, y1, x2, y2, col.r, col.g, col.b, alpha * col.a);
}

float getMonoCharAdvance(float size);

} // namespace eatsbits::ui
