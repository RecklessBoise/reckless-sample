#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "../Parameters.h"

namespace rs
{
/** Normalised (0..1) start position of each chop. Chop i ends where chop i+1 starts (the last ends at 1). */
using ChopStarts = std::array<float, numChops>;

/** Where a sample came from, so it can be stored in sessions and presets. */
struct SampleSource
{
    enum class Kind { none, factory, file };

    Kind kind = Kind::none;
    int factoryIndex = -1;
    juce::File file;

    bool operator== (const SampleSource& o) const { return kind == o.kind && factoryIndex == o.factoryIndex && file == o.file; }

    juce::ValueTree toValueTree() const;
    static SampleSource fromValueTree (const juce::ValueTree&);
};

/** Immutable audio for a loaded sample. Shared between threads by reference count. */
struct SampleData : juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SampleData>;

    juce::AudioBuffer<float> audio; // always stereo
    double sampleRate = 44100.0;
    juce::String name;
    SampleSource source;
    double bpm = 120.0;         // detected or known tempo
    bool bpmIsKnown = false;    // true for factory loops or tempo found in the file name

    int getNumSamples() const { return audio.getNumSamples(); }
    double getLengthSeconds() const { return getNumSamples() / sampleRate; }
};

/** Maximum length accepted for an imported sample, in seconds. */
inline constexpr double maxSampleSeconds = 600.0;

/** Reads an audio file into a SampleData, converting to stereo. Returns nullptr and fills error on failure. */
SampleData::Ptr loadSampleFile (const juce::File& file, juce::String& error);

/** Wraps an already rendered buffer (mono or stereo) as SampleData. */
SampleData::Ptr makeSampleData (juce::AudioBuffer<float>&& audio, double sampleRate, const juce::String& name);

ChopStarts equalChops();

/** Finds numChops strong transients, always starting with the first sound in the sample. */
ChopStarts transientChops (const SampleData& sample);

ChopStarts computeChops (const SampleData& sample, ChopMode mode);

/** Sorts and clamps chop starts so every chop has a positive length. */
ChopStarts sanitiseChops (ChopStarts starts);

juce::StringArray supportedAudioExtensions();
} // namespace rs
