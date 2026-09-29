#include "PluginShell.h"
#include "../../PluginProcessor.h"
#include "../../Model/XmlFile.h"
#include "../../StudioHandover.h"

namespace sis
{
namespace
{
    constexpr int labelHeight = 17;
    constexpr int knobSlot = 64;   // Regler 44 px plus Luft für die Beschriftung
}

PluginShell::PluginShell (StudioProcessor& p)
    : processor (p),
      ctx { p, p.getModel(), ui, commands,
            [this] (const juce::String& m) { showToast (m); },
            [this] (const juce::String& n) { processor.getHistory().nameNextStep (n); } },
      icon (appIcon()),
      zones (ctx),
      keyboard (p.getModel(), p.getKeyboardState()),
      sheet (ctx)
{
    ui.velocityLayers = false;   // Zonenstreifen: nur horizontal

    keyboard.onHighlightChanged = [this] (int note, int velocity)
    {
        if (note < 0)
        {
            footerText.clear();
        }
        else
        {
            const auto* zone = processor.getModel().findZoneForNote (note, velocity);
            footerText = "Note " + noteName (note) + " → "_u + (zone != nullptr ? zone->name : "keine Zone");
        }
        repaint (footer);
    };

    for (int macro = 0; macro < InstrumentModel::numMacros; ++macro)
    {
        auto& knob = macroKnobs[(size_t) macro];
        knob.setLabel (macroName (macro));

        knob.onDragStart = [this, macro] { processor.getHistory().nameNextStep (macroName (macro)); };

        knob.onValueChange = [this, macro] (float v)
        {
            auto& model = processor.getModel();
            model.macros[(size_t) macro] = v;
            model.applyMacro (macro);
            model.notifyChanged();
        };

        addAndMakeVisible (knob);
    }

    FlatButton::Style hostStyle;
    hostStyle.background = juce::Colours::transparentBlack;
    hostStyle.border = juce::Colours::transparentBlack;
    hostStyle.text = colours::windowText;
    hostStyle.hoverBackground = colours::windowBarLine;
    hostStyle.fontSize = 10.0f;
    hostStyle.mono = true;

    studioButton.setStyle (hostStyle);
    studioButton.onClick = [this]
    {
        const auto app = handover::findStudioExecutable();

        if (app == juce::File())
        {
            showToast ("Sample Instrument Studio wurde auf diesem Rechner noch nie gestartet"_u);
            return;
        }

        const auto file = handover::transferFile();
        auto xml = processor.createProjectState().createXml();

        if (xml == nullptr || ! writeXmlDirectly (*xml, file))
        {
            showToast ("Der Stand konnte nicht übergeben werden"_u);
            return;
        }

        if (app.startAsProcess (file.getFullPathName().quoted()))
            showToast ("Der aktuelle Stand wird im Studio geöffnet"_u);
        else
            showToast ("Das Studio ließ sich nicht starten"_u);
    };

    FlatButton::Style fieldStyle;
    fieldStyle.background = colours::surface;
    fieldStyle.border = colours::lineStrongAlt;
    fieldStyle.text = colours::text;
    fieldStyle.hoverBackground = colours::rowHover;
    fieldStyle.fontSize = 11.5f;

    presetButton.setStyle (fieldStyle);
    presetButton.onClick = [this] { showPresetMenu(); };

    playButton.setStyle (fieldStyle);
    playButton.onClick = [this] { togglePlayback(); };

    zones.onOpenZone = [this] (const juce::String&)
    {
        sheet.setVisible (true);
        sheet.toFront (false);
        sheet.resized();
    };

    sheet.onOpenStudio = [this]
    {
        sheet.setVisible (false);
        studioButton.triggerClick();
    };

    // Siehe PluginEditor.cpp: ohne DAW ist der Dialog sonst nicht anzusehen
    sheet.setVisible (juce::SystemStats::getEnvironmentVariable ("SIS_PLUGIN_VIEW", {}) == "sheet");

    addAndMakeVisible (zones);
    addAndMakeVisible (keyboard);
    addAndMakeVisible (studioButton);
    addAndMakeVisible (presetButton);
    addAndMakeVisible (playButton);
    addChildComponent (sheet);
    addChildComponent (toast);

    processor.getModel().addChangeListener (this);
    ui.addChangeListener (this);
    updateMacroKnobs();
    changeListenerCallback (nullptr);
}

PluginShell::~PluginShell()
{
    if (auditionNote >= 0)
        processor.getKeyboardState().noteOff (1, auditionNote, 0.0f);

    ui.removeChangeListener (this);
    processor.getModel().removeChangeListener (this);
}

void PluginShell::togglePlayback()
{
    /* Dieselbe Vorhörfunktion wie im Studio: die Taste hält den Grundton der gewählten
       Zone, sie startet keinen eigenen Transport. Den gibt die DAW vor. */
    if (auditionNote >= 0)
    {
        processor.getKeyboardState().noteOff (1, auditionNote, 0.0f);
        auditionNote = -1;
        ui.playing = false;
        ui.changed();
        return;
    }

    const auto* zone = processor.getModel().getSelectedZone();

    if (zone == nullptr)
    {
        showToast ("Keine Zone gewählt"_u);
        return;
    }

    if (! processor.zoneHasAudio (*zone))
    {
        showToast ("Zone " + zone->name + " hat keine Audiodaten"_u);
        return;
    }

    auditionNote = zone->rootNote;
    processor.getKeyboardState().noteOn (1, auditionNote, 0.8f);
    ui.playing = true;
    ui.changed();
}

void PluginShell::showToast (const juce::String& message)
{
    toast.show (message);
    toast.updatePosition();
    toast.toFront (false);
}

juce::File PluginShell::presetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile (JucePlugin_Name)
               .getChildFile ("Presets");
}

void PluginShell::showPresetMenu()
{
    juce::PopupMenu menu;

    auto files = presetFolder().findChildFiles (juce::File::findFiles, false, "*.sisp");
    files.sort();

    const auto current = processor.getModel().projectFile;

    for (int i = 0; i < files.size(); ++i)
        menu.addItem (i + 1, files[i].getFileNameWithoutExtension(), true, files[i] == current);

    if (files.isEmpty())
        menu.addItem (-1, "Noch keine Presets gesichert"_u, false, false);

    menu.addSeparator();
    menu.addItem (1000, "Aktuellen Stand sichern …"_u);
    menu.addItem (1001, "Ordner zeigen …"_u);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton),
                        [this, files] (int result)
    {
        if (result == 1000)
            savePreset();
        else if (result == 1001)
        {
            presetFolder().createDirectory();
            presetFolder().revealToUser();
        }
        else if (result > 0 && result <= files.size())
            loadPreset (files[result - 1]);
    });
}

void PluginShell::loadPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr
        || ! processor.loadProjectState (juce::ValueTree::fromXml (*xml), file.getParentDirectory()))
    {
        showToast ("Preset konnte nicht gelesen werden: "_u + file.getFileName());
        return;
    }

    processor.getModel().projectFile = file;
    processor.getModel().notifyChanged();
    showToast (file.getFileNameWithoutExtension() + " geladen");
}

void PluginShell::savePreset()
{
    nameWindow = std::make_unique<juce::AlertWindow> ("Preset sichern", "Name des Presets:",
                                                      juce::MessageBoxIconType::NoIcon, this);
    nameWindow->addTextEditor ("name", processor.getModel().name);
    nameWindow->addButton ("Sichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameWindow->addButton ("Abbrechen"_u, 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PluginShell> safe (this);
    nameWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr)
            return;

        const auto name = safe->nameWindow->getTextEditorContents ("name").trim();
        safe->nameWindow = nullptr;

        if (result == 0 || name.isEmpty())
            return;

        const auto file = presetFolder().getChildFile (juce::File::createLegalFileName (name) + ".sisp");
        auto xml = safe->processor.createProjectState().createXml();

        if (xml == nullptr || ! writeXmlDirectly (*xml, file))
        {
            safe->showToast ("Preset konnte nicht geschrieben werden"_u);
            return;
        }

        safe->processor.getModel().projectFile = file;
        safe->showToast (name + " gesichert");
    }), false);
}

void PluginShell::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    // Host-Leiste
    g.setColour (colours::windowHover);
    g.fillRect (hostBar);
    g.setFont (monoFont (10.0f));
    g.setColour (colours::windowText);
    g.drawText ("HOST · INSTRUMENT"_u, hostBar.reduced (12, 0), juce::Justification::centredLeft, false);
    g.drawText ("SISTUDIO VST3", hostBar.withTrimmedRight (studioButton.getWidth() + 18),
                juce::Justification::centredRight, false);

    // Kopf
    g.setColour (colours::bars);
    g.fillRect (header);
    draw::bottomLine (g, header, colours::line);

    auto h = header.reduced (12, 0);
    if (icon.isValid())
        g.drawImage (icon, h.removeFromLeft (18).withSizeKeepingCentre (18, 18).toFloat(), juce::RectanglePlacement::centred);
    h.removeFromLeft (9);
    g.setColour (colours::text);
    g.setFont (sansFont (13.0f, Weight::semibold));
    g.drawText (processor.getModel().name, h.withTrimmedRight (presetButton.getWidth() + playButton.getWidth() + 20),
                juce::Justification::centredLeft, true);

    // Abschnittstitel über Zonenstreifen und Makros
    draw::capsLabel (g, "Zonen", zoneLabelArea);

    auto hint = zoneLabelArea;
    hint.removeFromLeft (juce::jmin (54, hint.getWidth()));
    g.setColour (colours::textTertiary);
    g.setFont (monoFont (10.0f));
    g.drawText ("Doppelklick öffnet die Bearbeitung"_u, hint, juce::Justification::centredLeft, false);

    draw::capsLabel (g, "Makros", macroLabelArea);

    // Fußzeile
    g.setColour (colours::bars);
    g.fillRect (footer);
    g.setColour (colours::line);
    g.fillRect (footer.getX(), footer.getY(), footer.getWidth(), 1);
    g.setFont (monoFont (10.0f));
    g.setColour (colours::textSecondary);

    const auto* zone = processor.getModel().getSelectedZone();
    const auto context = footerText.isNotEmpty() ? footerText
                                                 : (zone != nullptr ? "Zone " + zone->name : juce::String ("Keine Zone"));
    g.drawText (context, footer.reduced (12, 0), juce::Justification::centredLeft, true);
    g.drawText (juce::String (processor.getActiveVoiceCount()) + " Stimmen", footer.reduced (12, 0),
                juce::Justification::centredRight, false);
}

void PluginShell::resized()
{
    auto area = getLocalBounds();
    hostBar = area.removeFromTop (30);
    header = area.removeFromTop (44);
    footer = area.removeFromBottom (26);

    {
        auto bar = hostBar.reduced (10, 0);
        const int width = studioButton.getTextWidth() + 12;
        studioButton.setBounds (bar.removeFromRight (width).withSizeKeepingCentre (width, 20));
    }

    {
        auto h = header.reduced (12, 0);
        playButton.setBounds (h.removeFromRight (32).withSizeKeepingCentre (32, 28));
        h.removeFromRight (9);
        presetButton.setBounds (h.removeFromRight (juce::jmin (190, h.getWidth() / 2))
                                    .withSizeKeepingCentre (juce::jmin (190, h.getWidth() / 2), 28));
    }

    area.reduce (12, 12);
    keyboard.setBounds (area.removeFromBottom (metrics::pluginKeyboard));
    area.removeFromBottom (12);

    /* Zonenstreifen und Makros stehen nebeneinander, und der Streifen bleibt bei seinen
       50 px. Wächst das Fenster, wächst nicht der Streifen mit - er hätte nichts mit der
       Höhe anzufangen; der Platz bleibt darunter frei. */
    auto row = area.removeFromTop (labelHeight + 5 + juce::jmax (metrics::pluginZoneStrip,
                                                                 MacroKnob::fullHeight));

    const int macroWidth = InstrumentModel::numMacros * knobSlot + (InstrumentModel::numMacros - 1) * 11;
    auto macroColumn = row.removeFromRight (juce::jmin (macroWidth, row.getWidth() / 2));
    row.removeFromRight (16);

    macroLabelArea = macroColumn.removeFromTop (labelHeight);
    macroColumn.removeFromTop (5);
    macroArea = macroColumn.removeFromTop (MacroKnob::fullHeight);

    {
        auto knobs = macroArea;
        const int slot = (knobs.getWidth() - (InstrumentModel::numMacros - 1) * 11)
                             / InstrumentModel::numMacros;

        for (auto& knob : macroKnobs)
        {
            knob.setBounds (knobs.removeFromLeft (slot));
            knobs.removeFromLeft (11);
        }
    }

    zoneLabelArea = row.removeFromTop (labelHeight);
    row.removeFromTop (5);
    zones.setBounds (row.removeFromTop (metrics::pluginZoneStrip));

    sheet.setBounds (getLocalBounds());
    toast.updatePosition();
}

void PluginShell::mouseUp (const juce::MouseEvent&)
{
    grabKeyboardFocus();
}

void PluginShell::updateMacroKnobs()
{
    const auto& model = processor.getModel();

    for (int macro = 0; macro < InstrumentModel::numMacros; ++macro)
    {
        auto& knob = macroKnobs[(size_t) macro];
        knob.setValue (model.macros[(size_t) macro]);
        knob.setAssigned (! model.macroTargets[(size_t) macro].empty());
    }
}

void PluginShell::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateMacroKnobs();

    const auto& model = processor.getModel();
    presetButton.setButtonText ("Preset: "_u + (model.projectFile != juce::File()
                                                    ? model.projectFile.getFileNameWithoutExtension()
                                                    : model.name));

    playButton.setIcon (ui.playing ? &draw::stopIcon : &draw::playIcon);
    repaint();
}
} // namespace sis
