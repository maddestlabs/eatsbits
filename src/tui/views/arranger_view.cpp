#include "eatsbits/tui/views/arranger_view.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::tui {

ArrangerView::ArrangerView() {
    setupDefaultProject();
}

void ArrangerView::setupDefaultProject() {
    tracks_.clear();

    // Track 1: Eats-909 (Drum Kit)
    ArrangerTrack drums;
    drums.name = "Eats-909";
    drums.volume = 0.90f;
    drums.pan = 0.50f;
    drums.clips.push_back({"Drums Main", 0, 4, Color::NeonGreen(), true});
    drums.clips.push_back({"Drums Fill", 4, 4, Color::NeonGreen(), true});
    tracks_.push_back(drums);

    // Track 2: Eats-303 (Acid Bassline)
    ArrangerTrack tb303;
    tb303.name = "Eats-303";
    tb303.volume = 0.85f;
    tb303.pan = 0.45f;
    tb303.clips.push_back({"Acid Riff", 0, 4, Color::AcidAmber(), true});
    tb303.clips.push_back({"Acid Slide", 4, 4, Color::AcidAmber(), true});
    tracks_.push_back(tb303);

    // Track 3: SNESSynth (Lead / Chords)
    ArrangerTrack snes;
    snes.name = "SNESSynth";
    snes.volume = 0.78f;
    snes.pan = 0.65f;
    snes.clips.push_back({"Retro Pad", 2, 6, Color::RetroPurple(), true});
    tracks_.push_back(snes);

    // Track 4: MasterEcho (FX / Delay)
    ArrangerTrack delay;
    delay.name = "AcidEcho";
    delay.volume = 0.65f;
    delay.pan = 0.50f;
    delay.clips.push_back({"Dub Echoes", 0, 8, Color::CyberCyan(), true});
    tracks_.push_back(delay);
}

std::string ArrangerView::getBraillePanGlyph(float pan) {
    if (pan < 0.35f) {
        return "⢎⡱"; // Leaning Left
    } else if (pan > 0.65f) {
        return "⢹⡇"; // Leaning Right
    } else {
        return "⠸⠇"; // Centered
    }
}

const ArrangerTrack* ArrangerView::getSelectedTrack() const {
    if (selectedTrack_ >= 0 && selectedTrack_ < static_cast<int>(tracks_.size())) {
        return &tracks_[static_cast<size_t>(selectedTrack_)];
    }
    return nullptr;
}

const AudioClip* ArrangerView::getSelectedClip() const {
    const auto* track = getSelectedTrack();
    if (track && selectedClip_ >= 0 && selectedClip_ < static_cast<int>(track->clips.size())) {
        return &track->clips[static_cast<size_t>(selectedClip_)];
    }
    return nullptr;
}

void ArrangerView::render(CellSurface& surface, const Rect& area, const TelemetrySnapshot& telemetry, bool isFocused) {
    if (area.width < 50 || area.height < 10) return;

    Color borderCol = isFocused ? Color::AcidAmber() : Color::Gray();
    std::string title = isFocused ? "ARRANGER TIMELINE [ACTIVE]" : "ARRANGER TIMELINE";
    surface.drawBox(area, borderCol, Color::Black(), isFocused, title);

    const int headerWidth = 24;
    const int inspectorWidth = showInspector_ ? std::min(24, area.width / 4) : 0;
    const int timelineWidth = area.width - 2 - headerWidth - inspectorWidth;

    cachedTrackHeadersArea_ = {area.x + 1, area.y + 1, headerWidth, area.height - 2};
    cachedTimelineArea_ = {area.x + 1 + headerWidth, area.y + 1, timelineWidth, area.height - 2};
    cachedInspectorArea_ = {area.x + 1 + headerWidth + timelineWidth, area.y + 1, inspectorWidth, area.height - 2};

    // 1. Timeline Ruler (Top row of timeline)
    Rect rulerArea{cachedTimelineArea_.x, cachedTimelineArea_.y, cachedTimelineArea_.width, 1};
    int numBars = std::max(1, cachedTimelineArea_.width / colsPerBar_);
    renderRuler(surface, rulerArea, timelineScrollX_, numBars, telemetry);

    // 2. Render Tracks & Lanes (2 rows per track)
    const int trackContentY = cachedTimelineArea_.y + 1;
    const int availableRows = cachedTimelineArea_.height - 1;
    const int maxVisibleTracks = availableRows / 2;

    for (int t = 0; t < maxVisibleTracks; ++t) {
        int trackIdx = trackScrollY_ + t;
        if (trackIdx >= static_cast<int>(tracks_.size())) break;

        auto& track = tracks_[static_cast<size_t>(trackIdx)];
        int rowY = trackContentY + t * 2;
        bool isSelected = (trackIdx == selectedTrack_);

        Rect headerRect{cachedTrackHeadersArea_.x, rowY, headerWidth, 2};
        renderTrackHeader(surface, headerRect, track, trackIdx, isSelected);

        Rect laneRect{cachedTimelineArea_.x, rowY, timelineWidth, 2};
        renderTrackLane(surface, laneRect, track, trackIdx, timelineScrollX_, numBars, telemetry);
    }

    // 3. Right Sidebar Inspector
    if (showInspector_ && inspectorWidth > 0) {
        renderInspector(surface, cachedInspectorArea_);
    }
}

void ArrangerView::renderRuler(CellSurface& surface, const Rect& area, int startBar, int numBars, const TelemetrySnapshot& telemetry) {
    surface.fillRect(area, Color::DarkGray(), ' ');

    for (int b = 0; b < numBars; ++b) {
        int barNum = startBar + b + 1;
        int barX = area.x + b * colsPerBar_;
        if (barX + colsPerBar_ > area.x + area.width) break;

        char barLabel[16];
        snprintf(barLabel, sizeof(barLabel), "%d.1", barNum);
        surface.drawText(barX, area.y, barLabel, Color::LightGray(), Color::DarkGray(), static_cast<uint8_t>(TextAttr::Bold));
        if (b > 0) {
            surface.setCell(barX - 1, area.y, Cell{0x2502, Color::Gray(), Color::DarkGray(), 0});
        }
    }

    // Live playhead indicator on ruler
    if (telemetry.isPlaying) {
        int currentBar = static_cast<int>(telemetry.currentStep / 16);
        float stepInBar = static_cast<float>(telemetry.currentStep % 16) / 16.0f;
        int playheadX = area.x + (currentBar - startBar) * colsPerBar_ + static_cast<int>(stepInBar * static_cast<float>(colsPerBar_));

        if (playheadX >= area.x && playheadX < area.x + area.width) {
            surface.setCell(playheadX, area.y, Cell{0x25BC, Color::AcidAmber(), Color::DarkGray(), static_cast<uint8_t>(TextAttr::Bold)});
        }
    }
}

void ArrangerView::renderTrackHeader(CellSurface& surface, const Rect& area, ArrangerTrack& track, int trackIdx, bool isSelected) {
    (void)trackIdx;
    Color bg = isSelected ? Color::FromHex(0x1F222B) : Color::Black();

    // Row 1: Track Name + Mute/Solo/Arm buttons
    std::string name = track.name;
    if (name.size() > 9) name = name.substr(0, 9);
    while (name.size() < 9) name.push_back(' ');

    Color nameCol = isSelected ? Color::AcidAmber() : (track.muted ? Color::Gray() : Color::White());
    surface.drawText(area.x, area.y, name, nameCol, bg, isSelected ? static_cast<uint8_t>(TextAttr::Bold) : 0);

    // [M] Mute button
    Color muteBg = track.muted ? Color::DangerRed() : Color::DarkGray();
    Color muteFg = track.muted ? Color::White() : Color::LightGray();
    surface.drawText(area.x + 10, area.y, "[M]", muteFg, muteBg);

    // [S] Solo button
    Color soloBg = track.solo ? Color::AcidYellow() : Color::DarkGray();
    Color soloFg = track.solo ? Color::Black() : Color::LightGray();
    surface.drawText(area.x + 14, area.y, "[S]", soloFg, soloBg);

    // [*] Arm button
    Color armBg = track.armed ? Color::DangerRed() : Color::DarkGray();
    Color armFg = track.armed ? Color::White() : Color::LightGray();
    surface.drawText(area.x + 18, area.y, "[*]", armFg, armBg);

    // Separator line
    surface.drawText(area.x + 22, area.y, "│", Color::DarkGray(), bg);

    // Row 2: Volume Slider and Braille Pan Dial
    surface.drawText(area.x, area.y + 1, "Vol ", Color::LightGray(), bg);

    // 7-character slider: Vol ────+──
    const int sliderWidth = 7;
    int thumbPos = std::clamp(static_cast<int>(track.volume * static_cast<float>(sliderWidth - 1)), 0, sliderWidth - 1);
    for (int i = 0; i < sliderWidth; ++i) {
        char32_t faderChar = (i == thumbPos) ? '+' : 0x2500;
        Color faderCol = (i == thumbPos) ? Color::White() : Color::Gray();
        uint8_t faderAttr = (i == thumbPos) ? static_cast<uint8_t>(TextAttr::Bold) : uint8_t(0);
        surface.setCell(area.x + 4 + i, area.y + 1, Cell{faderChar, faderCol, bg, faderAttr});
    }

    // Pan with rotating Braille dial
    surface.drawText(area.x + 12, area.y + 1, "Pan ", Color::LightGray(), bg);
    std::string panGlyph = getBraillePanGlyph(track.pan);
    surface.drawText(area.x + 16, area.y + 1, panGlyph, Color::AcidAmber(), bg, static_cast<uint8_t>(TextAttr::Bold));

    surface.drawText(area.x + 22, area.y + 1, "│", Color::DarkGray(), bg);
}

void ArrangerView::renderTrackLane(CellSurface& surface, const Rect& area, ArrangerTrack& track, int trackIdx, int startBar, int numBars, const TelemetrySnapshot& telemetry) {
    Color laneBg = (trackIdx % 2 == 0) ? Color::FromHex(0x12141A) : Color::FromHex(0x151820);
    surface.fillRect(area, laneBg, ' ');

    // Vertical grid lines for bars
    for (int b = 0; b <= numBars; ++b) {
        int gx = area.x + b * colsPerBar_;
        if (gx < area.x + area.width) {
            surface.setCell(gx, area.y, Cell{0x2502, Color::FromHex(0x222632), laneBg, 0});
            surface.setCell(gx, area.y + 1, Cell{0x2502, Color::FromHex(0x222632), laneBg, 0});
        }
    }

    // Render Audio Clips
    for (size_t c = 0; c < track.clips.size(); ++c) {
        const auto& clip = track.clips[c];
        int clipStartCol = area.x + (clip.startBar - startBar) * colsPerBar_;
        int clipWidthCols = clip.lengthBars * colsPerBar_;

        if (clipStartCol + clipWidthCols <= area.x || clipStartCol >= area.x + area.width) {
            continue; // Out of view
        }

        int drawX = std::max(area.x, clipStartCol);
        int drawWidth = std::min(area.x + area.width - drawX, clipStartCol + clipWidthCols - drawX);
        if (drawWidth <= 2) continue;

        bool isClipSelected = (trackIdx == selectedTrack_ && static_cast<int>(c) == selectedClip_);
        Color clipBg = track.muted ? Color::DarkGray() : clip.color.blend(Color::Black(), 0.65f);
        Color clipFg = track.muted ? Color::LightGray() : clip.color;

        // Row 1: Clip Header (Move handle)
        std::string clipHeader = "╭ " + clip.name + " ";
        while (static_cast<int>(clipHeader.size()) < drawWidth - 1) {
            clipHeader += "─";
        }
        clipHeader += "╮";
        if (static_cast<int>(clipHeader.size()) > drawWidth) {
            clipHeader = clipHeader.substr(0, static_cast<size_t>(drawWidth));
        }

        surface.drawText(drawX, area.y, clipHeader, clipFg, clipBg, isClipSelected ? static_cast<uint8_t>(TextAttr::Bold) : 0);

        // Row 2: Micro-Waveform + Resize Grip [→]
        surface.fillRect({drawX, area.y + 1, drawWidth, 1}, clipBg, ' ');
        surface.setCell(drawX, area.y + 1, Cell{0x2502, clipFg, clipBg, 0}); // Left border '│'

        // Micro Braille waveform preview
        static const char* kWave[4] = {"⠁⠃⠇⡇", "⢀⣀⡠⠤", "⠐⠒⠓⠕", "⠖⠗⠘⠙"};
        for (int wx = drawX + 1; wx < drawX + drawWidth - 3; wx += 4) {
            std::string wChunk = kWave[(wx / 4) % 4];
            surface.drawText(wx, area.y + 1, wChunk, clipFg, clipBg);
        }

        // Resize handle on the right edge
        int handleX = drawX + drawWidth - 3;
        if (handleX > drawX + 1) {
            surface.drawText(handleX, area.y + 1, "[→]", Color::White(), clipBg, static_cast<uint8_t>(TextAttr::Bold));
        }
        surface.setCell(drawX + drawWidth - 1, area.y + 1, Cell{0x2502, clipFg, clipBg, 0}); // Right border '│'
    }

    // Playhead vertical bar
    if (telemetry.isPlaying) {
        int currentBar = static_cast<int>(telemetry.currentStep / 16);
        float stepInBar = static_cast<float>(telemetry.currentStep % 16) / 16.0f;
        int playheadX = area.x + (currentBar - startBar) * colsPerBar_ + static_cast<int>(stepInBar * static_cast<float>(colsPerBar_));

        if (playheadX >= area.x && playheadX < area.x + area.width) {
            surface.setCell(playheadX, area.y, Cell{0x2502, Color::AcidAmber(), laneBg, static_cast<uint8_t>(TextAttr::Bold)});
            surface.setCell(playheadX, area.y + 1, Cell{0x2502, Color::AcidAmber(), laneBg, static_cast<uint8_t>(TextAttr::Bold)});
        }
    }
}

void ArrangerView::renderInspector(CellSurface& surface, const Rect& area) {
    if (area.width < 12 || area.height < 8) return;

    // Sidebar Frame
    surface.drawBox(area, Color::Gray(), Color::Black(), false, "INSPECTOR");

    const auto* track = getSelectedTrack();
    const auto* clip = getSelectedClip();

    int curY = area.y + 2;

    // Clip Section
    surface.drawText(area.x + 2, curY++, "CLIP PROPERTIES", Color::AcidAmber(), Color::Black(), static_cast<uint8_t>(TextAttr::Bold));
    if (clip) {
        surface.drawText(area.x + 2, curY++, "Name: " + clip->name, Color::White(), Color::Black());
        surface.drawText(area.x + 2, curY++, "Start: Bar " + std::to_string(clip->startBar + 1), Color::LightGray(), Color::Black());
        surface.drawText(area.x + 2, curY++, "Length: " + std::to_string(clip->lengthBars) + " Bars", Color::LightGray(), Color::Black());
        surface.drawText(area.x + 2, curY++, "Loop: [ON]", Color::NeonGreen(), Color::Black());
    } else {
        surface.drawText(area.x + 2, curY++, "No Clip Selected", Color::DarkGray(), Color::Black());
        curY += 3;
    }

    curY += 2;
    // Track Section
    surface.drawText(area.x + 2, curY++, "TRACK PROPERTIES", Color::CyberCyan(), Color::Black(), static_cast<uint8_t>(TextAttr::Bold));
    if (track) {
        surface.drawText(area.x + 2, curY++, "Track: " + track->name, Color::White(), Color::Black());
        char volStr[32];
        snprintf(volStr, sizeof(volStr), "Gain: %3.1f dB", (track->volume - 0.8f) * 20.0f);
        surface.drawText(area.x + 2, curY++, volStr, Color::LightGray(), Color::Black());

        char panStr[32];
        snprintf(panStr, sizeof(panStr), "Pan: %s (%2.0f%%)", getBraillePanGlyph(track->pan).c_str(), track->pan * 100.0f);
        surface.drawText(area.x + 2, curY++, panStr, Color::LightGray(), Color::Black());

        surface.drawText(area.x + 2, curY++, "Out: MasterBus", Color::Gray(), Color::Black());
    }
}

bool ArrangerView::handleMouseDown(int mouseX, int mouseY, int button) {
    if (button != 0) return false; // Only Left Click

    // 1. Check Track Headers
    if (cachedTrackHeadersArea_.contains(mouseX, mouseY)) {
        int relY = mouseY - (cachedTrackHeadersArea_.y + 1);
        int trackIdx = trackScrollY_ + (relY / 2);
        if (trackIdx >= 0 && trackIdx < static_cast<int>(tracks_.size())) {
            selectedTrack_ = trackIdx;
            selectedClip_ = -1; // Deselect clip
            auto& track = tracks_[static_cast<size_t>(trackIdx)];
            int rowInTrack = relY % 2;
            int relX = mouseX - cachedTrackHeadersArea_.x;

            if (rowInTrack == 0) {
                // Row 1: Mute/Solo/Arm buttons
                if (relX >= 10 && relX <= 12) {
                    track.muted = !track.muted;
                    return true;
                } else if (relX >= 14 && relX <= 16) {
                    track.solo = !track.solo;
                    return true;
                } else if (relX >= 18 && relX <= 20) {
                    track.armed = !track.armed;
                    return true;
                }
            } else {
                // Row 2: Volume Fader / Pan Dial
                if (relX >= 4 && relX <= 10) {
                    dragState_.mode = DragMode::ScrubVolume;
                    dragState_.trackIndex = trackIdx;
                    dragState_.startMouseX = mouseX;
                    dragState_.originalVal = track.volume;
                    return true;
                } else if (relX >= 16 && relX <= 18) {
                    dragState_.mode = DragMode::ScrubPan;
                    dragState_.trackIndex = trackIdx;
                    dragState_.startMouseX = mouseX;
                    dragState_.originalVal = track.pan;
                    return true;
                }
            }
            return true;
        }
    }

    // 2. Check Timeline Clips
    if (cachedTimelineArea_.contains(mouseX, mouseY)) {
        int relY = mouseY - (cachedTimelineArea_.y + 1);
        int trackIdx = trackScrollY_ + (relY / 2);
        if (trackIdx >= 0 && trackIdx < static_cast<int>(tracks_.size())) {
            selectedTrack_ = trackIdx;
            auto& track = tracks_[static_cast<size_t>(trackIdx)];
            int rowInTrack = relY % 2;

            for (size_t c = 0; c < track.clips.size(); ++c) {
                const auto& clip = track.clips[c];
                int clipStartCol = cachedTimelineArea_.x + (clip.startBar - timelineScrollX_) * colsPerBar_;
                int clipWidthCols = clip.lengthBars * colsPerBar_;

                if (mouseX >= clipStartCol && mouseX < clipStartCol + clipWidthCols) {
                    selectedClip_ = static_cast<int>(c);

                    if (rowInTrack == 0) {
                        // Top row -> Move clip
                        dragState_.mode = DragMode::MoveClip;
                        dragState_.trackIndex = trackIdx;
                        dragState_.clipIndex = static_cast<int>(c);
                        dragState_.startMouseX = mouseX;
                        dragState_.originalStartBar = clip.startBar;
                        return true;
                    } else {
                        // Bottom row: check if near right edge -> Resize clip
                        if (mouseX >= clipStartCol + clipWidthCols - 3) {
                            dragState_.mode = DragMode::ResizeClip;
                            dragState_.trackIndex = trackIdx;
                            dragState_.clipIndex = static_cast<int>(c);
                            dragState_.startMouseX = mouseX;
                            dragState_.originalLengthBars = clip.lengthBars;
                            return true;
                        } else {
                            // Clicked micro-waveform -> Select clip
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

bool ArrangerView::handleMouseDrag(int mouseX, int mouseY) {
    (void)mouseY;
    if (dragState_.mode == DragMode::None) return false;

    if (dragState_.mode == DragMode::MoveClip) {
        if (dragState_.trackIndex >= 0 && dragState_.trackIndex < static_cast<int>(tracks_.size())) {
            auto& track = tracks_[static_cast<size_t>(dragState_.trackIndex)];
            if (dragState_.clipIndex >= 0 && dragState_.clipIndex < static_cast<int>(track.clips.size())) {
                auto& clip = track.clips[static_cast<size_t>(dragState_.clipIndex)];
                int deltaBars = (mouseX - dragState_.startMouseX) / colsPerBar_;
                clip.startBar = std::max(0, dragState_.originalStartBar + deltaBars);
                return true;
            }
        }
    } else if (dragState_.mode == DragMode::ResizeClip) {
        if (dragState_.trackIndex >= 0 && dragState_.trackIndex < static_cast<int>(tracks_.size())) {
            auto& track = tracks_[static_cast<size_t>(dragState_.trackIndex)];
            if (dragState_.clipIndex >= 0 && dragState_.clipIndex < static_cast<int>(track.clips.size())) {
                auto& clip = track.clips[static_cast<size_t>(dragState_.clipIndex)];
                int deltaBars = (mouseX - dragState_.startMouseX) / colsPerBar_;
                clip.lengthBars = std::max(1, dragState_.originalLengthBars + deltaBars);
                return true;
            }
        }
    } else if (dragState_.mode == DragMode::ScrubVolume) {
        if (dragState_.trackIndex >= 0 && dragState_.trackIndex < static_cast<int>(tracks_.size())) {
            auto& track = tracks_[static_cast<size_t>(dragState_.trackIndex)];
            float delta = static_cast<float>(mouseX - dragState_.startMouseX) * 0.05f;
            track.volume = std::clamp(dragState_.originalVal + delta, 0.0f, 1.0f);
            return true;
        }
    } else if (dragState_.mode == DragMode::ScrubPan) {
        if (dragState_.trackIndex >= 0 && dragState_.trackIndex < static_cast<int>(tracks_.size())) {
            auto& track = tracks_[static_cast<size_t>(dragState_.trackIndex)];
            float delta = static_cast<float>(mouseX - dragState_.startMouseX) * 0.05f;
            track.pan = std::clamp(dragState_.originalVal + delta, 0.0f, 1.0f);
            return true;
        }
    }

    return false;
}

void ArrangerView::handleMouseUp() {
    dragState_.mode = DragMode::None;
}

void ArrangerView::handleScroll(int delta) {
    timelineScrollX_ = std::max(0, timelineScrollX_ + delta);
}

} // namespace eatsbits::tui
