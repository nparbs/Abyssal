#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Presets.h"
#include "PresetManager.h"

class AbyssalProcessor : public juce::AudioProcessor,
                         private juce::Timer
{
public:
    AbyssalProcessor();
    ~AbyssalProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Abyssal"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //============================================================================== UI API (message thread)
    juce::AudioProcessorValueTreeState apvts;
    abyssal::PresetManager presets;

    juce::String loadNamModel (const juce::File& file);
    void unloadNamModel();
    juce::File getNamFile() const { return engine.getNamConst().getModelFile(); }
    bool hasNamModel() const { return engine.getNamConst().hasModel(); }
    double getNamSampleRate() const { return engine.getNamConst().getModelSampleRate(); }

    juce::String loadUserIR (int slot, const juce::File& file);
    juce::File getUserIR (int slot) const { return engine.getCabinetConst().getUserIR (slot); }

    // presets
    juce::String savePreset (const juce::File& file);
    juce::String loadPreset (const juce::File& file);
    void loadFactoryPreset (int index);
    void loadEntry (const abyssal::PresetManager::Entry& entry);
    juce::String getPresetName() const { return presetName; }
    juce::String getPresetCategory() const { return presetCategory; }
    juce::String getPresetKey() const { return presetKey; }

    // pedal order
    abyssal::ChainOrder getChainOrder() const { return chainOrder; }
    void setChainOrder (const abyssal::ChainOrder& order);
    juce::ChangeBroadcaster chainChanged, presetChanged;

    float getInputPeak() const noexcept  { return engine.inputPeak.load(); }
    float getOutputPeak() const noexcept { return engine.outputPeak.load(); }
    double getCurrentSampleRate() const noexcept { return currentRate; }

    int lastUiPage = 1;
    double uiScale = 1.0;

private:
    void timerCallback() override;
    void syncCabSources();
    void restoreFiles (const juce::ValueTree& state);
    juce::ValueTree createStateTree();
    void applyStateTree (const juce::ValueTree& tree, bool keepGlobals);
    void setPresetInfo (const juce::String& name, const juce::String& category, const juce::String& key);

    abyssal::ParamCache cache;
    abyssal::Engine engine;
    abyssal::EngineParams params;
    abyssal::ChainOrder chainOrder = abyssal::ChainOrder::defaultOrder();

    juce::String presetName { "Init" }, presetCategory { "Utility" }, presetKey { "factory:Init" };
    int currentProgram = 0;
    double currentRate = 48000.0;

    // Idle detection: skip DSP when input has been silent and tails have died out.
    juce::int64 silentInputSamples = 0, quietOutputSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbyssalProcessor)
};
