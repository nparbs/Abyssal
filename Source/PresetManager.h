#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace abyssal
{

/** Lists factory + user presets and stores favourites / UI preferences in a settings file shared
    by every instance of the plugin. */
class PresetManager
{
public:
    struct Entry
    {
        juce::String name, category;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;

        juce::String key() const { return factory ? "factory:" + name : "user:" + file.getFullPathName(); }
    };

    static constexpr const char* userCategory = "User";

    PresetManager();

    static juce::File getUserPresetDirectory();

    void refresh();
    const juce::Array<Entry>& getEntries() const noexcept { return entries; }
    int indexOfKey (const juce::String& key) const;

    /** "All", "Favourites", then the factory categories, then "User" if any user presets exist. */
    juce::StringArray getFilterCategories() const;

    /** Indices into getEntries() matching a category filter and search text. */
    juce::Array<int> filter (const juce::String& category, const juce::String& search) const;

    bool isFavourite (const juce::String& key) const { return favourites.contains (key); }
    void setFavourite (const juce::String& key, bool shouldBeFavourite);

    double getDefaultUiScale() const;
    void setDefaultUiScale (double scale);

private:
    void loadSettings();

    juce::Array<Entry> entries;
    juce::StringArray favourites;
    std::unique_ptr<juce::PropertiesFile> settings;
};

} // namespace abyssal
