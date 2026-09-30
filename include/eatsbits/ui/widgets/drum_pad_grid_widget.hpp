#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <functional>
#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"

namespace eatsbits::ui {

/**
 * Definition for a General MIDI / MPC Drum Pad.
 */
struct DrumPadDef {
    uint8_t note{36};
    std::string label;
    std::string defaultEngine;
    Color accentColor;
    Rect2D bounds{0.0f, 0.0f, 0.0f, 0.0f};
    bool isTriggered{false};
    float triggerTimer{0.0f};
};

enum class DrumKitBank : uint8_t {
    CoreKit = 0,     // 16 Pads: Kicks, Snares, Hats, Toms, Cymbals
    Percussion = 1   // 16 Pads: Latin, Timbales, Congas, Bongos, Shakers
};

enum class DrumKitProfile : uint8_t {
    StandardGm = 0,
    Eats808 = 1,
    Eats909 = 2
};

/**
 * Interactive Visual Drum Pad Grid Widget (Eatsbeats Parity).
 * Provides a 4x4 tactile rubber MPC / SP-1200 style pad matrix with:
 * - 16-pad Core Kit + 16-pad Latin/Aux Percussion bank switching
 * - Note numbers (36..81 GM mapping) and customizable engine labels
 * - Authentic tactile gradient beveling and glowing LED trigger indicators
 * - Vertical velocity sensitivity (tap higher in pad = harder strike)
 * - Automatic 808 / 909 / Standard styling matching active drum voices
 */
class DrumPadGridWidget {
public:
    DrumPadGridWidget();
    ~DrumPadGridWidget() = default;

    void layout(const Rect2D& bounds) noexcept;
    void update(float dt) noexcept;
    void render(BatchRenderer2D& r, const ThemeTokens& theme) noexcept;
    bool handlePointer(const PointerEvent& ev) noexcept;

    void setBank(DrumKitBank bank) noexcept;
    [[nodiscard]] DrumKitBank getBank() const noexcept { return activeBank_; }

    void setProfile(DrumKitProfile profile) noexcept { profile_ = profile; }
    [[nodiscard]] DrumKitProfile getProfile() const noexcept { return profile_; }

    void triggerPad(uint8_t note, float velocity = 0.90f) noexcept;
    void releasePad(uint8_t note) noexcept;
    void releaseAllPads() noexcept;

    [[nodiscard]] const std::vector<DrumPadDef>& getActivePads() const noexcept {
        return (activeBank_ == DrumKitBank::CoreKit) ? corePads_ : percPads_;
    }

    [[nodiscard]] Rect2D getBounds() const noexcept { return bounds_; }

    // Callbacks
    std::function<void(uint8_t note, float velocity)> onPadTrigger;
    std::function<void(uint8_t note)> onPadRelease;

private:
    void initFactoryPads();

    Rect2D bounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D headerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D bankCoreBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D bankPercBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D gridBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    DrumKitBank activeBank_{DrumKitBank::CoreKit};
    DrumKitProfile profile_{DrumKitProfile::StandardGm};

    std::vector<DrumPadDef> corePads_;
    std::vector<DrumPadDef> percPads_;

    std::unordered_map<int, int> activePadPointers_; // ev.id -> padIndex
};

} // namespace eatsbits::ui
