#ifndef EATS_TRACK_PROPERTIES_PRESENTER_HPP
#define EATS_TRACK_PROPERTIES_PRESENTER_HPP

#include "presenter_base.hpp"
#include "parameter_presenter.hpp"
#include "value_edit_presenter.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace eatsbits::presenter {

enum class TrackParamType {
    Volume,
    Pan,
    EqHpf,
    EqLowGain,
    EqLowMidGain,
    EqHighMidGain,
    EqHighGain,
    EqLpf,
    CompThreshold,
    CompRatio,
    CompAttack,
    CompRelease,
    CompMakeup,
    Send1,
    Send2
};

/**
 * @brief Presenter encapsulating track channel strip parameters, EQ, compressor, sends,
 * and parameter conversion math across both Desktop GUI and Terminal TUI.
 */
class TrackPropertiesPresenter : public PresenterBase {
public:
    TrackPropertiesPresenter();

    static const ParameterDescriptor& getStandardDescriptor(TrackParamType type);

    static ValueEditConfig makeEditConfig(TrackParamType type,
                                          float currentValue,
                                          const std::string& trackTitle,
                                          std::function<void(float)> onCommit);

    static ValueEditConfig makeGenericKnobEditConfig(const std::string& title,
                                                     const std::string& paramName,
                                                     float currentValue,
                                                     std::function<void(float)> onCommit,
                                                     float minVal = 0.0f,
                                                     float maxVal = 1.0f,
                                                     float defaultVal = 0.5f,
                                                     const std::string& unit = "",
                                                     bool allowPercentage = true);

    ParameterPresenter* getParameter(TrackParamType type);
    ParameterPresenter* findParameter(const std::string& name);

    void setTrackName(std::string name);
    [[nodiscard]] const std::string& getTrackName() const noexcept { return trackName_; }

    void syncVolume(float val) { setParamValue(TrackParamType::Volume, val); }
    void syncPan(float val) { setParamValue(TrackParamType::Pan, val); }

private:
    void setParamValue(TrackParamType type, float val);

    std::string trackName_{"Track 1"};
    std::vector<ParameterPresenter> standardParams_;
    std::unordered_map<TrackParamType, size_t> typeMap_;
};

} // namespace eatsbits::presenter

#endif // EATS_TRACK_PROPERTIES_PRESENTER_HPP
