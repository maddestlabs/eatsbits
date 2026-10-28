#include "eatsbits/ui/widgets/tracker_cell_dialog.hpp"
#include "eatsbits/ui/views/edit_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/widgets/piano_keyboard.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace eatsbits::ui {

namespace {

constexpr int kMinPitch = 12;  // C0
constexpr int kMaxPitch = 108; // C8

struct FxPresetDef {
    const char* code;
    const char* label;
};

const FxPresetDef kFxPresets[] = {
    {"00", "00 NONE"},
    {"01", "01 UP"},
    {"02", "02 DN"},
    {"03", "03 TONE"},
    {"04", "04 VIB"},
    {"0A", "0A VOL"}
};

inline std::string intToHex2(int val) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << std::clamp(val, 0, 255);
    return ss.str();
}

inline int hex2ToInt(const std::string& hexStr) {
    if (hexStr.empty() || hexStr == "..") return 0;
    try {
        return std::stoi(hexStr, nullptr, 16);
    } catch (...) {
        return 0;
    }
}

} // namespace

TrackerCellDialog::TrackerCellDialog() = default;

void TrackerCellDialog::open(int step, int lane, const std::string& trackName, const PianoRollNote* note,
                             const std::vector<std::string>& customColKeys) {
    step_ = step;
    lane_ = lane;
    trackName_ = trackName;
    isOpen_ = true;
    isDraggingPitch_ = false;
    isDraggingVelocity_ = false;
    draggingChipFxIdx_ = -1;

    if (note) {
        hasExistingNote_ = true;
        pitch_ = std::clamp(static_cast<int>(note->pitch), kMinPitch, kMaxPitch);
        velocity_ = std::clamp(note->velocity, 0.05f, 1.0f);
        isSlide_ = note->isSlide;
        effectCommand_ = note->effectCommand.empty() ? "00" : note->effectCommand;
        customParams_ = note->customParams;
    } else {
        hasExistingNote_ = false;
        pitch_ = 60; // Default C4
        velocity_ = 0.85f;
        isSlide_ = false;
        effectCommand_ = "00";
        customParams_.clear();
    }

    // Prepare Chip FX entries
    chipFxEntries_.clear();
    std::vector<std::string> keys = customColKeys;
    if (keys.empty()) {
        keys = {"echo", "alg", "fb"};
    }
    for (const auto& k : keys) {
        TrackerChipFxEntry entry;
        entry.key = k;
        std::string lbl = k;
        for (auto& c : lbl) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        entry.label = lbl;

        auto it = customParams_.find(k);
        if (it != customParams_.end() && !it->second.empty()) {
            entry.value = it->second;
        } else {
            entry.value = "00";
        }
        chipFxEntries_.push_back(entry);
    }
}

void TrackerCellDialog::auditionCurrentPitch() {
    if (onAudition) {
        onAudition(pitch_, velocity_);
    }
}

void TrackerCellDialog::layout(float screenW, float screenH) {
    const float w = std::min(500.0f, screenW - 20.0f);
    const float h = std::min(520.0f, screenH - 20.0f);
    const float x = (screenW - w) * 0.5f;
    const float y = (screenH - h) * 0.5f;

    dialogBounds_ = Rect2D(x, y, w, h);
    closeBtnBounds_ = Rect2D(x + w - 34.0f, y + 10.0f, 24.0f, 24.0f);

    float curY = y + 50.0f;

    // 1. Pitch Section (C0 to C8 Single Continuous Slider)
    auditionBtnBounds_ = Rect2D(x + w - 116.0f, curY, 96.0f, 24.0f);
    curY += 26.0f;

    pitchDownBtnBounds_ = Rect2D(x + 20.0f, curY, 28.0f, 24.0f);
    pitchSliderBounds_ = Rect2D(x + 54.0f, curY, w - 40.0f - 68.0f, 24.0f);
    pitchUpBtnBounds_ = Rect2D(x + w - 48.0f, curY, 28.0f, 24.0f);
    curY += 38.0f;

    // 2. Velocity Section
    velSliderBounds_ = Rect2D(x + 20.0f, curY + 14.0f, w - 40.0f, 16.0f);
    curY += 36.0f;

    velPresetBounds_.clear();
    const float presetW = (w - 40.0f - 18.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        velPresetBounds_.push_back(Rect2D(x + 20.0f + static_cast<float>(i) * (presetW + 6.0f), curY, presetW, 22.0f));
    }
    curY += 30.0f;

    // 3. Slide & FX Section
    slideToggleBounds_ = Rect2D(x + 20.0f, curY, 130.0f, 24.0f);

    fxPresetBounds_.clear();
    const float fxStartX = slideToggleBounds_.right() + 10.0f;
    const float fxTotalW = x + w - 20.0f - fxStartX;
    const float fxW = (fxTotalW - (5.0f * 4.0f)) / 5.0f;
    for (int i = 0; i < 5; ++i) {
        fxPresetBounds_.push_back(Rect2D(fxStartX + static_cast<float>(i) * (fxW + 4.0f), curY, fxW, 24.0f));
    }
    curY += 34.0f;

    // 4. Chip FX Section
    const float chipRowH = 26.0f;
    for (size_t i = 0; i < chipFxEntries_.size() && i < 3; ++i) {
        auto& entry = chipFxEntries_[i];
        float rowY = curY + static_cast<float>(i) * (chipRowH + 6.0f);
        entry.downBtnBounds = Rect2D(x + 120.0f, rowY, 26.0f, chipRowH);
        entry.sliderBounds = Rect2D(x + 152.0f, rowY, w - 198.0f - 30.0f, chipRowH);
        entry.upBtnBounds = Rect2D(entry.sliderBounds.right() + 6.0f, rowY, 26.0f, chipRowH);
    }

    // 5. Action Buttons at Bottom
    const float botY = y + h - 40.0f;
    deleteBtnBounds_ = Rect2D(x + 20.0f, botY, 110.0f, 28.0f);
    applyBtnBounds_ = Rect2D(x + w - 110.0f, botY, 90.0f, 28.0f);
    cancelBtnBounds_ = Rect2D(applyBtnBounds_.x - 80.0f, botY, 70.0f, 28.0f);
}

void TrackerCellDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-transparent backdrop overlay
    drawRect(r, 0.0f, 0.0f, 4000.0f, 4000.0f, Color(0.0f, 0.0f, 0.0f, 0.72f));

    // 2. Dialog Chassis
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 10.0f, theme.panelBackground);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 10.0f, theme.primaryAccent, 1.8f);

    // Header bar
    float hdrH = 40.0f;
    drawRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, hdrH, theme.panelHeader);
    drawLine(r, dialogBounds_.x, dialogBounds_.y + hdrH, dialogBounds_.x + dialogBounds_.w, dialogBounds_.y + hdrH,
             theme.borderSubtle, 0.8f, 1.5f);

    // Title & step badge
    drawText(r, "TRACKER CELL EDITOR", dialogBounds_.x + 18.0f, dialogBounds_.y + 11.0f, 12.0f, theme.primaryAccent);

    std::string stepStr = (step_ < 10 ? "0" : "") + std::to_string(step_);
    std::string posInfo = "STEP: " + stepStr + " (BEAT " + std::to_string(step_ / 4 + 1) + "." + std::to_string(step_ % 4 + 1) +
                          ") | LANE " + std::to_string(lane_ + 1);
    if (!trackName_.empty()) {
        posInfo += " [" + trackName_ + "]";
    }
    drawText(r, posInfo, dialogBounds_.x + 18.0f, dialogBounds_.y + 25.0f, 9.0f, theme.textMuted);

    // Close Button [X]
    drawRoundedRect(r, closeBtnBounds_.x, closeBtnBounds_.y, closeBtnBounds_.w, closeBtnBounds_.h, 3.0f, theme.controlBackground, 0.8f);
    drawCenteredText(r, "X", closeBtnBounds_, 9.5f, theme.textPrimary);

    // 3. Pitch Section (C0 - C8 Slider)
    std::string noteName = PianoKeyboard::getNoteName(pitch_);
    std::string pitchTitle = "NOTE PITCH: " + noteName + " (MIDI " + std::to_string(pitch_) + ")";
    drawText(r, pitchTitle, dialogBounds_.x + 20.0f, auditionBtnBounds_.y + 5.0f, 10.5f, theme.highlight);

    // Audition Badge Button
    drawRoundedRect(r, auditionBtnBounds_.x, auditionBtnBounds_.y, auditionBtnBounds_.w, auditionBtnBounds_.h, 3.0f,
                    theme.primaryAccent * 0.25f, 0.9f);
    drawRoundedRectOutline(r, auditionBtnBounds_.x, auditionBtnBounds_.y, auditionBtnBounds_.w, auditionBtnBounds_.h, 3.0f,
                           theme.primaryAccent, 1.0f, 1.2f);
    drawCenteredText(r, noteName + " ▶", auditionBtnBounds_, 9.5f, theme.primaryAccent);

    // [-] and [+] semitone buttons
    drawRoundedRect(r, pitchDownBtnBounds_.x, pitchDownBtnBounds_.y, pitchDownBtnBounds_.w, pitchDownBtnBounds_.h, 3.0f, theme.controlBackground);
    drawRoundedRectOutline(r, pitchDownBtnBounds_.x, pitchDownBtnBounds_.y, pitchDownBtnBounds_.w, pitchDownBtnBounds_.h, 3.0f, theme.borderSubtle, 1.0f);
    drawCenteredText(r, "-", pitchDownBtnBounds_, 11.0f, theme.textPrimary);

    drawRoundedRect(r, pitchUpBtnBounds_.x, pitchUpBtnBounds_.y, pitchUpBtnBounds_.w, pitchUpBtnBounds_.h, 3.0f, theme.controlBackground);
    drawRoundedRectOutline(r, pitchUpBtnBounds_.x, pitchUpBtnBounds_.y, pitchUpBtnBounds_.w, pitchUpBtnBounds_.h, 3.0f, theme.borderSubtle, 1.0f);
    drawCenteredText(r, "+", pitchUpBtnBounds_, 11.0f, theme.textPrimary);

    // Continuous Pitch Slider Track
    drawRoundedRect(r, pitchSliderBounds_.x, pitchSliderBounds_.y, pitchSliderBounds_.w, pitchSliderBounds_.h, 4.0f, theme.backgroundDark);
    drawRoundedRectOutline(r, pitchSliderBounds_.x, pitchSliderBounds_.y, pitchSliderBounds_.w, pitchSliderBounds_.h, 4.0f, theme.borderSubtle, 0.8f);

    // Octave Tick Marks C0..C8
    for (int oct = 0; oct <= 8; ++oct) {
        int octPitch = (oct + 1) * 12; // C0 = 12, C1 = 24 ... C8 = 108
        float norm = static_cast<float>(octPitch - kMinPitch) / static_cast<float>(kMaxPitch - kMinPitch);
        float tickX = pitchSliderBounds_.x + norm * pitchSliderBounds_.w;
        drawLine(r, tickX, pitchSliderBounds_.y + 2.0f, tickX, pitchSliderBounds_.y + pitchSliderBounds_.h - 2.0f,
                 (oct % 2 == 0) ? theme.primaryAccent : theme.textMuted, 0.5f, 1.0f);
        std::string octTag = "C" + std::to_string(oct);
        drawText(r, octTag, tickX - 6.0f, pitchSliderBounds_.y + pitchSliderBounds_.h + 2.0f, 7.5f, theme.textMuted, 0.7f);
    }

    // Active Pitch Thumb Fill
    float pitchNorm = std::clamp(static_cast<float>(pitch_ - kMinPitch) / static_cast<float>(kMaxPitch - kMinPitch), 0.0f, 1.0f);
    float thumbFillW = pitchSliderBounds_.w * pitchNorm;
    drawRoundedRect(r, pitchSliderBounds_.x, pitchSliderBounds_.y + 2.0f, thumbFillW, pitchSliderBounds_.h - 4.0f, 3.0f, theme.primaryAccent * 0.40f);
    drawCircle(r, pitchSliderBounds_.x + thumbFillW, pitchSliderBounds_.y + pitchSliderBounds_.h * 0.5f, 8.0f, theme.primaryAccent);
    drawCircle(r, pitchSliderBounds_.x + thumbFillW, pitchSliderBounds_.y + pitchSliderBounds_.h * 0.5f, 4.0f, Color(0.05f, 0.08f, 0.12f, 1.0f));

    // 4. Velocity Section
    float velPct = velocity_ * 100.0f;
    std::string velTitle = "VELOCITY: V" + std::to_string(static_cast<int>(velPct)) + " (" + std::to_string(static_cast<int>(velPct)) + "%)";
    drawText(r, velTitle, dialogBounds_.x + 20.0f, velSliderBounds_.y - 12.0f, 9.0f, theme.textSecondary);

    drawRoundedRect(r, velSliderBounds_.x, velSliderBounds_.y, velSliderBounds_.w, velSliderBounds_.h, 4.0f, theme.backgroundDark);
    float velFillW = velSliderBounds_.w * std::clamp(velocity_, 0.0f, 1.0f);
    drawRoundedRect(r, velSliderBounds_.x, velSliderBounds_.y, velFillW, velSliderBounds_.h, 4.0f, theme.secondaryAccent * 0.45f);
    drawCircle(r, velSliderBounds_.x + velFillW, velSliderBounds_.y + velSliderBounds_.h * 0.5f, 6.0f, theme.secondaryAccent);

    const char* presetLabels[] = {"25%", "50%", "75%", "100%"};
    for (size_t i = 0; i < velPresetBounds_.size() && i < 4; ++i) {
        const auto& pb = velPresetBounds_[i];
        drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 3.0f, theme.controlBackground, 0.85f);
        drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 3.0f, theme.borderSubtle, 1.0f);
        drawCenteredText(r, presetLabels[i], pb, 9.0f, theme.textPrimary);
    }

    // 5. Slide & FX Section
    drawRoundedRect(r, slideToggleBounds_.x, slideToggleBounds_.y, slideToggleBounds_.w, slideToggleBounds_.h, 3.0f,
                    isSlide_ ? (theme.highlight * 0.35f) : theme.controlBackground);
    drawRoundedRectOutline(r, slideToggleBounds_.x, slideToggleBounds_.y, slideToggleBounds_.w, slideToggleBounds_.h, 3.0f,
                           isSlide_ ? theme.highlight : theme.borderSubtle, 1.0f);
    drawCenteredText(r, isSlide_ ? "SLIDE: ON (S)" : "SLIDE: OFF (.)", slideToggleBounds_, 9.0f,
                     isSlide_ ? theme.highlight : theme.textMuted);

    for (size_t i = 0; i < fxPresetBounds_.size() && i < 5; ++i) {
        const auto& fb = fxPresetBounds_[i];
        bool isFxSel = (effectCommand_ == kFxPresets[i].code);
        drawRoundedRect(r, fb.x, fb.y, fb.w, fb.h, 3.0f,
                        isFxSel ? (theme.secondaryAccent * 0.35f) : theme.controlBackground);
        drawRoundedRectOutline(r, fb.x, fb.y, fb.w, fb.h, 3.0f,
                               isFxSel ? theme.secondaryAccent : theme.borderSubtle, 1.0f);
        drawCenteredText(r, kFxPresets[i].label, fb, 8.5f,
                         isFxSel ? theme.secondaryAccent : theme.textMuted);
    }

    // 6. Chip FX Parameter Controls
    if (!chipFxEntries_.empty()) {
        float chipHeadY = chipFxEntries_.front().sliderBounds.y - 12.0f;
        drawText(r, "CHIP FX PARAMETERS (00 - FF)", dialogBounds_.x + 20.0f, chipHeadY, 9.0f, theme.textSecondary);

        for (const auto& entry : chipFxEntries_) {
            // Label & Value
            std::string cLabel = entry.label + ": " + entry.value;
            drawText(r, cLabel, dialogBounds_.x + 20.0f, entry.sliderBounds.y + 6.0f, 9.5f, theme.primaryAccent);

            // [-] button
            drawRoundedRect(r, entry.downBtnBounds.x, entry.downBtnBounds.y, entry.downBtnBounds.w, entry.downBtnBounds.h, 3.0f, theme.controlBackground);
            drawRoundedRectOutline(r, entry.downBtnBounds.x, entry.downBtnBounds.y, entry.downBtnBounds.w, entry.downBtnBounds.h, 3.0f, theme.borderSubtle, 1.0f);
            drawCenteredText(r, "-", entry.downBtnBounds, 10.0f, theme.textPrimary);

            // Mini slider track
            drawRoundedRect(r, entry.sliderBounds.x, entry.sliderBounds.y + 5.0f, entry.sliderBounds.w, entry.sliderBounds.h - 10.0f, 3.0f, theme.backgroundDark);
            int intVal = hex2ToInt(entry.value);
            float cNorm = static_cast<float>(intVal) / 255.0f;
            float cFillW = entry.sliderBounds.w * cNorm;
            drawRoundedRect(r, entry.sliderBounds.x, entry.sliderBounds.y + 5.0f, cFillW, entry.sliderBounds.h - 10.0f, 3.0f, theme.primaryAccent * 0.40f);
            drawCircle(r, entry.sliderBounds.x + cFillW, entry.sliderBounds.y + entry.sliderBounds.h * 0.5f, 6.0f, theme.primaryAccent);

            // [+] button
            drawRoundedRect(r, entry.upBtnBounds.x, entry.upBtnBounds.y, entry.upBtnBounds.w, entry.upBtnBounds.h, 3.0f, theme.controlBackground);
            drawRoundedRectOutline(r, entry.upBtnBounds.x, entry.upBtnBounds.y, entry.upBtnBounds.w, entry.upBtnBounds.h, 3.0f, theme.borderSubtle, 1.0f);
            drawCenteredText(r, "+", entry.upBtnBounds, 10.0f, theme.textPrimary);
        }
    }

    // 7. Action Buttons
    drawRoundedRect(r, deleteBtnBounds_.x, deleteBtnBounds_.y, deleteBtnBounds_.w, deleteBtnBounds_.h, 4.0f,
                    Color(0.85f, 0.2f, 0.2f) * 0.25f, 0.9f);
    drawRoundedRectOutline(r, deleteBtnBounds_.x, deleteBtnBounds_.y, deleteBtnBounds_.w, deleteBtnBounds_.h, 4.0f,
                           Color(0.9f, 0.3f, 0.3f), 1.0f);
    drawCenteredText(r, "DELETE NOTE", deleteBtnBounds_, 9.0f, Color(1.0f, 0.4f, 0.4f));

    drawRoundedRect(r, cancelBtnBounds_.x, cancelBtnBounds_.y, cancelBtnBounds_.w, cancelBtnBounds_.h, 4.0f,
                    theme.controlBackground, 0.9f);
    drawRoundedRectOutline(r, cancelBtnBounds_.x, cancelBtnBounds_.y, cancelBtnBounds_.w, cancelBtnBounds_.h, 4.0f,
                           theme.borderSubtle, 1.0f);
    drawCenteredText(r, "CANCEL", cancelBtnBounds_, 9.0f, theme.textPrimary);

    drawRoundedRect(r, applyBtnBounds_.x, applyBtnBounds_.y, applyBtnBounds_.w, applyBtnBounds_.h, 4.0f,
                    theme.primaryAccent, 0.95f);
    drawCenteredText(r, "APPLY", applyBtnBounds_, 9.5f, Color(0.05f, 0.08f, 0.12f, 1.0f));
}

bool TrackerCellDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;

    if (ev.action == PointerAction::Down) {
        if (!dialogBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        if (closeBtnBounds_.contains(ev.x, ev.y) || cancelBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        if (applyBtnBounds_.contains(ev.x, ev.y)) {
            for (const auto& entry : chipFxEntries_) {
                customParams_[entry.key] = entry.value;
            }
            if (onApply) {
                onApply(step_, lane_, pitch_, velocity_, isSlide_, effectCommand_, customParams_);
            }
            close();
            return true;
        }

        if (deleteBtnBounds_.contains(ev.x, ev.y)) {
            if (onDelete) {
                onDelete(step_, lane_);
            }
            close();
            return true;
        }

        // Pitch Nudge [-] / [+]
        if (pitchDownBtnBounds_.contains(ev.x, ev.y)) {
            pitch_ = static_cast<uint8_t>(std::clamp(pitch_ - 1, kMinPitch, kMaxPitch));
            auditionCurrentPitch();
            return true;
        }
        if (pitchUpBtnBounds_.contains(ev.x, ev.y)) {
            pitch_ = static_cast<uint8_t>(std::clamp(pitch_ + 1, kMinPitch, kMaxPitch));
            auditionCurrentPitch();
            return true;
        }

        // Pitch Slider
        if (pitchSliderBounds_.contains(ev.x, ev.y)) {
            isDraggingPitch_ = true;
            float norm = std::clamp((ev.x - pitchSliderBounds_.x) / pitchSliderBounds_.w, 0.0f, 1.0f);
            pitch_ = static_cast<uint8_t>(std::clamp(kMinPitch + static_cast<int>(std::round(norm * (kMaxPitch - kMinPitch))), kMinPitch, kMaxPitch));
            auditionCurrentPitch();
            return true;
        }

        // Audition Button
        if (auditionBtnBounds_.contains(ev.x, ev.y)) {
            auditionCurrentPitch();
            return true;
        }

        // Velocity Slider
        if (velSliderBounds_.contains(ev.x, ev.y)) {
            isDraggingVelocity_ = true;
            float norm = std::clamp((ev.x - velSliderBounds_.x) / velSliderBounds_.w, 0.05f, 1.0f);
            velocity_ = norm;
            return true;
        }

        // Velocity Presets
        static const float kPresetValues[] = {0.25f, 0.50f, 0.75f, 1.00f};
        for (size_t i = 0; i < velPresetBounds_.size() && i < 4; ++i) {
            if (velPresetBounds_[i].contains(ev.x, ev.y)) {
                velocity_ = kPresetValues[i];
                return true;
            }
        }

        // Slide Toggle
        if (slideToggleBounds_.contains(ev.x, ev.y)) {
            isSlide_ = !isSlide_;
            return true;
        }

        // FX Presets
        for (size_t i = 0; i < fxPresetBounds_.size() && i < 5; ++i) {
            if (fxPresetBounds_[i].contains(ev.x, ev.y)) {
                effectCommand_ = kFxPresets[i].code;
                return true;
            }
        }

        // Chip FX controls
        for (size_t i = 0; i < chipFxEntries_.size(); ++i) {
            auto& entry = chipFxEntries_[i];
            if (entry.downBtnBounds.contains(ev.x, ev.y)) {
                int cur = hex2ToInt(entry.value);
                entry.value = intToHex2(cur - 1);
                customParams_[entry.key] = entry.value;
                return true;
            }
            if (entry.upBtnBounds.contains(ev.x, ev.y)) {
                int cur = hex2ToInt(entry.value);
                entry.value = intToHex2(cur + 1);
                customParams_[entry.key] = entry.value;
                return true;
            }
            if (entry.sliderBounds.contains(ev.x, ev.y)) {
                draggingChipFxIdx_ = static_cast<int>(i);
                float norm = std::clamp((ev.x - entry.sliderBounds.x) / entry.sliderBounds.w, 0.0f, 1.0f);
                entry.value = intToHex2(static_cast<int>(std::round(norm * 255.0f)));
                customParams_[entry.key] = entry.value;
                return true;
            }
        }

        return true;
    }

    if (ev.action == PointerAction::Move) {
        if (isDraggingPitch_) {
            float norm = std::clamp((ev.x - pitchSliderBounds_.x) / pitchSliderBounds_.w, 0.0f, 1.0f);
            uint8_t newPitch = static_cast<uint8_t>(std::clamp(kMinPitch + static_cast<int>(std::round(norm * (kMaxPitch - kMinPitch))), kMinPitch, kMaxPitch));
            if (newPitch != pitch_) {
                pitch_ = newPitch;
                auditionCurrentPitch();
            }
            return true;
        }
        if (isDraggingVelocity_) {
            float norm = std::clamp((ev.x - velSliderBounds_.x) / velSliderBounds_.w, 0.05f, 1.0f);
            velocity_ = norm;
            return true;
        }
        if (draggingChipFxIdx_ >= 0 && draggingChipFxIdx_ < static_cast<int>(chipFxEntries_.size())) {
            auto& entry = chipFxEntries_[draggingChipFxIdx_];
            float norm = std::clamp((ev.x - entry.sliderBounds.x) / entry.sliderBounds.w, 0.0f, 1.0f);
            entry.value = intToHex2(static_cast<int>(std::round(norm * 255.0f)));
            customParams_[entry.key] = entry.value;
            return true;
        }
    }

    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        isDraggingPitch_ = false;
        isDraggingVelocity_ = false;
        draggingChipFxIdx_ = -1;
    }

    return dialogBounds_.contains(ev.x, ev.y);
}

bool TrackerCellDialog::handleKey(int key, int /*scancode*/, int action, int /*mods*/) {
    if (!isOpen_) return false;
    if (action != 1) return true;

    if (key == 256) { // ESC -> close
        close();
        return true;
    }

    if (key == 257 || key == 335) { // Enter -> apply
        for (const auto& entry : chipFxEntries_) {
            customParams_[entry.key] = entry.value;
        }
        if (onApply) {
            onApply(step_, lane_, pitch_, velocity_, isSlide_, effectCommand_, customParams_);
        }
        close();
        return true;
    }

    if (key == 261 || key == 259) { // Delete / Backspace -> delete note
        if (onDelete) {
            onDelete(step_, lane_);
        }
        close();
        return true;
    }

    if (key == 263) { // Left arrow -> semitone down
        if (pitch_ > kMinPitch) {
            pitch_--;
            auditionCurrentPitch();
        }
        return true;
    }

    if (key == 262) { // Right arrow -> semitone up
        if (pitch_ < kMaxPitch) {
            pitch_++;
            auditionCurrentPitch();
        }
        return true;
    }

    if (key == 264) { // Down arrow -> octave down
        pitch_ = static_cast<uint8_t>(std::clamp(static_cast<int>(pitch_) - 12, kMinPitch, kMaxPitch));
        auditionCurrentPitch();
        return true;
    }

    if (key == 265) { // Up arrow -> octave up
        pitch_ = static_cast<uint8_t>(std::clamp(static_cast<int>(pitch_) + 12, kMinPitch, kMaxPitch));
        auditionCurrentPitch();
        return true;
    }

    return true;
}

} // namespace eatsbits::ui
