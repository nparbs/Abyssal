#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace abyssal;

AbyssalProcessor::AbyssalProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      cache (apvts)
{
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

void AbyssalProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    syncCabSources();
    setLatencySamples (engine.getLatencySamples());
}

void AbyssalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    cache.fill (params);
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                params.bpm = *bpm;

    engine.process (buffer, params);
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

juce::String AbyssalProcessor::loadNamModel (const juce::File& file)
{
    const auto err = engine.getNam().loadModel (file);
    if (err.isEmpty())
        if (auto* p = apvts.getParameter (ids::ampMode))
            p->setValueNotifyingHost (1.0f); // switch to the capture automatically
    return err;
}

void AbyssalProcessor::unloadNamModel()
{
    engine.getNam().unloadModel();
    if (auto* p = apvts.getParameter (ids::ampMode))
        p->setValueNotifyingHost (0.0f);
}

juce::String AbyssalProcessor::loadUserIR (int slot, const juce::File& file)
{
    if (auto* p = apvts.getParameter (ids::cab (slot, "src")))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) Cabinet::userSource));
    if (auto* p = apvts.getParameter (ids::cab (slot, "on")))
        p->setValueNotifyingHost (1.0f);
    return engine.getCabinet().loadUserIR (slot, file);
}

//============================================================================== presets
juce::File AbyssalProcessor::getUserPresetDirectory() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Abyssal").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

int AbyssalProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }

const juce::String AbyssalProcessor::getProgramName (int index)
{
    const auto& p = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void AbyssalProcessor::setCurrentProgram (int index) { loadFactoryPreset (index); }

void AbyssalProcessor::loadFactoryPreset (int index)
{
    const auto& p = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) p.size()))
        return;
    currentProgram = index;
    applyFactoryPreset (apvts, p[(size_t) index]);
    presetName = p[(size_t) index].name;
}

juce::ValueTree AbyssalProcessor::createStateTree()
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiPage", lastUiPage, nullptr);
    state.setProperty ("namPath", getNamFile().getFullPathName(), nullptr);
    state.setProperty ("irA", getUserIR (0).getFullPathName(), nullptr);
    state.setProperty ("irB", getUserIR (1).getFullPathName(), nullptr);
    return state;
}

void AbyssalProcessor::applyStateTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (apvts.state.getType()))
        return;
    apvts.replaceState (tree);
    presetName = tree.getProperty ("presetName", "Init").toString();
    lastUiPage = (int) tree.getProperty ("uiPage", 1);
    restoreFiles (tree);
}

void AbyssalProcessor::restoreFiles (const juce::ValueTree& state)
{
    const juce::File nam (state.getProperty ("namPath").toString());
    const bool wantsNam = apvts.getRawParameterValue (ids::ampMode)->load() > 0.5f;
    if (nam.existsAsFile() && nam != getNamFile())
    {
        engine.getNam().loadModel (nam);
        if (! wantsNam)
            if (auto* p = apvts.getParameter (ids::ampMode)) p->setValueNotifyingHost (0.0f);
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
            applyStateTree (tree);
        else
            juce::MessageManager::callAsync ([this, tree] { applyStateTree (tree); });
    }
}

juce::String AbyssalProcessor::savePreset (const juce::File& file)
{
    auto target = file.withFileExtension ("abyssal");
    presetName = target.getFileNameWithoutExtension();
    auto xml = createStateTree().createXml();
    if (xml == nullptr || ! xml->writeTo (target))
        return "Couldn't write " + target.getFullPathName();
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
    applyStateTree (tree);
    presetName = file.getFileNameWithoutExtension();
    return {};
}

//==============================================================================
juce::AudioProcessorEditor* AbyssalProcessor::createEditor() { return new AbyssalEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AbyssalProcessor(); }
