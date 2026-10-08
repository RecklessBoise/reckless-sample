#include "WaveformView.h"
#include "Theme.h"

namespace rs
{
void WaveformView::setSample (SampleData::Ptr newSample, const ChopStarts& newChops)
{
    sample = std::move (newSample);
    chops = newChops;
    rebuildPeaks();
    repaint();
}

void WaveformView::setChops (const ChopStarts& newChops)
{
    if (dragging >= 0 || newChops == chops)
        return;
    chops = newChops;
    repaint();
}

void WaveformView::setPlayback (const std::array<float, numChops>& newPositions, const std::array<float, numChops>& newLevels,
                                const std::array<float, numChops>& audibleFractions)
{
    positions = newPositions;
    levels = newLevels;
    audible = audibleFractions;
    repaint();
}

void WaveformView::setRendering (bool isRendering)
{
    if (rendering != isRendering)
    {
        rendering = isRendering;
        repaint();
    }
}

void WaveformView::resized()
{
    rebuildPeaks();
}

void WaveformView::rebuildPeaks()
{
    peaks.clear();
    const int columns = (int) waveArea().getWidth();
    if (sample == nullptr || columns <= 0 || sample->getNumSamples() == 0)
        return;

    const int n = sample->getNumSamples();
    const auto* l = sample->audio.getReadPointer (0);
    const auto* r = sample->audio.getReadPointer (1);
    peaks.reserve ((size_t) columns);
    for (int c = 0; c < columns; ++c)
    {
        const int start = (int) ((juce::int64) c * n / columns);
        const int end = juce::jmax (start + 1, (int) ((juce::int64) (c + 1) * n / columns));
        float lo = 0.0f, hi = 0.0f;
        for (int i = start; i < end && i < n; ++i)
        {
            const float v = 0.5f * (l[i] + r[i]);
            lo = juce::jmin (lo, v);
            hi = juce::jmax (hi, v);
        }
        peaks.emplace_back (lo, hi);
    }
}

void WaveformView::paint (juce::Graphics& g)
{
    theme::drawPanel (g, getLocalBounds().toFloat());
    const auto area = waveArea();

    if (sample == nullptr || peaks.empty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::caption (12.0f));
        g.drawText ("GLISSE UN SAMPLE ICI", area, juce::Justification::centred);
        return;
    }

    const float midY = area.getCentreY();
    const float halfH = area.getHeight() * 0.48f;
    const auto xFor = [&] (float norm) { return area.getX() + norm * area.getWidth(); };

    // Active chop regions glow with their level.
    for (int i = 0; i < numChops; ++i)
    {
        const float x0 = xFor (chops[(size_t) i]), x1 = xFor (chopEnd (i));
        if (levels[(size_t) i] > 0.001f)
        {
            g.setGradientFill (juce::ColourGradient (theme::accent.withAlpha (0.22f * levels[(size_t) i]), x0, midY,
                                                     theme::accent.withAlpha (0.02f), x1, midY, false));
            g.fillRect (juce::Rectangle<float> (x0, area.getY(), x1 - x0, area.getHeight()));
        }
        // Dim the part of each chop cut off by the Length control.
        if (audible[(size_t) i] < 0.999f)
        {
            const float cut = x0 + (x1 - x0) * audible[(size_t) i];
            g.setColour (theme::black.withAlpha (0.45f));
            g.fillRect (juce::Rectangle<float> (cut, area.getY(), x1 - cut, area.getHeight()));
        }
    }

    // Waveform: soft glow underneath, crisp line on top.
    juce::Path wave;
    for (size_t c = 0; c < peaks.size(); ++c)
    {
        const float x = area.getX() + (float) c;
        wave.addLineSegment ({ x, midY - peaks[c].second * halfH, x, midY - peaks[c].first * halfH }, 1.0f);
    }
    g.setColour (theme::accent.withAlpha (0.12f));
    g.strokePath (wave, juce::PathStrokeType (3.0f));
    g.setGradientFill (juce::ColourGradient (theme::highlight, 0.0f, area.getY(), theme::violet, 0.0f, midY + halfH, false));
    g.fillPath (wave);

    // Chop markers with their number and key.
    static const char* keys = "ASDFGHJK";
    for (int i = 0; i < numChops; ++i)
    {
        const float x = xFor (chops[(size_t) i]);
        const bool active = dragging == i || hover == i;
        g.setColour (active ? theme::highlight : theme::accent.withAlpha (0.45f));
        g.drawLine (x, area.getY() - 4.0f, x, area.getBottom(), active ? 1.5f : 1.0f);

        juce::Path handle;
        handle.addTriangle (x, area.getY() - 4.0f, x + 7.0f, area.getY() - 12.0f, x, area.getY() - 18.0f);
        g.fillPath (handle);

        g.setFont (theme::caption (9.5f));
        g.setColour (levels[(size_t) i] > 0.01f ? theme::highlight : theme::textDim);
        g.drawText (juce::String (i + 1) + " " + juce::String::charToString ((juce::juce_wchar) keys[i]),
                    juce::Rectangle<float> (x + 9.0f, area.getY() - 20.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
    }

    // Playheads.
    for (int i = 0; i < numChops; ++i)
    {
        const float pos = positions[(size_t) i];
        if (pos < 0.0f)
            continue;
        const float x0 = xFor (chops[(size_t) i]), x1 = xFor (chopEnd (i));
        const float x = x0 + (x1 - x0) * pos;
        juce::Path head;
        head.startNewSubPath (x, area.getY());
        head.lineTo (x, area.getBottom());
        theme::strokeGlow (g, head, theme::highlight, 1.2f, 8.0f);
    }

    if (rendering)
    {
        g.setColour (theme::accent.withAlpha (0.8f));
        g.setFont (theme::caption (9.5f));
        g.drawText ("RENDERING", getLocalBounds().toFloat().reduced (14.0f, 6.0f).removeFromBottom (14.0f), juce::Justification::centredRight);
    }
}

int WaveformView::markerAt (float x) const
{
    const auto area = waveArea();
    int best = -1;
    float bestDist = 6.0f;
    for (int i = 1; i < numChops; ++i) // the first chop's start is draggable too, but prefer inner markers
    {
        const float d = std::abs (x - (area.getX() + chops[(size_t) i] * area.getWidth()));
        if (d < bestDist)
        {
            best = i;
            bestDist = d;
        }
    }
    if (best < 0 && std::abs (x - (area.getX() + chops[0] * area.getWidth())) < 6.0f)
        best = 0;
    return best;
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    const int m = sample != nullptr ? markerAt ((float) e.x) : -1;
    if (m != hover)
    {
        hover = m;
        setMouseCursor (m >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    if (sample == nullptr)
        return;
    dragging = markerAt ((float) e.x);
    if (dragging >= 0)
        return;

    // Clicking inside a chop plays it, like hitting its pad.
    const auto area = waveArea();
    const float norm = ((float) e.x - area.getX()) / area.getWidth();
    for (int i = numChops - 1; i >= 0; --i)
    {
        if (norm >= chops[(size_t) i])
        {
            clickedChop = i;
            if (onChopClicked)
                onChopClicked (i, true);
            break;
        }
    }
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    const auto area = waveArea();
    const float lo = dragging > 0 ? chops[(size_t) dragging - 1] + 0.002f : 0.0f;
    const float hi = dragging + 1 < numChops ? chops[(size_t) dragging + 1] - 0.002f : 0.998f;
    chops[(size_t) dragging] = juce::jlimit (lo, hi, ((float) e.x - area.getX()) / area.getWidth());
    repaint();
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0 && onChopsChanged)
        onChopsChanged (chops);
    dragging = -1;

    if (clickedChop >= 0 && onChopClicked)
        onChopClicked (clickedChop, false);
    clickedChop = -1;
}
} // namespace rs
