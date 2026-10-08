#include "Theme.h"

namespace rs::theme
{
juce::Font font (float height, bool bold, float kerning)
{
    auto options = juce::FontOptions().withName ("Avenir Next").withHeight (height).withStyle (bold ? "Demi Bold" : "Regular");
    return juce::Font (options).withExtraKerningFactor (kerning);
}

juce::Font caption (float height)
{
    return font (height, true, 0.18f);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title)
{
    g.setColour (panel);
    g.fillRoundedRectangle (area, 10.0f);

    // Faint top light, like the sheen on the reference photo.
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.07f), area.getX(), area.getY(),
                                             juce::Colours::transparentBlack, area.getX(), area.getY() + 60.0f, false));
    g.fillRoundedRectangle (area, 10.0f);

    g.setColour (outline);
    g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);

    if (title.isNotEmpty())
    {
        g.setColour (textDim);
        g.setFont (caption());
        g.drawText (title.toUpperCase(), area.reduced (14.0f, 8.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
    }
}

void strokeGlow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width, float glowWidth)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (colour.withMultipliedAlpha (0.07f * (float) (4 - i)));
        g.strokePath (path, juce::PathStrokeType (width + glowWidth * (float) i / 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Path heartPath (juce::Rectangle<float> r)
{
    juce::Path p;
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    p.startNewSubPath (x + w * 0.5f, y + h * 0.95f);
    p.cubicTo (x - w * 0.15f, y + h * 0.5f, x + w * 0.1f, y - h * 0.15f, x + w * 0.5f, y + h * 0.25f);
    p.cubicTo (x + w * 0.9f, y - h * 0.15f, x + w * 1.15f, y + h * 0.5f, x + w * 0.5f, y + h * 0.95f);
    p.closeSubPath();
    return p;
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, black);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, abyss);
    setColour (juce::TextButton::buttonOnColourId, violet);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, highlight);
    setColour (juce::ComboBox::backgroundColourId, abyss);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf0050b0d));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, deep);
    setColour (juce::PopupMenu::highlightedTextColourId, highlight);
    setColour (juce::TextEditor::backgroundColourId, abyss);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, violet.withAlpha (0.5f));
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent.withAlpha (0.6f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xf5040a0c));
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, outline);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, violet);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                    float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool hover = slider.isMouseOverOrDragging();

    // Body: dark lens with a soft accent rim light.
    g.setGradientFill (juce::ColourGradient (deep.withAlpha (0.9f), centre.x - radius * 0.4f, centre.y - radius * 0.6f,
                                             black, centre.x + radius * 0.5f, centre.y + radius, true));
    g.fillEllipse (juce::Rectangle<float> (radius * 1.5f, radius * 1.5f).withCentre (centre));
    g.setColour (outline.withMultipliedAlpha (hover ? 2.0f : 1.0f));
    g.drawEllipse (juce::Rectangle<float> (radius * 1.5f, radius * 1.5f).withCentre (centre), 1.0f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (deep);
    g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar parameters (pitch, fine) draw their arc from the centre.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        strokeGlow (g, value, accent, 2.5f, 7.0f);
    }

    const auto tip = centre.getPointOnCircumference (radius * 0.55f, angle);
    g.setColour (highlight);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (tip));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                    float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style == juce::Slider::LinearBarVertical || style == juce::Slider::LinearBar)
    {
        // Number box: drag up/down to change, double-click to type.
        const auto r = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (0.5f);
        g.setColour (abyss.withAlpha (0.9f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (slider.isMouseOverOrDragging() ? accent.withAlpha (0.6f) : outline);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        return;
    }
    LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, pos, minPos, maxPos, style, slider);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (slider.getSliderStyle() == juce::Slider::LinearBarVertical ? 15.0f : 11.0f, true));
    label->setColour (juce::Label::textColourId, text);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::TextEditor::backgroundColourId, abyss);
    label->setColour (juce::TextEditor::textColourId, highlight);
    return label;
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = juce::jmin (8.0f, r.getHeight() * 0.5f);
    const bool on = button.getToggleState();

    if (on)
    {
        g.setColour (accent.withAlpha (0.12f));
        g.fillRoundedRectangle (r.expanded (2.0f), corner + 2.0f);
        g.setGradientFill (juce::ColourGradient (violet.withAlpha (0.85f), r.getX(), r.getY(), deep, r.getX(), r.getBottom(), false));
    }
    else
    {
        g.setColour (abyss.withAlpha (down ? 1.0f : 0.85f));
    }
    g.fillRoundedRectangle (r, corner);

    g.setColour (on ? accent.withAlpha (0.7f) : highlighted ? accent.withAlpha (0.45f) : outline);
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.getToggleState() ? highlight : highlighted ? text : textDim);
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return caption (juce::jmin (11.5f, (float) buttonHeight * 0.5f));
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
    g.setColour (abyss.withAlpha (0.9f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.isMouseOver (true) ? accent.withAlpha (0.5f) : outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (accent);
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (12.5f); }

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font LookAndFeel::getPopupMenuFont() { return font (14.0f); }

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (outline);
    g.drawRect (0, 0, width, height);
}

juce::Font LookAndFeel::getAlertWindowTitleFont() { return font (17.0f, true, 0.08f); }
juce::Font LookAndFeel::getAlertWindowMessageFont() { return font (14.0f); }
juce::Font LookAndFeel::getAlertWindowFont() { return font (13.0f); }

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    g.setColour (abyss.withAlpha (0.95f));
    g.fillRoundedRectangle (juce::Rectangle<int> (width, height).toFloat(), 6.0f);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.hasKeyboardFocus (true) ? accent.withAlpha (0.6f) : outline);
    g.drawRoundedRectangle (juce::Rectangle<int> (width, height).toFloat().reduced (0.5f), 6.0f, 1.0f);
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    juce::Rectangle<int> thumb = vertical ? juce::Rectangle<int> (x + width / 2 - 2, thumbStart, 4, thumbSize)
                                          : juce::Rectangle<int> (thumbStart, y + height / 2 - 2, thumbSize, 4);
    g.setColour (violet.withAlpha (over || down ? 0.9f : 0.5f));
    g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
}

void LookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool mouseOver, bool mouseDragging)
{
    g.setColour (accent.withAlpha (mouseOver || mouseDragging ? 0.8f : 0.35f));
    for (float i = 0.3f; i < 1.0f; i += 0.3f)
        g.drawLine ((float) w * i, (float) h, (float) w, (float) h * i, 1.2f);
}
} // namespace rs::theme
