#include "SamplerEngine.h"

namespace rs
{
void SamplerEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    crossfadeSamples = juce::jmax (32, (int) (sampleRate * 0.006));
    for (auto& v : voices)
        v.envelope.setSampleRate (sampleRate);
    reset();
}

void SamplerEngine::reset()
{
    for (size_t i = 0; i < voices.size(); ++i)
    {
        auto& v = voices[i];
        v.active = v.held = v.latched = v.releasing = false;
        v.fadeRemaining = 0;
        v.fadeSet = nullptr;
        v.envelope.reset();
        padLevel[i] = 0.0f;
        padPosition[i] = -1.0f;
        padLatched[i] = false;
    }
}

void SamplerEngine::configureEnvelope (Voice& voice, const VoiceSettings& s) const
{
    const float minFade = s.clickFree ? 0.003f : 0.0f;
    juce::ADSR::Parameters p;
    p.attack = juce::jmax (minFade, s.attackMs * 0.001f);
    p.decay = 0.0f;
    p.sustain = 1.0f;
    p.release = juce::jmax (juce::jmax (minFade, 0.001f), s.releaseMs * 0.001f);
    voice.envelope.setParameters (p);
}

int SamplerEngine::playableLength (int chop, const RenderSet* set, const VoiceSettings& s) const
{
    if (set == nullptr)
        return 0;
    const int full = set->chops[(size_t) chop].getNumSamples();
    return full <= 0 ? 0 : juce::jlimit (1, full, (int) std::round ((float) full * s.length));
}

void SamplerEngine::beginCrossfade (Voice& voice, const RenderSet::Ptr& fromSet)
{
    if (! voice.active || fromSet == nullptr)
        return;
    voice.fadeSet = fromSet;
    voice.fadePosition = voice.position;
    voice.fadeGain = voice.gain * voice.lastEnvelope;
    voice.fadeRemaining = crossfadeSamples;
}

void SamplerEngine::setRenderSet (RenderSet::Ptr newSet)
{
    for (size_t i = 0; i < voices.size(); ++i)
    {
        auto& v = voices[i];
        if (! v.active || renderSet == nullptr || newSet == nullptr)
            continue;

        const int oldLength = renderSet->chops[i].getNumSamples();
        const int newLength = newSet->chops[i].getNumSamples();
        beginCrossfade (v, renderSet);
        v.position = oldLength > 0 ? v.position / oldLength * newLength : 0.0;
        v.fadeIn = true;
    }
    renderSet = std::move (newSet);
}

void SamplerEngine::startVoice (int chop, float velocity, const VoiceSettings& s)
{
    auto& v = voices[(size_t) chop];
    beginCrossfade (v, renderSet);
    configureEnvelope (v, s);
    v.envelope.reset();
    v.envelope.noteOn();
    v.active = true;
    v.releasing = false;
    v.fadeIn = false;
    v.passes = 0;
    v.position = 0.0;
    v.gain = 0.35f + 0.65f * velocity;
}

void SamplerEngine::noteOn (int chop, float velocity, const VoiceSettings& s)
{
    if (! juce::isPositiveAndBelow (chop, numChops))
        return;

    auto& v = voices[(size_t) chop];
    if (s.latch)
    {
        if (v.active && v.latched)
        {
            v.latched = false;
            v.releasing = true;
            v.envelope.noteOff();
        }
        else
        {
            startVoice (chop, velocity, s);
            v.latched = true;
        }
        padLatched[(size_t) chop] = v.latched;
        return;
    }

    startVoice (chop, velocity, s);
    v.latched = false;
    v.held = true;
    padLatched[(size_t) chop] = false;
}

void SamplerEngine::noteOff (int chop, const VoiceSettings& s)
{
    if (! juce::isPositiveAndBelow (chop, numChops) || s.latch)
        return;

    auto& v = voices[(size_t) chop];
    v.held = false;
    // A tap plays the chop through once; a hold that already looped fades out.
    if (v.active && ! v.latched && v.passes > 0)
    {
        v.releasing = true;
        v.envelope.noteOff();
    }
}

void SamplerEngine::allNotesOff()
{
    for (size_t i = 0; i < voices.size(); ++i)
    {
        auto& v = voices[i];
        v.held = v.latched = false;
        padLatched[i] = false;
        if (v.active)
        {
            v.releasing = true;
            v.envelope.noteOff();
        }
    }
}

void SamplerEngine::render (juce::AudioBuffer<float>& output, int startSample, int numSamples, const VoiceSettings& s)
{
    auto* outL = output.getWritePointer (0, startSample);
    auto* outR = output.getWritePointer (output.getNumChannels() > 1 ? 1 : 0, startSample);
    const bool stereoOut = output.getNumChannels() > 1;

    for (int chop = 0; chop < numChops; ++chop)
    {
        auto& v = voices[(size_t) chop];
        if (! v.active && v.fadeRemaining <= 0)
        {
            padLevel[(size_t) chop] = 0.0f;
            padPosition[(size_t) chop] = -1.0f;
            continue;
        }

        if (v.active)
            configureEnvelope (v, s);

        const auto* set = renderSet.get();
        const int length = playableLength (chop, set, s);
        const int edge = s.clickFree ? juce::jmin (length / 4, (int) (sampleRate * 0.003)) : 0;
        const float* srcL = length > 0 ? set->chops[(size_t) chop].getReadPointer (0) : nullptr;
        const float* srcR = length > 0 ? set->chops[(size_t) chop].getReadPointer (1) : nullptr;

        const auto* fadeSet = v.fadeSet.get();
        const int fadeLength = fadeSet != nullptr ? playableLength (chop, fadeSet, s) : 0;
        const float* fadeL = fadeLength > 0 ? fadeSet->chops[(size_t) chop].getReadPointer (0) : nullptr;
        const float* fadeR = fadeLength > 0 ? fadeSet->chops[(size_t) chop].getReadPointer (1) : nullptr;

        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float l = 0.0f, r = 0.0f;
            const float xfade = v.fadeRemaining > 0 ? (float) v.fadeRemaining / (float) crossfadeSamples : 0.0f;

            if (v.active)
            {
                if (length <= 0)
                {
                    v.active = false;
                }
                else
                {
                    const int p = juce::jlimit (0, length - 1, (int) v.position);
                    const float env = v.envelope.getNextSample();
                    v.lastEnvelope = env;
                    float g = env * v.gain;
                    if (edge > 0)
                        g *= juce::jmin (1.0f, (float) juce::jmin (p + 1, length - p) / (float) edge);
                    if (v.fadeIn && v.fadeRemaining > 0)
                        g *= 1.0f - xfade;
                    l = srcL[p] * g;
                    r = srcR[p] * g;
                    peak = juce::jmax (peak, env * v.gain);

                    v.position += 1.0;
                    if (v.position >= length)
                    {
                        if (v.held || v.latched || v.releasing)
                        {
                            v.position -= length;
                            ++v.passes;
                        }
                        else
                        {
                            v.active = false;
                        }
                    }
                    if (v.releasing && ! v.envelope.isActive())
                        v.active = false;
                }
            }

            if (v.fadeRemaining > 0)
            {
                const int fp = (int) v.fadePosition;
                if (fadeL != nullptr && fp < fadeLength)
                {
                    const float g = v.fadeGain * xfade;
                    l += fadeL[fp] * g;
                    r += fadeR[fp] * g;
                    v.fadePosition += 1.0;
                    --v.fadeRemaining;
                }
                else
                {
                    v.fadeRemaining = 0;
                }
                if (v.fadeRemaining == 0)
                    v.fadeIn = false;
            }

            outL[i] += l;
            if (stereoOut)
                outR[i] += r;
            else
                outL[i] += r;
        }

        if (v.fadeRemaining == 0)
            v.fadeSet = nullptr; // the processor keeps a reference, so this never frees on the audio thread

        padLevel[(size_t) chop] = v.active ? peak : 0.0f;
        padPosition[(size_t) chop] = v.active && length > 0 ? (float) (v.position / length) : -1.0f;
        if (! v.active)
        {
            v.latched = false;
            padLatched[(size_t) chop] = false;
        }
    }
}
} // namespace rs
