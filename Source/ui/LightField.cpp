#include "LightField.h"
#include "Theme.h"

namespace rs
{
namespace
{
    constexpr int pixelScale = 2; // the field renders at half resolution; upscaling adds the photo's soft blur
    constexpr int cell = 8;       // coarse grid for the light field, interpolated per pixel

    juce::PixelARGB mix (juce::Colour a, juce::Colour b, float t)
    {
        const auto c = a.interpolatedWith (b, t);
        return juce::PixelARGB (255, c.getRed(), c.getGreen(), c.getBlue());
    }

    float gaussian (float dx, float dy, float rx, float ry)
    {
        return std::exp (-(dx * dx) / (rx * rx) - (dy * dy) / (ry * ry));
    }
} // namespace

LightField::LightField()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (true);

    const std::pair<float, juce::Colour> stops[] { { 0.0f, theme::black }, { 0.22f, theme::abyss }, { 0.42f, theme::deepTeal },
                                                   { 0.65f, theme::teal }, { 0.85f, theme::cyan }, { 1.0f, theme::ice } };
    for (int i = 0; i < 256; ++i)
    {
        const float v = (float) i / 255.0f;
        size_t s = 0;
        while (s + 2 < std::size (stops) && v > stops[s + 1].first)
            ++s;
        const float t = (v - stops[s].first) / (stops[s + 1].first - stops[s].first);
        palette[(size_t) i] = mix (stops[s].second, stops[s + 1].second, juce::jlimit (0.0f, 1.0f, t));
    }
}

void LightField::resized()
{
    const int w = juce::jmax (1, getWidth() / pixelScale), h = juce::jmax (1, getHeight() / pixelScale);
    frame = juce::Image (juce::Image::ARGB, w, h, true);
    fieldW = w / cell + 2;
    fieldH = h / cell + 2;
    field.assign ((size_t) (fieldW * fieldH), 0.0f);
    renderFrame();
}

void LightField::update (float outputLevel, const std::array<float, numChops>& padLevels, const std::array<float, numChops>& padX)
{
    time += 1.0f / 30.0f;
    level += (juce::jlimit (0.0f, 1.2f, outputLevel) - level) * (outputLevel > level ? 0.5f : 0.08f);

    for (size_t i = 0; i < flares.size(); ++i)
    {
        auto& f = flares[i];
        const float pad = padLevels[i];
        if (pad > 0.05f && previousPad[i] <= 0.05f)
        {
            // New hit: a fresh flare and a scan line sweeping across the dots.
            f.energy = 1.25f;
            f.x = padX[i];
            f.y = 0.45f + random.nextFloat() * 0.25f;
            scan = 0.0f;
        }
        f.energy = juce::jmax (pad * 0.9f, f.energy * 0.93f);
        f.x += (padX[i] - f.x) * 0.05f + 0.002f * std::sin (time * 1.7f + (float) i);
        previousPad[i] = pad;
    }

    if (scan >= 0.0f)
    {
        scan += 0.035f;
        if (scan > 1.2f)
            scan = -1.0f;
    }

    renderFrame();
    repaint();
}

void LightField::renderFrame()
{
    if (! frame.isValid())
        return;

    const int w = frame.getWidth(), h = frame.getHeight();

    // 1) Coarse light field: drifting ambient light, pad flares and a dark silhouette.
    const float ambient = 0.32f + level * 0.45f;
    for (int gy = 0; gy < fieldH; ++gy)
    {
        for (int gx = 0; gx < fieldW; ++gx)
        {
            const float u = (float) (gx * cell) / (float) w;
            const float v = (float) (gy * cell) / (float) h;

            float light = 0.0f;
            light += ambient * gaussian (u - (0.22f + 0.08f * std::sin (time * 0.13f)), v - (0.38f + 0.1f * std::sin (time * 0.09f)), 0.32f, 0.2f);
            light += ambient * 0.9f * gaussian (u - (0.6f + 0.06f * std::sin (time * 0.11f + 1.0f)), v - (0.62f + 0.08f * std::cos (time * 0.1f)), 0.18f, 0.32f);
            light += ambient * 0.6f * gaussian (u - (0.08f + 0.05f * std::cos (time * 0.07f)), v - 0.08f, 0.2f, 0.14f);

            for (const auto& f : flares)
                if (f.energy > 0.01f)
                    light += f.energy * 0.85f * gaussian (u - f.x, v - f.y, 0.1f + f.energy * 0.12f, 0.16f + f.energy * 0.1f);

            // The dark shape on the right of the photo, slowly breathing.
            const float shadow = gaussian (u - (0.82f + 0.03f * std::sin (time * 0.2f)), v - 0.42f, 0.2f, 0.45f);
            light *= 1.0f - 0.85f * shadow;

            // Vignette.
            const float du = u - 0.45f, dv = v - 0.5f;
            light *= juce::jlimit (0.0f, 1.0f, 1.15f - 1.1f * (du * du + dv * dv));

            field[(size_t) (gy * fieldW + gx)] = light;
        }
    }

    // 2) Per pixel: wavy rows of dots modulated by the field, plus the scan line and grain.
    const juce::Image::BitmapData bitmap (frame, juce::Image::BitmapData::writeOnly);
    const float waveAmp = 0.8f + level * 5.0f;
    const float scanX = scan * (float) w;
    juce::uint32 grain = (juce::uint32) (time * 1000.0f) * 2654435761u + 1u;

    std::vector<float> wave ((size_t) w);
    for (int x = 0; x < w; ++x)
        wave[(size_t) x] = std::sin ((float) x * 0.031f + time * 1.4f) * waveAmp + std::sin ((float) x * 0.011f - time * 0.6f) * waveAmp * 0.6f;

    for (int y = 0; y < h; ++y)
    {
        auto* line = bitmap.getLinePointer (y);
        for (int x = 0; x < w; ++x)
        {
            const float yy = (float) y + wave[(size_t) x];

            const float fx = (float) x / (float) cell, fy = juce::jlimit (0.0f, (float) (fieldH - 2), yy / (float) cell);
            const int ix = juce::jmin ((int) fx, fieldW - 2), iy = (int) fy;
            const float tx = fx - (float) ix, ty = fy - (float) iy;
            const float* row0 = field.data() + iy * fieldW + ix;
            const float* row1 = row0 + fieldW;
            const float light = (row0[0] + (row0[1] - row0[0]) * tx) * (1.0f - ty) + (row1[0] + (row1[1] - row1[0]) * tx) * ty;

            // Rows of dots: bright every 3 pixels, dots offset on alternate rows.
            const float rowPhase = yy / 3.0f;
            const float rowMask = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::twoPi * rowPhase);
            const int rowIndex = (int) std::floor (rowPhase + 0.5f);
            const float colMask = 0.62f + 0.38f * std::cos (juce::MathConstants<float>::twoPi * ((float) x / 2.6f + (rowIndex & 1) * 0.5f));
            float value = light * (0.12f + 1.05f * rowMask * rowMask * colMask);

            if (scan >= 0.0f)
            {
                const float d = ((float) x - scanX) / 18.0f;
                value += 0.55f * std::exp (-d * d) * rowMask * juce::jmin (1.0f, light * 2.5f + 0.15f);
            }

            grain ^= grain << 13;
            grain ^= grain >> 17;
            grain ^= grain << 5;
            value += ((float) (grain & 0xff) / 255.0f - 0.5f) * 0.035f;

            const auto& c = palette[(size_t) juce::jlimit (0, 255, (int) (value * 255.0f))];
            auto* px = line + x * bitmap.pixelStride;
            reinterpret_cast<juce::PixelARGB*> (px)->set (c);
        }
    }
}

void LightField::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (frame, getLocalBounds().toFloat());
}
} // namespace rs
