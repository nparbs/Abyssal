#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

namespace abyssal::ui
{

using APVTS = juce::AudioProcessorValueTreeState;

//============================================================================== Knob
/** Rotary knob with its name underneath; shows the value while hovered or dragged. */
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& label,
          juce::Colour accent = colours::accent, bool bipolar = false);

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    juce::Slider slider;
    void setLabelColour (juce::Colour c) { labelColour = c; repaint(); }

private:
    juce::String label;
    juce::Colour labelColour = colours::text;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//============================================================================== Footswitch
/** Stomp switch with a status LED; bound to a bool parameter. */
class Footswitch : public juce::Button
{
public:
    Footswitch (APVTS& state, const juce::String& paramId, juce::Colour led = colours::accent);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Colour ledColour;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

/** Small toggle (e.g. BRIGHT / TIGHT / SYNC) bound to a bool parameter. */
class MiniToggle : public juce::TextButton
{
public:
    MiniToggle (APVTS& state, const juce::String& paramId, const juce::String& text, juce::Colour on = colours::accent);

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

/** Combo box bound to a choice parameter. */
class ChoiceBox : public juce::ComboBox
{
public:
    ChoiceBox (APVTS& state, const juce::String& paramId);

private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

/** Row of radio buttons bound to a choice parameter (amp channels, amp model type). */
class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (APVTS& state, const juce::String& paramId, juce::Array<juce::Colour> colours);
    void resized() override;
    std::function<void (int)> onChange;
    int getSelected() const noexcept { return selected; }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int selected = 0;
};

/** Vertical fader for the graphic EQ. */
class Fader : public juce::Component
{
public:
    Fader (APVTS& state, const juce::String& paramId, const juce::String& label, juce::Colour accent);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;

private:
    juce::String label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//============================================================================== Pedal
/** A stompbox enclosure: knobs on top, optional mini controls, footswitch at the bottom. */
class Pedal : public juce::Component
{
public:
    Pedal (APVTS& state, const juce::String& name, const juce::String& onParam, juce::Colour body, juce::Colour accent);

    Knob& addKnob (const juce::String& paramId, const juce::String& label, bool bipolar = false);
    void addToggle (const juce::String& paramId, const juce::String& text);
    void addChoice (const juce::String& paramId);
    void setKnobsPerRow (int n) { knobsPerRow = n; }
    void setBodyArea (std::function<void (juce::Rectangle<int>)> layoutFn) { customLayout = std::move (layoutFn); }

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Rectangle<int> getContentArea() const;

protected:
    APVTS& state;
    juce::String name;
    juce::Colour body, accent;
    Footswitch footswitch;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::Component> minis;
    int knobsPerRow = 2;
    std::function<void (juce::Rectangle<int>)> customLayout;
    juce::RangedAudioParameter* onParam = nullptr;
};

//============================================================================== Meter
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (std::function<float()> source);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::function<float()> source;
    float level = 0, peakHold = 0;
    int holdFrames = 0;
};

/** Draws the brushed-metal style screws used on pedals and the amp face. */
void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius);

} // namespace abyssal::ui
