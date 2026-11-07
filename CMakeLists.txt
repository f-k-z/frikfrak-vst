cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE STRING "")

project(FrikfrakVST VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

juce_add_binary_data(FrikfrakAssets SOURCES shared/assets/logo.svg)  # -> BinaryData::logo_svg

# Common setup for every plugin target in plugins/*
function(frikfrak_plugin_setup target)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/shared)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target}
        PRIVATE juce::juce_audio_utils FrikfrakAssets
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endfunction()

add_subdirectory(plugins/rhodes)
add_subdirectory(plugins/vinyl-noise)
add_subdirectory(plugins/kankyo)

# Pure C++ DSP tests (no JUCE). CHECK macro, so they run in every build type.
enable_testing()
add_executable(dsp_tests tests/dsp_tests.cpp)
target_include_directories(dsp_tests PRIVATE shared plugins/rhodes/src plugins/vinyl-noise/src plugins/kankyo/src plugins/glitch/src)
add_test(NAME dsp_tests COMMAND dsp_tests)
