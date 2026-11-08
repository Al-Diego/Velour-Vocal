#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// All parameter IDs live here so the processor, editor and tests agree.
namespace ParamIDs
{
    inline constexpr const char* lowAmount    = "lowAmount";
    inline constexpr const char* midAmount    = "midAmount";
    inline constexpr const char* highAmount   = "highAmount";
    inline constexpr const char* lowEnabled   = "lowEnabled";
    inline constexpr const char* midEnabled   = "midEnabled";
    inline constexpr const char* highEnabled  = "highEnabled";
    inline constexpr const char* lowSolo      = "lowSolo";
    inline constexpr const char* midSolo      = "midSolo";
    inline constexpr const char* highSolo     = "highSolo";
    inline constexpr const char* xoverLowMid  = "xoverLowMid";
    inline constexpr const char* xoverMidHigh = "xoverMidHigh";
    inline constexpr const char* highShape    = "highShape";
    inline constexpr const char* voiceProfile = "voiceProfile";
    inline constexpr const char* dynThreshold = "dynThreshold";
    inline constexpr const char* dynEnabled   = "dynEnabled";
    inline constexpr const char* outputGain   = "outputGain";
    inline constexpr const char* delta        = "delta";
}

namespace Params
{
    inline const juce::StringArray profileNames { "Male", "Female", "Neutral" };
    inline const juce::StringArray shapeNames   { "Focused", "Broad" };

    // Crossover defaults each voice profile snaps to when picked in the UI.
    struct ProfileDefaults { float xoverLowMid, xoverMidHigh; };
    ProfileDefaults defaultsForProfile (int profileIndex) noexcept;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
