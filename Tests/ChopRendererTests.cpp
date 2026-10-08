#include "dsp/ChopRenderer.h"
#include "dsp/FactorySamples.h"
#include "dsp/SamplerEngine.h"
#include <juce_dsp/juce_dsp.h>

namespace rs
{
namespace
{
    /** Frequency of the strongest FFT bin, refined with parabolic interpolation. */
    double dominantFrequency (const juce::AudioBuffer<float>& audio, double sampleRate)
    {
        constexpr int order = 14, size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data (size * 2, 0.0f);
        const int offset = juce::jmax (0, audio.getNumSamples() / 2 - size / 2);
        for (int i = 0; i < size && offset + i < audio.getNumSamples(); ++i)
            data[(size_t) i] = audio.getSample (0, offset + i) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));
        fft.performFrequencyOnlyForwardTransform (data.data());
        int best = 1;
        for (int b = 1; b < size / 2 - 1; ++b)
            if (data[(size_t) b] > data[(size_t) best])
                best = b;
        const double a = data[(size_t) best - 1], m = data[(size_t) best], c = data[(size_t) best + 1];
        const double shift = 0.5 * (a - c) / (a - 2.0 * m + c);
        return (best + shift) * sampleRate / size;
    }

    juce::AudioBuffer<float> sine (double freq, double seconds, double sr)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * sr));
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / sr);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }
} // namespace

class ChopRendererTests : public juce::UnitTest
{
public:
    ChopRendererTests() : juce::UnitTest ("Chop renderer", "RecklessSample") {}

    void runTest() override
    {
        constexpr double sr = 44100.0;
        const auto source = sine (440.0, 2.0, sr);

        beginTest ("Pitch up with keep speed keeps the duration");
        {
            RenderParams p;
            p.outputSampleRate = sr;
            p.pitchSemitones = 12.0f;
            p.keepSpeed = true;
            const auto out = renderRegion (source, 0, source.getNumSamples(), sr, p);
            expectEquals (out.getNumSamples(), source.getNumSamples());
            expectWithinAbsoluteError (dominantFrequency (out, sr), 880.0, 8.0);
        }

        beginTest ("Varispeed pitch changes the duration");
        {
            RenderParams p;
            p.outputSampleRate = sr;
            p.pitchSemitones = 12.0f;
            p.keepSpeed = false;
            const auto out = renderRegion (source, 0, source.getNumSamples(), sr, p);
            expectWithinAbsoluteError (out.getNumSamples(), source.getNumSamples() / 2, 2);
            expectWithinAbsoluteError (dominantFrequency (out, sr), 880.0, 8.0);
        }

        beginTest ("Tempo sync stretches without changing pitch");
        {
            RenderParams p;
            p.outputSampleRate = sr;
            p.syncMode = SyncMode::manual;
            p.sampleBpm = 90.0;
            p.targetBpm = 120.0;
            const auto out = renderRegion (source, 0, source.getNumSamples(), sr, p);
            expectWithinAbsoluteError (out.getNumSamples(), (int) (source.getNumSamples() * 90.0 / 120.0), 2);
            expectWithinAbsoluteError (dominantFrequency (out, sr), 440.0, 6.0);
        }

        beginTest ("Sample-rate conversion keeps pitch");
        {
            RenderParams p;
            p.outputSampleRate = 48000.0;
            const auto out = renderRegion (source, 0, source.getNumSamples(), sr, p);
            expectWithinAbsoluteError (out.getNumSamples(), (int) (source.getNumSamples() * 48000.0 / sr), 2);
            expectWithinAbsoluteError (dominantFrequency (out, 48000.0), 440.0, 6.0);
        }

        beginTest ("All factory samples render eight chops");
        for (int i = 0; i < getNumFactorySamples(); ++i)
        {
            RenderParams p;
            p.sample = createFactorySample (i);
            p.outputSampleRate = 48000.0;
            p.pitchSemitones = 3.0f;
            auto set = renderChops (p);
            expect (set != nullptr);
            for (const auto& chop : set->chops)
            {
                expect (chop.getNumSamples() > 0);
                expect (chop.getMagnitude (0, chop.getNumSamples()) < 4.0f);
            }
        }

        beginTest ("Engine: tap plays once, hold loops");
        {
            RenderParams p;
            p.sample = makeSampleData (sine (220.0, 0.8, sr), sr, "sine");
            p.outputSampleRate = sr;
            auto set = renderChops (p);

            SamplerEngine engine;
            engine.prepare (sr);
            engine.setRenderSet (set);
            VoiceSettings vs;
            const int chopLength = set->chops[0].getNumSamples();
            juce::AudioBuffer<float> out (2, chopLength * 3);

            out.clear();
            engine.noteOn (0, 1.0f, vs);
            engine.noteOff (0, vs);
            engine.render (out, 0, out.getNumSamples(), vs);
            expect (out.getMagnitude (0, chopLength / 2, chopLength / 2) > 0.1f, "tap plays");
            expect (out.getMagnitude (0, chopLength + 64, chopLength) < 1.0e-4f, "tap stops after one pass");

            out.clear();
            engine.noteOn (0, 1.0f, vs);
            engine.render (out, 0, out.getNumSamples(), vs);
            expect (out.getMagnitude (0, chopLength * 2, chopLength / 2) > 0.1f, "hold loops");
            engine.allNotesOff();
        }
    }
};

static ChopRendererTests chopRendererTests;
} // namespace rs
