#include "eatsbits/ui/widgets/plugin_search_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

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

static std::string toUpperStr(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

PluginSearchDialog::PluginSearchDialog() {
    initLibrary();
}

void PluginSearchDialog::initLibrary() {
    // 1. Initialize PresetManager to scan all preset scripts from files & built-in catalog
    project::PresetManager::instance().initialize();
    const auto& allPresets = project::PresetManager::instance().getAllPresets();

    allPresetLibrary_.clear();
    instrumentLibrary_.clear();
    drumsLibrary_.clear();
    audioFxLibrary_.clear();
    midiFxLibrary_.clear();
    midiSeqLibrary_.clear();
    utilityLibrary_.clear();

    for (const auto& p : allPresets) {
        PluginEntry entry;
        entry.id = p.id;
        entry.name = p.name;
        entry.category = p.category;
        entry.engineTag = p.engineTag.empty() ? p.subCategory : p.engineTag;
        entry.description = p.description;
        entry.r = p.colorR;
        entry.g = p.colorG;
        entry.b = p.colorB;
        entry.author = p.author;
        entry.filePath = p.filePath;

        allPresetLibrary_.push_back(entry);

        std::string cat = toUpperStr(p.category);
        if (cat == "DRUMS" || cat == "DRUM" || cat == "PERCUSSION") {
            drumsLibrary_.push_back(entry);
            instrumentLibrary_.push_back(entry); // Drum kits are playable as instruments
        } else if (cat == "AUDIO FX" || cat == "AUDIO_FX" || cat == "EFFECTS") {
            audioFxLibrary_.push_back(entry);
        } else if (cat == "MIDI FX" || cat == "MIDI_FX") {
            midiFxLibrary_.push_back(entry);
        } else if (cat == "MIDI SEQ" || cat == "MIDI_SEQ" || cat == "SEQUENCE") {
            midiSeqLibrary_.push_back(entry);
        } else if (cat == "UTILITY" || cat == "MACRO" || cat == "ACTION") {
            utilityLibrary_.push_back(entry);
        } else {
            instrumentLibrary_.push_back(entry);
        }
    }

    // Safety fallback: if preset folder was unavailable, ensure baseline devices exist
    if (instrumentLibrary_.empty()) {
        instrumentLibrary_ = {
            {"eats_303", "Roland TB-303 Acid Bass", "INSTRUMENTS", "Diode Ladder", "Classic 303 acid bassline synth with screaming resonance.", 0.13f, 0.96f, 0.91f},
            {"808_drums", "TR-808 Rhythm Kit", "DRUMS", "Analog Voice", "Deep booming sub bass kicks, punchy snares, and crisp metallic cymbals.", 1.0f, 0.0f, 0.48f},
            {"909_drums", "TR-909 Groove Kit", "DRUMS", "Hybrid PCM", "Thumping 909 kick, punchy claps, and open/closed hi-hat groove.", 1.0f, 0.16f, 0.43f},
            {"dx7_epiano", "Yamaha DX7 FM E-Piano", "INSTRUMENTS", "6-Operator FM", "Lush crystalline 1980s FM electric piano with velocity timbre dynamics.", 0.62f, 0.31f, 0.87f},
            {"concert_grand_piano", "Concert Grand Piano", "INSTRUMENTS", "Waveguide Physical", "Acoustic grand piano commuted physical modeling with bridge coupling.", 0.88f, 0.66f, 0.43f},
            {"c64_sid_synth", "Commodore 64 SID 8580", "INSTRUMENTS", "Chiptune SVF", "Retro chiptune 3-voice synth with hardware arpeggiator and LFSR noise.", 0.30f, 0.70f, 1.0f},
            {"snes_console_synth", "Super Nintendo S-DSP", "INSTRUMENTS", "16-Bit BRR", "Authentic 8-channel BRR sample synthesis with 8-tap FIR echo chamber.", 0.95f, 0.35f, 0.75f}
        };
        allPresetLibrary_ = instrumentLibrary_;
    }

    if (midiFxLibrary_.empty()) {
        midiFxLibrary_ = {
            {"scale_snap", "Scale Snap & Quantize", "MIDI FX", "SCALE_SNAP", "Snaps incoming notes to selected musical scale, root key, and octave.", 1.0f, 0.84f, 0.0f},
            {"arp_pro", "Arpeggiator Pro", "MIDI FX", "ARPEGGIATOR", "Syncable pattern arpeggiator with up/down/random and octave spreads.", 1.0f, 0.84f, 0.0f},
            {"humanize", "Humanize & Groove Drift", "MIDI FX", "HUMANIZE", "Injects analog micro-timing jitter and velocity randomization.", 1.0f, 0.84f, 0.0f},
            {"chord_gen", "Harmonic Chord Voicer", "MIDI FX", "CHORD_STABS", "Transforms single notes into rich musical chord voicings and spreads.", 1.0f, 0.84f, 0.0f},
            {"transpose", "Pitch Transposer", "MIDI FX", "TRANSPOSE", "Real-time semitone pitch transposition for melodic variations.", 1.0f, 0.84f, 0.0f}
        };
    }

    if (audioFxLibrary_.empty()) {
        audioFxLibrary_ = {
            {"stereo_delay", "Stereo Ping-Pong Delay", "AUDIO FX", "DELAY", "Dual stereo delay lines with tape flutter and feedback damping.", 0.74f, 0.0f, 1.0f},
            {"convolver_reverb", "Partitioned Convolver Reverb", "AUDIO FX", "CONVOLVER", "Dense acoustic space convolution with real-time room impulse responses.", 0.74f, 0.0f, 1.0f},
            {"analog_chorus", "Analog BBD Chorus / Flanger", "AUDIO FX", "CHORUS", "Rich spatial stereo widening, multi-voice chorus, and jet flanger.", 0.74f, 0.0f, 1.0f},
            {"studio_comp", "Studio Bus Compressor", "AUDIO FX", "COMP", "VCA bus dynamics with optical auto-release and makeup gain.", 0.74f, 0.0f, 1.0f},
            {"parametric_eq", "5-Band Parametric EQ", "AUDIO FX", "EQ", "Precision frequency sculpting with high/low shelving and peak filters.", 0.74f, 0.0f, 1.0f},
            {"tube_distortion", "Tube Overdrive & Saturation", "AUDIO FX", "DISTORTION", "Warm vacuum tube saturation, harmonic excitation, and soft clipping.", 0.74f, 0.0f, 1.0f},
            {"bitcrusher", "Vintage Sampler Crusher", "AUDIO FX", "BITCRUSHER", "Gritty 12-bit sampler crunch, sample rate reduction, and drive.", 0.74f, 0.0f, 1.0f}
        };
    }
}

void PluginSearchDialog::open(PluginDialogMode mode, const std::string& targetTrackName, uint32_t targetTrackIndex) {
    initLibrary();
    mode_ = mode;
    targetTrackName_ = targetTrackName;
    targetTrackIndex_ = targetTrackIndex;
    isOpen_ = true;
    searchQuery_.clear();
    searchCursorPos_ = 0;
    searchFocused_ = true;
    cursorBlinkTimer_ = 0.0f;
    selectedCategoryIndex_ = 0;
    selectedItemIndex_ = 0;
    scrollY_ = 0.0f;
    scrollArea_.setScrollY(0.0f);
}

void PluginSearchDialog::layout(float screenW, float screenH) {
    screenWidth_ = screenW;
    screenHeight_ = screenH;

    float dw = std::min(screenW * 0.90f, 620.0f);
    float dh = std::min(screenH * 0.85f, 540.0f);
    float dx = (screenW - dw) * 0.5f;
    float dy = (screenH - dh) * 0.5f;

    dialogBounds_ = Rect2D(dx, dy, dw, dh);
    closeBtnBounds_ = Rect2D(dx + dw - 36.0f, dy + 14.0f, 24.0f, 24.0f);
    searchBoxBounds_ = Rect2D(dx + 20.0f, dy + 92.0f, dw - 40.0f, 32.0f);
    searchClearBtnBounds_ = Rect2D(searchBoxBounds_.x + searchBoxBounds_.w - 28.0f, searchBoxBounds_.y + 4.0f, 24.0f, 24.0f);
}

void PluginSearchDialog::setSearchQuery(const std::string& query) {
    searchQuery_ = query;
    searchCursorPos_ = static_cast<int>(searchQuery_.length());
    selectedItemIndex_ = 0;
    scrollY_ = 0.0f;
    scrollArea_.setScrollY(0.0f);
    cursorBlinkTimer_ = 0.0f;
}

void PluginSearchDialog::clearSearch() {
    searchQuery_.clear();
    searchCursorPos_ = 0;
    selectedItemIndex_ = 0;
    scrollY_ = 0.0f;
    scrollArea_.setScrollY(0.0f);
    cursorBlinkTimer_ = 0.0f;
    searchFocused_ = true;
}

std::vector<PluginEntry> PluginSearchDialog::getFilteredEntries() const {
    const std::vector<PluginEntry>* source = nullptr;
    const std::vector<std::string>* cats = nullptr;

    if (mode_ == PluginDialogMode::SelectPreset) {
        source = &allPresetLibrary_;
        cats = &selectPresetCategories_;
    } else if (mode_ == PluginDialogMode::AddInstrument) {
        source = &instrumentLibrary_;
        cats = &instrumentCategories_;
    } else if (mode_ == PluginDialogMode::AddDrums) {
        source = &drumsLibrary_;
        cats = &drumsCategories_;
    } else if (mode_ == PluginDialogMode::AddMidiFx) {
        source = &midiFxLibrary_;
        cats = &midiFxCategories_;
    } else if (mode_ == PluginDialogMode::AddMidiSeq) {
        source = &midiSeqLibrary_;
        cats = &midiSeqCategories_;
    } else if (mode_ == PluginDialogMode::AddUtility) {
        source = &utilityLibrary_;
        cats = &utilityCategories_;
    } else {
        source = &audioFxLibrary_;
        cats = &audioFxCategories_;
    }

    std::vector<PluginEntry> results;
    std::string activeCat = (selectedCategoryIndex_ >= 0 && selectedCategoryIndex_ < static_cast<int>(cats->size()))
                                ? (*cats)[selectedCategoryIndex_]
                                : "ALL";

    // Split search query into whitespace-separated tokens for intelligent multi-term search
    std::vector<std::string> queryTokens;
    if (!searchQuery_.empty()) {
        std::istringstream iss(searchQuery_);
        std::string token;
        while (iss >> token) {
            queryTokens.push_back(token);
        }
    }

    for (const auto& entry : *source) {
        if (activeCat != "ALL") {
            std::string entryCatUpper = toUpperStr(entry.category);
            std::string activeCatUpper = toUpperStr(activeCat);

            bool catMatch = (entryCatUpper == activeCatUpper) ||
                            stringContainsCaseInsensitive(entry.category, activeCat) ||
                            stringContainsCaseInsensitive(entry.engineTag, activeCat) ||
                            stringContainsCaseInsensitive(entry.description, activeCat);
            if (!catMatch) continue;
        }

        if (!queryTokens.empty()) {
            bool allTokensMatch = true;
            for (const auto& tok : queryTokens) {
                bool tokMatches = stringContainsCaseInsensitive(entry.name, tok) ||
                                  stringContainsCaseInsensitive(entry.description, tok) ||
                                  stringContainsCaseInsensitive(entry.engineTag, tok) ||
                                  stringContainsCaseInsensitive(entry.author, tok) ||
                                  stringContainsCaseInsensitive(entry.id, tok) ||
                                  stringContainsCaseInsensitive(entry.category, tok);
                if (!tokMatches) {
                    allTokensMatch = false;
                    break;
                }
            }
            if (!allTokensMatch) continue;
        }
        results.push_back(entry);
    }
    return results;
}

void PluginSearchDialog::scrollIndexIntoView(int index) {
    float listY = searchBoxBounds_.y + searchBoxBounds_.h + 12.0f;
    float listH = dialogBounds_.y + dialogBounds_.h - listY - 16.0f;
    float cardH = 54.0f;
    float cardSpacing = 8.0f;
    float itemTop = static_cast<float>(index) * (cardH + cardSpacing);
    float itemBottom = itemTop + cardH;

    if (itemTop < scrollY_) {
        scrollY_ = itemTop;
    } else if (itemBottom > scrollY_ + listH) {
        scrollY_ = itemBottom - listH;
    }
    auto entries = getFilteredEntries();
    float totalContentH = static_cast<float>(entries.size()) * (cardH + cardSpacing);
    float maxScroll = std::max(0.0f, totalContentH - listH);
    scrollY_ = std::clamp(scrollY_, 0.0f, maxScroll);
    scrollArea_.setScrollY(scrollY_);
}

void PluginSearchDialog::selectHighlightedCard() {
    auto entries = getFilteredEntries();
    if (entries.empty()) return;

    if (selectedItemIndex_ < 0) selectedItemIndex_ = 0;
    if (selectedItemIndex_ >= static_cast<int>(entries.size())) {
        selectedItemIndex_ = static_cast<int>(entries.size()) - 1;
    }

    const auto& entry = entries[selectedItemIndex_];
    close();

    if (onPluginSelected) {
        onPluginSelected(mode_, entry, targetTrackIndex_);
    }

    const auto* pItem = project::PresetManager::instance().findPreset(entry.id);
    if (pItem && onPresetSelected) {
        onPresetSelected(*pItem, targetTrackIndex_);
    }
}

void PluginSearchDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-transparent backdrop overlay
    drawRect(r, 0.0f, 0.0f, screenWidth_, screenHeight_, 0.0f, 0.0f, 0.0f, 0.60f);

    // 2. Dialog Window Frame with soft glow & drop shadow
    drawRoundedRect(r, dialogBounds_.x - 3.0f, dialogBounds_.y - 3.0f, dialogBounds_.w + 6.0f, dialogBounds_.h + 6.0f, 13.0f,
                    0.0f, 0.0f, 0.0f, 0.45f);
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f,
                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.85f, 1.5f);

    // 3. Header Strip
    std::string title;
    if (mode_ == PluginDialogMode::SelectPreset) {
        title = targetTrackName_.empty() ? "PRESET LIBRARIAN • BROWSE & SELECT" : ("PRESETS • " + targetTrackName_);
    } else if (mode_ == PluginDialogMode::AddInstrument) {
        title = targetTrackName_.empty() ? "ADD INSTRUMENT • NEW TRACK" : ("CHANGE INSTRUMENT • " + targetTrackName_);
    } else if (mode_ == PluginDialogMode::AddDrums) {
        title = "DRUM KITS & SAMPLES • " + (targetTrackName_.empty() ? "TRACK" : targetTrackName_);
    } else if (mode_ == PluginDialogMode::AddMidiFx) {
        title = "ADD MIDI FX • " + (targetTrackName_.empty() ? "TRACK" : targetTrackName_);
    } else if (mode_ == PluginDialogMode::AddMidiSeq) {
        title = "ADD MIDI SEQUENCE • " + (targetTrackName_.empty() ? "TRACK" : targetTrackName_);
    } else if (mode_ == PluginDialogMode::AddUtility) {
        title = "UTILITY & MACRO SCRIPTS";
    } else {
        title = "ADD AUDIO FX • " + (targetTrackName_.empty() ? "TRACK" : targetTrackName_);
    }

    drawCircle(r, dialogBounds_.x + 24.0f, dialogBounds_.y + 26.0f, 4.5f,
               theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    drawText(r, title, dialogBounds_.x + 36.0f, dialogBounds_.y + 20.0f, 13.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    // Dynamic Filter Count pill in header
    auto entries = getFilteredEntries();
    std::string countStr;
    if (entries.empty()) {
        countStr = "0 MATCHES";
    } else if (!searchQuery_.empty()) {
        countStr = std::to_string(entries.size()) + (entries.size() == 1 ? " MATCH" : " MATCHES");
    } else {
        countStr = std::to_string(entries.size()) + " PRESETS";
    }
    float countW = countStr.length() * 6.5f + 14.0f;
    float countX = closeBtnBounds_.x - countW - 14.0f;
    float countY = dialogBounds_.y + 18.0f;
    drawRoundedRect(r, countX, countY, countW, 18.0f, 4.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
    drawCenteredText(r, countStr, countX, countY, countW, 18.0f, 9.0f,
                     entries.empty() ? 0.9f : theme.textMuted.r,
                     entries.empty() ? 0.4f : theme.textMuted.g,
                     entries.empty() ? 0.4f : theme.textMuted.b, 0.95f);

    // Close button (metallic screw icon)
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(lastMouseX_, lastMouseY_) ||
                    (std::hypot(lastMouseX_ - clCenterX, lastMouseY_ - clCenterY) <= 13.0f);
    drawScrewCloseButton(r, clCenterX, clCenterY, 9.0f, closeHov, theme.primaryAccent);

    drawLine(r, dialogBounds_.x, dialogBounds_.y + 48.0f, dialogBounds_.x + dialogBounds_.w, dialogBounds_.y + 48.0f,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // 4. Category Filter Chips
    const auto& cats = (mode_ == PluginDialogMode::SelectPreset) ? selectPresetCategories_ :
                       (mode_ == PluginDialogMode::AddInstrument) ? instrumentCategories_ :
                       (mode_ == PluginDialogMode::AddDrums) ? drumsCategories_ :
                       (mode_ == PluginDialogMode::AddMidiFx) ? midiFxCategories_ :
                       (mode_ == PluginDialogMode::AddMidiSeq) ? midiSeqCategories_ :
                       (mode_ == PluginDialogMode::AddUtility) ? utilityCategories_ : audioFxCategories_;

    float chipX = dialogBounds_.x + 20.0f;
    float chipY = dialogBounds_.y + 58.0f;
    for (size_t i = 0; i < cats.size(); ++i) {
        float chipW = static_cast<float>(cats[i].length()) * 7.5f + 16.0f;
        bool isAct = (static_cast<int>(i) == selectedCategoryIndex_);

        drawRoundedRect(r, chipX, chipY, chipW, 24.0f, 4.0f,
                        isAct ? theme.secondaryAccent.r * 0.25f : theme.controlBackground.r,
                        isAct ? theme.secondaryAccent.g * 0.25f : theme.controlBackground.g,
                        isAct ? theme.secondaryAccent.b * 0.25f : theme.controlBackground.b, 0.9f);
        if (isAct) {
            drawRoundedRectOutline(r, chipX, chipY, chipW, 24.0f, 4.0f,
                                   theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.9f, 1.2f);
        }
        drawCenteredText(r, cats[i], chipX, chipY, chipW, 24.0f, 9.5f,
                         isAct ? theme.secondaryAccent.r : theme.textMuted.r,
                         isAct ? theme.secondaryAccent.g : theme.textMuted.g,
                         isAct ? theme.secondaryAccent.b : theme.textMuted.b, 1.0f);

        chipX += chipW + 6.0f;
    }

    // 5. Refined Filter Text Box with Focus Glow & Blinking Cursor
    drawRoundedRect(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 5.0f,
                    theme.backgroundDark.r, theme.backgroundDark.g, theme.backgroundDark.b, 0.96f);
    if (searchFocused_) {
        drawRoundedRectOutline(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 5.0f,
                               theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.95f, 1.5f);
    } else {
        drawRoundedRectOutline(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 5.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);
    }

    drawText(r, "[Q]", searchBoxBounds_.x + 10.0f, searchBoxBounds_.y + 8.5f, 10.0f,
             searchFocused_ ? theme.primaryAccent.r : theme.textMuted.r,
             searchFocused_ ? theme.primaryAccent.g : theme.textMuted.g,
             searchFocused_ ? theme.primaryAccent.b : theme.textMuted.b, 0.9f);

    float textStartX = searchBoxBounds_.x + 36.0f;
    float textMaxW = searchBoxBounds_.w - 68.0f;
    float charW = getMonoCharAdvance(11.0f);

    if (searchQuery_.empty()) {
        std::string placeholder = "Search library by name, type, engine, tags, or author... (Arrow keys to navigate, Enter to load)";
        drawText(r, placeholder, textStartX, searchBoxBounds_.y + 8.5f, 10.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, searchFocused_ ? 0.45f : 0.6f);
        if (searchFocused_) {
            cursorBlinkTimer_ += (1.0f / 60.0f);
            if (fmod(cursorBlinkTimer_, 1.0f) < 0.55f) {
                drawLine(r, textStartX, searchBoxBounds_.y + 7.0f, textStartX, searchBoxBounds_.y + searchBoxBounds_.h - 7.0f,
                         theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f, 1.5f);
            }
        }
    } else {
        // Scissor search text strictly within the search box boundaries
        r.pushScissor(textStartX - 2.0f, searchBoxBounds_.y, textMaxW, searchBoxBounds_.h);
        drawMonoText(r, searchQuery_, textStartX, searchBoxBounds_.y + 8.0f, 11.0f,
                     theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

        if (searchFocused_) {
            cursorBlinkTimer_ += (1.0f / 60.0f);
            if (fmod(cursorBlinkTimer_, 1.0f) < 0.55f) {
                float cursorX = textStartX + static_cast<float>(searchCursorPos_) * charW;
                drawLine(r, cursorX, searchBoxBounds_.y + 7.0f, cursorX, searchBoxBounds_.y + searchBoxBounds_.h - 7.0f,
                         theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f, 1.5f);
            }
        }
        r.popScissor();

        // Clear [X] Button on Right of Search Box
        bool clearHov = searchClearBtnBounds_.contains(lastMouseX_, lastMouseY_);
        drawRoundedRect(r, searchClearBtnBounds_.x, searchClearBtnBounds_.y,
                        searchClearBtnBounds_.w, searchClearBtnBounds_.h, 4.0f,
                        clearHov ? 0.35f : 0.22f,
                        clearHov ? 0.38f : 0.24f,
                        clearHov ? 0.42f : 0.28f, clearHov ? 0.95f : 0.75f);
        drawCenteredText(r, "x", searchClearBtnBounds_.x, searchClearBtnBounds_.y,
                         searchClearBtnBounds_.w, searchClearBtnBounds_.h, 11.0f,
                         clearHov ? 1.0f : 0.8f,
                         clearHov ? 1.0f : 0.8f,
                         clearHov ? 1.0f : 0.8f, 1.0f);
    }

    // 6. Plugin Cards List with ScrollableArea & STRICT SCISSOR CONTAINMENT
    float listY = searchBoxBounds_.y + searchBoxBounds_.h + 12.0f;
    float listH = dialogBounds_.y + dialogBounds_.h - listY - 16.0f;
    float cardW = dialogBounds_.w - 40.0f;
    float cardH = 54.0f;
    float cardSpacing = 8.0f;

    float totalContentH = static_cast<float>(entries.size()) * (cardH + cardSpacing);
    scrollArea_.setViewport(dialogBounds_.x + 20.0f, listY, cardW, listH);
    scrollArea_.setContentHeight(totalContentH);
    scrollArea_.setScrollY(scrollY_);
    scrollY_ = scrollArea_.getScrollY();

    // STRICT SCISSOR CONTAINMENT: Completely confines scrolling within vertical layout bounds
    r.pushScissor(dialogBounds_.x + 18.0f, listY, cardW + 4.0f, listH);

    if (entries.empty()) {
        float emptyY = listY + listH * 0.32f;
        drawCenteredText(r, "NO MATCHING PRESETS OR PLUGINS FOUND",
                         dialogBounds_.x + 20.0f, emptyY, cardW, 20.0f, 12.0f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);

        std::string queryHint = "No results matching \"" + searchQuery_ + "\"";
        drawCenteredText(r, queryHint,
                         dialogBounds_.x + 20.0f, emptyY + 24.0f, cardW, 16.0f, 10.0f,
                         theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.9f);

        drawCenteredText(r, "Click [X] or press Esc to clear filter",
                         dialogBounds_.x + 20.0f, emptyY + 44.0f, cardW, 16.0f, 9.5f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.6f);
    } else {
        for (size_t i = 0; i < entries.size(); ++i) {
            float cy = listY + static_cast<float>(i) * (cardH + cardSpacing) - scrollY_;
            if (!scrollArea_.isVisible(cy, cardH)) continue;

            const auto& entry = entries[i];
            bool isKeyboardHighlighted = (static_cast<int>(i) == selectedItemIndex_);
            bool isHovered = Rect2D(dialogBounds_.x + 20.0f, cy, cardW, cardH).contains(lastMouseX_, lastMouseY_);

            // Card Container
            drawRoundedRect(r, dialogBounds_.x + 20.0f, cy, cardW, cardH, 6.0f,
                            isKeyboardHighlighted ? theme.controlBackground.r * 1.35f :
                            (isHovered ? theme.controlBackground.r * 1.15f : theme.controlBackground.r),
                            isKeyboardHighlighted ? theme.controlBackground.g * 1.35f :
                            (isHovered ? theme.controlBackground.g * 1.15f : theme.controlBackground.g),
                            isKeyboardHighlighted ? theme.controlBackground.b * 1.35f :
                            (isHovered ? theme.controlBackground.b * 1.15f : theme.controlBackground.b), 0.96f);

            if (isKeyboardHighlighted) {
                drawRoundedRectOutline(r, dialogBounds_.x + 20.0f, cy, cardW, cardH, 6.0f,
                                       theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.95f, 1.5f);
            } else {
                drawRoundedRectOutline(r, dialogBounds_.x + 20.0f, cy, cardW, cardH, 6.0f,
                                       theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, isHovered ? 0.9f : 0.6f, 1.0f);
            }

            // Color tag strip on left
            drawRoundedRect(r, dialogBounds_.x + 22.0f, cy + 4.0f, 5.0f, cardH - 8.0f, 2.0f,
                            entry.r, entry.g, entry.b, 1.0f);

            // Category Badge
            std::string catBadge = toUpperStr(entry.category);
            float catW = catBadge.length() * 6.0f + 10.0f;
            drawRoundedRect(r, dialogBounds_.x + 36.0f, cy + 8.0f, catW, 14.0f, 3.0f,
                            entry.r * 0.25f, entry.g * 0.25f, entry.b * 0.25f, 0.9f);
            drawCenteredText(r, catBadge, dialogBounds_.x + 36.0f, cy + 8.0f, catW, 14.0f, 8.0f,
                             entry.r, entry.g, entry.b, 1.0f);

            // Title
            float titleX = dialogBounds_.x + 36.0f + catW + 8.0f;
            drawText(r, entry.name, titleX, cy + 8.0f, 12.0f,
                     theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

            // Engine Tag
            if (!entry.engineTag.empty()) {
                float engX = titleX + entry.name.length() * 7.5f + 10.0f;
                float engW = entry.engineTag.length() * 6.5f + 10.0f;
                drawRoundedRect(r, engX, cy + 8.0f, engW, 14.0f, 3.0f,
                                0.2f, 0.22f, 0.28f, 0.8f);
                drawText(r, entry.engineTag, engX + 5.0f, cy + 9.5f, 8.5f,
                         theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.95f);
            }

            // Author (if provided)
            if (!entry.author.empty() && entry.author != "Eatsbeats / Eatsbits") {
                drawText(r, "by " + entry.author, dialogBounds_.x + cardW - 190.0f, cy + 9.5f, 8.5f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.7f);
            }

            // Description
            std::string desc = entry.description;
            if (desc.length() > 95) desc = desc.substr(0, 92) + "...";
            drawText(r, desc, dialogBounds_.x + 36.0f, cy + 29.0f, 9.5f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);

            // Action Button on Right (+ LOAD / + ADD)
            float btnW = 68.0f;
            float btnH = 26.0f;
            float btnX = dialogBounds_.x + 20.0f + cardW - btnW - 12.0f;
            float btnY = cy + (cardH - btnH) * 0.5f;

            bool btnHover = Rect2D(btnX, btnY, btnW, btnH).contains(lastMouseX_, lastMouseY_);
            Color addBg{entry.r * (btnHover ? 0.38f : 0.20f),
                        entry.g * (btnHover ? 0.38f : 0.20f),
                        entry.b * (btnHover ? 0.38f : 0.20f), 0.95f};
            Color addBorder{entry.r, entry.g, entry.b, btnHover ? 1.0f : 0.85f};
            Color addText{entry.r, entry.g, entry.b, 1.0f};

            std::string btnLabel = (mode_ == PluginDialogMode::SelectPreset) ? "LOAD" : "+ ADD";
            drawButton(r, Rect2D{btnX, btnY, btnW, btnH}, btnLabel, addBg, addBorder, addText, 10.0f, 4.0f, 1.0f);
        }
    }

    r.popScissor();

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
    float listY = searchBoxBounds_.y + searchBoxBounds_.h + 12.0f;
    float listH = dialogBounds_.y + dialogBounds_.h - listY - 16.0f;
    float cardW = dialogBounds_.w - 40.0f;
    float cardH = 54.0f;
    float cardSpacing = 8.0f;
    float totalContentH = static_cast<float>(entries.size()) * (cardH + cardSpacing);
    float maxScroll = std::max(0.0f, totalContentH - listH);

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
        scrollArea_.setScrollY(scrollY_);
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

    // Clear Button [X] inside search box
    if (!searchQuery_.empty() && searchClearBtnBounds_.contains(ev.x, ev.y)) {
        clearSearch();
        return true;
    }

    // Search Box Click: Acquire focus and place cursor
    if (searchBoxBounds_.contains(ev.x, ev.y)) {
        searchFocused_ = true;
        cursorBlinkTimer_ = 0.0f;
        float textStartX = searchBoxBounds_.x + 36.0f;
        float charW = getMonoCharAdvance(11.0f);
        int idx = std::clamp(static_cast<int>(std::round((ev.x - textStartX) / charW)),
                             0, static_cast<int>(searchQuery_.length()));
        searchCursorPos_ = idx;
        return true;
    }

    // Category Chips
    const auto& cats = (mode_ == PluginDialogMode::SelectPreset) ? selectPresetCategories_ :
                       (mode_ == PluginDialogMode::AddInstrument) ? instrumentCategories_ :
                       (mode_ == PluginDialogMode::AddDrums) ? drumsCategories_ :
                       (mode_ == PluginDialogMode::AddMidiFx) ? midiFxCategories_ :
                       (mode_ == PluginDialogMode::AddMidiSeq) ? midiSeqCategories_ :
                       (mode_ == PluginDialogMode::AddUtility) ? utilityCategories_ : audioFxCategories_;

    float chipX = dialogBounds_.x + 20.0f;
    float chipY = dialogBounds_.y + 58.0f;
    for (size_t i = 0; i < cats.size(); ++i) {
        float chipW = static_cast<float>(cats[i].length()) * 7.5f + 16.0f;
        Rect2D chipRect(chipX, chipY, chipW, 24.0f);
        if (chipRect.contains(ev.x, ev.y)) {
            selectedCategoryIndex_ = static_cast<int>(i);
            selectedItemIndex_ = 0;
            scrollY_ = 0.0f;
            scrollArea_.setScrollY(0.0f);
            return true;
        }
        chipX += chipW + 6.0f;
    }

    // Card Add Buttons / Card Selection: Strictly confined to vertical list bounds [listY, listY + listH]
    if (ev.y >= listY && ev.y <= listY + listH) {
        for (size_t i = 0; i < entries.size(); ++i) {
            float cy = listY + static_cast<float>(i) * (cardH + cardSpacing) - scrollY_;
            if (cy + cardH < listY || cy > listY + listH) continue;

            float btnW = 68.0f;
            float btnH = 26.0f;
            float btnX = dialogBounds_.x + 20.0f + cardW - btnW - 12.0f;
            float btnY = cy + (cardH - btnH) * 0.5f;

            Rect2D cardRect(dialogBounds_.x + 20.0f, cy, cardW, cardH);
            Rect2D btnRect(btnX, btnY, btnW, btnH);

            if (btnRect.contains(ev.x, ev.y) || cardRect.contains(ev.x, ev.y)) {
                selectedItemIndex_ = static_cast<int>(i);
                selectHighlightedCard();
                return true;
            }
        }
    }

    return true; // Modal absorbed
}

bool PluginSearchDialog::handleChar(char32_t codepoint) {
    if (!isOpen_) return false;
    if (codepoint >= 32 && codepoint <= 126) {
        char ch = static_cast<char>(codepoint);
        if (searchCursorPos_ >= 0 && searchCursorPos_ <= static_cast<int>(searchQuery_.length())) {
            searchQuery_.insert(searchCursorPos_, 1, ch);
            searchCursorPos_++;
        } else {
            searchQuery_ += ch;
            searchCursorPos_ = static_cast<int>(searchQuery_.length());
        }
        selectedItemIndex_ = 0;
        scrollY_ = 0.0f;
        scrollArea_.setScrollY(0.0f);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }
    return false;
}

bool PluginSearchDialog::handleKey(int key, [[maybe_unused]] int scancode, int action, int mods) {
    if (!isOpen_) return false;
    if (action != 1 && action != 2) return false;

    auto entries = getFilteredEntries();
    bool isCtrl = (mods & 0x0002) != 0;

    // Escape: clear search query first if non-empty, otherwise close dialog
    if (key == 256) {
        if (!searchQuery_.empty()) {
            clearSearch();
            return true;
        }
        close();
        if (onClose) onClose();
        return true;
    }

    // Down Arrow: navigate list down
    if (key == 264) {
        if (!entries.empty()) {
            selectedItemIndex_ = (selectedItemIndex_ + 1) % static_cast<int>(entries.size());
            scrollIndexIntoView(selectedItemIndex_);
        }
        return true;
    }

    // Up Arrow: navigate list up
    if (key == 265) {
        if (!entries.empty()) {
            selectedItemIndex_ = (selectedItemIndex_ - 1 + static_cast<int>(entries.size())) % static_cast<int>(entries.size());
            scrollIndexIntoView(selectedItemIndex_);
        }
        return true;
    }

    // Left Arrow: move search cursor left
    if (key == 263) {
        if (searchCursorPos_ > 0) {
            searchCursorPos_--;
            cursorBlinkTimer_ = 0.0f;
        }
        return true;
    }

    // Right Arrow: move search cursor right
    if (key == 262) {
        if (searchCursorPos_ < static_cast<int>(searchQuery_.length())) {
            searchCursorPos_++;
            cursorBlinkTimer_ = 0.0f;
        }
        return true;
    }

    // Home: move search cursor to beginning
    if (key == 268) {
        searchCursorPos_ = 0;
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // End: move search cursor to end
    if (key == 269) {
        searchCursorPos_ = static_cast<int>(searchQuery_.length());
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Enter / Numpad Enter: commit selection
    if (key == 257 || key == 335) {
        selectHighlightedCard();
        return true;
    }

    // Backspace: delete character or word before cursor
    if (key == 259) {
        if (isCtrl) {
            while (searchCursorPos_ > 0 && searchQuery_[searchCursorPos_ - 1] == ' ') {
                searchQuery_.erase(searchCursorPos_ - 1, 1);
                searchCursorPos_--;
            }
            while (searchCursorPos_ > 0 && searchQuery_[searchCursorPos_ - 1] != ' ') {
                searchQuery_.erase(searchCursorPos_ - 1, 1);
                searchCursorPos_--;
            }
        } else if (searchCursorPos_ > 0 && !searchQuery_.empty()) {
            searchQuery_.erase(searchCursorPos_ - 1, 1);
            searchCursorPos_--;
        }
        selectedItemIndex_ = 0;
        scrollY_ = 0.0f;
        scrollArea_.setScrollY(0.0f);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Delete: delete character at cursor
    if (key == 261) {
        if (searchCursorPos_ < static_cast<int>(searchQuery_.length())) {
            searchQuery_.erase(searchCursorPos_, 1);
            selectedItemIndex_ = 0;
            scrollY_ = 0.0f;
            scrollArea_.setScrollY(0.0f);
            cursorBlinkTimer_ = 0.0f;
        }
        return true;
    }

    // Ctrl+A: select all / clear
    if (isCtrl && (key == 65 || key == 97)) {
        clearSearch();
        return true;
    }

    // Ctrl+V: paste from clipboard
    if (isCtrl && (key == 86 || key == 118)) {
        if (clipboardProvider_) {
            std::string clipText = clipboardProvider_();
            std::string valid;
            for (char c : clipText) {
                if (c >= 32 && c <= 126) valid += c;
            }
            if (!valid.empty()) {
                searchQuery_.insert(searchCursorPos_, valid);
                searchCursorPos_ += static_cast<int>(valid.length());
                selectedItemIndex_ = 0;
                scrollY_ = 0.0f;
                scrollArea_.setScrollY(0.0f);
                cursorBlinkTimer_ = 0.0f;
            }
        }
        return true;
    }

    // Fallback printable character input (e.g. headless unit testing without GLFW onChar)
    if (!isCtrl && (mods & 0x0004) == 0 && key >= 32 && key <= 126) {
        char ch = static_cast<char>(key);
        bool isShift = (mods & 0x0001) != 0;
        if (!isShift && ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch + 32);
        }
        if (searchCursorPos_ >= 0 && searchCursorPos_ <= static_cast<int>(searchQuery_.length())) {
            searchQuery_.insert(searchCursorPos_, 1, ch);
            searchCursorPos_++;
        } else {
            searchQuery_ += ch;
            searchCursorPos_ = static_cast<int>(searchQuery_.length());
        }
        selectedItemIndex_ = 0;
        scrollY_ = 0.0f;
        scrollArea_.setScrollY(0.0f);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    return true; // Modal absorbs all other keystrokes
}

} // namespace eatsbits::ui
