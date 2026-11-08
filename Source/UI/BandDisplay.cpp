#include "BandDisplay.h"
#include "Parameters.h"

using namespace juce;

namespace
{
    constexpr float maxReductionDb = 12.0f;  // full height of the GR curve
    constexpr float spectrumFloorDb = -90.0f;
}

BandDisplay::BandDisplay (AudioProcessorValueTreeState& state, Role r, float lo, float hi, std::vector<float> t)
    : role (r), minHz (lo), maxHz (hi), ticks (std::move (t)),
      lowMidParam (state.getParameter (ParamIDs::xoverLowMid)),
      midHighParam (state.getParameter (ParamIDs::xoverMidHigh))
{
    spectrum.fill (spectrumFloorDb);

    if (role != Role::High) handles.push_back ({ lowMidParam,  false });
    if (role != Role::Low)  handles.push_back ({ midHighParam, true });

    setTooltip ("Teal: gain reduction. Grey: input spectrum. Drag the handles to move the crossovers (double-click resets).");
}

void BandDisplay::setFrame (const DisplayFrame& f)
{
    for (size_t i = 0; i < reduction.size(); ++i)
    {
        // quick rise, gentle fall for a calm display
        reduction[i] = f.reductionDb[i] > reduction[i] ? f.reductionDb[i]
                                                       : reduction[i] + 0.35f * (f.reductionDb[i] - reduction[i]);
        spectrum[i] += 0.5f * (f.spectrumDb[i] - spectrum[i]);
    }
    repaint();
}

void BandDisplay::setActive (bool a)
{
    if (a != active) { active = a; repaint(); }
}

Rectangle<float> BandDisplay::plotArea() const
{
    auto b = getLocalBounds().toFloat();
    return b.withTrimmedBottom (b.getHeight() * 0.17f).reduced (1.0f);
}

float BandDisplay::xForFreq (float hz) const
{
    const auto p = plotArea();
    return p.getX() + p.getWidth() * std::log (hz / minHz) / std::log (maxHz / minHz);
}

float BandDisplay::freqForX (float x) const
{
    const auto p = plotArea();
    return minHz * std::pow (maxHz / minHz, (x - p.getX()) / p.getWidth());
}

float BandDisplay::handleFreq (const Handle& h) const
{
    return h.param->convertFrom0to1 (h.param->getValue());
}

float BandDisplay::lerpPoints (const std::array<float, DisplayFrame::numPoints>& a, float hz)
{
    const float idx = jlimit (0.0f, (float) (DisplayFrame::numPoints - 1), DisplayFrame::pointForFrequency (hz));
    const int i0 = (int) idx, i1 = jmin (i0 + 1, DisplayFrame::numPoints - 1);
    const float t = idx - (float) i0;
    return a[(size_t) i0] + t * (a[(size_t) i1] - a[(size_t) i0]);
}

String BandDisplay::formatHz (float hz, bool precise)
{
    if (hz < 1000.0f)
        return String (roundToInt (hz)) + (precise ? " Hz" : "");
    const float k = hz / 1000.0f;
    const bool whole = std::abs (k - std::round (k)) < 0.05f;
    return (whole && ! precise ? String (roundToInt (k)) : String (k, precise ? 2 : 1)) + (precise ? " kHz" : "k");
}

//==============================================================================
void BandDisplay::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plot = plotArea();
    const float radius = jmax (3.0f, plot.getHeight() * 0.05f);
    const float scale = plot.getHeight() / 90.0f;

    // screen
    Path screen;
    screen.addRoundedRectangle (plot, radius);
    g.setColour (Theme::screenBg);
    g.fillPath (screen);

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (screen);

        // grid
        g.setColour (Theme::screenGrid);
        for (float t : ticks)
            g.drawVerticalLine (roundToInt (xForFreq (t)), plot.getY(), plot.getBottom());
        for (float db : { 3.0f, 6.0f, 9.0f })
            g.drawHorizontalLine (roundToInt (plot.getY() + plot.getHeight() * db / maxReductionDb), plot.getX(), plot.getRight());

        // input spectrum, from the bottom
        Path spec;
        spec.startNewSubPath (plot.getX(), plot.getBottom());
        for (float x = plot.getX(); x <= plot.getRight() + 1.0f; x += 1.5f)
        {
            const float db = jlimit (spectrumFloorDb, 0.0f, lerpPoints (spectrum, freqForX (x)));
            spec.lineTo (x, plot.getBottom() - plot.getHeight() * (db - spectrumFloorDb) / -spectrumFloorDb);
        }
        spec.lineTo (plot.getRight(), plot.getBottom());
        spec.closeSubPath();
        g.setColour (Theme::textDim.withAlpha (0.13f));
        g.fillPath (spec);
        g.setColour (Theme::textDim.withAlpha (0.3f));
        g.strokePath (spec, PathStrokeType (1.0f));

        // gain reduction, hanging from the top
        Path gr, grLine;
        gr.startNewSubPath (plot.getX(), plot.getY());
        for (float x = plot.getX(); x <= plot.getRight() + 1.0f; x += 1.5f)
        {
            const float db = jlimit (0.0f, maxReductionDb, lerpPoints (reduction, freqForX (x)));
            const float y = plot.getY() + plot.getHeight() * db / maxReductionDb;
            gr.lineTo (x, y);
            if (x <= plot.getX()) grLine.startNewSubPath (x, y); else grLine.lineTo (x, y);
        }
        gr.lineTo (plot.getRight(), plot.getY());
        gr.closeSubPath();

        const auto fill = active ? Theme::accent : Theme::ledOff;
        g.setGradientFill (ColourGradient (fill.withAlpha (0.95f), 0.0f, plot.getY(),
                                           fill.withAlpha (0.45f), 0.0f, plot.getBottom(), false));
        g.fillPath (gr);
        g.setColour (active ? Theme::accentDeep : Theme::textDim);
        g.strokePath (grLine, PathStrokeType (jmax (1.0f, 1.3f * scale)));

        // shade the parts that belong to neighbouring bands
        const float xLM = xForFreq (handleFreq ({ lowMidParam, false }));
        const float xMH = xForFreq (handleFreq ({ midHighParam, true }));
        g.setColour (Theme::bgBottom.withAlpha (0.6f));
        if (role == Role::Low)  g.fillRect (Rectangle<float>::leftTopRightBottom (xLM, plot.getY(), plot.getRight(), plot.getBottom()));
        if (role == Role::Mid)
        {
            g.fillRect (Rectangle<float>::leftTopRightBottom (plot.getX(), plot.getY(), xLM, plot.getBottom()));
            g.fillRect (Rectangle<float>::leftTopRightBottom (xMH, plot.getY(), plot.getRight(), plot.getBottom()));
        }
        if (role == Role::High) g.fillRect (Rectangle<float>::leftTopRightBottom (plot.getX(), plot.getY(), xMH, plot.getBottom()));

        // dB scale
        g.setColour (Theme::textDim.withAlpha (0.8f));
        g.setFont (Theme::font (jmax (8.0f, 9.0f * scale)));
        for (float db : { 3.0f, 6.0f, 9.0f })
            g.drawText ("-" + String (roundToInt (db)),
                        Rectangle<float> (plot.getRight() - 24.0f * scale, plot.getY() + plot.getHeight() * db / maxReductionDb - 11.0f * scale,
                                          21.0f * scale, 10.0f * scale),
                        Justification::centredRight, false);
    }

    // inner edge
    g.setColour (Theme::panelLine);
    g.strokePath (screen, PathStrokeType (1.0f));

    // handles
    for (int i = 0; i < (int) handles.size(); ++i)
    {
        const auto& h = handles[(size_t) i];
        const float f = handleFreq (h);
        if (f < minHz || f > maxHz)
            continue;

        const float x = xForFreq (f);
        const bool hot = i == hoverHandle || i == dragHandle;
        g.setColour (Theme::dark2.withAlpha (hot ? 0.95f : 0.75f));
        g.drawLine (x, plot.getY(), x, plot.getBottom(), jmax (1.0f, 1.5f * scale));

        const float r = jmax (4.0f, 6.5f * scale) * (hot ? 1.15f : 1.0f);
        const Point<float> c (x, h.atTop ? plot.getY() + r + 2.0f : plot.getBottom() - r - 2.0f);
        Path knob;
        knob.addEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        DropShadow (Colours::black.withAlpha (0.3f), roundToInt (r * 0.8f), { 0, 1 }).drawForPath (g, knob);
        g.setColour (Theme::dark);
        g.fillPath (knob);
        g.setColour (Theme::accent);
        g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), jmax (1.2f, r * 0.3f));

        if (hot)
        {
            const auto text = formatHz (f, true);
            const auto font = Theme::font (jmax (9.0f, 10.0f * scale), true);
            g.setFont (font);
            const float w = GlyphArrangement::getStringWidth (font, text) + 10.0f * scale;
            const float hgt = 15.0f * scale;
            auto pill = Rectangle<float> (w, hgt).withCentre ({ x, c.y + (h.atTop ? 1.0f : -1.0f) * (r + hgt) });
            pill.setX (jlimit (plot.getX() + 2.0f, plot.getRight() - w - 2.0f, pill.getX()));
            g.setColour (Theme::dark.withAlpha (0.9f));
            g.fillRoundedRectangle (pill, hgt * 0.5f);
            g.setColour (Colours::white);
            g.drawText (text, pill, Justification::centred, false);
        }
    }

    // frequency labels
    const auto labels = bounds.withTop (plot.getBottom() + 1.0f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (jmax (8.0f, labels.getHeight() * 0.68f)));
    for (float t : ticks)
    {
        const float x = xForFreq (t);
        g.drawText (formatHz (t, false), Rectangle<float> (40.0f * scale, labels.getHeight()).withCentre ({ x, labels.getCentreY() })
                                             .constrainedWithin (labels),
                    Justification::centred, false);
    }
}

//==============================================================================
int BandDisplay::handleAt (float x) const
{
    const float tolerance = jmax (6.0f, plotArea().getHeight() * 0.1f);
    int best = -1;
    float bestDist = tolerance;
    for (int i = 0; i < (int) handles.size(); ++i)
    {
        const float d = std::abs (xForFreq (handleFreq (handles[(size_t) i])) - x);
        if (d <= bestDist) { best = i; bestDist = d; }
    }
    return best;
}

void BandDisplay::mouseMove (const MouseEvent& e)
{
    const int h = handleAt (e.position.x);
    if (h != hoverHandle)
    {
        hoverHandle = h;
        setMouseCursor (h >= 0 ? MouseCursor::LeftRightResizeCursor : MouseCursor::NormalCursor);
        repaint();
    }
}

void BandDisplay::mouseExit (const MouseEvent&)
{
    if (hoverHandle >= 0) { hoverHandle = -1; repaint(); }
}

void BandDisplay::mouseDown (const MouseEvent& e)
{
    dragHandle = handleAt (e.position.x);
    if (dragHandle >= 0)
        handles[(size_t) dragHandle].param->beginChangeGesture();
}

void BandDisplay::mouseDrag (const MouseEvent& e)
{
    if (dragHandle < 0)
        return;

    auto* p = handles[(size_t) dragHandle].param;
    const auto range = p->getNormalisableRange();
    const float hz = jlimit (range.start, range.end, freqForX (e.position.x));
    p->setValueNotifyingHost (p->convertTo0to1 (hz));
    repaint();
}

void BandDisplay::mouseUp (const MouseEvent&)
{
    if (dragHandle >= 0)
        handles[(size_t) dragHandle].param->endChangeGesture();
    dragHandle = -1;
    repaint();
}

void BandDisplay::mouseDoubleClick (const MouseEvent& e)
{
    const int h = handleAt (e.position.x);
    if (h < 0)
        return;

    auto* p = handles[(size_t) h].param;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->getDefaultValue());
    p->endChangeGesture();
    repaint();
}
