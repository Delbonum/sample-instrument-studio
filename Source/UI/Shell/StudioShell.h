#pragma once

#include <set>

#include <JuceHeader.h>

#include "../Editor/EditorView.h"
#include "../Export/ExportView.h"
#include "../Mapping/MappingView.h"
#include "../Panels/SampleBrowser.h"
#include "../Panels/TrackInspector.h"
#include "../Panels/ZonePanel.h"
#include "StatusBar.h"
#include "StudioContext.h"
#include "TitleBar.h"
#include "Toast.h"
#include "ToolBar.h"

namespace sis
{
class StudioProcessor;

/** Platzhalter für Ansichten, die noch umgesetzt werden (Editor, Export). */
class PendingView final : public juce::Component
{
public:
    PendingView (juce::String title, juce::String description);
    void paint (juce::Graphics&) override;

private:
    juce::String title, description;
};

/** Standalone-Rahmen: Fensterleiste, Menüleiste, Werkzeugleiste, Arbeitsbereich, Statusleiste.
    Hält den UI-Zustand, die Befehle (Menü + Tastenkürzel) und die Einblendungen. */
class StudioShell final : public juce::Component,
                          public juce::MenuBarModel,
                          public juce::ApplicationCommandTarget,
                          public juce::DragAndDropContainer,   // Samples aus dem Browser in die Mitte ziehen
                          private juce::ChangeListener,
                          private juce::Timer
{
public:
    StudioShell (StudioProcessor&, bool drawOwnTitleBar);
    ~StudioShell() override;

    /** Nur im Standalone gesetzt: öffnet den Audio-/MIDI-Dialog des Hosts. */
    std::function<void()> onShowAudioSettings;

    void showToast (const juce::String&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType) override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

    // ApplicationCommandTarget
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void setView (View);

    /** Buchstabentasten spielen Noten, solange sie gedrückt sind. */
    void updateComputerKeyboard();
    void releaseComputerKeyboard();
    int computerKeyToNote (int keyCode) const;
    void toggleChrome (bool& flag, const juce::String& label, const juce::String& shortcut);
    void togglePlayback();

    /** Wiedergabe spielt die gewählte Zone vor: eine Note auf dem Grundton,
        die Spur-Versätze auf der Zeitachse ergeben den zeitlichen Aufbau. */
    void startAudition();
    void stopAudition();

    // Welche Noten die Computertastatur gerade hält
    std::set<int> heldComputerNotes;

    void newInstrument();
    void openProject();
    void saveProject (bool askForFile);
    bool writeProject (const juce::File&);
    void importSamples();

    /** Fragt nach und rendert dann die gewählte Zone samt Effekten in eine WAV-Datei. */
    void bounceSelectedZone();

    StudioProcessor& processor;
    UiState ui;
    juce::ApplicationCommandManager commandManager;
    StudioContext ctx;

    const bool hasTitleBar;
    TitleBar titleBar;
    juce::MenuBarComponent menuBar;
    ToolBar toolBar;
    SampleBrowser sampleBrowser;
    TrackInspector trackInspector;
    ZonePanel zonePanel;
    MappingView mappingView;
    EditorView editorView;
    ExportView exportView;
    StatusBar statusBar;
    Toast toast;
    std::unique_ptr<juce::AlertWindow> newInstrumentWindow;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<juce::FileChooser> fileChooser;
    double lastTick = 0.0;
    int auditionNote = -1;

    // Windows liefert Zeichen-Tasten (z. B. Leertaste) als WM_KEYDOWN und als WM_CHAR;
    // beides landet bei der Befehlszuordnung. Ohne Entprellung schaltet die Leertaste
    // die Wiedergabe sofort wieder aus.
    juce::CommandID lastKeyCommand = 0;
    juce::uint32 lastKeyCommandTime = 0;
    bool isRepeatedKeyInvocation (const InvocationInfo&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioShell)
};
} // namespace sis
