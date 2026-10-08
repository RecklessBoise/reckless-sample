#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"
#include "dsp/ChopRenderer.h"
#include "dsp/FxChain.h"
#include "dsp/SamplerEngine.h"
#include "presets/PresetManager.h"
#include <map>
#include <mutex>

class RecklessSampleProcessor : public juce::AudioProcessor,
                                public rs::PresetHost,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::AsyncUpdater
{
public:
    RecklessSampleProcessor();
    ~RecklessSampleProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Sample management (message thread)
    bool loadSampleFile (const juce::File& file, juce::String& error);
    void loadFactorySample (int index);
    void stepSample (int delta);
    void redetectBpm();
    void scaleSampleBpm (double factor);

    rs::SampleData::Ptr getSample() const;
    rs::ChopStarts getChopStarts() const;
    void setChopStarts (const rs::ChopStarts& starts);
    juce::String getSampleCounterText() const;
    int getSampleVersion() const { return sampleVersion.load(); }

    // PresetHost
    juce::ValueTree getSampleState() const override;
    void applySampleState (const juce::ValueTree& state) override;
    void applyFactorySample (int factoryIndex, int chopMode) override;

    //==============================================================================
    /** Triggers a chop from the UI (mouse or computer keyboard). */
    void triggerPadFromUi (int chop, bool down);
    static int noteToChop (int midiNote);
    static int chopToNote (int chop);

    double getHostBpm() const { return hostBpm.load(); }
    bool isHostPlaying() const { return hostPlaying.load(); }
    double getEffectiveTargetBpm() const;

    /** Tempo used by tempo-synced Length and the delay: DAW tempo, else the manual or sample tempo. */
    double getGrooveBpm() const;

    /** Seconds of one Length note value at the current tempo. */
    double getLengthDivisionSeconds() const;

    /** For the waveform: fraction of each chop that is heard, and the rendered duration of the whole sample. */
    std::array<float, rs::numChops> getAudibleFractions() const;
    double getRenderedSampleSeconds() const;
    bool isRendering() const { return rendering.load(); }

    juce::AudioProcessorValueTreeState apvts;
    rs::SamplerEngine engine;
    rs::FxChain fx;
    std::unique_ptr<rs::PresetManager> presetManager;

    /** UI zoom (1 = 100%). Only changed by the user, never by the host's own window sizing. */
    std::atomic<float> uiScale { 1.0f };

    /** Errors raised while restoring a session (missing files), shown by the editor. */
    void reportError (const juce::String& message);
    juce::String takeError();

private:
    class RenderThread;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    void setSample (rs::SampleData::Ptr newSample, std::optional<rs::ChopStarts> chops, bool updateBpm);
    rs::RenderParams makeRenderParams() const;
    rs::VoiceSettings voiceSettings() const;
    rs::FxSettings fxSettings() const;
    void publishRender (rs::RenderSet::Ptr set);
    void collectRetiredRenders();
    void handleMidi (const juce::MidiMessage& message, const rs::VoiceSettings& settings);
    float param (const char* id) const { return params.at (id)->load(); }

    std::map<juce::String, std::atomic<float>*> params;

    mutable std::mutex sampleMutex;
    rs::SampleData::Ptr sample;
    rs::ChopStarts chopStarts = rs::equalChops();
    int chopsComputedForMode = 0;
    std::atomic<int> sampleVersion { 0 };

    juce::MidiKeyboardState keyboardState;

    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<bool> rendering { false };

    juce::SpinLock renderLock;
    rs::RenderSet::Ptr pendingRender;
    std::array<rs::RenderSet::Ptr, 16> retiredRenders;

    std::unique_ptr<RenderThread> renderThread;

    juce::CriticalSection errorLock;
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessSampleProcessor)
};
