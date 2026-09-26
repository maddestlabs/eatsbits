#include "eatsbits/ui/widgets/project_browser_drawer.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::ui {

ProjectBrowserDrawer::ProjectBrowserDrawer() {
    categories_ = {"ALL", "SYNTH", "BASS", "DRUMS", "CHIPTUNE", "PHYSICAL", "FX"};

    presets_ = {
        {"acid_303", "Acid Odyssey 303", "BASS", "Authentic Roland TB-303 Diode Ladder", "eats_303"},
        {"tr808_kit", "TR-808 Classic Kit", "DRUMS", "Analog Bridged-T Percussion", "drum_kit"},
        {"tr909_tech", "TR-909 Techno Drive", "DRUMS", "Punchy 909 Dance Kit", "drum_kit"},
        {"dx7_rhodes", "DX7 Electric Piano", "SYNTH", "6-Operator FM Dual Bus Synthesis", "dx7"},
        {"sid_lead", "C64 SID Space Arp", "CHIPTUNE", "MOS 6581 3-Voice 23-bit Noise", "sid"},
        {"snes_echo", "SNES Chrono Echo", "CHIPTUNE", "SPC700 16-Bit S-DSP 8-Tap Echo", "snes"},
        {"ym2612_fm", "Genesis Thunder FM", "CHIPTUNE", "OPN2 4-Op Logarithmic Feedback", "ym2612"},
        {"piano_grand", "Concert Grand Piano", "PHYSICAL", "Bank-Bensa Commuted Waveguide", "physical_piano"},
        {"upright_bass", "Upright Double Bass", "PHYSICAL", "Ebony Fingerboard Collision Growl", "upright_bass"},
        {"spanish_guitar", "Spanish Classical Guitar", "PHYSICAL", "Nylon String Torres Body Resonator", "guitar_nylon"},
        {"steel_acoustic", "Steel Acoustic Guitar", "PHYSICAL", "Plectrum Multi-Tap Comb Resonator", "guitar_steel"},
        {"cathedral_verb", "Stone Cathedral Reverb", "FX", "Zero-Latency Procedural Convolution", "convolver"}
    };

    macros_ = {
        {"macro_acid", "Acid 303 Generator", "SYNTH", "Generates randomized Roland acid lines", "macro"},
        {"macro_909", "909 Techno Synthesizer", "DRUMS", "Generates 4-on-the-floor industrial beats", "macro"},
        {"macro_humanize", "Global Humanizer", "UTILITY", "Micro-timing and velocity randomization", "macro"},
        {"macro_arp", "Instant Arpeggiator", "MIDI", "Transforms chords into rhythmic arps", "macro"}
    };
}

void ProjectBrowserDrawer::open() noexcept {
    isOpen_ = true;
}

void ProjectBrowserDrawer::close() noexcept {
    isOpen_ = false;
    if (onClose) onClose();
}

void ProjectBrowserDrawer::toggle() noexcept {
    if (isOpen_) close();
    else open();
}

void ProjectBrowserDrawer::layout(float screenWidth, float screenHeight, float topHeaderHeight, float bottomNavHeight) {
    float drawerH = screenHeight - topHeaderHeight - bottomNavHeight;
    float currentX = screenWidth - (kDrawerWidth * animProgress_);

    drawerBounds_ = Rect2D(currentX, topHeaderHeight, kDrawerWidth, drawerH);

    float headerY = topHeaderHeight + 8.0f;
    closeBtnBounds_ = Rect2D(currentX + kDrawerWidth - 32.0f, headerY, 24.0f, 24.0f);

    float tabY = headerY + 32.0f;
    float tabW = (kDrawerWidth - 24.0f) / 3.0f;
    tabPresetsBounds_ = Rect2D(currentX + 12.0f, tabY, tabW, 24.0f);
    tabMacrosBounds_ = Rect2D(currentX + 12.0f + tabW, tabY, tabW, 24.0f);
    tabHistoryBounds_ = Rect2D(currentX + 12.0f + (tabW * 2.0f), tabY, tabW, 24.0f);
}

void ProjectBrowserDrawer::update(float dt) {
    float target = isOpen_ ? 1.0f : 0.0f;
    float speed = 12.0f;
    animProgress_ += (target - animProgress_) * std::clamp(dt * speed, 0.0f, 1.0f);

    if (std::abs(animProgress_ - target) < 0.005f) {
        animProgress_ = target;
    }

    animOffset_ = kDrawerWidth * (1.0f - animProgress_);
}

void ProjectBrowserDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (animProgress_ <= 0.001f) return;

    if (isOpen_ && animProgress_ > 0.05f) {
        float shadowAlpha = 0.35f * animProgress_;
        drawRect(r, 0.0f, drawerBounds_.y, drawerBounds_.x, drawerBounds_.h,
                 0.0f, 0.0f, 0.0f, shadowAlpha);
    }

    drawRect(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.w, drawerBounds_.h,
             theme.panelBackground.r * 0.90f, theme.panelBackground.g * 0.90f, theme.panelBackground.b * 0.90f, 0.98f);

    drawLine(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.x, drawerBounds_.y + drawerBounds_.h,
             theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.7f * animProgress_, 2.0f);

    drawText(r, "PROJECT BROWSER", drawerBounds_.x + 14.0f, drawerBounds_.y + 12.0f, 12.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    drawButton(r, closeBtnBounds_, "X", theme.panelHeader, Color(0.0f, 0.0f, 0.0f, 0.0f), theme.primaryAccent, 11.0f, 4.0f, 0.0f);

    auto drawTab = [&](const Rect2D& b, const std::string& label, bool active) {
        if (active) {
            Color bg{theme.secondaryAccent.r * 0.3f, theme.secondaryAccent.g * 0.3f, theme.secondaryAccent.b * 0.3f, 0.95f};
            drawButton(r, b, label, bg, theme.secondaryAccent, theme.secondaryAccent, 10.0f, 3.0f, 1.0f);
        } else {
            drawButton(r, b, label, theme.panelHeader, Color(0.0f, 0.0f, 0.0f, 0.0f), theme.textMuted, 10.0f, 3.0f, 0.0f);
        }
    };

    drawTab(tabPresetsBounds_, "PRESETS", activeTab_ == BrowserDrawerTab::Presets);
    drawTab(tabMacrosBounds_, "MACROS", activeTab_ == BrowserDrawerTab::Macros);
    drawTab(tabHistoryBounds_, "HISTORY", activeTab_ == BrowserDrawerTab::History);

    float listY = tabPresetsBounds_.y + 36.0f;
    float cardH = 46.0f;

    const auto& list = (activeTab_ == BrowserDrawerTab::Presets) ? presets_ : macros_;

    for (size_t i = 0; i < list.size(); ++i) {
        float cy = listY + static_cast<float>(i) * (cardH + 6.0f) - scrollOffset_;
        if (cy + cardH < drawerBounds_.y || cy > drawerBounds_.y + drawerBounds_.h - 20.0f) {
            continue;
        }

        const auto& item = list[i];
        bool isSel = (static_cast<int>(i) == selectedIndex_);

        drawRoundedRect(r, drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH, 4.0f,
                        isSel ? theme.panelHeader.r * 1.3f : theme.panelHeader.r,
                        isSel ? theme.panelHeader.g * 1.3f : theme.panelHeader.g,
                        isSel ? theme.panelHeader.b * 1.3f : theme.panelHeader.b, 0.85f);

        if (isSel) {
            drawRoundedRectOutline(r, drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH, 4.0f,
                                   theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f, 1.0f);
        }

        drawText(r, item.name, drawerBounds_.x + 20.0f, cy + 6.0f, 11.0f,
                 isSel ? theme.primaryAccent.r : theme.textPrimary.r,
                 isSel ? theme.primaryAccent.g : theme.textPrimary.g,
                 isSel ? theme.primaryAccent.b : theme.textPrimary.b, 1.0f);

        drawText(r, item.description, drawerBounds_.x + 20.0f, cy + 24.0f, 9.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

        float tagW = 54.0f;
        float tagX = drawerBounds_.x + kDrawerWidth - 24.0f - tagW;
        drawRoundedRect(r, tagX, cy + 6.0f, tagW, 16.0f, 2.0f,
                        theme.secondaryAccent.r * 0.2f, theme.secondaryAccent.g * 0.2f, theme.secondaryAccent.b * 0.2f, 0.8f);
        drawCenteredText(r, item.category, tagX, cy + 6.0f, tagW, 16.0f, 8.5f,
                         theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.95f);
    }
}

bool ProjectBrowserDrawer::handlePointer(const PointerEvent& ev) {
    if (animProgress_ <= 0.05f) return false;

    if (isOpen_ && ev.action == PointerAction::Down && ev.x < drawerBounds_.x) {
        close();
        return true;
    }

    if (!drawerBounds_.contains(ev.x, ev.y)) return false;

    if (ev.action == PointerAction::Down) {
        if (closeBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        if (tabPresetsBounds_.contains(ev.x, ev.y)) {
            setTab(BrowserDrawerTab::Presets);
            return true;
        }
        if (tabMacrosBounds_.contains(ev.x, ev.y)) {
            setTab(BrowserDrawerTab::Macros);
            return true;
        }
        if (tabHistoryBounds_.contains(ev.x, ev.y)) {
            setTab(BrowserDrawerTab::History);
            return true;
        }

        float listY = tabPresetsBounds_.y + 36.0f;
        float cardH = 46.0f;
        const auto& list = (activeTab_ == BrowserDrawerTab::Presets) ? presets_ : macros_;

        for (size_t i = 0; i < list.size(); ++i) {
            float cy = listY + static_cast<float>(i) * (cardH + 6.0f) - scrollOffset_;
            Rect2D cardRect(drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH);
            if (cardRect.contains(ev.x, ev.y)) {
                selectedIndex_ = static_cast<int>(i);
                if (activeTab_ == BrowserDrawerTab::Presets && onSelectPreset) {
                    onSelectPreset(list[i].id);
                } else if (activeTab_ == BrowserDrawerTab::Macros && onRunMacro) {
                    onRunMacro(list[i].id);
                }
                return true;
            }
        }
    } else if (ev.action == PointerAction::Scroll) {
        scrollOffset_ -= ev.scrollY * 24.0f;
        scrollOffset_ = std::max(0.0f, scrollOffset_);
        return true;
    }

    return true;
}

} // namespace eatsbits::ui
