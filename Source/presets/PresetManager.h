#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"

namespace rs
{
/** The plug-in side of preset loading: sample choice and chop points are not parameters. */
struct PresetHost
{
    virtual ~PresetHost() = default;
    virtual juce::ValueTree getSampleState() const = 0;
    virtual void applySampleState (const juce::ValueTree& state) = 0;
    virtual void applyFactorySample (int factoryIndex, int chopMode) = 0;
};

/**
    Factory presets (built into the plug-in), user presets (files) and likes.
    Likes and user presets are stored in the user's application data folder,
    so they are shared by every instance of the plug-in. Message thread only.
*/
class PresetManager : public juce::ChangeBroadcaster,
                      private juce::Timer
{
public:
    enum class Bank { all = 0, factory, liked, user };

    struct Preset
    {
        juce::String id;
        juce::String name;
        juce::String category;
        bool isFactory = false;
        int factoryIndex = -1; // index into getFactoryPresets()
        juce::File file;       // user presets only
    };

    static inline const juce::String fileExtension { ".rspreset" };
    static juce::File defaultRootDirectory();

    PresetManager (juce::AudioProcessorValueTreeState& state, PresetHost& host, juce::File rootDirectory = defaultRootDirectory());
    ~PresetManager() override;

    void refresh();

    const std::vector<Preset>& getPresets() const { return presets; }
    std::vector<Preset> getFiltered (Bank bank, const juce::String& search = {}, const juce::String& category = {}) const;
    juce::StringArray getCategories() const;
    const Preset* findById (const juce::String& id) const;

    bool isLiked (const juce::String& id) const { return likes.contains (id); }

    /** Re-reads likes and user presets if another plug-in instance changed them. */
    void reloadIfChangedOnDisk();
    void setLiked (const juce::String& id, bool liked);
    void toggleLiked (const juce::String& id) { setLiked (id, ! isLiked (id)); }
    int getNumLiked() const { return likes.size(); }

    bool loadPreset (const Preset& preset);
    bool loadPresetById (const juce::String& id);

    /** Steps through the presets of a bank (wrapping around). */
    void loadAdjacent (int delta, Bank bank);

    juce::Result saveUserPreset (const juce::String& name, const juce::String& category);
    bool userPresetExists (const juce::String& name) const;
    juce::Result deleteUserPreset (const Preset& preset);
    juce::Result renameUserPreset (const Preset& preset, const juce::String& newName);

    /** Thread-safe: hosts may save the session from a background thread. */
    juce::String getCurrentPresetId() const;
    juce::String getCurrentPresetName() const;
    void setCurrentPresetId (const juce::String& id);

    juce::File getUserPresetDirectory() const { return rootDir.getChildFile ("Presets"); }
    juce::File getLikesFile() const { return rootDir.getChildFile ("likes.json"); }

    /** The preset file format, exposed for tests. */
    juce::ValueTree createPresetState (const juce::String& name, const juce::String& category) const;
    void applyPresetState (const juce::ValueTree& preset);

private:
    void timerCallback() override { reloadIfChangedOnDisk(); }
    void loadLikes();
    void saveLikes() const;
    juce::String diskSignature() const;
    void setCurrentId (const juce::String& id);
    void applyFactory (const FactoryPreset& preset);

    juce::AudioProcessorValueTreeState& apvts;
    PresetHost& host;
    juce::File rootDir;
    std::vector<Preset> presets;
    juce::StringArray likes;
    juce::String currentId;
    juce::CriticalSection idLock;
    juce::String lastDiskSignature;
};
} // namespace rs
