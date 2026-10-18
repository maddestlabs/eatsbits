#ifndef EATS_CLAVINET_NODE_HPP
#define EATS_CLAVINET_NODE_HPP

#include "../graph_node.hpp"
#include "../../dsp/clavinet_core.hpp"

namespace eatsbits::audio {

/**
 * Modular Hohner Clavinet D6 Physical Modeling Instrument Node.
 * Replaces Grand Piano fallback with authentic rubber-tipped hammer strike,
 * taut steel string digital waveguide, key-off yarn damper thump,
 * dual single-coil pickups with A-B funk quack, and 4-rocker EQ filter bank.
 */
class ClavinetNode : public GraphNode {
public:
    explicit ClavinetNode(std::string name = "ClavinetD6")
        : GraphNode(std::move(name)) {
        // Generator source node: 0 inputs, 1 stereo output port (2 channels: L/R)
        addOutputPort(2);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        clavinet_.prepare(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        clavinet_.reset();
    }

    void noteOn(uint8_t note, float velocity = 0.85f) noexcept {
        clavinet_.noteOn(note, velocity);
    }

    void noteOff(uint8_t note) noexcept {
        clavinet_.noteOff(note);
    }

    void allNotesOff() noexcept {
        clavinet_.allNotesOff();
    }

    // Direct Parameter Setters
    void setPickupSelect(float val) noexcept { clavinet_.setPickupSelect(val); }
    [[nodiscard]] float getPickupSelect() const noexcept { return clavinet_.getPickupSelect(); }

    void setPhaseInvert(float val) noexcept { clavinet_.setPhaseInvert(val); }
    [[nodiscard]] float getPhaseInvert() const noexcept { return clavinet_.getPhaseInvert(); }

    void setBrilliant(float val) noexcept { clavinet_.setBrilliant(val); }
    [[nodiscard]] float getBrilliant() const noexcept { return clavinet_.getBrilliant(); }

    void setTreble(float val) noexcept { clavinet_.setTreble(val); }
    [[nodiscard]] float getTreble() const noexcept { return clavinet_.getTreble(); }

    void setMedium(float val) noexcept { clavinet_.setMedium(val); }
    [[nodiscard]] float getMedium() const noexcept { return clavinet_.getMedium(); }

    void setSoft(float val) noexcept { clavinet_.setSoft(val); }
    [[nodiscard]] float getSoft() const noexcept { return clavinet_.getSoft(); }

    void setHammerHardness(float val) noexcept { clavinet_.setHammerHardness(val); }
    [[nodiscard]] float getHammerHardness() const noexcept { return clavinet_.getHammerHardness(); }

    void setHammerSnap(float val) noexcept { clavinet_.setHammerSnap(val); }
    [[nodiscard]] float getHammerSnap() const noexcept { return clavinet_.getHammerSnap(); }

    void setSustain(float val) noexcept { clavinet_.setSustain(val); }
    [[nodiscard]] float getSustain() const noexcept { return clavinet_.getSustain(); }

    void setDamperThump(float val) noexcept { clavinet_.setDamperThump(val); }
    [[nodiscard]] float getDamperThump() const noexcept { return clavinet_.getDamperThump(); }

    void setDrive(float val) noexcept { clavinet_.setDrive(val); }
    [[nodiscard]] float getDrive() const noexcept { return clavinet_.getDrive(); }

    void setParameterByName(const std::string& name, float value) noexcept {
        clavinet_.setParameterByName(name, value);
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setPickupSelect(value); break;
            case 1: setPhaseInvert(value); break;
            case 2: setBrilliant(value); break;
            case 3: setTreble(value); break;
            case 4: setMedium(value); break;
            case 5: setSoft(value); break;
            case 6: setHammerHardness(value); break;
            case 7: setHammerSnap(value); break;
            case 8: setSustain(value); break;
            case 9: setDamperThump(value); break;
            case 10: setDrive(value); break;
            default: break;
        }
    }

    void handleEvent(const AudioEvent& event) noexcept override {
        switch (event.type) {
            case AudioEventType::NoteOn:
                noteOn(event.note, event.velocity);
                break;
            case AudioEventType::NoteOff:
                noteOff(event.note);
                break;
            case AudioEventType::AllNotesOff:
                allNotesOff();
                break;
            case AudioEventType::SetParameter:
                setParameter(event.paramId, event.paramValue);
                break;
            default:
                break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override {
        float* outL = getOutputBuffer(0, 0);
        float* outR = getOutputBuffer(0, 1);

        if (!outL || !outR) return;

        if (!enabled_) {
            std::fill_n(outL, numFrames, 0.0f);
            std::fill_n(outR, numFrames, 0.0f);
            return;
        }

        clavinet_.process(outL, outR, numFrames);
    }

private:
    dsp::ClavinetCore clavinet_;
};

} // namespace eatsbits::audio

#endif // EATS_CLAVINET_NODE_HPP
