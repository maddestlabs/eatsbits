#include "eatsbits/ui/widgets/drum_pad_grid_widget.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>

namespace eatsbits::ui {

DrumPadGridWidget::DrumPadGridWidget() {
    initFactoryPads();
}

void DrumPadGridWidget::initFactoryPads() {
    corePads_.clear();
    percPads_.clear();

    // -------------------------------------------------------------
    // Core Kit (16 Pads)
    // -------------------------------------------------------------
    // ROW 1: Cymbals
    corePads_.push_back({49, "CRASH 1", "Crash Cymbal", Color(1.0f, 0.80f, 0.0f, 1.0f)});
    corePads_.push_back({51, "RIDE 1", "Ride Cymbal", Color(1.0f, 0.835f, 0.31f, 1.0f)});
    corePads_.push_back({53, "RIDE BELL", "Ride Bell", Color(1.0f, 0.878f, 0.51f, 1.0f)});
    corePads_.push_back({55, "SPLASH", "Splash Cymbal", Color(1.0f, 0.96f, 0.616f, 1.0f)});

    // ROW 2: Toms
    corePads_.push_back({50, "HIGH TOM", "FM Tom", Color(0.67f, 0.28f, 0.74f, 1.0f)});
    corePads_.push_back({47, "LOW-MID TOM", "FM Tom", Color(0.557f, 0.14f, 0.667f, 1.0f)});
    corePads_.push_back({43, "HI FLOOR", "FM Tom", Color(0.416f, 0.106f, 0.604f, 1.0f)});
    corePads_.push_back({41, "LOW FLOOR", "FM Tom", Color(0.29f, 0.078f, 0.549f, 1.0f)});

    // ROW 3: Hats & Electronic Snare
    corePads_.push_back({42, "CLOSED HAT", "Inharmonic Hat", Color(0.149f, 0.776f, 0.855f, 1.0f)});
    corePads_.push_back({44, "PEDAL HAT", "Inharmonic Hat", Color(0.0f, 0.675f, 0.757f, 1.0f)});
    corePads_.push_back({46, "OPEN HAT", "Inharmonic Hat", Color(0.0f, 0.514f, 0.561f, 1.0f)});
    corePads_.push_back({40, "ELEC SNARE", "Snare Synth", Color(1.0f, 0.439f, 0.263f, 1.0f)});

    // ROW 4: Kicks & Snares
    corePads_.push_back({36, "KICK 1", "FM Acoustic Kick", Color(1.0f, 0.09f, 0.267f, 1.0f)});
    corePads_.push_back({38, "AC. SNARE", "FM Acoustic Snare", Color(1.0f, 0.569f, 0.0f, 1.0f)});
    corePads_.push_back({37, "SIDE STICK", "Wood Clack", Color(0.553f, 0.431f, 0.388f, 1.0f)});
    corePads_.push_back({39, "HAND CLAP", "Cluster Clap", Color(1.0f, 0.322f, 0.322f, 1.0f)});

    // -------------------------------------------------------------
    // Percussion Kit (16 Pads)
    // -------------------------------------------------------------
    // ROW 1: Latin Cymbals & Bells
    percPads_.push_back({57, "CRASH 2", "Crash Cymbal", Color(1.0f, 0.702f, 0.0f, 1.0f)});
    percPads_.push_back({52, "CHINA", "China Cymbal", Color(1.0f, 0.561f, 0.0f, 1.0f)});
    percPads_.push_back({56, "COWBELL", "Tuned FM Bell", Color(0.0f, 0.902f, 0.463f, 1.0f)});
    percPads_.push_back({54, "TAMBOURINE", "Jingle Burst", Color(0.412f, 0.941f, 0.682f, 1.0f)});

    // ROW 2: Bongos & Timbales
    percPads_.push_back({60, "HI BONGO", "Slap FM Bongo", Color(0.0f, 0.69f, 1.0f, 1.0f)});
    percPads_.push_back({61, "LOW BONGO", "Slap FM Bongo", Color(0.0f, 0.569f, 0.918f, 1.0f)});
    percPads_.push_back({65, "HI TIMBALE", "FM Timbale", Color(0.161f, 0.475f, 1.0f, 1.0f)});
    percPads_.push_back({66, "LOW TIMBALE", "FM Timbale", Color(0.082f, 0.396f, 0.753f, 1.0f)});

    // ROW 3: Congas & Agogo
    percPads_.push_back({62, "MUTE CONGA", "Mute Conga", Color(0.878f, 0.251f, 0.984f, 1.0f)});
    percPads_.push_back({63, "OPEN CONGA", "Open Conga", Color(0.835f, 0.0f, 0.976f, 1.0f)});
    percPads_.push_back({64, "LOW CONGA", "Low Conga", Color(0.667f, 0.0f, 1.0f, 1.0f)});
    percPads_.push_back({67, "HI AGOGO", "FM Agogo", Color(1.0f, 0.251f, 0.506f, 1.0f)});

    // ROW 4: Wood & Shakers
    percPads_.push_back({75, "CLAVES", "Hardwood Clave", Color(0.843f, 0.8f, 0.784f, 1.0f)});
    percPads_.push_back({76, "HI BLOCK", "Wood Block", Color(0.737f, 0.667f, 0.643f, 1.0f)});
    percPads_.push_back({70, "MARACAS", "Noise Shaker", Color(0.502f, 0.796f, 0.769f, 1.0f)});
    percPads_.push_back({81, "TRIANGLE", "FM Triangle", Color(0.698f, 1.0f, 0.349f, 1.0f)});
}

void DrumPadGridWidget::setBank(DrumKitBank bank) noexcept {
    activeBank_ = bank;
    releaseAllPads();
}

void DrumPadGridWidget::triggerPad(uint8_t note, float velocity) noexcept {
    auto& pads = (activeBank_ == DrumKitBank::CoreKit) ? corePads_ : percPads_;
    for (auto& pad : pads) {
        if (pad.note == note) {
            pad.isTriggered = true;
            pad.triggerTimer = 0.16f; // Flash duration in seconds
            break;
        }
    }
    if (onPadTrigger) {
        onPadTrigger(note, velocity);
    }
}

void DrumPadGridWidget::releasePad(uint8_t note) noexcept {
    if (onPadRelease) {
        onPadRelease(note);
    }
}

void DrumPadGridWidget::releaseAllPads() noexcept {
    auto& pads = (activeBank_ == DrumKitBank::CoreKit) ? corePads_ : percPads_;
    for (auto& pad : pads) {
        if (pad.isTriggered) {
            pad.isTriggered = false;
            pad.triggerTimer = 0.0f;
            if (onPadRelease) onPadRelease(pad.note);
        }
    }
    activePadPointers_.clear();
}

void DrumPadGridWidget::layout(const Rect2D& bounds) noexcept {
    bounds_ = bounds;

    const float headerH = 22.0f;
    headerBounds_ = Rect2D(bounds.x, bounds.y, bounds.w, headerH);

    // Bank buttons in header
    const float bankBtnW = 96.0f;
    const float bankBtnH = 18.0f;
    float rightMargin = bounds.x + bounds.w - 12.0f;
    bankPercBtnBounds_ = Rect2D(rightMargin - bankBtnW, bounds.y + 2.0f, bankBtnW, bankBtnH);
    bankCoreBtnBounds_ = Rect2D(bankPercBtnBounds_.x - bankBtnW - 4.0f, bounds.y + 2.0f, bankBtnW, bankBtnH);

    // Grid bounds
    const float gridTop = bounds.y + headerH + 4.0f;
    const float gridH = bounds.h - headerH - 8.0f;
    gridBounds_ = Rect2D(bounds.x + 8.0f, gridTop, bounds.w - 16.0f, gridH);

    // Compute 4x4 pad positions
    const float gap = 6.0f;
    const float padW = (gridBounds_.w - 3.0f * gap) / 4.0f;
    const float padH = (gridBounds_.h - 3.0f * gap) / 4.0f;

    auto layoutList = [padW, padH, gap, this](std::vector<DrumPadDef>& pads) {
        for (size_t i = 0; i < pads.size() && i < 16; ++i) {
            size_t row = i / 4;
            size_t col = i % 4;
            float px = gridBounds_.x + static_cast<float>(col) * (padW + gap);
            float py = gridBounds_.y + static_cast<float>(row) * (padH + gap);
            pads[i].bounds = Rect2D(px, py, padW, padH);
        }
    };

    layoutList(corePads_);
    layoutList(percPads_);
}

void DrumPadGridWidget::update(float dt) noexcept {
    auto updatePads = [dt](std::vector<DrumPadDef>& pads) {
        for (auto& pad : pads) {
            if (pad.triggerTimer > 0.0f) {
                pad.triggerTimer -= dt;
                if (pad.triggerTimer <= 0.0f) {
                    pad.triggerTimer = 0.0f;
                    pad.isTriggered = false;
                }
            }
        }
    };
    updatePads(corePads_);
    updatePads(percPads_);
}

void DrumPadGridWidget::render(BatchRenderer2D& r, const ThemeTokens& theme) noexcept {
    if (bounds_.w <= 10.0f || bounds_.h <= 10.0f) return;

    // 1. Chassis Background with Subtle Bevel
    drawRoundedRect(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h, 6.0f,
                    0.09f, 0.095f, 0.11f, 0.98f);
    drawRoundedRectOutline(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.2f);

    // 2. Header Bar
    Color kitAccent = (profile_ == DrumKitProfile::Eats808)
                          ? Color(1.0f, 0.60f, 0.0f, 1.0f)   // 808 Orange
                          : ((profile_ == DrumKitProfile::Eats909)
                                 ? Color(0.16f, 0.71f, 0.96f, 1.0f) // 909 Cyan
                                 : theme.primaryAccent);

    std::string titleStr = (profile_ == DrumKitProfile::Eats808)
                               ? "EATS-808 DRUM MACHINE PADS"
                               : ((profile_ == DrumKitProfile::Eats909)
                                      ? "EATS-909 PERCUSSION PADS"
                                      : "MODULAR DRUM PADS (MPC MATRIX)");

    drawText(r, titleStr, bounds_.x + 12.0f, bounds_.y + 6.0f, 10.0f, kitAccent);

    // Dynamic Sensitivity Hint
    std::string hintStr = "TOUCH SENSITIVE (TAP TOP=LOUD, BOTTOM=SOFT)";
    drawText(r, hintStr, bounds_.x + 220.0f, bounds_.y + 7.0f, 8.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.65f);

    // Bank Buttons
    bool isCore = (activeBank_ == DrumKitBank::CoreKit);
    drawButton(r, bankCoreBtnBounds_, "CORE KIT (16)",
               isCore ? kitAccent.withAlpha(0.25f) : theme.controlBackground,
               isCore ? kitAccent : theme.borderSubtle,
               isCore ? kitAccent : theme.textMuted,
               8.5f, 3.0f, 1.0f);

    drawButton(r, bankPercBtnBounds_, "PERCUSSION (16)",
               !isCore ? kitAccent.withAlpha(0.25f) : theme.controlBackground,
               !isCore ? kitAccent : theme.borderSubtle,
               !isCore ? kitAccent : theme.textMuted,
               8.5f, 3.0f, 1.0f);

    // 3. Render 4x4 Pad Grid
    const auto& pads = (activeBank_ == DrumKitBank::CoreKit) ? corePads_ : percPads_;
    for (size_t i = 0; i < pads.size() && i < 16; ++i) {
        const auto& pad = pads[i];
        const auto& pb = pad.bounds;

        // Base Rubber Pad Fill
        Color padBaseBg = pad.isTriggered
                              ? pad.accentColor.withAlpha(0.55f)
                              : Color(0.14f, 0.15f, 0.18f, 0.95f);
        drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 4.0f, padBaseBg);

        // Tactile Chamfered Rim / Border
        Color padRimColor = pad.isTriggered
                                ? pad.accentColor
                                : Color(0.22f, 0.24f, 0.29f, 0.85f);
        float padRimWidth = pad.isTriggered ? 2.0f : 1.2f;
        drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 4.0f, padRimColor, padRimWidth);

        // Subtle Inner Pad Shadow / Recess
        drawRect(r, pb.x + 3.0f, pb.y + 3.0f, pb.w - 6.0f, 1.5f,
                 0.0f, 0.0f, 0.0f, pad.isTriggered ? 0.0f : 0.35f);

        // Top Row: Note Number (Left) and Trigger LED Indicator (Right)
        std::string noteStr = std::to_string(pad.note);
        drawMonoText(r, noteStr, pb.x + 6.0f, pb.y + 4.0f, 8.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.70f);

        // LED dot in upper-right corner
        float ledX = pb.x + pb.w - 8.0f;
        float ledY = pb.y + 7.0f;
        if (pad.isTriggered) {
            drawCircle(r, ledX, ledY, 3.2f, pad.accentColor.r, pad.accentColor.g, pad.accentColor.b, 1.0f);
        } else {
            drawCircle(r, ledX, ledY, 2.2f, 0.25f, 0.27f, 0.32f, 0.60f);
        }

        // Center Pad Instrument Title
        Color labelColor = pad.isTriggered ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textPrimary;
        drawCenteredText(r, pad.label, pb.x + 4.0f, pb.y + (pb.h - 12.0f) * 0.42f,
                         pb.w - 8.0f, 14.0f, 9.5f, labelColor);

        // Bottom Voice Engine Badge
        const float badgeH = 11.0f;
        const float badgeW = pb.w - 12.0f;
        const float badgeX = pb.x + 6.0f;
        const float badgeY = pb.y + pb.h - badgeH - 4.0f;

        Color badgeBg = pad.isTriggered
                            ? Color(0.0f, 0.0f, 0.0f, 0.45f)
                            : Color(0.0f, 0.0f, 0.0f, 0.25f);
        drawRoundedRect(r, badgeX, badgeY, badgeW, badgeH, 2.0f, badgeBg);

        Color badgeTextCol = pad.isTriggered ? pad.accentColor : theme.textMuted;
        drawCenteredText(r, pad.defaultEngine, badgeX, badgeY, badgeW, badgeH, 7.5f, badgeTextCol);
    }
}

bool DrumPadGridWidget::handlePointer(const PointerEvent& ev) noexcept {
    if (!bounds_.contains(ev.x, ev.y)) return false;

    // 1. Bank Switch Buttons
    if (ev.action == PointerAction::Down) {
        if (bankCoreBtnBounds_.contains(ev.x, ev.y)) {
            setBank(DrumKitBank::CoreKit);
            return true;
        }
        if (bankPercBtnBounds_.contains(ev.x, ev.y)) {
            setBank(DrumKitBank::Percussion);
            return true;
        }

        // 2. Pad Grid Hit-Testing (Multi-Touch Polyphony)
        auto& pads = (activeBank_ == DrumKitBank::CoreKit) ? corePads_ : percPads_;
        for (size_t i = 0; i < pads.size() && i < 16; ++i) {
            if (pads[i].bounds.contains(ev.x, ev.y)) {
                activePadPointers_[ev.id] = static_cast<int>(i);

                // Y-axis velocity sensitivity:
                // Touching top of pad gives velocity ~ 1.0; bottom gives ~ 0.65
                float relY = (ev.y - pads[i].bounds.y) / std::max(1.0f, pads[i].bounds.h);
                float velocity = (ev.pressure > 0.05f) ? ev.pressure : std::clamp(1.0f - relY * 0.35f, 0.55f, 1.0f);

                triggerPad(pads[i].note, velocity);
                return true;
            }
        }
    } else if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        auto it = activePadPointers_.find(ev.id);
        if (it != activePadPointers_.end()) {
            int padIdx = it->second;
            activePadPointers_.erase(it);
            auto& pads = (activeBank_ == DrumKitBank::CoreKit) ? corePads_ : percPads_;
            if (padIdx >= 0 && padIdx < static_cast<int>(pads.size())) {
                releasePad(pads[padIdx].note);
            }
            return true;
        }
    }

    return true; // Absorb events within widget bounds
}

} // namespace eatsbits::ui
