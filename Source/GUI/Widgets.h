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

    /** Points the knob at another parameter (the amp's knobs follow the selected channel). */
    void bind (const juce::String& paramId);

private:
    APVTS& state;
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
    void bind (const juce::String& paramId);

private:
    APVTS& state;
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
    void setTooltip (const juce::String& tip) { for (auto* b : buttons) b->setTooltip (tip); }

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
/** A stompbox enclosure: knobs on top, optional mini controls, footswitch at the bottom.
    Dragging the enclosure starts a drag-and-drop ("fx:<id>") so it can be rearranged. */
class Pedal : public juce::Component
{
public:
    Pedal (APVTS& state, const juce::String& name, const juce::String& onParam, juce::Colour body, juce::Colour accent);

    Knob& addKnob (const juce::String& paramId, const juce::String& label, bool bipolar = false);
    void addToggle (const juce::String& paramId, const juce::String& text);
    void addChoice (const juce::String& paramId);
    /** Any other small control for the mini row (e.g. a tap-tempo button). The pedal owns it. */
    void addMini (juce::Component* c);
    void setKnobsPerRow (int n) { knobsPerRow = n; }
    void setBodyArea (std::function<void (juce::Rectangle<int>)> layoutFn) { customLayout = std::move (layoutFn); }

    void setFxId (int id) noexcept { fxId = id; }
    int  getFxId() const noexcept  { return fxId; }
    /** Relative width on the pedalboard (the graphic EQ, delay and reverb are double-width). */
    virtual int getWidthUnits() const { return widthUnits; }
    void setWidthUnits (int units) { widthUnits = units; }
    std::function<void (Pedal&)> onContextMenu;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    juce::Rectangle<int> getContentArea() const;

protected:
    APVTS& state;
    juce::String name;
    juce::Colour body, accent;
    Footswitch footswitch;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::Component> minis;
    int knobsPerRow = 2;
    int widthUnits = 1;
    int fxId = -1;
    bool dragStarted = false;
    std::function<void (juce::Rectangle<int>)> customLayout;
    juce::RangedAudioParameter* onParam = nullptr;
};

/** Double-width pedal with nine band faders and a level fader. */
class GraphicEQPedal : public Pedal
{
public:
    GraphicEQPedal (APVTS& state, const juce::String& onParam, const juce::StringArray& bandParams,
                    const juce::StringArray& bandLabels, const juce::String& levelParam);
    int getWidthUnits() const override { return 2; }

private:
    juce::OwnedArray<Fader> faders;
};

/** Delay pedal: greys out the Time knob while tempo sync is on, and has a tap-tempo button. */
class DelayPedal : public Pedal, private juce::Timer
{
public:
    DelayPedal (APVTS& state, const juce::String& onParam, const juce::String& syncParam, const juce::String& timeParam,
                juce::Colour body, juce::Colour accent);
    Knob* timeKnob = nullptr;

    /** Registers a tap at `nowMs`; from the second tap on, sets the delay time to the average gap
        (and switches tempo sync off). Returns the new time in ms, or 0 if there's nothing to set yet. */
    float tap (double nowMs);

private:
    void timerCallback() override;
    std::atomic<float>* sync = nullptr;
    juce::String syncId, timeId;
    juce::Array<double> taps;
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
