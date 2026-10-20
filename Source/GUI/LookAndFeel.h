#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abyssal::ui
{

namespace colours
{
    const juce::Colour bg        { 0xff0a0b0e };
    const juce::Colour panel     { 0xff14161b };
    const juce::Colour panelHi   { 0xff1e2128 };
    const juce::Colour outline   { 0xff2a2e37 };
    const juce::Colour text      { 0xffe9e9ec };
    const juce::Colour textDim   { 0xff8b909b };
    const juce::Colour accent    { 0xffe8432a }; // molten red
    const juce::Colour accentAlt { 0xff36c6ff }; // cold cyan
    const juce::Colour amber     { 0xffffa726 };
    const juce::Colour ledOff    { 0xff3a1410 };
}

juce::Font font (float height, bool bold = false);

class AbyssalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AbyssalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (14.0f, true); }
    juce::Font getPopupMenuFont() override { return font (15.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getLabelFont (juce::Label&) override { return font (13.0f); }
};

} // namespace abyssal::ui
