#pragma once

#include "widgets/track_properties_panel.hpp"
#include <string>
#include <vector>
#include <span>

namespace eatsbits::ui {

struct ArrangerClip {
    std::string name{"Clip"};
    uint32_t startBar{1};
    uint32_t barLength{4};
    bool isLooped{true};
    uint32_t loopLengthBars{4};
    bool isAudio{false};
    int transposeSemitones{0};
    bool isReversed{false};
    float gainDb{0.0f};
    float r{0.0f}, g{0.9f}, b{1.0f};
    std::string eatscriptCode{""};
};

struct ArrangerTrackData {
    std::string id{""};
    std::string name{"Track"};
    std::string instrument{""};
    std::string instrumentEngine{""};
    std::string eatscriptCode{""};
    std::string type{"SYNTH"};
    std::string iconRef{""};
    float r{0.0f}, g{0.9f}, b{1.0f};
    float volume{0.8f};
    float pan{0.0f};
    bool mute{false};
    bool solo{false};
    bool freeze{false};
    float eqLow{0.5f};
    float eqMid{0.5f};
    float eqHigh{0.5f};
    ChordFollowMode chordFollowMode{ChordFollowMode::Off};
    TrackMidiFxData midiFx;
    TrackAudioFxData audioFx;
    std::vector<ArrangerClip> clips;
    std::optional<GuiPanelDef> customGuiPanel;
};

} // namespace eatsbits::ui
