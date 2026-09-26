#include "eatsbits/ui/theme.hpp"
#include <array>
#include <mutex>

namespace eatsbits::ui {

namespace {

float hue2rgb(float p, float q, float t) noexcept {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

} // anonymous namespace

Color::Hsl Color::toHsl() const noexcept {
    float maxVal = std::max({r, g, b});
    float minVal = std::min({r, g, b});
    float delta = maxVal - minVal;

    Hsl hsl;
    hsl.l = (maxVal + minVal) * 0.5f;

    if (delta <= 1e-5f) {
        hsl.h = 0.0f;
        hsl.s = 0.0f;
    } else {
        hsl.s = (hsl.l > 0.5f) ? (delta / (2.0f - maxVal - minVal)) : (delta / (maxVal + minVal));

        if (maxVal == r) {
            hsl.h = (g - b) / delta + (g < b ? 6.0f : 0.0f);
        } else if (maxVal == g) {
            hsl.h = (b - r) / delta + 2.0f;
        } else {
            hsl.h = (r - g) / delta + 4.0f;
        }
        hsl.h *= 60.0f;
        if (hsl.h < 0.0f) hsl.h += 360.0f;
    }

    return hsl;
}

Color Color::fromHsl(float h, float s, float l, float a) noexcept {
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    l = std::clamp(l, 0.0f, 1.0f);

    if (s <= 1e-5f) {
        return Color(l, l, l, a);
    }

    float q = (l < 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
    float p = 2.0f * l - q;
    float hNorm = h / 360.0f;

    float r_ = hue2rgb(p, q, hNorm + 1.0f / 3.0f);
    float g_ = hue2rgb(p, q, hNorm);
    float b_ = hue2rgb(p, q, hNorm - 1.0f / 3.0f);

    return Color(r_, g_, b_, a);
}

Color Color::lighten(float amount) const noexcept {
    Hsl hsl = toHsl();
    hsl.l = std::clamp(hsl.l + amount, 0.0f, 1.0f);
    return fromHsl(hsl.h, hsl.s, hsl.l, a);
}

Color Color::darken(float amount) const noexcept {
    Hsl hsl = toHsl();
    hsl.l = std::clamp(hsl.l - amount, 0.0f, 1.0f);
    return fromHsl(hsl.h, hsl.s, hsl.l, a);
}

Color Color::saturate(float amount) const noexcept {
    Hsl hsl = toHsl();
    hsl.s = std::clamp(hsl.s + amount, 0.0f, 1.0f);
    return fromHsl(hsl.h, hsl.s, hsl.l, a);
}

Color Color::desaturate(float amount) const noexcept {
    Hsl hsl = toHsl();
    hsl.s = std::clamp(hsl.s - amount, 0.0f, 1.0f);
    return fromHsl(hsl.h, hsl.s, hsl.l, a);
}

Color Color::complementary() const noexcept {
    Hsl hsl = toHsl();
    hsl.h = std::fmod(hsl.h + 180.0f, 360.0f);
    return fromHsl(hsl.h, hsl.s, hsl.l, a);
}

Color Color::analogous(float angleDegrees) const noexcept {
    Hsl hsl = toHsl();
    hsl.h = std::fmod(hsl.h + angleDegrees, 360.0f);
    if (hsl.h < 0.0f) hsl.h += 360.0f;
    return fromHsl(hsl.h, hsl.s, hsl.l, a);
}

Color Color::blend(const Color& bg, const Color& fg, float weight) noexcept {
    weight = std::clamp(weight, 0.0f, 1.0f);
    float inv = 1.0f - weight;
    return Color(
        bg.r * inv + fg.r * weight,
        bg.g * inv + fg.g * weight,
        bg.b * inv + fg.b * weight,
        bg.a * inv + fg.a * weight
    );
}

Color Color::ensureContrast(const Color& bg, float minRatio) const noexcept {
    float bgLum = bg.luminance();
    Color candidate = *this;

    for (int step = 0; step < 10; ++step) {
        float cLum = candidate.luminance();
        float l1 = std::max(bgLum, cLum);
        float l2 = std::min(bgLum, cLum);
        float ratio = (l1 + 0.05f) / (l2 + 0.05f);
        if (ratio >= minRatio) {
            return candidate;
        }
        if (bg.isDark()) {
            candidate = candidate.lighten(0.1f);
        } else {
            candidate = candidate.darken(0.1f);
        }
    }
    return candidate;
}

// ----------------------------------------------------------------------------
// Theme Derivation Engine
// ----------------------------------------------------------------------------

ThemeTokens Theme::buildTokens(const ThemeSeed& seed) {
    ThemeTokens t;
    t.name = seed.name;
    t.description = seed.description;
    t.isLight = seed.isLight;

    // 1. Chassis & Panel Backgrounds
    if (!seed.isLight) {
        t.backgroundDark            = seed.overrideBackgroundDark.value_or(seed.chassisBase.darken(0.35f));
        t.panelBackground           = seed.chassisBase;
        t.panelHeader               = seed.overridePanelHeader.value_or(seed.chassisBase.lighten(0.12f));
        t.panelHeaderGradientTop    = t.panelHeader.lighten(0.06f);
        t.panelHeaderGradientBottom = t.panelHeader.darken(0.06f);
        t.controlBackground         = seed.chassisBase.lighten(0.05f);
        t.controlWell               = seed.overrideControlWell.value_or(seed.chassisBase.darken(0.20f));
        t.borderSubtle              = seed.chassisBase.lighten(0.18f);
    } else {
        t.backgroundDark            = seed.overrideBackgroundDark.value_or(seed.chassisBase.darken(0.04f));
        t.panelBackground           = seed.chassisBase;
        t.panelHeader               = seed.overridePanelHeader.value_or(seed.chassisBase.darken(0.08f));
        t.panelHeaderGradientTop    = t.panelHeader.lighten(0.03f);
        t.panelHeaderGradientBottom = t.panelHeader.darken(0.04f);
        t.controlBackground         = seed.chassisBase.darken(0.05f);
        t.controlWell               = seed.overrideControlWell.value_or(seed.chassisBase.darken(0.14f));
        t.borderSubtle              = seed.chassisBase.darken(0.18f);
    }

    // 2. Accents & Interactions
    t.primaryAccent    = seed.primaryAccent;
    t.secondaryAccent  = seed.secondaryAccent.value_or(seed.primaryAccent.analogous(40.0f));
    t.highlight        = seed.primaryAccent.lighten(0.12f);
    t.borderFocus      = seed.primaryAccent.withAlpha(0.85f);
    t.playActive       = Color::fromHex(0x00D95A); // Pro audio green
    t.stopActive       = Color::fromHex(0x718096); // Slate
    t.recordActive     = Color::fromHex(0xFF334B); // Red
    t.muteActive       = Color::fromHex(0xFF3B30); // Alert Red
    t.soloActive       = Color::fromHex(0xFFCC00); // Amber Yellow

    // 3. Displays & Nixie Tubes
    t.lcdBackground = seed.overrideLcdBackground.value_or(t.backgroundDark);
    t.lcdBorder     = seed.overrideLcdBorder.value_or(t.borderSubtle.darken(0.05f));
    t.lcdText       = seed.overrideLcdText.value_or(seed.primaryAccent.saturate(0.1f));
    t.lcdDotGrid    = t.lcdBackground.lighten(seed.isLight ? -0.05f : 0.08f);
    t.lcdGlow       = seed.primaryAccent.withAlpha(0.40f);

    t.tempoText     = seed.overrideTempoText.value_or(seed.isLight ? seed.primaryAccent.darken(0.2f) : seed.primaryAccent.lighten(0.35f));
    t.tempoGlow     = seed.overrideTempoGlow.value_or(seed.primaryAccent.withAlpha(0.55f));

    // 4. Arranger & Piano Roll
    if (!seed.isLight) {
        t.timelineRuler   = t.panelHeader.darken(0.05f);
        t.playhead        = seed.primaryAccent.lighten(0.1f);
        t.loopRegion      = seed.primaryAccent.withAlpha(0.20f);
        t.gridLineMajor   = Color::blend(t.backgroundDark, Color::fromHex(0xFFFFFF), 0.12f);
        t.gridLineMinor   = Color::blend(t.backgroundDark, Color::fromHex(0xFFFFFF), 0.05f);
        t.pianoWhiteKey   = Color::fromHex(0xEDEAE5);
        t.pianoBlackKey   = Color::fromHex(0x1B1917);
        t.pianoLaneDark   = t.backgroundDark;
        t.pianoLaneLight  = t.backgroundDark.lighten(0.035f);
        t.noteDefault     = seed.primaryAccent;
        t.noteSelected    = Color::fromHex(0xFFFFFF);
    } else {
        t.timelineRuler   = t.panelHeader.darken(0.03f);
        t.playhead        = seed.primaryAccent;
        t.loopRegion      = seed.primaryAccent.withAlpha(0.25f);
        t.gridLineMajor   = Color::blend(t.backgroundDark, Color::fromHex(0x000000), 0.18f);
        t.gridLineMinor   = Color::blend(t.backgroundDark, Color::fromHex(0x000000), 0.07f);
        t.pianoWhiteKey   = Color::fromHex(0xFFFFFF);
        t.pianoBlackKey   = Color::fromHex(0x2D3748);
        t.pianoLaneDark   = t.backgroundDark.darken(0.03f);
        t.pianoLaneLight  = t.backgroundDark;
        t.noteDefault     = seed.primaryAccent;
        t.noteSelected    = Color::fromHex(0x0F172A);
    }

    // 5. Code Editor & Scripting
    if (!seed.isLight) {
        t.codeEditorBg         = seed.overrideCodeEditorBg.value_or(t.backgroundDark.darken(0.05f));
        t.codeEditorGutter     = t.codeEditorBg.lighten(0.035f);
        t.codeEditorText       = seed.primaryAccent.lighten(0.30f);
        t.codeEditorGutterText = Color::fromHex(0x535D6E);
        t.codeEditorBorder     = t.borderSubtle;
    } else {
        t.codeEditorBg         = seed.overrideCodeEditorBg.value_or(Color::fromHex(0xFFFFFF));
        t.codeEditorGutter     = Color::fromHex(0xF1F5F9);
        t.codeEditorText       = Color::fromHex(0x0F172A);
        t.codeEditorGutterText = Color::fromHex(0x94A3B8);
        t.codeEditorBorder     = Color::fromHex(0xCBD5E1);
    }

    // 6. Typography
    if (!seed.isLight) {
        t.textPrimary   = seed.overrideTextPrimary.value_or(Color::fromHex(0xF0F4F8));
        t.textSecondary = seed.overrideTextSecondary.value_or(Color::fromHex(0x8E9BAE));
        t.textMuted     = seed.overrideTextMuted.value_or(Color::fromHex(0x535D6E));
        t.textInverse   = Color::fromHex(0x0F172A);
    } else {
        t.textPrimary   = seed.overrideTextPrimary.value_or(Color::fromHex(0x0F172A));
        t.textSecondary = seed.overrideTextSecondary.value_or(Color::fromHex(0x334155));
        t.textMuted     = seed.overrideTextMuted.value_or(Color::fromHex(0x64748B));
        t.textInverse   = Color::fromHex(0xF0F4F8);
    }

    // 7. Contextual 2.5D Lighting & Shadows
    if (seed.overrideGlobalLight) {
        t.globalLight = *seed.overrideGlobalLight;
    } else {
        if (seed.isLight) {
            // Bright daylight studio: higher virtual elevation, softer shadows
            t.globalLight = LightSource2D(640.0f, -220.0f, 600.0f, 0.70f);
        } else {
            // Vintage hardware rack: focused warm studio spot lamp
            t.globalLight = LightSource2D(640.0f, -180.0f, 500.0f, 0.88f);
        }
    }

    return t;
}

// ----------------------------------------------------------------------------
// Curated Eatsbeats Presets
// ----------------------------------------------------------------------------

namespace {

struct PresetRegistry {
    std::vector<ThemeTokens> presets;
    Theme::Preset currentPreset{Theme::Preset::AteTrack};
    std::mutex mutex;

    PresetRegistry() {
        presets.resize(Theme::kPresetCount);

        // 1. Preset 0: Ate Track (Default Vintage Hardware Console)
        {
            ThemeSeed seed;
            seed.name = "Ate Track";
            seed.description = "80s 8-Track Vintage Hardware Console (Aged chassis, warm amber nixie glow)";
            seed.isLight = false;
            seed.chassisBase   = Color::fromHex(0x25211C); // Aged warm metal chassis surface
            seed.primaryAccent = Color::fromHex(0xFFFF8C00); // Warm amber / vintage nixie glow
            seed.secondaryAccent = Color::fromHex(0xFFFFB700); // Warm antique gold

            // Artisanal overrides for authentic 80s hardware look (sepia tan, brown, gold, zero blue)
            seed.overrideBackgroundDark = Color::fromHex(0x141210); // Weathered vintage rack dark background
            seed.overridePanelHeader    = Color::fromHex(0x363028); // Dark warm brushed metallic header
            seed.overrideControlWell    = Color::fromHex(0x181614); // Recessed control well background
            seed.overrideTempoText      = Color::fromHex(0xFFFFF2D6); // Warm glowing amber-cream
            seed.overrideTempoGlow      = Color::fromHex(0xFFFF8C00); // Vintage amber glow bloom
            seed.overrideLcdBackground  = Color::fromHex(0x14110E); // Smoked warm amber glass
            seed.overrideLcdBorder      = Color::fromHex(0x423627); // Aged bronze/brass bezel
            seed.overrideLcdText        = Color::fromHex(0xFFFFB347); // Vintage amber LCD pixels
            seed.overrideCodeEditorBg   = Color::fromHex(0x141210);
            seed.overrideTextPrimary    = Color::fromHex(0xF6F0E6); // Warm parchment cream
            seed.overrideTextSecondary  = Color::fromHex(0xC4B49E); // Warm sepia tan (replaces cold blue-grey)
            seed.overrideTextMuted      = Color::fromHex(0x7A6E5E); // Warm deep sepia

            presets[static_cast<size_t>(Theme::Preset::AteTrack)] = Theme::buildTokens(seed);
        }

        // 2. Preset 1: Midnight Bites (Obsidian Cyber Neon)
        {
            ThemeSeed seed;
            seed.name = "Midnight Bites";
            seed.description = "Obsidian dark cyber theme with electric neon cyan and glowing magenta";
            seed.isLight = false;
            seed.chassisBase   = Color::fromHex(0x101010);
            seed.primaryAccent = Color::fromHex(0x21F4E8); // EatsBeats signature neon cyan
            seed.secondaryAccent = Color::fromHex(0xFFFF007A); // Glowing magenta

            seed.overrideBackgroundDark = Color::fromHex(0x000000); // Pure black
            seed.overridePanelHeader    = Color::fromHex(0x181818);
            seed.overrideControlWell    = Color::fromHex(0x222222);
            seed.overrideTempoText      = Color::fromHex(0xE0FFFF); // Electric neon cyan-white
            seed.overrideTempoGlow      = Color::fromHex(0x21F4E8);
            seed.overrideLcdBackground  = Color::fromHex(0x080D11);
            seed.overrideLcdBorder      = Color::fromHex(0x132A32);
            seed.overrideLcdText        = Color::fromHex(0x38EEDD); // Neon cyan LCD pixels
            seed.overrideCodeEditorBg   = Color::fromHex(0x0D0F17);

            presets[static_cast<size_t>(Theme::Preset::MidnightBites)] = Theme::buildTokens(seed);
        }

        // 3. Preset 2: Light Snack (Bright Studio Light)
        {
            ThemeSeed seed;
            seed.name = "Light Snack";
            seed.description = "Bright high-contrast daylight studio theme for maximum visibility";
            seed.isLight = true;
            seed.chassisBase   = Color::fromHex(0xFFFFFF); // Pure white panel
            seed.primaryAccent = Color::fromHex(0x007799); // Deep contrast teal
            seed.secondaryAccent = Color::fromHex(0x0284C7);

            seed.overrideBackgroundDark = Color::fromHex(0xF4F6F9); // Crisp light background
            seed.overridePanelHeader    = Color::fromHex(0xE2E8F0); // Light slate header
            seed.overrideControlWell    = Color::fromHex(0xCBD5E1); // Soft control fill
            seed.overrideTempoText      = Color::fromHex(0x0F172A);
            seed.overrideTempoGlow      = Color::fromHex(0x00B4D8);
            seed.overrideLcdBackground  = Color::fromHex(0xE2E8F0);
            seed.overrideLcdBorder      = Color::fromHex(0x94A3B8);
            seed.overrideLcdText        = Color::fromHex(0x0F172A); // High-contrast studio slate
            seed.overrideTextPrimary    = Color::fromHex(0x0F172A);

            presets[static_cast<size_t>(Theme::Preset::LightSnack)] = Theme::buildTokens(seed);
        }

        // 4. Preset 3: Breakfast (Solarized Light)
        {
            ThemeSeed seed;
            seed.name = "Breakfast";
            seed.description = "Solarized light theme with warm parchment and gold nixie glow";
            seed.isLight = true;
            seed.chassisBase   = Color::fromHex(0xEEE8D5); // Solarized base2
            seed.primaryAccent = Color::fromHex(0xB58900); // Solarized yellow/gold
            seed.secondaryAccent = Color::fromHex(0x2AA198); // Solarized cyan

            seed.overrideBackgroundDark = Color::fromHex(0xFDF6E3); // Solarized base3 light
            seed.overridePanelHeader    = Color::fromHex(0xE0D8C3);
            seed.overrideControlWell    = Color::fromHex(0xE4DCC8);
            seed.overrideTempoText      = Color::fromHex(0x586E75); // Solarized base01
            seed.overrideTempoGlow      = Color::fromHex(0xB58900);
            seed.overrideLcdBackground  = Color::fromHex(0xE8E2CF);
            seed.overrideLcdBorder      = Color::fromHex(0xB58900);
            seed.overrideLcdText        = Color::fromHex(0x586E75);
            seed.overrideTextPrimary    = Color::fromHex(0x073642); // Solarized base02

            presets[static_cast<size_t>(Theme::Preset::Breakfast)] = Theme::buildTokens(seed);
        }

        // 5. Preset 4: Dinner (Solarized Dark)
        {
            ThemeSeed seed;
            seed.name = "Dinner";
            seed.description = "Solarized dark theme with deep oceanic teal and cyan accents";
            seed.isLight = false;
            seed.chassisBase   = Color::fromHex(0x073642); // Solarized base02
            seed.primaryAccent = Color::fromHex(0x2AA198); // Solarized cyan
            seed.secondaryAccent = Color::fromHex(0x268BD2); // Solarized blue

            seed.overrideBackgroundDark = Color::fromHex(0x002B36); // Solarized base03
            seed.overridePanelHeader    = Color::fromHex(0x0A4858);
            seed.overrideControlWell    = Color::fromHex(0x001F27);
            seed.overrideTempoText      = Color::fromHex(0xEEE8D5); // Solarized base2
            seed.overrideTempoGlow      = Color::fromHex(0x2AA198);
            seed.overrideLcdBackground  = Color::fromHex(0x00212B);
            seed.overrideLcdBorder      = Color::fromHex(0x0A4858);
            seed.overrideLcdText        = Color::fromHex(0x2AA198);
            seed.overrideTextPrimary    = Color::fromHex(0xEEE8D5);

            presets[static_cast<size_t>(Theme::Preset::Dinner)] = Theme::buildTokens(seed);
        }
    }
};

PresetRegistry& getRegistry() {
    static PresetRegistry registry;
    return registry;
}

} // anonymous namespace

const ThemeTokens& Theme::get(Preset preset) {
    auto& reg = getRegistry();
    size_t idx = static_cast<size_t>(preset);
    if (idx >= reg.presets.size()) {
        idx = 0;
    }
    return reg.presets[idx];
}

const ThemeTokens& Theme::current() {
    auto& reg = getRegistry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    return get(reg.currentPreset);
}

void Theme::setPreset(Preset preset) {
    auto& reg = getRegistry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    reg.currentPreset = preset;
}

Theme::Preset Theme::getCurrentPreset() {
    auto& reg = getRegistry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    return reg.currentPreset;
}

const std::vector<ThemeTokens>& Theme::getAllPresets() {
    return getRegistry().presets;
}

} // namespace eatsbits::ui
