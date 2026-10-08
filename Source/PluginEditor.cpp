#include "PluginEditor.h"

using namespace rs;

namespace
{
constexpr const char* padKeys = "asdfghjk";
constexpr float scales[] { 0.6f, 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
} // namespace

RecklessSampleEditor::RecklessSampleEditor (RecklessSampleProcessor& p)
    : AudioProcessorEditor (p),
      owner (p),
      presetBar (*p.presetManager),
      chopMode (p.apvts, ParamID::chopMode, { "EQUAL", "TRANSIENT" }),
      volume (p.apvts, ParamID::volume, "Volume"),
      length (p.apvts, ParamID::length, "Length"),
      lengthDiv (p.apvts, ParamID::lengthDiv, "Length"),
      attack (p.apvts, ParamID::attack, "Attack"),
      release (p.apvts, ParamID::release, "Release"),
      latch (p.apvts, ParamID::latch, "LATCH"),
      clickFree (p.apvts, ParamID::clickFree, "CLICK-FREE"),
      reverse (p.apvts, ParamID::reverse, "REVERSE"),
      lengthSync (p.apvts, ParamID::lengthSync, "LENGTH SYNC"),
      pitch (p.apvts, ParamID::pitch, "Pitch"),
      fine (p.apvts, ParamID::fine, "Fine"),
      duration (p.apvts, ParamID::duration, "Duration"),
      keepSpeed (p.apvts, ParamID::keepSpeed, "KEEP SPEED"),
      syncMode (p.apvts, ParamID::syncMode, { "OFF", "DAW", "MANUAL" }),
      sampleBpm (p.apvts, ParamID::sampleBpm),
      targetBpm (p.apvts, ParamID::manualBpm),
      filterType (p.apvts, ParamID::filterType, { "LP", "BP", "HP" }),
      cutoff (p.apvts, ParamID::cutoff, "Cutoff"),
      resonance (p.apvts, ParamID::resonance, "Reso"),
      drive (p.apvts, ParamID::drive, "Drive"),
      crush (p.apvts, ParamID::crush, "Crush"),
      chorus (p.apvts, ParamID::chorusMix, "Chorus"),
      delay (p.apvts, ParamID::delayMix, "Delay"),
      delayTime (p.apvts, ParamID::delayTime, "Time"),
      feedback (p.apvts, ParamID::delayFb, "Feedback"),
      reverb (p.apvts, ParamID::reverbMix, "Reverb"),
      room (p.apvts, ParamID::reverbSize, "Room"),
      browser (*p.presetManager)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);

    content.addAndMakeVisible (lightField);
    content.addAndMakeVisible (chrome);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &presetBar, &scaleButton, &prevSample, &nextSample, &loadButton, &sampleCounter, &sampleName, &chopMode,
             &waveform, &pads, &volume, &length, &lengthDiv, &attack, &release, &latch, &clickFree, &reverse, &lengthSync, &pitch, &fine, &duration,
             &keepSpeed, &syncMode, &sampleBpm, &targetBpm, &detectButton, &doubleButton, &halveButton, &dawBpm, &tempoRatio,
             &filterType, &cutoff, &resonance, &drive, &crush, &chorus, &delay, &delayTime, &feedback, &reverb, &room })
        content.addAndMakeVisible (c);

    content.addChildComponent (browser);
    content.addChildComponent (toast);

    // Preset bar & browser.
    presetBar.onToggleBrowser = [this] { toggleBrowser(); };
    presetBar.getBank = [this] { return browser.getBank(); };
    browser.onClose = [this] { toggleBrowser(); };

    scaleButton.onClick = [this] { showScaleMenu(); };
    scaleButton.setTooltip (juce::String::fromUTF8 ("Taille de l'interface (tu peux aussi tirer le coin en bas à droite)"));

    // Sample strip.
    prevSample.onClick = [this] { owner.stepSample (-1); };
    nextSample.onClick = [this] { owner.stepSample (1); };
    loadButton.onClick = [this] { chooseSampleFile(); };
    prevSample.setTooltip (juce::String::fromUTF8 ("Sample précédent"));
    nextSample.setTooltip ("Sample suivant");
    loadButton.setTooltip ("Charger un fichier audio (ou glisse-le sur le plugin)");
    chopMode.setTooltip (juce::String::fromUTF8 ("Découpe en 8 parts égales ou sur les transitoires"));
    for (auto* l : { &sampleCounter, &sampleName })
    {
        l->setColour (juce::Label::textColourId, theme::text);
        l->setInterceptsMouseClicks (false, false);
    }
    sampleCounter.setFont (theme::caption (11.0f));
    sampleCounter.setJustificationType (juce::Justification::centred);
    sampleName.setFont (theme::font (15.0f, true, 0.04f));

    waveform.onChopsChanged = [this] (const ChopStarts& c) { owner.setChopStarts (c); };
    waveform.onChopClicked = [this] (int chop, bool down) { owner.triggerPadFromUi (chop, down); };
    pads.onPad = [this] (int chop, bool down) { owner.triggerPadFromUi (chop, down); };

    // Tooltips that explain the less obvious controls.
    keepSpeed.setTooltip (juce::String::fromUTF8 ("Allumé : le pitch change sans changer la vitesse (time-stretch). Éteint : pitch façon vinyle/varispeed."));
    syncMode.setTooltip (juce::String::fromUTF8 ("Cale le sample sur le tempo : DAW = tempo du projet, MANUAL = tempo choisi à la main"));
    sampleBpm.setTooltip (juce::String::fromUTF8 ("Tempo du sample (détecté à l'import). Glisse ou double-clique pour corriger."));
    targetBpm.setTooltip ("Tempo cible en mode MANUAL (et tempo du delay hors DAW)");
    latch.setTooltip (juce::String::fromUTF8 ("Un appui lance la boucle, un second l'arrête"));
    clickFree.setTooltip (juce::String::fromUTF8 ("Micro-fondus pour éviter les clics aux bords des chops"));
    length.slider.setTooltip (juce::String::fromUTF8 ("Portion de chaque chop qui est jouée"));
    lengthDiv.slider.setTooltip (juce::String::fromUTF8 ("Durée de chaque chop en valeur de note, calée sur le tempo du projet"));
    lengthSync.setTooltip (juce::String::fromUTF8 ("Cale la durée des chops (Length) sur le tempo du projet : 1/16, 1/8, 1/4, 1 mesure…"));
    duration.slider.setTooltip (juce::String::fromUTF8 ("Raccourcit ou allonge le sample sans changer le pitch (×2 = deux fois plus long)"));

    detectButton.setTooltip (juce::String::fromUTF8 ("Re-détecter le tempo du sample"));
    detectButton.onClick = [this] { owner.redetectBpm(); };
    doubleButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97" "2"));
    halveButton.setButtonText (juce::String::fromUTF8 ("\xc3\xb7" "2"));
    doubleButton.onClick = [this] { owner.scaleSampleBpm (2.0); };
    halveButton.onClick = [this] { owner.scaleSampleBpm (0.5); };

    for (auto* l : { &dawBpm, &tempoRatio })
    {
        l->setFont (theme::caption (10.0f));
        l->setColour (juce::Label::textColourId, theme::textDim);
        l->setJustificationType (juce::Justification::centredLeft);
    }

    toast.setFont (theme::font (13.0f));
    toast.setColour (juce::Label::backgroundColourId, theme::black.withAlpha (0.92f));
    toast.setColour (juce::Label::outlineColourId, theme::accent.withAlpha (0.5f));
    toast.setColour (juce::Label::textColourId, theme::highlight);
    toast.setJustificationType (juce::Justification::centred);

    // Resizable at a fixed aspect ratio; everything scales as vector graphics.
    setResizable (true, true);
    setResizeLimits (theme::baseWidth / 2, theme::baseHeight / 2, theme::baseWidth * 2, theme::baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) theme::baseWidth / theme::baseHeight);

    setWantsKeyboardFocus (true);
    layout();
    const float scale = owner.uiScale.load();
    setSize (juce::roundToInt ((float) theme::baseWidth * scale), juce::roundToInt ((float) theme::baseHeight * scale));

    refreshSample();
    startTimerHz (30);
}

RecklessSampleEditor::~RecklessSampleEditor()
{
    stopTimer();
    for (int i = 0; i < numChops; ++i)
        if (keyDown[(size_t) i])
            owner.triggerPadFromUi (i, false);
    setLookAndFeel (nullptr);
}

//==============================================================================
void RecklessSampleEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::black);
}

void RecklessSampleEditor::resized()
{
    const float scale = (float) getWidth() / (float) theme::baseWidth;
    content.setBounds (0, 0, theme::baseWidth, theme::baseHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
    // Only remember sizes the user chose (menu, corner or window drag). Some hosts,
    // like FL Studio, shrink the window on their own when it opens; those sizes
    // are ignored and the remembered zoom is restored by the timer.
    const bool userDrag = openFrames > 15 && juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown();
    if (scaleFromMenu || userDrag)
    {
        owner.uiScale = scale;
        userResized = true;
    }
    scaleButton.setButtonText (juce::String (juce::roundToInt (scale * 100.0f)) + "%");
}

void RecklessSampleEditor::layout()
{
    const auto full = juce::Rectangle<int> (0, 0, theme::baseWidth, theme::baseHeight);
    lightField.setBounds (full);
    chrome.setBounds (full);

    // Header.
    presetBar.setBounds (300, 14, 540, 32);
    scaleButton.setBounds (912, 16, 68, 28);

    // Sample strip.
    prevSample.setBounds (20, 58, 28, 26);
    sampleCounter.setBounds (50, 58, 64, 26);
    nextSample.setBounds (116, 58, 28, 26);
    sampleName.setBounds (154, 58, 400, 26);
    chopMode.setBounds (680, 58, 180, 26);
    loadButton.setBounds (868, 58, 112, 26);

    waveform.setBounds (20, 92, 960, 170);
    pads.setBounds (20, 272, 960, 96);

    // Panels.
    playPanel = { 20, 380, 252, 246 };
    pitchPanel = { 282, 380, 330, 246 };
    fxPanel = { 622, 380, 358, 246 };

    {
        auto r = playPanel.reduced (12).withTrimmedTop (22);
        auto row = r.removeFromTop (74);
        const int w = row.getWidth() / 4;
        volume.setBounds (row.removeFromLeft (w));
        length.setBounds (row.removeFromLeft (w));
        lengthDiv.setBounds (length.getBounds());
        attack.setBounds (row.removeFromLeft (w));
        release.setBounds (row.removeFromLeft (w));
        r.removeFromTop (14);
        auto toggles = r.removeFromTop (28);
        const int tw = toggles.getWidth() / 3;
        for (auto* t : { &latch, &clickFree, &reverse })
            t->setBounds (toggles.removeFromLeft (tw).reduced (3, 0));
        r.removeFromTop (8);
        lengthSync.setBounds (r.removeFromTop (28).removeFromLeft (tw * 2).reduced (3, 0));
    }

    {
        auto r = pitchPanel.reduced (12).withTrimmedTop (22);
        auto row = r.removeFromTop (74);
        for (auto* k : { &pitch, &fine, &duration })
            k->setBounds (row.removeFromLeft (64));
        keepSpeed.setBounds (row.withSizeKeepingCentre (row.getWidth() - 16, 34).translated (0, -8));

        r.removeFromTop (8);
        auto syncRow = r.removeFromTop (26);
        syncRow.removeFromLeft (70);
        syncMode.setBounds (syncRow);

        r.removeFromTop (10);
        auto bpmRow = r.removeFromTop (30);
        bpmRow.removeFromLeft (70);
        sampleBpm.setBounds (bpmRow.removeFromLeft (84));
        bpmRow.removeFromLeft (8);
        detectButton.setBounds (bpmRow.removeFromLeft (66).reduced (0, 2));
        bpmRow.removeFromLeft (4);
        doubleButton.setBounds (bpmRow.removeFromLeft (36).reduced (0, 2));
        bpmRow.removeFromLeft (4);
        halveButton.setBounds (bpmRow.removeFromLeft (36).reduced (0, 2));

        r.removeFromTop (8);
        auto targetRow = r.removeFromTop (30);
        targetRow.removeFromLeft (70);
        targetBpm.setBounds (targetRow.removeFromLeft (84));
        targetRow.removeFromLeft (10);
        dawBpm.setBounds (targetRow.removeFromTop (15));
        tempoRatio.setBounds (targetRow);
    }

    {
        auto r = fxPanel.reduced (12);
        filterType.setBounds (r.removeFromTop (22).removeFromRight (120));
        r.removeFromTop (6);
        auto row1 = r.removeFromTop (74);
        auto row2 = r.removeFromTop (80).withTrimmedTop (8);
        const int w = row1.getWidth() / 5;
        for (auto* k : { &cutoff, &resonance, &drive, &crush, &chorus })
            k->setBounds (row1.removeFromLeft (w));
        for (auto* k : { &delay, &delayTime, &feedback, &reverb, &room })
            k->setBounds (row2.removeFromLeft (w));
    }

    browser.setBounds (20, 54, 360, 572);
    toast.setBounds (300, 596, 400, 30);
}

//==============================================================================
void RecklessSampleEditor::Chrome::paint (juce::Graphics& g)
{
    // Logo.
    g.setFont (theme::font (24.0f, true, 0.22f));
    g.setColour (theme::highlight);
    g.drawText ("RECKLESS", 20, 12, 160, 36, juce::Justification::centredLeft);
    g.setFont (theme::font (24.0f, false, 0.22f));
    g.setColour (theme::accent);
    g.drawText ("SAMPLE", 170, 12, 120, 36, juce::Justification::centredLeft);

    theme::drawPanel (g, editor.playPanel.toFloat(), "Play");
    theme::drawPanel (g, editor.pitchPanel.toFloat(), "Pitch & Tempo");
    theme::drawPanel (g, editor.fxPanel.toFloat(), "Color & Space");

    g.setFont (theme::caption (9.5f));
    g.setColour (theme::textDim);
    const auto label = [&] (const juce::String& text, juce::Rectangle<int> r) { g.drawText (text, r, juce::Justification::centredLeft); };
    const int x = editor.pitchPanel.getX() + 12;
    label ("SYNC", { x, editor.syncMode.getY(), 60, editor.syncMode.getHeight() });
    label ("SAMPLE BPM", { x, editor.sampleBpm.getY(), 66, editor.sampleBpm.getHeight() });
    label ("TARGET BPM", { x, editor.targetBpm.getY(), 66, editor.targetBpm.getHeight() });

    // MIDI mapping reminder.
    g.setFont (theme::caption (8.5f));
    g.setColour (theme::textDim.withAlpha (0.8f));
    g.drawFittedText ("MIDI : TOUCHES BLANCHES C3-C4  /  PADS C1-G1  /  CLAVIER A S D F G H J K",
                      editor.playPanel.reduced (12).removeFromBottom (40), juce::Justification::centredLeft, 2);
}

//==============================================================================
void RecklessSampleEditor::timerCallback()
{
    std::array<float, numChops> levels {}, positions {};
    std::array<bool, numChops> latched {};
    for (size_t i = 0; i < (size_t) numChops; ++i)
    {
        levels[i] = owner.engine.padLevel[i].load();
        positions[i] = owner.engine.padPosition[i].load();
        latched[i] = owner.engine.padLatched[i].load();
    }

    // Keep the zoom the user chose when the host resizes the window by itself on open.
    if (++openFrames <= 60 && ! userResized)
    {
        const float wanted = owner.uiScale.load();
        if (std::abs ((float) getWidth() / (float) theme::baseWidth - wanted) > 0.01f)
            setSize (juce::roundToInt ((float) theme::baseWidth * wanted), juce::roundToInt ((float) theme::baseHeight * wanted));
    }

    const bool synced = owner.apvts.getRawParameterValue (ParamID::lengthSync)->load() > 0.5f;
    length.setVisible (! synced);
    lengthDiv.setVisible (synced);

    pads.setLevels (levels, latched);
    waveform.setPlayback (positions, levels, owner.getAudibleFractions());
    waveform.setRendering (owner.isRendering());
    lightField.update (owner.fx.outputLevel.load(), levels, pads.padCentres (content));

    if (owner.getSampleVersion() != lastSampleVersion)
        refreshSample();
    else
        waveform.setChops (owner.getChopStarts());

    // Tempo readouts.
    const double host = owner.getHostBpm();
    const auto mode = (SyncMode) juce::roundToInt (owner.apvts.getRawParameterValue (ParamID::syncMode)->load());
    dawBpm.setText (host > 0.0 ? "DAW " + juce::String (host, 2) + " BPM" + (owner.isHostPlaying() ? juce::String::fromUTF8 ("  \xe2\x96\xb6") : juce::String())
                               : juce::String ("DAW : PAS DE TEMPO"),
                    juce::dontSendNotification);
    dawBpm.setColour (juce::Label::textColourId, mode == SyncMode::host ? theme::accent : theme::textDim);

    const double sampleTempo = owner.apvts.getRawParameterValue (ParamID::sampleBpm)->load();
    const double ratio = mode == SyncMode::off ? 1.0 : owner.getEffectiveTargetBpm() / juce::jmax (1.0, sampleTempo);
    const double seconds = owner.getRenderedSampleSeconds();
    tempoRatio.setText (juce::String::fromUTF8 ("DUR\xc3\x89" "E ") + juce::String (seconds, 2) + " S"
                            + (mode == SyncMode::off ? juce::String() : "  /  SYNC " + juce::String (ratio, 3) + juce::String::fromUTF8 ("\xc3\x97")),
                        juce::dontSendNotification);
    targetBpm.setAlpha (mode == SyncMode::manual ? 1.0f : 0.45f);

    if (const auto error = owner.takeError(); error.isNotEmpty())
        showError (error);
    if (toastFrames > 0 && --toastFrames == 0)
        toast.setVisible (false);
}

void RecklessSampleEditor::refreshSample()
{
    lastSampleVersion = owner.getSampleVersion();
    const auto s = owner.getSample();
    waveform.setSample (s, owner.getChopStarts());
    sampleName.setText (s != nullptr ? s->name : juce::String ("Aucun sample"), juce::dontSendNotification);
    sampleCounter.setText (owner.getSampleCounterText(), juce::dontSendNotification);
}

void RecklessSampleEditor::showError (const juce::String& message)
{
    toast.setText (message, juce::dontSendNotification);
    toast.setVisible (true);
    toast.toFront (false);
    toastFrames = 30 * 4;
}

void RecklessSampleEditor::toggleBrowser()
{
    const bool open = ! browser.isVisible();
    browser.setVisible (open);
    if (open)
        browser.toFront (false);
    presetBar.setBrowserOpen (open);
}

void RecklessSampleEditor::showScaleMenu()
{
    juce::PopupMenu menu;
    const float current = (float) getWidth() / (float) theme::baseWidth;
    for (float s : scales)
        menu.addItem (juce::String (juce::roundToInt (s * 100.0f)) + "%", true, std::abs (s - current) < 0.01f,
                      [this, s]
                      {
                          scaleFromMenu = true;
                          setSize (juce::roundToInt ((float) theme::baseWidth * s), juce::roundToInt ((float) theme::baseHeight * s));
                          scaleFromMenu = false;
                      });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&scaleButton));
}

void RecklessSampleEditor::chooseSampleFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choisir un sample", juce::File(),
                                                       supportedAudioExtensions().joinIntoString (";"));
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();
                                  if (file == juce::File())
                                      return;
                                  juce::String error;
                                  if (! owner.loadSampleFile (file, error))
                                      showError (error);
                              });
}

//==============================================================================
bool RecklessSampleEditor::keyPressed (const juce::KeyPress& key)
{
    // Swallow pad keys so the host does not also react to them.
    return juce::String (padKeys).containsChar (juce::CharacterFunctions::toLowerCase (key.getTextCharacter()));
}

bool RecklessSampleEditor::keyStateChanged (bool)
{
    if (dynamic_cast<juce::TextEditor*> (getCurrentlyFocusedComponent()) != nullptr)
        return false;

    bool handled = false;
    for (int i = 0; i < numChops; ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (padKeys[i]) || juce::KeyPress::isKeyCurrentlyDown (juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) padKeys[i]));
        if (down != keyDown[(size_t) i])
        {
            keyDown[(size_t) i] = down;
            owner.triggerPadFromUi (i, down);
            handled = true;
        }
    }
    return handled;
}

bool RecklessSampleEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    const auto extensions = supportedAudioExtensions();
    for (const auto& f : files)
        for (const auto& ext : extensions)
            if (juce::File (f).hasFileExtension (ext.fromLastOccurrenceOf (".", false, false)))
                return true;
    return false;
}

void RecklessSampleEditor::filesDropped (const juce::StringArray& files, int, int)
{
    juce::String error;
    for (const auto& f : files)
        if (owner.loadSampleFile (juce::File (f), error))
            return;
    showError (error);
}
