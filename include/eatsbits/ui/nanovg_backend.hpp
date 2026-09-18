#ifndef EATS_NANOVG_BACKEND_HPP
#define EATS_NANOVG_BACKEND_HPP

#include <vector>
#include <string>
#include <cstdint>

namespace eatsbits::ui {

/**
 * 2D Vertex stream emitted by NanoVG vector tessellation.
 */
struct VectorVertex2D {
    float x, y;
    float u, v;
    uint32_t color;
};

/**
 * Draw call command sent to Google Filament or any GPU rasterizer.
 */
struct VectorDrawCall {
    uint32_t vertexOffset{0};
    uint32_t vertexCount{0};
    uint32_t indexOffset{0};
    uint32_t indexCount{0};
    int imageTextureId{0};
    float scissor[4]{0.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * Abstract Render Backend for NanoVG.
 * Bridges NanoVG tessellation into Filament vertex/index buffers.
 */
class INanoVGBackend {
public:
    virtual ~INanoVGBackend() = default;
    virtual bool initialize(uint32_t width, uint32_t height) = 0;
    virtual void beginFrame(float width, float height, float pixelRatio) = 0;
    virtual void submitBatch(const VectorVertex2D* vertices, size_t vCount,
                             const uint32_t* indices, size_t iCount,
                             const VectorDrawCall& call) = 0;
    virtual void endFrame() = 0;
};

/**
 * Core DAW vector widget definitions.
 */
struct RotaryKnob {
    std::string name{"Knob"};
    float minVal{0.0f};
    float maxVal{1.0f};
    float currentVal{0.5f};
    float x{0.0f}, y{0.0f}, radius{24.0f};

    [[nodiscard]] float getNormalized() const noexcept {
        return (maxVal > minVal) ? (currentVal - minVal) / (maxVal - minVal) : 0.0f;
    }
};

struct LinearFader {
    std::string name{"Fader"};
    float minVal{0.0f};
    float maxVal{1.0f};
    float currentVal{0.8f};
    float x{0.0f}, y{0.0f}, width{16.0f}, height{120.0f};
};

struct VuMeterWidget {
    float peakL{0.0f};
    float peakR{0.0f};
    float rmsL{0.0f};
    float rmsR{0.0f};
    float x{0.0f}, y{0.0f}, width{30.0f}, height{120.0f};
};

} // namespace eatsbits::ui

#endif // EATS_NANOVG_BACKEND_HPP
