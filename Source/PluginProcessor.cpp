#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace abyssal;

namespace
{
    /** One undo step: the whole state before and after. The first perform() is a no-op because the
        change has already happened by the time it's recorded. */
    class StateChange : public juce::UndoableAction
    {
    public:
        StateChange (std::function<void (const juce::ValueTree&)> restoreFn, juce::ValueTree beforeState, juce::ValueTree afterState)
            : restore (std::move (restoreFn)), before (std::move (beforeState)), after (std::move (afterState)) {}

        bool perform() override
        {
            if (std::exchange (firstPerform, false)) return true;
            restore (after);
            return true;
        }
        bool undo() override { restore (before); return true; }
        int getSizeInUnits() override { return 1; }

    private:
        std::function<void (const juce::ValueTree&)> restore;
        juce::ValueTree before, after;
        bool firstPerform = true;
    };

    /** Compares the parts of two state trees an undo step cares about (parameters, pedal order, files). */
    bool sameSound (const juce::ValueTree& a, const juce::ValueTree& b)
    {
        for (auto* prop : { "chainOrder", "namPath", "irA", "irB", "presetKey" })
            if (a.getProperty (prop) != b.getProperty (prop))
                return false;
        if (a.getNumChildren() != b.getNumChildren())
            return false;
        for (int i = 0; i < a.getNumChildren(); ++i)
            if (! a.getChild (i).isEquivalentTo (b.getChild (i)))
                return false;
        return true;
    }
}

AbyssalProcessor::AbyssalProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      cache (apvts)
{
    uiScale = presets.getDefaultUiScale();
    engine.setChainOrder (chainOrder);
    loadSettingsDefaults();
    addListener (this);
    startTimerHz (20);
}

AbyssalProcessor::~AbyssalProcessor()
{
    removeListener (this);
    stopTimer();
}

void AbyssalProcessor::loadSettingsDefaults()
{
    // Rig-specific settings follow the user from project to project.
    tunerMute = presets.getSetting ("tunerMute", 1.0) > 0.5;
    tunerReference = (float) juce::jlimit (420.0, 460.0, presets.getSetting ("tunerA4", 440.0));
    if (auto* p = apvts.getParameter (ids::namCalibrate))
        p->setValueNotifyingHost (presets.getSetting ("namCalibrate", 0.0) > 0.5 ? 1.0f : 0.0f);
    if (auto* p = apvts.getParameter (ids::namCalLevel))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) presets.getSetting ("namCalLevel", 12.0)));
}

bool AbyssalProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const bool inOk  = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

juce::AudioProcessorParameter* AbyssalProcessor::getBypassParameter() const
{
    return apvts.getParameter (ids::bypass);
}

void AbyssalProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    syncCabSources();
    setLatencySamples (engine.getLatencySamples());
    silentInputSamples = quietOutputSamples = 0;
}

void AbyssalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();

    // A mono input bus feeds both sides so the Left/Right/Sum input selector always has signal.
    if (numIn == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    for (int ch = std::max (numIn, 2); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    cache.fill (params);
    params.mute = tunerOpen.load() && tunerMute.load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                params.bpm = *bpm;

    // Idle skip: if the input has been silent for a second and the output (including delay and
    // reverb tails) has stayed below -100 dBFS for longer than the longest delay, stop processing.
    // Saves CPU on tracks that aren't playing; processing resumes on the first non-silent sample.
    float inPk = 0;
    for (int ch = 0; ch < std::min (numIn, buffer.getNumChannels()); ++ch)
        inPk = std::max (inPk, buffer.getMagnitude (ch, 0, n));

    silentInputSamples = inPk < 1.0e-6f ? silentInputSamples + n : 0;
    const bool idle = ! params.bypass && ! tunerOpen.load()
                      && silentInputSamples > (juce::int64) currentRate
                      && quietOutputSamples > (juce::int64) (currentRate * 2.6);
    if (idle)
    {
        buffer.clear();
        engine.inputPeak = 0.0f;
        engine.outputPeak = 0.0f;
        return;
    }

    engine.process (buffer, params);
    quietOutputSamples = engine.lastBlockPeak < 1.0e-5f ? quietOutputSamples + n : 0;
}

//============================================================================== files
void AbyssalProcessor::syncCabSources()
{
    engine.getCabinet().setAlign (apvts.getRawParameterValue (ids::cabAlign)->load() > 0.5f);
    for (int s = 0; s < 2; ++s)
    {
        const int src = (int) apvts.getRawParameterValue (ids::cab (s, "src"))->load();
        if (src != engine.getCabinet().getLoadedSource (s))
            engine.getCabinet().setSource (s, src);
    }
}

void AbyssalProcessor::timerCallback()
{
    syncCabSources();

    // The chain's latency changes with the amp mode, the capture's rate and drop tune.
    const int latency = engine.getLatencySamples();
    if (latency != getLatencySamples())
        setLatencySamples (latency);

    // Remember the interface calibration as the default for new instances.
    const double cal = apvts.getRawParameterValue (ids::namCalibrate)->load();
    const double calLevel = apvts.getRawParameterValue (ids::namCalLevel)->load();
    if (cal != presets.getSetting ("namCalibrate", 0.0))    presets.setSetting ("namCalibrate", cal);
    if (calLevel != presets.getSetting ("namCalLevel", 12.0)) presets.setSetting ("namCalLevel", calLevel);
}

float AbyssalProcessor::getNamCalibrationDb() const noexcept
{
    EngineParams p;
    p.namCalibrate = apvts.getRawParameterValue (ids::namCalibrate)->load() > 0.5f;
    p.namInterfaceDbu = apvts.getRawParameterValue (ids::namCalLevel)->load();
    return Engine::namCalibrationDb (p, engine.getNamConst());
}

//============================================================================== tuner
void AbyssalProcessor::setTunerOpen (bool open)
{
    tunerOpen = open;
    engine.tunerEnabled = open;
}

void AbyssalProcessor::setTunerMute (bool shouldMute)
{
    tunerMute = shouldMute;
    presets.setSetting ("tunerMute", shouldMute ? 1.0 : 0.0);
}

void AbyssalProcessor::setTunerReference (float hz)
{
    tunerReference = juce::jlimit (420.0f, 460.0f, hz);
    presets.setSetting ("tunerA4", tunerReference);
}

double AbyssalProcessor::readTunerInput (float* dest, int n) const
{
    auto& tap = const_cast<Engine&> (engine).getTunerTap();
    tap.read (dest, n);
    return tap.getRate();
}

//============================================================================== undo
void AbyssalProcessor::audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int)
{
    // Gestures come from our own editor (hosts don't send them for automation), on the message thread.
    if (! juce::MessageManager::getInstance()->isThisTheMessageThread() || restoringSnapshot)
        return;
    if (openGestures++ == 0)
        gestureStart = createStateTree();
}

void AbyssalProcessor::audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int index)
{
    if (! juce::MessageManager::getInstance()->isThisTheMessageThread() || restoringSnapshot || openGestures == 0)
        return;
    if (--openGestures > 0)
        return;

    juce::String name = "Change";
    if (auto* p = getParameters()[index])
        name = p->getName (64);
    // Global setup parameters aren't part of the sound, so they don't get undo steps.
    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (getParameters()[index]))
        if (isGlobalParameter (rp->paramID))
            return;
    commitUndoStep (name, std::exchange (gestureStart, {}));
}

void AbyssalProcessor::commitUndoStep (const juce::String& name, const juce::ValueTree& before)
{
    if (! before.isValid())
        return;
    auto after = createStateTree();
    if (sameSound (before, after))
        return;

    undoManager.beginNewTransaction (name);
    undoManager.perform (new StateChange ([this] (const juce::ValueTree& t)
    {
        const juce::ScopedValueSetter<bool> svs (restoringSnapshot, true);
        applyStateTree (t, Restore::snapshot);
    }, before, after));
}

void AbyssalProcessor::performUndoable (const juce::String& name, const std::function<void()>& change)
{
    auto before = createStateTree();
    change();
    commitUndoStep (name, before);
}

void AbyssalProcessor::changeChainOrder (const ChainOrder& order)
{
    performUndoable ("Pedal order", [&] { setChainOrder (order); });
}

//============================================================================== A/B
void AbyssalProcessor::selectSnapshot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeSnapshot)
        return;

    // Not an undo step: undoing it would leave the slots pointing the wrong way round. Edits made
    // on either side are still undoable.
    auto live = createStateTree();
    if (storedSnapshot.isValid())
    {
        const juce::ScopedValueSetter<bool> svs (restoringSnapshot, true);
        applyStateTree (storedSnapshot, Restore::snapshot);
    }
    storedSnapshot = live;    // first use: B starts as a copy of A
    activeSnapshot = slot;
    snapshotChanged.sendChangeMessage();
}

void AbyssalProcessor::copyToOtherSnapshot()
{
    storedSnapshot = createStateTree();
    snapshotChanged.sendChangeMessage();
}

//============================================================================== libraries
const FileLibrary& AbyssalProcessor::irLibrary()
{
    static const FileLibrary lib ("IRs", "*.wav;*.aif;*.aiff;*.flac");
    return lib;
}

const FileLibrary& AbyssalProcessor::namLibrary()
{
    static const FileLibrary lib ("NAM", "*.nam");
    return lib;
}

void AbyssalProcessor::loadNamModel (const juce::File& original, LoadDone done)
{
    if (! namLibrary().contains (original))
        lastNamBrowseDirectory = original.getParentDirectory();

    const int generation = ++namLoadGeneration;
    namLoadingFile = original;

    // Build the model off the message thread: large captures take a noticeable moment.
    namLoader.addJob ([this, original, generation, done, weak = juce::WeakReference<AbyssalProcessor> (this)]
    {
        if (generation != namLoadGeneration.load())
            return; // superseded before it started
        juce::String err;
        auto prepared = engine.getNamConst().prepareModel (original, err);

        juce::MessageManager::callAsync ([weak, original, generation, done, err, prepared]() mutable
        {
            auto* self = weak.get();
            if (self == nullptr || generation != self->namLoadGeneration.load())
                return; // the plugin closed, or a newer load (or unload) took over
            self->namLoadingFile = juce::File();
            if (prepared != nullptr)
            {
                // Load the original first so broken files never end up in the library.
                self->engine.getNam().installModel (std::move (prepared), namLibrary().add (original));
                if (auto* p = self->apvts.getParameter (ids::ampMode))
                    p->setValueNotifyingHost (1.0f); // switch to the capture automatically
            }
            if (done) done (err);
        });
    });
}

void AbyssalProcessor::unloadNamModel()
{
    ++namLoadGeneration; // cancels a load in progress
    namLoadingFile = juce::File();
    engine.getNam().unloadModel();
    if (auto* p = apvts.getParameter (ids::ampMode))
        p->setValueNotifyingHost (0.0f);
}

void AbyssalProcessor::importNamFolder (const juce::File& folder, LoadDone done)
{
    lastNamBrowseDirectory = folder.getParentDirectory();
    const auto imported = namLibrary().importFolder (folder);
    if (imported.isEmpty())
        return done ("No .nam files found in " + folder.getFileName());
    loadNamModel (imported.getFirst(), std::move (done));
}

bool AbyssalProcessor::removeFromNamLibrary (const juce::File& file)
{
    if (getNamFile() == file)
        unloadNamModel();
    return namLibrary().remove (file);
}

bool AbyssalProcessor::removeFromIRLibrary (const juce::File& file)
{
    // Slots using it fall back to a built-in cab (the file goes to the Recycle Bin, not deleted).
    for (int s = 0; s < 2; ++s)
        if (getUserIR (s) == file)
            selectBuiltInCab (s, 0);
    return irLibrary().remove (file);
}

void AbyssalProcessor::selectBuiltInCab (int slot, int builtIn)
{
    if (auto* p = apvts.getParameter (ids::cab (slot, "src")))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, Cabinet::numBuiltIns - 1, builtIn)));
}

juce::String AbyssalProcessor::importIRFolder (int slot, const juce::File& folder)
{
    lastIRBrowseDirectory = folder.getParentDirectory();
    const auto imported = irLibrary().importFolder (folder);
    if (imported.isEmpty())
        return "No IR files (.wav / .aif / .flac) found in " + folder.getFileName();
    return loadUserIR (slot, imported.getFirst());
}

juce::String AbyssalProcessor::loadUserIR (int slot, const juce::File& original)
{
    if (! irLibrary().contains (original))
        lastIRBrowseDirectory = original.getParentDirectory();

    // Validate before copying so broken files never end up in the library.
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (original));
        if (reader == nullptr || reader->lengthInSamples < 16)
            return "Couldn't read " + original.getFileName() + " as an impulse response";
    }
    const auto file = irLibrary().add (original);

    if (auto* p = apvts.getParameter (ids::cab (slot, "src")))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) Cabinet::userSource));
    if (auto* p = apvts.getParameter (ids::cab (slot, "on")))
        p->setValueNotifyingHost (1.0f);
    return engine.getCabinet().loadUserIR (slot, file);
}

//============================================================================== chain order
void AbyssalProcessor::setChainOrder (const ChainOrder& order)
{
    if (order == chainOrder)
        return;
    chainOrder = order;
    engine.setChainOrder (order);
    chainChanged.sendChangeMessage();
}

//============================================================================== presets
int AbyssalProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }

const juce::String AbyssalProcessor::getProgramName (int index)
{
    const auto& p = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void AbyssalProcessor::setCurrentProgram (int index)
{
    // Some hosts re-send the current program while restoring a project; ignoring a no-op change
    // stops them from wiping the restored sound back to the factory preset.
    if (index == currentProgram)
        return;
    loadFactoryPreset (index);
}

void AbyssalProcessor::setPresetInfo (const juce::String& name, const juce::String& category, const juce::String& key,
                                      const juce::String& notes)
{
    presetName = name;
    presetCategory = category;
    presetKey = key;
    presetNotes = notes;
    presetChanged.sendChangeMessage();
}

void AbyssalProcessor::loadFactoryPreset (int index)
{
    const auto& p = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) p.size()))
        return;
    const auto& preset = p[(size_t) index];
    currentProgram = index;
    applyFactoryPreset (apvts, preset);
    setChainOrder (preset.chain.isEmpty() ? ChainOrder::defaultOrder() : ChainOrder::fromString (preset.chain));
    setPresetInfo (preset.name, preset.category, "factory:" + preset.name);
}

void AbyssalProcessor::loadEntry (const PresetManager::Entry& entry)
{
    performUndoable ("Load " + entry.name, [&]
    {
        if (entry.factory) loadFactoryPreset (entry.factoryIndex);
        else               loadPreset (entry.file);
    });
}

juce::ValueTree AbyssalProcessor::createStateTree()
{
    auto state = apvts.copyState();
    state.setProperty ("version", 4, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetCategory", presetCategory, nullptr);
    state.setProperty ("presetKey", presetKey, nullptr);
    state.setProperty ("notes", presetNotes, nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("chainOrder", chainOrder.toString(), nullptr);
    state.setProperty ("uiPage", lastUiPage, nullptr);
    state.setProperty ("uiScale", uiScale, nullptr);
    state.setProperty ("namPath", getNamFile().getFullPathName(), nullptr);
    state.setProperty ("irA", getUserIR (0).getFullPathName(), nullptr);
    state.setProperty ("irB", getUserIR (1).getFullPathName(), nullptr);
    return state;
}

void AbyssalProcessor::applyStateTree (const juce::ValueTree& source, Restore mode)
{
    if (! source.hasType (apvts.state.getType()))
        return;

    auto tree = source.createCopy();
    tree.removeChild (tree.getChildWithName ("SNAPSHOT"), nullptr); // session-only A/B slot, not a parameter
    migrateState (tree);

    // Parameters newer than the saved state start from their defaults, not from whatever the
    // previous sound had them at.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! tree.getChildWithProperty ("id", rp->paramID).isValid())
            {
                juce::ValueTree child ("PARAM");
                child.setProperty ("id", rp->paramID, nullptr);
                child.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
                tree.appendChild (child, nullptr);
            }

    // Presets never change routing / input level / bypass / calibration.
    const bool keepGlobals = mode != Restore::session;
    std::vector<std::pair<juce::RangedAudioParameter*, float>> globals;
    if (keepGlobals)
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (isGlobalParameter (rp->paramID))
                    globals.push_back ({ rp, rp->getValue() });

    apvts.replaceState (tree);

    for (auto& [param, value] : globals)
        param->setValueNotifyingHost (value);

    setChainOrder (ChainOrder::fromString (tree.getProperty ("chainOrder").toString()));
    if (mode == Restore::session)
    {
        lastUiPage = (int) tree.getProperty ("uiPage", 1);
        uiScale = (double) tree.getProperty ("uiScale", uiScale);
    }
    if (mode != Restore::preset)
    {
        currentProgram = (int) tree.getProperty ("program", 0);
        setPresetInfo (tree.getProperty ("presetName", "Init").toString(),
                       tree.getProperty ("presetCategory", "Utility").toString(),
                       tree.getProperty ("presetKey", "factory:Init").toString(),
                       tree.getProperty ("notes").toString());
    }
    restoreFiles (tree);
}

void AbyssalProcessor::restoreFiles (const juce::ValueTree& state)
{
    // Synchronous on purpose: the host expects the sound to be ready once the state is set.
    const juce::File nam (state.getProperty ("namPath").toString());
    if (nam.existsAsFile() && nam != getNamFile())
    {
        ++namLoadGeneration;
        engine.getNam().loadModel (nam);
    }

    const juce::String irKeys[] = { "irA", "irB" };
    for (int s = 0; s < 2; ++s)
    {
        const juce::File ir (state.getProperty (irKeys[s]).toString());
        if (ir.existsAsFile() && ir != getUserIR (s))
        {
            // Load the file but keep whatever source the preset selected.
            const float src = apvts.getRawParameterValue (ids::cab (s, "src"))->load();
            engine.getCabinet().loadUserIR (s, ir);
            engine.getCabinet().setSource (s, (int) src);
        }
    }
    syncCabSources();
}

void AbyssalProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = createStateTree();
    state.setProperty ("activeSnapshot", activeSnapshot, nullptr);
    if (storedSnapshot.isValid())
    {
        juce::ValueTree slot ("SNAPSHOT");
        slot.appendChild (storedSnapshot.createCopy(), nullptr);
        state.appendChild (slot, nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AbyssalProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        auto restore = [this, tree]
        {
            applyStateTree (tree, Restore::session);
            activeSnapshot = juce::jlimit (0, 1, (int) tree.getProperty ("activeSnapshot", 0));
            storedSnapshot = tree.getChildWithName ("SNAPSHOT").getChild (0).createCopy();
            undoManager.clearUndoHistory();
            snapshotChanged.sendChangeMessage();
        };
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            restore();
        else
            juce::MessageManager::callAsync (restore);
    }
}

juce::String AbyssalProcessor::savePreset (const juce::File& file, const juce::String& category, const juce::String& notes)
{
    auto target = file.withFileExtension ("abyssal");
    const auto cat = category.trim().isNotEmpty() ? category.trim() : juce::String (PresetManager::userCategory);
    setPresetInfo (target.getFileNameWithoutExtension(), cat, "user:" + target.getFullPathName(), notes);
    auto xml = createStateTree().createXml();
    if (xml == nullptr || ! xml->writeTo (target))
        return "Couldn't write " + target.getFullPathName();
    presets.refresh();
    return {};
}

juce::String AbyssalProcessor::loadPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return "Couldn't read " + file.getFileName();
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (apvts.state.getType()))
        return file.getFileName() + " is not an Abyssal preset";
    applyStateTree (tree, Restore::preset);
    const auto category = tree.getProperty ("presetCategory").toString().trim();
    setPresetInfo (file.getFileNameWithoutExtension(), category.isNotEmpty() ? category : juce::String (PresetManager::userCategory),
                   "user:" + file.getFullPathName(), tree.getProperty ("notes").toString());
    return {};
}

juce::String AbyssalProcessor::editUserPreset (const PresetManager::Entry& entry, const juce::String& newName,
                                               const juce::String& category, const juce::String& notes)
{
    if (entry.factory || ! entry.file.existsAsFile())
        return "Only user presets can be edited";

    const auto name = juce::File::createLegalFileName (newName.trim());
    if (name.isEmpty())
        return "The preset needs a name";

    auto xml = juce::XmlDocument::parse (entry.file);
    if (xml == nullptr)
        return "Couldn't read " + entry.file.getFileName();

    const auto target = entry.file.getSiblingFile (name + ".abyssal");
    const bool renaming = target != entry.file;
    // Case-only renames point at the same file on Windows, so don't treat those as clashes.
    if (renaming && target.existsAsFile() && ! target.getFullPathName().equalsIgnoreCase (entry.file.getFullPathName()))
        return "A preset called \"" + name + "\" already exists";

    const auto cat = category.trim().isNotEmpty() ? category.trim() : juce::String (PresetManager::userCategory);
    xml->setAttribute ("presetName", name);
    xml->setAttribute ("presetCategory", cat);
    xml->setAttribute ("notes", notes);

    if (renaming)
    {
        if (! entry.file.moveFileTo (target))
            return "Couldn't rename " + entry.file.getFileName();
        presets.renameFavourite (entry.key(), "user:" + target.getFullPathName());
    }
    if (! xml->writeTo (target))
        return "Couldn't write " + target.getFullPathName();

    if (presetKey == entry.key())
        setPresetInfo (name, cat, "user:" + target.getFullPathName(), notes);
    presets.refresh();
    return {};
}

bool AbyssalProcessor::deleteUserPreset (const PresetManager::Entry& entry)
{
    if (entry.factory || ! entry.file.isAChildOf (PresetManager::getUserPresetDirectory()) || ! entry.file.moveToTrash())
        return false;
    presets.renameFavourite (entry.key(), {});
    if (presetKey == entry.key()) // the sound stays; it just isn't saved anywhere any more
        setPresetInfo (presetName, presetCategory, "unsaved:" + presetName, presetNotes);
    presets.refresh();
    return true;
}

//==============================================================================
juce::AudioProcessorEditor* AbyssalProcessor::createEditor() { return new AbyssalEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AbyssalProcessor(); }
