#pragma once

#include "Widgets.h"

namespace abyssal::ui
{

/** Overlay for naming a preset: name, category (pick one or type a new one) and free-form notes.
    Used both for saving and for editing a user preset from the browser. */
class PresetDialog : public juce::Component
{
public:
    PresetDialog();

    struct Details { juce::String name, category, notes; };

    /** Shows the dialog. `confirm` gets the details when the user presses the action button; it
        returns an error to show, or "" to close the dialog. */
    void show (const juce::String& title, const juce::String& actionText, const Details& initial,
               const juce::StringArray& categories, std::function<juce::String (const Details&)> confirm);
    void dismiss();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void submit();
    juce::Rectangle<int> panelBounds() const;

    juce::String title;
    juce::TextEditor name, notes;
    juce::ComboBox category;
    juce::TextButton action { "SAVE" }, cancel { "CANCEL" };
    juce::Label error;
    std::function<juce::String (const Details&)> onConfirm;
};

} // namespace abyssal::ui
