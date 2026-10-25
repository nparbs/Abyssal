#include "PresetManager.h"
#include "Presets.h"

namespace abyssal
{

PresetManager::PresetManager()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "Abyssal";
    opts.filenameSuffix = ".settings";
    opts.folderName = "Abyssal";
    opts.osxLibrarySubFolder = "Application Support";
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    opts.millisecondsBeforeSaving = 1500;
    settings = std::make_unique<juce::PropertiesFile> (opts);
    refresh();
}

juce::File PresetManager::getUserPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Abyssal").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

void PresetManager::loadSettings()
{
    settings->reload(); // another instance may have changed it
    favourites = juce::StringArray::fromLines (settings->getValue ("favourites"));
    favourites.removeEmptyStrings();
}

void PresetManager::refresh()
{
    loadSettings();
    entries.clearQuick();

    const auto& factory = getFactoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        entries.add ({ factory[(size_t) i].name, factory[(size_t) i].category, true, i, {} });

    auto files = getUserPresetDirectory().findChildFiles (juce::File::findFiles, true, "*.abyssal");
    files.sort();
    for (auto& f : files)
    {
        // Only the root element's attributes are needed; presets are small, so a full parse is fine.
        juce::String category = userCategory, notes;
        if (auto xml = juce::XmlDocument::parse (f))
        {
            category = xml->getStringAttribute ("presetCategory", userCategory).trim();
            notes = xml->getStringAttribute ("notes");
            if (category.isEmpty()) category = userCategory;
        }
        entries.add ({ f.getFileNameWithoutExtension(), category, false, -1, f, notes });
    }
}

int PresetManager::indexOfKey (const juce::String& key) const
{
    for (int i = 0; i < entries.size(); ++i)
        if (entries.getReference (i).key() == key)
            return i;
    return -1;
}

juce::StringArray PresetManager::getFilterCategories() const
{
    juce::StringArray cats { "All", "Favourites" };
    for (auto& c : getFactoryCategories())
        for (auto& e : entries)
            if (e.category == c) { cats.add (c); break; }

    juce::StringArray custom;
    for (auto& e : entries)
        if (! e.factory && e.category != userCategory && ! cats.contains (e.category))
            custom.addIfNotAlreadyThere (e.category);
    custom.sortNatural();
    cats.addArray (custom);

    for (auto& e : entries)
        if (! e.factory) { cats.add (userCategory); break; }
    return cats;
}

juce::StringArray PresetManager::getSaveCategories() const
{
    auto cats = getFactoryCategories();
    cats.removeString ("Utility");
    juce::StringArray custom;
    for (auto& e : entries)
        if (! e.factory && ! cats.contains (e.category) && e.category != userCategory)
            custom.addIfNotAlreadyThere (e.category);
    custom.sortNatural();
    cats.addArray (custom);
    cats.add (userCategory);
    return cats;
}

void PresetManager::renameFavourite (const juce::String& oldKey, const juce::String& newKey)
{
    loadSettings();
    const int i = favourites.indexOf (oldKey);
    if (i < 0) return;
    if (newKey.isEmpty() || favourites.contains (newKey)) favourites.remove (i);
    else                                                  favourites.set (i, newKey);
    settings->setValue ("favourites", favourites.joinIntoString ("\n"));
    settings->saveIfNeeded();
}

juce::Array<int> PresetManager::filter (const juce::String& category, const juce::String& search) const
{
    juce::Array<int> out;
    const auto words = juce::StringArray::fromTokens (search.toLowerCase(), " ", "");

    for (int i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries.getReference (i);
        if (category == "Favourites" && ! isFavourite (e.key())) continue;
        if (category == userCategory)
        {
            if (e.factory) continue;
        }
        else if (category != "All" && category != "Favourites" && e.category != category) continue;

        bool match = true;
        const auto hay = (e.name + " " + e.category + " " + e.notes).toLowerCase();
        for (auto& w : words)
            if (w.isNotEmpty() && ! hay.contains (w)) { match = false; break; }
        if (match) out.add (i);
    }
    return out;
}

void PresetManager::setFavourite (const juce::String& key, bool shouldBeFavourite)
{
    loadSettings();
    if (shouldBeFavourite) favourites.addIfNotAlreadyThere (key);
    else                   favourites.removeString (key);
    settings->setValue ("favourites", favourites.joinIntoString ("\n"));
    settings->saveIfNeeded();
}

double PresetManager::getDefaultUiScale() const
{
    return juce::jlimit (0.7, 2.0, settings->getDoubleValue ("uiScale", 1.0));
}

void PresetManager::setDefaultUiScale (double scale)
{
    settings->setValue ("uiScale", scale); // saved lazily by PropertiesFile, not on every resize step
}

} // namespace abyssal
