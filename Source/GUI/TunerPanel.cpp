#include "TunerPanel.h"
#include "../PluginProcessor.h"

namespace abyssal::ui
{

namespace
{
    const juce::Colour inTune { 0xff3ddc84 };
    constexpr float inTuneCents = 3.0f;
}

TunerPanel::TunerPanel (AbyssalProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    samples.resize (2048);

    closeButton.onClick = [this] { close(); };
    muteButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonOnColourId, colours::accent);
    muteButton.setToggleState (proc.getTunerMute(), juce::dontSendNotification);
    muteButton.setTooltip ("Silence the output while the tuner is open");
    muteButton.onClick = [this] { proc.setTunerMute (muteButton.getToggleState()); };

    refDown.onClick = [this] { proc.setTunerReference (std::round (proc.getTunerReference()) - 1.0f); updateReferenceLabel(); };
    refUp.onClick   = [this] { proc.setTunerReference (std::round (proc.getTunerReference()) + 1.0f); updateReferenceLabel(); };
    refDown.setTooltip ("Lower the A4 reference");
    refUp.setTooltip ("Raise the A4 reference");
    refLabel.setJustificationType (juce::Justification::centred);
    refLabel.setFont (font (14.0f, true));
    refLabel.setColour (juce::Label::textColourId, colours::text);
    updateReferenceLabel();

    for (auto* c : std::initializer_list<juce::Component*> { &closeButton, &muteButton, &refDown, &refUp, &refLabel })
        addAndMakeVisible (c);
}

TunerPanel::~TunerPanel()
{
    if (proc.isTunerOpen())
        proc.setTunerOpen (false);
}

void TunerPanel::updateReferenceLabel()
{
    refLabel.setText ("A4 = " + juce::String (juce::roundToInt (proc.getTunerReference())) + " Hz", juce::dontSendNotification);
}

void TunerPanel::open()
{
    proc.setTunerOpen (true);
    muteButton.setToggleState (proc.getTunerMute(), juce::dontSendNotification);
    hasPitch = false;
    framesWithoutPitch = 1000;
    recentCount = 0;
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    startTimerHz (30);
    if (onVisibilityChange) onVisibilityChange();
}

void TunerPanel::close()
{
    stopTimer();
    proc.setTunerOpen (false);
    setVisible (false);
    if (onVisibilityChange) onVisibilityChange();
}

void TunerPanel::timerCallback()
{
    const double rate = proc.readTunerInput (samples.data(), (int) samples.size());
    const auto r = detector.detect (samples.data(), (int) samples.size(), rate);

    if (r.frequency > 0.0f && r.clarity > 0.85f)
    {
        // Median of the last few readings throws out the odd octave jump during the pick attack.
        recent[(size_t) recentPos] = r.frequency;
        recentPos = (recentPos + 1) % (int) recent.size();
        recentCount = std::min (recentCount + 1, (int) recent.size());
        std::array<float, 5> sorted {};
        std::copy (recent.begin(), recent.begin() + recentCount, sorted.begin());
        std::sort (sorted.begin(), sorted.begin() + recentCount);
        const float hz = sorted[(size_t) recentCount / 2];

        const auto note = NoteReading::fromFrequency (hz, proc.getTunerReference());
        const bool sameNote = hasPitch && note.name == noteName && note.octave == octave;
        shownCents = sameNote ? shownCents + 0.35f * (note.cents - shownCents) : note.cents;
        shownHz = hz;
        noteName = note.name;
        octave = note.octave;
        hasPitch = true;
        framesWithoutPitch = 0;
    }
    else if (++framesWithoutPitch > 15) // hold the last reading for half a second
    {
        hasPitch = false;
        recentCount = 0;
    }
    repaint (panelBounds());
}

juce::Rectangle<int> TunerPanel::panelBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (680, 440);
}

void TunerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.7f));
    auto panel = panelBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (panel, 14.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panel, 14.0f, 1.0f);

    auto inner = panelBounds().reduced (24, 18);
    g.setColour (colours::text);
    g.setFont (font (20.0f, true));
    g.drawText ("TUNER", inner.removeFromTop (34), juce::Justification::centredLeft);

    const bool locked = hasPitch && std::abs (shownCents) < inTuneCents;
    const auto needleColour = ! hasPitch ? colours::textDim : locked ? inTune : std::abs (shownCents) < 15.0f ? colours::amber : colours::accent;

    // ---- note name
    auto noteArea = inner.removeFromTop (150);
    g.setColour (hasPitch ? (locked ? inTune : colours::text) : colours::outline);
    g.setFont (font (110.0f, true));
    const auto name = hasPitch ? noteName : juce::String ("-");
    const float nameW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), name);
    const float x0 = (float) noteArea.getCentreX() - nameW * 0.5f;
    g.drawText (name, juce::Rectangle<float> (x0, (float) noteArea.getY(), nameW + 4, (float) noteArea.getHeight()),
                juce::Justification::centredLeft);
    if (hasPitch)
    {
        g.setFont (font (30.0f, true));
        g.setColour (colours::textDim);
        g.drawText (juce::String (octave), juce::Rectangle<float> (x0 + nameW + 4, (float) noteArea.getCentreY(), 60, 50),
                    juce::Justification::topLeft);
    }

    // ---- cents scale with needle
    auto scale = inner.removeFromTop (110).reduced (20, 0).toFloat();
    const float midX = scale.getCentreX();
    const float halfW = scale.getWidth() * 0.5f;
    auto xForCents = [&] (float c) { return midX + juce::jlimit (-50.0f, 50.0f, c) / 50.0f * halfW; };

    g.setColour (inTune.withAlpha (0.12f));
    g.fillRoundedRectangle (juce::Rectangle<float> (xForCents (-inTuneCents), scale.getY(), xForCents (inTuneCents) - xForCents (-inTuneCents),
                                                    scale.getHeight() - 26), 4.0f);
    for (int c = -50; c <= 50; c += 5)
    {
        const bool major = c % 25 == 0;
        const float x = xForCents ((float) c);
        g.setColour (c == 0 ? colours::text : major ? colours::textDim : colours::outline);
        const float h = c == 0 ? 50.0f : major ? 34.0f : 20.0f;
        g.fillRect (x - (c == 0 ? 1.5f : 1.0f), scale.getY() + (50.0f - h) * 0.5f + 10.0f, c == 0 ? 3.0f : 2.0f, h);
        if (major)
        {
            g.setFont (font (11.0f, true));
            g.drawText (c > 0 ? "+" + juce::String (c) : juce::String (c), juce::Rectangle<float> (x - 20, scale.getBottom() - 22, 40, 16),
                        juce::Justification::centred);
        }
    }
    if (hasPitch)
    {
        const float x = xForCents (shownCents);
        g.setColour (needleColour.withAlpha (0.25f));
        g.fillRoundedRectangle (x - 8.0f, scale.getY(), 16.0f, scale.getHeight() - 26, 6.0f);
        g.setColour (needleColour);
        g.fillRoundedRectangle (x - 2.5f, scale.getY(), 5.0f, scale.getHeight() - 26, 2.5f);
    }

    // ---- readouts
    auto readout = inner.removeFromTop (34);
    g.setFont (font (16.0f, true));
    g.setColour (hasPitch ? needleColour : colours::textDim);
    g.drawText (hasPitch ? (shownCents >= 0 ? "+" : "") + juce::String (shownCents, 1) + " cents" : juce::String ("Play a single string"),
                readout.removeFromLeft (readout.getWidth() / 2).withTrimmedLeft (20), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    if (hasPitch)
        g.drawText (juce::String (shownHz, 2) + " Hz", readout.withTrimmedRight (20), juce::Justification::centredRight);

    g.setFont (font (12.0f));
    g.drawText (proc.getTunerMute() ? "Output muted while tuning  -  Esc to close" : "Output live while tuning  -  Esc to close",
                panelBounds().reduced (24, 14).removeFromBottom (20), juce::Justification::centredLeft);
}

void TunerPanel::resized()
{
    auto inner = panelBounds().reduced (24, 18);
    auto top = inner.removeFromTop (34);
    closeButton.setBounds (top.removeFromRight (90).reduced (0, 2));
    top.removeFromRight (10);
    muteButton.setBounds (top.removeFromRight (80).reduced (0, 2));
    top.removeFromRight (18);
    refUp.setBounds (top.removeFromRight (30).reduced (0, 2));
    refLabel.setBounds (top.removeFromRight (110));
    refDown.setBounds (top.removeFromRight (30).reduced (0, 2));
}

void TunerPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()))
        close();
}

bool TunerPanel::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}

} // namespace abyssal::ui
