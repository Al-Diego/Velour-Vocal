#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "VelourLookAndFeel.h"
#include "BandKnob.h"
#include "BandDisplay.h"
#include "DynFader.h"

class VelourVocalEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit VelourVocalEditor (VelourVocalProcessor&);
    ~VelourVocalEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 760, baseHeight = 400;

private:
    void timerCallback() override;
    void applyProfileCrossovers (int profileIndex);
    juce::Rectangle<int> scaled (float x, float y, float w, float h) const;

    VelourVocalProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    VelourLookAndFeel lnf;
    float scale = 1.0f;
    bool sizeRestored = false;

    juce::ComboBox profileBox, presetBox;
    std::unique_ptr<juce::ParameterAttachment> profileAttachment;

    PillButton deltaButton { juce::String::fromUTF8 ("\xce\x94") };
    juce::AudioProcessorValueTreeState::ButtonAttachment deltaAttachment;

    CurveButton bellButton  { CurveButton::Icon::Bell };
    CurveButton shelfButton { CurveButton::Icon::Shelf };
    std::unique_ptr<juce::ParameterAttachment> shapeAttachment;

    BandKnob lowKnob, midKnob, highKnob;
    BandDisplay lowDisplay, midDisplay, highDisplay;
    DynFader dynFader;

    ArcKnob outKnob { ArcKnob::Style::Small };
    juce::AudioProcessorValueTreeState::SliderAttachment outAttachment;

    juce::TooltipWindow tooltips { this, 700 };
    DisplayFrame frame;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelourVocalEditor)
};
