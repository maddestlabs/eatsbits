#include "eatsbits/ui/widgets/space_visualizer_widget.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace eatsbits::ui {

SpaceVisualizerWidget::SpaceVisualizerWidget(float /*height*/) {
    scopeL_.fill(0.0f);
    scopeR_.fill(0.0f);
}

void SpaceVisualizerWidget::feedAudio(const float* left, const float* right, size_t count) noexcept {
    if (!left || !right || count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        scopeL_[writeIdx_] = left[i];
        scopeR_[writeIdx_] = right[i];
        writeIdx_ = (writeIdx_ + 1) % SCOPE_POINTS;
    }

    updateStereoMetrics();
}

void SpaceVisualizerWidget::updateStereoMetrics() noexcept {
    float dotProduct = 0.0f;
    float energyL = 0.0f;
    float energyR = 0.0f;
    float midEnergy = 0.0f;
    float sideEnergy = 0.0f;

    for (size_t i = 0; i < SCOPE_POINTS; ++i) {
        const float l = scopeL_[i];
        const float r = scopeR_[i];
        const float m = 0.7071f * (l + r);
        const float s = 0.7071f * (l - r);

        dotProduct += l * r;
        energyL += l * l;
        energyR += r * r;
        midEnergy += m * m;
        sideEnergy += s * s;
    }

    const float denom = std::sqrt(energyL * energyR) + 1e-6f;
    metrics_.phaseCorrelation = std::clamp(dotProduct / denom, -1.0f, 1.0f);

    const float totalEnergy = energyL + energyR + 1e-6f;
    metrics_.balanceLR = std::clamp((energyR - energyL) / totalEnergy, -1.0f, 1.0f);

    metrics_.midEnergy = midEnergy;
    metrics_.sideEnergy = sideEnergy;
    metrics_.midSideRatio = midEnergy / (sideEnergy + 1e-6f);
}

void SpaceVisualizerWidget::layout(const Rect2D& bounds) {
    bounds_ = bounds;

    modeToggleBounds_ = Rect2D(bounds.x + 12.0f, bounds.y + 8.0f, 160.0f, 22.0f);

    const float sideW = 140.0f;
    scopeBounds_ = Rect2D(bounds.x + 12.0f, bounds.y + 34.0f, bounds.w - sideW - 32.0f, bounds.h - 42.0f);
    metricsBounds_ = Rect2D(scopeBounds_.right() + 12.0f, scopeBounds_.y, sideW, scopeBounds_.h);
}

void SpaceVisualizerWidget::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    // 1. Chassis container
    drawRoundedRect(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h, 8.0f, theme.panelBackground);
    drawRoundedRectOutline(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h, 8.0f, theme.borderSubtle, 1.0f);

    // 2. Mode Toggle Pill
    drawRoundedRect(r, modeToggleBounds_.x, modeToggleBounds_.y, modeToggleBounds_.w, modeToggleBounds_.h, 4.0f, theme.panelHeader);
    const float halfPill = modeToggleBounds_.w * 0.5f;

    bool isGonio = (mode_ == SpaceVisualizerMode::StereoGoniometer);
    drawRoundedRect(r, isGonio ? modeToggleBounds_.x : (modeToggleBounds_.x + halfPill),
                    modeToggleBounds_.y, halfPill, modeToggleBounds_.h, 4.0f, theme.primaryAccent);

    drawText(r, "GONIOMETER", modeToggleBounds_.x + 8.0f, modeToggleBounds_.y + 5.0f, 8.5f,
             isGonio ? Color(0.05f, 0.08f, 0.12f, 1.0f) : theme.textMuted);
    drawText(r, "2.5D ROOM", modeToggleBounds_.x + halfPill + 10.0f, modeToggleBounds_.y + 5.0f, 8.5f,
             !isGonio ? Color(0.05f, 0.08f, 0.12f, 1.0f) : theme.textMuted);

    // 3. Render Canvas based on Mode
    if (mode_ == SpaceVisualizerMode::StereoGoniometer) {
        // --- Lissajous Goniometer Scope ---
        const auto& sb = scopeBounds_;
        drawRoundedRect(r, sb.x, sb.y, sb.w, sb.h, 6.0f, Color(0.03f, 0.05f, 0.08f, 1.0f));
        drawRoundedRectOutline(r, sb.x, sb.y, sb.w, sb.h, 6.0f, Color(0.14f, 0.20f, 0.30f, 1.0f), 1.0f);

        const float cx = sb.x + sb.w * 0.5f;
        const float cy = sb.y + sb.h * 0.5f;
        const float radius = std::min(sb.w, sb.h) * 0.44f;

        // Polar concentric rings & axes
        drawCircleOutline(r, cx, cy, radius * 0.33f, Color(0.12f, 0.16f, 0.24f, 0.7f), 0.8f);
        drawCircleOutline(r, cx, cy, radius * 0.66f, Color(0.12f, 0.16f, 0.24f, 0.7f), 0.8f);
        drawCircleOutline(r, cx, cy, radius, Color(0.20f, 0.26f, 0.38f, 0.9f), 1.0f);

        // Diagonal 45-degree Left and Right axes
        drawLine(r, cx - radius * 0.707f, cy + radius * 0.707f, cx + radius * 0.707f, cy - radius * 0.707f, Color(0.18f, 0.25f, 0.35f, 0.8f), 1.0f);
        drawLine(r, cx - radius * 0.707f, cy - radius * 0.707f, cx + radius * 0.707f, cy + radius * 0.707f, Color(0.18f, 0.25f, 0.35f, 0.8f), 1.0f);

        // Center crosshairs (Mid vertical, Side horizontal)
        drawLine(r, cx, cy - radius, cx, cy + radius, Color(0.25f, 0.35f, 0.50f, 0.9f), 1.0f);
        drawLine(r, cx - radius, cy, cx + radius, cy, Color(0.25f, 0.35f, 0.50f, 0.9f), 1.0f);

        drawText(r, "+M (MONO)", cx - 22.0f, cy - radius - 10.0f, 8.0f, theme.primaryAccent);
        drawText(r, "-S", cx - radius - 12.0f, cy - 4.0f, 8.0f, theme.textMuted);
        drawText(r, "+S (STEREO)", cx + radius + 4.0f, cy - 4.0f, 8.0f, theme.textMuted);

        // Draw Lissajous Samples (Mid on Y, Side on X)
        for (size_t i = 0; i < SCOPE_POINTS; ++i) {
            float l = scopeL_[i];
            float rSample = scopeR_[i];

            // 45-degree rotation mapping
            float side = (l - rSample) * 0.7071f;
            float mid = (l + rSample) * 0.7071f;

            float px = cx + side * radius;
            float py = cy - mid * radius;

            drawCircle(r, px, py, 1.4f, Color(0.0f, 0.95f, 0.85f, 0.65f));
        }

        // --- Metrics Sidebar ---
        const auto& mb = metricsBounds_;
        drawRoundedRect(r, mb.x, mb.y, mb.w, mb.h, 6.0f, theme.panelHeader);
        drawRoundedRectOutline(r, mb.x, mb.y, mb.w, mb.h, 6.0f, theme.borderSubtle, 1.0f);

        drawText(r, "STEREO CORRELATION", mb.x + 8.0f, mb.y + 8.0f, 8.5f, theme.textPrimary);

        // Phase Correlation Meter (-1 to +1)
        float corY = mb.y + 24.0f;
        float corW = mb.w - 16.0f;
        drawRoundedRect(r, mb.x + 8.0f, corY, corW, 10.0f, 2.0f, Color(0.08f, 0.12f, 0.16f, 1.0f));

        float corNorm = std::clamp((metrics_.phaseCorrelation + 1.0f) * 0.5f, 0.0f, 1.0f);
        Color corCol = (metrics_.phaseCorrelation >= 0.0f) ? Color(0.0f, 1.0f, 0.40f, 1.0f) : Color(1.0f, 0.20f, 0.20f, 1.0f);
        drawRoundedRect(r, mb.x + 8.0f, corY, corW * corNorm, 10.0f, 2.0f, corCol);

        std::ostringstream corSs;
        corSs << (metrics_.phaseCorrelation >= 0 ? "+" : "") << std::fixed << std::setprecision(2) << metrics_.phaseCorrelation;
        drawText(r, corSs.str(), mb.x + 8.0f, corY + 14.0f, 9.0f, corCol);
        drawText(r, "-1 (OUT)", mb.x + 8.0f, corY + 26.0f, 7.5f, theme.textMuted);
        drawText(r, "+1 (IN)", mb.x + corW - 18.0f, corY + 26.0f, 7.5f, theme.textMuted);

        // Balance Meter (L <-> R)
        float balY = corY + 44.0f;
        drawText(r, "L/R BALANCE", mb.x + 8.0f, balY, 8.5f, theme.textPrimary);
        drawRoundedRect(r, mb.x + 8.0f, balY + 12.0f, corW, 8.0f, 2.0f, Color(0.08f, 0.12f, 0.16f, 1.0f));

        float balNorm = std::clamp((metrics_.balanceLR + 1.0f) * 0.5f, 0.0f, 1.0f);
        drawCircle(r, mb.x + 8.0f + corW * balNorm, balY + 16.0f, 5.0f, theme.primaryAccent);

        // M/S Ratio Readout
        float msY = balY + 36.0f;
        drawText(r, "M/S RATIO", mb.x + 8.0f, msY, 8.5f, theme.textPrimary);
        std::ostringstream msSs;
        msSs << std::fixed << std::setprecision(1) << metrics_.midSideRatio << ":1";
        drawText(r, msSs.str(), mb.x + 8.0f, msY + 12.0f, 9.5f, theme.textSecondary);

    } else {
        // --- 2.5D Room Perspective Visualizer ---
        const auto& sb = scopeBounds_;
        drawRoundedRect(r, sb.x, sb.y, sb.w, sb.h, 6.0f, Color(0.04f, 0.06f, 0.09f, 1.0f));
        drawRoundedRectOutline(r, sb.x, sb.y, sb.w, sb.h, 6.0f, Color(0.18f, 0.24f, 0.34f, 1.0f), 1.0f);

        // Perspective box coordinates
        float pad = 24.0f;
        float fL = sb.x + pad;
        float fR = sb.right() - pad;
        float fB = sb.bottom() - pad;
        float fT = sb.y + pad + 20.0f;

        // Back wall perspective vanishing box
        float bL = sb.x + pad * 2.2f;
        float bR = sb.right() - pad * 2.2f;
        float bB = sb.bottom() - pad * 2.2f;
        float bT = sb.y + pad + 36.0f;

        // Floor grid
        drawLine(r, fL, fB, bL, bB, Color(0.16f, 0.22f, 0.32f, 0.8f), 1.0f);
        drawLine(r, fR, fB, bR, bB, Color(0.16f, 0.22f, 0.32f, 0.8f), 1.0f);
        drawLine(r, bL, bB, bR, bB, Color(0.16f, 0.22f, 0.32f, 0.8f), 1.0f);
        drawLine(r, fL, fB, fR, fB, Color(0.16f, 0.22f, 0.32f, 0.8f), 1.0f);

        // Ceiling & Corner pillars
        drawLine(r, fL, fT, bL, bT, Color(0.12f, 0.16f, 0.24f, 0.6f), 0.8f);
        drawLine(r, fR, fT, bR, bT, Color(0.12f, 0.16f, 0.24f, 0.6f), 0.8f);
        drawLine(r, bL, bT, bR, bT, Color(0.12f, 0.16f, 0.24f, 0.6f), 0.8f);
        drawLine(r, bL, bT, bL, bB, Color(0.14f, 0.18f, 0.28f, 0.7f), 0.8f);
        drawLine(r, bR, bT, bR, bB, Color(0.14f, 0.18f, 0.28f, 0.7f), 0.8f);

        // Convert room source (x, z) to perspective screen coordinates
        auto roomToScreen = [&](float rx, float rz) -> std::pair<float, float> {
            float normZ = std::clamp(rz / room_.roomLength, 0.0f, 1.0f);
            float normX = std::clamp(rx / room_.roomWidth, 0.0f, 1.0f);

            float curL = fL + (bL - fL) * normZ;
            float curR = fR + (bR - fR) * normZ;
            float curY = fB + (bB - fB) * normZ;

            float curX = curL + (curR - curL) * normX;
            return {curX, curY};
        };

        auto [srcSx, srcSy] = roomToScreen(room_.sourceX, room_.sourceZ);
        auto [lisSx, lisSy] = roomToScreen(room_.listenerX, room_.listenerZ);

        // Direct sound ray
        drawLine(r, srcSx, srcSy, lisSx, lisSy, Color(0.0f, 0.95f, 1.0f, 0.85f), 1.8f);

        // Wall reflection bounce ray
        auto [bounceSx, bounceSy] = roomToScreen(0.0f, (room_.sourceZ + room_.listenerZ) * 0.5f);
        drawLine(r, srcSx, srcSy, bounceSx, bounceSy, Color(1.0f, 0.55f, 0.0f, 0.5f), 1.0f);
        drawLine(r, bounceSx, bounceSy, lisSx, lisSy, Color(1.0f, 0.55f, 0.0f, 0.5f), 1.0f);

        // Sound Source Icon (Orange speaker)
        drawCircle(r, srcSx, srcSy, 8.0f, Color(1.0f, 0.55f, 0.0f, 0.35f));
        drawCircle(r, srcSx, srcSy, 5.0f, Color(1.0f, 0.55f, 0.0f, 1.0f));
        drawText(r, "SOURCE", srcSx - 16.0f, srcSy + 8.0f, 8.0f, Color(1.0f, 0.65f, 0.20f, 1.0f));

        // Listener Icon (Cyan mic)
        drawCircle(r, lisSx, lisSy, 8.0f, Color(0.0f, 0.90f, 1.0f, 0.35f));
        drawCircle(r, lisSx, lisSy, 5.0f, Color(0.0f, 0.90f, 1.0f, 1.0f));
        drawText(r, "LISTENER", lisSx - 18.0f, lisSy + 8.0f, 8.0f, theme.primaryAccent);

        // --- Room Dimensions Sidebar ---
        const auto& mb = metricsBounds_;
        drawRoundedRect(r, mb.x, mb.y, mb.w, mb.h, 6.0f, theme.panelHeader);
        drawRoundedRectOutline(r, mb.x, mb.y, mb.w, mb.h, 6.0f, theme.borderSubtle, 1.0f);

        drawText(r, "ROOM DIMENSIONS", mb.x + 8.0f, mb.y + 8.0f, 8.5f, theme.textPrimary);

        auto drawDim = [&](const std::string& name, float val, float yOffset) {
            drawText(r, name, mb.x + 8.0f, mb.y + yOffset, 8.0f, theme.textMuted);
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1) << val << " m";
            drawText(r, ss.str(), mb.x + 8.0f, mb.y + yOffset + 12.0f, 9.5f, theme.textSecondary);
        };

        drawDim("WIDTH", room_.roomWidth, 24.0f);
        drawDim("LENGTH", room_.roomLength, 52.0f);
        drawDim("HEIGHT", room_.roomHeight, 80.0f);

        // Direct Distance calculation
        float dx = room_.listenerX - room_.sourceX;
        float dz = room_.listenerZ - room_.sourceZ;
        float dist = std::sqrt(dx * dx + dz * dz);
        drawDim("DIRECT DISTANCE", dist, 114.0f);
    }
}

bool SpaceVisualizerWidget::handlePointer(const PointerEvent& ev) {
    if (!bounds_.contains(ev.x, ev.y)) return false;

    if (ev.action == PointerAction::Down) {
        // Toggle mode
        if (modeToggleBounds_.contains(ev.x, ev.y)) {
            mode_ = (mode_ == SpaceVisualizerMode::StereoGoniometer)
                ? SpaceVisualizerMode::AcousticRoom2_5D
                : SpaceVisualizerMode::StereoGoniometer;
            return true;
        }

        // Room mode dragging
        if (mode_ == SpaceVisualizerMode::AcousticRoom2_5D && scopeBounds_.contains(ev.x, ev.y)) {
            // Drag source or listener
            float normX = std::clamp((ev.x - scopeBounds_.x) / scopeBounds_.w, 0.0f, 1.0f);
            float normZ = std::clamp((scopeBounds_.bottom() - ev.y) / scopeBounds_.h, 0.0f, 1.0f);

            float clickX = normX * room_.roomWidth;
            float clickZ = (1.0f - normZ) * room_.roomLength;

            float dSource = std::hypot(clickX - room_.sourceX, clickZ - room_.sourceZ);
            float dListener = std::hypot(clickX - room_.listenerX, clickZ - room_.listenerZ);

            if (dSource < dListener && dSource < 3.0f) {
                dragTarget_ = 1;
                room_.sourceX = clickX;
                room_.sourceZ = clickZ;
                if (onRoomCoordsChanged) onRoomCoordsChanged(room_);
                return true;
            } else if (dListener < 3.0f) {
                dragTarget_ = 2;
                room_.listenerX = clickX;
                room_.listenerZ = clickZ;
                if (onRoomCoordsChanged) onRoomCoordsChanged(room_);
                return true;
            }
        }
    }

    if (ev.action == PointerAction::Move && dragTarget_ != 0) {
        float normX = std::clamp((ev.x - scopeBounds_.x) / scopeBounds_.w, 0.0f, 1.0f);
        float normZ = std::clamp((scopeBounds_.bottom() - ev.y) / scopeBounds_.h, 0.0f, 1.0f);

        float newX = normX * room_.roomWidth;
        float newZ = (1.0f - normZ) * room_.roomLength;

        if (dragTarget_ == 1) {
            room_.sourceX = newX;
            room_.sourceZ = newZ;
        } else if (dragTarget_ == 2) {
            room_.listenerX = newX;
            room_.listenerZ = newZ;
        }
        if (onRoomCoordsChanged) onRoomCoordsChanged(room_);
        return true;
    }

    if (ev.action == PointerAction::Up) {
        dragTarget_ = 0;
        return true;
    }

    return true;
}

} // namespace eatsbits::ui
