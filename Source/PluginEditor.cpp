#include "PluginEditor.h"

using namespace abyssal;
using namespace abyssal::ui;

static constexpr int headerHeight = 72;
static constexpr int chainHeight = 84;

AbyssalEditor::AbyssalEditor (AbyssalProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      inputKnob (p.apvts, ids::inGain, "Input", colours::accentAlt, true),
      outputKnob (p.apvts, ids::outGain, "Output", colours::accentAlt, true),
      inMeter ([&p] { return p.getInputPeak(); }),
      outMeter ([&p] { return p.getOutputPeak(); }),
      chain (p), preFX (p), ampPage (p), cabPage (p), eqPage (p), postFX (p)
{
    setLookAndFeel (&laf);

    pages = { &preFX, &ampPage, &cabPage, &eqPage, &postFX };
    for (auto* pg : pages)
        addChildComponent (pg);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevPreset, &nextPreset, &savePreset, &loadPreset,
                                                             &inputKnob, &outputKnob, &inMeter, &outMeter, &chain })
        addAndMakeVisible (c);

    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.onChange = [this] { selectPreset (presetBox.getSelectedId()); };
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };

    savePreset.onClick = [this]
    {
        auto dir = proc.getUserPresetDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Save preset", dir.getChildFile (proc.getPresetName() + ".abyssal"), "*.abyssal");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            const auto err = proc.savePreset (f);
            if (err.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset", err);
            refreshPresetList();
        });
    };

    loadPreset.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load preset", proc.getUserPresetDirectory(), "*.abyssal");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            const auto err = proc.loadPreset (f);
            if (err.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load preset", err);
            refreshPresetList();
        });
    };

    chain.onSelect = [this] (int i) { showPage (i); };

    refreshPresetList();
    showPage (juce::jlimit (0, 4, proc.lastUiPage));
    setSize (1180, 800);
    startTimerHz (8);
}

AbyssalEditor::~AbyssalEditor() { setLookAndFeel (nullptr); }

void AbyssalEditor::showPage (int index)
{
    for (int i = 0; i < (int) pages.size(); ++i)
        pages[(size_t) i]->setVisible (i == index);
    chain.setSelected (index);
    proc.lastUiPage = index;
}

void AbyssalEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& factory = getFactoryPresets();
    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < (int) factory.size(); ++i)
        presetBox.addItem (factory[(size_t) i].name, i + 1);

    userPresets = proc.getUserPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.abyssal");
    userPresets.sort();
    if (! userPresets.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userPresets.size(); ++i)
            presetBox.addItem (userPresets[i].getFileNameWithoutExtension(), userIdOffset + i + 1);
    }

    shownPresetName = proc.getPresetName();
    presetBox.setText (shownPresetName, juce::dontSendNotification);
}

void AbyssalEditor::selectPreset (int itemId)
{
    if (itemId <= 0) return;
    if (itemId > userIdOffset)
    {
        const auto f = userPresets[itemId - userIdOffset - 1];
        if (f.existsAsFile()) proc.loadPreset (f);
    }
    else
    {
        proc.loadFactoryPreset (itemId - 1);
    }
    shownPresetName = proc.getPresetName();
}

void AbyssalEditor::stepPreset (int delta)
{
    juce::Array<int> idList;
    for (int i = 0; i < presetBox.getNumItems(); ++i)
        idList.add (presetBox.getItemId (i));
    if (idList.isEmpty()) return;

    int idx = idList.indexOf (presetBox.getSelectedId());
    if (idx < 0)
    {
        for (int i = 0; i < presetBox.getNumItems(); ++i)
            if (presetBox.getItemText (i) == proc.getPresetName()) idx = i;
    }
    idx = (idx < 0 ? 0 : (idx + delta + idList.size()) % idList.size());
    presetBox.setSelectedId (idList[idx], juce::sendNotificationSync);
}

void AbyssalEditor::timerCallback()
{
    if (proc.getPresetName() != shownPresetName)
    {
        shownPresetName = proc.getPresetName();
        presetBox.setText (shownPresetName, juce::dontSendNotification);
    }
}

void AbyssalEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    // Header bar
    auto header = getLocalBounds().removeFromTop (headerHeight).toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17181d), 0, 0, colours::bg, 0, header.getBottom(), false));
    g.fillRect (header);
    g.setColour (colours::outline);
    g.drawHorizontalLine (headerHeight - 1, 0, (float) getWidth());

    // Logo: a stylised "abyss" chevron + wordmark
    juce::Path mark;
    const float lx = 26, ly = 18, ls = 36;
    mark.startNewSubPath (lx, ly);
    mark.lineTo (lx + ls * 0.5f, ly + ls);
    mark.lineTo (lx + ls, ly);
    mark.lineTo (lx + ls * 0.78f, ly);
    mark.lineTo (lx + ls * 0.5f, ly + ls * 0.6f);
    mark.lineTo (lx + ls * 0.22f, ly);
    mark.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::accent.brighter (0.3f), lx, ly, colours::accent.darker (0.5f), lx, ly + ls, false));
    g.fillPath (mark);

    g.setColour (colours::text);
    g.setFont (font (30.0f, true));
    g.drawText ("ABYSSAL", juce::Rectangle<float> (74, 10, 200, 34), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (font (11.0f, true));
    g.drawText ("METAL AMP SUITE", juce::Rectangle<float> (76, 42, 200, 16), juce::Justification::centredLeft);

    g.setFont (font (10.0f, true));
    g.drawText ("IN", inMeter.getBounds().translated (0, inMeter.getHeight() + 1).withHeight (12).expanded (8, 0), juce::Justification::centred);
    g.drawText ("OUT", outMeter.getBounds().translated (0, outMeter.getHeight() + 1).withHeight (12).expanded (10, 0), juce::Justification::centred);
}

void AbyssalEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (headerHeight).reduced (20, 14);

    // right: input / output
    auto io = header.removeFromRight (230);
    outMeter.setBounds (io.removeFromRight (10).withTrimmedBottom (12));
    io.removeFromRight (4);
    outputKnob.setBounds (io.removeFromRight (64).expanded (0, 8));
    io.removeFromRight (18);
    inMeter.setBounds (io.removeFromRight (10).withTrimmedBottom (12));
    io.removeFromRight (4);
    inputKnob.setBounds (io.removeFromRight (64).expanded (0, 8));

    // centre: preset browser
    auto presets = header.withSizeKeepingCentre (520, 34);
    prevPreset.setBounds (presets.removeFromLeft (34));
    presets.removeFromLeft (6);
    loadPreset.setBounds (presets.removeFromRight (66));
    presets.removeFromRight (6);
    savePreset.setBounds (presets.removeFromRight (66));
    presets.removeFromRight (6);
    nextPreset.setBounds (presets.removeFromRight (34));
    presets.removeFromRight (6);
    presetBox.setBounds (presets);

    chain.setBounds (r.removeFromTop (chainHeight));
    for (auto* pg : pages)
        pg->setBounds (r.reduced (8, 6));
}
