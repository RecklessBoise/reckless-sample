#include "FxChain.h"

namespace rs
{
void FxChain::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
    filter.prepare (spec);
    chorus.prepare (spec);
    chorus.setRate (0.6f);
    chorus.setDepth (0.35f);
    chorus.setCentreDelay (9.0f);
    chorus.setFeedback (0.0f);
    reverb.prepare (spec);

    delayBuffer.setSize (2, (int) (sampleRate * 4.0) + 1);

    cutoffSmooth.reset (sampleRate, 0.03);
    volumeSmooth.reset (sampleRate, 0.03);
    driveSmooth.reset (sampleRate, 0.03);
    delayTimeSmooth.reset (sampleRate, 0.25);
    reset();
}

void FxChain::reset()
{
    filter.reset();
    chorus.reset();
    reverb.reset();
    delayBuffer.clear();
    delayWrite = 0;
    delayDamp[0] = delayDamp[1] = 0.0f;
    crushHold[0] = crushHold[1] = 0.0f;
    crushCounter = 0.0f;
    levelFollower = 0.0f;
}

void FxChain::processDelay (juce::AudioBuffer<float>& buffer, const FxSettings& s)
{
    const int size = delayBuffer.getNumSamples();
    const float seconds = (float) (s.delayBeats * 60.0 / juce::jlimit (30.0, 300.0, s.bpm));
    delayTimeSmooth.setTargetValue (juce::jlimit (0.01f, 3.9f, seconds) * (float) sampleRate);

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    auto* dl = delayBuffer.getWritePointer (0);
    auto* dr = delayBuffer.getWritePointer (1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float delaySamples = delayTimeSmooth.getNextValue();
        float readPos = (float) delayWrite - delaySamples;
        if (readPos < 0.0f)
            readPos += (float) size;
        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % size;
        const float frac = readPos - (float) i0;
        const float wetL = dl[i0] + frac * (dl[i1] - dl[i0]);
        const float wetR = dr[i0] + frac * (dr[i1] - dr[i0]);

        // Ping-pong: each side feeds the other, with gentle damping in the loop.
        delayDamp[0] += 0.35f * (wetR - delayDamp[0]);
        delayDamp[1] += 0.35f * (wetL - delayDamp[1]);
        dl[delayWrite] = 0.5f * (l[i] + r[i]) + delayDamp[0] * s.delayFeedback;
        dr[delayWrite] = delayDamp[1] * s.delayFeedback;

        l[i] += wetL * s.delayMix;
        r[i] += wetR * s.delayMix;
        delayWrite = (delayWrite + 1) % size;
    }
}

void FxChain::process (juce::AudioBuffer<float>& buffer, const FxSettings& s)
{
    if (buffer.getNumChannels() < 2)
        return;

    const int n = buffer.getNumSamples();
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    // Drive: soft saturation with level compensation.
    driveSmooth.setTargetValue (s.drive);
    if (s.drive > 0.0001f || driveSmooth.isSmoothing())
    {
        for (int i = 0; i < n; ++i)
        {
            const float d = driveSmooth.getNextValue();
            const float g = 1.0f + d * 24.0f;
            const float makeup = (1.0f + d * 2.0f) / g;
            l[i] = std::tanh (l[i] * g) * makeup;
            r[i] = std::tanh (r[i] * g) * makeup;
        }
    }

    // Crush: sample-rate reduction plus bit depth reduction.
    if (s.crush > 0.0001f)
    {
        const float holdSamples = 1.0f + s.crush * s.crush * 24.0f;
        const float levels = std::pow (2.0f, 16.0f - s.crush * 12.0f);
        for (int i = 0; i < n; ++i)
        {
            crushCounter += 1.0f;
            if (crushCounter >= holdSamples)
            {
                crushCounter -= holdSamples;
                crushHold[0] = std::round (l[i] * levels) / levels;
                crushHold[1] = std::round (r[i] * levels) / levels;
            }
            l[i] = crushHold[0];
            r[i] = crushHold[1];
        }
    }

    // Filter: skipped when fully open as a low-pass.
    const bool filterOpen = s.filterType == FilterType::lowPass && s.cutoff >= 19999.0f && ! cutoffSmooth.isSmoothing();
    cutoffSmooth.setTargetValue (s.cutoff);
    if (! filterOpen)
    {
        using T = juce::dsp::StateVariableTPTFilterType;
        filter.setType (s.filterType == FilterType::lowPass ? T::lowpass : s.filterType == FilterType::bandPass ? T::bandpass : T::highpass);
        filter.setResonance (0.5f * std::pow (24.0f, s.resonance));
        for (int i = 0; i < n; ++i)
        {
            filter.setCutoffFrequency (juce::jmin (cutoffSmooth.getNextValue(), (float) sampleRate * 0.45f));
            l[i] = filter.processSample (0, l[i]);
            r[i] = filter.processSample (1, r[i]);
        }
    }
    else
    {
        cutoffSmooth.skip (n);
    }

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    if (s.chorusMix > 0.0001f)
    {
        chorus.setMix (s.chorusMix);
        chorus.process (context);
    }

    if (s.delayMix > 0.0001f)
    {
        processDelay (buffer, s);
        delayActive = true;
    }
    else if (delayActive)
    {
        delayBuffer.clear();
        delayActive = false;
    }

    if (s.reverbMix > 0.0001f)
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize = 0.3f + s.reverbSize * 0.69f;
        p.damping = 0.45f;
        p.wetLevel = s.reverbMix;
        p.dryLevel = 1.0f - 0.5f * s.reverbMix;
        p.width = 1.0f;
        reverb.setParameters (p);
        reverb.process (context);
    }

    volumeSmooth.setTargetValue (s.volume);
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = volumeSmooth.getNextValue();
        l[i] *= g;
        r[i] *= g;
        peak = juce::jmax (peak, std::abs (l[i]), std::abs (r[i]));
    }

    // Peak follower with a slow release keeps the visuals smooth.
    const float release = std::exp (-(float) n / (float) (sampleRate * 0.25));
    levelFollower = juce::jmax (peak, levelFollower * release);
    outputLevel = levelFollower;
}
} // namespace rs
