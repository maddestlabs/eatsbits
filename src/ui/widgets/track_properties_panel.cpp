#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "eatsbits/ui/widgets/track_properties_panel.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/icon_registry.hpp"
#include "eatsbits/ui/gui_window.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <string_view>
#include "eatsbits/presenter/track_properties_presenter.hpp"

namespace {
inline bool stringEqualsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](char c1, char c2) noexcept {
            return std::tolower(static_cast<unsigned char>(c1)) ==
                   std::tolower(static_cast<unsigned char>(c2));
        });
}

inline eatsbits::ui::ValueEditRequest toValueEditRequest(const eatsbits::presenter::ValueEditConfig& cfg,
                                                        const eatsbits::ui::Color& accent) {
    eatsbits::ui::ValueEditRequest req;
    req.title = cfg.title;
    req.paramName = cfg.paramName;
    req.isTextMode = cfg.isTextMode;
    req.initialText = cfg.initialText;
    req.currentValue = cfg.currentValue;
    req.minValue = cfg.minValue;
    req.maxValue = cfg.maxValue;
    req.defaultValue = cfg.defaultValue;
    req.hasDefault = cfg.hasDefault;
    req.isInteger = cfg.isInteger;
    req.allowPercentage = cfg.allowPercentage;
    req.unit = cfg.unit;
    req.accentColor = accent;
    req.onCommit = cfg.onCommit;
    req.onCommitText = cfg.onCommitText;
    return req;
}
} // namespace

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace eatsbits::ui {

static const float kQuickPalette[8][3] = {
    {0.13f, 0.96f, 0.91f}, // Neon Cyan
    {1.0f,  0.55f, 0.0f }, // Neon Amber
    {0.0f,  1.0f,  0.40f}, // Acid Green
    {1.0f,  0.0f,  0.48f}, // Hot Pink
    {0.74f, 0.0f,  1.0f }, // Electric Purple
    {1.0f,  0.20f, 0.20f}, // Crimson Red
    {1.0f,  0.90f, 0.0f }, // Gold Yellow
    {0.20f, 0.60f, 1.0f }  // Sky Blue
};

static float computeMidiFxRackHeight(const TrackPropertiesDrawerData& data) {
    float h = 42.0f; // Header
    if (data.midiFx.empty()) {
        h += 38.0f;
    } else {
        for (const auto& fx : data.midiFx) {
            h += (fx.isExpanded ? 180.0f : 42.0f) + 8.0f;
        }
    }
    return h + 4.0f;
}

static float computeAudioFxRackHeight(const TrackPropertiesDrawerData& data) {
    float h = 42.0f; // Header
    if (data.audioFx.empty()) {
        h += 38.0f;
    } else {
        for (const auto& fx : data.audioFx) {
            h += (fx.isExpanded ? 180.0f : 42.0f) + 8.0f;
        }
    }
    return h + 4.0f;
}

static void drawModernPillSwitch(BatchRenderer2D& r, float x, float y, bool enabled, const Color& activeColor) {
    float w = 15.0f;
    float h = 26.0f;
    float rad = 7.5f;

    // Outer dark pill housing shadow
    drawRoundedRect(r, x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, rad + 1.0f, 0.05f, 0.06f, 0.09f, 0.6f);

    // Pill body
    if (enabled) {
        drawRoundedRect(r, x, y, w, h, rad, activeColor.r, activeColor.g, activeColor.b, 1.0f);
        drawRoundedRectOutline(r, x, y, w, h, rad, activeColor.r * 1.2f, activeColor.g * 1.2f, activeColor.b * 1.2f, 0.9f, 1.0f);
    } else {
        drawRoundedRect(r, x, y, w, h, rad, 0.16f, 0.18f, 0.22f, 1.0f);
        drawRoundedRectOutline(r, x, y, w, h, rad, 0.25f, 0.28f, 0.35f, 0.8f, 1.0f);
    }

    // Circular metallic slider thumb
    float thumbR = 5.8f;
    float thumbCx = x + 7.5f;
    float thumbCy = enabled ? (y + 7.5f) : (y + h - 7.5f);

    // Drop shadow
    drawCircle(r, thumbCx, thumbCy + 1.0f, thumbR + 0.8f, 0.04f, 0.05f, 0.07f, 0.65f);

    // 3-stop metallic radial gradient
    drawCircleRadial3StopGradient(r, thumbCx, thumbCy, thumbR,
                                  Color(0.98f, 0.98f, 1.0f, 1.0f),
                                  Color(0.75f, 0.78f, 0.84f, 1.0f),
                                  Color(0.38f, 0.40f, 0.46f, 1.0f),
                                  -1.5f, -1.5f, 0.50f, 20);

    // Metallic rim outline
    drawCircleOutline(r, thumbCx, thumbCy, thumbR, 0.28f, 0.30f, 0.36f, 0.95f, 1.0f);
}

TrackPropertiesPanel::TrackPropertiesPanel() {
    // Configure embedded PluginSearchDialog
    pluginDialog_.onPluginSelected = [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t trackIndex) {
        if (mode == PluginDialogMode::AddInstrument) {
            if (onChangeInstrument) onChangeInstrument(trackIndex);
        } else if (mode == PluginDialogMode::AddMidiFx) {
            if (onAddMidiFx) onAddMidiFx(trackIndex);
        } else if (mode == PluginDialogMode::AddAudioFx) {
            if (onAddAudioFx) onAddAudioFx(trackIndex);
        }
    };
}

void TrackPropertiesPanel::syncGuiPanelFromTrackData(const TrackPropertiesDrawerData& data) {
    guiPanel_.title = data.presetTitle.empty() ? data.instrument : data.presetTitle;
    guiPanel_.subtitle = data.presetSubtitle.empty() ? (data.trackName + " • " + data.instrument) : data.presetSubtitle;
    guiPanel_.accentColor = Color(data.r, data.g, data.b, 1.0f);

    const std::string& eng = data.instrumentEngine;
    bool is303 = (eng == "tb303" || data.instrument.find("303") != std::string::npos ||
                  guiPanel_.title.find("303") != std::string::npos);

    auto findKnobVal = [&](const std::string& p1, const std::string& p2, float def) -> float {
        for (const auto& k : data.knobs) {
            if (stringEqualsIgnoreCase(k.name, p1) || stringEqualsIgnoreCase(k.label, p1) ||
                (!p2.empty() && (stringEqualsIgnoreCase(k.name, p2) || stringEqualsIgnoreCase(k.label, p2)))) {
                return k.value;
            }
        }
        return def;
    };

    if (is303) {
        guiPanel_.chassisStyle = GuiChassisStyle::MinimalWhite;
        guiPanel_.woodCheeks = false;
        guiPanel_.hideHeader = true;
        guiPanel_.cornerRadius = 6.0f;

        if (guiPanel_.rows.size() != 2 || guiPanel_.rows[0].widgets.size() != 8) {
            guiPanel_.rows.clear();

            GuiRowDef r1;
            r1.widgets.push_back({"w_waveform", GuiWidgetType::Knob, "WAVEFORM", "waveform", GuiKnobStyle::Tb303SelectorSilver, 52.0f, findKnobVal("waveform", "wave", 0.0f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"div1", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r1.widgets.push_back({"w_pitch", GuiWidgetType::Knob, "PITCH", "pitch", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("pitch", "tuning", 0.5f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"w_cutoff", GuiWidgetType::Knob, "CUTOFF", "cutoff", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("cutoff", "", 0.65f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"w_res", GuiWidgetType::Knob, "RESONANCE", "resonance", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("resonance", "reson", 0.75f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"w_env", GuiWidgetType::Knob, "ENV MOD", "envMod", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("envMod", "env", 0.60f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"w_decay", GuiWidgetType::Knob, "DECAY", "decay", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("decay", "", 0.45f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"w_accent", GuiWidgetType::Knob, "ACCENT", "accent", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("accent", "", 0.75f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            guiPanel_.rows.push_back(r1);

            GuiRowDef r2;
            r2.widgets.push_back({"w_octave", GuiWidgetType::Knob, "OCTAVE", "octave", GuiKnobStyle::Tb303SelectorBlack, 52.0f, findKnobVal("octave", "", 0.5f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r2.widgets.push_back({"div2", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r2.widgets.push_back({"w_subosc", GuiWidgetType::ToggleSwitch, "SUB OSC", "subOsc", GuiKnobStyle::Standard, 48.0f, findKnobVal("subOsc", "subWaveform", 0.0f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r2.widgets.push_back({"w_subvol", GuiWidgetType::Knob, "SUB VOL", "subVol", GuiKnobStyle::MiniPotCream, 48.0f, findKnobVal("subVol", "subVolume", 0.35f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r2.widgets.push_back({"div3", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r2.widgets.push_back({"w_glide", GuiWidgetType::Knob, "GLIDE CURVE", "glideCurve", GuiKnobStyle::MiniPotCream, 48.0f, findKnobVal("glideCurve", "glide", 0.40f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r2.widgets.push_back({"div4", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r2.widgets.push_back({"w_drive", GuiWidgetType::Knob, "DRIVE", "drive", GuiKnobStyle::MiniPotCream, 48.0f, findKnobVal("drive", "overdrive", 0.25f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            guiPanel_.rows.push_back(r2);
        } else {
            for (auto& row : guiPanel_.rows) {
                for (auto& w : row.widgets) {
                    if (w.type == GuiWidgetType::Divider) continue;
                    w.currentVal = findKnobVal(w.param, "", w.currentVal);
                }
            }
        }
        return;
    }

    guiPanel_.hideHeader = false;
    guiPanel_.woodCheeks = true;
    guiPanel_.cornerRadius = 6.0f;
    if (eng == "tr808" || eng == "tr909") {
        guiPanel_.chassisStyle = GuiChassisStyle::Grunge;
    } else if (eng == "dx7") {
        guiPanel_.chassisStyle = GuiChassisStyle::DarkChassis;
    } else if (eng == "piano" || eng == "piano_physical") {
        guiPanel_.chassisStyle = GuiChassisStyle::Walnut;
    } else if (eng == "snes") {
        guiPanel_.chassisStyle = GuiChassisStyle::Snes;
        guiPanel_.woodCheeks = false;
    } else if (eng == "c64") {
        guiPanel_.chassisStyle = GuiChassisStyle::PcbGreen;
        guiPanel_.woodCheeks = false;
    } else if (eng == "convolver") {
        guiPanel_.chassisStyle = GuiChassisStyle::BrushedSteel;
    } else {
        guiPanel_.chassisStyle = GuiChassisStyle::DarkChassis;
    }

    size_t numKnobs = data.knobs.size();
    if (numKnobs == 0) return;
    size_t displayKnobs = std::min(numKnobs, size_t{6});

    if (guiPanel_.rows.size() != 1 || guiPanel_.rows[0].widgets.size() != displayKnobs) {
        guiPanel_.rows.clear();
        GuiRowDef r1;
        for (size_t i = 0; i < displayKnobs; ++i) {
            const auto& k = data.knobs[i];
            GuiKnobStyle kStyle = GuiKnobStyle::Standard;
            if (eng == "tr808" || eng == "tr909") {
                if (i == 0) kStyle = GuiKnobStyle::BakeliteSkirt;
                else if (i == 1) kStyle = GuiKnobStyle::CreamFluted;
                else if (i == 2) kStyle = GuiKnobStyle::AnodizedKnurled;
                else if (i == 3) kStyle = GuiKnobStyle::TwoToneStepped;
                else if (i == 4) kStyle = GuiKnobStyle::Tb303Halo;
                else kStyle = GuiKnobStyle::Standard;
            } else if (eng == "dx7") {
                if (i == 0) kStyle = GuiKnobStyle::AnodizedKnurled;
                else if (i == 1) kStyle = GuiKnobStyle::TwoToneStepped;
                else if (i == 2) kStyle = GuiKnobStyle::BakeliteSkirt;
                else if (i == 3) kStyle = GuiKnobStyle::CreamFluted;
                else if (i == 4) kStyle = GuiKnobStyle::Standard;
                else kStyle = GuiKnobStyle::Tb303Halo;
            } else {
                kStyle = static_cast<GuiKnobStyle>(i % 5);
            }

            GuiWidgetDef w;
            w.id = "w_" + k.name;
            w.type = GuiWidgetType::Knob;
            w.label = k.label;
            w.param = k.name;
            w.knobStyle = kStyle;
            w.size = 52.0f;
            w.currentVal = k.value;
            w.minVal = 0.0f;
            w.maxVal = 1.0f;
            w.unit = "";
            w.accentColor = guiPanel_.accentColor;
            r1.widgets.push_back(w);
        }
        guiPanel_.rows.push_back(r1);
    } else {
        for (size_t i = 0; i < displayKnobs && i < guiPanel_.rows[0].widgets.size(); ++i) {
            guiPanel_.rows[0].widgets[i].currentVal = data.knobs[i].value;
            guiPanel_.rows[0].widgets[i].label = data.knobs[i].label;
            guiPanel_.rows[0].widgets[i].param = data.knobs[i].name;
            guiPanel_.rows[0].widgets[i].accentColor = guiPanel_.accentColor;
        }
    }
}

void TrackPropertiesPanel::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;

    float padding = (bounds_.w >= 560.0f) ? 14.0f : 8.0f;
    float contentW = bounds_.w - (padding * 2.0f);
    float startY = bounds_.y + 4.0f;
    float curY = startY - scrollY_;

    bool isWide = (bounds_.w >= 560.0f);

    // 0. Optional Top Track Ribbon
    if (showTrackRibbon_) {
        float ribbonH = 34.0f;
        ribbonBounds_ = Rect2D(bounds_.x + padding, curY, contentW, ribbonH);
        curY += ribbonH + 8.0f;
    } else {
        ribbonBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // 1. Track Identity Header Card (reclaimed vertical space by removing subtitle)
    float headerH = isWide ? 42.0f : 38.0f;
    headerCardBounds_ = Rect2D(bounds_.x + padding, curY, contentW, headerH);
    curY += headerH + 6.0f;

    // 2. Track Color Swatches Card (directly below Track Title)
    float colorH = 36.0f;
    colorCardBounds_ = Rect2D(bounds_.x + padding, curY, contentW, colorH);
    curY += colorH + 8.0f;

    // 3. Channel Mixer Quick Controls Card (Volume & Pan removed from sidebar)
    mixerCardBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);

    // 4. 3-Band Parametric EQ Card (available when mixer mode is active)
    float eqH = 138.0f;
    eqCardBounds_ = Rect2D(bounds_.x + padding, curY, contentW, eqH);
    // Note: only advances curY during render if eq is active

    // 5. Dynamic Instrument Hardware Faceplate Card
    float faceplateH = isWide ? 280.0f : 240.0f;
    faceplateBounds_ = Rect2D(bounds_.x + padding, curY, contentW, faceplateH);
    curY += faceplateH + 8.0f;

    // 6. Harmonic Chord Track Follow Settings Card
    float chordFollowH = 88.0f;
    chordFollowBounds_ = Rect2D(bounds_.x + padding, curY, contentW, chordFollowH);
    curY += chordFollowH + 8.0f;

    // 7. MIDI FX Pipeline Rack Card
    float midiFxH = isWide ? 120.0f : 106.0f;
    midiFxBounds_ = Rect2D(bounds_.x + padding, curY, contentW, midiFxH);
    curY += midiFxH + 8.0f;

    // 8. Audio FX Insert Rack Card
    float audioFxH = isWide ? 120.0f : 106.0f;
    audioFxBounds_ = Rect2D(bounds_.x + padding, curY, contentW, audioFxH);
    curY += audioFxH + 8.0f;

    colorPaletteBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);

    curY += 12.0f;
    totalContentHeight_ = (curY + scrollY_) - startY;
    needScrollbar_ = (totalContentHeight_ > bounds_.h);
    scrollbarBounds_ = Rect2D(bounds_.x + bounds_.w - 7.0f, bounds_.y + 2.0f, 5.0f, bounds_.h - 4.0f);

    // Layout embedded plugin search dialog
    pluginDialog_.layout(ctx.logicalWidth > 0.0f ? ctx.logicalWidth : 1280.0f,
                         ctx.logicalHeight > 0.0f ? ctx.logicalHeight : 800.0f);
}

void TrackPropertiesPanel::update(float dt) noexcept {
    if (scroller_.isGliding()) {
        float dx = 0.0f, dy = 0.0f;
        scroller_.step(dt, dx, dy);
        float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
        scrollY_ = std::clamp(scrollY_ - dy, 0.0f, maxScroll);
        if (onScrollChanged) onScrollChanged(scrollY_);
    }
}

void TrackPropertiesPanel::render(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                                  float mouseX, float mouseY) {
    if (scroller_.isGliding()) {
        update(1.0f / 60.0f);
    }
    if (isLongPressActive_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - touchDownTimePoint_).count();
        if (elapsedMs >= 450) {
            isLongPressActive_ = false;
            dragMode_ = DragMode::None;
            activeKnobIndex_ = -1;
            draggingRow_ = -1;
            draggingWidget_ = -1;
            openValueEditForHit(longPressHit_, data, longPressOpenValueEdit_);
        }
    }
    data.syncKnobsIfEmpty();

    float padding = (bounds_.w >= 560.0f) ? 14.0f : 8.0f;
    float contentX = bounds_.x + padding;
    float contentW = bounds_.w - (padding * 2.0f);
    bool isWide = (bounds_.w >= 560.0f);

    // Master Console Mode (Mixer Master selected)
    if (data.isMasterSelected || data.tab == TrackPropertiesTab::Master) {
        renderMasterSection(r, theme, data, contentX, bounds_.y + 8.0f - scrollY_, contentW, mouseX, mouseY);
        if (needScrollbar_) renderScrollbar(r, theme);
        if (!isInsideDrawer_ && mouseX >= 0.0f && mouseY >= 0.0f) {
            std::string tip = getTooltip(mouseX, mouseY, data);
            if (!tip.empty()) drawTooltipBadge(r, tip, mouseX, mouseY - 14.0f, theme, false, bounds_.x + bounds_.w);
        }
        return;
    }

    // Clip Mode (Arranger Clip selected)
    if (data.tab == TrackPropertiesTab::Clip) {
        renderClipSection(r, theme, data, contentX, bounds_.y + 8.0f - scrollY_, contentW, mouseX, mouseY);
        if (needScrollbar_) renderScrollbar(r, theme);
        if (!isInsideDrawer_ && mouseX >= 0.0f && mouseY >= 0.0f) {
            std::string tip = getTooltip(mouseX, mouseY, data);
            if (!tip.empty()) drawTooltipBadge(r, tip, mouseX, mouseY - 14.0f, theme, false, bounds_.x + bounds_.w);
        }
        return;
    }

    // Top Track Navigation Ribbon
    if (showTrackRibbon_) {
        renderTrackSelectorRibbon(r, theme, data);
    }

    float curCardY = bounds_.y + 8.0f - scrollY_;
    if (showTrackRibbon_) {
        renderTrackSelectorRibbon(r, theme, data);
        curCardY += ribbonBounds_.h + 6.0f;
    }

    float headH = isWide ? 40.0f : 36.0f;
    headerCardBounds_ = Rect2D(contentX, curCardY, contentW, headH);
    renderHeaderCard(r, theme, data, headerCardBounds_.x, headerCardBounds_.y, headerCardBounds_.w, isWide, mouseX, mouseY);
    curCardY += headH + 8.0f;

    colorCardBounds_ = Rect2D(contentX, curCardY, contentW, 36.0f);
    renderColorPalette(r, theme, data, colorCardBounds_.x, colorCardBounds_.y, colorCardBounds_.w);
    curCardY += 36.0f + 8.0f;

    if (data.isMixerMode) {
        eqCardBounds_ = Rect2D(contentX, curCardY, contentW, 138.0f);
        renderEqCard(r, theme, data, eqCardBounds_.x, eqCardBounds_.y, eqCardBounds_.w);
        curCardY += 138.0f + 8.0f;
    }

    float faceplateH = data.instrumentExpanded ? (isWide ? 280.0f : 240.0f) : 38.0f;
    faceplateBounds_ = Rect2D(contentX, curCardY, contentW, faceplateH);
    renderFaceplateCard(r, theme, data, faceplateBounds_.x, faceplateBounds_.y, faceplateBounds_.w, isWide);
    curCardY += faceplateH + 8.0f;

    chordFollowBounds_ = Rect2D(contentX, curCardY, contentW, 88.0f);
    renderChordFollowCard(r, theme, data, chordFollowBounds_.x, chordFollowBounds_.y, chordFollowBounds_.w);
    curCardY += 88.0f + 8.0f;

    float mfxH = computeMidiFxRackHeight(data);
    midiFxBounds_ = Rect2D(contentX, curCardY, contentW, mfxH);
    renderMidiFxCard(r, theme, data, midiFxBounds_.x, midiFxBounds_.y, midiFxBounds_.w, isWide);
    curCardY += mfxH + 8.0f;

    float afxH = computeAudioFxRackHeight(data);
    audioFxBounds_ = Rect2D(contentX, curCardY, contentW, afxH);
    renderAudioFxCard(r, theme, data, audioFxBounds_.x, audioFxBounds_.y, audioFxBounds_.w, isWide);
    curCardY += afxH + 12.0f;

    totalContentHeight_ = (curCardY + scrollY_) - (bounds_.y + 8.0f);
    needScrollbar_ = (totalContentHeight_ > bounds_.h);

    if (needScrollbar_) {
        renderScrollbar(r, theme);
    }

    if (!isInsideDrawer_ && mouseX >= 0.0f && mouseY >= 0.0f) {
        std::string tip = getTooltip(mouseX, mouseY, data);
        if (!tip.empty()) {
            drawTooltipBadge(r, tip, mouseX, mouseY - 14.0f, theme, false, bounds_.x + bounds_.w);
        }
    }
}

void TrackPropertiesPanel::renderTrackSelectorRibbon(BatchRenderer2D& r, const ThemeTokens& theme,
                                                     const TrackPropertiesDrawerData& data) {
    if (ribbonBounds_.y + ribbonBounds_.h < bounds_.y || ribbonBounds_.y > bounds_.y + bounds_.h) return;

    size_t trackCount = data.allTrackNames.empty() ? 5 : data.allTrackNames.size();
    const float startX = ribbonBounds_.x + 4.0f;
    const float tabW = std::clamp((ribbonBounds_.w - 40.0f) / static_cast<float>(trackCount), 90.0f, 130.0f);
    const float tabGap = 8.0f;
    const float tabH = ribbonBounds_.h - 4.0f;

    for (size_t i = 0; i < trackCount; ++i) {
        float tx = startX + static_cast<float>(i) * (tabW + tabGap);
        if (tx + tabW > ribbonBounds_.x + ribbonBounds_.w) break;

        bool isActive = (i == data.trackIndex);
        std::string tName = (i < data.allTrackNames.size()) ? data.allTrackNames[i] : ("Track " + std::to_string(i + 1));
        Color tCol = (i < data.allTrackColors.size()) ? data.allTrackColors[i] : Color(data.r, data.g, data.b);

        drawRoundedRect(r, tx, ribbonBounds_.y + 2.0f, tabW, tabH, 4.0f,
                        isActive ? (theme.primaryAccent * 0.28f) : theme.controlBackground,
                        isActive ? 0.95f : 0.70f);
        drawRoundedRectOutline(r, tx, ribbonBounds_.y + 2.0f, tabW, tabH, 4.0f,
                               isActive ? theme.primaryAccent : theme.borderSubtle,
                               isActive ? 0.95f : 0.40f, isActive ? 1.5f : 1.0f);

        // Accent indicator dot
        drawCircle(r, tx + 10.0f, ribbonBounds_.y + 2.0f + tabH * 0.5f, 3.5f,
                   tCol.r, tCol.g, tCol.b, isActive ? 1.0f : 0.6f);

        drawText(r, tName, tx + 20.0f, ribbonBounds_.y + 11.0f, 10.5f,
                 isActive ? theme.primaryAccent.r : theme.textPrimary.r,
                 isActive ? theme.primaryAccent.g : theme.textPrimary.g,
                 isActive ? theme.primaryAccent.b : theme.textPrimary.b, 1.0f);
    }
}

void TrackPropertiesPanel::renderHeaderCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                            TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide,
                                            float mouseX, float mouseY) {
    float h = isWide ? 40.0f : 36.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // Track icon button prior to title (clickable to open track icon dialog)
    float iconBtnX = cx + 8.0f;
    float iconBtnW = 24.0f;
    float iconBtnH = 24.0f;
    float iconBtnY = cy + (h - iconBtnH) * 0.5f;
    bool iconHov = (mouseX >= iconBtnX && mouseX <= iconBtnX + iconBtnW &&
                    mouseY >= iconBtnY && mouseY <= iconBtnY + iconBtnH);
    if (iconHov) {
        drawRoundedRect(r, iconBtnX, iconBtnY, iconBtnW, iconBtnH, 4.0f, 1.0f, 1.0f, 1.0f, 0.08f);
    }
    float iconSize = 16.0f;
    float iconX = iconBtnX + (iconBtnW - iconSize) * 0.5f;
    float iconY = iconBtnY + (iconBtnH - iconSize) * 0.5f;
    IconRegistry::instance().renderIcon(r, data.iconRef.empty() ? "preset:inst_synth" : data.iconRef,
                                        iconX, iconY, iconSize, Color(data.r, data.g, data.b, 1.0f));

    // Right-aligned Edit icon (pencil icon, same size as title text 10.0f, NO border around it)
    float editSize = 10.0f;
    float editBtnW = 20.0f;
    float editBtnH = 20.0f;
    float editBtnX = cx + cw - 12.0f - editBtnW;
    float editBtnY = cy + (h - editBtnH) * 0.5f;
    bool editHov = (mouseX >= editBtnX - 4.0f && mouseX <= editBtnX + editBtnW + 4.0f &&
                    mouseY >= cy && mouseY <= cy + h);
    Color editColor = editHov ? theme.primaryAccent : theme.textSecondary;
    drawIconEdit(r, editBtnX + (editBtnW - editSize) * 0.5f, editBtnY + (editBtnH - editSize) * 0.5f, editSize, editColor);

    // Track Title (vertically centered on single line, size 10.0f matching clip title text)
    float titleX = iconBtnX + iconBtnW + 6.0f;
    float titleY = cy + (h - 10.0f) * 0.5f;
    drawText(r, data.trackName, titleX, titleY, 10.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
}

void TrackPropertiesPanel::renderMixerControlsCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                                   TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    float h = isWide ? 68.0f : 60.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    float sY = cy + (isWide ? 25.0f : 21.0f);
    float sH = isWide ? 18.0f : 16.0f;

    // 1. Expanded Volume Slider (takes majority of width)
    float vLabelX = cx + 14.0f;
    float vTrackX = vLabelX + (isWide ? 38.0f : 32.0f);
    float panAreaW = isWide ? 85.0f : 74.0f;
    float vTrackW = (cx + cw - panAreaW - 55.0f) - vTrackX;
    if (vTrackW < 50.0f) vTrackW = 50.0f;

    drawText(r, "VOL", vLabelX, sY + 3.0f, 10.5f, theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    drawRoundedRect(r, vTrackX, sY, vTrackW, sH, 3.0f, 0.06f, 0.07f, 0.09f, 1.0f);
    drawRoundedRectOutline(r, vTrackX, sY, vTrackW, sH, 3.0f, 0.20f, 0.22f, 0.28f, 1.0f, 1.0f);

    float normVol = std::clamp(data.volume / 1.5f, 0.0f, 1.0f);
    if (normVol > 0.01f) {
        drawRoundedRect(r, vTrackX + 2.0f, sY + 2.0f, (vTrackW - 4.0f) * normVol, sH - 4.0f, 2.5f,
                        data.r * 0.85f, data.g * 0.85f, data.b * 0.85f, 0.95f);
    }
    float volThumbX = vTrackX + normVol * vTrackW;
    drawRoundedRect(r, volThumbX - 4.0f, sY - 2.0f, 8.0f, sH + 4.0f, 2.0f, 0.80f, 0.82f, 0.88f, 1.0f);
    int volPct = static_cast<int>(std::round(data.volume * 100.0f));
    drawText(r, std::to_string(volPct) + "%", vTrackX + vTrackW + 8.0f, sY + 3.0f, 10.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // 2. Rotary Pan Knob (right side)
    float panKcx = cx + cw - (isWide ? 40.0f : 34.0f);
    float panKcy = cy + (h * 0.5f) - (isWide ? 2.0f : 1.0f);
    float kPanRadius = isWide ? 13.5f : 12.0f;
    bool isDraggingPan = (dragMode_ == DragMode::PanKnob);

    drawCircle(r, panKcx, panKcy + 2.0f, kPanRadius + 1.0f, 0.05f, 0.06f, 0.08f, 0.35f);
    drawCircle(r, panKcx, panKcy, kPanRadius, 0.16f, 0.18f, 0.22f, 1.0f);
    drawCircleOutline(r, panKcx, panKcy, kPanRadius,
                      isDraggingPan ? theme.highlight.r : theme.borderSubtle.r,
                      isDraggingPan ? theme.highlight.g : theme.borderSubtle.g,
                      isDraggingPan ? theme.highlight.b : theme.borderSubtle.b,
                      isDraggingPan ? 1.0f : 0.7f, 1.2f);

    // Center Detent Tick Mark at 12 o'clock
    drawLine(r, panKcx, panKcy - kPanRadius - 2.0f, panKcx, panKcy - kPanRadius + 2.0f,
             0.50f, 0.55f, 0.65f, 0.8f, 1.0f);

    // Rotary Needle
    constexpr float kMaxPanAngle = 2.35619449f;  // +135 deg
    float curPanAngle = data.pan * kMaxPanAngle; // 0.0 = straight UP (12 o'clock)
    float needleX = panKcx + std::sin(curPanAngle) * (kPanRadius - 3.0f);
    float needleY = panKcy - std::cos(curPanAngle) * (kPanRadius - 3.0f);
    drawLine(r, panKcx, panKcy, needleX, needleY,
             isDraggingPan ? theme.highlight.r : (std::abs(data.pan) < 0.04f ? 0.70f : data.r),
             isDraggingPan ? theme.highlight.g : (std::abs(data.pan) < 0.04f ? 0.75f : data.g),
             isDraggingPan ? theme.highlight.b : (std::abs(data.pan) < 0.04f ? 0.85f : data.b),
             1.0f, 2.0f);
    drawCircle(r, panKcx, panKcy, 3.0f, 0.25f, 0.28f, 0.32f, 1.0f);

    // PAN Label above knob
    drawCenteredText(r, "PAN", panKcx - 24.0f, panKcy - kPanRadius - 10.0f, 48.0f, 10.0f, 8.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    // Readout below knob ("C", "L25", "R40")
    std::string panStr = (std::abs(data.pan) < 0.04f) ? "C" : ((data.pan < 0.0f) ? "L" + std::to_string(static_cast<int>(std::round(-data.pan * 100.0f))) : "R" + std::to_string(static_cast<int>(std::round(data.pan * 100.0f))));
    drawCenteredText(r, panStr, panKcx - 24.0f, panKcy + kPanRadius + 3.0f, 48.0f, 10.0f, 8.0f,
                     isDraggingPan ? theme.highlight.r : theme.primaryAccent.r,
                     isDraggingPan ? theme.highlight.g : theme.primaryAccent.g,
                     isDraggingPan ? theme.highlight.b : theme.primaryAccent.b, 1.0f);
}

void TrackPropertiesPanel::renderEqCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                        TrackPropertiesDrawerData& data, float cx, float cy, float cw) {
    float eqCardH = 138.0f;
    if (cy + eqCardH < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, eqCardH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, eqCardH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "CHANNEL EQ (3-BAND PARAMETRIC)", cx + 12.0f, cy + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    float eqTglW = 60.0f;
    float eqTglX = cx + cw - eqTglW - 10.0f;
    Color eqBg = data.eqEnabled ? (theme.primaryAccent * 0.3f) : Color(0.12f, 0.12f, 0.14f, 0.9f);
    Color eqBorder = data.eqEnabled ? theme.primaryAccent : theme.borderSubtle;
    Color eqText = data.eqEnabled ? theme.primaryAccent : theme.textMuted;
    drawButton(r, Rect2D(eqTglX, cy + 6.0f, eqTglW, 20.0f),
               data.eqEnabled ? "ACTIVE" : "BYPASS", eqBg, eqBorder, eqText, 8.5f, 3.0f, 1.0f);

    float kw3 = cw / 3.0f;
    auto drawEqKnob = [&](float kx, float ky, const std::string& label, const std::string& valStr, float normVal) {
        drawCircle(r, kx, ky, 13.0f, 0.14f, 0.16f, 0.20f, 1.0f);
        drawCircleOutline(r, kx, ky, 13.0f, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
        float ang = -2.35f + normVal * 4.71f;
        drawLine(r, kx, ky, kx + std::cos(ang) * 10.0f, ky + std::sin(ang) * 10.0f,
                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f, 1.8f);
        drawText(r, label, kx - static_cast<float>(label.length()) * 2.7f, ky + 16.0f, 7.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
        drawText(r, valStr, kx - static_cast<float>(valStr.length()) * 2.5f, ky + 26.0f, 7.5f,
                 theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    };

    float eqRow1Y = cy + 45.0f;
    float normHpf = std::clamp((data.eqHpf - 20.0f) / 480.0f, 0.0f, 1.0f);
    drawEqKnob(cx + kw3 * 0.5f, eqRow1Y, "HPF CUT", std::to_string(static_cast<int>(data.eqHpf)) + "Hz", normHpf);

    float normLow = std::clamp((data.eqLowGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::string lowStr = (data.eqLowGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.eqLowGain)) + "dB";
    drawEqKnob(cx + kw3 * 1.5f, eqRow1Y, "LOW GAIN", lowStr, normLow);

    float normHigh = std::clamp((data.eqHighGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::string highStr = (data.eqHighGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.eqHighGain)) + "dB";
    drawEqKnob(cx + kw3 * 2.5f, eqRow1Y, "HIGH GAIN", highStr, normHigh);

    float eqRow2Y = cy + 98.0f;
    float normMidF = std::clamp((data.eqMidFreq - 200.0f) / 7800.0f, 0.0f, 1.0f);
    std::string midFStr = data.eqMidFreq >= 1000.0f ? (std::to_string(static_cast<int>(data.eqMidFreq / 1000.0f)) + "kHz") : (std::to_string(static_cast<int>(data.eqMidFreq)) + "Hz");
    drawEqKnob(cx + kw3 * 0.5f, eqRow2Y, "MID FREQ", midFStr, normMidF);

    float normMidG = std::clamp((data.eqMidGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::string midGStr = (data.eqMidGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.eqMidGain)) + "dB";
    drawEqKnob(cx + kw3 * 1.5f, eqRow2Y, "MID GAIN", midGStr, normMidG);

    float normMidQ = std::clamp((data.eqMidQ - 0.3f) / 9.7f, 0.0f, 1.0f);
    drawEqKnob(cx + kw3 * 2.5f, eqRow2Y, "MID Q", "Q=" + std::to_string(static_cast<int>(data.eqMidQ)), normMidQ);
}

void TrackPropertiesPanel::renderFaceplateCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                              TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    float faceH = data.instrumentExpanded ? (isWide ? 280.0f : 240.0f) : 38.0f;
    if (cy + faceH < bounds_.y || cy > bounds_.y + bounds_.h) return;

    // Outer card container background (matching Eatsbeats original instrument layout)
    drawRoundedRect(r, cx, cy, cw, faceH, 8.0f, 0.11f, 0.12f, 0.14f, 0.98f);
    drawRoundedRectOutline(r, cx, cy, cw, faceH, 8.0f, 0.24f, 0.26f, 0.32f, 0.7f, 1.0f);

    // 1. Top Instrument Header
    float dotX = cx + 16.0f;
    float dotY = cy + 18.0f;
    float dotR = 5.0f;
    drawCircle(r, dotX, dotY, dotR + 3.0f, data.r, data.g, data.b, 0.30f);
    drawCircle(r, dotX, dotY, dotR, data.r, data.g, data.b, 1.0f);

    float fullBtnW = 54.0f;
    float fullBtnH = 22.0f;
    float fullBtnX = cx + cw - 10.0f - fullBtnW;
    float fullBtnY = cy + 7.0f;

    float presetBtnW = 76.0f;
    float presetBtnH = 22.0f;
    float presetBtnX = fullBtnX - 6.0f - presetBtnW;
    float presetBtnY = cy + 7.0f;

    float designBtnW = 26.0f;
    float designBtnH = 22.0f;
    float designBtnX = presetBtnX - 6.0f - designBtnW;
    float designBtnY = cy + 7.0f;

    // Design icon button (links to Design section)
    drawRoundedRect(r, designBtnX, designBtnY, designBtnW, designBtnH, 4.0f, 0.13f, 0.14f, 0.18f, 0.95f);
    drawRoundedRectOutline(r, designBtnX, designBtnY, designBtnW, designBtnH, 4.0f, 0.32f, 0.36f, 0.45f, 0.85f, 1.0f);
    drawDesignChipIcon(r, designBtnX + 5.0f, designBtnY + 3.0f, 16.0f, Color(0.95f, 0.65f, 0.15f, 1.0f));

    // Preset button (opens Preset dialog)
    drawRoundedRect(r, presetBtnX, presetBtnY, presetBtnW, presetBtnH, 4.0f, 0.13f, 0.14f, 0.18f, 0.95f);
    drawRoundedRectOutline(r, presetBtnX, presetBtnY, presetBtnW, presetBtnH, 4.0f, 0.32f, 0.36f, 0.45f, 0.85f, 1.0f);
    drawSlidersTuneIcon(r, presetBtnX + 7.0f, presetBtnY + 5.0f, 12.0f, theme.textSecondary);
    drawText(r, "PRESETS", presetBtnX + 23.0f, presetBtnY + 6.0f, 9.0f, theme.textSecondary);

    // Fullscreen icon button (triggers Fullscreen Device modal)
    Color fullGold(0.95f, 0.62f, 0.10f, 1.0f);
    drawRoundedRect(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 4.0f, 0.14f, 0.12f, 0.10f, 0.95f);
    drawRoundedRectOutline(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 4.0f, fullGold.r, fullGold.g, fullGold.b, 0.85f, 1.2f);
    drawFullscreenIcon(r, fullBtnX + 6.0f, fullBtnY + 5.0f, 11.0f, fullGold, 1.4f);
    drawText(r, "FULL", fullBtnX + 21.0f, fullBtnY + 5.5f, 9.0f, fullGold);

    // Instrument Title in uppercase bold with dropdown chevron
    std::string instTitle = data.presetTitle.empty() ? data.instrument : data.presetTitle;
    for (char& ch : instTitle) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    float availTitleW = designBtnX - (dotX + 13.0f) - 22.0f;
    int maxChars = std::max(6, static_cast<int>(availTitleW / 7.2f));
    if (static_cast<int>(instTitle.length()) > maxChars) {
        instTitle = instTitle.substr(0, maxChars - 3) + "...";
    }
    float titleX = dotX + 13.0f;
    drawText(r, instTitle, titleX, cy + 12.0f, 11.5f, 0.96f, 0.97f, 0.99f, 1.0f);

    float chevX = titleX + static_cast<float>(instTitle.length()) * 6.5f + 8.0f;
    float chevY = cy + 18.0f;
    Color trackAccent(data.r, data.g, data.b, 1.0f);
    if (data.instrumentExpanded) {
        drawChevronUp(r, chevX, chevY, 7.0f, trackAccent, 1.6f);
    } else {
        drawChevronDown(r, chevX, chevY, 7.0f, trackAccent, 1.6f);
    }

    if (data.instrumentExpanded) {
        // 2. Unified Hardware Faceplate with authentic wood cheeks, 3D radial gradients, and matching controls
        float guiX = cx + 8.0f;
        float guiY = cy + 34.0f;
        float guiW = cw - 16.0f;
        float guiH = faceH - 34.0f - 38.0f;

        syncGuiPanelFromTrackData(data);
        Rect2D fpRect(guiX, guiY, guiW, guiH);
        drawGuiFaceplate(r, guiPanel_, fpRect, theme, data.scopeBuffer, data.scopeBufferCount, draggingRow_, draggingWidget_);

        // 3. Below the GUI, right-aligned 'Change Instrument' option
        float chgBtnW = 145.0f;
        float chgBtnH = 24.0f;
        float chgBtnX = cx + cw - 8.0f - chgBtnW;
        float chgBtnY = guiY + guiH + 7.0f;
        drawRoundedRect(r, chgBtnX, chgBtnY, chgBtnW, chgBtnH, 4.0f, 0.12f, 0.14f, 0.18f, 0.95f);
        drawRoundedRectOutline(r, chgBtnX, chgBtnY, chgBtnW, chgBtnH, 4.0f, 0.28f, 0.32f, 0.40f, 0.85f, 1.0f);
        drawText(r, "⇄  CHANGE INSTRUMENT", chgBtnX + 11.0f, chgBtnY + 7.0f, 8.5f, theme.textSecondary);
    }
}

void TrackPropertiesPanel::renderChordFollowCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                                 TrackPropertiesDrawerData& data, float cx, float cy, float cw) {
    float h = 88.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "HARMONIC CHORD TRACK FOLLOW", cx + 12.0f, cy + 10.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // 5 Mode Chips
    const char* chipLabels[5] = {"OFF", "CHORD", "BASS", "SCALE", "COLOR"};
    ChordFollowMode chipModes[5] = {ChordFollowMode::Off, ChordFollowMode::Chord, ChordFollowMode::Bass, ChordFollowMode::Scale, ChordFollowMode::ColorLead};
    float chipW = (cw - 170.0f) / 5.0f;
    float chipH = 24.0f;
    float chipY = cy + 32.0f;

    for (int i = 0; i < 5; ++i) {
        float chipX = cx + 12.0f + static_cast<float>(i) * (chipW + 5.0f);
        bool isSel = (data.chordFollowMode == chipModes[i]);
        drawRoundedRect(r, chipX, chipY, chipW, chipH, 3.5f,
                        isSel ? (theme.primaryAccent * 0.30f) : Color(0.12f, 0.14f, 0.18f, 0.9f),
                        isSel ? 0.95f : 0.65f);
        drawRoundedRectOutline(r, chipX, chipY, chipW, chipH, 3.5f,
                               isSel ? theme.primaryAccent : theme.borderSubtle,
                               isSel ? 0.95f : 0.40f, isSel ? 1.4f : 1.0f);
        drawCenteredText(r, chipLabels[i], chipX, chipY + 2.0f, chipW, chipH, 8.5f,
                         isSel ? theme.primaryAccent.r : theme.textMuted.r,
                         isSel ? theme.primaryAccent.g : theme.textMuted.g,
                         isSel ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);
    }

    // Bake to MIDI action button
    float bakeW = 100.0f;
    float bakeX = cx + cw - bakeW - 12.0f;
    drawButton(r, Rect2D(bakeX, chipY, bakeW, chipH), "[ BAKE TO MIDI ]",
               Color(0.16f, 0.20f, 0.28f, 1.0f), theme.borderSubtle, theme.secondaryAccent, 8.5f, 3.5f, 1.0f);

    // Explanatory dynamic caption
    std::string desc = "Track notes play freely without chord progression transposition.";
    if (data.chordFollowMode == ChordFollowMode::Bass) {
        desc = "Follows root/bass harmonic voice leading with octave constraint.";
    } else if (data.chordFollowMode == ChordFollowMode::Chord) {
        desc = "Conforms notes to polyphonic triad/7th voicing across bars.";
    } else if (data.chordFollowMode == ChordFollowMode::Scale) {
        desc = "Constrains melodies dynamically to chord scale tones.";
    } else if (data.chordFollowMode == ChordFollowMode::ColorLead) {
        desc = "Voice leads tension color tones (9th, 11th, 13th) to resolution.";
    }
    drawText(r, desc, cx + 12.0f, cy + 64.0f, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
}

void TrackPropertiesPanel::renderMidiFxCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                            TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    (void)isWide;
    float h = computeMidiFxRackHeight(data);
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    // Rack outer container
    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    Color accent = theme.primaryAccent;

    // Signal/MIDI icon glyph (3 vertical rounded bars)
    float iconX = cx + 12.0f;
    float iconY = cy + 12.0f;
    drawLine(r, iconX, iconY + 5.0f, iconX, iconY + 13.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);
    drawLine(r, iconX + 4.0f, iconY + 2.0f, iconX + 4.0f, iconY + 16.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);
    drawLine(r, iconX + 8.0f, iconY + 6.0f, iconX + 8.0f, iconY + 12.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);

    // Header Title
    drawText(r, "MIDI FX RACK (" + std::to_string(data.midiFx.size()) + ")", cx + 28.0f, cy + 11.0f, 10.5f,
             accent.r, accent.g, accent.b, 1.0f);

    // + ADD MIDI FX Button
    float addBtnW = 95.0f;
    float addBtnH = 22.0f;
    float addBtnX = cx + cw - addBtnW - 10.0f;
    drawRoundedRect(r, addBtnX, cy + 8.0f, addBtnW, addBtnH, 4.0f, 0.12f, 0.15f, 0.20f, 0.95f);
    drawRoundedRectOutline(r, addBtnX, cy + 8.0f, addBtnW, addBtnH, 4.0f, accent.r, accent.g, accent.b, 0.85f, 1.0f);
    drawCenteredText(r, "+ ADD MIDI FX", addBtnX, cy + 8.0f, addBtnW, addBtnH, 8.5f, accent.r, accent.g, accent.b, 1.0f);

    if (data.midiFx.empty()) {
        float phY = cy + 38.0f;
        drawRoundedRect(r, cx + 10.0f, phY, cw - 20.0f, 32.0f, 4.0f, 0.08f, 0.09f, 0.12f, 0.65f);
        drawRoundedRectOutline(r, cx + 10.0f, phY, cw - 20.0f, 32.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.40f, 1.0f);
        drawText(r, "No MIDI FX loaded. Click + ADD MIDI FX to insert.", cx + 22.0f, phY + 10.0f, 9.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.80f);
        return;
    }

    float itemY = cy + 38.0f;
    float itemW = cw - 16.0f;
    float itemX = cx + 8.0f;

    for (size_t mi = 0; mi < data.midiFx.size(); ++mi) {
        auto& fx = data.midiFx[mi];
        fx.ensureDefaultKnobs();

        float itemH = fx.isExpanded ? 180.0f : 42.0f;
        bool isFirst = (mi == 0);
        bool isLast = (mi == data.midiFx.size() - 1);

        // Card Container
        drawRoundedRect(r, itemX, itemY, itemW, itemH, 6.0f, 0.13f, 0.14f, 0.17f, 0.98f);
        drawRoundedRectOutline(r, itemX, itemY, itemW, itemH, 6.0f,
                               fx.enabled ? accent.r : 0.20f,
                               fx.enabled ? accent.g : 0.22f,
                               fx.enabled ? accent.b : 0.28f,
                               fx.enabled ? 0.75f : 0.40f, 1.0f);

        // 1. Vertical modern pill switch
        float swX = itemX + 10.0f;
        float swY = itemY + 8.0f;
        drawModernPillSwitch(r, swX, swY, fx.enabled, accent);

        // 2. Uppercase title
        std::string upperName = fx.name;
        for (char& c : upperName) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        float titleX = itemX + 32.0f;
        float titleY = itemY + 13.0f;
        drawText(r, upperName, titleX, titleY, 11.0f,
                 fx.enabled ? 0.96f : 0.55f,
                 fx.enabled ? 0.97f : 0.58f,
                 fx.enabled ? 0.99f : 0.65f, 1.0f);

        // 3. Chevron next to title
        float textW = static_cast<float>(upperName.length()) * 6.8f;
        float chevX = titleX + textW + 8.0f;
        float chevY = itemY + 20.0f;
        if (fx.isExpanded) {
            drawChevronUp(r, chevX, chevY, 7.0f, accent, 1.6f);
        } else {
            drawChevronDown(r, chevX, chevY, 7.0f, accent, 1.6f);
        }

        // 4. Action buttons on right: Fullscreen (⛶), Move Up (^), Move Down (v), Trash (delete)
        float fullX = itemX + itemW - 90.0f;
        float upX = itemX + itemW - 68.0f;
        float downX = itemX + itemW - 46.0f;
        float delX = itemX + itemW - 20.0f;
        float btnCenterY = itemY + 20.0f;

        Color arrowCol = Color(0.55f, 0.60f, 0.68f, 0.85f);
        Color dimmedArrow = Color(0.30f, 0.32f, 0.38f, 0.35f);

        drawFullscreenIcon(r, fullX - 5.5f, btnCenterY - 5.5f, 11.0f, accent, 1.4f);
        drawChevronUp(r, upX, btnCenterY, 8.5f, isFirst ? dimmedArrow : arrowCol, 1.6f);
        drawChevronDown(r, downX, btnCenterY, 8.5f, isLast ? dimmedArrow : arrowCol, 1.6f);
        drawTrashIcon(r, delX, btnCenterY, 13.0f, Color(0.55f, 0.60f, 0.68f, 0.85f), 1.3f);

        // 5. If expanded: Fullscreen button + Inset Faceplate
        if (fx.isExpanded) {
            // Fullscreen button
            float fullBtnW = 54.0f;
            float fullBtnH = 20.0f;
            float fullBtnX = itemX + itemW - fullBtnW - 10.0f;
            float fullBtnY = itemY + 36.0f;
            drawRoundedRect(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 3.0f, 0.12f, 0.16f, 0.22f, 0.95f);
            drawRoundedRectOutline(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 3.0f, accent.r, accent.g, accent.b, 0.65f, 1.0f);
            drawFullscreenIcon(r, fullBtnX + 6.0f, fullBtnY + 4.5f, 11.0f, accent, 1.4f);
            drawText(r, "FULL", fullBtnX + 21.0f, fullBtnY + 5.0f, 8.5f, accent);

            // Inset Hardware Faceplate
            float fpX = itemX + 10.0f;
            float fpY = itemY + 60.0f;
            float fpW = itemW - 20.0f;
            float fpH = 108.0f;

            drawRoundedRect(r, fpX, fpY, fpW, fpH, 6.0f, 0.13f, 0.15f, 0.19f, 1.0f);
            drawRoundedRectOutline(r, fpX, fpY, fpW, fpH, 6.0f, 0.08f, 0.09f, 0.12f, 0.90f, 1.2f);

            // Knobs inside faceplate
            size_t knobCount = std::min(fx.knobs.size(), size_t{4});
            if (knobCount > 0) {
                float colW = fpW / static_cast<float>(knobCount);
                for (size_t ki = 0; ki < knobCount; ++ki) {
                    const auto& knob = fx.knobs[ki];
                    float kcx = fpX + (static_cast<float>(ki) + 0.5f) * colW;
                    float kcy = fpY + 36.0f;
                    float kRad = 15.0f;

                    drawCircle(r, kcx, kcy + 2.0f, kRad + 1.0f, 0.05f, 0.06f, 0.08f, 0.45f);
                    drawCircleRadial3StopGradient(r, kcx, kcy, kRad,
                                                  Color(0.85f, 0.88f, 0.92f, 1.0f),
                                                  Color(0.40f, 0.44f, 0.50f, 1.0f),
                                                  Color(0.18f, 0.20f, 0.24f, 1.0f),
                                                  -0.25f * kRad, -0.30f * kRad, 0.50f, 24);
                    drawCircleOutline(r, kcx, kcy, kRad, 0.30f, 0.34f, 0.40f, 0.9f, 1.0f);

                    constexpr float minA = -2.35619449f;
                    constexpr float maxA = 2.35619449f;
                    float curA = minA + std::clamp(knob.value, 0.0f, 1.0f) * (maxA - minA);
                    drawLine(r, kcx, kcy, kcx + std::sin(curA) * (kRad - 2.0f), kcy - std::cos(curA) * (kRad - 2.0f),
                             accent.r, accent.g, accent.b, 1.0f, 1.8f);

                    float lblY = fpY + 58.0f;
                    drawCenteredText(r, knob.label, kcx - colW * 0.5f, lblY, colW, 14.0f, 8.5f,
                                     0.70f, 0.75f, 0.85f, 1.0f);

                    float pillW = 42.0f;
                    float pillH = 15.0f;
                    float pillX = kcx - (pillW * 0.5f);
                    float pillY = fpY + 76.0f;
                    drawRoundedRect(r, pillX, pillY, pillW, pillH, 3.0f, 0.08f, 0.09f, 0.12f, 0.85f);
                    drawRoundedRectOutline(r, pillX, pillY, pillW, pillH, 3.0f, 0.25f, 0.28f, 0.35f, 0.70f, 1.0f);
                    drawCenteredText(r, knob.display, pillX, pillY + 1.0f, pillW, pillH, 7.5f, 0.80f, 0.84f, 0.90f, 1.0f);
                }
            }
        }

        itemY += itemH + 8.0f;
    }
}

void TrackPropertiesPanel::renderAudioFxCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                             TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    (void)isWide;
    float h = computeAudioFxRackHeight(data);
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    // Rack outer container
    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // Audio FX Blue Accent (matches Eatsbeats reference image #1A73E8 / Sky Blue)
    Color accent = Color(0.20f, 0.60f, 1.0f, 1.0f);

    // Signal/waveform icon glyph (3 vertical rounded bars)
    float iconX = cx + 12.0f;
    float iconY = cy + 12.0f;
    drawLine(r, iconX, iconY + 5.0f, iconX, iconY + 13.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);
    drawLine(r, iconX + 4.0f, iconY + 2.0f, iconX + 4.0f, iconY + 16.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);
    drawLine(r, iconX + 8.0f, iconY + 6.0f, iconX + 8.0f, iconY + 12.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);

    // Header Title
    drawText(r, "AUDIO FX RACK (" + std::to_string(data.audioFx.size()) + ")", cx + 28.0f, cy + 11.0f, 10.5f,
             accent.r, accent.g, accent.b, 1.0f);

    // + ADD FX Button
    float addBtnW = 85.0f;
    float addBtnH = 22.0f;
    float addBtnX = cx + cw - addBtnW - 10.0f;
    drawRoundedRect(r, addBtnX, cy + 8.0f, addBtnW, addBtnH, 4.0f, 0.12f, 0.15f, 0.20f, 0.95f);
    drawRoundedRectOutline(r, addBtnX, cy + 8.0f, addBtnW, addBtnH, 4.0f, accent.r, accent.g, accent.b, 0.85f, 1.0f);
    drawCenteredText(r, "+ ADD FX", addBtnX, cy + 8.0f, addBtnW, addBtnH, 9.0f, accent.r, accent.g, accent.b, 1.0f);

    if (data.audioFx.empty()) {
        float phY = cy + 38.0f;
        drawRoundedRect(r, cx + 10.0f, phY, cw - 20.0f, 32.0f, 4.0f, 0.08f, 0.09f, 0.12f, 0.65f);
        drawRoundedRectOutline(r, cx + 10.0f, phY, cw - 20.0f, 32.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.35f, 1.0f);
        drawText(r, "No audio effects inserted. Click + ADD FX to insert.", cx + 22.0f, phY + 10.0f, 9.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.75f);
        return;
    }

    float itemY = cy + 38.0f;
    float itemW = cw - 16.0f;
    float itemX = cx + 8.0f;

    for (size_t fi = 0; fi < data.audioFx.size(); ++fi) {
        auto& fx = data.audioFx[fi];
        fx.ensureDefaultKnobs();

        float itemH = fx.isExpanded ? 180.0f : 42.0f;
        bool isFirst = (fi == 0);
        bool isLast = (fi == data.audioFx.size() - 1);

        // Card Container
        drawRoundedRect(r, itemX, itemY, itemW, itemH, 6.0f, 0.13f, 0.14f, 0.17f, 0.98f);
        drawRoundedRectOutline(r, itemX, itemY, itemW, itemH, 6.0f,
                               fx.enabled ? accent.r : 0.20f,
                               fx.enabled ? accent.g : 0.22f,
                               fx.enabled ? accent.b : 0.28f,
                               fx.enabled ? 0.75f : 0.40f, 1.0f);

        // 1. Vertical modern pill switch
        float swX = itemX + 10.0f;
        float swY = itemY + 8.0f;
        drawModernPillSwitch(r, swX, swY, fx.enabled, accent);

        // 2. Uppercase title
        std::string upperName = fx.name;
        for (char& c : upperName) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        float titleX = itemX + 32.0f;
        float titleY = itemY + 13.0f;
        drawText(r, upperName, titleX, titleY, 11.0f,
                 fx.enabled ? 0.96f : 0.55f,
                 fx.enabled ? 0.97f : 0.58f,
                 fx.enabled ? 0.99f : 0.65f, 1.0f);

        // 3. Chevron next to title
        float textW = static_cast<float>(upperName.length()) * 6.8f;
        float chevX = titleX + textW + 8.0f;
        float chevY = itemY + 20.0f;
        if (fx.isExpanded) {
            drawChevronUp(r, chevX, chevY, 7.0f, accent, 1.6f);
        } else {
            drawChevronDown(r, chevX, chevY, 7.0f, accent, 1.6f);
        }

        // 4. Action buttons on right: Fullscreen (⛶), Move Up (^), Move Down (v), Trash (delete)
        float fullX = itemX + itemW - 90.0f;
        float upX = itemX + itemW - 68.0f;
        float downX = itemX + itemW - 46.0f;
        float delX = itemX + itemW - 20.0f;
        float btnCenterY = itemY + 20.0f;

        Color arrowCol = Color(0.55f, 0.60f, 0.68f, 0.85f);
        Color dimmedArrow = Color(0.30f, 0.32f, 0.38f, 0.35f);

        drawFullscreenIcon(r, fullX - 5.5f, btnCenterY - 5.5f, 11.0f, accent, 1.4f);
        drawChevronUp(r, upX, btnCenterY, 8.5f, isFirst ? dimmedArrow : arrowCol, 1.6f);
        drawChevronDown(r, downX, btnCenterY, 8.5f, isLast ? dimmedArrow : arrowCol, 1.6f);
        drawTrashIcon(r, delX, btnCenterY, 13.0f, Color(0.55f, 0.60f, 0.68f, 0.85f), 1.3f);

        // 5. If expanded: Fullscreen button + Inset Faceplate
        if (fx.isExpanded) {
            // Fullscreen button
            float fullBtnW = 54.0f;
            float fullBtnH = 20.0f;
            float fullBtnX = itemX + itemW - fullBtnW - 10.0f;
            float fullBtnY = itemY + 36.0f;
            drawRoundedRect(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 3.0f, 0.12f, 0.16f, 0.22f, 0.95f);
            drawRoundedRectOutline(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 3.0f, accent.r, accent.g, accent.b, 0.65f, 1.0f);
            drawFullscreenIcon(r, fullBtnX + 6.0f, fullBtnY + 4.5f, 11.0f, accent, 1.4f);
            drawText(r, "FULL", fullBtnX + 21.0f, fullBtnY + 5.0f, 8.5f, accent);

            // Inset Hardware Faceplate
            float fpX = itemX + 10.0f;
            float fpY = itemY + 60.0f;
            float fpW = itemW - 20.0f;
            float fpH = 108.0f;

            bool isSnes = (fx.background == "snes");
            bool isGrunge = (fx.background == "grunge");
            bool isSilver = (fx.background == "silver");

            float bgR = isSnes ? 0.85f : (isGrunge ? 0.19f : (isSilver ? 0.78f : 0.13f));
            float bgG = isSnes ? 0.84f : (isGrunge ? 0.16f : (isSilver ? 0.80f : 0.15f));
            float bgB = isSnes ? 0.81f : (isGrunge ? 0.14f : (isSilver ? 0.82f : 0.18f));

            drawRoundedRect(r, fpX, fpY, fpW, fpH, 6.0f, bgR, bgG, bgB, 1.0f);
            drawRoundedRectOutline(r, fpX, fpY, fpW, fpH, 6.0f, 0.08f, 0.09f, 0.12f, 0.90f, 1.2f);

            // Knobs inside faceplate
            size_t knobCount = std::min(fx.knobs.size(), size_t{4});
            if (knobCount > 0) {
                float colW = fpW / static_cast<float>(knobCount);
                for (size_t ki = 0; ki < knobCount; ++ki) {
                    const auto& knob = fx.knobs[ki];
                    float kcx = fpX + (static_cast<float>(ki) + 0.5f) * colW;
                    float kcy = fpY + 36.0f;
                    float kRad = 15.0f;

                    drawCircle(r, kcx, kcy + 2.0f, kRad + 1.0f, 0.05f, 0.06f, 0.08f, 0.45f);

                    if (isSnes) {
                        drawCircleRadial3StopGradient(r, kcx, kcy, kRad,
                                                      Color(0.98f, 0.97f, 0.95f, 1.0f),
                                                      Color(0.85f, 0.83f, 0.79f, 1.0f),
                                                      Color(0.55f, 0.53f, 0.49f, 1.0f),
                                                      -0.25f * kRad, -0.30f * kRad, 0.50f, 24);
                        drawCircleOutline(r, kcx, kcy, kRad, 0.45f, 0.43f, 0.40f, 0.9f, 1.0f);
                    } else {
                        drawCircleRadial3StopGradient(r, kcx, kcy, kRad,
                                                      Color(0.85f, 0.88f, 0.92f, 1.0f),
                                                      Color(0.40f, 0.44f, 0.50f, 1.0f),
                                                      Color(0.18f, 0.20f, 0.24f, 1.0f),
                                                      -0.25f * kRad, -0.30f * kRad, 0.50f, 24);
                        drawCircleOutline(r, kcx, kcy, kRad, 0.30f, 0.34f, 0.40f, 0.9f, 1.0f);
                    }

                    constexpr float minA = -2.35619449f;
                    constexpr float maxA = 2.35619449f;
                    float curA = minA + std::clamp(knob.value, 0.0f, 1.0f) * (maxA - minA);
                    float needleR = isSnes ? 0.38f : accent.r;
                    float needleG = isSnes ? 0.28f : accent.g;
                    float needleB = isSnes ? 0.65f : accent.b;
                    drawLine(r, kcx, kcy, kcx + std::sin(curA) * (kRad - 2.0f), kcy - std::cos(curA) * (kRad - 2.0f),
                             needleR, needleG, needleB, 1.0f, 1.8f);

                    float lblY = fpY + 58.0f;
                    float lblR = isSnes ? 0.42f : 0.70f;
                    float lblG = isSnes ? 0.35f : 0.75f;
                    float lblB = isSnes ? 0.75f : 0.85f;
                    drawCenteredText(r, knob.label, kcx - colW * 0.5f, lblY, colW, 14.0f, 8.5f,
                                     lblR, lblG, lblB, 1.0f);

                    float pillW = 42.0f;
                    float pillH = 15.0f;
                    float pillX = kcx - (pillW * 0.5f);
                    float pillY = fpY + 76.0f;
                    if (isSnes) {
                        drawRoundedRect(r, pillX, pillY, pillW, pillH, 3.0f, 0.22f, 0.24f, 0.28f, 0.25f);
                        drawRoundedRectOutline(r, pillX, pillY, pillW, pillH, 3.0f, 0.35f, 0.38f, 0.44f, 0.35f, 1.0f);
                        drawCenteredText(r, knob.display, pillX, pillY + 1.0f, pillW, pillH, 7.5f, 0.30f, 0.32f, 0.38f, 1.0f);
                    } else {
                        drawRoundedRect(r, pillX, pillY, pillW, pillH, 3.0f, 0.08f, 0.09f, 0.12f, 0.85f);
                        drawRoundedRectOutline(r, pillX, pillY, pillW, pillH, 3.0f, 0.25f, 0.28f, 0.35f, 0.70f, 1.0f);
                        drawCenteredText(r, knob.display, pillX, pillY + 1.0f, pillW, pillH, 7.5f, 0.80f, 0.84f, 0.90f, 1.0f);
                    }
                }
            }
        }

        itemY += itemH + 8.0f;
    }
}

void TrackPropertiesPanel::renderColorPalette(BatchRenderer2D& r, const ThemeTokens& theme,
                                              TrackPropertiesDrawerData& data, float cx, float cy, float cw) {
    float h = 36.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "TRACK COLOR", cx + 12.0f, cy + 12.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    float startX = cx + (cw >= 560.0f ? 120.0f : 95.0f);
    float availW = (cx + cw - 14.0f) - startX;
    float swatchSpacing = availW / 8.0f;
    float scy = cy + h * 0.5f;

    for (int p = 0; p < 8; ++p) {
        float scx = startX + static_cast<float>(p) * swatchSpacing + swatchSpacing * 0.5f;
        float cr = kQuickPalette[p][0];
        float cg = kQuickPalette[p][1];
        float cb = kQuickPalette[p][2];
        bool isCurCol = (std::abs(data.r - cr) < 0.05f && std::abs(data.g - cg) < 0.05f && std::abs(data.b - cb) < 0.05f);

        drawCircle(r, scx, scy, 9.0f, cr, cg, cb, 1.0f);
        if (isCurCol) {
            drawCircleOutline(r, scx, scy, 12.5f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f);
        }
    }
}

void TrackPropertiesPanel::renderMasterSection(BatchRenderer2D& r, const ThemeTokens& theme,
                                               TrackPropertiesDrawerData& data, float cx, float cy, float cw,
                                               float mouseX, float mouseY) {
    (void)mouseX; (void)mouseY;
    float curY = cy;

    // Header card
    float headH = 50.0f;
    drawRoundedRect(r, cx, curY, cw, headH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, headH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawRoundedRect(r, cx + 10.0f, curY + 10.0f, 5.0f, 30.0f, 2.5f, 1.0f, 0.85f, 0.0f, 1.0f);
    drawText(r, "MASTER BUS CONSOLE", cx + 24.0f, curY + 12.0f, 13.5f, 1.0f, 0.88f, 0.10f, 1.0f);
    drawText(r, "STEREO SUMMING & DYNAMICS MASTERING CHAIN", cx + 24.0f, curY + 30.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
    curY += headH + 8.0f;

    // Master EQ Card
    float eqH = 100.0f;
    drawRoundedRect(r, cx, curY, cw, eqH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, eqH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawText(r, "MASTER BUS EQUALIZER", cx + 12.0f, curY + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    float kw4 = cw / 4.0f;
    auto drawKnobMini = [&](float kx, float ky, const std::string& label, const std::string& valStr, float normVal) {
        drawCircle(r, kx, ky, 12.0f, 0.14f, 0.16f, 0.20f, 1.0f);
        drawCircleOutline(r, kx, ky, 12.0f, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
        float ang = -2.35f + normVal * 4.71f;
        drawLine(r, kx, ky, kx + std::cos(ang) * 9.0f, ky + std::sin(ang) * 9.0f,
                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f, 1.8f);
        drawCenteredText(r, label, kx - 30.0f, ky + 14.0f, 60.0f, 10.0f, 7.5f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
        drawCenteredText(r, valStr, kx - 30.0f, ky + 24.0f, 60.0f, 10.0f, 7.5f,
                         theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    };

    float eqRowY = curY + 44.0f;
    drawKnobMini(cx + kw4 * 0.5f, eqRowY, "SUB CUT", std::to_string(static_cast<int>(data.masterSubCut)) + "Hz", (data.masterSubCut - 20.0f) / 25.0f);
    drawKnobMini(cx + kw4 * 1.5f, eqRowY, "LOW", (data.masterLowGain >= 0 ? "+" : "") + std::to_string(static_cast<int>(data.masterLowGain)) + "dB", (data.masterLowGain + 12.0f) / 24.0f);
    drawKnobMini(cx + kw4 * 2.5f, eqRowY, "MID", (data.masterMidGain >= 0 ? "+" : "") + std::to_string(static_cast<int>(data.masterMidGain)) + "dB", (data.masterMidGain + 12.0f) / 24.0f);
    drawKnobMini(cx + kw4 * 3.5f, eqRowY, "HIGH", (data.masterHighGain >= 0 ? "+" : "") + std::to_string(static_cast<int>(data.masterHighGain)) + "dB", (data.masterHighGain + 12.0f) / 24.0f);
    curY += eqH + 8.0f;

    // Master Limiter Card
    float limH = 110.0f;
    drawRoundedRect(r, cx, curY, cw, limH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, limH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawText(r, "BRICKWALL LIMITER & LOUDNESS", cx + 12.0f, curY + 8.0f, 10.5f,
             theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);

    float autoBtnW = 100.0f;
    drawButton(r, Rect2D(cx + cw - autoBtnW - 10.0f, curY + 6.0f, autoBtnW, 20.0f), "[ AUTO-MASTER ]",
               theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent, 8.5f, 3.0f, 1.0f);

    float limRowY = curY + 48.0f;
    float kw3 = cw / 3.0f;
    drawKnobMini(cx + kw3 * 0.5f, limRowY, "CEILING", std::to_string(data.masterCeilingDbfs).substr(0, 4) + "dB", (data.masterCeilingDbfs + 2.0f) / 2.0f);
    drawKnobMini(cx + kw3 * 1.5f, limRowY, "DRIVE", "+" + std::to_string(static_cast<int>(data.masterLimiterDrive)) + "dB", data.masterLimiterDrive / 12.0f);
    drawKnobMini(cx + kw3 * 2.5f, limRowY, "TARGET LUFS", std::to_string(static_cast<int>(data.masterTargetLufs)) + "LUFS", (data.masterTargetLufs + 24.0f) / 18.0f);
}

void TrackPropertiesPanel::renderClipSection(BatchRenderer2D& r, const ThemeTokens& theme,
                                             TrackPropertiesDrawerData& data, float cx, float cy, float cw,
                                             float mouseX, float mouseY) {
    (void)mouseX; (void)mouseY;
    float curY = cy;

    float headH = 50.0f;
    drawRoundedRect(r, cx, curY, cw, headH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, headH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawRoundedRect(r, cx + 10.0f, curY + 10.0f, 5.0f, 30.0f, 2.5f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    drawText(r, data.clipName, cx + 24.0f, curY + 12.0f, 13.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "MIDI SEQUENCE CLIP", cx + 24.0f, curY + 30.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
    curY += headH + 8.0f;

    // Timing & Looping Card
    float timeH = 90.0f;
    drawRoundedRect(r, cx, curY, cw, timeH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, timeH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawText(r, "CLIP TIMING & PLAYBACK", cx + 12.0f, curY + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    std::string posStr = "Start: Bar " + std::to_string(data.clipStartBar) + "  •  Length: " + std::to_string(data.clipLengthBars) + " Bars";
    drawText(r, posStr, cx + 12.0f, curY + 32.0f, 9.5f, theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);

    float loopBtnW = 90.0f;
    drawButton(r, Rect2D(cx + 12.0f, curY + 54.0f, loopBtnW, 24.0f),
               data.clipLooped ? "LOOP: ON" : "LOOP: OFF",
               data.clipLooped ? theme.primaryAccent * 0.3f : Color(0.12f, 0.14f, 0.18f, 0.9f),
               theme.borderSubtle, theme.primaryAccent, 8.5f, 3.0f, 1.0f);

    // Edit in Piano Roll Button
    float editBtnW = 140.0f;
    drawButton(r, Rect2D(cx + cw - editBtnW - 12.0f, curY + 54.0f, editBtnW, 24.0f),
               "[ EDIT IN PIANO ROLL ]", theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent,
               8.5f, 3.0f, 1.0f);
}

void TrackPropertiesPanel::renderScrollbar(BatchRenderer2D& r, const ThemeTokens& theme) {
    drawRoundedRect(r, scrollbarBounds_.x, scrollbarBounds_.y, scrollbarBounds_.w, scrollbarBounds_.h, 2.5f,
                    Color(0.06f, 0.07f, 0.09f, 0.85f));

    float maxScroll = (std::max)(0.0f, totalContentHeight_ - bounds_.h);
    if (maxScroll <= 0.0f) return;

    float thumbRatio = std::clamp(bounds_.h / totalContentHeight_, 0.15f, 0.90f);
    float thumbH = scrollbarBounds_.h * thumbRatio;
    float normScroll = std::clamp(scrollY_ / maxScroll, 0.0f, 1.0f);
    float thumbY = scrollbarBounds_.y + normScroll * (scrollbarBounds_.h - thumbH);

    bool isDragging = (dragMode_ == DragMode::Scrollbar);
    Color thumbColor = isDragging ? theme.primaryAccent : Color(0.40f, 0.45f, 0.55f, 0.70f);
    drawRoundedRect(r, scrollbarBounds_.x, thumbY, scrollbarBounds_.w, thumbH, 2.5f,
                    thumbColor, isDragging ? 1.0f : 0.70f);
}

std::string TrackPropertiesPanel::getTooltip(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept {
    if (!bounds_.contains(mx, my)) return "";

    auto hit = hitTest(mx, my, data);
    if (!hit.hit) return "";

    switch (hit.area) {
        case TrackPropertiesHitArea::TrackIcon:
            return "Change Track Icon & Color";
        case TrackPropertiesHitArea::RenameButton:
            return "Edit Track Name: " + data.trackName;
        case TrackPropertiesHitArea::ColorSwatch:
            return "Select Track Color Palette";
        case TrackPropertiesHitArea::VolumeSlider: {
            int volPct = static_cast<int>(std::round(data.volume * 100.0f));
            return "Track Volume (" + std::to_string(volPct) + "%)";
        }
        case TrackPropertiesHitArea::PanKnob: {
            std::string panStr = (std::abs(data.pan) < 0.04f) ? "Center" : ((data.pan < 0.0f) ? "Left " + std::to_string(static_cast<int>(std::round(-data.pan * 100.0f))) + "%" : "Right " + std::to_string(static_cast<int>(std::round(data.pan * 100.0f))) + "%");
            return "Track Stereo Pan: " + panStr;
        }
        case TrackPropertiesHitArea::DesignButton:
            return "Open in Design View (Modular / Code)";
        case TrackPropertiesHitArea::PresetButton:
            return "Browse Presets & Sound Library";
        case TrackPropertiesHitArea::FullscreenInstrument:
            return "Open Fullscreen Device Panel (F)";
        case TrackPropertiesHitArea::ToggleInstrumentExpand:
            return data.instrumentExpanded ? "Collapse Instrument Device" : "Expand Instrument Device";
        case TrackPropertiesHitArea::ChangeInstrument:
            return "Choose Instrument Plugin or Preset";
        case TrackPropertiesHitArea::InstrumentKnob: {
            if (hit.index >= 0 && static_cast<size_t>(hit.index) < data.knobs.size()) {
                const auto& k = data.knobs[hit.index];
                return k.label + ": " + k.display;
            }
            return "Instrument Parameter";
        }
        case TrackPropertiesHitArea::ChordFollowChip: {
            const char* modes[5] = {"Off (Chromatic)", "Chord Follow", "Bass Root Follow", "Scale Snap", "Color / Harmonic Lead"};
            if (hit.index >= 0 && hit.index < 5) {
                return std::string("Harmonic Follow: ") + modes[hit.index];
            }
            return "Harmonic Chord Track Follow";
        }
        case TrackPropertiesHitArea::BakeChords:
            return "Bake Transposed Notes into MIDI Clip";
        case TrackPropertiesHitArea::AddMidiFx:
            return "Add MIDI Effect (Arpeggiator, Chords, Humanize, Scale Snap)";
        case TrackPropertiesHitArea::ToggleMidiFx:
            return "Bypass / Enable MIDI Effect";
        case TrackPropertiesHitArea::MoveMidiFxUp:
            return "Move MIDI Effect Earlier in Chain";
        case TrackPropertiesHitArea::MoveMidiFxDown:
            return "Move MIDI Effect Later in Chain";
        case TrackPropertiesHitArea::RemoveMidiFx:
            return "Remove MIDI Effect";
        case TrackPropertiesHitArea::FullscreenMidiFx:
            return "Open Fullscreen Effect Editor";
        case TrackPropertiesHitArea::ToggleMidiFxExpand:
            return "Expand / Collapse MIDI Effect";
        case TrackPropertiesHitArea::MidiFxKnob: {
            size_t mi = static_cast<size_t>(hit.index / 10);
            size_t ki = static_cast<size_t>(hit.index % 10);
            if (mi < data.midiFx.size() && ki < data.midiFx[mi].knobs.size()) {
                const auto& k = data.midiFx[mi].knobs[ki];
                return k.label + ": " + k.display;
            }
            return "MIDI FX Parameter";
        }
        case TrackPropertiesHitArea::AddAudioFx:
            return "Add Audio Effect (Reverb, Delay, Bitcrusher, Limiter)";
        case TrackPropertiesHitArea::ToggleAudioFx:
            return "Bypass / Enable Audio Effect";
        case TrackPropertiesHitArea::MoveAudioFxUp:
            return "Move Audio Effect Earlier in Chain";
        case TrackPropertiesHitArea::MoveAudioFxDown:
            return "Move Audio Effect Later in Chain";
        case TrackPropertiesHitArea::RemoveAudioFx:
            return "Remove Audio Effect";
        case TrackPropertiesHitArea::FullscreenAudioFx:
            return "Open Fullscreen Effect Editor";
        case TrackPropertiesHitArea::ToggleAudioFxExpand:
            return "Expand / Collapse Audio Effect";
        case TrackPropertiesHitArea::AudioFxKnob: {
            size_t fi = static_cast<size_t>(hit.index / 10);
            size_t ki = static_cast<size_t>(hit.index % 10);
            if (fi < data.audioFx.size() && ki < data.audioFx[fi].knobs.size()) {
                const auto& k = data.audioFx[fi].knobs[ki];
                return k.label + ": " + k.display;
            }
            return "Audio FX Parameter";
        }
        case TrackPropertiesHitArea::EqToggle:
            return "Toggle 3-Band Parametric EQ";
        case TrackPropertiesHitArea::ClipEditInPianoRoll:
            return "Open Selected Clip in Piano Roll / MIDI Editor";
        case TrackPropertiesHitArea::ClipLoopToggle:
            return "Toggle Clip Loop Playback";
        case TrackPropertiesHitArea::PullTab:
            return "Properties Drawer Pull Tab";
        case TrackPropertiesHitArea::CloseButton:
            return "Close Properties (Esc)";
        case TrackPropertiesHitArea::TabTrack:
            return "Track Properties & Device Chain";
        case TrackPropertiesHitArea::TabClip:
            return "Clip Inspector & Playback Parameters";
        default:
            break;
    }
    return "";
}

TrackPropertiesHitResult TrackPropertiesPanel::hitTest(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept {
    TrackPropertiesHitResult res;

    // Scrollbar hit
    if (scrollbarBounds_.contains(mx, my)) {
        res.hit = true;
        res.area = TrackPropertiesHitArea::ScrollbarTrack;
        return res;
    }

    // Ribbon hits
    if (showTrackRibbon_ && ribbonBounds_.contains(mx, my)) {
        size_t trackCount = data.allTrackNames.empty() ? 5 : data.allTrackNames.size();
        const float startX = ribbonBounds_.x + 4.0f;
        const float tabW = std::clamp((ribbonBounds_.w - 40.0f) / static_cast<float>(trackCount), 90.0f, 130.0f);
        const float tabGap = 8.0f;
        for (size_t i = 0; i < trackCount; ++i) {
            float tx = startX + static_cast<float>(i) * (tabW + tabGap);
            if (mx >= tx && mx <= tx + tabW) {
                res.hit = true;
                res.index = static_cast<int>(i);
                return res;
            }
        }
    }

    // Header Card Hits: [Icon] [Title] [EDIT]
    if (headerCardBounds_.contains(mx, my)) {
        float cx = headerCardBounds_.x;
        float cy = headerCardBounds_.y;
        float cw = headerCardBounds_.w;
        bool isWide = (bounds_.w >= 560.0f);
        float h = isWide ? 40.0f : 36.0f;

        // 1. Track icon button on left
        float iconBtnX = cx + 8.0f;
        float iconBtnW = 24.0f;
        float iconBtnH = 24.0f;
        float iconBtnY = cy + (h - iconBtnH) * 0.5f;
        if (mx >= iconBtnX && mx <= iconBtnX + iconBtnW && my >= iconBtnY && my <= iconBtnY + iconBtnH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::TrackIcon;
            return res;
        }

        // 2. Edit icon button on right
        float editBtnW = 20.0f;
        float editBtnH = 20.0f;
        float editBtnX = cx + cw - 12.0f - editBtnW;
        float editBtnY = cy + (h - editBtnH) * 0.5f;
        if (mx >= editBtnX - 4.0f && mx <= editBtnX + editBtnW + 4.0f && my >= cy && my <= cy + h) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::RenameButton;
            return res;
        }

        // Clicking anywhere on track title text also triggers track rename / properties dialog
        res.hit = true;
        res.area = TrackPropertiesHitArea::RenameButton;
        return res;
    }

    // Color Swatches Card Hits (directly below Header Card)
    if (colorCardBounds_.contains(mx, my)) {
        float cx = colorCardBounds_.x;
        float cy = colorCardBounds_.y;
        float cw = colorCardBounds_.w;
        float startX = cx + (cw >= 560.0f ? 120.0f : 95.0f);
        float availW = (cx + cw - 14.0f) - startX;
        float swatchSpacing = availW / 8.0f;
        float scy = cy + 18.0f;
        for (int p = 0; p < 8; ++p) {
            float scx = startX + static_cast<float>(p) * swatchSpacing + swatchSpacing * 0.5f;
            if (std::hypot(mx - scx, my - scy) <= 12.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ColorSwatch;
                res.index = p;
                return res;
            }
        }
    }

    // Faceplate Hits (Eatsbeats Original Instrument Layout)
    if (faceplateBounds_.contains(mx, my)) {
        float cx = faceplateBounds_.x;
        float cy = faceplateBounds_.y;
        float cw = faceplateBounds_.w;
        bool isWide = (bounds_.w >= 560.0f);
        float faceH = isWide ? 260.0f : 240.0f;

        float fullBtnW = 54.0f;
        float fullBtnH = 22.0f;
        float fullBtnX = cx + cw - 10.0f - fullBtnW;
        float fullBtnY = cy + 7.0f;

        float presetBtnW = 76.0f;
        float presetBtnH = 22.0f;
        float presetBtnX = fullBtnX - 6.0f - presetBtnW;
        float presetBtnY = cy + 7.0f;

        float designBtnW = 26.0f;
        float designBtnH = 22.0f;
        float designBtnX = presetBtnX - 6.0f - designBtnW;
        float designBtnY = cy + 7.0f;

        // Fullscreen button
        if (mx >= fullBtnX && mx <= fullBtnX + fullBtnW && my >= fullBtnY && my <= fullBtnY + fullBtnH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::FullscreenInstrument;
            return res;
        }

        // Preset button
        if (mx >= presetBtnX && mx <= presetBtnX + presetBtnW && my >= presetBtnY && my <= presetBtnY + presetBtnH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::PresetButton;
            return res;
        }

        // Design button
        if (mx >= designBtnX && mx <= designBtnX + designBtnW && my >= designBtnY && my <= designBtnY + designBtnH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::DesignButton;
            return res;
        }

        // Toggle Instrument Collapse / Expand by clicking title/chevron/dot
        if (mx >= cx && mx <= designBtnX - 4.0f && my >= cy && my <= cy + 34.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::ToggleInstrumentExpand;
            return res;
        }

        if (data.instrumentExpanded) {
            // Change instrument button below GUI
            float guiX = cx + 8.0f;
            float guiY = cy + 34.0f;
            float guiW = cw - 16.0f;
            float guiH = faceH - 34.0f - 38.0f;

            float chgBtnW = 145.0f;
            float chgBtnH = 24.0f;
            float chgBtnX = cx + cw - 8.0f - chgBtnW;
            float chgBtnY = guiY + guiH + 7.0f;
            if (mx >= chgBtnX && mx <= chgBtnX + chgBtnW && my >= chgBtnY && my <= chgBtnY + chgBtnH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ChangeInstrument;
                return res;
            }

            // Knobs - first test guiPanel_ widget bounds if populated
            for (size_t rIdx = 0; rIdx < guiPanel_.rows.size(); ++rIdx) {
                const auto& row = guiPanel_.rows[rIdx];
                for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                    const auto& w = row.widgets[wIdx];
                    if (w.type == GuiWidgetType::Divider) continue;
                    if (w.bounds.contains(mx, my)) {
                        res.hit = true;
                        res.area = TrackPropertiesHitArea::InstrumentKnob;
                        res.index = static_cast<int>(rIdx * 100 + wIdx);
                        res.normVal = w.currentVal;
                        return res;
                    }
                }
            }

            size_t knobCount = std::min(data.knobs.size(), static_cast<size_t>(6));
            if (isWide) {
                float knobAreaW = guiW - 210.0f;
                float kStep = knobAreaW / static_cast<float>(std::max(size_t{1}, knobCount));
                for (size_t k = 0; k < knobCount; ++k) {
                    float kcx = guiX + static_cast<float>(k) * kStep + (kStep * 0.5f);
                    float kcy = guiY + guiH * 0.5f - 8.0f;
                    if (std::hypot(mx - kcx, my - kcy) <= 22.0f) {
                        res.hit = true;
                        res.area = TrackPropertiesHitArea::InstrumentKnob;
                        res.index = static_cast<int>(k);
                        return res;
                    }
                }
            } else {
                float colW = guiW / 3.0f;
                for (size_t k = 0; k < knobCount; ++k) {
                    int row = static_cast<int>(k / 3);
                    int col = static_cast<int>(k % 3);
                    float kcx = guiX + static_cast<float>(col) * colW + (colW * 0.5f);
                    float kcy = guiY + 28.0f + static_cast<float>(row) * 64.0f;
                    if (std::hypot(mx - kcx, my - kcy) <= 20.0f) {
                        res.hit = true;
                        res.area = TrackPropertiesHitArea::InstrumentKnob;
                        res.index = static_cast<int>(k);
                        return res;
                    }
                }
            }
        }
    }

    // Chord Follow Hits
    if (chordFollowBounds_.contains(mx, my)) {
        float cx = chordFollowBounds_.x;
        float cy = chordFollowBounds_.y;
        float cw = chordFollowBounds_.w;
        float chipW = (cw - 170.0f) / 5.0f;
        float chipH = 24.0f;
        float chipY = cy + 32.0f;

        for (int i = 0; i < 5; ++i) {
            float chipX = cx + 12.0f + static_cast<float>(i) * (chipW + 5.0f);
            if (mx >= chipX && mx <= chipX + chipW && my >= chipY && my <= chipY + chipH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ChordFollowChip;
                res.index = i;
                return res;
            }
        }

        float bakeW = 100.0f;
        float bakeX = cx + cw - bakeW - 12.0f;
        if (mx >= bakeX && mx <= bakeX + bakeW && my >= chipY && my <= chipY + chipH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::BakeChords;
            return res;
        }
    }

    // MIDI FX Hits
    float curCardY = chordFollowBounds_.y + chordFollowBounds_.h + 8.0f;
    float mfxH = computeMidiFxRackHeight(data);
    const_cast<TrackPropertiesPanel*>(this)->midiFxBounds_ = Rect2D(bounds_.x + 8.0f, curCardY, bounds_.w - 16.0f, mfxH);
    curCardY += mfxH + 8.0f;

    float afxH = computeAudioFxRackHeight(data);
    const_cast<TrackPropertiesPanel*>(this)->audioFxBounds_ = Rect2D(bounds_.x + 8.0f, curCardY, bounds_.w - 16.0f, afxH);

    if (midiFxBounds_.contains(mx, my)) {
        float cx = midiFxBounds_.x;
        float cy = midiFxBounds_.y;
        float cw = midiFxBounds_.w;
        float addBtnW = 95.0f;
        float addBtnX = cx + cw - addBtnW - 10.0f;
        if (mx >= addBtnX && mx <= addBtnX + addBtnW && my >= cy + 6.0f && my <= cy + 30.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::AddMidiFx;
            return res;
        }

        float itemY = cy + 38.0f;
        float itemW = cw - 16.0f;
        float itemX = cx + 8.0f;
        for (size_t mi = 0; mi < data.midiFx.size(); ++mi) {
            const auto& fx = data.midiFx[mi];
            float itemH = fx.isExpanded ? 180.0f : 42.0f;

            // Power switch toggle
            if (mx >= itemX + 6.0f && mx <= itemX + 30.0f && my >= itemY + 4.0f && my <= itemY + 36.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ToggleMidiFx;
                res.index = static_cast<int>(mi);
                return res;
            }

            // Move Up (^)
            if (mx >= itemX + itemW - 74.0f && mx <= itemX + itemW - 54.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::MoveMidiFxUp;
                res.index = static_cast<int>(mi);
                return res;
            }

            // Move Down (v)
            if (mx >= itemX + itemW - 53.0f && mx <= itemX + itemW - 34.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::MoveMidiFxDown;
                res.index = static_cast<int>(mi);
                return res;
            }

            // Delete trash can
            if (mx >= itemX + itemW - 32.0f && mx <= itemX + itemW - 6.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::RemoveMidiFx;
                res.index = static_cast<int>(mi);
                return res;
            }

            // Fullscreen in header
            if (mx >= itemX + itemW - 102.0f && mx <= itemX + itemW - 74.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::FullscreenMidiFx;
                res.index = static_cast<int>(mi);
                return res;
            }

            // Title or Chevron (toggle expand/collapse)
            if (mx >= itemX + 30.0f && mx <= itemX + itemW - 105.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ToggleMidiFxExpand;
                res.index = static_cast<int>(mi);
                return res;
            }

            if (fx.isExpanded) {
                // Fullscreen button
                if (mx >= itemX + itemW - 70.0f && mx <= itemX + itemW - 6.0f && my >= itemY + 34.0f && my <= itemY + 58.0f) {
                    res.hit = true;
                    res.area = TrackPropertiesHitArea::FullscreenMidiFx;
                    res.index = static_cast<int>(mi);
                    return res;
                }

                // Inset Knobs
                float fpX = itemX + 10.0f;
                float fpY = itemY + 60.0f;
                float fpW = itemW - 20.0f;
                if (mx >= fpX && mx <= fpX + fpW && my >= fpY + 10.0f && my <= fpY + 98.0f) {
                    size_t knobCount = std::min(fx.knobs.size(), size_t{4});
                    if (knobCount > 0) {
                        float colW = fpW / static_cast<float>(knobCount);
                        int kIdx = static_cast<int>((mx - fpX) / colW);
                        if (kIdx >= 0 && kIdx < static_cast<int>(knobCount)) {
                            res.hit = true;
                            res.area = TrackPropertiesHitArea::MidiFxKnob;
                            res.index = static_cast<int>(mi * 10 + kIdx);
                            res.normVal = fx.knobs[kIdx].value;
                            return res;
                        }
                    }
                }
            }

            itemY += itemH + 8.0f;
        }
    }

    // Audio FX Hits
    if (audioFxBounds_.contains(mx, my)) {
        float cx = audioFxBounds_.x;
        float cy = audioFxBounds_.y;
        float cw = audioFxBounds_.w;
        float addBtnW = 85.0f;
        float addBtnX = cx + cw - addBtnW - 10.0f;
        if (mx >= addBtnX && mx <= addBtnX + addBtnW && my >= cy + 6.0f && my <= cy + 30.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::AddAudioFx;
            return res;
        }

        float itemY = cy + 38.0f;
        float itemW = cw - 16.0f;
        float itemX = cx + 8.0f;
        for (size_t fi = 0; fi < data.audioFx.size(); ++fi) {
            const auto& fx = data.audioFx[fi];
            float itemH = fx.isExpanded ? 180.0f : 42.0f;

            // Power switch toggle
            if (mx >= itemX + 6.0f && mx <= itemX + 30.0f && my >= itemY + 4.0f && my <= itemY + 36.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ToggleAudioFx;
                res.index = static_cast<int>(fi);
                return res;
            }

            // Move Up (^)
            if (mx >= itemX + itemW - 74.0f && mx <= itemX + itemW - 54.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::MoveAudioFxUp;
                res.index = static_cast<int>(fi);
                return res;
            }

            // Move Down (v)
            if (mx >= itemX + itemW - 53.0f && mx <= itemX + itemW - 34.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::MoveAudioFxDown;
                res.index = static_cast<int>(fi);
                return res;
            }

            // Delete trash can
            if (mx >= itemX + itemW - 32.0f && mx <= itemX + itemW - 6.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::RemoveAudioFx;
                res.index = static_cast<int>(fi);
                return res;
            }

            // Fullscreen in header
            if (mx >= itemX + itemW - 102.0f && mx <= itemX + itemW - 74.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::FullscreenAudioFx;
                res.index = static_cast<int>(fi);
                return res;
            }

            // Title or Chevron (toggle expand/collapse)
            if (mx >= itemX + 30.0f && mx <= itemX + itemW - 105.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ToggleAudioFxExpand;
                res.index = static_cast<int>(fi);
                return res;
            }

            if (fx.isExpanded) {
                // Fullscreen button
                if (mx >= itemX + itemW - 70.0f && mx <= itemX + itemW - 6.0f && my >= itemY + 34.0f && my <= itemY + 58.0f) {
                    res.hit = true;
                    res.area = TrackPropertiesHitArea::FullscreenAudioFx;
                    res.index = static_cast<int>(fi);
                    return res;
                }

                // Inset Knobs
                float fpX = itemX + 10.0f;
                float fpY = itemY + 60.0f;
                float fpW = itemW - 20.0f;
                if (mx >= fpX && mx <= fpX + fpW && my >= fpY + 10.0f && my <= fpY + 98.0f) {
                    size_t knobCount = std::min(fx.knobs.size(), size_t{4});
                    if (knobCount > 0) {
                        float colW = fpW / static_cast<float>(knobCount);
                        int kIdx = static_cast<int>((mx - fpX) / colW);
                        if (kIdx >= 0 && kIdx < static_cast<int>(knobCount)) {
                            res.hit = true;
                            res.area = TrackPropertiesHitArea::AudioFxKnob;
                            res.index = static_cast<int>(fi * 10 + kIdx);
                            res.normVal = fx.knobs[kIdx].value;
                            return res;
                        }
                    }
                }
            }

            itemY += itemH + 8.0f;
        }
    }

    return res;
}

bool TrackPropertiesPanel::executeHitAction(const TrackPropertiesHitResult& hit,
                                             TrackPropertiesDrawerData& data,
                                             const ViewContext& ctx) {
    if (showTrackRibbon_ && hit.area == TrackPropertiesHitArea::None) {
        if (onTrackSelected) onTrackSelected(static_cast<uint32_t>(hit.index));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::TrackIcon) {
        if (onChooseTrackIcon) onChooseTrackIcon(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::RenameButton) {
        if (ctx.onOpenValueEdit) {
            ValueEditRequest req;
            req.title = "EDIT TRACK PROPERTIES";
            req.paramName = "Track Name";
            req.isTextMode = true;
            req.initialText = data.trackName;
            req.accentColor = Color(data.r, data.g, data.b, 1.0f);
            req.onCommitText = [this, &data, trackIdx = data.trackIndex](const std::string& newName) {
                if (!newName.empty()) {
                    data.trackName = newName;
                    if (trackIdx < data.allTrackNames.size()) {
                        data.allTrackNames[trackIdx] = newName;
                    }
                    if (onTrackRenameWithText) {
                        onTrackRenameWithText(trackIdx, newName);
                    }
                }
            };
            ctx.onOpenValueEdit(req);
        }
        if (onTrackRename) onTrackRename(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ColorSwatch) {
        if (hit.index >= 0 && hit.index < 8) {
            data.r = kQuickPalette[hit.index][0];
            data.g = kQuickPalette[hit.index][1];
            data.b = kQuickPalette[hit.index][2];
            if (onColorChanged) onColorChanged(data.trackIndex, data.r, data.g, data.b);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::CodeButton) {
        if (onOpenCodeEditor) onOpenCodeEditor(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MuteButton) {
        data.mute = !data.mute;
        if (onMuteToggled) onMuteToggled(data.trackIndex, data.mute);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::SoloButton) {
        data.solo = !data.solo;
        if (onSoloToggled) onSoloToggled(data.trackIndex, data.solo);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::FreezeButton) {
        data.freeze = !data.freeze;
        if (onFreezeToggled) onFreezeToggled(data.trackIndex, data.freeze);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::DesignButton) {
        if (onOpenDesign) onOpenDesign(data.trackIndex);
        if (ctx.onNavigateTab) ctx.onNavigateTab(WorkspaceView::Design);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::PresetButton) {
        pluginDialog_.open(PluginDialogMode::SelectPreset, data.trackName, data.trackIndex);
        if (onOpenPresets) onOpenPresets(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::InstrumentPrevPreset) {
        if (onPrevPreset) onPrevPreset();
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::InstrumentNextPreset) {
        if (onNextPreset) onNextPreset();
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::FullscreenInstrument) {
        if (onOpenFullscreenDevice) onOpenFullscreenDevice(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ToggleInstrumentExpand) {
        data.instrumentExpanded = !data.instrumentExpanded;
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::FullscreenMidiFx) {
        if (onOpenFullscreenMidiFx) onOpenFullscreenMidiFx(data.trackIndex, static_cast<size_t>(hit.index));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::FullscreenAudioFx) {
        if (onOpenFullscreenAudioFx) onOpenFullscreenAudioFx(data.trackIndex, static_cast<size_t>(hit.index));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ChangeInstrument) {
        pluginDialog_.open(PluginDialogMode::AddInstrument, data.trackName, data.trackIndex);
        if (onChangeInstrument) onChangeInstrument(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ToggleAudioFxExpand) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.audioFx.size())) {
            data.audioFx[hit.index].isExpanded = !data.audioFx[hit.index].isExpanded;
            if (onAudioFxChanged) onAudioFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ToggleMidiFxExpand) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.midiFx.size())) {
            data.midiFx[hit.index].isExpanded = !data.midiFx[hit.index].isExpanded;
            if (onMidiFxChanged) onMidiFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MoveAudioFxUp) {
        if (hit.index > 0 && hit.index < static_cast<int>(data.audioFx.size())) {
            size_t fromIdx = static_cast<size_t>(hit.index);
            size_t toIdx = fromIdx - 1;
            std::swap(data.audioFx[fromIdx], data.audioFx[toIdx]);
            if (onReorderAudioFx) onReorderAudioFx(data.trackIndex, fromIdx, toIdx);
            if (onAudioFxChanged) onAudioFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MoveAudioFxDown) {
        if (hit.index >= 0 && hit.index + 1 < static_cast<int>(data.audioFx.size())) {
            size_t fromIdx = static_cast<size_t>(hit.index);
            size_t toIdx = fromIdx + 1;
            std::swap(data.audioFx[fromIdx], data.audioFx[toIdx]);
            if (onReorderAudioFx) onReorderAudioFx(data.trackIndex, fromIdx, toIdx);
            if (onAudioFxChanged) onAudioFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MoveMidiFxUp) {
        if (hit.index > 0 && hit.index < static_cast<int>(data.midiFx.size())) {
            size_t fromIdx = static_cast<size_t>(hit.index);
            size_t toIdx = fromIdx - 1;
            std::swap(data.midiFx[fromIdx], data.midiFx[toIdx]);
            if (onReorderMidiFx) onReorderMidiFx(data.trackIndex, fromIdx, toIdx);
            if (onMidiFxChanged) onMidiFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MoveMidiFxDown) {
        if (hit.index >= 0 && hit.index + 1 < static_cast<int>(data.midiFx.size())) {
            size_t fromIdx = static_cast<size_t>(hit.index);
            size_t toIdx = fromIdx + 1;
            std::swap(data.midiFx[fromIdx], data.midiFx[toIdx]);
            if (onReorderMidiFx) onReorderMidiFx(data.trackIndex, fromIdx, toIdx);
            if (onMidiFxChanged) onMidiFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ChordFollowChip) {
        ChordFollowMode modes[5] = {ChordFollowMode::Off, ChordFollowMode::Chord, ChordFollowMode::Bass, ChordFollowMode::Scale, ChordFollowMode::ColorLead};
        if (hit.index >= 0 && hit.index < 5) {
            data.chordFollowMode = modes[hit.index];
            if (onChordFollowChanged) onChordFollowChanged(data.trackIndex, data.chordFollowMode);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::BakeChords) {
        if (onBakeChords) onBakeChords(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::AddMidiFx) {
        pluginDialog_.open(PluginDialogMode::AddMidiFx, data.trackName, data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::AddAudioFx) {
        pluginDialog_.open(PluginDialogMode::AddAudioFx, data.trackName, data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::RemoveMidiFx) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.midiFx.size())) {
            if (onRemoveMidiFx) onRemoveMidiFx(data.trackIndex, static_cast<size_t>(hit.index));
            data.midiFx.erase(data.midiFx.begin() + hit.index);
            if (onMidiFxChanged) onMidiFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::RemoveAudioFx) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.audioFx.size())) {
            if (onRemoveAudioFx) onRemoveAudioFx(data.trackIndex, static_cast<size_t>(hit.index));
            data.audioFx.erase(data.audioFx.begin() + hit.index);
            if (onAudioFxChanged) onAudioFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ToggleMidiFx) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.midiFx.size())) {
            data.midiFx[hit.index].enabled = !data.midiFx[hit.index].enabled;
            if (onToggleMidiFx) onToggleMidiFx(data.trackIndex, static_cast<size_t>(hit.index), data.midiFx[hit.index].enabled);
            if (onMidiFxChanged) onMidiFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ToggleAudioFx) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.audioFx.size())) {
            data.audioFx[hit.index].enabled = !data.audioFx[hit.index].enabled;
            if (onToggleAudioFx) onToggleAudioFx(data.trackIndex, static_cast<size_t>(hit.index), data.audioFx[hit.index].enabled);
            if (onAudioFxChanged) onAudioFxChanged(data.trackIndex);
        }
        return true;
    }

    return false;
}

bool TrackPropertiesPanel::openValueEditForHit(const TrackPropertiesHitResult& hit, TrackPropertiesDrawerData& data,
                                               const std::function<void(const ValueEditRequest&)>& onOpenValueEdit) {
    if (!onOpenValueEdit || !hit.hit) return false;

    if (hit.area == TrackPropertiesHitArea::VolumeSlider) {
        auto cfg = presenter::TrackPropertiesPresenter::makeEditConfig(
            presenter::TrackParamType::Volume, data.volume, data.trackName,
            [this, &data](float val) {
                data.volume = val;
                if (onVolumeChanged) onVolumeChanged(data.trackIndex, val);
            });
        onOpenValueEdit(toValueEditRequest(cfg, Color(data.r, data.g, data.b, 1.0f)));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::PanKnob) {
        auto cfg = presenter::TrackPropertiesPresenter::makeEditConfig(
            presenter::TrackParamType::Pan, data.pan, data.trackName,
            [this, &data](float val) {
                data.pan = val;
                if (onPanChanged) onPanChanged(data.trackIndex, val);
            });
        onOpenValueEdit(toValueEditRequest(cfg, Color(data.r, data.g, data.b, 1.0f)));
        return true;
    }


    if (hit.area == TrackPropertiesHitArea::InstrumentKnob) {
        int rIdx = hit.index / 100;
        int wIdx = hit.index % 100;
        std::string pName = "Param";
        std::string lName = "Param";
        float cVal = 0.5f;
        if (rIdx < static_cast<int>(guiPanel_.rows.size()) &&
            wIdx < static_cast<int>(guiPanel_.rows[rIdx].widgets.size())) {
            const auto& w = guiPanel_.rows[rIdx].widgets[wIdx];
            if (w.type == GuiWidgetType::Divider) return false;
            pName = w.param.empty() ? w.label : w.param;
            lName = w.label.empty() ? pName : w.label;
            cVal = w.currentVal;
        } else if (hit.index < static_cast<int>(data.knobs.size())) {
            pName = data.knobs[hit.index].name;
            lName = data.knobs[hit.index].label.empty() ? pName : data.knobs[hit.index].label;
            cVal = data.knobs[hit.index].value;
        }
        ValueEditRequest req;
        req.title = data.trackName + " • " + lName;
        req.paramName = lName;
        req.currentValue = cVal;
        req.minValue = 0.0f;
        req.maxValue = 1.0f;
        req.defaultValue = 0.5f;
        req.hasDefault = true;
        req.allowPercentage = true;
        req.accentColor = Color(data.r, data.g, data.b, 1.0f);
        req.onCommit = [this, &data, pName, rIdx, wIdx](float val) {
            if (rIdx < static_cast<int>(guiPanel_.rows.size()) &&
                wIdx < static_cast<int>(guiPanel_.rows[rIdx].widgets.size())) {
                guiPanel_.rows[rIdx].widgets[wIdx].currentVal = val;
            }
            for (auto& k : data.knobs) {
                if (stringEqualsIgnoreCase(k.name, pName)) {
                    k.value = val;
                    k.display = std::to_string(static_cast<int>(std::round(val * 100.0f))) + "%";
                    break;
                }
            }
            if (onParamChanged) {
                onParamChanged(data.trackIndex, pName, val);
            }
        };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::AudioFxKnob) {
        size_t fxIdx = static_cast<size_t>(hit.index / 10);
        size_t kIdx = static_cast<size_t>(hit.index % 10);
        if (fxIdx < data.audioFx.size() && kIdx < data.audioFx[fxIdx].knobs.size()) {
            auto& fx = data.audioFx[fxIdx];
            auto& k = fx.knobs[kIdx];
            ValueEditRequest req;
            req.title = fx.name + " • " + (k.label.empty() ? k.name : k.label);
            req.paramName = k.name;
            req.currentValue = k.value;
            req.minValue = 0.0f;
            req.maxValue = 1.0f;
            req.defaultValue = 0.5f;
            req.hasDefault = true;
            req.allowPercentage = true;
            req.accentColor = Color(0.2f, 0.8f, 1.0f, 1.0f);
            req.onCommit = [this, &data, fxIdx, kIdx, pName = k.name](float val) {
                if (fxIdx < data.audioFx.size() && kIdx < data.audioFx[fxIdx].knobs.size()) {
                    data.audioFx[fxIdx].knobs[kIdx].value = val;
                    data.audioFx[fxIdx].knobs[kIdx].display = std::to_string(static_cast<int>(std::round(val * 100.0f))) + "%";
                    if (pName == "drive" || pName == "Drive" || pName == "time" || pName == "decay") {
                        data.audioFx[fxIdx].drive = val;
                    } else if (pName == "mix" || pName == "Mix" || pName == "gain") {
                        data.audioFx[fxIdx].mix = val;
                    }
                }
                if (onAudioFxParamChanged) {
                    onAudioFxParamChanged(data.trackIndex, pName, val);
                }
            };
            onOpenValueEdit(req);
            return true;
        }
    }

    if (hit.area == TrackPropertiesHitArea::MidiFxKnob) {
        size_t fxIdx = static_cast<size_t>(hit.index / 10);
        size_t kIdx = static_cast<size_t>(hit.index % 10);
        if (fxIdx < data.midiFx.size() && kIdx < data.midiFx[fxIdx].knobs.size()) {
            auto& fx = data.midiFx[fxIdx];
            auto& k = fx.knobs[kIdx];
            ValueEditRequest req;
            req.title = fx.name + " • " + (k.label.empty() ? k.name : k.label);
            req.paramName = k.name;
            req.currentValue = k.value;
            req.minValue = 0.0f;
            req.maxValue = 1.0f;
            req.defaultValue = 0.5f;
            req.hasDefault = true;
            req.allowPercentage = true;
            req.accentColor = Color(0.9f, 0.7f, 0.2f, 1.0f);
            req.onCommit = [this, &data, fxIdx, kIdx, pName = k.name](float val) {
                if (fxIdx < data.midiFx.size() && kIdx < data.midiFx[fxIdx].knobs.size()) {
                    data.midiFx[fxIdx].knobs[kIdx].value = val;
                    data.midiFx[fxIdx].knobs[kIdx].display = std::to_string(static_cast<int>(std::round(val * 100.0f))) + "%";
                }
                if (onMidiFxParamChanged) {
                    onMidiFxParamChanged(data.trackIndex, pName, val);
                }
            };
            onOpenValueEdit(req);
            return true;
        }
    }

    if (hit.area == TrackPropertiesHitArea::EqHpf) {
        auto cfg = presenter::TrackPropertiesPresenter::makeEditConfig(
            presenter::TrackParamType::EqHpf, data.eqHpf, data.trackName,
            [&data](float val) { data.eqHpf = val; });
        onOpenValueEdit(toValueEditRequest(cfg, Color(data.r, data.g, data.b, 1.0f)));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::EqLowGain) {
        auto cfg = presenter::TrackPropertiesPresenter::makeEditConfig(
            presenter::TrackParamType::EqLowGain, data.eqLowGain, data.trackName,
            [&data](float val) { data.eqLowGain = val; });
        onOpenValueEdit(toValueEditRequest(cfg, Color(data.r, data.g, data.b, 1.0f)));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::EqHighGain) {
        auto cfg = presenter::TrackPropertiesPresenter::makeEditConfig(
            presenter::TrackParamType::EqHighGain, data.eqHighGain, data.trackName,
            [&data](float val) { data.eqHighGain = val; });
        onOpenValueEdit(toValueEditRequest(cfg, Color(data.r, data.g, data.b, 1.0f)));
        return true;
    }


    if (hit.area == TrackPropertiesHitArea::EqMidFreq) {
        ValueEditRequest req;
        req.title = data.trackName + " • Mid Freq";
        req.paramName = "Mid Freq";
        req.currentValue = data.eqMidFreq;
        req.minValue = 200.0f;
        req.maxValue = 8000.0f;
        req.defaultValue = 1000.0f;
        req.unit = "Hz";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.accentColor = Color(data.r, data.g, data.b, 1.0f);
        req.onCommit = [&data](float val) { data.eqMidFreq = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::EqMidGain) {
        ValueEditRequest req;
        req.title = data.trackName + " • Mid Gain";
        req.paramName = "Mid Gain";
        req.currentValue = data.eqMidGain;
        req.minValue = -18.0f;
        req.maxValue = 18.0f;
        req.defaultValue = 0.0f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.accentColor = Color(data.r, data.g, data.b, 1.0f);
        req.onCommit = [&data](float val) { data.eqMidGain = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::EqMidQ) {
        ValueEditRequest req;
        req.title = data.trackName + " • Mid Q";
        req.paramName = "Mid Q";
        req.currentValue = data.eqMidQ;
        req.minValue = 0.3f;
        req.maxValue = 10.0f;
        req.defaultValue = 1.0f;
        req.hasDefault = true;
        req.allowPercentage = false;
        req.accentColor = Color(data.r, data.g, data.b, 1.0f);
        req.onCommit = [&data](float val) { data.eqMidQ = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterEqSubCut) {
        ValueEditRequest req;
        req.title = "MASTER SUB CUT";
        req.paramName = "Sub Cut";
        req.currentValue = data.masterSubCut;
        req.minValue = 20.0f;
        req.maxValue = 45.0f;
        req.defaultValue = 25.0f;
        req.unit = "Hz";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterSubCut = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterEqLow) {
        ValueEditRequest req;
        req.title = "MASTER LOW GAIN";
        req.paramName = "Master Low Gain";
        req.currentValue = data.masterLowGain;
        req.minValue = -12.0f;
        req.maxValue = 12.0f;
        req.defaultValue = 0.0f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterLowGain = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterEqMid) {
        ValueEditRequest req;
        req.title = "MASTER MID GAIN";
        req.paramName = "Master Mid Gain";
        req.currentValue = data.masterMidGain;
        req.minValue = -12.0f;
        req.maxValue = 12.0f;
        req.defaultValue = 0.0f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterMidGain = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterEqHigh) {
        ValueEditRequest req;
        req.title = "MASTER HIGH GAIN";
        req.paramName = "Master High Gain";
        req.currentValue = data.masterHighGain;
        req.minValue = -12.0f;
        req.maxValue = 12.0f;
        req.defaultValue = 0.0f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterHighGain = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterCeiling) {
        ValueEditRequest req;
        req.title = "MASTER LIMITER CEILING";
        req.paramName = "Ceiling";
        req.currentValue = data.masterCeilingDbfs;
        req.minValue = -2.0f;
        req.maxValue = 0.0f;
        req.defaultValue = -0.3f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterCeilingDbfs = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterDrive) {
        ValueEditRequest req;
        req.title = "MASTER LIMITER DRIVE";
        req.paramName = "Drive";
        req.currentValue = data.masterLimiterDrive;
        req.minValue = 0.0f;
        req.maxValue = 12.0f;
        req.defaultValue = 0.0f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterLimiterDrive = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterLufs) {
        ValueEditRequest req;
        req.title = "MASTER TARGET LUFS";
        req.paramName = "Target LUFS";
        req.currentValue = data.masterTargetLufs;
        req.minValue = -24.0f;
        req.maxValue = -6.0f;
        req.defaultValue = -14.0f;
        req.unit = "LUFS";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterTargetLufs = val; };
        onOpenValueEdit(req);
        return true;
    }

    return false;
}

bool TrackPropertiesPanel::handlePointer(const PointerEvent& ev, TrackPropertiesDrawerData& data,
                                         const ViewContext& ctx) {
    data.syncKnobsIfEmpty();

    // 0. Forward to embedded PluginSearchDialog if open
    if (pluginDialog_.isOpen()) {
        if (pluginDialog_.handlePointer(ev)) return true;
        return true;
    }

    // Mouse Wheel Scrolling
    if (ev.action == PointerAction::Scroll) {
        float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
        scrollY_ = std::clamp(scrollY_ - ev.scrollY * 28.0f, 0.0f, maxScroll);
        if (onScrollChanged) onScrollChanged(scrollY_);
        return true;
    }

    // Pointer Down
    if (ev.action == PointerAction::Down) {
        scroller_.stop();

        // Check Scrollbar Hit
        if (scrollbarBounds_.contains(ev.x, ev.y)) {
            dragMode_ = DragMode::Scrollbar;
            dragStartY_ = ev.y;
            dragStartScrollY_ = scrollY_;
            return true;
        }

        auto hit = hitTest(ev.x, ev.y, data);
        if (hit.hit) {
            // Universal Right-Click on any knob or slider opens manual ValueEditDialog
            if (ev.button == PointerButton::Right) {
                if (openValueEditForHit(hit, data, ctx.onOpenValueEdit)) {
                    return true;
                }
            }

            // Mobile / Touch Long-Press Detection (>= 0.45s hold triggers ValueEditDialog)
            if (ev.type == PointerType::Touch || ctx.isMobile) {
                if (hit.area == TrackPropertiesHitArea::VolumeSlider ||
                    hit.area == TrackPropertiesHitArea::PanKnob ||
                    hit.area == TrackPropertiesHitArea::InstrumentKnob ||
                    hit.area == TrackPropertiesHitArea::AudioFxKnob ||
                    hit.area == TrackPropertiesHitArea::MidiFxKnob ||
                    hit.area == TrackPropertiesHitArea::EqHpf ||
                    hit.area == TrackPropertiesHitArea::EqLowGain ||
                    hit.area == TrackPropertiesHitArea::EqHighGain ||
                    hit.area == TrackPropertiesHitArea::EqMidFreq ||
                    hit.area == TrackPropertiesHitArea::EqMidGain ||
                    hit.area == TrackPropertiesHitArea::EqMidQ ||
                    hit.area == TrackPropertiesHitArea::MasterEqSubCut ||
                    hit.area == TrackPropertiesHitArea::MasterEqLow ||
                    hit.area == TrackPropertiesHitArea::MasterEqMid ||
                    hit.area == TrackPropertiesHitArea::MasterEqHigh ||
                    hit.area == TrackPropertiesHitArea::MasterCeiling ||
                    hit.area == TrackPropertiesHitArea::MasterDrive ||
                    hit.area == TrackPropertiesHitArea::MasterLufs) {
                    isLongPressActive_ = true;
                    longPressTimer_ = 0.0f;
                    touchDownTimePoint_ = std::chrono::steady_clock::now();
                    longPressPos_ = Point2D{ev.x, ev.y};
                    longPressHit_ = hit;
                    longPressOpenValueEdit_ = ctx.onOpenValueEdit;
                }
            }

            // Check continuous drag controls:
            if (hit.area == TrackPropertiesHitArea::VolumeSlider) {
                dragMode_ = DragMode::VolumeSlider;
                data.volume = hit.normVal * 1.5f;
                if (onVolumeChanged) onVolumeChanged(data.trackIndex, data.volume);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::PanKnob) {
                dragMode_ = DragMode::PanKnob;
                dragStartY_ = ev.y;
                dragStartVal_ = data.pan;
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::InstrumentKnob) {
                dragMode_ = DragMode::InstrumentKnob;
                activeKnobIndex_ = hit.index;
                dragStartY_ = ev.y;
                int rIdx = hit.index / 100;
                int wIdx = hit.index % 100;
                if (rIdx < static_cast<int>(guiPanel_.rows.size()) &&
                    wIdx < static_cast<int>(guiPanel_.rows[rIdx].widgets.size())) {
                    auto& w = guiPanel_.rows[rIdx].widgets[wIdx];
                    if (w.type == GuiWidgetType::ToggleSwitch) {
                        float newVal = (w.currentVal > 0.5f) ? 0.0f : 1.0f;
                        w.currentVal = newVal;
                        for (auto& k : data.knobs) {
                            if (stringEqualsIgnoreCase(k.name, w.param)) {
                                k.value = newVal;
                                k.display = (newVal > 0.5f) ? "On" : "Off";
                                break;
                            }
                        }
                        if (onParamChanged) {
                            onParamChanged(data.trackIndex, w.param, newVal);
                        }
                        dragMode_ = DragMode::None;
                        return true;
                    }
                    dragStartVal_ = w.currentVal;
                    draggingRow_ = rIdx;
                    draggingWidget_ = wIdx;
                } else {
                    dragStartVal_ = (activeKnobIndex_ < static_cast<int>(data.knobs.size()))
                                        ? data.knobs[activeKnobIndex_].value
                                        : 0.5f;
                    draggingRow_ = activeKnobIndex_ / 6;
                    draggingWidget_ = activeKnobIndex_ % 6;
                }
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::AudioFxKnob) {
                dragMode_ = DragMode::AudioFxKnob;
                activeFxIndex_ = static_cast<size_t>(hit.index / 10);
                activeKnobIndex_ = hit.index % 10;
                dragStartY_ = ev.y;
                if (activeFxIndex_ < data.audioFx.size() &&
                    static_cast<size_t>(activeKnobIndex_) < data.audioFx[activeFxIndex_].knobs.size()) {
                    dragStartVal_ = data.audioFx[activeFxIndex_].knobs[activeKnobIndex_].value;
                } else {
                    dragStartVal_ = 0.5f;
                }
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::MidiFxKnob) {
                dragMode_ = DragMode::MidiFxKnob;
                activeFxIndex_ = static_cast<size_t>(hit.index / 10);
                activeKnobIndex_ = hit.index % 10;
                dragStartY_ = ev.y;
                if (activeFxIndex_ < data.midiFx.size() &&
                    static_cast<size_t>(activeKnobIndex_) < data.midiFx[activeFxIndex_].knobs.size()) {
                    dragStartVal_ = data.midiFx[activeFxIndex_].knobs[activeKnobIndex_].value;
                } else {
                    dragStartVal_ = 0.5f;
                }
                return true;
            }

            // Clickable button / chip / swatch targets:
            if (ev.type == PointerType::Touch) {
                // Touch: queue hit as pending; if user drags to scroll, cancel it
                pendingHitResult_ = hit;
                touchStartY_ = ev.y;
                touchStartX_ = ev.x;
                touchStartScrollY_ = scrollY_;
                touchDragCommitted_ = false;
                touchStartTimeMs_ = ev.timestampMs;
                scroller_.reset();
                scroller_.addSample(ev.x, ev.y, ev.timestampMs);
                return true;
            } else {
                // Desktop Mouse: crisp immediate trigger
                return executeHitAction(hit, data, ctx);
            }
        }

        // Hit empty area of panel: allow drag to scroll directly!
        if (bounds_.contains(ev.x, ev.y)) {
            pendingHitResult_ = TrackPropertiesHitResult{};
            touchStartY_ = ev.y;
            touchStartX_ = ev.x;
            touchStartScrollY_ = scrollY_;
            touchDragCommitted_ = (ev.type != PointerType::Touch);
            dragMode_ = DragMode::TouchScroll;
            scroller_.reset();
            scroller_.addSample(ev.x, ev.y, ev.timestampMs);
            return true;
        }
    }

    // Pointer Move
    if (ev.action == PointerAction::Move) {
        if (isLongPressActive_) {
            if (std::hypot(ev.x - longPressPos_.x, ev.y - longPressPos_.y) > 10.0f) {
                isLongPressActive_ = false;
            }
        }

        if (dragMode_ == DragMode::TouchScroll) {
            float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
            scrollY_ = std::clamp(touchStartScrollY_ - (ev.y - touchStartY_), 0.0f, maxScroll);
            scroller_.addSample(ev.x, ev.y, ev.timestampMs);
            if (onScrollChanged) onScrollChanged(scrollY_);
            return true;
        }

        if (pendingHitResult_.hit && !touchDragCommitted_) {
            float dy = std::abs(ev.y - touchStartY_);
            float slop = (ev.type == PointerType::Touch) ? 14.0f : 4.0f;
            if (dy > slop) {
                touchDragCommitted_ = true;
                dragMode_ = DragMode::TouchScroll;
                float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
                scrollY_ = std::clamp(touchStartScrollY_ - (ev.y - touchStartY_), 0.0f, maxScroll);
                scroller_.addSample(ev.x, ev.y, ev.timestampMs);
                if (onScrollChanged) onScrollChanged(scrollY_);
                return true;
            }
        }

        if (dragMode_ == DragMode::Scrollbar) {
            float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
            if (maxScroll > 0.0f) {
                float dy = ev.y - dragStartY_;
                float scrollDelta = (dy / scrollbarBounds_.h) * totalContentHeight_;
                scrollY_ = std::clamp(dragStartScrollY_ + scrollDelta, 0.0f, maxScroll);
                if (onScrollChanged) onScrollChanged(scrollY_);
            }
            return true;
        }

        if (dragMode_ == DragMode::VolumeSlider) {
            float cx = mixerCardBounds_.x;
            float cw = mixerCardBounds_.w;
            bool isWide = (bounds_.w >= 560.0f);
            float vLabelX = cx + 14.0f;
            float vTrackX = vLabelX + (isWide ? 38.0f : 32.0f);
            float panAreaW = isWide ? 85.0f : 74.0f;
            float vTrackW = (cx + cw - panAreaW - 55.0f) - vTrackX;
            if (vTrackW < 50.0f) vTrackW = 50.0f;

            float norm = std::clamp((ev.x - vTrackX) / vTrackW, 0.0f, 1.0f);
            data.volume = norm * 1.5f;
            if (onVolumeChanged) onVolumeChanged(data.trackIndex, data.volume);
            return true;
        }

        if (dragMode_ == DragMode::PanKnob) {
            float dy = dragStartY_ - ev.y;
            data.pan = std::clamp(dragStartVal_ + (dy / 100.0f), -1.0f, 1.0f);
            if (onPanChanged) onPanChanged(data.trackIndex, data.pan);
            return true;
        }

        if (dragMode_ == DragMode::InstrumentKnob) {
            float dy = dragStartY_ - ev.y;
            float newVal = std::clamp(dragStartVal_ + (dy / 150.0f), 0.0f, 1.0f);
            std::string pName;
            if (draggingRow_ >= 0 && draggingRow_ < static_cast<int>(guiPanel_.rows.size()) &&
                draggingWidget_ >= 0 && draggingWidget_ < static_cast<int>(guiPanel_.rows[draggingRow_].widgets.size())) {
                auto& w = guiPanel_.rows[draggingRow_].widgets[draggingWidget_];
                w.currentVal = newVal;
                pName = w.param;
            }

            bool updated = false;
            for (auto& k : data.knobs) {
                if (!pName.empty() && stringEqualsIgnoreCase(k.name, pName)) {
                    k.value = newVal;
                    k.display = std::to_string(static_cast<int>(std::round(newVal * 100.0f))) + "%";
                    updated = true;
                    break;
                }
            }
            if (!updated && activeKnobIndex_ >= 0 && activeKnobIndex_ < static_cast<int>(data.knobs.size())) {
                data.knobs[activeKnobIndex_].value = newVal;
                data.knobs[activeKnobIndex_].display = std::to_string(static_cast<int>(std::round(newVal * 100.0f))) + "%";
                if (pName.empty()) pName = data.knobs[activeKnobIndex_].name;
            }
            if (onParamChanged && !pName.empty()) {
                onParamChanged(data.trackIndex, pName, newVal);
            }
            return true;
        }

        if (dragMode_ == DragMode::AudioFxKnob && activeFxIndex_ < data.audioFx.size()) {
            auto& fx = data.audioFx[activeFxIndex_];
            if (activeKnobIndex_ >= 0 && static_cast<size_t>(activeKnobIndex_) < fx.knobs.size()) {
                float dy = dragStartY_ - ev.y;
                float newVal = std::clamp(dragStartVal_ + (dy / 120.0f), 0.0f, 1.0f);
                fx.knobs[activeKnobIndex_].value = newVal;

                const auto& kname = fx.knobs[activeKnobIndex_].name;
                if (kname == "BITS" || kname == "bits") {
                    int b = 4 + static_cast<int>(std::round(newVal * 12.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(b) + " bit";
                } else if (kname == "CRUSH" || kname == "crush") {
                    int c = 1 + static_cast<int>(std::round(newVal * 31.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(c) + "x";
                } else if (kname == "DRIVE" || kname == "drive") {
                    float d = 1.0f + newVal * 9.0f;
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.1f x", d);
                    fx.knobs[activeKnobIndex_].display = buf;
                    fx.drive = newVal;
                } else if (kname == "MIX" || kname == "mix") {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.1f", newVal);
                    fx.knobs[activeKnobIndex_].display = buf;
                    fx.mix = newVal;
                } else if (kname == "TIME" || kname == "time") {
                    int ms = 10 + static_cast<int>(std::round(newVal * 990.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(ms) + " ms";
                } else if (kname == "FEEDBACK" || kname == "feedback" || kname == "FDBK") {
                    int pct = static_cast<int>(std::round(newVal * 100.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(pct) + "%";
                } else if (kname == "RATE" || kname == "rate") {
                    float hz = 0.1f + newVal * 9.9f;
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.1f Hz", hz);
                    fx.knobs[activeKnobIndex_].display = buf;
                } else if (kname == "DEPTH" || kname == "depth") {
                    int pct = static_cast<int>(std::round(newVal * 100.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(pct) + "%";
                } else {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.2f", newVal);
                    fx.knobs[activeKnobIndex_].display = buf;
                }

                if (activeKnobIndex_ == 0 && fx.knobs.size() == 2) fx.drive = newVal;

                if (onAudioFxParamChanged) {
                    onAudioFxParamChanged(data.trackIndex, kname, newVal);
                }
                if (onAudioFxChanged) {
                    onAudioFxChanged(data.trackIndex);
                }
                return true;
            }
        }

        if (dragMode_ == DragMode::MidiFxKnob && activeFxIndex_ < data.midiFx.size()) {
            auto& fx = data.midiFx[activeFxIndex_];
            if (activeKnobIndex_ >= 0 && static_cast<size_t>(activeKnobIndex_) < fx.knobs.size()) {
                float dy = dragStartY_ - ev.y;
                float newVal = std::clamp(dragStartVal_ + (dy / 120.0f), 0.0f, 1.0f);
                fx.knobs[activeKnobIndex_].value = newVal;

                const auto& kname = fx.knobs[activeKnobIndex_].name;
                if (kname == "RATE" || kname == "rate") {
                    int div = static_cast<int>(4 * std::pow(2.0f, std::round(newVal * 3.0f)));
                    fx.knobs[activeKnobIndex_].display = "1/" + std::to_string(div);
                } else if (kname == "STEPS" || kname == "steps") {
                    int s = 1 + static_cast<int>(std::round(newVal * 15.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(s);
                } else if (kname == "OCTAVES" || kname == "octaves") {
                    int oct = 1 + static_cast<int>(std::round(newVal * 3.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(oct);
                } else if (kname == "GATE" || kname == "gate" || kname == "SWING" || kname == "swing" ||
                           kname == "CHANCE" || kname == "chance") {
                    int pct = static_cast<int>(std::round(newVal * 100.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(pct) + "%";
                } else {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.2f", newVal);
                    fx.knobs[activeKnobIndex_].display = buf;
                }

                if (onMidiFxParamChanged) {
                    onMidiFxParamChanged(data.trackIndex, kname, newVal);
                }
                if (onMidiFxChanged) {
                    onMidiFxChanged(data.trackIndex);
                }
                return true;
            }
        }
    }

    // Pointer Up or Cancel
    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        isLongPressActive_ = false;
        if (dragMode_ == DragMode::TouchScroll) {
            dragMode_ = DragMode::None;
            scroller_.endDrag(ev.timestampMs);
            return true;
        }

        if (pendingHitResult_.hit) {
            TrackPropertiesHitResult hitToExec = pendingHitResult_;
            pendingHitResult_ = TrackPropertiesHitResult{};
            if (!touchDragCommitted_ && ev.action == PointerAction::Up) {
                return executeHitAction(hitToExec, data, ctx);
            }
            return true;
        }

        if (dragMode_ != DragMode::None) {
            dragMode_ = DragMode::None;
            activeKnobIndex_ = -1;
            draggingRow_ = -1;
            draggingWidget_ = -1;
            return true;
        }
    }

    return false;
}

bool TrackPropertiesPanel::handleKey(int key, int scancode, int action, int mods,
                                     [[maybe_unused]] const ViewContext& ctx) {
    if (pluginDialog_.isOpen()) {
        if (pluginDialog_.handleKey(key, scancode, action, mods)) return true;
    }
    return false;
}

} // namespace eatsbits::ui
