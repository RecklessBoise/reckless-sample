#include "dsp/BpmDetector.h"
#include "dsp/FactorySamples.h"

namespace rs
{
class BpmDetectorTests : public juce::UnitTest
{
public:
    BpmDetectorTests() : juce::UnitTest ("BPM detection", "RecklessSample") {}

    void runTest() override
    {
        beginTest ("Tempo from file names");
        expectEquals (*bpmFromFileName ("loop_92bpm.wav"), 92.0);
        expectEquals (*bpmFromFileName ("Drums 140 BPM.aif"), 140.0);
        expectEquals (*bpmFromFileName ("BPM_87.5 keys.wav"), 87.5);
        expect (! bpmFromFileName ("kick.wav").has_value());
        expect (! bpmFromFileName ("bpm.wav").has_value());

        beginTest ("Folding into 70-180");
        expectEquals (foldBpm (60.0), 120.0);
        expectEquals (foldBpm (200.0), 100.0);
        expectEquals (foldBpm (128.0), 128.0);

        beginTest ("Click track");
        for (double bpm : { 90.0, 120.0, 140.0 })
        {
            const double sr = 44100.0;
            const int beats = 16;
            juce::AudioBuffer<float> clicks (1, (int) (beats * 60.0 / bpm * sr));
            clicks.clear();
            for (int b = 0; b < beats; ++b)
            {
                const int start = (int) (b * 60.0 / bpm * sr);
                for (int i = 0; i < 400 && start + i < clicks.getNumSamples(); ++i)
                    clicks.setSample (0, start + i, std::sin ((float) i * 0.3f) * std::exp ((float) -i / 80.0f));
            }
            const auto result = detectBpm (clicks, sr);
            expectWithinAbsoluteError (result.bpm, bpm, 1.0, "click track at " + juce::String (bpm));
        }

        beginTest ("Factory drum loops");
        for (int i = 0; i < getNumFactorySamples(); ++i)
        {
            const auto& info = getFactorySampleInfo (i);
            if (juce::String (info.category) != "Drums" || juce::String (info.name) == "Perc Kit")
                continue;
            auto sample = createFactorySample (i);
            const auto result = detectBpm (sample->audio, sample->sampleRate);
            const bool ok = std::abs (result.bpm - info.bpm) < 1.5 || std::abs (result.bpm * 2.0 - info.bpm) < 1.5
                            || std::abs (result.bpm / 2.0 - info.bpm) < 1.5;
            expect (ok, juce::String (info.name) + ": expected " + juce::String (info.bpm) + " got " + juce::String (result.bpm));
        }
    }
};

static BpmDetectorTests bpmDetectorTests;
} // namespace rs
