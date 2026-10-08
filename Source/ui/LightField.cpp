#include "LightField.h"
#include "Theme.h"

namespace rs
{
namespace
{
    constexpr int pixelScale = 2; // renders at half resolution; upscaling adds the soft blur of the photo
    constexpr int cell = 8;       // coarse grid for the light, interpolated per pixel

    struct Rgb { float r, g, b; };

    Rgb rgb (juce::Colour c) { return { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue() }; }

    float gaussian (float dx, float dy, float rx, float ry)
    {
        return std::exp (-(dx * dx) / (rx * rx) - (dy * dy) / (ry * ry));
    }

    float smoothstep (float e0, float e1, float x)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0));
        return t * t * (3.0f - 2.0f * t);
    }
} // namespace

LightField::LightField()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (true);

    const juce::Colour flareColours[] { theme::accent, theme::phosphor, theme::violet };
    for (size_t i = 0; i < flares.size(); ++i)
        flares[i].colour = flareColours[i % 3];
}

void LightField::resized()
{
    const int w = juce::jmax (1, getWidth() / pixelScale), h = juce::jmax (1, getHeight() / pixelScale);
    frame = juce::Image (juce::Image::ARGB, w, h, true);
    fieldW = w / cell + 2;
    fieldH = h / cell + 2;
    for (auto& f : field)
        f.assign ((size_t) (fieldW * fieldH), 0.0f);
    renderFrame();
}

void LightField::update (float outputLevel, const std::array<float, numChops>& padLevels, const std::array<float, numChops>& padX)
{
    time += 1.0f / 30.0f;
    level += (juce::jlimit (0.0f, 1.2f, outputLevel) - level) * (outputLevel > level ? 0.5f : 0.08f);
    hitFlash *= 0.85f;

    for (size_t i = 0; i < flares.size(); ++i)
    {
        auto& f = flares[i];
        const float pad = padLevels[i];
        if (pad > 0.05f && previousPad[i] <= 0.05f)
        {
            // New hit: a coloured flare, a flash, and a VHS tear somewhere on screen.
            f.energy = 1.3f;
            f.x = padX[i];
            f.y = 0.5f + random.nextFloat() * 0.3f;
            hitFlash = 1.0f;

            auto& tear = *std::min_element (tears.begin(), tears.end(), [] (auto& a, auto& b) { return a.frames < b.frames; });
            tear.y = random.nextFloat();
            tear.height = 0.03f + random.nextFloat() * 0.12f;
            tear.offset = (random.nextBool() ? 1.0f : -1.0f) * (6.0f + random.nextFloat() * 22.0f);
            tear.frames = 4 + random.nextInt (6);
        }
        f.energy = juce::jmax (pad * 0.9f, f.energy * 0.92f);
        f.x += (padX[i] - f.x) * 0.05f;
        previousPad[i] = pad;
    }

    for (auto& t : tears)
    {
        if (t.frames > 0)
        {
            --t.frames;
            t.offset *= -0.7f; // jitter back and forth while it settles
        }
    }

    // The tracking band rolls faster when the sound is loud.
    tracking += 0.004f + level * 0.02f;
    tracking -= std::floor (tracking);

    renderFrame();
    repaint();
}

void LightField::renderField()
{
    const Rgb grey = rgb (theme::phosphor);
    const Rgb pink = rgb (theme::accent);
    const Rgb purple = rgb (theme::violet);
    const float ambient = 0.5f + level * 0.35f + hitFlash * 0.12f;

    for (int gy = 0; gy < fieldH; ++gy)
    {
        for (int gx = 0; gx < fieldW; ++gx)
        {
            const float u = (float) gx / (float) (fieldW - 2);
            const float v = (float) gy / (float) (fieldH - 2);
            Rgb c { 0.0f, 0.0f, 0.0f };
            auto add = [&] (const Rgb& col, float amount)
            {
                c.r += col.r * amount;
                c.g += col.g * amount;
                c.b += col.b * amount;
            };

            // Grey-teal screen light, brightest towards the upper middle.
            add (grey, ambient * (0.55f + 0.45f * gaussian (u - (0.38f + 0.05f * std::sin (time * 0.15f)), v - 0.35f, 0.55f, 0.5f)));

            // Magenta band across the bottom, swelling with the level.
            const float bandCentre = 0.78f + 0.03f * std::sin (time * 0.4f);
            add (pink, (0.45f + level * 0.6f) * gaussian (0.0f, v - bandCentre, 1.0f, 0.13f + level * 0.05f)
                           * (0.7f + 0.3f * gaussian (u - 0.3f, 0.0f, 0.5f, 1.0f)));

            // Flares above the pads that were just hit.
            for (const auto& f : flares)
                if (f.energy > 0.01f)
                    add (rgb (f.colour), f.energy * 0.9f * gaussian (u - f.x, v - f.y, 0.07f + f.energy * 0.08f, 0.22f + f.energy * 0.1f));

            // Dark shape on the right with the violet block inside it.
            const float edge = 0.8f + 0.05f * std::sin (v * 5.0f + 0.6f) + 0.04f * smoothstep (0.35f, 0.7f, v);
            const float shadow = smoothstep (edge - 0.03f, edge + 0.02f, u);
            c.r *= 1.0f - 0.92f * shadow;
            c.g *= 1.0f - 0.92f * shadow;
            c.b *= 1.0f - 0.92f * shadow;
            const float block = smoothstep (0.9f, 0.94f, u) * (1.0f - smoothstep (0.3f, 0.38f, v));
            add (purple, block * (0.85f + 0.09f * std::sin (time * 1.3f) + level * 0.5f + hitFlash * 0.4f));

            // Vignette.
            const float du = u - 0.45f, dv = v - 0.5f;
            const float vignette = juce::jlimit (0.0f, 1.0f, 1.1f - 0.9f * (du * du + dv * dv));

            const size_t idx = (size_t) (gy * fieldW + gx);
            field[0][idx] = c.r * vignette;
            field[1][idx] = c.g * vignette;
            field[2][idx] = c.b * vignette;
        }
    }
}

void LightField::renderFrame()
{
    if (! frame.isValid())
        return;

    renderField();

    const int w = frame.getWidth(), h = frame.getHeight();
    const juce::Image::BitmapData bitmap (frame, juce::Image::BitmapData::writeOnly);

    // Chromatic aberration: red and blue drift apart with the level and on hits.
    const float shift = 0.6f + level * 2.5f + hitFlash * 2.0f;
    const float trackingY = tracking * (float) h;
    juce::uint32 grain = (juce::uint32) (time * 1000.0f) * 2654435761u + 1u;

    // Vertical phosphor triads (R, G, B columns) like the photo.
    constexpr float triad[3][3] { { 1.15f, 0.55f, 0.7f }, { 0.6f, 1.15f, 0.75f }, { 0.7f, 0.6f, 1.2f } };

    const auto sample = [&] (const std::vector<float>& f, float x, float y)
    {
        const float fx = juce::jlimit (0.0f, (float) (fieldW - 2) - 0.001f, x / (float) cell);
        const float fy = juce::jlimit (0.0f, (float) (fieldH - 2) - 0.001f, y / (float) cell);
        const int ix = (int) fx, iy = (int) fy;
        const float tx = fx - (float) ix, ty = fy - (float) iy;
        const float* r0 = f.data() + iy * fieldW + ix;
        const float* r1 = r0 + fieldW;
        return (r0[0] + (r0[1] - r0[0]) * tx) * (1.0f - ty) + (r1[0] + (r1[1] - r1[0]) * tx) * ty;
    };

    for (int y = 0; y < h; ++y)
    {
        // Horizontal displacement: slow wobble, plus the tears triggered by hits.
        float offset = std::sin ((float) y * 0.05f + time * 2.0f) * (0.3f + level * 1.2f);
        for (const auto& t : tears)
            if (t.frames > 0 && std::abs ((float) y / (float) h - t.y) < t.height)
                offset += t.offset;

        // Rolling tracking band: brighter, noisier, shifted lines.
        float bandDist = std::abs ((float) y - trackingY);
        bandDist = juce::jmin (bandDist, (float) h - bandDist);
        const float band = std::exp (-bandDist * bandDist / (60.0f + level * 200.0f));
        offset += band * (3.0f + level * 8.0f);

        const float scan = (y & 1) != 0 ? 0.82f : 1.0f; // faint horizontal scanlines
        auto* line = bitmap.getLinePointer (y);

        for (int x = 0; x < w; ++x)
        {
            const float xs = (float) x + offset;
            const float* t = triad[x % 3];

            grain ^= grain << 13;
            grain ^= grain >> 17;
            grain ^= grain << 5;
            const float noise = ((float) (grain & 0xff) / 255.0f - 0.5f) * (0.09f + band * 0.35f);

            const float r = (sample (field[0], xs + shift, (float) y) * t[0] + noise + band * 0.08f) * scan;
            const float g = (sample (field[1], xs, (float) y) * t[1] + noise + band * 0.08f) * scan;
            const float b = (sample (field[2], xs - shift, (float) y) * t[2] + noise + band * 0.1f) * scan;

            auto* px = reinterpret_cast<juce::PixelARGB*> (line + x * bitmap.pixelStride);
            px->setARGB (255,
                         (juce::uint8) juce::jlimit (0, 255, (int) (r * 255.0f)),
                         (juce::uint8) juce::jlimit (0, 255, (int) (g * 255.0f)),
                         (juce::uint8) juce::jlimit (0, 255, (int) (b * 255.0f)));
        }
    }
}

void LightField::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (frame, getLocalBounds().toFloat());
}
} // namespace rs
