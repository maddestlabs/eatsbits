#ifndef EATS_PROCEDURAL_IR_GENERATOR_HPP
#define EATS_PROCEDURAL_IR_GENERATOR_HPP

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <array>
#include <memory>
#include <sstream>

namespace eatsbits::audio {

/// Supported acoustic materials with frequency-dependent absorption and scattering.
enum class AcousticMaterialType {
    BirchPlywood,
    PineWood,
    AcousticFoam,
    Concrete,
    StudioWood,
    VelvetDrapes,
    SheetMetal,
    Carpet,
    Custom
};

/// Physical acoustic properties of boundary surfaces.
struct AcousticMaterial {
    AcousticMaterialType type{AcousticMaterialType::StudioWood};
    const char* displayName{"Wood Paneling (Live Room)"};
    float alphaLow{0.20f};   // 125 - 250 Hz absorption coefficient (0.0 to 1.0)
    float alphaMid{0.12f};   // 500 - 1000 Hz absorption coefficient (0.0 to 1.0)
    float alphaHigh{0.08f};  // 2000 - 4000 Hz absorption coefficient (0.0 to 1.0)
    float diffusion{0.45f};  // Scattering coefficient (0.0 to 1.0)

    [[nodiscard]] static AcousticMaterial get(AcousticMaterialType t) noexcept {
        switch (t) {
            case AcousticMaterialType::BirchPlywood:
                return { t, "18mm Birch Plywood (Cab)", 0.28f, 0.15f, 0.10f, 0.25f };
            case AcousticMaterialType::PineWood:
                return { t, "Pine Wood (Vintage Cab)", 0.24f, 0.18f, 0.12f, 0.30f };
            case AcousticMaterialType::AcousticFoam:
                return { t, "Acoustic Foam / Studio", 0.15f, 0.70f, 0.95f, 0.85f };
            case AcousticMaterialType::Concrete:
                return { t, "Hard Concrete / Marble", 0.01f, 0.02f, 0.03f, 0.10f };
            case AcousticMaterialType::StudioWood:
                return { t, "Wood Paneling (Live Room)", 0.20f, 0.12f, 0.08f, 0.45f };
            case AcousticMaterialType::VelvetDrapes:
                return { t, "Heavy Velvet Drapes", 0.05f, 0.35f, 0.75f, 0.60f };
            case AcousticMaterialType::SheetMetal:
                return { t, "Sheet Metal / Plate", 0.02f, 0.03f, 0.04f, 0.05f };
            case AcousticMaterialType::Carpet:
                return { t, "Thin Carpet on Concrete", 0.02f, 0.15f, 0.50f, 0.40f };
            default:
                return { AcousticMaterialType::StudioWood, "Wood Paneling", 0.20f, 0.12f, 0.08f, 0.45f };
        }
    }
};

/// Parameters defining a physical room, non-linear reverb chamber, or speaker cabinet.
struct AcousticSpaceParams {
    std::string name{"Custom Space"};
    float width{8.0f};        // meters (Lx)
    float length{12.0f};      // meters (Ly)
    float height{4.0f};       // meters (Lz)
    float sourceX{0.5f};      // normalized 0..1 inside enclosure
    float sourceY{0.5f};
    float sourceZ{0.5f};
    float listenerX{0.5f};
    float listenerY{0.8f};
    float listenerZ{0.5f};
    AcousticMaterialType material{AcousticMaterialType::StudioWood};
    float rt60{1.8f};         // seconds (reverberation decay time)
    float damping{0.5f};      // 0.0 (bright, minimal absorption) to 1.0 (heavy absorption)
    float diffusion{0.5f};    // 0.0 (specular) to 1.0 (fully scattered)

    // Non-Linear / Gated Reverb Envelope
    bool isGated{false};      // Classic 80s Phil Collins non-linear gated decay
    float gateHoldMs{180.0f}; // Duration before sharp cutoff
    float gateReleaseMs{20.0f};// Cutoff slope
    bool isReverse{false};    // Reverse swell envelope

    // Speaker Cabinet / Enclosure Mode
    bool isCabinetMode{false};
    float micDistance{0.05f}; // meters (e.g. 0.025 to 0.5m)
    float micAngleDeg{0.0f};  // degrees off-axis (0 to 90)
    bool isOpenBack{false};   // rear dipole cancellation
    float stereoWidth{0.20f}; // meters ear or mic spacing

    [[nodiscard]] AcousticSpaceParams withRoomScale(float factor) const {
        AcousticSpaceParams copy = *this;
        factor = std::clamp(factor, 0.1f, 5.0f);
        copy.width = std::max(0.2f, width * factor);
        copy.length = std::max(0.2f, length * factor);
        copy.height = std::max(0.2f, height * factor);
        return copy;
    }

    [[nodiscard]] AcousticSpaceParams withDecayScale(float factor) const {
        AcousticSpaceParams copy = *this;
        copy.rt60 = std::clamp(rt60 * factor, 0.015f, 12.0f);
        return copy;
    }

    [[nodiscard]] AcousticSpaceParams withDamping(float newDamping) const {
        AcousticSpaceParams copy = *this;
        copy.damping = std::clamp(newDamping, 0.0f, 1.0f);
        return copy;
    }

    [[nodiscard]] std::string toJson() const {
        std::ostringstream ss;
        ss << "{\n"
           << "  \"name\": \"" << name << "\",\n"
           << "  \"width\": " << width << ",\n"
           << "  \"length\": " << length << ",\n"
           << "  \"height\": " << height << ",\n"
           << "  \"sourceX\": " << sourceX << ",\n"
           << "  \"sourceY\": " << sourceY << ",\n"
           << "  \"sourceZ\": " << sourceZ << ",\n"
           << "  \"listenerX\": " << listenerX << ",\n"
           << "  \"listenerY\": " << listenerY << ",\n"
           << "  \"listenerZ\": " << listenerZ << ",\n"
           << "  \"material\": " << static_cast<int>(material) << ",\n"
           << "  \"rt60\": " << rt60 << ",\n"
           << "  \"damping\": " << damping << ",\n"
           << "  \"diffusion\": " << diffusion << ",\n"
           << "  \"isGated\": " << (isGated ? "true" : "false") << ",\n"
           << "  \"gateHoldMs\": " << gateHoldMs << ",\n"
           << "  \"gateReleaseMs\": " << gateReleaseMs << ",\n"
           << "  \"isReverse\": " << (isReverse ? "true" : "false") << ",\n"
           << "  \"isCabinetMode\": " << (isCabinetMode ? "true" : "false") << ",\n"
           << "  \"micDistance\": " << micDistance << ",\n"
           << "  \"micAngleDeg\": " << micAngleDeg << ",\n"
           << "  \"isOpenBack\": " << (isOpenBack ? "true" : "false") << ",\n"
           << "  \"stereoWidth\": " << stereoWidth << "\n"
           << "}";
        return ss.str();
    }
};

/// High-resolution stereo impulse response buffer.
struct StereoIRBuffer {
    std::vector<float> left;
    std::vector<float> right;

    [[nodiscard]] size_t size() const noexcept { return left.size(); }
    [[nodiscard]] bool empty() const noexcept { return left.empty(); }
    void clear() noexcept { left.clear(); right.clear(); }
};

/**
 * Procedural Impulse Response Generator.
 * Synthesizes high-fidelity impulse responses purely through physical boundary
 * simulation, 3D Image Source Method (ISM), frequency-dependent damping,
 * velvet noise diffuse tails, and non-linear envelopes.
 */
class ProceduralIRGenerator {
public:
    static constexpr float kSpeedOfSound = 343.0f; // m/s
    static constexpr float kPi = 3.14159265358979323846f;

    /// Returns the built-in stock preset catalog (Rooms, Halls, Plates, Springs, Gated, Cabinets).
    [[nodiscard]] static const std::vector<AcousticSpaceParams>& getStockPresets();

    /// Finds a preset by name or partial match.
    [[nodiscard]] static const AcousticSpaceParams* findPreset(const std::string& name);

    /// Registers a custom acoustic space into the runtime catalog.
    static bool registerCustomPreset(const AcousticSpaceParams& params);

    /// Returns a list of all available preset names.
    [[nodiscard]] static std::vector<std::string> getAvailablePresetNames();

    /// Generates a synthesized stereo impulse response buffer.
    [[nodiscard]] static StereoIRBuffer generateStereo(
        const AcousticSpaceParams& p,
        int sampleRate = 44100,
        int maxSamples = 8192
    );

    /// Generates a synthesized mono impulse response buffer.
    [[nodiscard]] static std::vector<float> generateMono(
        const AcousticSpaceParams& p,
        int sampleRate = 44100,
        int maxSamples = 8192
    );

    /// Normalizes peak amplitude across stereo channels.
    static void normalizeStereo(
        std::vector<float>& left,
        std::vector<float>& right,
        float maxPeak = 0.95f
    ) noexcept;

private:
    struct Biquad {
        float b0{1.0f}, b1{0.0f}, b2{0.0f}, a1{0.0f}, a2{0.0f};
        float x1{0.0f}, x2{0.0f}, y1{0.0f}, y2{0.0f};

        static Biquad highPass(float fs, float fc, float q) noexcept;
        static Biquad lowPass(float fs, float fc, float q) noexcept;
        static Biquad peakingEQ(float fs, float fc, float q, float gainDb) noexcept;
        void processInPlace(std::vector<float>& buffer) noexcept;
    };

    static StereoIRBuffer generateRoomIRStereo(const AcousticSpaceParams& p, int sampleRate, int maxSamples);
    static StereoIRBuffer generateCabinetIRStereo(const AcousticSpaceParams& p, int sampleRate, int maxSamples);
};

} // namespace eatsbits::audio

#endif // EATS_PROCEDURAL_IR_GENERATOR_HPP
