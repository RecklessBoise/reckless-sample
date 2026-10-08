#include "PresetManager.h"
#include "../Parameters.h"

namespace rs
{
namespace
{
    const juce::Identifier presetTag { "RecklessPreset" };
    const juce::Identifier paramsTag { "Parameters" };
    const juce::Identifier paramTag { "Param" };
    const juce::Identifier sampleStateTag { "SampleState" };
    const juce::Identifier nameAttr { "name" };
    const juce::Identifier categoryAttr { "category" };
    const juce::Identifier versionAttr { "version" };
    const juce::Identifier idAttr { "id" };
    const juce::Identifier valueAttr { "value" };

    const juce::String factoryPrefix { "factory:" };
    const juce::String userPrefix { "user:" };

    void setParameter (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float realValue)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }

    void resetParametersToDefault (juce::AudioProcessorValueTreeState& apvts)
    {
        for (const auto& id : allParameterIds())
            if (auto* param = apvts.getParameter (id))
                param->setValueNotifyingHost (param->getDefaultValue());
    }
} // namespace

juce::File PresetManager::defaultRootDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
        .getChildFile ("Application Support")
#endif
        .getChildFile ("Reckless Sample");
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state, PresetHost& h, juce::File root)
    : apvts (state), host (h), rootDir (std::move (root))
{
    getUserPresetDirectory().createDirectory();
    loadLikes();
    refresh();
}

void PresetManager::refresh()
{
    presets.clear();
    const auto& factory = getFactoryPresets();
    for (size_t i = 0; i < factory.size(); ++i)
        presets.push_back ({ factoryPrefix + factory[i].name, factory[i].name, factory[i].category, true, (int) i, {} });

    auto files = getUserPresetDirectory().findChildFiles (juce::File::findFiles, false, "*" + fileExtension);
    files.sort();
    for (const auto& file : files)
    {
        juce::String category = "User";
        if (auto xml = juce::XmlDocument::parse (file))
            category = xml->getStringAttribute (categoryAttr, "User");
        presets.push_back ({ userPrefix + file.getFileNameWithoutExtension(), file.getFileNameWithoutExtension(), category, false, -1, file });
    }
    sendChangeMessage();
}

std::vector<PresetManager::Preset> PresetManager::getFiltered (Bank bank, const juce::String& search, const juce::String& category) const
{
    std::vector<Preset> out;
    for (const auto& p : presets)
    {
        if (bank == Bank::factory && ! p.isFactory)
            continue;
        if (bank == Bank::user && p.isFactory)
            continue;
        if (bank == Bank::liked && ! isLiked (p.id))
            continue;
        if (category.isNotEmpty() && p.category != category)
            continue;
        if (search.isNotEmpty() && ! p.name.containsIgnoreCase (search) && ! p.category.containsIgnoreCase (search))
            continue;
        out.push_back (p);
    }
    return out;
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray categories;
    for (const auto& p : presets)
        categories.addIfNotAlreadyThere (p.category);
    return categories;
}

const PresetManager::Preset* PresetManager::findById (const juce::String& id) const
{
    for (const auto& p : presets)
        if (p.id == id)
            return &p;
    return nullptr;
}

void PresetManager::setLiked (const juce::String& id, bool liked)
{
    if (liked == isLiked (id))
        return;
    if (liked)
        likes.add (id);
    else
        likes.removeString (id);
    saveLikes();
    sendChangeMessage();
}

void PresetManager::loadLikes()
{
    likes.clear();
    const auto parsed = juce::JSON::parse (getLikesFile());
    if (auto* array = parsed.getArray())
        for (const auto& v : *array)
            likes.addIfNotAlreadyThere (v.toString());
}

void PresetManager::saveLikes() const
{
    juce::Array<juce::var> array;
    for (const auto& id : likes)
        array.add (id);
    rootDir.createDirectory();
    getLikesFile().replaceWithText (juce::JSON::toString (juce::var (array)));
}

void PresetManager::applyFactory (const FactoryPreset& preset)
{
    resetParametersToDefault (apvts);
    setParameter (apvts, ParamID::chopMode, (float) preset.chopMode);
    for (const auto& [id, value] : preset.values)
        setParameter (apvts, id, value);
    host.applyFactorySample (preset.sampleIndex, preset.chopMode);
}

juce::ValueTree PresetManager::createPresetState (const juce::String& name, const juce::String& category) const
{
    juce::ValueTree preset (presetTag);
    preset.setProperty (versionAttr, 1, nullptr);
    preset.setProperty (nameAttr, name, nullptr);
    preset.setProperty (categoryAttr, category, nullptr);

    juce::ValueTree params (paramsTag);
    for (const auto& id : allParameterIds())
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            juce::ValueTree p (paramTag);
            p.setProperty (idAttr, id, nullptr);
            p.setProperty (valueAttr, param->convertFrom0to1 (param->getValue()), nullptr);
            params.appendChild (p, nullptr);
        }
    }
    preset.appendChild (params, nullptr);

    juce::ValueTree sample (sampleStateTag);
    sample.appendChild (host.getSampleState().createCopy(), nullptr);
    preset.appendChild (sample, nullptr);
    return preset;
}

void PresetManager::applyPresetState (const juce::ValueTree& preset)
{
    resetParametersToDefault (apvts);
    for (const auto& p : preset.getChildWithName (paramsTag))
        setParameter (apvts, p.getProperty (idAttr).toString(), (float) p.getProperty (valueAttr));

    const auto sample = preset.getChildWithName (sampleStateTag);
    if (sample.getNumChildren() > 0)
        host.applySampleState (sample.getChild (0));
}

bool PresetManager::loadPreset (const Preset& preset)
{
    if (preset.isFactory)
    {
        const auto& factory = getFactoryPresets();
        if (! juce::isPositiveAndBelow (preset.factoryIndex, (int) factory.size()))
            return false;
        applyFactory (factory[(size_t) preset.factoryIndex]);
    }
    else
    {
        auto xml = juce::XmlDocument::parse (preset.file);
        if (xml == nullptr || ! xml->hasTagName (presetTag.toString()))
            return false;
        applyPresetState (juce::ValueTree::fromXml (*xml));
    }

    currentId = preset.id;
    sendChangeMessage();
    return true;
}

bool PresetManager::loadPresetById (const juce::String& id)
{
    if (const auto* p = findById (id))
        return loadPreset (*p);
    return false;
}

void PresetManager::loadAdjacent (int delta, Bank bank)
{
    const auto list = getFiltered (bank);
    if (list.empty())
        return;

    int index = -1;
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].id == currentId)
            index = (int) i;

    const int size = (int) list.size();
    const int next = index < 0 ? (delta > 0 ? 0 : size - 1) : ((index + delta) % size + size) % size;
    loadPreset (list[(size_t) next]);
}

juce::Result PresetManager::saveUserPreset (const juce::String& name, const juce::String& category)
{
    const auto cleanName = juce::File::createLegalFileName (name.trim());
    if (cleanName.isEmpty())
        return juce::Result::fail ("Donne un nom au preset.");

    const auto state = createPresetState (cleanName, category.trim().isEmpty() ? "User" : category.trim());
    const auto file = getUserPresetDirectory().getChildFile (cleanName + fileExtension);
    getUserPresetDirectory().createDirectory();

    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (file))
        return juce::Result::fail ("Impossible d'enregistrer " + file.getFullPathName());

    refresh();
    currentId = userPrefix + cleanName;
    sendChangeMessage();
    return juce::Result::ok();
}

juce::Result PresetManager::deleteUserPreset (const Preset& preset)
{
    if (preset.isFactory)
        return juce::Result::fail ("Les presets d'usine ne peuvent pas être supprimés.");
    if (! preset.file.deleteFile())
        return juce::Result::fail ("Impossible de supprimer " + preset.file.getFileName());

    setLiked (preset.id, false);
    if (currentId == preset.id)
        currentId.clear();
    refresh();
    return juce::Result::ok();
}

juce::Result PresetManager::renameUserPreset (const Preset& preset, const juce::String& newName)
{
    if (preset.isFactory)
        return juce::Result::fail ("Les presets d'usine ne peuvent pas être renommés.");

    const auto cleanName = juce::File::createLegalFileName (newName.trim());
    if (cleanName.isEmpty())
        return juce::Result::fail ("Nom invalide.");

    const auto target = getUserPresetDirectory().getChildFile (cleanName + fileExtension);
    if (target.exists())
        return juce::Result::fail ("Un preset porte déjà ce nom.");

    // Keep the name stored inside the file in sync with the file name.
    if (auto xml = juce::XmlDocument::parse (preset.file))
    {
        xml->setAttribute (nameAttr, cleanName);
        xml->writeTo (preset.file);
    }
    if (! preset.file.moveFileTo (target))
        return juce::Result::fail ("Impossible de renommer le preset.");

    const auto newId = userPrefix + cleanName;
    if (isLiked (preset.id))
    {
        likes.removeString (preset.id);
        likes.add (newId);
        saveLikes();
    }
    if (currentId == preset.id)
        currentId = newId;
    refresh();
    return juce::Result::ok();
}

juce::String PresetManager::getCurrentPresetName() const
{
    if (const auto* p = findById (currentId))
        return p->name;
    return "No Preset";
}

void PresetManager::setCurrentPresetId (const juce::String& id)
{
    currentId = id;
    sendChangeMessage();
}
} // namespace rs
