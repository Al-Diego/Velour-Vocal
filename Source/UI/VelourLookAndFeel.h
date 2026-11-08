#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

/** Light-grey / teal look. Most controls draw themselves (see below);
    this mainly styles combo boxes, popup menus and tooltips. */
class VelourLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VelourLookAndFeel();

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    int getSliderThumbRadius (juce::Slider&) override;

    float uiScale = 1.0f; // set by the editor on resize
};

//==============================================================================
/** Round status LED with an optional caption, used for band/Dyn on-off toggles. */
class LedButton : public juce::Button
{
public:
    explicit LedButton (const juce::String& caption);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/** Headphone icon toggle used for per-band solo. */
class HeadphoneButton : public juce::Button
{
public:
    HeadphoneButton();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/** Rounded pill toggle with a text glyph (used for Delta). */
class PillButton : public juce::Button
{
public:
    explicit PillButton (const juce::String& glyph);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/** Small segment button with a drawn curve icon: bell (focused) or shelf (broad). */
class CurveButton : public juce::Button
{
public:
    enum class Icon { Bell, Shelf };
    explicit CurveButton (Icon);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
private:
    Icon icon;
};

//==============================================================================
/** Rotary knob drawn entirely in vector graphics.
    Large style: light puck, dark face, teal amount arc, value in the middle,
    plus a thin inner ring showing live gain reduction.
    Small style: light puck with a pointer and a bipolar arc (for Output). */
class ArcKnob : public juce::Slider
{
public:
    enum class Style { Band, Small };
    explicit ArcKnob (Style);

    void paint (juce::Graphics&) override;

    void setReductionDb (float db);
    void setActive (bool shouldBeActive);
    juce::Rectangle<float> getKnobBounds() const;

private:
    Style style;
    float reductionDb = 0.0f;
    bool active = true;
};
