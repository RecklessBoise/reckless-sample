#include "ChopRenderer.h"

#include <signalsmith-stretch/signalsmith-stretch.h>

namespace rs
{
namespace
{
    bool nearlyEqual (double a, double b, double tolerance = 1.0e-4) { return std::abs (a - b) <= tolerance; }

    /** High-quality resampling; ratio is input samples consumed per output sample. */
    juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& in, double ratio)
    {
        const int outLength = juce::jmax (1, (int) std::floor (in.getNumSamples() / ratio));
        juce::AudioBuffer<float> out (in.getNumChannels(), outLength);

        // Pad with silence so the interpolator can read past the end safely.
        juce::AudioBuffer<float> padded (in.getNumChannels(), in.getNumSamples() + 64);
        padded.clear();
        for (int c = 0; c < in.getNumChannels(); ++c)
        {
            padded.copyFrom (c, 0, in, c, 0, in.getNumSamples());
            juce::WindowedSincInterpolator interp;
            interp.process (ratio, padded.getReadPointer (c), out.getWritePointer (c), outLength);
        }
        return out;
    }

    /** Time-stretch and transpose with Signalsmith Stretch, producing exactly outLength samples. */
    juce::AudioBuffer<float> stretch (const juce::AudioBuffer<float>& in, int outLength, double transpose, double sampleRate)
    {
        const int channels = in.getNumChannels();
        signalsmith::stretch::SignalsmithStretch<float> stretcher;
        stretcher.presetDefault (channels, (float) sampleRate);
        stretcher.setTransposeFactor ((float) transpose);

        // Short regions are padded with silence, then trimmed back. The stretcher needs
        // at least outputSeekLength(rate) input samples to align its output, and that
        // grows with the playback rate (shortening a chop by 4 needs ~4x more input).
        const int inLength = in.getNumSamples();
        const double rate = (double) juce::jmax (1, inLength) / (double) juce::jmax (1, outLength);
        const int needed = stretcher.outputSeekLength ((float) rate) + stretcher.intervalSamples();
        const int paddedIn = juce::jmax (inLength, stretcher.blockSamples() + stretcher.intervalSamples() * 2, needed);
        const int paddedOut = juce::jmax (outLength, (int) std::ceil ((double) paddedIn / rate));

        juce::AudioBuffer<float> input (channels, paddedIn);
        input.clear();
        for (int c = 0; c < channels; ++c)
            input.copyFrom (c, 0, in, c, 0, inLength);

        juce::AudioBuffer<float> output (channels, paddedOut);
        output.clear();
        if (! stretcher.exact (input.getArrayOfReadPointers(), paddedIn, output.getArrayOfWritePointers(), paddedOut))
        {
            // Should not happen with the padding above; never return uninitialised audio.
            // Fall back to plain resampling to the requested length.
            auto fallback = resample (in, rate);
            fallback.setSize (channels, outLength, true, true);
            return fallback;
        }

        output.setSize (channels, outLength, true);
        return output;
    }
} // namespace

bool RenderParams::operator== (const RenderParams& o) const
{
    return sample == o.sample && chopStarts == o.chopStarts && nearlyEqual (outputSampleRate, o.outputSampleRate)
        && nearlyEqual (pitchSemitones, o.pitchSemitones) && keepSpeed == o.keepSpeed && nearlyEqual (duration, o.duration)
        && syncMode == o.syncMode && nearlyEqual (sampleBpm, o.sampleBpm, 0.005)
        && (syncMode == SyncMode::off || nearlyEqual (targetBpm, o.targetBpm, 0.005)) && reverse == o.reverse;
}

bool RenderParams::sameSource (const RenderParams& o) const
{
    return sample == o.sample && chopStarts == o.chopStarts && nearlyEqual (outputSampleRate, o.outputSampleRate)
        && keepSpeed == o.keepSpeed && reverse == o.reverse;
}

double RenderParams::tempoFactor() const
{
    double factor = 1.0 / juce::jlimit (0.05f, 20.0f, duration);
    if (syncMode != SyncMode::off && sampleBpm > 0.0 && targetBpm > 0.0)
        factor *= targetBpm / sampleBpm;
    return juce::jlimit (0.05, 20.0, factor);
}

double RenderParams::durationFactor() const
{
    const double pitchRatio = std::pow (2.0, pitchSemitones / 12.0);
    return 1.0 / (tempoFactor() * (keepSpeed ? 1.0 : pitchRatio));
}

juce::AudioBuffer<float> renderRegion (const juce::AudioBuffer<float>& source, int start, int end, double sourceRate,
                                       const RenderParams& params)
{
    start = juce::jlimit (0, source.getNumSamples(), start);
    end = juce::jlimit (start, source.getNumSamples(), end);
    const int length = end - start;
    if (length < 2)
        return juce::AudioBuffer<float> (2, 0);

    juce::AudioBuffer<float> region (source.getNumChannels(), length);
    for (int c = 0; c < source.getNumChannels(); ++c)
        region.copyFrom (c, 0, source, c, start, length);
    if (params.reverse)
        region.reverse (0, length);

    const double pitchRatio = std::pow (2.0, params.pitchSemitones / 12.0);

    // 1) Varispeed: sample-rate conversion plus classic "tape" pitch when keepSpeed is off.
    const double varispeed = params.keepSpeed ? 1.0 : pitchRatio;
    const double resampleRatio = sourceRate / params.outputSampleRate * varispeed;
    if (! nearlyEqual (resampleRatio, 1.0, 1.0e-6))
        region = resample (region, resampleRatio);

    // 2) Time-stretch for tempo, and transpose when the pitch must not change speed.
    const double stretchFactor = 1.0 / params.tempoFactor();
    const double transpose = params.keepSpeed ? pitchRatio : 1.0;
    if (! nearlyEqual (stretchFactor, 1.0) || ! nearlyEqual (transpose, 1.0))
    {
        const int outLength = juce::jmax (1, (int) std::round (region.getNumSamples() * stretchFactor));
        const bool tinyAndShortened = region.getNumSamples() < (int) (params.outputSampleRate * 0.1) && outLength < region.getNumSamples();

        if (tinyAndShortened)
        {
            // A spectral stretch (blocks of ~0.1 s) cannot squeeze a few milliseconds into
            // fewer: keep the pitch (and the attack) and simply cut the end with a fade.
            if (! nearlyEqual (transpose, 1.0))
                region = stretch (region, region.getNumSamples(), transpose, params.outputSampleRate);
            region.setSize (region.getNumChannels(), outLength, true);
            const int fade = juce::jmin (outLength / 2, (int) (params.outputSampleRate * 0.002));
            if (fade > 0)
                region.applyGainRamp (outLength - fade, fade, 1.0f, 0.0f);
        }
        else
        {
            region = stretch (region, outLength, transpose, params.outputSampleRate);
        }
    }

    return region;
}

RenderSet::Ptr renderChops (const RenderParams& params, const std::function<bool()>& shouldAbort)
{
    RenderSet::Ptr set = new RenderSet();
    set->sampleRate = params.outputSampleRate;
    if (params.sample == nullptr)
        return set;

    const auto& audio = params.sample->audio;
    const int total = audio.getNumSamples();
    const auto starts = sanitiseChops (params.chopStarts);

    for (int i = 0; i < numChops; ++i)
    {
        if (shouldAbort())
            return nullptr;

        const int start = (int) std::round (starts[(size_t) i] * (float) total);
        const int end = i + 1 < numChops ? (int) std::round (starts[(size_t) i + 1] * (float) total) : total;
        set->chops[(size_t) i] = renderRegion (audio, start, end, params.sample->sampleRate, params);
    }
    return set;
}
} // namespace rs
