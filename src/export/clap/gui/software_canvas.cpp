#include "eatsbits/export/clap/gui/software_canvas.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <numbers>

namespace eatsbits::export_clap::gui {

SoftwareCanvas::SoftwareCanvas(uint32_t width, uint32_t height, double scale) {
    resize(width, height, scale);
}

void SoftwareCanvas::resize(uint32_t width, uint32_t height, double scale) {
    width_ = (width > 0) ? width : 800;
    height_ = (height > 0) ? height : 480;
    scale_ = (scale > 0.0) ? scale : 1.0;
    pixels_.assign(static_cast<size_t>(width_) * height_, rgba(30, 32, 36, 255));
}

void SoftwareCanvas::clear(uint32_t color) noexcept {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

void SoftwareCanvas::drawBrushedMetal(int x, int y, int w, int h, uint32_t baseCol, uint32_t grainCol, float intensity) noexcept {
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(static_cast<int>(width_), x + w);
    const int y1 = std::min(static_cast<int>(height_), y + h);
    if (x0 >= x1 || y0 >= y1) return;

    const uint8_t br = (baseCol >> 16) & 0xFF;
    const uint8_t bg = (baseCol >> 8) & 0xFF;
    const uint8_t bb = baseCol & 0xFF;

    const uint8_t gr = (grainCol >> 16) & 0xFF;
    const uint8_t gg = (grainCol >> 8) & 0xFF;
    const uint8_t gb = grainCol & 0xFF;

    // Fast horizontal pseudo-random hash generator for brushed anisotropic grain
    for (int py = y0; py < y1; ++py) {
        // Vertical subtle shading gradient across the chassis
        const float vGrad = 1.0f - (static_cast<float>(py - y0) / static_cast<float>(h)) * 0.12f;
        uint32_t rowSeed = static_cast<uint32_t>(py * 1664525u + 1013904223u);

        for (int px = x0; px < x1; ++px) {
            // High horizontal correlation for linear brushed metal look
            rowSeed = rowSeed * 1103515245u + 12345u;
            const float noise = ((rowSeed >> 16) & 0xFF) / 255.0f;
            const float gFactor = (noise - 0.5f) * intensity;

            float r = static_cast<float>(br) * vGrad + (static_cast<float>(gr) - static_cast<float>(br)) * gFactor;
            float g = static_cast<float>(bg) * vGrad + (static_cast<float>(gg) - static_cast<float>(bg)) * gFactor;
            float b = static_cast<float>(bb) * vGrad + (static_cast<float>(gb) - static_cast<float>(bb)) * gFactor;

            const uint8_t outR = static_cast<uint8_t>(std::clamp(r + 0.5f, 0.0f, 255.0f));
            const uint8_t outG = static_cast<uint8_t>(std::clamp(g + 0.5f, 0.0f, 255.0f));
            const uint8_t outB = static_cast<uint8_t>(std::clamp(b + 0.5f, 0.0f, 255.0f));

            pixels_[static_cast<size_t>(py) * width_ + static_cast<size_t>(px)] = rgba(outR, outG, outB, 255);
        }
    }
}

void SoftwareCanvas::drawRect(int x, int y, int w, int h, uint32_t color) noexcept {
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(static_cast<int>(width_), x + w);
    const int y1 = std::min(static_cast<int>(height_), y + h);
    if (x0 >= x1 || y0 >= y1) return;

    const uint8_t r = (color >> 16) & 0xFF;
    const uint8_t g = (color >> 8) & 0xFF;
    const uint8_t b = color & 0xFF;
    const uint8_t a = (color >> 24) & 0xFF;

    if (a == 255) {
        for (int py = y0; py < y1; ++py) {
            uint32_t* row = &pixels_[static_cast<size_t>(py) * width_ + static_cast<size_t>(x0)];
            std::fill_n(row, x1 - x0, color);
        }
    } else if (a > 0) {
        for (int py = y0; py < y1; ++py) {
            for (int px = x0; px < x1; ++px) {
                blendPixel(px, py, r, g, b, a);
            }
        }
    }
}

void SoftwareCanvas::drawRectGradientV(int x, int y, int w, int h, uint32_t colTop, uint32_t colBottom) noexcept {
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(static_cast<int>(width_), x + w);
    const int y1 = std::min(static_cast<int>(height_), y + h);
    if (x0 >= x1 || y0 >= y1 || h <= 0) return;

    const float tr = static_cast<float>((colTop >> 16) & 0xFF);
    const float tg = static_cast<float>((colTop >> 8) & 0xFF);
    const float tb = static_cast<float>(colTop & 0xFF);
    const float ta = static_cast<float>((colTop >> 24) & 0xFF);

    const float br = static_cast<float>((colBottom >> 16) & 0xFF);
    const float bg = static_cast<float>((colBottom >> 8) & 0xFF);
    const float bb = static_cast<float>(colBottom & 0xFF);
    const float ba = static_cast<float>((colBottom >> 24) & 0xFF);

    for (int py = y0; py < y1; ++py) {
        const float t = static_cast<float>(py - y) / static_cast<float>(h);
        const uint8_t r = static_cast<uint8_t>(tr + (br - tr) * t + 0.5f);
        const uint8_t g = static_cast<uint8_t>(tg + (bg - tg) * t + 0.5f);
        const uint8_t b = static_cast<uint8_t>(tb + (bb - tb) * t + 0.5f);
        const uint8_t a = static_cast<uint8_t>(ta + (ba - ta) * t + 0.5f);

        if (a == 255) {
            const uint32_t c = rgba(r, g, b, 255);
            uint32_t* row = &pixels_[static_cast<size_t>(py) * width_ + static_cast<size_t>(x0)];
            std::fill_n(row, x1 - x0, c);
        } else {
            for (int px = x0; px < x1; ++px) {
                blendPixel(px, py, r, g, b, a);
            }
        }
    }
}

void SoftwareCanvas::drawRectOutline(int x, int y, int w, int h, uint32_t color, float strokeWidth) noexcept {
    const int sw = std::max(1, static_cast<int>(strokeWidth + 0.5f));
    drawRect(x, y, w, sw, color);
    drawRect(x, y + h - sw, w, sw, color);
    drawRect(x, y + sw, sw, h - (sw * 2), color);
    drawRect(x + w - sw, y + sw, sw, h - (sw * 2), color);
}

void SoftwareCanvas::drawRoundedRect(float x, float y, float w, float h, float radius, uint32_t color) noexcept {
    const int minX = std::max(0, static_cast<int>(std::floor(x)));
    const int minY = std::max(0, static_cast<int>(std::floor(y)));
    const int maxX = std::min(static_cast<int>(width_), static_cast<int>(std::ceil(x + w)));
    const int maxY = std::min(static_cast<int>(height_), static_cast<int>(std::ceil(y + h)));
    if (minX >= maxX || minY >= maxY) return;

    const uint8_t r = (color >> 16) & 0xFF;
    const uint8_t g = (color >> 8) & 0xFF;
    const uint8_t b = color & 0xFF;
    const uint8_t a = (color >> 24) & 0xFF;

    const float rad = std::max(0.0f, std::min(radius, std::min(w * 0.5f, h * 0.5f)));
    const float innerX0 = x + rad;
    const float innerX1 = x + w - rad;
    const float innerY0 = y + rad;
    const float innerY1 = y + h - rad;

    for (int py = minY; py < maxY; ++py) {
        const float cy = static_cast<float>(py) + 0.5f;
        for (int px = minX; px < maxX; ++px) {
            const float cx = static_cast<float>(px) + 0.5f;
            float dist = 0.0f;

            if (cx < innerX0 && cy < innerY0) {
                dist = std::hypot(cx - innerX0, cy - innerY0) - rad;
            } else if (cx > innerX1 && cy < innerY0) {
                dist = std::hypot(cx - innerX1, cy - innerY0) - rad;
            } else if (cx < innerX0 && cy > innerY1) {
                dist = std::hypot(cx - innerX0, cy - innerY1) - rad;
            } else if (cx > innerX1 && cy > innerY1) {
                dist = std::hypot(cx - innerX1, cy - innerY1) - rad;
            } else {
                dist = -1.0f; // Inside main body
            }

            const float cov = std::clamp(0.5f - dist, 0.0f, 1.0f);
            if (cov > 0.0f) {
                blendPixel(px, py, r, g, b, static_cast<uint8_t>(a * cov + 0.5f));
            }
        }
    }
}

void SoftwareCanvas::drawRoundedRectOutline(float x, float y, float w, float h, float radius, uint32_t color, float strokeWidth) noexcept {
    const int minX = std::max(0, static_cast<int>(std::floor(x - strokeWidth)));
    const int minY = std::max(0, static_cast<int>(std::floor(y - strokeWidth)));
    const int maxX = std::min(static_cast<int>(width_), static_cast<int>(std::ceil(x + w + strokeWidth)));
    const int maxY = std::min(static_cast<int>(height_), static_cast<int>(std::ceil(y + h + strokeWidth)));
    if (minX >= maxX || minY >= maxY) return;

    const uint8_t r = (color >> 16) & 0xFF;
    const uint8_t g = (color >> 8) & 0xFF;
    const uint8_t b = color & 0xFF;
    const uint8_t a = (color >> 24) & 0xFF;

    const float rad = std::max(0.0f, std::min(radius, std::min(w * 0.5f, h * 0.5f)));
    const float innerX0 = x + rad;
    const float innerX1 = x + w - rad;
    const float innerY0 = y + rad;
    const float innerY1 = y + h - rad;
    const float halfSw = strokeWidth * 0.5f;

    for (int py = minY; py < maxY; ++py) {
        const float cy = static_cast<float>(py) + 0.5f;
        for (int px = minX; px < maxX; ++px) {
            const float cx = static_cast<float>(px) + 0.5f;
            float dist = 0.0f;

            if (cx < innerX0 && cy < innerY0) {
                dist = std::hypot(cx - innerX0, cy - innerY0) - rad;
            } else if (cx > innerX1 && cy < innerY0) {
                dist = std::hypot(cx - innerX1, cy - innerY0) - rad;
            } else if (cx < innerX0 && cy > innerY1) {
                dist = std::hypot(cx - innerX0, cy - innerY1) - rad;
            } else if (cx > innerX1 && cy > innerY1) {
                dist = std::hypot(cx - innerX1, cy - innerY1) - rad;
            } else {
                const float dx = std::max(x - cx, cx - (x + w));
                const float dy = std::max(y - cy, cy - (y + h));
                dist = std::max(dx, dy);
            }

            const float edgeDist = std::abs(dist);
            const float cov = std::clamp(halfSw + 0.5f - edgeDist, 0.0f, 1.0f);
            if (cov > 0.0f) {
                blendPixel(px, py, r, g, b, static_cast<uint8_t>(a * cov + 0.5f));
            }
        }
    }
}

void SoftwareCanvas::drawCircle(float cx, float cy, float radius, uint32_t color) noexcept {
    if (radius <= 0.0f) return;
    const int minX = std::max(0, static_cast<int>(std::floor(cx - radius - 1.0f)));
    const int minY = std::max(0, static_cast<int>(std::floor(cy - radius - 1.0f)));
    const int maxX = std::min(static_cast<int>(width_), static_cast<int>(std::ceil(cx + radius + 1.0f)));
    const int maxY = std::min(static_cast<int>(height_), static_cast<int>(std::ceil(cy + radius + 1.0f)));
    if (minX >= maxX || minY >= maxY) return;

    const uint8_t r = (color >> 16) & 0xFF;
    const uint8_t g = (color >> 8) & 0xFF;
    const uint8_t b = color & 0xFF;
    const uint8_t a = (color >> 24) & 0xFF;

    for (int py = minY; py < maxY; ++py) {
        const float dy = static_cast<float>(py) + 0.5f - cy;
        const float dy2 = dy * dy;
        for (int px = minX; px < maxX; ++px) {
            const float dx = static_cast<float>(px) + 0.5f - cx;
            const float d = std::sqrt(dx * dx + dy2);
            const float cov = std::clamp(radius + 0.5f - d, 0.0f, 1.0f);
            if (cov > 0.0f) {
                blendPixel(px, py, r, g, b, static_cast<uint8_t>(a * cov + 0.5f));
            }
        }
    }
}

void SoftwareCanvas::drawCircleOutline(float cx, float cy, float radius, uint32_t color, float strokeWidth) noexcept {
    if (radius <= 0.0f || strokeWidth <= 0.0f) return;
    const float halfSw = strokeWidth * 0.5f;
    const float outerR = radius + halfSw + 1.0f;
    const int minX = std::max(0, static_cast<int>(std::floor(cx - outerR)));
    const int minY = std::max(0, static_cast<int>(std::floor(cy - outerR)));
    const int maxX = std::min(static_cast<int>(width_), static_cast<int>(std::ceil(cx + outerR)));
    const int maxY = std::min(static_cast<int>(height_), static_cast<int>(std::ceil(cy + outerR)));
    if (minX >= maxX || minY >= maxY) return;

    const uint8_t r = (color >> 16) & 0xFF;
    const uint8_t g = (color >> 8) & 0xFF;
    const uint8_t b = color & 0xFF;
    const uint8_t a = (color >> 24) & 0xFF;

    for (int py = minY; py < maxY; ++py) {
        const float dy = static_cast<float>(py) + 0.5f - cy;
        const float dy2 = dy * dy;
        for (int px = minX; px < maxX; ++px) {
            const float dx = static_cast<float>(px) + 0.5f - cx;
            const float d = std::sqrt(dx * dx + dy2);
            const float dist = std::abs(d - radius);
            const float cov = std::clamp(halfSw + 0.5f - dist, 0.0f, 1.0f);
            if (cov > 0.0f) {
                blendPixel(px, py, r, g, b, static_cast<uint8_t>(a * cov + 0.5f));
            }
        }
    }
}

void SoftwareCanvas::drawCircleRadialGradient(float cx, float cy, float radius,
                                            uint32_t innerCol, uint32_t outerCol,
                                            float offX, float offY) noexcept {
    if (radius <= 0.0f) return;
    const int minX = std::max(0, static_cast<int>(std::floor(cx - radius - 1.0f)));
    const int minY = std::max(0, static_cast<int>(std::floor(cy - radius - 1.0f)));
    const int maxX = std::min(static_cast<int>(width_), static_cast<int>(std::ceil(cx + radius + 1.0f)));
    const int maxY = std::min(static_cast<int>(height_), static_cast<int>(std::ceil(cy + radius + 1.0f)));
    if (minX >= maxX || minY >= maxY) return;

    const float ir = static_cast<float>((innerCol >> 16) & 0xFF);
    const float ig = static_cast<float>((innerCol >> 8) & 0xFF);
    const float ib = static_cast<float>(innerCol & 0xFF);
    const float ia = static_cast<float>((innerCol >> 24) & 0xFF);

    const float or_ = static_cast<float>((outerCol >> 16) & 0xFF);
    const float og = static_cast<float>((outerCol >> 8) & 0xFF);
    const float ob = static_cast<float>(outerCol & 0xFF);
    const float oa = static_cast<float>((outerCol >> 24) & 0xFF);

    const float lightX = cx + offX;
    const float lightY = cy + offY;

    for (int py = minY; py < maxY; ++py) {
        const float dy = static_cast<float>(py) + 0.5f - cy;
        const float dy2 = dy * dy;
        const float lightDy = static_cast<float>(py) + 0.5f - lightY;

        for (int px = minX; px < maxX; ++px) {
            const float dx = static_cast<float>(px) + 0.5f - cx;
            const float d = std::sqrt(dx * dx + dy2);
            const float cov = std::clamp(radius + 0.5f - d, 0.0f, 1.0f);
            if (cov <= 0.0f) continue;

            const float lightDx = static_cast<float>(px) + 0.5f - lightX;
            const float lightDist = std::sqrt(lightDx * lightDx + lightDy * lightDy);
            const float t = std::clamp(lightDist / radius, 0.0f, 1.0f);

            const uint8_t r = static_cast<uint8_t>(ir + (or_ - ir) * t + 0.5f);
            const uint8_t g = static_cast<uint8_t>(ig + (og - ig) * t + 0.5f);
            const uint8_t b = static_cast<uint8_t>(ib + (ob - ib) * t + 0.5f);
            const uint8_t a = static_cast<uint8_t>((ia + (oa - ia) * t) * cov + 0.5f);

            blendPixel(px, py, r, g, b, a);
        }
    }
}

void SoftwareCanvas::drawArc(float cx, float cy, float radius, float startAngle, float endAngle,
                            uint32_t color, float strokeWidth) noexcept {
    if (radius <= 0.0f || strokeWidth <= 0.0f) return;
    const float halfSw = strokeWidth * 0.5f;
    const float outerR = radius + halfSw + 1.0f;
    const int minX = std::max(0, static_cast<int>(std::floor(cx - outerR)));
    const int minY = std::max(0, static_cast<int>(std::floor(cy - outerR)));
    const int maxX = std::min(static_cast<int>(width_), static_cast<int>(std::ceil(cx + outerR)));
    const int maxY = std::min(static_cast<int>(height_), static_cast<int>(std::ceil(cy + outerR)));
    if (minX >= maxX || minY >= maxY) return;

    const uint8_t r = (color >> 16) & 0xFF;
    const uint8_t g = (color >> 8) & 0xFF;
    const uint8_t b = color & 0xFF;
    const uint8_t a = (color >> 24) & 0xFF;

    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;

    // Normalize sweep range
    float sweep = endAngle - startAngle;
    while (sweep < 0.0f) sweep += twoPi;

    for (int py = minY; py < maxY; ++py) {
        const float dy = static_cast<float>(py) + 0.5f - cy;
        const float dy2 = dy * dy;
        for (int px = minX; px < maxX; ++px) {
            const float dx = static_cast<float>(px) + 0.5f - cx;
            const float d = std::sqrt(dx * dx + dy2);
            const float dist = std::abs(d - radius);
            const float radCov = std::clamp(halfSw + 0.5f - dist, 0.0f, 1.0f);
            if (radCov <= 0.0f) continue;

            float ang = std::atan2(dy, dx) - startAngle;
            while (ang < 0.0f) ang += twoPi;
            while (ang >= twoPi) ang -= twoPi;

            float angCov = 0.0f;
            if (ang <= sweep) {
                angCov = 1.0f;
            } else {
                // Angular antialiasing at sweep ends
                const float dAng = std::min(ang - sweep, twoPi - ang);
                const float arcPixelDist = dAng * radius;
                angCov = std::clamp(0.5f - arcPixelDist, 0.0f, 1.0f);
            }

            const float totalCov = radCov * angCov;
            if (totalCov > 0.0f) {
                blendPixel(px, py, r, g, b, static_cast<uint8_t>(a * totalCov + 0.5f));
            }
        }
    }
}

void SoftwareCanvas::drawLine(float x0, float y0, float x1, float y1, uint32_t color, float strokeWidth) noexcept {
    const float halfSw = strokeWidth * 0.5f;
    const int minX = std::max(0, static_cast<int>(std::floor(std::min(x0, x1) - halfSw - 1.0f)));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min(y0, y1) - halfSw - 1.0f)));
    const int maxX = std::min(static_cast<int>(width_), static_cast<int>(std::ceil(std::max(x0, x1) + halfSw + 1.0f)));
    const int maxY = std::min(static_cast<int>(height_), static_cast<int>(std::ceil(std::max(y0, y1) + halfSw + 1.0f)));
    if (minX >= maxX || minY >= maxY) return;

    const uint8_t r = (color >> 16) & 0xFF;
    const uint8_t g = (color >> 8) & 0xFF;
    const uint8_t b = color & 0xFF;
    const uint8_t a = (color >> 24) & 0xFF;

    const float vx = x1 - x0;
    const float vy = y1 - y0;
    const float lenSq = vx * vx + vy * vy;

    if (lenSq < 1e-6f) {
        drawCircle(x0, y0, halfSw, color);
        return;
    }

    const float invLenSq = 1.0f / lenSq;

    for (int py = minY; py < maxY; ++py) {
        const float cy = static_cast<float>(py) + 0.5f;
        for (int px = minX; px < maxX; ++px) {
            const float cx = static_cast<float>(px) + 0.5f;
            const float t = std::clamp(((cx - x0) * vx + (cy - y0) * vy) * invLenSq, 0.0f, 1.0f);
            const float qx = x0 + t * vx;
            const float qy = y0 + t * vy;
            const float d = std::hypot(cx - qx, cy - qy);

            const float cov = std::clamp(halfSw + 0.5f - d, 0.0f, 1.0f);
            if (cov > 0.0f) {
                blendPixel(px, py, r, g, b, static_cast<uint8_t>(a * cov + 0.5f));
            }
        }
    }
}

void SoftwareCanvas::drawCircleDropShadow(float cx, float cy, float radius, float blurRadius,
                                       float lightAngle, float alpha) noexcept {
    const float offDist = blurRadius * 0.45f;
    const float shadowCx = cx - std::cos(lightAngle) * offDist;
    const float shadowCy = cy - std::sin(lightAngle) * offDist;
    const float shadowR = radius + blurRadius * 0.35f;

    const int minX = std::max(0, static_cast<int>(std::floor(shadowCx - shadowR - blurRadius)));
    const int minY = std::max(0, static_cast<int>(std::floor(shadowCy - shadowR - blurRadius)));
    const int maxX = std::min(static_cast<int>(width_), static_cast<int>(std::ceil(shadowCx + shadowR + blurRadius)));
    const int maxY = std::min(static_cast<int>(height_), static_cast<int>(std::ceil(shadowCy + shadowR + blurRadius)));
    if (minX >= maxX || minY >= maxY) return;

    for (int py = minY; py < maxY; ++py) {
        const float dy = static_cast<float>(py) + 0.5f - shadowCy;
        const float dy2 = dy * dy;
        for (int px = minX; px < maxX; ++px) {
            const float dx = static_cast<float>(px) + 0.5f - shadowCx;
            const float d = std::sqrt(dx * dx + dy2);
            if (d < shadowR) {
                const float falloff = std::clamp((shadowR - d) / blurRadius, 0.0f, 1.0f);
                const uint8_t shadowA = static_cast<uint8_t>(alpha * 255.0f * falloff + 0.5f);
                blendPixel(px, py, 0, 0, 0, shadowA);
            }
        }
    }
}

void SoftwareCanvas::drawKnurledRing(float cx, float cy, float rInner, float rOuter, int numTeeth,
                                  float currentAngle, float lightAngle,
                                  uint32_t baseCol, uint32_t specCol, uint32_t shadowCol) noexcept {
    if (rInner >= rOuter || numTeeth <= 0) return;
    const int minX = std::max(0, static_cast<int>(std::floor(cx - rOuter - 1.0f)));
    const int minY = std::max(0, static_cast<int>(std::floor(cy - rOuter - 1.0f)));
    const int maxX = std::min(static_cast<int>(width_), static_cast<int>(std::ceil(cx + rOuter + 1.0f)));
    const int maxY = std::min(static_cast<int>(height_), static_cast<int>(std::ceil(cy + rOuter + 1.0f)));
    if (minX >= maxX || minY >= maxY) return;

    const float br = static_cast<float>((baseCol >> 16) & 0xFF);
    const float bg = static_cast<float>((baseCol >> 8) & 0xFF);
    const float bb = static_cast<float>(baseCol & 0xFF);

    const float sr = static_cast<float>((specCol >> 16) & 0xFF);
    const float sg = static_cast<float>((specCol >> 8) & 0xFF);
    const float sb = static_cast<float>(specCol & 0xFF);

    const float dr = static_cast<float>((shadowCol >> 16) & 0xFF);
    const float dg = static_cast<float>((shadowCol >> 8) & 0xFF);
    const float db = static_cast<float>(shadowCol & 0xFF);

    const float teethFreq = static_cast<float>(numTeeth);
    const float lightDirX = std::cos(lightAngle);
    const float lightDirY = std::sin(lightAngle);

    for (int py = minY; py < maxY; ++py) {
        const float dy = static_cast<float>(py) + 0.5f - cy;
        const float dy2 = dy * dy;
        for (int px = minX; px < maxX; ++px) {
            const float dx = static_cast<float>(px) + 0.5f - cx;
            const float d = std::sqrt(dx * dx + dy2);
            if (d < rInner || d > rOuter) continue;

            const float ang = std::atan2(dy, dx) - currentAngle;
            const float toothWave = (std::sin(ang * teethFreq) + 1.0f) * 0.5f; // 0 (groove) to 1 (ridge)

            // Surface normal dot light
            const float normX = dx / d;
            const float normY = dy / d;
            const float nDotL = normX * lightDirX + normY * lightDirY; // -1 to +1

            float r = br;
            float g = bg;
            float b = bb;

            if (toothWave < 0.35f) {
                // Deep fluted groove shadow
                const float sFactor = 1.0f - (toothWave / 0.35f);
                r = r + (dr - r) * sFactor;
                g = g + (dg - g) * sFactor;
                b = b + (db - b) * sFactor;
            } else {
                // Ridge highlight
                const float hFactor = (toothWave - 0.35f) / 0.65f;
                const float spec = std::max(0.0f, nDotL) * hFactor;
                r = r + (sr - r) * spec * 0.65f;
                g = g + (sg - g) * spec * 0.65f;
                b = b + (sb - b) * spec * 0.65f;
            }

            // Outer and inner rim antialiasing
            float cov = 1.0f;
            if (d > rOuter - 0.8f) cov = std::clamp(rOuter + 0.5f - d, 0.0f, 1.0f);
            if (d < rInner + 0.8f) cov = std::clamp(d - (rInner - 0.5f), 0.0f, 1.0f);

            blendPixel(px, py,
                       static_cast<uint8_t>(std::clamp(r + 0.5f, 0.0f, 255.0f)),
                       static_cast<uint8_t>(std::clamp(g + 0.5f, 0.0f, 255.0f)),
                       static_cast<uint8_t>(std::clamp(b + 0.5f, 0.0f, 255.0f)),
                       static_cast<uint8_t>(255.0f * cov + 0.5f));
        }
    }
}

// =========================================================================
// EMBEDDED HIGH-PRECISION VECTOR TYPOGRAPHY ENGINE (ROLAND SILKSCREEN)
// =========================================================================

// Normalized glyph stroke list on a 10x16 cell:
// x0, y0, x1, y1. Terminated by x0 = -1.
struct GlyphStroke {
    int8_t x0, y0, x1, y1;
};

static const GlyphStroke s_glyphA[] = {{1, 16, 5, 1}, {5, 1, 9, 16}, {3, 10, 7, 10}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphB[] = {{1, 1, 1, 16}, {1, 1, 7, 1}, {7, 1, 9, 4}, {9, 4, 7, 8}, {1, 8, 7, 8}, {7, 8, 9, 12}, {9, 12, 7, 16}, {1, 16, 7, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphC[] = {{9, 3, 6, 1}, {6, 1, 3, 1}, {3, 1, 1, 4}, {1, 4, 1, 13}, {1, 13, 3, 16}, {3, 16, 6, 16}, {6, 16, 9, 14}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphD[] = {{1, 1, 1, 16}, {1, 1, 6, 1}, {6, 1, 9, 4}, {9, 4, 9, 13}, {9, 13, 6, 16}, {6, 16, 1, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphE[] = {{1, 1, 1, 16}, {1, 1, 8, 1}, {1, 8, 6, 8}, {1, 16, 8, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphF[] = {{1, 1, 1, 16}, {1, 1, 8, 1}, {1, 8, 6, 8}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphG[] = {{9, 3, 6, 1}, {6, 1, 3, 1}, {3, 1, 1, 4}, {1, 4, 1, 13}, {1, 13, 3, 16}, {3, 16, 7, 16}, {7, 16, 9, 13}, {9, 13, 9, 9}, {9, 9, 5, 9}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphH[] = {{1, 1, 1, 16}, {9, 1, 9, 16}, {1, 8, 9, 8}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphI[] = {{5, 1, 5, 16}, {2, 1, 8, 1}, {2, 16, 8, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphJ[] = {{8, 1, 8, 13}, {8, 13, 6, 16}, {6, 16, 3, 16}, {3, 16, 1, 13}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphK[] = {{1, 1, 1, 16}, {8, 1, 1, 8}, {2, 8, 8, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphL[] = {{1, 1, 1, 16}, {1, 16, 8, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphM[] = {{1, 16, 1, 1}, {1, 1, 5, 10}, {5, 10, 9, 1}, {9, 1, 9, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphN[] = {{1, 16, 1, 1}, {1, 1, 9, 16}, {9, 16, 9, 1}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphO[] = {{3, 1, 7, 1}, {7, 1, 9, 4}, {9, 4, 9, 13}, {9, 13, 7, 16}, {7, 16, 3, 16}, {3, 16, 1, 13}, {1, 13, 1, 4}, {1, 4, 3, 1}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphP[] = {{1, 1, 1, 16}, {1, 1, 7, 1}, {7, 1, 9, 4}, {9, 4, 7, 8}, {7, 8, 1, 8}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphQ[] = {{3, 1, 7, 1}, {7, 1, 9, 4}, {9, 4, 9, 13}, {9, 13, 7, 16}, {7, 16, 3, 16}, {3, 16, 1, 13}, {1, 13, 1, 4}, {1, 4, 3, 1}, {6, 12, 10, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphR[] = {{1, 1, 1, 16}, {1, 1, 7, 1}, {7, 1, 9, 4}, {9, 4, 7, 8}, {7, 8, 1, 8}, {4, 8, 9, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphS[] = {{8, 3, 6, 1}, {6, 1, 3, 1}, {3, 1, 1, 4}, {1, 4, 2, 7}, {2, 7, 7, 9}, {7, 9, 9, 12}, {9, 12, 7, 16}, {7, 16, 3, 16}, {3, 16, 1, 14}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphT[] = {{1, 1, 9, 1}, {5, 1, 5, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphU[] = {{1, 1, 1, 13}, {1, 13, 3, 16}, {3, 16, 7, 16}, {7, 16, 9, 13}, {9, 13, 9, 1}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphV[] = {{1, 1, 5, 16}, {5, 16, 9, 1}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphW[] = {{1, 1, 3, 16}, {3, 16, 5, 6}, {5, 6, 7, 16}, {7, 16, 9, 1}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphX[] = {{1, 1, 9, 16}, {9, 1, 1, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphY[] = {{1, 1, 5, 8}, {9, 1, 5, 8}, {5, 8, 5, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphZ[] = {{1, 1, 9, 1}, {9, 1, 1, 16}, {1, 16, 9, 16}, {-1, 0, 0, 0}};

static const GlyphStroke s_glyph0[] = {{3, 1, 7, 1}, {7, 1, 9, 4}, {9, 4, 9, 13}, {9, 13, 7, 16}, {7, 16, 3, 16}, {3, 16, 1, 13}, {1, 13, 1, 4}, {1, 4, 3, 1}, {8, 3, 2, 14}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyph1[] = {{3, 4, 5, 1}, {5, 1, 5, 16}, {2, 16, 8, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyph2[] = {{2, 4, 4, 1}, {4, 1, 7, 1}, {7, 1, 9, 4}, {9, 4, 1, 16}, {1, 16, 9, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyph3[] = {{1, 1, 8, 1}, {8, 1, 4, 7}, {4, 7, 7, 7}, {7, 7, 9, 11}, {9, 11, 7, 16}, {7, 16, 2, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyph4[] = {{7, 16, 7, 1}, {7, 1, 1, 11}, {1, 11, 9, 11}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyph5[] = {{8, 1, 2, 1}, {2, 1, 2, 7}, {2, 7, 7, 7}, {7, 7, 9, 11}, {9, 11, 7, 16}, {7, 16, 2, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyph6[] = {{8, 3, 4, 1}, {4, 1, 2, 6}, {2, 6, 2, 13}, {2, 13, 5, 16}, {5, 16, 8, 15}, {8, 15, 9, 11}, {9, 11, 7, 7}, {7, 7, 2, 7}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyph7[] = {{1, 1, 9, 1}, {9, 1, 4, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyph8[] = {{3, 1, 7, 1}, {7, 1, 9, 4}, {9, 4, 7, 8}, {7, 8, 3, 8}, {3, 8, 1, 4}, {1, 4, 3, 1}, {7, 8, 9, 12}, {9, 12, 7, 16}, {7, 16, 3, 16}, {3, 16, 1, 12}, {1, 12, 3, 8}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyph9[] = {{8, 9, 3, 9}, {3, 9, 1, 5}, {1, 5, 3, 1}, {3, 1, 7, 1}, {7, 1, 9, 5}, {9, 5, 9, 12}, {9, 12, 6, 16}, {6, 16, 2, 14}, {-1, 0, 0, 0}};

static const GlyphStroke s_glyphDot[] = {{5, 14, 5, 16}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphColon[] = {{5, 5, 5, 7}, {5, 12, 5, 14}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphDash[] = {{2, 8, 8, 8}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphPlus[] = {{2, 8, 8, 8}, {5, 5, 5, 11}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphPercent[] = {{2, 3, 3, 4}, {8, 13, 9, 14}, {9, 2, 1, 15}, {-1, 0, 0, 0}};
static const GlyphStroke s_glyphSlash[] = {{2, 16, 8, 1}, {-1, 0, 0, 0}};

static const GlyphStroke* getGlyphStrokes(char c) noexcept {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A'); // Small caps / uppercase
    switch (c) {
        case 'A': return s_glyphA;
        case 'B': return s_glyphB;
        case 'C': return s_glyphC;
        case 'D': return s_glyphD;
        case 'E': return s_glyphE;
        case 'F': return s_glyphF;
        case 'G': return s_glyphG;
        case 'H': return s_glyphH;
        case 'I': return s_glyphI;
        case 'J': return s_glyphJ;
        case 'K': return s_glyphK;
        case 'L': return s_glyphL;
        case 'M': return s_glyphM;
        case 'N': return s_glyphN;
        case 'O': return s_glyphO;
        case 'P': return s_glyphP;
        case 'Q': return s_glyphQ;
        case 'R': return s_glyphR;
        case 'S': return s_glyphS;
        case 'T': return s_glyphT;
        case 'U': return s_glyphU;
        case 'V': return s_glyphV;
        case 'W': return s_glyphW;
        case 'X': return s_glyphX;
        case 'Y': return s_glyphY;
        case 'Z': return s_glyphZ;
        case '0': return s_glyph0;
        case '1': return s_glyph1;
        case '2': return s_glyph2;
        case '3': return s_glyph3;
        case '4': return s_glyph4;
        case '5': return s_glyph5;
        case '6': return s_glyph6;
        case '7': return s_glyph7;
        case '8': return s_glyph8;
        case '9': return s_glyph9;
        case '.': return s_glyphDot;
        case ':': return s_glyphColon;
        case '-': return s_glyphDash;
        case '+': return s_glyphPlus;
        case '%': return s_glyphPercent;
        case '/': return s_glyphSlash;
        default: return nullptr;
    }
}

float SoftwareCanvas::measureStringWidth(std::string_view text, float scale) const noexcept {
    const float charW = 12.0f * scale;
    return text.size() * charW;
}

void SoftwareCanvas::drawString(std::string_view text, float x, float y, float scale, uint32_t color, float strokeWidth) noexcept {
    if (text.empty() || scale <= 0.0f) return;
    const float charAdvance = 12.0f * scale;
    const float normScale = scale;
    float curX = x;

    for (char c : text) {
        if (c == ' ') {
            curX += charAdvance * 0.75f;
            continue;
        }

        const GlyphStroke* strokes = getGlyphStrokes(c);
        if (strokes) {
            for (size_t i = 0; strokes[i].x0 >= 0; ++i) {
                const float p0x = curX + static_cast<float>(strokes[i].x0) * normScale;
                const float p0y = y + static_cast<float>(strokes[i].y0) * normScale;
                const float p1x = curX + static_cast<float>(strokes[i].x1) * normScale;
                const float p1y = y + static_cast<float>(strokes[i].y1) * normScale;
                drawLine(p0x, p0y, p1x, p1y, color, strokeWidth * scale);
            }
        }
        curX += charAdvance;
    }
}

void SoftwareCanvas::drawStringCentered(std::string_view text, float cx, float cy, float scale, uint32_t color, float strokeWidth) noexcept {
    const float w = measureStringWidth(text, scale);
    const float h = 16.0f * scale;
    drawString(text, cx - w * 0.5f, cy - h * 0.5f, scale, color, strokeWidth);
}

} // namespace eatsbits::export_clap::gui
