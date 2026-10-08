#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <optional>

namespace rs
{
struct BpmResult
{
    double bpm = 120.0;
    float confidence = 0.0f; // 0..1
    bool fromFileName = false;
};

/** Positive spectral flux of a mono mix with its local mean removed, one value per hop. */
std::vector<float> onsetStrength (const juce::AudioBuffer<float>& audio, int hop);

/** Looks for a tempo written in a file name, e.g. "loop_92bpm.wav" or "BPM 140 - drums.aif". */
std::optional<double> bpmFromFileName (const juce::String& fileName);

/** Estimates tempo from audio (onset autocorrelation), preferring a tempo written in the file name. */
BpmResult detectBpm (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& fileName = {});

/** Folds a tempo into the 70-180 range by halving or doubling. */
double foldBpm (double bpm);
} // namespace rs
