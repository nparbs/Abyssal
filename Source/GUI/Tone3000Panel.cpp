#include "Tone3000Panel.h"
#include "../PluginProcessor.h"

namespace abyssal::ui
{

namespace
{
    const char* tabNames[] = { "FAVORITES", "DOWNLOADED", "CREATED", "TRENDING", "LATEST" };
    const t3k::Client::List tabLists[] = { t3k::Client::List::favorited, t3k::Client::List::downloaded,
                                           t3k::Client::List::created, t3k::Client::List::trending,
                                           t3k::Client::List::latest };

    void drawBadge (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour c)
    {
        g.setColour (c.withAlpha (0.18f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (c);
        g.setFont (font (10.0f, true));
        g.drawText (text.toUpperCase(), r, juce::Justification::centred);
    }
}

//============================================================================== list models
struct Tone3000Panel::ToneListModel : juce::ListBoxModel
{
    explicit ToneListModel (Tone3000Panel& p) : panel (p) {}
    int getNumRows() override { return panel.tones.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, panel.tones.size())) return;
        const auto& t = panel.tones.getReference (row);
        auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2).toFloat();
        g.setColour (selected ? colours::accent.withAlpha (0.25f) : (row % 2 ? colours::panelHi : colours::panel));
        g.fillRoundedRectangle (r, 5.0f);

        // Creator avatar (their initial while it loads, or if they have none)
        const juce::Rectangle<float> av (10.0f, (float) h * 0.5f - 15.0f, 30.0f, 30.0f);
        if (auto img = panel.avatarFor (t.user); img.isValid())
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addEllipse (av);
            g.reduceClipRegion (clip);
            g.drawImage (img, av, juce::RectanglePlacement::fillDestination);
        }
        else
        {
            g.setColour (colours::outline);
            g.fillEllipse (av);
            g.setColour (colours::textDim);
            g.setFont (font (13.0f, true));
            g.drawText (t.user.username.substring (0, 1).toUpperCase(), av, juce::Justification::centred);
        }

        const int textX = 50;
        g.setColour (colours::text);
        g.setFont (font (14.0f, true));
        g.drawText (t.title, textX, 4, w - textX - 40, 20, juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.setFont (font (12.0f));
        // display_name is only set for verified creators
        const auto creator = t.user.verified && t.user.displayName.isNotEmpty() ? t.user.displayName + " (@" + t.user.username + ")"
                                                                                : "@" + t.user.username;
        g.drawText ("by " + creator, textX, 24, w - textX - 110, 16, juce::Justification::centredLeft);
        g.drawText (juce::String (t.modelsCount) + (t.isIR() ? " IRs" : " models"), w - 110, 24, 98, 16, juce::Justification::centredRight);

        const bool fav = panel.isFavorite (t.id);
        drawStar (g, starArea (w), fav, fav ? colours::amber : colours::textDim);
    }

    static juce::Rectangle<float> starArea (int w) { return { (float) w - 30.0f, 5.0f, 16.0f, 16.0f }; }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (! juce::isPositiveAndBelow (row, panel.tones.size()))
            return;
        // The event comes from the row component, which spans the list's width.
        if (starArea (e.eventComponent->getWidth()).expanded (6.0f).contains (e.position))
            panel.toggleFavorite (panel.tones[row]);
        else
            panel.setTone (panel.tones[row]);
    }

    Tone3000Panel& panel;
};

struct Tone3000Panel::ModelListModel : juce::ListBoxModel
{
    explicit ModelListModel (Tone3000Panel& p) : panel (p) {}
    int getNumRows() override { return panel.models.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, panel.models.size())) return;
        const auto& m = panel.models.getReference (row);
        const bool have = panel.fileFor (m).existsAsFile();
        const bool busy = panel.downloading.contains (m.id);

        auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2).toFloat();
        g.setColour (selected ? colours::accent.withAlpha (0.2f) : (row % 2 ? colours::panelHi : colours::panel));
        g.fillRoundedRectangle (r, 5.0f);

        g.setColour (colours::text);
        g.setFont (font (13.5f));
        g.drawFittedText (m.name, 12, 0, w - 250, h, juce::Justification::centredLeft, 1, 0.8f);

        float x = (float) w - 232;
        if (m.size.isNotEmpty())
        {
            drawBadge (g, { x, (float) h * 0.5f - 9, 62, 18 }, m.size, m.size == "standard" ? colours::accentAlt : colours::textDim);
            x += 66;
        }
        if (m.architecture.isNotEmpty())
            drawBadge (g, { x, (float) h * 0.5f - 9, 34, 18 }, "A" + m.architecture, colours::amber);

        auto action = juce::Rectangle<float> ((float) w - 92, (float) h * 0.5f - 12, 82, 24);
        g.setColour (busy ? colours::outline : have ? colours::accentAlt.withAlpha (0.3f) : colours::accent.withAlpha (0.8f));
        g.fillRoundedRectangle (action, 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (font (11.0f, true));
        g.drawText (busy ? "..." : have ? "LOAD" : "DOWNLOAD", action, juce::Justification::centred);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (! juce::isPositiveAndBelow (row, panel.models.size())) return;
        const auto m = panel.models[row];
        const auto file = panel.fileFor (m);
        if (file.existsAsFile()) panel.loadFile (file);
        else if (! panel.downloading.contains (m.id)) panel.download (m, true);
    }

    Tone3000Panel& panel;
};

//============================================================================== panel
Tone3000Panel::Tone3000Panel (AbyssalProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    toneModel = std::make_unique<ToneListModel> (*this);
    modelModel = std::make_unique<ModelListModel> (*this);
    toneList.setModel (toneModel.get());
    modelList.setModel (modelModel.get());

    for (auto* lb : { &toneList, &modelList })
    {
        lb->setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        lb->setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    }
    toneList.setRowHeight (46);
    modelList.setRowHeight (34);

    keyEditor.setTextToShowWhenEmpty ("Paste your TONE3000 publishable key (t3k_pub_...)", colours::textDim);
    keyEditor.setFont (font (13.0f));
    keyEditor.setColour (juce::TextEditor::backgroundColourId, colours::bg);
    keyEditor.setColour (juce::TextEditor::outlineColourId, colours::outline);
    keyEditor.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    keyEditor.setColour (juce::TextEditor::textColourId, colours::text);
    keyEditor.onReturnKey = [this] { saveKey.triggerClick(); };

    getKeyLink.setFont (font (12.0f), false);
    getKeyLink.setColour (juce::HyperlinkButton::textColourId, colours::accentAlt);
    toneLink.setFont (font (12.0f), false);
    toneLink.setColour (juce::HyperlinkButton::textColourId, colours::accentAlt);

    saveKey.onClick = [this]
    {
        const auto key = keyEditor.getText().trim();
        if (! key.startsWith ("t3k_pub_"))
            return setStatus ("That doesn't look like a publishable key - it should start with t3k_pub_ (never paste the secret key).", true);
        proc.presets.setTone3000Key (key);
        client->setPublishableKey (key);
        setStatus ("Key saved. Click CONNECT or pick a tone to sign in.");
        refreshConnection();
    };

    connectButton.onClick = [this]
    {
        if (client->isConnected())
        {
            client->disconnect();
            tones.clear(); toneList.updateContent();
            setStatus ("Disconnected from TONE3000.");
            return;
        }
        setStatus ("Finish signing in in your web browser...");
        client->connect ([safe = juce::Component::SafePointer<Tone3000Panel> (this)] (t3k::Result<bool> r)
        {
            if (safe == nullptr) return;
            safe->setStatus (r.ok ? "Connected to TONE3000." : r.error.isEmpty() ? "Sign-in canceled." : r.error, ! r.ok && r.error.isNotEmpty());
            safe->refreshConnection();
            if (r.ok) safe->showList (safe->currentTab < 0 ? 0 : safe->currentTab);
        });
    };

    closeButton.onClick = [this] { close(); };
    namButton.onClick = [this] { setFormat (false); };
    irButton.onClick = [this] { setFormat (true); };
    namButton.setColour (juce::TextButton::buttonOnColourId, colours::amber);
    irButton.setColour (juce::TextButton::buttonOnColourId, colours::accentAlt);
    pickButton.setColour (juce::TextButton::buttonColourId, colours::accent.withMultipliedBrightness (0.6f));
    pickButton.onClick = [this] { pickOnWebsite(); };
    downloadAll.onClick = [this] { downloadAllStandard(); };
    loadMore.onClick = [this] { showList (currentTab, true); };
    favoriteButton.setClickingTogglesState (false);
    favoriteButton.setColour (juce::TextButton::buttonOnColourId, colours::amber.withMultipliedBrightness (0.6f));
    favoriteButton.onClick = [this] { if (tone) toggleFavorite (*tone); };

    for (int i = 0; i < 5; ++i)
    {
        auto* b = tabs.add (new juce::TextButton (tabNames[i]));
        b->onClick = [this, i] { showList (i); };
        addAndMakeVisible (b);
    }

    for (auto* c : std::initializer_list<juce::Component*> { &keyEditor, &saveKey, &getKeyLink, &connectButton, &closeButton,
                                                             &namButton, &irButton, &pickButton, &toneList, &modelList,
                                                             &downloadAll, &toneLink })
        addAndMakeVisible (c);
    addChildComponent (loadMore);
    addChildComponent (favoriteButton);

    client->addChangeListener (this);
    client->setPublishableKey (proc.presets.getTone3000Key());
    keyEditor.setText (client->getPublishableKey(), false);
    refreshConnection();
    setTone ({});
}

Tone3000Panel::~Tone3000Panel()
{
    client->removeChangeListener (this);
    toneList.setModel (nullptr);
    modelList.setModel (nullptr);
}

void Tone3000Panel::open (Target t)
{
    target = t;
    client->setPublishableKey (proc.presets.getTone3000Key());
    setFormat (t != Target::amp);
    setVisible (true);
    refreshConnection();
    toFront (true);
    if (client->isConnected() && tones.isEmpty())
        showList (currentTab < 0 ? 0 : currentTab);
    grabKeyboardFocus();
}

void Tone3000Panel::close()
{
    setVisible (false);
}

void Tone3000Panel::refreshConnection()
{
    const bool hasKey = client->hasKey();
    const bool connected = client->isConnected();
    if (! connected)
    {
        favoriteIds.clear();
        favoritesLoaded = false;
    }
    else if (isVisible() && ! favoritesLoaded)
    {
        favoritesLoaded = true; // only once the panel is in use - no network traffic just for opening a project
        loadKnownFavorites();
    }
    keyEditor.setVisible (! hasKey);
    saveKey.setVisible (! hasKey);
    getKeyLink.setVisible (! hasKey);
    connectButton.setVisible (hasKey);
    connectButton.setButtonText (connected ? "DISCONNECT" : "CONNECT");
    pickButton.setEnabled (hasKey);
    for (auto* t : tabs) t->setEnabled (connected);
    refreshFavoriteButton();
    repaint();
}

void Tone3000Panel::setFormat (bool ir)
{
    irMode = ir;
    namButton.setToggleState (! ir, juce::dontSendNotification);
    irButton.setToggleState (ir, juce::dontSendNotification);
    if (currentTab >= 0 && client->isConnected())
        showList (currentTab);
    repaint();
}

void Tone3000Panel::setStatus (const juce::String& text, bool isError)
{
    status = text;
    statusIsError = isError;
    repaint();
}

void Tone3000Panel::showList (int tab, bool nextPage)
{
    if (nextPage && (tab != currentTab || listPage >= listTotalPages))
        return;
    currentTab = tab;
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setToggleState (i == tab, juce::dontSendNotification);
    if (! client->isConnected())
        return setStatus ("Connect to TONE3000 to see your lists.");

    const int page = nextPage ? listPage + 1 : 1;
    setStatus ("Loading " + juce::String (tabNames[tab]).toLowerCase() + (page > 1 ? " (page " + juce::String (page) + ")..." : "..."));
    loadMore.setEnabled (false);
    const int serial = ++requestSerial;
    client->listTones (tabLists[tab], page, [safe = juce::Component::SafePointer<Tone3000Panel> (this), serial, page, tab] (t3k::Result<t3k::TonePage> r)
    {
        if (safe == nullptr || serial != safe->requestSerial) return;
        if (page == 1)
        {
            safe->tones.clear();
            safe->hiddenCount = 0;
        }
        if (! r.ok)
        {
            safe->setStatus (r.error, true);
        }
        else
        {
            safe->listPage = page;
            safe->listTotalPages = r.value.totalPages;
            // These lists can't be filtered by format server-side, so filter here.
            for (auto& t : r.value.tones)
            {
                if (tabLists[tab] == t3k::Client::List::favorited)
                    safe->favoriteIds.addIfNotAlreadyThere (t.id);
                if (t.isIR() == safe->irMode && (t.isIR() || t.format == "nam"))
                    safe->tones.add (t);
                else
                    ++safe->hiddenCount;
            }
            const int hidden = safe->hiddenCount;
            safe->setStatus (juce::String (safe->tones.size()) + (safe->irMode ? " IR tones" : " NAM tones")
                             + (hidden > 0 ? "  (" + juce::String (hidden) + " of another format hidden)" : juce::String())
                             + (safe->listPage < safe->listTotalPages ? "  -  more available" : juce::String()));
        }
        safe->loadMore.setVisible (safe->listPage < safe->listTotalPages);
        safe->loadMore.setEnabled (true);
        safe->toneList.updateContent();
        safe->toneList.repaint();
        safe->refreshFavoriteButton();
    });
}

void Tone3000Panel::loadKnownFavorites (int page)
{
    // Quietly learns which tones are favourited so the stars are right on every tab.
    client->listTones (t3k::Client::List::favorited, page, [safe = juce::Component::SafePointer<Tone3000Panel> (this), page] (t3k::Result<t3k::TonePage> r)
    {
        if (safe == nullptr || ! r.ok) return;
        for (auto& t : r.value.tones)
            safe->favoriteIds.addIfNotAlreadyThere (t.id);
        safe->toneList.repaint();
        safe->refreshFavoriteButton();
        if (page < r.value.totalPages && page < 10)
            safe->loadKnownFavorites (page + 1);
    });
}

void Tone3000Panel::toggleFavorite (const t3k::Tone& t)
{
    const bool makeFavorite = ! isFavorite (t.id);
    // Optimistic: flip the star now, put it back if the request fails.
    if (makeFavorite) favoriteIds.addIfNotAlreadyThere (t.id);
    else              favoriteIds.removeAllInstancesOf (t.id);
    toneList.repaint();
    refreshFavoriteButton();

    client->setFavorite (t.id, makeFavorite, [safe = juce::Component::SafePointer<Tone3000Panel> (this), t, makeFavorite] (t3k::Result<bool> r)
    {
        if (safe == nullptr) return;
        if (! r.ok)
        {
            if (makeFavorite) safe->favoriteIds.removeAllInstancesOf (t.id);
            else              safe->favoriteIds.addIfNotAlreadyThere (t.id);
            safe->setStatus (r.error, true);
        }
        else
        {
            safe->setStatus ((makeFavorite ? "Added " : "Removed ") + t.title + (makeFavorite ? " to" : " from") + " your TONE3000 favorites.");
        }
        safe->toneList.repaint();
        safe->refreshFavoriteButton();
    });
}

void Tone3000Panel::refreshFavoriteButton()
{
    favoriteButton.setVisible (tone.has_value() && client->isConnected());
    if (! tone) return;
    const bool fav = isFavorite (tone->id);
    favoriteButton.setButtonText (fav ? "FAVORITED" : "FAVORITE");
    favoriteButton.setToggleState (fav, juce::dontSendNotification);
}

juce::Image Tone3000Panel::avatarFor (const t3k::EmbeddedUser& user)
{
    if (user.avatarUrl.isEmpty())
        return {};
    if (auto it = avatars.find (user.avatarUrl); it != avatars.end())
        return it->second;
    if (! avatarRequests.contains (user.avatarUrl))
    {
        avatarRequests.add (user.avatarUrl);
        client->fetchImage (user.avatarUrl, [safe = juce::Component::SafePointer<Tone3000Panel> (this), url = user.avatarUrl] (juce::Image img)
        {
            if (safe == nullptr) return;
            // Keep a small copy: rows only draw it at 30 px.
            safe->avatars[url] = img.isValid() ? img.rescaled (64, 64, juce::Graphics::highResamplingQuality) : juce::Image();
            safe->toneList.repaint();
        });
    }
    return {};
}

void Tone3000Panel::pickOnWebsite()
{
    setStatus ("Pick a tone in your web browser - it will appear here.");
    client->selectTone (irMode ? "ir" : "nam", [safe = juce::Component::SafePointer<Tone3000Panel> (this)] (t3k::Result<int> r)
    {
        if (safe == nullptr) return;
        safe->refreshConnection();
        if (! r.ok)
            return safe->setStatus (r.error.isEmpty() ? "Canceled." : r.error, r.error.isNotEmpty());
        safe->showTone (r.value);
        if (auto* top = safe->getTopLevelComponent()) top->toFront (true);
    });
}

void Tone3000Panel::showTone (int toneId)
{
    setStatus ("Loading tone...");
    const int serial = ++requestSerial;
    client->getTone (toneId, [safe = juce::Component::SafePointer<Tone3000Panel> (this), serial] (t3k::Result<t3k::Tone> r)
    {
        if (safe == nullptr || serial != safe->requestSerial) return;
        if (! r.ok) return safe->setStatus (r.error, true);
        safe->setTone (r.value);
    });
}

void Tone3000Panel::setTone (const t3k::Tone& t)
{
    models.clear();
    modelList.updateContent();
    if (t.id == 0)
    {
        tone.reset();
        refreshFavoriteButton();
        toneLink.setVisible (false);
        downloadAll.setEnabled (false);
        repaint();
        return;
    }

    tone = t;
    refreshFavoriteButton();
    if (t.isIR() != irMode) setFormat (t.isIR());
    toneLink.setURL (juce::URL (t.url.isNotEmpty() ? t.url : "https://www.tone3000.com/tones/" + juce::String (t.id)));
    toneLink.setVisible (true);
    setStatus ("Loading models...");

    const int serial = ++requestSerial;
    client->listModels (t.id, [safe = juce::Component::SafePointer<Tone3000Panel> (this), serial] (t3k::Result<juce::Array<t3k::Model>> r)
    {
        if (safe == nullptr || serial != safe->requestSerial) return;
        if (! r.ok) return safe->setStatus (r.error, true);
        safe->models = r.value;
        safe->modelList.updateContent();
        safe->modelList.repaint();
        safe->downloadAll.setEnabled (! r.value.isEmpty());
        safe->setStatus (juce::String (r.value.size()) + " files. Click one to download it and load it into the "
                         + (safe->target == Target::amp ? juce::String ("amp.") : juce::String ("cab slot.")));
    });
    repaint();
}

juce::File Tone3000Panel::folderFor (const t3k::Tone& t) const
{
    const auto& lib = t.isIR() ? AbyssalProcessor::irLibrary() : AbyssalProcessor::namLibrary();
    return lib.getDirectory().getChildFile ("TONE3000")
              .getChildFile (t3k::safeFileName (t.title + " - " + t.user.username));
}

juce::File Tone3000Panel::fileFor (const t3k::Model& m) const
{
    if (! tone) return {};
    auto ext = m.extension();
    if (ext.isEmpty()) ext = tone->isIR() ? ".wav" : ".nam";
    return folderFor (*tone).getChildFile (t3k::safeFileName (m.name) + ext);
}

void Tone3000Panel::writeCredits (const t3k::Tone& t) const
{
    // Attribution travels with the files (TONE3000's design rules ask apps to credit creators).
    auto f = folderFor (t).getChildFile ("TONE3000 credits.txt");
    if (f.existsAsFile()) return;
    f.replaceWithText (t.title + "\r\nby @" + t.user.username + (t.user.displayName.isNotEmpty() ? " (" + t.user.displayName + ")" : juce::String())
                       + "\r\n" + t.url + "\r\nLicense: " + (t.license.isNotEmpty() ? t.license : juce::String ("see the tone page"))
                       + "\r\n\r\nDownloaded from TONE3000 (www.tone3000.com) with Abyssal.\r\n");
}

void Tone3000Panel::download (const t3k::Model& m, bool loadAfter)
{
    if (! tone) return;
    const auto file = fileFor (m);
    const auto currentTone = *tone;
    downloading.addIfNotAlreadyThere (m.id);
    modelList.repaint();
    setStatus ("Downloading " + m.name + "...");

    client->downloadModel (m, file, [safe = juce::Component::SafePointer<Tone3000Panel> (this), m, loadAfter, currentTone] (t3k::Result<juce::File> r)
    {
        if (safe == nullptr) return;
        safe->downloading.removeFirstMatchingValue (m.id);
        safe->modelList.repaint();
        if (! r.ok) return safe->setStatus (r.error, true);
        safe->writeCredits (currentTone);
        safe->setStatus ("Saved " + r.value.getFileName() + " to your library.");
        if (loadAfter) safe->loadFile (r.value);
    });
}

void Tone3000Panel::downloadAllStandard()
{
    juce::Array<t3k::Model> todo;
    for (auto& m : models)
        if ((tone && tone->isIR()) || m.size.isEmpty() || m.size == "standard" || m.size == "custom")
            if (! fileFor (m).existsAsFile())
                todo.add (m);
    if (todo.isEmpty())
        return setStatus ("Everything at standard size is already in your library.");
    for (int i = 0; i < todo.size(); ++i)
        download (todo[i], i == 0); // load the first one so you hear something straight away
}

void Tone3000Panel::loadFile (const juce::File& file)
{
    const bool amp = target == Target::amp;
    auto finished = [safe = juce::Component::SafePointer<Tone3000Panel> (this), file, amp] (const juce::String& err)
    {
        if (safe == nullptr) return;
        safe->setStatus (err.isNotEmpty() ? err : "Loaded " + file.getFileNameWithoutExtension()
                                                  + (amp ? " into the amp." : " into the cab."), err.isNotEmpty());
    };
    switch (target)
    {
        case Target::amp:
            setStatus ("Loading " + file.getFileNameWithoutExtension() + "...");
            proc.loadNamModel (file, finished);
            break;
        case Target::cabA: finished (proc.loadUserIR (0, file)); break;
        case Target::cabB: finished (proc.loadUserIR (1, file)); break;
    }
}

//============================================================================== layout + paint
juce::Rectangle<int> Tone3000Panel::panelBounds() const { return getLocalBounds().reduced (30, 16); }

void Tone3000Panel::resized()
{
    auto r = panelBounds().reduced (20, 14);
    headerArea = r.removeFromTop (36);
    auto h = headerArea;
    closeButton.setBounds (h.removeFromRight (80).reduced (0, 2));
    h.removeFromRight (8);
    connectButton.setBounds (h.removeFromRight (120).reduced (0, 2));
    getKeyLink.setBounds (h.removeFromRight (110));
    saveKey.setBounds (h.removeFromRight (90).reduced (0, 2));
    h.removeFromRight (6);
    keyEditor.setBounds (h.removeFromRight (std::min (360, h.getWidth() - 220)).reduced (0, 3));

    r.removeFromTop (10);
    auto row = r.removeFromTop (34);
    namButton.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (6);
    irButton.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (16);
    pickButton.setBounds (row.removeFromLeft (300));

    r.removeFromBottom (26); // status line
    r.removeFromTop (12);
    auto tabCol = r.removeFromLeft (150);
    for (auto* t : tabs) { t->setBounds (tabCol.removeFromTop (32).reduced (0, 2)); tabCol.removeFromTop (2); }
    r.removeFromLeft (12);
    auto listCol = r.removeFromLeft (360);
    loadMore.setBounds (listCol.removeFromBottom (30).reduced (60, 0));
    listCol.removeFromBottom (6);
    toneList.setBounds (listCol);
    r.removeFromLeft (16);

    detailArea = r;
    auto d = r;
    d.removeFromTop (70);
    auto linkRow = d.removeFromTop (24);
    toneLink.setBounds (linkRow.removeFromLeft (180));
    favoriteButton.setBounds (linkRow.removeFromRight (120));
    d.removeFromTop (6);
    downloadAll.setBounds (d.removeFromBottom (32).removeFromRight (220));
    d.removeFromBottom (8);
    modelList.setBounds (d);
}

void Tone3000Panel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));
    auto panel = panelBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (panel, 12.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panel, 12.0f, 1.0f);

    // TONE3000 wordmark (text rendition) + connection state
    auto h = headerArea;
    g.setColour (colours::text);
    g.setFont (font (22.0f, true));
    g.drawText ("TONE3000", h.removeFromLeft (130), juce::Justification::centredLeft);
    g.setFont (font (12.0f, true));
    g.setColour (client->isConnected() ? juce::Colour (0xff3ddc84) : colours::textDim);
    g.drawText (! client->hasKey() ? "ADD YOUR API KEY" : client->isConnected() ? "CONNECTED" : "NOT CONNECTED",
                h.removeFromLeft (140), juce::Justification::centredLeft);

    // Target
    g.setColour (colours::textDim);
    g.setFont (font (11.0f, true));
    const juce::String targetText = target == Target::amp ? "LOADS INTO: AMP (NAM CAPTURE)"
                                  : target == Target::cabA ? "LOADS INTO: CAB MIC A" : "LOADS INTO: CAB MIC B";
    g.drawText (targetText, pickButton.getBounds().withX (pickButton.getRight() + 16).withWidth (300), juce::Justification::centredLeft);

    // Tone detail header
    auto d = detailArea;
    if (tone)
    {
        g.setColour (colours::text);
        g.setFont (font (18.0f, true));
        g.drawFittedText (tone->title, d.removeFromTop (26), juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (colours::accentAlt);
        g.setFont (font (13.0f, true));
        g.drawText ("by @" + tone->user.username + (tone->user.verified ? "  (verified creator)" : ""), d.removeFromTop (20), juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.setFont (font (12.0f));
        g.drawText (tone->gear.toUpperCase() + "  -  " + juce::String (tone->downloadsCount) + " downloads  -  "
                        + (tone->license.isNotEmpty() ? tone->license : juce::String ("license on tone page")),
                    d.removeFromTop (20), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (colours::textDim);
        g.setFont (font (14.0f));
        g.drawFittedText (client->hasKey()
                              ? "Pick a tone on tone3000.com, or choose one from your lists on the left."
                              : "To use TONE3000 you need a free publishable key:\n"
                                "1. Sign in at tone3000.com and open Settings > API.\n"
                                "2. Create a key and paste the publishable key (t3k_pub_...) above.\n"
                                "Abyssal never needs or stores your secret key.",
                          d.removeFromTop (110), juce::Justification::topLeft, 5);
    }

    // Status line
    auto statusArea = panelBounds().reduced (20, 10).removeFromBottom (20);
    g.setColour (statusIsError ? colours::accent : colours::textDim);
    g.setFont (font (12.0f));
    g.drawText (status, statusArea, juce::Justification::centredLeft);
    g.setColour (colours::textDim.withAlpha (0.6f));
    g.drawText ("Tones by their creators on TONE3000. Files are saved to Documents\\Abyssal\\" + juce::String (irMode ? "IRs" : "NAM") + "\\TONE3000",
                statusArea, juce::Justification::centredRight);
}

bool Tone3000Panel::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}

void Tone3000Panel::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()))
        close();
}

} // namespace abyssal::ui
