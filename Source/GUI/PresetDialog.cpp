#include "PresetDialog.h"

namespace abyssal::ui
{

namespace
{
    void styleEditor (juce::TextEditor& e)
    {
        e.setFont (font (15.0f));
        e.setColour (juce::TextEditor::backgroundColourId, colours::bg);
        e.setColour (juce::TextEditor::outlineColourId, colours::outline);
        e.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
        e.setColour (juce::TextEditor::textColourId, colours::text);
    }
}

PresetDialog::PresetDialog()
{
    setWantsKeyboardFocus (true);

    styleEditor (name);
    name.setTextToShowWhenEmpty ("Preset name", colours::textDim);
    name.onReturnKey = [this] { submit(); };
    name.onEscapeKey = [this] { dismiss(); };

    styleEditor (notes);
    notes.setMultiLine (true, true);
    notes.setReturnKeyStartsNewLine (true);
    notes.setScrollbarsShown (true);
    notes.setTextToShowWhenEmpty ("Notes: guitar, tuning, pickups, what it's for...", colours::textDim);
    notes.onEscapeKey = [this] { dismiss(); };

    category.setEditableText (true);
    category.setTooltip ("Pick a category or type a new one");

    error.setColour (juce::Label::textColourId, colours::accent);
    error.setFont (font (13.0f, true));

    action.setColour (juce::TextButton::buttonColourId, colours::accent.withMultipliedBrightness (0.6f));
    action.onClick = [this] { submit(); };
    cancel.onClick = [this] { dismiss(); };

    for (auto* c : std::initializer_list<juce::Component*> { &name, &notes, &category, &action, &cancel, &error })
        addAndMakeVisible (c);
}

void PresetDialog::show (const juce::String& t, const juce::String& actionText, const Details& initial,
                         const juce::StringArray& categories, std::function<juce::String (const Details&)> confirm)
{
    title = t;
    action.setButtonText (actionText);
    onConfirm = std::move (confirm);

    name.setText (initial.name, false);
    notes.setText (initial.notes, false);
    category.clear (juce::dontSendNotification);
    category.addItemList (categories, 1);
    category.setText (initial.category, juce::dontSendNotification);
    error.setText ({}, juce::dontSendNotification);

    setVisible (true);
    toFront (true);
    name.grabKeyboardFocus();
    name.selectAll();
}

void PresetDialog::dismiss()
{
    setVisible (false);
    onConfirm = nullptr;
}

void PresetDialog::submit()
{
    Details d { name.getText().trim(), category.getText().trim(), notes.getText().trim() };
    if (d.name.isEmpty())
    {
        error.setText ("Give the preset a name", juce::dontSendNotification);
        name.grabKeyboardFocus();
        return;
    }
    if (onConfirm == nullptr)
        return dismiss();

    const auto err = onConfirm (d);
    if (err.isNotEmpty())
        error.setText (err, juce::dontSendNotification);
    else
        dismiss();
}

juce::Rectangle<int> PresetDialog::panelBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (560, 400);
}

void PresetDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));
    auto panel = panelBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (panel, 12.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panel, 12.0f, 1.0f);

    auto inner = panelBounds().reduced (24, 18);
    g.setColour (colours::text);
    g.setFont (font (20.0f, true));
    g.drawText (title, inner.removeFromTop (34), juce::Justification::centredLeft);

    g.setColour (colours::textDim);
    g.setFont (font (11.0f, true));
    inner.removeFromTop (8);
    g.drawText ("NAME", inner.removeFromTop (16), juce::Justification::centredLeft);
    inner.removeFromTop (40);
    g.drawText ("CATEGORY", inner.removeFromTop (16), juce::Justification::centredLeft);
    inner.removeFromTop (40);
    g.drawText ("NOTES", inner.removeFromTop (16), juce::Justification::centredLeft);
}

void PresetDialog::resized()
{
    auto inner = panelBounds().reduced (24, 18);
    inner.removeFromTop (34 + 8 + 16);
    name.setBounds (inner.removeFromTop (34));
    inner.removeFromTop (6 + 16);
    category.setBounds (inner.removeFromTop (34).withWidth (260));
    inner.removeFromTop (6 + 16);

    auto buttons = inner.removeFromBottom (34);
    action.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (8);
    cancel.setBounds (buttons.removeFromRight (100));
    error.setBounds (buttons);
    inner.removeFromBottom (10);
    notes.setBounds (inner);
}

void PresetDialog::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()))
        dismiss();
}

bool PresetDialog::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { dismiss(); return true; }
    return false;
}

} // namespace abyssal::ui
