# ETLimit - professional mastering limiter (VST3)
#
# Build layout
#   core/    pure C++ DSP library (no JUCE). Everything audio-related lives here
#            so it can be unit-tested and measured without a DAW.
#   plugin/  thin JUCE wrapper: parameters, host communication, GUI.
#   tests/   C++ unit tests (Catch2), an offline render tool and the
#            independent Python reference measurements.

cmake_minimum_required(VERSION 3.24)

# Honour CMAKE_MSVC_RUNTIME_LIBRARY (policy CMP0091).
cmake_policy(SET CMP0091 NEW)

project(ETLimit VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows: link the C++ runtime statically into the plugin, so users don't
# need to install the Visual C++ Redistributable for it to load.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Plugins are shared libraries, so every static library they link must be
# position independent on Linux/macOS.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ETLIMIT_BUILD_PLUGIN "Build the JUCE VST3 plugin" ON)
option(ETLIMIT_BUILD_TESTS  "Build unit tests and the offline render tool" ON)

include(FetchContent)

# Shared warning settings. We deliberately do NOT enable -ffast-math or
# /fp:fast anywhere: it lets the compiler assume NaN/Inf never happen, which
# would silently remove our NaN protection and can change rounding in ways
# that break the limiter's ceiling guarantee.
add_library(etlimit_warnings INTERFACE)
if(MSVC)
    target_compile_options(etlimit_warnings INTERFACE /W4 /permissive-)
else()
    target_compile_options(etlimit_warnings INTERFACE -Wall -Wextra -Wshadow -Wnon-virtual-dtor)
endif()

add_subdirectory(core)

if(ETLIMIT_BUILD_PLUGIN)
    # JUCE is pinned to an exact release so every build uses identical code.
    # To build offline, pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(plugin)
endif()

if(ETLIMIT_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.9.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()
