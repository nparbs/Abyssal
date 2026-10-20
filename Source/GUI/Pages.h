#pragma once

#include "Widgets.h"
#include "../DSP/Chain.h"

class AbyssalProcessor;

namespace abyssal::ui
{

//============================================================================== Pedalboards
/** Owns one pedal component per effect. Pedals are re-parented between the pre-amp and post-amp
    boards as the chain order changes. */
class PedalSet
{
public:
    explicit PedalSet (AbyssalProcessor&);
    Pedal& get (int fxId) { return *pedals[(size_t) fxId]; }

private:
    void showContextMenu (Pedal&);
    AbyssalProcessor& proc;
    std::array<std::unique_ptr<Pedal>, abyssal::numFx> pedals;
};

/** One side of the amp. Shows that section's pedals in order and accepts dropped pedals. */
class BoardPage : public juce::Component,
                  public juce::DragAndDropTarget,
                  private juce::ChangeListener
{
public:
    BoardPage (AbyssalProcessor&, PedalSet&, bool preAmp);
    ~BoardPage() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void visibilityChanged() override { if (isVisible()) rebuild(); }

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails& d) override { itemDragMove (d); }
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override { insertIndex = -1; repaint(); }
    void itemDropped (const SourceDetails&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { rebuild(); }
    void rebuild();
    int insertionIndexFor (juce::Point<int>) const;

    AbyssalProcessor& proc;
    PedalSet& set;
    const bool preAmp;
    juce::Array<int> items;
    juce::Array<juce::Rectangle<int>> cells;
    juce::Rectangle<int> boardArea;
    int insertIndex = -1;
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
    void timerCallback() override;
    juce::Rectangle<float> speakerBounds() const;
    juce::Point<float> micPoint (int slot) const;

    AbyssalProcessor& proc;
    int dragging = -1;
    float lastState[4] { -1, -1, -1, -1 };
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

//============================================================================== Signal chain navigator
class ChainBar : public juce::Component,
                 public juce::DragAndDropTarget,
                 private juce::Timer
{
public:
    explicit ChainBar (AbyssalProcessor&);

    // Drop a pedal on PRE FX / POST FX to move it across the amp; hovering switches page.
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override { dragHover = -1; repaint(); }
    void itemDropped (const SourceDetails&) override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void setSelected (int i) { selected = i; repaint(); }
    std::function<void (int)> onSelect;

    static constexpr int numBlocks = 5;

private:
    void timerCallback() override;
    int blockAt (juce::Point<float>) const;
    juce::Rectangle<float> blockBounds (int i) const;
    juce::String statusFor (int i, bool& lit) const;

    AbyssalProcessor& proc;
    int selected = 1, hover = -1, dragHover = -1;
    juce::uint32 dragHoverStart = 0;
    juce::StringArray lastStatus;
};

} // namespace abyssal::ui
