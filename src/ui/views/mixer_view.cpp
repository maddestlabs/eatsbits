#include "eatsbits/ui/views/mixer_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/ui/clay/clay_context.hpp"
#include "clay.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace eatsbits::ui {

MixerView::MixerView() {
    clayContext_ = std::make_unique<clay::ClayContext>();
    clayContext_->initialize(1280.0f, 720.0f);

    masterChannel_.name = "MASTER";
    masterChannel_.type = "BUS";
    masterChannel_.r = 1.0f;
    masterChannel_.g = 0.85f;
    masterChannel_.b = 0.0f;
    masterChannel_.fader = 0.85f;
    masterChannel_.pan = 0.0f;
    masterChannel_.mute = false;

    channels_ = {
        {"DXVII E-Piano", "SYNTH", 0.62f, 0.31f, 0.87f, 0.75f, 0.0f, false, false, false, false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f,
         true, 40.0f, -2.0f, 1500.0f, 2.0f, 1.1f, 0.0f, "DXVII FM", 0.50f, 0.65f, 0.70f, 0.60f, "ALGO", "FEEDBACK", "BRIGHT", "CHORUS", {}, {}}
    };

    // Default standalone properties drawer is closed
    propertiesDrawer_.setExpanded(false);

    propertiesDrawer_.onScrollChanged = [this](float sy) {
        if (onScrollChanged) onScrollChanged(sy);
    };

    // Shared TrackPropertiesDrawer callbacks across Arranger and Mixer
    propertiesDrawer_.onVolumeChanged = [this](uint32_t trackIdx, float vol) {
        if (isMasterSelected_) {
            masterChannel_.fader = vol;
            if (onMasterVolumeChanged) onMasterVolumeChanged(vol);
            return;
        }
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].fader = vol;
        }
        if (onVolumeChanged) {
            onVolumeChanged(trackIdx, vol);
        }
    };
    propertiesDrawer_.onPanChanged = [this](uint32_t trackIdx, float pan) {
        if (isMasterSelected_) {
            masterChannel_.pan = pan;
            if (onMasterPanChanged) onMasterPanChanged(pan);
            return;
        }
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].pan = pan;
        }
        if (onPanChanged) {
            onPanChanged(trackIdx, pan);
        }
    };
    propertiesDrawer_.onMuteToggled = [this](uint32_t trackIdx, bool mute) {
        if (isMasterSelected_) {
            masterChannel_.mute = mute;
            if (onMasterMuteToggled) onMasterMuteToggled(mute);
            return;
        }
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].mute = mute;
        }
        if (onMuteToggled) {
            onMuteToggled(trackIdx, mute);
        }
    };
    propertiesDrawer_.onSoloToggled = [this](uint32_t trackIdx, bool solo) {
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].solo = solo;
        }
        if (onSoloToggled) {
            onSoloToggled(trackIdx, solo);
        }
    };
    propertiesDrawer_.onParamChanged = [this](uint32_t trackIdx, const std::string& paramName, float normVal) {
        if (isMasterSelected_) {
            if (paramName == "masterEqEnabled") masterEqEnabled_ = (normVal > 0.5f);
            else if (paramName == "masterSubCut") masterSubCut_ = 20.0f + normVal * 25.0f;
            else if (paramName == "masterLowGain") masterLowGain_ = -12.0f + normVal * 24.0f;
            else if (paramName == "masterMidGain") masterMidGain_ = -12.0f + normVal * 24.0f;
            else if (paramName == "masterHighGain") masterHighGain_ = -12.0f + normVal * 24.0f;
            else if (paramName == "masterLimiterEnabled") masterLimiterEnabled_ = (normVal > 0.5f);
            else if (paramName == "masterCeilingDbfs") masterCeilingDbfs_ = -2.0f + normVal * 2.0f;
            else if (paramName == "masterLimiterDrive") masterLimiterDrive_ = normVal * 12.0f;
            else if (paramName == "masterTargetLufs") masterTargetLufs_ = -24.0f + normVal * 18.0f;
            syncDrawerData();
            return;
        }

        if (trackIdx < channels_.size()) {
            auto& ch = channels_[trackIdx];
            bool isEqParam = (paramName.rfind("eq", 0) == 0);
            if (!isEqParam) {
                ch.setKnobValue(paramName, normVal);
                drawerData_.setKnobValue(paramName, normVal);

                std::string lowerParam = paramName;
                for (char& c : lowerParam) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                if (lowerParam == "cutoff" || lowerParam == "tone" || lowerParam == "param1") {
                    ch.knob1 = normVal;
                } else if (lowerParam == "resonance" || lowerParam == "snappy" || lowerParam == "punch" || lowerParam == "param2") {
                    ch.knob2 = normVal;
                } else if (lowerParam == "decay" || lowerParam == "tune" || lowerParam == "bright" || lowerParam == "param3") {
                    ch.knob3 = normVal;
                } else if (lowerParam == "accent" || lowerParam == "tuning" || lowerParam == "crack" || lowerParam == "detune" || lowerParam == "param4") {
                    ch.knob4 = normVal;
                }
            } else {
                if (paramName == "eqEnabled") ch.eqEnabled = (normVal > 0.5f);
                else if (paramName == "eqHpf") ch.eqHpf = 20.0f + normVal * 480.0f;
                else if (paramName == "eqLowGain") ch.eqLowGain = -18.0f + normVal * 36.0f;
                else if (paramName == "eqMidFreq") ch.eqMidFreq = 200.0f + normVal * 7800.0f;
                else if (paramName == "eqMidGain") ch.eqMidGain = -18.0f + normVal * 36.0f;
                else if (paramName == "eqMidQ") ch.eqMidQ = 0.3f + normVal * 9.7f;
                else if (paramName == "eqHighGain") ch.eqHighGain = -18.0f + normVal * 36.0f;
            }
        }
        if (onParamChanged) {
            onParamChanged(trackIdx, paramName, normVal);
        }
    };
    propertiesDrawer_.onColorChanged = [this](uint32_t trackIdx, float r, float g, float b) {
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].r = r;
            channels_[trackIdx].g = g;
            channels_[trackIdx].b = b;
            drawerData_.r = r;
            drawerData_.g = g;
            drawerData_.b = b;
        }
        if (onColorChanged) {
            onColorChanged(trackIdx, r, g, b);
        }
    };
    propertiesDrawer_.onChooseTrackIcon = [this](uint32_t trackIdx) {
        if (onChooseTrackIcon) {
            onChooseTrackIcon(trackIdx);
        }
    };
    propertiesDrawer_.onTrackRenameWithText = [this](uint32_t trackIdx, const std::string& newName) {
        if (!newName.empty()) {
            drawerData_.trackName = newName;
            if (trackIdx < drawerData_.allTrackNames.size()) {
                drawerData_.allTrackNames[trackIdx] = newName;
            }
            if (trackIdx < channels_.size()) {
                channels_[trackIdx].name = newName;
            }
            if (onTrackRename) {
                onTrackRename(trackIdx, newName);
            }
        }
    };
    propertiesDrawer_.getPanel().getPluginSearchDialog().onPluginSelected =
        [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t targetIdx) {
            if (mode == PluginDialogMode::AddInstrument) {
                if (targetIdx < channels_.size()) {
                    channels_[targetIdx].instrument = entry.name;
                    channels_[targetIdx].r = entry.r;
                    channels_[targetIdx].g = entry.g;
                    channels_[targetIdx].b = entry.b;
                }
                drawerData_.instrument = entry.name;
                drawerData_.instrumentEngine = entry.id.empty() ? entry.engineTag : entry.id;
                drawerData_.presetTitle = entry.name;
                drawerData_.presetSubtitle = (targetIdx < channels_.size() ? channels_[targetIdx].name : "Track") + " • " + entry.name;
                drawerData_.r = entry.r;
                drawerData_.g = entry.g;
                drawerData_.b = entry.b;
                drawerData_.knobs.clear();
                drawerData_.syncKnobsIfEmpty();
                if (onInstrumentChanged) {
                    onInstrumentChanged(targetIdx, drawerData_.instrumentEngine, entry.name);
                }
            } else if (mode == PluginDialogMode::AddMidiFx) {
                if (targetIdx < channels_.size()) {
                    TrackMidiFxItem mfx;
                    mfx.id = entry.id;
                    mfx.name = entry.name;
                    mfx.type = (entry.engineTag.empty() || entry.engineTag == "MIDI Processor" || entry.engineTag == "DSP Effect") ? entry.name : entry.engineTag;
                    mfx.enabled = true;
                    mfx.isExpanded = true;
                    mfx.ensureDefaultKnobs();
                    channels_[targetIdx].midiFx.push_back(mfx);
                    drawerData_.midiFx.push_back(mfx);
                    if (onMidiFxChanged) onMidiFxChanged(targetIdx);
                }
            } else if (mode == PluginDialogMode::AddAudioFx) {
                if (targetIdx < channels_.size()) {
                    TrackAudioFxItem afx;
                    afx.id = entry.id;
                    afx.name = entry.name;
                    afx.type = entry.engineTag.empty() ? entry.name : entry.engineTag;
                    afx.enabled = true;
                    afx.isExpanded = true;
                    afx.ensureDefaultKnobs();
                    channels_[targetIdx].audioFx.push_back(afx);
                    drawerData_.audioFx.push_back(afx);
                    if (onAudioFxChanged) onAudioFxChanged(targetIdx);
                }
            }
        };
    propertiesDrawer_.onRemoveMidiFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (trackIdx < channels_.size() && fxIdx < channels_[trackIdx].midiFx.size()) {
            channels_[trackIdx].midiFx.erase(channels_[trackIdx].midiFx.begin() + fxIdx);
            if (onMidiFxChanged) onMidiFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onRemoveAudioFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (trackIdx < channels_.size() && fxIdx < channels_[trackIdx].audioFx.size()) {
            channels_[trackIdx].audioFx.erase(channels_[trackIdx].audioFx.begin() + fxIdx);
            if (onAudioFxChanged) onAudioFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onToggleMidiFx = [this](uint32_t trackIdx, size_t fxIdx, bool en) {
        if (trackIdx < channels_.size() && fxIdx < channels_[trackIdx].midiFx.size()) {
            channels_[trackIdx].midiFx[fxIdx].enabled = en;
            if (onToggleMidiFx) onToggleMidiFx(trackIdx, fxIdx, en);
            if (onMidiFxChanged) onMidiFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onToggleAudioFx = [this](uint32_t trackIdx, size_t fxIdx, bool en) {
        if (trackIdx < channels_.size() && fxIdx < channels_[trackIdx].audioFx.size()) {
            channels_[trackIdx].audioFx[fxIdx].enabled = en;
            if (onToggleAudioFx) onToggleAudioFx(trackIdx, fxIdx, en);
            if (onAudioFxChanged) onAudioFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onReorderAudioFx = [this](uint32_t trackIdx, size_t fromIdx, size_t toIdx) {
        if (trackIdx < channels_.size()) {
            auto& list = channels_[trackIdx].audioFx;
            if (fromIdx < list.size() && toIdx < list.size() && fromIdx != toIdx) {
                auto item = list[fromIdx];
                list.erase(list.begin() + fromIdx);
                list.insert(list.begin() + toIdx, item);
                drawerData_.audioFx = list;
                if (onAudioFxChanged) onAudioFxChanged(trackIdx);
            }
        }
    };
    propertiesDrawer_.onReorderMidiFx = [this](uint32_t trackIdx, size_t fromIdx, size_t toIdx) {
        if (trackIdx < channels_.size()) {
            auto& list = channels_[trackIdx].midiFx;
            if (fromIdx < list.size() && toIdx < list.size() && fromIdx != toIdx) {
                auto item = list[fromIdx];
                list.erase(list.begin() + fromIdx);
                list.insert(list.begin() + toIdx, item);
                drawerData_.midiFx = list;
                if (onMidiFxChanged) onMidiFxChanged(trackIdx);
            }
        }
    };
    propertiesDrawer_.onAudioFxChanged = [this](uint32_t trackIdx) {
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].audioFx = drawerData_.audioFx;
            if (onAudioFxChanged) onAudioFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onMidiFxChanged = [this](uint32_t trackIdx) {
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].midiFx = drawerData_.midiFx;
            if (onMidiFxChanged) onMidiFxChanged(trackIdx);
        }
    };
    propertiesDrawer_.onOpenFullscreenDevice = [this](uint32_t trackIdx) {
        if (onOpenFullscreenDevice) onOpenFullscreenDevice(trackIdx);
    };
    propertiesDrawer_.onOpenFullscreenAudioFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (onOpenFullscreenAudioFx) onOpenFullscreenAudioFx(trackIdx, fxIdx);
    };
    propertiesDrawer_.onOpenFullscreenMidiFx = [this](uint32_t trackIdx, size_t fxIdx) {
        if (onOpenFullscreenMidiFx) onOpenFullscreenMidiFx(trackIdx, fxIdx);
    };
    propertiesDrawer_.onOpenDesign = [this](uint32_t trackIdx) {
        if (onOpenDesign) onOpenDesign(trackIdx);
    };
    propertiesDrawer_.onAddClip = [this](uint32_t trackIdx) {
        if (onAddClip) onAddClip(trackIdx);
    };
    propertiesDrawer_.onDeleteTrack = [this](uint32_t trackIdx) {
        if (onDeleteTrack) onDeleteTrack(trackIdx);
    };
    propertiesDrawer_.onDuplicateTrack = [this](uint32_t trackIdx) {
        if (onDuplicateTrack) onDuplicateTrack(trackIdx);
    };
    propertiesDrawer_.onChordFollowChanged = [this](uint32_t trackIdx, theory::ChordFollowMode mode) {
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].chordFollowMode = mode;
            drawerData_.chordFollowMode = mode;
        }
        if (onTrackChordFollowChanged) onTrackChordFollowChanged(trackIdx, mode);
    };
    propertiesDrawer_.onBakeChords = [this](uint32_t trackIdx) {
        drawerData_.chordFollowMode = theory::ChordFollowMode::Off;
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].chordFollowMode = theory::ChordFollowMode::Off;
        }
        if (onBakeChords) onBakeChords(trackIdx);
    };
    propertiesDrawer_.onAuditionNote = [this](uint32_t trackIdx, uint8_t note, float vel) {
        if (onAuditionNote) onAuditionNote(trackIdx, note, vel);
    };

    ViewContext defaultCtx;
    layout(Rect2D(0.0f, 56.0f, 1280.0f, 700.0f), defaultCtx);
}

MixerView::~MixerView() = default;

void MixerView::setPreset(ModularMixerPreset preset) noexcept {
    switch (preset) {
        case ModularMixerPreset::Full:
            showRouting_ = true;
            showAutomation_ = true;
            showPan_ = true;
            showFaders_ = true;
            showMeters_ = true;
            showButtons_ = true;
            showReadouts_ = true;
            break;
        case ModularMixerPreset::FadersOnly:
            showRouting_ = false;
            showAutomation_ = false;
            showPan_ = false;
            showFaders_ = true;
            showMeters_ = false;
            showButtons_ = false;
            showReadouts_ = true;
            break;
        case ModularMixerPreset::FadersAndMeters:
            showRouting_ = false;
            showAutomation_ = false;
            showPan_ = false;
            showFaders_ = true;
            showMeters_ = true;
            showButtons_ = false;
            showReadouts_ = true;
            break;
    }
}

void MixerView::setDensityMode(MixerDensityMode mode) noexcept {
    densityMode_ = mode;
    autoDensity_ = false;
    switch (densityMode_) {
        case MixerDensityMode::Comfortable:
            masterStripWidth_ = 140.0f;
            channelStripWidth_ = 140.0f;
            channelGap_ = 10.0f;
            break;
        case MixerDensityMode::Compact:
            masterStripWidth_ = 90.0f;
            channelStripWidth_ = 78.0f;
            channelGap_ = 6.0f;
            break;
        case MixerDensityMode::Micro:
            masterStripWidth_ = 65.0f;
            channelStripWidth_ = 50.0f;
            channelGap_ = 4.0f;
            break;
    }
}

void MixerView::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;

    // Responsive auto-density: auto-switch to Compact/Micro on small screens/mobile
    if (autoDensity_) {
        if (ctx.isMobile || bounds_.w < 600.0f) {
            densityMode_ = MixerDensityMode::Micro;
        } else if (bounds_.w < 850.0f) {
            densityMode_ = MixerDensityMode::Compact;
        } else {
            densityMode_ = MixerDensityMode::Comfortable;
        }
    }

    switch (densityMode_) {
        case MixerDensityMode::Comfortable:
            masterStripWidth_ = 140.0f;
            channelStripWidth_ = 140.0f;
            channelGap_ = 10.0f;
            break;
        case MixerDensityMode::Compact:
            masterStripWidth_ = 90.0f;
            channelStripWidth_ = 78.0f;
            channelGap_ = 6.0f;
            break;
        case MixerDensityMode::Micro:
            masterStripWidth_ = 65.0f;
            channelStripWidth_ = 50.0f;
            channelGap_ = 4.0f;
            break;
    }

    // Top Options Toolbar (always visible, 32px height)
    float toolbarH = 32.0f;
    float toolbarW = std::max(0.0f, bounds_.w - ctx.browserOffset);
    toolbarBounds_ = Rect2D(bounds_.x, bounds_.y, toolbarW, toolbarH);

    // Sliding Track Properties Drawer Layout
    propertiesDrawer_.layout(bounds_, ctx.browserOffset);
    float drawerTotalW = propertiesDrawer_.getEffectiveWidth();

    // Declarative Clay flex container layout for mixer workspace
    if (clayContext_ && clayContext_->isInitialized()) {
        clayContext_->setDimensions(bounds_.w, bounds_.h);
        clayContext_->beginLayout();

        CLAY(CLAY_ID("MixerViewRoot"), {
            .layout = {
                .sizing = { CLAY_SIZING_FIXED(bounds_.w), CLAY_SIZING_FIXED(bounds_.h) },
                .childGap = 0,
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            }
        }) {
            // Toolbar row
            CLAY(CLAY_ID("MixerToolbar"), {
                .layout = {
                    .sizing = { CLAY_SIZING_FIXED(toolbarW), CLAY_SIZING_FIXED(toolbarH) },
                    .padding = { 10, 10, 6, 6 },
                    .childGap = 8,
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                }
            }) {}

            // Main Mixer Workspace Row
            CLAY(CLAY_ID("MixerBody"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .padding = { 0, 0, 8, 8 },
                    .childGap = 0,
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                }
            }) {
                if (masterPosition_ == MixerMasterPosition::Right) {
                    CLAY(CLAY_ID("ChannelsScrollArea"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                            .padding = { 40, 10, 0, 0 },
                        }
                    }) {}
                    CLAY(CLAY_ID("MasterStrip"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_FIXED(masterStripWidth_), CLAY_SIZING_GROW(0) },
                        }
                    }) {}
                    if (drawerTotalW > 0.0f) {
                        CLAY(CLAY_ID("PropertiesDrawerSlot"), {
                            .layout = {
                                .sizing = { CLAY_SIZING_FIXED(drawerTotalW), CLAY_SIZING_GROW(0) },
                            }
                        }) {}
                    }
                } else {
                    CLAY(CLAY_ID("MasterStrip"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_FIXED(masterStripWidth_), CLAY_SIZING_GROW(0) },
                        }
                    }) {}
                    float spacerW = (densityMode_ == MixerDensityMode::Comfortable) ? 55.0f :
                                    ((densityMode_ == MixerDensityMode::Compact) ? 20.0f : 14.0f);
                    CLAY(CLAY_ID("ChannelsSpacer"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_FIXED(spacerW), CLAY_SIZING_GROW(0) },
                        }
                    }) {}
                    CLAY(CLAY_ID("ChannelsScrollArea"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                        }
                    }) {}
                    if (drawerTotalW > 0.0f) {
                        CLAY(CLAY_ID("PropertiesDrawerSlot"), {
                            .layout = {
                                .sizing = { CLAY_SIZING_FIXED(drawerTotalW), CLAY_SIZING_GROW(0) },
                            }
                        }) {}
                    }
                }
            }
        }

        clayContext_->endLayout(0.016f);

        auto tbData = clayContext_->getElementData(CLAY_ID("MixerToolbar"));
        if (tbData.found) {
            toolbarBounds_ = Rect2D(bounds_.x + tbData.boundingBox.x, bounds_.y + tbData.boundingBox.y,
                                    tbData.boundingBox.width, tbData.boundingBox.height);
        }
        auto masterData = clayContext_->getElementData(CLAY_ID("MasterStrip"));
        if (masterData.found) {
            masterBounds_ = Rect2D(bounds_.x + masterData.boundingBox.x, bounds_.y + masterData.boundingBox.y,
                                   masterData.boundingBox.width, masterData.boundingBox.height);
        }
        auto channelsData = clayContext_->getElementData(CLAY_ID("ChannelsScrollArea"));
        if (channelsData.found) {
            channelsScrollBounds_ = Rect2D(bounds_.x + channelsData.boundingBox.x, bounds_.y + channelsData.boundingBox.y,
                                           std::max(80.0f, channelsData.boundingBox.width), channelsData.boundingBox.height);
        }
    } else {
        float stripTopY = bounds_.y + toolbarH + 8.0f;
        float stripH = bounds_.h - toolbarH - 16.0f;

        if (masterPosition_ == MixerMasterPosition::Right) {
            float mX = bounds_.x + bounds_.w - drawerTotalW - ctx.browserOffset - masterStripWidth_ - 10.0f;
            masterBounds_ = Rect2D(mX, stripTopY, masterStripWidth_, stripH);
            channelsScrollBounds_ = Rect2D(bounds_.x + 40.0f, stripTopY, std::max(80.0f, mX - bounds_.x - 50.0f), stripH);
        } else {
            masterBounds_ = Rect2D(bounds_.x, stripTopY, masterStripWidth_, stripH);
            float scrollX = (densityMode_ == MixerDensityMode::Comfortable) ? (bounds_.x + 195.0f) :
                            ((densityMode_ == MixerDensityMode::Compact) ? (bounds_.x + masterStripWidth_ + 20.0f) :
                                                                           (bounds_.x + masterStripWidth_ + 14.0f));
            float scrollW = bounds_.w - scrollX - drawerTotalW - ctx.browserOffset;
            channelsScrollBounds_ = Rect2D(scrollX, stripTopY, std::max(80.0f, scrollW), stripH);
        }
    }

    if (masterPosition_ == MixerMasterPosition::Right) {
        optionsPillBounds_ = Rect2D(bounds_.x + 10.0f, bounds_.y + 6.0f, 68.0f, 20.0f);
    } else {
        optionsPillBounds_ = Rect2D(bounds_.x + masterStripWidth_ + 14.0f, bounds_.y + 6.0f, 68.0f, 20.0f);
    }
}

MixerView::FaderGeometry MixerView::getMasterFaderGeometry() const noexcept {
    FaderGeometry geom;
    float mX = (masterBounds_.x == 0.0f) ? 12.0f : masterBounds_.x;
    float mY = masterBounds_.y;
    float stripH = masterBounds_.h;
    float lcdH = (densityMode_ == MixerDensityMode::Micro) ? 24.0f : ((densityMode_ == MixerDensityMode::Compact) ? 32.0f : 38.0f);
    float ky = mY + 8.0f + (showReadouts_ ? lcdH : 0.0f) + 16.0f;
    geom.wellTopY = ky + (showPan_ ? 38.0f : 10.0f);
    geom.wellBottomY = mY + stripH - 12.0f;
    geom.wellH = std::max(60.0f, geom.wellBottomY - geom.wellTopY);
    geom.trackX = (densityMode_ == MixerDensityMode::Micro) ? (mX + 22.0f) :
                  ((densityMode_ == MixerDensityMode::Compact) ? (mX + 28.0f) : (mX + 38.0f));

    float norm = presenter::audio_taper::gainToTravel(masterChannel_.fader);
    geom.thumbY = geom.wellBottomY - norm * geom.wellH;
    float thumbW = (densityMode_ == MixerDensityMode::Micro) ? 14.0f : ((densityMode_ == MixerDensityMode::Compact) ? 18.0f : 22.0f);
    float thumbH = (densityMode_ == MixerDensityMode::Micro) ? 20.0f : ((densityMode_ == MixerDensityMode::Compact) ? 26.0f : 34.0f);
    geom.well = Rect2D(geom.trackX - thumbW * 0.5f, geom.wellTopY, thumbW, geom.wellH);
    geom.thumb = Rect2D(geom.trackX - thumbW * 0.5f, geom.thumbY - thumbH * 0.5f, thumbW, thumbH);
    return geom;
}

MixerView::FaderGeometry MixerView::getChannelFaderGeometry(size_t index, float cx) const noexcept {
    FaderGeometry geom;
    float cy = masterBounds_.y;
    float stripH = masterBounds_.h;
    float lcdH = (densityMode_ == MixerDensityMode::Micro) ? 24.0f : ((densityMode_ == MixerDensityMode::Compact) ? 32.0f : 38.0f);
    float ky = cy + 8.0f + (showReadouts_ ? lcdH : 0.0f) + 16.0f;
    geom.wellTopY = ky + (showPan_ ? 38.0f : 10.0f);
    geom.wellBottomY = cy + stripH - 12.0f;
    geom.wellH = std::max(60.0f, geom.wellBottomY - geom.wellTopY);
    geom.trackX = (densityMode_ == MixerDensityMode::Micro) ? (cx + 18.0f) :
                  ((densityMode_ == MixerDensityMode::Compact) ? (cx + 20.0f) : (cx + 26.0f));

    float faderVal = (index < channels_.size()) ? channels_[index].fader : 1.0f;
    float norm = presenter::audio_taper::gainToTravel(faderVal);
    geom.thumbY = geom.wellBottomY - norm * geom.wellH;
    float thumbW = (densityMode_ == MixerDensityMode::Micro) ? 12.0f : ((densityMode_ == MixerDensityMode::Compact) ? 16.0f : 20.0f);
    float thumbH = (densityMode_ == MixerDensityMode::Micro) ? 20.0f : ((densityMode_ == MixerDensityMode::Compact) ? 26.0f : 34.0f);
    geom.well = Rect2D(geom.trackX - thumbW * 0.5f, geom.wellTopY, thumbW, geom.wellH);
    geom.thumb = Rect2D(geom.trackX - thumbW * 0.5f, geom.thumbY - thumbH * 0.5f, thumbW, thumbH);
    return geom;
}

bool MixerView::hitTestMasterFader(float x, float y) const noexcept {
    auto geom = getMasterFaderGeometry();
    Rect2D grabThumb(geom.thumb.x - 8.0f, geom.thumb.y - 8.0f, geom.thumb.w + 16.0f, geom.thumb.h + 16.0f);
    Rect2D grabWell(geom.well.x - 8.0f, geom.well.y - 4.0f, geom.well.w + 16.0f, geom.well.h + 8.0f);
    return grabThumb.contains(x, y) || grabWell.contains(x, y) ||
           (x >= geom.trackX - 16.0f && x <= geom.trackX + 16.0f && y >= geom.wellTopY - 4.0f && y <= geom.wellBottomY + 4.0f);
}

bool MixerView::hitTestChannelFader(size_t index, float cx, float x, float y) const noexcept {
    auto geom = getChannelFaderGeometry(index, cx);
    Rect2D grabThumb(geom.thumb.x - 8.0f, geom.thumb.y - 8.0f, geom.thumb.w + 16.0f, geom.thumb.h + 16.0f);
    Rect2D grabWell(geom.well.x - 8.0f, geom.well.y - 4.0f, geom.well.w + 16.0f, geom.well.h + 8.0f);
    return grabThumb.contains(x, y) || grabWell.contains(x, y) ||
           (x >= geom.trackX - 14.0f && x <= geom.trackX + 14.0f && y >= geom.wellTopY - 4.0f && y <= geom.wellBottomY + 4.0f);
}

bool MixerView::isFaderDragging(int index) const noexcept {
    if (activeFaderIndex_ == index) return true;
    for (const auto& [id, sess] : activeFaderSessions_) {
        if (sess.faderIndex == index) return true;
    }
    return false;
}

bool MixerView::isPanDragging(int index) const noexcept {
    if (activePanIndex_ == index) return true;
    for (const auto& [id, sess] : activePanSessions_) {
        if (sess.panIndex == index) return true;
    }
    return false;
}

void MixerView::resetSessions() noexcept {
    activeFaderSessions_.clear();
    activePanSessions_.clear();
    activeFaderIndex_ = -2;
    activePanIndex_ = -2;
    showTooltip_ = false;
}

void MixerView::syncFromWindow(
    std::span<const ArrangerTrackData> tracks,
    uint32_t selectedTrackIdx,
    float masterVol, float masterPanVal, bool masterMuteVal,
    float masterPeakLeft, float masterPeakRight,
    std::span<const float> chPeaksL, std::span<const float> chPeaksR,
    bool propertiesExpanded, float propertiesWidth,
    bool showRouting, bool showPan, bool showButtons, bool showMeters,
    bool showAutomation, bool showReadouts, bool browserOpen, float scrollY) {

    bool isMasterDragging = isFaderDragging(-1) || isPanDragging(-1);
    if (!isMasterDragging) {
        masterChannel_.fader = masterVol;
        masterChannel_.pan = masterPanVal;
    }
    masterChannel_.mute = masterMuteVal;
    masterChannel_.peakL = masterPeakLeft;
    masterChannel_.peakR = masterPeakRight;
    masterChannel_.peakHoldL = std::max(masterChannel_.peakHoldL * 0.96f, masterPeakLeft);
    masterChannel_.peakHoldR = std::max(masterChannel_.peakHoldR * 0.96f, masterPeakRight);

    selectedChannel_ = static_cast<int>(selectedTrackIdx);

    if (!propertiesDrawer_.isDragging()) {
        propertiesDrawer_.setExpanded(propertiesExpanded);
        propertiesDrawer_.setWidth(propertiesWidth);
        propertiesDrawer_.setScrollY(scrollY);
    }

    showRouting_ = showRouting;
    showPan_ = showPan;
    showButtons_ = showButtons;
    showMeters_ = showMeters;
    showAutomation_ = showAutomation;
    showReadouts_ = showReadouts;

    if (channels_.size() != tracks.size()) {
        channels_.resize(tracks.size());
    }
    size_t count = channels_.size();
    if (selectedChannel_ >= static_cast<int>(count)) {
        selectedChannel_ = std::max(0, static_cast<int>(count) - 1);
    }

    for (size_t i = 0; i < count; ++i) {
        const auto& trk = tracks[i];
        if (channels_[i].name != trk.name) {
            channels_[i].name = trk.name;
        }
        bool isChDragging = isFaderDragging(static_cast<int>(i)) || isPanDragging(static_cast<int>(i));
        if (!isChDragging) {
            channels_[i].fader = trk.volume;
            channels_[i].pan = trk.pan;
        }
        channels_[i].mute = trk.mute;
        channels_[i].solo = trk.solo;
        channels_[i].freeze = trk.freeze;
        channels_[i].r = trk.r;
        channels_[i].g = trk.g;
        channels_[i].b = trk.b;
        channels_[i].iconRef = trk.iconRef;
        if (channels_[i].instrument != trk.instrument) {
            channels_[i].instrument = trk.instrument;
        }
        channels_[i].chordFollowMode = trk.chordFollowMode;

        if (i < chPeaksL.size()) {
            channels_[i].peakL = chPeaksL[i];
            channels_[i].peakHoldL = std::max(channels_[i].peakHoldL * 0.96f, chPeaksL[i]);
        }
        if (i < chPeaksR.size()) {
            channels_[i].peakR = chPeaksR[i];
            channels_[i].peakHoldR = std::max(channels_[i].peakHoldR * 0.96f, chPeaksR[i]);
        }
    }
}

void MixerView::syncFromWindow(
    std::span<const ArrangerTrackData> tracks,
    uint32_t selectedTrackIdx,
    float masterVol, float masterPanVal, bool masterMuteVal,
    float masterPeakLeft, float masterPeakRight,
    const float* chPeaksL, const float* chPeaksR, size_t numPeaks,
    bool propertiesExpanded, float propertiesWidth,
    bool showRouting, bool showPan, bool showButtons, bool showMeters,
    bool showAutomation, bool showReadouts, bool browserOpen, float scrollY) {

    std::span<const float> spanL(chPeaksL ? chPeaksL : nullptr, chPeaksL ? numPeaks : 0);
    std::span<const float> spanR(chPeaksR ? chPeaksR : nullptr, chPeaksR ? numPeaks : 0);
    syncFromWindow(tracks, selectedTrackIdx, masterVol, masterPanVal, masterMuteVal,
                   masterPeakLeft, masterPeakRight, spanL, spanR,
                   propertiesExpanded, propertiesWidth,
                   showRouting, showPan, showButtons, showMeters,
                   showAutomation, showReadouts, browserOpen, scrollY);
}

void MixerView::syncFromWindow(
    const std::vector<std::string>& trackNames,
    const std::vector<float>& trackVols,
    const std::vector<float>& trackPans,
    const std::vector<bool>& trackMutes,
    const std::vector<bool>& trackSolos,
    const std::vector<bool>& trackFreezes,
    const std::vector<Color>& trackColors,
    uint32_t selectedTrackIdx,
    float masterVol, float masterPanVal, bool masterMuteVal,
    float masterPeakLeft, float masterPeakRight,
    const float* chPeaksL, const float* chPeaksR, size_t numPeaks,
    bool propertiesExpanded, float propertiesWidth,
    bool showRouting, bool showPan, bool showButtons, bool showMeters,
    bool showAutomation, bool showReadouts, bool browserOpen, float scrollY) {

    bool isMasterDragging = isFaderDragging(-1) || isPanDragging(-1);
    if (!isMasterDragging) {
        masterChannel_.fader = masterVol;
        masterChannel_.pan = masterPanVal;
    }
    masterChannel_.mute = masterMuteVal;
    masterChannel_.peakL = masterPeakLeft;
    masterChannel_.peakR = masterPeakRight;
    masterChannel_.peakHoldL = std::max(masterChannel_.peakHoldL * 0.96f, masterPeakLeft);
    masterChannel_.peakHoldR = std::max(masterChannel_.peakHoldR * 0.96f, masterPeakRight);

    selectedChannel_ = static_cast<int>(selectedTrackIdx);

    if (!propertiesDrawer_.isDragging()) {
        propertiesDrawer_.setExpanded(propertiesExpanded);
        propertiesDrawer_.setWidth(propertiesWidth);
        propertiesDrawer_.setScrollY(scrollY);
    }

    showRouting_ = showRouting;
    showPan_ = showPan;
    showButtons_ = showButtons;
    showMeters_ = showMeters;
    showAutomation_ = showAutomation;
    showReadouts_ = showReadouts;

    if (channels_.size() != trackNames.size()) {
        channels_.resize(trackNames.size());
    }
    size_t count = channels_.size();
    if (selectedChannel_ >= static_cast<int>(count)) {
        selectedChannel_ = std::max(0, static_cast<int>(count) - 1);
    }

    for (size_t i = 0; i < count; ++i) {
        if (i < trackNames.size()) channels_[i].name = trackNames[i];
        bool isChDragging = isFaderDragging(static_cast<int>(i)) || isPanDragging(static_cast<int>(i));
        if (!isChDragging) {
            if (i < trackVols.size()) channels_[i].fader = trackVols[i];
            if (i < trackPans.size()) channels_[i].pan = trackPans[i];
        }
        if (i < trackMutes.size()) channels_[i].mute = trackMutes[i];
        if (i < trackSolos.size()) channels_[i].solo = trackSolos[i];
        if (i < trackFreezes.size()) channels_[i].freeze = trackFreezes[i];
        if (i < trackColors.size()) {
            channels_[i].r = trackColors[i].r;
            channels_[i].g = trackColors[i].g;
            channels_[i].b = trackColors[i].b;
        }
        if (chPeaksL && i < numPeaks) {
            channels_[i].peakL = chPeaksL[i];
            channels_[i].peakHoldL = std::max(channels_[i].peakHoldL * 0.96f, chPeaksL[i]);
        }
        if (chPeaksR && i < numPeaks) {
            channels_[i].peakR = chPeaksR[i];
            channels_[i].peakHoldR = std::max(channels_[i].peakHoldR * 0.96f, chPeaksR[i]);
        }
    }
}

void MixerView::syncDrawerData() {
    drawerData_.isMixerMode = true;
    drawerData_.isMasterSelected = isMasterSelected_;

    drawerData_.masterEqEnabled = masterEqEnabled_;
    drawerData_.masterSubCut = masterSubCut_;
    drawerData_.masterLowGain = masterLowGain_;
    drawerData_.masterMidGain = masterMidGain_;
    drawerData_.masterHighGain = masterHighGain_;

    drawerData_.masterLimiterEnabled = masterLimiterEnabled_;
    drawerData_.masterCeilingDbfs = masterCeilingDbfs_;
    drawerData_.masterLimiterDrive = masterLimiterDrive_;
    drawerData_.masterTargetLufs = masterTargetLufs_;
    drawerData_.masterAudioFx = masterAudioFx_;

    drawerData_.totalTracks = static_cast<uint32_t>(channels_.size());

    if (isMasterSelected_) {
        drawerData_.trackIndex = 0;
        drawerData_.trackName = "Master Bus";
        drawerData_.trackType = "MASTER";
        drawerData_.volume = masterChannel_.fader;
        drawerData_.pan = masterChannel_.pan;
        drawerData_.mute = masterChannel_.mute;
        drawerData_.solo = false;
        drawerData_.freeze = false;
        drawerData_.r = 1.0f;
        drawerData_.g = 0.70f;
        drawerData_.b = 0.10f;
        drawerData_.audioFx = masterAudioFx_;
    } else if (selectedChannel_ >= 0 && selectedChannel_ < static_cast<int>(channels_.size())) {
        const auto& ch = channels_[selectedChannel_];
        drawerData_.trackIndex = static_cast<uint32_t>(selectedChannel_);
        drawerData_.trackName = ch.name;
        drawerData_.trackType = ch.instrument.empty() ? "AUDIO" : "INSTRUMENT";
        drawerData_.iconRef = ch.iconRef;
        drawerData_.volume = ch.fader;
        drawerData_.pan = ch.pan;
        drawerData_.mute = ch.mute;
        drawerData_.solo = ch.solo;
        drawerData_.freeze = ch.freeze;
        drawerData_.r = ch.r;
        drawerData_.g = ch.g;
        drawerData_.b = ch.b;
        drawerData_.instrument = ch.instrument;
        drawerData_.chordFollowMode = ch.chordFollowMode;

        // Channel EQ
        drawerData_.eqEnabled = ch.eqEnabled;
        drawerData_.eqHpf = ch.eqHpf;
        drawerData_.eqLowGain = ch.eqLowGain;
        drawerData_.eqMidFreq = ch.eqMidFreq;
        drawerData_.eqMidGain = ch.eqMidGain;
        drawerData_.eqMidQ = ch.eqMidQ;
        drawerData_.eqHighGain = ch.eqHighGain;

        drawerData_.knobs = ch.knobs;
        drawerData_.syncKnobsIfEmpty();
        drawerData_.midiFx = ch.midiFx;
        drawerData_.audioFx = ch.audioFx;
    }
}

void MixerView::render(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    float frameDt = ctx.dt > 0.0f ? ctx.dt : 0.016f;
    propertiesDrawer_.update(frameDt);
    propertiesDrawer_.layout(bounds_, ctx.browserOffset);

    // 0. Meter ballistics and peaks are updated from TelemetryPresenter via syncFromWindow

    // 1. Pinned Master Bus Strip
    renderMasterStrip(r, theme, masterBounds_);

    // Divider Line between Master Bus and Scrollable Track Strips
    if (masterPosition_ == MixerMasterPosition::Right) {
        float divX = masterBounds_.x - 12.0f;
        drawLine(r, divX, masterBounds_.y + 4.0f, divX, masterBounds_.y + masterBounds_.h - 4.0f,
                 theme.borderSubtle, 1.5f);
    } else {
        float divX = masterBounds_.x + masterBounds_.w + 12.0f;
        drawLine(r, divX, masterBounds_.y + 4.0f, divX, masterBounds_.y + masterBounds_.h - 4.0f,
                 theme.borderSubtle, 1.5f);
    }

    // 2. Horizontally Scrollable Virtualized Track Strips
    const float startX = (masterPosition_ == MixerMasterPosition::Right) ?
                         (bounds_.x + 12.0f) :
                         ((densityMode_ == MixerDensityMode::Comfortable) ? (masterBounds_.x + 195.0f) :
                         ((densityMode_ == MixerDensityMode::Compact) ? (masterBounds_.x + masterStripWidth_ + 20.0f) :
                                                                        (masterBounds_.x + masterStripWidth_ + 14.0f)));
    const float stripW = channelStripWidth_;
    const float gap = channelGap_;

    float rightBound = propertiesDrawer_.getPullTabBounds().x;
    if (rightBound <= 0.0f) {
        rightBound = bounds_.x + bounds_.w - ctx.browserOffset;
    }
    if (masterPosition_ == MixerMasterPosition::Right) {
        rightBound = std::min(rightBound, masterBounds_.x - 14.0f);
    }

    for (size_t i = 0; i < channels_.size(); ++i) {
        float cx = startX + static_cast<float>(i) * (stripW + gap) - scrollX_;
        if (masterPosition_ != MixerMasterPosition::Right) {
            if (cx + stripW < 185.0f && densityMode_ == MixerDensityMode::Comfortable) continue;
            if (cx + stripW < masterBounds_.x + masterStripWidth_ + 10.0f) continue;
        } else {
            if (cx + stripW < bounds_.x) continue;
        }
        if (cx > rightBound) break;

        Rect2D stripRect(cx, masterBounds_.y, stripW, masterBounds_.h);
        renderChannelStrip(r, theme, channels_[i], stripRect, i, selectedChannel_ == static_cast<int>(i));
    }

    // 3. Top Options Toolbar (Always visible, renders crisp above channel strips)
    renderToolbar(r, theme);

    // 4. Sliding Track Properties Drawer (Reusing Decoupled Drawer)
    syncDrawerData();
    propertiesDrawer_.render(r, theme, drawerData_, ctx.mouseX, ctx.mouseY);

    // 4. Floating tactile tooltip badge on top
    renderTooltip(r, theme);
}

void MixerView::renderToolbar(BatchRenderer2D& r, const ThemeTokens& theme) {
    // Top Options Toolbar (always visible in Mixer)
    float tx = toolbarBounds_.x;
    float ty = toolbarBounds_.y;
    float tw = toolbarBounds_.w;
    float th = toolbarBounds_.h;

    drawRoundedRect(r, tx, ty, tw, th, 0.0f, theme.panelHeader);
    drawLine(r, tx, ty + th - 1.0f, tx + tw, ty + th - 1.0f, theme.borderSubtle, 1.5f);

    // Title
    drawText(r, "MIXER", tx + 12.0f, ty + 8.5f, 10.0f, theme.primaryAccent);

    auto drawPill = [&](float x, float y, float w, float h, const char* label, bool active, Color activeColor) {
        if (active) {
            drawRoundedRect(r, x, y, w, h, 3.0f, activeColor.withAlpha(0.25f));
            drawRoundedRectOutline(r, x, y, w, h, 3.0f, activeColor, 1.2f);
            drawText(r, label, x + 6.0f, y + 4.5f, 8.5f, activeColor);
        } else {
            drawRoundedRect(r, x, y, w, h, 3.0f, theme.controlBackground);
            drawRoundedRectOutline(r, x, y, w, h, 3.0f, theme.borderSubtle, 1.0f);
            drawText(r, label, x + 6.0f, y + 4.5f, 8.5f, theme.textSecondary);
        }
    };

    // Density Mode Pills
    float curX = tx + 64.0f;
    drawPill(curX, ty + 4.0f, 76.0f, 22.0f, "COMFORT", densityMode_ == MixerDensityMode::Comfortable, theme.primaryAccent);
    curX += 80.0f;
    drawPill(curX, ty + 4.0f, 66.0f, 22.0f, "COMPACT", densityMode_ == MixerDensityMode::Compact, theme.secondaryAccent);
    curX += 70.0f;
    drawPill(curX, ty + 4.0f, 54.0f, 22.0f, "MICRO", densityMode_ == MixerDensityMode::Micro, theme.highlight);
    curX += 60.0f;

    // Divider
    drawLine(r, curX, ty + 6.0f, curX, ty + th - 6.0f, theme.borderSubtle, 1.0f);
    curX += 8.0f;

    // Section Toggle Pills
    drawPill(curX, ty + 4.0f, 56.0f, 22.0f, "METERS", showMeters_, theme.highlight);
    curX += 60.0f;
    drawPill(curX, ty + 4.0f, 62.0f, 22.0f, "ROUTING", showRouting_, theme.secondaryAccent);
    curX += 66.0f;
    drawPill(curX, ty + 4.0f, 42.0f, 22.0f, "PAN", showPan_, theme.primaryAccent);
    curX += 46.0f;
    drawPill(curX, ty + 4.0f, 68.0f, 22.0f, "READOUT", showReadouts_, theme.textPrimary);
    curX += 74.0f;

    // Master Position Pill
    std::string mPosLabel = (masterPosition_ == MixerMasterPosition::Left) ? "MST: LEFT" : "MST: RIGHT";
    drawPill(curX, ty + 4.0f, 74.0f, 22.0f, mPosLabel.c_str(), true, theme.primaryAccent);
}

void MixerView::renderBacklitLcd(BatchRenderer2D& r, const ThemeTokens& theme, float x, float y, float w, float h,
                                 const std::string& title, const std::string& leftText, const std::string& rightText,
                                 const Color& titleColor) {
    // Inset LCD bezel & well using semantic theme tokens
    drawRoundedRect(r, x, y, w, h, 4.0f, theme.lcdBackground);
    drawRoundedRectOutline(r, x, y, w, h, 4.0f, theme.lcdBorder, 1.4f);

    // Inner bevel shadow (top and left inset shadow from ambient light)
    drawLine(r, x + 1.0f, y + 1.0f, x + w - 1.0f, y + 1.0f, 0.0f, 0.0f, 0.0f, 0.45f, 1.0f);
    drawLine(r, x + 1.0f, y + 2.0f, x + 1.0f, y + h - 1.0f, 0.0f, 0.0f, 0.0f, 0.35f, 1.0f);

    // Subtle dot matrix scan lines
    for (float gy = y + 4.0f; gy < y + h - 2.0f; gy += 3.5f) {
        drawLine(r, x + 4.0f, gy, x + w - 4.0f, gy, theme.lcdDotGrid, 1.0f);
    }

    // Top centered title
    float textW = static_cast<float>(title.length()) * 6.6f;
    float textX = x + std::max(4.0f, (w - textW) * 0.5f);
    drawText(r, title, textX, y + 4.0f, 10.5f, titleColor);

    // Bottom row: Left status (pan / st-out) | Right status (vol %)
    Color subText = theme.lcdText.withAlpha(0.85f);
    drawText(r, leftText, x + 6.0f, y + 20.0f, 9.0f, subText);
    float rw = static_cast<float>(rightText.length()) * 6.0f;
    drawText(r, rightText, x + w - rw - 6.0f, y + 20.0f, 9.0f, subText);

    // Realistic smooth glass reflection sheen across upper glass half
    float sheenH = std::max(4.0f, (h - 4.0f) * 0.45f);
    drawRoundedRect(r, x + 2.0f, y + 2.0f, w - 4.0f, sheenH, 3.0f, Color(1.0f, 1.0f, 1.0f, 0.045f));
    // Soft top-edge light catch on glass
    drawLine(r, x + 3.0f, y + 2.5f, x + w - 3.0f, y + 2.5f, 1.0f, 1.0f, 1.0f, 0.14f, 1.0f);
    // Subtle bottom-right inner specular bounce highlight
    drawLine(r, x + 2.0f, y + h - 1.2f, x + w - 2.0f, y + h - 1.2f, 1.0f, 1.0f, 1.0f, 0.08f, 1.0f);
}

void MixerView::renderRotaryPanKnob(BatchRenderer2D& r, const ThemeTokens& theme, float kx, float ky, float radius, float pan,
                                    const Color& accentColor) {
    // If user explicitly forced minimalist mode, keep the lightweight flat vector dial
    if (theme.controlMode == ControlAestheticMode::ForceMinimal) {
        drawCircle(r, kx, ky, radius + 2.0f, theme.controlWell.withAlpha(0.6f));
        drawCircle(r, kx, ky, radius, theme.controlBackground);
        drawCircleOutline(r, kx, ky, radius, theme.borderSubtle, 1.2f);
        float angle = pan * 2.35619449f;
        float sinA = std::sin(angle), cosA = std::cos(angle);
        drawLine(r, kx + sinA * 3.5f, ky - cosA * 3.5f,
                 kx + sinA * (radius - 2.0f), ky - cosA * (radius - 2.0f),
                 accentColor, 2.0f);
        drawCircle(r, kx, ky, 2.0f, theme.borderFocus);
        return;
    }

    const LightSource2D& light = theme.globalLight;
    Point lDir = light.getLightDirection(kx, ky);
    float lightAngle = std::atan2(lDir.y, lDir.x);
    bool isLightChassis = theme.isLight;

    // 1. Analytical Parallax Drop Shadow onto Faceplate
    // Computes contextual shadow offset based on position relative to overhead light
    r.drawCircleDropShadow(kx, ky, radius * 1.05f, 5.5f, light, isLightChassis ? 0.35f : 0.48f);

    // 2. Recessed Sunken Faceplate Mounting Gutter / Trench Well
    float trenchR = radius + 2.2f;
    Color wellDark = theme.controlWell.darken(0.35f);
    r.drawCircle(kx, ky, trenchR, wellDark.r, wellDark.g, wellDark.b, 0.95f, 32);
    r.drawCircleOutline(kx, ky, trenchR, 0.02f, 0.03f, 0.04f, 0.90f, 1.2f, 32);
    // Subtle chamfer highlight arc on trench shoulder facing away from light
    r.drawArc(kx, ky, trenchR, lightAngle + 1.8f, lightAngle + 4.2f,
              1.0f, 1.0f, 1.0f, isLightChassis ? 0.40f : 0.20f, 1.0f, 24);

    // 3. Calibration Dial Ticks & Numerical Graduations (e.g. 25, 50, 75, 100)
    constexpr float minAngle = -2.35619449f; // -135 degrees
    constexpr float maxAngle =  2.35619449f; // +135 degrees
    float currentAngle = pan * 2.35619449f; // clamped in [-135, +135]

    float trackR = radius + 2.0f;
    float tickR0 = trackR + 1.2f;
    float tickLen = (densityMode_ == MixerDensityMode::Comfortable) ? 3.8f : 2.5f;
    float majorTickLen = (densityMode_ == MixerDensityMode::Comfortable) ? 5.8f : 3.8f;
    Color tickCol = isLightChassis ? Color(0.20f, 0.22f, 0.26f, 0.85f) : Color(0.68f, 0.72f, 0.78f, 0.75f);

    const int numTicks = 8;
    for (int t = 0; t <= numTicks; ++t) {
        float normT = static_cast<float>(t) / static_cast<float>(numTicks);
        float a = minAngle + normT * (maxAngle - minAngle);
        float sa = std::sin(a), ca = std::cos(a);
        bool isCenter = (t == numTicks / 2);
        bool isMajor = (t == 0 || t == numTicks || isCenter || t == 2 || t == 6);

        if (isCenter) {
            // Bold center detent calibration block at 12 o'clock
            drawLine(r, kx, ky - tickR0, kx, ky - (tickR0 + majorTickLen + 1.5f),
                     accentColor.r, accentColor.g, accentColor.b, 0.90f, 2.2f);
        } else {
            float tLen = isMajor ? majorTickLen : tickLen;
            float tWidth = isMajor ? 1.4f : 1.0f;
            drawLine(r, kx + sa * tickR0, ky - ca * tickR0,
                     kx + sa * (tickR0 + tLen), ky - ca * (tickR0 + tLen),
                     tickCol.r, tickCol.g, tickCol.b, tickCol.a, tWidth);
        }
    }

    // Dial numeric labels in Comfortable density mode (100, 50 | 50, 100)
    if (densityMode_ == MixerDensityMode::Comfortable && radius >= 14.0f) {
        float labelR = tickR0 + majorTickLen + 5.0f;
        Color lblCol = isLightChassis ? Color(0.25f, 0.28f, 0.32f, 0.80f) : Color(0.60f, 0.65f, 0.72f, 0.80f);

        auto drawDialLabel = [&](float ang, const char* txt) {
            float lx = kx + std::sin(ang) * labelR;
            float ly = ky - std::cos(ang) * labelR;
            drawCenteredText(r, txt, lx - 12.0f, ly - 5.0f, 24.0f, 10.0f, 6.5f, lblCol);
        };
        drawDialLabel(minAngle, "100");
        drawDialLabel(minAngle * 0.5f, "50");
        drawDialLabel(maxAngle * 0.5f, "50");
        drawDialLabel(maxAngle, "100");
    }

    // 4. Directional Knurled / Fluted Rotor Rim
    float capR = radius * 0.72f;
    Color bodyBase = isLightChassis ? Color(0.72f, 0.74f, 0.78f, 1.0f) : Color(0.14f, 0.15f, 0.18f, 1.0f);
    Color bodyHl   = bodyBase.lighten(isLightChassis ? 0.20f : 0.35f);
    Color bodySh   = bodyBase.darken(isLightChassis ? 0.25f : 0.45f);

    r.drawFlutedRing(kx, ky, capR, radius, 22, 0.12f, 0.0f, light,
                     bodyBase.r, bodyBase.g, bodyBase.b,
                     bodyHl.r, bodyHl.g, bodyHl.b,
                     bodySh.r, bodySh.g, bodySh.b);
    r.drawCircleOutline(kx, ky, radius, bodySh.r * 0.8f, bodySh.g * 0.8f, bodySh.b * 0.8f, 0.85f, 1.0f, 32);

    // 5. Specular Highlight Glint facing the virtual overhead light
    r.drawArc(kx, ky, radius - 0.5f, lightAngle - 1.25f, lightAngle + 1.25f,
              1.0f, 1.0f, 1.0f, isLightChassis ? 0.65f : 0.35f, 1.2f, 24);

    // 6. Concave Inset Dish Center Cap with directional inverted depth
    Color capBase = isLightChassis ? Color(0.82f, 0.84f, 0.88f, 1.0f) : Color(0.10f, 0.11f, 0.13f, 1.0f);
    Color capHl   = capBase.lighten(isLightChassis ? 0.15f : 0.28f);
    Color capSh   = capBase.darken(isLightChassis ? 0.20f : 0.35f);

    r.drawCircleRadial3StopGradient(kx, ky, capR,
                                    capHl.r, capHl.g, capHl.b, 1.0f,
                                    capBase.r, capBase.g, capBase.b, 1.0f,
                                    capSh.r, capSh.g, capSh.b, 1.0f,
                                    lDir.x * capR * 0.28f, lDir.y * capR * 0.28f, 0.50f, 28);
    // Beveled rim separating knurled body and cap
    r.drawCircleOutline(kx, ky, capR,
                        isLightChassis ? 0.45f : 0.04f,
                        isLightChassis ? 0.47f : 0.05f,
                        isLightChassis ? 0.52f : 0.06f,
                        0.95f, 1.2f, 28);

    // 7. Subtle Active Bipolar Halo Arc (glow from center to current value)
    if (std::abs(pan) > 0.015f) {
        float centerA = 0.0f;
        float a0 = std::min(centerA, currentAngle);
        float a1 = std::max(centerA, currentAngle);
        r.drawArc(kx, ky, capR - 1.2f, a0, a1,
                  accentColor.r, accentColor.g, accentColor.b, 0.60f, 1.8f);
    }

    // 8. Illuminated Indicator Pip & Notch
    float sinA = std::sin(currentAngle);
    float cosA = std::cos(currentAngle);

    // Radial etched indicator groove extending across cap and knurl rim
    drawLine(r, kx + sinA * (capR * 0.35f), ky - cosA * (capR * 0.35f),
             kx + sinA * (radius - 1.2f),   ky - cosA * (radius - 1.2f),
             accentColor.r, accentColor.g, accentColor.b, 0.95f, 2.0f);

    // Glowing jewel pip near cap perimeter
    float pipDist = capR * 0.72f;
    float pipX = kx + sinA * pipDist;
    float pipY = ky - cosA * pipDist;
    r.drawCircle(pipX, pipY, 2.0f, accentColor.r, accentColor.g, accentColor.b, 1.0f);
    r.drawCircle(pipX, pipY, 0.9f, 1.0f, 1.0f, 1.0f, 0.90f); // Bright center specular dot

    // Center jewel screw/rivet
    r.drawCircle(kx, ky, 2.2f, capSh.r, capSh.g, capSh.b, 1.0f);
    r.drawCircleOutline(kx, ky, 2.2f, 0.02f, 0.03f, 0.04f, 0.85f, 1.0f);
}

void MixerView::renderCenterButton(BatchRenderer2D& r, const ThemeTokens& theme, float bx, float by, float bw, float bh) {
    const LightSource2D& light = theme.globalLight;
    r.drawRectDropShadow(bx, by, bw, bh, 2.5f, 2.0f, light, theme.isLight ? 0.25f : 0.35f);
    Color bg = theme.controlBackground;
    drawRoundedRect(r, bx, by, bw, bh, 2.5f, bg);
    drawRoundedRectOutline(r, bx, by, bw, bh, 2.5f, theme.borderSubtle, 1.0f);
    // Bevel highlight
    drawLine(r, bx + 1.5f, by + 1.0f, bx + bw - 1.5f, by + 1.0f, 1.0f, 1.0f, 1.0f, theme.isLight ? 0.50f : 0.18f, 1.0f);
    drawCenteredText(r, "C", bx, by + 1.0f, bw, bh, 8.5f, theme.textSecondary);
}

void MixerView::renderTooltip(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!showTooltip_ || tooltipText_.empty()) return;

    float tw = static_cast<float>(tooltipText_.length()) * 6.5f + 16.0f;
    float th = 20.0f;
    float tx = tooltipX_ - tw * 0.5f;

    const float effW = r.getCurrentWidth();
    const float effH = r.getCurrentHeight();

    if (effW > 0.0f) {
        tx = std::clamp(tx, 8.0f, std::max(8.0f, effW - tw - 8.0f));
    }

    float ty = tooltipY_ - 26.0f;
    // Auto-flip and clamp within the Mixer DAW workspace (y in [58.0, effH - 51.0])
    // Never crosses into the top transport faceplate (y < 56.0f) or bottom chin (y > effH - 48.0f)
    if (ty < 58.0f) {
        ty = tooltipY_ + 14.0f; // Flip below cursor when fader is near top
    }
    if (effH > 0.0f && ty + th > (effH - 51.0f)) {
        ty = tooltipY_ - 26.0f; // Flip above cursor when fader is near bottom chin
    }
    const float maxDawY = (effH > 0.0f) ? (effH - 48.0f - th - 3.0f) : 9999.0f;
    ty = std::clamp(ty, 58.0f, std::max(58.0f, maxDawY));

    // Drop shadow
    drawRoundedRect(r, tx + 1.0f, ty + 2.0f, tw, th, 4.0f, Color(0.0f, 0.0f, 0.0f, 0.45f));
    // Panel Header background & outline
    drawRoundedRect(r, tx, ty, tw, th, 4.0f, theme.panelHeader);
    drawRoundedRectOutline(r, tx, ty, tw, th, 4.0f, theme.borderFocus, 1.0f);
    // Crisp text
    drawText(r, tooltipText_, tx + 8.0f, ty + 4.5f, 9.5f, theme.textPrimary);
}

void MixerView::renderMasterStrip(BatchRenderer2D& r, const ThemeTokens& theme, const Rect2D& b) {
    float mX = (b.x == 0.0f) ? 12.0f : b.x;
    float mW = masterStripWidth_;
    float mY = b.y;
    float stripH = b.h;

    // Master Chassis with theme colors
    drawRoundedRect(r, mX, mY, mW, stripH, 8.0f, theme.panelBackground.darken(0.04f), 0.98f);
    Color masterBorder = isMasterSelected_ ? theme.primaryAccent : theme.primaryAccent.withAlpha(0.75f);
    drawRoundedRectOutline(r, mX, mY, mW, stripH, 8.0f, masterBorder, isMasterSelected_ ? 2.0f : 1.5f);

    float lcdH = (densityMode_ == MixerDensityMode::Micro) ? 24.0f : ((densityMode_ == MixerDensityMode::Compact) ? 32.0f : 38.0f);
    int mVolPct = static_cast<int>(std::round(masterChannel_.fader * 100.0f));

    if (showReadouts_) {
        if (densityMode_ == MixerDensityMode::Micro) {
            renderBacklitLcd(r, theme, mX + 4.0f, mY + 6.0f, mW - 8.0f, lcdH, "MST", "", std::to_string(mVolPct) + "%", theme.primaryAccent);
        } else if (densityMode_ == MixerDensityMode::Compact) {
            renderBacklitLcd(r, theme, mX + 6.0f, mY + 6.0f, mW - 12.0f, lcdH, "MASTER", "out", std::to_string(mVolPct) + "%", theme.primaryAccent);
        } else {
            renderBacklitLcd(r, theme, mX + 8.0f, mY + 8.0f, mW - 16.0f, lcdH, "MASTER", "st-out", std::to_string(mVolPct) + "%", theme.primaryAccent);
        }
    }

    // Master Balance Control (Rotary Knob + "C" button)
    float kx = mX + mW * 0.5f;
    float ky = mY + 8.0f + (showReadouts_ ? lcdH : 0.0f) + 16.0f;
    if (showPan_) {
        float panRadius = (densityMode_ == MixerDensityMode::Micro) ? 9.0f : ((densityMode_ == MixerDensityMode::Compact) ? 12.0f : 16.0f);
        renderRotaryPanKnob(r, theme, kx, ky, panRadius, masterChannel_.pan, theme.primaryAccent);
        if (densityMode_ == MixerDensityMode::Comfortable) {
            renderCenterButton(r, theme, kx - 9.0f, ky + 18.0f, 18.0f, 14.0f);
        }
    }

    // Fader & Meter Well
    auto geom = getMasterFaderGeometry();
    float wellTopY = geom.wellTopY;
    float wellBottomY = geom.wellBottomY;
    float wellH = geom.wellH;
    float fx = geom.trackX;

    // Master Fader Track Slot (Recessed Trench with Chamfer Edge)
    float slotW = (densityMode_ == MixerDensityMode::Micro) ? 3.0f : 4.5f;
    float slotX = fx - slotW * 0.5f;
    r.drawRoundedRect(slotX, wellTopY, slotW, wellH, 1.5f,
                      theme.controlWell.r * 0.45f, theme.controlWell.g * 0.45f, theme.controlWell.b * 0.45f, 1.0f);
    // Left inner shadow
    r.drawLine(slotX, wellTopY, slotX, wellBottomY, 0.0f, 0.0f, 0.0f, 0.65f, 1.0f);
    // Right bevel highlight
    r.drawLine(slotX + slotW, wellTopY, slotX + slotW, wellBottomY, 1.0f, 1.0f, 1.0f, theme.isLight ? 0.35f : 0.15f, 1.0f);
    // Center guide rail
    r.drawLine(fx, wellTopY, fx, wellBottomY, 0.0f, 0.0f, 0.0f, 0.35f, 1.0f);

    // Calibration dB tick marks along the fader slot (Left side of track fx)
    if (densityMode_ != MixerDensityMode::Micro) {
        auto drawTick = [&](float gain, const char* label, bool prominent) {
            float t = presenter::audio_taper::gainToTravel(gain);
            float y = wellBottomY - t * wellH;
            float tickLen = prominent ? 6.0f : 4.0f;
            float alpha = prominent ? 0.70f : 0.35f;
            drawLine(r, fx - 16.0f, y, fx - 16.0f + tickLen, y, theme.textMuted.withAlpha(alpha), 1.0f);
            if (label && densityMode_ == MixerDensityMode::Comfortable) {
                drawText(r, label, fx - 34.0f, y - 3.5f, 6.5f, theme.textMuted.withAlpha(alpha));
            }
        };
        drawTick(1.5f, "+6", false);
        drawTick(1.0f, "0", true);
        drawTick(0.501187f, "-6", false);
        drawTick(0.251189f, "-12", false);
        drawTick(0.0f, "-inf", true);
    }

    float mFy = geom.thumbY;
    bool isLight = theme.isLight;
    float tx = geom.thumb.x;
    float ty = geom.thumb.y;
    float tw = geom.thumb.w;
    float th = geom.thumb.h;

    // Tactile Drop Shadow cast by vertical hardware fader cap onto chassis and well
    r.drawRectDropShadow(tx, ty, tw, th, 4.0f, 6.0f, theme.globalLight, isLight ? 0.35f : 0.50f);

    // Multi-tier Elevation Base (Upper ramp, Center plateau, Lower ramp)
    Color rampTop = isLight ? Color(0.92f, 0.94f, 0.96f, 1.0f) : Color(0.36f, 0.38f, 0.42f, 1.0f);
    Color rampMid = isLight ? Color(0.78f, 0.80f, 0.84f, 1.0f) : Color(0.22f, 0.24f, 0.27f, 1.0f);
    Color rampBot = isLight ? Color(0.60f, 0.62f, 0.66f, 1.0f) : Color(0.10f, 0.11f, 0.13f, 1.0f);

    float halfH = th * 0.5f;
    // Top elevated slope
    r.drawRectGradient(tx, ty, tw, halfH, rampTop.r, rampTop.g, rampTop.b, rampMid.r, rampMid.g, rampMid.b, 1.0f);
    // Bottom elevated slope
    r.drawRectGradient(tx, ty + halfH, tw, halfH, rampMid.r, rampMid.g, rampMid.b, rampBot.r, rampBot.g, rampBot.b, 1.0f);

    // Center finger-well depression (subtle recessed trough)
    float fWellH = th * 0.38f;
    float fWellY = mFy - fWellH * 0.5f;
    r.drawRect(tx + 2.0f, fWellY, tw - 4.0f, fWellH, 0.0f, 0.0f, 0.0f, isLight ? 0.08f : 0.22f);

    // Bevel outlines
    r.drawRoundedRectOutline(tx, ty, tw, th, 2.5f,
                             isLight ? 0.38f : 0.06f, isLight ? 0.40f : 0.06f, isLight ? 0.44f : 0.07f, 0.90f, 1.0f);

    // Top specular edge highlight
    r.drawLine(tx + 1.0f, ty + 0.8f, tx + tw - 1.0f, ty + 0.8f, 1.0f, 1.0f, 1.0f, isLight ? 0.85f : 0.55f, 1.0f);
    // Left edge light highlight
    r.drawLine(tx + 0.8f, ty + 2.0f, tx + 0.8f, ty + th - 2.0f, 1.0f, 1.0f, 1.0f, isLight ? 0.45f : 0.25f, 1.0f);
    // Right edge bevel shadow
    r.drawLine(tx + tw - 0.8f, ty + 2.0f, tx + tw - 0.8f, ty + th - 2.0f, 0.0f, 0.0f, 0.0f, 0.45f, 1.0f);
    // Bottom shadow edge
    r.drawLine(tx + 1.0f, ty + th - 0.8f, tx + tw - 1.0f, ty + th - 0.8f, 0.0f, 0.0f, 0.0f, 0.70f, 1.0f);

    // Horizontal milled tactile anti-slip grip ridges
    for (float off : {-7.0f, -4.0f, 4.0f, 7.0f}) {
        float gy = mFy + off;
        if (gy > ty + 3.0f && gy < ty + th - 3.0f) {
            r.drawLine(tx + 3.0f, gy, tx + tw - 3.0f, gy,
                       0.0f, 0.0f, 0.0f, isLight ? 0.35f : 0.75f, 1.0f);
            r.drawLine(tx + 3.0f, gy + 0.8f, tx + tw - 3.0f, gy + 0.8f,
                       1.0f, 1.0f, 1.0f, isLight ? 0.40f : 0.18f, 0.8f);
        }
    }

    // Prominent Glowing Center Indicator Stripe (Accent color + white core)
    r.drawLine(tx + 1.5f, mFy, tx + tw - 1.5f, mFy,
               theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f, 2.2f);
    r.drawLine(tx + 3.0f, mFy, tx + tw - 3.0f, mFy,
               1.0f, 1.0f, 1.0f, 0.95f, 1.0f);

    // Master Dual Stereo Meter on Right (if showMeters_)
    if (showMeters_) {
        float mx = (densityMode_ == MixerDensityMode::Micro) ? (mX + 36.0f) : ((densityMode_ == MixerDensityMode::Compact) ? (mX + 52.0f) : (mX + 70.0f));
        float mw = (densityMode_ == MixerDensityMode::Micro) ? 18.0f : ((densityMode_ == MixerDensityMode::Compact) ? 28.0f : 44.0f);
        renderLedMeter(r, theme, mx, wellTopY, mw, wellH,
                       masterChannel_.mute ? 0.0f : masterChannel_.peakL,
                       masterChannel_.mute ? 0.0f : masterChannel_.peakR,
                       masterChannel_.peakHoldL, masterChannel_.peakHoldR);
    }
}

void MixerView::renderChannelStrip(BatchRenderer2D& r, const ThemeTokens& theme, MixerChannelStrip& ch,
                                   const Rect2D& b, size_t index, bool isSelected) {
    float cx = b.x;
    float cy = b.y;
    float stripW = channelStripWidth_;
    float stripH = b.h;

    // Chassis
    Color bg = isSelected ? theme.panelBackground.lighten(0.04f) : theme.panelBackground;
    drawRoundedRect(r, cx, cy, stripW, stripH, 8.0f, bg);
    if (isSelected) {
        drawRoundedRectOutline(r, cx, cy, stripW, stripH, 8.0f, Color(ch.r, ch.g, ch.b), 2.0f);
    } else {
        drawRoundedRectOutline(r, cx, cy, stripW, stripH, 8.0f, theme.borderSubtle, 1.0f);
    }

    // Top Backlit LCD / Header
    float lcdH = (densityMode_ == MixerDensityMode::Micro) ? 24.0f : ((densityMode_ == MixerDensityMode::Compact) ? 32.0f : 38.0f);
    int volPct = static_cast<int>(std::round(ch.fader * 100.0f));
    std::string titleStr = ch.name;
    std::transform(titleStr.begin(), titleStr.end(), titleStr.begin(), ::toupper);

    if (showReadouts_) {
        if (densityMode_ == MixerDensityMode::Micro) {
            std::string shortTitle = titleStr.substr(0, std::min<size_t>(4, titleStr.length()));
            renderBacklitLcd(r, theme, cx + 4.0f, cy + 6.0f, stripW - 8.0f, lcdH, shortTitle, "", std::to_string(volPct) + "%", Color(ch.r, ch.g, ch.b));
        } else if (densityMode_ == MixerDensityMode::Compact) {
            std::string shortTitle = titleStr.substr(0, std::min<size_t>(7, titleStr.length()));
            std::string panStr = (std::abs(ch.pan) < 0.05f) ? "C" : ((ch.pan < 0) ? ("L" + std::to_string(static_cast<int>(-ch.pan * 100))) : ("R" + std::to_string(static_cast<int>(ch.pan * 100))));
            renderBacklitLcd(r, theme, cx + 6.0f, cy + 6.0f, stripW - 12.0f, lcdH, shortTitle, panStr, std::to_string(volPct) + "%", Color(ch.r, ch.g, ch.b));
        } else {
            std::string panStr = (std::abs(ch.pan) < 0.03f) ? "center" : ((ch.pan < 0) ? ("L" + std::to_string(static_cast<int>(-ch.pan * 100))) : ("R" + std::to_string(static_cast<int>(ch.pan * 100))));
            renderBacklitLcd(r, theme, cx + 8.0f, cy + 8.0f, stripW - 16.0f, lcdH, titleStr, panStr, std::to_string(volPct) + "%", Color(ch.r, ch.g, ch.b));
        }
    }

    // Pan Rotary Knob & "C" button (if showPan_)
    float kx = cx + stripW * 0.5f;
    float ky = cy + 8.0f + (showReadouts_ ? lcdH : 0.0f) + 16.0f;
    if (showPan_) {
        float panRadius = (densityMode_ == MixerDensityMode::Micro) ? 9.0f : ((densityMode_ == MixerDensityMode::Compact) ? 12.0f : 16.0f);
        renderRotaryPanKnob(r, theme, kx, ky, panRadius, ch.pan, Color(ch.r, ch.g, ch.b));
        if (densityMode_ == MixerDensityMode::Comfortable) {
            renderCenterButton(r, theme, kx - 9.0f, ky + 18.0f, 18.0f, 14.0f);
        }
    }

    // Lower Section: Fader (Left) + Stereo Meter (Center) + Hardware Buttons (Right)
    auto geom = getChannelFaderGeometry(index, cx);
    float wellTopY = geom.wellTopY;
    float wellBottomY = geom.wellBottomY;
    float wellH = geom.wellH;
    float fx = geom.trackX;

    // 1. Vertical Console Fader (Recessed Trench with Chamfer Edge)
    float slotW = (densityMode_ == MixerDensityMode::Micro) ? 3.0f : 4.5f;
    float slotX = fx - slotW * 0.5f;
    r.drawRoundedRect(slotX, wellTopY, slotW, wellH, 1.5f,
                      theme.controlWell.r * 0.45f, theme.controlWell.g * 0.45f, theme.controlWell.b * 0.45f, 1.0f);
    // Left inner shadow
    r.drawLine(slotX, wellTopY, slotX, wellBottomY, 0.0f, 0.0f, 0.0f, 0.65f, 1.0f);
    // Right bevel highlight
    r.drawLine(slotX + slotW, wellTopY, slotX + slotW, wellBottomY, 1.0f, 1.0f, 1.0f, theme.isLight ? 0.35f : 0.15f, 1.0f);
    // Center guide rail
    r.drawLine(fx, wellTopY, fx, wellBottomY, 0.0f, 0.0f, 0.0f, 0.35f, 1.0f);

    // Calibration tick markings along the fader slot (Comfortable & Compact)
    if (densityMode_ != MixerDensityMode::Micro) {
        auto drawChTick = [&](float gain, const char* label, bool prominent) {
            float t = presenter::audio_taper::gainToTravel(gain);
            float y = wellBottomY - t * wellH;
            float alpha = prominent ? 0.65f : 0.30f;
            drawLine(r, fx - 15.0f, y, fx - 11.0f, y, theme.textMuted.withAlpha(alpha), 1.0f);
            if (label && prominent && densityMode_ == MixerDensityMode::Comfortable) {
                drawText(r, label, fx - 24.0f, y - 3.5f, 6.5f, theme.textMuted.withAlpha(alpha));
            }
        };
        drawChTick(1.5f, "+6", false);
        drawChTick(1.0f, "0", true);
        drawChTick(0.501187f, "-6", false);
        drawChTick(0.251189f, "-12", false);
        drawChTick(0.0f, "-inf", true);
    }

    float fy = geom.thumbY;
    bool isLight = theme.isLight;
    float tx = geom.thumb.x;
    float ty = geom.thumb.y;
    float tw = geom.thumb.w;
    float th = geom.thumb.h;

    // Tactile Drop Shadow cast by vertical fader cap onto chassis and slot
    r.drawRectDropShadow(tx, ty, tw, th, 4.0f, 6.0f, theme.globalLight, isLight ? 0.35f : 0.50f);

    // Multi-tier Elevation Base (Upper ramp, Center plateau, Lower ramp)
    Color rampTop = isLight ? Color(0.92f, 0.94f, 0.96f, 1.0f) : Color(0.36f, 0.38f, 0.42f, 1.0f);
    Color rampMid = isLight ? Color(0.78f, 0.80f, 0.84f, 1.0f) : Color(0.22f, 0.24f, 0.27f, 1.0f);
    Color rampBot = isLight ? Color(0.60f, 0.62f, 0.66f, 1.0f) : Color(0.10f, 0.11f, 0.13f, 1.0f);

    float halfH = th * 0.5f;
    // Top elevated slope
    r.drawRectGradient(tx, ty, tw, halfH, rampTop.r, rampTop.g, rampTop.b, rampMid.r, rampMid.g, rampMid.b, 1.0f);
    // Bottom elevated slope
    r.drawRectGradient(tx, ty + halfH, tw, halfH, rampMid.r, rampMid.g, rampMid.b, rampBot.r, rampBot.g, rampBot.b, 1.0f);

    // Center finger-well depression (subtle recessed trough)
    float fWellH = th * 0.38f;
    float fWellY = fy - fWellH * 0.5f;
    r.drawRect(tx + 2.0f, fWellY, tw - 4.0f, fWellH, 0.0f, 0.0f, 0.0f, isLight ? 0.08f : 0.22f);

    // Bevel outlines
    r.drawRoundedRectOutline(tx, ty, tw, th, 2.5f,
                             isLight ? 0.38f : 0.06f, isLight ? 0.40f : 0.06f, isLight ? 0.44f : 0.07f, 0.90f, 1.0f);

    // Top specular edge highlight
    r.drawLine(tx + 1.0f, ty + 0.8f, tx + tw - 1.0f, ty + 0.8f, 1.0f, 1.0f, 1.0f, isLight ? 0.85f : 0.55f, 1.0f);
    // Left edge light highlight
    r.drawLine(tx + 0.8f, ty + 2.0f, tx + 0.8f, ty + th - 2.0f, 1.0f, 1.0f, 1.0f, isLight ? 0.45f : 0.25f, 1.0f);
    // Right edge bevel shadow
    r.drawLine(tx + tw - 0.8f, ty + 2.0f, tx + tw - 0.8f, ty + th - 2.0f, 0.0f, 0.0f, 0.0f, 0.45f, 1.0f);
    // Bottom shadow edge
    r.drawLine(tx + 1.0f, ty + th - 0.8f, tx + tw - 1.0f, ty + th - 0.8f, 0.0f, 0.0f, 0.0f, 0.70f, 1.0f);

    // Horizontal milled tactile anti-slip grip ridges
    for (float off : {-7.0f, -4.0f, 4.0f, 7.0f}) {
        float gy = fy + off;
        if (gy > ty + 3.0f && gy < ty + th - 3.0f) {
            r.drawLine(tx + 3.0f, gy, tx + tw - 3.0f, gy,
                       0.0f, 0.0f, 0.0f, isLight ? 0.35f : 0.75f, 1.0f);
            r.drawLine(tx + 3.0f, gy + 0.8f, tx + tw - 3.0f, gy + 0.8f,
                       1.0f, 1.0f, 1.0f, isLight ? 0.40f : 0.18f, 0.8f);
        }
    }

    // Glowing center indicator stripe in channel accent color
    r.drawLine(tx + 1.5f, fy, tx + tw - 1.5f, fy,
               ch.r, ch.g, ch.b, 1.0f, 2.2f);
    r.drawLine(tx + 3.0f, fy, tx + tw - 3.0f, fy,
               1.0f, 1.0f, 1.0f, 0.95f, 1.0f);

    // 2. Inset Glass Dual Stereo Meter (Right of fader)
    if (showMeters_) {
        float mx = (densityMode_ == MixerDensityMode::Micro) ? (cx + 32.0f) : ((densityMode_ == MixerDensityMode::Compact) ? (cx + 38.0f) : (cx + 50.0f));
        float mw = (densityMode_ == MixerDensityMode::Micro) ? 12.0f : ((densityMode_ == MixerDensityMode::Compact) ? 18.0f : 38.0f);
        renderLedMeter(r, theme, mx, wellTopY, mw, wellH,
                       ch.mute ? 0.0f : ch.peakL,
                       ch.mute ? 0.0f : ch.peakR,
                       ch.peakHoldL, ch.peakHoldR);
    }

    // 3. Compact Hardware Buttons Column (Far Right: M, S, *)
    if (showButtons_) {
        Color muteActiveBg = theme.muteActive;
        Color muteActiveFg = Color{1.0f, 1.0f, 1.0f, 1.0f};
        Color muteInactiveBg = theme.controlBackground;
        Color muteInactiveBorder = theme.muteActive.withAlpha(0.65f);
        Color muteInactiveFg = theme.muteActive;

        Color soloActiveBg = theme.soloActive;
        Color soloActiveFg = Color{0.10f, 0.10f, 0.10f, 1.0f};
        Color soloInactiveBg = theme.controlBackground;
        Color soloInactiveBorder = theme.soloActive.withAlpha(0.65f);
        Color soloInactiveFg = theme.soloActive;

        Color freezeActiveBg = theme.secondaryAccent;
        Color freezeActiveFg = Color{0.05f, 0.10f, 0.15f, 1.0f};
        Color freezeInactiveBg = theme.controlBackground;
        Color freezeInactiveBorder = theme.secondaryAccent.withAlpha(0.65f);
        Color freezeInactiveFg = theme.secondaryAccent;

        if (densityMode_ == MixerDensityMode::Micro) {
            float mbx = cx + 4.0f;
            float mby = wellTopY - 16.0f;
            float mbw = 18.0f;
            float mbh = 14.0f;
            drawButton(r, Rect2D{mbx, mby, mbw, mbh}, "M", ch.mute ? muteActiveBg : muteInactiveBg,
                       muteInactiveBorder, ch.mute ? muteActiveFg : muteInactiveFg, 7.5f, 2.0f, ch.mute ? 0.0f : 1.0f);
            drawButton(r, Rect2D{mbx + 20.0f, mby, mbw, mbh}, "S", ch.solo ? soloActiveBg : soloInactiveBg,
                       soloInactiveBorder, ch.solo ? soloActiveFg : soloInactiveFg, 7.5f, 2.0f, ch.solo ? 0.0f : 1.0f);
        } else if (densityMode_ == MixerDensityMode::Compact) {
            float bx = cx + 58.0f;
            float by = wellTopY + 2.0f;
            float bw = 16.0f;
            float bh = 20.0f;
            if (ch.mute) {
                drawRoundedRect(r, bx, by, bw, bh, 2.5f, muteActiveBg);
                drawRoundedRectOutline(r, bx, by, bw, bh, 2.5f, muteActiveBg.lighten(0.3f), 1.2f);
                drawCenteredText(r, "M", bx, by + 1.0f, bw, bh, 8.5f, muteActiveFg);
            } else {
                r.drawRectDropShadow(bx, by, bw, bh, 2.0f, 2.0f, theme.globalLight, isLight ? 0.20f : 0.35f);
                drawRoundedRect(r, bx, by, bw, bh, 2.5f, muteInactiveBg);
                drawRoundedRectOutline(r, bx, by, bw, bh, 2.5f, muteInactiveBorder, 1.0f);
                drawLine(r, bx + 1.0f, by + 0.8f, bx + bw - 1.0f, by + 0.8f, 1.0f, 1.0f, 1.0f, isLight ? 0.45f : 0.18f, 1.0f);
                drawCenteredText(r, "M", bx, by + 1.0f, bw, bh, 8.5f, muteInactiveFg);
            }

            float sy = by + bh + 4.0f;
            if (ch.solo) {
                drawRoundedRect(r, bx, sy, bw, bh, 2.5f, soloActiveBg);
                drawRoundedRectOutline(r, bx, sy, bw, bh, 2.5f, soloActiveBg.lighten(0.3f), 1.2f);
                drawCenteredText(r, "S", bx, sy + 1.0f, bw, bh, 8.5f, soloActiveFg);
            } else {
                r.drawRectDropShadow(bx, sy, bw, bh, 2.0f, 2.0f, theme.globalLight, isLight ? 0.20f : 0.35f);
                drawRoundedRect(r, bx, sy, bw, bh, 2.5f, soloInactiveBg);
                drawRoundedRectOutline(r, bx, sy, bw, bh, 2.5f, soloInactiveBorder, 1.0f);
                drawLine(r, bx + 1.0f, sy + 0.8f, bx + bw - 1.0f, sy + 0.8f, 1.0f, 1.0f, 1.0f, isLight ? 0.45f : 0.18f, 1.0f);
                drawCenteredText(r, "S", bx, sy + 1.0f, bw, bh, 8.5f, soloInactiveFg);
            }
        } else {
            float bx = cx + 96.0f;
            float by = wellTopY + 2.0f;
            float bw = 26.0f;
            float bh = 24.0f;
            if (ch.mute) {
                // Tactile depressed illuminated yellow/gold button
                drawRoundedRect(r, bx, by, bw, bh, 3.0f, muteActiveBg);
                drawRoundedRectOutline(r, bx, by, bw, bh, 3.0f, muteActiveBg.lighten(0.3f), 1.2f);
                drawLine(r, bx + 1.0f, by + 1.0f, bx + bw - 1.0f, by + 1.0f, 0.0f, 0.0f, 0.0f, 0.40f, 1.0f);
                drawCenteredText(r, "M", bx, by + 2.0f, bw, bh, 10.5f, muteActiveFg);
            } else {
                r.drawRectDropShadow(bx, by, bw, bh, 3.0f, 2.5f, theme.globalLight, isLight ? 0.20f : 0.35f);
                drawRoundedRect(r, bx, by, bw, bh, 3.0f, muteInactiveBg);
                drawRoundedRectOutline(r, bx, by, bw, bh, 3.0f, muteInactiveBorder, 1.2f);
                drawLine(r, bx + 1.5f, by + 1.0f, bx + bw - 1.5f, by + 1.0f, 1.0f, 1.0f, 1.0f, isLight ? 0.50f : 0.20f, 1.0f);
                drawCenteredText(r, "M", bx, by + 1.0f, bw, bh, 10.5f, muteInactiveFg);
            }

            float sy = by + bh + 4.0f;
            if (ch.solo) {
                // Tactile illuminated emerald green button
                drawRoundedRect(r, bx, sy, bw, bh, 3.0f, soloActiveBg);
                drawRoundedRectOutline(r, bx, sy, bw, bh, 3.0f, soloActiveBg.lighten(0.3f), 1.2f);
                drawLine(r, bx + 1.0f, sy + 1.0f, bx + bw - 1.0f, sy + 1.0f, 0.0f, 0.0f, 0.0f, 0.40f, 1.0f);
                drawCenteredText(r, "S", bx, sy + 2.0f, bw, bh, 10.5f, soloActiveFg);
            } else {
                r.drawRectDropShadow(bx, sy, bw, bh, 3.0f, 2.5f, theme.globalLight, isLight ? 0.20f : 0.35f);
                drawRoundedRect(r, bx, sy, bw, bh, 3.0f, soloInactiveBg);
                drawRoundedRectOutline(r, bx, sy, bw, bh, 3.0f, soloInactiveBorder, 1.2f);
                drawLine(r, bx + 1.5f, sy + 1.0f, bx + bw - 1.5f, sy + 1.0f, 1.0f, 1.0f, 1.0f, isLight ? 0.50f : 0.20f, 1.0f);
                drawCenteredText(r, "S", bx, sy + 1.0f, bw, bh, 10.5f, soloInactiveFg);
            }

            float fzy = sy + bh + 4.0f;
            if (ch.freeze) {
                drawRoundedRect(r, bx, fzy, bw, bh, 3.0f, freezeActiveBg);
                drawRoundedRectOutline(r, bx, fzy, bw, bh, 3.0f, freezeActiveBg.lighten(0.3f), 1.2f);
                drawLine(r, bx + 1.0f, fzy + 1.0f, bx + bw - 1.0f, fzy + 1.0f, 0.0f, 0.0f, 0.0f, 0.40f, 1.0f);
                drawCenteredText(r, "*", bx, fzy + 2.0f, bw, bh, 11.5f, freezeActiveFg);
            } else {
                r.drawRectDropShadow(bx, fzy, bw, bh, 3.0f, 2.5f, theme.globalLight, isLight ? 0.20f : 0.35f);
                drawRoundedRect(r, bx, fzy, bw, bh, 3.0f, freezeInactiveBg);
                drawRoundedRectOutline(r, bx, fzy, bw, bh, 3.0f, freezeInactiveBorder, 1.2f);
                drawLine(r, bx + 1.5f, fzy + 1.0f, bx + bw - 1.5f, fzy + 1.0f, 1.0f, 1.0f, 1.0f, isLight ? 0.50f : 0.20f, 1.0f);
                drawCenteredText(r, "*", bx, fzy + 1.0f, bw, bh, 11.5f, freezeInactiveFg);
            }
        }
    }
}

void MixerView::renderLedMeter(BatchRenderer2D& r, const ThemeTokens& theme, float mx, float my, float mw, float mh,
                              float levelL, float levelR, float holdL, float holdR) {
    // Inset glass dark well using theme
    drawRoundedRect(r, mx, my, mw, mh, 4.0f, theme.controlWell);
    drawRoundedRectOutline(r, mx, my, mw, mh, 4.0f, theme.borderSubtle, 1.4f);

    // Centered printed dB scale markings (Eatsbeats parity)
    static const char* dbLabels[11] = {
        "-inf", "-6", "-12", "-18", "-24", "-30", "-36", "-42", "-48", "-54", "-60"
    };

    for (int i = 0; i < 11; ++i) {
        float frac = static_cast<float>(i) / 10.0f;
        float lblY = my + 6.0f + frac * (mh - 16.0f);
        const char* lbl = dbLabels[i];
        float lw = static_cast<float>(std::strlen(lbl)) * 4.8f;
        drawText(r, lbl, mx + (mw - lw) * 0.5f, lblY - 3.5f, 7.0f, theme.textMuted);
    }

    // Dual LED columns
    const float ledW = 4.5f;
    const float leftX = mx + 3.5f;
    const float rightX = mx + mw - 3.5f - ledW;

    const int totalSegs = 22;
    const float segGap = 1.0f;
    float segH = (mh - 6.0f - (totalSegs - 1) * segGap) / static_cast<float>(totalSegs);

    for (int s = 0; s < totalSegs; ++s) {
        float thresh = static_cast<float>(s + 1) / static_cast<float>(totalSegs);
        bool onL = (levelL >= thresh - (1.0f / totalSegs) * 0.5f);
        bool onR = (levelR >= thresh - (1.0f / totalSegs) * 0.5f);

        int yIndexFromTop = totalSegs - 1 - s;
        float sy = my + 3.0f + yIndexFromTop * (segH + segGap);

        float lr, lg, lb, ur, ug, ub;
        if (thresh > 0.88f) {
            lr = 1.0f; lg = 0.09f; lb = 0.27f;
            ur = 0.17f; ug = 0.03f; ub = 0.05f;
        } else if (thresh > 0.70f) {
            lr = 1.0f; lg = 0.75f; lb = 0.03f;
            ur = 0.16f; ug = 0.11f; ub = 0.03f;
        } else {
            lr = 0.0f; lg = 0.90f; lb = 0.46f;
            ur = 0.03f; ug = 0.13f; ub = 0.07f;
        }

        drawRect(r, leftX, sy, ledW, segH, onL ? lr : ur, onL ? lg : ug, onL ? lb : ub, 1.0f);
        drawRect(r, rightX, sy, ledW, segH, onR ? lr : ur, onR ? lg : ug, onR ? lb : ub, 1.0f);
    }

    // Top Peak Clip Indicators
    if (levelL >= 0.95f) {
        drawRect(r, leftX, my + 1.0f, ledW, 2.5f, 1.0f, 0.0f, 0.33f, 1.0f);
    }
    if (levelR >= 0.95f) {
        drawRect(r, rightX, my + 1.0f, ledW, 2.5f, 1.0f, 0.0f, 0.33f, 1.0f);
    }

    // Inset Glass Lighting: Top and left inner shadows from chassis rim
    drawLine(r, mx + 1.0f, my + 1.0f, mx + mw - 1.0f, my + 1.0f, 0.0f, 0.0f, 0.0f, 0.50f, 1.0f);
    drawLine(r, mx + 1.0f, my + 2.0f, mx + 1.0f, my + mh - 1.0f, 0.0f, 0.0f, 0.0f, 0.40f, 1.0f);

    // Realistic horizontal glass sheen reflection across upper portion (No diagonal line)
    float glossH = std::max(6.0f, (mh - 4.0f) * 0.28f);
    drawRoundedRect(r, mx + 2.0f, my + 2.0f, mw - 4.0f, glossH, 2.5f, Color(1.0f, 1.0f, 1.0f, 0.05f));
    // Crisp top-edge light catch on glass
    drawLine(r, mx + 3.0f, my + 2.5f, mx + mw - 3.0f, my + 2.5f, 1.0f, 1.0f, 1.0f, 0.16f, 1.0f);
    // Subtle bottom inner reflection bounce
    drawLine(r, mx + 2.0f, my + mh - 1.2f, mx + mw - 2.0f, my + mh - 1.2f, 1.0f, 1.0f, 1.0f, 0.07f, 1.0f);
}

bool MixerView::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    syncDrawerData();

    // 0. Top Collapsible Options Toolbar clicks
    // 0a. Disallow mixer interactions in Project Browser area
    if (ctx.browserOffset > 0.0f && ev.x >= bounds_.x + bounds_.w - ctx.browserOffset) {
        return false;
    }

    // 0b. Top Options Toolbar
    if (toolbarBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            float tx = toolbarBounds_.x;
            float ty = toolbarBounds_.y;

            // Density Mode Pills
            // COMFORT [tx + 64, ty + 4, 76, 22]
            if (ev.x >= tx + 64.0f && ev.x <= tx + 140.0f && ev.y >= ty + 4.0f && ev.y <= ty + 26.0f) {
                setDensityMode(MixerDensityMode::Comfortable);
                autoDensity_ = false;
                return true;
            }
            // COMPACT [tx + 144, ty + 4, 66, 22]
            if (ev.x >= tx + 144.0f && ev.x <= tx + 210.0f && ev.y >= ty + 4.0f && ev.y <= ty + 26.0f) {
                setDensityMode(MixerDensityMode::Compact);
                autoDensity_ = false;
                return true;
            }
            // MICRO [tx + 214, ty + 4, 54, 22]
            if (ev.x >= tx + 214.0f && ev.x <= tx + 268.0f && ev.y >= ty + 4.0f && ev.y <= ty + 26.0f) {
                setDensityMode(MixerDensityMode::Micro);
                autoDensity_ = false;
                return true;
            }

            // Section Toggles
            // METERS [tx + 276, ty + 4, 56, 22]
            if (ev.x >= tx + 276.0f && ev.x <= tx + 332.0f && ev.y >= ty + 4.0f && ev.y <= ty + 26.0f) {
                showMeters_ = !showMeters_;
                if (onSectionTogglesChanged) onSectionTogglesChanged(showRouting_, showPan_, showButtons_, showMeters_, showReadouts_);
                return true;
            }
            // ROUTING [tx + 336, ty + 4, 62, 22]
            if (ev.x >= tx + 336.0f && ev.x <= tx + 398.0f && ev.y >= ty + 4.0f && ev.y <= ty + 26.0f) {
                showRouting_ = !showRouting_;
                if (onSectionTogglesChanged) onSectionTogglesChanged(showRouting_, showPan_, showButtons_, showMeters_, showReadouts_);
                return true;
            }
            // PAN [tx + 402, ty + 4, 42, 22]
            if (ev.x >= tx + 402.0f && ev.x <= tx + 444.0f && ev.y >= ty + 4.0f && ev.y <= ty + 26.0f) {
                showPan_ = !showPan_;
                if (onSectionTogglesChanged) onSectionTogglesChanged(showRouting_, showPan_, showButtons_, showMeters_, showReadouts_);
                return true;
            }
            // READOUT [tx + 448, ty + 4, 68, 22]
            if (ev.x >= tx + 448.0f && ev.x <= tx + 516.0f && ev.y >= ty + 4.0f && ev.y <= ty + 26.0f) {
                showReadouts_ = !showReadouts_;
                if (onSectionTogglesChanged) onSectionTogglesChanged(showRouting_, showPan_, showButtons_, showMeters_, showReadouts_);
                return true;
            }
            // MASTER POS [tx + 522, ty + 4, 74, 22]
            if (ev.x >= tx + 522.0f && ev.x <= tx + 596.0f && ev.y >= ty + 4.0f && ev.y <= ty + 26.0f) {
                setMasterPosition(masterPosition_ == MixerMasterPosition::Left ? MixerMasterPosition::Right : MixerMasterPosition::Left);
                return true;
            }
        }
        return true;
    }

    // 1. Sliding Track Properties Drawer (Active Drag, Pull-tab, Drag-resize, Controls)
    if (propertiesDrawer_.isDragging() ||
        propertiesDrawer_.getPullTabBounds().contains(ev.x, ev.y) ||
        (propertiesDrawer_.isExpanded() && propertiesDrawer_.getDrawerBounds().contains(ev.x, ev.y))) {
        if (propertiesDrawer_.handlePointer(ev, drawerData_, ctx)) {
            if (onPropertiesDrawerStateChanged) {
                onPropertiesDrawerStateChanged(propertiesDrawer_.isExpanded(), propertiesDrawer_.getWidth());
            }
        }
        return true;
    }

    // 1b. Multi-Touch Fast Session Dispatch for Move
    if (ev.action == PointerAction::Move) {
        auto itFader = activeFaderSessions_.find(ev.id);
        if (itFader != activeFaderSessions_.end()) {
            if (itFader->second.faderIndex == -1) {
                // Master Fader
                auto geom = getMasterFaderGeometry();
                float dy = itFader->second.dragStartY - ev.y;
                float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                float deltaTravel = (dy / geom.wellH) * fineScale;
                float newTravel = std::clamp(itFader->second.initialTravel + deltaTravel, 0.0f, 1.0f);
                float newFader = presenter::audio_taper::travelToGain(newTravel);

                masterChannel_.fader = newFader;
                if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(newFader);
                if (onMasterVolumeChanged) onMasterVolumeChanged(newFader);

                int pct = static_cast<int>(std::round(newFader * 100.0f));
                tooltipText_ = "Master: " + presenter::audio_taper::formatDb(newFader) + " (" + std::to_string(pct) + "%)";
                tooltipX_ = ((masterBounds_.x == 0.0f) ? 12.0f : masterBounds_.x) + 60.0f;
                tooltipY_ = ev.y;
                showTooltip_ = true;
                return true;
            } else if (itFader->second.faderIndex >= 0 && itFader->second.faderIndex < static_cast<int>(channels_.size())) {
                size_t idx = static_cast<size_t>(itFader->second.faderIndex);
                const float startX = (masterPosition_ == MixerMasterPosition::Right) ?
                                     (bounds_.x + 12.0f) :
                                     ((densityMode_ == MixerDensityMode::Comfortable) ? (masterBounds_.x + 195.0f) :
                                     ((densityMode_ == MixerDensityMode::Compact) ? (masterBounds_.x + masterStripWidth_ + 20.0f) :
                                                                                    (masterBounds_.x + masterStripWidth_ + 14.0f)));
                float cx = startX + static_cast<float>(idx) * (channelStripWidth_ + channelGap_) - scrollX_;
                auto geom = getChannelFaderGeometry(idx, cx);
                float dy = itFader->second.dragStartY - ev.y;
                float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                float deltaTravel = (dy / geom.wellH) * fineScale;
                float newTravel = std::clamp(itFader->second.initialTravel + deltaTravel, 0.0f, 1.0f);
                float newFader = presenter::audio_taper::travelToGain(newTravel);

                channels_[idx].fader = newFader;
                if (ctx.audioEngine) ctx.audioEngine->setTrackVolume(static_cast<uint32_t>(idx), newFader);
                if (onVolumeChanged) onVolumeChanged(static_cast<uint32_t>(idx), newFader);

                int pct = static_cast<int>(std::round(newFader * 100.0f));
                tooltipText_ = channels_[idx].name + ": " + presenter::audio_taper::formatDb(newFader) + " (" + std::to_string(pct) + "%)";
                tooltipX_ = cx + 45.0f;
                tooltipY_ = ev.y;
                showTooltip_ = true;
                return true;
            }
        }

        auto itPan = activePanSessions_.find(ev.id);
        if (itPan != activePanSessions_.end()) {
            if (itPan->second.panIndex == -1) {
                // Master Pan
                float dy = itPan->second.dragStartY - ev.y;
                float dx = ev.x - itPan->second.dragStartX;
                float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                float delta = ((std::abs(dy) > std::abs(dx)) ? (dy / 100.0f) : (dx / 100.0f)) * fineScale;
                masterChannel_.pan = std::clamp(itPan->second.initialPanVal + delta, -1.0f, 1.0f);
                if (onMasterPanChanged) onMasterPanChanged(masterChannel_.pan);
                return true;
            } else if (itPan->second.panIndex >= 0 && itPan->second.panIndex < static_cast<int>(channels_.size())) {
                size_t idx = static_cast<size_t>(itPan->second.panIndex);
                float dy = itPan->second.dragStartY - ev.y;
                float dx = ev.x - itPan->second.dragStartX;
                float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                float delta = ((std::abs(dy) > std::abs(dx)) ? (dy / 100.0f) : (dx / 100.0f)) * fineScale;
                channels_[idx].pan = std::clamp(itPan->second.initialPanVal + delta, -1.0f, 1.0f);
                if (onPanChanged) onPanChanged(static_cast<uint32_t>(idx), channels_[idx].pan);
                return true;
            }
        }
    }

    // 1c. Multi-Touch Fast Session Dispatch for Up / Cancel
    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        bool handled = false;
        auto itFader = activeFaderSessions_.find(ev.id);
        if (itFader != activeFaderSessions_.end()) {
            activeFaderSessions_.erase(itFader);
            if (activeFaderSessions_.empty()) {
                activeFaderIndex_ = -2;
                showTooltip_ = false;
            }
            handled = true;
        }
        auto itPan = activePanSessions_.find(ev.id);
        if (itPan != activePanSessions_.end()) {
            activePanSessions_.erase(itPan);
            if (activePanSessions_.empty()) {
                activePanIndex_ = -2;
            }
            handled = true;
        }
        if (handled) return true;
    }

    float rightBound = propertiesDrawer_.getPullTabBounds().x;
    if (rightBound <= 0.0f) {
        rightBound = (bounds_.w > 0.0f) ? (bounds_.x + bounds_.w - ctx.browserOffset - 24.0f) : 1256.0f;
    }
    if (masterPosition_ == MixerMasterPosition::Right) {
        rightBound = std::min(rightBound, masterBounds_.x - 14.0f);
    }

    // 2. Master Strip Pointer Events
    float mX = (masterBounds_.x == 0.0f) ? 12.0f : masterBounds_.x;
    float mW = masterStripWidth_;
    float mY = masterBounds_.y;
    float stripH = masterBounds_.h;

    bool isMasterDragging = (activeFaderIndex_ == -1 || activePanIndex_ == -1);
    for (const auto& [id, sess] : activeFaderSessions_) {
        if (sess.faderIndex == -1) isMasterDragging = true;
    }
    for (const auto& [id, sess] : activePanSessions_) {
        if (sess.panIndex == -1) isMasterDragging = true;
    }

    if (isMasterDragging || (masterBounds_.contains(ev.x, ev.y) && ev.x < rightBound)) {
        float lcdH = (densityMode_ == MixerDensityMode::Micro) ? 24.0f : ((densityMode_ == MixerDensityMode::Compact) ? 32.0f : 38.0f);
        float kx = mX + mW * 0.5f;
        float ky = mY + 8.0f + (showReadouts_ ? lcdH : 0.0f) + 16.0f;
        float panRadius = (densityMode_ == MixerDensityMode::Micro) ? 9.0f : ((densityMode_ == MixerDensityMode::Compact) ? 12.0f : 16.0f);

        if (ev.action == PointerAction::Down) {
            // Right-click manual value edit dialog
            if (ev.button == PointerButton::Right) {
                // Master Pan knob
                if (showPan_ && std::hypot(ev.x - kx, ev.y - ky) <= (panRadius + 4.0f)) {
                    if (ctx.onOpenValueEdit) {
                        ValueEditRequest req;
                        req.title = "MASTER PAN";
                        req.paramName = "Master Pan";
                        req.currentValue = masterChannel_.pan;
                        req.minValue = -1.0f;
                        req.maxValue = 1.0f;
                        req.defaultValue = 0.0f;
                        req.hasDefault = true;
                        req.allowPercentage = false;
                        req.accentColor = ctx.theme ? ctx.theme->primaryAccent : Color(1.0f, 0.70f, 0.10f);
                        req.onCommit = [this](float val) {
                            masterChannel_.pan = val;
                            if (onMasterPanChanged) onMasterPanChanged(val);
                        };
                        ctx.onOpenValueEdit(req);
                    }
                    return true;
                }
                // Master Fader slot / cap
                if (hitTestMasterFader(ev.x, ev.y)) {
                    if (ctx.onOpenValueEdit) {
                        ValueEditRequest req;
                        req.title = "MASTER VOLUME";
                        req.paramName = "Master Volume";
                        req.currentValue = masterChannel_.fader;
                        req.minValue = 0.0f;
                        req.maxValue = 1.5f;
                        req.defaultValue = 1.0f;
                        req.hasDefault = true;
                        req.allowPercentage = true;
                        req.accentColor = ctx.theme ? ctx.theme->primaryAccent : Color(1.0f, 0.70f, 0.10f);
                        req.onCommit = [this, ctx](float val) {
                            masterChannel_.fader = val;
                            if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(val);
                            if (onMasterVolumeChanged) onMasterVolumeChanged(val);
                        };
                        ctx.onOpenValueEdit(req);
                    }
                    return true;
                }
            }

            // Left-click / Touch Down:

            // 1. Master Pan knob (tested first so nothing intercepts it!)
            if (showPan_ && std::hypot(ev.x - kx, ev.y - ky) <= (panRadius + 4.0f)) {
                if (ev.mods.ctrl || ev.mods.meta) {
                    masterChannel_.pan = 0.0f;
                    if (onMasterPanChanged) onMasterPanChanged(0.0f);
                    return true;
                }
                activePanIndex_ = -1;
                dragStartX_ = ev.x;
                dragStartY_ = ev.y;
                initialPanVal_ = masterChannel_.pan;
                activePanSessions_[ev.id] = {-1, ev.x, ev.y, masterChannel_.pan};
                return true;
            }

            // 2. "C" button tap -> Center Pan (Comfortable mode)
            if (showPan_ && densityMode_ == MixerDensityMode::Comfortable &&
                ev.x >= kx - 11.0f && ev.x <= kx + 11.0f && ev.y >= ky + 16.0f && ev.y <= ky + 34.0f) {
                masterChannel_.pan = 0.0f;
                if (onMasterPanChanged) onMasterPanChanged(0.0f);
                return true;
            }

            // 3. Master Fader tap
            if (hitTestMasterFader(ev.x, ev.y)) {
                if (ev.mods.ctrl || ev.mods.meta) {
                    masterChannel_.fader = 1.0f;
                    if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(1.0f);
                    if (onMasterVolumeChanged) onMasterVolumeChanged(1.0f);
                    return true;
                }
                auto now = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFaderClickTime_).count();
                if (lastFaderClickIndex_ == -1 && elapsedMs < 350) {
                    masterChannel_.fader = 1.0f;
                    if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(1.0f);
                    if (onMasterVolumeChanged) onMasterVolumeChanged(1.0f);
                    lastFaderClickTime_ = {};
                    lastFaderClickIndex_ = -2;
                    activeFaderIndex_ = -2;
                    activeFaderSessions_.erase(ev.id);
                    showTooltip_ = false;
                    return true;
                }
                lastFaderClickTime_ = now;
                lastFaderClickIndex_ = -1;

                activeFaderIndex_ = -1;
                dragStartY_ = ev.y;
                initialFaderVal_ = masterChannel_.fader;
                initialFaderTravel_ = presenter::audio_taper::gainToTravel(masterChannel_.fader);
                activeFaderSessions_[ev.id] = {-1, ev.y, masterChannel_.fader, initialFaderTravel_};

                int pct = static_cast<int>(std::round(masterChannel_.fader * 100.0f));
                tooltipText_ = "Master: " + presenter::audio_taper::formatDb(masterChannel_.fader) + " (" + std::to_string(pct) + "%)";
                tooltipX_ = mX + 60.0f;
                tooltipY_ = ev.y;
                showTooltip_ = true;
                return true;
            }

            // 4. Left-click on LCD Volume % readout -> Open manual edit dialog
            if (showReadouts_ && ev.x >= mX + mW - 50.0f && ev.x <= mX + mW - 6.0f &&
                ev.y >= mY + 6.0f && ev.y <= mY + 6.0f + lcdH) {
                if (ctx.onOpenValueEdit) {
                    ValueEditRequest req;
                    req.title = "MASTER VOLUME";
                    req.paramName = "Master Volume";
                    req.currentValue = masterChannel_.fader;
                    req.minValue = 0.0f;
                    req.maxValue = 1.5f;
                    req.defaultValue = 1.0f;
                    req.hasDefault = true;
                    req.allowPercentage = true;
                    req.accentColor = ctx.theme ? ctx.theme->primaryAccent : Color(1.0f, 0.70f, 0.10f);
                    req.onCommit = [this, ctx](float val) {
                        masterChannel_.fader = val;
                        if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(val);
                        if (onMasterVolumeChanged) onMasterVolumeChanged(val);
                    };
                    ctx.onOpenValueEdit(req);
                }
                return true;
            }

            // 5. LCD Screen tap or Chassis tap -> Select Master & Expand Sidebar
            if (ev.x >= mX && ev.x <= mX + mW && ev.y >= mY && ev.y <= mY + stripH) {
                isMasterSelected_ = true;
                syncDrawerData();
                propertiesDrawer_.setExpanded(true);
                if (onPropertiesDrawerStateChanged) onPropertiesDrawerStateChanged(true, propertiesDrawer_.getWidth());
                return true;
            }
        } else if (ev.action == PointerAction::Move) {
            auto it = activeFaderSessions_.find(ev.id);
            if (it != activeFaderSessions_.end() && it->second.faderIndex == -1) {
                auto geom = getMasterFaderGeometry();
                float dy = it->second.dragStartY - ev.y;
                float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                float deltaTravel = (dy / geom.wellH) * fineScale;
                float newTravel = std::clamp(it->second.initialTravel + deltaTravel, 0.0f, 1.0f);
                float newFader = presenter::audio_taper::travelToGain(newTravel);

                masterChannel_.fader = newFader;
                if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(newFader);
                if (onMasterVolumeChanged) onMasterVolumeChanged(newFader);

                int pct = static_cast<int>(std::round(newFader * 100.0f));
                tooltipText_ = "Master: " + presenter::audio_taper::formatDb(newFader) + " (" + std::to_string(pct) + "%)";
                tooltipX_ = mX + 60.0f;
                tooltipY_ = ev.y;
                showTooltip_ = true;
                return true;
            }
            if (activeFaderIndex_ == -1) {
                auto geom = getMasterFaderGeometry();
                float dy = dragStartY_ - ev.y;
                float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                float deltaTravel = (dy / geom.wellH) * fineScale;
                float newTravel = std::clamp(initialFaderTravel_ + deltaTravel, 0.0f, 1.0f);
                float newFader = presenter::audio_taper::travelToGain(newTravel);

                masterChannel_.fader = newFader;
                if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(newFader);
                if (onMasterVolumeChanged) onMasterVolumeChanged(newFader);

                int pct = static_cast<int>(std::round(newFader * 100.0f));
                tooltipText_ = "Master: " + presenter::audio_taper::formatDb(newFader) + " (" + std::to_string(pct) + "%)";
                tooltipX_ = mX + 60.0f;
                tooltipY_ = ev.y;
                showTooltip_ = true;
                return true;
            }
            auto panIt = activePanSessions_.find(ev.id);
            if (panIt != activePanSessions_.end() && panIt->second.panIndex == -1) {
                float dy = panIt->second.dragStartY - ev.y;
                float dx = ev.x - panIt->second.dragStartX;
                float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                float delta = ((std::abs(dy) > std::abs(dx)) ? (dy / 100.0f) : (dx / 100.0f)) * fineScale;
                masterChannel_.pan = std::clamp(panIt->second.initialPanVal + delta, -1.0f, 1.0f);
                if (onMasterPanChanged) onMasterPanChanged(masterChannel_.pan);
                return true;
            }
            if (activePanIndex_ == -1) {
                float dy = dragStartY_ - ev.y;
                float dx = ev.x - dragStartX_;
                float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                float delta = ((std::abs(dy) > std::abs(dx)) ? (dy / 100.0f) : (dx / 100.0f)) * fineScale;
                masterChannel_.pan = std::clamp(initialPanVal_ + delta, -1.0f, 1.0f);
                if (onMasterPanChanged) onMasterPanChanged(masterChannel_.pan);
                return true;
            }
        }
    }

    // 3. Channel Strips Pointer Events
    const float startX = (masterPosition_ == MixerMasterPosition::Right) ?
                         (bounds_.x + 12.0f) :
                         ((densityMode_ == MixerDensityMode::Comfortable) ? (masterBounds_.x + 195.0f) :
                         ((densityMode_ == MixerDensityMode::Compact) ? (masterBounds_.x + masterStripWidth_ + 20.0f) :
                                                                        (masterBounds_.x + masterStripWidth_ + 14.0f)));
    const float stripW = channelStripWidth_;
    const float gap = channelGap_;

    for (size_t i = 0; i < channels_.size(); ++i) {
        float cx = startX + static_cast<float>(i) * (stripW + gap) - scrollX_;
        if (masterPosition_ != MixerMasterPosition::Right) {
            if (cx + stripW < 185.0f && densityMode_ == MixerDensityMode::Comfortable) continue;
            if (cx + stripW < masterBounds_.x + masterStripWidth_ + 10.0f) continue;
        } else {
            if (cx + stripW < bounds_.x) continue;
        }
        if (cx > rightBound) break;

        Rect2D stripRect(cx, masterBounds_.y, stripW, masterBounds_.h);

        float lcdH = (densityMode_ == MixerDensityMode::Micro) ? 24.0f : ((densityMode_ == MixerDensityMode::Compact) ? 32.0f : 38.0f);
        float kx = cx + stripW * 0.5f;
        float ky = masterBounds_.y + 8.0f + (showReadouts_ ? lcdH : 0.0f) + 16.0f;
        float panRadius = (densityMode_ == MixerDensityMode::Micro) ? 9.0f : ((densityMode_ == MixerDensityMode::Compact) ? 12.0f : 16.0f);

        auto geom = getChannelFaderGeometry(i, cx);
        float wellTopY = geom.wellTopY;

        bool isDraggingThis = (activeFaderIndex_ == static_cast<int>(i) || activePanIndex_ == static_cast<int>(i));
        for (const auto& [id, sess] : activeFaderSessions_) {
            if (sess.faderIndex == static_cast<int>(i)) isDraggingThis = true;
        }
        for (const auto& [id, sess] : activePanSessions_) {
            if (sess.panIndex == static_cast<int>(i)) isDraggingThis = true;
        }

        if (isDraggingThis || (stripRect.contains(ev.x, ev.y) && ev.x < rightBound)) {
            if (ev.action == PointerAction::Down) {
                selectedChannel_ = static_cast<int>(i);
                isMasterSelected_ = false;
                syncDrawerData();
                if (onTrackSelected) onTrackSelected(static_cast<uint32_t>(i));

                // Right-click manual value edit dialog
                if (ev.button == PointerButton::Right) {
                    // Channel Pan knob
                    if (showPan_ && std::hypot(ev.x - kx, ev.y - ky) <= (panRadius + 4.0f)) {
                        if (ctx.onOpenValueEdit) {
                            ValueEditRequest req;
                            req.title = channels_[i].name + " PAN";
                            req.paramName = channels_[i].name + " Pan";
                            req.currentValue = channels_[i].pan;
                            req.minValue = -1.0f;
                            req.maxValue = 1.0f;
                            req.defaultValue = 0.0f;
                            req.hasDefault = true;
                            req.allowPercentage = false;
                            req.accentColor = Color(channels_[i].r, channels_[i].g, channels_[i].b, 1.0f);
                            req.onCommit = [this, i](float val) {
                                channels_[i].pan = val;
                                if (onPanChanged) onPanChanged(static_cast<uint32_t>(i), val);
                            };
                            ctx.onOpenValueEdit(req);
                        }
                        return true;
                    }
                    // Channel Fader slot / cap
                    if (hitTestChannelFader(i, cx, ev.x, ev.y)) {
                        if (ctx.onOpenValueEdit) {
                            ValueEditRequest req;
                            req.title = channels_[i].name + " VOLUME";
                            req.paramName = channels_[i].name + " Volume";
                            req.currentValue = channels_[i].fader;
                            req.minValue = 0.0f;
                            req.maxValue = 1.5f;
                            req.defaultValue = 1.0f;
                            req.hasDefault = true;
                            req.allowPercentage = true;
                            req.accentColor = Color(channels_[i].r, channels_[i].g, channels_[i].b, 1.0f);
                            req.onCommit = [this, i](float val) {
                                channels_[i].fader = val;
                                if (onVolumeChanged) onVolumeChanged(static_cast<uint32_t>(i), val);
                            };
                            ctx.onOpenValueEdit(req);
                        }
                        return true;
                    }
                }

                // Left-click / Touch Down:

                // 1. Channel Pan knob tap (tested first so buttons/card don't steal clicks!)
                if (showPan_ && std::hypot(ev.x - kx, ev.y - ky) <= (panRadius + 4.0f)) {
                    if (ev.mods.ctrl || ev.mods.meta) {
                        channels_[i].pan = 0.0f;
                        if (onPanChanged) onPanChanged(static_cast<uint32_t>(i), 0.0f);
                        return true;
                    }
                    activePanIndex_ = static_cast<int>(i);
                    dragStartX_ = ev.x;
                    dragStartY_ = ev.y;
                    initialPanVal_ = channels_[i].pan;
                    activePanSessions_[ev.id] = {static_cast<int>(i), ev.x, ev.y, channels_[i].pan};
                    return true;
                }

                // 2. "C" button tap -> Center Pan (Comfortable mode)
                if (showPan_ && densityMode_ == MixerDensityMode::Comfortable &&
                    ev.x >= kx - 11.0f && ev.x <= kx + 11.0f && ev.y >= ky + 16.0f && ev.y <= ky + 34.0f) {
                    channels_[i].pan = 0.0f;
                    if (onPanChanged) onPanChanged(static_cast<uint32_t>(i), 0.0f);
                    return true;
                }

                // 3. Compact Hardware Buttons (Mute, Solo, Freeze) - EXACT geometric coordinates
                if (showButtons_) {
                    bool hitMute = false;
                    bool hitSolo = false;
                    bool hitFreeze = false;

                    if (densityMode_ == MixerDensityMode::Micro) {
                        float mbx = cx + 4.0f;
                        float mby = wellTopY - 16.0f;
                        float mbw = 18.0f;
                        float mbh = 14.0f;
                        hitMute = (ev.x >= mbx && ev.x <= mbx + mbw && ev.y >= mby && ev.y <= mby + mbh);
                        hitSolo = (ev.x >= mbx + 20.0f && ev.x <= mbx + 20.0f + mbw && ev.y >= mby && ev.y <= mby + mbh);
                    } else if (densityMode_ == MixerDensityMode::Compact) {
                        float bx = cx + 58.0f;
                        float by = wellTopY + 2.0f;
                        float bw = 16.0f;
                        float bh = 20.0f;
                        hitMute = (ev.x >= bx && ev.x <= bx + bw && ev.y >= by && ev.y <= by + bh);
                        float sy = by + bh + 4.0f;
                        hitSolo = (ev.x >= bx && ev.x <= bx + bw && ev.y >= sy && ev.y <= sy + bh);
                    } else { // Comfortable mode
                        float bx = cx + 96.0f;
                        float by = wellTopY + 2.0f;
                        float bw = 26.0f;
                        float bh = 24.0f;
                        hitMute = (ev.x >= bx && ev.x <= bx + bw && ev.y >= by && ev.y <= by + bh);
                        float sy = by + bh + 4.0f;
                        hitSolo = (ev.x >= bx && ev.x <= bx + bw && ev.y >= sy && ev.y <= sy + bh);
                        float fzy = sy + bh + 4.0f;
                        hitFreeze = (ev.x >= bx && ev.x <= bx + bw && ev.y >= fzy && ev.y <= fzy + bh);
                    }

                    if (hitMute) {
                        channels_[i].mute = !channels_[i].mute;
                        if (onMuteToggled) onMuteToggled(static_cast<uint32_t>(i), channels_[i].mute);
                        if (ctx.audioEngine) ctx.audioEngine->setTrackMute(static_cast<uint32_t>(i), channels_[i].mute);
                        return true;
                    }
                    if (hitSolo) {
                        channels_[i].solo = !channels_[i].solo;
                        if (onSoloToggled) onSoloToggled(static_cast<uint32_t>(i), channels_[i].solo);
                        if (ctx.audioEngine) ctx.audioEngine->setTrackSolo(static_cast<uint32_t>(i), channels_[i].solo);
                        return true;
                    }
                    if (hitFreeze) {
                        channels_[i].freeze = !channels_[i].freeze;
                        if (onFreezeToggled) onFreezeToggled(static_cast<uint32_t>(i), channels_[i].freeze);
                        return true;
                    }
                }

                // 4. Fader slot tap
                if (hitTestChannelFader(i, cx, ev.x, ev.y)) {
                    if (ev.mods.ctrl || ev.mods.meta) {
                        channels_[i].fader = 1.0f;
                        if (ctx.audioEngine) ctx.audioEngine->setTrackVolume(static_cast<uint32_t>(i), 1.0f);
                        if (onVolumeChanged) onVolumeChanged(static_cast<uint32_t>(i), 1.0f);
                        return true;
                    }
                    auto now = std::chrono::steady_clock::now();
                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFaderClickTime_).count();
                    if (lastFaderClickIndex_ == static_cast<int>(i) && elapsedMs < 350) {
                        channels_[i].fader = 1.0f;
                        if (ctx.audioEngine) ctx.audioEngine->setTrackVolume(static_cast<uint32_t>(i), 1.0f);
                        if (onVolumeChanged) onVolumeChanged(static_cast<uint32_t>(i), 1.0f);
                        lastFaderClickTime_ = {};
                        lastFaderClickIndex_ = -2;
                        activeFaderIndex_ = -2;
                        activeFaderSessions_.erase(ev.id);
                        showTooltip_ = false;
                        return true;
                    }
                    lastFaderClickTime_ = now;
                    lastFaderClickIndex_ = static_cast<int>(i);

                    activeFaderIndex_ = static_cast<int>(i);
                    dragStartY_ = ev.y;
                    initialFaderVal_ = channels_[i].fader;
                    initialFaderTravel_ = presenter::audio_taper::gainToTravel(channels_[i].fader);
                    activeFaderSessions_[ev.id] = {static_cast<int>(i), ev.y, channels_[i].fader, initialFaderTravel_};

                    int pct = static_cast<int>(std::round(channels_[i].fader * 100.0f));
                    tooltipText_ = channels_[i].name + ": " + presenter::audio_taper::formatDb(channels_[i].fader) + " (" + std::to_string(pct) + "%)";
                    tooltipX_ = cx + 45.0f;
                    tooltipY_ = ev.y;
                    showTooltip_ = true;
                    return true;
                }

                // 5. Left-click on Channel LCD Volume % readout
                if (showReadouts_ && ev.x >= cx + stripW - 48.0f && ev.x <= cx + stripW - 4.0f &&
                    ev.y >= masterBounds_.y + 6.0f && ev.y <= masterBounds_.y + 6.0f + lcdH) {
                    if (ctx.onOpenValueEdit) {
                        ValueEditRequest req;
                        req.title = channels_[i].name + " VOLUME";
                        req.paramName = channels_[i].name + " Volume";
                        req.currentValue = channels_[i].fader;
                        req.minValue = 0.0f;
                        req.maxValue = 1.5f;
                        req.defaultValue = 1.0f;
                        req.hasDefault = true;
                        req.allowPercentage = true;
                        req.accentColor = Color(channels_[i].r, channels_[i].g, channels_[i].b, 1.0f);
                        req.onCommit = [this, i](float val) {
                            channels_[i].fader = val;
                            if (onVolumeChanged) onVolumeChanged(static_cast<uint32_t>(i), val);
                        };
                        ctx.onOpenValueEdit(req);
                    }
                    return true;
                }

                // 6. General card / Backlit LCD tap -> Select track & open sidebar
                if (stripRect.contains(ev.x, ev.y)) {
                    propertiesDrawer_.setExpanded(true);
                    if (onPropertiesDrawerStateChanged) onPropertiesDrawerStateChanged(true, propertiesDrawer_.getWidth());
                    return true;
                }
            } else if (ev.action == PointerAction::Move) {
                auto sessionIt = activeFaderSessions_.find(ev.id);
                if (sessionIt != activeFaderSessions_.end() && sessionIt->second.faderIndex == static_cast<int>(i)) {
                    float dy = sessionIt->second.dragStartY - ev.y;
                    float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                    float deltaTravel = (dy / geom.wellH) * fineScale;
                    float newTravel = std::clamp(sessionIt->second.initialTravel + deltaTravel, 0.0f, 1.0f);
                    float newFader = presenter::audio_taper::travelToGain(newTravel);

                    channels_[i].fader = newFader;
                    if (ctx.audioEngine) ctx.audioEngine->setTrackVolume(static_cast<uint32_t>(i), newFader);
                    if (onVolumeChanged) onVolumeChanged(static_cast<uint32_t>(i), newFader);

                    int pct = static_cast<int>(std::round(newFader * 100.0f));
                    tooltipText_ = channels_[i].name + ": " + presenter::audio_taper::formatDb(newFader) + " (" + std::to_string(pct) + "%)";
                    tooltipX_ = cx + 45.0f;
                    tooltipY_ = ev.y;
                    showTooltip_ = true;
                    return true;
                }
                if (activeFaderIndex_ == static_cast<int>(i)) {
                    float dy = dragStartY_ - ev.y;
                    float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                    float deltaTravel = (dy / geom.wellH) * fineScale;
                    float newTravel = std::clamp(initialFaderTravel_ + deltaTravel, 0.0f, 1.0f);
                    float newFader = presenter::audio_taper::travelToGain(newTravel);

                    channels_[i].fader = newFader;
                    if (ctx.audioEngine) ctx.audioEngine->setTrackVolume(static_cast<uint32_t>(i), newFader);
                    if (onVolumeChanged) onVolumeChanged(static_cast<uint32_t>(i), newFader);

                    int pct = static_cast<int>(std::round(newFader * 100.0f));
                    tooltipText_ = channels_[i].name + ": " + presenter::audio_taper::formatDb(newFader) + " (" + std::to_string(pct) + "%)";
                    tooltipX_ = cx + 45.0f;
                    tooltipY_ = ev.y;
                    showTooltip_ = true;
                    return true;
                }
                auto panSessionIt = activePanSessions_.find(ev.id);
                if (panSessionIt != activePanSessions_.end() && panSessionIt->second.panIndex == static_cast<int>(i)) {
                    float dy = panSessionIt->second.dragStartY - ev.y;
                    float dx = ev.x - panSessionIt->second.dragStartX;
                    float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                    float delta = ((std::abs(dy) > std::abs(dx)) ? (dy / 100.0f) : (dx / 100.0f)) * fineScale;
                    channels_[i].pan = std::clamp(panSessionIt->second.initialPanVal + delta, -1.0f, 1.0f);
                    if (onPanChanged) onPanChanged(static_cast<uint32_t>(i), channels_[i].pan);
                    return true;
                }
                if (activePanIndex_ == static_cast<int>(i)) {
                    float dy = dragStartY_ - ev.y;
                    float dx = ev.x - dragStartX_;
                    float fineScale = ev.mods.shift ? 0.10f : 1.0f;
                    float delta = ((std::abs(dy) > std::abs(dx)) ? (dy / 100.0f) : (dx / 100.0f)) * fineScale;
                    channels_[i].pan = std::clamp(initialPanVal_ + delta, -1.0f, 1.0f);
                    if (onPanChanged) onPanChanged(static_cast<uint32_t>(i), channels_[i].pan);
                    return true;
                }
            }
        }
    }

    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        bool handled = false;
        auto it = activeFaderSessions_.find(ev.id);
        if (it != activeFaderSessions_.end()) {
            activeFaderSessions_.erase(it);
            if (activeFaderSessions_.empty()) {
                activeFaderIndex_ = -2;
                showTooltip_ = false;
            }
            handled = true;
        }
        auto itPan = activePanSessions_.find(ev.id);
        if (itPan != activePanSessions_.end()) {
            activePanSessions_.erase(itPan);
            if (activePanSessions_.empty()) {
                activePanIndex_ = -2;
            }
            handled = true;
        }
        if (handled) return true;
        if (activeFaderIndex_ != -2 || activePanIndex_ != -2 || showTooltip_) {
            activeFaderIndex_ = -2;
            activePanIndex_ = -2;
            showTooltip_ = false;
            activeFaderSessions_.clear();
            activePanSessions_.clear();
            return true;
        }
    }

    // Horizontal Scroll
    if (ev.action == PointerAction::Scroll && channelsScrollBounds_.contains(ev.x, ev.y)) {
        float delta = (std::abs(ev.scrollX) > 0.001f ? ev.scrollX : -ev.scrollY);
        scrollX_ -= delta * 30.0f;
        scrollX_ = std::max(0.0f, scrollX_);
        return true;
    }

    return false;
}

bool MixerView::handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
    if (propertiesDrawer_.isPluginDialogOpen()) {
        return propertiesDrawer_.handleKey(key, scancode, action, mods, ctx);
    }
    return false;
}

bool MixerView::handleChar(char32_t codepoint, [[maybe_unused]] const ViewContext& ctx) {
    if (propertiesDrawer_.isPluginDialogOpen()) {
        return propertiesDrawer_.handleChar(codepoint);
    }
    return false;
}

} // namespace eatsbits::ui


