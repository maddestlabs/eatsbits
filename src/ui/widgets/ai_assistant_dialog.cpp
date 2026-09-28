#include "eatsbits/ui/widgets/ai_assistant_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace eatsbits::ui {

AiAssistantDialog::AiAssistantDialog() {
    apiKeyInput_ = client_.getApiKey();
    if (!apiKeyInput_.empty()) {
        isConnected_ = true;
        connectionStatus_ = "API Key Active";
    }
}

void AiAssistantDialog::open(AiDialogTab initialTab) {
    activeTab_ = initialTab;
    isOpen_ = true;
    focusedInput_ = -1;
}

void AiAssistantDialog::layout(float screenW, float screenH) {
    const float w = std::min(680.0f, screenW - 32.0f);
    const float h = std::min(540.0f, screenH - 32.0f);
    const float x = (screenW - w) * 0.5f;
    const float y = (screenH - h) * 0.5f;

    dialogBounds_ = Rect2D(x, y, w, h);

    // Header tabs
    const float tabY = y + 50.0f;
    const float tabW = (w - 48.0f) / 4.0f;
    tabBounds_.clear();
    for (size_t i = 0; i < 4; ++i) {
        tabBounds_.push_back(Rect2D(x + 24.0f + i * tabW, tabY, tabW - 6.0f, 28.0f));
    }

    closeBtnBounds_ = Rect2D(x + w - 44.0f, y + 16.0f, 24.0f, 24.0f);

    const float contentY = tabY + 40.0f;
    const float contentW = w - 48.0f;

    // --- Tab 0: Compose ---
    composeInputBounds_ = Rect2D(x + 24.0f, contentY + 24.0f, contentW, 44.0f);
    generateSongBtnBounds_ = Rect2D(x + 24.0f, composeInputBounds_.bottom() + 12.0f, 220.0f, 32.0f);

    // --- Tab 1: Sound Design ---
    soundCategoryPillBounds_.clear();
    const float catW = (contentW - 12.0f) / 3.0f;
    for (size_t i = 0; i < 3; ++i) {
        soundCategoryPillBounds_.push_back(Rect2D(x + 24.0f + i * (catW + 6.0f), contentY + 8.0f, catW, 26.0f));
    }
    soundInputBounds_ = Rect2D(x + 24.0f, contentY + 44.0f, contentW, 36.0f);
    generateSoundBtnBounds_ = Rect2D(x + 24.0f, soundInputBounds_.bottom() + 10.0f, 180.0f, 30.0f);
    injectCodeBtnBounds_ = Rect2D(generateSoundBtnBounds_.right() + 12.0f, soundInputBounds_.bottom() + 10.0f, 180.0f, 30.0f);
    codePreviewBounds_ = Rect2D(x + 24.0f, generateSoundBtnBounds_.bottom() + 12.0f, contentW, 230.0f);

    // --- Tab 2: Auto-Mix ---
    genrePillBounds_.clear();
    static const char* genres[] = {"Synthwave", "Acid Techno", "Lofi Chill", "Cyberpunk", "Rock/Acoustic"};
    const float gpW = (contentW - 20.0f) / 5.0f;
    for (size_t i = 0; i < 5; ++i) {
        genrePillBounds_.push_back(Rect2D(x + 24.0f + i * (gpW + 5.0f), contentY + 22.0f, gpW, 26.0f));
    }

    lufsPillBounds_.clear();
    const float lpW = (contentW - 18.0f) / 4.0f;
    for (size_t i = 0; i < 4; ++i) {
        lufsPillBounds_.push_back(Rect2D(x + 24.0f + i * (lpW + 6.0f), contentY + 76.0f, lpW, 26.0f));
    }

    runAutoMixBtnBounds_ = Rect2D(x + 24.0f, contentY + 116.0f, 220.0f, 34.0f);
    mixSummaryBounds_ = Rect2D(x + 24.0f, runAutoMixBtnBounds_.bottom() + 12.0f, contentW, 200.0f);

    // --- Tab 3: Settings ---
    apiKeyInputBounds_ = Rect2D(x + 24.0f, contentY + 30.0f, contentW - 140.0f, 34.0f);
    testConnBtnBounds_ = Rect2D(apiKeyInputBounds_.right() + 10.0f, apiKeyInputBounds_.y, 130.0f, 34.0f);
    modelToggleBounds_ = Rect2D(x + 24.0f, apiKeyInputBounds_.bottom() + 24.0f, 260.0f, 30.0f);
}

void AiAssistantDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-transparent backdrop overlay
    drawRect(r, 0.0f, 0.0f, 4000.0f, 4000.0f, Color(0.0f, 0.0f, 0.0f, 0.72f));

    // 2. Dialog Chassis with Primary Accent Glow Border
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f, theme.panelBackground);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f, theme.primaryAccent, 1.8f);

    // Header Title & Close Button
    drawText(r, "AI INTELLIGENT DAW ASSISTANT", dialogBounds_.x + 24.0f, dialogBounds_.y + 20.0f, 13.0f, theme.primaryAccent);
    drawText(r, "Natural language copilot, DSP EatScript sound designer & auto-mixer",
             dialogBounds_.x + 24.0f, dialogBounds_.y + 36.0f, 9.5f, theme.textMuted);

    // Close button (metallic screw icon)
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(lastMouseX_, lastMouseY_) ||
                    (std::hypot(lastMouseX_ - clCenterX, lastMouseY_ - clCenterY) <= 13.0f);
    drawScrewCloseButton(r, clCenterX, clCenterY, 9.0f, closeHov, theme.primaryAccent);

    // Render Navigation Tabs
    renderTabs(r, theme);

    // Render Active Content Tab
    switch (activeTab_) {
        case AiDialogTab::Compose: renderComposeTab(r, theme); break;
        case AiDialogTab::SoundDesign: renderSoundDesignTab(r, theme); break;
        case AiDialogTab::AutoMix: renderAutoMixTab(r, theme); break;
        case AiDialogTab::Settings: renderSettingsTab(r, theme); break;
    }
}

void AiAssistantDialog::renderTabs(BatchRenderer2D& r, const ThemeTokens& theme) {
    static const char* tabTitles[] = {
        "1. COMPOSE & SONGS",
        "2. SOUND DESIGN",
        "3. AUTO-MIX & MASTER",
        "4. GEMINI SETTINGS"
    };

    for (size_t i = 0; i < tabBounds_.size(); ++i) {
        const auto& tb = tabBounds_[i];
        bool selected = (static_cast<size_t>(activeTab_) == i);
        Color fillCol = selected ? theme.primaryAccent : theme.panelHeader;
        Color textCol = selected ? Color(0.05f, 0.08f, 0.12f, 1.0f) : theme.textSecondary;

        drawRoundedRect(r, tb.x, tb.y, tb.w, tb.h, 4.0f, fillCol);
        if (!selected) {
            drawRoundedRectOutline(r, tb.x, tb.y, tb.w, tb.h, 4.0f, theme.borderSubtle, 1.0f);
        }
        drawCenteredText(r, tabTitles[i], tb, 9.5f, textCol);
    }
}

void AiAssistantDialog::renderComposeTab(BatchRenderer2D& r, const ThemeTokens& theme) {
    drawText(r, "SONG & ARRANGEMENT PROMPT", dialogBounds_.x + 24.0f, composeInputBounds_.y - 12.0f, 9.0f, theme.textMuted);

    // Prompt input field
    drawRoundedRect(r, composeInputBounds_.x, composeInputBounds_.y, composeInputBounds_.w, composeInputBounds_.h, 4.0f, Color(0.08f, 0.12f, 0.16f, 1.0f));
    drawRoundedRectOutline(r, composeInputBounds_.x, composeInputBounds_.y, composeInputBounds_.w, composeInputBounds_.h, 4.0f,
                           (focusedInput_ == 0) ? theme.borderFocus : theme.borderSubtle, 1.0f);
    drawText(r, composePrompt_, composeInputBounds_.x + 10.0f, composeInputBounds_.y + 16.0f, 10.0f, theme.textPrimary);

    // Generate Button
    drawButton(r, generateSongBtnBounds_, "GENERATE BLUEPRINT", theme.primaryAccent, theme.borderFocus, Color(0.05f, 0.08f, 0.12f, 1.0f), 10.0f);

    // Blueprint results preview
    float prevY = generateSongBtnBounds_.bottom() + 16.0f;
    float prevH = dialogBounds_.bottom() - prevY - 20.0f;
    drawRoundedRect(r, dialogBounds_.x + 24.0f, prevY, dialogBounds_.w - 48.0f, prevH, 4.0f, Color(0.05f, 0.07f, 0.10f, 1.0f));
    drawRoundedRectOutline(r, dialogBounds_.x + 24.0f, prevY, dialogBounds_.w - 48.0f, prevH, 4.0f, theme.borderSubtle, 1.0f);

    if (generatedBlueprint_.empty()) {
        drawText(r, "Enter a song style prompt above to generate structured arrangement blueprints.",
                 dialogBounds_.x + 36.0f, prevY + 24.0f, 10.0f, theme.textMuted);
    } else {
        drawText(r, "GENERATED SONG BLUEPRINT:", dialogBounds_.x + 36.0f, prevY + 16.0f, 9.5f, theme.primaryAccent);
        drawText(r, generatedBlueprint_, dialogBounds_.x + 36.0f, prevY + 36.0f, 9.0f, theme.textSecondary);
    }
}

void AiAssistantDialog::renderSoundDesignTab(BatchRenderer2D& r, const ThemeTokens& theme) {
    // Sound category pills
    static const char* catLabels[] = {"SYNTH INSTRUMENT", "AUDIO FX", "MIDI FX"};
    for (size_t i = 0; i < soundCategoryPillBounds_.size(); ++i) {
        const auto& pb = soundCategoryPillBounds_[i];
        bool sel = (soundCategory_ == static_cast<int>(i));
        drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 4.0f, sel ? theme.primaryAccent : theme.panelHeader);
        if (!sel) drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 4.0f, theme.borderSubtle, 1.0f);
        drawCenteredText(r, catLabels[i], pb, 9.0f, sel ? Color(0.05f, 0.08f, 0.12f, 1.0f) : theme.textSecondary);
    }

    // Sound prompt input
    drawRoundedRect(r, soundInputBounds_.x, soundInputBounds_.y, soundInputBounds_.w, soundInputBounds_.h, 4.0f, Color(0.08f, 0.12f, 0.16f, 1.0f));
    drawRoundedRectOutline(r, soundInputBounds_.x, soundInputBounds_.y, soundInputBounds_.w, soundInputBounds_.h, 4.0f,
                           (focusedInput_ == 1) ? theme.borderFocus : theme.borderSubtle, 1.0f);
    drawText(r, soundPrompt_, soundInputBounds_.x + 8.0f, soundInputBounds_.y + 12.0f, 10.0f, theme.textPrimary);

    // Buttons
    drawButton(r, generateSoundBtnBounds_, "GENERATE EATSCRIPT", theme.primaryAccent, theme.borderFocus, Color(0.05f, 0.08f, 0.12f, 1.0f), 9.5f);
    drawButton(r, injectCodeBtnBounds_, "INJECT INTO TRACK", theme.panelHeader, theme.borderSubtle, theme.textSecondary, 9.5f);

    // Code preview card
    drawRoundedRect(r, codePreviewBounds_.x, codePreviewBounds_.y, codePreviewBounds_.w, codePreviewBounds_.h, 4.0f, Color(0.04f, 0.06f, 0.09f, 1.0f));
    drawRoundedRectOutline(r, codePreviewBounds_.x, codePreviewBounds_.y, codePreviewBounds_.w, codePreviewBounds_.h, 4.0f, theme.borderSubtle, 1.0f);

    if (generatedCode_.empty()) {
        drawText(r, "-- Click 'Generate EatScript' to synthesize custom DSP code from prompt",
                 codePreviewBounds_.x + 12.0f, codePreviewBounds_.y + 20.0f, 9.5f, theme.textMuted);
    } else {
        // Render first 8 lines
        std::istringstream stream(generatedCode_);
        std::string line;
        float ly = codePreviewBounds_.y + 16.0f;
        while (std::getline(stream, line) && ly < codePreviewBounds_.bottom() - 16.0f) {
            drawText(r, line, codePreviewBounds_.x + 12.0f, ly, 9.0f, theme.codeEditorText);
            ly += 14.0f;
        }
    }
}

void AiAssistantDialog::renderAutoMixTab(BatchRenderer2D& r, const ThemeTokens& theme) {
    // 1. Genre selection
    drawText(r, "GENRE TARGET PROFILE", dialogBounds_.x + 24.0f, genrePillBounds_[0].y - 12.0f, 8.5f, theme.textMuted);
    static const char* genreNames[] = {"Synthwave", "Acid Techno", "Lofi Chill", "Cyberpunk", "Rock/Acoustic"};
    for (size_t i = 0; i < genrePillBounds_.size(); ++i) {
        const auto& pb = genrePillBounds_[i];
        bool sel = (selectedGenre_ == genreNames[i]);
        drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 4.0f, sel ? theme.primaryAccent : theme.panelHeader);
        if (!sel) drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 4.0f, theme.borderSubtle, 1.0f);
        drawCenteredText(r, genreNames[i], pb, 8.5f, sel ? Color(0.05f, 0.08f, 0.12f, 1.0f) : theme.textSecondary);
    }

    // 2. Target LUFS selection
    drawText(r, "TARGET LOUDNESS CEILING", dialogBounds_.x + 24.0f, lufsPillBounds_[0].y - 12.0f, 8.5f, theme.textMuted);
    static const struct { const char* name; float lufs; } lufsPresets[] = {
        {"-14 LUFS (STREAMING)", -14.0f},
        {"-9 LUFS (CLUB/EDM)", -9.0f},
        {"-16 LUFS (BROADCAST)", -16.0f},
        {"-18 LUFS (AUDIOPHILE)", -18.0f}
    };
    for (size_t i = 0; i < lufsPillBounds_.size(); ++i) {
        const auto& pb = lufsPillBounds_[i];
        bool sel = (std::abs(selectedTargetLufs_ - lufsPresets[i].lufs) < 0.1f);
        drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 4.0f, sel ? theme.primaryAccent : theme.panelHeader);
        if (!sel) drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 4.0f, theme.borderSubtle, 1.0f);
        drawCenteredText(r, lufsPresets[i].name, pb, 8.5f, sel ? Color(0.05f, 0.08f, 0.12f, 1.0f) : theme.textSecondary);
    }

    // 3. Action button
    drawButton(r, runAutoMixBtnBounds_, "EXECUTE AUTO-MIX & MASTER", theme.primaryAccent, theme.borderFocus, Color(0.05f, 0.08f, 0.12f, 1.0f), 10.0f);

    // 4. Mix Summary Card
    drawRoundedRect(r, mixSummaryBounds_.x, mixSummaryBounds_.y, mixSummaryBounds_.w, mixSummaryBounds_.h, 4.0f, Color(0.06f, 0.08f, 0.12f, 1.0f));
    drawRoundedRectOutline(r, mixSummaryBounds_.x, mixSummaryBounds_.y, mixSummaryBounds_.w, mixSummaryBounds_.h, 4.0f, theme.borderSubtle, 1.0f);

    drawText(r, "MIX & MASTER RESULTS:", mixSummaryBounds_.x + 12.0f, mixSummaryBounds_.y + 16.0f, 9.5f, theme.textPrimary);
    drawText(r, mixSummary_, mixSummaryBounds_.x + 12.0f, mixSummaryBounds_.y + 36.0f, 9.0f, theme.textSecondary);

    if (lastTracksAdjusted_ > 0) {
        std::string adjStr = "Successfully balanced & adjusted " + std::to_string(lastTracksAdjusted_) + " tracks.";
        drawText(r, adjStr, mixSummaryBounds_.x + 12.0f, mixSummaryBounds_.y + 58.0f, 9.0f, theme.primaryAccent);
    }
}

void AiAssistantDialog::renderSettingsTab(BatchRenderer2D& r, const ThemeTokens& theme) {
    drawText(r, "GOOGLE GEMINI API KEY (BYOK)", dialogBounds_.x + 24.0f, apiKeyInputBounds_.y - 12.0f, 8.5f, theme.textMuted);

    // API Key input box
    drawRoundedRect(r, apiKeyInputBounds_.x, apiKeyInputBounds_.y, apiKeyInputBounds_.w, apiKeyInputBounds_.h, 4.0f, Color(0.08f, 0.12f, 0.16f, 1.0f));
    drawRoundedRectOutline(r, apiKeyInputBounds_.x, apiKeyInputBounds_.y, apiKeyInputBounds_.w, apiKeyInputBounds_.h, 4.0f,
                           (focusedInput_ == 3) ? theme.borderFocus : theme.borderSubtle, 1.0f);

    std::string maskedKey = apiKeyInput_.empty() ? "Paste your AI Studio Gemini API Key here..." : (apiKeyInput_.substr(0, 6) + "****************");
    drawText(r, maskedKey, apiKeyInputBounds_.x + 10.0f, apiKeyInputBounds_.y + 11.0f, 9.5f,
             apiKeyInput_.empty() ? theme.textMuted : theme.textPrimary);

    // Test Connection Button
    drawButton(r, testConnBtnBounds_, "TEST CONNECTION", theme.panelHeader, theme.borderSubtle, theme.textSecondary, 9.0f);

    // Status indicator
    Color statusCol = isConnected_ ? Color(0.20f, 0.85f, 0.40f, 1.0f) : theme.textMuted;
    drawText(r, "Status: " + connectionStatus_, dialogBounds_.x + 24.0f, apiKeyInputBounds_.bottom() + 16.0f, 9.0f, statusCol);

    // Model selection
    drawText(r, "ACTIVE GEMINI MODEL", dialogBounds_.x + 24.0f, modelToggleBounds_.y - 10.0f, 8.5f, theme.textMuted);
    drawRoundedRect(r, modelToggleBounds_.x, modelToggleBounds_.y, modelToggleBounds_.w, modelToggleBounds_.h, 4.0f, theme.panelHeader);
    drawRoundedRectOutline(r, modelToggleBounds_.x, modelToggleBounds_.y, modelToggleBounds_.w, modelToggleBounds_.h, 4.0f, theme.borderSubtle, 1.0f);
    drawText(r, "Model: " + client_.getModel(), modelToggleBounds_.x + 10.0f, modelToggleBounds_.y + 9.0f, 9.5f, theme.primaryAccent);
}

void AiAssistantDialog::executeAutoMix(sequencer::StepSequencer& seq) {
    ai::AiMixResult res = ai::AiMixingEngine::runAutoMixMaster(seq, client_, selectedGenre_, selectedTargetLufs_, mixInstructions_);
    mixSummary_ = res.summary;
    lastTracksAdjusted_ = res.tracksAdjusted;
    if (onMixCompleted) {
        onMixCompleted(res);
    }
}

bool AiAssistantDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;
    lastMouseX_ = ev.x;
    lastMouseY_ = ev.y;

    if (!dialogBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            close();
            return true;
        }
        return false;
    }

    if (ev.action == PointerAction::Down) {
        // Close button
        if (closeBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        // Navigation Tabs
        for (size_t i = 0; i < tabBounds_.size(); ++i) {
            if (tabBounds_[i].contains(ev.x, ev.y)) {
                activeTab_ = static_cast<AiDialogTab>(i);
                focusedInput_ = -1;
                return true;
            }
        }

        // --- Tab 0: Compose ---
        if (activeTab_ == AiDialogTab::Compose) {
            if (composeInputBounds_.contains(ev.x, ev.y)) {
                focusedInput_ = 0;
                return true;
            }
            if (generateSongBtnBounds_.contains(ev.x, ev.y)) {
                generatedBlueprint_ = client_.generateSongBlueprint(composePrompt_);
                if (onSongBlueprintGenerated) onSongBlueprintGenerated(generatedBlueprint_);
                return true;
            }
        }

        // --- Tab 1: Sound Design ---
        if (activeTab_ == AiDialogTab::SoundDesign) {
            for (size_t i = 0; i < soundCategoryPillBounds_.size(); ++i) {
                if (soundCategoryPillBounds_[i].contains(ev.x, ev.y)) {
                    soundCategory_ = static_cast<int>(i);
                    return true;
                }
            }
            if (soundInputBounds_.contains(ev.x, ev.y)) {
                focusedInput_ = 1;
                return true;
            }
            if (generateSoundBtnBounds_.contains(ev.x, ev.y)) {
                static const char* catNames[] = {"instrument", "audio_fx", "midi_fx"};
                generatedCode_ = client_.generateEatscript(soundPrompt_, catNames[soundCategory_]);
                return true;
            }
            if (injectCodeBtnBounds_.contains(ev.x, ev.y)) {
                if (!generatedCode_.empty() && onCodeInjected) {
                    static const char* catNames[] = {"instrument", "audio_fx", "midi_fx"};
                    onCodeInjected(generatedCode_, catNames[soundCategory_]);
                    close();
                }
                return true;
            }
        }

        // --- Tab 2: Auto-Mix ---
        if (activeTab_ == AiDialogTab::AutoMix) {
            static const char* genreNames[] = {"Synthwave", "Acid Techno", "Lofi Chill", "Cyberpunk", "Rock/Acoustic"};
            for (size_t i = 0; i < genrePillBounds_.size(); ++i) {
                if (genrePillBounds_[i].contains(ev.x, ev.y)) {
                    selectedGenre_ = genreNames[i];
                    return true;
                }
            }
            static const float lufsVals[] = {-14.0f, -9.0f, -16.0f, -18.0f};
            for (size_t i = 0; i < lufsPillBounds_.size(); ++i) {
                if (lufsPillBounds_[i].contains(ev.x, ev.y)) {
                    selectedTargetLufs_ = lufsVals[i];
                    return true;
                }
            }
            if (runAutoMixBtnBounds_.contains(ev.x, ev.y)) {
                // If executed from UI, signal or prepare summary
                mixSummary_ = "Auto-mix ready to execute on sequencer tracks.";
                return true;
            }
        }

        // --- Tab 3: Settings ---
        if (activeTab_ == AiDialogTab::Settings) {
            if (apiKeyInputBounds_.contains(ev.x, ev.y)) {
                focusedInput_ = 3;
                return true;
            }
            if (testConnBtnBounds_.contains(ev.x, ev.y)) {
                ai::GeminiResponse resp = client_.testConnection();
                isConnected_ = resp.success;
                connectionStatus_ = resp.success ? "Connected (200 OK)" : "Connection Failed";
                return true;
            }
            if (modelToggleBounds_.contains(ev.x, ev.y)) {
                std::string cur = client_.getModel();
                client_.setModel(cur == "gemini-2.5-flash" ? "gemini-1.5-flash" : "gemini-2.5-flash");
                return true;
            }
        }

        return true;
    }

    return true;
}

bool AiAssistantDialog::handleKey(int key, int /*scancode*/, int action, int /*mods*/) {
    if (!isOpen_) return false;

    if (action == 1) { // GLFW_PRESS
        if (key == 256) { // GLFW_KEY_ESCAPE
            close();
            return true;
        }
        if (key == 259) { // GLFW_KEY_BACKSPACE
            if (focusedInput_ == 0 && !composePrompt_.empty()) composePrompt_.pop_back();
            if (focusedInput_ == 1 && !soundPrompt_.empty()) soundPrompt_.pop_back();
            if (focusedInput_ == 3 && !apiKeyInput_.empty()) {
                apiKeyInput_.pop_back();
                client_.setApiKey(apiKeyInput_);
            }
            return true;
        }
    }
    return true;
}

bool AiAssistantDialog::handleChar(unsigned int codepoint) {
    if (!isOpen_) return false;
    if (codepoint >= 32 && codepoint < 127) {
        char c = static_cast<char>(codepoint);
        if (focusedInput_ == 0) composePrompt_ += c;
        if (focusedInput_ == 1) soundPrompt_ += c;
        if (focusedInput_ == 3) {
            apiKeyInput_ += c;
            client_.setApiKey(apiKeyInput_);
        }
        return true;
    }
    return false;
}

} // namespace eatsbits::ui
