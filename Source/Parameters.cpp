#include "Parameters.h"

namespace rs
{
namespace
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using Attr = juce::AudioParameterFloatAttributes;

    juce::NormalisableRange<float> skewedRange (float min, float max, float centre)
    {
        juce::NormalisableRange<float> r (min, max);
        r.setSkewForCentre (centre);
        return r;
    }

    Attr withSuffix (const juce::String& suffix, int decimals)
    {
        return Attr().withStringFromValueFunction ([suffix, decimals] (float v, int)
                                                   { return juce::String (v, decimals) + suffix; });
    }

    Attr percent()
    {
        return Attr().withStringFromValueFunction ([] (float v, int)
                                                   { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; });
    }

    Attr multiplier()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + juce::String::fromUTF8 ("\xc3\x97"); });
    }

    Attr hertz()
    {
        return Attr().withStringFromValueFunction ([] (float v, int)
                                                   { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz"
                                                                         : juce::String (juce::roundToInt (v)) + " Hz"; });
    }

    Attr semitones()
    {
        return Attr().withStringFromValueFunction ([] (float v, int)
                                                   {
                                                       const auto n = juce::roundToInt (v);
                                                       return (n > 0 ? "+" : "") + juce::String (n) + " st";
                                                   });
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using PID = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<APF> (PID { ParamID::volume, 1 }, "Volume", juce::NormalisableRange<float> (0.0f, 1.5f), 1.0f, percent()),
                std::make_unique<APF> (PID { ParamID::length, 1 }, "Length", juce::NormalisableRange<float> (0.05f, 1.0f), 1.0f, multiplier()),
                std::make_unique<APF> (PID { ParamID::attack, 1 }, "Attack", skewedRange (0.0f, 2000.0f, 80.0f), 2.0f, withSuffix (" ms", 0)),
                std::make_unique<APF> (PID { ParamID::release, 1 }, "Release", skewedRange (1.0f, 4000.0f, 200.0f), 40.0f, withSuffix (" ms", 0)),
                std::make_unique<APB> (PID { ParamID::latch, 1 }, "Latch", false),
                std::make_unique<APB> (PID { ParamID::clickFree, 1 }, "Click-free", true),
                std::make_unique<APB> (PID { ParamID::reverse, 1 }, "Reverse", false),
                std::make_unique<APC> (PID { ParamID::chopMode, 1 }, "Chop Mode", juce::StringArray { "Equal", "Transient" }, 0));

    layout.add (std::make_unique<APF> (PID { ParamID::pitch, 1 }, "Pitch", juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, semitones()),
                std::make_unique<APF> (PID { ParamID::fine, 1 }, "Fine", juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f, withSuffix (" ct", 0)),
                std::make_unique<APB> (PID { ParamID::keepSpeed, 1 }, "Keep Speed", true),
                std::make_unique<APF> (PID { ParamID::speed, 1 }, "Speed", skewedRange (0.25f, 4.0f, 1.0f), 1.0f, multiplier()),
                std::make_unique<APC> (PID { ParamID::syncMode, 1 }, "Tempo Sync", juce::StringArray { "Off", "DAW", "Manual" }, 0),
                std::make_unique<APF> (PID { ParamID::sampleBpm, 1 }, "Sample BPM", juce::NormalisableRange<float> (40.0f, 250.0f, 0.01f), 120.0f, withSuffix (" bpm", 2)),
                std::make_unique<APF> (PID { ParamID::manualBpm, 1 }, "Target BPM", juce::NormalisableRange<float> (40.0f, 250.0f, 0.01f), 120.0f, withSuffix (" bpm", 2)));

    layout.add (std::make_unique<APC> (PID { ParamID::filterType, 1 }, "Filter Type", juce::StringArray { "Low-pass", "Band-pass", "High-pass" }, 0),
                std::make_unique<APF> (PID { ParamID::cutoff, 1 }, "Cutoff", skewedRange (20.0f, 20000.0f, 1000.0f), 20000.0f, hertz()),
                std::make_unique<APF> (PID { ParamID::resonance, 1 }, "Resonance", juce::NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent()),
                std::make_unique<APF> (PID { ParamID::drive, 1 }, "Drive", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()),
                std::make_unique<APF> (PID { ParamID::crush, 1 }, "Crush", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()),
                std::make_unique<APF> (PID { ParamID::chorusMix, 1 }, "Chorus", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()),
                std::make_unique<APF> (PID { ParamID::delayMix, 1 }, "Delay", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()),
                std::make_unique<APC> (PID { ParamID::delayTime, 1 }, "Delay Time", delayDivisionNames, 2),
                std::make_unique<APF> (PID { ParamID::delayFb, 1 }, "Feedback", juce::NormalisableRange<float> (0.0f, 0.95f), 0.4f, percent()),
                std::make_unique<APF> (PID { ParamID::reverbMix, 1 }, "Reverb", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()),
                std::make_unique<APF> (PID { ParamID::reverbSize, 1 }, "Room", juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent()));

    return layout;
}

const juce::StringArray& allParameterIds()
{
    static const juce::StringArray ids {
        ParamID::volume, ParamID::length, ParamID::attack, ParamID::release, ParamID::latch, ParamID::clickFree,
        ParamID::reverse, ParamID::chopMode, ParamID::pitch, ParamID::fine, ParamID::keepSpeed, ParamID::speed,
        ParamID::syncMode, ParamID::sampleBpm, ParamID::manualBpm, ParamID::filterType, ParamID::cutoff,
        ParamID::resonance, ParamID::drive, ParamID::crush, ParamID::chorusMix, ParamID::delayMix,
        ParamID::delayTime, ParamID::delayFb, ParamID::reverbMix, ParamID::reverbSize
    };
    return ids;
}
} // namespace rs
