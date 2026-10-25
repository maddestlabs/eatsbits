#include "eatsbits/export/clap/gui/instrument_faceplate.hpp"
#include <clap/clap.h>
#include <clap/ext/params.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

namespace eatsbits::export_clap::gui {

InstrumentFaceplate::InstrumentFaceplate(IParamProvider* paramProvider,
                                         const clap_host* host,
                                         InstrumentGuiDesc desc) noexcept
    : paramProvider_(paramProvider), host_(host), desc_(std::move(desc)) {
    // Determine maximum parameter ID across all widgets
    uint32_t maxParamId = 0;
    for (const auto& row : desc_.rows) {
        for (const auto& w : row.widgets) {
            if (w.type != WidgetType::Divider && w.paramId > maxParamId) {
                maxParamId = w.paramId;
            }
        }
    }
    cachedParams_.assign(maxParamId + 1, 0.0);
    syncParamsFromHost();
}

void InstrumentFaceplate::syncParamsFromHost() noexcept {
    if (!paramProvider_) return;
    for (size_t i = 0; i < cachedParams_.size(); ++i) {
        cachedParams_[i] = paramProvider_->getParamValue(static_cast<uint32_t>(i));
    }
}

void InstrumentFaceplate::sendParamChange(uint32_t paramId, double value) noexcept {
    if (!paramProvider_) return;
    paramProvider_->setParamValue(paramId, value);
    if (paramId < cachedParams_.size()) {
        cachedParams_[paramId] = value;
    }
    isDirty_ = true;

    if (host_ && host_->get_extension) {
        const auto* hostParams = static_cast<const clap_host_params_t*>(
            host_->get_extension(host_, CLAP_EXT_PARAMS));
        if (hostParams && hostParams->request_flush) {
            hostParams->request_flush(host_);
        }
    }
}

bool InstrumentFaceplate::onTimer() noexcept {
    ++timerTick_;
    bool needRepaint = false;

    if (paramProvider_) {
        for (size_t i = 0; i < cachedParams_.size(); ++i) {
            const double cur = paramProvider_->getParamValue(static_cast<uint32_t>(i));
            if (std::abs(cur - cachedParams_[i]) > 1e-4) {
                cachedParams_[i] = cur;
                needRepaint = true;
            }
        }
    }

    if (isDirty_) {
        needRepaint = true;
        isDirty_ = false;
    }
    return needRepaint;
}

void InstrumentFaceplate::render(SoftwareCanvas& canvas) noexcept {
    const float w = static_cast<float>(canvas.width());
    const float h = static_cast<float>(canvas.height());
    const float scale = w / 800.0f;

    placedWidgets_.clear();

    // 1. Hardware Faceplate Background
    // Clean, modern minimal white hardware panel with brushed texture
    canvas.drawBrushedMetal(0, 0, canvas.width(), canvas.height(),
                           SoftwareCanvas::rgba(238, 240, 244, 255),
                           SoftwareCanvas::rgba(215, 218, 224, 255),
                           0.25f);

    // Beveled chassis border
    const int borderW = std::max(2, static_cast<int>(3.0f * scale));
    canvas.drawRectOutline(0, 0, canvas.width(), canvas.height(),
                          SoftwareCanvas::rgba(60, 64, 72, 255), borderW);
    canvas.drawRectOutline(borderW, borderW, canvas.width() - borderW * 2, canvas.height() - borderW * 2,
                          SoftwareCanvas::rgba(255, 255, 255, 180), 1.0f);

    // Corner Screws
    const float screwR = 4.0f * scale;
    const float screwMargin = 10.0f * scale;
    const float screwPositions[4][2] = {
        {screwMargin, screwMargin},
        {w - screwMargin, screwMargin},
        {screwMargin, h - screwMargin},
        {w - screwMargin, h - screwMargin}
    };
    for (const auto& sp : screwPositions) {
        canvas.drawCircle(sp[0], sp[1], screwR, SoftwareCanvas::rgba(140, 144, 150, 255));
        canvas.drawCircleOutline(sp[0], sp[1], screwR, SoftwareCanvas::rgba(80, 84, 90, 255), 1.0f);
        canvas.drawLine(sp[0] - screwR * 0.7f, sp[1], sp[0] + screwR * 0.7f, sp[1],
                        SoftwareCanvas::rgba(60, 64, 70, 255), 1.2f * scale);
    }

    // 2. Header Bar with Title and Subtitle
    const float headerH = 34.0f * scale;
    canvas.drawRect(static_cast<int>(18.0f * scale), static_cast<int>(12.0f * scale),
                    static_cast<int>(w - 36.0f * scale), static_cast<int>(headerH),
                    SoftwareCanvas::rgba(26, 28, 32, 255));
    canvas.drawRectOutline(static_cast<int>(18.0f * scale), static_cast<int>(12.0f * scale),
                           static_cast<int>(w - 36.0f * scale), static_cast<int>(headerH),
                           SoftwareCanvas::rgba(70, 74, 82, 255), 1.0f);

    canvas.drawString(desc_.title, 32.0f * scale, 20.0f * scale, 1.05f * scale,
                      SoftwareCanvas::rgba(240, 244, 252, 255), 1.4f);

    if (desc_.subtitle && desc_.subtitle[0] != '\0') {
        canvas.drawString(desc_.subtitle, 260.0f * scale, 22.0f * scale, 0.65f * scale,
                          SoftwareCanvas::rgba(175, 182, 195, 255), 1.1f);
    }

    // Decorative Horizontal Grooves
    const float grooveY = 12.0f * scale + headerH + 8.0f * scale;
    canvas.drawLine(18.0f * scale, grooveY, w - 18.0f * scale, grooveY,
                    SoftwareCanvas::rgba(150, 155, 162, 255), 1.2f * scale);
    canvas.drawLine(18.0f * scale, grooveY + 1.0f, w - 18.0f * scale, grooveY + 1.0f,
                    SoftwareCanvas::rgba(255, 255, 255, 180), 1.0f * scale);

    // 3. Declarative Rows Layout
    const size_t numRows = desc_.rows.size();
    if (numRows == 0) return;

    const float availY = grooveY + 10.0f * scale;
    const float availH = h - availY - 12.0f * scale;
    const float rowH = availH / static_cast<float>(numRows);
    const float kRad = 22.0f * scale;

    char readoutBuf[32];

    for (size_t r = 0; r < numRows; ++r) {
        const auto& row = desc_.rows[r];
        const size_t numWidgets = row.widgets.size();
        if (numWidgets == 0) continue;

        const float rowCenterY = availY + (static_cast<float>(r) + 0.5f) * rowH - 6.0f * scale;
        const float startX = 48.0f * scale;
        const float endX = w - 48.0f * scale;
        const float stepX = (numWidgets > 1) ? ((endX - startX) / static_cast<float>(numWidgets - 1)) : 0.0f;

        for (size_t wi = 0; wi < numWidgets; ++wi) {
            const auto& wdesc = row.widgets[wi];
            const float cx = startX + static_cast<float>(wi) * stepX;
            const float cy = rowCenterY;

            if (wdesc.type == WidgetType::Divider) {
                // Vertical divider line
                canvas.drawLine(cx, cy - rowH * 0.38f, cx, cy + rowH * 0.38f,
                                SoftwareCanvas::rgba(160, 165, 172, 255), 1.0f);
                canvas.drawLine(cx + 1.0f, cy - rowH * 0.38f, cx + 1.0f, cy + rowH * 0.38f,
                                SoftwareCanvas::rgba(255, 255, 255, 180), 1.0f);
                continue;
            }

            PlacedWidget pw;
            pw.type = wdesc.type;
            pw.paramId = wdesc.paramId;
            pw.label = wdesc.label;
            pw.style = wdesc.style;
            pw.minValue = wdesc.minValue;
            pw.maxValue = wdesc.maxValue;
            pw.defaultValue = wdesc.defaultValue;
            pw.isStepped = wdesc.isStepped;
            pw.cx = cx;
            pw.cy = cy;
            pw.radius = kRad;
            pw.w = 32.0f * scale;
            pw.h = 50.0f * scale;

            const size_t widgetIdx = placedWidgets_.size();
            placedWidgets_.push_back(pw);

            const double curVal = (pw.paramId < cachedParams_.size()) ? cachedParams_[pw.paramId] : pw.defaultValue;
            const bool isHover = (hoveredIndex_ == static_cast<int>(widgetIdx));
            const bool isActive = (activeIndex_ == static_cast<int>(widgetIdx));

            if (pw.type == WidgetType::Switch) {
                renderSwitch(canvas, pw, curVal >= 0.5, isHover);
            } else {
                const double span = (pw.maxValue > pw.minValue) ? (pw.maxValue - pw.minValue) : 1.0;
                const float normVal = static_cast<float>(std::clamp((curVal - pw.minValue) / span, 0.0, 1.0));

                if (paramProvider_) {
                    paramProvider_->valueToText(pw.paramId, curVal, readoutBuf, sizeof(readoutBuf));
                } else {
                    std::snprintf(readoutBuf, sizeof(readoutBuf), "%.2f", curVal);
                }

                if (pw.style == KnobStyle::Selector) {
                    renderSelectorKnob(canvas, pw, normVal, isActive, isHover, readoutBuf);
                } else if (pw.style == KnobStyle::CreamFluted) {
                    renderCreamFlutedKnob(canvas, pw, normVal, isActive, isHover, readoutBuf);
                } else {
                    renderPotentiometerKnob(canvas, pw, normVal, isActive, isHover, readoutBuf);
                }
            }
        }
    }
}

void InstrumentFaceplate::renderPotentiometerKnob(SoftwareCanvas& canvas, const PlacedWidget& pw,
                                                  float normVal, bool isActive, bool isHovered,
                                                  const char* readout) noexcept {
    constexpr float minAngle = -2.35619449f; // -135 deg
    constexpr float maxAngle = 2.35619449f;  // +135 deg
    const float currentAngle = minAngle + normVal * (maxAngle - minAngle);
    constexpr float lightAngle = -0.785398f; // -45 deg light

    const float cx = pw.cx;
    const float cy = pw.cy;
    const float kRad = pw.radius;

    // 1. Recessed Beveled Well
    const float wellOuterR = kRad * 1.48f;
    const float wellInnerR = kRad * 1.30f;

    canvas.drawCircle(cx, cy, wellOuterR, SoftwareCanvas::rgba(30, 32, 36, 255));
    canvas.drawArc(cx, cy, wellInnerR, lightAngle - 1.57f, lightAngle + 1.57f,
                   SoftwareCanvas::rgba(15, 17, 20, 240), 2.0f);
    canvas.drawArc(cx, cy, wellInnerR, lightAngle + 1.57f, lightAngle + 4.71f,
                   SoftwareCanvas::rgba(215, 222, 235, 220), 1.5f);
    canvas.drawCircle(cx, cy, wellInnerR - 1.0f, SoftwareCanvas::rgba(44, 46, 52, 255));

    // 2. 11 Radial Hash Dial Ticks
    constexpr int kTickCount = 11;
    const float tickStep = (maxAngle - minAngle) / static_cast<float>(kTickCount - 1);
    for (int t = 0; t < kTickCount; ++t) {
        const float a = minAngle + static_cast<float>(t) * tickStep;
        const float sa = std::sin(a);
        const float ca = std::cos(a);
        const bool isCenter = (t == (kTickCount / 2));
        const float rStart = wellOuterR + 1.5f;
        const float rEnd = rStart + (isCenter ? 6.5f : 4.0f);

        canvas.drawLine(cx + sa * rStart, cy - ca * rStart,
                        cx + sa * rEnd, cy - ca * rEnd,
                        SoftwareCanvas::rgba(24, 26, 32, 230), isCenter ? 2.0f : 1.2f);
    }

    // 3. Drop shadow onto well
    canvas.drawCircleDropShadow(cx, cy, kRad * 1.05f, 6.0f, lightAngle, 0.60f);

    // 4. Knurled Fluted Rotor Grip
    canvas.drawKnurledRing(cx, cy, kRad * 0.70f, kRad * 1.05f, 36, currentAngle, lightAngle,
                          SoftwareCanvas::rgba(180, 185, 195, 255),
                          SoftwareCanvas::rgba(255, 255, 255, 255),
                          SoftwareCanvas::rgba(38, 40, 46, 255));

    // 5. Center Rotor Cap
    const float capR = kRad * 0.72f;
    const float offX = std::cos(lightAngle) * capR * 0.35f;
    const float offY = std::sin(lightAngle) * capR * 0.35f;

    canvas.drawCircleRadialGradient(cx, cy, capR,
                                   SoftwareCanvas::rgba(250, 252, 255, 255),
                                   SoftwareCanvas::rgba(120, 126, 136, 255),
                                   offX, offY);
    canvas.drawCircleOutline(cx, cy, capR, SoftwareCanvas::rgba(90, 94, 102, 255), 1.0f);

    // 6. Inlaid Indicator Line
    const float sa = std::sin(currentAngle);
    const float ca = std::cos(currentAngle);
    const float iStart = capR * 0.20f;
    const float iEnd = kRad * 1.05f - 0.5f;

    canvas.drawLine(cx + sa * iStart + 0.6f, cy - ca * iStart + 0.6f,
                    cx + sa * iEnd + 0.6f, cy - ca * iEnd + 0.6f,
                    SoftwareCanvas::rgba(30, 32, 38, 220), 2.2f);

    uint32_t needleCol = SoftwareCanvas::rgba(240, 244, 250, 255);
    if (isActive) {
        needleCol = SoftwareCanvas::rgba(255, 220, 25, 255); // Acid Neon Feedback
    } else if (isHovered) {
        needleCol = SoftwareCanvas::rgba(70, 210, 255, 255); // Cyan Hover
    }
    canvas.drawLine(cx + sa * iStart, cy - ca * iStart,
                    cx + sa * iEnd, cy - ca * iEnd,
                    needleCol, 2.0f);

    // 7. Label and Readout Tooltip
    canvas.drawStringCentered(pw.label, cx, cy + wellOuterR + 9.0f, 0.55f,
                              SoftwareCanvas::rgba(28, 30, 36, 255), 1.1f);

    if (isActive || isHovered) {
        const float badgeW = canvas.measureStringWidth(readout, 0.52f) + 12.0f;
        const float badgeH = 16.0f;
        const float badgeX = cx - badgeW * 0.5f;
        const float badgeY = cy - wellOuterR - 16.0f;

        canvas.drawRoundedRect(badgeX, badgeY, badgeW, badgeH, 3.0f,
                               SoftwareCanvas::rgba(18, 20, 24, 235));
        canvas.drawRoundedRectOutline(badgeX, badgeY, badgeW, badgeH, 3.0f,
                                      isActive ? SoftwareCanvas::rgba(255, 220, 25, 255)
                                               : SoftwareCanvas::rgba(70, 210, 255, 255),
                                      1.0f);
        canvas.drawStringCentered(readout, cx, badgeY + badgeH * 0.5f, 0.52f,
                                  SoftwareCanvas::rgba(245, 248, 255, 255), 1.0f);
    }
}

void InstrumentFaceplate::renderSelectorKnob(SoftwareCanvas& canvas, const PlacedWidget& pw,
                                             float normVal, bool isActive, bool isHovered,
                                             const char* readout) noexcept {
    constexpr float minAngle = -1.8f;
    constexpr float maxAngle = 1.8f;
    const float currentAngle = minAngle + normVal * (maxAngle - minAngle);

    const float cx = pw.cx;
    const float cy = pw.cy;
    const float kRad = pw.radius;

    // Stepped Selector Dial Face
    canvas.drawCircle(cx, cy, kRad * 1.35f, SoftwareCanvas::rgba(35, 38, 44, 255));
    canvas.drawCircleOutline(cx, cy, kRad * 1.35f, SoftwareCanvas::rgba(70, 75, 85, 255), 1.5f);

    // Detent Ticks
    constexpr int detentCount = 5;
    for (int d = 0; d < detentCount; ++d) {
        const float a = minAngle + (maxAngle - minAngle) * (static_cast<float>(d) / static_cast<float>(detentCount - 1));
        const float sa = std::sin(a);
        const float ca = std::cos(a);
        canvas.drawLine(cx + sa * (kRad * 1.35f + 2.0f), cy - ca * (kRad * 1.35f + 2.0f),
                        cx + sa * (kRad * 1.35f + 6.0f), cy - ca * (kRad * 1.35f + 6.0f),
                        SoftwareCanvas::rgba(30, 32, 38, 240), 1.8f);
    }

    // Rotor Body (Bakelite Stepped Boss)
    canvas.drawCircle(cx, cy, kRad * 0.95f, SoftwareCanvas::rgba(55, 58, 66, 255));
    canvas.drawCircleOutline(cx, cy, kRad * 0.95f, SoftwareCanvas::rgba(25, 26, 30, 255), 1.2f);

    // Metallic Skirt
    canvas.drawCircle(cx, cy, kRad * 0.68f, SoftwareCanvas::rgba(200, 205, 215, 255));
    canvas.drawCircleOutline(cx, cy, kRad * 0.68f, SoftwareCanvas::rgba(90, 95, 105, 255), 1.0f);

    // Pointer lever indicator
    const float sa = std::sin(currentAngle);
    const float ca = std::cos(currentAngle);
    canvas.drawLine(cx, cy, cx + sa * (kRad * 1.15f), cy - ca * (kRad * 1.15f),
                    isActive ? SoftwareCanvas::rgba(255, 220, 25, 255)
                             : (isHovered ? SoftwareCanvas::rgba(70, 210, 255, 255)
                                          : SoftwareCanvas::rgba(240, 245, 252, 255)),
                    2.4f);

    canvas.drawStringCentered(pw.label, cx, cy + kRad * 1.35f + 9.0f, 0.55f,
                              SoftwareCanvas::rgba(28, 30, 36, 255), 1.1f);

    if (isActive || isHovered) {
        const float badgeW = canvas.measureStringWidth(readout, 0.52f) + 12.0f;
        const float badgeH = 16.0f;
        const float badgeX = cx - badgeW * 0.5f;
        const float badgeY = cy - kRad * 1.35f - 16.0f;

        canvas.drawRoundedRect(badgeX, badgeY, badgeW, badgeH, 3.0f,
                               SoftwareCanvas::rgba(18, 20, 24, 235));
        canvas.drawStringCentered(readout, cx, badgeY + badgeH * 0.5f, 0.52f,
                                  SoftwareCanvas::rgba(245, 248, 255, 255), 1.0f);
    }
}

void InstrumentFaceplate::renderCreamFlutedKnob(SoftwareCanvas& canvas, const PlacedWidget& pw,
                                                float normVal, bool isActive, bool isHovered,
                                                const char* readout) noexcept {
    constexpr float minAngle = -2.35619449f;
    constexpr float maxAngle = 2.35619449f;
    const float currentAngle = minAngle + normVal * (maxAngle - minAngle);

    const float cx = pw.cx;
    const float cy = pw.cy;
    const float kRad = pw.radius;

    // Outer Skirt Collet (Cream / Chrome)
    canvas.drawCircle(cx, cy, kRad * 1.25f, SoftwareCanvas::rgba(235, 232, 222, 255));
    canvas.drawCircleOutline(cx, cy, kRad * 1.25f, SoftwareCanvas::rgba(90, 88, 82, 255), 1.2f);

    // Fluted Ridges (18 flutes)
    constexpr int flutes = 18;
    for (int f = 0; f < flutes; ++f) {
        const float fa = currentAngle + (static_cast<float>(f) / static_cast<float>(flutes)) * 6.2831853f;
        const float sa = std::sin(fa);
        const float ca = std::cos(fa);
        canvas.drawLine(cx + sa * (kRad * 0.85f), cy - ca * (kRad * 0.85f),
                        cx + sa * (kRad * 1.20f), cy - ca * (kRad * 1.20f),
                        SoftwareCanvas::rgba(80, 78, 72, 160), 1.5f);
    }

    // Inner Cream Cap
    canvas.drawCircle(cx, cy, kRad * 0.75f, SoftwareCanvas::rgba(246, 244, 236, 255));
    canvas.drawCircleOutline(cx, cy, kRad * 0.75f, SoftwareCanvas::rgba(140, 136, 128, 255), 1.0f);

    // Pointer Dot / Notch
    const float sa = std::sin(currentAngle);
    const float ca = std::cos(currentAngle);
    canvas.drawLine(cx + sa * (kRad * 0.20f), cy - ca * (kRad * 0.20f),
                    cx + sa * (kRad * 1.15f), cy - ca * (kRad * 1.15f),
                    isActive ? SoftwareCanvas::rgba(255, 140, 20, 255)
                             : (isHovered ? SoftwareCanvas::rgba(20, 180, 255, 255)
                                          : SoftwareCanvas::rgba(40, 38, 34, 255)),
                    2.2f);

    canvas.drawStringCentered(pw.label, cx, cy + kRad * 1.25f + 9.0f, 0.55f,
                              SoftwareCanvas::rgba(28, 30, 36, 255), 1.1f);

    if (isActive || isHovered) {
        const float badgeW = canvas.measureStringWidth(readout, 0.52f) + 12.0f;
        const float badgeH = 16.0f;
        const float badgeX = cx - badgeW * 0.5f;
        const float badgeY = cy - kRad * 1.25f - 16.0f;

        canvas.drawRoundedRect(badgeX, badgeY, badgeW, badgeH, 3.0f,
                               SoftwareCanvas::rgba(18, 20, 24, 235));
        canvas.drawStringCentered(readout, cx, badgeY + badgeH * 0.5f, 0.52f,
                                  SoftwareCanvas::rgba(245, 248, 255, 255), 1.0f);
    }
}

void InstrumentFaceplate::renderSwitch(SoftwareCanvas& canvas, const PlacedWidget& pw,
                                      bool isOn, bool isHovered) noexcept {
    const float x = pw.cx - pw.w * 0.5f;
    const float y = pw.cy - pw.h * 0.5f;

    // Recessed switch well
    canvas.drawRoundedRect(x, y, pw.w, pw.h, 3.0f, SoftwareCanvas::rgba(32, 34, 38, 255));
    canvas.drawRoundedRectOutline(x, y, pw.w, pw.h, 3.0f, SoftwareCanvas::rgba(70, 74, 82, 255), 1.2f);

    // Mechanical toggle bat
    const float batH = pw.h * 0.44f;
    const float batW = pw.w - 4.0f;
    const float batX = x + 2.0f;
    const float batY = isOn ? (y + pw.h - batH - 2.0f) : (y + 2.0f);

    canvas.drawRoundedRect(batX, batY, batW, batH, 2.5f,
                           isHovered ? SoftwareCanvas::rgba(225, 230, 240, 255)
                                     : SoftwareCanvas::rgba(185, 190, 200, 255));
    canvas.drawRoundedRectOutline(batX, batY, batW, batH, 2.5f,
                                  SoftwareCanvas::rgba(75, 80, 88, 255), 1.0f);

    canvas.drawStringCentered(pw.label, pw.cx, y + pw.h + 12.0f, 0.55f,
                              SoftwareCanvas::rgba(28, 30, 36, 255), 1.1f);
}

bool InstrumentFaceplate::onMouseDown(int x, int y, bool /*isShift*/) noexcept {
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);

    for (size_t i = 0; i < placedWidgets_.size(); ++i) {
        const auto& pw = placedWidgets_[i];
        if (pw.type == WidgetType::Switch) {
            if (std::abs(fx - pw.cx) <= pw.w * 0.5f && std::abs(fy - pw.cy) <= pw.h * 0.5f) {
                const double cur = (pw.paramId < cachedParams_.size()) ? cachedParams_[pw.paramId] : 0.0;
                sendParamChange(pw.paramId, (cur < 0.5) ? 1.0 : 0.0);
                return true;
            }
        } else {
            if (std::hypot(fx - pw.cx, fy - pw.cy) <= pw.radius * 1.5f) {
                activeIndex_ = static_cast<int>(i);
                dragStartY_ = y;
                dragStartVal_ = (pw.paramId < cachedParams_.size()) ? cachedParams_[pw.paramId] : pw.defaultValue;
                isDirty_ = true;
                return true;
            }
        }
    }
    return false;
}

bool InstrumentFaceplate::onMouseMove(int x, int y, bool isLeftDown, bool isShift) noexcept {
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);

    if (activeIndex_ >= 0 && isLeftDown) {
        const auto& pw = placedWidgets_[static_cast<size_t>(activeIndex_)];
        const int deltaY = dragStartY_ - y;
        const double sensitivity = isShift ? 0.001 : 0.006;
        const double span = pw.maxValue - pw.minValue;

        double newVal = dragStartVal_ + static_cast<double>(deltaY) * sensitivity * span;
        if (pw.isStepped) {
            newVal = std::round(newVal);
        }
        newVal = std::clamp(newVal, pw.minValue, pw.maxValue);
        sendParamChange(pw.paramId, newVal);
        return true;
    }

    int newHover = -1;
    for (size_t i = 0; i < placedWidgets_.size(); ++i) {
        const auto& pw = placedWidgets_[i];
        if (pw.type == WidgetType::Switch) {
            if (std::abs(fx - pw.cx) <= pw.w * 0.5f && std::abs(fy - pw.cy) <= pw.h * 0.5f) {
                newHover = static_cast<int>(i);
                break;
            }
        } else {
            if (std::hypot(fx - pw.cx, fy - pw.cy) <= pw.radius * 1.5f) {
                newHover = static_cast<int>(i);
                break;
            }
        }
    }

    if (newHover != hoveredIndex_) {
        hoveredIndex_ = newHover;
        isDirty_ = true;
        return true;
    }
    return false;
}

bool InstrumentFaceplate::onMouseUp(int /*x*/, int /*y*/) noexcept {
    if (activeIndex_ >= 0) {
        activeIndex_ = -1;
        isDirty_ = true;
        return true;
    }
    return false;
}

bool InstrumentFaceplate::onMouseLeave() noexcept {
    if (hoveredIndex_ >= 0 || activeIndex_ >= 0) {
        hoveredIndex_ = -1;
        activeIndex_ = -1;
        isDirty_ = true;
        return true;
    }
    return false;
}

} // namespace eatsbits::export_clap::gui
