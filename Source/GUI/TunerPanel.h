#pragma once

#include "Widgets.h"
#include "../DSP/Tuner.h"

class AbyssalProcessor;

namespace abyssal::ui
{

/** Full-window chromatic tuner. Opening it starts the input tap (and mutes the output if MUTE is
    on); closing it stops both. */
class TunerPanel : public juce::Component,
                   private juce::Timer
{
public:
    explicit TunerPanel (AbyssalProcessor&);
    ~TunerPanel() override;

    void open();
    void close();
    std::function<void()> onVisibilityChange;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void updateReferenceLabel();
    juce::Rectangle<int> panelBounds() const;

    AbyssalProcessor& proc;
    PitchDetector detector;
    std::vector<float> samples;

    // display state
    float shownCents = 0.0f, shownHz = 0.0f;
    juce::String noteName;
    int octave = 0;
    bool hasPitch = false;
    int framesWithoutPitch = 1000;
    std::array<float, 5> recent {};
    int recentCount = 0, recentPos = 0;

    juce::TextButton closeButton { "CLOSE" }, muteButton { "MUTE" }, refDown { "-" }, refUp { "+" };
    juce::Label refLabel;
};

} // namespace abyssal::ui
