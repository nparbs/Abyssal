#include "PresetBrowser.h"
#include "../PluginProcessor.h"

namespace abyssal::ui
{

void drawStar (juce::Graphics& g, juce::Rectangle<float> area, bool filled, juce::Colour colour)
{
    const float r = std::min (area.getWidth(), area.getHeight()) * 0.5f;
    juce::Path star;
    star.addStar (area.getCentre(), 5, r * 0.45f, r, 0.0f);
    g.setColour (colour);
    if (filled) g.fillPath (star);
    else        g.strokePath (star, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
}

void StarButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panelHi.brighter (highlighted ? 0.08f : 0.0f));
    g.fillRoundedRectangle (r.reduced (0.5f), 4.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    drawStar (g, r.reduced (7.0f), filled, filled ? colours::amber : colours::textDim);
}

//==============================================================================
PresetBrowser::PresetBrowser (AbyssalProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);

    search.setTextToShowWhenEmpty ("Search presets...", colours::textDim);
    search.setFont (font (15.0f));
    search.setColour (juce::TextEditor::backgroundColourId, colours::bg);
    search.setColour (juce::TextEditor::outlineColourId, colours::outline);
    search.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    search.setColour (juce::TextEditor::textColourId, colours::text);
    search.onTextChange = [this] { applyFilter(); };
    search.onEscapeKey = [this] { close(); };
    search.onReturnKey = [this] { if (visible.size() > 0) { loadRow (0); close(); } };
    addAndMakeVisible (search);

    closeButton.onClick = [this] { close(); };
    addAndMakeVisible (closeButton);

    list.setRowHeight (34);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    proc.presetChanged.addChangeListener (this);
    rebuildCategories();
    applyFilter();
}

PresetBrowser::~PresetBrowser() { proc.presetChanged.removeChangeListener (this); }

void PresetBrowser::rebuildCategories()
{
    categoryButtons.clear();
    for (auto& c : proc.presets.getFilterCategories())
    {
        auto* b = categoryButtons.add (new juce::TextButton (c.toUpperCase()));
        b->setClickingTogglesState (false);
        b->setToggleState (c == category, juce::dontSendNotification);
        b->onClick = [this, c]
        {
            category = c;
            for (auto* other : categoryButtons)
                other->setToggleState (other->getButtonText() == c.toUpperCase(), juce::dontSendNotification);
            applyFilter();
        };
        addAndMakeVisible (b);
    }
    resized();
}

juce::Array<int> PresetBrowser::getVisibleEntries() const
{
    return proc.presets.filter (category, search.getText());
}

void PresetBrowser::applyFilter()
{
    visible = getVisibleEntries();
    list.updateContent();

    // Highlight the loaded preset without reloading it.
    const juce::ScopedValueSetter<bool> svs (suppressLoad, true);
    const int current = proc.presets.indexOfKey (proc.getPresetKey());
    const int row = visible.indexOf (current);
    if (row >= 0) { list.selectRow (row); list.scrollToEnsureRowIsOnscreen (row); }
    else          list.deselectAllRows();
    list.repaint();
}

void PresetBrowser::open()
{
    proc.presets.refresh();
    rebuildCategories();
    applyFilter();
    setVisible (true);
    toFront (true);
    search.grabKeyboardFocus();
}

void PresetBrowser::close()
{
    setVisible (false);
    if (onClose) onClose();
}

void PresetBrowser::loadRow (int row)
{
    if (! juce::isPositiveAndBelow (row, visible.size()))
        return;
    const auto& entries = proc.presets.getEntries();
    const int idx = visible[row];
    if (juce::isPositiveAndBelow (idx, entries.size()))
        proc.loadEntry (entries.getReference (idx));
    list.repaint();
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, visible.size()))
        return;
    const auto& e = proc.presets.getEntries().getReference (visible[row]);
    const bool current = e.key() == proc.getPresetKey();
    auto r = juce::Rectangle<int> (0, 0, width, height).reduced (2, 2).toFloat();

    g.setColour (selected ? colours::accent.withAlpha (0.25f) : (row % 2 ? colours::panelHi : colours::panel));
    g.fillRoundedRectangle (r, 5.0f);
    if (current)
    {
        g.setColour (colours::accent);
        g.drawRoundedRectangle (r, 5.0f, 1.5f);
    }

    drawStar (g, juce::Rectangle<float> (10, (float) height * 0.5f - 9, 18, 18),
              proc.presets.isFavourite (e.key()), proc.presets.isFavourite (e.key()) ? colours::amber : colours::textDim);

    g.setColour (colours::text);
    g.setFont (font (15.0f, current));
    g.drawText (e.name, 40, 0, width - 300, height, juce::Justification::centredLeft);

    g.setColour (colours::textDim);
    g.setFont (font (12.0f, true));
    g.drawText (e.category.toUpperCase(), width - 250, 0, 140, height, juce::Justification::centredLeft);

    auto chip = juce::Rectangle<float> ((float) width - 96, (float) height * 0.5f - 10, 84, 20);
    g.setColour (e.factory ? colours::outline : colours::accentAlt.withAlpha (0.25f));
    g.fillRoundedRectangle (chip, 10.0f);
    g.setColour (e.factory ? colours::textDim : colours::accentAlt);
    g.setFont (font (10.5f, true));
    g.drawText (e.factory ? "FACTORY" : "USER", chip, juce::Justification::centred);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, visible.size()))
        return;
    const auto& entry = proc.presets.getEntries().getReference (visible[row]);

    if (e.x < 36) // star column toggles the favourite instead of loading
    {
        proc.presets.setFavourite (entry.key(), ! proc.presets.isFavourite (entry.key()));
        if (category == "Favourites") applyFilter();
        list.repaint();
        return;
    }
    loadRow (row);
}

void PresetBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent& e)
{
    if (e.x < 36) return;
    loadRow (row);
    close();
}

void PresetBrowser::selectedRowsChanged (int lastRowSelected)
{
    if (! suppressLoad && lastRowSelected >= 0)
        loadRow (lastRowSelected);
}

bool PresetBrowser::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    if (k == juce::KeyPress::downKey || k == juce::KeyPress::upKey)
    {
        list.grabKeyboardFocus();
        return list.keyPressed (k);
    }
    return false;
}

void PresetBrowser::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()))
        close(); // click outside the panel dismisses it
}

juce::Rectangle<int> PresetBrowser::panelBounds() const
{
    return getLocalBounds().reduced (40, 20);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));
    auto panel = panelBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (panel, 12.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panel, 12.0f, 1.0f);

    auto inner = panelBounds().reduced (20, 16);
    g.setColour (colours::text);
    g.setFont (font (20.0f, true));
    g.drawText ("PRESETS", inner.removeFromTop (34), juce::Justification::centredLeft);

    g.setColour (colours::textDim);
    g.setFont (font (11.0f, true));
    g.drawText ("CATEGORY", inner.withTrimmedTop (10).removeFromTop (16).removeFromLeft (180), juce::Justification::centredLeft);
    g.setFont (font (12.0f));
    g.drawText ("Click or use the arrow keys to audition  -  Enter / double-click to close  -  click the star to favourite",
                panelBounds().reduced (20, 10).removeFromBottom (20), juce::Justification::centredRight);
}

void PresetBrowser::resized()
{
    auto inner = panelBounds().reduced (20, 16);
    auto top = inner.removeFromTop (34);
    closeButton.setBounds (top.removeFromRight (90).reduced (0, 2));
    top.removeFromRight (12);
    search.setBounds (top.removeFromRight (360).reduced (0, 2));

    inner.removeFromTop (30);
    inner.removeFromBottom (24);
    auto cats = inner.removeFromLeft (180);
    for (auto* b : categoryButtons)
    {
        b->setBounds (cats.removeFromTop (32).reduced (0, 2));
        cats.removeFromTop (2);
    }
    inner.removeFromLeft (16);
    list.setBounds (inner);
}

} // namespace abyssal::ui
