#include "SampleData.h"
#include "BpmDetector.h"

namespace rs
{
namespace
{
    const juce::Identifier sourceTag { "Sample" };
    const juce::Identifier kindAttr { "kind" };
    const juce::Identifier indexAttr { "factoryIndex" };
    const juce::Identifier pathAttr { "path" };

    juce::AudioFormatManager& formatManager()
    {
        struct Holder
        {
            Holder() { manager.registerBasicFormats(); }
            juce::AudioFormatManager manager;
        };
        static Holder holder;
        return holder.manager;
    }
} // namespace

juce::ValueTree SampleSource::toValueTree() const
{
    juce::ValueTree v (sourceTag);
    v.setProperty (kindAttr, kind == Kind::factory ? "factory" : kind == Kind::file ? "file" : "none", nullptr);
    if (kind == Kind::factory)
        v.setProperty (indexAttr, factoryIndex, nullptr);
    if (kind == Kind::file)
        v.setProperty (pathAttr, file.getFullPathName(), nullptr);
    return v;
}

SampleSource SampleSource::fromValueTree (const juce::ValueTree& v)
{
    SampleSource s;
    const auto kind = v.getProperty (kindAttr).toString();
    if (kind == "factory")
    {
        s.kind = Kind::factory;
        s.factoryIndex = v.getProperty (indexAttr, 0);
    }
    else if (kind == "file")
    {
        const auto path = v.getProperty (pathAttr).toString();
        if (juce::File::isAbsolutePath (path))
        {
            s.kind = Kind::file;
            s.file = juce::File (path);
        }
    }
    return s;
}

const juce::String& SampleData::getEmbeddedAudio() const
{
    std::call_once (embedOnce, [this]
    {
        if (getLengthSeconds() > maxEmbeddedSeconds || getNumSamples() == 0)
            return;

        juce::MemoryBlock block;
        {
            std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::MemoryOutputStream> (block, false);
            juce::FlacAudioFormat flac;
            const auto options = juce::AudioFormatWriterOptions {}
                                     .withSampleRate (std::round (sampleRate))
                                     .withNumChannels (audio.getNumChannels())
                                     .withBitsPerSample (24);
            auto writer = flac.createWriterFor (stream, options);
            if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (audio, 0, getNumSamples()))
                return;
        } // the writer and stream flush into 'block' here
        embedded = block.toBase64Encoding();
    });
    return embedded;
}

SampleData::Ptr decodeEmbeddedAudio (const juce::String& base64, const juce::String& name)
{
    juce::MemoryBlock block;
    if (base64.isEmpty() || ! block.fromBase64Encoding (base64))
        return nullptr;

    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (block, false), true));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;

    const int numSamples = (int) reader->lengthInSamples;
    const int channels = (int) juce::jlimit (1u, 2u, reader->numChannels);
    juce::AudioBuffer<float> audio (channels, numSamples);
    if (! reader->read (&audio, 0, numSamples, 0, true, channels > 1))
        return nullptr;

    return makeSampleData (std::move (audio), reader->sampleRate, name);
}

juce::StringArray supportedAudioExtensions()
{
    return juce::StringArray::fromTokens (formatManager().getWildcardForAllFormats(), ";", "");
}

SampleData::Ptr makeSampleData (juce::AudioBuffer<float>&& audio, double sampleRate, const juce::String& name)
{
    SampleData::Ptr data = new SampleData();
    data->sampleRate = sampleRate;
    data->name = name;
    if (audio.getNumChannels() == 1)
    {
        juce::AudioBuffer<float> stereo (2, audio.getNumSamples());
        stereo.copyFrom (0, 0, audio, 0, 0, audio.getNumSamples());
        stereo.copyFrom (1, 0, audio, 0, 0, audio.getNumSamples());
        data->audio = std::move (stereo);
    }
    else
    {
        data->audio = std::move (audio);
    }
    return data;
}

SampleData::Ptr loadSampleFile (const juce::File& file, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager().createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Format audio non reconnu : " + file.getFileName();
        return nullptr;
    }

    const auto maxSamples = (juce::int64) (maxSampleSeconds * reader->sampleRate);
    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > maxSamples)
    {
        error = juce::String::fromUTF8 ("Sample trop long : 3 minutes maximum (") + file.getFileName() + ").";
        return nullptr;
    }

    const int numSamples = (int) reader->lengthInSamples;
    const int channels = (int) juce::jlimit (1u, 2u, reader->numChannels);
    juce::AudioBuffer<float> audio (channels, numSamples);
    if (! reader->read (&audio, 0, numSamples, 0, true, channels > 1))
    {
        error = "Impossible de lire " + file.getFileName();
        return nullptr;
    }

    auto data = makeSampleData (std::move (audio), reader->sampleRate, file.getFileNameWithoutExtension());
    data->source.kind = SampleSource::Kind::file;
    data->source.file = file;

    const auto result = detectBpm (data->audio, data->sampleRate, file.getFileName());
    data->bpm = result.bpm;
    data->bpmIsKnown = result.fromFileName;
    return data;
}

ChopStarts equalChops()
{
    ChopStarts starts {};
    for (int i = 0; i < numChops; ++i)
        starts[(size_t) i] = (float) i / (float) numChops;
    return starts;
}

ChopStarts sanitiseChops (ChopStarts starts)
{
    constexpr float minGap = 0.002f;
    std::sort (starts.begin(), starts.end());
    starts[0] = juce::jlimit (0.0f, 1.0f - minGap * numChops, starts[0]);
    for (size_t i = 1; i < starts.size(); ++i)
    {
        const float maxStart = 1.0f - minGap * (float) (numChops - (int) i);
        starts[i] = juce::jlimit (starts[i - 1] + minGap, maxStart, starts[i]);
    }
    return starts;
}

ChopStarts transientChops (const SampleData& sample)
{
    constexpr int hop = 512;
    const auto novelty = onsetStrength (sample.audio, hop);
    const int n = sample.getNumSamples();
    if (novelty.size() < (size_t) numChops * 4 || n <= 0)
        return equalChops();

    struct Peak { size_t frame; float strength; };
    std::vector<Peak> peaks;
    for (size_t i = 1; i + 1 < novelty.size(); ++i)
        if (novelty[i] > 0.0f && novelty[i] >= novelty[i - 1] && novelty[i] > novelty[i + 1])
            peaks.push_back ({ i, novelty[i] });

    std::sort (peaks.begin(), peaks.end(), [] (auto& a, auto& b) { return a.strength > b.strength; });

    // The first chop always starts at the first audible onset (or zero).
    const float frameToNorm = (float) hop / (float) n;
    std::vector<float> chosen;
    const size_t firstFrame = peaks.empty() ? 0 : std::min_element (peaks.begin(), peaks.end(), [] (auto& a, auto& b) { return a.frame < b.frame; })->frame;
    chosen.push_back (juce::jmax (0.0f, (float) firstFrame * frameToNorm - 0.002f));

    const float minSpacing = 0.25f / (float) numChops;
    for (const auto& p : peaks)
    {
        if ((int) chosen.size() == numChops)
            break;
        const float pos = (float) p.frame * frameToNorm;
        if (std::all_of (chosen.begin(), chosen.end(), [&] (float c) { return std::abs (c - pos) > minSpacing; }))
            chosen.push_back (pos);
    }

    if ((int) chosen.size() < numChops)
        return equalChops();

    ChopStarts starts {};
    std::copy_n (chosen.begin(), numChops, starts.begin());
    return sanitiseChops (starts);
}

ChopStarts computeChops (const SampleData& sample, ChopMode mode)
{
    return mode == ChopMode::transient ? transientChops (sample) : equalChops();
}
} // namespace rs
