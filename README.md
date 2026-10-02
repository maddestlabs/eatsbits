# Eatsbits (C++20 Engine)

> **Next-Generation Native Rebuild of Eatsbeats**  
> *A high-performance, low-latency Digital Audio Workstation and live-coding environment built in pure C++20.*

---

## 🏛️ The Bigger Picture: Beyond the DAW (Engine Core & Vision)

While **Eatsbits** seeks to poise itself as a cutting-edge Digital Audio Workstation, it is architected as the flagship anchor application for a much larger ambition: a **lean, web-first, mobile-first, dual-surface (GUI + TUI) game and application engine**.

We're seeking to prove out the engine's design by applying it against the brutal real-time constraints of hard real-time audio threads, zero-allocation DSP, 120 FPS WebGPU vector rendering, and dual-surface TUI/GUI behavioral parity.

* **Flagship Anchor Application:** Eatsbits DAW (multitrack sequencer, modular synthesizer, and live-coding studio).
* **Universal Scripting Core:** [Eatscript](include/eatsbits/eatscript), a Pythonic DSL featuring both a zero-allocation bytecode VM and an AOT C-ABI transpiler.
* **Dual-Surface MVP Architecture:** Headless [Presenters](include/eatsbits/presenter) driving both desktop/mobile hardware-accelerated vector UI (WebGPU / Dawn) and low-latency terminal TUI ([CellSurface](include/eatsbits/tui/cell_surface.hpp)).
* **GPU-driven Terminal & Eatscript Shell:** A WebGPU-accelerated terminal emulator and structured interactive shell powered by Eatscript.
* **Future OS Foundation:** An operating system built around Eatscript for simplified, readable, API-driven system interaction.

> **Architectural Blueprint & AI Pair-Programming Guide:**  
> To prevent architectural drift and align both human developers and AI pair-programmers (Gemini / Antigravity) with our long-term modular vision, refer to:
> - **[engine-blueprint.md](engine-blueprint.md)**: Master deconstruction roadmap, gap analysis, and AI development guardrails.
> - **[gui-mvp.md](gui-mvp.md)**: Model-View-Presenter (MVP) specification for zero-allocation dual-surface TUI/GUI parity.

---

## 1. Architectural Highlights

- **Zero-Allocation Audio Thread:** Strict real-time safety in the audio callback. No dynamic memory allocations (`malloc`, `new`), lock contention, or garbage collection pauses.
- **Lock-Free Concurrency:** Single-Producer Single-Consumer (`SpscRingBuffer`) wait-free circular queues for NoteOn/NoteOff events, parameter changes, and UI meter feedback.
- **Audio I/O & Drivers:** Cross-platform single-header `miniaudio.h` driving hardware directly via WASAPI, ASIO, CoreAudio, ALSA, and AAudio (WebAssembly / AudioWorklet for web).
- **Dual-Mode Eatscript Engine:**
  - **C++ Bytecode VM:** Flat instruction set for live scripting, instant hot-reloading, and real-time execution with zero runtime heap allocations.
  - **AOT C++ Transpiler:** Transpiles Eatscript Pythonic scripts into standalone C++ code exposing the uniform `EatsPluginDescriptor` C-ABI.
- **DSP Suite:**
  - Authentic Roland TB-303 diode ladder filter and voice state with 4x oversampling, 60ms analog slides, accents, and saturation.
  - Polyphonic 16-voice multi-waveform synthesizer with PolyBLEP anti-aliased oscillators (Sine, Saw, Square, Triangle, Noise) and sample-accurate ADSR envelopes.
  - Direct Form II Transposed Biquad filters with Butterworth/Chebyshev coefficient calculations.
- **Modular DSP Audio Graph & Evaluator:** Directed Acyclic Graph (DAG) pipeline with cycle detection, Kahn topological sorting, multi-source bus summing, and lock-free double-buffered execution plan swapping.
- **Zero-Allocation Buffer Arena:** Pre-allocated planar buffer arena guaranteeing strict zero dynamic memory allocations during graph evaluation on the audio thread.
- **Composable Node Library:**
  - `PhysicalInstrumentNode`: High-performance polyphonic physical modeling instrument source node supporting Stanford CCRMA / Bank-Bensa Concert Grand Piano, Upright Double Bass (with fretboard collision), Spanish Classical Guitar, and Steel Acoustic Guitar.
  - `WaveguideNode`: Modular digital waveguide string resonator node with 1-pole loop loss damping, MPE modulation, and fractional delay.
  - `ModalResonatorNode`: Modular parallel 2nd-order bandpass resonator bank for acoustic bodies, soundboards, and air cavities.
  - `ConvolverNode`: Zero-latency real-time stereo convolution reverb node with physics-based procedural room & cabinet simulation, pre-delay, damping filters, and click-free hot-swapping.
  - `SnesNode`: Super Nintendo S-SMP / SPC700 16-bit S-DSP sound chip with 12 BRR waveforms, 4-point Gaussian edge smoothing, 6 envelope modes (ADSR, Direct, Linear/Exp/Bent GAIN), 15-bit Galois LFSR noise, PMOD cross-channel pitch modulation, and 8-tap programmable FIR stereo echo unit.
  - `Ym2612Node`: Sega Genesis / Mega Drive Yamaha YM2612 (OPN2) 4-operator FM sound chip with 8 algorithms, Operator 1 self-feedback, 0.75dB logarithmic Total Level attenuation, and SFXR procedural patch generator (Laser, Explosion, Powerup, Coin, Jump, Hit).
  - `Dx7Node`: Yamaha DX7 6-Operator Frequency Modulation (FM) synthesis engine with all 32 routing algorithms (MSFA dual-bus architecture), high-resolution quarter-wave sine LUT, operator feedback loops (0..7), 4-rate 4-level envelopes (R1..R4, L1..L4), Keyboard Rate/Level Scaling (KSR/KLS), 64-step velocity scaling, and 8-voice polyphony.
  - `SidNode`: Authentic Commodore 64 SID (`MOS 6581` / `MOS 8580`) 3-voice chiptune sound chip with 23-bit Galois LFSR noise, 12-bit hardware PWM, hard sync, ring modulation, and 12dB/oct resonant SVF filter.
  - `DrumKitNode`: General MIDI percussion kit with authentic analog 808/909 physical drum synthesis.
  - `Tb303Node`: Modular Roland TB-303 diode ladder acid bassline source.
  - `PolySynthNode`: 16-voice PolyBLEP polyphonic synthesizer source.
  - `BiquadNode`: Stereo Direct Form II Transposed multi-mode filter.
  - `DelayNode`: Stereo tape-style echo with feedback and dry/wet mix.
  - `GainNode`: Constant-power panning, volume, and mute stage.
- **DSP Suite:**
  - **Stanford CCRMA / Bank-Bensa Commuted Waveguide Piano & Acoustic Physical Modeling Engine:** Real-time physical modeling suite implementing the Stanford CCRMA / Bank & Bensa (2005) commuted digital waveguide piano synthesis engine with 3-stage allpass inharmonic dispersion filters, 88-key empirical breakpoint tables, coupled trichord delay lines with bridge transfer coupling matrix, 4-stage non-linear felt hammer cascade, strike position notch comb filtering, and 2D spruce soundboard modal decay. Includes the **3/4 Upright Double Bass** (Hunt-Crossley elastodynamic side-finger pull, 4-point Hermite cubic waveguide, 1st-order allpass dispersion, non-linear ebony fingerboard collision solver modeling fretless growl and slap, and 58Hz/98Hz/145Hz carved spruce modal body cavity resonator), and **Spanish Classical / Steel Acoustic Guitars** (plectrum multi-tap comb brush, nylon/bronze string waveguides, and Torres fan-braced modal body resonator banks).
  - **Zero-Latency Convolution Reverb & Procedural IR Simulation Engine:** Pure C++20 real-time convolution engine (`ConvolverCore`) with circular history buffers, 4x unrolled SIMD loops, and strict zero-allocation audio callbacks. Features a complete physics-based **Procedural IR Generator** modeling acoustic materials (wood, concrete, foam, metal, carpet), early binaural reflections via Order $\le 3$ Image Source Method (ISM), late diffuse reverberation via dual-seed Velvet Noise with exponential $RT_{60}$ decay, and guitar/bass speaker cabinet simulation (box standing waves, rear dipole cancellation, and 3-stage biquad speaker impedance filters). Ships with 13 stock space/cabinet presets: *Stone Cathedral*, *Great Hall*, *Studio Live Room*, *Warm Room*, *Small Vocal Booth*, *Tile Bathroom*, *Plate Reverb*, *Spring Tank*, *4x12 Vintage Stack (Closed)*, *2x12 British Celestion*, *1x12 Tweed Combo (Open-Back)*, *Bass 8x10 Fridge*, and *Small Radio Speaker*.
  - **Super Nintendo S-SMP / SPC700 16-Bit S-DSP Engine:** 8 polyphonic channels with 12 single/multi-cycle BRR wavetables, 4-point Gaussian low-pass interpolation, 6 hardware envelope modes, 15-bit Galois LFSR pseudo-random noise generator, cross-channel pitch modulation (`PMOD`), and hardware 8-tap programmable FIR stereo echo buffer with selectable DSP profiles (`surround_reverb`, `dark_hall`, `metallic_chorus`, `slapback`).
  - **Sega Genesis Yamaha YM2612 (OPN2) 4-Op FM Engine:** 6 polyphonic channels with all 8 OPN/OPN2 routing algorithms, Operator 1 self-modulating feedback (levels 0..7) with 2-sample stability averaging, 0.75dB Total Level attenuation curve, pitch sweeps, and integrated SFXR procedural generator for playable retro instrument patches.
  - **Yamaha DX7 6-Operator FM Engine:** Full 32 routing algorithms modeled after the original MSFA dual-bus architecture. Features a 4096-sample sub-sample interpolated quarter-wave sine LUT, 8-level operator feedback loops, 4-stage rate/level envelopes with authentic exponential decay curves, logarithmic Keyboard Rate Scaling (KSR 0..7), Keyboard Level Scaling (KLS breakpoint/curve), 64-step velocity curves, macro timbre shaping (Brightness, Tine, Body Warmth), and 8-voice polyphony with lowest-level voice stealing.
  - **Commodore 64 SID Sound Chip (`MOS 6581 / 8580`):** 3 polyphonic voices, 23-bit Galois LFSR pseudo-random noise generator, 12-bit pulse width modulation with dedicated LFO, oscillator hard sync, ring modulation, hardware ADSR envelope tables matching 6581 divider clock cycles, 50Hz PAL / 60Hz NTSC chiptune arpeggiator, and 12dB/oct Chamberlin State-Variable Filter (SVF) with non-linear FET saturation vs. clean linear responses.
  - **TR-808 & TR-909 Drum Synths:** Bridged-T resonant kicks with pitch sweeps & saturation, dual-shell snares with snappy wire noise, 6-oscillator inharmonic metallic hi-hats with choke logic, 4-burst hand claps, and tuned cowbells.
  - Authentic Roland TB-303 diode ladder filter and voice state with 4x oversampling, 60ms analog slides, accents, and saturation.
  - Polyphonic 16-voice multi-waveform synthesizer with PolyBLEP anti-aliased oscillators (Sine, Saw, Square, Triangle, Noise) and sample-accurate ADSR envelopes.
  - Direct Form II Transposed Biquad filters with Butterworth/Chebyshev coefficient calculations.
  - Master stereo mixer strip with constant-power panning and peak/RMS metering.
- **Interactive Multi-Track Arranger Timeline:** Musical bar ruler with click-and-drag continuous transport playhead scrubbing, loop region markers, dynamic track header cards with interactive Mute, Solo, Volume slider controls, clip selection, and minimap overview scrollbar.
- **Interactive Piano Roll, Tracker & Score Editor:** 2.5-octave virtual piano keyboard with real-time sound auditioning via `AudioEngine::postNoteOn()`, alternating semitone pitch lanes, clickable note block placement, Oswald pitch labels, bottom velocity stalk lane, SMuFL Bravura notation engraving, and sub-view navigation (`PIANO ROLL`, `TRACKER`, `SCORE`, `SCRIPT`).
- **The 5 Pillars of Eatscript DAW Alignment:**
  1. **Declarative Note Scripting & Bi-directional Sync (`EDIT > SCRIPT`):** Canonical pythonic representation of track/clip notes (`notes = [{"pitch": 36, "start": 0.0, "duration": 0.75, "vel": 0.9}]`). Editing in script updates piano roll, tracker, and score instantly; editing graphically updates script text.
  2. **Track Instruments & Dual-Mode SIMD Acceleration (`DESIGN > EATSCRIPT`):** All track instruments are editable Eatscript scripts with dual-mode dispatch: declares native flags (`Eats303 = True`, `JC303 = True`, `Analog808Kick = True`, `SIDSynth = True`, `ym2612 = True`, etc.) or `# @engine: eats_303` metadata for pre-compiled C++ SIMD DSP, or executes custom mathematical oscillators via `eatscript::VM`.
  3. **Scriptable Stereo Audio FX Processing:** Native support for `def process(input_l, input_r, params) -> [out_l, out_r]` inside `eatscript::VM` and `EatscriptNode` streaming real-time stereo audio frames with zero runtime heap allocations.
  4. **Scriptable MIDI FX Pipeline Transformers:** Modular MIDI insert rack (`MidiPipelineEngine`) supporting production-grade Arpeggiation (9 patterns, multi-octave cycling, sub-step rates, swing), Scale Snapping (major/minor), Chord Following (`chord`, `bass`, `scale`, `colorLead`), Organic Humanizing, Semitone Transposing, and Chord Stabs/Voicings.
  5. **DAW Macro Runtime & Unified `.eats` Container Format:** Non-realtime macro execution engine (`MacroRuntime`) orchestrating `eat.daw` / `project` API for procedural composition (e.g. Acid 303 generator, 909 Techno drum synthesizer, global humanizer), and 100% interoperable `.eats` project save/load container format (`EatsProjectSerializer`) matching original Eatsbeats song specifications.
- **Vector UI & Typography Engine:** Anti-aliased TrueType typography powered by FontStash / `stb_truetype` with dynamic texture atlas caching, subpixel kerning, and multi-tier font fallback chaining. Ships with Google Fonts **Oswald** (condensed for maximum horizontal economy across track headers, mixer strips, and transport readouts) and supports user runtime font configuration.
- **Vector UI & Modern GPU Abstraction:** NanoVG header integration decoupled from legacy backends, streaming vector vertices directly to modern explicit GPU pipelines (WebGPU / Vulkan / Metal / DirectX 12).
- **Strict Rendering Constraints (Zero OpenGL / Zero GLSL Policy):**
  - **Zero OpenGL / WebGL / GLSL:** Legacy OpenGL and GLSL are strictly prohibited across the entire project. OpenGL presents excessive driver overhead, unpredictable vendor state machine stalls, lack of modern multithreaded command recording, and formal deprecation on macOS/iOS. Runtime GLSL parsing/compilation is completely disallowed.
  - **Modern Explicit Low-Overhead GPU Backends Only:** All rendering pipelines exclusively target modern explicit APIs:
    - **WebGPU (WGSL):** First-class unified cross-platform architecture for both Desktop (via Google Dawn / `wgpu-native` driving Vulkan, Metal, and D3D12) and WebAssembly (native browser WebGPU).
    - **Vulkan (SPIR-V):** Ultra-low driver overhead for Windows, Linux, and Android.
    - **Metal (MSL):** Zero-overhead native execution on macOS and iOS.
    - **Direct3D 12 (HLSL):** Native high-performance Windows driver execution.
  - **Shader Authoring Standards:** All shaders must be authored in **HLSL** or **WGSL**, with Ahead-of-Time (AOT) toolchain compilation (`dxc`, `tint`, `naga`) into SPIR-V, MSL, and DXIL/DXBC. This eliminates runtime shader compilation hitches, guarantees predictable pipeline state objects (PSOs), and delivers maximum hardware throughput.
- **Top-Left Brand Logo Menu & Project Hub:** Skeuomorphic modal dialog accessible via the top-left brand logo button or global keyboard shortcuts (`Alt+F`, `F1`, `Ctrl+S`, `Ctrl+O`, `Ctrl+N`, `Ctrl+E`). Features a 5-section accordion: Project Hub (Title/Author inline editing, Save, Save As, Load, New/Reset, Bounce WAV, Script View), Session Persistence & Auto-Restore, Display & Workspace (Themes, UI Scaling, CRT Shaders, Animations), Audio Engine Config (driver stats, sample rate, latency readouts), and Vintage Synth Credits.
- **3-Tier Deriving Theme Engine & Eatsbeats Consoles:** Comprehensive perceptual color math (ITU-R BT.709 luminance, HSL space, lightness/saturation scaling, and WCAG contrast ratio enforcement) with zero-maintenance automatic derivation of 50+ semantic tokens from minimal seed colors, paired with optional artisanal overrides. Ports the 5 original Eatsbeats theme presets:
  1. **Ate Track** *(Default)*: 80s 8-track vintage analog console with weathered dark rack (`#141210`), aged chassis metal (`#24211D`), and warm glowing amber Nixie tubes (`#FF8C00`).
  2. **Midnight Bites**: Obsidian dark cyber theme with pure black (`#000000`), electric neon cyan (`#21F4E8`), and glowing magenta.
  3. **Light Snack**: Bright daylight studio theme with clean white panels (`#FFFFFF`), light slate headers (`#E2E8F0`), and high-contrast deep teal accents (`#007799`).
  4. **Breakfast**: Solarized light theme with creamy parchment background (`#FDF6E3`), base2 panels (`#EEE8D5`), and luminous warm gold tube glow (`#B58900`).
  5. **Dinner**: Solarized dark theme with deep oceanic teal background (`#002B36`), base02 panels (`#073642`), and solarized cyan accents (`#2AA198`).
- **Google Dawn & WebGPU Apocalypse CRT Pipeline:** Encapsulates the workstation with an authentic, rich hardware CRT presentation powered by pure WebGPU Shading Language (WGSL) and Google Dawn (`DawnBridge`):
  - **Google Dawn Native Backend:** Modern WebGPU architecture driving physical GPU hardware via Vulkan, Metal, and Direct3D 12 with zero legacy OpenGL dependencies and minimal binary footprint (only 1.64 MB GUI executable).
  - **Screen-Space Pure WGSL Shader (`crt_screen.wgsl`):** Meshless fullscreen triangle processing (`@builtin(vertex_index)`), vertical zoned processing preserving 100% rectilinear clarity on top transport and bottom navigation bars, dynamic incandescent spotlight sweep, mechanical rumble micro-jitter, and dynamic power sag dimming.
  - **Tactile Skeuomorphic Controls in Flat Mode:** 3D beveled keycaps with chamfer catch-lights, perimeter drop shadows, recessed button wells, and illuminated amber LED tallies.
  - **100% Rectilinear Non-3D Default:** Pristine clarity, zero spherical barrel warping, and unwarped 1:1 mouse hit-detection for accurate knob tweaking and cable patching.

---

## 2. Directory Layout

```
eatsbits/
├── CMakeLists.txt                         # Root CMake configuration (C++20, MSVC/GCC/Clang)
├── README.md                             # Documentation & user guide
├── engine-blueprint.md                   # Engine deconstruction blueprint & AI guidelines
├── gui-mvp.md                            # Model-View-Presenter dual-surface (TUI/GUI) architecture
├── port.md                               # Eatsbeats (Flutter) to Eatsbits (C++20) porting tracker
├── progress.md                           # Verification & test suite tracker
├── build.ps1                              # Automated Windows PowerShell build script
├── build.sh                               # Automated POSIX / Linux / macOS build script
├── assets/
│   └── fonts/                             # Bundled TrueType font assets
│       ├── Oswald-Regular.ttf             # Google Fonts Oswald (Condensed 400)
│       ├── Oswald-Medium.ttf              # Primary UI font (Condensed 500)
│       ├── Oswald-Bold.ttf                # Bold accent font (Condensed 700)
│       ├── Oswald-Variable.ttf            # Variable font asset
│       └── ShareTechMono-Regular.ttf      # Monospace font for script editor & metrics readouts (400)
├── include/
│   └── eatsbits/
│       ├── abi/
│       │   └── eats_plugin_abi.h          # Uniform C-ABI plugin interface
│       ├── audio/
│       │   ├── audio_engine.hpp           # Miniaudio wrapper, device lifecycle, real-time thread loop
│       │   ├── ringbuffer.hpp             # Lock-free wait-free SPSC queue
│       │   ├── dsp/
│       │   │   ├── dynamics_processor.hpp    # Studio compressor with soft-knee & sidechain, zero-overshoot lookahead limiter
│       │   │   ├── parametric_eq.hpp         # 5-band studio parametric equalizer with frequency response evaluation
│       │   │   ├── chorus_flanger.hpp        # Stereo chorus & flanger with quadrature LFOs & fractional delay lines
│       │   │   ├── piano_physical_tables.hpp # Empirical 88-key physical piano measurements (Bank, Bensa, Smith, Michon)
│       │   │   ├── exciters.hpp              # Mechanical exciters (Hammer, Plectrum, Commuted Soundboard, Upright Pluck)
│       │   │   ├── modal_resonator.hpp       # Parallel 2nd-order bandpass modal resonator bank & body cavities
│       │   │   ├── waveguide_core.hpp        # Digital Waveguide, Commuted Piano Waveguide & Upright Bass Waveguide
│       │   │   ├── prng.hpp                  # Fast deterministic pseudo-random number generator (Mulberry32)
│       │   │   ├── procedural_ir.hpp         # Physics-based room & cabinet procedural impulse response simulation
│       │   │   ├── convolver_core.hpp        # Zero-allocation real-time stereo convolution reverb engine
│       │   │   ├── snes_dsp_core.hpp         # Super Nintendo S-SMP / SPC700 16-bit S-DSP chip emulation
│       │   │   ├── ym2612_core.hpp           # Sega Genesis Yamaha YM2612 (OPN2) 4-operator FM sound chip
│       │   │   ├── dx7_core.hpp              # Yamaha DX7 6-operator FM synthesis, 32 algorithms & tables
│       │   │   ├── sid_core.hpp              # Commodore 64 SID MOS 6581/8580 chiptune emulation
│       │   │   ├── drum_synths.hpp           # TR-808 & TR-909 analog physical drum synthesis models
│       │   │   ├── biquad.hpp                # Biquad filter (LowPass, HighPass, BandPass, Notch, etc.)
│       │   │   ├── oscillator.hpp            # Anti-aliased PolyBLEP multi-waveform oscillator
│       │   │   ├── adsr.hpp                  # Sample-accurate ADSR envelope generator
│       │   │   ├── poly_synth.hpp            # Polyphonic voice allocation & synthesis engine
│       │   │   └── tb303_core.hpp            # Ported Robin Schmidt / Mystran TB-303 diode ladder DSP
│       │   └── graph/
│       │       ├── graph_node.hpp            # Base class for modular audio nodes & port routing
│       │       ├── audio_graph.hpp           # Modular DAG evaluator, topology sorter & buffer arena
│       │       ├── audio_node.hpp            # Abstract audio processing node interface
│       │       ├── mixer.hpp                 # Master stereo channel strip with peak & RMS metering
│       │       └── nodes/
│       │           ├── compressor_node.hpp       # Modular stereo compressor node with sidechain input
│       │           ├── limiter_node.hpp          # Modular brickwall lookahead peak limiter node
│       │           ├── parametric_eq_node.hpp    # Modular 5-band studio parametric EQ node
│       │           ├── chorus_node.hpp           # Modular stereo chorus / flanger insert node
│       │           ├── physical_instrument_node.hpp # Polyphonic physical modeling instrument source node
│       │           ├── waveguide_node.hpp        # Modular digital waveguide string resonator node
│       │           ├── modal_resonator_node.hpp  # Modular modal body cavity resonator node
│       │           ├── convolver_node.hpp        # Modular convolution reverb insert effect node
│       │           ├── snes_node.hpp             # Modular Super Nintendo S-DSP generator node
│       │           ├── ym2612_node.hpp           # Modular Sega Genesis YM2612 4-operator FM generator node
│       │           ├── dx7_node.hpp              # Modular Yamaha DX7 6-operator FM generator node
│       │           ├── sid_node.hpp              # Modular Commodore 64 SID generator node
│       │           ├── drum_kit_node.hpp         # Modular TR-808 & TR-909 GM drum machine kit node
│       │           ├── tb303_node.hpp            # Modular TB-303 source node
│       │           ├── poly_synth_node.hpp       # Modular polyphonic synth node
│       │           ├── biquad_node.hpp           # Modular stereo biquad filter node
│       │           ├── delay_node.hpp            # Modular stereo echo/delay node
│       │           └── gain_node.hpp             # Modular volume & panning node
│       ├── eatscript/
│       │   ├── token.hpp                  # Token types and Pythonic token definitions
│       │   ├── lexer.hpp                  # Indentation and bracket-aware Pythonic scanner
│       │   ├── ast.hpp                    # AST nodes (FunctionDef, Return, Assign, If, Call, etc.)
│       │   ├── parser.hpp                 # Recursive-descent parser
│       │   ├── bytecode.hpp               # Compact bytecode instruction set & opcodes
│       │   ├── vm.hpp                     # Zero-allocation real-time bytecode VM (Stereo Audio FX & Math synths)
│       │   ├── transpiler.hpp             # AOT C++ code generator
│       │   ├── note_script.hpp            # Bi-directional Note Scripting DSL (EDIT > SCRIPT)
│       │   ├── dispatch_scanner.hpp       # Native SIMD dispatch scanner & parameter extractor (DESIGN > EATSCRIPT)
│       │   ├── midi_fx_pipeline.hpp       # Scriptable MIDI FX Pipeline Transformers (Arpeggiator, Chord, Humanize)
│       │   └── macro_runtime.hpp          # eat.daw / project macro runtime & generative compositions
│       └── ui/
│           ├── canvas_renderer.hpp        # Modular Eurorack canvas, catenary cables, oscilloscope & SVG/ANSI visualizer
│           ├── dawn_bridge.hpp            # Google Dawn & WebGPU pipeline bridge, pure WGSL Apocalypse CRT shader
│           ├── gui_window.hpp             # GLFW 3 desktop window, knob drag, cable patch & sequencer hit-test
│           └── nanovg_backend.hpp         # Decoupled NanoVG backend & vector widget models
│       ├── export/
│       │   └── wav_exporter.hpp           # High-speed RIFF/WAVE 16/24/32-bit exporter with TPDF dither
│       ├── sequencer/
│       │   ├── transport.hpp              # High-precision transport clock with swing shuffle
│       │   └── step_sequencer.hpp         # Multi-track/pattern tracker sequencer
│       └── project/
│           ├── project_file.hpp           # Dual-format project manager (.eats script & JSON)
│           ├── eats_serializer.hpp        # Authentic .eats song container format serializer/deserializer
│           └── preset_loader.hpp          # Declarative def gui(): schema parser & .eats preset loader
├── assets/
│   └── shaders/
│       └── crt_screen.wgsl                # Pure WGSL Apocalypse CRT shader (spotlight, rumble, sag, scanlines)
├── src/
│   ├── audio/
│   │   ├── audio_engine.cpp               # Miniaudio hardware device binding & event dispatch
│   │   ├── audio_graph.cpp                # Topological DAG evaluation & execution plan compiler
│   │   └── export/
│   │       └── wav_exporter.cpp           # WAV binary RIFF writer, TPDF dither, master bounce & stems
│   ├── sequencer/
│   │   └── step_sequencer.cpp             # Tracker sequencing & frame-accurate note scheduling
│   ├── eatscript/
│   │   ├── note_script.cpp                # Declarative note serialization & parsing engine
│   │   ├── dispatch_scanner.cpp           # Native dispatch tag scanner & def init() parameter parser
│   │   ├── midi_fx_pipeline.cpp           # Arpeggiator, Scale Snap, Chord Follow, Humanize, and track sync
│   │   └── macro_runtime.cpp              # Procedural Acid 303, 909 drums, and eat.daw macro engine
│   ├── project/
│   │   ├── project_file.cpp               # Project container loader & saver (.eats / JSON)
│   │   ├── eats_serializer.cpp            # Authentic Eatsbeats .eats song file format serializer
│   │   └── preset_loader.cpp              # Declarative GUI flex layout & .eats preset file parser
│   ├── ui/
│   │   ├── canvas_renderer.cpp            # Modular layout, catenary cable math, oscilloscope & SVG/ANSI exporter
│   │   ├── dawn_bridge.cpp                # Google Dawn / WebGPU backend & native presentation rasterizer
│   │   └── gui_window.cpp                 # GLFW event handling, mouse interaction & real-time UI loop
│   ├── eatscript/
│   │   ├── lexer.cpp
│   │   ├── parser.cpp
│   │   ├── vm.cpp
│   │   └── transpiler.cpp
│   ├── plugins/
│   │   └── builtin_303.cpp                # Native TB-303 C-ABI plugin implementation
│   ├── main.cpp                           # Interactive CLI REPL, sequencer, and audio test harness
│   └── gui_main.cpp                       # Interactive desktop GUI workstation (GLFW + WebGPU/NanoVG)
├── third_party/
│   ├── miniaudio/
│   │   └── miniaudio.h                    # Vendored single-header audio library
│   └── nanovg/
│       ├── nanovg.h                       # NanoVG public vector canvas API
│       ├── nanovg.c
│       ├── fontstash.h
│       ├── stb_image.h
│       └── stb_truetype.h
└── tests/
    ├── test_audio_realtime.cpp            # 32/64/128-sample buffer stress tests & latency benchmarks
    ├── test_audio_graph.cpp               # Modular DAG topological sort, cycle detection & node benchmarks
    ├── test_drum_synths.cpp               # TR-808/909 drum synthesis, GM pad mapping & choke benchmarks
    ├── test_eatscript_vm.cpp              # Parser, AST, and VM execution unit tests
    ├── test_transpiler.cpp                # AOT transpiler verification tests
    ├── test_sequencer_project.cpp         # Tracker sequencing, swing clock & .eats project serialization
    ├── test_ui_canvas.cpp                 # Vector UI canvas, catenary cables, oscilloscope & SVG export
    ├── test_wav_export.cpp                # 16/24/32-bit WAV writer, TPDF dither, master bounce & stems
    ├── test_gui_interaction.cpp           # Desktop GUI hit-testing, knob tweaking, cable drag & drop, CRT mesh
    ├── test_preset_loader.cpp             # Declarative def gui(): flex layout, .eats preset parsing & DSP binding
    ├── test_sid_synth.cpp                 # C64 SID chip waveforms, PWM, noise, SVF filter & arpeggiator
    ├── test_dx7_synth.cpp                 # Yamaha DX7 32 algorithms, envelopes, velocity & 8-voice polyphony
    ├── test_snes_dsp.cpp                  # Super Nintendo S-DSP 12 waveforms, Gaussian smoothing, 8-tap FIR echo
    ├── test_ym2612_fm.cpp                 # Sega Genesis YM2612 8 algorithms, feedback, Total Level & SFXR presets
    ├── test_convolver.cpp                 # Convolution reverb & 13 procedural IR presets, Dirac impulse, 10s benchmark
    ├── test_physical_modeling.cpp         # Stanford CCRMA Bank/Bensa piano, upright bass, guitars & modal resonators
    ├── test_studio_fx.cpp                 # Compressor, brickwall limiter, 5-band parametric EQ, chorus & channel strip
    └── test_note_script.cpp               # Bi-directional Eatscript note serialization, parsing, error tolerance & roundtrip
```

---

## 3. Building & Running

### Prerequisites
- CMake 3.20+
- C++20 compliant compiler (MSVC 2022, GCC 11+, or Clang 13+)

### Automated Build Scripts
Eatsbits includes automated root build scripts matching the workflow of Eatsbeats:

- **Windows (PowerShell):**
  ```powershell
  .\build.ps1             # Fast build of application binaries only (eatsbits_gui.exe, eatsbits_cli.exe)
  .\build.ps1 -Run        # Fast build and immediately launch eatsbits_gui.exe
  .\build.ps1 -Test       # Build test suite and run complete 21-target automated tests
  .\build.ps1 -Clean -Run # Clean rebuild and launch GUI
  ```
- **Linux / macOS (Bash):**
  ```bash
  ./build.sh              # Fast build of application binaries only (Release mode)
  ./build.sh --run        # Fast build and launch eatsbits_gui
  ./build.sh --test       # Build test suite and run tests
  ./build.sh --clean      # Clean rebuild
  ```

### Manual CMake Build Steps
```powershell
# Configure CMake (fetches and configures GLFW 3.4 automatically)
cmake -B build -S .

# Build Release binaries
cmake --build build --config Release

# Run automated test suite (100% passing across 42 targets)
ctest --test-dir build -C Release --output-on-failure
```

### Standalone Executable Targets
1. **`eatsbits_gui.exe`:** Hardware-accelerated Desktop GUI DAW (Dawn / WebGPU / NanoVG).
2. **`eatsbits_tui.exe`:** Ultra-low latency terminal DAW with ANSI differential rendering and Braille telemetry.
3. **`eatsbits_term.exe`:** Standalone WebGPU GPU terminal emulator with built-in Eatscript shell.
4. **`eatscript_cli.exe`:** Standalone Eatscript compiler, bytecode VM, AOT C++ transpiler, and interactive REPL.
5. **`eatsbits_cli.exe`:** CLI demo harness and audio auditioning testbench.

### Running the Standalone WebGPU Terminal & Eatscript Shell
```powershell
.\build\Release\eatsbits_term.exe
```

### Running the Standalone Eatscript REPL / CLI
```powershell
# Interactive REPL
.\build\Release\eatscript_cli.exe --shell

# Direct Expression Evaluation
.\build\Release\eatscript_cli.exe --eval "print(math.sin(math.pi * 0.5))"

# Disassemble Script Bytecode
.\build\Release\eatscript_cli.exe --bytecode synth.eats

# AOT Transpile to C-ABI C++ Plugin
.\build\Release\eatscript_cli.exe --transpile synth.eats -o synth_plugin.cpp
```

### Running the Interactive Desktop GUI (GLFW + Google Dawn / WebGPU / NanoVG)
```powershell
.\build\Release\eatsbits_gui.exe
# Or simply:
.\build.ps1 -Run
```
Inside the Desktop GUI:
- **Interactive WebGPU Terminal & REPL Drawer:** Expandable terminal drawer docked above the bottom bar with live Host ABI reflection to the running audio engine. Type expressions, trigger sounds, or inspect BPM and telemetry live.
- **Canonical Bottom Hardware Navigation Panel:** Instant switching between `ARRANGER` (Multi-track playlist timeline), `EDIT` (Tracker & Piano Roll sub-views), `TRACK` (Instrument hardware faceplate & audio FX rack), `MIXER` (Studio console faders & meters), and `DESIGN` (Eurorack Modular rack & Eatscript IDE).
- **Top Transport Header:** Live Bar:Beat:Tick Nixie/LCD Song Position readout (`001:01:01`), Quantize Snap selector (`1/16`), `[METRO]` Metronome toggle, `[LOOP]` Loop region toggle, and `[BROWSER]` Project Browser drawer button.
- **Arranger Timeline Workspace (`ARRANGER`):** Multi-track playlist canvas with 5 track cards, time ruler, loop markers, clip blocks with mini step waveforms, real-time live playhead, and minimap overview scrollbar.
- **Declarative Hardware Faceplates (`TRACK`):** Renders authentic Roland TB-303, TR-808, TR-909, Studio Echo, Commodore 64 SID (`c64_breadbin`), Yamaha DX7 (`dx7_faceplate`), Super Nintendo S-DSP (`snes_faceplate`), Sega Genesis YM2612 (`ym2612_faceplate`), Convolution Reverb (`convolver_faceplate`), Concert Grand Piano (`piano_ebony`), Upright Double Bass (`warm_amber`), Spanish Classical Guitar (`spanish_cedar`), and Steel Acoustic Guitar (`spruce_top`) chassis generated directly from Eatscript `def gui():` definitions.
- **Preset Browser (`< PREV` / `NEXT >`):** Cycle through 13 canonical `.eats` presets with instantaneous flex re-layout and real-time DSP binding.
- **Interactive Rotary Knobs & Switches:** Roland stepped selectors (`tb303_selector`), fluted skirted potentiometers (`tb303_potentiometer`), and glowing neon amber Nixie tube readouts. Click and drag up/down on any knob to smoothly sweep parameters in real-time.
- **Interactive Live Eatscript IDE (`DESIGN` -> `EATSCRIPT`):** Live code DSP synths and audio FX in Pythonic Eatscript. Features syntax colorization, cursor navigation & auto-indent, 5 stock sound generator/FX templates, real-time bytecode compilation into `eatscript::VM`, disassembled opcode inspector, AOT C++ code preview, and zero-allocation hot-reloading directly into active track nodes.
- **Dynamic Catenary Patch Cables (`DESIGN` -> `MODULAR RACK`):** Click any orange output jack to drag a live patch cord with gravitational sag; drop onto a cyan input jack to rewire the audio DAG on the fly. Right-click any jack to disconnect all cables.
- **Multi-Track Sequencer (`EDIT`):** Click any step pad to toggle notes on/off.
- **Live Oscilloscope & VU Meters:** Real-time green phosphor CRT oscilloscope trace and stereo peak-hold meters driven by the lock-free audio thread.
- **Spacebar:** Toggles sequencer transport play/pause.
- **Navigation Shortcuts:** `F1`–`F5` or `'1'`–`'5'` to switch views; `Tab` to cycle sequentially.


### Running the Interactive CLI
```powershell
.\build\Release\eatsbits_cli.exe
```

Inside the CLI:
- Press `1` to play the authentic 16-step TB-303 Acid bassline sequence with dynamic cutoff sweeps and slides.
- Press `2` to trigger a rich polyphonic 4-voice C Major chord.
- Press `3` to live-code custom Eatscript scripts, execute them on the VM, and transpile them into native C-ABI C++ plugins.
- Press `4` to audition the real-time **Modular Audio Graph** (TB-303 -> Stereo Echo Delay -> Master Gain).
- Press `5` to play the full synchronized **Acid Beat Demo** (TB-303 Acid Bassline + TR-808/909 Drums + Delay).
- Press `6` to audition the **Sample-Accurate Multi-Track Sequencer & `.eats` Project I/O** (TB-303 + Drums + Live Eatscript Arp with seamless serialization round-trip).
- Press `7` to display the **Modular Vector UI & ANSI Real-Time Visualizer** (live ASCII oscilloscope trace, dual-channel VU peak meters, 16-step tracker LED grid, and instant high-res `rack_patchbay.svg` vector export).
- Press `8` to trigger the **Studio Audio Bouncing Engine** (renders 24-bit uncompressed studio master WAV and isolated multi-track stems to `bounces/stems/` with TPDF dithering).

---

## 4. Benchmark & Stress-Test Results

Ran on 44.1kHz / 48kHz stereo configuration:
- **Acoustic Physical Modeling Engine (Concert Grand Piano):** 10.0 seconds of 4-voice physical modeling audio (Bank-Bensa commuted waveguide with 3-stage allpass dispersion & bridge coupling) rendered in **82.2ms** (**121.6x faster than real-time budget**).
- **Convolution Reverb & Procedural IR:** 10.0 seconds of stereo audio convolved with a dense 2048-tap impulse response rendered in **2325ms** (**4.3x faster than real-time budget** with zero dynamic memory allocation).
- **Super Nintendo S-DSP Engine:** 10.0 seconds of 8-voice polyphony with 8-tap FIR echo rendered in **216.8ms** (**46.1x faster than real-time budget**).
- **Sega Genesis YM2612 4-Op FM Engine:** 10.0 seconds of 6-voice polyphony rendered in **522.6ms** (**19.1x faster than real-time budget**).
- **Yamaha DX7 6-Operator FM Engine:** 10.0 seconds of 8-voice polyphony (48 active operators with dual-bus routing and feedback loops) rendered in **405.5ms** (**24.7x faster than real-time budget**).
- **Commodore 64 SID MOS 6581/8580 Engine:** 10.0 seconds of 3-voice polyphony with 23-bit LFSR noise and SVF filter rendered in **113.8ms** (**87.8x faster than real-time budget**).
- **Offline Project Bouncing & Stem Rendering:** 24-bit PCM multi-track audio rendered at **92.9x faster than real-time speed** (2.0s rendered in **21.5ms**).
- **Sample-Accurate Sequencer & Audio Graph:** 10.0 seconds of multi-track sequenced audio rendered in **176.3ms** (**56.7x faster than real-time budget**).
- **Modular Audio Graph (Multi-Node DAG):** 6.67 seconds of audio rendered through chained nodes in **49.3ms** (**135x faster than real-time budget**).
- **Analog Drum Synthesis Suite (TR-808 & 909):** 6.67 seconds of 6-voice polyphonic drum rendering in **73.5ms** (**90.7x faster than real-time budget**).
- **32-sample buffer size:** 1.33 seconds of audio rendered in **4.33ms** (**307x faster than real-time budget**).
- **64-sample buffer size:** 2.67 seconds of audio rendered in **9.08ms** (**293x faster than real-time budget**).
- **128-sample buffer size:** 5.33 seconds of audio rendered in **17.83ms** (**299x faster than real-time budget**).
- **TB-303 4x-oversampled diode ladder:** 2.0 seconds rendered in **32.32ms** (**61.8x faster than real-time budget**).
- **SPSC Ringbuffer concurrency:** 50,000 inter-thread events delivered with **0 dropped packets** and **0 lock contention**.

---

## Modular C++ UI/UX Architecture (Eatsbeats Parity)

Eatsbits features a modular native C++ UI/UX architecture mirroring the ergonomics and clarity of Eatsbeats:

- **5 Dedicated Workspace Tabs:**
  - **Arranger (`WorkspaceView::Arranger`):** Multi-track playlist, edge-drag clip resizing, clip movement, duplication (`Ctrl+D`), and playhead follow mode (`F`).
  - **Edit (`WorkspaceView::Edit`):** 4 switchable sub-views (**Piano Roll**, **Tracker**, **Score**, **Eatscript Note Sync**), ghost notes with opacity slider, and sliding note inspector sidebar.
  - **Track (`WorkspaceView::Track`):** Track header card, authentic instrument faceplate with sweepable rotary controls (Roland TB-303, TR-808, TR-909, DX7), MIDI FX rack (Arpeggiator, Scale Snap, Humanize), and Audio FX rack (Delay, Chorus, Parametric EQ, Dynamics, Convolver).
  - **Mixer (`WorkspaceView::Mixer`):** **Pinned Master Channel Strip on the left** (always visible during horizontal panning), horizontally scrollable track strips with peak-hold LED meters, long-throw tactile volume faders, pan knobs, and collapsible Track Properties inspector.
  - **Design (`WorkspaceView::Design`):** Eurorack modular synthesizer rack with dynamic catenary sagging patch cables, live Eatscript coding IDE with hot-reload, and visual GUI designer.

- **Ubiquitous Drawers & Global Controls:**
  - **Virtual Piano Keyboard Drawer (`VirtualKeyboardDrawer`):** Universal collapsible slide-up piano keyboard docked directly above the bottom navigation bar across all tabs, with octave shifting and multi-touch / mouse glissando auditioning.
  - **Project Browser Drawer (`ProjectBrowserDrawer`):** Slide-in drawer on the right side with animated transitions, featuring Presets, procedural Macros, and Diff History milestones.
  - **Transport Header (`TransportHeader`):** Persistent top strip featuring Play/Stop, Nixie Bar:Beat:Tick LCD readout, BPM scrubber, Snap quantize, Loop, and Metronome.
  - **Bottom Navigation Bar (`BottomNavBar`):** 5-tab hardware button strip with extruded 3D mechanical keycaps, illuminated LED tallies, and keyboard hotkeys (`1`-`5`, `F1`-`F5`).

- **Unified Pointer & Gesture Arena:**
  - **Input (`PointerEvent`):** Unified touch and mouse pointer representation across desktop and mobile.
  - **Kinetic Momentum (`KineticScroller`):** Exponential decay inertia glide for fluid tactile scrolling.
  - **Gesture Discrimination (`GestureRecognizer`):** Platform-aware slop thresholds (**16px touch slop** for mobile finger taps vs. **3px mouse slop** for desktop precision).

