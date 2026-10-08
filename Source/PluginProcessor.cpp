#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/BpmDetector.h"
#include "dsp/FactorySamples.h"

using namespace rs;

namespace
{
const juce::Identifier sampleRefTag { "SampleRef" };
const juce::Identifier editorTag { "Editor" };
const juce::Identifier presetTag { "CurrentPreset" };
const juce::Identifier chopsAttr { "chops" };
const juce::Identifier chopModeAttr { "chopMode" };
const juce::Identifier scaleAttr { "scale" };
const juce::Identifier idAttr { "id" };

constexpr int whiteKeyNotes[numChops] { 60, 62, 64, 65, 67, 69, 71, 72 };
constexpr int drumPadBase = 36;

juce::String chopsToString (const ChopStarts& c)
{
    juce::StringArray parts;
    for (float v : c)
        parts.add (juce::String (v, 6));
    return parts.joinIntoString (" ");
}

std::optional<ChopStarts> chopsFromString (const juce::String& s)
{
    const auto parts = juce::StringArray::fromTokens (s, " ", "");
    if (parts.size() != numChops)
        return std::nullopt;
    ChopStarts c {};
    for (int i = 0; i < numChops; ++i)
        c[(size_t) i] = parts[i].getFloatValue();
    return sanitiseChops (c);
}
} // namespace

//==============================================================================
class RecklessSampleProcessor::RenderThread : public juce::Thread
{
public:
    explicit RenderThread (RecklessSampleProcessor& p) : juce::Thread ("Reckless Sample render"), owner (p) {}

    void run() override
    {
        while (! threadShouldExit())
        {
            const auto wanted = owner.makeRenderParams();
            if (wanted.outputSampleRate > 0.0 && wanted.sample != nullptr && (! hasRendered || wanted != lastRendered))
            {
                owner.rendering = true;
                auto set = renderChops (wanted, [&] { return threadShouldExit() || owner.makeRenderParams() != wanted; });
                if (set != nullptr)
                {
                    owner.publishRender (set);
                    lastRendered = wanted;
                    hasRendered = true;
                }
                owner.collectRetiredRenders();
                owner.rendering = false;
                continue; // re-check straight away in case something changed during the render
            }
            owner.collectRetiredRenders();
            wait (20);
        }
    }

private:
    RecklessSampleProcessor& owner;
    RenderParams lastRendered;
    bool hasRendered = false;
};

//==============================================================================
RecklessSampleProcessor::RecklessSampleProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RecklessSample", createParameterLayout())
{
    for (const auto& id : allParameterIds())
        params[id] = apvts.getRawParameterValue (id);

    apvts.addParameterListener (ParamID::chopMode, this);
    presetManager = std::make_unique<PresetManager> (apvts, *this);

    // Start with something to play.
    presetManager->loadPresetById ("factory:" + getFactoryPresets()[1].name);

    renderThread = std::make_unique<RenderThread> (*this);
    renderThread->startThread (juce::Thread::Priority::normal);
}

RecklessSampleProcessor::~RecklessSampleProcessor()
{
    apvts.removeParameterListener (ParamID::chopMode, this);
    cancelPendingUpdate();
    renderThread->stopThread (5000);
}

//==============================================================================
void RecklessSampleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool rateChanged = ! juce::approximatelyEqual (currentSampleRate.load(), sampleRate);
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();

    // Render synchronously when the rate changes so the first notes are in tune.
    if (rateChanged)
    {
        const auto renderParams = makeRenderParams();
        if (renderParams.sample != nullptr)
            if (auto set = renderChops (renderParams))
                publishRender (set);
    }
}

void RecklessSampleProcessor::releaseResources()
{
    engine.reset();
    fx.reset();
}

bool RecklessSampleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

int RecklessSampleProcessor::noteToChop (int midiNote)
{
    for (int i = 0; i < numChops; ++i)
        if (whiteKeyNotes[i] == midiNote)
            return i;
    if (midiNote >= drumPadBase && midiNote < drumPadBase + numChops)
        return midiNote - drumPadBase;
    return -1;
}

int RecklessSampleProcessor::chopToNote (int chop)
{
    return whiteKeyNotes[juce::jlimit (0, numChops - 1, chop)];
}

void RecklessSampleProcessor::triggerPadFromUi (int chop, bool down)
{
    if (! juce::isPositiveAndBelow (chop, numChops))
        return;
    if (down)
        keyboardState.noteOn (1, chopToNote (chop), 0.9f);
    else
        keyboardState.noteOff (1, chopToNote (chop), 0.0f);
}

VoiceSettings RecklessSampleProcessor::voiceSettings() const
{
    VoiceSettings s;
    s.length = param (ParamID::length);
    if (param (ParamID::lengthSync) > 0.5f && currentSampleRate.load() > 0.0)
        s.lengthSamples = juce::jmax (16, (int) std::round (getLengthDivisionSeconds() * currentSampleRate.load()));
    s.attackMs = param (ParamID::attack);
    s.releaseMs = param (ParamID::release);
    s.latch = param (ParamID::latch) > 0.5f;
    s.clickFree = param (ParamID::clickFree) > 0.5f;
    return s;
}

FxSettings RecklessSampleProcessor::fxSettings() const
{
    FxSettings s;
    s.volume = param (ParamID::volume);
    s.filterType = (FilterType) juce::roundToInt (param (ParamID::filterType));
    s.cutoff = param (ParamID::cutoff);
    s.resonance = param (ParamID::resonance);
    s.drive = param (ParamID::drive);
    s.crush = param (ParamID::crush);
    s.chorusMix = param (ParamID::chorusMix);
    s.delayMix = param (ParamID::delayMix);
    s.delayBeats = delayDivisionsInBeats[juce::jlimit (0, 5, juce::roundToInt (param (ParamID::delayTime)))];
    s.delayFeedback = param (ParamID::delayFb);
    s.reverbMix = param (ParamID::reverbMix);
    s.reverbSize = param (ParamID::reverbSize);
    s.bpm = getGrooveBpm();
    return s;
}

void RecklessSampleProcessor::handleMidi (const juce::MidiMessage& m, const VoiceSettings& settings)
{
    if (m.isNoteOn())
    {
        engine.noteOn (noteToChop (m.getNoteNumber()), m.getFloatVelocity(), settings);
    }
    else if (m.isNoteOff())
    {
        engine.noteOff (noteToChop (m.getNoteNumber()), settings);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        engine.allNotesOff();
    }
}

void RecklessSampleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                hostBpm = *bpm;
            hostPlaying = position->getIsPlaying();
        }
    }

    // Pick up freshly rendered chops. The old set is parked in retiredRenders so it
    // is freed later on the render thread, never here.
    {
        const juce::SpinLock::ScopedTryLockType lock (renderLock);
        if (lock.isLocked() && pendingRender != nullptr)
        {
            auto slot = std::find (retiredRenders.begin(), retiredRenders.end(), nullptr);
            if (slot != retiredRenders.end())
            {
                *slot = engine.getRenderSet();
                engine.setRenderSet (pendingRender);
                pendingRender = nullptr; // still referenced by the engine, so nothing is freed
            }
        }
    }

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    const auto settings = voiceSettings();
    int position = 0;
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        if (eventPos > position)
            engine.render (buffer, position, eventPos - position, settings);
        position = eventPos;
        handleMidi (metadata.getMessage(), settings);
    }
    if (position < numSamples)
        engine.render (buffer, position, numSamples - position, settings);

    if (buffer.getNumChannels() > 1)
        fx.process (buffer, fxSettings());
}

//==============================================================================
void RecklessSampleProcessor::publishRender (RenderSet::Ptr set)
{
    RenderSet::Ptr previous;
    {
        const juce::SpinLock::ScopedLockType lock (renderLock);
        previous = std::move (pendingRender);
        pendingRender = std::move (set);
    }
    // 'previous' (never picked up by the audio thread) is released here, off the audio thread.
}

void RecklessSampleProcessor::collectRetiredRenders()
{
    std::vector<RenderSet::Ptr> toFree;
    {
        const juce::SpinLock::ScopedLockType lock (renderLock);
        for (auto& r : retiredRenders)
        {
            // Only our reference left: no voice is crossfading from it any more.
            if (r != nullptr && r->getReferenceCount() == 1)
            {
                toFree.push_back (std::move (r));
                r = nullptr;
            }
        }
    }
}

RenderParams RecklessSampleProcessor::makeRenderParams() const
{
    RenderParams p;
    {
        const std::lock_guard<std::mutex> lock (sampleMutex);
        p.sample = sample;
        p.chopStarts = chopStarts;
    }
    p.outputSampleRate = currentSampleRate.load();
    p.pitchSemitones = param (ParamID::pitch) + param (ParamID::fine) / 100.0f;
    p.keepSpeed = param (ParamID::keepSpeed) > 0.5f;
    p.duration = param (ParamID::duration);
    p.syncMode = (SyncMode) juce::roundToInt (param (ParamID::syncMode));
    p.sampleBpm = param (ParamID::sampleBpm);
    p.targetBpm = getEffectiveTargetBpm();
    p.reverse = param (ParamID::reverse) > 0.5f;
    return p;
}

double RecklessSampleProcessor::getGrooveBpm() const
{
    if (hostBpm.load() > 0.0)
        return hostBpm.load();
    const auto mode = (SyncMode) juce::roundToInt (param (ParamID::syncMode));
    return mode == SyncMode::manual ? param (ParamID::manualBpm) : param (ParamID::sampleBpm);
}

double RecklessSampleProcessor::getLengthDivisionSeconds() const
{
    const int index = juce::jlimit (0, (int) std::size (lengthDivisionsInBeats) - 1, juce::roundToInt (param (ParamID::lengthDiv)));
    return lengthDivisionsInBeats[index] * 60.0 / juce::jlimit (20.0, 400.0, getGrooveBpm());
}

std::array<float, numChops> RecklessSampleProcessor::getAudibleFractions() const
{
    std::array<float, numChops> fractions;
    fractions.fill (param (ParamID::length));
    if (param (ParamID::lengthSync) < 0.5f)
        return fractions;

    const auto p = makeRenderParams();
    if (p.sample == nullptr)
        return fractions;
    const auto starts = sanitiseChops (p.chopStarts);
    const double total = p.sample->getLengthSeconds() * p.durationFactor();
    const double division = getLengthDivisionSeconds();
    for (int i = 0; i < numChops; ++i)
    {
        const double chopSeconds = ((i + 1 < numChops ? starts[(size_t) i + 1] : 1.0f) - starts[(size_t) i]) * total;
        fractions[(size_t) i] = chopSeconds > 0.0 ? (float) juce::jmin (1.0, division / chopSeconds) : 1.0f;
    }
    return fractions;
}

double RecklessSampleProcessor::getRenderedSampleSeconds() const
{
    const auto p = makeRenderParams();
    return p.sample != nullptr ? p.sample->getLengthSeconds() * p.durationFactor() : 0.0;
}

double RecklessSampleProcessor::getEffectiveTargetBpm() const
{
    const auto mode = (SyncMode) juce::roundToInt (param (ParamID::syncMode));
    if (mode == SyncMode::host)
        // Without a host tempo (e.g. standalone), play at the sample's own speed.
        return hostBpm.load() > 0.0 ? hostBpm.load() : (double) param (ParamID::sampleBpm);
    return param (ParamID::manualBpm);
}

//==============================================================================
void RecklessSampleProcessor::setSample (SampleData::Ptr newSample, std::optional<ChopStarts> chops, bool updateBpm)
{
    if (newSample == nullptr)
        return;

    const int mode = juce::roundToInt (param (ParamID::chopMode));
    const auto starts = chops.has_value() ? *chops : computeChops (*newSample, (ChopMode) mode);
    {
        const std::lock_guard<std::mutex> lock (sampleMutex);
        sample = newSample;
        chopStarts = starts;
        chopsComputedForMode = mode;
    }

    if (updateBpm)
        if (auto* p = apvts.getParameter (ParamID::sampleBpm))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) newSample->bpm));

    ++sampleVersion;
}

bool RecklessSampleProcessor::loadSampleFile (const juce::File& file, juce::String& error)
{
    auto data = rs::loadSampleFile (file, error);
    if (data == nullptr)
        return false;
    setSample (data, std::nullopt, true);
    presetManager->setCurrentPresetId ({});
    return true;
}

void RecklessSampleProcessor::loadFactorySample (int index)
{
    setSample (createFactorySample (index), std::nullopt, true);
}

void RecklessSampleProcessor::stepSample (int delta)
{
    const auto current = getSample();
    if (current != nullptr && current->source.kind == SampleSource::Kind::file)
    {
        // Browse the other audio files in the same folder, like a sample browser.
        const auto dir = current->source.file.getParentDirectory();
        auto files = dir.findChildFiles (juce::File::findFiles, false, supportedAudioExtensions().joinIntoString (";"));
        files.sort();
        const int count = files.size();
        if (count > 0)
        {
            const int index = juce::jmax (0, files.indexOf (current->source.file));
            for (int attempt = 1; attempt <= count; ++attempt)
            {
                juce::String error;
                if (loadSampleFile (files[((index + delta * attempt) % count + count) % count], error))
                    return;
            }
        }
        return;
    }

    const int count = getNumFactorySamples();
    const int index = current != nullptr ? current->source.factoryIndex : 0;
    loadFactorySample (((index + delta) % count + count) % count);
}

void RecklessSampleProcessor::reportError (const juce::String& message)
{
    const juce::ScopedLock lock (errorLock);
    lastError = message;
}

juce::String RecklessSampleProcessor::takeError()
{
    const juce::ScopedLock lock (errorLock);
    return std::exchange (lastError, {});
}

juce::String RecklessSampleProcessor::getSampleCounterText() const
{
    const auto current = getSample();
    if (current == nullptr)
        return "-";
    if (current->source.kind == SampleSource::Kind::factory)
        return juce::String (current->source.factoryIndex + 1) + " / " + juce::String (getNumFactorySamples());
    return "USER";
}

void RecklessSampleProcessor::redetectBpm()
{
    const auto current = getSample();
    if (current == nullptr)
        return;
    const auto result = detectBpm (current->audio, current->sampleRate, current->bpmIsKnown ? current->name : juce::String());
    const double bpm = current->bpmIsKnown && current->source.kind == SampleSource::Kind::factory ? current->bpm : result.bpm;
    if (auto* p = apvts.getParameter (ParamID::sampleBpm))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) bpm));
}

void RecklessSampleProcessor::scaleSampleBpm (double factor)
{
    if (auto* p = apvts.getParameter (ParamID::sampleBpm))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (param (ParamID::sampleBpm) * factor)));
}

SampleData::Ptr RecklessSampleProcessor::getSample() const
{
    const std::lock_guard<std::mutex> lock (sampleMutex);
    return sample;
}

ChopStarts RecklessSampleProcessor::getChopStarts() const
{
    const std::lock_guard<std::mutex> lock (sampleMutex);
    return chopStarts;
}

void RecklessSampleProcessor::setChopStarts (const ChopStarts& starts)
{
    {
        const std::lock_guard<std::mutex> lock (sampleMutex);
        chopStarts = sanitiseChops (starts);
    }
    ++sampleVersion;
}

void RecklessSampleProcessor::parameterChanged (const juce::String& id, float)
{
    if (id == ParamID::chopMode)
        triggerAsyncUpdate();
}

void RecklessSampleProcessor::handleAsyncUpdate()
{
    const int mode = juce::roundToInt (param (ParamID::chopMode));
    SampleData::Ptr current;
    {
        const std::lock_guard<std::mutex> lock (sampleMutex);
        if (mode == chopsComputedForMode || sample == nullptr)
            return;
        current = sample;
    }
    setSample (current, computeChops (*current, (ChopMode) mode), false);
}

//==============================================================================
juce::ValueTree RecklessSampleProcessor::getSampleState() const
{
    juce::ValueTree v (sampleRefTag);
    const auto current = getSample();
    if (current != nullptr)
        v.appendChild (current->source.toValueTree(), nullptr);
    v.setProperty (chopsAttr, chopsToString (getChopStarts()), nullptr);
    v.setProperty (chopModeAttr, chopsComputedForMode, nullptr);
    return v;
}

void RecklessSampleProcessor::applySampleState (const juce::ValueTree& state)
{
    const auto source = SampleSource::fromValueTree (state.getChild (0));
    const auto chops = chopsFromString (state.getProperty (chopsAttr).toString());

    SampleData::Ptr data;
    if (source.kind == SampleSource::Kind::factory)
    {
        data = createFactorySample (source.factoryIndex);
    }
    else if (source.kind == SampleSource::Kind::file)
    {
        juce::String error;
        data = rs::loadSampleFile (source.file, error);
        if (data == nullptr)
            reportError ("Sample introuvable : " + source.file.getFullPathName());
    }

    // The sample BPM parameter is already restored, so keep it.
    if (data != nullptr)
        setSample (data, chops, false);
}

void RecklessSampleProcessor::applyFactorySample (int factoryIndex, int)
{
    setSample (createFactorySample (factoryIndex), std::nullopt, false);
}

//==============================================================================
void RecklessSampleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (const auto& tag : { sampleRefTag, editorTag, presetTag })
        state.removeChild (state.getChildWithName (tag), nullptr);

    state.appendChild (getSampleState(), nullptr);

    juce::ValueTree editor (editorTag);
    editor.setProperty (scaleAttr, uiScale.load(), nullptr);
    state.appendChild (editor, nullptr);

    juce::ValueTree preset (presetTag);
    preset.setProperty (idAttr, presetManager->getCurrentPresetId(), nullptr);
    state.appendChild (preset, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RecklessSampleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const auto sampleRef = state.getChildWithName (sampleRefTag).createCopy();
    const auto editor = state.getChildWithName (editorTag);
    const auto preset = state.getChildWithName (presetTag);

    if (editor.isValid())
        uiScale = juce::jlimit (0.5f, 2.0f, (float) editor.getProperty (scaleAttr, 1.0f));

    for (const auto& tag : { sampleRefTag, editorTag, presetTag })
        state.removeChild (state.getChildWithName (tag), nullptr);
    apvts.replaceState (state);

    if (sampleRef.isValid())
        applySampleState (sampleRef);
    if (preset.isValid())
        presetManager->setCurrentPresetId (preset.getProperty (idAttr).toString());
}

//==============================================================================
juce::AudioProcessorEditor* RecklessSampleProcessor::createEditor()
{
    return new RecklessSampleEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RecklessSampleProcessor();
}
