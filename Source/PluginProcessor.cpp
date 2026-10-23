#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace abyssal;

AbyssalProcessor::AbyssalProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      cache (apvts)
{
    uiScale = presets.getDefaultUiScale();
    engine.setChainOrder (chainOrder);
    startTimerHz (20);
}

AbyssalProcessor::~AbyssalProcessor() { stopTimer(); }

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
    const bool idle = ! params.bypass
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
    for (int s = 0; s < 2; ++s)
    {
        const int src = (int) apvts.getRawParameterValue (ids::cab (s, "src"))->load();
        if (src != engine.getCabinet().getLoadedSource (s))
            engine.getCabinet().setSource (s, src);
    }
}

void AbyssalProcessor::timerCallback() { syncCabSources(); }

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

void AbyssalProcessor::setPresetInfo (const juce::String& name, const juce::String& category, const juce::String& key)
{
    presetName = name;
    presetCategory = category;
    presetKey = key;
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
    if (entry.factory) loadFactoryPreset (entry.factoryIndex);
    else               loadPreset (entry.file);
}

juce::ValueTree AbyssalProcessor::createStateTree()
{
    auto state = apvts.copyState();
    state.setProperty ("version", 2, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetCategory", presetCategory, nullptr);
    state.setProperty ("presetKey", presetKey, nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("chainOrder", chainOrder.toString(), nullptr);
    state.setProperty ("uiPage", lastUiPage, nullptr);
    state.setProperty ("uiScale", uiScale, nullptr);
    state.setProperty ("namPath", getNamFile().getFullPathName(), nullptr);
    state.setProperty ("irA", getUserIR (0).getFullPathName(), nullptr);
    state.setProperty ("irB", getUserIR (1).getFullPathName(), nullptr);
    return state;
}

void AbyssalProcessor::applyStateTree (const juce::ValueTree& tree, bool keepGlobals)
{
    if (! tree.hasType (apvts.state.getType()))
        return;

    // Presets never change routing / input level / bypass.
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
    if (! keepGlobals)
    {
        currentProgram = (int) tree.getProperty ("program", 0);
        lastUiPage = (int) tree.getProperty ("uiPage", 1);
        uiScale = (double) tree.getProperty ("uiScale", uiScale);
        setPresetInfo (tree.getProperty ("presetName", "Init").toString(),
                       tree.getProperty ("presetCategory", "Utility").toString(),
                       tree.getProperty ("presetKey", "factory:Init").toString());
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
    if (auto xml = createStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void AbyssalProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            applyStateTree (tree, false);
        else
            juce::MessageManager::callAsync ([this, tree] { applyStateTree (tree, false); });
    }
}

juce::String AbyssalProcessor::savePreset (const juce::File& file)
{
    auto target = file.withFileExtension ("abyssal");
    setPresetInfo (target.getFileNameWithoutExtension(), PresetManager::userCategory, "user:" + target.getFullPathName());
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
    applyStateTree (tree, true);
    setPresetInfo (file.getFileNameWithoutExtension(), PresetManager::userCategory, "user:" + file.getFullPathName());
    return {};
}

//==============================================================================
juce::AudioProcessorEditor* AbyssalProcessor::createEditor() { return new AbyssalEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AbyssalProcessor(); }
