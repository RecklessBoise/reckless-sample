#include "PresetBrowser.h"
#include "Theme.h"

namespace rs
{
namespace
{
    constexpr int heartWidth = 34;
    const juce::String allCategories { "Toutes categories" };
}

PresetBrowser::PresetBrowser (PresetManager& m) : manager (m)
{
    likedTab.setButtonText (juce::String::fromUTF8 ("\xe2\x99\xa5 LIKED"));

    const std::pair<juce::TextButton*, PresetManager::Bank> tabs[] {
        { &allTab, PresetManager::Bank::all }, { &factoryTab, PresetManager::Bank::factory },
        { &likedTab, PresetManager::Bank::liked }, { &userTab, PresetManager::Bank::user }
    };
    for (auto [button, b] : tabs)
    {
        addAndMakeVisible (button);
        button->setRadioGroupId (1);
        button->setClickingTogglesState (true);
        button->onClick = [this, target = b] { setBank (target); };
    }
    allTab.setToggleState (true, juce::dontSendNotification);

    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { if (onClose) onClose(); };

    addAndMakeVisible (search);
    search.setTextToShowWhenEmpty ("Rechercher...", theme::textDim);
    search.setFont (theme::font (13.0f));
    search.onTextChange = [this] { rebuild(); };

    addAndMakeVisible (category);
    category.onChange = [this] { rebuild(); };

    addAndMakeVisible (list);
    list.setModel (this);
    list.setRowHeight (30);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);

    addAndMakeVisible (countLabel);
    countLabel.setFont (theme::caption (10.0f));
    countLabel.setColour (juce::Label::textColourId, theme::textDim);

    manager.addChangeListener (this);
    rebuild();
}

PresetBrowser::~PresetBrowser()
{
    manager.removeChangeListener (this);
}

void PresetBrowser::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuild();
}

void PresetBrowser::setBank (PresetManager::Bank newBank)
{
    bank = newBank;
    rebuild();
}

void PresetBrowser::rebuild()
{
    // Keep the category list in sync with what exists.
    const auto previous = category.getText();
    category.clear (juce::dontSendNotification);
    category.addItem (allCategories, 1);
    const auto categories = manager.getCategories();
    for (int i = 0; i < categories.size(); ++i)
        category.addItem (categories[i], i + 2);
    category.setText (previous.isEmpty() || ! categories.contains (previous) ? allCategories : previous, juce::dontSendNotification);

    const auto chosen = category.getText() == allCategories ? juce::String() : category.getText();
    rows = manager.getFiltered (bank, search.getText().trim(), chosen);
    list.updateContent();
    list.repaint();

    countLabel.setText (juce::String ((int) rows.size()) + " PRESETS  /  " + juce::String (manager.getNumLiked()) + " LIKED",
                        juce::dontSendNotification);

    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].id == manager.getCurrentPresetId())
            list.selectRow ((int) i, true, true);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::black.withAlpha (0.94f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (theme::text);
    g.setFont (theme::caption (12.0f));
    g.drawText ("PRESETS", r.reduced (14.0f, 10.0f).removeFromTop (20.0f), juce::Justification::centredLeft);

    if (rows.empty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (13.0f));
        const auto message = bank == PresetManager::Bank::liked ? juce::String::fromUTF8 ("Aucun preset like. Clique sur \xe2\x99\xa5 pour en ajouter.")
                           : bank == PresetManager::Bank::user  ? juce::String ("Aucun preset perso. Clique sur SAVE pour en creer.")
                                                                : juce::String ("Aucun resultat.");
        g.drawFittedText (message, list.getBounds().reduced (20), juce::Justification::centred, 3);
    }
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (24);
    closeButton.setBounds (top.removeFromRight (24));
    r.removeFromTop (8);

    auto tabs = r.removeFromTop (26);
    const int tabW = tabs.getWidth() / 4;
    for (auto* b : { &allTab, &factoryTab, &likedTab, &userTab })
        b->setBounds (tabs.removeFromLeft (tabW).reduced (2, 0));

    r.removeFromTop (8);
    search.setBounds (r.removeFromTop (28));
    r.removeFromTop (6);
    category.setBounds (r.removeFromTop (26));
    r.removeFromTop (8);
    countLabel.setBounds (r.removeFromBottom (18));
    list.setBounds (r);
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;
    const auto& p = rows[(size_t) row];
    auto r = juce::Rectangle<int> (width, height).toFloat().reduced (2.0f, 1.0f);

    if (selected)
    {
        g.setColour (theme::deepTeal.withAlpha (0.9f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::cyan.withAlpha (0.5f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    auto text = r.reduced (10.0f, 0.0f).withTrimmedRight ((float) heartWidth);
    g.setColour (selected ? theme::ice : theme::text);
    g.setFont (theme::font (13.0f, selected));
    g.drawText (p.name, text.removeFromLeft (text.getWidth() * 0.72f), juce::Justification::centredLeft, true);
    g.setColour (theme::textDim);
    g.setFont (theme::caption (9.0f));
    g.drawText ((p.isFactory ? "" : "USER  ") + p.category.toUpperCase(), text, juce::Justification::centredRight, true);

    const auto heartArea = r.removeFromRight ((float) heartWidth).withSizeKeepingCentre (14.0f, 12.0f);
    const auto heart = theme::heartPath (heartArea);
    if (manager.isLiked (p.id))
    {
        g.setColour (theme::heart);
        g.fillPath (heart);
    }
    else
    {
        g.setColour (theme::textDim.withAlpha (0.6f));
        g.strokePath (heart, juce::PathStrokeType (1.2f));
    }
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;
    const auto preset = rows[(size_t) row];

    if (e.mods.isPopupMenu())
    {
        showUserMenu (preset);
        return;
    }

    // Clicking the heart toggles the like; anywhere else loads the preset.
    if (e.x >= list.getVisibleRowWidth() - heartWidth)
        manager.toggleLiked (preset.id);
    else
        manager.loadPreset (preset);
}

void PresetBrowser::showUserMenu (const PresetManager::Preset& preset)
{
    juce::PopupMenu menu;
    menu.addItem ("Charger", [this, preset] { manager.loadPreset (preset); });
    menu.addItem (manager.isLiked (preset.id) ? "Retirer des likes" : "Liker", [this, id = preset.id] { manager.toggleLiked (id); });

    if (! preset.isFactory)
    {
        menu.addSeparator();
        menu.addItem ("Renommer...", [this, preset]
        {
            auto* dialog = new juce::AlertWindow ("Renommer", {}, juce::MessageBoxIconType::NoIcon, this);
            dialog->addTextEditor ("name", preset.name);
            dialog->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            dialog->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this, preset, dialog] (int result)
            {
                if (result == 1)
                {
                    const auto renamed = manager.renameUserPreset (preset, dialog->getTextEditorContents ("name"));
                    if (renamed.failed())
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Reckless Sample", renamed.getErrorMessage(), {}, this);
                }
            }), true);
        });
        menu.addItem ("Supprimer", [this, preset]
        {
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Supprimer le preset",
                                                "Supprimer \"" + preset.name + "\" ? Cette action est definitive.",
                                                "Supprimer", "Annuler", this,
                                                juce::ModalCallbackFunction::create ([this, preset] (int result)
                                                {
                                                    if (result == 1)
                                                        manager.deleteUserPreset (preset);
                                                }));
        });
        menu.addItem ("Afficher dans le Finder", [preset] { preset.file.revealToUser(); });
    }
    menu.showMenuAsync (juce::PopupMenu::Options());
}
} // namespace rs
