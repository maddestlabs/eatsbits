#include "eatsbits/ui/widgets/audio_to_midi_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <iomanip>
#include <sstream>

namespace eatsbits::ui {

AudioToMidiDialog::AudioToMidiDialog() {
    trackNameField_.setText("Transcribed Audio (MIDI)");
}

void AudioToMidiDialog::open(const std::optional<audio::DecodedAudioBuffer>& initialBuffer,
                            const std::string& initialName) {
    isOpen_ = true;
    isProcessing_ = false;
    progress_ = 0.0f;
    statusMessage_ = "Ready to transcribe";
    activeDragSlider_ = -1;
    cancellationToken_.reset();

    if (initialBuffer.has_value() && !initialBuffer->empty()) {
        setAudioBuffer(*initialBuffer, initialName.empty() ? "Selected Audio Clip" : initialName);
    } else if (audioBuffer_.empty()) {
        audioFileName_ = "No audio loaded (Click Browse or load from Arranger clip)";
        waveform_ = audio::WaveformOverview{};
    }
}

void AudioToMidiDialog::close() noexcept {
    stopAudition();
    if (isProcessing_) {
        cancellationToken_.cancel();
    }
    isOpen_ = false;
    isProcessing_ = false;
    if (onClose) onClose();
}

void AudioToMidiDialog::setAudioBuffer(const audio::DecodedAudioBuffer& buffer, const std::string& fileName) {
    audioBuffer_ = buffer;
    audioFileName_ = fileName.empty() ? "Audio Recording" : fileName;
    waveform_ = audio::WaveformOverview::generate(audioBuffer_.samples, 160);

    std::string cleanName = audioFileName_;
    size_t lastDot = cleanName.find_last_of('.');
    if (lastDot != std::string::npos) {
        cleanName = cleanName.substr(0, lastDot);
    }
    trackNameField_.setText(cleanName + " (MIDI)");
}

bool AudioToMidiDialog::loadAudioFile(const std::string& filePath) {
    auto buf = audio::DecodedAudioBuffer::decodeFromFile(filePath);
    if (!buf.empty()) {
        setAudioBuffer(buf, filePath);
        statusMessage_ = "Loaded " + filePath;
        return true;
    }
    statusMessage_ = "Failed to decode audio file";
    return false;
}

void AudioToMidiDialog::stopAudition() noexcept {
    if (isAuditioning_) {
        isAuditioning_ = false;
        auditionPlayheadSec_ = 0.0f;
        if (onAuditionStop) onAuditionStop();
    }
}

void AudioToMidiDialog::update(float dt) {
    if (!isOpen_) return;

    if (isAuditioning_) {
        auditionPlayheadSec_ += dt;
        double totalSec = audioBuffer_.getDurationSeconds();
        if (totalSec > 0.0 && auditionPlayheadSec_ >= static_cast<float>(totalSec)) {
            stopAudition();
        }
    }
}

void AudioToMidiDialog::layout(float screenW, float screenH) {
    screenW_ = screenW;
    screenH_ = screenH;

    float dialogW = std::min(740.0f, screenW - 32.0f);
    float dialogH = std::min(580.0f, screenH - 32.0f);
    float dialogX = (screenW - dialogW) * 0.5f;
    float dialogY = (screenH - dialogH) * 0.5f;
    dialogBounds_ = Rect2D{dialogX, dialogY, dialogW, dialogH};

    // Close button
    closeBtnBounds_ = Rect2D{dialogX + dialogW - 38.0f, dialogY + 12.0f, 26.0f, 26.0f};

    // Waveform & Audio Source card
    float cardY = dialogY + 52.0f;
    float cardH = 96.0f;
    float cardW = dialogW - 32.0f;
    browseBtnBounds_ = Rect2D{dialogX + dialogW - 200.0f, cardY + 10.0f, 96.0f, 26.0f};
    auditionBtnBounds_ = Rect2D{dialogX + dialogW - 98.0f, cardY + 10.0f, 82.0f, 26.0f};
    waveformBounds_ = Rect2D{dialogX + 24.0f, cardY + 44.0f, cardW - 16.0f, 44.0f};

    // Engine Mode selector
    float modeY = cardY + cardH + 12.0f;
    float modeBtnW = (cardW - 12.0f) / 3.0f;
    modeHybridBounds_ = Rect2D{dialogX + 16.0f, modeY, modeBtnW, 28.0f};
    modeYinBounds_ = Rect2D{dialogX + 16.0f + modeBtnW + 6.0f, modeY, modeBtnW, 28.0f};
    modePercussiveBounds_ = Rect2D{dialogX + 16.0f + (modeBtnW + 6.0f) * 2.0f, modeY, modeBtnW, 28.0f};

    // Sliders
    float sliderStartY = modeY + 38.0f;
    float sliderRowH = 36.0f;
    float sliderW = cardW - 140.0f;
    onsetSliderBounds_ = Rect2D{dialogX + 130.0f, sliderStartY + 6.0f, sliderW, 14.0f};
    frameSliderBounds_ = Rect2D{dialogX + 130.0f, sliderStartY + sliderRowH + 6.0f, sliderW, 14.0f};
    durationSliderBounds_ = Rect2D{dialogX + 130.0f, sliderStartY + sliderRowH * 2.0f + 6.0f, sliderW, 14.0f};
    velocitySliderBounds_ = Rect2D{dialogX + 130.0f, sliderStartY + sliderRowH * 3.0f + 6.0f, sliderW, 14.0f};

    // Destination & Options
    float destY = sliderStartY + sliderRowH * 4.0f + 8.0f;
    createTrackToggleBounds_ = Rect2D{dialogX + 16.0f, destY, 150.0f, 26.0f};
    trackNameInputBounds_ = Rect2D{dialogX + 172.0f, destY, 260.0f, 26.0f};
    extractChordsToggleBounds_ = Rect2D{dialogX + 440.0f, destY, 260.0f, 26.0f};

    // Footer actions
    float footY = dialogY + dialogH - 46.0f;
    cancelBtnBounds_ = Rect2D{dialogX + dialogW - 370.0f, footY, 100.0f, 32.0f};
    transcribeBtnBounds_ = Rect2D{dialogX + dialogW - 256.0f, footY, 240.0f, 32.0f};
}

void AudioToMidiDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Darkened modal backdrop
    r.drawRect(0.0f, 0.0f, screenW_, screenH_, 0.0f, 0.0f, 0.0f, 0.70f);

    // 2. Dialog Chassis with subtle border glow
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f,
                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.65f, 1.5f);

    renderHeader(r, theme);
    renderWaveformSection(r, theme);
    renderEngineModeTabs(r, theme);
    renderSlidersSection(r, theme);
    renderDestinationSection(r, theme);
    renderProgressAndActions(r, theme);
}

void AudioToMidiDialog::renderHeader(BatchRenderer2D& r, const ThemeTokens& theme) {
    // Header bar
    drawRoundedRect(r, dialogBounds_.x + 2.0f, dialogBounds_.y + 2.0f, dialogBounds_.w - 4.0f, 40.0f, 6.0f,
                    theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.95f);
    drawLine(r, dialogBounds_.x, dialogBounds_.y + 42.0f, dialogBounds_.x + dialogBounds_.w, dialogBounds_.y + 42.0f,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.0f);

    // Title & Icon badge
    drawRoundedRect(r, dialogBounds_.x + 14.0f, dialogBounds_.y + 11.0f, 22.0f, 20.0f, 4.0f,
                    theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.25f);
    drawCenteredText(r, "AI", dialogBounds_.x + 14.0f, dialogBounds_.y + 11.0f, 22.0f, 20.0f, 9.5f,
                     theme.primaryAccent, 1.0f);

    drawText(r, "AUDIO TO MIDI CONVERTER", dialogBounds_.x + 44.0f, dialogBounds_.y + 13.0f, 12.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "Neural & DSP Transcriber", dialogBounds_.x + 230.0f, dialogBounds_.y + 15.0f, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);

    // Close button (metallic screw icon)
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(lastMouseX_, lastMouseY_) ||
                    (std::hypot(lastMouseX_ - clCenterX, lastMouseY_ - clCenterY) <= 13.0f);
    drawScrewCloseButton(r, clCenterX, clCenterY, 9.0f, closeHov, theme.primaryAccent);
}

void AudioToMidiDialog::renderWaveformSection(BatchRenderer2D& r, const ThemeTokens& theme) {
    float cardX = dialogBounds_.x + 16.0f;
    float cardY = dialogBounds_.y + 50.0f;
    float cardW = dialogBounds_.w - 32.0f;
    float cardH = 94.0f;

    // Card background
    drawRoundedRect(r, cardX, cardY, cardW, cardH, 6.0f,
                    theme.panelHeader.r * 0.7f, theme.panelHeader.g * 0.7f, theme.panelHeader.b * 0.7f, 0.7f);
    drawRoundedRectOutline(r, cardX, cardY, cardW, cardH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);

    // File name and duration
    std::string displayName = audioFileName_;
    if (displayName.length() > 42) displayName = displayName.substr(0, 39) + "...";

    drawText(r, displayName, cardX + 12.0f, cardY + 10.0f, 11.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    std::ostringstream meta;
    meta << std::fixed << std::setprecision(2) << audioBuffer_.getDurationSeconds() << "s | "
         << audioBuffer_.sampleRate << " Hz | "
         << (audioBuffer_.channels == 2 ? "Stereo" : "Mono");
    drawText(r, meta.str(), cardX + 12.0f, cardY + 26.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    // Browse Button
    drawButton(r, browseBtnBounds_, "BROWSE...", theme.panelHeader, theme.borderSubtle, theme.textPrimary, 9.5f, 4.0f);

    // Audition Button
    Color audBg = isAuditioning_ ? theme.primaryAccent : theme.panelHeader;
    Color audFg = isAuditioning_ ? Color(0.0f, 0.0f, 0.0f, 1.0f) : theme.textPrimary;
    drawButton(r, auditionBtnBounds_, isAuditioning_ ? "STOP" : "AUDITION",
               audBg, theme.borderSubtle, audFg, 9.5f, 4.0f);

    // Waveform preview box
    drawRoundedRect(r, waveformBounds_.x, waveformBounds_.y, waveformBounds_.w, waveformBounds_.h, 4.0f,
                    0.05f, 0.07f, 0.09f, 0.95f);
    drawRoundedRectOutline(r, waveformBounds_.x, waveformBounds_.y, waveformBounds_.w, waveformBounds_.h, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.4f, 1.0f);

    // Render waveform peaks
    if (!waveform_.maxPeaks.empty()) {
        size_t nPeaks = waveform_.maxPeaks.size();
        float halfH = waveformBounds_.h * 0.5f;
        float centerY = waveformBounds_.y + halfH;
        float dx = waveformBounds_.w / static_cast<float>(nPeaks);

        for (size_t i = 0; i < nPeaks; ++i) {
            float px = waveformBounds_.x + static_cast<float>(i) * dx;
            float topY = centerY - waveform_.maxPeaks[i] * halfH * 0.9f;
            float botY = centerY - waveform_.minPeaks[i] * halfH * 0.9f;
            if (botY - topY < 1.0f) botY = topY + 1.0f;

            r.drawRect(px, topY, std::max(1.0f, dx * 0.8f), botY - topY,
                       theme.primaryAccent.r * 0.85f, theme.primaryAccent.g * 0.85f, theme.primaryAccent.b * 0.85f, 0.85f);
        }

        // Audition playhead cursor
        double dur = audioBuffer_.getDurationSeconds();
        if (isAuditioning_ && dur > 0.0) {
            float playX = waveformBounds_.x + static_cast<float>(auditionPlayheadSec_ / dur) * waveformBounds_.w;
            r.drawLine(playX, waveformBounds_.y, playX, waveformBounds_.y + waveformBounds_.h,
                       1.0f, 0.9f, 0.1f, 1.0f, 2.0f);
        }
    } else {
        drawCenteredText(r, "Click BROWSE... or drag an audio file to analyze waveform",
                         waveformBounds_, 9.5f, theme.textMuted, 0.7f);
    }
}

void AudioToMidiDialog::renderEngineModeTabs(BatchRenderer2D& r, const ThemeTokens& theme) {
    auto renderTab = [&](const Rect2D& b, const std::string& label, bool active) {
        Color bg = active ? theme.primaryAccent : theme.panelHeader;
        Color border = active ? theme.primaryAccent : theme.borderSubtle;
        Color fg = active ? Color(0.0f, 0.0f, 0.0f, 1.0f) : theme.textMuted;
        drawButton(r, b, label, bg, border, fg, 9.5f, 4.0f, 1.0f);
    };

    renderTab(modeHybridBounds_, "HYBRID DSP (POLYPHONIC)", options_.mode == audio::TranscriptionEngineMode::HybridDsp);
    renderTab(modeYinBounds_, "YIN PITCH (MONO LEAD/BASS)", options_.mode == audio::TranscriptionEngineMode::YinMonophonic);
    renderTab(modePercussiveBounds_, "PERCUSSIVE (DRUM KITS)", options_.mode == audio::TranscriptionEngineMode::PercussiveTransient);
}

void AudioToMidiDialog::renderSlidersSection(BatchRenderer2D& r, const ThemeTokens& theme) {
    float startX = dialogBounds_.x + 16.0f;

    auto renderSlider = [&](const std::string& label, const std::string& valueStr,
                            const Rect2D& track, float normVal) {
        float labelY = track.y - 1.0f;
        drawText(r, label, startX, labelY, 9.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.95f);

        // Value text
        drawText(r, valueStr, track.x + track.w + 10.0f, labelY, 9.5f,
                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

        // Track bar
        drawRoundedRect(r, track.x, track.y + 3.0f, track.w, track.h - 6.0f, 3.0f, 0.15f, 0.17f, 0.20f, 0.9f);

        // Filled portion
        float fillW = std::clamp(normVal, 0.0f, 1.0f) * track.w;
        drawRoundedRect(r, track.x, track.y + 3.0f, fillW, track.h - 6.0f, 3.0f,
                        theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.85f);

        // Handle thumb
        float thumbX = track.x + fillW;
        r.drawCircle(thumbX, track.y + track.h * 0.5f, 6.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    };

    // 1. Onset Sensitivity
    std::ostringstream s0; s0 << std::fixed << std::setprecision(2) << options_.onsetThreshold;
    renderSlider("Onset Sensitivity", s0.str(), onsetSliderBounds_, options_.onsetThreshold);

    // 2. Frame / Sustain Threshold
    std::ostringstream s1; s1 << std::fixed << std::setprecision(2) << options_.frameThreshold;
    renderSlider("Frame Threshold", s1.str(), frameSliderBounds_, options_.frameThreshold);

    // 3. Min Note Duration
    std::ostringstream s2; s2 << static_cast<int>(options_.minNoteDurationMs) << " ms";
    float normDur = (options_.minNoteDurationMs - 20.0f) / 280.0f;
    renderSlider("Min Note Duration", s2.str(), durationSliderBounds_, normDur);

    // 4. Velocity Sensitivity
    std::ostringstream s3; s3 << std::fixed << std::setprecision(1) << options_.velocitySensitivity << "x";
    float normVel = (options_.velocitySensitivity - 0.5f) / 1.5f;
    renderSlider("Velocity Scale", s3.str(), velocitySliderBounds_, normVel);
}

void AudioToMidiDialog::renderDestinationSection(BatchRenderer2D& r, const ThemeTokens& theme) {
    // 1. Create New Track Toggle Checkbox
    drawRoundedRect(r, createTrackToggleBounds_.x, createTrackToggleBounds_.y + 4.0f, 16.0f, 16.0f, 3.0f,
                    theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.9f);
    drawRoundedRectOutline(r, createTrackToggleBounds_.x, createTrackToggleBounds_.y + 4.0f, 16.0f, 16.0f, 3.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.9f, 1.0f);
    if (createNewTrack_) {
        r.drawRect(createTrackToggleBounds_.x + 3.0f, createTrackToggleBounds_.y + 7.0f, 10.0f, 10.0f,
                   theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    }
    drawText(r, "Create New Track:", createTrackToggleBounds_.x + 22.0f, createTrackToggleBounds_.y + 5.0f, 9.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    // Track Name Input Field
    drawRoundedRect(r, trackNameInputBounds_.x, trackNameInputBounds_.y, trackNameInputBounds_.w, trackNameInputBounds_.h, 4.0f,
                    0.10f, 0.12f, 0.15f, 0.95f);
    drawRoundedRectOutline(r, trackNameInputBounds_.x, trackNameInputBounds_.y, trackNameInputBounds_.w, trackNameInputBounds_.h, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.0f);
    drawText(r, trackNameField_.getText(), trackNameInputBounds_.x + 8.0f, trackNameInputBounds_.y + 6.0f, 10.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    // 2. Extract Chords Toggle Checkbox
    drawRoundedRect(r, extractChordsToggleBounds_.x, extractChordsToggleBounds_.y + 4.0f, 16.0f, 16.0f, 3.0f,
                    theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.9f);
    drawRoundedRectOutline(r, extractChordsToggleBounds_.x, extractChordsToggleBounds_.y + 4.0f, 16.0f, 16.0f, 3.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.9f, 1.0f);
    if (extractChords_) {
        r.drawRect(extractChordsToggleBounds_.x + 3.0f, extractChordsToggleBounds_.y + 7.0f, 10.0f, 10.0f,
                   theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    }
    drawText(r, "Extract Chords to Chord Track", extractChordsToggleBounds_.x + 22.0f, extractChordsToggleBounds_.y + 5.0f, 9.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
}

void AudioToMidiDialog::renderProgressAndActions(BatchRenderer2D& r, const ThemeTokens& theme) {
    float startX = dialogBounds_.x + 16.0f;
    float cardW = dialogBounds_.w - 32.0f;

    // Progress bar & status
    float progY = dialogBounds_.y + dialogBounds_.h - 96.0f;
    drawText(r, statusMessage_, startX, progY - 2.0f, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.95f);

    if (isProcessing_) {
        drawRoundedRect(r, startX, progY + 16.0f, cardW, 8.0f, 4.0f, 0.12f, 0.14f, 0.18f, 0.9f);
        float fillW = std::clamp(progress_, 0.05f, 1.0f) * cardW;
        drawRoundedRect(r, startX, progY + 16.0f, fillW, 8.0f, 4.0f,
                        theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    }

    // Cancel Button
    drawButton(r, cancelBtnBounds_, "CANCEL", theme.panelHeader, theme.borderSubtle, theme.textPrimary, 10.0f, 4.0f);

    // Transcribe & Apply Button
    Color tBg = audioBuffer_.empty() ? Color(0.2f, 0.2f, 0.2f, 0.8f) : theme.primaryAccent;
    Color tFg = audioBuffer_.empty() ? Color(0.5f, 0.5f, 0.5f, 1.0f) : Color(0.0f, 0.0f, 0.0f, 1.0f);
    drawButton(r, transcribeBtnBounds_, isProcessing_ ? "TRANSCRIBING..." : "TRANSCRIBE & APPLY",
               tBg, Color(0, 0, 0, 0), tFg, 10.5f, 4.0f, 0.0f);
}

void AudioToMidiDialog::runTranscription() {
    if (audioBuffer_.empty()) {
        statusMessage_ = "Please load or select an audio file first!";
        return;
    }

    isProcessing_ = true;
    progress_ = 0.0f;
    statusMessage_ = "Starting transcription engine...";

    options_.extractChords = extractChords_;

    auto result = audio::AudioToMidiEngine::transcribeAudioBuffer(
        audioBuffer_,
        options_,
        &cancellationToken_,
        [this](float p, const std::string& msg) {
            progress_ = p;
            statusMessage_ = msg;
        });

    if (cancellationToken_.isCancelled()) {
        statusMessage_ = "Transcription cancelled";
        isProcessing_ = false;
        return;
    }

    isProcessing_ = false;
    progress_ = 1.0f;

    std::string customName = trackNameField_.getText().empty() ? "Transcribed Audio" : trackNameField_.getText();

    if (onTranscriptionComplete) {
        onTranscriptionComplete(result, createNewTrack_, customName, extractChords_);
    }

    close();
}

bool AudioToMidiDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;
    lastMouseX_ = ev.x;
    lastMouseY_ = ev.y;

    // Check outside dialog click
    if (ev.action == PointerAction::Down && !dialogBounds_.contains(ev.x, ev.y)) {
        close();
        return true;
    }

    if (ev.action == PointerAction::Down) {
        // Close button
        if (closeBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        // Browse button
        if (browseBtnBounds_.contains(ev.x, ev.y)) {
            if (onBrowseAudioFile) {
                std::string picked = onBrowseAudioFile();
                if (!picked.empty()) {
                    auto buf = audio::DecodedAudioBuffer::decodeFromFile(picked);
                    if (!buf.empty()) {
                        setAudioBuffer(buf, picked);
                        statusMessage_ = "Loaded " + picked;
                    } else {
                        statusMessage_ = "Failed to decode audio file";
                    }
                }
            }
            return true;
        }

        // Audition button
        if (auditionBtnBounds_.contains(ev.x, ev.y)) {
            if (isAuditioning_) {
                stopAudition();
            } else if (!audioBuffer_.empty()) {
                isAuditioning_ = true;
                auditionPlayheadSec_ = 0.0f;
                if (onAuditionStart) onAuditionStart(audioBuffer_.samples, audioBuffer_.sampleRate);
            }
            return true;
        }

        // Mode tabs
        if (modeHybridBounds_.contains(ev.x, ev.y)) {
            options_.mode = audio::TranscriptionEngineMode::HybridDsp;
            return true;
        }
        if (modeYinBounds_.contains(ev.x, ev.y)) {
            options_.mode = audio::TranscriptionEngineMode::YinMonophonic;
            return true;
        }
        if (modePercussiveBounds_.contains(ev.x, ev.y)) {
            options_.mode = audio::TranscriptionEngineMode::PercussiveTransient;
            return true;
        }

        // Sliders
        if (onsetSliderBounds_.contains(ev.x, ev.y)) {
            activeDragSlider_ = 0;
            float norm = (ev.x - onsetSliderBounds_.x) / onsetSliderBounds_.w;
            options_.onsetThreshold = std::clamp(norm, 0.05f, 0.95f);
            return true;
        }
        if (frameSliderBounds_.contains(ev.x, ev.y)) {
            activeDragSlider_ = 1;
            float norm = (ev.x - frameSliderBounds_.x) / frameSliderBounds_.w;
            options_.frameThreshold = std::clamp(norm, 0.05f, 0.95f);
            return true;
        }
        if (durationSliderBounds_.contains(ev.x, ev.y)) {
            activeDragSlider_ = 2;
            float norm = (ev.x - durationSliderBounds_.x) / durationSliderBounds_.w;
            options_.minNoteDurationMs = 20.0f + std::clamp(norm, 0.0f, 1.0f) * 280.0f;
            return true;
        }
        if (velocitySliderBounds_.contains(ev.x, ev.y)) {
            activeDragSlider_ = 3;
            float norm = (ev.x - velocitySliderBounds_.x) / velocitySliderBounds_.w;
            options_.velocitySensitivity = 0.5f + std::clamp(norm, 0.0f, 1.0f) * 1.5f;
            return true;
        }

        // Toggles
        if (createTrackToggleBounds_.contains(ev.x, ev.y)) {
            createNewTrack_ = !createNewTrack_;
            return true;
        }
        if (extractChordsToggleBounds_.contains(ev.x, ev.y)) {
            extractChords_ = !extractChords_;
            return true;
        }

        // Actions
        if (cancelBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }
        if (transcribeBtnBounds_.contains(ev.x, ev.y)) {
            runTranscription();
            return true;
        }
    } else if (ev.action == PointerAction::Move && activeDragSlider_ >= 0) {
        if (activeDragSlider_ == 0) {
            float norm = (ev.x - onsetSliderBounds_.x) / onsetSliderBounds_.w;
            options_.onsetThreshold = std::clamp(norm, 0.05f, 0.95f);
        } else if (activeDragSlider_ == 1) {
            float norm = (ev.x - frameSliderBounds_.x) / frameSliderBounds_.w;
            options_.frameThreshold = std::clamp(norm, 0.05f, 0.95f);
        } else if (activeDragSlider_ == 2) {
            float norm = (ev.x - durationSliderBounds_.x) / durationSliderBounds_.w;
            options_.minNoteDurationMs = 20.0f + std::clamp(norm, 0.0f, 1.0f) * 280.0f;
        } else if (activeDragSlider_ == 3) {
            float norm = (ev.x - velocitySliderBounds_.x) / velocitySliderBounds_.w;
            options_.velocitySensitivity = 0.5f + std::clamp(norm, 0.0f, 1.0f) * 1.5f;
        }
        return true;
    } else if (ev.action == PointerAction::Up) {
        activeDragSlider_ = -1;
    }

    return true; // Block clicks through to background workspace while modal is open
}

bool AudioToMidiDialog::handleKey(int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
    if (!isOpen_) return false;

    if (action == 1 /* GLFW_PRESS */) {
        if (key == 256 /* GLFW_KEY_ESCAPE */) {
            close();
            return true;
        }
        if (key == 257 /* GLFW_KEY_ENTER */) {
            runTranscription();
            return true;
        }
        if (key == 32 /* GLFW_KEY_SPACE */) {
            if (isAuditioning_) stopAudition();
            else if (!audioBuffer_.empty()) {
                isAuditioning_ = true;
                auditionPlayheadSec_ = 0.0f;
                if (onAuditionStart) onAuditionStart(audioBuffer_.samples, audioBuffer_.sampleRate);
            }
            return true;
        }
        if (key == 259 /* GLFW_KEY_BACKSPACE */) {
            std::string t = trackNameField_.getText();
            if (!t.empty()) {
                t.pop_back();
                trackNameField_.setText(t);
            }
            return true;
        }
    }
    return true;
}

bool AudioToMidiDialog::handleChar(unsigned int codepoint) {
    if (!isOpen_) return false;
    if (codepoint >= 32 && codepoint <= 126) {
        std::string t = trackNameField_.getText();
        t.push_back(static_cast<char>(codepoint));
        trackNameField_.setText(t);
        return true;
    }
    return false;
}

} // namespace eatsbits::ui
