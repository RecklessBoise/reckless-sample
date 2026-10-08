#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Parameters.h"

namespace rs
{
/** The eight chop pads. Each lights up with its voice's envelope. */
class PadGrid : public juce::Component
{
public:
    PadGrid();

    std::function<void (int chop, bool down)> onPad;

    void setLevels (const std::array<float, numChops>& levels, const std::array<bool, numChops>& latched);

    /** Pad centres as a fraction of the given width, for the background animation. */
    std::array<float, numChops> padCentres (const juce::Component& relativeTo) const;

    void resized() override;

private:
    class Pad : public juce::Component
    {
    public:
        Pad (PadGrid& owner, int index);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        float level = 0.0f, glow = 0.0f;
        bool latched = false, pressed = false;

    private:
        PadGrid& owner;
        int index;
    };

    std::vector<std::unique_ptr<Pad>> pads;
};
} // namespace rs
