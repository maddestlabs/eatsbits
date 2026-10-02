#ifndef EATS_PRESENTER_DRAG_TYPES_HPP
#define EATS_PRESENTER_DRAG_TYPES_HPP

namespace eatsbits::ui {

/**
 * @brief Identifiers for mouse drag and gesture interaction modes across Eatsbits.
 */
enum class DragMode {
    None,
    Knob,
    HardwareKnob,
    PatchCable,
    BpmScrubber,
    SwingScrubber,
    MasterVolume,
    MixerFader,
    MixerPan,
    MasterFader,
    MasterPan,
    ArrangerRulerScrub,
    ArrangerTrackVolume,
    ArrangerTrackPan,
    ArrangerOverviewScroll,
    ArrangerPropertiesResize,
    MixerPropertiesResize,
    TrackInspectorVolume,
    TrackInspectorPan,
    TrackInspectorFxKnob,
    TrackInspectorScrollbar,
    PianoRollMarquee,
    PianoRollNoteMove,
    PianoRollNoteResize,
    PianoRollMiddlePan,
    PianoRollScrollbarV,
    PianoRollScrollbarH,
    VirtualKeyboardGlissando,
    ProjectHubScroll,
    CrtTweakerSlider,
    UiScaleSlider
};

} // namespace eatsbits::ui

#endif // EATS_PRESENTER_DRAG_TYPES_HPP
