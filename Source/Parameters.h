#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace rs
{
constexpr int numChops = 8;

namespace ParamID
{
    // Playback
    inline constexpr auto volume     = "volume";
    inline constexpr auto length     = "length";
    inline constexpr auto lengthSync = "lengthSync";
    inline constexpr auto lengthDiv  = "lengthDiv";
    inline constexpr auto attack     = "attack";
    inline constexpr auto release    = "release";
    inline constexpr auto latch      = "latch";
    inline constexpr auto clickFree  = "clickFree";
    inline constexpr auto reverse    = "reverse";
    inline constexpr auto chopMode   = "chopMode";

    // Pitch / time
    inline constexpr auto pitch      = "pitch";
    inline constexpr auto fine       = "fine";
    inline constexpr auto keepSpeed  = "keepSpeed";
    inline constexpr auto duration   = "duration";
    inline constexpr auto syncMode   = "syncMode";
    inline constexpr auto sampleBpm  = "sampleBpm";
    inline constexpr auto manualBpm  = "manualBpm";

    // FX
    inline constexpr auto filterType = "filterType";
    inline constexpr auto cutoff     = "cutoff";
    inline constexpr auto resonance  = "resonance";
    inline constexpr auto drive      = "drive";
    inline constexpr auto crush      = "crush";
    inline constexpr auto chorusMix  = "chorusMix";
    inline constexpr auto delayMix   = "delayMix";
    inline constexpr auto delayTime  = "delayTime";
    inline constexpr auto delayFb    = "delayFeedback";
    inline constexpr auto reverbMix  = "reverbMix";
    inline constexpr auto reverbSize = "reverbSize";
}

enum class SyncMode { off = 0, host, manual };
enum class ChopMode { equal = 0, transient };
enum class FilterType { lowPass = 0, bandPass, highPass };

/** Delay divisions offered by the delayTime choice, in quarter notes. */
inline constexpr float delayDivisionsInBeats[] { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
inline const juce::StringArray delayDivisionNames { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" };

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Note values offered when Length is synced to the tempo, in quarter notes. */
inline constexpr float lengthDivisionsInBeats[] { 0.0625f, 0.125f, 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f, 8.0f };
inline const juce::StringArray lengthDivisionNames { "1/64", "1/32", "1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2", "1 bar", "2 bars" };

/** Every parameter ID, in a stable order. Used by presets and tests. */
const juce::StringArray& allParameterIds();
} // namespace rs
