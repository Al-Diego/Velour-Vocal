#include "Parameters.h"

namespace Params
{
ProfileDefaults defaultsForProfile (int profileIndex) noexcept
{
    switch (profileIndex)
    {
        case 0:  return { 250.0f, 3500.0f };  // Male: lower chest/box region, earlier sibilance
        case 1:  return { 350.0f, 5000.0f };  // Female: higher fundamental, higher sibilance
        default: return { 300.0f, 4000.0f };  // Neutral
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using APF = AudioParameterFloat;
    using APB = AudioParameterBool;
    using APC = AudioParameterChoice;

    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<APF> (ParameterID { id, 1 }, name,
                                      NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                      AudioParameterFloatAttributes()
                                          .withLabel ("%")
                                          .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); }));
    };

    auto hz = [] (float v, int)
    {
        return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz";
    };

    auto db = [] (float v, int) { return String (v, 1) + " dB"; };

    layout.add (percent (ParamIDs::lowAmount,  "Low Amount",  25.0f),
                percent (ParamIDs::midAmount,  "Mid Amount",  40.0f),
                percent (ParamIDs::highAmount, "High Amount", 35.0f));

    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::lowEnabled,  1 }, "Low On",  true),
                std::make_unique<APB> (ParameterID { ParamIDs::midEnabled,  1 }, "Mid On",  true),
                std::make_unique<APB> (ParameterID { ParamIDs::highEnabled, 1 }, "High On", true),
                std::make_unique<APB> (ParameterID { ParamIDs::lowSolo,     1 }, "Low Solo",  false),
                std::make_unique<APB> (ParameterID { ParamIDs::midSolo,     1 }, "Mid Solo",  false),
                std::make_unique<APB> (ParameterID { ParamIDs::highSolo,    1 }, "High Solo", false));

    NormalisableRange<float> lowMidRange (150.0f, 600.0f, 1.0f);
    lowMidRange.setSkewForCentre (300.0f);
    NormalisableRange<float> midHighRange (2000.0f, 8000.0f, 10.0f);
    midHighRange.setSkewForCentre (4000.0f);

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::xoverLowMid, 1 }, "Low/Mid Crossover", lowMidRange, 300.0f,
                                       AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (hz)),
                std::make_unique<APF> (ParameterID { ParamIDs::xoverMidHigh, 1 }, "Mid/High Crossover", midHighRange, 4000.0f,
                                       AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (hz)));

    layout.add (std::make_unique<APC> (ParameterID { ParamIDs::highShape, 1 },    "High Shape",    shapeNames,   0),
                std::make_unique<APC> (ParameterID { ParamIDs::voiceProfile, 1 }, "Voice Profile", profileNames, 2));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::dynThreshold, 1 }, "Dyn Threshold",
                                       NormalisableRange<float> (-24.0f, 0.0f, 0.1f), -12.0f,
                                       AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (db)),
                std::make_unique<APB> (ParameterID { ParamIDs::dynEnabled, 1 }, "Dyn On", false));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::outputGain, 1 }, "Output",
                                       NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                       AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (db)),
                std::make_unique<APB> (ParameterID { ParamIDs::delta, 1 }, "Delta", false));

    return layout;
}
} // namespace Params
