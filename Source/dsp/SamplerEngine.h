#pragma once

#include "ChopRenderer.h"

namespace rs
{
/** Per-block settings that do not need a re-render. */
struct VoiceSettings
{
    float length = 1.0f;     // fraction of each chop that plays
    float attackMs = 2.0f;
    float releaseMs = 40.0f;
    bool latch = false;
    bool clickFree = true;
};

/**
    Plays the pre-rendered chops. One voice per chop: tap plays the chop once,
    holding loops it, latch toggles looping on each tap.
    All methods are called on the audio thread except the atomics read by the UI.
*/
class SamplerEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    /** Swaps in freshly rendered audio. Playing voices crossfade to the new chop at the same relative position. */
    void setRenderSet (RenderSet::Ptr newSet);
    const RenderSet::Ptr& getRenderSet() const { return renderSet; }

    void noteOn (int chop, float velocity, const VoiceSettings&);
    void noteOff (int chop, const VoiceSettings&);
    void allNotesOff();

    void render (juce::AudioBuffer<float>& output, int startSample, int numSamples, const VoiceSettings&);

    /** UI feedback: envelope level and normalised position of each chop (-1 when silent). */
    std::array<std::atomic<float>, numChops> padLevel {};
    std::array<std::atomic<float>, numChops> padPosition {};

    /** Whether a chop is currently looping because of latch. */
    std::array<std::atomic<bool>, numChops> padLatched {};

private:
    struct Voice
    {
        bool active = false;
        bool held = false;
        bool latched = false;
        bool releasing = false;
        bool fadeIn = false;      // new audio fades in while the old fades out (re-render)
        int passes = 0;
        float lastEnvelope = 0.0f;
        double position = 0.0;
        float gain = 1.0f;
        juce::ADSR envelope;

        // Fade-out of the previous audio after a retrigger or a re-render.
        RenderSet::Ptr fadeSet;
        double fadePosition = 0.0;
        float fadeGain = 0.0f;
        int fadeRemaining = 0;
    };

    void startVoice (int chop, float velocity, const VoiceSettings&);
    void beginCrossfade (Voice& voice, const RenderSet::Ptr& fromSet);
    void configureEnvelope (Voice& voice, const VoiceSettings&) const;
    int playableLength (int chop, const RenderSet* set, const VoiceSettings&) const;

    std::array<Voice, numChops> voices;
    RenderSet::Ptr renderSet;
    double sampleRate = 44100.0;
    int crossfadeSamples = 256;
};
} // namespace rs
