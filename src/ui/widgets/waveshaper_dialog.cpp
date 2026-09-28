#include "eatsbits/ui/widgets/waveshaper_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace eatsbits::ui {

namespace {

inline float fastTanh(float x) noexcept {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

} // namespace

float WaveshaperDialog::evaluateTransfer(const WaveshaperParams& params, float x) noexcept {
    const float xPre = std::clamp(x * params.preGain, -8.0f, 8.0f);
    float y = xPre;

    switch (params.shape) {
        case WaveshaperShape::SoftSaturation: {
            const float k = 1.0f + (params.tension + 1.0f) * 3.0f;
            y = fastTanh(xPre * k) / fastTanh(k);
            break;
        }
        case WaveshaperShape::TubeAsymmetric: {
            if (xPre > 0.0f) {
                y = 1.0f - std::exp(-xPre * (2.0f + params.tension * 1.5f));
            } else {
                y = -(1.0f - std::exp(xPre * (1.2f - params.tension * 0.5f))) * 0.85f;
            }
            break;
        }
        case WaveshaperShape::SineWavefold: {
            constexpr float PI = 3.14159265f;
            y = std::sin(xPre * PI * (1.0f + (params.tension + 1.0f) * 0.7f));
            break;
        }
        case WaveshaperShape::Angry1: {
            const float foldVal = std::fmod(std::abs(xPre * (2.0f + params.tension * 1.5f)), 2.0f);
            y = (foldVal > 1.0f ? 2.0f - foldVal : foldVal) * 2.0f - 1.0f;
            if (xPre < 0.0f) y = -y;
            break;
        }
        case WaveshaperShape::Angry2: {
            constexpr float PI = 3.14159265f;
            const float fold2 = std::sin(xPre * PI * 2.0f * (1.2f + params.tension));
            y = fastTanh(fold2 * 2.5f);
            break;
        }
    }

    y = std::clamp(y * params.postGain, -1.0f, 1.0f);

    // Dry/Wet blend
    if (params.dryWet < 0.999f) {
        y = (1.0f - params.dryWet) * x + params.dryWet * y;
    }

    return y;
}

WaveshaperDialog::WaveshaperDialog() {
    harmonics_.fill(0.0f);
    updateHarmonics();
}

void WaveshaperDialog::open(const WaveshaperParams& initialParams) {
    params_ = initialParams;
    isOpen_ = true;
    updateHarmonics();
}

void WaveshaperDialog::updateHarmonics() noexcept {
    // Generate test sinusoidal probe signal through the transfer curve
    constexpr size_t N = 128;
    std::array<float, N> signal{};
    constexpr float TWO_PI = 6.283185307f;

    for (size_t n = 0; n < N; ++n) {
        float inSample = std::sin(TWO_PI * static_cast<float>(n) / static_cast<float>(N));
        signal[n] = evaluateTransfer(params_, inSample);
    }

    // DC removal if filter is active
    if (params_.dcFilter) {
        float mean = 0.0f;
        for (float s : signal) mean += s;
        mean /= static_cast<float>(N);
        for (float& s : signal) s -= mean;
    }

    // Direct Discrete Fourier Transform for harmonics 1 to 8
    float maxHarmonic = 0.0001f;
    for (size_t k = 1; k <= 8; ++k) {
        float re = 0.0f;
        float im = 0.0f;
        for (size_t n = 0; n < N; ++n) {
            float phase = TWO_PI * static_cast<float>(k * n) / static_cast<float>(N);
            re += signal[n] * std::cos(phase);
            im -= signal[n] * std::sin(phase);
        }
        float mag = (2.0f / static_cast<float>(N)) * std::sqrt(re * re + im * im);
        harmonics_[k - 1] = mag;
        if (mag > maxHarmonic) maxHarmonic = mag;
    }

    // Normalize harmonic amplitudes for visualization
    for (size_t k = 0; k < 8; ++k) {
        harmonics_[k] = std::clamp(harmonics_[k] / maxHarmonic, 0.0f, 1.0f);
    }
}

void WaveshaperDialog::layout(float screenW, float screenH) {
    const float w = std::min(640.0f, screenW - 40.0f);
    const float h = std::min(520.0f, screenH - 40.0f);
    const float x = (screenW - w) * 0.5f;
    const float y = (screenH - h) * 0.5f;

    dialogBounds_ = Rect2D(x, y, w, h);

    // Preset pills row
    const float pillY = y + 54.0f;
    const float pillW = (w - 48.0f) / 5.0f;
    shapePillBounds_.clear();
    for (size_t i = 0; i < 5; ++i) {
        shapePillBounds_.push_back(Rect2D(x + 24.0f + i * pillW, pillY, pillW - 6.0f, 26.0f));
    }

    // 2D Transfer Curve Canvas
    const float canvasW = w * 0.56f;
    const float canvasH = 240.0f;
    curveCanvasBounds_ = Rect2D(x + 24.0f, pillY + 36.0f, canvasW, canvasH);

    // Harmonics Bar Graph
    const float harmX = curveCanvasBounds_.right() + 16.0f;
    const float harmW = (x + w - 24.0f) - harmX;
    harmonicsBounds_ = Rect2D(harmX, curveCanvasBounds_.y, harmW, canvasH);

    // Control Sliders (Pre-Gain, Post-Gain, Dry/Wet)
    const float sliderY = curveCanvasBounds_.bottom() + 16.0f;
    const float sliderW = (w - 48.0f - 32.0f) / 3.0f;
    preGainBounds_ = Rect2D(x + 24.0f, sliderY, sliderW, 40.0f);
    postGainBounds_ = Rect2D(preGainBounds_.right() + 16.0f, sliderY, sliderW, 40.0f);
    dryWetBounds_ = Rect2D(postGainBounds_.right() + 16.0f, sliderY, sliderW, 40.0f);

    // Badges / Options (DC Filter, Oversampling)
    const float optY = sliderY + 50.0f;
    dcFilterBounds_ = Rect2D(x + 24.0f, optY, 110.0f, 28.0f);
    oversamplingBounds_ = Rect2D(dcFilterBounds_.right() + 12.0f, optY, 120.0f, 28.0f);

    // Bottom Action Buttons
    applyBtnBounds_ = Rect2D(x + w - 144.0f, optY, 120.0f, 32.0f);
    closeBtnBounds_ = Rect2D(applyBtnBounds_.x - 90.0f, optY, 80.0f, 32.0f);
}

void WaveshaperDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-transparent backdrop overlay
    drawRect(r, 0.0f, 0.0f, 4000.0f, 4000.0f, Color(0.0f, 0.0f, 0.0f, 0.72f));

    // 2. Dialog Chassis with Primary Accent Glow Border
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f, theme.panelBackground);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f, theme.primaryAccent, 1.8f);

    // Header Title & Icon
    drawText(r, "WAVESHAPER TRANSFER CURVE EDITOR", dialogBounds_.x + 24.0f, dialogBounds_.y + 20.0f, 13.0f, theme.primaryAccent);
    drawText(r, "Non-linear transfer distortion, wavefolding & harmonic saturation", dialogBounds_.x + 24.0f, dialogBounds_.y + 36.0f, 9.5f, theme.textMuted);

    // 3. Preset Shape Selection Pills
    static const char* shapeLabels[] = {
        "SOFT TANH", "TUBE DRIVE", "SINE FOLD", "ANGRY 1", "ANGRY 2"
    };
    for (size_t i = 0; i < shapePillBounds_.size(); ++i) {
        const auto& pb = shapePillBounds_[i];
        bool selected = (static_cast<size_t>(params_.shape) == i);
        Color fillCol = selected ? theme.primaryAccent : theme.panelHeader;
        Color textCol = selected ? Color(0.05f, 0.08f, 0.12f, 1.0f) : theme.textSecondary;

        drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 4.0f, fillCol);
        if (!selected) {
            drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 4.0f, theme.borderSubtle, 1.0f);
        }
        drawCenteredText(r, shapeLabels[i], pb, 9.5f, textCol);
    }

    // 4. Transfer Curve Canvas
    const auto& cb = curveCanvasBounds_;
    drawRoundedRect(r, cb.x, cb.y, cb.w, cb.h, 6.0f, Color(0.04f, 0.06f, 0.09f, 1.0f));
    drawRoundedRectOutline(r, cb.x, cb.y, cb.w, cb.h, 6.0f, Color(0.18f, 0.24f, 0.34f, 1.0f), 1.2f);

    // Grid lines
    constexpr int numDivs = 8;
    for (int i = 1; i < numDivs; ++i) {
        float gx = cb.x + (cb.w / numDivs) * static_cast<float>(i);
        float gy = cb.y + (cb.h / numDivs) * static_cast<float>(i);
        drawLine(r, gx, cb.y, gx, cb.bottom(), Color(0.12f, 0.16f, 0.24f, 0.8f), 0.8f);
        drawLine(r, cb.x, gy, cb.right(), gy, Color(0.12f, 0.16f, 0.24f, 0.8f), 0.8f);
    }

    // Zero-crossing axes
    const float cx = cb.x + cb.w * 0.5f;
    const float cy = cb.y + cb.h * 0.5f;
    drawLine(r, cx, cb.y, cx, cb.bottom(), Color(0.25f, 0.35f, 0.50f, 0.9f), 1.2f);
    drawLine(r, cb.x, cy, cb.right(), cy, Color(0.25f, 0.35f, 0.50f, 0.9f), 1.2f);

    // Diagonal Linear Reference
    drawLine(r, cb.x, cb.bottom(), cb.right(), cb.y, Color(0.20f, 0.26f, 0.36f, 0.7f), 1.0f);

    // Transfer curve sampling & rendering
    constexpr size_t NUM_POINTS = 64;
    float prevX = 0.0f;
    float prevY = 0.0f;

    for (size_t i = 0; i <= NUM_POINTS; ++i) {
        float normX = (static_cast<float>(i) / static_cast<float>(NUM_POINTS)) * 2.0f - 1.0f;
        float normY = evaluateTransfer(params_, normX);

        float scrX = cb.x + (normX + 1.0f) * 0.5f * cb.w;
        float scrY = cb.y + (1.0f - (normY + 1.0f) * 0.5f) * cb.h;

        if (i > 0) {
            drawLine(r, prevX, prevY, scrX, scrY, Color(0.0f, 0.90f, 1.0f, 0.95f), 2.2f);
        }
        prevX = scrX;
        prevY = scrY;
    }

    // Tension Control Handle at center
    const float handleX = cx;
    const float handleY = cb.y + (1.0f - ((params_.tension * 0.5f) + 0.5f)) * cb.h;
    drawCircle(r, handleX, handleY, 8.0f, Color(0.0f, 0.90f, 1.0f, 0.35f));
    drawCircle(r, handleX, handleY, 5.0f, Color(1.0f, 1.0f, 1.0f, 1.0f));
    drawCircleOutline(r, handleX, handleY, 5.0f, theme.primaryAccent, 1.5f);

    // Tension readout badge on canvas
    std::ostringstream tensSs;
    tensSs << "TENSION: " << (params_.tension >= 0 ? "+" : "") << std::fixed << std::setprecision(2) << params_.tension;
    drawText(r, tensSs.str(), cb.x + 8.0f, cb.y + 8.0f, 9.0f, theme.textSecondary);
    drawText(r, "DRAG CURVE TO TWEAK", cb.x + 8.0f, cb.bottom() - 16.0f, 8.5f, theme.textMuted);

    // 5. Harmonic Spectrum Visualizer
    const auto& hb = harmonicsBounds_;
    drawRoundedRect(r, hb.x, hb.y, hb.w, hb.h, 6.0f, Color(0.06f, 0.08f, 0.12f, 1.0f));
    drawRoundedRectOutline(r, hb.x, hb.y, hb.w, hb.h, 6.0f, Color(0.18f, 0.24f, 0.34f, 1.0f), 1.0f);
    drawText(r, "HARMONIC SPECTRUM", hb.x + 8.0f, hb.y + 8.0f, 9.5f, theme.textPrimary);

    const float barAreaY = hb.y + 26.0f;
    const float barAreaH = hb.h - 46.0f;
    const float barW = (hb.w - 24.0f) / 8.0f;

    for (size_t k = 0; k < 8; ++k) {
        float mag = harmonics_[k];
        float bh = barAreaH * mag;
        float bx = hb.x + 12.0f + k * barW;
        float by = barAreaY + barAreaH - bh;

        Color barCol = (k == 0) ? theme.primaryAccent : Color(1.0f, 0.20f + k * 0.10f, 0.40f, 0.9f);
        drawRoundedRect(r, bx + 2.0f, by, barW - 4.0f, bh, 2.0f, barCol);

        // Label (1f, 2f, etc.)
        std::string lbl = std::to_string(k + 1) + "f";
        drawText(r, lbl, bx + 4.0f, barAreaY + barAreaH + 4.0f, 8.0f, theme.textMuted);
    }

    // 6. Sliders (Pre-Gain, Post-Gain, Dry/Wet)
    auto drawSlider = [&](const Rect2D& b, const std::string& name, float val, float minV, float maxV, const std::string& unit) {
        drawRoundedRect(r, b.x, b.y, b.w, b.h, 4.0f, theme.panelHeader);
        drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 4.0f, theme.borderSubtle, 1.0f);

        std::ostringstream ss;
        ss << name << ": " << std::fixed << std::setprecision(2) << val << unit;
        drawText(r, ss.str(), b.x + 8.0f, b.y + 5.0f, 9.0f, theme.textSecondary);

        // Slider track
        float trackY = b.y + 24.0f;
        float trackW = b.w - 16.0f;
        drawRoundedRect(r, b.x + 8.0f, trackY, trackW, 4.0f, 2.0f, Color(0.10f, 0.14f, 0.20f, 1.0f));

        float norm = std::clamp((val - minV) / (maxV - minV), 0.0f, 1.0f);
        drawRoundedRect(r, b.x + 8.0f, trackY, trackW * norm, 4.0f, 2.0f, theme.primaryAccent);
        drawCircle(r, b.x + 8.0f + trackW * norm, trackY + 2.0f, 5.0f, Color(1.0f, 1.0f, 1.0f, 1.0f));
    };

    drawSlider(preGainBounds_, "DRIVE (PRE)", params_.preGain, 0.1f, 4.0f, "x");
    drawSlider(postGainBounds_, "LEVEL (POST)", params_.postGain, 0.0f, 2.0f, "x");
    drawSlider(dryWetBounds_, "DRY / WET", params_.dryWet * 100.0f, 0.0f, 100.0f, "%");

    // 7. DC Filter & Oversampling Toggles
    drawRoundedRect(r, dcFilterBounds_.x, dcFilterBounds_.y, dcFilterBounds_.w, dcFilterBounds_.h, 4.0f,
                    params_.dcFilter ? Color(0.10f, 0.35f, 0.20f, 0.9f) : theme.panelHeader);
    drawRoundedRectOutline(r, dcFilterBounds_.x, dcFilterBounds_.y, dcFilterBounds_.w, dcFilterBounds_.h, 4.0f,
                           params_.dcFilter ? Color(0.20f, 0.85f, 0.40f, 0.8f) : theme.borderSubtle, 1.0f);
    drawText(r, params_.dcFilter ? "DC FILTER: ON" : "DC FILTER: OFF", dcFilterBounds_.x + 10.0f, dcFilterBounds_.y + 8.0f,
             9.0f, params_.dcFilter ? Color(0.20f, 0.85f, 0.40f, 1.0f) : theme.textMuted);

    drawRoundedRect(r, oversamplingBounds_.x, oversamplingBounds_.y, oversamplingBounds_.w, oversamplingBounds_.h, 4.0f, theme.panelHeader);
    drawRoundedRectOutline(r, oversamplingBounds_.x, oversamplingBounds_.y, oversamplingBounds_.w, oversamplingBounds_.h, 4.0f, theme.borderSubtle, 1.0f);
    std::string osLbl = "OVERSAMPLE: " + std::to_string(params_.oversampling) + "X";
    drawText(r, osLbl, oversamplingBounds_.x + 10.0f, oversamplingBounds_.y + 8.0f, 9.0f, theme.primaryAccent);

    // 8. Buttons
    drawButton(r, closeBtnBounds_, "CANCEL", theme.panelHeader, theme.borderSubtle, theme.textSecondary, 10.0f);
    drawButton(r, applyBtnBounds_, "APPLY SHAPER", theme.primaryAccent, theme.borderFocus, Color(0.05f, 0.08f, 0.12f, 1.0f), 10.0f);
}

bool WaveshaperDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;

    // Check click outside dialog bounds to dismiss
    if (!dialogBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            close();
            return true;
        }
        return false;
    }

    if (ev.action == PointerAction::Down) {
        // Preset shape pills
        for (size_t i = 0; i < shapePillBounds_.size(); ++i) {
            if (shapePillBounds_[i].contains(ev.x, ev.y)) {
                params_.shape = static_cast<WaveshaperShape>(i);
                updateHarmonics();
                if (onParamsChanged) onParamsChanged(params_);
                return true;
            }
        }

        // Curve canvas drag start
        if (curveCanvasBounds_.contains(ev.x, ev.y)) {
            isDraggingCurve_ = true;
            float normX = (ev.x - curveCanvasBounds_.x) / curveCanvasBounds_.w;
            float normY = 1.0f - (ev.y - curveCanvasBounds_.y) / curveCanvasBounds_.h;
            params_.tension = std::clamp((normY - normX) * 2.5f, -1.0f, 1.0f);
            updateHarmonics();
            if (onParamsChanged) onParamsChanged(params_);
            return true;
        }

        // Sliders
        if (preGainBounds_.contains(ev.x, ev.y)) {
            isDraggingPreGain_ = true;
            float norm = std::clamp((ev.x - (preGainBounds_.x + 8.0f)) / (preGainBounds_.w - 16.0f), 0.0f, 1.0f);
            params_.preGain = 0.1f + norm * 3.9f;
            updateHarmonics();
            if (onParamsChanged) onParamsChanged(params_);
            return true;
        }
        if (postGainBounds_.contains(ev.x, ev.y)) {
            isDraggingPostGain_ = true;
            float norm = std::clamp((ev.x - (postGainBounds_.x + 8.0f)) / (postGainBounds_.w - 16.0f), 0.0f, 1.0f);
            params_.postGain = norm * 2.0f;
            updateHarmonics();
            if (onParamsChanged) onParamsChanged(params_);
            return true;
        }
        if (dryWetBounds_.contains(ev.x, ev.y)) {
            isDraggingDryWet_ = true;
            float norm = std::clamp((ev.x - (dryWetBounds_.x + 8.0f)) / (dryWetBounds_.w - 16.0f), 0.0f, 1.0f);
            params_.dryWet = norm;
            updateHarmonics();
            if (onParamsChanged) onParamsChanged(params_);
            return true;
        }

        // Toggles
        if (dcFilterBounds_.contains(ev.x, ev.y)) {
            params_.dcFilter = !params_.dcFilter;
            updateHarmonics();
            if (onParamsChanged) onParamsChanged(params_);
            return true;
        }
        if (oversamplingBounds_.contains(ev.x, ev.y)) {
            params_.oversampling = (params_.oversampling == 1) ? 2 : (params_.oversampling == 2 ? 4 : 1);
            if (onParamsChanged) onParamsChanged(params_);
            return true;
        }

        // Action Buttons
        if (applyBtnBounds_.contains(ev.x, ev.y)) {
            if (onApplied) onApplied(params_);
            close();
            return true;
        }
        if (closeBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        return true;
    }

    if (ev.action == PointerAction::Move) {
        if (isDraggingCurve_) {
            float normX = std::clamp((ev.x - curveCanvasBounds_.x) / curveCanvasBounds_.w, 0.0f, 1.0f);
            float normY = std::clamp(1.0f - (ev.y - curveCanvasBounds_.y) / curveCanvasBounds_.h, 0.0f, 1.0f);
            params_.tension = std::clamp((normY - normX) * 2.5f, -1.0f, 1.0f);
            updateHarmonics();
            if (onParamsChanged) onParamsChanged(params_);
            return true;
        }
        if (isDraggingPreGain_) {
            float norm = std::clamp((ev.x - (preGainBounds_.x + 8.0f)) / (preGainBounds_.w - 16.0f), 0.0f, 1.0f);
            params_.preGain = 0.1f + norm * 3.9f;
            updateHarmonics();
            if (onParamsChanged) onParamsChanged(params_);
            return true;
        }
        if (isDraggingPostGain_) {
            float norm = std::clamp((ev.x - (postGainBounds_.x + 8.0f)) / (postGainBounds_.w - 16.0f), 0.0f, 1.0f);
            params_.postGain = norm * 2.0f;
            updateHarmonics();
            if (onParamsChanged) onParamsChanged(params_);
            return true;
        }
        if (isDraggingDryWet_) {
            float norm = std::clamp((ev.x - (dryWetBounds_.x + 8.0f)) / (dryWetBounds_.w - 16.0f), 0.0f, 1.0f);
            params_.dryWet = norm;
            updateHarmonics();
            if (onParamsChanged) onParamsChanged(params_);
            return true;
        }
    }

    if (ev.action == PointerAction::Up) {
        isDraggingCurve_ = false;
        isDraggingPreGain_ = false;
        isDraggingPostGain_ = false;
        isDraggingDryWet_ = false;
        return true;
    }

    return true;
}

bool WaveshaperDialog::handleKey(int key, int /*scancode*/, int action, int /*mods*/) {
    if (!isOpen_) return false;
    if (action == 1) { // GLFW_PRESS
        if (key == 256) { // GLFW_KEY_ESCAPE
            close();
            return true;
        }
        if (key == 257) { // GLFW_KEY_ENTER
            if (onApplied) onApplied(params_);
            close();
            return true;
        }
    }
    return true;
}

} // namespace eatsbits::ui
