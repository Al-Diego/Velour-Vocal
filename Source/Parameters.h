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

    // Factory presets. They set the band amounts, high shape and Dyn, reset
    // delta/solo/output, and use the crossovers of the *current* voice profile
    // (the profile itself is left as the user set it).
    struct FactoryPreset
    {
        const char* name;
        float low, mid, high;   // 0..100
        bool highBroad;
        bool dynOn;
        float dynThresholdDb;
    };

    inline constexpr FactoryPreset factoryPresets[] =
    {
        { "Default",        25.0f, 40.0f, 35.0f, false, false, -12.0f },
        { "Clean",          15.0f, 30.0f, 30.0f, true,  false, -12.0f },
        { "De-Ess",          0.0f, 10.0f, 55.0f, false, false, -12.0f },
        { "Tame Harshness", 10.0f, 50.0f, 40.0f, false, true,  -14.0f },
        { "Remove Mud",     45.0f, 25.0f,  0.0f, false, false, -12.0f },
    };
    inline constexpr int numFactoryPresets = (int) (sizeof (factoryPresets) / sizeof (factoryPresets[0]));
}
