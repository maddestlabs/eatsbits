#include "eatsbits/ui/widgets/circle_of_fifths_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <cmath>
#include <algorithm>

namespace eatsbits::ui {

static constexpr float kPi = 3.14159265358979323846f;

CircleOfFifthsDialog::CircleOfFifthsDialog() {
    updateCurrentChord();
}

void CircleOfFifthsDialog::open(uint32_t targetBar,
                                int songKeyRoot,
                                bool isSongKeyMinor,
                                const std::optional<theory::ChordEvent>& initialChord) {
    targetBar_ = targetBar;
    songKeyRoot_ = (songKeyRoot % 12 + 12) % 12;
    isSongKeyMinor_ = isSongKeyMinor;
    isOpen_ = true;
    activeTab_ = CircleDialogTab::Wheel;
    presetsScrollY_ = 0.0f;

    if (initialChord.has_value()) {
        isEditingExisting_ = true;
        currentChord_ = *initialChord;
        selectedRoot_ = currentChord_.rootPitchClass;
        selectedQuality_ = currentChord_.quality;
        selectedBass_ = currentChord_.bassPitchClass;
        selectedBarLength_ = currentChord_.barLength;
    } else {
        isEditingExisting_ = false;
        selectedRoot_ = songKeyRoot_;
        selectedQuality_ = isSongKeyMinor_ ? theory::ChordQuality::Minor : theory::ChordQuality::Major;
        selectedBass_ = -1;
        selectedBarLength_ = 1.0f;
        updateCurrentChord();
    }
}

void CircleOfFifthsDialog::updateCurrentChord() {
    currentChord_.id = "chord_" + std::to_string(targetBar_) + "_" + std::to_string(selectedRoot_);
    currentChord_.startBar = targetBar_;
    currentChord_.barLength = selectedBarLength_;
    currentChord_.rootPitchClass = selectedRoot_;
    currentChord_.quality = selectedQuality_;
    currentChord_.bassPitchClass = selectedBass_;
}

void CircleOfFifthsDialog::layout(float screenW, float screenH) {
    screenWidth_ = screenW;
    screenHeight_ = screenH;

    float dw = std::min(700.0f, screenW - 32.0f);
    float dh = std::min(550.0f, screenH - 40.0f);
    float dx = (screenW - dw) * 0.5f;
    float dy = (screenH - dh) * 0.5f;

    dialogBounds_ = Rect2D{dx, dy, dw, dh};
    closeBtnBounds_ = Rect2D{dx + dw - 32.0f, dy + 10.0f, 22.0f, 22.0f};

    // Wheel layout on left side of modal
    wheelOuterRadius_ = std::min(115.0f, dh * 0.24f);
    wheelInnerRadius_ = wheelOuterRadius_ * 0.58f;
    wheelCenterX_ = dx + 155.0f;
    wheelCenterY_ = dy + 195.0f;

    auditionBtnBounds_ = Rect2D{wheelCenterX_ - 95.0f, wheelCenterY_ + wheelOuterRadius_ + 14.0f, 190.0f, 28.0f};

    // Bottom modal action buttons
    float btnY = dy + dh - 42.0f;
    applyBtnBounds_ = Rect2D{dx + dw - 180.0f, btnY, 166.0f, 30.0f};
    deleteBtnBounds_ = Rect2D{dx + dw - 310.0f, btnY, 120.0f, 30.0f};
    cancelBtnBounds_ = Rect2D{dx + 16.0f, btnY, 90.0f, 30.0f};
}

void CircleOfFifthsDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-translucent dark modal backdrop with soft scrim
    drawRect(r, 0.0f, 0.0f, screenWidth_, screenHeight_, 0.0f, 0.0f, 0.0f, 0.78f);

    // 2. Modal Body with rich glowing border
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f,
                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.85f, 1.5f);

    // 3. Modal Header Bar
    float hdrH = 42.0f;
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, hdrH, 8.0f,
                    theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.98f);
    drawLine(r, dialogBounds_.x, dialogBounds_.y + hdrH, dialogBounds_.x + dialogBounds_.w, dialogBounds_.y + hdrH,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    // Title Icon & Label
    drawCircle(r, dialogBounds_.x + 22.0f, dialogBounds_.y + 21.0f, 6.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    drawCircle(r, dialogBounds_.x + 22.0f, dialogBounds_.y + 21.0f, 3.5f, 0.05f, 0.05f, 0.07f, 1.0f);
    drawText(r, "CHORDS & HARMONY", dialogBounds_.x + 36.0f, dialogBounds_.y + 14.0f, 12.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    // Key Badge & Bar Badge
    std::string keyStr = "KEY: " + std::string(theory::ChordTheory::pitchClassNames[songKeyRoot_]) +
                         (isSongKeyMinor_ ? " MINOR" : " MAJOR");
    float keyBadgeW = 100.0f;
    float keyBadgeX = dialogBounds_.x + dialogBounds_.w - 240.0f;
    drawRoundedRect(r, keyBadgeX, dialogBounds_.y + 10.0f, keyBadgeW, 22.0f, 4.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
    drawRoundedRectOutline(r, keyBadgeX, dialogBounds_.y + 10.0f, keyBadgeW, 22.0f, 4.0f,
                           theme.highlight.r * 0.7f, theme.highlight.g * 0.7f, theme.highlight.b * 0.7f, 0.9f, 1.0f);
    drawText(r, keyStr, keyBadgeX + 8.0f, dialogBounds_.y + 15.0f, 9.5f,
             theme.highlight.r, theme.highlight.g, theme.highlight.b, 1.0f);

    std::string barStr = "BAR " + std::to_string(targetBar_ + 1);
    float barBadgeX = keyBadgeX + keyBadgeW + 8.0f;
    drawRoundedRect(r, barBadgeX, dialogBounds_.y + 10.0f, 56.0f, 22.0f, 4.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
    drawText(r, barStr, barBadgeX + 8.0f, dialogBounds_.y + 15.0f, 9.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // Close button (metallic screw icon)
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(lastMouseX_, lastMouseY_) ||
                    (std::hypot(lastMouseX_ - clCenterX, lastMouseY_ - clCenterY) <= 13.0f);
    drawScrewCloseButton(r, clCenterX, clCenterY, 9.0f, closeHov, theme.primaryAccent);

    // 4. Modal Navigation Tabs
    float tabY = dialogBounds_.y + hdrH + 8.0f;
    float tabW = 150.0f;
    float tabH = 26.0f;

    auto drawTab = [&](CircleDialogTab tab, const char* label, float x) {
        bool isSel = (activeTab_ == tab);
        drawRoundedRect(r, x, tabY, tabW, tabH, 4.0f,
                        isSel ? theme.primaryAccent.r * 0.25f : theme.controlBackground.r,
                        isSel ? theme.primaryAccent.g * 0.25f : theme.controlBackground.g,
                        isSel ? theme.primaryAccent.b * 0.25f : theme.controlBackground.b, 0.9f);
        if (isSel) {
            drawRoundedRectOutline(r, x, tabY, tabW, tabH, 4.0f,
                                   theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f, 1.2f);
        }
        drawText(r, label, x + 16.0f, tabY + 7.0f, 9.5f,
                 isSel ? theme.primaryAccent.r : theme.textMuted.r,
                 isSel ? theme.primaryAccent.g : theme.textMuted.g,
                 isSel ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);
    };

    drawTab(CircleDialogTab::Wheel, "CIRCLE OF FIFTHS", dialogBounds_.x + 16.0f);
    drawTab(CircleDialogTab::Presets, "PROGRESSION PRESETS", dialogBounds_.x + 16.0f + tabW + 8.0f);
    drawTab(CircleDialogTab::MidiExtract, "EXTRACT FROM MIDI", dialogBounds_.x + 16.0f + (tabW + 8.0f) * 2.0f);

    // 5. Active Tab Content
    if (activeTab_ == CircleDialogTab::Wheel) {
        renderWheelTab(r, theme);
    } else if (activeTab_ == CircleDialogTab::Presets) {
        renderPresetsTab(r, theme);
    } else {
        renderExtractTab(r, theme);
    }

    // 6. Bottom Action Buttons Bar
    drawLine(r, dialogBounds_.x, dialogBounds_.y + dialogBounds_.h - 52.0f,
             dialogBounds_.x + dialogBounds_.w, dialogBounds_.y + dialogBounds_.h - 52.0f,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);

    // Cancel Button
    drawButton(r, cancelBtnBounds_, "CLOSE",
               theme.controlBackground, theme.borderSubtle, theme.textPrimary, 10.0f, 4.0f, 1.0f);

    // Delete Button (only if editing an existing chord)
    if (isEditingExisting_) {
        drawButton(r, deleteBtnBounds_, "DELETE CHORD",
                   Color{0.25f, 0.08f, 0.08f, 0.95f}, Color{0.85f, 0.20f, 0.20f, 1.0f}, Color{1.0f, 0.40f, 0.40f, 1.0f}, 10.0f, 4.0f, 1.2f);
    }

    // Apply / Insert Button
    std::string applyLabel = isEditingExisting_ ? "UPDATE CHORD" : "INSERT CHORD";
    drawButton(r, applyBtnBounds_, applyLabel.c_str(),
               Color{0.06f, 0.22f, 0.20f, 0.95f}, theme.primaryAccent, theme.primaryAccent, 10.5f, 4.0f, 1.5f);
}

void CircleOfFifthsDialog::renderWheelTab(BatchRenderer2D& r, const ThemeTokens& theme) {
    // -------------------------------------------------------------
    // LEFT SIDE: Circle of Fifths Wheel
    // -------------------------------------------------------------
    float cx = wheelCenterX_;
    float cy = wheelCenterY_;
    float R_out = wheelOuterRadius_;
    float R_mid = wheelInnerRadius_;
    float R_in = 20.0f;

    // Center circular hub
    drawCircle(r, cx, cy, R_out + 3.0f, 0.06f, 0.07f, 0.09f, 0.9f);
    drawCircleOutline(r, cx, cy, R_out + 2.0f, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.5f);

    // Draw 12 sectors
    const float stepAngle = (2.0f * kPi) / 12.0f;

    for (int s = 0; s < 12; ++s) {
        float startA = -kPi * 0.5f + static_cast<float>(s) * stepAngle - (stepAngle * 0.5f);
        float midA = -kPi * 0.5f + static_cast<float>(s) * stepAngle;
        float endA = startA + stepAngle;

        int majRoot = theory::ChordTheory::circleOfFifthsMajor[s];
        int minRoot = theory::ChordTheory::circleOfFifthsMinor[s];

        bool isMajSelected = (selectedRoot_ == majRoot &&
            (selectedQuality_ != theory::ChordQuality::Minor &&
             selectedQuality_ != theory::ChordQuality::Minor7 &&
             selectedQuality_ != theory::ChordQuality::Min9));

        bool isMinSelected = (selectedRoot_ == minRoot &&
            (selectedQuality_ == theory::ChordQuality::Minor ||
             selectedQuality_ == theory::ChordQuality::Minor7 ||
             selectedQuality_ == theory::ChordQuality::Min9));

        bool isSongKey = (majRoot == songKeyRoot_ && !isSongKeyMinor_) ||
                         (minRoot == songKeyRoot_ && isSongKeyMinor_);

        // Outer Major Ring Wedge Sector
        float rMajor = isMajSelected ? theme.primaryAccent.r : 0.12f;
        float gMajor = isMajSelected ? theme.primaryAccent.g : 0.14f;
        float bMajor = isMajSelected ? theme.primaryAccent.b : 0.18f;
        float aMajor = isMajSelected ? 0.90f : 0.70f;

        // Draw outer wedge approximation (using 4 segments)
        float rMidRadius = (R_out + R_mid) * 0.5f;
        float labelX = cx + std::cos(midA) * rMidRadius;
        float labelY = cy + std::sin(midA) * rMidRadius;

        // Draw sector boundary lines
        drawLine(r, cx + std::cos(startA) * R_mid, cy + std::sin(startA) * R_mid,
                 cx + std::cos(startA) * R_out, cy + std::sin(startA) * R_out,
                 theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.45f, 1.0f);

        // Highlight active major sector dot/pill
        if (isMajSelected) {
            drawCircle(r, labelX, labelY, 13.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.35f);
            drawCircleOutline(r, labelX, labelY, 13.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f, 1.5f);
        } else if (majRoot == songKeyRoot_ && !isSongKeyMinor_) {
            drawCircleOutline(r, labelX, labelY, 12.0f, theme.highlight.r, theme.highlight.g, theme.highlight.b, 0.9f, 1.2f);
        }

        const char* majLbl = theory::ChordTheory::circleMajorLabels[s];
        drawText(r, majLbl, labelX - (std::strlen(majLbl) > 1 ? 7.0f : 4.0f), labelY - 5.0f, 10.5f,
                 isMajSelected ? 1.0f : theme.textPrimary.r,
                 isMajSelected ? 1.0f : theme.textPrimary.g,
                 isMajSelected ? 1.0f : theme.textPrimary.b, 1.0f);

        // Inner Minor Ring Wedge Sector
        float rInnerRadius = (R_mid + R_in) * 0.5f;
        float minLabelX = cx + std::cos(midA) * rInnerRadius;
        float minLabelY = cy + std::sin(midA) * rInnerRadius;

        drawLine(r, cx + std::cos(startA) * R_in, cy + std::sin(startA) * R_in,
                 cx + std::cos(startA) * R_mid, cy + std::sin(startA) * R_mid,
                 theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.35f, 1.0f);

        if (isMinSelected) {
            drawCircle(r, minLabelX, minLabelY, 11.0f, theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.35f);
            drawCircleOutline(r, minLabelX, minLabelY, 11.0f, theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f, 1.5f);
        } else if (minRoot == songKeyRoot_ && isSongKeyMinor_) {
            drawCircleOutline(r, minLabelX, minLabelY, 10.0f, theme.highlight.r, theme.highlight.g, theme.highlight.b, 0.9f, 1.2f);
        }

        const char* minLbl = theory::ChordTheory::circleMinorLabels[s];
        drawText(r, minLbl, minLabelX - (std::strlen(minLbl) > 2 ? 8.0f : 6.0f), minLabelY - 4.5f, 8.5f,
                 isMinSelected ? 1.0f : theme.textMuted.r,
                 isMinSelected ? 1.0f : theme.textMuted.g,
                 isMinSelected ? 1.0f : theme.textMuted.b, 1.0f);
    }

    // Concentric ring dividing lines
    drawCircleOutline(r, cx, cy, R_mid, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
    drawCircleOutline(r, cx, cy, R_in, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // Center Hub Label: Song Key Root
    drawCircle(r, cx, cy, R_in, 0.08f, 0.09f, 0.12f, 1.0f);
    std::string keyShort = std::string(theory::ChordTheory::pitchClassNames[songKeyRoot_]) +
                           (isSongKeyMinor_ ? "m" : "");
    drawText(r, keyShort, cx - (keyShort.length() > 2 ? 8.0f : 5.0f), cy - 4.5f, 9.0f,
             theme.highlight.r, theme.highlight.g, theme.highlight.b, 1.0f);

    // Audition Button below wheel
    std::string audStr = "AUDITION (" + currentChord_.getDisplayName() + ")";
    drawButton(r, auditionBtnBounds_, audStr.c_str(),
               theme.controlBackground, theme.primaryAccent, theme.primaryAccent, 10.0f, 5.0f, 1.0f);

    // -------------------------------------------------------------
    // RIGHT SIDE: Chord Hero Card & Modifiers Matrix
    // -------------------------------------------------------------
    float rightX = dialogBounds_.x + 320.0f;
    float rightW = dialogBounds_.w - 336.0f;
    float curY = dialogBounds_.y + 82.0f;

    // 1. Hero Card: Displays Selected Chord Name in large Oswald Font & Roman Numeral Degree Badge
    float heroH = 58.0f;
    drawRoundedRect(r, rightX, curY, rightW, heroH, 6.0f,
                    0.08f, 0.10f, 0.14f, 0.95f);
    drawRoundedRectOutline(r, rightX, curY, rightW, heroH, 6.0f,
                           theme.primaryAccent.r * 0.6f, theme.primaryAccent.g * 0.6f, theme.primaryAccent.b * 0.6f, 0.8f, 1.2f);

    drawText(r, "SELECTED CHORD", rightX + 14.0f, curY + 8.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    std::string chordName = currentChord_.getDisplayName();
    drawText(r, chordName, rightX + 14.0f, curY + 22.0f, 22.0f,
             1.0f, 1.0f, 1.0f, 1.0f);

    // Roman Numeral Degree Badge
    std::string roman = theory::ChordTheory::getRomanNumeral(
        songKeyRoot_, isSongKeyMinor_, currentChord_.rootPitchClass, currentChord_.quality);

    float badgeW = 56.0f;
    float badgeH = 38.0f;
    float badgeX = rightX + rightW - badgeW - 12.0f;
    float badgeY = curY + 10.0f;
    drawRoundedRect(r, badgeX, badgeY, badgeW, badgeH, 4.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
    drawRoundedRectOutline(r, badgeX, badgeY, badgeW, badgeH, 4.0f,
                           theme.highlight.r, theme.highlight.g, theme.highlight.b, 0.85f, 1.2f);
    drawText(r, "DEGREE", badgeX + 11.0f, badgeY + 4.0f, 8.0f,
             theme.highlight.r, theme.highlight.g, theme.highlight.b, 1.0f);
    drawText(r, roman, badgeX + (badgeW * 0.5f) - (roman.length() * 3.5f), badgeY + 16.0f, 14.0f,
             1.0f, 1.0f, 1.0f, 1.0f);

    curY += heroH + 12.0f;

    // 2. Chord Quality & Extensions Matrix (14 Chips)
    drawText(r, "CHORD QUALITY & EXTENSIONS", rightX, curY, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
    curY += 14.0f;

    static const theory::ChordQuality kAllQualities[] = {
        theory::ChordQuality::Major,
        theory::ChordQuality::Minor,
        theory::ChordQuality::Dominant7,
        theory::ChordQuality::Major7,
        theory::ChordQuality::Minor7,
        theory::ChordQuality::Diminished,
        theory::ChordQuality::Augmented,
        theory::ChordQuality::HalfDiminished7,
        theory::ChordQuality::Sus2,
        theory::ChordQuality::Sus4,
        theory::ChordQuality::Add9,
        theory::ChordQuality::Min9,
        theory::ChordQuality::Maj9,
        theory::ChordQuality::Dom9
    };

    float chipX = rightX;
    float chipY = curY;
    for (int q = 0; q < 14; ++q) {
        theory::ChordQuality quality = kAllQualities[q];
        const char* qName = theory::getChordQualityDisplayName(quality);
        float qw = static_cast<float>(std::strlen(qName)) * 7.5f + 18.0f;

        if (chipX + qw > rightX + rightW) {
            chipX = rightX;
            chipY += 26.0f;
        }

        bool isSel = (selectedQuality_ == quality);
        drawRoundedRect(r, chipX, chipY, qw, 22.0f, 4.0f,
                        isSel ? theme.primaryAccent.r : theme.controlBackground.r,
                        isSel ? theme.primaryAccent.g : theme.controlBackground.g,
                        isSel ? theme.primaryAccent.b : theme.controlBackground.b, isSel ? 0.95f : 0.75f);
        if (!isSel) {
            drawRoundedRectOutline(r, chipX, chipY, qw, 22.0f, 4.0f,
                                   theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
        }
        drawText(r, qName, chipX + 8.0f, chipY + 5.0f, 9.5f,
                 isSel ? 0.05f : theme.textPrimary.r,
                 isSel ? 0.05f : theme.textPrimary.g,
                 isSel ? 0.05f : theme.textPrimary.b, 1.0f);

        chipX += qw + 6.0f;
    }

    curY = chipY + 34.0f;

    // 3. Bass / Slash Note Selector
    drawText(r, "BASS / SLASH NOTE (INVERSION)", rightX, curY, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
    curY += 14.0f;

    // "Root Bass" chip
    bool isRootBass = (selectedBass_ < 0);
    drawRoundedRect(r, rightX, curY, 74.0f, 22.0f, 4.0f,
                    isRootBass ? theme.highlight.r : theme.controlBackground.r,
                    isRootBass ? theme.highlight.g : theme.controlBackground.g,
                    isRootBass ? theme.highlight.b : theme.controlBackground.b, isRootBass ? 0.95f : 0.75f);
    drawText(r, "Root Bass", rightX + 8.0f, curY + 5.0f, 9.5f,
             isRootBass ? 0.05f : theme.textPrimary.r,
             isRootBass ? 0.05f : theme.textPrimary.g,
             isRootBass ? 0.05f : theme.textPrimary.b, 1.0f);

    // 12 Pitch Classes for custom slash bass
    float bX = rightX + 80.0f;
    for (int pc = 0; pc < 12; ++pc) {
        if (bX + 22.0f > rightX + rightW) {
            bX = rightX + 80.0f;
            curY += 24.0f;
        }
        bool isThisBass = (selectedBass_ == pc);
        drawRoundedRect(r, bX, curY, 20.0f, 20.0f, 3.0f,
                        isThisBass ? theme.primaryAccent.r : theme.controlBackground.r,
                        isThisBass ? theme.primaryAccent.g : theme.controlBackground.g,
                        isThisBass ? theme.primaryAccent.b : theme.controlBackground.b, isThisBass ? 0.95f : 0.6f);
        const char* pcName = theory::ChordTheory::pitchClassNames[pc];
        drawText(r, pcName, bX + (std::strlen(pcName) > 1 ? 2.5f : 5.0f), curY + 4.5f, 8.5f,
                 isThisBass ? 0.05f : theme.textMuted.r,
                 isThisBass ? 0.05f : theme.textMuted.g,
                 isThisBass ? 0.05f : theme.textMuted.b, 1.0f);
        bX += 24.0f;
    }

    curY += 32.0f;

    // 4. Bar Length Selector
    drawText(r, "DURATION", rightX, curY, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
    curY += 14.0f;

    static const float kLengths[] = {0.5f, 1.0f, 2.0f, 4.0f};
    static const char* kLengthLabels[] = {"0.5 Bar", "1 Bar", "2 Bars", "4 Bars"};
    float durX = rightX;
    for (int d = 0; d < 4; ++d) {
        bool isSel = (std::abs(selectedBarLength_ - kLengths[d]) < 0.05f);
        drawRoundedRect(r, durX, curY, 62.0f, 22.0f, 4.0f,
                        isSel ? theme.primaryAccent.r * 0.35f : theme.controlBackground.r,
                        isSel ? theme.primaryAccent.g * 0.35f : theme.controlBackground.g,
                        isSel ? theme.primaryAccent.b * 0.35f : theme.controlBackground.b, 0.9f);
        if (isSel) {
            drawRoundedRectOutline(r, durX, curY, 62.0f, 22.0f, 4.0f,
                                   theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f, 1.0f);
        }
        drawText(r, kLengthLabels[d], durX + 8.0f, curY + 5.0f, 9.5f,
                 isSel ? theme.primaryAccent.r : theme.textMuted.r,
                 isSel ? theme.primaryAccent.g : theme.textMuted.g,
                 isSel ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);
        durX += 68.0f;
    }
}

void CircleOfFifthsDialog::renderPresetsTab(BatchRenderer2D& r, const ThemeTokens& theme) {
    const auto& presets = theory::ChordTheory::getProgressionPresets();

    float startX = dialogBounds_.x + 16.0f;
    float startY = dialogBounds_.y + 82.0f;
    float cardW = dialogBounds_.w - 32.0f;
    float cardH = 50.0f;

    float curY = startY - presetsScrollY_;

    for (size_t i = 0; i < presets.size(); ++i) {
        if (curY + cardH < startY || curY > dialogBounds_.y + dialogBounds_.h - 60.0f) {
            curY += cardH + 8.0f;
            continue;
        }

        const auto& pr = presets[i];
        bool isSelected = (selectedPresetIdx_ == static_cast<int>(i));

        // Preset Card Body
        drawRoundedRect(r, startX, curY, cardW, cardH, 5.0f,
                        isSelected ? 0.12f : theme.controlBackground.r,
                        isSelected ? 0.15f : theme.controlBackground.g,
                        isSelected ? 0.20f : theme.controlBackground.b, 0.95f);
        drawRoundedRectOutline(r, startX, curY, cardW, cardH, 5.0f,
                               isSelected ? theme.highlight.r : theme.borderSubtle.r,
                               isSelected ? theme.highlight.g : theme.borderSubtle.g,
                               isSelected ? theme.highlight.b : theme.borderSubtle.b, isSelected ? 0.95f : 0.6f, isSelected ? 1.5f : 1.0f);

        // Genre Pill
        drawRoundedRect(r, startX + 10.0f, curY + 8.0f, 70.0f, 16.0f, 3.0f,
                        theme.primaryAccent.r * 0.2f, theme.primaryAccent.g * 0.2f, theme.primaryAccent.b * 0.2f, 0.9f);
        drawText(r, pr.genre, startX + 14.0f, curY + 11.0f, 8.5f,
                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

        // Preset Name
        drawText(r, pr.name, startX + 88.0f, curY + 8.0f, 11.5f,
                 theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

        // Roman Summary Badge
        std::string romanSum = pr.getRomanSummary(songKeyRoot_, isSongKeyMinor_);
        drawText(r, romanSum, startX + 88.0f, curY + 26.0f, 10.0f,
                 theme.highlight.r, theme.highlight.g, theme.highlight.b, 1.0f);

        // Apply button inside card
        float applyW = 90.0f;
        float applyX = startX + cardW - applyW - 10.0f;
        drawButton(r, Rect2D{applyX, curY + 12.0f, applyW, 26.0f}, "APPLY",
                   Color{0.10f, 0.22f, 0.20f, 0.95f}, theme.primaryAccent, theme.primaryAccent, 9.5f, 3.0f, 1.0f);

        curY += cardH + 8.0f;
    }
}

void CircleOfFifthsDialog::renderExtractTab(BatchRenderer2D& r, const ThemeTokens& theme) {
    float startX = dialogBounds_.x + 24.0f;
    float curY = dialogBounds_.y + 90.0f;
    float maxW = dialogBounds_.w - 48.0f;

    drawText(r, "AUTOMATIC MIDI HARMONY EXTRACTION", startX, curY, 13.0f,
             theme.highlight.r, theme.highlight.g, theme.highlight.b, 1.0f);
    curY += 20.0f;

    drawText(r, "Analyze existing MIDI clips or tracks to automatically detect root notes, chord qualities, and inversions,", startX, curY, 10.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.95f);
    curY += 15.0f;
    drawText(r, "generating a synchronized sequence of ChordEvents directly onto the Chord Track timeline.", startX, curY, 10.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.95f);
    curY += 35.0f;

    // 1. Extract from Active Track Card
    drawRoundedRect(r, startX, curY, maxW, 60.0f, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, startX, curY, maxW, 60.0f, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    drawText(r, "EXTRACT FROM ACTIVE TRACK", startX + 16.0f, curY + 12.0f, 11.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "Analyzes all clips across the entire active track timeline to build chord events.", startX + 16.0f, curY + 30.0f, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);

    drawButton(r, Rect2D{startX + maxW - 130.0f, curY + 15.0f, 115.0f, 30.0f}, "EXTRACT TRACK",
               Color{0.10f, 0.22f, 0.20f, 0.95f}, theme.primaryAccent, theme.primaryAccent, 10.0f, 4.0f, 1.0f);

    curY += 76.0f;

    // 2. Extract from Active Clip Card
    drawRoundedRect(r, startX, curY, maxW, 60.0f, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, startX, curY, maxW, 60.0f, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    drawText(r, "EXTRACT FROM SELECTED CLIP", startX + 16.0f, curY + 12.0f, 11.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "Converts note events inside the currently selected clip into chord blocks.", startX + 16.0f, curY + 30.0f, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);

    drawButton(r, Rect2D{startX + maxW - 130.0f, curY + 15.0f, 115.0f, 30.0f}, "EXTRACT CLIP",
               Color{0.15f, 0.18f, 0.24f, 0.95f}, theme.secondaryAccent, theme.secondaryAccent, 10.0f, 4.0f, 1.0f);
}

void CircleOfFifthsDialog::handleWheelClick(float localX, float localY) {
    float dx = localX - wheelCenterX_;
    float dy = localY - wheelCenterY_;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist > wheelOuterRadius_ || dist < 20.0f) return;

    // Angle theta from top (-pi/2) clockwise
    float angle = std::atan2(dy, dx);
    float normAngle = angle + (kPi * 0.5f);
    if (normAngle < 0.0f) normAngle += (2.0f * kPi);

    float stepAngle = (2.0f * kPi) / 12.0f;
    int sector = static_cast<int>(std::floor((normAngle + (stepAngle * 0.5f)) / stepAngle)) % 12;

    if (dist >= wheelInnerRadius_) {
        // Outer Major Ring tapped
        selectedRoot_ = theory::ChordTheory::circleOfFifthsMajor[sector];
        if (selectedQuality_ == theory::ChordQuality::Minor ||
            selectedQuality_ == theory::ChordQuality::Minor7 ||
            selectedQuality_ == theory::ChordQuality::Min9) {
            selectedQuality_ = theory::ChordQuality::Major;
        }
    } else {
        // Inner Minor Ring tapped
        selectedRoot_ = theory::ChordTheory::circleOfFifthsMinor[sector];
        if (selectedQuality_ == theory::ChordQuality::Major ||
            selectedQuality_ == theory::ChordQuality::Major7 ||
            selectedQuality_ == theory::ChordQuality::Maj9) {
            selectedQuality_ = theory::ChordQuality::Minor;
        }
    }

    selectedBass_ = -1; // Reset slash bass to default root
    updateCurrentChord();

    // Instant audition
    if (onAuditionChord) onAuditionChord(currentChord_);
}

bool CircleOfFifthsDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;
    lastMouseX_ = ev.x;
    lastMouseY_ = ev.y;

    // Block events from penetrating underlying DAW views
    if (ev.action == PointerAction::Scroll) {
        if (activeTab_ == CircleDialogTab::Presets) {
            presetsScrollY_ = std::max(0.0f, presetsScrollY_ - ev.scrollY * 25.0f);
        }
        return true;
    }

    if (ev.action == PointerAction::Down) {
        // Close button
        if (closeBtnBounds_.contains(ev.x, ev.y) || cancelBtnBounds_.contains(ev.x, ev.y)) {
            close();
            if (onClose) onClose();
            return true;
        }

        // Tab switcher
        float tabY = dialogBounds_.y + 42.0f + 8.0f;
        float tabW = 150.0f;
        float tabH = 26.0f;

        if (ev.y >= tabY && ev.y <= tabY + tabH) {
            if (ev.x >= dialogBounds_.x + 16.0f && ev.x <= dialogBounds_.x + 16.0f + tabW) {
                activeTab_ = CircleDialogTab::Wheel;
                return true;
            } else if (ev.x >= dialogBounds_.x + 16.0f + tabW + 8.0f && ev.x <= dialogBounds_.x + 16.0f + (tabW * 2.0f) + 8.0f) {
                activeTab_ = CircleDialogTab::Presets;
                return true;
            } else if (ev.x >= dialogBounds_.x + 16.0f + (tabW + 8.0f) * 2.0f && ev.x <= dialogBounds_.x + 16.0f + (tabW + 8.0f) * 3.0f) {
                activeTab_ = CircleDialogTab::MidiExtract;
                return true;
            }
        }

        // Apply Button
        if (applyBtnBounds_.contains(ev.x, ev.y)) {
            updateCurrentChord();
            if (onChordApplied) onChordApplied(currentChord_);
            close();
            return true;
        }

        // Delete Button
        if (isEditingExisting_ && deleteBtnBounds_.contains(ev.x, ev.y)) {
            if (onChordDeleted) onChordDeleted(currentChord_.id);
            close();
            return true;
        }

        // Handle Wheel Tab Interactions
        if (activeTab_ == CircleDialogTab::Wheel) {
            // Audition button
            if (auditionBtnBounds_.contains(ev.x, ev.y)) {
                updateCurrentChord();
                if (onAuditionChord) onAuditionChord(currentChord_);
                return true;
            }

            // Wheel click
            float wheelDist = std::hypot(ev.x - wheelCenterX_, ev.y - wheelCenterY_);
            if (wheelDist <= wheelOuterRadius_ && wheelDist >= 20.0f) {
                handleWheelClick(ev.x, ev.y);
                return true;
            }

            // Right side modifiers: Quality Chips, Bass notes, Durations
            float rightX = dialogBounds_.x + 320.0f;
            float rightW = dialogBounds_.w - 336.0f;

            // 1. Quality chips hit test
            float chipX = rightX;
            float chipY = dialogBounds_.y + 82.0f + 58.0f + 12.0f + 14.0f;
            static const theory::ChordQuality kAllQualities[] = {
                theory::ChordQuality::Major, theory::ChordQuality::Minor, theory::ChordQuality::Dominant7,
                theory::ChordQuality::Major7, theory::ChordQuality::Minor7, theory::ChordQuality::Diminished,
                theory::ChordQuality::Augmented, theory::ChordQuality::HalfDiminished7, theory::ChordQuality::Sus2,
                theory::ChordQuality::Sus4, theory::ChordQuality::Add9, theory::ChordQuality::Min9,
                theory::ChordQuality::Maj9, theory::ChordQuality::Dom9
            };

            for (int q = 0; q < 14; ++q) {
                const char* qName = theory::getChordQualityDisplayName(kAllQualities[q]);
                float qw = static_cast<float>(std::strlen(qName)) * 7.5f + 18.0f;
                if (chipX + qw > rightX + rightW) {
                    chipX = rightX;
                    chipY += 26.0f;
                }
                if (ev.x >= chipX && ev.x <= chipX + qw && ev.y >= chipY && ev.y <= chipY + 22.0f) {
                    selectedQuality_ = kAllQualities[q];
                    updateCurrentChord();
                    if (onAuditionChord) onAuditionChord(currentChord_);
                    return true;
                }
                chipX += qw + 6.0f;
            }

            // 2. Bass chips hit test
            float bassY = chipY + 34.0f + 14.0f;
            if (ev.x >= rightX && ev.x <= rightX + 74.0f && ev.y >= bassY && ev.y <= bassY + 22.0f) {
                selectedBass_ = -1; // Root bass
                updateCurrentChord();
                if (onAuditionChord) onAuditionChord(currentChord_);
                return true;
            }

            float bX = rightX + 80.0f;
            float curBY = bassY;
            for (int pc = 0; pc < 12; ++pc) {
                if (bX + 22.0f > rightX + rightW) {
                    bX = rightX + 80.0f;
                    curBY += 24.0f;
                }
                if (ev.x >= bX && ev.x <= bX + 20.0f && ev.y >= curBY && ev.y <= curBY + 20.0f) {
                    selectedBass_ = pc;
                    updateCurrentChord();
                    if (onAuditionChord) onAuditionChord(currentChord_);
                    return true;
                }
                bX += 24.0f;
            }

            // 3. Duration chips
            float durY = curBY + 32.0f + 14.0f;
            static const float kLengths[] = {0.5f, 1.0f, 2.0f, 4.0f};
            float dX = rightX;
            for (int d = 0; d < 4; ++d) {
                if (ev.x >= dX && ev.x <= dX + 62.0f && ev.y >= durY && ev.y <= durY + 22.0f) {
                    selectedBarLength_ = kLengths[d];
                    updateCurrentChord();
                    return true;
                }
                dX += 68.0f;
            }
        } else if (activeTab_ == CircleDialogTab::Presets) {
            // Presets tab hit test: Apply button on each card
            const auto& presets = theory::ChordTheory::getProgressionPresets();
            float startX = dialogBounds_.x + 16.0f;
            float startY = dialogBounds_.y + 82.0f;
            float cardW = dialogBounds_.w - 32.0f;
            float cardH = 50.0f;
            float curY = startY - presetsScrollY_;

            for (size_t i = 0; i < presets.size(); ++i) {
                if (curY + cardH >= startY && curY <= dialogBounds_.y + dialogBounds_.h - 60.0f) {
                    float applyW = 90.0f;
                    float applyX = startX + cardW - applyW - 10.0f;
                    Rect2D applyRect{applyX, curY + 12.0f, applyW, 26.0f};

                    if (applyRect.contains(ev.x, ev.y)) {
                        if (onProgressionApplied) {
                            onProgressionApplied(presets[i], targetBar_);
                        }
                        close();
                        return true;
                    }

                    if (Rect2D{startX, curY, cardW, cardH}.contains(ev.x, ev.y)) {
                        selectedPresetIdx_ = static_cast<int>(i);
                        return true;
                    }
                }
                curY += cardH + 8.0f;
            }
        } else if (activeTab_ == CircleDialogTab::MidiExtract) {
            float startX = dialogBounds_.x + 24.0f;
            float curY = dialogBounds_.y + 90.0f + 20.0f + 15.0f + 10.0f + 35.0f;
            float maxW = dialogBounds_.w - 48.0f;

            Rect2D extractTrackBtn{startX + maxW - 130.0f, curY + 15.0f, 115.0f, 30.0f};
            if (extractTrackBtn.contains(ev.x, ev.y)) {
                if (onExtractFromActiveTrack) onExtractFromActiveTrack();
                close();
                return true;
            }

            curY += 76.0f;
            Rect2D extractClipBtn{startX + maxW - 130.0f, curY + 15.0f, 115.0f, 30.0f};
            if (extractClipBtn.contains(ev.x, ev.y)) {
                if (onExtractFromActiveClip) onExtractFromActiveClip();
                close();
                return true;
            }
        }

        // Tap outside dialog closes modal
        if (!dialogBounds_.contains(ev.x, ev.y)) {
            close();
            if (onClose) onClose();
            return true;
        }

        return true;
    }

    return true;
}

bool CircleOfFifthsDialog::handleKey(int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
    if (!isOpen_) return false;

    if (action == 1 /* GLFW_PRESS */) {
        if (key == 256 /* GLFW_KEY_ESCAPE */) {
            close();
            if (onClose) onClose();
            return true;
        } else if (key == 257 /* GLFW_KEY_ENTER */) {
            updateCurrentChord();
            if (onChordApplied) onChordApplied(currentChord_);
            close();
            return true;
        }
    }
    return true;
}

} // namespace eatsbits::ui
