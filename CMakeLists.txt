cmake_minimum_required(VERSION 3.22)

project(VelourVocal VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# MSVC: read sources as UTF-8, build several files in parallel, static runtime
# (so the plugin doesn't need the VC++ redistributable installed on the user's machine).
if(MSVC)
    add_compile_options(/utf-8 /MP)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(VELOUR_COPY_PLUGIN_AFTER_BUILD "Copy plugins into the system plugin folders after building" OFF)
option(VELOUR_BUILD_TESTS "Build the offline DSP test runner" ON)
option(VELOUR_BUILD_PLUGIN "Build the plugin itself" ON)

# ---------------------------------------------------------------------------
# JUCE: use ./JUCE if it exists (git submodule), otherwise download it.
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(juce)
endif()

# ---------------------------------------------------------------------------
# DSP sources shared by the plugin and the test runner
# ---------------------------------------------------------------------------
set(VELOUR_DSP_SOURCES
    Source/DSP/SpectralSuppressor.cpp
    Source/DSP/SpectralSuppressor.h
    Source/DSP/DisplayFifo.h)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
if(VELOUR_BUILD_PLUGIN)
juce_add_plugin(VelourVocal
    PRODUCT_NAME                "Velour Vocal"
    COMPANY_NAME                "Velour Audio"
    BUNDLE_ID                   "com.velouraudio.velourvocal"
    PLUGIN_MANUFACTURER_CODE    Velr
    PLUGIN_CODE                 VlVc
    FORMATS                     VST3 AU Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Dynamics EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     ${VELOUR_COPY_PLUGIN_AFTER_BUILD})

target_sources(VelourVocal PRIVATE
    ${VELOUR_DSP_SOURCES}
    Source/Parameters.h
    Source/Parameters.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/UI/Theme.h
    Source/UI/VelourLookAndFeel.h
    Source/UI/VelourLookAndFeel.cpp
    Source/UI/BandKnob.h
    Source/UI/BandKnob.cpp
    Source/UI/BandDisplay.h
    Source/UI/BandDisplay.cpp
    Source/UI/DynFader.h
    Source/UI/DynFader.cpp
    Source/UI/PluginEditor.h
    Source/UI/PluginEditor.cpp)

target_include_directories(VelourVocal PRIVATE Source)

target_compile_definitions(VelourVocal PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VelourVocal
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------------------
# Offline DSP tests (console app, no GUI, no host needed)
# ---------------------------------------------------------------------------
if(VELOUR_BUILD_TESTS)
    juce_add_console_app(VelourVocalTests PRODUCT_NAME "VelourVocalTests")
    target_sources(VelourVocalTests PRIVATE Tests/DSPTests.cpp ${VELOUR_DSP_SOURCES})
    target_include_directories(VelourVocalTests PRIVATE Source)
    target_compile_definitions(VelourVocalTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(VelourVocalTests
        PRIVATE juce::juce_core juce::juce_audio_basics juce::juce_dsp
        PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
