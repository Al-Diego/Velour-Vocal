#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/DisplayFifo.h"
#include "Theme.h"

/** Shows one band's slice of the spectrum: the input spectrum (faint, from the
    bottom) and the gain-reduction curve (teal, hanging from the top).
    The area belonging to neighbouring bands is shaded. Crossover handles can
    be dragged left/right (double-click resets). */
class BandDisplay : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    enum class Role { Low, Mid, High };

    BandDisplay (juce::AudioProcessorValueTreeState&, Role, float minHz, float maxHz, std::vector<float> ticksHz);

    void setFrame (const DisplayFrame&);
    void setActive (bool);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Handle
    {
        juce::RangedAudioParameter* param;
        bool atTop;
    };

    juce::Rectangle<float> plotArea() const;
    float xForFreq (float hz) const;
    float freqForX (float x) const;
    float handleFreq (const Handle&) const;
    int handleAt (float x) const;
    static float lerpPoints (const std::array<float, DisplayFrame::numPoints>&, float hz);
    static juce::String formatHz (float hz, bool precise);

    Role role;
    float minHz, maxHz;
    std::vector<float> ticks;
    std::vector<Handle> handles;
    juce::RangedAudioParameter* lowMidParam;
    juce::RangedAudioParameter* midHighParam;

    std::array<float, DisplayFrame::numPoints> reduction {}, spectrum {};
    int hoverHandle = -1, dragHandle = -1;
    bool active = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandDisplay)
};
