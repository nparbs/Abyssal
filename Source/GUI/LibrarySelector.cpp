#include "LibrarySelector.h"
#include <map>

namespace abyssal::ui
{

namespace
{
    enum MenuIds { loadFileId = 100, loadFolderId = 101, openFolderId = 102, removeId = 103, onlineId = 104, libraryBase = 1000 };
}

LibrarySelector::LibrarySelector (Config c) : config (std::move (c))
{
    box.onPopup = [this] { showMenu(); };
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    prev.setTooltip ("Previous " + config.kind + " in this folder");
    next.setTooltip ("Next " + config.kind + " in this folder");
    for (auto* comp : std::initializer_list<juce::Component*> { &prev, &box, &next })
        addAndMakeVisible (comp);
    refresh();
    startTimerHz (4);
}

void LibrarySelector::resized()
{
    auto r = getLocalBounds();
    prev.setBounds (r.removeFromLeft (28));
    next.setBounds (r.removeFromRight (28));
    box.setBounds (r.reduced (4, 0));
}

void LibrarySelector::refresh()
{
    const auto text = config.displayText ? config.displayText() : juce::String();
    if (box.getText() != text)
        box.setText (text, juce::dontSendNotification);
}

void LibrarySelector::report (const juce::String& error)
{
    if (error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Abyssal", error);
    refresh();
    if (onChange) onChange();
}

LibrarySelector::Config::Done LibrarySelector::reporter()
{
    return [safe = juce::Component::SafePointer<LibrarySelector> (this)] (const juce::String& err)
    {
        if (safe != nullptr) safe->report (err);
    };
}

void LibrarySelector::load (const juce::File& file)
{
    config.loadFile (file, reporter());
    refresh(); // shows "Loading..." straight away when the load runs in the background
}

void LibrarySelector::step (int delta)
{
    const auto file = config.library->step (config.currentFile(), delta);
    if (file.existsAsFile())
        load (file);
}

void LibrarySelector::showMenu()
{
    const auto lib = config.library->getDirectory();
    const auto current = config.currentFile();

    juce::PopupMenu menu;
    if (config.addTopItems)
        config.addTopItems (menu);

    menu.addSectionHeader ("YOUR " + config.kind.toUpperCase() + " LIBRARY");
    menuFiles = config.library->list();
    if (menuFiles.isEmpty())
        menu.addItem (-1, "(empty - load a file or folder to add it)", false, false);

    // Root files go straight into the menu; each sub-folder (pack) becomes a submenu.
    std::map<juce::String, juce::PopupMenu> folders;
    for (int i = 0; i < menuFiles.size(); ++i)
    {
        const auto& f = menuFiles.getReference (i);
        const auto folder = config.library->folderOf (f);
        if (folder.isEmpty())
            menu.addItem (libraryBase + i, f.getFileNameWithoutExtension(), true, f == current);
        else
            folders[folder].addItem (libraryBase + i, f.getFileNameWithoutExtension(), true, f == current);
    }
    for (auto& [name, sub] : folders)
    {
        const bool holdsCurrent = current.existsAsFile() && config.library->folderOf (current) == name;
        menu.addSubMenu (name, sub, true, nullptr, holdsCurrent);
    }

    menu.addSeparator();
    menu.addItem (loadFileId, "Load " + config.kind + " file...");
    menu.addItem (loadFolderId, "Load folder of " + config.kind + "s...");
    if (config.openOnline)
        menu.addItem (onlineId, "Get " + config.kind + "s from TONE3000...");
    menu.addItem (openFolderId, "Open " + config.kind + " library folder");
    if (current.existsAsFile() && config.library->contains (current))
        menu.addItem (removeId, "Remove \"" + current.getFileNameWithoutExtension() + "\" from library");

    if (config.addBottomItems)
        config.addBottomItems (menu);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&box).withMinimumWidth (box.getWidth()),
                        [safe = juce::Component::SafePointer<LibrarySelector> (this)] (int r)
                        {
                            if (safe != nullptr) safe->handleResult (r);
                        });
}

void LibrarySelector::handleResult (int r)
{
    if (r <= 0) return;

    if (r >= libraryBase && r - libraryBase < menuFiles.size())
        return load (menuFiles[r - libraryBase]);

    switch (r)
    {
        case loadFileId:   browseForFile(); return;
        case loadFolderId: browseForFolder(); return;
        case openFolderId: config.library->getDirectory().startAsProcess(); return;
        case onlineId:     if (config.openOnline) config.openOnline(); return;
        case removeId:
        {
            const auto file = config.currentFile();
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Remove " + config.kind,
                "Move \"" + file.getFileName() + "\" from your " + config.kind + " library to the Recycle Bin?",
                "Remove", "Cancel", this,
                juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<LibrarySelector> (this), file] (int ok)
                {
                    if (safe == nullptr) return;
                    if (ok != 0) safe->config.removeFile (file);
                    safe->report ({});
                }));
            return;
        }
        default:
            if (config.handleExtraItem) config.handleExtraItem (r);
            report ({});
            return;
    }
}

void LibrarySelector::browseForFile()
{
    auto start = config.browseStart ? config.browseStart() : juce::File();
    if (! start.isDirectory()) start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Load " + config.kind + " (it will be added to your library)",
                                                   start, config.library->getWildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<LibrarySelector> (this)] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        safe->load (fc.getResult());
    });
}

void LibrarySelector::browseForFolder()
{
    auto start = config.browseStart ? config.browseStart() : juce::File();
    if (! start.isDirectory()) start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Choose a folder of " + config.kind + "s to add to your library", start);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe = juce::Component::SafePointer<LibrarySelector> (this)] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        juce::MouseCursor::showWaitCursor(); // copying a pack can take a moment
        safe->config.importFolder (fc.getResult(), safe->reporter());
        juce::MouseCursor::hideWaitCursor();
        safe->refresh();
    });
}

} // namespace abyssal::ui
