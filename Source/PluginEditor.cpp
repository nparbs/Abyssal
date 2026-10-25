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

    auto area = getLocalBounds().reduced (10, 2);
    g.setColour (colours::textDim);
    g.setFont (font (9.5f, true));
    g.drawText (category.toUpperCase(), area.removeFromTop (12), juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.setFont (font (15.0f, true));
    g.drawFittedText (name, area, juce::Justification::centredLeft, 1, 0.75f);
}

//==============================================================================
void ArrowButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panelHi.brighter (highlighted && isEnabled() ? 0.08f : 0.0f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    // A hooked arrow: an arc that ends in an arrowhead pointing back (undo) or forward (redo).
    auto a = r.reduced (8.0f).translated (0.0f, down ? 1.0f : 0.0f);
    const float cx = a.getCentreX(), cy = a.getCentreY() + 1.0f, rad = a.getWidth() * 0.42f;
    juce::Path arc;
    arc.addCentredArc (cx, cy, rad, rad, 0.0f, -juce::MathConstants<float>::halfPi * 1.25f, juce::MathConstants<float>::halfPi * 0.9f, true);
    juce::Path head;
    const float hx = cx - rad * std::sin (juce::MathConstants<float>::halfPi * 1.25f);
    const float hy = cy - rad * std::cos (juce::MathConstants<float>::halfPi * 1.25f);
    head.addTriangle (hx - 5.0f, hy - 1.0f, hx + 3.0f, hy - 6.0f, hx + 2.5f, hy + 3.5f);
    if (redo)
    {
        const auto flip = juce::AffineTransform::scale (-1.0f, 1.0f, cx, cy);
        arc.applyTransform (flip);
        head.applyTransform (flip);
    }
    const auto c = isEnabled() ? (highlighted ? colours::text : colours::textDim) : colours::outline;
    g.setColour (c);
    g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillPath (head);
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
      browser (p), tone3000 (p), tuner (p)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    pages = { &preBoard, &ampPage, &cabPage, &eqPage, &postBoard };
    for (auto* pg : pages)
        addChildComponent (pg);

    for (auto* c : std::initializer_list<juce::Component*> { &undoButton, &redoButton, &snapA, &snapB, &tunerButton,
                                                             &prevPreset, &presetDisplay, &nextPreset, &favourite, &savePreset,
                                                             &loadPreset, &inputSelect, &outputSelect, &inputKnob,
                                                             &outputKnob, &inMeter, &outMeter, &chain })
        addAndMakeVisible (c);
    addChildComponent (browser);
    addChildComponent (tone3000);
    addChildComponent (tuner);
    addChildComponent (saveDialog);

    undoButton.onClick = [this] { proc.undo(); };
    redoButton.onClick = [this] { proc.redo(); };

    for (auto* b : { &snapA, &snapB })
    {
        const int slot = b == &snapA ? 0 : 1;
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, colours::accentAlt.withMultipliedBrightness (0.7f));
        b->setTooltip (juce::String ("Compare: switch to sound ") + (slot == 0 ? "A" : "B") + "  (right-click to copy)");
        b->onClick = [this, slot]
        {
            if (std::exchange (snapMenuShown, false)) return;
            proc.selectSnapshot (slot);
        };
        b->addMouseListener (this, false);
    }

    tunerButton.setClickingTogglesState (false);
    tunerButton.setColour (juce::TextButton::buttonOnColourId, colours::accent.withMultipliedBrightness (0.7f));
    tunerButton.setTooltip ("Chromatic tuner (mutes the output while open)");
    tunerButton.onClick = [this]
    {
        if (tuner.isVisible()) tuner.close();
        else { browser.setVisible (false); tuner.open(); }
    };
    tuner.onVisibilityChange = [this] { tunerButton.setToggleState (tuner.isVisible(), juce::dontSendNotification); };
    proc.openTone3000 = [this] (int target)
    {
        browser.setVisible (false);
        tone3000.open (target == 0 ? Tone3000Panel::Target::amp
                                   : target == 1 ? Tone3000Panel::Target::cabA : Tone3000Panel::Target::cabB);
    };

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

    savePreset.setTooltip ("Save the current sound as a user preset");
    savePreset.onClick = [this] { openSaveDialog(); };

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
    proc.snapshotChanged.addChangeListener (this);
    proc.getUndoManager().addChangeListener (this);

    refreshPresetInfo();
    refreshUndoButtons();
    refreshSnapshotButtons();
    showPage (juce::jlimit (0, 4, proc.lastUiPage));
}

MainView::~MainView()
{
    proc.openTone3000 = nullptr;
    proc.presetChanged.removeChangeListener (this);
    proc.snapshotChanged.removeChangeListener (this);
    proc.getUndoManager().removeChangeListener (this);
}

void MainView::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &proc.getUndoManager()) refreshUndoButtons();
    else if (source == &proc.snapshotChanged) refreshSnapshotButtons();
    else refreshPresetInfo();
}

void MainView::refreshUndoButtons()
{
    auto& um = proc.getUndoManager();
    undoButton.setEnabled (um.canUndo());
    redoButton.setEnabled (um.canRedo());
    undoButton.setTooltip (um.canUndo() ? "Undo " + um.getUndoDescription() + "  (Ctrl+Z)" : juce::String ("Nothing to undo"));
    redoButton.setTooltip (um.canRedo() ? "Redo " + um.getRedoDescription() + "  (Ctrl+Y)" : juce::String ("Nothing to redo"));
}

void MainView::refreshSnapshotButtons()
{
    snapA.setToggleState (proc.getActiveSnapshot() == 0, juce::dontSendNotification);
    snapB.setToggleState (proc.getActiveSnapshot() == 1, juce::dontSendNotification);
}

void MainView::showSnapshotMenu (int slot)
{
    const auto other = juce::String (proc.getActiveSnapshot() == 0 ? "B" : "A");
    juce::PopupMenu m;
    m.addSectionHeader ("A / B COMPARE");
    m.addItem (1, "Copy this sound to " + other);
    m.addItem (2, "Switch to " + juce::String (slot == 0 ? "A" : "B"), proc.getActiveSnapshot() != slot);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slot == 0 ? &snapA : &snapB), [this, slot] (int r)
    {
        if (r == 1) proc.copyToOtherSnapshot();
        if (r == 2) proc.selectSnapshot (slot);
    });
}

void MainView::openSaveDialog()
{
    browser.setVisible (false);
    tuner.setVisible (false);

    // Start from the loaded preset's details. Factory presets keep their category.
    const PresetDialog::Details initial { proc.getPresetName(), proc.getPresetCategory() == "Utility" ? juce::String (PresetManager::userCategory)
                                                                                                      : proc.getPresetCategory(),
                                          proc.getPresetNotes() };
    saveDialog.show ("SAVE PRESET", "SAVE", initial, proc.presets.getSaveCategories(), [this] (const PresetDialog::Details& d)
    {
        const auto name = juce::File::createLegalFileName (d.name);
        if (name.isEmpty())
            return juce::String ("That name can't be used as a file name");
        const auto file = PresetManager::getUserPresetDirectory().getChildFile (name + ".abyssal");
        const bool overwritingOther = file.existsAsFile() && "user:" + file.getFullPathName() != proc.getPresetKey();

        auto save = [this, file, d]
        {
            const auto err = proc.savePreset (file, d.category, d.notes);
            if (err.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset", err);
        };

        if (! overwritingOther)
        {
            save();
            return juce::String();
        }

        auto options = juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, "Save preset",
                                                                     "A preset called \"" + name + "\" already exists. Replace it?",
                                                                     "Replace", "Cancel", this);
        juce::AlertWindow::showAsync (options, [this, save] (int result)
        {
            if (result == 1) { save(); saveDialog.dismiss(); }
        });
        return juce::String ("Waiting for confirmation...");
    });
}

void MainView::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent != &snapA && e.eventComponent != &snapB)
        return;
    snapMenuShown = e.mods.isPopupMenu();
    if (snapMenuShown)
        showSnapshotMenu (e.eventComponent == &snapA ? 0 : 1);
}

bool MainView::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    if (mods.isCommandDown() && k.getKeyCode() == 'Z')
    {
        if (mods.isShiftDown()) proc.redo();
        else                    proc.undo();
        return true;
    }
    if (mods.isCommandDown() && k.getKeyCode() == 'Y')
    {
        proc.redo();
        return true;
    }
    return false;
}

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
    header.removeFromLeft (200); // logo

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

    // centre: undo / redo, preset bar, A/B, tuner
    auto bar = header.withSizeKeepingCentre (header.getWidth(), 36);
    bar.removeFromRight (12);
    undoButton.setBounds (bar.removeFromLeft (30));
    bar.removeFromLeft (4);
    redoButton.setBounds (bar.removeFromLeft (30));
    bar.removeFromLeft (12);
    tunerButton.setBounds (bar.removeFromRight (62));
    bar.removeFromRight (8);
    snapB.setBounds (bar.removeFromRight (30));
    bar.removeFromRight (3);
    snapA.setBounds (bar.removeFromRight (30));
    bar.removeFromRight (12);

    prevPreset.setBounds (bar.removeFromLeft (28));
    bar.removeFromLeft (4);
    loadPreset.setBounds (bar.removeFromRight (50));
    bar.removeFromRight (4);
    savePreset.setBounds (bar.removeFromRight (50));
    bar.removeFromRight (4);
    favourite.setBounds (bar.removeFromRight (34));
    bar.removeFromRight (4);
    nextPreset.setBounds (bar.removeFromRight (28));
    bar.removeFromRight (4);
    presetDisplay.setBounds (bar);

    chain.setBounds (r.removeFromTop (chainHeight));
    for (auto* pg : pages)
        pg->setBounds (r.reduced (8, 6));
    browser.setBounds (getLocalBounds().withTrimmedTop (headerHeight));
    tone3000.setBounds (getLocalBounds().withTrimmedTop (headerHeight));
    tuner.setBounds (getLocalBounds().withTrimmedTop (headerHeight));
    saveDialog.setBounds (getLocalBounds());
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
