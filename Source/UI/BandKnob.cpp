#include "BandKnob.h"

BandKnob::BandKnob (juce::AudioProcessorValueTreeState& state, const juce::String& name,
                    const char* amountId, const char* enabledId, const char* soloId)
    : enable (name),
      amountAttachment (state, amountId, knob),
      enableAttachment (state, enabledId, enable),
      soloAttachment (state, soloId, solo)
{
    knob.setDoubleClickReturnValue (true, (double) state.getParameter (amountId)->convertFrom0to1 (
                                              state.getParameter (amountId)->getDefaultValue()));
    knob.setTooltip (name + " amount: how strongly resonances in this band are reduced");
    enable.setTooltip ("Turn the " + name + " band on/off");
    solo.setTooltip ("Solo: listen to the " + name + " band on its own");
    enable.onStateChange = [this] { refreshActiveState(); };

    addAndMakeVisible (knob);
    addAndMakeVisible (enable);
    addAndMakeVisible (solo); // added last so it sits on top of the knob
    refreshActiveState();
}

void BandKnob::resized()
{
    auto b = getLocalBounds();
    const int headerH = headerHeight;
    auto header = b.removeFromTop (headerH);

    const int ledW = juce::roundToInt ((float) headerH * 3.4f);
    enable.setBounds (header.withSizeKeepingCentre (ledW, headerH));
    knob.setBounds (b);

    const auto kb = knob.getKnobBounds().translated ((float) knob.getX(), (float) knob.getY());
    const float faceR = kb.getWidth() * 0.5f * 0.8f;
    const int soloSize = juce::roundToInt (faceR * 0.42f);
    solo.setBounds (juce::Rectangle<int> (soloSize, soloSize)
                        .withCentre (kb.getCentre().translated (0.0f, faceR * 0.45f).roundToInt()));
}
