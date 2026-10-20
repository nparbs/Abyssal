#pragma once

#include "Widgets.h"

class AbyssalProcessor;

namespace abyssal::ui
{

//============================================================================== Pre-amp pedalboard
class PreFXPage : public juce::Component
{
public:
    explicit PreFXPage (AbyssalProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    Pedal gate, octaver, drop, comp, od, geq;
    juce::OwnedArray<Fader> faders;
};

//============================================================================== Amp head
class AmpPage : public juce::Component, private juce::Timer
{
public:
    explicit AmpPage (AbyssalProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void channelChanged (int ch);
    void modeChanged (int mode);
    void refreshNamLabel();

    AbyssalProcessor& proc;
    ChoiceButtons channel, mode;
    Knob gain, bass, mid, treble, presence, depth, master, namIn, namOut;
    MiniToggle bright, tight;
    juce::TextButton loadCapture { "LOAD .NAM CAPTURE" }, unloadCapture { "UNLOAD" };
    juce::Label namName, namInfo;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Colour accent = colours::accent;
    juce::String channelName = "LEAD";
    juce::Rectangle<int> housing, face, grill, modelPanel;
    int currentMode = 0;
};

//============================================================================== Cabinet
class CabView : public juce::Component, private juce::Timer
{
public:
    explicit CabView (AbyssalProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> speakerBounds() const;
    juce::Point<float> micPoint (int slot) const;

    AbyssalProcessor& proc;
    int dragging = -1;
};

class CabSlotPanel : public juce::Component
{
public:
    CabSlotPanel (AbyssalProcessor&, int slot);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refreshFileLabel();

private:
    AbyssalProcessor& proc;
    int slot;
    juce::Colour accent;
    MiniToggle onToggle;
    ChoiceBox source;
    juce::TextButton loadIR { "LOAD IR..." };
    juce::Label fileLabel;
    Knob level, pan, mic;
    std::unique_ptr<juce::FileChooser> chooser;
};

class CabPage : public juce::Component, private juce::Timer
{
public:
    explicit CabPage (AbyssalProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { slotA.refreshFileLabel(); slotB.refreshFileLabel(); }
    CabView view;
    CabSlotPanel slotA, slotB;
    MiniToggle cabOn;
};

//============================================================================== Parametric EQ
class EQGraph : public juce::Component, private juce::Timer
{
public:
    explicit EQGraph (AbyssalProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    juce::Point<float> nodePos (int node) const;
    int hitNode (juce::Point<float>) const;
    void setParam (const juce::String& id, float value);
    float getParam (const juce::String& id) const;

    AbyssalProcessor& proc;
    int dragNode = -1;
    juce::Path curve;
    std::vector<float> lastValues;
    static constexpr float maxDb = 18.0f;
};

class EQPage : public juce::Component
{
public:
    explicit EQPage (AbyssalProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    EQGraph graph;
    MiniToggle eqOn;
    juce::OwnedArray<Knob> knobs;
};

//============================================================================== Post FX
class PostFXPage : public juce::Component, private juce::Timer
{
public:
    explicit PostFXPage (AbyssalProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    AbyssalProcessor& proc;
    Pedal chorus, flanger, delay, reverb;
    Knob* delayTime = nullptr;
};

//============================================================================== Signal chain navigator
class ChainBar : public juce::Component, private juce::Timer
{
public:
    explicit ChainBar (AbyssalProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void setSelected (int i) { selected = i; repaint(); }
    std::function<void (int)> onSelect;

    static constexpr int numBlocks = 5;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> blockBounds (int i) const;
    juce::String statusFor (int i, bool& lit) const;

    AbyssalProcessor& proc;
    int selected = 1, hover = -1;
};

} // namespace abyssal::ui
