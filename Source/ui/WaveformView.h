#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/SampleData.h"

namespace rs
{
/** Shows the sample with its chop markers (draggable) and a playhead per sounding chop. */
class WaveformView : public juce::Component
{
public:
    std::function<void (const ChopStarts&)> onChopsChanged;
    std::function<void (int chop, bool down)> onChopClicked;

    void setSample (SampleData::Ptr sample, const ChopStarts& chops);
    void setChops (const ChopStarts& chops);

    /** Per-frame update: normalised position within each chop (-1 when silent), level, and the fraction of each chop that is heard. */
    void setPlayback (const std::array<float, numChops>& positions, const std::array<float, numChops>& levels,
                      const std::array<float, numChops>& audibleFractions);

    void setRendering (bool isRendering);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void rebuildPeaks();
    int markerAt (float x) const;
    float chopEnd (int chop) const { return chop + 1 < numChops ? chops[(size_t) chop + 1] : 1.0f; }
    juce::Rectangle<float> waveArea() const { return getLocalBounds().toFloat().reduced (12.0f, 26.0f).withTrimmedBottom (-10.0f); }

    SampleData::Ptr sample;
    ChopStarts chops = equalChops();
    std::vector<std::pair<float, float>> peaks;
    std::array<float, numChops> positions {}, levels {};
    std::array<float, numChops> audible { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    bool rendering = false;
    int dragging = -1, hover = -1, clickedChop = -1;
};
} // namespace rs
