#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rs::theme
{
// Palette taken from the reference photo: a CRT/VHS screen with vertical RGB lines,
// grey-teal glow, a magenta band and a bright violet block in the dark.
inline const juce::Colour black     { 0xff07050b };
inline const juce::Colour abyss     { 0xff120c1a };
inline const juce::Colour deep      { 0xff2a1838 };
inline const juce::Colour violet    { 0xff6b3fd6 };
inline const juce::Colour accent    { 0xffe25fa8 };
inline const juce::Colour phosphor  { 0xff7ea4a8 };
inline const juce::Colour highlight { 0xfff4eaff };
inline const juce::Colour textDim   { 0xff8c8099 };
inline const juce::Colour text      { 0xffe4dcef };
inline const juce::Colour panel     { 0xa80a0710 };
inline const juce::Colour outline   { 0x30c9a7ff };
inline const juce::Colour heart     { 0xffff7cc0 };

constexpr int baseWidth = 1000;
constexpr int baseHeight = 640;

juce::Font font (float height, bool bold = false, float kerning = 0.0f);

/** Small uppercase caption with wide letter spacing. */
juce::Font caption (float height = 10.5f);

void drawPanel (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title = {});

/** Draws a soft glow by layering strokes. */
void strokeGlow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width, float glowWidth);

juce::Path heartPath (juce::Rectangle<float> area);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool mouseOver, bool mouseDragging) override;
};
} // namespace rs::theme
