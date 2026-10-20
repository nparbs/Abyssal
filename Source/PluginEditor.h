#pragma once

#include "PluginProcessor.h"
#include "GUI/Pages.h"
#include "GUI/PresetBrowser.h"

/** Shows the current preset's name and category; clicking opens the browser. */
class PresetDisplay : public juce::Button
{
public:
    PresetDisplay() : juce::Button ("preset") {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::String name { "Init" }, category { "Utility" };
};

/** Everything inside the plugin window, laid out at a fixed 1180 x 800 and scaled as a whole. */
class MainView : public juce::Component,
                 public juce::DragAndDropContainer,
                 private juce::ChangeListener
{
public:
    static constexpr int width = 1180, height = 800;

    explicit MainView (AbyssalProcessor&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshPresetInfo(); }
    void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&) override;
    void showPage (int index);
    void refreshPresetInfo();
    void stepPreset (int delta);

    AbyssalProcessor& proc;

    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, savePreset { "SAVE" }, loadPreset { "LOAD" };
    PresetDisplay presetDisplay;
    abyssal::ui::StarButton favourite;
    abyssal::ui::ChoiceBox inputSelect, outputSelect;
    abyssal::ui::Knob inputKnob, outputKnob;
    abyssal::ui::LevelMeter inMeter, outMeter;

    abyssal::ui::ChainBar chain;
    abyssal::ui::PedalSet pedals;
    abyssal::ui::BoardPage preBoard;
    abyssal::ui::AmpPage ampPage;
    abyssal::ui::CabPage cabPage;
    abyssal::ui::EQPage eqPage;
    abyssal::ui::BoardPage postBoard;
    abyssal::ui::PresetBrowser browser;
    std::array<juce::Component*, 5> pages {};

    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };
};

class AbyssalEditor : public juce::AudioProcessorEditor
{
public:
    explicit AbyssalEditor (AbyssalProcessor&);
    ~AbyssalEditor() override;

    void resized() override;

private:
    abyssal::ui::AbyssalLookAndFeel laf;
    AbyssalProcessor& proc;
    MainView view;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbyssalEditor)
};
