#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <mutex>
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

    /** FLAC copy of the audio as base64, so sessions and presets still work if the file
        moves. Encoded once, on first use; empty for samples longer than maxEmbeddedSeconds. */
    const juce::String& getEmbeddedAudio() const;

private:
    mutable std::once_flag embedOnce;
    mutable juce::String embedded;
};

/** Maximum length accepted for an imported sample, in seconds. Longer files make every
    pitch / tempo change slow to re-render and use a lot of memory. */
inline constexpr double maxSampleSeconds = 180.0;

/** Samples up to this length are stored inside sessions and presets (about 10 MB at most). */
inline constexpr double maxEmbeddedSeconds = 60.0;

/** Rebuilds a sample from getEmbeddedAudio(). Returns nullptr on failure. */
SampleData::Ptr decodeEmbeddedAudio (const juce::String& base64, const juce::String& name);

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
