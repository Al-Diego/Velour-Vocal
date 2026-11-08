#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "VelourLookAndFeel.h"

/** One band's controls: on/off LED + name, the amount knob, and the solo button
    that sits inside the knob face. */
class BandKnob : public juce::Component
{
public:
    BandKnob (juce::AudioProcessorValueTreeState&, const juce::String& name,
              const char* amountId, const char* enabledId, const char* soloId);

    void resized() override;

    void setReductionDb (float db) { knob.setReductionDb (db); }
    bool isBandEnabled() const     { return enable.getToggleState(); }
    void refreshActiveState()      { knob.setActive (enable.getToggleState()); }
    void setHeaderHeight (int h)   { headerHeight = h; resized(); }

private:
    ArcKnob knob { ArcKnob::Style::Band };
    LedButton enable;
    HeadphoneButton solo;
    int headerHeight = 20;

    juce::AudioProcessorValueTreeState::SliderAttachment amountAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment enableAttachment, soloAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandKnob)
};
