#include "eatsbits/ui/widgets/note_splitter_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <sstream>

namespace eatsbits::ui {

namespace {

inline std::string pitchToNoteName(uint8_t pitch) {
    static const char* noteNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    int noteIdx = pitch % 12;
    int octave = (pitch / 12) - 1;
    return std::string(noteNames[noteIdx]) + std::to_string(octave) + " (" + std::to_string(pitch) + ")";
}

} // namespace

NoteSplitterDialog::NoteSplitterDialog() = default;

void NoteSplitterDialog::open(size_t sourceTrackIdx, const std::string& trackName, const std::vector<eatscript::MidiNote>& notes) {
    sourceTrackIndex_ = sourceTrackIdx;
    sourceTrackName_ = trackName;
    sourceNotes_ = notes;
    isOpen_ = true;
    isDraggingSlider1_ = false;
    isDraggingSlider2_ = false;
    updatePreview();
}

void NoteSplitterDialog::updatePreview() {
    previewResults_ = sequencer::NoteSplitterEngine::splitNotes(sourceNotes_, mode_, params_);
}

void NoteSplitterDialog::layout(float screenW, float screenH) {
    const float w = std::min(600.0f, screenW - 40.0f);
    const float h = std::min(490.0f, screenH - 40.0f);
    const float x = (screenW - w) * 0.5f;
    const float y = (screenH - h) * 0.5f;

    dialogBounds_ = Rect2D(x, y, w, h);

    // Mode tabs row
    const float tabY = y + 54.0f;
    const float tabW = (w - 48.0f) / 4.0f;
    modeTabBounds_.clear();
    for (size_t i = 0; i < 4; ++i) {
        modeTabBounds_.push_back(Rect2D(x + 24.0f + i * tabW, tabY, tabW - 6.0f, 28.0f));
    }

    // Parameter sliders row
    const float sliderY = tabY + 38.0f;
    const float sliderW = (w - 48.0f - 16.0f) * 0.5f;
    paramSlider1Bounds_ = Rect2D(x + 24.0f, sliderY, sliderW, 40.0f);
    paramSlider2Bounds_ = Rect2D(paramSlider1Bounds_.right() + 16.0f, sliderY, sliderW, 40.0f);

    // Preview list box
    const float prevY = sliderY + 50.0f;
    const float prevH = 190.0f;
    previewCardBounds_ = Rect2D(x + 24.0f, prevY, w - 48.0f, prevH);

    // Remove source toggle & Action buttons
    const float botY = previewCardBounds_.bottom() + 16.0f;
    removeToggleBounds_ = Rect2D(x + 24.0f, botY, 220.0f, 32.0f);
    splitBtnBounds_ = Rect2D(x + w - 164.0f, botY, 140.0f, 34.0f);
    cancelBtnBounds_ = Rect2D(splitBtnBounds_.x - 90.0f, botY, 80.0f, 34.0f);
}

void NoteSplitterDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-transparent backdrop overlay
    drawRect(r, 0.0f, 0.0f, 4000.0f, 4000.0f, Color(0.0f, 0.0f, 0.0f, 0.72f));

    // 2. Dialog Chassis
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f, theme.panelBackground);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f, theme.primaryAccent, 1.8f);

    // Title
    drawText(r, "NOTE & CHORD VOICE SPLITTER", dialogBounds_.x + 24.0f, dialogBounds_.y + 20.0f, 13.0f, theme.primaryAccent);
    std::string sub = "Source: \"" + sourceTrackName_ + "\" (" + std::to_string(sourceNotes_.size()) + " notes) -> Distribute polyphony into melodic stems";
    drawText(r, sub, dialogBounds_.x + 24.0f, dialogBounds_.y + 36.0f, 9.5f, theme.textMuted);

    // 3. Mode Tabs
    static const char* tabLabels[] = {
        "3-WAY VOICE", "PIANO CLEFS", "4-VOICE SATB", "DRUM DEMUX"
    };
    for (size_t i = 0; i < modeTabBounds_.size(); ++i) {
        const auto& tb = modeTabBounds_[i];
        bool selected = (static_cast<size_t>(mode_) == i);
        Color fillCol = selected ? theme.primaryAccent : theme.panelHeader;
        Color textCol = selected ? Color(0.05f, 0.08f, 0.12f, 1.0f) : theme.textSecondary;

        drawRoundedRect(r, tb.x, tb.y, tb.w, tb.h, 4.0f, fillCol);
        if (!selected) {
            drawRoundedRectOutline(r, tb.x, tb.y, tb.w, tb.h, 4.0f, theme.borderSubtle, 1.0f);
        }
        drawCenteredText(r, tabLabels[i], tb, 9.5f, textCol);
    }

    // 4. Parameter Sliders (Contextual per Mode)
    auto drawPitchSlider = [&](const Rect2D& b, const std::string& title, uint8_t pitch, uint8_t minP, uint8_t maxP) {
        drawRoundedRect(r, b.x, b.y, b.w, b.h, 4.0f, theme.panelHeader);
        drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 4.0f, theme.borderSubtle, 1.0f);

        std::string lbl = title + ": " + pitchToNoteName(pitch);
        drawText(r, lbl, b.x + 8.0f, b.y + 5.0f, 9.0f, theme.textSecondary);

        float trackY = b.y + 24.0f;
        float trackW = b.w - 16.0f;
        drawRoundedRect(r, b.x + 8.0f, trackY, trackW, 4.0f, 2.0f, Color(0.10f, 0.14f, 0.20f, 1.0f));

        float norm = std::clamp(static_cast<float>(pitch - minP) / static_cast<float>(maxP - minP), 0.0f, 1.0f);
        drawRoundedRect(r, b.x + 8.0f, trackY, trackW * norm, 4.0f, 2.0f, theme.primaryAccent);
        drawCircle(r, b.x + 8.0f + trackW * norm, trackY + 2.0f, 5.0f, Color(1.0f, 1.0f, 1.0f, 1.0f));
    };

    if (mode_ == sequencer::SplitMode::ThreeWayVoice) {
        drawPitchSlider(paramSlider1Bounds_, "BASS SPLIT", params_.bassSplitPitch, 24, 60);
        drawPitchSlider(paramSlider2Bounds_, "LEAD THRESHOLD", params_.leadThresholdPitch, 48, 84);
    } else if (mode_ == sequencer::SplitMode::BassTrebleClefs) {
        drawPitchSlider(paramSlider1Bounds_, "PIVOT PITCH", params_.pivotPitch, 36, 84);
        drawRoundedRect(r, paramSlider2Bounds_.x, paramSlider2Bounds_.y, paramSlider2Bounds_.w, paramSlider2Bounds_.h, 4.0f, theme.panelHeader);
        drawText(r, "Low notes -> Bass Clef | High notes -> Treble", paramSlider2Bounds_.x + 8.0f, paramSlider2Bounds_.y + 14.0f, 9.0f, theme.textMuted);
    } else {
        drawRoundedRect(r, paramSlider1Bounds_.x, paramSlider1Bounds_.y, paramSlider1Bounds_.w, paramSlider1Bounds_.h, 4.0f, theme.panelHeader);
        drawText(r, mode_ == sequencer::SplitMode::FourVoiceSatb ? "Automatic SATB Voice Distribution" : "GM Standard Percussion Demuxer",
                 paramSlider1Bounds_.x + 8.0f, paramSlider1Bounds_.y + 14.0f, 9.5f, theme.textSecondary);
        drawRoundedRect(r, paramSlider2Bounds_.x, paramSlider2Bounds_.y, paramSlider2Bounds_.w, paramSlider2Bounds_.h, 4.0f, theme.panelHeader);
        drawText(r, mode_ == sequencer::SplitMode::FourVoiceSatb ? "Clusters chord voicings chronologically" : "Maps Kick, Snare, Hats, Toms into stems",
                 paramSlider2Bounds_.x + 8.0f, paramSlider2Bounds_.y + 14.0f, 9.0f, theme.textMuted);
    }

    // 5. Preview Cards List
    const auto& pb = previewCardBounds_;
    drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 6.0f, Color(0.06f, 0.08f, 0.12f, 1.0f));
    drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 6.0f, Color(0.18f, 0.24f, 0.34f, 1.0f), 1.0f);

    drawText(r, "OUTPUT TRACKS PREVIEW (" + std::to_string(previewResults_.size()) + " tracks will be created):",
             pb.x + 12.0f, pb.y + 10.0f, 9.5f, theme.textPrimary);

    const float cardY0 = pb.y + 28.0f;
    const float cardH = 34.0f;

    for (size_t i = 0; i < previewResults_.size(); ++i) {
        const auto& res = previewResults_[i];
        float cy = cardY0 + i * (cardH + 6.0f);
        if (cy + cardH > pb.bottom() - 6.0f) break;

        // Card row
        drawRoundedRect(r, pb.x + 10.0f, cy, pb.w - 20.0f, cardH, 4.0f, theme.panelHeader);
        // Colored accent strip on left
        drawRoundedRect(r, pb.x + 10.0f, cy, 6.0f, cardH, 2.0f, res.color);

        // Track name
        drawText(r, res.name, pb.x + 24.0f, cy + 9.0f, 10.0f, theme.textPrimary);

        // Pitch range info
        std::string rangeStr = "Range: " + pitchToNoteName(res.minPitch) + " - " + pitchToNoteName(res.maxPitch);
        drawText(r, rangeStr, pb.x + 240.0f, cy + 10.0f, 9.0f, theme.textMuted);

        // Note count badge
        std::string countStr = std::to_string(res.noteCount) + " notes";
        drawRoundedRect(r, pb.x + pb.w - 96.0f, cy + 6.0f, 76.0f, 20.0f, 3.0f, Color(0.10f, 0.14f, 0.20f, 1.0f));
        drawText(r, countStr, pb.x + pb.w - 86.0f, cy + 10.0f, 9.0f, theme.primaryAccent);
    }

    if (previewResults_.empty()) {
        drawText(r, "No notes found in source track to split.", pb.x + 16.0f, pb.y + 50.0f, 10.0f, theme.textMuted);
    }

    // 6. Remove Source Track Checkbox
    drawRoundedRect(r, removeToggleBounds_.x, removeToggleBounds_.y, 16.0f, 16.0f, 3.0f, theme.panelHeader);
    drawRoundedRectOutline(r, removeToggleBounds_.x, removeToggleBounds_.y, 16.0f, 16.0f, 3.0f, theme.borderSubtle, 1.0f);
    if (removeSourceTrack_) {
        drawRoundedRect(r, removeToggleBounds_.x + 3.0f, removeToggleBounds_.y + 3.0f, 10.0f, 10.0f, 2.0f, theme.primaryAccent);
    }
    drawText(r, "Mute source track after split", removeToggleBounds_.x + 24.0f, removeToggleBounds_.y + 3.0f, 9.5f, theme.textSecondary);

    // 7. Action Buttons
    drawButton(r, cancelBtnBounds_, "CANCEL", theme.panelHeader, theme.borderSubtle, theme.textSecondary, 10.0f);
    drawButton(r, splitBtnBounds_, "SPLIT INTO TRACKS", theme.primaryAccent, theme.borderFocus, Color(0.05f, 0.08f, 0.12f, 1.0f), 10.0f);
}

bool NoteSplitterDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;

    if (!dialogBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            close();
            return true;
        }
        return false;
    }

    if (ev.action == PointerAction::Down) {
        // Mode tabs
        for (size_t i = 0; i < modeTabBounds_.size(); ++i) {
            if (modeTabBounds_[i].contains(ev.x, ev.y)) {
                mode_ = static_cast<sequencer::SplitMode>(i);
                updatePreview();
                return true;
            }
        }

        // Sliders
        if (mode_ == sequencer::SplitMode::ThreeWayVoice) {
            if (paramSlider1Bounds_.contains(ev.x, ev.y)) {
                isDraggingSlider1_ = true;
                float norm = std::clamp((ev.x - (paramSlider1Bounds_.x + 8.0f)) / (paramSlider1Bounds_.w - 16.0f), 0.0f, 1.0f);
                params_.bassSplitPitch = static_cast<uint8_t>(std::round(24 + norm * 36));
                updatePreview();
                return true;
            }
            if (paramSlider2Bounds_.contains(ev.x, ev.y)) {
                isDraggingSlider2_ = true;
                float norm = std::clamp((ev.x - (paramSlider2Bounds_.x + 8.0f)) / (paramSlider2Bounds_.w - 16.0f), 0.0f, 1.0f);
                params_.leadThresholdPitch = static_cast<uint8_t>(std::round(48 + norm * 36));
                updatePreview();
                return true;
            }
        } else if (mode_ == sequencer::SplitMode::BassTrebleClefs) {
            if (paramSlider1Bounds_.contains(ev.x, ev.y)) {
                isDraggingSlider1_ = true;
                float norm = std::clamp((ev.x - (paramSlider1Bounds_.x + 8.0f)) / (paramSlider1Bounds_.w - 16.0f), 0.0f, 1.0f);
                params_.pivotPitch = static_cast<uint8_t>(std::round(36 + norm * 48));
                updatePreview();
                return true;
            }
        }

        // Remove source toggle
        if (removeToggleBounds_.contains(ev.x, ev.y)) {
            removeSourceTrack_ = !removeSourceTrack_;
            return true;
        }

        // Action Buttons
        if (splitBtnBounds_.contains(ev.x, ev.y)) {
            if (onSplitConfirmed) {
                onSplitConfirmed(sourceTrackIndex_, mode_, params_, removeSourceTrack_);
            }
            close();
            return true;
        }
        if (cancelBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        return true;
    }

    if (ev.action == PointerAction::Move) {
        if (isDraggingSlider1_) {
            if (mode_ == sequencer::SplitMode::ThreeWayVoice) {
                float norm = std::clamp((ev.x - (paramSlider1Bounds_.x + 8.0f)) / (paramSlider1Bounds_.w - 16.0f), 0.0f, 1.0f);
                params_.bassSplitPitch = static_cast<uint8_t>(std::round(24 + norm * 36));
                updatePreview();
            } else if (mode_ == sequencer::SplitMode::BassTrebleClefs) {
                float norm = std::clamp((ev.x - (paramSlider1Bounds_.x + 8.0f)) / (paramSlider1Bounds_.w - 16.0f), 0.0f, 1.0f);
                params_.pivotPitch = static_cast<uint8_t>(std::round(36 + norm * 48));
                updatePreview();
            }
            return true;
        }
        if (isDraggingSlider2_ && mode_ == sequencer::SplitMode::ThreeWayVoice) {
            float norm = std::clamp((ev.x - (paramSlider2Bounds_.x + 8.0f)) / (paramSlider2Bounds_.w - 16.0f), 0.0f, 1.0f);
            params_.leadThresholdPitch = static_cast<uint8_t>(std::round(48 + norm * 36));
            updatePreview();
            return true;
        }
    }

    if (ev.action == PointerAction::Up) {
        isDraggingSlider1_ = false;
        isDraggingSlider2_ = false;
        return true;
    }

    return true;
}

bool NoteSplitterDialog::handleKey(int key, int /*scancode*/, int action, int /*mods*/) {
    if (!isOpen_) return false;
    if (action == 1) { // GLFW_PRESS
        if (key == 256) { // GLFW_KEY_ESCAPE
            close();
            return true;
        }
        if (key == 257) { // GLFW_KEY_ENTER
            if (onSplitConfirmed) {
                onSplitConfirmed(sourceTrackIndex_, mode_, params_, removeSourceTrack_);
            }
            close();
            return true;
        }
    }
    return true;
}

} // namespace eatsbits::ui
