#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Parameters.h"

namespace rs
{
/**
    Animated background in the style of the reference photo: a filmed CRT/VHS
    screen with vertical RGB phosphor lines, grey-teal light, a magenta band at
    the bottom, a violet block glowing in the dark top-right corner.

    It reacts to playback: each hit flashes a coloured flare above its pad and
    tears a band of lines sideways (VHS glitch), the colour channels drift
    apart with the output level, and a tracking band rolls down the screen.
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
    void renderField();
    void renderFrame();

    struct Flare
    {
        float x = 0.5f, y = 0.6f, energy = 0.0f;
        juce::Colour colour;
    };

    struct Tear
    {
        float y = 0.0f, height = 0.0f, offset = 0.0f;
        int frames = 0;
    };

    juce::Image frame;
    std::array<std::vector<float>, 3> field; // coarse light, one grid per colour channel
    int fieldW = 0, fieldH = 0;

    std::array<Flare, numChops> flares;
    std::array<float, numChops> previousPad {};
    std::array<Tear, 3> tears;
    float level = 0.0f, time = 0.0f, tracking = 0.0f, hitFlash = 0.0f;
    juce::Random random { 42 };
};
} // namespace rs
