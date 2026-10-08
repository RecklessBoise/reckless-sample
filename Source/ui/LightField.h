#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Parameters.h"

namespace rs
{
/**
    Animated background in the style of the reference photo: rows of light dots
    seen through a blurred lens, with drifting cyan light. Playing a chop sends a
    flare across the field, the rows ripple with the output level, and a bright
    scan line sweeps through on each new hit.
*/
class LightField : public juce::Component
{
public:
    LightField();

    /** Called every frame with the current levels (0..1). padX is each pad's centre, 0..1 across the width. */
    void update (float outputLevel, const std::array<float, numChops>& padLevels, const std::array<float, numChops>& padX);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void renderFrame();

    struct Flare
    {
        float x = 0.5f, y = 0.5f, energy = 0.0f;
    };

    juce::Image frame;
    std::array<juce::PixelARGB, 256> palette;
    std::vector<float> field;
    int fieldW = 0, fieldH = 0;

    std::array<Flare, numChops> flares;
    std::array<float, numChops> previousPad {};
    float level = 0.0f, scan = -1.0f, time = 0.0f;
    juce::Random random { 42 };
};
} // namespace rs
