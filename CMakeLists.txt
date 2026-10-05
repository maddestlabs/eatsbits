cmake_minimum_required(VERSION 3.20)
project(eatsbits VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler optimizations and warnings
if(MSVC)
    add_compile_options(/W4 /O2 /permissive- /wd4244 /wd4267 /wd4324 /openmp)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(EMSCRIPTEN)
    add_compile_options(-Wall -Wextra -O3 -msimd128 -fwasm-exceptions)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN NDEBUG)
else()
    add_compile_options(-Wall -Wextra -O3 -fopenmp)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Global Includes
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanovg
)

if(NOT EMSCRIPTEN)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/wgpu/include)
    if(WIN32)
        link_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/wgpu/lib)
        link_libraries("${CMAKE_CURRENT_SOURCE_DIR}/third_party/wgpu/lib/wgpu_native.dll.lib")
    endif()
endif()

# 1. Fundamental Zero-Dependency Core Primitives (Time, Geometry, Color, Memory, Math)
add_library(eats_core INTERFACE)
target_include_directories(eats_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 2. Standalone Eatscript Scripting & Host ABI Library (Lexer, Parser, VM, Transpiler, Evaluator, Host ABI, Stdlib, REPL)
add_library(eats_script STATIC
    src/eatscript/lexer.cpp
    src/eatscript/parser.cpp
    src/eatscript/vm.cpp
    src/eatscript/transpiler.cpp
    src/eatscript/evaluator.cpp
    src/eatscript/host_abi.cpp
    src/eatscript/stdlib.cpp
    src/eatscript/repl.cpp
)
target_link_libraries(eats_script PUBLIC eats_core)

# Standalone Eatscript CLI Executable (Zero Audio/DSP/GUI Hardware Dependencies)
add_executable(eatscript_cli src/eatscript_main.cpp)
target_link_libraries(eatscript_cli PRIVATE eats_script)

# 3. Real-Time Audio DSP, Modular Graph, Sequencer, Synthesis Nodes, Project I/O
add_library(eats_audio STATIC
    src/audio/audio_engine.cpp
    src/audio/audio_graph.cpp
    src/sequencer/step_sequencer.cpp
    src/project/project_file.cpp
    src/project/preset_loader.cpp
    src/eatscript/note_script.cpp
    src/eatscript/dispatch_scanner.cpp
    src/eatscript/midi_fx_pipeline.cpp
    src/eatscript/macro_runtime.cpp
    src/project/eats_serializer.cpp
    src/project/diff_history_manager.cpp
    src/theory/chord_model.cpp
    src/plugins/builtin_303.cpp
    src/audio/export/wav_exporter.cpp
    src/audio/audio_to_midi_engine.cpp
    src/audio/procedural_ir_generator.cpp
    src/audio/soundfont/soundfont_decoder.cpp
    src/audio/graph/nodes/soundfont_node.cpp
    src/procgen/song_archetypes.cpp
    src/procgen/procedural_acid_engine.cpp
    src/procgen/procedural_drum_engine.cpp
    src/procgen/procedural_piano_engine.cpp
    src/procgen/procedural_ensemble_engine.cpp
    src/procgen/procedural_song_engine.cpp
    src/audio/track_freeze_engine.cpp
    src/sequencer/note_splitter_engine.cpp
    src/ai/gemini_client.cpp
    src/ai/ai_mixing_engine.cpp
    src/lyrics/lyric_track.cpp
    src/audio/graph/nodes/tts_synth_node.cpp
    src/project/eats_builtin_presets.cpp
    src/project/preset_manager.cpp
)
target_link_libraries(eats_audio PUBLIC eats_core eats_script)

# Core Platform Libraries (Audio Only)
if(EMSCRIPTEN)
    target_compile_options(eats_audio PUBLIC -msimd128 -O3)
    target_compile_definitions(eats_audio PUBLIC NDEBUG)
    target_link_libraries(eats_audio PUBLIC)
elseif(WIN32)
    target_link_libraries(eats_audio PUBLIC ole32 winmm winhttp)
elseif(APPLE)
    target_link_libraries(eats_audio PUBLIC "-framework CoreAudio" "-framework AudioToolbox")
elseif(UNIX)
    target_link_libraries(eats_audio PUBLIC pthread dl m)
endif()

# 4. Headless Presentation Logic, Dual-Surface State Machines & Interaction Models
add_library(eats_presenter STATIC
    src/presenter/parameter_presenter.cpp
    src/presenter/value_edit_presenter.cpp
    src/presenter/track_properties_presenter.cpp
    src/presenter/scalar_drag_presenter.cpp
    src/presenter/timeline_scrub_presenter.cpp
    src/presenter/splitter_drag_presenter.cpp
    src/presenter/cable_patch_presenter.cpp
    src/presenter/marquee_select_presenter.cpp
    src/presenter/telemetry_presenter.cpp
    src/presenter/scroll_physics_presenter.cpp
    src/presenter/zoom_pan_presenter.cpp
    src/ui/element2d.cpp
)
target_link_libraries(eats_presenter PUBLIC eats_core eats_audio)

# Windowing and Graphics Toolchain (GUI only)
if (EMSCRIPTEN)
    # Emscripten WebAssembly & WebGPU configuration
    add_compile_definitions(EATS_ENABLE_WEBGPU=1)
else()
    # GLFW Library (cross-platform desktop windowing)
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# 5. Hardware-Accelerated 2D Batch Renderer (Dawn/WebGPU & Monospace Glyph Engine)
add_library(eats_render_wgpu STATIC
    src/ui/batch_renderer_2d.cpp
    src/ui/dawn_bridge.cpp
    src/ui/canvas_renderer.cpp
    src/ui/draw_utils.cpp
    src/ui/svg_path.cpp
    src/ui/theme.cpp
    src/ui/monospace_font_8x16.cpp
    src/ui/icon_registry.cpp
    src/ui/gui_panel_def.cpp
    third_party/nanovg/nanovg.c
)
target_link_libraries(eats_render_wgpu PUBLIC eats_core eats_presenter)

if(EMSCRIPTEN)
    target_compile_options(eats_render_wgpu PUBLIC --use-port=emdawnwebgpu -fwasm-exceptions)
    target_link_options(eats_render_wgpu PUBLIC --use-port=emdawnwebgpu -sUSE_GLFW=3 -fwasm-exceptions)
elseif(WIN32)
    set(WGPU_NATIVE_LIB "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wgpu/lib/wgpu_native.dll.lib")
    target_link_libraries(eats_render_wgpu PUBLIC 
        comdlg32 user32 gdi32 shlwapi opengl32 glfw "${WGPU_NATIVE_LIB}"
    )
elseif(APPLE)
    target_link_libraries(eats_render_wgpu PUBLIC glfw)
elseif(UNIX)
    target_link_libraries(eats_render_wgpu PUBLIC glfw)
endif()

# 6. ANSI Diff Cell Surface & Terminal State Machine
add_library(eats_render_tui STATIC
    src/tui/cell_surface.cpp
    src/tui/ansi_diff_renderer.cpp
    src/tui/terminal_device.cpp
    src/tui/input_parser.cpp
    src/tui/audio_telemetry.cpp
    src/terminal/terminal_grid.cpp
    src/terminal/ansi_parser.cpp
)
target_link_libraries(eats_render_tui PUBLIC eats_core eats_presenter eats_audio)

# 7. Deterministic Headless Audio-Visual Video & Frame Exporter
add_library(eats_video_export STATIC
    src/export/headless_video_exporter.cpp
)
target_link_libraries(eats_video_export PUBLIC eats_render_wgpu eats_presenter eats_audio eats_core)

# Compatibility Interface: eatsbits_core (aggregates eats_audio, eats_presenter, eats_core, eats_script)
add_library(eatsbits_core INTERFACE)
target_link_libraries(eatsbits_core INTERFACE eats_audio eats_presenter eats_core eats_script)

# GUI Desktop Core Library (Window, Views, Widgets)
add_library(eatsbits_gui_core STATIC
    src/ui/gui_window.cpp
    src/ui/widgets/piano_keyboard.cpp
    src/ui/widgets/virtual_keyboard_drawer.cpp
    src/ui/widgets/arranger_mixer_drawer.cpp
    src/ui/widgets/drum_pad_grid_widget.cpp
    src/ui/widgets/project_browser_drawer.cpp
    src/ui/widgets/transport_header.cpp
    src/ui/widgets/bottom_nav_bar.cpp
    src/ui/widgets/plugin_search_dialog.cpp
    src/ui/widgets/circle_of_fifths_dialog.cpp
    src/ui/widgets/track_properties_panel.cpp
    src/ui/widgets/track_properties_drawer.cpp
    src/ui/widgets/terminal_console_drawer.cpp
    src/ui/widgets/value_edit_dialog.cpp
    src/ui/widgets/text_field_widget.cpp
    src/ui/widgets/text_editor_widget.cpp
    src/ui/widgets/icon_search_dialog.cpp
    src/ui/widgets/command_palette_dialog.cpp
    src/ui/widgets/audio_to_midi_dialog.cpp
    src/ui/widgets/waveshaper_dialog.cpp
    src/ui/widgets/note_splitter_dialog.cpp
    src/ui/widgets/color_picker_dialog.cpp
    src/ui/widgets/theme_browser_dialog.cpp
    src/ui/widgets/space_visualizer_widget.cpp
    src/ui/widgets/ai_assistant_dialog.cpp
    src/ui/widgets/fullscreen_device_modal.cpp
    src/ui/views/arranger_view.cpp
    src/ui/views/edit_view.cpp
    src/ui/views/track_inspector_view.cpp
    src/ui/views/mixer_view.cpp
    src/ui/views/design_view.cpp
)
target_link_libraries(eatsbits_gui_core PUBLIC eats_render_wgpu eats_render_tui eats_video_export eats_presenter eats_audio eats_core)

# TUI Views and App Library
add_library(eatsbits_tui_core STATIC
    src/tui/views/arranger_view.cpp
    src/tui/views/braille_scope.cpp
    src/tui/views/tracker_grid.cpp
    src/tui/views/param_rack.cpp
    src/tui/tui_app.cpp
)
target_link_libraries(eatsbits_tui_core PUBLIC eats_render_tui eats_presenter eats_audio eats_core)

if (EMSCRIPTEN)
    # WebAssembly & WebGPU Application
    add_executable(eatsbits_web src/gui_main.cpp)
    target_compile_options(eatsbits_web PRIVATE -O3 -msimd128 -fwasm-exceptions)
    target_compile_definitions(eatsbits_web PRIVATE NDEBUG)
    target_link_libraries(eatsbits_web PRIVATE eatsbits_gui_core)
    set_target_properties(eatsbits_web PROPERTIES
        OUTPUT_NAME "eatsbits"
        SUFFIX ".html"
        LINK_FLAGS "-O3 -msimd128 -sNO_EXIT_RUNTIME=1 -fwasm-exceptions --use-port=emdawnwebgpu -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/presets@/presets --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
    )
    add_custom_command(TARGET eatsbits_web POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/eatsbits.html"
            "${CMAKE_BINARY_DIR}/index.html"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/manifest.json"
            "${CMAKE_BINARY_DIR}/manifest.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/sw.js"
            "${CMAKE_BINARY_DIR}/sw.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/app_icon.svg"
            "${CMAKE_BINARY_DIR}/app_icon.svg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/icon-192.png"
            "${CMAKE_BINARY_DIR}/icon-192.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/icon-512.png"
            "${CMAKE_BINARY_DIR}/icon-512.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/icon-maskable-192.png"
            "${CMAKE_BINARY_DIR}/icon-maskable-192.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/icon-maskable-512.png"
            "${CMAKE_BINARY_DIR}/icon-maskable-512.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/apple-touch-icon.png"
            "${CMAKE_BINARY_DIR}/apple-touch-icon.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/favicon.ico"
            "${CMAKE_BINARY_DIR}/favicon.ico"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/tui.html"
            "${CMAKE_BINARY_DIR}/tui.html"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/CNAME"
            "${CMAKE_BINARY_DIR}/CNAME"
        COMMENT "Copying PWA manifest, service worker, icons, CNAME, and index.html to web root"
    )
else()
    # CLI Demo Harness Application
    add_executable(eatsbits_cli src/main.cpp)
    target_link_libraries(eatsbits_cli PRIVATE eatsbits_gui_core)

    # Terminal-Based High-Performance DAW
    add_executable(eatsbits_tui src/tui_main.cpp)
    target_link_libraries(eatsbits_tui PRIVATE eatsbits_tui_core eatsbits_core)

    # Desktop GUI Application
    if(WIN32)
        add_executable(eatsbits_gui WIN32 src/gui_main.cpp src/ui/resources/eatsbits.rc)
        if(MSVC)
            set_target_properties(eatsbits_gui PROPERTIES
                LINK_FLAGS "/ENTRY:mainCRTStartup"
            )
        endif()
    else()
        add_executable(eatsbits_gui src/gui_main.cpp)
    endif()
    target_link_libraries(eatsbits_gui PRIVATE eatsbits_gui_core)
    if(WIN32)
        add_custom_command(TARGET eatsbits_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wgpu/lib/wgpu_native.dll"
            "$<TARGET_FILE_DIR:eatsbits_gui>"
        )
    endif()

    # Standalone WebGPU GPU Terminal & Eatscript Shell
    add_executable(eatsbits_term src/term_main.cpp)
    target_link_libraries(eatsbits_term PRIVATE eats_render_wgpu eats_render_tui eats_script eats_core)
    if(WIN32)
        add_custom_command(TARGET eatsbits_term POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wgpu/lib/wgpu_native.dll"
            "$<TARGET_FILE_DIR:eatsbits_term>"
        )
    endif()
endif()

# Tests
option(BUILD_TESTING "Build automated test suite" ON)

if(BUILD_TESTING)
    enable_testing()

    add_executable(test_tui_surface tests/test_tui_surface.cpp)
    target_link_libraries(test_tui_surface PRIVATE eatsbits_tui_core eatsbits_core)
    add_test(NAME TuiSurfaceTest COMMAND test_tui_surface)

    add_executable(test_audio_realtime tests/test_audio_realtime.cpp)
    target_link_libraries(test_audio_realtime PRIVATE eatsbits_core)
    add_test(NAME AudioRealtimeTest COMMAND test_audio_realtime)

    add_executable(test_eatscript_vm tests/test_eatscript_vm.cpp)
    target_link_libraries(test_eatscript_vm PRIVATE eatsbits_core)
    add_test(NAME EatscriptVmTest COMMAND test_eatscript_vm)

add_executable(test_transpiler tests/test_transpiler.cpp)
target_link_libraries(test_transpiler PRIVATE eatsbits_core)
add_test(NAME TranspilerTest COMMAND test_transpiler)

add_executable(test_audio_graph tests/test_audio_graph.cpp)
target_link_libraries(test_audio_graph PRIVATE eatsbits_core)
add_test(NAME AudioGraphTest COMMAND test_audio_graph)

add_executable(test_drum_synths tests/test_drum_synths.cpp)
target_link_libraries(test_drum_synths PRIVATE eatsbits_core)
add_test(NAME DrumSynthsTest COMMAND test_drum_synths)

add_executable(test_sequencer_project tests/test_sequencer_project.cpp)
target_link_libraries(test_sequencer_project PRIVATE eatsbits_core)
add_test(NAME SequencerProjectTest COMMAND test_sequencer_project)

add_executable(test_ui_canvas tests/test_ui_canvas.cpp)
target_link_libraries(test_ui_canvas PRIVATE eatsbits_gui_core)
add_test(NAME UiCanvasTest COMMAND test_ui_canvas)

add_executable(test_wav_export tests/test_wav_export.cpp)
target_link_libraries(test_wav_export PRIVATE eatsbits_core)
add_test(NAME WavExportTest COMMAND test_wav_export)

add_executable(test_gui_interaction tests/test_gui_interaction.cpp)
if(WIN32)
    target_link_libraries(test_gui_interaction PRIVATE eatsbits_gui_core "${WGPU_NATIVE_LIB}")
    add_custom_command(TARGET test_gui_interaction POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wgpu/lib/wgpu_native.dll"
        "$<TARGET_FILE_DIR:test_gui_interaction>"
    )
else()
    target_link_libraries(test_gui_interaction PRIVATE eatsbits_gui_core)
endif()
add_test(NAME GuiInteractionTest COMMAND test_gui_interaction)

add_executable(test_preset_loader tests/test_preset_loader.cpp)
target_link_libraries(test_preset_loader PRIVATE eatsbits_gui_core)
add_test(NAME PresetLoaderTest COMMAND test_preset_loader)

add_executable(test_sid_synth tests/test_sid_synth.cpp)
target_link_libraries(test_sid_synth PRIVATE eatsbits_core)
add_test(NAME SidSynthTest COMMAND test_sid_synth)

add_executable(test_dx7_synth tests/test_dx7_synth.cpp)
target_link_libraries(test_dx7_synth PRIVATE eatsbits_core)
add_test(NAME Dx7SynthTest COMMAND test_dx7_synth)

add_executable(test_snes_dsp tests/test_snes_dsp.cpp)
target_link_libraries(test_snes_dsp PRIVATE eatsbits_core)
add_test(NAME SnesDspTest COMMAND test_snes_dsp)

add_executable(test_ym2612_fm tests/test_ym2612_fm.cpp)
target_link_libraries(test_ym2612_fm PRIVATE eatsbits_core)
add_test(NAME Ym2612FmTest COMMAND test_ym2612_fm)

add_executable(test_convolver tests/test_convolver.cpp)
target_link_libraries(test_convolver PRIVATE eatsbits_core)
add_test(NAME ConvolverTest COMMAND test_convolver)

add_executable(test_soundfont tests/test_soundfont.cpp)
target_link_libraries(test_soundfont PRIVATE eatsbits_core)
add_test(NAME SoundFontTest COMMAND test_soundfont)

add_executable(test_physical_modeling tests/test_physical_modeling.cpp)
target_link_libraries(test_physical_modeling PRIVATE eatsbits_core)
add_test(NAME PhysicalModelingTest COMMAND test_physical_modeling)

add_executable(test_studio_fx tests/test_studio_fx.cpp)
target_link_libraries(test_studio_fx PRIVATE eatsbits_core)
add_test(NAME StudioFxTest COMMAND test_studio_fx)

add_executable(test_note_script tests/test_note_script.cpp)
target_link_libraries(test_note_script PRIVATE eatsbits_core)
add_test(NAME NoteScriptTest COMMAND test_note_script)

add_executable(test_dispatch_scanner tests/test_dispatch_scanner.cpp)
target_link_libraries(test_dispatch_scanner PRIVATE eatsbits_core)
add_test(NAME DispatchScannerTest COMMAND test_dispatch_scanner)

add_executable(test_midi_fx tests/test_midi_fx.cpp)
target_link_libraries(test_midi_fx PRIVATE eatsbits_core)
add_test(NAME MidiFxTest COMMAND test_midi_fx)

add_executable(test_macro_eats tests/test_macro_eats.cpp)
target_link_libraries(test_macro_eats PRIVATE eatsbits_core)
add_test(NAME MacroEatsTest COMMAND test_macro_eats)

add_executable(test_svg_path tests/test_svg_path.cpp)
target_link_libraries(test_svg_path PRIVATE eatsbits_gui_core)
add_test(NAME SvgPathTest COMMAND test_svg_path)

add_executable(test_theme tests/test_theme.cpp)
target_link_libraries(test_theme PRIVATE eatsbits_gui_core)
add_test(NAME ThemeTest COMMAND test_theme)

add_executable(test_ui_widgets tests/test_ui_widgets.cpp)
target_link_libraries(test_ui_widgets PRIVATE eatsbits_gui_core eatsbits_core)
add_test(NAME UiWidgetsTest COMMAND test_ui_widgets)

add_executable(test_diff_history tests/test_diff_history.cpp)
target_link_libraries(test_diff_history PRIVATE eatsbits_core)
add_test(NAME DiffHistoryTest COMMAND test_diff_history)

add_executable(test_modular_ui tests/test_modular_ui.cpp)
target_link_libraries(test_modular_ui PRIVATE eatsbits_gui_core)
add_test(NAME ModularUiTest COMMAND test_modular_ui)

add_executable(test_chord_track tests/test_chord_track.cpp)
target_link_libraries(test_chord_track PRIVATE eatsbits_gui_core)
add_test(NAME ChordTrackTest COMMAND test_chord_track)

add_executable(test_icon_system tests/test_icon_system.cpp)
target_link_libraries(test_icon_system PRIVATE eatsbits_gui_core)
add_test(NAME IconSystemTest COMMAND test_icon_system)

add_executable(test_audio_to_midi tests/test_audio_to_midi.cpp)
target_link_libraries(test_audio_to_midi PRIVATE eatsbits_gui_core)
add_test(NAME AudioToMidiTest COMMAND test_audio_to_midi)

add_executable(test_procgen tests/test_procgen.cpp)
target_link_libraries(test_procgen PRIVATE eatsbits_gui_core)
add_test(NAME ProcGenTest COMMAND test_procgen)

add_executable(test_track_freeze tests/test_track_freeze.cpp)
target_link_libraries(test_track_freeze PRIVATE eatsbits_gui_core)
add_test(NAME TrackFreezeTest COMMAND test_track_freeze)

add_executable(test_creative_dialogs tests/test_creative_dialogs.cpp)
target_link_libraries(test_creative_dialogs PRIVATE eatsbits_gui_core)
add_test(NAME CreativeDialogsTest COMMAND test_creative_dialogs)

add_executable(test_ai_assistant tests/test_ai_assistant.cpp)
target_link_libraries(test_ai_assistant PRIVATE eatsbits_gui_core)
add_test(NAME AiAssistantTest COMMAND test_ai_assistant)

add_executable(test_lyric_track tests/test_lyric_track.cpp)
target_link_libraries(test_lyric_track PRIVATE eatsbits_core)
add_test(NAME LyricTrackTest COMMAND test_lyric_track)

add_executable(test_builtin_presets tests/test_builtin_presets.cpp)
target_link_libraries(test_builtin_presets PRIVATE eatsbits_core)
add_test(NAME BuiltinPresetsTest COMMAND test_builtin_presets)

add_executable(test_presenters tests/test_presenters.cpp)
target_link_libraries(test_presenters PRIVATE eatsbits_core)
add_test(NAME PresentersTest COMMAND test_presenters)

add_executable(test_eatscript_host_abi tests/test_eatscript_host_abi.cpp)
target_link_libraries(test_eatscript_host_abi PRIVATE eats_script)
add_test(NAME EatscriptHostAbiTest COMMAND test_eatscript_host_abi)

add_executable(test_webgpu_terminal tests/test_webgpu_terminal.cpp)
target_link_libraries(test_webgpu_terminal PRIVATE eatsbits_gui_core eatsbits_tui_core eats_script)
add_test(NAME WebGpuTerminalTest COMMAND test_webgpu_terminal)

add_executable(test_headless_video_exporter tests/test_headless_video_exporter.cpp)
target_link_libraries(test_headless_video_exporter PRIVATE eats_video_export eats_render_wgpu eats_presenter eats_audio eats_core)
add_test(NAME HeadlessVideoExporterTest COMMAND test_headless_video_exporter)

add_executable(test_terminal_console tests/test_terminal_console.cpp)
target_link_libraries(test_terminal_console PRIVATE eatsbits_gui_core eatsbits_core eats_script)
add_test(NAME TerminalConsoleTest COMMAND test_terminal_console)

add_executable(test_text_engine tests/test_text_engine.cpp)
target_link_libraries(test_text_engine PRIVATE eats_core)
add_test(NAME TextEngineTest COMMAND test_text_engine)

endif() # BUILD_TESTING
