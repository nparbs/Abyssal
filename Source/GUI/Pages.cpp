#include "Pages.h"
#include "../PluginProcessor.h"
#include <map>

namespace abyssal::ui
{

namespace
{
    void fillPageBackground (juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto r = area.toFloat().reduced (8.0f, 2.0f);
        g.setGradientFill (juce::ColourGradient (colours::panel, 0, r.getY(), colours::bg, 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (colours::outline.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    }

    void showError (const juce::String& title, const juce::String& message)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, message);
    }

    const juce::Colour bandColours[4] = { juce::Colour (0xffff6b4a), juce::Colour (0xffffc94a),
                                          juce::Colour (0xff4ad9ff), juce::Colour (0xffb57bff) };
}

//==============================================================================
// PedalSet
//==============================================================================
PedalSet::PedalSet (AbyssalProcessor& p) : proc (p)
{
    auto& st = p.apvts;
    auto make = [&] (int id, Pedal* pd)
    {
        pd->setFxId (id);
        pd->onContextMenu = [this] (Pedal& x) { showContextMenu (x); };
        pedals[(size_t) id].reset (pd);
        return pd;
    };

    auto* gate = make (fxGate, new Pedal (st, "Noise Gate", ids::gateOn, juce::Colour (0xff2c3038), juce::Colour (0xff3ddc84)));
    gate->addKnob (ids::gateThresh, "Threshold");
    gate->addKnob (ids::gateRelease, "Release");
    gate->addToggle (ids::gateKey, "INPUT KEY");

    auto* oct = make (fxOctaver, new Pedal (st, "Octaver", ids::octOn, juce::Colour (0xff4a2a78), juce::Colour (0xffc58cff)));
    oct->addKnob (ids::octDry, "Dry");
    oct->addKnob (ids::octDown1, "-1 Oct");
    oct->addKnob (ids::octDown2, "-2 Oct");

    auto* drop = make (fxDrop, new Pedal (st, "Drop Tune", ids::dropOn, juce::Colour (0xff0e5661), juce::Colour (0xff4dd9ec)));
    drop->addKnob (ids::dropSemis, "Semitones", true);
    drop->addKnob (ids::dropMix, "Mix");

    auto* comp = make (fxComp, new Pedal (st, "Compressor", ids::compOn, juce::Colour (0xff1c3d86), juce::Colour (0xff64b0ff)));
    comp->setKnobsPerRow (3);
    comp->addKnob (ids::compThresh, "Thresh");
    comp->addKnob (ids::compRatio, "Ratio");
    comp->addKnob (ids::compAttack, "Attack");
    comp->addKnob (ids::compRelease, "Release");
    comp->addKnob (ids::compLevel, "Level");

    auto* od = make (fxOverdrive, new Pedal (st, "Overdrive", ids::odOn, juce::Colour (0xff2e7a34), juce::Colour (0xffc6ff8a)));
    od->addKnob (ids::odDrive, "Drive");
    od->addKnob (ids::odTone, "Tone");
    od->addKnob (ids::odLevel, "Level");

    juce::StringArray bandIds, bandLabels;
    for (int b = 0; b < GraphicEQParams::numBands; ++b)
    {
        const float f = GraphicEQParams::freqs[b];
        bandIds.add (ids::geqBand (b));
        bandLabels.add (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "K" : juce::String ((int) f));
    }
    make (fxGEQ, new GraphicEQPedal (st, ids::geqOn, bandIds, bandLabels, ids::geqLevel));

    auto* cho = make (fxChorus, new Pedal (st, "Chorus", ids::choOn, juce::Colour (0xff17508f), juce::Colour (0xff7cc4ff)));
    cho->addKnob (ids::choRate, "Rate");
    cho->addKnob (ids::choDepth, "Depth");
    cho->addKnob (ids::choMix, "Mix");

    auto* fl = make (fxFlanger, new Pedal (st, "Flanger", ids::flOn, juce::Colour (0xff5c1f86), juce::Colour (0xffe08cff)));
    fl->addKnob (ids::flRate, "Rate");
    fl->addKnob (ids::flDepth, "Depth");
    fl->addKnob (ids::flFeedback, "Feedback", true);
    fl->addKnob (ids::flMix, "Mix");

    auto* dly = new DelayPedal (st, ids::dlyOn, ids::dlySync, ids::dlyTime, juce::Colour (0xff353c45), colours::amber);
    make (fxDelay, dly);
    dly->setWidthUnits (2);
    dly->setKnobsPerRow (4);
    dly->timeKnob = &dly->addKnob (ids::dlyTime, "Time");
    dly->addKnob (ids::dlyFeedback, "Feedback");
    dly->addKnob (ids::dlyTone, "Tone");
    dly->addKnob (ids::dlyMix, "Mix");
    dly->addChoice (ids::dlyMode);
    dly->addToggle (ids::dlySync, "SYNC");
    dly->addChoice (ids::dlyDiv);
    dly->addToggle (ids::dlyPing, "PING");
    {
        auto* tapButton = new juce::TextButton ("TAP");
        tapButton->setTooltip ("Tap the delay time (turns tempo sync off)");
        tapButton->onClick = [dly, tapButton]
        {
            const float ms = dly->tap (juce::Time::getMillisecondCounterHiRes());
            tapButton->setButtonText (ms > 0.0f ? juce::String (juce::roundToInt (ms)) : juce::String ("TAP"));
        };
        dly->addMini (tapButton);
    }

    auto* rev = make (fxReverb, new Pedal (st, "Reverb", ids::revOn, juce::Colour (0xff0c4a43), juce::Colour (0xff5ef2d6)));
    rev->setWidthUnits (2);
    rev->setKnobsPerRow (6);
    rev->addKnob (ids::revSize, "Size");
    rev->addKnob (ids::revDamp, "Damping");
    rev->addKnob (ids::revPredelay, "Pre-Delay");
    rev->addKnob (ids::revWidth, "Width");
    rev->addKnob (ids::revMix, "Mix");
    rev->addKnob (ids::revShimmer, "Shimmer");
    rev->addChoice (ids::revMode);

    auto* wah = make (fxWah, new Pedal (st, "Wah", ids::wahOn, juce::Colour (0xff1f1f22), juce::Colour (0xffff9f43)));
    wah->setKnobsPerRow (3);
    wah->addKnob (ids::wahPos, "Pedal").slider.setTooltip ("Pedal position, heel to toe (automate it, or switch to Auto)");
    wah->addKnob (ids::wahSens, "Sens").slider.setTooltip ("Auto mode: how far your picking pushes the sweep");
    wah->addKnob (ids::wahQ, "Peak");
    wah->addChoice (ids::wahMode);

    auto* wham = make (fxWhammy, new Pedal (st, "Whammy", ids::whamOn, juce::Colour (0xff8a1c1c), juce::Colour (0xffff6d5a)));
    wham->addKnob (ids::whamPos, "Pedal").slider.setTooltip ("Treadle position: 0 = no shift, full = the whole interval");
    wham->addKnob (ids::whamMix, "Mix");
    wham->addChoice (ids::whamInterval);

    auto* pha = make (fxPhaser, new Pedal (st, "Phaser", ids::phaOn, juce::Colour (0xff9a4a12), juce::Colour (0xffffa94d)));
    pha->addKnob (ids::phaRate, "Rate");
    pha->addKnob (ids::phaDepth, "Depth");
    pha->addKnob (ids::phaFeedback, "Fdbk");
    pha->addKnob (ids::phaMix, "Mix");
    pha->addChoice (ids::phaStages);

    auto* trem = make (fxTremolo, new Pedal (st, "Tremolo", ids::tremOn, juce::Colour (0xff5b4a1d), juce::Colour (0xffffd166)));
    trem->addKnob (ids::tremRate, "Rate");
    trem->addKnob (ids::tremDepth, "Depth");
    trem->addKnob (ids::tremShape, "Shape");
    trem->addToggle (ids::tremSync, "SYNC");
    trem->addChoice (ids::tremDiv);
    trem->addToggle (ids::tremPan, "PAN");

    auto* dbl = make (fxDoubler, new Pedal (st, "Doubler", ids::dblOn, juce::Colour (0xff3a2f5c), juce::Colour (0xffb9a4ff)));
    dbl->addKnob (ids::dblWidth, "Width");
    dbl->addKnob (ids::dblTime, "Time");
    dbl->addKnob (ids::dblDetune, "Detune");
}

void PedalSet::showContextMenu (Pedal& pedal)
{
    const int id = pedal.getFxId();
    const auto order = proc.getChainOrder();
    const bool pre = order.isPreAmp (id);
    const auto section = order.section (pre);
    const int pos = section.indexOf (id);

    juce::PopupMenu m;
    m.addSectionHeader (juce::String (fxShortName (id)) + (pre ? "  -  before the amp" : "  -  after the amp"));
    m.addItem (1, pre ? "Move after the amp" : "Move before the amp");
    m.addItem (2, "Move left", pos > 0);
    m.addItem (3, "Move right", pos < section.size() - 1);
    m.addSeparator();
    m.addItem (4, "Reset pedal order");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&pedal), [this, id, pre, pos] (int r)
    {
        auto o = proc.getChainOrder();
        switch (r)
        {
            case 1: o.moveTo (id, ! pre, -1); break;
            case 2: o.moveTo (id, pre, pos - 1); break;
            case 3: o.moveTo (id, pre, pos + 1); break;
            case 4: o = ChainOrder::defaultOrder(); break;
            default: return;
        }
        proc.changeChainOrder (o);
    });
}

//==============================================================================
// BoardPage
//==============================================================================
BoardPage::BoardPage (AbyssalProcessor& p, PedalSet& s, bool pre) : proc (p), set (s), preAmp (pre)
{
    proc.chainChanged.addChangeListener (this);
    rebuild();
}

BoardPage::~BoardPage() { proc.chainChanged.removeChangeListener (this); }

void BoardPage::rebuild()
{
    items = proc.getChainOrder().section (preAmp);

    // Let go of pedals that moved to the other side of the amp.
    for (int i = getNumChildComponents(); --i >= 0;)
        if (auto* pd = dynamic_cast<Pedal*> (getChildComponent (i)))
            if (! items.contains (pd->getFxId()))
                removeChildComponent (pd);

    // Only the visible board claims its pedals, so the hidden one can't steal them back.
    if (isVisible())
        for (int id : items)
            addAndMakeVisible (set.get (id));
    resized();
    repaint();
}

void BoardPage::resized()
{
    cells.clearQuick();
    boardArea = getLocalBounds().reduced (24, 10).withTrimmedTop (30);
    if (items.isEmpty())
        return;

    auto unitsOf = [this] (int id) { return set.get (id).getWidthUnits(); };
    int totalUnits = 0;
    for (int id : items) totalUnits += unitsOf (id);

    const int gap = 14;
    const int rows = totalUnits <= 6 ? 1 : totalUnits <= 12 ? 2 : 3;

    // Split the pedals over the rows as evenly as possible by width.
    juce::Array<juce::Array<int>> rowItems;
    rowItems.resize (rows);
    int acc = 0;
    for (int id : items)
    {
        const int row = std::min (rows - 1, acc * rows / totalUnits);
        rowItems.getReference (row).add (id);
        acc += unitsOf (id);
    }

    int maxUnits = 1;
    for (auto& r : rowItems)
    {
        int u = 0;
        for (int id : r) u += unitsOf (id);
        maxUnits = std::max (maxUnits, u);
    }

    const int unitW = std::min (rows == 1 ? 260 : 230, (boardArea.getWidth() - gap * (maxUnits - 1)) / maxUnits);
    const int rowH = rows == 1 ? std::min (boardArea.getHeight(), 470) : (boardArea.getHeight() - gap * (rows - 1)) / rows;
    int y = rows == 1 ? boardArea.getCentreY() - rowH / 2 : boardArea.getY();
    auto widthOf = [&] (int id) { return unitsOf (id) * unitW + (unitsOf (id) - 1) * gap; };

    for (auto& r : rowItems)
    {
        int rowW = gap * (r.size() - 1);
        for (int id : r) rowW += widthOf (id);
        int x = boardArea.getCentreX() - rowW / 2;
        for (int id : r)
        {
            const juce::Rectangle<int> cell (x, y, widthOf (id), rowH);
            if (set.get (id).getParentComponent() == this)
                set.get (id).setBounds (cell);
            cells.add (cell);
            x += cell.getWidth() + gap;
        }
        y += rowH + gap;
    }
}

void BoardPage::paint (juce::Graphics& g)
{
    fillPageBackground (g, getLocalBounds());
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int y = 60; y < getHeight() - 10; y += 46)
        g.fillRect (16, y, getWidth() - 32, 18);

    auto title = getLocalBounds().reduced (28, 12).removeFromTop (22);
    g.setColour (colours::textDim);
    g.setFont (font (12.0f, true));
    g.drawText (preAmp ? "PRE-AMP PEDALS  (MONO)" : "POST-AMP EFFECTS  (STEREO)", title, juce::Justification::centredLeft);
    g.setFont (font (12.0f));
    g.drawText (juce::String ("Drag to reorder  -  drop on ") + (preAmp ? "POST FX" : "PRE FX")
                    + " above to move across the amp  -  right-click for options",
                title, juce::Justification::centredRight);

    if (items.isEmpty())
    {
        g.setFont (font (16.0f, true));
        g.drawText ("No pedals here - drag some in", boardArea, juce::Justification::centred);
    }
}

void BoardPage::paintOverChildren (juce::Graphics& g)
{
    if (insertIndex < 0)
        return;

    juce::Rectangle<int> ref;
    int x;
    if (cells.isEmpty())                 { ref = boardArea.withSizeKeepingCentre (8, 300); x = boardArea.getCentreX(); }
    else if (insertIndex < cells.size()) { ref = cells[insertIndex]; x = ref.getX() - 7; }
    else                                 { ref = cells.getLast(); x = ref.getRight() + 7; }

    g.setColour (colours::accent.withAlpha (0.25f));
    g.fillRoundedRectangle ((float) x - 9.0f, (float) ref.getY(), 18.0f, (float) ref.getHeight(), 6.0f);
    g.setColour (colours::accent);
    g.fillRoundedRectangle ((float) x - 2.5f, (float) ref.getY(), 5.0f, (float) ref.getHeight(), 2.5f);
}

int BoardPage::insertionIndexFor (juce::Point<int> pos) const
{
    for (int i = 0; i < cells.size(); ++i)
        if (pos.y < cells[i].getBottom() && pos.x < cells[i].getCentreX())
            return i;
    return cells.size();
}

bool BoardPage::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("fx:");
}

void BoardPage::itemDragMove (const SourceDetails& d)
{
    const int idx = insertionIndexFor (d.localPosition);
    if (idx != insertIndex) { insertIndex = idx; repaint(); }
}

void BoardPage::itemDropped (const SourceDetails& d)
{
    const int id = d.description.toString().fromFirstOccurrenceOf ("fx:", false, false).getIntValue();
    int idx = insertionIndexFor (d.localPosition);
    insertIndex = -1;

    // Removing the pedal from its old slot shifts everything after it.
    const int cur = items.indexOf (id);
    if (cur >= 0 && cur < idx) --idx;

    auto order = proc.getChainOrder();
    order.moveTo (id, preAmp, idx);
    proc.changeChainOrder (order);
    rebuild();
}

//==============================================================================
// Library dropdown configs
//==============================================================================
namespace
{
    LibrarySelector::Config namConfig (AbyssalProcessor& p)
    {
        LibrarySelector::Config c;
        c.library = &AbyssalProcessor::namLibrary();
        c.kind = "capture";
        // While a capture loads, treat it as current so < / > keep stepping from it.
        c.currentFile = [&p]
        {
            const auto loading = p.getNamLoadingFile();
            return loading != juce::File() ? loading : p.hasNamModel() ? p.getNamFile() : juce::File();
        };
        c.displayText = [&p]
        {
            const auto loading = p.getNamLoadingFile();
            if (loading != juce::File()) return "Loading " + loading.getFileNameWithoutExtension() + "...";
            return p.hasNamModel() ? p.getNamFile().getFileNameWithoutExtension() : juce::String ("No capture loaded");
        };
        c.loadFile = [&p] (const juce::File& f, LibrarySelector::Config::Done done) { p.loadNamModel (f, std::move (done)); };
        c.importFolder = [&p] (const juce::File& f, LibrarySelector::Config::Done done) { p.importNamFolder (f, std::move (done)); };
        c.removeFile = [&p] (const juce::File& f) { return p.removeFromNamLibrary (f); };
        c.browseStart = [&p] { return p.lastNamBrowseDirectory; };
        c.openOnline = [&p] { if (p.openTone3000) p.openTone3000 (0); };
        c.addBottomItems = [&p] (juce::PopupMenu& m) { if (p.hasNamModel()) m.addItem (200, "Unload capture"); };
        c.handleExtraItem = [&p] (int id) { if (id == 200) p.unloadNamModel(); };
        return c;
    }

    LibrarySelector::Config irConfig (AbyssalProcessor& p, int slot)
    {
        auto src = [&p, slot] { return (int) p.apvts.getRawParameterValue (ids::cab (slot, "src"))->load(); };

        LibrarySelector::Config c;
        c.library = &AbyssalProcessor::irLibrary();
        c.kind = "IR";
        c.currentFile = [&p, slot, src] { return src() == Cabinet::userSource ? p.getUserIR (slot) : juce::File(); };
        c.displayText = [&p, slot, src]
        {
            const int s = src();
            if (s < Cabinet::numBuiltIns) return Cabinet::getSourceNames()[s];
            return p.getUserIR (slot).existsAsFile() ? p.getUserIR (slot).getFileNameWithoutExtension()
                                                     : juce::String ("User IR (none loaded)");
        };
        c.loadFile = [&p, slot] (const juce::File& f, LibrarySelector::Config::Done done) { done (p.loadUserIR (slot, f)); };
        c.importFolder = [&p, slot] (const juce::File& f, LibrarySelector::Config::Done done) { done (p.importIRFolder (slot, f)); };
        c.removeFile = [&p] (const juce::File& f) { return p.removeFromIRLibrary (f); };
        c.browseStart = [&p] { return p.lastIRBrowseDirectory; };
        c.openOnline = [&p, slot] { if (p.openTone3000) p.openTone3000 (1 + slot); };
        c.addTopItems = [src] (juce::PopupMenu& m)
        {
            m.addSectionHeader ("BUILT-IN CABS");
            const auto names = Cabinet::getSourceNames();
            for (int i = 0; i < Cabinet::numBuiltIns; ++i)
                m.addItem (1 + i, names[i], true, src() == i);
        };
        c.handleExtraItem = [&p, slot] (int id)
        {
            if (id >= 1 && id <= Cabinet::numBuiltIns) p.selectBuiltInCab (slot, id - 1);
        };
        return c;
    }
}

//==============================================================================
// AmpPage
//==============================================================================
AmpPage::AmpPage (AbyssalProcessor& p)
    : proc (p),
      channel (p.apvts, ids::ampChannel, { colours::accentAlt, colours::amber, colours::accent, colours::abyss }),
      mode (p.apvts, ids::ampMode, { colours::accent, colours::amber }),
      power (p.apvts, ids::ampPower, { colours::textDim, colours::textDim }),
      gain (p.apvts, ids::ampGain, "Gain"),
      bass (p.apvts, ids::ampBass, "Bass"),
      mid (p.apvts, ids::ampMid, "Middle"),
      treble (p.apvts, ids::ampTreble, "Treble"),
      presence (p.apvts, ids::ampPresence, "Presence"),
      depth (p.apvts, ids::ampDepth, "Depth"),
      master (p.apvts, ids::ampMaster, "Master"),
      namIn (p.apvts, ids::namIn, "Input", colours::amber, true),
      namOut (p.apvts, ids::namOut, "Output", colours::amber, true),
      calLevel (p.apvts, ids::namCalLevel, "Interface", colours::amber),
      bright (p.apvts, ids::ampBright, "BRIGHT"),
      tight (p.apvts, ids::ampTight, "TIGHT"),
      calibrate (p.apvts, ids::namCalibrate, "CALIBRATE", colours::amber),
      capture (namConfig (p))
{
    for (auto* c : std::initializer_list<juce::Component*> { &channel, &mode, &power, &gain, &bass, &mid, &treble, &presence, &depth,
                                                             &master, &namIn, &namOut, &calLevel, &bright, &tight, &calibrate,
                                                             &capture, &loadCapture, &loadCaptureFolder, &unloadCapture,
                                                             &namInfo, &tone3000 })
        addAndMakeVisible (c);

    power.setTooltip ("Power tubes: EL34 (upper-mid bite, earlier break-up) or 6L6 (rounder mids, firmer lows, more headroom)");
    calibrate.setTooltip ("Match the capture's recorded input level to your interface, so captures react to your "
                          "playing like the real amp did. Needs a capture that records its input level.");
    calLevel.slider.setTooltip ("Your interface's input level, in dBu, for a 0 dBFS signal (see its specs, often 'max input level')");

    namInfo.setFont (font (13.0f));
    namInfo.setColour (juce::Label::textColourId, colours::textDim);

    loadCapture.onClick = [this] { capture.browseForFile(); };
    loadCaptureFolder.onClick = [this] { capture.browseForFolder(); };
    tone3000.setColour (juce::TextButton::buttonColourId, colours::accent.withMultipliedBrightness (0.45f));
    tone3000.setTooltip ("Browse and download NAM captures from TONE3000");
    tone3000.onClick = [this] { if (proc.openTone3000) proc.openTone3000 (0); };
    capture.onChange = [this] { refreshNamLabel(); };
    unloadCapture.onClick = [this] { proc.unloadNamModel(); refreshNamLabel(); };

    channel.onChange = [this] (int c) { channelChanged (c); };
    mode.onChange = [this] (int m) { modeChanged (m); };
    channelChanged (channel.getSelected());
    modeChanged (mode.getSelected());
    refreshNamLabel();
    startTimerHz (4);
}

void AmpPage::timerCallback()
{
    refreshNamLabel();
    if (currentMode == 1) // follow the TONE switch (also when automated)
        for (auto* k : { &bass, &mid, &treble, &presence })
            k->setEnabled (bright.getToggleState());
}

void AmpPage::refreshNamLabel()
{
    juce::String info = "Load a .nam file or a whole folder of captures to play through neural captures of real amps.";
    if (proc.getNamLoadingFile() != juce::File())
        info = "Loading capture...";
    else if (proc.hasNamModel())
    {
        const double mr = proc.getNamSampleRate(), hr = proc.getCurrentSampleRate();
        info = "Trained at " + juce::String (mr / 1000.0, 1) + " kHz";
        if (std::abs (mr - hr) > 0.5)
            info << "  -  resampled from the host's " << juce::String (hr / 1000.0, 1) << " kHz";
        if (proc.namHasInputLevel())
        {
            info << "  -  captured at " << juce::String (proc.getNamInputLevelDbu(), 1) << " dBu";
            const float cal = proc.getNamCalibrationDb();
            if (calibrate.getToggleState())
                info << " (input " << (cal >= 0 ? "+" : "") << juce::String (cal, 1) << " dB)";
        }
        else if (calibrate.getToggleState())
            info << "  -  no input level recorded, calibration off";
    }
    if (namInfo.getText() != info) namInfo.setText (info, juce::dontSendNotification);
    unloadCapture.setEnabled (proc.hasNamModel());
    calLevel.setEnabled (currentMode == 1 && calibrate.getToggleState());
}

void AmpPage::channelChanged (int ch)
{
    static const juce::Colour cols[] = { colours::accentAlt, colours::amber, colours::accent, colours::abyss };
    static const char* names[] = { "CLEAN", "CRUNCH", "LEAD", "ABYSS" };
    currentChannel = juce::jlimit (0, ids::numAmpChannels - 1, ch);
    accent = cols[currentChannel];
    channelName = names[currentChannel];
    bindFaceplate();
    repaint();
}

void AmpPage::bindFaceplate()
{
    Knob* faceplate[] = { &gain, &bass, &mid, &treble, &presence, &depth, &master };
    const bool capture = currentMode == 1;

    if (capture)
    {
        // A capture has no tone stack: Bass / Middle / Treble / Presence become an optional EQ after it.
        bass.bind (ids::namBass);
        mid.bind (ids::namMid);
        treble.bind (ids::namTreble);
        presence.bind (ids::namPresence);
        bright.bind (ids::namTone);
        bright.setButtonText ("TONE");
        bright.setTooltip ("Bass / Middle / Treble / Presence after the capture");
        const bool toneOn = bright.getToggleState();
        for (auto* k : faceplate)
            k->setEnabled (toneOn && (k == &bass || k == &mid || k == &treble || k == &presence));
    }
    else
    {
        // Every channel has its own faceplate: point the controls at this channel's parameters.
        for (int k = 0; k < 7; ++k)
            faceplate[k]->bind (ids::ampKnob (currentChannel, ids::ampKnobKeys[k]));
        bright.bind (ids::ampKnob (currentChannel, "bright"));
        tight.bind (ids::ampKnob (currentChannel, "tight"));
        bright.setButtonText ("BRIGHT");
        bright.setTooltip ({});
        for (auto* k : faceplate)
            k->setEnabled (true);
    }

    const auto col = capture ? colours::amber : accent;
    for (auto* k : faceplate)
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, col);
    bright.setColour (juce::TextButton::buttonOnColourId, col);
    tight.setColour (juce::TextButton::buttonOnColourId, col);
    tight.setVisible (! capture);
}

void AmpPage::modeChanged (int m)
{
    currentMode = m;
    bindFaceplate();
    channel.setEnabled (m == 0);
    power.setEnabled (m == 0);
    namIn.setEnabled (m == 1);
    namOut.setEnabled (m == 1);
    calibrate.setEnabled (m == 1);
    calLevel.setEnabled (m == 1 && calibrate.getToggleState());
    repaint();
}

void AmpPage::resized()
{
    auto area = getLocalBounds().reduced (24, 10);
    housing = area.removeFromTop (420);
    modelPanel = area.withTrimmedTop (14);

    auto inner = housing.reduced (30, 28);
    face = inner.removeFromTop (220);
    inner.removeFromTop (14);
    grill = inner;

    auto f = face.reduced (18, 14);
    f.removeFromLeft (210); // logo
    auto top = f.removeFromTop (34);
    channel.setBounds (top.removeFromRight (480));
    top.removeFromRight (18);
    power.setBounds (top.removeFromRight (150));
    f.removeFromTop (8);

    auto toggles = f.removeFromRight (104);
    bright.setBounds (toggles.removeFromTop (toggles.getHeight() / 2).withSizeKeepingCentre (88, 32));
    tight.setBounds (toggles.withSizeKeepingCentre (88, 32));

    const int w = f.getWidth() / 7;
    for (auto* k : { &gain, &bass, &mid, &treble, &presence, &depth, &master })
        k->setBounds (f.removeFromLeft (w).reduced (6, 0));

    auto mp = modelPanel.reduced (22, 16);
    auto knobsArea = mp.removeFromRight (300);
    namIn.setBounds (knobsArea.removeFromLeft (100).reduced (8, 0));
    namOut.setBounds (knobsArea.removeFromLeft (100).reduced (8, 0));
    calibrate.setBounds (knobsArea.removeFromTop (26).reduced (4, 0));
    knobsArea.removeFromTop (4);
    calLevel.setBounds (knobsArea.reduced (8, 0));

    mp.removeFromTop (24); // title
    auto row = mp.removeFromTop (34);
    mode.setBounds (row.removeFromLeft (300));
    row.removeFromLeft (16);
    capture.setBounds (row.removeFromLeft (std::min (row.getWidth(), 460)));
    mp.removeFromTop (8);
    row = mp.removeFromTop (30);
    row.removeFromLeft (316);
    loadCapture.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (6);
    loadCaptureFolder.setBounds (row.removeFromLeft (130));
    row.removeFromLeft (6);
    unloadCapture.setBounds (row.removeFromLeft (84));
    row.removeFromLeft (6);
    tone3000.setBounds (row.removeFromLeft (104));
    mp.removeFromTop (6);
    namInfo.setBounds (mp.removeFromTop (22).withTrimmedLeft (316));
}

void AmpPage::paint (juce::Graphics& g)
{
    fillPageBackground (g, getLocalBounds());

    // ---- housing (tolex)
    auto h = housing.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (h.translated (0, 8), 16.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1e22), h.getX(), h.getY(),
                                             juce::Colour (0xff0d0e10), h.getX(), h.getBottom(), false));
    g.fillRoundedRectangle (h, 16.0f);
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    for (float x = h.getX() - h.getHeight(); x < h.getRight(); x += 5.0f)
        g.drawLine (x, h.getBottom(), x + h.getHeight(), h.getY(), 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawRoundedRectangle (h.reduced (1.0f), 15.0f, 1.0f);

    // metal corner protectors
    for (auto c : { h.getTopLeft(), h.getTopRight(), h.getBottomLeft(), h.getBottomRight() })
    {
        juce::Rectangle<float> cr (c.x - 22, c.y - 22, 44, 44);
        cr = cr.constrainedWithin (h);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa9adb5), cr.getX(), cr.getY(),
                                                 juce::Colour (0xff3a3d43), cr.getRight(), cr.getBottom(), false));
        g.fillRoundedRectangle (cr, 8.0f);
        drawScrew (g, cr.getCentre(), 3.5f);
    }

    // ---- faceplate
    auto f = face.toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2c32), f.getX(), f.getY(),
                                             juce::Colour (0xff121317), f.getX(), f.getBottom(), false));
    g.fillRoundedRectangle (f, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.03f));
    for (float y = f.getY() + 2; y < f.getBottom(); y += 2.0f)
        g.drawHorizontalLine ((int) y, f.getX() + 4, f.getRight() - 4);
    g.setColour (accent.withAlpha (0.85f));
    g.fillRect (f.getX() + 12, f.getBottom() - 6, f.getWidth() - 24, 2.0f); // accent piping
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (f, 8.0f, 2.0f);

    // logo block
    auto logo = face.reduced (24, 20).removeFromLeft (200).toFloat();
    g.setColour (colours::text);
    g.setFont (font (36.0f, true));
    g.drawText ("ABYSSAL", logo.removeFromTop (48), juce::Justification::centredLeft);
    g.setColour (accent);
    g.setFont (font (20.0f, true));
    g.drawText (currentMode == 1 ? "CAPTURE" : channelName, logo.removeFromTop (26), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (font (11.0f, true));
    g.drawText ("HIGH GAIN AMPLIFIER", logo.removeFromTop (18), juce::Justification::centredLeft);
    g.drawText (currentMode == 1 ? "NEURAL CAPTURE MODE" : "4-CHANNEL  /  120W", logo.removeFromTop (16), juce::Justification::centredLeft);

    // power + standby jewel light
    const juce::Point<float> jewel (logo.getX() + 14, logo.getBottom() - 22);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.7f), jewel, accent.withAlpha (0.0f), jewel.translated (26, 0), true));
    g.fillEllipse (jewel.x - 26, jewel.y - 26, 52, 52);
    g.setColour (accent.brighter (0.4f));
    g.fillEllipse (jewel.x - 8, jewel.y - 8, 16, 16);

    // ---- grill cloth
    auto gr = grill.toFloat();
    g.setColour (juce::Colour (0xff0b0b0d));
    g.fillRoundedRectangle (gr, 6.0f);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (grill);
        g.setColour (accent.withAlpha (0.06f));
        for (float x = gr.getX() - gr.getHeight(); x < gr.getRight(); x += 9.0f)
        {
            g.drawLine (x, gr.getBottom(), x + gr.getHeight(), gr.getY(), 1.3f);
            g.drawLine (x, gr.getY(), x + gr.getHeight(), gr.getBottom(), 1.3f);
        }
    }
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (gr, 6.0f, 3.0f);

    auto badge = gr.withSizeKeepingCentre (180, 40);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9ccd2), badge.getX(), badge.getY(),
                                             juce::Colour (0xff5a5e66), badge.getX(), badge.getBottom(), false));
    g.fillRoundedRectangle (badge, 5.0f);
    g.setColour (juce::Colour (0xff141518));
    g.setFont (font (22.0f, true));
    g.drawText ("ABYSSAL", badge, juce::Justification::centred);

    // ---- model panel
    auto mp = modelPanel.toFloat();
    g.setColour (colours::panelHi);
    g.fillRoundedRectangle (mp, 10.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (mp, 10.0f, 1.0f);
    g.setColour (colours::textDim);
    g.setFont (font (12.0f, true));
    g.drawText ("AMP MODEL", modelPanel.reduced (22, 14).removeFromTop (18), juce::Justification::centredLeft);
    if (currentMode == 1)
    {
        g.setFont (font (12.0f));
        g.drawText ("Capture mode: the amp's tone stack is bypassed - switch on TONE for an EQ after the capture.",
                    modelPanel.reduced (22, 10).removeFromBottom (18), juce::Justification::centredLeft);
    }
}

//==============================================================================
// Cabinet
//==============================================================================
void CabView::timerCallback()
{
    float now[4];
    for (int s = 0; s < 2; ++s)
    {
        now[s * 2]     = proc.apvts.getRawParameterValue (ids::cab (s, "mic"))->load();
        now[s * 2 + 1] = proc.apvts.getRawParameterValue (ids::cab (s, "on"))->load();
    }
    if (std::equal (now, now + 4, lastState))
        return;
    std::copy (now, now + 4, lastState);
    repaint();
}

CabView::CabView (AbyssalProcessor& p) : proc (p)
{
    startTimerHz (15);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> CabView::speakerBounds() const
{
    return getLocalBounds().toFloat().reduced (40.0f, 40.0f);
}

juce::Point<float> CabView::micPoint (int slot) const
{
    const auto sb = speakerBounds();
    const float r = sb.getWidth() * 0.25f - 10.0f;
    const auto c = juce::Point<float> (sb.getX() + sb.getWidth() * 0.25f, sb.getY() + sb.getHeight() * 0.25f);
    const float mic = proc.apvts.getRawParameterValue (ids::cab (slot, "mic"))->load();
    return { c.x + mic * r * 0.92f, c.y + (slot == 0 ? -r * 0.22f : r * 0.22f) };
}

void CabView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (10.0f);

    // Cabinet box
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b.translated (0, 8), 14.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1d21), b.getX(), b.getY(),
                                             juce::Colour (0xff0c0d0f), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 14.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (b.reduced (1.0f), 13.0f, 1.0f);

    // Speakers (2x2)
    const auto sb = speakerBounds();
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 2; ++col)
        {
            const auto c = juce::Point<float> (sb.getX() + sb.getWidth() * (0.25f + 0.5f * (float) col),
                                               sb.getY() + sb.getHeight() * (0.25f + 0.5f * (float) row));
            const float r = sb.getWidth() * 0.25f - 10.0f;
            g.setColour (juce::Colour (0xff050506));
            g.fillEllipse (c.x - r - 6, c.y - r - 6, (r + 6) * 2, (r + 6) * 2);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2c30), c.x, c.y - r,
                                                     juce::Colour (0xff0e0f11), c.x, c.y + r, false));
            g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            for (float rr = r * 0.4f; rr < r; rr += r * 0.12f)
                g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.0f);
            const float cap = r * 0.3f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c52), c.x - cap, c.y - cap,
                                                     juce::Colour (0xff151618), c.x + cap, c.y + cap, false));
            g.fillEllipse (c.x - cap, c.y - cap, cap * 2, cap * 2);
        }

    // Mic markers on the top-left speaker
    for (int s = 0; s < 2; ++s)
    {
        const bool on = proc.apvts.getRawParameterValue (ids::cab (s, "on"))->load() > 0.5f;
        const auto col = (s == 0 ? colours::accent : colours::accentAlt).withAlpha (on ? 1.0f : 0.3f);
        const auto p = micPoint (s);
        g.setColour (col.withAlpha (on ? 0.25f : 0.1f));
        g.fillEllipse (p.x - 16, p.y - 16, 32, 32);
        g.setColour (col);
        g.drawEllipse (p.x - 11, p.y - 11, 22, 22, 2.0f);
        g.setFont (font (12.0f, true));
        g.drawText (s == 0 ? "A" : "B", juce::Rectangle<float> (p.x - 11, p.y - 11, 22, 22), juce::Justification::centred);
    }

    g.setColour (colours::textDim);
    g.setFont (font (12.0f));
    g.drawText ("Drag the A / B mics between the cap (centre) and the cone edge",
                getLocalBounds().removeFromBottom (30), juce::Justification::centred);
}

void CabView::mouseDown (const juce::MouseEvent& e)
{
    dragging = -1;
    float best = 24.0f;
    for (int s = 0; s < 2; ++s)
    {
        const float d = micPoint (s).getDistanceFrom (e.position);
        if (d < best) { best = d; dragging = s; }
    }
    if (dragging >= 0)
        if (auto* p = proc.apvts.getParameter (ids::cab (dragging, "mic")))
            p->beginChangeGesture();
}

void CabView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const auto sb = speakerBounds();
    const float r = sb.getWidth() * 0.25f - 10.0f;
    const float cx = sb.getX() + sb.getWidth() * 0.25f;
    const float mic = juce::jlimit (0.0f, 1.0f, (e.position.x - cx) / (r * 0.92f));
    if (auto* p = proc.apvts.getParameter (ids::cab (dragging, "mic")))
        p->setValueNotifyingHost (p->convertTo0to1 (mic));
    repaint();
}

void CabView::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        if (auto* p = proc.apvts.getParameter (ids::cab (dragging, "mic")))
            p->endChangeGesture();
    dragging = -1;
}

CabSlotPanel::CabSlotPanel (AbyssalProcessor& p, int s)
    : proc (p), slot (s),
      accent (s == 0 ? colours::accent : colours::accentAlt),
      onToggle (p.apvts, ids::cab (s, "on"), s == 0 ? "MIC A" : "MIC B", s == 0 ? colours::accent : colours::accentAlt),
      invert (p.apvts, ids::cab (s, "inv"), "POLARITY", s == 0 ? colours::accent : colours::accentAlt),
      source (irConfig (p, s)),
      micType (p.apvts, ids::cab (s, "mtype")),
      level (p.apvts, ids::cab (s, "level"), "Level", s == 0 ? colours::accent : colours::accentAlt),
      pan (p.apvts, ids::cab (s, "pan"), "Pan", s == 0 ? colours::accent : colours::accentAlt, true),
      mic (p.apvts, ids::cab (s, "mic"), "Mic Pos", s == 0 ? colours::accent : colours::accentAlt),
      distance (p.apvts, ids::cab (s, "dist"), "Distance", s == 0 ? colours::accent : colours::accentAlt)
{
    for (auto* c : std::initializer_list<juce::Component*> { &onToggle, &invert, &source, &loadIR, &loadFolder, &tone3000, &fileLabel,
                                                             &micType, &level, &pan, &mic, &distance })
        addAndMakeVisible (c);
    micType.setTooltip ("Mic type: dynamic (the classic close mic), ribbon (dark, smooth, big lows) or condenser (open, airy top)");
    invert.setTooltip ("Flip this mic's polarity (try it when blending two mics sounds thin)");
    distance.slider.setTooltip ("Mic distance from the grille, up to about 50 cm: less proximity bass, less air, more floor reflection");

    fileLabel.setFont (font (13.0f));
    fileLabel.setColour (juce::Label::textColourId, colours::textDim);

    loadIR.onClick = [this] { source.browseForFile(); };
    loadFolder.onClick = [this] { source.browseForFolder(); };
    tone3000.setColour (juce::TextButton::buttonColourId, colours::accent.withMultipliedBrightness (0.45f));
    tone3000.setTooltip ("Browse and download cab IRs from TONE3000");
    tone3000.onClick = [this] { if (proc.openTone3000) proc.openTone3000 (1 + slot); };
    source.onChange = [this] { refreshFileLabel(); };
    refreshFileLabel();
}

void CabSlotPanel::refreshFileLabel()
{
    const int src = (int) proc.apvts.getRawParameterValue (ids::cab (slot, "src"))->load();
    const auto ir = proc.getUserIR (slot);
    const auto& lib = AbyssalProcessor::irLibrary();
    juce::String text, tip;
    if (src != Cabinet::userSource)    text = "Built-in synthesized cabinet";
    else if (! ir.existsAsFile())      text = "No IR file yet - playing the Modern 4x12";
    else if (lib.contains (ir))        text = lib.folderOf (ir).isEmpty() ? "In your IR library" : "Library: " + lib.folderOf (ir);
    else                               { text = "From " + ir.getParentDirectory().getFileName() + " (not in library)"; tip = ir.getFullPathName(); }

    if (fileLabel.getText() != text)
        fileLabel.setText (text, juce::dontSendNotification);
    fileLabel.setTooltip (tip);
}

void CabSlotPanel::resized()
{
    auto r = getLocalBounds().reduced (18, 14);
    auto row = r.removeFromTop (32);
    onToggle.setBounds (row.removeFromLeft (90));
    row.removeFromLeft (10);
    source.setBounds (row);
    r.removeFromTop (8);
    row = r.removeFromTop (30);
    loadIR.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (6);
    loadFolder.setBounds (row.removeFromLeft (124));
    row.removeFromLeft (6);
    tone3000.setBounds (row.removeFromLeft (96));
    row.removeFromLeft (10);
    fileLabel.setBounds (row);
    r.removeFromTop (8);
    row = r.removeFromTop (28);
    micType.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (8);
    invert.setBounds (row.removeFromLeft (100));
    r.removeFromTop (6);
    const int w = r.getWidth() / 4;
    for (auto* k : { &level, &pan, &mic, &distance })
        k->setBounds (r.removeFromLeft (w).reduced (6, 0));
}

void CabSlotPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panelHi);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (accent.withAlpha (0.9f));
    g.fillRoundedRectangle (r.removeFromLeft (4.0f).reduced (0, 10), 2.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 10.0f, 1.0f);
}

CabPage::CabPage (AbyssalProcessor& p)
    : view (p), slotA (p, 0), slotB (p, 1), cabOn (p.apvts, ids::cabOn, "CABINET ON"),
      align (p.apvts, ids::cabAlign, "ALIGN MICS", colours::accentAlt)
{
    for (auto* c : std::initializer_list<juce::Component*> { &view, &slotA, &slotB, &cabOn, &align })
        addAndMakeVisible (c);
    align.setTooltip ("Line the two mics up in time: IR start offsets and mic distances no longer cause phase cancellation");
    startTimerHz (4);
}

void CabPage::resized()
{
    auto r = getLocalBounds().reduced (28, 16);
    view.setBounds (r.removeFromLeft (560));
    r.removeFromLeft (24);
    auto header = r.removeFromTop (34);
    cabOn.setBounds (header.removeFromRight (150));
    header.removeFromRight (8);
    align.setBounds (header.removeFromRight (130));
    r.removeFromTop (12);
    const int h = (r.getHeight() - 16) / 2;
    slotA.setBounds (r.removeFromTop (h));
    r.removeFromTop (16);
    slotB.setBounds (r);
}

void CabPage::paint (juce::Graphics& g)
{
    fillPageBackground (g, getLocalBounds());
    g.setColour (colours::textDim);
    g.setFont (font (12.0f, true));
    auto title = getLocalBounds().reduced (28, 16).withTrimmedLeft (584).removeFromTop (34);
    g.drawText ("DUAL MIC CABINET", title, juce::Justification::centredLeft);
}

//==============================================================================
// EQ
//==============================================================================
EQGraph::EQGraph (AbyssalProcessor& p) : proc (p) { startTimerHz (30); }

float EQGraph::xForFreq (float f) const
{
    return (float) getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
}
float EQGraph::freqForX (float x) const
{
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, x / (float) getWidth()));
}
float EQGraph::yForDb (float db) const
{
    return juce::jmap (db, maxDb, -maxDb, 10.0f, (float) getHeight() - 10.0f);
}
float EQGraph::dbForY (float y) const
{
    return juce::jmap (y, 10.0f, (float) getHeight() - 10.0f, maxDb, -maxDb);
}

float EQGraph::getParam (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void EQGraph::setParam (const juce::String& id, float value)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

juce::Point<float> EQGraph::nodePos (int node) const
{
    if (node == 0) return { xForFreq (getParam (ids::peqHpf)), yForDb (0) };
    if (node == 5) return { xForFreq (getParam (ids::peqLpf)), yForDb (0) };
    const int b = node - 1;
    return { xForFreq (getParam (ids::peq (b, "freq"))), yForDb (getParam (ids::peq (b, "gain"))) };
}

int EQGraph::hitNode (juce::Point<float> pos) const
{
    int best = -1;
    float bestD = 16.0f;
    for (int n = 0; n < 6; ++n)
    {
        const float d = nodePos (n).getDistanceFrom (pos);
        if (d < bestD) { bestD = d; best = n; }
    }
    return best;
}

void EQGraph::timerCallback()
{
    std::vector<float> values { getParam (ids::peqOn), getParam (ids::peqHpf), getParam (ids::peqLpf) };
    for (int b = 0; b < ParamEQParams::numBands; ++b)
        for (auto* f : { "freq", "gain", "q" })
            values.push_back (getParam (ids::peq (b, f)));
    if (values == lastValues && ! curve.isEmpty())
        return;
    lastValues = values;

    ParamEQParams p;
    p.hpfHz = values[1];
    p.lpfHz = values[2];
    for (int b = 0; b < ParamEQParams::numBands; ++b)
        p.band[b] = { values[(size_t) (3 + b * 3)], values[(size_t) (4 + b * 3)], values[(size_t) (5 + b * 3)] };

    curve.clear();
    for (int x = 0; x <= getWidth(); x += 2)
    {
        const float db = juce::jlimit (-maxDb - 6.0f, maxDb + 6.0f, ParamEQ::responseDb (p, freqForX ((float) x), 48000.0));
        if (x == 0) curve.startNewSubPath ((float) x, yForDb (db));
        else        curve.lineTo ((float) x, yForDb (db));
    }
    repaint();
}

void EQGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c0d10));
    g.fillRoundedRectangle (r, 8.0f);

    g.setFont (font (11.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f);
        g.setColour (colours::outline.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, 6.0f, r.getBottom() - 6.0f);
        g.setColour (colours::textDim);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x + 3, r.getBottom() - 20, 40, 14), juce::Justification::left);
    }
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = yForDb (db);
        g.setColour (colours::outline.withAlpha (db == 0.0f ? 1.0f : 0.5f));
        g.drawHorizontalLine ((int) y, 6.0f, r.getRight() - 6.0f);
        g.setColour (colours::textDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (8, y - 14, 40, 12), juce::Justification::left);
    }

    const bool on = getParam (ids::peqOn) > 0.5f;
    const auto col = on ? colours::accent : colours::textDim;

    juce::Path fill (curve);
    fill.lineTo (r.getRight(), yForDb (0));
    fill.lineTo (0, yForDb (0));
    fill.closeSubPath();
    g.setColour (col.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (curve, juce::PathStrokeType (2.5f));

    for (int n = 0; n < 6; ++n)
    {
        const auto p = nodePos (n);
        const auto nc = (n == 0 || n == 5) ? colours::text : bandColours[n - 1];
        g.setColour (nc.withAlpha (on ? 0.25f : 0.1f));
        g.fillEllipse (p.x - 13, p.y - 13, 26, 26);
        g.setColour (nc.withAlpha (on ? 1.0f : 0.4f));
        g.drawEllipse (p.x - 9, p.y - 9, 18, 18, 2.0f);
        g.setFont (font (11.0f, true));
        g.drawText (n == 0 ? "LC" : n == 5 ? "HC" : juce::String (n), juce::Rectangle<float> (p.x - 12, p.y - 9, 24, 18),
                    juce::Justification::centred);
    }

    g.setColour (colours::textDim);
    g.setFont (font (12.0f));
    g.drawText ("Drag nodes  -  mouse wheel sets Q  -  double-click resets gain", r.reduced (12, 8).removeFromTop (16),
                juce::Justification::topRight);
}

void EQGraph::mouseDown (const juce::MouseEvent& e)
{
    dragNode = hitNode (e.position);
    if (dragNode < 0) return;
    juce::StringArray idsToTouch;
    if (dragNode == 0) idsToTouch.add (ids::peqHpf);
    else if (dragNode == 5) idsToTouch.add (ids::peqLpf);
    else { idsToTouch.add (ids::peq (dragNode - 1, "freq")); idsToTouch.add (ids::peq (dragNode - 1, "gain")); }
    for (auto& id : idsToTouch)
        if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture();
}

void EQGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode < 0) return;
    const float f = freqForX (e.position.x);
    if (dragNode == 0)      setParam (ids::peqHpf, juce::jlimit (20.0f, 500.0f, f));
    else if (dragNode == 5) setParam (ids::peqLpf, juce::jlimit (2000.0f, 20000.0f, f));
    else
    {
        setParam (ids::peq (dragNode - 1, "freq"), juce::jlimit (30.0f, 16000.0f, f));
        setParam (ids::peq (dragNode - 1, "gain"), juce::jlimit (-15.0f, 15.0f, dbForY (e.position.y)));
    }
}

void EQGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragNode < 0) return;
    juce::StringArray idsToTouch;
    if (dragNode == 0) idsToTouch.add (ids::peqHpf);
    else if (dragNode == 5) idsToTouch.add (ids::peqLpf);
    else { idsToTouch.add (ids::peq (dragNode - 1, "freq")); idsToTouch.add (ids::peq (dragNode - 1, "gain")); }
    for (auto& id : idsToTouch)
        if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture();
    dragNode = -1;
}

void EQGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int n = hitNode (e.position);
    if (n < 1 || n > 4) return;
    const auto id = ids::peq (n - 1, "q");
    setParam (id, juce::jlimit (0.2f, 8.0f, getParam (id) * (w.deltaY > 0 ? 1.12f : 1.0f / 1.12f)));
}

void EQGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int n = hitNode (e.position);
    if (n >= 1 && n <= 4)
        setParam (ids::peq (n - 1, "gain"), 0.0f);
}

EQPage::EQPage (AbyssalProcessor& p)
    : graph (p), eqOn (p.apvts, ids::peqOn, "POST EQ ON")
{
    addAndMakeVisible (graph);
    addAndMakeVisible (eqOn);
    addAndMakeVisible (knobs.add (new Knob (p.apvts, ids::peqHpf, "Low Cut", colours::text)));
    for (int b = 0; b < ParamEQParams::numBands; ++b)
    {
        const auto n = juce::String (b + 1);
        addAndMakeVisible (knobs.add (new Knob (p.apvts, ids::peq (b, "freq"), "Freq " + n, bandColours[b])));
        addAndMakeVisible (knobs.add (new Knob (p.apvts, ids::peq (b, "gain"), "Gain " + n, bandColours[b], true)));
        addAndMakeVisible (knobs.add (new Knob (p.apvts, ids::peq (b, "q"), "Q " + n, bandColours[b])));
    }
    addAndMakeVisible (knobs.add (new Knob (p.apvts, ids::peqLpf, "High Cut", colours::text)));
}

void EQPage::resized()
{
    auto r = getLocalBounds().reduced (28, 16);
    auto top = r.removeFromTop (32);
    eqOn.setBounds (top.removeFromRight (150));
    r.removeFromTop (8);
    auto strip = r.removeFromBottom (104);
    r.removeFromBottom (12);
    graph.setBounds (r);

    const int w = strip.getWidth() / knobs.size();
    for (auto* k : knobs)
        k->setBounds (strip.removeFromLeft (w).reduced (3, 0));
}

void EQPage::paint (juce::Graphics& g)
{
    fillPageBackground (g, getLocalBounds());
    g.setColour (colours::textDim);
    g.setFont (font (12.0f, true));
    g.drawText ("POST-CAB PARAMETRIC EQ", getLocalBounds().reduced (28, 16).removeFromTop (32), juce::Justification::centredLeft);
}

//==============================================================================
// ChainBar
//==============================================================================
ChainBar::ChainBar (AbyssalProcessor& p) : proc (p) { startTimerHz (6); }

void ChainBar::timerCallback()
{
    juce::StringArray now;
    for (int i = 0; i < numBlocks; ++i)
    {
        bool lit = false;
        now.add (statusFor (i, lit) + (lit ? "1" : "0"));
    }
    if (now != lastStatus)
    {
        lastStatus = now;
        repaint();
    }
}

int ChainBar::blockAt (juce::Point<float> p) const
{
    for (int i = 0; i < numBlocks; ++i)
        if (blockBounds (i).contains (p)) return i;
    return -1;
}

bool ChainBar::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("fx:");
}

void ChainBar::itemDragMove (const SourceDetails& d)
{
    const int b = blockAt (d.localPosition.toFloat());
    const int target = (b == 0 || b == numBlocks - 1) ? b : -1;
    if (target != dragHover)
    {
        dragHover = target;
        dragHoverStart = juce::Time::getMillisecondCounter();
        repaint();
    }
    // Hovering over a pedal block opens that board so the pedal can be dropped precisely.
    else if (dragHover >= 0 && dragHover != selected && juce::Time::getMillisecondCounter() - dragHoverStart > 550)
    {
        setSelected (dragHover);
        if (onSelect) onSelect (dragHover);
    }
}

void ChainBar::itemDropped (const SourceDetails& d)
{
    const int b = blockAt (d.localPosition.toFloat());
    dragHover = -1;
    repaint();
    if (b != 0 && b != numBlocks - 1)
        return;

    const int id = d.description.toString().fromFirstOccurrenceOf ("fx:", false, false).getIntValue();
    auto order = proc.getChainOrder();
    order.moveTo (id, b == 0, -1);
    proc.changeChainOrder (order);
}


juce::Rectangle<float> ChainBar::blockBounds (int i) const
{
    auto r = getLocalBounds().toFloat().reduced (70.0f, 10.0f);
    const float gap = 30.0f;
    const float w = (r.getWidth() - gap * (numBlocks - 1)) / numBlocks;
    return { r.getX() + (float) i * (w + gap), r.getY(), w, r.getHeight() };
}

juce::String ChainBar::statusFor (int i, bool& lit) const
{
    auto on = [this] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; };
    juce::StringArray active;

    auto sectionStatus = [&] (bool pre)
    {
        for (int id : proc.getChainOrder().section (pre))
            if (on (ids::fxOnParam (id))) active.add (fxShortName (id));
        lit = ! active.isEmpty();
        return lit ? active.joinIntoString (" > ") : juce::String ("ALL BYPASSED");
    };

    switch (i)
    {
        case 0:
            return sectionStatus (true);
        case 1:
        {
            lit = true;
            if (on (ids::ampMode))
                return proc.hasNamModel() ? "NAM: " + proc.getNamFile().getFileNameWithoutExtension() : "NAM: NO CAPTURE";
            static const char* names[] = { "CLEAN", "CRUNCH", "LEAD", "ABYSS" };
            return juce::String (names[juce::jlimit (0, ids::numAmpChannels - 1, (int) proc.apvts.getRawParameterValue (ids::ampChannel)->load())])
                   + (on (ids::ampPower) ? "  /  6L6" : "  /  EL34");
        }
        case 2:
        {
            lit = on (ids::cabOn) && (on (ids::cab (0, "on")) || on (ids::cab (1, "on")));
            if (! lit) return "BYPASSED";
            const auto names = Cabinet::getSourceNames();
            for (int s = 0; s < 2; ++s)
                if (on (ids::cab (s, "on")))
                {
                    const int src = (int) proc.apvts.getRawParameterValue (ids::cab (s, "src"))->load();
                    active.add (src == Cabinet::userSource && proc.getUserIR (s).existsAsFile()
                                    ? proc.getUserIR (s).getFileNameWithoutExtension() : names[src]);
                }
            return active.joinIntoString (" + ");
        }
        case 3:
            lit = on (ids::peqOn);
            return lit ? "LC " + juce::String ((int) proc.apvts.getRawParameterValue (ids::peqHpf)->load()) + " Hz  /  HC "
                             + juce::String (proc.apvts.getRawParameterValue (ids::peqLpf)->load() / 1000.0f, 1) + " kHz"
                       : "BYPASSED";
        default:
            return sectionStatus (false);
    }
}

void ChainBar::paint (juce::Graphics& g)
{
    static const char* titles[] = { "PRE FX", "AMP", "CAB", "EQ", "POST FX" };
    const float midY = (float) getHeight() * 0.5f;

    // Cable through the whole chain, with IN / OUT jacks
    g.setColour (juce::Colour (0xff2a2d34));
    g.drawLine (40.0f, midY, (float) getWidth() - 40.0f, midY, 4.0f);
    for (auto [x, label] : { std::pair<float, const char*> { 34.0f, "IN" }, { (float) getWidth() - 34.0f, "OUT" } })
    {
        g.setColour (juce::Colour (0xff8d9199));
        g.fillEllipse (x - 9, midY - 9, 18, 18);
        g.setColour (juce::Colours::black);
        g.fillEllipse (x - 4, midY - 4, 8, 8);
        g.setColour (colours::textDim);
        g.setFont (font (10.0f, true));
        g.drawText (label, juce::Rectangle<float> (x - 20, midY + 10, 40, 14), juce::Justification::centred);
    }

    for (int i = 0; i < numBlocks; ++i)
    {
        auto b = blockBounds (i);
        bool lit = false;
        const auto status = statusFor (i, lit);
        const bool sel = i == selected;

        if (sel || i == dragHover)
        {
            g.setColour (colours::accent.withAlpha (i == dragHover ? 0.4f : 0.18f));
            g.fillRoundedRectangle (b.expanded (4.0f), 12.0f);
        }
        g.setGradientFill (juce::ColourGradient (sel ? juce::Colour (0xff2a2228) : colours::panelHi, 0, b.getY(),
                                                 colours::panel, 0, b.getBottom(), false));
        g.fillRoundedRectangle (b, 9.0f);
        g.setColour (sel ? colours::accent : (i == hover ? colours::textDim : colours::outline));
        g.drawRoundedRectangle (b, 9.0f, sel ? 2.0f : 1.0f);

        // status LED
        g.setColour (lit ? colours::accent : colours::ledOff);
        g.fillEllipse (b.getX() + 12, b.getY() + 13, 8, 8);

        g.setColour (sel ? juce::Colours::white : colours::text);
        g.setFont (font (15.0f, true));
        g.drawText (titles[i], b.withTrimmedLeft (28).withHeight (32), juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.setFont (font (11.0f, true));
        g.drawFittedText (status.toUpperCase(), b.reduced (12, 0).withTrimmedTop (30).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }
}

void ChainBar::mouseDown (const juce::MouseEvent& e)
{
    const int i = blockAt (e.position);
    if (i >= 0)
    {
        setSelected (i);
        if (onSelect) onSelect (i);
    }
}

void ChainBar::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < numBlocks; ++i)
        if (blockBounds (i).contains (e.position)) h = i;
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

} // namespace abyssal::ui
