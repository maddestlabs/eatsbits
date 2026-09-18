cmake_minimum_required(VERSION 3.20)
project(eatsbits VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler optimizations and warnings
if(MSVC)
    add_compile_options(/W4 /O2 /permissive- /wd4244 /wd4267)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -O3)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Global Includes
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanovg
)

# Core Engine Static Library
add_library(eatsbits_core STATIC
    src/audio/audio_engine.cpp
    src/eatscript/lexer.cpp
    src/eatscript/parser.cpp
    src/eatscript/vm.cpp
    src/eatscript/transpiler.cpp
    src/plugins/builtin_303.cpp
    third_party/nanovg/nanovg.c
)

# Platform Libraries
if(WIN32)
    target_link_libraries(eatsbits_core PUBLIC ole32 winmm)
elseif(APPLE)
    target_link_libraries(eatsbits_core PUBLIC "-framework CoreAudio" "-framework AudioToolbox")
elseif(UNIX)
    target_link_libraries(eatsbits_core PUBLIC pthread dl m)
endif()

# CLI Application
add_executable(eatsbits_cli src/main.cpp)
target_link_libraries(eatsbits_cli PRIVATE eatsbits_core)

# Tests
enable_testing()

add_executable(test_audio_realtime tests/test_audio_realtime.cpp)
target_link_libraries(test_audio_realtime PRIVATE eatsbits_core)
add_test(NAME AudioRealtimeTest COMMAND test_audio_realtime)

add_executable(test_eatscript_vm tests/test_eatscript_vm.cpp)
target_link_libraries(test_eatscript_vm PRIVATE eatsbits_core)
add_test(NAME EatscriptVmTest COMMAND test_eatscript_vm)

add_executable(test_transpiler tests/test_transpiler.cpp)
target_link_libraries(test_transpiler PRIVATE eatsbits_core)
add_test(NAME TranspilerTest COMMAND test_transpiler)
