#include "eatsbits/ui/widgets/plugin_search_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cctype>

namespace eatsbits::ui {

static bool stringContainsCaseInsensitive(const std::string& str, const std::string& sub) {
    if (sub.empty()) return true;
    auto it = std::search(
        str.begin(), str.end(),
        sub.begin(), sub.end(),
        [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return (it != str.end());
}

PluginSearchDialog::PluginSearchDialog() {
    initLibrary();
}

void PluginSearchDialog::initLibrary() {
    instrumentLibrary_ = {
        {"303_acid", "Roland TB-303 Acid", "BASS", "Diode Ladder", "Classic 303 acid bassline synth with screaming resonance and slide.", 1.0f, 0.55f, 0.0f},
        {"808_drums", "TR-808 Rhythm Kit", "DRUMS", "Analog Voice", "Deep booming sub bass kicks, punchy snares, and crisp metallic cymbals.", 0.13f, 0.96f, 0.91f},
        {"909_drums", "TR-909 Groove Kit", "DRUMS", "Hybrid PCM", "Thumping 909 kick, punchy claps, and open/closed hi-hat groove.", 1.0f, 0.16f, 0.43f},
        {"dx7_rhodes", "Yamaha DX7 FM E-Piano", "FM / RETRO", "6-Operator FM", "Lush crystalline 1980s FM electric piano with velocity timbre dynamics.", 0.62f, 0.31f, 0.87f},
        {"grand_piano", "Concert Grand Piano", "ACOUSTIC", "Waveguide Physical", "Acoustic grand piano commuted physical modeling with bridge coupling.", 0.88f, 0.66f, 0.43f},
        {"sid_lead", "Commodore 64 SID 8580", "FM / RETRO", "Chiptune SVF", "Retro chiptune 3-voice synth with hardware arpeggiator and LFSR noise.", 0.30f, 0.70f, 1.0f},
        {"snes_spc", "Super Nintendo S-DSP", "FM / RETRO", "16-Bit BRR", "Authentic 8-channel BRR sample synthesis with 8-tap FIR echo chamber.", 0.95f, 0.35f, 0.75f},
        {"modular_synth", "Eurorack Modular Synth", "SYNTHS", "Analog Patchable", "Dual complex VCO with wavefolder, low-pass gate, and ADSR envelopes.", 0.20f, 0.95f, 0.55f},
        {"eatscript_dsp", "Eatscript Custom DSP", "SYNTHS", "Live Bytecode VM", "Programmable sound synthesis engine powered by live JIT Eatscript VM.", 0.95f, 0.85f, 0.15f}
    };

    midiFxLibrary_ = {
        {"scale_snap", "Scale Snap & Quantize", "SCALE & KEY", "Pitch Quantizer", "Snaps incoming notes to selected musical scale, root key, and octave.", 1.0f, 0.55f, 0.0f},
        {"arp_pro", "Arpeggiator Pro", "ARPEGGIATOR", "Multi-Octave Arp", "Syncable pattern arpeggiator with up/down/random and octave spreads.", 0.13f, 0.96f, 0.91f},
        {"humanize", "Humanize & Groove Drift", "HUMANIZE", "Micro-Timing Drift", "Injects analog micro-timing jitter and velocity randomization.", 0.20f, 0.95f, 0.55f},
        {"chord_gen", "Harmonic Chord Voicer", "CHORDS", "Chord Inversions", "Transforms single notes into rich musical chord voicings and spreads.", 0.62f, 0.31f, 0.87f},
        {"velocity_filter", "Note Velocity Compressor", "SCALE & KEY", "Dynamics Shaper", "Compresses, expands, and limits MIDI note velocities.", 0.88f, 0.66f, 0.43f}
    };

    audioFxLibrary_ = {
        {"tube_distortion", "Tube Distortion", "DISTORTION", "Triode Saturation", "Warm analog asymmetric tube saturation, drive boost, and tone filter.", 1.0f, 0.55f, 0.0f},
        {"stereo_delay", "Stereo Ping-Pong Delay", "DELAY & REVERB", "BPM-Synced Delay", "Dual stereo delay lines with tape flutter and feedback damping.", 0.13f, 0.96f, 0.91f},
        {"chorus_flanger", "Analog BBD Chorus / Flanger", "MODULATION", "Bucket Brigade", "Rich spatial stereo widening, multi-voice chorus, and jet flanger.", 0.62f, 0.31f, 0.87f},
        {"parametric_eq", "3-Band Parametric Studio EQ", "DYNAMICS & EQ", "Biquad Filter", "Precision Low Shelf, Mid Bell, and High Shelf parametric equalization.", 0.20f, 0.95f, 0.55f},
        {"dynamics_comp", "Studio Dynamics Compressor", "DYNAMICS & EQ", "RMS Feed-Forward", "Punchy VCA studio compression with attack, release, and makeup gain.", 1.0f, 0.16f, 0.43f},
        {"convolver_reverb", "Partitioned Convolver Reverb", "DELAY & REVERB", "Zero-Latency IR", "Dense acoustic space convolution with real-time room impulse responses.", 0.30f, 0.70f, 1.0f}
    };
}

void PluginSearchDialog::open(PluginDialogMode mode, const std::string& targetTrackName, uint32_t targetTrackIndex) {
    mode_ = mode;
    targetTrackName_ = targetTrackName;
    targetTrackIndex_ = targetTrackIndex;
    isOpen_ = true;
    searchQuery_.clear();
    selectedCategoryIndex_ = 0;
    scrollY_ = 0.0f;
}

void PluginSearchDialog::layout(float screenW, float screenH) {
    screenWidth_ = screenW;
    screenHeight_ = screenH;

    float dw = std::min(screenW * 0.85f, 580.0f);
    float dh = std::min(screenH * 0.82f, 520.0f);
    float dx = (screenW - dw) * 0.5f;
    float dy = (screenH - dh) * 0.5f;

    dialogBounds_ = Rect2D(dx, dy, dw, dh);
    closeBtnBounds_ = Rect2D(dx + dw - 36.0f, dy + 14.0f, 24.0f, 24.0f);
    searchBoxBounds_ = Rect2D(dx + 20.0f, dy + 92.0f, dw - 40.0f, 32.0f);
}

std::vector<PluginEntry> PluginSearchDialog::getFilteredEntries() const {
    const std::vector<PluginEntry>* source = nullptr;
    const std::vector<std::string>* cats = nullptr;

    if (mode_ == PluginDialogMode::AddInstrument) {
        source = &instrumentLibrary_;
        cats = &instrumentCategories_;
    } else if (mode_ == PluginDialogMode::AddMidiFx) {
        source = &midiFxLibrary_;
        cats = &midiFxCategories_;
    } else {
        source = &audioFxLibrary_;
        cats = &audioFxCategories_;
    }

    std::vector<PluginEntry> results;
    std::string activeCat = (selectedCategoryIndex_ >= 0 && selectedCategoryIndex_ < static_cast<int>(cats->size()))
                                ? (*cats)[selectedCategoryIndex_]
                                : "ALL";

    for (const auto& entry : *source) {
        if (activeCat != "ALL" && entry.category != activeCat) {
            continue;
        }
        if (!searchQuery_.empty()) {
            if (!stringContainsCaseInsensitive(entry.name, searchQuery_) &&
                !stringContainsCaseInsensitive(entry.description, searchQuery_) &&
                !stringContainsCaseInsensitive(entry.engineTag, searchQuery_)) {
                continue;
            }
        }
        results.push_back(entry);
    }
    return results;
}

void PluginSearchDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-transparent full-screen darkening backdrop (consistent with core modal dialogs)
    drawRect(r, 0.0f, 0.0f, screenWidth_, screenHeight_, 0.0f, 0.0f, 0.0f, 0.55f);

    // 2. Dialog Window Chassis with soft drop shadow & subtle gold/accent outline
    drawRoundedRect(r, dialogBounds_.x - 3.0f, dialogBounds_.y - 3.0f, dialogBounds_.w + 6.0f, dialogBounds_.h + 6.0f, 13.0f,
                    0.0f, 0.0f, 0.0f, 0.45f);
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f,
                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.85f, 1.5f);

    // 3. Header Strip
    std::string title;
    if (mode_ == PluginDialogMode::AddInstrument) {
        title = targetTrackName_.empty() ? "ADD INSTRUMENT • NEW TRACK" : ("CHANGE INSTRUMENT • " + targetTrackName_);
    } else if (mode_ == PluginDialogMode::AddMidiFx) {
        title = "ADD MIDI FX • " + (targetTrackName_.empty() ? "TRACK" : targetTrackName_);
    } else {
        title = "ADD AUDIO FX • " + (targetTrackName_.empty() ? "TRACK" : targetTrackName_);
    }

    drawCircle(r, dialogBounds_.x + 24.0f, dialogBounds_.y + 26.0f, 4.0f,
               theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    drawText(r, title, dialogBounds_.x + 36.0f, dialogBounds_.y + 20.0f, 13.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    // Close button (metallic screw icon)
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(lastMouseX_, lastMouseY_) ||
                    (std::hypot(lastMouseX_ - clCenterX, lastMouseY_ - clCenterY) <= 13.0f);
    drawScrewCloseButton(r, clCenterX, clCenterY, 9.0f, closeHov, theme.primaryAccent);

    drawLine(r, dialogBounds_.x, dialogBounds_.y + 48.0f, dialogBounds_.x + dialogBounds_.w, dialogBounds_.y + 48.0f,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // 4. Category Filter Chips
    const auto& cats = (mode_ == PluginDialogMode::AddInstrument)
                           ? instrumentCategories_
                           : ((mode_ == PluginDialogMode::AddMidiFx) ? midiFxCategories_ : audioFxCategories_);

    float chipX = dialogBounds_.x + 20.0f;
    float chipY = dialogBounds_.y + 58.0f;
    for (size_t i = 0; i < cats.size(); ++i) {
        float chipW = static_cast<float>(cats[i].length()) * 7.5f + 18.0f;
        bool isAct = (static_cast<int>(i) == selectedCategoryIndex_);

        drawRoundedRect(r, chipX, chipY, chipW, 24.0f, 4.0f,
                        isAct ? theme.secondaryAccent.r * 0.25f : theme.controlBackground.r,
                        isAct ? theme.secondaryAccent.g * 0.25f : theme.controlBackground.g,
                        isAct ? theme.secondaryAccent.b * 0.25f : theme.controlBackground.b, 0.9f);
        if (isAct) {
            drawRoundedRectOutline(r, chipX, chipY, chipW, 24.0f, 4.0f,
                                   theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.9f, 1.0f);
        }
        drawCenteredText(r, cats[i], chipX, chipY, chipW, 24.0f, 9.5f,
                         isAct ? theme.secondaryAccent.r : theme.textMuted.r,
                         isAct ? theme.secondaryAccent.g : theme.textMuted.g,
                         isAct ? theme.secondaryAccent.b : theme.textMuted.b, 1.0f);

        chipX += chipW + 6.0f;
    }

    // 5. Search Bar
    drawRoundedRect(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 4.0f,
                    theme.backgroundDark.r, theme.backgroundDark.g, theme.backgroundDark.b, 0.95f);
    drawRoundedRectOutline(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    drawText(r, "[Q]", searchBoxBounds_.x + 10.0f, searchBoxBounds_.y + 8.0f, 10.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

    if (searchQuery_.empty()) {
        drawText(r, "Search library (type to filter)...", searchBoxBounds_.x + 36.0f, searchBoxBounds_.y + 8.0f, 11.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.5f);
    } else {
        drawMonoText(r, searchQuery_, searchBoxBounds_.x + 36.0f, searchBoxBounds_.y + 8.0f, 11.0f,
                     theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    }

    // 6. Plugin Cards List with ScrollableArea
    float listY = searchBoxBounds_.y + searchBoxBounds_.h + 12.0f;
    float listH = dialogBounds_.y + dialogBounds_.h - listY - 16.0f;
    float cardW = dialogBounds_.w - 40.0f;
    float cardH = 54.0f;

    auto entries = getFilteredEntries();
    float totalContentH = static_cast<float>(entries.size()) * (cardH + 8.0f);
    scrollArea_.setViewport(dialogBounds_.x + 20.0f, listY, cardW, listH);
    scrollArea_.setContentHeight(totalContentH);
    scrollArea_.setScrollY(scrollY_);
    scrollY_ = scrollArea_.getScrollY();

    for (size_t i = 0; i < entries.size(); ++i) {
        float cy = listY + static_cast<float>(i) * (cardH + 8.0f) - scrollY_;
        if (!scrollArea_.isVisible(cy, cardH)) continue;

        const auto& entry = entries[i];

        // Card Container
        drawRoundedRect(r, dialogBounds_.x + 20.0f, cy, cardW, cardH, 6.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
        drawRoundedRectOutline(r, dialogBounds_.x + 20.0f, cy, cardW, cardH, 6.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

        // Color tag strip on left
        drawRoundedRect(r, dialogBounds_.x + 22.0f, cy + 4.0f, 5.0f, cardH - 8.0f, 2.0f,
                        entry.r, entry.g, entry.b, 1.0f);

        // Title & Engine Tag
        drawText(r, entry.name, dialogBounds_.x + 36.0f, cy + 8.0f, 12.0f,
                 theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

        drawRoundedRect(r, dialogBounds_.x + 36.0f + entry.name.length() * 7.5f + 10.0f, cy + 8.0f,
                        entry.engineTag.length() * 6.5f + 10.0f, 14.0f, 3.0f,
                        entry.r * 0.2f, entry.g * 0.2f, entry.b * 0.2f, 0.8f);
        drawText(r, entry.engineTag, dialogBounds_.x + 36.0f + entry.name.length() * 7.5f + 15.0f, cy + 9.5f, 8.5f,
                 entry.r, entry.g, entry.b, 1.0f);

        // Description
        drawText(r, entry.description, dialogBounds_.x + 36.0f, cy + 28.0f, 9.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);

        // Add Button on Right
        float btnW = 68.0f;
        float btnH = 26.0f;
        float btnX = dialogBounds_.x + 20.0f + cardW - btnW - 12.0f;
        float btnY = cy + (cardH - btnH) * 0.5f;

        Color addBg{theme.primaryAccent.r * 0.25f, theme.primaryAccent.g * 0.25f, theme.primaryAccent.b * 0.25f, 0.9f};
        Color addBorder{theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.85f};
        drawButton(r, Rect2D{btnX, btnY, btnW, btnH}, "+ ADD", addBg, addBorder, theme.primaryAccent, 10.0f, 4.0f, 1.0f);
    }

    // Scrollbar rendering
    if (scrollArea_.canScroll()) {
        scrollArea_.renderScrollbar(r, theme);
    }
}

bool PluginSearchDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;
    lastMouseX_ = ev.x;
    lastMouseY_ = ev.y;

    auto entries = getFilteredEntries();
    float maxScroll = std::max(0.0f, static_cast<float>(entries.size()) * 62.0f - (dialogBounds_.h - 180.0f));

    if (scrollArea_.handlePointer(ev)) {
        scrollY_ = scrollArea_.getScrollY();
        return true;
    }

    if (ev.action == PointerAction::Scroll) {
        if (scrollArea_.handleScroll(ev.scrollY, ev.x, ev.y)) {
            scrollY_ = scrollArea_.getScrollY();
            return true;
        }
        scrollY_ = std::clamp(scrollY_ - ev.scrollY * 28.0f, 0.0f, maxScroll);
        return true;
    }

    if (ev.action == PointerAction::Move && isDraggingScroll_) {
        float dy = ev.y - dragStartY_;
        scrollY_ = std::clamp(dragStartScrollY_ - dy, 0.0f, maxScroll);
        scrollArea_.setScrollY(scrollY_);
        return true;
    }

    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        isDraggingScroll_ = false;
    }

    if (ev.action != PointerAction::Down) return true; // Absorb events behind modal

    isDraggingScroll_ = true;
    dragStartY_ = ev.y;
    dragStartScrollY_ = scrollY_;

    // Click outside dialog to close
    if (!dialogBounds_.contains(ev.x, ev.y)) {
        close();
        if (onClose) onClose();
        return true;
    }

    // Close button
    if (closeBtnBounds_.contains(ev.x, ev.y)) {
        close();
        if (onClose) onClose();
        return true;
    }

    // Category Chips
    const auto& cats = (mode_ == PluginDialogMode::AddInstrument)
                           ? instrumentCategories_
                           : ((mode_ == PluginDialogMode::AddMidiFx) ? midiFxCategories_ : audioFxCategories_);
    float chipX = dialogBounds_.x + 20.0f;
    float chipY = dialogBounds_.y + 58.0f;
    for (size_t i = 0; i < cats.size(); ++i) {
        float chipW = static_cast<float>(cats[i].length()) * 7.5f + 18.0f;
        Rect2D chipRect(chipX, chipY, chipW, 24.0f);
        if (chipRect.contains(ev.x, ev.y)) {
            selectedCategoryIndex_ = static_cast<int>(i);
            scrollY_ = 0.0f;
            return true;
        }
        chipX += chipW + 6.0f;
    }

    // Card Add Buttons
    float listY = searchBoxBounds_.y + searchBoxBounds_.h + 12.0f;
    float cardW = dialogBounds_.w - 40.0f;
    float cardH = 54.0f;

    for (size_t i = 0; i < entries.size(); ++i) {
        float cy = listY + static_cast<float>(i) * (cardH + 8.0f) - scrollY_;
        float btnW = 68.0f;
        float btnH = 26.0f;
        float btnX = dialogBounds_.x + 20.0f + cardW - btnW - 12.0f;
        float btnY = cy + (cardH - btnH) * 0.5f;

        Rect2D cardRect(dialogBounds_.x + 20.0f, cy, cardW, cardH);
        Rect2D btnRect(btnX, btnY, btnW, btnH);

        if (btnRect.contains(ev.x, ev.y) || cardRect.contains(ev.x, ev.y)) {
            const auto& entry = entries[i];
            close();
            if (onPluginSelected) {
                onPluginSelected(mode_, entry, targetTrackIndex_);
            }
            return true;
        }
    }

    return true; // Modal absorbed
}

bool PluginSearchDialog::handleKey(int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
    if (!isOpen_) return false;
    if (action != 1 && action != 2) return false;

    if (key == 256) { // Escape
        close();
        if (onClose) onClose();
        return true;
    }

    if (key == 259) { // Backspace
        if (!searchQuery_.empty()) {
            searchQuery_.pop_back();
        }
        return true;
    }

    if (key >= 32 && key <= 126) {
        searchQuery_ += static_cast<char>(key);
        return true;
    }

    return false;
}

} // namespace eatsbits::ui
