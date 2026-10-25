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
        juce::String notes;

        juce::String key() const { return factory ? "factory:" + name : "user:" + file.getFullPathName(); }
    };

    /** The browser filter that lists every user preset, and the category of user presets saved
        without one. */
    static constexpr const char* userCategory = "User";

    PresetManager();

    static juce::File getUserPresetDirectory();

    void refresh();
    const juce::Array<Entry>& getEntries() const noexcept { return entries; }
    int indexOfKey (const juce::String& key) const;

    /** "All", "Favourites", the factory categories, categories only user presets use, then "User"
        if any user presets exist. */
    juce::StringArray getFilterCategories() const;

    /** Every category a preset could be saved in: the factory ones plus any used by user presets. */
    juce::StringArray getSaveCategories() const;

    /** Moves a user preset's favourite (stored by file path) to its new file after a rename. */
    void renameFavourite (const juce::String& oldKey, const juce::String& newKey);

    /** Indices into getEntries() matching a category filter and search text. */
    juce::Array<int> filter (const juce::String& category, const juce::String& search) const;

    bool isFavourite (const juce::String& key) const { return favourites.contains (key); }
    void setFavourite (const juce::String& key, bool shouldBeFavourite);

    juce::String getTone3000Key() const { return settings->getValue ("t3kPublishableKey"); }
    void setTone3000Key (const juce::String& key) { settings->setValue ("t3kPublishableKey", key.trim()); settings->saveIfNeeded(); }

    double getDefaultUiScale() const;
    void setDefaultUiScale (double scale);

    /** Machine-wide preferences shared by every instance (tuner, calibration defaults, ...). */
    double getSetting (const juce::String& key, double fallback) const { return settings->getDoubleValue (key, fallback); }
    void setSetting (const juce::String& key, double value) { settings->setValue (key, value); }

private:
    void loadSettings();

    juce::Array<Entry> entries;
    juce::StringArray favourites;
    std::unique_ptr<juce::PropertiesFile> settings;
};

} // namespace abyssal
