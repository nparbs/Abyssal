#pragma once

#include "LookAndFeel.h"
#include "../FileLibrary.h"

namespace abyssal::ui
{

/** [<] [ dropdown ] [>] for a FileLibrary. The dropdown lists the library (sub-folders become
    submenus) plus actions: load a file, load a whole folder, open the folder, remove the current
    file. The arrows step through the current file's folder, handy for auditioning a pack. */
class LibrarySelector : public juce::Component, private juce::Timer
{
public:
    struct Config
    {
        const FileLibrary* library = nullptr;
        juce::String kind;                                    // "IR", "capture" - used in menu text
        std::function<juce::File()> currentFile;              // the active library file, or File() if none
        std::function<juce::String()> displayText;
        // Loading can finish later (e.g. on a background thread): call `done` with an error or "".
        using Done = std::function<void (const juce::String& error)>;
        std::function<void (const juce::File&, Done)> loadFile;
        std::function<void (const juce::File&, Done)> importFolder;
        std::function<bool (const juce::File&)> removeFile;
        std::function<juce::File()> browseStart;
        std::function<void()> openOnline;                     // "Get from TONE3000..."

        // Optional extra sections: ids 1..99 above the library, 200..299 below it.
        std::function<void (juce::PopupMenu&)> addTopItems, addBottomItems;
        std::function<void (int)> handleExtraItem;
    };

    explicit LibrarySelector (Config);

    void resized() override;
    void refresh();
    void browseForFile();
    void browseForFolder();

    std::function<void()> onChange;

private:
    struct Box : juce::ComboBox
    {
        std::function<void()> onPopup;
        void showPopup() override { if (onPopup) onPopup(); }
    };

    void timerCallback() override { refresh(); }
    void showMenu();
    void handleResult (int result);
    void step (int delta);
    void report (const juce::String& error);
    void load (const juce::File& file);
    Config::Done reporter();

    Config config;
    Box box;
    juce::TextButton prev { "<" }, next { ">" };
    juce::Array<juce::File> menuFiles;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace abyssal::ui
