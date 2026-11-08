#include "PluginEditor.h"

using namespace juce;

namespace
{
    const Identifier uiWidthId ("uiWidth");
}

VelourVocalEditor::VelourVocalEditor (VelourVocalProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      state (p.apvts),
      deltaAttachment (state, ParamIDs::delta, deltaButton),
      lowKnob  (state, "Low",  ParamIDs::lowAmount,  ParamIDs::lowEnabled,  ParamIDs::lowSolo),
      midKnob  (state, "Mid",  ParamIDs::midAmount,  ParamIDs::midEnabled,  ParamIDs::midSolo),
      highKnob (state, "High", ParamIDs::highAmount, ParamIDs::highEnabled, ParamIDs::highSolo),
      lowDisplay  (state, BandDisplay::Role::Low,  20.0f,   700.0f,  { 50.0f, 100.0f, 200.0f, 500.0f }),
      midDisplay  (state, BandDisplay::Role::Mid,  120.0f,  10000.0f, { 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f }),
      highDisplay (state, BandDisplay::Role::High, 1800.0f, 20000.0f, { 3000.0f, 5000.0f, 8000.0f, 12000.0f }),
      dynFader (state),
      outAttachment (state, ParamIDs::outputGain, outKnob)
{
    setLookAndFeel (&lnf);

    // --- voice profile (manual attachment so only *user* changes reset the crossovers)
    profileBox.addItemList (Params::profileNames, 1);
    profileBox.setTooltip ("Voice profile: tunes detection to the voice and sets typical crossover points");
    addAndMakeVisible (profileBox);
    profileAttachment = std::make_unique<ParameterAttachment> (
        *state.getParameter (ParamIDs::voiceProfile),
        [this] (float v) { profileBox.setSelectedItemIndex (roundToInt (v), dontSendNotification); });
    profileAttachment->sendInitialUpdate();
    profileBox.onChange = [this]
    {
        const int index = profileBox.getSelectedItemIndex();
        profileAttachment->setValueAsCompleteGesture ((float) index);
        applyProfileCrossovers (index);
    };

    // --- delta
    deltaButton.setTooltip ("Delta: hear only what is being removed");
    addAndMakeVisible (deltaButton);

    // --- high band shape (radio pair)
    bellButton.setTooltip ("Focused: targets narrow peaks such as sibilance");
    shelfButton.setTooltip ("Broad: gentler, wider smoothing of the top end");
    shapeAttachment = std::make_unique<ParameterAttachment> (
        *state.getParameter (ParamIDs::highShape),
        [this] (float v)
        {
            bellButton.setToggleState (v < 0.5f, dontSendNotification);
            shelfButton.setToggleState (v >= 0.5f, dontSendNotification);
        });
    shapeAttachment->sendInitialUpdate();
    bellButton.onClick  = [this] { shapeAttachment->setValueAsCompleteGesture (0.0f); };
    shelfButton.onClick = [this] { shapeAttachment->setValueAsCompleteGesture (1.0f); };
    addAndMakeVisible (bellButton);
    addAndMakeVisible (shelfButton);

    for (Component* c : { (Component*) &lowKnob, (Component*) &midKnob, (Component*) &highKnob,
                          (Component*) &lowDisplay, (Component*) &midDisplay, (Component*) &highDisplay,
                          (Component*) &dynFader })
        addAndMakeVisible (c);

    outKnob.setDoubleClickReturnValue (true, 0.0);
    outKnob.setTooltip ("Output gain");
    outKnob.onValueChange = [this] { repaint (scaled (640, 340, 110, 40)); };
    addAndMakeVisible (outKnob);

    // --- size: resizable, fixed aspect ratio, remembered in the plugin state
    const int w = jlimit (baseWidth * 3 / 4, baseWidth * 2, (int) state.state.getProperty (uiWidthId, baseWidth));
    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (w, roundToInt ((float) w * baseHeight / baseWidth));
    sizeRestored = true;

    startTimerHz (30);
}

VelourVocalEditor::~VelourVocalEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VelourVocalEditor::applyProfileCrossovers (int profileIndex)
{
    const auto d = Params::defaultsForProfile (profileIndex);
    for (auto [id, hz] : { std::pair<const char*, float> { ParamIDs::xoverLowMid, d.xoverLowMid },
                           std::pair<const char*, float> { ParamIDs::xoverMidHigh, d.xoverMidHigh } })
    {
        auto* param = state.getParameter (id);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (hz));
        param->endChangeGesture();
    }
}

Rectangle<int> VelourVocalEditor::scaled (float x, float y, float w, float h) const
{
    return Rectangle<float> (x * scale, y * scale, w * scale, h * scale).toNearestInt();
}

//==============================================================================
void VelourVocalEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Theme::bgTop, 0.0f, 0.0f, Theme::bgBottom, 0.0f, b.getBottom(), false));
    g.fillAll();

    // title: "Velour | Vocal"
    {
        const auto bold = Theme::font (25.0f * scale, true);
        const auto light = Theme::font (25.0f * scale);
        const float x = 24.0f * scale, y = 14.0f * scale, h = 30.0f * scale;
        const float w1 = GlyphArrangement::getStringWidth (bold, "Velour");
        g.setColour (Theme::text);
        g.setFont (bold);
        g.drawText ("Velour", Rectangle<float> (x, y, w1 + 2.0f, h), Justification::centredLeft, false);
        g.setColour (Theme::accent);
        g.fillRect (Rectangle<float> (x + w1 + 6.0f * scale, y + h * 0.2f, 1.5f * scale, h * 0.6f));
        g.setColour (Theme::text.withAlpha (0.75f));
        g.setFont (light);
        g.drawText ("Vocal", Rectangle<float> (x + w1 + 12.0f * scale, y, 120.0f * scale, h), Justification::centredLeft, false);
    }

    // embossed divider before the dynamics column
    const float dx = 626.0f * scale;
    g.setColour (Theme::panelLine);
    g.drawLine (dx, 22.0f * scale, dx, 378.0f * scale, 1.0f);
    g.setColour (Colours::white.withAlpha (0.8f));
    g.drawLine (dx + 1.0f, 22.0f * scale, dx + 1.0f, 378.0f * scale, 1.0f);

    // Out label + value
    g.setColour (Theme::text);
    g.setFont (Theme::font (14.0f * scale));
    g.drawText ("Out", scaled (640, 342, 110, 18), Justification::centred, false);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (12.0f * scale));
    const auto out = (float) outKnob.getValue();
    g.drawText ((out > 0.05f ? "+" : "") + String (out, 1) + " dB", scaled (640, 360, 110, 16), Justification::centred, false);
}

void VelourVocalEditor::resized()
{
    scale = (float) getWidth() / (float) baseWidth;
    lnf.uiScale = scale;
    if (sizeRestored && getWidth() > 0)
        state.state.setProperty (uiWidthId, getWidth(), nullptr);

    profileBox.setBounds (scaled (24, 54, 122, 26));
    deltaButton.setBounds (scaled (448, 20, 48, 30));

    const int header = roundToInt (22.0f * scale);
    for (auto* k : { &lowKnob, &midKnob, &highKnob })
        k->setHeaderHeight (header);

    lowKnob.setBounds  (scaled (20,  100, 140, 156));
    midKnob.setBounds  (scaled (193, 14,  240, 248));
    highKnob.setBounds (scaled (466, 100, 140, 156));

    bellButton.setBounds  (scaled (500, 68, 34, 22));
    shelfButton.setBounds (scaled (538, 68, 34, 22));

    lowDisplay.setBounds  (scaled (20,  268, 140, 112));
    midDisplay.setBounds  (scaled (176, 268, 274, 112));
    highDisplay.setBounds (scaled (466, 268, 140, 112));

    dynFader.setBounds (scaled (656, 16, 72, 262));
    outKnob.setBounds  (scaled (663, 286, 58, 58));
}

void VelourVocalEditor::timerCallback()
{
    auto& engine = processor.getEngine();

    if (engine.getDisplayFifo().pullLatest (frame))
    {
        lowDisplay.setFrame (frame);
        midDisplay.setFrame (frame);
        highDisplay.setFrame (frame);
    }

    lowKnob.setReductionDb  (engine.getBandReductionDb (SpectralSuppressor::Low));
    midKnob.setReductionDb  (engine.getBandReductionDb (SpectralSuppressor::Mid));
    highKnob.setReductionDb (engine.getBandReductionDb (SpectralSuppressor::High));

    lowDisplay.setActive  (lowKnob.isBandEnabled());
    midDisplay.setActive  (midKnob.isBandEnabled());
    highDisplay.setActive (highKnob.isBandEnabled());

    dynFader.setMeters (engine.getOverallReductionDb(), engine.getDetectedLevelDb());
}
