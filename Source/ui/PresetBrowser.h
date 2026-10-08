#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../presets/PresetManager.h"

namespace rs
{
/** Slide-over panel listing presets by bank (All / Factory / Liked / User) with search and categories. */
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::ChangeListener
{
public:
    explicit PresetBrowser (PresetManager& manager);
    ~PresetBrowser() override;

    PresetManager::Bank getBank() const { return bank; }
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override {}
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void setBank (PresetManager::Bank newBank);
    void rebuild();
    void showUserMenu (const PresetManager::Preset& preset);

    PresetManager& manager;
    PresetManager::Bank bank = PresetManager::Bank::all;
    std::vector<PresetManager::Preset> rows;

    juce::TextButton allTab { "ALL" }, factoryTab { "FACTORY" }, likedTab, userTab { "USER" }, closeButton { "X" };
    juce::TextEditor search;
    juce::ComboBox category;
    juce::ListBox list;
    juce::Label countLabel;
};
} // namespace rs
