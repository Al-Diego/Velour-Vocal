#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Theme
{
    using juce::Colour;

    inline const Colour bgTop       { 0xffeff1f4 };
    inline const Colour bgBottom    { 0xffdcdfe4 };
    inline const Colour panelLine   { 0xffc9ced4 };
    inline const Colour text        { 0xff28303a };
    inline const Colour textDim     { 0xff7d8792 };
    inline const Colour accent      { 0xff1cc7dc };
    inline const Colour accentDeep  { 0xff0b8ea1 };
    inline const Colour ledOff      { 0xffb6bdc5 };
    inline const Colour knobLight   { 0xfffbfcfd };
    inline const Colour knobShade   { 0xffd2d7dd };
    inline const Colour dark        { 0xff18222a };
    inline const Colour dark2       { 0xff2b3a44 };
    inline const Colour screenBg    { 0xfff6f8fa };
    inline const Colour screenGrid  { 0xffe1e5ea };

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height).withStyle (bold ? "Bold" : "Regular"));
    }
}
