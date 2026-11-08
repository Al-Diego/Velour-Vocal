#include "VelourLookAndFeel.h"

using namespace juce;

VelourLookAndFeel::VelourLookAndFeel()
{
    setColour (ComboBox::textColourId,                 Colours::white);
    setColour (ComboBox::arrowColourId,                Colours::white);
    setColour (PopupMenu::backgroundColourId,          Theme::dark2);
    setColour (PopupMenu::textColourId,                Colours::white);
    setColour (PopupMenu::highlightedBackgroundColourId, Theme::accentDeep);
    setColour (PopupMenu::highlightedTextColourId,     Colours::white);
    setColour (TooltipWindow::backgroundColourId,      Theme::dark2);
    setColour (TooltipWindow::textColourId,            Colours::white);
    setColour (TooltipWindow::outlineColourId,         Colours::transparentBlack);
}

void VelourLookAndFeel::drawComboBox (Graphics& g, int width, int height, bool isButtonDown,
                                      int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const float radius = r.getHeight() * 0.5f;

    g.setGradientFill (ColourGradient (Theme::dark2.brighter (box.isMouseOver() ? 0.15f : 0.0f), 0.0f, r.getY(),
                                       Theme::dark, 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (Colours::white.withAlpha (isButtonDown ? 0.25f : 0.1f));
    g.drawRoundedRectangle (r, radius, 1.0f);

    // small down-arrow
    const float s = r.getHeight() * 0.18f;
    const float cx = r.getRight() - r.getHeight() * 0.6f, cy = r.getCentreY();
    Path arrow;
    arrow.addTriangle (cx - s, cy - s * 0.5f, cx + s, cy - s * 0.5f, cx, cy + s * 0.6f);
    g.setColour (Colours::white.withAlpha (0.85f));
    g.fillPath (arrow);
}

void VelourLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    const int h = box.getHeight();
    label.setBounds (h / 2, 0, box.getWidth() - h - h / 2, h);
    label.setFont (getComboBoxFont (box));
}

Font VelourLookAndFeel::getComboBoxFont (ComboBox& box)
{
    return Theme::font ((float) box.getHeight() * 0.5f, true);
}

Font VelourLookAndFeel::getPopupMenuFont()
{
    return Theme::font (15.0f * uiScale);
}

void VelourLookAndFeel::drawPopupMenuBackground (Graphics& g, int width, int height)
{
    g.fillAll (Theme::dark2);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRect (0, 0, width, height);
}

int VelourLookAndFeel::getSliderThumbRadius (Slider& s)
{
    if (s.getSliderStyle() == Slider::LinearVertical)
        return roundToInt ((float) s.getWidth() * 0.2f);
    return LookAndFeel_V4::getSliderThumbRadius (s);
}

//==============================================================================
LedButton::LedButton (const String& caption) : Button (caption)
{
    setClickingTogglesState (true);
}

void LedButton::paintButton (Graphics& g, bool highlighted, bool)
{
    auto b = getLocalBounds().toFloat();
    const float d = b.getHeight() * 0.5f;
    auto led = Rectangle<float> (d, d).withCentre ({ b.getX() + d * 1.0f, b.getCentreY() });
    const bool on = getToggleState();

    if (on)
    {
        g.setColour (Theme::accent.withAlpha (0.28f));
        g.fillEllipse (led.expanded (d * 0.35f));
        g.setGradientFill (ColourGradient (Theme::accent.brighter (0.5f), led.getCentreX(), led.getY(),
                                           Theme::accentDeep, led.getCentreX(), led.getBottom(), false));
    }
    else
    {
        g.setGradientFill (ColourGradient (Theme::ledOff.darker (0.15f), led.getCentreX(), led.getY(),
                                           Theme::ledOff.brighter (0.2f), led.getCentreX(), led.getBottom(), false));
    }
    g.fillEllipse (led);
    g.setColour (Colours::black.withAlpha (0.15f));
    g.drawEllipse (led, 0.8f);

    if (getButtonText().isNotEmpty())
    {
        g.setColour (Theme::text.withAlpha (highlighted ? 1.0f : 0.85f));
        g.setFont (Theme::font (b.getHeight() * 0.66f));
        g.drawText (getButtonText(), b.withTrimmedLeft (d * 2.0f), Justification::centredLeft, false);
    }
}

//==============================================================================
HeadphoneButton::HeadphoneButton() : Button ("Solo")
{
    setClickingTogglesState (true);
}

void HeadphoneButton::paintButton (Graphics& g, bool highlighted, bool)
{
    auto b = getLocalBounds().toFloat();
    const float s = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const bool on = getToggleState();

    if (on)
    {
        g.setColour (Theme::accent.withAlpha (0.18f));
        g.fillEllipse (Rectangle<float> (s, s).withCentre (c));
    }

    Path band;
    band.addCentredArc (c.x, c.y + s * 0.06f, s * 0.27f, s * 0.27f, 0.0f,
                        -MathConstants<float>::halfPi * 1.05f, MathConstants<float>::halfPi * 1.05f, true);

    const auto colour = on ? Theme::accent
                           : Colours::white.withAlpha (highlighted ? 0.7f : 0.38f);
    g.setColour (colour);
    g.strokePath (band, PathStrokeType (s * 0.075f, PathStrokeType::curved, PathStrokeType::rounded));

    const float cupW = s * 0.13f, cupH = s * 0.24f, cupY = c.y + s * 0.02f;
    g.fillRoundedRectangle (c.x - s * 0.33f, cupY, cupW, cupH, cupW * 0.4f);
    g.fillRoundedRectangle (c.x + s * 0.33f - cupW, cupY, cupW, cupH, cupW * 0.4f);
}

//==============================================================================
PillButton::PillButton (const String& glyph) : Button (glyph)
{
    setClickingTogglesState (true);
}

void PillButton::paintButton (Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const float radius = b.getHeight() * 0.5f;
    const bool on = getToggleState();

    Path pill;
    pill.addRoundedRectangle (b, radius);
    DropShadow (Colours::black.withAlpha (0.18f), roundToInt (b.getHeight() * 0.2f), { 0, 1 }).drawForPath (g, pill);

    if (on)
        g.setGradientFill (ColourGradient (Theme::accent.brighter (0.2f), 0.0f, b.getY(), Theme::accentDeep, 0.0f, b.getBottom(), false));
    else
        g.setGradientFill (ColourGradient (Theme::knobLight, 0.0f, b.getY(),
                                           Theme::knobShade.darker (down ? 0.1f : 0.0f), 0.0f, b.getBottom(), false));
    g.fillPath (pill);

    g.setColour (Colours::white.withAlpha (highlighted ? 1.0f : 0.8f));
    g.drawRoundedRectangle (b.reduced (0.5f), radius, 0.8f);

    g.setColour (on ? Colours::white : Theme::text);
    g.setFont (Theme::font (b.getHeight() * 0.62f, true));
    g.drawText (getButtonText(), b, Justification::centred, false);
}

//==============================================================================
CurveButton::CurveButton (Icon i) : Button (i == Icon::Bell ? "Focused" : "Broad"), icon (i) {}

void CurveButton::paintButton (Graphics& g, bool highlighted, bool)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float radius = b.getHeight() * 0.3f;
    const bool on = getToggleState();

    Path box;
    box.addRoundedRectangle (b, radius);
    if (! on)
        DropShadow (Colours::black.withAlpha (0.14f), roundToInt (b.getHeight() * 0.18f), { 0, 1 }).drawForPath (g, box);

    if (on)
        g.setGradientFill (ColourGradient (Theme::dark2, 0.0f, b.getY(), Theme::dark, 0.0f, b.getBottom(), false));
    else
        g.setGradientFill (ColourGradient (Theme::knobLight, 0.0f, b.getY(), Theme::knobShade, 0.0f, b.getBottom(), false));
    g.fillPath (box);

    // curve icon
    auto area = b.reduced (b.getWidth() * 0.2f, b.getHeight() * 0.28f);
    Path curve;
    constexpr int steps = 24;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        float v; // 0 = bottom, 1 = top
        if (icon == Icon::Bell)
            v = std::exp (-std::pow ((t - 0.5f) / 0.13f, 2.0f));
        else
            v = 1.0f / (1.0f + std::exp (-(t - 0.5f) * 11.0f));
        const float x = area.getX() + t * area.getWidth();
        const float y = area.getBottom() - v * area.getHeight();
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    g.setColour (on ? Theme::accent : Theme::textDim.withAlpha (highlighted ? 1.0f : 0.75f));
    g.strokePath (curve, PathStrokeType (jmax (1.2f, b.getHeight() * 0.09f), PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
ArcKnob::ArcKnob (Style s) : style (s)
{
    setSliderStyle (Slider::RotaryVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setVelocityBasedMode (false);
    setMouseDragSensitivity (style == Style::Band ? 260 : 200);
}

void ArcKnob::setReductionDb (float db)
{
    // fast attack, slow fall so the ring is readable
    const float next = db > reductionDb ? db : reductionDb + (db - reductionDb) * 0.15f;
    if (std::abs (next - reductionDb) > 0.01f)
    {
        reductionDb = next;
        repaint();
    }
}

void ArcKnob::setActive (bool a)
{
    if (a != active) { active = a; repaint(); }
}

Rectangle<float> ArcKnob::getKnobBounds() const
{
    auto b = getLocalBounds().toFloat();
    const float size = jmin (b.getWidth(), b.getHeight()) * 0.88f;
    return Rectangle<float> (size, size).withCentre (b.getCentre().translated (0.0f, -size * 0.02f));
}

void ArcKnob::paint (Graphics& g)
{
    const auto b = getKnobBounds();
    const float R = b.getWidth() * 0.5f;
    const auto c = b.getCentre();
    const auto rp = getRotaryParameters();
    const float start = rp.startAngleRadians, end = rp.endAngleRadians;
    const float prop = (float) valueToProportionOfLength (getValue());
    const float angle = start + prop * (end - start);
    const auto accent = active ? Theme::accent : Theme::ledOff;

    // light puck with a soft drop shadow
    Path puck;
    puck.addEllipse (b);
    DropShadow (Colours::black.withAlpha (0.26f), roundToInt (R * 0.24f), { 0, roundToInt (R * 0.1f) }).drawForPath (g, puck);
    g.setGradientFill (ColourGradient (Theme::knobLight, c.x, b.getY(), Theme::knobShade, c.x, b.getBottom(), false));
    g.fillPath (puck);
    g.setColour (Colours::white);
    g.drawEllipse (b.reduced (1.0f), 1.0f);

    if (style == Style::Band)
    {
        const float r = R * 0.80f;
        const auto face = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);

        // recessed edge
        g.setColour (Colours::black.withAlpha (0.16f));
        g.fillEllipse (face.expanded (R * 0.035f).translated (0.0f, -R * 0.015f));
        g.setGradientFill (ColourGradient (Theme::dark2, c.x, face.getY(), Theme::dark, c.x, face.getBottom(), false));
        g.fillEllipse (face);

        const float arcR = r * 0.84f, thick = jmax (2.0f, r * 0.075f);
        const PathStrokeType stroke (thick, PathStrokeType::curved, PathStrokeType::rounded);

        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.strokePath (track, stroke);

        if (prop > 0.001f)
        {
            Path value;
            value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, angle, true);
            g.setColour (accent.withAlpha (0.22f));
            g.strokePath (value, PathStrokeType (thick * 2.4f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (value, stroke);
        }

        // live gain-reduction ring (0..12 dB)
        const float grProp = jlimit (0.0f, 1.0f, reductionDb / 12.0f);
        if (grProp > 0.004f && active)
        {
            const float grR = r * 0.69f;
            Path gr;
            gr.addCentredArc (c.x, c.y, grR, grR, 0.0f, start, start + grProp * (end - start), true);
            g.setColour (Colours::white.withAlpha (0.5f));
            g.strokePath (gr, PathStrokeType (jmax (1.2f, r * 0.03f), PathStrokeType::curved, PathStrokeType::rounded));
        }

        g.setColour (Colours::white.withAlpha (active ? 0.92f : 0.45f));
        g.setFont (Theme::font (r * 0.3f));
        g.drawText (String (roundToInt (getValue())),
                    Rectangle<float> (r * 1.3f, r * 0.42f).withCentre (c.translated (0.0f, -r * 0.08f)),
                    Justification::centred, false);
    }
    else // Small: bipolar arc and pointer
    {
        const float arcR = R * 0.78f, thick = jmax (2.0f, R * 0.09f);
        const PathStrokeType stroke (thick, PathStrokeType::curved, PathStrokeType::rounded);
        const float mid = start + 0.5f * (end - start);

        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
        g.setColour (Theme::knobShade.darker (0.15f));
        g.strokePath (track, stroke);

        if (std::abs (angle - mid) > 0.01f)
        {
            Path value;
            value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (mid, angle), jmax (mid, angle), true);
            g.setColour (accent);
            g.strokePath (value, stroke);
        }

        const float capR = R * 0.52f;
        g.setGradientFill (ColourGradient (Theme::dark2, c.x, c.y - capR, Theme::dark, c.x, c.y + capR, false));
        g.fillEllipse (Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c));

        const auto tip = c.getPointOnCircumference (capR * 0.78f, angle);
        const auto base = c.getPointOnCircumference (capR * 0.3f, angle);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.drawLine ({ base, tip }, jmax (1.5f, R * 0.07f));
    }
}
