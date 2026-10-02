#include "eatsbits/presenter/track_properties_presenter.hpp"

namespace eatsbits::presenter {

static const ParameterDescriptor kStandardTrackDescriptors[] = {
    // Volume
    {"volume", "Volume", "", 0.0f, 1.5f, 0.8f, 0.05f, false, true},
    // Pan
    {"pan", "Pan", "", -1.0f, 1.0f, 0.0f, 0.05f, false, false},
    // EqHpf
    {"eq_hpf", "HPF Cut", "Hz", 20.0f, 500.0f, 20.0f, 10.0f, false, false},
    // EqLowGain
    {"eq_low_gain", "Low Gain", "dB", -18.0f, 18.0f, 0.0f, 0.5f, false, false},
    // EqLowMidGain
    {"eq_low_mid_gain", "Low-Mid Gain", "dB", -18.0f, 18.0f, 0.0f, 0.5f, false, false},
    // EqHighMidGain
    {"eq_high_mid_gain", "High-Mid Gain", "dB", -18.0f, 18.0f, 0.0f, 0.5f, false, false},
    // EqHighGain
    {"eq_high_gain", "High Gain", "dB", -18.0f, 18.0f, 0.0f, 0.5f, false, false},
    // EqLpf
    {"eq_lpf", "LPF Cut", "Hz", 1000.0f, 20000.0f, 20000.0f, 100.0f, false, false},
    // CompThreshold
    {"comp_threshold", "Comp Thresh", "dB", -60.0f, 0.0f, -12.0f, 1.0f, false, false},
    // CompRatio
    {"comp_ratio", "Comp Ratio", ":1", 1.0f, 20.0f, 4.0f, 0.5f, false, false},
    // CompAttack
    {"comp_attack", "Comp Attack", "ms", 0.1f, 100.0f, 10.0f, 1.0f, false, false},
    // CompRelease
    {"comp_release", "Comp Release", "ms", 10.0f, 1000.0f, 100.0f, 10.0f, false, false},
    // CompMakeup
    {"comp_makeup", "Comp Makeup", "dB", 0.0f, 24.0f, 0.0f, 0.5f, false, false},
    // Send1
    {"send_1", "Send 1", "", 0.0f, 1.0f, 0.0f, 0.05f, false, true},
    // Send2
    {"send_2", "Send 2", "", 0.0f, 1.0f, 0.0f, 0.05f, false, true}
};

const ParameterDescriptor& TrackPropertiesPresenter::getStandardDescriptor(TrackParamType type) {
    size_t idx = static_cast<size_t>(type);
    if (idx < sizeof(kStandardTrackDescriptors) / sizeof(kStandardTrackDescriptors[0])) {
        return kStandardTrackDescriptors[idx];
    }
    return kStandardTrackDescriptors[0];
}

ValueEditConfig TrackPropertiesPresenter::makeEditConfig(TrackParamType type,
                                                         float currentValue,
                                                         const std::string& trackTitle,
                                                         std::function<void(float)> onCommit) {
    const auto& desc = getStandardDescriptor(type);
    ValueEditConfig cfg;
    cfg.title = trackTitle.empty() ? desc.label : (trackTitle + " • " + desc.label);
    cfg.paramName = desc.label;
    cfg.isTextMode = false;
    cfg.currentValue = currentValue;
    cfg.minValue = desc.minValue;
    cfg.maxValue = desc.maxValue;
    cfg.defaultValue = desc.defaultValue;
    cfg.hasDefault = true;
    cfg.isInteger = desc.isInteger;
    cfg.allowPercentage = desc.allowPercentage;
    cfg.unit = desc.unit;
    cfg.onCommit = std::move(onCommit);
    return cfg;
}

ValueEditConfig TrackPropertiesPresenter::makeGenericKnobEditConfig(const std::string& title,
                                                                    const std::string& paramName,
                                                                    float currentValue,
                                                                    std::function<void(float)> onCommit,
                                                                    float minVal,
                                                                    float maxVal,
                                                                    float defaultVal,
                                                                    const std::string& unit,
                                                                    bool allowPercentage) {
    ValueEditConfig cfg;
    cfg.title = title;
    cfg.paramName = paramName;
    cfg.isTextMode = false;
    cfg.currentValue = currentValue;
    cfg.minValue = minVal;
    cfg.maxValue = maxVal;
    cfg.defaultValue = defaultVal;
    cfg.hasDefault = true;
    cfg.isInteger = false;
    cfg.allowPercentage = allowPercentage;
    cfg.unit = unit;
    cfg.onCommit = std::move(onCommit);
    return cfg;
}

TrackPropertiesPresenter::TrackPropertiesPresenter() {
    size_t total = sizeof(kStandardTrackDescriptors) / sizeof(kStandardTrackDescriptors[0]);
    standardParams_.reserve(total);
    for (size_t i = 0; i < total; ++i) {
        TrackParamType t = static_cast<TrackParamType>(i);
        typeMap_[t] = i;
        standardParams_.emplace_back(kStandardTrackDescriptors[i]);
    }
}

ParameterPresenter* TrackPropertiesPresenter::getParameter(TrackParamType type) {
    auto it = typeMap_.find(type);
    if (it != typeMap_.end() && it->second < standardParams_.size()) {
        return &standardParams_[it->second];
    }
    return nullptr;
}

ParameterPresenter* TrackPropertiesPresenter::findParameter(const std::string& name) {
    for (auto& p : standardParams_) {
        if (p.getName() == name || p.getLabel() == name) {
            return &p;
        }
    }
    return nullptr;
}

void TrackPropertiesPresenter::setTrackName(std::string name) {
    if (trackName_ != name) {
        trackName_ = std::move(name);
        markDirty();
    }
}

void TrackPropertiesPresenter::setParamValue(TrackParamType type, float val) {
    auto* p = getParameter(type);
    if (p) {
        p->setValue(val);
        if (p->isDirty()) {
            markDirty();
        }
    }
}

} // namespace eatsbits::presenter
