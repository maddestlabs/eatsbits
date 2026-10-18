#include "eatsbits/ui/widgets/transport_header.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/ui/clay/clay_context.hpp"
#include "clay.h"
#include <iomanip>
#include <sstream>

namespace eatsbits::ui {

TransportHeader::TransportHeader() = default;
TransportHeader::~TransportHeader() = default;
TransportHeader::TransportHeader(TransportHeader&&) noexcept = default;
TransportHeader& TransportHeader::operator=(TransportHeader&&) noexcept = default;

void TransportHeader::declareLayout(clay::ClayContext& ctx, float screenWidth, float height) {
    (void)ctx;
    float cy = 8.0f;
    float btnH = 32.0f;

    CLAY(CLAY_ID("TransportHeaderRoot"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(screenWidth), CLAY_SIZING_FIXED(height) },
            .padding = { 12, 12, static_cast<uint16_t>(cy), static_cast<uint16_t>(cy) },
            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        }
    }) {
        // 1. Logo Button (EB)
        CLAY(CLAY_ID("TransportLogo"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(32.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // Gap 10 to play
        CLAY(CLAY_ID("GapLogoPlay"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(10.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // 2. Play Button
        CLAY(CLAY_ID("TransportPlay"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(42.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // Gap 4 to stop
        CLAY(CLAY_ID("GapPlayStop"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(4.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // 3. Stop Button
        CLAY(CLAY_ID("TransportStop"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(36.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // Gap 10 to time display
        CLAY(CLAY_ID("GapStopTime"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(10.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // 4. Time Display (Nixie)
        CLAY(CLAY_ID("TransportTimeDisplay"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(105.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // Gap 9 to BPM
        CLAY(CLAY_ID("GapTimeBpm"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(9.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // 5. BPM Scrubber
        CLAY(CLAY_ID("TransportBpm"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(75.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // Gap 9 to Snap
        CLAY(CLAY_ID("GapBpmSnap"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(9.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // 6. Snap Quantize
        CLAY(CLAY_ID("TransportSnap"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(54.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // Gap 8 to Loop
        CLAY(CLAY_ID("GapSnapLoop"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(8.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // 7. Loop Toggle
        CLAY(CLAY_ID("TransportLoop"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(50.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // Gap 6 to Metro
        CLAY(CLAY_ID("GapLoopMetro"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(6.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // 8. Metronome Toggle
        CLAY(CLAY_ID("TransportMetro"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(50.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}

        // Center flex-grow spacer
        CLAY(CLAY_ID("TransportSpacer"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(btnH) }
            }
        }) {}

        // 9. Browser Button pinned to right
        CLAY(CLAY_ID("TransportBrowser"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(88.0f), CLAY_SIZING_FIXED(btnH) } }
        }) {}
    }
}

void TransportHeader::layout(float screenWidth, float height, clay::ClayContext* ctx) {
    clay::ClayContext* activeCtx = ctx ? ctx : clay::ClayContext::getCurrent();
    bool isStandalonePass = false;

    if (!activeCtx) {
        // Shared fallback arena - eliminates the per-widget 512KB micro-arena allocation!
        static clay::ClayContext s_sharedFallbackCtx;
        if (!s_sharedFallbackCtx.isInitialized()) {
            s_sharedFallbackCtx.initialize(screenWidth, height, 512 * 1024);
        }
        activeCtx = &s_sharedFallbackCtx;
        isStandalonePass = true;
    }

    if (isStandalonePass) {
        activeCtx->setDimensions(screenWidth, height);
        activeCtx->makeCurrent();
        activeCtx->beginLayout();
        declareLayout(*activeCtx, screenWidth, height);
        activeCtx->endLayout();
    }

    auto toRect = [](const Clay_BoundingBox& b) noexcept -> Rect2D {
        return Rect2D{ b.x, b.y, b.width, b.height };
    };

    bounds_ = toRect(activeCtx->getElementData(CLAY_ID("TransportHeaderRoot")).boundingBox);
    logoBounds_ = toRect(activeCtx->getElementData(CLAY_ID("TransportLogo")).boundingBox);
    playBounds_ = toRect(activeCtx->getElementData(CLAY_ID("TransportPlay")).boundingBox);
    stopBounds_ = toRect(activeCtx->getElementData(CLAY_ID("TransportStop")).boundingBox);
    timeDisplayBounds_ = toRect(activeCtx->getElementData(CLAY_ID("TransportTimeDisplay")).boundingBox);
    bpmBounds_ = toRect(activeCtx->getElementData(CLAY_ID("TransportBpm")).boundingBox);
    snapBounds_ = toRect(activeCtx->getElementData(CLAY_ID("TransportSnap")).boundingBox);
    loopBounds_ = toRect(activeCtx->getElementData(CLAY_ID("TransportLoop")).boundingBox);
    metroBounds_ = toRect(activeCtx->getElementData(CLAY_ID("TransportMetro")).boundingBox);
    browserBounds_ = toRect(activeCtx->getElementData(CLAY_ID("TransportBrowser")).boundingBox);
}

void TransportHeader::render(BatchRenderer2D& r, const ThemeTokens& theme, audio::AudioEngine* engine, bool isBrowserOpen) {
    drawRect(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h,
             theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 1.0f);
    drawLine(r, bounds_.x, bounds_.y + bounds_.h, bounds_.x + bounds_.w, bounds_.y + bounds_.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.9f, 1.5f);

    // 1. Logo Button
    drawButton(r, logoBounds_, "EB",
               theme.primaryAccent * 0.25f, theme.primaryAccent, theme.primaryAccent,
               13.0f, 4.0f, 1.0f);

    // 2. Play Button
    bool isPlaying = engine && engine->getSequencer().isPlaying();
    Color playBg = isPlaying ? (theme.primaryAccent * 0.35f) : theme.panelBackground;
    Color playBorder = isPlaying ? theme.primaryAccent : theme.borderSubtle;
    Color playText = isPlaying ? theme.primaryAccent : theme.textPrimary;
    drawButton(r, playBounds_, ">", playBg, playBorder, playText, 14.0f, 4.0f, 1.0f);

    // 3. Stop Button
    drawRoundedRect(r, stopBounds_.x, stopBounds_.y, stopBounds_.w, stopBounds_.h, 4.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.95f);
    drawRoundedRectOutline(r, stopBounds_.x, stopBounds_.y, stopBounds_.w, stopBounds_.h, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);
    float stopSqX = stopBounds_.x + (stopBounds_.w - 12.0f) * 0.5f;
    float stopSqY = stopBounds_.y + (stopBounds_.h - 12.0f) * 0.5f;
    drawRect(r, stopSqX, stopSqY, 12.0f, 12.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.9f);

    // 4. Time Display (Nixie Bar:Beat:Tick)
    drawRoundedRect(r, timeDisplayBounds_.x, timeDisplayBounds_.y, timeDisplayBounds_.w, timeDisplayBounds_.h, 4.0f,
                    theme.lcdBackground.r, theme.lcdBackground.g, theme.lcdBackground.b, 1.0f);
    drawRoundedRectOutline(r, timeDisplayBounds_.x, timeDisplayBounds_.y, timeDisplayBounds_.w, timeDisplayBounds_.h, 4.0f,
                           theme.lcdBorder.r, theme.lcdBorder.g, theme.lcdBorder.b, 0.8f, 1.0f);

    uint32_t step = engine ? engine->getSequencer().getTransport().getCurrentStep() : 0;
    uint32_t bar = (step / 16) + 1;
    uint32_t beat = ((step % 16) / 4) + 1;
    uint32_t tick = (step % 4) + 1;

    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "%03u:%02u:%02u", bar, beat, tick);
    drawCenteredText(r, timeBuf, timeDisplayBounds_, 13.0f, theme.tempoText);

    // 5. BPM Display
    drawRoundedRect(r, bpmBounds_.x, bpmBounds_.y, bpmBounds_.w, bpmBounds_.h, 4.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.9f);
    float bpmTextY = bpmBounds_.y + (bpmBounds_.h - 11.5f) * 0.5f;
    float curBpm = engine ? static_cast<float>(engine->getSequencer().getTransport().getBpm()) : 128.0f;
    char bpmBuf[32];
    std::snprintf(bpmBuf, sizeof(bpmBuf), "%.1f", curBpm);
    drawText(r, bpmBuf, bpmBounds_.x + 10.0f, bpmTextY, 11.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "BPM", bpmBounds_.x + 46.0f, bpmTextY + 1.0f, 9.0f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

    // 6. Snap
    drawButton(r, snapBounds_, "1/16",
               theme.panelBackground, theme.borderSubtle, theme.secondaryAccent, 10.5f, 4.0f, 0.0f);

    // 7. Loop Button
    Color loopBg = loopActive_ ? (theme.primaryAccent * 0.25f) : theme.panelBackground;
    Color loopBorder = loopActive_ ? theme.primaryAccent : theme.borderSubtle;
    Color loopText = loopActive_ ? theme.primaryAccent : theme.textMuted;
    drawButton(r, loopBounds_, "LOOP", loopBg, loopBorder, loopText, 10.0f, 4.0f, 1.0f);

    // 8. Metronome Button
    Color metroBg = metroActive_ ? (theme.secondaryAccent * 0.25f) : theme.panelBackground;
    Color metroBorder = metroActive_ ? theme.secondaryAccent : theme.borderSubtle;
    Color metroText = metroActive_ ? theme.secondaryAccent : theme.textMuted;
    drawButton(r, metroBounds_, "METRO", metroBg, metroBorder, metroText, 10.0f, 4.0f, 1.0f);

    // 9. Browser Drawer Button
    Color brwBg = isBrowserOpen ? (theme.secondaryAccent * 0.3f) : theme.panelBackground;
    Color brwBorder = isBrowserOpen ? theme.secondaryAccent : theme.borderSubtle;
    Color brwText = isBrowserOpen ? theme.secondaryAccent : theme.textPrimary;
    drawButton(r, browserBounds_, "[BROWSER]", brwBg, brwBorder, brwText, 10.5f, 4.0f, 1.0f);

    // Floating tooltip badge on hover
    if (hoverX_ >= 0.0f && hoverY_ >= 0.0f) {
        std::string tip = getTooltip(hoverX_, hoverY_);
        if (!tip.empty()) {
            drawTooltipBadge(r, tip, hoverX_, bounds_.y + 10.0f, theme, false, bounds_.w, 0.0f, bounds_.y, bounds_.y + bounds_.h - 2.0f);
        }
    }
}

std::string TransportHeader::getTooltip(float x, float y) const noexcept {
    if (logoBounds_.contains(x, y)) {
        return "EATSBITS Project Hub & Settings";
    }
    if (playBounds_.contains(x, y)) {
        return "Play / Pause (Space)";
    }
    if (stopBounds_.contains(x, y)) {
        return "Stop Playback (Return)";
    }
    if (timeDisplayBounds_.contains(x, y)) {
        return "Song Position (Bar : Beat : Tick)";
    }
    if (bpmBounds_.contains(x, y)) {
        return "Project Tempo (BPM) - Click to Edit";
    }
    if (snapBounds_.contains(x, y)) {
        return "Timeline Grid Snap (1/16)";
    }
    if (loopBounds_.contains(x, y)) {
        return "Toggle Loop Playback (L)";
    }
    if (metroBounds_.contains(x, y)) {
        return "Metronome Click (C)";
    }
    if (browserBounds_.contains(x, y)) {
        return "Preset & Sample Browser (B)";
    }
    return "";
}

bool TransportHeader::handlePointer(const PointerEvent& ev, audio::AudioEngine* engine) {
    if (ev.action == PointerAction::Move || ev.action == PointerAction::Down) {
        hoverX_ = ev.x;
        hoverY_ = ev.y;
    }
    if (ev.action != PointerAction::Down) return false;

    if (logoBounds_.contains(ev.x, ev.y)) {
        if (onToggleProjectHub) onToggleProjectHub();
        return true;
    }
    if (bpmBounds_.contains(ev.x, ev.y)) {
        if (onOpenValueEdit) {
            float bVal = engine ? static_cast<float>(engine->getSequencer().getTransport().getBpm()) : 128.0f;
            ValueEditRequest req;
            req.title = "PROJECT TEMPO";
            req.paramName = "Tempo";
            req.currentValue = bVal;
            req.minValue = 20.0f;
            req.maxValue = 300.0f;
            req.defaultValue = 120.0f;
            req.hasDefault = true;
            req.allowPercentage = false;
            req.unit = "BPM";
            req.accentColor = Color(1.0f, 0.70f, 0.10f);
            req.onCommit = [engine](float val) {
                if (engine) engine->getSequencer().getTransport().setBpm(val);
            };
            onOpenValueEdit(req);
        }
        return true;
    }
    if (playBounds_.contains(ev.x, ev.y)) {
        if (onTogglePlay) onTogglePlay();
        else if (engine) {
            if (engine->getSequencer().isPlaying()) {
                engine->stopPlayback();
            } else {
                engine->getSequencer().start();
            }
        }
        return true;
    }
    if (stopBounds_.contains(ev.x, ev.y)) {
        if (onStop) onStop();
        else if (engine) engine->stopPlayback();
        return true;
    }
    if (loopBounds_.contains(ev.x, ev.y)) {
        loopActive_ = !loopActive_;
        if (onToggleLoop) onToggleLoop();
        return true;
    }
    if (metroBounds_.contains(ev.x, ev.y)) {
        metroActive_ = !metroActive_;
        if (onToggleMetronome) onToggleMetronome();
        return true;
    }
    if (browserBounds_.contains(ev.x, ev.y)) {
        if (onToggleBrowser) onToggleBrowser();
        return true;
    }

    return bounds_.contains(ev.x, ev.y);
}

} // namespace eatsbits::ui
