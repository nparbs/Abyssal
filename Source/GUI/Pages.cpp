#include "Pages.h"
#include "../PluginProcessor.h"

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
// PreFXPage
//==============================================================================
PreFXPage::PreFXPage (AbyssalProcessor& p)
    : gate    (p.apvts, "Noise Gate", ids::gateOn,  juce::Colour (0xff2c3038), juce::Colour (0xff3ddc84)),
      octaver (p.apvts, "Octaver",    ids::octOn,   juce::Colour (0xff4a2a78), juce::Colour (0xffc58cff)),
      drop    (p.apvts, "Drop Tune",  ids::dropOn,  juce::Colour (0xff0e5661), juce::Colour (0xff4dd9ec)),
      comp    (p.apvts, "Compressor", ids::compOn,  juce::Colour (0xff1c3d86), juce::Colour (0xff64b0ff)),
      od      (p.apvts, "Overdrive",  ids::odOn,    juce::Colour (0xff2e7a34), juce::Colour (0xffc6ff8a)),
      geq     (p.apvts, "Graphic EQ", ids::geqOn,   juce::Colour (0xff3a3e46), colours::amber)
{
    gate.addKnob (ids::gateThresh, "Threshold");
    gate.addKnob (ids::gateRelease, "Release");

    octaver.addKnob (ids::octDry, "Dry");
    octaver.addKnob (ids::octDown1, "-1 Oct");
    octaver.addKnob (ids::octDown2, "-2 Oct");

    drop.addKnob (ids::dropSemis, "Semitones", true);
    drop.addKnob (ids::dropMix, "Mix");

    comp.setKnobsPerRow (3);
    comp.addKnob (ids::compThresh, "Thresh");
    comp.addKnob (ids::compRatio, "Ratio");
    comp.addKnob (ids::compAttack, "Attack");
    comp.addKnob (ids::compRelease, "Release");
    comp.addKnob (ids::compLevel, "Level");

    od.addKnob (ids::odDrive, "Drive");
    od.addKnob (ids::odTone, "Tone");
    od.addKnob (ids::odLevel, "Level");

    for (int b = 0; b < GraphicEQParams::numBands; ++b)
    {
        const float f = GraphicEQParams::freqs[b];
        const auto label = f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "K" : juce::String ((int) f);
        geq.addAndMakeVisible (faders.add (new Fader (p.apvts, ids::geqBand (b), label, colours::amber)));
    }
    geq.addAndMakeVisible (faders.add (new Fader (p.apvts, ids::geqLevel, "LEVEL", colours::accent)));
    geq.setBodyArea ([this] (juce::Rectangle<int> r)
    {
        r = r.reduced (10, 0);
        const int w = r.getWidth() / faders.size();
        for (int i = 0; i < faders.size(); ++i)
        {
            if (i == faders.size() - 1) r.removeFromLeft (4); // separate the level fader
            faders[i]->setBounds (r.removeFromLeft (w).reduced (2, 0));
        }
    });

    for (auto* c : { &gate, &octaver, &drop, &comp, &od, &geq })
        addAndMakeVisible (c);
}

void PreFXPage::resized()
{
    auto area = getLocalBounds().reduced (24, 12);
    auto row1 = area.removeFromTop (330);
    const int gap = 14;
    const int w = (row1.getWidth() - gap * 4) / 5;
    for (auto* pd : { &gate, &octaver, &drop, &comp, &od })
    {
        pd->setBounds (row1.removeFromLeft (w));
        row1.removeFromLeft (gap);
    }
    area.removeFromTop (14);
    geq.setBounds (area.withSizeKeepingCentre (700, area.getHeight()));
}

void PreFXPage::paint (juce::Graphics& g)
{
    fillPageBackground (g, getLocalBounds());
    // Pedalboard rails
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int y = 40; y < getHeight() - 10; y += 46)
        g.fillRect (16, y, getWidth() - 32, 18);

    // Patch cables between the pedals on the top row
    g.setColour (juce::Colour (0xff111111));
    for (auto [a, b] : { std::pair<juce::Component*, juce::Component*> { &gate, &octaver }, { &octaver, &drop }, { &drop, &comp }, { &comp, &od } })
    {
        const float y = (float) a->getY() + 40.0f;
        g.drawLine ((float) a->getRight() - 4, y, (float) b->getX() + 4, y, 5.0f);
    }
}

//==============================================================================
// AmpPage
//==============================================================================
AmpPage::AmpPage (AbyssalProcessor& p)
    : proc (p),
      channel (p.apvts, ids::ampChannel, { colours::accentAlt, colours::amber, colours::accent }),
      mode (p.apvts, ids::ampMode, { colours::accent, colours::amber }),
      gain (p.apvts, ids::ampGain, "Gain"),
      bass (p.apvts, ids::ampBass, "Bass"),
      mid (p.apvts, ids::ampMid, "Middle"),
      treble (p.apvts, ids::ampTreble, "Treble"),
      presence (p.apvts, ids::ampPresence, "Presence"),
      depth (p.apvts, ids::ampDepth, "Depth"),
      master (p.apvts, ids::ampMaster, "Master"),
      namIn (p.apvts, ids::namIn, "Input", colours::amber, true),
      namOut (p.apvts, ids::namOut, "Output", colours::amber, true),
      bright (p.apvts, ids::ampBright, "BRIGHT"),
      tight (p.apvts, ids::ampTight, "TIGHT")
{
    for (auto* c : std::initializer_list<juce::Component*> { &channel, &mode, &gain, &bass, &mid, &treble, &presence, &depth,
                                                             &master, &namIn, &namOut, &bright, &tight, &loadCapture,
                                                             &unloadCapture, &namName, &namInfo })
        addAndMakeVisible (c);

    namName.setFont (font (16.0f, true));
    namName.setColour (juce::Label::textColourId, colours::text);
    namInfo.setFont (font (13.0f));
    namInfo.setColour (juce::Label::textColourId, colours::textDim);

    loadCapture.onClick = [this]
    {
        auto start = proc.getNamFile().existsAsFile() ? proc.getNamFile().getParentDirectory()
                                                       : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        chooser = std::make_unique<juce::FileChooser> ("Load a Neural Amp Modeler capture", start, "*.nam");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            const auto err = proc.loadNamModel (f);
            if (err.isNotEmpty()) showError ("NAM capture", err);
            refreshNamLabel();
        });
    };
    unloadCapture.onClick = [this] { proc.unloadNamModel(); refreshNamLabel(); };

    channel.onChange = [this] (int c) { channelChanged (c); };
    mode.onChange = [this] (int m) { modeChanged (m); };
    channelChanged (channel.getSelected());
    modeChanged (mode.getSelected());
    refreshNamLabel();
    startTimerHz (4);
}

void AmpPage::timerCallback() { refreshNamLabel(); }

void AmpPage::refreshNamLabel()
{
    juce::String name = "No capture loaded", info = "Load a .nam file to play through a neural capture of a real amp.";
    if (proc.hasNamModel())
    {
        name = proc.getNamFile().getFileNameWithoutExtension();
        const double mr = proc.getNamSampleRate(), hr = proc.getCurrentSampleRate();
        info = "Trained at " + juce::String (mr / 1000.0, 1) + " kHz";
        if (std::abs (mr - hr) > 0.5)
            info << "  -  resampled from the host's " << juce::String (hr / 1000.0, 1) << " kHz";
    }
    if (namName.getText() != name) namName.setText (name, juce::dontSendNotification);
    if (namInfo.getText() != info) namInfo.setText (info, juce::dontSendNotification);
    unloadCapture.setEnabled (proc.hasNamModel());
}

void AmpPage::channelChanged (int ch)
{
    static const juce::Colour cols[] = { colours::accentAlt, colours::amber, colours::accent };
    static const char* names[] = { "CLEAN", "CRUNCH", "LEAD" };
    accent = cols[juce::jlimit (0, 2, ch)];
    channelName = names[juce::jlimit (0, 2, ch)];
    for (auto* k : { &gain, &bass, &mid, &treble, &presence, &depth, &master })
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    bright.setColour (juce::TextButton::buttonOnColourId, accent);
    tight.setColour (juce::TextButton::buttonOnColourId, accent);
    repaint();
}

void AmpPage::modeChanged (int m)
{
    currentMode = m;
    for (auto* k : { &gain, &bass, &mid, &treble, &presence, &depth, &master })
        k->setEnabled (m == 0);
    bright.setEnabled (m == 0);
    tight.setEnabled (m == 0);
    channel.setEnabled (m == 0);
    namIn.setEnabled (m == 1);
    namOut.setEnabled (m == 1);
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
    channel.setBounds (top.removeFromRight (390));
    f.removeFromTop (8);

    auto toggles = f.removeFromRight (104);
    bright.setBounds (toggles.removeFromTop (toggles.getHeight() / 2).withSizeKeepingCentre (88, 32));
    tight.setBounds (toggles.withSizeKeepingCentre (88, 32));

    const int w = f.getWidth() / 7;
    for (auto* k : { &gain, &bass, &mid, &treble, &presence, &depth, &master })
        k->setBounds (f.removeFromLeft (w).reduced (6, 0));

    auto mp = modelPanel.reduced (22, 16);
    auto knobsArea = mp.removeFromRight (220);
    namIn.setBounds (knobsArea.removeFromLeft (110).reduced (8, 0));
    namOut.setBounds (knobsArea.reduced (8, 0));

    mp.removeFromTop (24); // title
    auto row = mp.removeFromTop (36);
    mode.setBounds (row.removeFromLeft (330));
    row.removeFromLeft (20);
    loadCapture.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (8);
    unloadCapture.setBounds (row.removeFromLeft (90));
    mp.removeFromTop (8);
    namName.setBounds (mp.removeFromTop (24));
    namInfo.setBounds (mp.removeFromTop (22));
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
    g.drawText (currentMode == 1 ? "NEURAL CAPTURE MODE" : "3-CHANNEL  /  120W", logo.removeFromTop (16), juce::Justification::centredLeft);

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
        g.drawText ("Capture mode bypasses the amp's tone controls - shape the tone on the EQ page.",
                    modelPanel.reduced (22, 10).removeFromBottom (18), juce::Justification::centredLeft);
    }
}

//==============================================================================
// Cabinet
//==============================================================================
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
      source (p.apvts, ids::cab (s, "src")),
      level (p.apvts, ids::cab (s, "level"), "Level", s == 0 ? colours::accent : colours::accentAlt),
      pan (p.apvts, ids::cab (s, "pan"), "Pan", s == 0 ? colours::accent : colours::accentAlt, true),
      mic (p.apvts, ids::cab (s, "mic"), "Mic Pos", s == 0 ? colours::accent : colours::accentAlt)
{
    for (auto* c : std::initializer_list<juce::Component*> { &onToggle, &source, &loadIR, &fileLabel, &level, &pan, &mic })
        addAndMakeVisible (c);

    fileLabel.setFont (font (13.0f));
    fileLabel.setColour (juce::Label::textColourId, colours::textDim);

    loadIR.onClick = [this]
    {
        auto start = proc.getUserIR (slot).existsAsFile() ? proc.getUserIR (slot).getParentDirectory()
                                                           : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        chooser = std::make_unique<juce::FileChooser> ("Load a cabinet impulse response", start, "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            const auto err = proc.loadUserIR (slot, f);
            if (err.isNotEmpty()) showError ("Impulse response", err);
            refreshFileLabel();
        });
    };
    refreshFileLabel();
}

void CabSlotPanel::refreshFileLabel()
{
    const int src = (int) proc.apvts.getRawParameterValue (ids::cab (slot, "src"))->load();
    juce::String text;
    if (src == Cabinet::userSource)
        text = proc.getUserIR (slot).existsAsFile() ? proc.getUserIR (slot).getFileName()
                                                    : "No IR file yet - playing the Modern 4x12";
    else
        text = "Built-in synthesized cabinet";
    if (fileLabel.getText() != text)
        fileLabel.setText (text, juce::dontSendNotification);
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
    loadIR.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (10);
    fileLabel.setBounds (row);
    r.removeFromTop (8);
    const int w = r.getWidth() / 3;
    for (auto* k : { &level, &pan, &mic })
        k->setBounds (r.removeFromLeft (w).reduced (8, 0));
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
    : view (p), slotA (p, 0), slotB (p, 1), cabOn (p.apvts, ids::cabOn, "CABINET ON")
{
    for (auto* c : std::initializer_list<juce::Component*> { &view, &slotA, &slotB, &cabOn })
        addAndMakeVisible (c);
    startTimerHz (4);
}

void CabPage::resized()
{
    auto r = getLocalBounds().reduced (28, 16);
    view.setBounds (r.removeFromLeft (560));
    r.removeFromLeft (24);
    cabOn.setBounds (r.removeFromTop (34).removeFromRight (150));
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
// Post FX
//==============================================================================
PostFXPage::PostFXPage (AbyssalProcessor& p)
    : proc (p),
      chorus  (p.apvts, "Chorus",  ids::choOn, juce::Colour (0xff17508f), juce::Colour (0xff7cc4ff)),
      flanger (p.apvts, "Flanger", ids::flOn,  juce::Colour (0xff5c1f86), juce::Colour (0xffe08cff)),
      delay   (p.apvts, "Delay",   ids::dlyOn, juce::Colour (0xff353c45), colours::amber),
      reverb  (p.apvts, "Reverb",  ids::revOn, juce::Colour (0xff0c4a43), juce::Colour (0xff5ef2d6))
{
    chorus.addKnob (ids::choRate, "Rate");
    chorus.addKnob (ids::choDepth, "Depth");
    chorus.addKnob (ids::choMix, "Mix");

    flanger.addKnob (ids::flRate, "Rate");
    flanger.addKnob (ids::flDepth, "Depth");
    flanger.addKnob (ids::flFeedback, "Feedback", true);
    flanger.addKnob (ids::flMix, "Mix");

    delayTime = &delay.addKnob (ids::dlyTime, "Time");
    delay.addKnob (ids::dlyFeedback, "Feedback");
    delay.addKnob (ids::dlyTone, "Tone");
    delay.addKnob (ids::dlyMix, "Mix");
    delay.addToggle (ids::dlySync, "SYNC");
    delay.addChoice (ids::dlyDiv);
    delay.addToggle (ids::dlyPing, "PING");

    reverb.setKnobsPerRow (3);
    reverb.addKnob (ids::revSize, "Size");
    reverb.addKnob (ids::revDamp, "Damping");
    reverb.addKnob (ids::revPredelay, "Pre-Delay");
    reverb.addKnob (ids::revWidth, "Width");
    reverb.addKnob (ids::revMix, "Mix");

    for (auto* c : { &chorus, &flanger, &delay, &reverb })
        addAndMakeVisible (c);
    startTimerHz (5);
}

void PostFXPage::timerCallback()
{
    const bool sync = proc.apvts.getRawParameterValue (ids::dlySync)->load() > 0.5f;
    if (delayTime != nullptr && delayTime->isEnabled() == sync)
        delayTime->setEnabled (! sync);
}

void PostFXPage::resized()
{
    auto r = getLocalBounds().reduced (30, 24);
    const int gap = 22;
    const int w = (r.getWidth() - gap * 3) / 4;
    r = r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), 470));
    for (auto* pd : { &chorus, &flanger, &delay, &reverb })
    {
        pd->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (gap);
    }
}

void PostFXPage::paint (juce::Graphics& g)
{
    fillPageBackground (g, getLocalBounds());
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int y = 50; y < getHeight() - 10; y += 52)
        g.fillRect (16, y, getWidth() - 32, 20);
}

//==============================================================================
// ChainBar
//==============================================================================
ChainBar::ChainBar (AbyssalProcessor& p) : proc (p) { startTimerHz (6); }

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

    switch (i)
    {
        case 0:
            if (on (ids::gateOn)) active.add ("GATE");
            if (on (ids::octOn))  active.add ("OCT");
            if (on (ids::dropOn)) active.add ("DROP");
            if (on (ids::compOn)) active.add ("COMP");
            if (on (ids::odOn))   active.add ("OD");
            if (on (ids::geqOn))  active.add ("GEQ");
            lit = ! active.isEmpty();
            return lit ? active.joinIntoString (" / ") : "ALL BYPASSED";
        case 1:
        {
            lit = true;
            if (on (ids::ampMode))
                return proc.hasNamModel() ? "NAM: " + proc.getNamFile().getFileNameWithoutExtension() : "NAM: NO CAPTURE";
            static const char* names[] = { "CLEAN CHANNEL", "CRUNCH CHANNEL", "LEAD CHANNEL" };
            return names[juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue (ids::ampChannel)->load())];
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
            if (on (ids::choOn)) active.add ("CHORUS");
            if (on (ids::flOn))  active.add ("FLANGER");
            if (on (ids::dlyOn)) active.add ("DELAY");
            if (on (ids::revOn)) active.add ("REVERB");
            lit = ! active.isEmpty();
            return lit ? active.joinIntoString (" / ") : "ALL BYPASSED";
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

        if (sel)
        {
            g.setColour (colours::accent.withAlpha (0.18f));
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
    for (int i = 0; i < numBlocks; ++i)
        if (blockBounds (i).contains (e.position))
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
