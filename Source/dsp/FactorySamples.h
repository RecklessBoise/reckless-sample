#pragma once

#include "SampleData.h"

namespace rs
{
/**
    Factory sounds are synthesised in code at load time, so the plug-in ships
    without any third-party audio and every sound is free to use.
    Each sample is two bars (8 beats) long, so equal chops land on the beat.
*/
struct FactorySampleInfo
{
    const char* name;
    const char* category;
    double bpm;
};

int getNumFactorySamples();
const FactorySampleInfo& getFactorySampleInfo (int index);

/** Synthesises (or returns the cached) factory sample. Thread-safe. */
SampleData::Ptr createFactorySample (int index);

inline constexpr double factorySampleRate = 44100.0;
} // namespace rs
