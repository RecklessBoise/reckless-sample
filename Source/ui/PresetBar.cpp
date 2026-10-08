#include "PresetBar.h"
#include "Theme.h"

namespace rs
{
void HeartButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto area = getLocalBounds().toFloat().reduced (6.0f).withSizeKeepingCentre (18.0f, 16.0f);
    const auto heart = theme::heartPath (area);
    if (getToggleState())
    {
        g.setColour (theme::cyan.withAlpha (0.35f));
        g.fillPath (theme::heartPath (area.expanded (3.0f)));
        g.setColour (theme::heart);
        g.fillPath (heart);
    }
    else
    {
        g.setColour (highlighted ? theme::cyan : theme::textDim);
        g.strokePath (heart, juce::PathStrokeType (1.5f));
    }
}

//==============================================================================
PresetBar::PresetBar (PresetManager& m) : manager (m)
{
    for (auto* b : { &browseButton, &prevButton, &nextButton, &nameButton, &saveButton })
        addAndMakeVisible (b);
    addAndMakeVisible (likeButton);

    browseButton.setTooltip ("Ouvrir la banque de presets");
    likeButton.setTooltip ("Liker ce preset (banque LIKED)");
    saveButton.setTooltip ("Enregistrer tes reglages comme preset");

    browseButton.onClick = [this] { if (onToggleBrowser) onToggleBrowser(); };
    prevButton.onClick = [this] { manager.loadAdjacent (-1, getBank()); };
    nextButton.onClick = [this] { manager.loadAdjacent (1, getBank()); };
    nameButton.onClick = [this] { showPresetMenu(); };
    likeButton.onClick = [this]
    {
        const auto id = manager.getCurrentPresetId();
        if (id.isNotEmpty())
            manager.toggleLiked (id);
    };
    saveButton.onClick = [this] { showSaveDialog(); };

    manager.addChangeListener (this);
    refresh();
}

PresetBar::~PresetBar()
{
    manager.removeChangeListener (this);
}

void PresetBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void PresetBar::refresh()
{
    const auto id = manager.getCurrentPresetId();
    nameButton.setButtonText (manager.getCurrentPresetName());
    likeButton.setToggleState (id.isNotEmpty() && manager.isLiked (id), juce::dontSendNotification);
    likeButton.setEnabled (id.isNotEmpty());
    repaint();
}

void PresetBar::paint (juce::Graphics&) {}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    browseButton.setBounds (r.removeFromLeft (78).reduced (0, 2));
    r.removeFromLeft (8);
    saveButton.setBounds (r.removeFromRight (64).reduced (0, 2));
    r.removeFromRight (4);
    likeButton.setBounds (r.removeFromRight (34));
    r.removeFromRight (4);
    prevButton.setBounds (r.removeFromLeft (30).reduced (0, 2));
    nextButton.setBounds (r.removeFromRight (30).reduced (0, 2));
    nameButton.setBounds (r.reduced (4, 2));
}

void PresetBar::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto current = manager.getCurrentPresetId();

    auto addPresets = [&] (juce::PopupMenu& target, const std::vector<PresetManager::Preset>& list)
    {
        for (const auto& p : list)
        {
            const auto label = (manager.isLiked (p.id) ? juce::String::fromUTF8 ("\xe2\x99\xa5 ") : juce::String()) + p.name;
            target.addItem (label, true, p.id == current, [this, id = p.id] { manager.loadPresetById (id); });
        }
    };

    juce::PopupMenu liked, user, factory;
    addPresets (liked, manager.getFiltered (PresetManager::Bank::liked));
    addPresets (user, manager.getFiltered (PresetManager::Bank::user));
    for (const auto& category : manager.getCategories())
    {
        const auto list = manager.getFiltered (PresetManager::Bank::factory, {}, category);
        if (list.empty())
            continue;
        juce::PopupMenu sub;
        addPresets (sub, list);
        factory.addSubMenu (category, sub);
    }

    menu.addSectionHeader ("RECKLESS SAMPLE");
    menu.addSubMenu (juce::String::fromUTF8 ("\xe2\x99\xa5 Liked (") + juce::String (manager.getNumLiked()) + ")", liked, liked.getNumItems() > 0);
    menu.addSubMenu ("User", user, user.getNumItems() > 0);
    menu.addSubMenu ("Factory", factory);
    menu.addSeparator();
    menu.addItem ("Enregistrer...", [this] { showSaveDialog(); });
    menu.addItem ("Ouvrir le dossier des presets", [this] { manager.getUserPresetDirectory().revealToUser(); });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton).withMinimumWidth (nameButton.getWidth()));
}

void PresetBar::showSaveDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Enregistrer le preset",
                                                      "Ton preset sera dans la banque USER. Tu pourras le liker pour le retrouver dans LIKED.",
                                                      juce::MessageBoxIconType::NoIcon, this);

    const auto* current = manager.findById (manager.getCurrentPresetId());
    saveDialog->addTextEditor ("name", current != nullptr && ! current->isFactory ? current->name : juce::String(), "Nom");

    auto categories = manager.getCategories();
    categories.removeString ("Init");
    categories.addIfNotAlreadyThere ("User", 0);
    saveDialog->addComboBox ("category", categories, "Categorie");
    if (auto* box = saveDialog->getComboBoxComponent ("category"))
    {
        box->setEditableText (true); // allow a new category name
        box->setText (current != nullptr && current->category != "Init" ? current->category : "User", juce::dontSendNotification);
    }

    saveDialog->addButton ("Enregistrer", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (saveDialog == nullptr)
            return;
        if (result == 1)
        {
            const auto name = saveDialog->getTextEditorContents ("name");
            auto* box = saveDialog->getComboBoxComponent ("category");
            const auto category = box != nullptr ? box->getText() : juce::String ("User");
            const auto saved = manager.saveUserPreset (name, category);
            if (saved.failed())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Reckless Sample", saved.getErrorMessage(), {}, this);
        }
        saveDialog.reset();
    }), false);
}
} // namespace rs
