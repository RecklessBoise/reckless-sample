// Renders the plug-in editor to PNG files while a few chops play, for the README
// and for checking the animated UI without a DAW.
#include "PluginProcessor.h"
#include <CoreFoundation/CoreFoundation.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
class FakeAudio : public juce::Thread
{
public:
    explicit FakeAudio (juce::AudioProcessor& p) : juce::Thread ("fake audio"), processor (p) {}
    void run() override
    {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        while (! threadShouldExit())
        {
            processor.processBlock (buffer, midi);
            wait (10);
        }
    }
    juce::AudioProcessor& processor;
};
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& processor = dynamic_cast<RecklessSampleProcessor&> (*base);
    std::cout << "processor created" << std::endl;
    processor.prepareToPlay (48000.0, 512);
    std::cout << "prepared" << std::endl;

    FakeAudio audio (processor);
    audio.startThread();

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());

    // Pump the main run loop by hand so JUCE timers (the animation) keep running.
    auto pump = [] (int ms)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
        while (juce::Time::getMillisecondCounter() < end)
            CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.005, false);
    };

    auto shoot = [&] (const juce::String& name, float scale)
    {
        editor->setSize (juce::roundToInt (1000 * scale), juce::roundToInt (640 * scale));
        pump (150);
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto file = outDir.getChildFile (name);
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::cout << "wrote " << file.getFullPathName() << std::endl;
    };

    pump (500);
    shoot ("idle.png", 1.0f);
    processor.triggerPadFromUi (2, true);
    processor.triggerPadFromUi (5, true);
    pump (250);
    shoot ("playing.png", 1.0f);
    pump (300);
    shoot ("playing-150.png", 1.5f);
    processor.triggerPadFromUi (2, false);
    processor.triggerPadFromUi (5, false);
    shoot ("small-60.png", 0.6f);

    // Preset browser with a couple of liked presets.
    processor.presetManager->setLiked ("factory:Lofi Keys - Dusty Tape", true);
    processor.presetManager->setLiked ("factory:Dusty Boom Bap - Clean", true);
    std::function<juce::Button* (juce::Component&)> findBrowse = [&] (juce::Component& c) -> juce::Button*
    {
        if (auto* b = dynamic_cast<juce::Button*> (&c); b != nullptr && b->getButtonText() == "BROWSE")
            return b;
        for (auto* child : c.getChildren())
            if (auto* found = findBrowse (*child))
                return found;
        return nullptr;
    };
    if (auto* browse = findBrowse (*editor))
        browse->triggerClick();
    pump (200);
    shoot ("browser.png", 1.0f);
    processor.presetManager->setLiked ("factory:Lofi Keys - Dusty Tape", false);
    processor.presetManager->setLiked ("factory:Dusty Boom Bap - Clean", false);

    audio.stopThread (2000);
    editor.reset();
    return 0;
}
