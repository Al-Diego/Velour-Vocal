#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "VelourLookAndFeel.h"

/** Vertical threshold fader. Inside the slot it shows:
    - teal bar from the top: overall gain reduction (0..24 dB)
    - faint bar from the bottom: detected input level on the fader's own scale,
      so you can see when the signal crosses the threshold. */
class FaderSlider : public juce::Slider
{
public:
    FaderSlider();
    void paint (juce::Graphics&) override;

    void setMeters (float reductionDb, float levelDb, bool dynActive);

private:
    float reduction = 0.0f, level = -60.0f;
    bool dynOn = false;
};

class DynFader : public juce::Component
{
public:
    explicit DynFader (juce::AudioProcessorValueTreeState&);
    void resized() override;

    void setMeters (float reductionDb, float levelDb) { fader.setMeters (reductionDb, levelDb, enable.getToggleState()); }

private:
    LedButton enable { "Dyn" };
    FaderSlider fader;
    juce::AudioProcessorValueTreeState::SliderAttachment faderAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment enableAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynFader)
};
