// =============================================================================
// Eatsbits Apocalypse CRT Workstation Partial-Screen Shader (WGSL)
// Applies exclusively to the central DAW workspace (below transport, above chin)
// Top transport bar and bottom navigation bar remain 100% pristine and untouched.
// =============================================================================

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
