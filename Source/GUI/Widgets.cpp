#include "Widgets.h"

namespace abyssal::ui
{

void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb9bcc3), c.x - r, c.y - r,
                                             juce::Colour (0xff43464d), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
    g.drawLine (c.x - r * 0.6f, c.y - r * 0.3f, c.x + r * 0.6f, c.y + r * 0.3f, 1.2f);
}

//============================================================================== Knob
Knob::Knob (APVTS& s, const juce::String& paramId, const juce::String& text, juce::Colour accent, bool bipolar)
    : state (s), label (text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setMouseDragSensitivity (220);
    slider.setPopupMenuEnabled (true);
    slider.onValueChange = [this] { repaint(); };
    slider.addMouseListener (this, false);
    addAndMakeVisible (slider);
    bind (paramId);
}

void Knob::bind (const juce::String& paramId)
{
    param = state.getParameter (paramId);
    jassert (param != nullptr);
    attachment.reset(); // detach first so the old parameter isn't touched by the new range
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (18);
    const int size = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (size, size));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom (18);
    juce::String text = label;
    if (slider.isMouseOverOrDragging() && param != nullptr)
    {
        text = param->getCurrentValueAsText();
        if (param->getLabel().isNotEmpty())
            text << " " << param->getLabel();
    }
    g.setColour (slider.isMouseOverOrDragging() ? juce::Colours::white : labelColour);
    g.setFont (font (12.5f, true));
    g.drawFittedText (text.toUpperCase(), r, juce::Justification::centred, 1, 0.8f);
}

//============================================================================== Footswitch
Footswitch::Footswitch (APVTS& state, const juce::String& paramId, juce::Colour led)
    : juce::Button (paramId), ledColour (led)
{
    setClickingTogglesState (true);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
}

void Footswitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();

    // LED
    const float ledR = 5.5f;
    const juce::Point<float> ledC (r.getCentreX(), r.getY() + ledR + 2.0f);
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (ledColour.withAlpha (0.55f), ledC, ledColour.withAlpha (0.0f),
                                                 ledC.translated (ledR * 3.5f, 0), true));
        g.fillEllipse (ledC.x - ledR * 3.5f, ledC.y - ledR * 3.5f, ledR * 7, ledR * 7);
    }
    g.setColour (on ? ledColour.brighter (0.3f) : ledColour.withMultipliedBrightness (0.25f));
    g.fillEllipse (ledC.x - ledR, ledC.y - ledR, ledR * 2, ledR * 2);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (ledC.x - ledR, ledC.y - ledR, ledR * 2, ledR * 2, 1.0f);

    // Stomp switch
    auto sw = r.withTrimmedTop (ledR * 2 + 10.0f);
    const float sr = std::min (sw.getWidth(), sw.getHeight()) * 0.5f - 2.0f;
    const auto c = sw.getCentre();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - sr, c.y - sr + 3, sr * 2, sr * 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe4e6ea), c.x - sr, c.y - sr,
                                             juce::Colour (0xff5d6168), c.x + sr, c.y + sr, false));
    g.fillEllipse (c.x - sr, c.y - sr, sr * 2, sr * 2);
    const float ir = sr * (down ? 0.62f : 0.68f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8d9199), c.x, c.y - ir,
                                             juce::Colour (0xffd2d5da), c.x, c.y + ir, false));
    g.fillEllipse (c.x - ir, c.y - ir, ir * 2, ir * 2);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse (c.x - sr, c.y - sr, sr * 2, sr * 2);
    }
}

//============================================================================== MiniToggle / ChoiceBox
MiniToggle::MiniToggle (APVTS& s, const juce::String& paramId, const juce::String& text, juce::Colour on)
    : juce::TextButton (text), state (s)
{
    setClickingTogglesState (true);
    setColour (juce::TextButton::buttonOnColourId, on);
    bind (paramId);
}

void MiniToggle::bind (const juce::String& paramId)
{
    attachment.reset();
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
}

ChoiceBox::ChoiceBox (APVTS& state, const juce::String& paramId)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        addItemList (p->choices, 1);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, *this);
}

//============================================================================== ChoiceButtons
ChoiceButtons::ChoiceButtons (APVTS& state, const juce::String& paramId, juce::Array<juce::Colour> cols)
{
    auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId));
    jassert (p != nullptr);

    for (int i = 0; i < p->choices.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (p->choices[i].toUpperCase()));
        b->setColour (juce::TextButton::buttonOnColourId, cols[juce::jmin (i, cols.size() - 1)]);
        b->onClick = [this, i] { attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v)
    {
        selected = juce::roundToInt (v);
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == selected, juce::dontSendNotification);
        if (onChange) onChange (selected);
    });
    attachment->sendInitialUpdate();
}

void ChoiceButtons::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / std::max (1, buttons.size());
    for (auto* b : buttons)
        b->setBounds (r.removeFromLeft (w).reduced (3, 0));
}

//============================================================================== Fader
Fader::Fader (APVTS& state, const juce::String& paramId, const juce::String& text, juce::Colour accent)
    : label (text)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    slider.setDoubleClickReturnValue (true, 0.0);
}

void Fader::resized() { slider.setBounds (getLocalBounds().withTrimmedBottom (16)); }

void Fader::paint (juce::Graphics& g)
{
    g.setColour (colours::text.withAlpha (0.8f));
    g.setFont (font (11.0f, true));
    g.drawText (label, getLocalBounds().removeFromBottom (14), juce::Justification::centred);
}

//============================================================================== Pedal
Pedal::Pedal (APVTS& s, const juce::String& n, const juce::String& onId, juce::Colour b, juce::Colour a)
    : state (s), name (n), body (b), accent (a), footswitch (s, onId, a)
{
    onParam = s.getParameter (onId);
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    footswitch.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    footswitch.onStateChange = [this] { repaint(); };
    addAndMakeVisible (footswitch);
}

Knob& Pedal::addKnob (const juce::String& paramId, const juce::String& label, bool bipolar)
{
    auto* k = knobs.add (new Knob (state, paramId, label, accent, bipolar));
    k->setLabelColour (body.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff15161a) : colours::text);
    addAndMakeVisible (k);
    return *k;
}

void Pedal::addToggle (const juce::String& paramId, const juce::String& text)
{
    addAndMakeVisible (minis.add (new MiniToggle (state, paramId, text, accent)));
}

void Pedal::addChoice (const juce::String& paramId)
{
    addAndMakeVisible (minis.add (new ChoiceBox (state, paramId)));
}

void Pedal::addMini (juce::Component* c)
{
    addAndMakeVisible (minis.add (c));
}

juce::Rectangle<int> Pedal::getContentArea() const
{
    auto r = getLocalBounds().reduced (16, 18);
    r.removeFromBottom (76 + 30);
    if (! minis.isEmpty()) r.removeFromBottom (minis.size() > 3 ? 60 : 30);
    return r;
}

void Pedal::resized()
{
    auto r = getLocalBounds().reduced (16, 18);
    footswitch.setBounds (r.removeFromBottom (76).withSizeKeepingCentre (60, 76));
    r.removeFromBottom (30); // name plate

    if (! minis.isEmpty())
    {
        // Up to three per row; a fourth or fifth control starts a second row above.
        const int rows = minis.size() > 3 ? 2 : 1;
        const int perRow = (minis.size() + rows - 1) / rows;
        juce::Rectangle<int> rowAreas[2];
        for (int i = rows; --i >= 0;)
        {
            rowAreas[i] = r.removeFromBottom (26);
            r.removeFromBottom (4);
        }
        for (int row = 0; row < rows; ++row)
        {
            const int first = row * perRow, count = std::min (perRow, minis.size() - first);
            auto line = rowAreas[row];
            const int w = line.getWidth() / std::max (1, count);
            for (int i = 0; i < count; ++i)
                minis[first + i]->setBounds (line.removeFromLeft (w).reduced (2, 0));
        }
    }

    if (customLayout)
    {
        customLayout (r);
        return;
    }

    if (knobs.isEmpty())
        return;

    const int perRow = std::min (knobsPerRow, knobs.size());
    const int rows = (knobs.size() + perRow - 1) / perRow;
    const int cellH = r.getHeight() / rows;
    for (int row = 0; row < rows; ++row)
    {
        auto line = r.removeFromTop (cellH);
        const int inRow = std::min (perRow, knobs.size() - row * perRow);
        const int cellW = line.getWidth() / perRow;
        line = line.withSizeKeepingCentre (cellW * inRow, line.getHeight());
        for (int i = 0; i < inRow; ++i)
        {
            auto cell = line.removeFromLeft (cellW).reduced (2);
            const int size = std::min (cell.getWidth(), cell.getHeight() - 18);
            knobs[row * perRow + i]->setBounds (cell.withSizeKeepingCentre (size, size + 18));
        }
    }
}

void Pedal::mouseDown (const juce::MouseEvent& e)
{
    dragStarted = false;
    if (e.mods.isPopupMenu() && onContextMenu)
        onContextMenu (*this);
}

void Pedal::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || fxId < 0 || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 6)
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragStarted = true;
        container->startDragging ("fx:" + juce::String (fxId), this);
    }
}

void Pedal::paintOverChildren (juce::Graphics& g)
{
    // Dim the pedal while it is being dragged so the drop target reads clearly.
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (const_cast<Pedal*> (this));
    if (dragStarted && container != nullptr && container->isDragAndDropActive())
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 12.0f);
    }
}

void Pedal::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);

    // Drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0, 5), 12.0f);

    // Enclosure
    g.setGradientFill (juce::ColourGradient (body.brighter (0.25f), r.getX(), r.getY(),
                                             body.darker (0.35f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (r.reduced (1.0f), 11.0f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r, 12.0f, 1.5f);

    // Bottom section where the switch lives
    auto lower = r.withTop (r.getBottom() - 112.0f).reduced (8.0f, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (lower, 8.0f);

    for (auto p : { r.getTopLeft().translated (10, 10), r.getTopRight().translated (-10, 10),
                    r.getBottomLeft().translated (10, -10), r.getBottomRight().translated (-10, -10) })
        drawScrew (g, p, 4.0f);

    // Name plate
    const bool on = onParam != nullptr && onParam->getValue() > 0.5f;
    auto plate = getLocalBounds().reduced (16, 18).removeFromBottom (76 + 30).removeFromTop (30).toFloat();
    const bool lightBody = body.getPerceivedBrightness() > 0.6f;
    g.setColour (lightBody ? juce::Colour (0xff15161a) : juce::Colours::white.withAlpha (on ? 0.95f : 0.6f));
    g.setFont (font (std::min (19.0f, plate.getWidth() / (float) std::max (4, name.length()) * 1.6f), true));
    g.drawText (name.toUpperCase(), plate, juce::Justification::centred);
}

//============================================================================== GraphicEQPedal
GraphicEQPedal::GraphicEQPedal (APVTS& s, const juce::String& onId, const juce::StringArray& bandParams,
                                const juce::StringArray& bandLabels, const juce::String& levelParam)
    : Pedal (s, "Graphic EQ", onId, juce::Colour (0xff3a3e46), colours::amber)
{
    for (int i = 0; i < bandParams.size(); ++i)
        addAndMakeVisible (faders.add (new Fader (s, bandParams[i], bandLabels[i], colours::amber)));
    addAndMakeVisible (faders.add (new Fader (s, levelParam, "LEVEL", colours::accent)));

    setBodyArea ([this] (juce::Rectangle<int> r)
    {
        r = r.reduced (6, 0);
        const int w = r.getWidth() / faders.size();
        for (int i = 0; i < faders.size(); ++i)
        {
            if (i == faders.size() - 1) r.removeFromLeft (4); // separate the level fader
            faders[i]->setBounds (r.removeFromLeft (w).reduced (1, 0));
        }
    });
}

//============================================================================== DelayPedal
DelayPedal::DelayPedal (APVTS& s, const juce::String& onId, const juce::String& syncParam, const juce::String& timeParam,
                        juce::Colour b, juce::Colour a)
    : Pedal (s, "Delay", onId, b, a), syncId (syncParam), timeId (timeParam)
{
    sync = s.getRawParameterValue (syncParam);
    startTimerHz (5);
}

float DelayPedal::tap (double nowMs)
{
    // A pause of more than two seconds starts a new count.
    if (! taps.isEmpty() && nowMs - taps.getLast() > 2000.0)
        taps.clearQuick();
    taps.add (nowMs);
    while (taps.size() > 5)
        taps.remove (0);
    if (taps.size() < 2)
        return 0.0f;

    const float ms = juce::jlimit (10.0f, 2000.0f, (float) ((taps.getLast() - taps.getFirst()) / (taps.size() - 1)));
    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* p = state.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    };
    set (syncId, 0.0f);
    set (timeId, ms);
    return ms;
}

void DelayPedal::timerCallback()
{
    const bool synced = sync != nullptr && sync->load() > 0.5f;
    if (timeKnob != nullptr && timeKnob->isEnabled() == synced)
        timeKnob->setEnabled (! synced);
}

//============================================================================== LevelMeter
LevelMeter::LevelMeter (std::function<float()> s) : source (std::move (s)) { startTimerHz (30); }

void LevelMeter::timerCallback()
{
    const float v = source();
    level = std::max (v, level * 0.82f);
    if (v >= peakHold) { peakHold = v; holdFrames = 45; }
    else if (--holdFrames <= 0) peakHold *= 0.92f;
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 2.0f);

    auto toY = [&] (float lin)
    {
        const float db = juce::jlimit (-60.0f, 6.0f, juce::Decibels::gainToDecibels (lin, -60.0f));
        return juce::jmap (db, -60.0f, 6.0f, r.getBottom(), r.getY());
    };

    const float y = toY (level);
    juce::ColourGradient grad (juce::Colour (0xff3ddc84), 0, r.getBottom(), juce::Colour (0xffff3b30), 0, r.getY(), false);
    grad.addColour (0.75, juce::Colour (0xffffc107));
    g.setGradientFill (grad);
    g.fillRect (r.withTop (y).reduced (1.0f, 0));

    g.setColour (peakHold > 1.0f ? juce::Colour (0xffff3b30) : juce::Colours::white.withAlpha (0.8f));
    g.fillRect (r.getX() + 1, toY (peakHold) - 1.0f, r.getWidth() - 2, 2.0f);
}

} // namespace abyssal::ui
