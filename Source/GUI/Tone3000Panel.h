#pragma once

#include "Widgets.h"
#include "PresetBrowser.h" // drawStar
#include <map>
#include "../Online/Tone3000.h"

class AbyssalProcessor;

namespace abyssal::ui
{

/** Full-window TONE3000 browser: connect, pick a tone on tone3000.com, or browse your Favorites /
    Downloaded / Created and Trending / Latest; then download models into the NAM or IR library
    and load them into the amp or a cab slot. */
class Tone3000Panel : public juce::Component, private juce::ChangeListener
{
public:
    explicit Tone3000Panel (AbyssalProcessor&);
    ~Tone3000Panel() override;

    enum class Target { amp, cabA, cabB };
    void open (Target target);
    void close();

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct ToneListModel;
    struct ModelListModel;

    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshConnection(); }
    void refreshConnection();
    void setFormat (bool ir);
    void showList (int tab, bool nextPage = false);
    void toggleFavorite (const t3k::Tone& tone);
    bool isFavorite (int toneId) const { return favoriteIds.contains (toneId); }
    void refreshFavoriteButton();
    void loadKnownFavorites (int page = 1);
    /** The creator's avatar, or an invalid image while it downloads (or if there is none). */
    juce::Image avatarFor (const t3k::EmbeddedUser& user);
    void pickOnWebsite();
    void showTone (int toneId);
    void setTone (const t3k::Tone& tone);
    void download (const t3k::Model& model, bool loadAfter);
    void downloadAllStandard();
    void loadFile (const juce::File& file);
    juce::File fileFor (const t3k::Model& model) const;
    juce::File folderFor (const t3k::Tone& tone) const;
    void writeCredits (const t3k::Tone& tone) const;
    void setStatus (const juce::String& text, bool isError = false);
    juce::Rectangle<int> panelBounds() const;

    AbyssalProcessor& proc;
    juce::SharedResourcePointer<t3k::Client> client;
    Target target = Target::amp;
    bool irMode = false;
    int currentTab = -1;

    juce::Array<t3k::Tone> tones;
    std::optional<t3k::Tone> tone;
    juce::Array<t3k::Model> models;
    juce::Array<int> downloading;
    juce::String status;
    bool statusIsError = false;
    int requestSerial = 0;
    bool favoritesLoaded = false;
    int listPage = 1, listTotalPages = 1, hiddenCount = 0;
    juce::Array<int> favoriteIds;   // known favourites (from the Favorites list and toggles here)
    std::map<juce::String, juce::Image> avatars;
    juce::StringArray avatarRequests;

    juce::TextEditor keyEditor;
    juce::TextButton saveKey { "SAVE KEY" }, connectButton { "CONNECT" }, closeButton { "CLOSE" };
    juce::HyperlinkButton getKeyLink { "Get a free key", juce::URL ("https://www.tone3000.com/settings") };
    juce::TextButton namButton { "NAM CAPTURES" }, irButton { "CABINET IRs" };
    juce::TextButton pickButton { "PICK A TONE ON TONE3000.COM" };
    juce::OwnedArray<juce::TextButton> tabs;
    std::unique_ptr<ToneListModel> toneModel;
    std::unique_ptr<ModelListModel> modelModel;
    juce::ListBox toneList, modelList;
    juce::TextButton downloadAll { "DOWNLOAD ALL STANDARD" }, loadMore { "LOAD MORE" }, favoriteButton { "FAVORITE" };
    juce::HyperlinkButton toneLink { "Open on tone3000.com", juce::URL() };

    juce::Rectangle<int> detailArea, headerArea;
};

} // namespace abyssal::ui
