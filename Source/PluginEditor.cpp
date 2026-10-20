#include "PluginEditor.h"

using namespace abyssal;
using namespace abyssal::ui;

static constexpr int headerHeight = 72;
static constexpr int chainHeight = 84;

//==============================================================================
void PresetDisplay::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::bg.brighter (highlighted ? 0.06f : 0.0f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (highlighted ? colours::accent.withAlpha (0.7f) : colours::outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    auto area = getLocalBounds().reduced (12, 0);
    g.setColour (colours::textDim);
    g.setFont (font (10.5f, true));
    g.drawText (category.toUpperCase(), area.removeFromRight (90), juce::Justification::centredRight);
    g.setColour (colours::text);
    g.setFont (font (16.0f, true));
    g.drawFittedText (name, area, juce::Justification::centredLeft, 1, 0.8f);
}

//==============================================================================
MainView::MainView (AbyssalProcessor& p)
    : proc (p),
      inputSelect (p.apvts, ids::inputMode),
      outputSelect (p.apvts, ids::outputMode),
      inputKnob (p.apvts, ids::inGain, "Input", colours::accentAlt, true),
      outputKnob (p.apvts, ids::outGain, "Output", colours::accentAlt, true),
      inMeter ([&p] { return p.getInputPeak(); }),
      outMeter ([&p] { return p.getOutputPeak(); }),
      chain (p), pedals (p),
      preBoard (p, pedals, true), ampPage (p), cabPage (p), eqPage (p), postBoard (p, pedals, false),
      browser (p)
{
    setOpaque (true);

    pages = { &preBoard, &ampPage, &cabPage, &eqPage, &postBoard };
    for (auto* pg : pages)
        addChildComponent (pg);

    for (auto* c : std::initializer_list<juce::Component*> { &prevPreset, &presetDisplay, &nextPreset, &favourite, &savePreset,
                                                             &loadPreset, &inputSelect, &outputSelect, &inputKnob,
                                                             &outputKnob, &inMeter, &outMeter, &chain })
        addAndMakeVisible (c);
    addChildComponent (browser);

    inputSelect.setTooltip ("Which input channel carries your guitar");
    outputSelect.setTooltip ("Stereo, or mono for double-tracked hard-panned takes");

    presetDisplay.onClick = [this] { if (browser.isVisible()) browser.close(); else browser.open(); };
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    favourite.onClick = [this]
    {
        const auto key = proc.getPresetKey();
        proc.presets.setFavourite (key, ! proc.presets.isFavourite (key));
        refreshPresetInfo();
    };

    savePreset.onClick = [this]
    {
        auto dir = PresetManager::getUserPresetDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Save preset", dir.getChildFile (proc.getPresetName() + ".abyssal"), "*.abyssal");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            const auto err = proc.savePreset (f);
            if (err.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset", err);
        });
    };

    loadPreset.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load preset", PresetManager::getUserPresetDirectory(), "*.abyssal");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            const auto err = proc.loadPreset (f);
            if (err.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load preset", err);
        });
    };

    chain.onSelect = [this] (int i) { showPage (i); };
    proc.presetChanged.addChangeListener (this);

    refreshPresetInfo();
    showPage (juce::jlimit (0, 4, proc.lastUiPage));
}

MainView::~MainView() { proc.presetChanged.removeChangeListener (this); }

void MainView::showPage (int index)
{
    for (int i = 0; i < (int) pages.size(); ++i)
        pages[(size_t) i]->setVisible (i == index);
    chain.setSelected (index);
    proc.lastUiPage = index;
}

void MainView::dragOperationEnded (const juce::DragAndDropTarget::SourceDetails& d)
{
    if (auto* c = d.sourceComponent.get())
        c->repaint();
}

void MainView::refreshPresetInfo()
{
    presetDisplay.name = proc.getPresetName();
    presetDisplay.category = proc.getPresetCategory();
    presetDisplay.repaint();
    favourite.filled = proc.presets.isFavourite (proc.getPresetKey());
    favourite.repaint();
}

void MainView::stepPreset (int delta)
{
    // Steps through whatever the browser is filtered to (category + search), wrapping around.
    const auto list = browser.getVisibleEntries();
    if (list.isEmpty()) return;
    const auto& entries = proc.presets.getEntries();
    int pos = list.indexOf (proc.presets.indexOfKey (proc.getPresetKey()));
    pos = pos < 0 ? 0 : (pos + delta + list.size()) % list.size();
    proc.loadEntry (entries.getReference (list[pos]));
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

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

void MainView::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (headerHeight).reduced (20, 14);
    header.removeFromLeft (230); // logo

    // right: routing + input / output
    auto io = header.removeFromRight (330);
    outMeter.setBounds (io.removeFromRight (10).withTrimmedBottom (12));
    io.removeFromRight (4);
    outputKnob.setBounds (io.removeFromRight (64).expanded (0, 8));
    io.removeFromRight (16);
    inMeter.setBounds (io.removeFromRight (10).withTrimmedBottom (12));
    io.removeFromRight (4);
    inputKnob.setBounds (io.removeFromRight (64).expanded (0, 8));
    io.removeFromRight (12);
    auto routing = io.removeFromRight (104);
    inputSelect.setBounds (routing.removeFromTop (routing.getHeight() / 2).reduced (0, 2));
    outputSelect.setBounds (routing.reduced (0, 2));

    // centre: preset bar
    auto presets = header.withSizeKeepingCentre (std::min (header.getWidth(), 540), 36);
    prevPreset.setBounds (presets.removeFromLeft (32));
    presets.removeFromLeft (5);
    loadPreset.setBounds (presets.removeFromRight (58));
    presets.removeFromRight (5);
    savePreset.setBounds (presets.removeFromRight (58));
    presets.removeFromRight (5);
    favourite.setBounds (presets.removeFromRight (36));
    presets.removeFromRight (5);
    nextPreset.setBounds (presets.removeFromRight (32));
    presets.removeFromRight (5);
    presetDisplay.setBounds (presets);

    chain.setBounds (r.removeFromTop (chainHeight));
    for (auto* pg : pages)
        pg->setBounds (r.reduced (8, 6));
    browser.setBounds (getLocalBounds().withTrimmedTop (headerHeight));
}

//==============================================================================
AbyssalEditor::AbyssalEditor (AbyssalProcessor& p)
    : AudioProcessorEditor (&p), proc (p), view (p)
{
    // Read the saved size before setResizeLimits() triggers an intermediate resize.
    const double scale = juce::jlimit (0.6, 2.0, proc.uiScale);

    setLookAndFeel (&laf);
    addAndMakeVisible (view);

    // Scales the whole UI uniformly; FL Studio and Reaper both support resizable VST3 windows.
    setResizable (true, true);
    setResizeLimits (MainView::width * 6 / 10, MainView::height * 6 / 10, MainView::width * 2, MainView::height * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) MainView::width / (double) MainView::height);

    setSize (juce::roundToInt (MainView::width * scale), juce::roundToInt (MainView::height * scale));
    sizeRestored = true;
}

AbyssalEditor::~AbyssalEditor() { setLookAndFeel (nullptr); }

void AbyssalEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainView::width;
    view.setBounds (0, 0, MainView::width, MainView::height);
    view.setTransform (juce::AffineTransform::scale (scale));

    if (sizeRestored && std::abs (scale - proc.uiScale) > 0.001)
    {
        proc.uiScale = scale;
        proc.presets.setDefaultUiScale (scale); // new instances open at the last used size
    }
}
