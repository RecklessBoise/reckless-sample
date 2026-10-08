#pragma once

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/LightField.h"
#include "ui/PadGrid.h"
#include "ui/PresetBar.h"
#include "ui/PresetBrowser.h"
#include "ui/Theme.h"
#include "ui/WaveformView.h"

class RecklessSampleEditor : public juce::AudioProcessorEditor,
                             public juce::FileDragAndDropTarget,
                             private juce::Timer
{
public:
    explicit RecklessSampleEditor (RecklessSampleProcessor&);
    ~RecklessSampleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    /** Draws the panels and labels between the animated background and the controls. */
    class Chrome : public juce::Component
    {
    public:
        explicit Chrome (RecklessSampleEditor& e) : editor (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;

    private:
        RecklessSampleEditor& editor;
    };

    void timerCallback() override;
    void layout();
    void refreshSample();
    void showScaleMenu();
    void chooseSampleFile();
    void toggleBrowser();
    void showError (const juce::String& message);

    RecklessSampleProcessor& owner;
    rs::theme::LookAndFeel lookAndFeel;

    juce::Component content;
    rs::LightField lightField;
    Chrome chrome { *this };

    rs::PresetBar presetBar;
    juce::TextButton scaleButton;

    juce::TextButton prevSample { "<" }, nextSample { ">" }, loadButton { "LOAD SAMPLE" };
    juce::Label sampleCounter, sampleName;
    rs::Segmented chopMode;

    rs::WaveformView waveform;
    rs::PadGrid pads;

    // Play
    rs::Knob volume, length, lengthDiv, attack, release;
    rs::Toggle latch, clickFree, reverse, lengthSync;

    // Pitch & tempo
    rs::Knob pitch, fine, duration;
    rs::Toggle keepSpeed;
    rs::Segmented syncMode;
    rs::NumberBox sampleBpm, targetBpm;
    juce::TextButton detectButton { "DETECT" }, doubleButton, halveButton;
    juce::Label dawBpm, tempoRatio;

    // FX
    rs::Segmented filterType;
    rs::Knob cutoff, resonance, drive, crush, chorus, delay, delayTime, feedback, reverb, room;

    rs::PresetBrowser browser;
    juce::Label toast;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<juce::FileChooser> fileChooser;
    int lastSampleVersion = -1;
    int toastFrames = 0;
    int openFrames = 0;          // timer ticks since the editor opened
    bool scaleFromMenu = false;
    bool userResized = false;
    std::array<bool, rs::numChops> keyDown {};

    juce::Rectangle<int> playPanel, pitchPanel, fxPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessSampleEditor)
};
