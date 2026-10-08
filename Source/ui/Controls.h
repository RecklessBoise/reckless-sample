#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace rs
{
/** Rotary knob with its name underneath; shows the value while hovered or dragged. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& labelText)
        : name (labelText.toUpperCase())
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.setMouseDragSensitivity (180);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        slider.setDoubleClickReturnValue (true, (double) state.getParameterRange (paramId).convertFrom0to1 (state.getParameter (paramId)->getDefaultValue()));
    }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedBottom (16)); }

    void paint (juce::Graphics& g) override
    {
        const bool showValue = slider.isMouseOverOrDragging();
        g.setColour (showValue ? theme::ice : theme::textDim);
        g.setFont (showValue ? theme::font (11.0f, true) : theme::caption (9.5f));
        g.drawText (showValue ? slider.getTextFromValue (slider.getValue()) : name,
                    getLocalBounds().removeFromBottom (16), juce::Justification::centred, false);
    }

    juce::Slider slider;

private:
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Pill toggle bound to a bool parameter. */
class Toggle : public juce::TextButton
{
public:
    Toggle (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text)
        : juce::TextButton (text)
    {
        setClickingTogglesState (true);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, *this);
    }

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

/** Row of mutually exclusive buttons bound to a choice parameter. */
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::StringArray& labels)
    {
        auto* param = state.getParameter (paramId);
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (labels[i]));
            b->setRadioGroupId (1);
            b->onClick = [this, i] { attachment->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
        {
            const int index = juce::roundToInt (v);
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setToggleState (i == index, juce::dontSendNotification);
            if (onChange)
                onChange (index);
        });
        attachment->sendInitialUpdate();
    }

    std::function<void (int)> onChange;

    void resized() override
    {
        auto r = getLocalBounds();
        const int w = r.getWidth() / juce::jmax (1, buttons.size());
        for (auto* b : buttons)
            b->setBounds (r.removeFromLeft (w).reduced (1, 0));
    }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

/** Number box for BPM values: drag vertically or double-click to type. */
class NumberBox : public juce::Slider
{
public:
    NumberBox (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
    {
        setSliderStyle (juce::Slider::LinearBarVertical);
        setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 28);
        setNumDecimalPlacesToDisplay (2);
        setTextValueSuffix ("");
        setMouseDragSensitivity (400);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, *this);
        textFromValueFunction = [] (double v) { return juce::String (v, 2); };
        updateText();
    }

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
} // namespace rs
