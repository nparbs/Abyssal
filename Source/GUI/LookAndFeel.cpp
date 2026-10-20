#include "LookAndFeel.h"

namespace abyssal::ui
{

juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

AbyssalLookAndFeel::AbyssalLookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, panelHi);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.8f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, panelHi);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
}

void AbyssalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const auto accentCol = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = radius - 2.0f;
    const float arcW = std::max (2.5f, radius * 0.09f);

    // Value track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff23262d));
    g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0, std::min (from, angle), std::max (from, angle), true);
    g.setColour (accentCol.withAlpha (s.isEnabled() ? 1.0f : 0.3f));
    g.strokePath (value, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob body with knurled skirt
    const float bodyR = arcR - arcW - 3.0f;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - bodyR, c.y - bodyR + 3.0f, bodyR * 2.0f, bodyR * 2.0f);

    juce::ColourGradient skirt (juce::Colour (0xff4a4e57), c.x, c.y - bodyR, juce::Colour (0xff121317), c.x, c.y + bodyR, false);
    g.setGradientFill (skirt);
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    const int ridges = 36;
    for (int i = 0; i < ridges; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) ridges + angle;
        g.drawLine (c.x + std::sin (a) * bodyR * 0.86f, c.y - std::cos (a) * bodyR * 0.86f,
                    c.x + std::sin (a) * bodyR, c.y - std::cos (a) * bodyR, 1.0f);
    }

    const float capR = bodyR * 0.78f;
    juce::ColourGradient cap (juce::Colour (0xff3b3f48), c.x - capR * 0.5f, c.y - capR,
                              juce::Colour (0xff17191e), c.x + capR * 0.5f, c.y + capR, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    // Pointer
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.6f, -capR + 2.0f, 3.2f, capR * 0.55f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (s.isEnabled() ? 0.92f : 0.35f));
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void AbyssalLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }

    const auto accentCol = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float cx = (float) x + (float) w * 0.5f;
    const float top = (float) y + 6.0f, bottom = (float) (y + h) - 6.0f;

    // Groove
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (cx - 2.5f, top, 5.0f, bottom - top, 2.5f);

    // Centre detent mark and fill from centre
    const float mid = (top + bottom) * 0.5f;
    g.setColour (colours::textDim.withAlpha (0.5f));
    g.drawHorizontalLine ((int) mid, cx - 9.0f, cx + 9.0f);
    g.setColour (accentCol.withAlpha (0.85f));
    g.fillRect (cx - 1.5f, std::min (pos, mid), 3.0f, std::abs (pos - mid));

    // Fader cap
    const juce::Rectangle<float> capR (cx - 11.0f, pos - 6.0f, 22.0f, 12.0f);
    juce::ColourGradient grad (juce::Colour (0xffd8dade), capR.getX(), capR.getY(),
                               juce::Colour (0xff7d818a), capR.getX(), capR.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (capR, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawHorizontalLine ((int) pos, capR.getX() + 2.0f, capR.getRight() - 2.0f);
}

void AbyssalLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                               bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto onCol = b.findColour (juce::TextButton::buttonOnColourId);

    juce::Colour base = on ? onCol.withMultipliedBrightness (0.55f) : colours::panelHi;
    if (highlighted) base = base.brighter (0.08f);
    if (down) base = base.darker (0.15f);

    g.setGradientFill (juce::ColourGradient (base.brighter (0.06f), 0, r.getY(), base.darker (0.15f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? onCol.withAlpha (0.9f) : colours::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void AbyssalLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (font (std::min (14.0f, (float) b.getHeight() * 0.5f), true));
    g.setColour (b.getToggleState() ? juce::Colours::white : colours::text.withAlpha (b.isEnabled() ? 0.85f : 0.4f));
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, true);
}

void AbyssalLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::accent.withAlpha (0.7f) : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) width - 16.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void AbyssalLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

} // namespace abyssal::ui
