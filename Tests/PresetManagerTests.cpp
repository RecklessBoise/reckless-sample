#include "presets/PresetManager.h"
#include "Parameters.h"

namespace rs
{
namespace
{
    class DummyProcessor : public juce::AudioProcessor
    {
    public:
        DummyProcessor() : apvts (*this, nullptr, "Test", createParameterLayout()) {}
        const juce::String getName() const override { return "Dummy"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        juce::AudioProcessorValueTreeState apvts;
    };

    struct DummyHost : PresetHost
    {
        juce::ValueTree getSampleState() const override
        {
            juce::ValueTree v ("SampleRef");
            v.setProperty ("marker", marker, nullptr);
            return v;
        }
        void applySampleState (const juce::ValueTree& s) override { marker = s.getProperty ("marker"); }
        void applyFactorySample (int index, int) override { factoryIndex = index; }

        int marker = 0;
        int factoryIndex = -1;
    };

    float value (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        return apvts.getRawParameterValue (id)->load();
    }
} // namespace

class PresetManagerTests : public juce::UnitTest
{
public:
    PresetManagerTests() : juce::UnitTest ("Preset manager", "RecklessSample") {}

    void runTest() override
    {
        const auto root = juce::File::createTempFile ("reckless-presets");
        root.createDirectory();
        DummyProcessor processor;
        DummyHost host;

        {
            PresetManager manager (processor.apvts, host, root);

            beginTest ("Factory bank is large and unique");
            const auto factory = manager.getFiltered (PresetManager::Bank::factory);
            expect (factory.size() >= 300, "factory presets: " + juce::String ((int) factory.size()));
            juce::StringArray ids;
            for (const auto& p : factory)
                ids.add (p.id);
            ids.removeDuplicates (false);
            expectEquals (ids.size(), (int) factory.size());

            beginTest ("Loading a factory preset sets parameters and sample");
            const auto* chip = manager.findById ("factory:Lofi Keys - Chipmunk");
            expect (chip != nullptr);
            manager.loadPreset (*chip);
            expectEquals (value (processor.apvts, ParamID::pitch), 12.0f);
            expectEquals (host.factoryIndex, 5);
            expectEquals (manager.getCurrentPresetName(), juce::String ("Lofi Keys - Chipmunk"));

            beginTest ("Save, reload and list user presets");
            processor.apvts.getParameter (ParamID::reverbMix)->setValueNotifyingHost (0.42f);
            host.marker = 7;
            expect (manager.saveUserPreset ("My Sound", "Keys").wasOk());
            expect (manager.findById ("user:My Sound") != nullptr);
            expectEquals (manager.getFiltered (PresetManager::Bank::user).size(), (size_t) 1);

            processor.apvts.getParameter (ParamID::reverbMix)->setValueNotifyingHost (0.0f);
            host.marker = 0;
            expect (manager.loadPresetById ("user:My Sound"));
            expectWithinAbsoluteError (value (processor.apvts, ParamID::reverbMix), 0.42f, 0.001f);
            expectEquals (host.marker, 7);

            beginTest ("Likes build the liked bank and persist");
            manager.setLiked ("user:My Sound", true);
            manager.setLiked ("factory:Init", true);
            expectEquals (manager.getFiltered (PresetManager::Bank::liked).size(), (size_t) 2);

            beginTest ("Rename keeps the like");
            expect (manager.renameUserPreset (*manager.findById ("user:My Sound"), "Renamed").wasOk());
            expect (manager.isLiked ("user:Renamed"));
            expect (! manager.isLiked ("user:My Sound"));

            beginTest ("Invalid names are rejected");
            expect (manager.saveUserPreset ("   ", "User").failed());
        }

        {
            PresetManager reloaded (processor.apvts, host, root);
            expect (reloaded.isLiked ("factory:Init"));
            expect (reloaded.isLiked ("user:Renamed"));

            beginTest ("Delete removes file and like");
            expect (reloaded.deleteUserPreset (*reloaded.findById ("user:Renamed")).wasOk());
            expect (reloaded.findById ("user:Renamed") == nullptr);
            expect (! reloaded.isLiked ("user:Renamed"));

            beginTest ("Next/previous wraps within a bank");
            reloaded.loadPresetById ("factory:Init");
            reloaded.loadAdjacent (-1, PresetManager::Bank::liked);
            expectEquals (reloaded.getCurrentPresetId(), juce::String ("factory:Init"));
        }

        root.deleteRecursively();
    }
};

static PresetManagerTests presetManagerTests;
} // namespace rs
