#include "eatsbits/ui/dawn_bridge.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <vector>

#include <webgpu/webgpu.h>
#if !defined(__EMSCRIPTEN__)
#include <webgpu/wgpu.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#pragma comment(lib, "wgpu_native.dll.lib")
#endif

namespace eatsbits::ui {

// Embedded pure WGSL Apocalypse CRT shader (Hardware GPU native pipeline)
// Applies exclusively to central DAW workspace (below transport, above chin)
static const char* kCrtWgslSource = R"(
struct CrtUniforms {
    resolution: vec2<f32>,       // [width, height] (offset 0)
    lightPos: vec2<f32>,         // [lightX, lightY] (offset 8)
    rumbleOffset: vec2<f32>,     // [offX, offY] (offset 16)
    iTime: f32,                  // (offset 24)
    curvature: f32,              // (offset 28)
    scanlineIntensity: f32,      // (offset 32)
    topBarFraction: f32,         // (offset 36)
    bottomBarFraction: f32,      // (offset 40)
    crtEnabled: f32,             // 1.0 = on, 0.0 = bypass (offset 44)
    rumbleDim: f32,              // (offset 48)
    hWaveStrength: f32,          // (offset 52)
    gammaCorrection: f32,        // 1.0 = native Unorm, 2.2 = sRGB linearization (offset 56)
    padding: f32,                // (offset 60) -> total 64 bytes
};

@group(0) @binding(0) var dawTexture: texture_2d<f32>;
@group(0) @binding(1) var dawSampler: sampler;
@group(0) @binding(2) var<uniform> u: CrtUniforms;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>( 3.0, -1.0),
        vec2<f32>(-1.0,  3.0)
    );
    var out: VertexOutput;
    let p = pos[vertexIndex];
    out.position = vec4<f32>(p, 0.0, 1.0);
    out.uv = vec2<f32>((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);
    return out;
}

// Apocalypse CRT RGB chromatic fringing (rgbDistortion)
fn sampleRgbDistortion(uv: vec2<f32>, offset: f32) -> vec3<f32> {
    let r = textureSampleLevel(dawTexture, dawSampler, uv + vec2<f32>(offset, 0.0), 0.0).r;
    let g = textureSampleLevel(dawTexture, dawSampler, uv, 0.0).g;
    let b = textureSampleLevel(dawTexture, dawSampler, uv - vec2<f32>(offset, 0.0), 0.0).b;
    return vec3<f32>(r, g, b);
}

fn sdBox2D(p: vec2<f32>, b: vec2<f32>) -> f32 {
    let d = abs(p) - b;
    return length(max(d, vec2<f32>(0.0))) + min(max(d.x, d.y), 0.0);
}

// Procedural Eatsbits Brand Logo SDF (Canonical Amber Chassis + Monster Head + Eating Bits)
fn evaluateEatsbitsLogoSdf(p: vec2<f32>) -> f32 {
    // 1. Amber Chassis Rounded Square
    let dChassis = sdBox2D(p, vec2<f32>(0.28, 0.28)) - 0.156;

    // 2. Weathered Monster Head
    let pHead = p - vec2<f32>(-0.031, -0.002);
    let dHeadBox = sdBox2D(pHead, vec2<f32>(0.273, 0.303));
    let dMouth = sdBox2D(pHead - vec2<f32>(0.117, 0.047), vec2<f32>(0.156, 0.117));
    let dHead = max(dHeadBox, -dMouth);

    // 3. Eye cutout
    let dEye = length(pHead - vec2<f32>(0.008, -0.187)) - 0.062;
    let dHeadWithEye = max(dHead, -dEye);

    // 4. Two floating eating bits
    let dBit1 = sdBox2D(pHead - vec2<f32>(0.328, -0.004), vec2<f32>(0.039, 0.039)) - 0.008;
    let dBit2 = sdBox2D(pHead - vec2<f32>(0.316, 0.160), vec2<f32>(0.029, 0.029)) - 0.006;
    let dBits = min(dBit1, dBit2);

    let dMonster = min(dHeadWithEye, dBits);

    // Composite: chassis plate outline with monster emblem cutout
    return min(abs(dChassis) - 0.015, dMonster);
}

// Authentic Apocalypse CRT Overhead Spotlight Illumination
// Noticeable, atmospheric light cone with natural spherical falloff
fn calculateLightFactor(uv: vec2<f32>, lightPos: vec2<f32>) -> f32 {
    var lightDelta = uv - lightPos;
    lightDelta.x *= 1.35; // slight horizontal oval beam matching original
    let dist = length(lightDelta);
    // Smooth power-law falloff matching MaddestLabs
    let spot = pow(clamp(1.0 - (dist / 1.35), 0.0, 1.0), 1.1);
    // Dynamic range [0.68, 1.32]: rich, noticeable ambient spotlight that preserves DAW dark-mode contrast
    return mix(0.68, 1.32, spot);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var globalUV = in.uv;
    if (u.crtEnabled > 0.5) {
        globalUV += u.rumbleOffset;
    }

    // Pass-through when shader is toggled off
    if (u.crtEnabled < 0.5) {
        var rawCol = textureSampleLevel(dawTexture, dawSampler, in.uv, 0.0);
        if (u.gammaCorrection > 1.05) {
            rawCol = vec4<f32>(pow(max(rawCol.rgb, vec3<f32>(0.0)), vec3<f32>(u.gammaCorrection)), rawCol.a);
        }
        return rawCol;
    }

    let topCut = u.topBarFraction;
    let bottomCut = 1.0 - u.bottomBarFraction;

    // =========================================================================
    // 1. TOP TRANSPORT & BOTTOM CHIN (Fullscreen rumble, flat spotlight, seams & brushed metal)
    // =========================================================================
    if (globalUV.y < topCut || globalUV.y > bottomCut) {
        let sampleUV = clamp(globalUV, vec2<f32>(0.0001), vec2<f32>(0.9999));
        var panelCol = textureSampleLevel(dawTexture, dawSampler, sampleUV, 0.0).rgb;
        let panelLight = calculateLightFactor(globalUV, u.lightPos);
        panelCol *= panelLight;
        panelCol -= vec3<f32>(u.rumbleDim);

        // Diegetic horizontal brushed metal texture on chassis faceplates
        let brushed = sin(globalUV.x * u.resolution.x * 2.5) * 0.008;
        panelCol += vec3<f32>(brushed);

        // Top Panel Bottom Lip: Shadow crevice right at topCut seam
        if (globalUV.y < topCut) {
            let distToSeam = topCut - globalUV.y;
            if (distToSeam < 0.005) {
                let seamAO = smoothstep(0.0, 0.005, distToSeam);
                panelCol *= mix(0.45, 1.0, seamAO);
            }
        }

        // Bottom Panel Top Lip: Specular bevel highlight along the chin seam
        if (globalUV.y > bottomCut) {
            let distFromSeam = globalUV.y - bottomCut;
            if (distFromSeam < 0.006) {
                let rimHigh = smoothstep(0.006, 0.0, abs(distFromSeam - 0.0015)) * 0.22;
                panelCol += vec3<f32>(rimHigh);
            }
        }

        var finalCol = clamp(panelCol, vec3<f32>(0.0), vec3<f32>(1.0));
        if (u.gammaCorrection > 1.05) {
            finalCol = pow(finalCol, vec3<f32>(u.gammaCorrection));
        }
        return vec4<f32>(finalCol, 1.0);
    }
)"
R"(
    // =========================================================================
    // 2. CENTRAL DAW WORKSPACE CRT SHADER (Recessed 3D Shadow-Box Cavity)
    // =========================================================================
    let span = max(0.01, bottomCut - topCut);
    let suv = vec2<f32>(globalUV.x, (globalUV.y - topCut) / span);

    // Horizontal sync scan wave (only modulates phosphor electron beam & reflections, NEVER moves physical chassis)
    var hWave = 0.0;
    if (u.hWaveStrength > 0.00001) {
        hWave = sin(suv.y * 10.0 + u.iTime * 5.0) * u.hWaveStrength;
    }

    // 3D Chassis Beveled Frame Geometry (Sleek and flush to panel/screen edges)
    let frameY_Top = 0.008;      // Sleek top chamfer (~5px, perfectly flush to top transport)
    let frameY_Bottom = 0.012;   // Sleek bottom reflective chin (~8px, flush to bottom nav bar)
    let frameX = 0.004;          // Sleek side lip (~3px, flush to window borders)

    // Outer frame evaluated in screen space (guaranteed 100% flush to window and panels)
    let isTopLedge = (suv.y < frameY_Top);
    let isBottomLedge = (suv.y > 1.0 - frameY_Bottom);
    let isLeftLedge = (suv.x < frameX);
    let isRightLedge = (suv.x > 1.0 - frameX);
    let isFrame = isTopLedge || isBottomLedge || isLeftLedge || isRightLedge;

    // Subtle, elegant CRT bulb curvature (decreased for zero corner clipping and crisp readability)
    var curvedUV = suv;
    if (u.curvature > 0.001) {
        let center = vec2<f32>(0.5, 0.5);
        let dCenter = curvedUV - center;
        let dist = length(dCenter);
        // Gentle retro power curve
        curvedUV += dCenter * pow(dist, 2.5) * (u.curvature * 0.09);
    }

    // Spotlight illumination computed on curved CRT surface
    let crtLightFactor = calculateLightFactor(curvedUV, u.lightPos);

    if (isFrame) {
        // Distance from outer screen borders
        let distX = min(suv.x, 1.0 - suv.x);
        let distY = select(suv.y, 1.0 - suv.y, suv.y > 0.5);
        let normX = clamp(distX / frameX, 0.0, 1.0);
        let normY = select(
            clamp(suv.y / frameY_Top, 0.0, 1.0),
            clamp((1.0 - suv.y) / frameY_Bottom, 0.0, 1.0),
            suv.y > 0.5
        );

        // Base weathered gunmetal steel material
        var chassisColor = vec3<f32>(0.040, 0.041, 0.044);

        // 1. TOP HOOD INNER BEVEL: Angled downward away from overhead spotlight
        if (isTopLedge && normY <= normX) {
            let topSlope = mix(0.060, 0.024, normY);
            chassisColor = vec3<f32>(topSlope, topSlope * 0.98, topSlope * 0.94);

            // Specular highlight line along topCut seam
            let topLip = smoothstep(0.003, 0.0, suv.y) * 0.35;
            chassisColor += vec3<f32>(topLip, topLip * 0.96, topLip * 0.90);
        }
        // 2. BOTTOM CHIN INNER BEVEL: Angled upward, catches spotlight and phosphor reflection
        else if (isBottomLedge && normY <= normX) {
            let botSlope = mix(0.038, 0.082, 1.0 - normY);
            chassisColor = vec3<f32>(botSlope, botSlope * 0.97, botSlope * 0.93);

            // Specular rim line along bottom chin seam
            let botLip = smoothstep(0.003, 0.0, 1.0 - suv.y) * 0.30;
            chassisColor += vec3<f32>(botLip);

            // DIEGETIC PHOSPHOR SCREEN SPILL & BLURRED REFLECTION:
            // Samples DAW timeline tracks mirrored vertically onto the chin shelf
            let clampedReflY = clamp(topCut + (1.0 - (1.0 - suv.y) * 2.5) * span, topCut, bottomCut);
            let reflCoord = vec2<f32>(
                clamp(suv.x + hWave * 0.4, 0.002, 0.998),
                clampedReflY
            );
            let blurX = 6.0 / u.resolution.x;
            var spill = textureSampleLevel(dawTexture, dawSampler, reflCoord, 0.0).rgb * 0.32;
            spill += textureSampleLevel(dawTexture, dawSampler, reflCoord + vec2<f32>( blurX,  0.0), 0.0).rgb * 0.22;
            spill += textureSampleLevel(dawTexture, dawSampler, reflCoord - vec2<f32>( blurX,  0.0), 0.0).rgb * 0.22;
            spill += textureSampleLevel(dawTexture, dawSampler, reflCoord + vec2<f32>( blurX * 2.0, 0.0), 0.0).rgb * 0.12;
            spill += textureSampleLevel(dawTexture, dawSampler, reflCoord - vec2<f32>( blurX * 2.0, 0.0), 0.0).rgb * 0.12;

            // Blend reflection into the brushed metal surface
            chassisColor += spill * 0.35 * (1.0 - normY * 0.4);
        }
        // 3. VERTICAL SIDE JAMBS: Sleek side depth & soft horizontal gradient
        else {
            let sideSlope = mix(0.048, 0.024, normX);
            chassisColor = vec3<f32>(sideSlope, sideSlope * 0.96, sideSlope * 0.92);

            // Subtle outer edge highlight
            let sideLip = smoothstep(0.003, 0.0, distX) * 0.22;
            chassisColor += vec3<f32>(sideLip);
        }

        // 45-DEGREE CORNER MITER SEAMS:
        let miterDelta = abs(normX - normY);
        let miterCrease = mix(0.55, 1.0, smoothstep(0.0, 0.20, miterDelta));
        chassisColor *= miterCrease;

        var frameCol = clamp(chassisColor * crtLightFactor - vec3<f32>(u.rumbleDim), vec3<f32>(0.0), vec3<f32>(1.0));
        if (u.gammaCorrection > 1.05) {
            frameCol = pow(frameCol, vec3<f32>(u.gammaCorrection));
        }
        return vec4<f32>(frameCol, 1.0);
    }
)"
R"(
    // ACTIVE PHOSPHOR RASTER DISPLAY:
    // Fills 100% of the screen area between the flush bezels with zero empty space or cutouts
    let tubeUV = vec2<f32>(
        clamp((curvedUV.x - frameX) / (1.0 - 2.0 * frameX), 0.0, 1.0),
        clamp((curvedUV.y - frameY_Top) / (1.0 - frameY_Top - frameY_Bottom), 0.0, 1.0)
    );

    // Screen raster content wobbles horizontally with hWave
    let texUV = vec2<f32>(
        clamp(tubeUV.x + hWave, 0.001, 0.999),
        topCut + clamp(tubeUV.y, 0.001, 0.999) * span
    );

    // Subtle RGB chromatic fringing
    var color = sampleRgbDistortion(texUV, 0.0007);

    // PHYSICAL CHASSIS OVERHANG DROP SHADOW (AMBIENT OCCLUSION):
    // Realistic depth gradient along the top and side edges without obscuring text
    let topOverhangShadow = smoothstep(0.0, 0.12, tubeUV.y);
    let hoodAO = mix(0.70, 1.0, topOverhangShadow);

    let sideShadow = smoothstep(0.0, 0.030, min(tubeUV.x, 1.0 - tubeUV.x));
    let sideAO = mix(0.85, 1.0, sideShadow);

    let botShadow = smoothstep(0.0, 0.020, 1.0 - tubeUV.y);
    let botAO = mix(0.92, 1.0, botShadow);

    color *= (hoodAO * sideAO * botAO);

    // Crisp raster scanlines with preserved contrast
    let scanIntensity = select(u.scanlineIntensity * 0.5, 0.15, u.scanlineIntensity <= 0.001);
    let scanPhase = tubeUV.y * span * u.resolution.y;
    let scanPattern = sin(scanPhase * 3.14159265);
    let scanline = (1.0 - scanIntensity) + scanIntensity * (0.5 + 0.5 * scanPattern);
    color *= scanline;

    // Subtle aperture triad striping
    let triadPattern = sin(tubeUV.x * u.resolution.x * 3.14159265);
    let grille = 1.0 - 0.03 * (0.5 + 0.5 * triadPattern);
    color *= grille;

    // Smooth vignette darkening at tube edges
    let vigUV = tubeUV * (vec2<f32>(1.0) - tubeUV.yx);
    let vig = clamp(pow(vigUV.x * vigUV.y * 14.0, 0.16), 0.0, 1.0);
    color *= vig;

    // Ambient Environmental Reflection: 10x Scaled Eatsbits Logo with Stronger Frosted Blur & Shifted Right
    // Simulates atmospheric studio reflection on curved CRT bulb glass
    var reflP = tubeUV - vec2<f32>(0.58, 0.48); // Shifted slightly to the right
    reflP.x *= (u.resolution.x / u.resolution.y);
    // Subtle -11 degree rotation
    let cosR = 0.981;
    let sinR = -0.191;
    let rotLogoP = vec2<f32>(reflP.x * cosR - reflP.y * sinR, reflP.x * sinR + reflP.y * cosR);
    let logoCoord = rotLogoP / 0.82; // Scaled 10x (covering ~82% of screen)

    let dLogo = evaluateEatsbitsLogoSdf(logoCoord);
    // Stronger, soft diffuse frosted environmental blur (multi-scale smoothstep envelope)
    let wideHaze = smoothstep(0.18, -0.10, dLogo) * 0.45;
    let coreGlow = smoothstep(0.09, -0.05, dLogo) * 0.55;
    let logoGlow = wideHaze + coreGlow;
    let tubeEdgeFade = smoothstep(0.0, 0.12, tubeUV.x) * smoothstep(1.0, 0.88, tubeUV.x) *
                       smoothstep(0.0, 0.12, tubeUV.y) * smoothstep(1.0, 0.88, tubeUV.y);
    let logoReflection = logoGlow * tubeEdgeFade * 0.040;
    color += vec3<f32>(0.96, 0.92, 0.85) * logoReflection;

    // Ambient spotlight illumination (curved with CRT bulb) & sub-bass power sag
    color *= crtLightFactor;
    color -= vec3<f32>(u.rumbleDim);

    var finalCol = clamp(color, vec3<f32>(0.0), vec3<f32>(1.0));
    if (u.gammaCorrection > 1.05) {
        finalCol = pow(finalCol, vec3<f32>(u.gammaCorrection));
    }

    return vec4<f32>(finalCol, 1.0);
}
)";

// Strictly 64-byte aligned uniform buffer matching WGSL layout
struct alignas(16) GpuCrtUniforms {
    float resolution[2];     // offset 0
    float lightPos[2];       // offset 8
    float rumbleOffset[2];   // offset 16
    float iTime;             // offset 24
    float curvature;         // offset 28
    float scanlineIntensity; // offset 32
    float topBarFraction;    // offset 36
    float bottomBarFraction; // offset 40
    float crtEnabled;        // offset 44
    float rumbleDim;         // offset 48
    float hWaveStrength;     // offset 52
    float gammaCorrection;   // offset 56 (1.0 = native Unorm, 2.2 = sRGB linearization)
    float padding;           // offset 60 -> total 64 bytes
};
static_assert(sizeof(GpuCrtUniforms) == 64, "GpuCrtUniforms must be 64 bytes");

struct DawnBridge::Impl {
    uint32_t currentWidth{1280};
    uint32_t currentHeight{800};

#if defined(_WIN32)
    HWND hwnd{nullptr};
#endif

    // WebGPU Hardware State
    WGPUInstance instance{nullptr};
    WGPUAdapter adapter{nullptr};
    WGPUDevice device{nullptr};
    WGPUQueue queue{nullptr};
    WGPUSurface surface{nullptr};
    WGPURenderPipeline pipeline{nullptr};
    WGPUTexture dawTexture{nullptr};
    WGPUTextureView dawView{nullptr};
    WGPUSampler dawSampler{nullptr};
    WGPUBuffer uniformBuffer{nullptr};
    WGPUBindGroup bindGroup{nullptr};
    WGPUTextureFormat surfaceFormat{WGPUTextureFormat_BGRA8Unorm};
    bool gpuReady{false};

    std::filesystem::file_time_type lastShaderModTime{};
    uint32_t shaderCheckCounter{0};

    void checkAndReloadShader() {
        if (!device || !gpuReady) return;
        shaderCheckCounter++;
        if (shaderCheckCounter % 30 != 0) return; // poll filesystem every ~0.5 sec

        std::error_code ec;
        auto ftime = std::filesystem::last_write_time("assets/shaders/crt_screen.wgsl", ec);
        if (ec) return;
        if (lastShaderModTime == std::filesystem::file_time_type{}) {
            lastShaderModTime = ftime;
            return;
        }
        if (ftime == lastShaderModTime) return;
        lastShaderModTime = ftime;

        std::ifstream shaderFile("assets/shaders/crt_screen.wgsl");
        if (!shaderFile.is_open()) return;
        std::stringstream buffer;
        buffer << shaderFile.rdbuf();
        std::string shaderCode = buffer.str();
        if (shaderCode.empty()) return;

        WGPUShaderSourceWGSL wgslDesc{};
        wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
        wgslDesc.code = WGPUStringView{ shaderCode.c_str(), shaderCode.length() };

        WGPUShaderModuleDescriptor smDesc{};
        smDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDesc);
        WGPUShaderModule sm = wgpuDeviceCreateShaderModule(device, &smDesc);
        if (!sm) {
            std::cerr << "[DawnBridge] Hot-reload error: failed to compile WGSL shader\n";
            return;
        }

        WGPUColorTargetState colorTarget{};
        colorTarget.format = surfaceFormat;
        colorTarget.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState fragState{};
        fragState.module = sm;
        fragState.entryPoint = WGPUStringView{ "fs_main", 7 };
        fragState.targetCount = 1;
        fragState.targets = &colorTarget;

        WGPURenderPipelineDescriptor pDesc{};
        pDesc.vertex.module = sm;
        pDesc.vertex.entryPoint = WGPUStringView{ "vs_main", 7 };
        pDesc.fragment = &fragState;
        pDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pDesc.multisample.count = 1;
        pDesc.multisample.mask = ~0u;

        WGPURenderPipeline newPipeline = wgpuDeviceCreateRenderPipeline(device, &pDesc);
        wgpuShaderModuleRelease(sm);

        if (newPipeline) {
            if (pipeline) {
                wgpuRenderPipelineRelease(pipeline);
            }
            pipeline = newPipeline;
            std::cout << "[DawnBridge] Live hot-reloaded 'assets/shaders/crt_screen.wgsl' successfully!\n";
        } else {
            std::cerr << "[DawnBridge] Hot-reload error: failed to create render pipeline\n";
        }
    }

    void cleanupGpu() {
        if (bindGroup) {
            wgpuBindGroupRelease(bindGroup);
            bindGroup = nullptr;
        }
        if (uniformBuffer) {
            wgpuBufferDestroy(uniformBuffer);
            wgpuBufferRelease(uniformBuffer);
            uniformBuffer = nullptr;
        }
        if (dawSampler) {
            wgpuSamplerRelease(dawSampler);
            dawSampler = nullptr;
        }
        if (dawView) {
            wgpuTextureViewRelease(dawView);
            dawView = nullptr;
        }
        if (dawTexture) {
            wgpuTextureDestroy(dawTexture);
            wgpuTextureRelease(dawTexture);
            dawTexture = nullptr;
        }
        if (pipeline) {
            wgpuRenderPipelineRelease(pipeline);
            pipeline = nullptr;
        }
        if (surface) {
            wgpuSurfaceRelease(surface);
            surface = nullptr;
        }
        if (queue) {
            wgpuQueueRelease(queue);
            queue = nullptr;
        }
        if (device) {
            wgpuDeviceDestroy(device);
            wgpuDeviceRelease(device);
            device = nullptr;
        }
        if (adapter) {
            wgpuAdapterRelease(adapter);
            adapter = nullptr;
        }
        if (instance) {
            wgpuInstanceRelease(instance);
            instance = nullptr;
        }
        gpuReady = false;
    }

    bool initGpu(void* windowHandle, uint32_t width, uint32_t height) {
        if (!windowHandle || width == 0 || height == 0) return false;

#if defined(_WIN32)
        if (IsWindow(reinterpret_cast<HWND>(windowHandle))) {
            hwnd = reinterpret_cast<HWND>(windowHandle);
        } else {
            hwnd = glfwGetWin32Window(static_cast<GLFWwindow*>(windowHandle));
        }
        if (!hwnd) return false;

        currentWidth = width;
        currentHeight = height;

        WGPUInstanceDescriptor instDesc{};
        instance = wgpuCreateInstance(&instDesc);
        if (!instance) return false;

        WGPUSurfaceSourceWindowsHWND winDesc{};
        winDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
        winDesc.hinstance = GetModuleHandle(nullptr);
        winDesc.hwnd = hwnd;

        WGPUSurfaceDescriptor surfDesc{};
        surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&winDesc);
        surface = wgpuInstanceCreateSurface(instance, &surfDesc);
        if (!surface) {
            cleanupGpu();
            return false;
        }

        WGPURequestAdapterOptions opt{};
        opt.compatibleSurface = surface;
        opt.powerPreference = WGPUPowerPreference_HighPerformance;

        struct ReqData {
            WGPUAdapter adapter{nullptr};
            WGPUDevice device{nullptr};
        } req;

        WGPURequestAdapterCallbackInfo aCb{};
        aCb.mode = WGPUCallbackMode_AllowProcessEvents;
        aCb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adp, WGPUStringView, void* u, void*) {
            if (status == WGPURequestAdapterStatus_Success) static_cast<ReqData*>(u)->adapter = adp;
        };
        aCb.userdata1 = &req;
        wgpuInstanceRequestAdapter(instance, &opt, aCb);
        wgpuInstanceProcessEvents(instance);

        adapter = req.adapter;
        if (!adapter) {
            cleanupGpu();
            return false;
        }

        WGPUDeviceDescriptor dDesc{};
        WGPURequestDeviceCallbackInfo dCb{};
        dCb.mode = WGPUCallbackMode_AllowProcessEvents;
        dCb.callback = [](WGPURequestDeviceStatus status, WGPUDevice dev, WGPUStringView, void* u, void*) {
            if (status == WGPURequestDeviceStatus_Success) static_cast<ReqData*>(u)->device = dev;
        };
        dCb.userdata1 = &req;
        wgpuAdapterRequestDevice(adapter, &dDesc, dCb);
        wgpuInstanceProcessEvents(instance);

        device = req.device;
        if (!device) {
            cleanupGpu();
            return false;
        }
        queue = wgpuDeviceGetQueue(device);

        // Query preferred surface format
        WGPUSurfaceCapabilities caps{};
        wgpuSurfaceGetCapabilities(surface, adapter, &caps);

        // Select linear unorm format (BGRA8Unorm) instead of BGRA8UnormSrgb.
        // The DAW UI renderer already generates perceptual sRGB color values.
        // If an sRGB swapchain format is selected, the GPU hardware applies
        // an extra linear-to-sRGB gamma curve (c^(1/2.2)), which washes out all dark tones
        // and makes colors overbright.
        surfaceFormat = WGPUTextureFormat_BGRA8Unorm;
        bool formatFound = false;
        for (size_t i = 0; i < caps.formatCount; ++i) {
            if (caps.formats[i] == WGPUTextureFormat_BGRA8Unorm) {
                surfaceFormat = WGPUTextureFormat_BGRA8Unorm;
                formatFound = true;
                break;
            }
        }
        if (!formatFound) {
            for (size_t i = 0; i < caps.formatCount; ++i) {
                if (caps.formats[i] == WGPUTextureFormat_RGBA8Unorm) {
                    surfaceFormat = WGPUTextureFormat_RGBA8Unorm;
                    formatFound = true;
                    break;
                }
            }
        }
        if (!formatFound && caps.formatCount > 0) {
            surfaceFormat = caps.formats[0];
        }

        WGPUSurfaceConfiguration sConf{};
        sConf.device = device;
        sConf.format = surfaceFormat;
        sConf.usage = WGPUTextureUsage_RenderAttachment;
        sConf.width = width;
        sConf.height = height;
        sConf.presentMode = WGPUPresentMode_Fifo;
        wgpuSurfaceConfigure(surface, &sConf);

        // Compile pure WGSL shader
        std::string shaderCode = kCrtWgslSource;
        std::ifstream shaderFile("assets/shaders/crt_screen.wgsl");
        if (shaderFile.is_open()) {
            std::stringstream buffer;
            buffer << shaderFile.rdbuf();
            shaderCode = buffer.str();
        }

        WGPUShaderSourceWGSL wgslDesc{};
        wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
        wgslDesc.code = WGPUStringView{ shaderCode.c_str(), shaderCode.length() };

        WGPUShaderModuleDescriptor smDesc{};
        smDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDesc);
        WGPUShaderModule sm = wgpuDeviceCreateShaderModule(device, &smDesc);
        if (!sm) {
            cleanupGpu();
            return false;
        }

        // Post-processing render pipeline
        WGPUColorTargetState colorTarget{};
        colorTarget.format = surfaceFormat;
        colorTarget.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState fragState{};
        fragState.module = sm;
        fragState.entryPoint = WGPUStringView{ "fs_main", 7 };
        fragState.targetCount = 1;
        fragState.targets = &colorTarget;

        WGPURenderPipelineDescriptor pDesc{};
        pDesc.vertex.module = sm;
        pDesc.vertex.entryPoint = WGPUStringView{ "vs_main", 7 };
        pDesc.fragment = &fragState;
        pDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pDesc.multisample.count = 1;
        pDesc.multisample.mask = ~0u;

        pipeline = wgpuDeviceCreateRenderPipeline(device, &pDesc);
        wgpuShaderModuleRelease(sm);

        if (!pipeline) {
            cleanupGpu();
            return false;
        }

        // Hardware Bilinear Sampler
        WGPUSamplerDescriptor sampDesc{};
        sampDesc.addressModeU = WGPUAddressMode_ClampToEdge;
        sampDesc.addressModeV = WGPUAddressMode_ClampToEdge;
        sampDesc.addressModeW = WGPUAddressMode_ClampToEdge;
        sampDesc.magFilter = WGPUFilterMode_Linear;
        sampDesc.minFilter = WGPUFilterMode_Linear;
        sampDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
        sampDesc.maxAnisotropy = 1;
        dawSampler = wgpuDeviceCreateSampler(device, &sampDesc);

        // Uniform Buffer
        WGPUBufferDescriptor bDesc{};
        bDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        bDesc.size = sizeof(GpuCrtUniforms);
        uniformBuffer = wgpuDeviceCreateBuffer(device, &bDesc);

        // Allocate DAW Texture
        reallocateDawTexture(width, height);

        WGPUAdapterInfo info{};
        wgpuAdapterGetInfo(adapter, &info);
        std::cout << "[DawnBridge] Google Dawn / WebGPU hardware pipeline initialized on " 
                  << (info.device.data ? info.device.data : "GPU") 
                  << " (" << width << "x" << height << ")" << std::endl;

        gpuReady = true;
        return true;
#else
        (void)windowHandle;
        (void)width;
        (void)height;
        return false;
#endif
    }

    void reallocateDawTexture(uint32_t width, uint32_t height) {
        if (!device || width == 0 || height == 0) return;

        if (bindGroup) {
            wgpuBindGroupRelease(bindGroup);
            bindGroup = nullptr;
        }
        if (dawView) {
            wgpuTextureViewRelease(dawView);
            dawView = nullptr;
        }
        if (dawTexture) {
            wgpuTextureDestroy(dawTexture);
            wgpuTextureRelease(dawTexture);
            dawTexture = nullptr;
        }

        currentWidth = width;
        currentHeight = height;

        WGPUTextureDescriptor tDesc{};
        tDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        tDesc.dimension = WGPUTextureDimension_2D;
        tDesc.size = WGPUExtent3D{ width, height, 1 };
        tDesc.format = WGPUTextureFormat_BGRA8Unorm;
        tDesc.mipLevelCount = 1;
        tDesc.sampleCount = 1;
        dawTexture = wgpuDeviceCreateTexture(device, &tDesc);
        dawView = wgpuTextureCreateView(dawTexture, nullptr);

        WGPUBindGroupEntry entries[3]{};
        entries[0].binding = 0;
        entries[0].textureView = dawView;
        entries[1].binding = 1;
        entries[1].sampler = dawSampler;
        entries[2].binding = 2;
        entries[2].buffer = uniformBuffer;
        entries[2].size = sizeof(GpuCrtUniforms);

        WGPUBindGroupDescriptor bgDesc{};
        bgDesc.layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);
    }

    void resizeGpu(uint32_t width, uint32_t height) {
        if (!gpuReady || width == 0 || height == 0) return;
        if (width == currentWidth && height == currentHeight) return;

        currentWidth = width;
        currentHeight = height;

        WGPUSurfaceConfiguration sConf{};
        sConf.device = device;
        sConf.format = surfaceFormat;
        sConf.usage = WGPUTextureUsage_RenderAttachment;
        sConf.width = width;
        sConf.height = height;
        sConf.presentMode = WGPUPresentMode_Fifo;
        wgpuSurfaceConfigure(surface, &sConf);

        reallocateDawTexture(width, height);
    }
};

DawnBridge::DawnBridge(uint32_t fboWidth, uint32_t fboHeight)
    : fboWidth_(fboWidth), fboHeight_(fboHeight), pImpl_(std::make_unique<Impl>()) {
}

DawnBridge::~DawnBridge() {
    shutdownNative();
}

bool DawnBridge::initialize(uint32_t width, uint32_t height) {
    fboWidth_ = width;
    fboHeight_ = height;
    if (pImpl_) {
        pImpl_->currentWidth = width;
        pImpl_->currentHeight = height;
    }
    initialized_ = true;
    return true;
}

bool DawnBridge::initializeNative(void* windowHandle, uint32_t width, uint32_t height) {
    initialize(width, height);
    nativeWindowHandle_ = windowHandle;

    if (pImpl_) {
        if (pImpl_->initGpu(windowHandle, width, height)) {
            nativeActive_ = true;
            return true;
        }
    }

    nativeActive_ = true;
    return true;
}

void DawnBridge::shutdownNative() {
    if (pImpl_) {
        pImpl_->cleanupGpu();
    }
    nativeActive_ = false;
    initialized_ = false;
}

void DawnBridge::resize(uint32_t width, uint32_t height) {
    fboWidth_ = width;
    fboHeight_ = height;
    if (pImpl_) {
        pImpl_->resizeGpu(width, height);
    }
}

bool DawnBridge::isGpuAccelerated() const noexcept {
    return pImpl_ && pImpl_->gpuReady;
}

void DawnBridge::renderCrtScene(const uint32_t* dawPixelBuffer, uint32_t dawWidth, uint32_t dawHeight, float lampTime, float subBassEnergy) {
    if (!nativeActive_ || !dawPixelBuffer || dawWidth == 0 || dawHeight == 0) return;

    framesRendered_++;

    if (pImpl_ && pImpl_->gpuReady) {
        pImpl_->checkAndReloadShader();
        pImpl_->resizeGpu(dawWidth, dawHeight);

        // 1. Direct hardware texture upload
        WGPUTexelCopyTextureInfo dstInfo{};
        dstInfo.texture = pImpl_->dawTexture;
        dstInfo.mipLevel = 0;
        dstInfo.origin = WGPUOrigin3D{ 0, 0, 0 };
        dstInfo.aspect = WGPUTextureAspect_All;

        WGPUTexelCopyBufferLayout layout{};
        layout.offset = 0;
        layout.bytesPerRow = dawWidth * sizeof(uint32_t);
        layout.rowsPerImage = dawHeight;

        WGPUExtent3D writeSize{ dawWidth, dawHeight, 1 };
        wgpuQueueWriteTexture(
            pImpl_->queue, &dstInfo,
            dawPixelBuffer,
            static_cast<size_t>(dawWidth) * dawHeight * sizeof(uint32_t),
            &layout, &writeSize
        );

        // 2. Precalculate uniforms on CPU (zero per-fragment trigonometric/hash overhead)
        GpuCrtUniforms uniforms{};
        uniforms.resolution[0] = static_cast<float>(dawWidth);
        uniforms.resolution[1] = static_cast<float>(dawHeight);

        const float topCut = (dawHeight > 0) ? (static_cast<float>(matConfig_.topBarHeightPx) / static_cast<float>(dawHeight)) : 0.07f;
        const float bottomCut = (dawHeight > 0) ? (static_cast<float>(matConfig_.bottomBarHeightPx) / static_cast<float>(dawHeight)) : 0.06f;

        uniforms.topBarFraction = topCut;
        uniforms.bottomBarFraction = bottomCut;
        uniforms.iTime = lampTime;
        uniforms.curvature = matConfig_.curvature;
        uniforms.scanlineIntensity = matConfig_.scanlineIntensity;
        uniforms.crtEnabled = crtShaderEnabled_ ? 1.0f : 0.0f;

        const bool isSrgbTarget = (pImpl_->surfaceFormat == WGPUTextureFormat_BGRA8UnormSrgb ||
                                   pImpl_->surfaceFormat == WGPUTextureFormat_RGBA8UnormSrgb);
        uniforms.gammaCorrection = isSrgbTarget ? 2.2f : 1.0f;

        // Dynamic overhead swaying incandescent studio spotlight (smooth, balanced sweep)
        const float lightX = 0.5f + std::sin(lampTime * 1.5f) * 0.35f;
        uniforms.lightPos[0] = lightX;
        uniforms.lightPos[1] = 0.28f;

        // Real-time audio-reactive sub-bass mechanical rumble
        // Deep kicks, 808s, and acid basslines dynamically shake the chassis and cause diegetic PSU power sag!
        float rumbleStrength = 0.0f;
        if (subBassEnergy > 0.03f) {
            float norm = std::clamp((subBassEnergy - 0.03f) / 0.65f, 0.0f, 1.0f);
            rumbleStrength = norm * norm; // punchy quadratic onset
        }
        if (rumbleStrength > 0.001f) {
            uniforms.rumbleDim = 0.055f * rumbleStrength;
            uniforms.rumbleOffset[0] = std::sin(lampTime * 37.0f + 0.3f) * std::cos(lampTime * 23.0f) * rumbleStrength * 0.0035f;
            uniforms.rumbleOffset[1] = std::cos(lampTime * 31.0f - 0.7f) * std::sin(lampTime * 19.0f) * rumbleStrength * 0.0028f;
        } else {
            uniforms.rumbleDim = 0.0f;
            uniforms.rumbleOffset[0] = 0.0f;
            uniforms.rumbleOffset[1] = 0.0f;
        }

        // Horizontal sync scan wave
        float hCycle = 2.0f + std::fmod(std::abs(std::sin(std::floor(lampTime / 2.0f) * 12345.67f) * 43758.5453f), 1.0f);
        float hPhase = std::fmod(lampTime, hCycle);
        if (hPhase < 0.15f) {
            float hWaveNorm = hPhase / 0.15f;
            uniforms.hWaveStrength = std::sin(hWaveNorm * 3.14159265f) * 0.0035f;
        } else {
            uniforms.hWaveStrength = 0.0f;
        }

        wgpuQueueWriteBuffer(pImpl_->queue, pImpl_->uniformBuffer, 0, &uniforms, sizeof(uniforms));

        // 3. Acquire next swapchain surface texture
        WGPUSurfaceTexture surfTex{};
        wgpuSurfaceGetCurrentTexture(pImpl_->surface, &surfTex);
        if (surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
            surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
            return;
        }

        WGPUTextureView targetView = wgpuTextureCreateView(surfTex.texture, nullptr);

        // 4. Encode full-screen post-processing triangle pass
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(pImpl_->device, nullptr);
        WGPURenderPassColorAttachment ca{};
        ca.view = targetView;
        ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        ca.loadOp = WGPULoadOp_Clear;
        ca.storeOp = WGPUStoreOp_Store;
        ca.clearValue = WGPUColor{ 0.0, 0.0, 0.0, 1.0 };

        WGPURenderPassDescriptor rpDesc{};
        rpDesc.colorAttachmentCount = 1;
        rpDesc.colorAttachments = &ca;

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rpDesc);
        wgpuRenderPassEncoderSetPipeline(pass, pImpl_->pipeline);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, pImpl_->bindGroup, 0, nullptr);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);

        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
        wgpuCommandEncoderRelease(enc);

        // 5. Submit to GPU & present surface
        wgpuQueueSubmit(pImpl_->queue, 1, &cmd);
        wgpuCommandBufferRelease(cmd);

        wgpuSurfacePresent(pImpl_->surface);

        wgpuTextureViewRelease(targetView);
        wgpuTextureRelease(surfTex.texture);
    }
}

void DawnBridge::beginFrame(float /*width*/, float /*height*/, float /*pixelRatio*/) {
}

void DawnBridge::submitBatch(const VectorVertex2D* /*vertices*/, size_t vCount,
                             const uint32_t* /*indices*/, size_t /*iCount*/,
                             const VectorDrawCall& /*call*/) {
    totalVertices_ += vCount;
}

void DawnBridge::endFrame() {
    framesRendered_++;
}

const char* DawnBridge::getShaderSource() noexcept {
    return kCrtWgslSource;
}

} // namespace eatsbits::ui
