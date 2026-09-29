#include "TrackInspector.h"

#include <array>
#include <tuple>
#include "HostedPluginWindow.h"
#include "PluginChooser.h"
#include "../../PluginProcessor.h"

namespace sis
{
namespace
{
    constexpr int pad = 12;
    constexpr int rowHeight = 16;
    constexpr int rowGap = 9;
    constexpr int labelWidth = 42;
    constexpr int valueWidth = 50;
    constexpr int sectionTitle = 13;
    constexpr int cardGap = 5;

    juce::String panText (float pan)
    {
        if (juce::approximatelyEqual (pan, 0.0f))
            return "C";
        return (pan < 0.0f ? "L" : "R") + juce::String (juce::roundToInt (std::abs (pan) * 100.0f));
    }
}

//==============================================================================
EffectCard::EffectCard (StudioContext& c, int effectIndex) : ctx (c), index (effectIndex)
{
    toggleButton.setIcon ([this] (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour)
    {
        const auto* effect = getEffect();
        const bool on = effect != nullptr && effect->enabled;
        const auto track = area.withSizeKeepingCentre (26.0f, 14.0f);

        g.setColour (on ? colours::accent : colours::sliderTrack);
        g.fillRect (track);
        g.setColour (colours::line);
        g.drawRect (track, 1.0f);
        g.setColour (colours::white);
        g.fillRect (juce::Rectangle<float> (on ? track.getRight() - 12.0f : track.getX() + 1.0f,
                                            track.getY() + 1.0f, 10.0f, 10.0f));
    });

    toggleButton.onClick = [this]
    {
        if (auto* effect = getEffect())
        {
            ctx.step (effect->enabled ? "Effekt abgeschaltet"_u : "Effekt eingeschaltet"_u);
            effect->enabled = ! effect->enabled;
            ctx.model.notifyChanged();
        }
    };

    FlatButton::Style small;
    small.background = colours::white;
    small.border = colours::lineStrongAlt;
    small.text = colours::text;
    small.fontSize = 10.5f;
    openButton.setStyle (small);
    openButton.onClick = [this]
    {
        auto* effect = getEffect();
        auto* zone = ctx.model.getSelectedZone();

        if (effect == nullptr || zone == nullptr)
            return;

        if (auto plugin = ctx.processor.findHostedPlugin (zone->id, zone->selectedTrack, effect->pluginIdentifier))
            HostedPluginWindow::open (plugin, effect->name);
        else
            ctx.toast (effect->name + " ist nicht geladen · Plugin fehlt oder ist abgeschaltet"_u);
    };

    presetButton.setStyle (small);
    presetButton.onClick = [this] { showPresetMenu(); };

    /* Zwei schmale Dreiecke übereinander statt zweier beschrifteter Knöpfe: in der
       Kopfzeile ist der Platz knapp, und der Name des Effekts soll lesbar bleiben. */
    FlatButton::Style mover;
    mover.text = colours::textTertiary;
    mover.hoverText = colours::accentHover;

    const auto makeArrow = [] (bool up)
    {
        return [up] (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
        {
            const auto box = area.withSizeKeepingCentre (7.0f, 5.0f);
            juce::Path triangle;

            if (up)
                triangle.addTriangle (box.getX(), box.getBottom(), box.getRight(), box.getBottom(),
                                      box.getCentreX(), box.getY());
            else
                triangle.addTriangle (box.getX(), box.getY(), box.getRight(), box.getY(),
                                      box.getCentreX(), box.getBottom());

            g.setColour (colour);
            g.fillPath (triangle);
        };
    };

    moveUpButton.setStyle (mover);
    moveDownButton.setStyle (mover);
    moveUpButton.setIcon (makeArrow (true));
    moveDownButton.setIcon (makeArrow (false));

    moveUpButton.onClick = [this] { moveBy (-1); };
    moveDownButton.onClick = [this] { moveBy (1); };

    FlatButton::Style remove;
    remove.text = colours::textTertiary;
    remove.hoverText = colours::warning;
    remove.fontSize = 13.0f;
    removeButton.setStyle (remove);
    removeButton.onClick = [this]
    {
        auto* zone = ctx.model.getSelectedZone();
        auto* track = zone != nullptr ? zone->getSelectedTrack() : nullptr;

        if (track != nullptr && juce::isPositiveAndBelow (index, (int) track->effects.size()))
        {
            const auto& effect = track->effects[(size_t) index];
            const auto name = effect.name;

            // Ein offenes Plugin-Fenster darf den gelöschten Effekt nicht überleben
            if (effect.type == EffectType::external)
                if (auto plugin = ctx.processor.findHostedPlugin (zone->id, zone->selectedTrack,
                                                                  effect.pluginIdentifier))
                    HostedPluginWindow::closeFor (plugin.get());

            ctx.step (name + " entfernt");
            track->effects.erase (track->effects.begin() + index);
            ctx.model.notifyChanged();
            ctx.toast (name + " entfernt");
        }
    };

    addAndMakeVisible (toggleButton);
    addAndMakeVisible (removeButton);
    addAndMakeVisible (moveUpButton);
    addAndMakeVisible (moveDownButton);

    if (const auto* effect = getEffect())
    {
        if (effect->external)
            addAndMakeVisible (openButton);
        else if (! effect->parameters.empty())
            addAndMakeVisible (presetButton);

        for (size_t i = 0; i < effect->parameters.size(); ++i)
        {
            auto bar = std::make_unique<ValueBar>();
            bar->setValue (effect->parameters[i].value);
            bar->onSecondaryClick = [this, i] { showMacroMenu ((int) i); };
            bar->onValueChange = [this, i] (float v)
            {
                if (auto* e = getEffect())
                    if (i < e->parameters.size())
                    {
                        ctx.step (e->parameters[i].label + " geändert"_u);
                        e->parameters[i].value = v;
                        ctx.model.notifyChanged();
                    }
            };
            addAndMakeVisible (*bar);
            paramBars.push_back (std::move (bar));
        }
    }

    paramLabels.resize (paramBars.size());
    paramValues.resize (paramBars.size());
}

void EffectCard::showPresetMenu()
{
    auto* effect = getEffect();

    if (effect == nullptr)
        return;

    auto& library = ctx.processor.getPresetLibrary();
    const auto names = library.getNames (effect->type);

    constexpr int factoryId = 1;
    constexpr int saveId = 2;
    constexpr int firstPresetId = 100;
    constexpr int firstDeleteId = 200;

    juce::PopupMenu menu;
    menu.addItem (factoryId, "Grundstellung"_u);

    if (! names.isEmpty())
    {
        menu.addSeparator();

        for (int i = 0; i < names.size(); ++i)
            menu.addItem (firstPresetId + i, names[i]);
    }

    menu.addSeparator();
    menu.addItem (saveId, "Sichern als …"_u);

    if (! names.isEmpty())
    {
        juce::PopupMenu removeMenu;

        for (int i = 0; i < names.size(); ++i)
            removeMenu.addItem (firstDeleteId + i, names[i]);

        menu.addSubMenu ("Löschen"_u, removeMenu);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (180),
                        [this, names] (int result)
    {
        auto* chosen = getEffect();

        if (chosen == nullptr || result == 0)
            return;

        auto& presets = ctx.processor.getPresetLibrary();

        if (result == factoryId)
        {
            ctx.step (chosen->name + ": Grundstellung"_u);
            PresetLibrary::applyFactoryDefaults (*chosen);
            ctx.model.notifyChanged();
            ctx.toast (chosen->name + " auf Grundstellung"_u);
            return;
        }

        if (result == saveId)
        {
            askForPresetName();
            return;
        }

        if (result >= firstDeleteId)
        {
            const auto name = names[result - firstDeleteId];
            presets.remove (chosen->type, name);
            ctx.toast ("Preset "_u + name + " gelöscht"_u);
            return;
        }

        const auto name = names[result - firstPresetId];

        ctx.step (chosen->name + ": "_u + name);
        presets.apply (name, *chosen);
        ctx.model.notifyChanged();
        ctx.toast (name + " geladen"_u);
    });
}

void EffectCard::askForPresetName()
{
    auto* effect = getEffect();

    if (effect == nullptr)
        return;

    // Für einen Namen braucht es ein Eingabefeld; das kann nur ein eigenes Fenster
    auto* window = new juce::AlertWindow ("Preset sichern"_u,
                                          "Unter welchem Namen?"_u,
                                          juce::MessageBoxIconType::NoIcon, this);

    window->addTextEditor ("name", effect->name, "Name:"_u);
    window->addButton ("Sichern"_u, 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Abbrechen"_u, 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<EffectCard> safe (this);

    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
    {
        const auto name = window->getTextEditorContents ("name").trim();
        std::unique_ptr<juce::AlertWindow> owned (window);

        if (safe == nullptr || result != 1 || name.isEmpty())
            return;

        if (auto* effectToSave = safe->getEffect())
        {
            safe->ctx.processor.getPresetLibrary().save (name, *effectToSave);
            safe->ctx.toast ("Preset "_u + name + " gesichert"_u);
        }
    }), false);
}

Effect* EffectCard::getEffect() const
{
    auto* zone = ctx.model.getSelectedZone();
    auto* track = zone != nullptr ? zone->getSelectedTrack() : nullptr;

    if (track != nullptr && juce::isPositiveAndBelow (index, (int) track->effects.size()))
        return &track->effects[(size_t) index];

    return nullptr;
}

bool EffectCard::isCollapsed() const
{
    const auto* effect = getEffect();
    return effect != nullptr && ctx.ui.isEffectCollapsed (effect->name);
}

int EffectCard::getIdealHeight() const
{
    if (isCollapsed() || paramBars.empty())
        return 8 + 28 + 8;

    return 8 + 28 + 7 + (int) paramBars.size() * 13 + ((int) paramBars.size() - 1) * 5 + 8;
}

MacroTarget EffectCard::makeTarget (int parameterIndex) const
{
    MacroTarget target;

    if (const auto* zone = ctx.model.getSelectedZone())
    {
        target.zoneId = zone->id;
        target.trackIndex = zone->selectedTrack;
    }

    target.effectIndex = index;
    target.parameterIndex = parameterIndex;
    return target;
}

void EffectCard::showMacroMenu (int parameterIndex)
{
    const auto* effect = getEffect();

    if (effect == nullptr || ! juce::isPositiveAndBelow (parameterIndex, (int) effect->parameters.size()))
        return;

    const auto target = makeTarget (parameterIndex);
    const int current = ctx.model.macroFor (target);

    juce::PopupMenu menu;
    menu.addSectionHeader (effect->parameters[(size_t) parameterIndex].label);

    for (int macro = 0; macro < InstrumentModel::numMacros; ++macro)
        menu.addItem (macro + 1, macroName (macro), true, macro == current);

    menu.addSeparator();
    menu.addItem (100, "Keinem Makro"_u, current >= 0, current < 0);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (paramBars[(size_t) parameterIndex].get()),
                        [this, target] (int result)
    {
        if (result == 0)
            return;

        const int macro = result == 100 ? -1 : result - 1;

        ctx.step (macro < 0 ? "Makro gelöst"_u : macroName (macro) + " zugewiesen"_u);
        ctx.model.assignMacro (macro, target);
        ctx.model.notifyChanged();

        ctx.toast (macro < 0 ? "Regler folgt keinem Makro mehr"_u
                             : "Regler folgt "_u + macroName (macro));
    });
}

void EffectCard::moveBy (int delta)
{
    auto* zone = ctx.model.getSelectedZone();
    auto* track = zone != nullptr ? zone->getSelectedTrack() : nullptr;

    if (track == nullptr)
        return;

    const int target = index + delta;

    if (! juce::isPositiveAndBelow (index, (int) track->effects.size())
        || ! juce::isPositiveAndBelow (target, (int) track->effects.size()))
        return;

    ctx.step (track->effects[(size_t) index].name + " verschoben"_u);
    std::swap (track->effects[(size_t) index], track->effects[(size_t) target]);
    ctx.model.notifyChanged();
}

void EffectCard::mouseUp (const juce::MouseEvent& e)
{
    /* Ein Klick, der hier ankommt, hat keinen Knopf getroffen - die verbrauchen ihre
       eigenen. Nur die Kopfzeile klappt ein, nicht die Reglerfläche darunter. */
    auto* effect = getEffect();

    if (effect != nullptr && e.y < 36 && e.mouseWasClicked())
        ctx.ui.toggleEffectCollapsed (effect->name);
}

void EffectCard::updateValues()
{
    if (const auto* effect = getEffect())
        for (size_t i = 0; i < paramBars.size() && i < effect->parameters.size(); ++i)
            paramBars[i]->setValue (effect->parameters[i].value);

    repaint();
}

void EffectCard::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto top = area.removeFromTop (28);

    toggleButton.setBounds (top.removeFromLeft (26).withSizeKeepingCentre (26, 20));
    top.removeFromLeft (8);
    removeButton.setBounds (top.removeFromRight (18));

    for (auto* button : { &openButton, &presetButton })
    {
        if (! button->isVisible())
            continue;

        const int width = button->getTextWidth() + 14;
        top.removeFromRight (6);
        button->setBounds (top.removeFromRight (width).withSizeKeepingCentre (width, 20));
    }

    // Die beiden Dreiecke stehen übereinander in einer schmalen Spalte
    top.removeFromRight (6);
    auto mover = top.removeFromRight (14);
    moveUpButton.setBounds (mover.removeFromTop (13));
    moveDownButton.setBounds (mover.removeFromBottom (13));

    const bool collapsed = isCollapsed();

    for (auto& bar : paramBars)
        bar->setVisible (! collapsed);

    if (collapsed)
        return;

    area.removeFromTop (7);

    for (size_t i = 0; i < paramBars.size(); ++i)
    {
        auto row = area.removeFromTop (13);

        /* 76 px statt 52: die Beschriftungen der späteren Effekte sind länger geworden
           (RÜCKKOPPLUNG, KOMPRESSION), und ein abgeschnittener Reglername ist schlimmer
           als ein etwas kürzerer Balken. */
        paramLabels[i] = row.removeFromLeft (76);
        row.removeFromLeft (8);
        paramValues[i] = row.removeFromRight (34);
        row.removeFromRight (8);
        paramBars[i]->setBounds (row);
        area.removeFromTop (5);
    }
}

void EffectCard::paint (juce::Graphics& g)
{
    const auto* effect = getEffect();
    if (effect == nullptr)
        return;

    g.setColour (effect->enabled ? colours::surface : juce::Colour (0xffefeeea));
    g.fillRect (getLocalBounds());
    g.setColour (colours::lineFine);
    g.drawRect (getLocalBounds(), 1);

    auto top = getLocalBounds().reduced (8).removeFromTop (28);
    top.removeFromLeft (26 + 8);
    int rightEdge = top.getRight() - 18 - 6 - 14 - 6;

    for (const auto* button : { &openButton, &presetButton })
        if (button->isVisible())
            rightEdge -= button->getWidth() + 6;
    top.setRight (rightEdge);

    g.setColour (effect->enabled ? colours::text : colours::textSecondary);
    g.setFont (sansFont (11.5f, Weight::medium));
    g.drawText (effect->name, top.removeFromTop (15), juce::Justification::centredLeft, true);

    // Das Dreieck steht vor der Art, nicht vor dem Namen: dort kostet es keine Breite
    draw::disclosure (g, top.removeFromLeft (10), ! isCollapsed(), colours::textTertiary);
    top.removeFromLeft (3);

    g.setColour (effect->external ? colours::accentHover : colours::textSecondary);
    g.setFont (monoFont (10.0f));
    g.drawText (effect->kindLabel(), top, juce::Justification::centredLeft, true);

    if (isCollapsed())
        return;

    g.setFont (monoFont (10.0f));
    for (size_t i = 0; i < paramBars.size() && i < effect->parameters.size(); ++i)
    {
        // Wer einem Makro folgt, steht in der Akzentfarbe
        const bool linked = ctx.model.macroFor (makeTarget ((int) i)) >= 0;

        g.setColour (linked ? colours::accentHover : colours::textSecondary);
        g.drawText (effect->parameters[i].label, paramLabels[i], juce::Justification::centredLeft, true);
        g.drawText (juce::String (juce::roundToInt (effect->parameters[i].value * 100.0f)) + " %",
                    paramValues[i], juce::Justification::centredRight, false);
        paramBars[i]->setFillColour (effect->enabled ? colours::accent : colours::lineStrong);
    }
}

//==============================================================================
TrackInspectorContent::TrackInspectorContent (StudioContext& c) : ctx (c)
{
    gainBar.onValueChange = [this] (float v)
    {
        ctx.step ("Pegel geändert"_u);
        if (auto* t = getTrack()) { t->gain = v; ctx.model.notifyChanged(); }
    };
    panBar.setMode (ValueBar::Mode::bipolar);
    panBar.onValueChange = [this] (float v)
    {
        ctx.step ("Panorama geändert"_u);
        if (auto* t = getTrack()) { t->pan = v * 2.0f - 1.0f; ctx.model.notifyChanged(); }
    };
    centsBar.onValueChange = [this] (float v)
    {
        ctx.step ("Feinstimmung geändert"_u);
        if (auto* t = getTrack()) { t->cents = v; ctx.model.notifyChanged(); }
    };

    FlatButton::Style stepper;
    stepper.background = colours::white;
    stepper.border = colours::lineStrongAlt;
    stepper.text = colours::text;
    pitchDownButton.setStyle (stepper);
    pitchUpButton.setStyle (stepper);
    pitchDownButton.onClick = [this]
    {
        ctx.step ("Tonhöhe geändert"_u);
        if (auto* t = getTrack()) { t->pitch = juce::jmax (-24, t->pitch - 1); ctx.model.notifyChanged(); }
    };
    pitchUpButton.onClick = [this]
    {
        ctx.step ("Tonhöhe geändert"_u);
        if (auto* t = getTrack()) { t->pitch = juce::jmin (24, t->pitch + 1); ctx.model.notifyChanged(); }
    };

    FlatButton::Style smallStyle;
    smallStyle.background = colours::white;
    smallStyle.border = colours::lineStrongAlt;
    smallStyle.text = colours::text;
    smallStyle.mono = true;
    smallStyle.fontSize = 10.0f;
    resetStretchButton.setStyle (smallStyle);
    resetStretchButton.onClick = [this]
    {
        ctx.step ("Dehnung zurückgesetzt"_u);
        if (auto* c = currentClip (ctx.model, ctx.ui)) { c->stretch = 1.0; ctx.model.notifyChanged(); }
    };

    reverseButton.onClick = [this]
    {
        ctx.step ("Richtung umgekehrt"_u);
        if (auto* t = getTrack()) { t->reverse = ! t->reverse; ctx.model.notifyChanged(); }
    };

    FlatButton::Style plus;
    plus.background = colours::white;
    plus.border = colours::lineStrongAlt;
    plus.text = colours::text;
    plus.fontSize = 11.0f;
    addEffectButton.setStyle (plus);
    addEffectButton.onClick = [this] { showEffectMenu(); };

    for (const auto& name : { "Transienten-treu", "Glatt (Pad)", "Monophon", "Korn / Granular" })
        algorithmBox.addItem (juce::String::fromUTF8 (name), algorithmBox.getNumItems() + 1);

    algorithmBox.onChange = [this]
    {
        if (auto* t = getTrack())
        {
            ctx.step ("Stretch-Verfahren gewechselt"_u);
            t->algorithm = (StretchAlgorithm) juce::jmax (0, algorithmBox.getSelectedItemIndex());
            ctx.model.notifyChanged();
        }
    };

    for (const auto& name : { "One-Shot", "Sustain-Loop", "Vor/Rückwärts" })
        loopBox.addItem (juce::String::fromUTF8 (name), loopBox.getNumItems() + 1);

    loopBox.onChange = [this]
    {
        if (auto* t = getTrack())
        {
            ctx.step ("Loop-Modus gewechselt"_u);
            t->loop = (LoopMode) juce::jmax (0, loopBox.getSelectedItemIndex());
            ctx.model.notifyChanged();
        }
    };

    loopStartBar.onValueChange = [this] (float v)
    {
        ctx.step ("Loop-Beginn geändert"_u);
        if (auto* t = getTrack()) { t->loopStart = 0.9 * (double) v; ctx.model.notifyChanged(); }
    };
    crossfadeBar.onValueChange = [this] (float v)
    {
        ctx.step ("Loop-Überblendung geändert"_u);
        if (auto* t = getTrack()) { t->loopCrossfade = 0.5 * (double) v; ctx.model.notifyChanged(); }
    };

    for (auto* c2 : { &gainBar, &panBar, &centsBar, &loopStartBar, &crossfadeBar })
        addAndMakeVisible (c2);
    for (auto* b : { &pitchDownButton, &pitchUpButton, &resetStretchButton, &reverseButton, &addEffectButton })
        addAndMakeVisible (b);
    addAndMakeVisible (algorithmBox);
    addAndMakeVisible (loopBox);

    ctx.model.addChangeListener (this);
    rebuildIfNeeded();
}

TrackInspectorContent::~TrackInspectorContent()
{
    ctx.model.removeChangeListener (this);
}

Track* TrackInspectorContent::getTrack() const
{
    auto* zone = ctx.model.getSelectedZone();
    return zone != nullptr ? zone->getSelectedTrack() : nullptr;
}

bool TrackInspectorContent::isLooping() const
{
    const auto* track = getTrack();
    return track != nullptr && track->loop != LoopMode::oneShot;
}

juce::String TrackInspectorContent::makeSignature() const
{
    auto* zone = ctx.model.getSelectedZone();
    const auto* track = getTrack();

    if (zone == nullptr || track == nullptr)
        return "-";

    // Mit dem Loop-Modus kommen zwei Regler dazu – die Höhe ändert sich, also neu aufbauen
    juce::String s = zone->id + "/" + juce::String (zone->selectedTrack) + (isLooping() ? "/loop" : "");
    for (const auto& e : track->effects)
        s << "|" << e.name << ":" << (int) e.parameters.size() << (e.external ? "x" : "i");
    return s;
}

void TrackInspectorContent::rebuildIfNeeded()
{
    const auto newSignature = makeSignature();

    if (newSignature == signature)
    {
        updateValues();
        return;
    }

    signature = newSignature;
    effectCards.clear();

    const bool hasTrack = getTrack() != nullptr;
    const std::initializer_list<juce::Component*> controls {
        &gainBar, &panBar, &centsBar, &pitchDownButton, &pitchUpButton,
        &resetStretchButton, &reverseButton, &addEffectButton, &algorithmBox, &loopBox };

    for (auto* control : controls)
        control->setVisible (hasTrack);

    if (const auto* track = getTrack())
    {
        for (int i = 0; i < (int) track->effects.size(); ++i)
        {
            auto card = std::make_unique<EffectCard> (ctx, i);
            addAndMakeVisible (*card);
            effectCards.push_back (std::move (card));
        }

        algorithmBox.setSelectedItemIndex ((int) track->algorithm, juce::dontSendNotification);
        loopBox.setSelectedItemIndex ((int) track->loop, juce::dontSendNotification);
    }

    updateValues();

    if (auto* parent = getParentComponent())
        parent->resized();

    resized();
    repaint();
}

void TrackInspectorContent::updateValues()
{
    if (const auto* track = getTrack())
    {
        gainBar.setValue (track->gain);
        panBar.setValue (track->pan * 0.5f + 0.5f);
        centsBar.setValue (track->cents);
        loopStartBar.setValue ((float) (track->loopStart / 0.9));
        crossfadeBar.setValue ((float) (track->loopCrossfade / 0.5));
        loopStartBar.setFillColour (track->colour);
        crossfadeBar.setFillColour (track->colour);
        gainBar.setFillColour (track->colour);
        panBar.setFillColour (track->colour);
        centsBar.setFillColour (track->colour);
        algorithmBox.setSelectedItemIndex ((int) track->algorithm, juce::dontSendNotification);
        loopBox.setSelectedItemIndex ((int) track->loop, juce::dontSendNotification);

        FlatButton::Style reverse;
        reverse.fontSize = 11.5f;
        reverse.background = track->reverse ? colours::accentSoft : colours::white;
        reverse.text = track->reverse ? colours::accentDark : colours::textSecondary;
        reverse.border = track->reverse ? colours::accent : colours::lineStrongAlt;
        reverseButton.setStyle (reverse);
    }

    for (auto& card : effectCards)
        card->updateValues();

    repaint();
}

void TrackInspectorContent::showEffectMenu()
{
    // Die Liste kommt aus dem Modell, damit Name, Regler und Familie an einer Stelle stehen
    const auto available = Effect::builtIn();

    constexpr int externalItemId = 1000;   // weit weg von den laufenden Nummern

    juce::PopupMenu menu;

    /* Der Kanal-Streifen steht oben und nicht in seiner Familie: für die meisten Spuren
       ist er der schnellste Weg, und dafür soll man nicht erst ein Untermenü aufklappen. */
    for (int i = 0; i < (int) available.size(); ++i)
        if (available[(size_t) i].type == EffectType::channelStrip)
            menu.addItem (i + 1, available[(size_t) i].name);

    menu.addSeparator();

    for (const auto family : effectFamilyOrder())
    {
        juce::PopupMenu submenu;

        for (int i = 0; i < (int) available.size(); ++i)
        {
            const auto& effect = available[(size_t) i];

            if (effect.type == EffectType::channelStrip)
                continue;   // steht schon oben

            if (familyOf (effect.type) == family)
                submenu.addItem (i + 1, effect.name);
        }

        if (submenu.getNumItems() > 0)
            menu.addSubMenu (toDisplayString (family), submenu);
    }

    // Wer ohne Familie bleibt, soll trotzdem erreichbar sein statt still zu verschwinden
    for (int i = 0; i < (int) available.size(); ++i)
        if (! familyOf (available[(size_t) i].type).has_value())
            menu.addItem (i + 1, available[(size_t) i].name);

    /* Die installierten Plugins stehen im **selben** Menü. Dass die einen mit dem
       Instrument exportiert werden und die anderen auf dem fremden Rechner liegen müssen,
       ist eine Frage des Unterbaus — beim Aussuchen eines Klangs hilft die Unterscheidung
       niemandem. Sie steht dafür an der Karte des eingefügten Effekts.

       Die eigenen Effekt-VST3 werden dabei übersprungen: sie stehen schon oben als interne
       Effekte, und dort sind sie die bessere Wahl (sie wandern mit dem Export mit). */
    const auto installed = ctx.processor.getPluginLibrary().getEffects();
    juce::Array<juce::PluginDescription> foreign;

    for (const auto& description : installed)
        if (description.manufacturerName != "WiskundeKnobbel")
            foreign.add (description);

    menu.addSeparator();

    {
        juce::PopupMenu submenu;

        for (int i = 0; i < foreign.size(); ++i)
            submenu.addItem (externalItemId + 1 + i, foreign[i].name);

        if (foreign.isEmpty())
            submenu.addItem (-1, "Noch keine gefunden"_u, false, false);

        submenu.addSeparator();

        juce::PopupMenu::Item browse ("Suchen und verwalten …"_u);
        browse.itemID = externalItemId;
        browse.colour = colours::accentHover;
        submenu.addItem (std::move (browse));

        menu.addSubMenu ("Installierte Plugins"_u, submenu);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addEffectButton).withMinimumWidth (222),
                        [this, available, foreign] (int result)
    {
        auto* track = getTrack();
        if (track == nullptr || result == 0)
            return;

        // Ein Plugin, das schon in der Liste steht: ohne Umweg über den Dialog einfügen
        if (result > externalItemId)
        {
            const int index = result - externalItemId - 1;

            if (juce::isPositiveAndBelow (index, foreign.size()))
            {
                const auto& description = foreign[index];

                ctx.step (description.name + " eingefügt"_u);
                track->effects.push_back (Effect::makeExternal (description.name,
                                                                description.createIdentifierString()));
                ctx.model.notifyChanged();
                ctx.toast (description.name + " eingefügt · über „Öffnen“ erscheint seine Oberfläche"_u);
            }

            return;
        }

        if (result == externalItemId)
        {
            PluginChooser::show (ctx.processor.getPluginLibrary(), this,
                                 [this] (const juce::String& name, const juce::String& identifier)
            {
                if (auto* chosenTrack = getTrack())
                {
                    ctx.step (name + " eingefügt"_u);
                    chosenTrack->effects.push_back (Effect::makeExternal (name, identifier));
                    ctx.model.notifyChanged();
                    ctx.toast (name + " eingefügt · über „Öffnen“ erscheint seine Oberfläche"_u);
                }
            });
            return;
        }

        if (! juce::isPositiveAndBelow (result - 1, (int) available.size()))
            return;

        const auto& chosen = available[(size_t) (result - 1)];

        ctx.step (chosen.name + " eingefügt"_u);
        track->effects.push_back (chosen);
        ctx.model.notifyChanged();
        ctx.toast (chosen.name + " eingefügt · wirkt sofort"_u);
    });
}

int TrackInspectorContent::getIdealHeight() const
{
    const int levelHeight = pad + 3 * rowHeight + 2 * rowGap + pad;

    const int loopRows = isLooping() ? 2 * (9 + rowHeight) : 0;
    const int pitchHeight = ctx.ui.showPitchSection
                                ? pad + sectionTitle + 9 + 26 + 9 + rowHeight + 9 + rowHeight + 9 + 28 + 9 + 28 + pad + loopRows
                                : pad + sectionTitle + pad;

    int effectsHeight = pad + sectionTitle + 9;

    if (ctx.ui.showEffectSection)
    {
        for (const auto& card : effectCards)
            effectsHeight += card->getIdealHeight() + cardGap;

        effectsHeight += 10 + 28 + pad;
    }
    else
    {
        effectsHeight += pad;
    }

    return metrics::panelHeader + levelHeight + pitchHeight + effectsHeight;
}

void TrackInspectorContent::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (metrics::panelHeader);

    // Pegel und Panorama
    levelSection = area.removeFromTop (pad + 3 * rowHeight + 2 * rowGap + pad);
    {
        auto r = levelSection.reduced (pad);
        auto line = r.removeFromTop (rowHeight);
        gainLabel = line.removeFromLeft (labelWidth);
        line.removeFromLeft (8);
        gainValue = line.removeFromRight (valueWidth);
        line.removeFromRight (8);
        gainBar.setBounds (line);
        r.removeFromTop (rowGap);

        line = r.removeFromTop (rowHeight);
        panLabel = line.removeFromLeft (labelWidth);
        line.removeFromLeft (8);
        panValue = line.removeFromRight (valueWidth);
        line.removeFromRight (8);
        panBar.setBounds (line);
        r.removeFromTop (rowGap);

        line = r.removeFromTop (rowHeight);
        fadeLabel = line.removeFromLeft (labelWidth);
        line.removeFromLeft (8);
        fadeValue = line;
    }

    // Tonhöhe und Zeit
    const bool showPitch = ctx.ui.showPitchSection;

    const bool looping = isLooping();
    pitchSection = area.removeFromTop (showPitch
                                           ? pad + sectionTitle + 9 + 26 + 9 + rowHeight + 9 + rowHeight + 9 + 28 + 9 + 28 + pad + (looping ? 2 * (9 + rowHeight) : 0)
                                           : pad + sectionTitle + pad);

    const std::array<juce::Component*, 7> pitchChildren {
        &pitchDownButton, &pitchUpButton, &centsBar, &resetStretchButton,
        &reverseButton, &algorithmBox, &loopBox
    };

    for (auto* child : pitchChildren)
        child->setVisible (showPitch);

    loopStartBar.setVisible (showPitch && looping);
    crossfadeBar.setVisible (showPitch && looping);

    {
        auto r = pitchSection.reduced (pad);
        pitchTitleArea = r.removeFromTop (sectionTitle);
        r.removeFromTop (9);

        auto stepper = r.removeFromTop (26);
        pitchDownButton.setBounds (stepper.removeFromLeft (26));
        pitchUpButton.setBounds (stepper.removeFromRight (26));
        pitchValueArea = stepper.reduced (8, 0);
        r.removeFromTop (9);

        auto line = r.removeFromTop (rowHeight);
        centsLabel = line.removeFromLeft (labelWidth);
        line.removeFromLeft (8);
        centsValue = line.removeFromRight (valueWidth);
        line.removeFromRight (8);
        centsBar.setBounds (line);
        r.removeFromTop (9);

        line = r.removeFromTop (rowHeight);
        stretchLabel = line.removeFromLeft (labelWidth);
        line.removeFromLeft (8);
        resetStretchButton.setBounds (line.removeFromRight (34).withSizeKeepingCentre (34, 20));
        line.removeFromRight (8);
        stretchValue = line;
        r.removeFromTop (9);

        algorithmBox.setBounds (r.removeFromTop (28));
        r.removeFromTop (9);

        if (showPitch)
        {
            auto bottom = r.removeFromTop (28);
            reverseButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - 3));
            loopBox.setBounds (bottom.removeFromRight (bottom.getWidth() - 3));

            if (looping)
            {
                for (auto [bar, labelArea, valueArea] : { std::tuple { &loopStartBar, &loopStartLabel, &loopStartValue },
                                                          std::tuple { &crossfadeBar, &crossfadeLabel, &crossfadeValue } })
                {
                    r.removeFromTop (9);
                    auto loopLine = r.removeFromTop (rowHeight);
                    *labelArea = loopLine.removeFromLeft (labelWidth);
                    loopLine.removeFromLeft (8);
                    *valueArea = loopLine.removeFromRight (valueWidth);
                    loopLine.removeFromRight (8);
                    bar->setBounds (loopLine);
                }
            }
        }
    }

    // Effektkette
    const bool showEffects = ctx.ui.showEffectSection;
    effectSection = area;
    {
        auto r = effectSection.reduced (pad);
        auto title = r.removeFromTop (sectionTitle + 8);
        addEffectButton.setBounds (title.removeFromRight (26).withSizeKeepingCentre (26, 21));
        effectTitleArea = title;
        r.removeFromTop (9 - 8);

        for (auto& card : effectCards)
        {
            card->setVisible (showEffects);

            if (! showEffects)
                continue;

            card->setBounds (r.removeFromTop (card->getIdealHeight()));
            r.removeFromTop (cardGap);
        }

        if (showEffects)
        {
            r.removeFromTop (10);
            effectNote = r.removeFromTop (28);
        }
        else
        {
            effectNote = {};
        }
    }
}

void TrackInspectorContent::paintEffectTitle (juce::Graphics& g) const
{
    auto title = effectTitleArea;
    draw::disclosure (g, title.removeFromLeft (10), ctx.ui.showEffectSection, colours::textTertiary);
    title.removeFromLeft (4);
    draw::capsLabel (g, "Effekte der Spur", title);
}

void TrackInspectorContent::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;

    // Ein Klick auf die Überschrift klappt den Abschnitt zu oder auf
    if (pitchTitleArea.expanded (0, 4).contains (e.getPosition()))
    {
        ctx.ui.showPitchSection = ! ctx.ui.showPitchSection;
        ctx.ui.changed();
    }
    else if (effectTitleArea.expanded (0, 4).contains (e.getPosition()))
    {
        ctx.ui.showEffectSection = ! ctx.ui.showEffectSection;
        ctx.ui.changed();
    }
}

void TrackInspectorContent::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    const auto* track = getTrack();

    // Kopf: Farbquadrat und Spurname
    draw::bottomLine (g, headerArea, colours::lineFine);
    auto head = headerArea.reduced (pad, 0);
    g.setColour (track != nullptr ? track->colour : colours::textTertiary);
    g.fillRect (head.removeFromLeft (9).withSizeKeepingCentre (9, 9));
    head.removeFromLeft (8);
    draw::capsLabel (g, "Spur · "_u + (track != nullptr ? track->name : "keine"_u), head, 10.5f);

    draw::bottomLine (g, levelSection, colours::lineFine);
    draw::bottomLine (g, pitchSection, colours::lineFine);

    if (track == nullptr)
    {
        g.setColour (colours::textTertiary);
        g.setFont (sansFont (12.0f));
        g.drawText ("Keine Spur in dieser Zone"_u, levelSection.reduced (pad), juce::Justification::topLeft, true);
        return;
    }

    const auto smallMono = monoFont (10.0f);
    g.setFont (smallMono);

    auto label = [&g] (const juce::String& text, juce::Rectangle<int> area)
    {
        g.setColour (colours::textSecondary);
        g.drawText (text, area, juce::Justification::centredLeft, false);
    };
    auto value = [&g] (const juce::String& text, juce::Rectangle<int> area)
    {
        g.setColour (colours::textSecondary);
        g.drawText (text, area, juce::Justification::centredRight, false);
    };

    label ("GAIN", gainLabel);
    value (gainToText (track->gain), gainValue);
    label ("PAN", panLabel);
    value (panText (track->pan), panValue);
    label ("FADES", fadeLabel);

    // Fades, Dehnung und Loop-Werte in Sekunden gelten dem gewählten Clip der Spur
    const auto* clip = currentClip (ctx.model, ctx.ui);
    const double clipSeconds = clip != nullptr ? clip->length() * InstrumentModel::timelineSeconds : 0.0;
    g.setColour (colours::textSecondary);
    g.setFont (monoFont (11.0f));
    g.drawText (clip == nullptr ? juce::String ("kein Clip")
                                : juce::String (clipSeconds * clip->fadeIn, 2) + " s / " + juce::String (clipSeconds * clip->fadeOut, 2) + " s",
                fadeValue, juce::Justification::centredLeft, true);

    // Tonhöhe & Zeit
    {
        auto title = pitchTitleArea;
        draw::disclosure (g, title.removeFromLeft (10), ctx.ui.showPitchSection, colours::textTertiary);
        title.removeFromLeft (4);
        draw::capsLabel (g, "Tonhöhe & Zeit"_u, title);
    }

    if (! ctx.ui.showPitchSection)
    {
        paintEffectTitle (g);
        return;
    }

    g.setColour (colours::text);
    g.setFont (monoFont (14.0f));
    g.drawText ((track->pitch > 0 ? "+" : "") + juce::String (track->pitch) + " HT", pitchValueArea,
                juce::Justification::centred, false);

    g.setFont (smallMono);
    label ("CENT", centsLabel);
    const int cents = juce::roundToInt ((track->cents - 0.5f) * 100.0f);
    value ((cents > 0 ? "+" : "") + juce::String (cents), centsValue);
    label ("STRETCH", stretchLabel);

    g.setColour (colours::text);
    g.setFont (monoFont (12.0f));
    g.drawText (juce::String (clip != nullptr ? clip->stretch : 1.0, 2) + juce::String::fromUTF8 ("×"), stretchValue,
                juce::Justification::centredLeft, false);

    // Loop-Beginn in Sekunden des Clips, Überblendung in Millisekunden
    if (isLooping())
    {
        const double loopSeconds = clipSeconds * (1.0 - track->loopStart);

        g.setFont (smallMono);
        label ("LOOP AB", loopStartLabel);
        value (juce::String (clipSeconds * track->loopStart, 2) + " s", loopStartValue);
        label ("X-FADE", crossfadeLabel);
        value (juce::String (juce::roundToInt (loopSeconds * track->loopCrossfade * 1000.0)) + " ms", crossfadeValue);
    }

    // Effektkette
    paintEffectTitle (g);

    if (! ctx.ui.showEffectSection)
        return;

    if (effectCards.empty())
    {
        g.setColour (colours::textTertiary);
        g.setFont (monoFont (10.0f));
        g.drawText ("Noch keine Effekte · „+“ fügt einen ein"_u, effectNote, juce::Justification::topLeft, true);
    }
    else
    {
        g.setColour (colours::textTertiary);
        g.setFont (monoFont (10.0f));
        g.drawFittedText ("Alle Effekte wirken sofort · eigene wandern beim Export mit"_u, effectNote,
                          juce::Justification::topLeft, 2);
    }
}

void TrackInspectorContent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildIfNeeded();
}

//==============================================================================
TrackInspector::TrackInspector (StudioContext& c) : ctx (c), content (c)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    ctx.ui.addChangeListener (this);
}

TrackInspector::~TrackInspector()
{
    ctx.ui.removeChangeListener (this);
}

void TrackInspector::changeListenerCallback (juce::ChangeBroadcaster*)
{
    resized();
    repaint();
    content.repaint();   // ein anderer Clip gewählt: Fades und Dehnung zeigen dessen Werte
}

void TrackInspector::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::line);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
}

void TrackInspector::resized()
{
    viewport.setBounds (getLocalBounds().withTrimmedRight (1));
    const int height = content.getIdealHeight();
    const bool needsScroll = height > viewport.getHeight();
    content.setSize (viewport.getWidth() - (needsScroll ? viewport.getScrollBarThickness() : 0),
                     juce::jmax (height, viewport.getHeight()));
}
} // namespace sis
