#include "eatsbits/ui/widgets/bottom_nav_bar.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>

namespace eatsbits::ui {

void BottomNavBar::layout(float screenWidth, float screenHeight, float height) {
    bounds_ = Rect2D(0.0f, screenHeight - height, screenWidth, height);

    float padding = 8.0f;
    float availW = screenWidth - (padding * 2.0f);
    float btnW = (availW - (4.0f * 8.0f)) / 5.0f;
    float btnH = height - 12.0f;
    float btnY = bounds_.y + 6.0f;

    for (int i = 0; i < 5; ++i) {
        float bx = padding + static_cast<float>(i) * (btnW + 8.0f);
        tabBounds_[i] = Rect2D(bx, btnY, btnW, btnH);
    }
}

void BottomNavBar::render(BatchRenderer2D& r, const ThemeTokens& theme, int activeTab, bool isMobile) {
    // 1. Heavy Weathered Metal Chin Background Plate
    drawRect(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h,
             theme.panelHeader.r * 0.88f, theme.panelHeader.g * 0.88f, theme.panelHeader.b * 0.88f, 1.0f);
    
    // Top specular bevel line & shadow crevice along screen seam
    drawLine(r, bounds_.x, bounds_.y, bounds_.x + bounds_.w, bounds_.y,
             theme.borderSubtle.r * 1.6f, theme.borderSubtle.g * 1.6f, theme.borderSubtle.b * 1.6f, 0.85f, 1.5f);
    drawLine(r, bounds_.x, bounds_.y + 1.5f, bounds_.x + bounds_.w, bounds_.y + 1.5f,
             theme.backgroundDark.r * 0.5f, theme.backgroundDark.g * 0.5f, theme.backgroundDark.b * 0.5f, 0.70f, 1.0f);

    // Subtle horizontal brushed metal grain striations on chin plate
    for (float gy = bounds_.y + 3.0f; gy < bounds_.y + bounds_.h - 2.0f; gy += 4.0f) {
        int hash = static_cast<int>(gy * 11.3f) % 5;
        float alpha = (hash - 2) * 0.015f;
        if (std::abs(alpha) > 0.005f) {
            drawLine(r, bounds_.x, gy, bounds_.x + bounds_.w, gy,
                     (alpha > 0) ? 1.0f : 0.0f, (alpha > 0) ? 1.0f : 0.0f, (alpha > 0) ? 1.0f : 0.0f,
                     std::abs(alpha), 1.0f);
        }
    }

    const char* tabNames[5] = {
        "ARRANGER",
        "EDIT",
        "TRACK",
        "MIXER",
        "DESIGN"
    };

    for (int i = 0; i < 5; ++i) {
        const auto& b = tabBounds_[i];
        bool isActive = (i == activeTab);

        // A. Recessed button well (chunky hardware socket)
        drawRoundedRect(r, b.x - 1.5f, b.y - 1.5f, b.w + 3.0f, b.h + 3.0f, 4.0f,
                        theme.backgroundDark.r * 0.5f, theme.backgroundDark.g * 0.5f, theme.backgroundDark.b * 0.5f, 0.95f);
        drawRoundedRectOutline(r, b.x - 1.5f, b.y - 1.5f, b.w + 3.0f, b.h + 3.0f, 4.0f,
                               theme.backgroundDark.r * 0.25f, theme.backgroundDark.g * 0.25f, theme.backgroundDark.b * 0.25f, 0.85f, 1.0f);

        // B. Deep drop shadow under keycap
        drawRoundedRect(r, b.x, b.y + 2.5f, b.w, b.h, 3.5f,
                        0.0f, 0.0f, 0.0f, 0.45f);

        // C. Keycap faceplate (chunky 3D mechanical switch)
        float capR = isActive ? theme.panelBackground.r * 1.55f : theme.panelBackground.r * 1.15f;
        float capG = isActive ? theme.panelBackground.g * 1.55f : theme.panelBackground.g * 1.15f;
        float capB = isActive ? theme.panelBackground.b * 1.55f : theme.panelBackground.b * 1.15f;
        drawRoundedRect(r, b.x, b.y, b.w, b.h, 3.5f, capR, capG, capB, 0.98f);

        // D. Top specular highlight bevel line on keycap
        drawLine(r, b.x + 3.0f, b.y + 1.2f, b.x + b.w - 3.0f, b.y + 1.2f,
                 capR * 1.6f, capG * 1.6f, capB * 1.6f, 0.9f, 1.2f);
        // Bottom shadow bevel line on keycap
        drawLine(r, b.x + 3.0f, b.y + b.h - 1.2f, b.x + b.w - 3.0f, b.y + b.h - 1.2f,
                 capR * 0.45f, capG * 0.45f, capB * 0.45f, 0.9f, 1.2f);

        // E. Keycap border outline
        drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 3.5f,
                               isActive ? theme.primaryAccent.r : theme.borderSubtle.r * 0.8f,
                               isActive ? theme.primaryAccent.g : theme.borderSubtle.g * 0.8f,
                               isActive ? theme.primaryAccent.b : theme.borderSubtle.b * 0.8f,
                               isActive ? 0.95f : 0.65f, isActive ? 1.5f : 1.0f);

        // F. Illuminated LED indicator bar or tally
        float ledW = 14.0f;
        float ledH = 2.5f;
        float ledX = b.x + (b.w - ledW) * 0.5f;
        float ledY = b.y + 4.5f;
        if (isActive) {
            // Glowing LED with soft halo
            drawRoundedRect(r, ledX - 1.0f, ledY - 1.0f, ledW + 2.0f, ledH + 2.0f, 1.5f,
                            theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.40f);
            drawRoundedRect(r, ledX, ledY, ledW, ledH, 1.0f,
                            theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
        } else {
            drawRoundedRect(r, ledX, ledY, ledW, ledH, 1.0f, 0.18f, 0.19f, 0.22f, 0.70f);
        }

        // G. Chunky tactile label (illuminated amber legend if active, muted retro cream if inactive)
        std::string label = tabNames[i];
        float fontSize = isMobile ? 9.5f : 11.0f;
        drawCenteredText(r, label, b.x, b.y + 8.0f, b.w, b.h - 8.0f, fontSize,
                         isActive ? theme.primaryAccent.r : theme.textPrimary.r * 0.92f,
                         isActive ? theme.primaryAccent.g : theme.textPrimary.g * 0.92f,
                         isActive ? theme.primaryAccent.b : theme.textPrimary.b * 0.92f, 1.0f);
    }
}

bool BottomNavBar::handlePointer(const PointerEvent& ev, int& outNewTab) {
    if (ev.action != PointerAction::Down) return false;

    for (int i = 0; i < 5; ++i) {
        if (tabBounds_[i].contains(ev.x, ev.y)) {
            outNewTab = i;
            if (onTabSelected) onTabSelected(i);
            return true;
        }
    }

    return bounds_.contains(ev.x, ev.y);
}

bool BottomNavBar::handleKey(int key, int scancode, int action, int mods, int& outNewTab) {
    if (action != 1) return false;

    if (key >= 49 && key <= 53) {
        outNewTab = key - 49;
        if (onTabSelected) onTabSelected(outNewTab);
        return true;
    }
    if (key >= 290 && key <= 294) {
        outNewTab = key - 290;
        if (onTabSelected) onTabSelected(outNewTab);
        return true;
    }

    return false;
}

} // namespace eatsbits::ui
