# Eatsbits (C++20 Engine)

> **Next-Generation Native Rebuild of Eatsbeats**  
> *A high-performance, low-latency Digital Audio Workstation and live-coding environment built in pure C++20.*

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
  - Master stereo mixer strip with constant-power panning and peak/RMS metering.
- **Vector UI Abstraction:** NanoVG header integration decoupled from legacy OpenGL backends, ready to stream vector vertices directly to Google Filament vertex/index buffers.

---

## 2. Directory Layout

```
eatsbits/
├── CMakeLists.txt                         # Root CMake configuration (C++20, MSVC/GCC/Clang)
├── README.md                             # Documentation & user guide
├── include/
│   └── eatsbits/
│       ├── abi/
│       │   └── eats_plugin_abi.h          # Uniform C-ABI plugin interface
│       ├── audio/
│       │   ├── audio_engine.hpp           # Miniaudio wrapper, device lifecycle, real-time thread loop
│       │   ├── ringbuffer.hpp             # Lock-free wait-free SPSC queue
│       │   ├── dsp/
│       │   │   ├── biquad.hpp             # Biquad filter (LowPass, HighPass, BandPass, Notch, etc.)
│       │   │   ├── oscillator.hpp         # Anti-aliased PolyBLEP multi-waveform oscillator
│       │   │   ├── adsr.hpp               # Sample-accurate ADSR envelope generator
│       │   │   ├── poly_synth.hpp         # Polyphonic voice allocation & synthesis engine
│       │   │   └── tb303_core.hpp         # Ported Robin Schmidt / Mystran TB-303 diode ladder DSP
│       │   └── graph/
│       │       ├── audio_node.hpp         # Abstract audio processing node interface
│       │       └── mixer.hpp              # Master stereo channel strip with peak & RMS metering
│       ├── eatscript/
│       │   ├── token.hpp                  # Token types and Pythonic token definitions
│       │   ├── lexer.hpp                  # Indentation and bracket-aware Pythonic scanner
│       │   ├── ast.hpp                    # AST nodes (FunctionDef, Return, Assign, If, Call, etc.)
│       │   ├── parser.hpp                 # Recursive-descent parser
│       │   ├── bytecode.hpp               # Compact bytecode instruction set & opcodes
│       │   ├── vm.hpp                     # Zero-allocation real-time bytecode VM
│       │   └── transpiler.hpp             # AOT C++ code generator
│       └── ui/
│           └── nanovg_backend.hpp         # Decoupled NanoVG backend & vector widget models
├── src/
│   ├── audio/
│   │   └── audio_engine.cpp               # Miniaudio hardware device binding & event dispatch
│   ├── eatscript/
│   │   ├── lexer.cpp
│   │   ├── parser.cpp
│   │   ├── vm.cpp
│   │   └── transpiler.cpp
│   ├── plugins/
│   │   └── builtin_303.cpp                # Native TB-303 C-ABI plugin implementation
│   └── main.cpp                           # Interactive CLI REPL, sequencer, and audio test harness
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
    ├── test_eatscript_vm.cpp              # Parser, AST, and VM execution unit tests
    └── test_transpiler.cpp                # AOT transpiler verification tests
```

---

## 3. Building & Running

### Prerequisites
- CMake 3.20+
- C++20 compliant compiler (MSVC 2022, GCC 11+, or Clang 13+)

### Build Steps
```powershell
# Configure CMake
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build Release binaries
cmake --build build --config Release

# Run automated test suite
ctest --test-dir build -C Release --output-on-failure
```

### Running the Interactive CLI
```powershell
.\build\Release\eatsbits_cli.exe
```

Inside the CLI:
- Press `1` to play the authentic 16-step TB-303 Acid bassline sequence with dynamic cutoff sweeps and slides.
- Press `2` to trigger a rich polyphonic 4-voice C Major chord.
- Press `3` to live-code custom Eatscript scripts, execute them on the VM, and transpile them into native C-ABI C++ plugins.

---

## 4. Benchmark & Stress-Test Results

Ran on 48kHz stereo configuration:
- **32-sample buffer size:** 1.33 seconds of audio rendered in **4.33ms** (**307x faster than real-time budget**).
- **64-sample buffer size:** 2.67 seconds of audio rendered in **9.08ms** (**293x faster than real-time budget**).
- **128-sample buffer size:** 5.33 seconds of audio rendered in **17.83ms** (**299x faster than real-time budget**).
- **TB-303 4x-oversampled diode ladder:** 2.0 seconds rendered in **32.32ms** (**61.8x faster than real-time budget**).
- **SPSC Ringbuffer concurrency:** 50,000 inter-thread events delivered with **0 dropped packets** and **0 lock contention**.
