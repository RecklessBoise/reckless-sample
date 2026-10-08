#include "FactoryPresets.h"
#include "../Parameters.h"
#include "../dsp/FactorySamples.h"

namespace rs
{
namespace
{
    struct Style
    {
        const char* name;
        std::vector<std::pair<juce::String, float>> values;
    };

    std::vector<Style> styles (bool isBass)
    {
        using namespace ParamID;
        return {
            { "Clean", {} },
            { "Dusty Tape", { { crush, 0.25f }, { drive, 0.3f }, { cutoff, 5200.0f }, { resonance, 0.2f }, { reverbMix, 0.15f } } },
            { "Cathedral", { { reverbMix, 0.55f }, { reverbSize, 0.95f }, { attack, 30.0f }, { release, 900.0f } } },
            { "Half Time", { { duration, 2.0f } } },
            { "Double Time", { { duration, 0.5f }, { length, 0.5f } } },
            { "Chipmunk", { { pitch, 12.0f } } },
            { "Low Down", { { pitch, isBass ? -5.0f : -12.0f } } },
            { "Fifth Up", { { pitch, 7.0f }, { reverbMix, 0.2f } } },
            { "Varispeed Slow", { { keepSpeed, 0.0f }, { pitch, -5.0f }, { syncMode, 0.0f }, { cutoff, 9000.0f } } },
            { "Reverse Wash", { { reverse, 1.0f }, { reverbMix, 0.45f }, { delayMix, 0.25f } } },
            { "Dub Echo", { { delayMix, 0.45f }, { delayFb, 0.65f }, { delayTime, 2.0f }, { cutoff, 3000.0f }, { reverbMix, 0.2f } } },
            { "Chorus Dream", { { chorusMix, 0.6f }, { reverbMix, 0.35f }, { filterType, 2.0f }, { cutoff, 120.0f } } },
            { "Stutter", { { lengthSync, 1.0f }, { lengthDiv, 2.0f }, { latch, 1.0f }, { delayMix, 0.2f }, { delayTime, 0.0f } } },
            { "Crushed", { { crush, 0.7f }, { drive, 0.5f }, { volume, 0.8f } } },
            { "Telephone", { { filterType, 1.0f }, { cutoff, 1200.0f }, { resonance, 0.5f }, { drive, 0.4f } } },
            { "Ghost", { { filterType, 2.0f }, { cutoff, 600.0f }, { reverbMix, 0.6f }, { attack, 120.0f }, { release, 1500.0f }, { volume, 0.9f } } },
        };
    }
} // namespace

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = []
    {
        std::vector<FactoryPreset> list;
        list.push_back ({ "Init", "Init", 0, 0, {} });

        for (int s = 0; s < getNumFactorySamples(); ++s)
        {
            const auto& info = getFactorySampleInfo (s);
            const bool isBass = juce::String (info.category) == "Bass";
            for (const auto& style : styles (isBass))
            {
                FactoryPreset p;
                p.name = juce::String (info.name) + " - " + style.name;
                p.category = info.category;
                p.sampleIndex = s;
                // Factory loops follow the DAW tempo unless the style says otherwise.
                p.values.push_back ({ ParamID::syncMode, 1.0f });
                p.values.push_back ({ ParamID::sampleBpm, (float) info.bpm });
                for (const auto& v : style.values)
                    p.values.push_back (v);
                list.push_back (std::move (p));
            }
        }
        return list;
    }();
    return presets;
}
} // namespace rs
