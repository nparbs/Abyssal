#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Presets.h"
#include "PresetManager.h"
#include "FileLibrary.h"

class AbyssalProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorListener,
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

    using LoadDone = std::function<void (const juce::String& error)>;

    /** Loads a capture on a background thread; `done` runs on the message thread with an error or "".
        If another load starts first, this one is dropped and `done` is never called. */
    void loadNamModel (const juce::File& file, LoadDone done = {});
    void unloadNamModel();
    /** The capture being loaded right now, or File() if none. */
    juce::File getNamLoadingFile() const { return namLoadingFile; }
    juce::File getNamFile() const { return engine.getNamConst().getModelFile(); }
    bool hasNamModel() const { return engine.getNamConst().hasModel(); }
    double getNamSampleRate() const { return engine.getNamConst().getModelSampleRate(); }

    juce::String loadUserIR (int slot, const juce::File& file);
    juce::File getUserIR (int slot) const { return engine.getCabinetConst().getUserIR (slot); }
    void selectBuiltInCab (int slot, int builtIn);

    // Libraries (Documents/Abyssal/IRs and /NAM). Loaded files are copied in so they stay in the
    // dropdowns; whole folders can be imported.
    static const abyssal::FileLibrary& irLibrary();
    static const abyssal::FileLibrary& namLibrary();
    juce::String importIRFolder (int slot, const juce::File& folder);
    void importNamFolder (const juce::File& folder, LoadDone done);
    bool removeFromIRLibrary (const juce::File& file);
    bool removeFromNamLibrary (const juce::File& file);
    juce::File lastIRBrowseDirectory, lastNamBrowseDirectory;

    /** Set by the editor: opens the TONE3000 panel (0 = amp, 1 = cab mic A, 2 = cab mic B). */
    std::function<void (int target)> openTone3000;

    // presets
    juce::String savePreset (const juce::File& file, const juce::String& category = abyssal::PresetManager::userCategory,
                             const juce::String& notes = {});
    juce::String loadPreset (const juce::File& file);
    void loadFactoryPreset (int index);
    /** Loads a preset from the browser (undoable). */
    void loadEntry (const abyssal::PresetManager::Entry& entry);
    /** Renames a user preset and/or changes its category and notes. Returns an error or "". */
    juce::String editUserPreset (const abyssal::PresetManager::Entry& entry, const juce::String& newName,
                                 const juce::String& category, const juce::String& notes);
    /** Moves a user preset to the Recycle Bin. */
    bool deleteUserPreset (const abyssal::PresetManager::Entry& entry);
    juce::String getPresetName() const { return presetName; }
    juce::String getPresetCategory() const { return presetCategory; }
    juce::String getPresetKey() const { return presetKey; }
    juce::String getPresetNotes() const { return presetNotes; }
    bool isUserPreset() const { return presetKey.startsWith ("user:"); }

    // pedal order
    abyssal::ChainOrder getChainOrder() const { return chainOrder; }
    void setChainOrder (const abyssal::ChainOrder& order);
    /** setChainOrder() as an undoable edit, for the UI. */
    void changeChainOrder (const abyssal::ChainOrder& order);
    juce::ChangeBroadcaster chainChanged, presetChanged;

    // undo / redo: knob moves, switches, preset loads, pedal order and A/B switches
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    void undo() { undoManager.undo(); }
    void redo() { undoManager.redo(); }
    /** Runs `change` and records the whole state before/after as one undo step. */
    void performUndoable (const juce::String& name, const std::function<void()>& change);

    // A/B compare: two complete snapshots of the sound. The inactive one is stored; the active one is live.
    int getActiveSnapshot() const noexcept { return activeSnapshot; }
    void selectSnapshot (int slot);
    /** Copies the live sound into the other slot. */
    void copyToOtherSnapshot();
    juce::ChangeBroadcaster snapshotChanged;

    // tuner
    void setTunerOpen (bool open);
    bool isTunerOpen() const noexcept { return tunerOpen.load(); }
    bool getTunerMute() const noexcept { return tunerMute.load(); }
    void setTunerMute (bool shouldMute);
    float getTunerReference() const noexcept { return tunerReference; }
    void setTunerReference (float hz);
    /** Newest decimated input samples for pitch detection; returns the sample rate. */
    double readTunerInput (float* dest, int n) const;

    /** NAM input calibration currently applied, in dB (0 when off or the capture has no level). */
    float getNamCalibrationDb() const noexcept;
    bool namHasInputLevel() const { return engine.getNamConst().hasInputLevel(); }
    double getNamInputLevelDbu() const { return engine.getNamConst().getInputLevelDbu(); }

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

    enum class Restore
    {
        session,   // everything, as saved by the host
        preset,    // keeps setup parameters; the caller names the preset
        snapshot   // keeps setup parameters; preset name comes from the tree (undo, A/B)
    };
    void applyStateTree (const juce::ValueTree& tree, Restore mode);
    void setPresetInfo (const juce::String& name, const juce::String& category, const juce::String& key,
                        const juce::String& notes = {});

    // gesture tracking for undo
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override;
    void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int) override;
    void commitUndoStep (const juce::String& name, const juce::ValueTree& before);
    void loadSettingsDefaults();

    abyssal::ParamCache cache;
    abyssal::Engine engine;
    abyssal::EngineParams params;
    abyssal::ChainOrder chainOrder = abyssal::ChainOrder::defaultOrder();

    juce::String presetName { "Init" }, presetCategory { "Utility" }, presetKey { "factory:Init" }, presetNotes;
    int currentProgram = 0;
    double currentRate = 48000.0;

    juce::UndoManager undoManager { 0, 60 };
    juce::ValueTree gestureStart;
    int openGestures = 0;
    bool restoringSnapshot = false;

    int activeSnapshot = 0;
    juce::ValueTree storedSnapshot; // the inactive A/B slot (invalid until first used)

    std::atomic<bool> tunerOpen { false }, tunerMute { true };
    float tunerReference = 440.0f;

    // Idle detection: skip DSP when input has been silent and tails have died out.
    juce::int64 silentInputSamples = 0, quietOutputSamples = 0;

    // Background NAM loading. Declared after `engine` so the pool (and any running job) goes first.
    juce::ThreadPool namLoader { 1 };
    std::atomic<int> namLoadGeneration { 0 };
    juce::File namLoadingFile;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AbyssalProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbyssalProcessor)
};
