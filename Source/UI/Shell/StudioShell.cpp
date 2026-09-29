#include "StudioShell.h"
#include "../../Model/XmlFile.h"
#include "CreditsWindow.h"
#include "MacroWindow.h"
#include "../../PluginProcessor.h"

namespace sis
{
namespace
{
    const juce::String audioWildcard ("*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    juce::File defaultProjectFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    }
}

//==============================================================================
PendingView::PendingView (juce::String t, juce::String d)
    : title (std::move (t)), description (std::move (d))
{
}

void PendingView::paint (juce::Graphics& g)
{
    g.fillAll (colours::workspace);

    auto area = getLocalBounds().reduced (16, 14);
    g.setColour (colours::text);
    g.setFont (sansFont (13.0f, Weight::semibold));
    g.drawText (title, area.removeFromTop (28), juce::Justification::centredLeft, false);

    g.setColour (colours::textTertiary);
    g.setFont (monoFont (10.0f));
    g.drawFittedText (description, area.removeFromTop (40), juce::Justification::topLeft, 3);
}

//==============================================================================
StudioShell::StudioShell (StudioProcessor& p, bool drawOwnTitleBar)
    : processor (p),
      ctx { p, p.getModel(), ui, commandManager,
            [this] (const juce::String& m) { showToast (m); },
            [this] (const juce::String& n) { processor.getHistory().nameNextStep (n); } },
      hasTitleBar (drawOwnTitleBar),
      titleBar (p.getModel()),
      toolBar (ctx),
      sampleBrowser (ctx),
      trackInspector (ctx),
      zonePanel (ctx),
      mappingView (ctx),
      editorView (ctx),
      exportView (ctx),
      statusBar (ctx)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    menuBar.setModel (this);

    addChildComponent (titleBar);
    titleBar.setVisible (hasTitleBar);
    addAndMakeVisible (menuBar);
    addAndMakeVisible (toolBar);
    addAndMakeVisible (sampleBrowser);
    addChildComponent (trackInspector);
    addAndMakeVisible (zonePanel);
    addAndMakeVisible (mappingView);
    addChildComponent (editorView);
    addChildComponent (exportView);
    addAndMakeVisible (statusBar);
    addChildComponent (toast);

    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    addKeyListener (commandManager.getKeyMappings());

    // Klicks irgendwo im Rahmen holen den Tastaturfokus zurück (außer in Textfeldern),
    // damit Leertaste und F-Tasten immer greifen.
    addMouseListener (this, true);

    statusBar.onAudioSettingsRequested = [this]
    {
        if (onShowAudioSettings != nullptr)
            onShowAudioSettings();
    };

    ui.addChangeListener (this);


    // Menübeschriftungen und Ausgrauen folgen dem Verlauf
    processor.getHistory().onChanged = [this] { commandManager.commandStatusChanged(); };

    juce::Component::SafePointer<StudioShell> safe (this);
    juce::Timer::callAfterDelay (200, [safe]
    {
        if (safe != nullptr && safe->isShowing())
            safe->grabKeyboardFocus();
    });

    // Ohne offenes Audiogerät bleibt alles stumm - darauf gleich hinweisen
    juce::Timer::callAfterDelay (2500, [safe]
    {
        if (safe != nullptr && safe->onShowAudioSettings != nullptr && safe->processor.getSampleRate() <= 0.0)
            safe->showToast ("Kein Audioausgang aktiv · Datei → Audio-Einstellungen"_u);
    });
}

StudioShell::~StudioShell()
{
    // Der Prozessor überlebt die Oberfläche - den Rückruf nicht hängen lassen
    processor.getHistory().onChanged = nullptr;

    stopAudition();
    stopTimer();
    ui.removeChangeListener (this);
    removeMouseListener (this);
    removeKeyListener (commandManager.getKeyMappings());
    menuBar.setModel (nullptr);
}

void StudioShell::showToast (const juce::String& message)
{
    toast.show (message);
}

//==============================================================================
void StudioShell::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void StudioShell::resized()
{
    auto area = getLocalBounds();

    if (hasTitleBar)
        titleBar.setBounds (area.removeFromTop (metrics::titleBar));

    menuBar.setVisible (ui.showMenuBar);
    if (ui.showMenuBar)
        menuBar.setBounds (area.removeFromTop (metrics::menuBar));

    toolBar.setBounds (area.removeFromTop (metrics::toolBar));
    statusBar.setBounds (area.removeFromBottom (metrics::statusBar));

    // Im Editor: links die gewählte Spur, rechts die Sample-Liste.
    // In Mapping und Export: links die Sample-Liste, rechts die Zone.
    const bool inEditor = ui.view == View::editor;
    sampleBrowser.setBorderOnLeft (inEditor);

    trackInspector.setVisible (ui.showLeftPanel && inEditor);
    if (ui.showLeftPanel)
    {
        const auto left = area.removeFromLeft (metrics::leftPanel);

        if (inEditor)
            trackInspector.setBounds (left);
        else
            sampleBrowser.setBounds (left);
    }

    zonePanel.setVisible (ui.showRightPanel && ! inEditor);
    if (ui.showRightPanel)
    {
        const auto right = area.removeFromRight (metrics::rightPanel);

        if (inEditor)
            sampleBrowser.setBounds (right);
        else
            zonePanel.setBounds (right);
    }

    sampleBrowser.setVisible ((inEditor && ui.showRightPanel) || (! inEditor && ui.showLeftPanel));

    mappingView.setVisible (ui.view == View::mapping);
    editorView.setVisible (ui.view == View::editor);
    exportView.setVisible (ui.view == View::exportView);

    for (auto* view : { static_cast<juce::Component*> (&mappingView), static_cast<juce::Component*> (&editorView),
                        static_cast<juce::Component*> (&exportView) })
        view->setBounds (area);

    toast.updatePosition();
}

namespace
{
    /* Klaviaturbelegung wie auf einem deutschen Tastenfeld: die Heimatreihe sind die
       weißen Tasten, die Reihe darüber die schwarzen. Der Halbtonabstand zum Grundton
       der Reihe steht daneben. */
    struct ComputerKey { juce::juce_wchar character; int semitone; };

    const ComputerKey computerKeys[] = {
        { 'a', 0 }, { 'w', 1 }, { 's', 2 }, { 'e', 3 }, { 'd', 4 }, { 'f', 5 },
        { 't', 6 }, { 'g', 7 }, { 'z', 8 }, { 'h', 9 }, { 'u', 10 }, { 'j', 11 },
        { 'k', 12 }, { 'o', 13 }, { 'l', 14 }
    };
}

int StudioShell::computerKeyToNote (int keyCode) const
{
    /* Die Reihe liegt in der Oktave der gewählten Zone: so spielt man immer dort,
       wo man gerade arbeitet, ohne eine Oktavtaste zu brauchen. */
    const auto* zone = processor.getModel().getSelectedZone();
    const int base = zone != nullptr ? (zone->rootNote / 12) * 12 : 60;

    for (const auto& key : computerKeys)
        if (keyCode == (int) juce::CharacterFunctions::toUpperCase (key.character)
            || keyCode == (int) key.character)
            return juce::jlimit (0, 127, base + key.semitone);

    return -1;
}

void StudioShell::updateComputerKeyboard()
{
    std::set<int> nowHeld;

    for (const auto& key : computerKeys)
    {
        const int code = (int) juce::CharacterFunctions::toUpperCase (key.character);

        if (juce::KeyPress::isKeyCurrentlyDown (code))
        {
            const int note = computerKeyToNote (code);

            if (note >= 0)
                nowHeld.insert (note);
        }
    }

    if (nowHeld == heldComputerNotes)
        return;

    auto& keyboard = processor.getKeyboardState();

    for (const int note : heldComputerNotes)
        if (nowHeld.count (note) == 0)
            keyboard.noteOff (1, note, 0.0f);

    for (const int note : nowHeld)
        if (heldComputerNotes.count (note) == 0)
            keyboard.noteOn (1, note, 0.8f);

    heldComputerNotes = nowHeld;
}

void StudioShell::releaseComputerKeyboard()
{
    if (heldComputerNotes.empty())
        return;

    auto& keyboard = processor.getKeyboardState();

    for (const int note : heldComputerNotes)
        keyboard.noteOff (1, note, 0.0f);

    heldComputerNotes.clear();
}

bool StudioShell::keyStateChanged (bool)
{
    // Gedrückt halten heißt klingen lassen - ob es dann liegen bleibt, entscheidet
    // der Loop-Modus der Spur
    updateComputerKeyboard();
    return false;   // die Taste bleibt für alles andere durchlässig
}

void StudioShell::focusLost (FocusChangeType)
{
    releaseComputerKeyboard();
}

void StudioShell::mouseDown (const juce::MouseEvent& e)
{
    auto* source = e.eventComponent;
    const bool inTextEditor = dynamic_cast<juce::TextEditor*> (source) != nullptr
                              || (source != nullptr && source->findParentComponentOfClass<juce::TextEditor>() != nullptr);

    /* Eine Komponente, die selbst Tasten braucht (die Spuren im Editor für Entf, X und die
       Zwischenablage), behält ihren Fokus. Früher holte sich die Shell ihn bei jedem Klick
       zurück – dann kam Entf im Editor nur an, wenn die Reihenfolge zufällig passte. */
    const bool wantsKeys = dynamic_cast<TrackArea*> (source) != nullptr;

    if (! inTextEditor && ! wantsKeys && ! hasKeyboardFocus (false))
        grabKeyboardFocus();
}

void StudioShell::changeListenerCallback (juce::ChangeBroadcaster*)
{
    resized();
    repaint();
}

//==============================================================================
juce::StringArray StudioShell::getMenuBarNames()
{
    return { "Datei", "Bearbeiten", "Ansicht", "Instrument", "Hilfe" };
}

juce::PopupMenu StudioShell::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu menu;

    auto add = [&menu] (juce::CommandID id, const juce::String& text, const juce::String& shortcut, bool ticked = false)
    {
        juce::PopupMenu::Item item (text);
        item.itemID = (int) id;
        item.shortcutKeyDescription = shortcut;
        item.isTicked = ticked;
        menu.addItem (std::move (item));
    };

    switch (index)
    {
        case 0:
            add (cmd::newInstrument, "Neues Instrument", "Strg+N");
            add (cmd::openProject, "Öffnen … (.sisp)"_u, "Strg+O");
            add (cmd::saveProject, "Speichern", "Strg+S");
            add (cmd::saveProjectAs, "Speichern unter …"_u, {});
            menu.addSeparator();
            add (cmd::importSamples, "Samples importieren …"_u, "Strg+I");
            add (cmd::exportInstrument, "Instrument exportieren …"_u, "Strg+E");
            if (onShowAudioSettings != nullptr)
                add (cmd::audioSettings, "Audio-Einstellungen …"_u, "F12");
            menu.addSeparator();
            add (cmd::quit, "Beenden", "Alt+F4");
            break;

        case 1:
        {
            // Der Name des Schritts steht mit im Menü: "Rückgängig: Spur gelöscht"
            auto& history = processor.getHistory();
            const auto undoName = history.getUndoName();
            const auto redoName = history.getRedoName();

            add (cmd::undo, undoName.isNotEmpty() ? "Rückgängig: "_u + undoName : "Rückgängig"_u, "Strg+Z");
            add (cmd::redo, redoName.isNotEmpty() ? "Wiederherstellen: "_u + redoName : "Wiederherstellen"_u,
                 "Strg+Y");
        }
            menu.addSeparator();
            add (cmd::cut, "Ausschneiden", "Strg+X");
            add (cmd::copy, "Kopieren", "Strg+C");
            add (cmd::paste, "Einfügen"_u, "Strg+V");
            menu.addSeparator();
            add (cmd::trimToSelection, "Auf Auswahl zuschneiden", {});
            break;

        case 2:
            add (cmd::showMapping, "Mapping", "F2");
            add (cmd::showEditor, "Editor", "F3");
            add (cmd::showExport, "Export", "F4");
            menu.addSeparator();
            add (cmd::toggleMenuBar, "Menüleiste"_u, "Strg+M", ui.showMenuBar);
            add (cmd::toggleLeftPanel, "Linke Spalte", "F9", ui.showLeftPanel);
            add (cmd::toggleRightPanel, "Rechte Spalte", "F10", ui.showRightPanel);
            menu.addSeparator();
            add (cmd::toggleVelocityLayers, "Velocity-Layer", {}, ui.velocityLayers);
            add (cmd::toggleSnap, "Am Raster einrasten", {}, ui.snapToGrid);
            break;

        case 3:
            add (cmd::addZone, "Zone hinzufügen"_u, {});
            add (cmd::addTrack, "Spur hinzufügen"_u, {});
            menu.addSeparator();
            add (cmd::bounceZone, "Zone bouncen", {});
            add (cmd::assignMacros, "Makros zuweisen …"_u, {});
            break;

        case 4:
            add (cmd::openManual, "Handbuch", "F1");
            add (cmd::showShortcuts, "Tastaturkürzel"_u, {});
            menu.addSeparator();
            add (cmd::credits, "Über "_u + juce::String (JucePlugin_Name), {});
            break;

        default:
            break;
    }

    return menu;
}

void StudioShell::menuItemSelected (int menuItemID, int)
{
    commandManager.invokeDirectly (menuItemID, true);
}

//==============================================================================
void StudioShell::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    commands.addArray ({ cmd::newInstrument, cmd::openProject, cmd::saveProject, cmd::saveProjectAs,
                         cmd::importSamples, cmd::exportInstrument, cmd::audioSettings, cmd::quit,
                         cmd::undo, cmd::redo, cmd::cut, cmd::copy, cmd::paste, cmd::trimToSelection,
                         cmd::showMapping, cmd::showEditor, cmd::showExport,
                         cmd::toggleMenuBar, cmd::toggleLeftPanel, cmd::toggleRightPanel,
                         cmd::toggleVelocityLayers, cmd::toggleSnap,
                         cmd::addZone, cmd::addTrack, cmd::bounceZone, cmd::assignMacros,
                         cmd::openManual, cmd::showShortcuts, cmd::credits,
                         cmd::togglePlayback });
}

void StudioShell::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    using KP = juce::KeyPress;
    const auto ctrl = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;

    auto set = [&info] (const juce::String& name, const char* category)
    {
        info.setInfo (name, name, category, 0);
    };

    switch (id)
    {
        case cmd::newInstrument:    set ("Neues Instrument", "Datei");              info.addDefaultKeypress ('n', ctrl); break;
        case cmd::openProject:      set ("Öffnen"_u, "Datei");                       info.addDefaultKeypress ('o', ctrl); break;
        case cmd::saveProject:      set ("Speichern", "Datei");                     info.addDefaultKeypress ('s', ctrl); break;
        case cmd::saveProjectAs:    set ("Speichern unter", "Datei");               info.addDefaultKeypress ('s', ctrl | shift); break;
        case cmd::importSamples:    set ("Samples importieren", "Datei");           info.addDefaultKeypress ('i', ctrl); break;
        case cmd::exportInstrument: set ("Instrument exportieren", "Datei");        info.addDefaultKeypress ('e', ctrl); break;
        case cmd::audioSettings:
            set ("Audio-Einstellungen"_u, "Datei");
            info.addDefaultKeypress (KP::F12Key, 0);
            break;
        case cmd::quit:             set ("Beenden", "Datei");                       break;

        case cmd::undo:
            set ("Rückgängig"_u, "Bearbeiten");
            info.addDefaultKeypress ('z', ctrl);
            info.setActive (processor.getHistory().canUndo());
            break;

        case cmd::redo:
            set ("Wiederherstellen", "Bearbeiten");
            info.addDefaultKeypress ('y', ctrl);
            info.setActive (processor.getHistory().canRedo());
            break;
        case cmd::cut:              set ("Ausschneiden", "Bearbeiten");             info.addDefaultKeypress ('x', ctrl); break;
        case cmd::copy:             set ("Kopieren", "Bearbeiten");                 info.addDefaultKeypress ('c', ctrl); break;
        case cmd::paste:            set ("Einfügen"_u, "Bearbeiten");                info.addDefaultKeypress ('v', ctrl); break;
        case cmd::trimToSelection:  set ("Auf Auswahl zuschneiden", "Bearbeiten");  break;

        case cmd::showMapping:      set ("Mapping", "Ansicht");                     info.addDefaultKeypress (KP::F2Key, 0); break;
        case cmd::showEditor:       set ("Editor", "Ansicht");                      info.addDefaultKeypress (KP::F3Key, 0); break;
        case cmd::showExport:       set ("Export", "Ansicht");                      info.addDefaultKeypress (KP::F4Key, 0); break;

        case cmd::toggleMenuBar:
            set ("Menüleiste"_u, "Ansicht");
            info.setTicked (ui.showMenuBar);
            info.addDefaultKeypress ('m', ctrl);
            break;
        case cmd::toggleLeftPanel:
            set ("Linke Spalte", "Ansicht");
            info.setTicked (ui.showLeftPanel);
            info.addDefaultKeypress (KP::F9Key, 0);
            break;
        case cmd::toggleRightPanel:
            set ("Rechte Spalte", "Ansicht");
            info.setTicked (ui.showRightPanel);
            info.addDefaultKeypress (KP::F10Key, 0);
            break;
        case cmd::toggleVelocityLayers:
            set ("Velocity-Layer", "Ansicht");
            info.setTicked (ui.velocityLayers);
            break;
        case cmd::toggleSnap:
            set ("Am Raster einrasten", "Ansicht");
            info.setTicked (ui.snapToGrid);
            break;

        case cmd::addZone:          set ("Zone hinzufügen"_u, "Instrument");         break;
        case cmd::addTrack:         set ("Spur hinzufügen"_u, "Instrument");         break;
        case cmd::bounceZone:       set ("Zone bouncen", "Instrument");             break;
        case cmd::assignMacros:     set ("Makros zuweisen", "Instrument");          break;

        case cmd::openManual:       set ("Handbuch", "Hilfe");                      info.addDefaultKeypress (KP::F1Key, 0); break;
        case cmd::showShortcuts:    set ("Tastaturkürzel"_u, "Hilfe");               break;
        case cmd::credits:          set ("Über "_u + juce::String (JucePlugin_Name), "Hilfe"); break;

        case cmd::togglePlayback:   set ("Wiedergabe", "Transport");                info.addDefaultKeypress (KP::spaceKey, 0); break;

        default: break;
    }
}

bool StudioShell::isRepeatedKeyInvocation (const InvocationInfo& info)
{
    if (info.invocationMethod != InvocationInfo::fromKeyPress)
        return false;

    const auto now = juce::Time::getMillisecondCounter();
    const bool repeated = info.commandID == lastKeyCommand && now - lastKeyCommandTime < 150;

    lastKeyCommand = info.commandID;
    lastKeyCommandTime = now;
    return repeated;
}

bool StudioShell::perform (const InvocationInfo& info)
{
    // Ein Tastendruck darf einen Befehl nur einmal auslösen
    if (isRepeatedKeyInvocation (info))
        return true;

    auto notYet = [this, &info] (const juce::String& what)
    {
        showToast (commandManager.getNameOfCommand (info.commandID) + " · "_u + what);
    };

    switch (info.commandID)
    {
        case cmd::newInstrument:    newInstrument(); break;
        case cmd::openProject:      openProject(); break;
        case cmd::saveProject:      saveProject (false); break;
        case cmd::saveProjectAs:    saveProject (true); break;
        case cmd::importSamples:    importSamples(); break;
        case cmd::exportInstrument: setView (View::exportView); break;

        case cmd::audioSettings:
            if (onShowAudioSettings != nullptr)
                onShowAudioSettings();
            break;

        case cmd::quit:
            if (auto* app = juce::JUCEApplicationBase::getInstance())
                app->systemRequestedQuit();
            break;

        case cmd::undo:
        {
            auto& history = processor.getHistory();
            const auto name = history.getUndoName();
            showToast (history.undo() ? "Rückgängig: "_u + name : "Nichts zurückzunehmen"_u);
            break;
        }

        case cmd::redo:
        {
            auto& history = processor.getHistory();
            const auto name = history.getRedoName();
            showToast (history.redo() ? "Wiederhergestellt: "_u + name : "Nichts wiederherzustellen"_u);
            break;
        }

        // Die Zwischenablage gilt den Clips im Editor
        case cmd::cut:
        case cmd::copy:
        case cmd::paste:
            if (ui.view != View::editor)
            {
                showToast ("Ausschneiden, Kopieren und Einfügen gelten den Clips im Editor"_u);
                break;
            }

            if (info.commandID == cmd::copy)       editorView.copyClips();
            else if (info.commandID == cmd::cut)   editorView.cutClips();
            else                                   editorView.pasteClips();
            break;

        case cmd::trimToSelection:
        {
            auto* clip = currentClip (processor.getModel(), ui);

            if (clip == nullptr)
            {
                showToast ("Kein Clip gewählt"_u);
                break;
            }

            const double a = juce::jmin (ui.selectionStart, ui.selectionEnd);
            const double b = juce::jmax (ui.selectionEnd, a + 0.02);
            processor.getHistory().nameNextStep ("Auf Auswahl zugeschnitten"_u);
            clip->trimStart = a;
            clip->trimEnd = b;
            processor.getModel().notifyChanged();
            setView (View::editor);
            showToast ("Clip auf Auswahl zugeschnitten · "_u + clip->sample);
            break;
        }

        case cmd::showMapping: setView (View::mapping); break;
        case cmd::showEditor:  setView (View::editor); break;
        case cmd::showExport:  setView (View::exportView); break;

        case cmd::toggleMenuBar:    toggleChrome (ui.showMenuBar, "Menüleiste"_u, "Strg+M"); break;
        case cmd::toggleLeftPanel:  toggleChrome (ui.showLeftPanel, "Linke Spalte", "F9"); break;
        case cmd::toggleRightPanel: toggleChrome (ui.showRightPanel, "Rechte Spalte", "F10"); break;

        case cmd::toggleVelocityLayers:
            ui.velocityLayers = ! ui.velocityLayers;
            ui.changed();
            break;

        case cmd::toggleSnap:
            ui.snapToGrid = ! ui.snapToGrid;
            ui.changed();
            showToast (ui.snapToGrid ? "Raster ein · 1/16 der Zeitachse"_u : "Raster aus");
            break;

        case cmd::addZone:
            processor.getHistory().nameNextStep ("Zone hinzugefügt"_u);
            processor.getModel().addZone();
            setView (View::mapping);
            showToast ("Neue Zone angelegt · C4–B4"_u);
            break;

        case cmd::addTrack:
            if (auto* zone = processor.getModel().getSelectedZone())
            {
                Track t;
                const auto& palette = trackPalette()[zone->tracks.size() % trackPalette().size()];
                t.name = "Spur " + juce::String ((int) zone->tracks.size() + 1);
                t.colour = palette.main;
                t.softColour = palette.soft;
                t.gain = 0.8f;
                processor.getHistory().nameNextStep (t.name + " angelegt");
                zone->tracks.push_back (t);
                zone->selectedTrack = (int) zone->tracks.size() - 1;
                processor.getModel().notifyChanged();
                setView (View::editor);
                showToast (t.name + " angelegt · ein Sample aus der Liste daraufziehen"_u);
            }
            else
            {
                showToast ("Erst eine Zone anlegen oder wählen"_u);
            }
            break;

        case cmd::bounceZone:
            bounceSelectedZone();
            break;

        case cmd::assignMacros:
            MacroWindow::show (ctx, this);
            break;

        case cmd::openManual:
            notYet ("noch nicht geschrieben");
            break;

        case cmd::showShortcuts:
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::NoIcon, "Tastaturkürzel"_u,
                "Strg+M\tMenüleiste ein/aus\n"
                "F9 / F10\tLinke / rechte Spalte\n"
                "F2 / F3 / F4\tMapping / Editor / Export\n"
                "Leertaste\tWiedergabe\n"
                "Strg+N / O / S\tNeu / Öffnen / Speichern\n"
                "Strg+I\tSamples importieren\n"
                "Strg+E\tExportieren"_u,
                "OK", this);
            break;

        case cmd::credits:
            CreditsWindow::show (this);
            break;

        case cmd::togglePlayback: togglePlayback(); break;

        default:
            return false;
    }

    return true;
}

//==============================================================================
void StudioShell::setView (View v)
{
    if (ui.view != v)
    {
        ui.view = v;
        ui.changed();
    }
}

void StudioShell::toggleChrome (bool& flag, const juce::String& label, const juce::String& shortcut)
{
    flag = ! flag;
    ui.changed();
    showToast (label + (flag ? juce::String (" eingeblendet")
                             : " ausgeblendet · "_u + shortcut + " holt sie zurück"_u));
}

void StudioShell::startAudition()
{
    stopAudition();

    const auto* zone = processor.getModel().getSelectedZone();

    if (zone == nullptr)
    {
        showToast ("Keine Zone gewählt"_u);
        return;
    }

    if (! processor.zoneHasAudio (*zone))
    {
        showToast ("Zone " + zone->name + " hat keine Audiodaten · Sample doppelt anklicken"_u);
        return;
    }

    auditionNote = zone->rootNote;

    // Ab dem Locator: was davor liegt, überspringt die Engine
    processor.setPlaybackStart (auditionNote, ui.locator * InstrumentModel::timelineSeconds);
    processor.getKeyboardState().noteOn (1, auditionNote, 0.8f);
}

void StudioShell::stopAudition()
{
    if (auditionNote >= 0)
    {
        processor.getKeyboardState().noteOff (1, auditionNote, 0.0f);
        auditionNote = -1;
    }
}

void StudioShell::togglePlayback()
{
    ui.playing = ! ui.playing;

    if (ui.playing)
    {
        ui.playhead = ui.locator;
        lastTick = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (30);
        startAudition();
    }
    else
    {
        stopAudition();
        stopTimer();
    }

    ui.changed();
}

void StudioShell::timerCallback()
{
    // Laufmarke über die 8-Sekunden-Achse; die Audio-Wiedergabe folgt mit der Engine.
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jmin (0.05, (now - lastTick) / 1000.0);
    lastTick = now;

    ui.playhead += dt / InstrumentModel::timelineSeconds;

    /* Gespielt wird mindestens die 8-Sekunden-Achse, bei längeren Clips bis zu ihrem Ende –
       und immer ein Stück hinter den Locator. */
    const auto* zone = processor.getModel().getSelectedZone();
    const double end = juce::jmax (1.0, zone != nullptr ? zone->contentEnd() : 0.0, ui.locator + 0.125);

    if (ui.playhead > end)
    {
        if (ui.looping)
        {
            // Neue Runde: Zone erneut anspielen, wieder ab dem Locator
            ui.playhead = ui.locator;
            startAudition();
        }
        else
        {
            ui.playhead = ui.locator;
            ui.playing = false;
            stopAudition();
            stopTimer();
            ui.changed();
        }
    }
}

//==============================================================================
void StudioShell::newInstrument()
{
    /* Die Art wird gleich hier entschieden und nicht später irgendwo nachgetragen: sie
       bestimmt, ob die Taste die Tonhöhe vorgibt, und damit die ganze Arbeitsweise.
       Umstellen lässt sie sich trotzdem, oben rechts im Mapping. */
    newInstrumentWindow = std::make_unique<juce::AlertWindow> (
        "Neues Instrument",
        "Das aktuelle Instrument wird geschlossen. Nicht gespeicherte Änderungen gehen verloren.\n\n"
        "Ein Tonhöhen-Instrument verstimmt das Sample über die Tastatur. Ein Drumset spielt "
        "jedes Sample in seiner eigenen Tonhöhe – je Taste ein Teil des Kits."_u,
        juce::MessageBoxIconType::QuestionIcon, this);

    newInstrumentWindow->addButton ("Tonhöhen-Instrument"_u, 1);
    newInstrumentWindow->addButton ("Drumset", 2);
    newInstrumentWindow->addButton ("Abbrechen"_u, 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<StudioShell> safe (this);
    newInstrumentWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr)
            return;

        safe->newInstrumentWindow = nullptr;

        if (result == 0)
            return;

        auto& model = safe->processor.getModel();
        model.clear();
        model.kind = result == 2 ? InstrumentKind::drumKit : InstrumentKind::melodic;
        model.notifyChanged();

        safe->processor.getHistory().reset();   // das leere Instrument ist der neue Anfang
        safe->setView (View::mapping);
        safe->showToast ("Neues Instrument: "_u + toDisplayString (model.kind));
    }), false);
}

void StudioShell::openProject()
{
    const auto& current = processor.getModel().projectFile;
    fileChooser = std::make_unique<juce::FileChooser> ("Instrument öffnen"_u,
                                                       current.existsAsFile() ? current.getParentDirectory()
                                                                              : defaultProjectFolder(),
                                                       "*.sisp");

    juce::Component::SafePointer<StudioShell> safe (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (safe == nullptr || file == juce::File())
            return;

        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! safe->processor.loadProjectState (juce::ValueTree::fromXml (*xml),
                                                                  file.getParentDirectory()))
        {
            safe->showToast ("Datei konnte nicht gelesen werden: "_u + file.getFileName());
            return;
        }

        safe->processor.getModel().projectFile = file;
        safe->processor.getModel().notifyChanged();
        safe->setView (View::mapping);
        safe->showToast (file.getFileName() + " geöffnet"_u);
    });
}

bool StudioShell::writeProject (const juce::File& file)
{
    auto xml = processor.createProjectState().createXml();
    return xml != nullptr && writeXmlDirectly (*xml, file);
}

void StudioShell::saveProject (bool askForFile)
{
    auto& model = processor.getModel();

    if (! askForFile && model.projectFile != juce::File())
    {
        showToast (writeProject (model.projectFile) ? model.projectFile.getFileName() + " gespeichert"
                                                    : "Speichern fehlgeschlagen"_u);
        return;
    }

    const auto suggestion = (model.projectFile != juce::File() ? model.projectFile.getParentDirectory()
                                                               : defaultProjectFolder())
                                .getChildFile (juce::File::createLegalFileName (model.name) + ".sisp");

    fileChooser = std::make_unique<juce::FileChooser> ("Instrument speichern", suggestion, "*.sisp");

    juce::Component::SafePointer<StudioShell> safe (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (safe == nullptr || file == juce::File())
            return;

        file = file.withFileExtension ("sisp");
        auto& m = safe->processor.getModel();

        if (! safe->writeProject (file))
        {
            safe->showToast ("Speichern fehlgeschlagen"_u);
            return;
        }

        m.projectFile = file;
        m.name = file.getFileNameWithoutExtension();
        m.notifyChanged();
        safe->showToast (file.getFileName() + " gespeichert");
    });
}

void StudioShell::importSamples()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Samples importieren", defaultProjectFolder(), audioWildcard);

    juce::Component::SafePointer<StudioShell> safe (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [safe] (const juce::FileChooser& chooser)
    {
        if (safe == nullptr || chooser.getResults().isEmpty())
            return;

        const int added = safe->processor.getModel().importSamples (chooser.getResults(),
                                                                   safe->processor.getFormatManager());
        safe->showToast (added > 0 ? juce::String (added) + (added == 1 ? " Sample importiert" : " Samples importiert")
                                   : "Keine neuen Audiodateien gefunden"_u);
    });
}

void StudioShell::bounceSelectedZone()
{
    const auto* zone = processor.getModel().getSelectedZone();

    if (zone == nullptr || zone->tracks.empty())
    {
        showToast ("Erst eine Zone mit Spuren wählen"_u);
        return;
    }

    if (! processor.zoneHasAudio (*zone))
    {
        showToast ("Die Zone hat keine Audiodaten"_u);
        return;
    }

    const auto zoneId = zone->id;
    const int numTracks = (int) zone->tracks.size();
    const auto folder = processor.getDefaultBounceFolder();

    // Bouncen ist nicht rückgängig zu machen, solange es keinen Verlauf gibt
    const auto message = "Die "_u + juce::String (numTracks)
                         + (numTracks == 1 ? " Spur der Zone "_u : " Spuren der Zone "_u) + zone->name
                         + " werden mit allen Effekten in eine Audiodatei gerechnet und durch diese ersetzt.\n\n"_u
                         + "Danach stecken auch fremde Plugins fest im Instrument und überleben den Export. "_u
                         + "Rückgängig machen lässt sich das nicht.\n\n"_u
                         + "Ziel: "_u + folder.getFullPathName();

    juce::Component::SafePointer<StudioShell> safe (this);

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Zone bouncen"_u)
                                      .withMessage (message)
                                      .withButton ("Bouncen"_u)
                                      .withButton ("Abbrechen"_u)
                                      .withAssociatedComponent (safe),
                                  [safe, zoneId, folder] (int result)
    {
        if (safe == nullptr || result != 1)
            return;

        safe->stopAudition();
        safe->processor.getHistory().nameNextStep ("Zone gebounct"_u);

        const auto outcome = safe->processor.bounceZone (zoneId, folder);
        safe->showToast (outcome.succeeded ? outcome.message
                                           : "Bounce fehlgeschlagen · "_u + outcome.message);
    });
}
} // namespace sis
