#include "eatsbits/ui/dawn_bridge.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <vector>
#include "stb_image.h"
#include "crt_reflection_data.hpp"

#include <webgpu/webgpu.h>
#if !defined(__EMSCRIPTEN__)
#include <webgpu/wgpu.h>
#else
#include <emscripten.h>
#include <emscripten/html5.h>
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
    spotlightIntensity: f32,     // (offset 60)
    spotlightSize: f32,          // (offset 64)
    frameReflectLevel: f32,      // (offset 68)
    vignetteLevel: f32,          // (offset 72)
    panelSoftness: f32,          // (offset 76)
    panelSaturation: f32,        // (offset 80)
    panelBlackLift: f32,         // (offset 84)
    crtReflectionLevel: f32,     // (offset 88) 0.0 = off, 1.0 = full reflection image
    hsyncDistortion: f32,        // (offset 92) 0.0 = static/no h-sync wobble, 1.0 = normal (total 96 bytes)
};

@group(0) @binding(0) var dawTexture: texture_2d<f32>;
@group(0) @binding(1) var dawSampler: sampler;
@group(0) @binding(2) var<uniform> u: CrtUniforms;
@group(0) @binding(3) var reflectionTexture: texture_2d<f32>;

// Multi-tap frosted blur sampling of realistic background room reflection
fn sampleFrostedReflection(uv: vec2<f32>) -> vec3<f32> {
    let clampedUV = clamp(uv, vec2<f32>(0.002), vec2<f32>(0.998));
    let blur = vec2<f32>(2.4 / 426.0, 2.4 / 240.0);
    var col = textureSampleLevel(reflectionTexture, dawSampler, clampedUV, 0.0).rgb * 0.32;
    col += textureSampleLevel(reflectionTexture, dawSampler, clampedUV + vec2<f32>( blur.x,  0.0), 0.0).rgb * 0.17;
    col += textureSampleLevel(reflectionTexture, dawSampler, clampedUV - vec2<f32>( blur.x,  0.0), 0.0).rgb * 0.17;
    col += textureSampleLevel(reflectionTexture, dawSampler, clampedUV + vec2<f32>( 0.0,  blur.y), 0.0).rgb * 0.17;
    col += textureSampleLevel(reflectionTexture, dawSampler, clampedUV - vec2<f32>( 0.0,  blur.y), 0.0).rgb * 0.17;
    return col;
}

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

// Color conversion matching original Apocalypse CRT
fn hsl2rgb(c: vec3<f32>) -> vec3<f32> {
    let K = vec4<f32>(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    let p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, vec3<f32>(0.0), vec3<f32>(1.0)), c.y);
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
// Noticeable, atmospheric light cone with natural spherical falloff & user controls
fn calculateLightFactor(uv: vec2<f32>, lightPos: vec2<f32>, spotSize: f32, spotIntensity: f32) -> f32 {
    var lightDelta = uv - lightPos;
    lightDelta.x *= 1.35; // slight horizontal oval beam matching original
    let dist = length(lightDelta);
    let beamRadius = max(0.5, spotSize);
    let spot = pow(clamp(1.0 - (dist / beamRadius), 0.0, 1.0), 1.1);
    let intensity = clamp(spotIntensity, 0.0, 2.5);
    let minLight = mix(1.0, 0.68, intensity);
    let maxLight = mix(1.0, 1.32, intensity);
    return mix(minLight, maxLight, spot);
}

// Procedural 2D hash for micro-scratches and surface imperfections
fn hash21(p: vec2<f32>) -> f32 {
    var p3 = fract(vec3<f32>(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Smooth macroscopic surface curvature for normal-mapped spotlight interaction
// Zero high-frequency noise so it preserves the underlying 2D GUI elements cleanly
fn getChassisHeight(px: vec2<f32>) -> f32 {
    let w1 = sin(px.x * 0.012) * cos(px.y * 0.025);
    let w2 = sin(px.x * 0.025 + px.y * 0.014) * 0.4;
    return (w1 + w2) * 0.5;
}

// Heavy Industrial Weathered Chassis Shader with Dynamic Bump & Normal Mapping
fn evaluateChassisLighting(uv: vec2<f32>, res: vec2<f32>, lightPos: vec2<f32>, baseCol: vec3<f32>) -> vec3<f32> {
    let lum = dot(baseCol, vec3<f32>(0.299, 0.587, 0.114));
    let metalMask = 1.0 - smoothstep(0.24, 0.40, lum);

    let px = uv * res;
    let eps = 4.0;
    let hL = getChassisHeight(px - vec2<f32>(eps, 0.0));
    let hR = getChassisHeight(px + vec2<f32>(eps, 0.0));
    let hD = getChassisHeight(px - vec2<f32>(0.0, eps));
    let hU = getChassisHeight(px + vec2<f32>(0.0, eps));
    let norm = normalize(vec3<f32>((hL - hR) * 0.45, (hD - hU) * 0.45, 1.0));

    let lightDelta = uv - lightPos;
    let lightDir = normalize(vec3<f32>(-lightDelta.x * 1.5, -lightDelta.y * 2.0, 0.45));
    let nDotL = max(dot(norm, lightDir), 0.0);
    let bumpFactor = mix(0.94, 1.06, nDotL);

    let anisoSheen = pow(clamp(1.0 - abs(lightDelta.y) * 8.5, 0.0, 1.0), 2.2) *
                     pow(clamp(1.0 - abs(lightDelta.x) * 0.65, 0.0, 1.0), 1.2) * 0.045;

    return mix(baseCol, baseCol * bumpFactor + vec3<f32>(anisoSheen), metalMask);
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

        // Subtle sub-pixel blur so physical hardware panel graphics blend realistically (no razor-sharp vector edges)
        let pRadius = max(u.panelSoftness, 0.0);
        let pBlur = vec2<f32>(pRadius / u.resolution.x, pRadius / u.resolution.y);
        var panelCol = textureSampleLevel(dawTexture, dawSampler, sampleUV, 0.0).rgb * 0.36;
        panelCol += textureSampleLevel(dawTexture, dawSampler, sampleUV + vec2<f32>( pBlur.x, 0.0), 0.0).rgb * 0.16;
        panelCol += textureSampleLevel(dawTexture, dawSampler, sampleUV - vec2<f32>( pBlur.x, 0.0), 0.0).rgb * 0.16;
        panelCol += textureSampleLevel(dawTexture, dawSampler, sampleUV + vec2<f32>( 0.0, pBlur.y), 0.0).rgb * 0.16;
        panelCol += textureSampleLevel(dawTexture, dawSampler, sampleUV - vec2<f32>( 0.0, pBlur.y), 0.0).rgb * 0.16;

        // Reduce saturation (default 70%) to give hardware panels authentic industrial anodized feel
        let pLum = dot(panelCol, vec3<f32>(0.299, 0.587, 0.114));
        panelCol = mix(vec3<f32>(pLum), panelCol, clamp(u.panelSaturation, 0.0, 1.0));

        // Soften aggressive pitch-black lines/borders by gently lifting the black floor
        let blackLift = clamp(u.panelBlackLift, 0.0, 0.10);
        panelCol = mix(panelCol, max(panelCol, vec3<f32>(blackLift)), 0.75);

        let panelLight = calculateLightFactor(globalUV, u.lightPos, u.spotlightSize, u.spotlightIntensity);

        // Apply normal bump map & dynamic specular micro-glints from swaying spotlight
        panelCol = evaluateChassisLighting(globalUV, u.resolution, u.lightPos, panelCol);
        panelCol *= panelLight;
        panelCol -= vec3<f32>(u.rumbleDim);

        let lum = dot(panelCol, vec3<f32>(0.299, 0.587, 0.114));
        let metalMask = 1.0 - smoothstep(0.24, 0.40, lum);

        // Top Panel Top Lip: Specular bevel highlight along window ceiling
        if (globalUV.y < 0.004) {
            let topLip = smoothstep(0.004, 0.0, globalUV.y) * 0.16;
            panelCol += vec3<f32>(topLip) * metalMask;
        }

        // Top Panel Bottom Lip: Uniform machined chassis recession shadow & subtle seam bevel
        if (globalUV.y < topCut) {
            let distToSeam = topCut - globalUV.y;
            let distPx = distToSeam * u.resolution.y;
            if (distPx < 3.5) {
                let seamShadow = smoothstep(0.0, 3.5, distPx);
                panelCol *= mix(0.40, 1.0, seamShadow);
            }
        }

        // Bottom Panel Top Lip: Uniform machined chassis seam shadow & subtle specular highlight
        if (globalUV.y > bottomCut) {
            let distFromSeam = globalUV.y - bottomCut;
            let distPx = distFromSeam * u.resolution.y;
            if (distPx < 3.5) {
                let lipHighlight = smoothstep(3.0, 1.2, distPx) * smoothstep(0.2, 1.2, distPx) * 0.15 * panelLight;
                panelCol += vec3<f32>(lipHighlight) * metalMask;
            }
        }

        // Bottom Panel Bottom Lip: Soft chassis recession shadow
        if (globalUV.y > 0.995) {
            let botLip = smoothstep(0.995, 1.0, globalUV.y) * 0.12;
            panelCol -= vec3<f32>(botLip) * metalMask;
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
    if (u.hWaveStrength > 0.00001 && u.hsyncDistortion > 0.001) {
        hWave = sin(suv.y * 10.0 + u.iTime * 5.0) * (u.hWaveStrength * u.hsyncDistortion);
    }

    // Outer chassis frame dimensions: extends to panel edges and screen left/right borders
    // with rounded inner tube aperture and a realistic outer seam gap
    let screenRes = vec2<f32>(u.resolution.x, u.resolution.y * span);
    let frameWidthX_px = 22.0;
    let frameHeightY_px = 20.0;
    let cornerRadius_px = 20.0;

    let pCenter = abs(suv - vec2<f32>(0.5)) * screenRes;
    let sgn = sign(suv - vec2<f32>(0.5));
    let halfInner = vec2<f32>(0.5 * screenRes.x - frameWidthX_px, 0.5 * screenRes.y - frameHeightY_px);
    let q = pCenter - halfInner + vec2<f32>(cornerRadius_px);
    let dTube = length(max(q, vec2<f32>(0.0))) + min(max(q.x, q.y), 0.0) - cornerRadius_px;

    let isFrame = (dTube > 0.0);
    let crtLightFactor = calculateLightFactor(suv, u.lightPos, u.spotlightSize, u.spotlightIntensity);

    if (isFrame) {
        // Distance from panel edges (top transport, bottom chin, left/right window edges)
        let distEdgeX = min(suv.x, 1.0 - suv.x) * screenRes.x;
        let distEdgeY = min(suv.y, 1.0 - suv.y) * screenRes.y;
        let distToPanelEdge = min(distEdgeX, distEdgeY);

        // REVERSE VIGNETTE & BEVEL CAVITY SHADING:
        // The frame slopes down into the recessed CRT aperture cavity.
        // It gets darker as it progresses inward toward the CRT (dTube -> 0),
        // and brighter towards the elevated outer chassis edge (dTube -> frameWidthX_px).
        let bevelDist = clamp(dTube / frameWidthX_px, 0.0, 1.0);
        let intensity = mix(0.015, 0.055, bevelDist);

        // Compute exact outward normal vector of the tube aperture in screen pixel space.
        // In the rounded corners (q.x > 0 && q.y > 0), the normal smoothly rotates radially.
        // Along flat edges, it points strictly axis-aligned.
        var normPixel = vec2<f32>(0.0);
        if (q.x > 0.0 && q.y > 0.0) {
            normPixel = normalize(q) * sgn;
        } else if (q.x > q.y) {
            normPixel = vec2<f32>(sgn.x, 0.0);
        } else {
            normPixel = vec2<f32>(0.0, sgn.y);
        }

        // Mirror coordinates across the continuous tube boundary along the surface normal.
        // At dTube == 0 (boundary), reflPixel == currentPixel.
        // Inside the frame, it mirrors into the CRT tube along the normal, exactly matching
        // the active display in the center, along all edges, and through the rounded corners.
        let currentPixel = suv * screenRes;
        let reflPixel = currentPixel - 2.0 * dTube * normPixel;

        let frameFrac = vec2<f32>(frameWidthX_px, frameHeightY_px) / screenRes;
        let reflPos = reflPixel / screenRes;
        var reflTubeUV = (reflPos - frameFrac) / (vec2<f32>(1.0) - 2.0 * frameFrac);
        reflTubeUV = clamp(reflTubeUV, vec2<f32>(0.001), vec2<f32>(0.999));

        // Apply identical CRT curvature to the reflected coordinate so reflections
        // never deviate from the curved phosphor raster at any point across the display
        var curvedRefl = reflTubeUV;
        if (u.curvature > 0.001) {
            let center = vec2<f32>(0.5, 0.5);
            let dCenter = curvedRefl - center;
            let dist = length(dCenter);
            curvedRefl += dCenter * pow(dist, 2.6) * (u.curvature * 0.08);
        }

        // Apply horizontal sync scan wave to reflected coordinate
        let sampleCoord = vec2<f32>(
            clamp(curvedRefl.x + hWave, 0.001, 0.999),
            topCut + clamp(curvedRefl.y, 0.001, 0.999) * span
        );

        // Multi-tap frosted blur sampling of mirrored screen content (frameBlur)
        let blurX = 3.5 / u.resolution.x;
        let blurY = 3.5 / (u.resolution.y * span);
        var blurred = textureSampleLevel(dawTexture, dawSampler, sampleCoord, 0.0).rgb * 0.36;
        blurred += textureSampleLevel(dawTexture, dawSampler, sampleCoord + vec2<f32>( blurX,  0.0), 0.0).rgb * 0.16;
        blurred += textureSampleLevel(dawTexture, dawSampler, sampleCoord - vec2<f32>( blurX,  0.0), 0.0).rgb * 0.16;
        blurred += textureSampleLevel(dawTexture, dawSampler, sampleCoord + vec2<f32>( 0.0,  blurY), 0.0).rgb * 0.16;
        blurred += textureSampleLevel(dawTexture, dawSampler, sampleCoord - vec2<f32>( 0.0,  blurY), 0.0).rgb * 0.16;

        // Catch room environmental reflection in the glossy frame glass as well
        let roomRefl = sampleFrostedReflection(curvedRefl);
        blurred += roomRefl * 0.45;

        // Apocalypse CRT base gunmetal steel material with micro-grain
        var frameCol = hsl2rgb(vec3<f32>(0.025, 0.10, intensity));
        let grain = hash21(suv * u.resolution);
        frameCol *= (1.0 - 0.15 * grain);

        // Highly reflective mirrored frame (Apocalypse CRT frameReflect: adjustable)
        frameCol += blurred * u.frameReflectLevel;

        // REVERSE VIGNETTE:
        // Darkens as it progresses toward the CRT tube aperture,
        // pulling the frame into the deep 3D shadow recess cavity.
        let reverseVignette = mix(0.40, 1.0, pow(bevelDist, 0.85));
        frameCol *= reverseVignette;

        // AUTHENTIC CONFORMAL VIGNETTE:
        // 2D vignette across the chassis that deepens naturally in the rounded corners
        // ("stronger vignettes at the centers of the rounded areas"), producing
        // organic, sculpted corner depth.
        let vig = suv * (vec2<f32>(1.0) - suv.yx);
        let cornerVignette = clamp(pow(vig.x * vig.y * 18.0, 0.28 * u.vignetteLevel), 0.0, 1.0);
        frameCol *= cornerVignette;

        // SLIGHT EDGE GAP FOR REALISM:
        // Deep ~2px shadow seam separating frame from panels, with subtle outer lip glint
        let seamShadow = smoothstep(0.0, 2.0, distToPanelEdge);
        frameCol *= mix(0.25, 1.0, seamShadow);
        let seamGlint = smoothstep(3.2, 1.8, distToPanelEdge) * smoothstep(0.8, 1.8, distToPanelEdge) * 0.28 * crtLightFactor;
        frameCol += vec3<f32>(seamGlint);

        // Overhead swaying spotlight illumination
        frameCol *= crtLightFactor;

        frameCol -= vec3<f32>(u.rumbleDim);

        var finalFrame = clamp(frameCol, vec3<f32>(0.0), vec3<f32>(1.0));
        if (u.gammaCorrection > 1.05) {
            finalFrame = pow(finalFrame, vec3<f32>(u.gammaCorrection));
        }
        return vec4<f32>(finalFrame, 1.0);
    }
)"
R"(
    // ACTIVE PHOSPHOR RASTER DISPLAY:
    let frameFracX = frameWidthX_px / screenRes.x;
    let frameFracY = frameHeightY_px / screenRes.y;
    let tubeUV = (suv - vec2<f32>(frameFracX, frameFracY)) / (vec2<f32>(1.0) - 2.0 * vec2<f32>(frameFracX, frameFracY));

    // Subtle CRT bulb curvature within the rounded tube aperture
    var curvedTube = tubeUV;
    if (u.curvature > 0.001) {
        let center = vec2<f32>(0.5, 0.5);
        let dCenter = curvedTube - center;
        let dist = length(dCenter);
        curvedTube += dCenter * pow(dist, 2.6) * (u.curvature * 0.08);
    }

    // Screen raster content wobbles horizontally with hWave
    let texUV = vec2<f32>(
        clamp(curvedTube.x + hWave, 0.001, 0.999),
        topCut + clamp(curvedTube.y, 0.001, 0.999) * span
    );

    // Subtle RGB chromatic fringing
    var color = sampleRgbDistortion(texUV, 0.0007);

    // Bezel-to-tube inner junction drop shadow (hugging the rounded corner contour)
    let innerShadow = smoothstep(0.0, -3.5, dTube);
    color *= mix(0.78, 1.0, innerShadow);

    // Crisp raster scanlines with preserved contrast (can be disabled with scanlineIntensity = 0.0)
    let scanIntensity = u.scanlineIntensity;
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
    let vig = clamp(pow(vigUV.x * vigUV.y * 14.0, 0.16 * u.vignetteLevel), 0.0, 1.0);
    color *= vig;

    // Ambient Environmental Reflection: Vectorized Atmospheric Room with Backlit Eatsbits Window
    // Simulates realistic room composition reflection on curved CRT bulb glass
    var reflUV = curvedTube;
    let reflSample = sampleFrostedReflection(reflUV);
    let tubeEdgeFade = smoothstep(0.0, 0.08, tubeUV.x) * smoothstep(1.0, 0.92, tubeUV.x) *
                       smoothstep(0.0, 0.08, tubeUV.y) * smoothstep(1.0, 0.92, tubeUV.y);
    let reflectionGlow = reflSample * tubeEdgeFade * (0.125 * u.crtReflectionLevel);
    color += reflectionGlow;

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

// Strictly 96-byte aligned uniform buffer matching WGSL layout
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
    float spotlightIntensity;// offset 60
    float spotlightSize;     // offset 64
    float frameReflectLevel; // offset 68
    float vignetteLevel;     // offset 72
    float panelSoftness;     // offset 76
    float panelSaturation;   // offset 80
    float panelBlackLift;    // offset 84
    float crtReflectionLevel; // offset 88
    float hsyncDistortion;   // offset 92 -> total 96 bytes
};
static_assert(sizeof(GpuCrtUniforms) == 96, "GpuCrtUniforms must be 96 bytes");

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
    WGPUTexture reflectionTexture{nullptr};
    WGPUTextureView reflectionView{nullptr};
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
            reallocateDawTexture(currentWidth, currentHeight);
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
        if (reflectionView) {
            wgpuTextureViewRelease(reflectionView);
            reflectionView = nullptr;
        }
        if (reflectionTexture) {
            wgpuTextureDestroy(reflectionTexture);
            wgpuTextureRelease(reflectionTexture);
            reflectionTexture = nullptr;
        }
        if (pipeline) {
            wgpuRenderPipelineRelease(pipeline);
            pipeline = nullptr;
        }
        if (surface) {
#if !defined(__EMSCRIPTEN__)
            wgpuSurfaceRelease(surface);
#endif
            surface = nullptr;
        }
        if (queue) {
            wgpuQueueRelease(queue);
            queue = nullptr;
        }
        if (device) {
#if !defined(__EMSCRIPTEN__)
            wgpuDeviceDestroy(device);
            wgpuDeviceRelease(device);
#endif
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

    bool finishGpuInit(uint32_t width, uint32_t height) {
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

        // Load realistic room vector reflection texture (426x240)
        int reflW = 0;
        int reflH = 0;
        int channels = 0;
        uint8_t* reflPixels = stbi_load("assets/images/crt_reflection_bg.png", &reflW, &reflH, &channels, 4);
        if (!reflPixels) {
            reflPixels = stbi_load_from_memory(kDefaultReflectionPng, static_cast<int>(kDefaultReflectionPngSize), &reflW, &reflH, &channels, 4);
        }

        uint32_t texW = (reflPixels && reflW > 0) ? static_cast<uint32_t>(reflW) : 426u;
        uint32_t texH = (reflPixels && reflH > 0) ? static_cast<uint32_t>(reflH) : 240u;

        WGPUTextureDescriptor rDesc{};
        rDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        rDesc.dimension = WGPUTextureDimension_2D;
        rDesc.size = WGPUExtent3D{ texW, texH, 1 };
        rDesc.format = WGPUTextureFormat_RGBA8Unorm;
        rDesc.mipLevelCount = 1;
        rDesc.sampleCount = 1;
        reflectionTexture = wgpuDeviceCreateTexture(device, &rDesc);
        if (reflectionTexture) {
            reflectionView = wgpuTextureCreateView(reflectionTexture, nullptr);

            WGPUTexelCopyTextureInfo rDstInfo{};
            rDstInfo.texture = reflectionTexture;
            rDstInfo.mipLevel = 0;
            rDstInfo.origin = WGPUOrigin3D{ 0, 0, 0 };
            rDstInfo.aspect = WGPUTextureAspect_All;

            WGPUTexelCopyBufferLayout rLayout{};
            rLayout.offset = 0;
            rLayout.bytesPerRow = texW * 4;
            rLayout.rowsPerImage = texH;

            WGPUExtent3D rWriteSize{ texW, texH, 1 };

            if (reflPixels) {
                wgpuQueueWriteTexture(
                    queue, &rDstInfo,
                    reflPixels,
                    static_cast<size_t>(texW) * texH * 4,
                    &rLayout, &rWriteSize
                );
            } else {
                std::vector<uint32_t> fallback(texW * texH, 0xFF080604);
                wgpuQueueWriteTexture(
                    queue, &rDstInfo,
                    fallback.data(),
                    fallback.size() * sizeof(uint32_t),
                    &rLayout, &rWriteSize
                );
            }
        }
        if (reflPixels) {
            stbi_image_free(reflPixels);
            reflPixels = nullptr;
        }

        // Allocate DAW Texture with RenderAttachment usage for in-GPU direct rendering
        reallocateDawTexture(width, height);

        gpuReady = true;
        return true;
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

        if (!finishGpuInit(width, height)) {
            return false;
        }

        WGPUAdapterInfo info{};
        wgpuAdapterGetInfo(adapter, &info);
        std::cout << "[DawnBridge] Google Dawn / WebGPU hardware pipeline initialized on " 
                  << (info.device.data ? info.device.data : "GPU") 
                  << " (" << width << "x" << height << ")" << std::endl;

        return true;
#else
        (void)windowHandle;
        (void)width;
        (void)height;
        return false;
#endif
    }

    bool initGpuWeb(WGPUDevice webDevice, WGPUSurface webSurface, uint32_t width, uint32_t height) {
        if (!webDevice || !webSurface || width == 0 || height == 0) return false;

        device = webDevice;
        surface = webSurface;
        queue = wgpuDeviceGetQueue(device);
        currentWidth = width;
        currentHeight = height;

        surfaceFormat = WGPUTextureFormat_BGRA8Unorm;

        WGPUSurfaceConfiguration sConf{};
        sConf.device = device;
        sConf.format = surfaceFormat;
        sConf.usage = WGPUTextureUsage_RenderAttachment;
#if defined(__EMSCRIPTEN__)
        sConf.alphaMode = WGPUCompositeAlphaMode_Auto;
#endif
        sConf.width = width;
        sConf.height = height;
        sConf.presentMode = WGPUPresentMode_Fifo;
        wgpuSurfaceConfigure(surface, &sConf);

        if (!finishGpuInit(width, height)) {
            return false;
        }

        std::cout << "[DawnBridge] Google Dawn / WebGPU hardware pipeline initialized on Web ("
                  << width << "x" << height << ")" << std::endl;
        return true;
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
        tDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_RenderAttachment;
        tDesc.dimension = WGPUTextureDimension_2D;
        tDesc.size = WGPUExtent3D{ width, height, 1 };
        tDesc.format = WGPUTextureFormat_BGRA8Unorm;
        tDesc.mipLevelCount = 1;
        tDesc.sampleCount = 1;
        dawTexture = wgpuDeviceCreateTexture(device, &tDesc);
        dawView = wgpuTextureCreateView(dawTexture, nullptr);

        WGPUBindGroupEntry entries[4]{};
        entries[0].binding = 0;
        entries[0].textureView = dawView;
        entries[1].binding = 1;
        entries[1].sampler = dawSampler;
        entries[2].binding = 2;
        entries[2].buffer = uniformBuffer;
        entries[2].size = sizeof(GpuCrtUniforms);
        entries[3].binding = 3;
        entries[3].textureView = reflectionView ? reflectionView : dawView;

        WGPUBindGroupDescriptor bgDesc{};
        bgDesc.layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
        bgDesc.entryCount = 4;
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
#if defined(__EMSCRIPTEN__)
        sConf.alphaMode = WGPUCompositeAlphaMode_Auto;
#endif
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

bool DawnBridge::initializeWeb(void* webDevice, void* webSurface, uint32_t width, uint32_t height) {
    initialize(width, height);
    if (!webDevice || !webSurface || width == 0 || height == 0) return false;

    if (pImpl_) {
        if (pImpl_->initGpuWeb(static_cast<WGPUDevice>(webDevice), static_cast<WGPUSurface>(webSurface), width, height)) {
            nativeActive_ = true;
            return true;
        }
    }
    return false;
}

void* DawnBridge::getDawTextureView() const noexcept {
    return pImpl_ ? static_cast<void*>(pImpl_->dawView) : nullptr;
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

void DawnBridge::renderCrtScene(const uint32_t* dawPixelBuffer, uint32_t dawWidth, uint32_t dawHeight, float lampTime, float subBassEnergy, float renderScale) {
    if (renderScale > 0.001f) {
        renderScale_ = renderScale;
    }
#if !defined(__EMSCRIPTEN__)
    if (!nativeActive_ || !dawPixelBuffer || dawWidth == 0 || dawHeight == 0) return;
#else
    if (!nativeActive_ || dawWidth == 0 || dawHeight == 0) return;
#endif

    framesRendered_++;

    if (pImpl_ && pImpl_->gpuReady) {
        pImpl_->checkAndReloadShader();
        pImpl_->resizeGpu(dawWidth, dawHeight);

#if !defined(__EMSCRIPTEN__)
        // 1. Direct hardware texture upload
        if (dawPixelBuffer) {
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
        }
#else
        (void)dawPixelBuffer;
#endif

        // 2. Precalculate uniforms on CPU (zero per-fragment trigonometric/hash overhead)
        GpuCrtUniforms uniforms{};
        uniforms.resolution[0] = static_cast<float>(dawWidth);
        uniforms.resolution[1] = static_cast<float>(dawHeight);

        const float effH = (renderScale_ > 0.001f) ? (static_cast<float>(dawHeight) / renderScale_) : static_cast<float>(dawHeight);
        const float topCut = (effH > 0.0f) ? (static_cast<float>(matConfig_.topBarHeightPx) / effH) : 0.07f;
        const float bottomCut = (effH > 0.0f) ? (static_cast<float>(matConfig_.bottomBarHeightPx) / effH) : 0.06f;

        uniforms.topBarFraction = topCut;
        uniforms.bottomBarFraction = bottomCut;
        uniforms.iTime = lampTime;
        uniforms.curvature = matConfig_.curvature;
        uniforms.scanlineIntensity = matConfig_.scanlineIntensity;
        uniforms.crtEnabled = crtShaderEnabled_ ? 1.0f : 0.0f;

        const bool isSrgbTarget = (pImpl_->surfaceFormat == WGPUTextureFormat_BGRA8UnormSrgb ||
                                   pImpl_->surfaceFormat == WGPUTextureFormat_RGBA8UnormSrgb);
        uniforms.gammaCorrection = isSrgbTarget ? 2.2f : 1.0f;

        uniforms.spotlightIntensity = matConfig_.spotlightEnabled ? matConfig_.spotlightIntensity : 0.0f;
        uniforms.spotlightSize = matConfig_.spotlightSize;
        uniforms.frameReflectLevel = matConfig_.reflectionOpacity;
        uniforms.vignetteLevel = matConfig_.vignetteStrength;
        uniforms.panelSoftness = matConfig_.panelSoftness;
        uniforms.panelSaturation = matConfig_.panelSaturation;
        uniforms.panelBlackLift = matConfig_.panelBlackLift;
        uniforms.crtReflectionLevel = matConfig_.crtReflectionLevel;
        uniforms.hsyncDistortion = matConfig_.hsyncDistortion;

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

        currentRumbleOffsetX_ = uniforms.rumbleOffset[0];
        currentRumbleOffsetY_ = uniforms.rumbleOffset[1];
        currentHWaveStrength_ = uniforms.hWaveStrength;
        currentLampTime_ = lampTime;

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

#if !defined(__EMSCRIPTEN__)
        wgpuSurfacePresent(pImpl_->surface);
#endif

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
