#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rs::theme
{
// Palette taken from the reference photo: near-black, deep teal, cyan light, icy white.
inline const juce::Colour black     { 0xff020405 };
inline const juce::Colour abyss     { 0xff061014 };
inline const juce::Colour deepTeal  { 0xff0b2a30 };
inline const juce::Colour teal      { 0xff2a7482 };
inline const juce::Colour cyan      { 0xff73cfdd };
inline const juce::Colour ice       { 0xffdcf7ff };
inline const juce::Colour textDim   { 0xff5d8a93 };
inline const juce::Colour text      { 0xffc9ecf3 };
inline const juce::Colour panel     { 0xa8050b0d };
inline const juce::Colour outline   { 0x2a8fe3f0 };
inline const juce::Colour heart     { 0xffe8f9ff };

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
