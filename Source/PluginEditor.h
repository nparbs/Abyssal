#pragma once

#include "PluginProcessor.h"
#include "GUI/Pages.h"

class AbyssalEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit AbyssalEditor (AbyssalProcessor&);
    ~AbyssalEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPage (int index);
    void refreshPresetList();
    void selectPreset (int itemId);
    void stepPreset (int delta);

    abyssal::ui::AbyssalLookAndFeel laf;
    AbyssalProcessor& proc;

    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, savePreset { "SAVE" }, loadPreset { "LOAD" };
    abyssal::ui::Knob inputKnob, outputKnob;
    abyssal::ui::LevelMeter inMeter, outMeter;

    abyssal::ui::ChainBar chain;
    abyssal::ui::PreFXPage preFX;
    abyssal::ui::AmpPage ampPage;
    abyssal::ui::CabPage cabPage;
    abyssal::ui::EQPage eqPage;
    abyssal::ui::PostFXPage postFX;
    std::array<juce::Component*, 5> pages {};

    juce::Array<juce::File> userPresets;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String shownPresetName;

    static constexpr int userIdOffset = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbyssalEditor)
};
