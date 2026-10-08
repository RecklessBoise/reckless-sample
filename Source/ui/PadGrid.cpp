#include "PadGrid.h"
#include "Theme.h"

namespace rs
{
PadGrid::PadGrid()
{
    for (int i = 0; i < numChops; ++i)
    {
        pads.push_back (std::make_unique<Pad> (*this, i));
        addAndMakeVisible (*pads.back());
    }
}

void PadGrid::setLevels (const std::array<float, numChops>& levels, const std::array<bool, numChops>& latched)
{
    for (size_t i = 0; i < pads.size(); ++i)
    {
        auto& p = *pads[i];
        // Fast attack, slower fade so short hits stay visible.
        p.glow = juce::jmax (levels[i], p.glow * 0.86f);
        if (std::abs (p.level - levels[i]) > 0.002f || p.latched != latched[i] || p.glow > 0.003f)
        {
            p.level = levels[i];
            p.latched = latched[i];
            p.repaint();
        }
    }
}

std::array<float, numChops> PadGrid::padCentres (const juce::Component& relativeTo) const
{
    std::array<float, numChops> out {};
    for (size_t i = 0; i < pads.size(); ++i)
    {
        const auto centre = relativeTo.getLocalPoint (pads[i].get(), pads[i]->getLocalBounds().getCentre());
        out[i] = (float) centre.x / (float) juce::jmax (1, relativeTo.getWidth());
    }
    return out;
}

void PadGrid::resized()
{
    auto area = getLocalBounds();
    const int gap = 10;
    const int width = (area.getWidth() - gap * (numChops - 1)) / numChops;
    for (auto& p : pads)
    {
        p->setBounds (area.removeFromLeft (width));
        area.removeFromLeft (gap);
    }
}

//==============================================================================
PadGrid::Pad::Pad (PadGrid& o, int i) : owner (o), index (i)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PadGrid::Pad::paint (juce::Graphics& g)
{
    static const char* keys = "ASDFGHJK";
    const auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float lit = juce::jlimit (0.0f, 1.0f, glow);

    // Halo.
    if (lit > 0.01f)
    {
        g.setColour (theme::cyan.withAlpha (0.18f * lit));
        g.fillRoundedRectangle (r.expanded (3.0f), 12.0f);
    }

    // Glass body; the light rises from the bottom like the photo's lit dots.
    g.setGradientFill (juce::ColourGradient (theme::abyss.withAlpha (0.92f), r.getX(), r.getY(),
                                             theme::deepTeal.interpolatedWith (theme::cyan, lit * 0.6f).withAlpha (0.92f),
                                             r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.0f);

    // Dotted rows inside the pad brighten with the level.
    g.saveState();
    g.reduceClipRegion (r.reduced (6.0f).toNearestInt());
    for (float y = r.getY() + 8.0f; y < r.getBottom() - 4.0f; y += 5.0f)
    {
        const float rowLight = 0.08f + lit * 0.7f * (y - r.getY()) / r.getHeight();
        g.setColour (theme::ice.withAlpha (rowLight * 0.35f));
        for (float x = r.getX() + 6.0f + std::fmod (y, 2.0f); x < r.getRight() - 4.0f; x += 4.0f)
            g.fillRect (x, y, 1.6f, 1.2f);
    }
    g.restoreState();

    g.setColour (pressed || lit > 0.05f ? theme::cyan.withAlpha (0.5f + 0.5f * lit) : theme::outline);
    g.drawRoundedRectangle (r, 10.0f, pressed ? 1.6f : 1.0f);

    g.setColour (lit > 0.3f ? theme::ice : theme::text);
    g.setFont (theme::font (22.0f, true));
    g.drawText (juce::String (index + 1), r.reduced (12.0f, 8.0f), juce::Justification::topLeft);

    g.setColour (theme::textDim.interpolatedWith (theme::ice, lit));
    g.setFont (theme::caption (11.0f));
    g.drawText (juce::String::charToString ((juce::juce_wchar) keys[index]), r.reduced (12.0f, 10.0f), juce::Justification::bottomRight);

    if (latched)
    {
        g.setColour (theme::ice);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getRight() - 14.0f, r.getY() + 14.0f }));
    }
}

void PadGrid::Pad::mouseDown (const juce::MouseEvent&)
{
    pressed = true;
    repaint();
    if (owner.onPad)
        owner.onPad (index, true);
}

void PadGrid::Pad::mouseUp (const juce::MouseEvent&)
{
    pressed = false;
    repaint();
    if (owner.onPad)
        owner.onPad (index, false);
}
} // namespace rs
