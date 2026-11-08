#include "DynFader.h"
#include "Parameters.h"

using namespace juce;

FaderSlider::FaderSlider()
{
    setSliderStyle (Slider::LinearVertical);
    setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setDoubleClickReturnValue (true, -12.0);
    setTooltip ("Dyn threshold: below it processing eases off, above it processing gets stronger. "
                "Teal = gain reduction, grey = input level.");
}

void FaderSlider::setMeters (float reductionDb, float levelDb, bool dynActive)
{
    const float r = reductionDb > reduction ? reductionDb : reduction + 0.2f * (reductionDb - reduction);
    const float l = levelDb > level ? levelDb : level + 0.2f * (levelDb - level);
    if (std::abs (r - reduction) > 0.01f || std::abs (l - level) > 0.05f || dynActive != dynOn)
    {
        reduction = r; level = l; dynOn = dynActive;
        repaint();
    }
}

void FaderSlider::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth();
    const float thumbH = w * 0.4f;

    // slot
    const float slotW = w * 0.4f;
    auto slot = Rectangle<float> (slotW, b.getHeight() - 2.0f).withCentre (b.getCentre());
    const float radius = slotW * 0.5f;
    g.setGradientFill (ColourGradient (Theme::dark, 0.0f, slot.getY(), Theme::dark2, 0.0f, slot.getBottom(), false));
    g.fillRoundedRectangle (slot, radius);
    g.setColour (Colours::white.withAlpha (0.9f));
    g.drawRoundedRectangle (slot.expanded (1.0f), radius + 1.0f, 1.0f);

    auto inner = slot.reduced (slotW * 0.18f, slotW * 0.25f);
    const float innerR = inner.getWidth() * 0.5f;

    // input level on the fader scale (bottom up)
    const float yTop = (float) getPositionOfValue (0.0), yBottom = (float) getPositionOfValue (-24.0);
    if (level > -24.0f)
    {
        const float y = jlimit (inner.getY(), inner.getBottom(), (float) getPositionOfValue (jmin (0.0f, level)));
        g.setColour (Colours::white.withAlpha (dynOn ? 0.18f : 0.08f));
        g.fillRoundedRectangle (inner.withTop (y), innerR);
    }

    // gain reduction (top down, 0..24 dB)
    const float grProp = jlimit (0.0f, 1.0f, reduction / 24.0f);
    if (grProp > 0.002f)
    {
        auto bar = inner.withHeight (jmax (innerR * 2.0f, inner.getHeight() * grProp));
        g.setGradientFill (ColourGradient (Theme::accent.brighter (0.3f), 0.0f, bar.getY(),
                                           Theme::accent.withAlpha (0.6f), 0.0f, bar.getBottom(), false));
        g.fillRoundedRectangle (bar, innerR);
    }

    // scale ticks on the left
    g.setColour (Theme::textDim.withAlpha (0.7f));
    for (int db = 0; db >= -24; db -= 6)
    {
        const float y = jmap ((float) db, 0.0f, -24.0f, yTop, yBottom);
        g.drawLine (slot.getX() - w * 0.16f, y, slot.getX() - w * 0.06f, y, 1.0f);
    }

    // thumb
    const float y = (float) getPositionOfValue (getValue());
    auto thumb = Rectangle<float> (w * 0.92f, thumbH).withCentre ({ b.getCentreX(), y });
    Path p;
    p.addRoundedRectangle (thumb, thumbH * 0.5f);
    DropShadow (Colours::black.withAlpha (0.35f), roundToInt (thumbH * 0.3f), { 0, roundToInt (thumbH * 0.08f) }).drawForPath (g, p);
    g.setGradientFill (ColourGradient (Theme::knobLight, 0.0f, thumb.getY(), Theme::knobShade, 0.0f, thumb.getBottom(), false));
    g.fillPath (p);
    g.setColour (Colours::white);
    g.drawRoundedRectangle (thumb.reduced (0.5f), thumbH * 0.5f, 1.0f);

    // little side grips
    g.setColour (dynOn ? Theme::accentDeep : Theme::textDim);
    for (float side : { -1.0f, 1.0f })
    {
        const float x = thumb.getCentreX() + side * thumb.getWidth() * 0.4f;
        g.drawLine (x, y - thumbH * 0.18f, x, y + thumbH * 0.18f, jmax (1.2f, thumbH * 0.06f));
    }

    g.setColour (Theme::text);
    g.setFont (Theme::font (thumbH * 0.46f, true));
    g.drawText (String (getValue(), 1), thumb, Justification::centred, false);
}

//==============================================================================
DynFader::DynFader (AudioProcessorValueTreeState& state)
    : faderAttachment (state, ParamIDs::dynThreshold, fader),
      enableAttachment (state, ParamIDs::dynEnabled, enable)
{
    enable.setTooltip ("Level-dependent processing on/off");
    addAndMakeVisible (enable);
    addAndMakeVisible (fader);
}

void DynFader::resized()
{
    auto b = getLocalBounds();
    const int headerH = roundToInt ((float) getHeight() * 0.1f);
    enable.setBounds (b.removeFromTop (headerH).withSizeKeepingCentre (roundToInt ((float) headerH * 3.0f), headerH));
    b.removeFromTop (headerH / 3);
    fader.setBounds (b);
}
