#pragma once

#include "Widgets.h"
#include "PresetDialog.h"
#include "../PresetManager.h"

class AbyssalProcessor;

namespace abyssal::ui
{

/** Draws a five-point star; filled when favourited. */
void drawStar (juce::Graphics&, juce::Rectangle<float> area, bool filled, juce::Colour colour);

class StarButton : public juce::Button
{
public:
    StarButton() : juce::Button ("favourite") { setClickingTogglesState (false); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    bool filled = false;
};

/** Full-window preset browser: category list, search box, and a preset list with favourites.
    Clicking or arrowing through the list loads presets immediately; double-click / Enter closes.
    Right-click a user preset to rename it, change its category / notes, or delete it. */
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::ChangeListener
{
public:
    explicit PresetBrowser (AbyssalProcessor&);
    ~PresetBrowser() override;

    void open();
    void close();
    std::function<void()> onClose;

    /** The filtered list (category + search), used by the header's prev/next buttons. */
    juce::Array<int> getVisibleEntries() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int getNumRows() override { return visible.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void returnKeyPressed (int) override { close(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { list.repaint(); }

    void rebuildCategories();
    void applyFilter();
    void loadRow (int row);
    void showRowMenu (int row);
    void refreshAll();
    juce::Rectangle<int> panelBounds() const;

    AbyssalProcessor& proc;
    juce::String category { "All" };
    juce::Array<int> visible;
    bool suppressLoad = false;

    juce::TextEditor search;
    juce::TextButton closeButton { "CLOSE" };
    juce::OwnedArray<juce::TextButton> categoryButtons;
    juce::ListBox list { "presets", this };
    PresetDialog dialog;
};

} // namespace abyssal::ui
