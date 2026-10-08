#pragma once

#include "SampleData.h"
#include <functional>

namespace rs
{
/** Everything that changes the audio of the pre-rendered chops. */
struct RenderParams
{
    SampleData::Ptr sample;
    ChopStarts chopStarts = equalChops();
    double outputSampleRate = 44100.0;
    float pitchSemitones = 0.0f;  // pitch + fine
    bool keepSpeed = true;        // true: pitch without changing duration
    float speed = 1.0f;           // user speed multiplier
    SyncMode syncMode = SyncMode::off;
    double sampleBpm = 120.0;
    double targetBpm = 120.0;     // host or manual tempo, used when syncing
    bool reverse = false;

    bool operator== (const RenderParams& other) const;
    bool operator!= (const RenderParams& other) const { return ! (*this == other); }

    /** Factor applied to playback speed by tempo (speed x sync ratio). */
    double tempoFactor() const;

    /** Output duration divided by source duration. */
    double durationFactor() const;
};

/** The eight chops, rendered at the output sample rate with pitch and tempo applied. */
struct RenderSet : juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<RenderSet>;

    std::array<juce::AudioBuffer<float>, numChops> chops;
    double sampleRate = 44100.0;
};

/** Renders all chops. Runs on a background thread; returns nullptr if shouldAbort() becomes true. */
RenderSet::Ptr renderChops (const RenderParams& params, const std::function<bool()>& shouldAbort = [] { return false; });

/** Varispeed + time-stretch for one stereo region; exposed for tests. */
juce::AudioBuffer<float> renderRegion (const juce::AudioBuffer<float>& source, int start, int end, double sourceRate,
                                       const RenderParams& params);
} // namespace rs
