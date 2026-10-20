#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Presets.h"

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

    juce::String loadNamModel (const juce::File& file);
    void unloadNamModel();
    juce::File getNamFile() const { return engine.getNamConst().getModelFile(); }
    bool hasNamModel() const { return engine.getNamConst().hasModel(); }
    double getNamSampleRate() const { return engine.getNamConst().getModelSampleRate(); }

    juce::String loadUserIR (int slot, const juce::File& file);
    juce::File getUserIR (int slot) const { return engine.getCabinetConst().getUserIR (slot); }

    juce::File getUserPresetDirectory() const;
    juce::String savePreset (const juce::File& file);
    juce::String loadPreset (const juce::File& file);
    juce::String getPresetName() const { return presetName; }
    void loadFactoryPreset (int index);

    float getInputPeak() const noexcept  { return engine.inputPeak.load(); }
    float getOutputPeak() const noexcept { return engine.outputPeak.load(); }
    float getGateReductionDb() const noexcept { return engine.gateGainReduction.load(); }
    double getCurrentSampleRate() const noexcept { return currentRate; }

    int lastUiPage = 1;

private:
    void timerCallback() override;
    void syncCabSources();
    void restoreFiles (const juce::ValueTree& state);
    juce::ValueTree createStateTree();
    void applyStateTree (const juce::ValueTree& tree);

    abyssal::ParamCache cache;
    abyssal::Engine engine;
    abyssal::EngineParams params;

    juce::String presetName { "Init" };
    int currentProgram = 0;
    double currentRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbyssalProcessor)
};
