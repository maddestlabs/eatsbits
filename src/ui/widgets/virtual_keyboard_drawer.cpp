#include "eatsbits/ui/widgets/virtual_keyboard_drawer.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <algorithm>

namespace eatsbits::ui {

VirtualKeyboardDrawer::VirtualKeyboardDrawer() {
    DrawerConfig cfg;
    cfg.edge = DockEdge::Bottom;
    cfg.tab.title = "KEYS / PADS";
    cfg.tab.tabWidth = kPullTabWidth;
    cfg.tab.tabHeight = kPullTabHeight;
    cfg.tab.preferredExtent = kPianoDrawerHeight + kPullTabHeight;
    cfg.tab.minExtent = kPianoDrawerHeight + kPullTabHeight;
    cfg.tab.maxExtent = kDrumDrawerHeight + kPullTabHeight;
    cfg.tab.resizable = false;
    cfg.tab.showHeaderBar = false;
    cfg.tab.hotkey = 'K';
    drawer_.setConfig(cfg);

    PianoKeyboardConfig pcfg;
    pcfg.orientation = KeyboardOrientation::Horizontal;
    pcfg.baseOctave = baseOctave_;
    pcfg.octavesCount = 3;
    pcfg.minPitch = (baseOctave_ + 1) * 12;
    pcfg.maxPitch = pcfg.minPitch + (pcfg.octavesCount * 12);
    keyboard_.setConfig(pcfg);
}

void VirtualKeyboardDrawer::setMode(KeyboardDrawerMode mode) noexcept {
    mode_ = mode;
    float h = (mode_ == KeyboardDrawerMode::DrumPads ? kDrumDrawerHeight : kPianoDrawerHeight) + kPullTabHeight;
    drawer_.setExtent(h);
}

void VirtualKeyboardDrawer::setBaseOctave(int oct) noexcept {
    baseOctave_ = std::clamp(oct, 1, 6);
    auto cfg = keyboard_.getConfig();
    cfg.baseOctave = baseOctave_;
    cfg.minPitch = (baseOctave_ + 1) * 12;
    cfg.maxPitch = cfg.minPitch + (cfg.octavesCount * 12);
    keyboard_.setConfig(cfg);
}

void VirtualKeyboardDrawer::update(float dt) noexcept {
    drawer_.update(dt);
    drumPadGrid_.update(dt);
}

void VirtualKeyboardDrawer::layout(float screenWidth, float bottomNavTopY) {
    drawer_.layout(Rect2D(0.0f, 0.0f, screenWidth, bottomNavTopY), 0.0f);

    float tabX = (screenWidth - kPullTabWidth) * 0.5f;
    float tabY = bottomNavTopY - kPullTabHeight;

    float activeDrawerH = (mode_ == KeyboardDrawerMode::DrumPads) ? kDrumDrawerHeight : kPianoDrawerHeight;

    if (drawer_.isExpanded()) {
        float drawerY = bottomNavTopY - activeDrawerH - kPullTabHeight;
        drawerBounds_ = Rect2D(0.0f, drawerY, screenWidth, activeDrawerH + kPullTabHeight);
        pullTabBounds_ = Rect2D(tabX, drawerY, kPullTabWidth, kPullTabHeight);

        // Control strip at top of drawer
        float ctrlY = drawerY + kPullTabHeight + 2.0f;

        // Mode switch buttons on left of control strip
        modeKeysBtnBounds_ = Rect2D(10.0f, ctrlY, 52.0f, 20.0f);
        modePadsBtnBounds_ = Rect2D(66.0f, ctrlY, 52.0f, 20.0f);

        if (mode_ == KeyboardDrawerMode::Piano) {
            // Octave controls
            octDownBounds_ = Rect2D(128.0f, ctrlY, 54.0f, 20.0f);
            octUpBounds_ = Rect2D(186.0f, ctrlY, 54.0f, 20.0f);

            // Keys area
            float keysY = ctrlY + 24.0f;
            float keysH = activeDrawerH - 28.0f;
            keysBounds_ = Rect2D(10.0f, keysY, screenWidth - 20.0f, keysH);
        } else {
            octDownBounds_ = Rect2D();
            octUpBounds_ = Rect2D();
            keysBounds_ = Rect2D();

            // Drum Pad grid bounds
            float padGridY = ctrlY + 2.0f;
            float padGridH = activeDrawerH - 6.0f;
            drumPadGrid_.layout(Rect2D(10.0f, padGridY, screenWidth - 20.0f, padGridH));
        }
    } else {
        drawerBounds_ = Rect2D(tabX, tabY, kPullTabWidth, kPullTabHeight);
        pullTabBounds_ = Rect2D(tabX, tabY, kPullTabWidth, kPullTabHeight);
        modeKeysBtnBounds_ = Rect2D();
        modePadsBtnBounds_ = Rect2D();
        octDownBounds_ = Rect2D();
        octUpBounds_ = Rect2D();
        keysBounds_ = Rect2D();
        drumPadGrid_.layout(Rect2D());
    }
}

void VirtualKeyboardDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme, audio::AudioEngine& engine) {
    (void)engine;

    const bool isExpanded = drawer_.isExpanded();

    // 1. Draw Pull Tab
    drawRoundedRect(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f,
                    theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.95f);
    drawRoundedRectOutline(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    // Instrument Icon & Label
    float iconW = 16.0f;
    float iconH = 14.0f;
    float iconX = pullTabBounds_.x + 8.0f;
    float iconY = pullTabBounds_.y + (pullTabBounds_.h - iconH) * 0.5f;

    Color iconCol = isExpanded ? theme.primaryAccent : theme.textSecondary;
    drawPianoIcon(r, iconX, iconY, iconW, iconH, iconCol);

    std::string tabLabel;
    if (mode_ == KeyboardDrawerMode::DrumPads) {
        tabLabel = isExpanded ? "MPC DRUM PADS" : "DRUM PADS (16)";
    } else {
        tabLabel = isExpanded ? "VIRTUAL PIANO" : ("PIANO (C" + std::to_string(baseOctave_) + ")");
    }

    float labelAreaW = pullTabBounds_.w - (iconX + iconW) - 24.0f;
    Color tabCol = isExpanded ? theme.primaryAccent : theme.textPrimary;
    drawCenteredText(r, tabLabel, iconX + iconW + 4.0f, pullTabBounds_.y, labelAreaW, pullTabBounds_.h, 10.0f, tabCol);

    // Active status mini-LED on right
    float ledX = pullTabBounds_.x + pullTabBounds_.w - 12.0f;
    float ledY = pullTabBounds_.y + (pullTabBounds_.h * 0.5f);
    if (isExpanded) {
        drawCircle(r, ledX, ledY, 3.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    } else {
        drawCircle(r, ledX, ledY, 2.5f, 0.35f, 0.35f, 0.35f, 0.8f);
    }

    if (!isExpanded) return;

    // 2. Draw Drawer Background Panel
    float activeDrawerH = (mode_ == KeyboardDrawerMode::DrumPads) ? kDrumDrawerHeight : kPianoDrawerHeight;
    drawRect(r, drawerBounds_.x, drawerBounds_.y + kPullTabHeight, drawerBounds_.w, activeDrawerH,
             theme.panelBackground.r * 0.85f, theme.panelBackground.g * 0.85f, theme.panelBackground.b * 0.85f, 0.98f);
    drawLine(r, drawerBounds_.x, drawerBounds_.y + kPullTabHeight, drawerBounds_.x + drawerBounds_.w, drawerBounds_.y + kPullTabHeight,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.5f);

    // 3. Mode Toggle Buttons: [ KEYS ] and [ PADS ]
    bool isPiano = (mode_ == KeyboardDrawerMode::Piano);
    drawButton(r, modeKeysBtnBounds_, "KEYS",
               isPiano ? theme.primaryAccent.withAlpha(0.25f) : theme.controlBackground,
               isPiano ? theme.primaryAccent : theme.borderSubtle,
               isPiano ? theme.primaryAccent : theme.textMuted,
               9.0f, 3.0f, 1.0f);

    drawButton(r, modePadsBtnBounds_, "PADS",
               !isPiano ? theme.secondaryAccent.withAlpha(0.25f) : theme.controlBackground,
               !isPiano ? theme.secondaryAccent : theme.borderSubtle,
               !isPiano ? theme.secondaryAccent : theme.textMuted,
               9.0f, 3.0f, 1.0f);

    if (mode_ == KeyboardDrawerMode::DrumPads) {
        // Delegate rendering to DrumPadGridWidget
        drumPadGrid_.render(r, theme);
        return;
    }

    // 4. Piano Mode: Octave Controls & Range Readout
    drawButton(r, octDownBounds_, "< OCT",
               theme.panelHeader, theme.borderSubtle, theme.textPrimary, 10.0f, 3.0f, 1.0f);
    drawButton(r, octUpBounds_, "OCT >",
               theme.panelHeader, theme.borderSubtle, theme.textPrimary, 10.0f, 3.0f, 1.0f);

    std::string rangeStr = "RANGE: C" + std::to_string(baseOctave_) + " - C" + std::to_string(baseOctave_ + 3) +
                           "  (TOUCH & DRAG GLISSANDO ACTIVE)";
    float rangeY = octUpBounds_.y + (octUpBounds_.h - 10.0f) * 0.5f;
    drawText(r, rangeStr, octUpBounds_.x + octUpBounds_.w + 16.0f, rangeY, 10.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);

    // 5. Draw Horizontal Piano Keys (Unified PianoKeyboard)
    keyboard_.render(r, keysBounds_, theme, 0.0f);
}

bool VirtualKeyboardDrawer::handlePointer(const PointerEvent& ev, audio::AudioEngine& engine) {
    // 1. Pull Tab Hit Testing
    if (pullTabBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            toggleExpanded();
            if (!drawer_.isExpanded()) {
                releaseAllNotes(engine);
                drumPadGrid_.releaseAllPads();
            }
            return true;
        }
    }

    if (!drawer_.isExpanded()) return false;

    // 2. Control Strip: Mode Buttons [ KEYS | PADS ]
    if (ev.action == PointerAction::Down) {
        if (modeKeysBtnBounds_.contains(ev.x, ev.y)) {
            releaseAllNotes(engine);
            drumPadGrid_.releaseAllPads();
            setMode(KeyboardDrawerMode::Piano);
            return true;
        }
        if (modePadsBtnBounds_.contains(ev.x, ev.y)) {
            releaseAllNotes(engine);
            drumPadGrid_.releaseAllPads();
            setMode(KeyboardDrawerMode::DrumPads);
            return true;
        }
    }

    // 3. Drum Pad Mode Event Routing
    if (mode_ == KeyboardDrawerMode::DrumPads) {
        // Wire callbacks to audio engine
        drumPadGrid_.onPadTrigger = [this, &engine](uint8_t note, float velocity) {
            triggerNoteOn(static_cast<int>(note), velocity, engine);
        };
        drumPadGrid_.onPadRelease = [this, &engine](uint8_t note) {
            triggerNoteOff(static_cast<int>(note), engine);
        };

        if (drumPadGrid_.handlePointer(ev)) {
            return true;
        }
        if (drawerBounds_.contains(ev.x, ev.y)) {
            return true;
        }
        return false;
    }

    // 4. Piano Mode: Octave Buttons
    if (ev.action == PointerAction::Down) {
        if (octDownBounds_.contains(ev.x, ev.y)) {
            releaseAllNotes(engine);
            setBaseOctave(baseOctave_ - 1);
            return true;
        }
        if (octUpBounds_.contains(ev.x, ev.y)) {
            releaseAllNotes(engine);
            setBaseOctave(baseOctave_ + 1);
            return true;
        }
    }

    // 5. Piano Keys Interaction & Multi-Touch Polyphonic Glissando
    if (keysBounds_.contains(ev.x, ev.y) || keyboard_.hasActiveTouches()) {
        if (keyboard_.handlePointer(ev, keysBounds_, 0.0f, &engine, activeTrackIndex_)) {
            return true;
        }
    }

    if (drawerBounds_.contains(ev.x, ev.y)) {
        return true;
    }

    return false;
}

void VirtualKeyboardDrawer::triggerNoteOn(int pitch, float velocity, audio::AudioEngine& engine) {
    keyboard_.triggerNoteOn(pitch, velocity, &engine, activeTrackIndex_);
}

void VirtualKeyboardDrawer::triggerNoteOff(int pitch, audio::AudioEngine& engine) {
    keyboard_.triggerNoteOff(pitch, &engine, activeTrackIndex_);
}

void VirtualKeyboardDrawer::releaseAllNotes(audio::AudioEngine& engine) {
    keyboard_.releaseAllNotes(&engine, activeTrackIndex_);
}

} // namespace eatsbits::ui
