#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../presets/PresetManager.h"

namespace rs
{
/** A heart that is filled when the preset is liked. */
class HeartButton : public juce::Button
{
public:
    HeartButton() : juce::Button ("Like") { setClickingTogglesState (false); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/** Header preset controls: browser toggle, previous/next, name menu, like and save. */
class PresetBar : public juce::Component, private juce::ChangeListener
{
public:
    PresetBar (PresetManager& manager);
    ~PresetBar() override;

    std::function<void()> onToggleBrowser;
    std::function<PresetManager::Bank()> getBank = [] { return PresetManager::Bank::all; };

    void setBrowserOpen (bool open) { browseButton.setToggleState (open, juce::dontSendNotification); }
    void showSaveDialog();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    void showPresetMenu();

    PresetManager& manager;
    juce::TextButton browseButton { "BROWSE" }, prevButton { "<" }, nextButton { ">" }, nameButton, saveButton { "SAVE" };
    HeartButton likeButton;
    std::unique_ptr<juce::AlertWindow> saveDialog;
};
} // namespace rs
