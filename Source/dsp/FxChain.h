#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Parameters.h"

namespace rs
{
struct FxSettings
{
    float volume = 1.0f;
    FilterType filterType = FilterType::lowPass;
    float cutoff = 20000.0f;
    float resonance = 0.1f;
    float drive = 0.0f;
    float crush = 0.0f;
    float chorusMix = 0.0f;
    float delayMix = 0.0f;
    float delayBeats = 0.75f;
    float delayFeedback = 0.4f;
    float reverbMix = 0.0f;
    float reverbSize = 0.6f;
    double bpm = 120.0;
};

/** Drive -> crush -> filter -> chorus -> tempo-synced ping-pong delay -> reverb -> volume. */
class FxChain
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const FxSettings& settings);

    /** Smoothed output level, for the UI animation. */
    std::atomic<float> outputLevel { 0.0f };

private:
    void processDelay (juce::AudioBuffer<float>& buffer, const FxSettings& s);

    double sampleRate = 44100.0;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;

    juce::AudioBuffer<float> delayBuffer;
    int delayWrite = 0;
    bool delayActive = false;
    float delayDamp[2] {};

    juce::SmoothedValue<float> cutoffSmooth, volumeSmooth, driveSmooth, delayTimeSmooth, delayMixSmooth;

    // Effects keep running for a moment after their mix reaches zero, so their own
    // smoothing can fade them out instead of cutting them (and clicking).
    int chorusHold = 0, reverbHold = 0;
    float crushHold[2] {};
    float crushCounter = 0.0f;
    float levelFollower = 0.0f;
};
} // namespace rs
