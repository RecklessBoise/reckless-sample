#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace rs
{
struct FactoryPreset
{
    juce::String name;
    juce::String category;
    int sampleIndex = 0;
    int chopMode = 0;
    std::vector<std::pair<juce::String, float>> values; // parameter id -> real value; others use defaults
};

/** All factory presets: every factory sample combined with a set of styles, plus an init preset. */
const std::vector<FactoryPreset>& getFactoryPresets();
} // namespace rs
